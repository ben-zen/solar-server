// Copyright (C) Ben Lewis, 2026.
// SPDX-License-Identifier: MIT
//
// Renogy solar controller register parsing and communication.
// Derived from sophienyaa/NodeRenogy (MIT licensed).
// See https://github.com/sophienyaa/NodeRenogy

#include "renogy.hh"

#include <algorithm>
#include <cstring>
#include <thread>

#include <fmt/format.h>

// ---------------------------------------------------------------------------
// Helper: combine two consecutive 16-bit registers into a 32-bit value
// (big-endian, high register first).
// ---------------------------------------------------------------------------

static uint32_t combine_registers(uint16_t hi, uint16_t lo) {
  return (static_cast<uint32_t>(hi) << 16) | static_cast<uint32_t>(lo);
}

// ---------------------------------------------------------------------------
// Helper: reverse bit order in a byte.
// The NodeRenogy source notes that the load-status bits appear to be in
// completely inverted order.
// ---------------------------------------------------------------------------

static uint8_t mirror_bits(uint8_t n) {
  uint8_t result = 0;
  for (int i = 0; i < 8; ++i) {
    result = static_cast<uint8_t>((result << 1) | (n & 1));
    n >>= 1;
  }
  return result;
}

// ---------------------------------------------------------------------------
// Parse live telemetry registers
// ---------------------------------------------------------------------------

std::optional<controller_data>
parse_controller_data(const std::vector<uint16_t> &regs) {
  if (regs.size() < renogy_data_num_registers) {
    return std::nullopt;
  }

  controller_data d{};

  // Register 0x100 – Battery state of charge (%)
  d.battery_capacity_pct = static_cast<uint8_t>(regs[0]);

  // Register 0x101 – Battery voltage (raw × 0.1 V)
  d.battery_voltage = static_cast<float>(regs[1]) * 0.1f;

  // Register 0x102 – Battery charge current (raw × 0.01 A)
  d.battery_charge_current = static_cast<float>(regs[2]) * 0.01f;

  // Register 0x103 – Controller temp (high byte) / battery temp (low byte)
  d.controller_temperature = static_cast<int8_t>((regs[3] >> 8) & 0xFF);
  d.battery_temperature = static_cast<int8_t>(regs[3] & 0xFF);

  // Register 0x104 – Load voltage (raw × 0.1 V)
  d.load_voltage = static_cast<float>(regs[4]) * 0.1f;

  // Register 0x105 – Load current (raw × 0.01 A)
  d.load_current = static_cast<float>(regs[5]) * 0.01f;

  // Register 0x106 – Load power (W)
  d.load_power = regs[6];

  // Register 0x107 – Solar panel voltage (raw × 0.1 V)
  d.solar_voltage = static_cast<float>(regs[7]) * 0.1f;

  // Register 0x108 – Solar panel current (raw × 0.01 A)
  d.solar_current = static_cast<float>(regs[8]) * 0.01f;

  // Register 0x109 – Solar panel power (W)
  d.solar_power = regs[9];

  // Register 0x10A is a write-only register (turn on load); skip index 10.

  // Register 0x10B – Min battery voltage today (raw × 0.1 V)
  d.min_battery_voltage_today = static_cast<float>(regs[11]) * 0.1f;

  // Register 0x10C – Max battery voltage today (raw × 0.1 V)
  d.max_battery_voltage_today = static_cast<float>(regs[12]) * 0.1f;

  // Register 0x10D – Max charge current today (raw × 0.01 A)
  d.max_charge_current_today = static_cast<float>(regs[13]) * 0.01f;

  // Register 0x10E – Max discharge current today (raw × 0.01 A)
  d.max_discharge_current_today = static_cast<float>(regs[14]) * 0.01f;

  // Register 0x10F – Max charge power today (W)
  d.max_charge_power_today = regs[15];

  // Register 0x110 – Max discharge power today (W)
  d.max_discharge_power_today = regs[16];

  // Register 0x111 – Charge amp-hours today
  d.charge_ah_today = regs[17];

  // Register 0x112 – Discharge amp-hours today
  d.discharge_ah_today = regs[18];

  // Register 0x113 – Charge watt-hours today
  d.charge_wh_today = regs[19];

  // Register 0x114 – Discharge watt-hours today
  d.discharge_wh_today = regs[20];

  // Register 0x115 – Controller uptime (days)
  d.controller_uptime_days = regs[21];

  // Register 0x116 – Total battery over-discharges
  d.total_battery_over_discharges = regs[22];

  // Register 0x117 – Total battery full charges
  d.total_battery_full_charges = regs[23];

  // Registers 0x118–0x119 – Total charging amp-hours (32 bits)
  d.total_charge_ah = combine_registers(regs[24], regs[25]);

  // Registers 0x11A–0x11B – Total discharging amp-hours (32 bits)
  d.total_discharge_ah = combine_registers(regs[26], regs[27]);

  // Registers 0x11C–0x11D – Cumulative power generated (kWh, 32 bits)
  d.cumulative_power_generated_kwh = combine_registers(regs[28], regs[29]);

  // Registers 0x11E–0x11F – Cumulative power consumed (kWh, 32 bits)
  d.cumulative_power_consumed_kwh = combine_registers(regs[30], regs[31]);

  // Register 0x120 – Load status (high byte, bits reversed) + charging
  // state (low byte)
  d.load_status = mirror_bits(static_cast<uint8_t>((regs[32] >> 8) & 0xFF));
  d.charge_state =
      static_cast<charging_state>(static_cast<uint8_t>(regs[32] & 0xFF));

  // Registers 0x121–0x122 – Fault codes (32 bits)
  d.fault_codes = combine_registers(regs[33], regs[34]);

  return d;
}

// ---------------------------------------------------------------------------
// Parse controller identification registers
// ---------------------------------------------------------------------------

std::optional<controller_info>
parse_controller_info(const std::vector<uint16_t> &regs) {
  if (regs.size() < renogy_info_num_registers) {
    return std::nullopt;
  }

  controller_info info{};

  // Register 0x00A – Voltage rating (high byte) / current rating (low byte)
  info.voltage_rating = static_cast<uint8_t>((regs[0] >> 8) & 0xFF);
  info.current_rating = static_cast<uint8_t>(regs[0] & 0xFF);

  // Register 0x00B – Discharge current (high byte) / type (low byte)
  info.discharge_current_rating = static_cast<uint8_t>((regs[1] >> 8) & 0xFF);
  info.controller_type =
      ((regs[1] & 0xFF) == 0) ? "Controller" : "Inverter";

  // Registers 0x00C–0x013 – Model string (8 registers, 2 ASCII chars each)
  std::string model;
  for (size_t i = 2; i <= 9; ++i) {
    char hi_char = static_cast<char>((regs[i] >> 8) & 0xFF);
    char lo_char = static_cast<char>(regs[i] & 0xFF);
    if (hi_char != '\0') {
      model += hi_char;
    }
    if (lo_char != '\0') {
      model += lo_char;
    }
  }

  // Trim leading/trailing whitespace.
  auto start = model.find_first_not_of(' ');
  auto end = model.find_last_not_of(' ');
  if (start != std::string::npos) {
    info.model = model.substr(start, end - start + 1);
  }

  // Registers 0x014–0x015 – Software version (4 bytes → Vx.y.z)
  uint8_t sw_major = static_cast<uint8_t>(regs[10] & 0xFF);
  uint8_t sw_minor = static_cast<uint8_t>((regs[11] >> 8) & 0xFF);
  uint8_t sw_patch = static_cast<uint8_t>(regs[11] & 0xFF);
  info.software_version = fmt::format("V{}.{}.{}", sw_major, sw_minor, sw_patch);

  // Registers 0x016–0x017 – Hardware version (4 bytes → Vx.y.z)
  uint8_t hw_major = static_cast<uint8_t>(regs[12] & 0xFF);
  uint8_t hw_minor = static_cast<uint8_t>((regs[13] >> 8) & 0xFF);
  uint8_t hw_patch = static_cast<uint8_t>(regs[13] & 0xFF);
  info.hardware_version = fmt::format("V{}.{}.{}", hw_major, hw_minor, hw_patch);

  // Registers 0x018–0x019 – Serial number (32 bits)
  info.serial_number = combine_registers(regs[14], regs[15]);

  // Register 0x01A – Modbus address
  info.modbus_address = static_cast<uint8_t>(regs[16] & 0xFF);

  return info;
}

// ---------------------------------------------------------------------------
// renogy_controller implementation
// ---------------------------------------------------------------------------

renogy_controller::renogy_controller(uint8_t device_addr)
    : m_device_addr(device_addr) {}

bool renogy_controller::open(const std::string &device, int baud_rate) {
  return m_port.open(device, baud_rate);
}

bool renogy_controller::is_open() const { return m_port.is_open(); }

modbus_response renogy_controller::read_registers(uint16_t start_register,
                                                   uint16_t num_registers) {
  auto request = modbus_read_holding_registers_request(m_device_addr,
                                                       start_register,
                                                       num_registers);

  ssize_t written = m_port.write(request.data(), request.size());
  if (written < 0 || static_cast<size_t>(written) != request.size()) {
    return modbus_response{{}, "failed to write request to serial port"};
  }

  // Allow time for the controller to process and respond.  Modbus RTU
  // specifies a 3.5-character silence between frames; at 9600 baud one
  // character ≈ 1 ms, so a short sleep is sufficient.
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  // Expected response size: addr(1) + func(1) + count(1) +
  // data(num_registers * 2) + CRC(2).
  size_t expected =
      static_cast<size_t>(3) + (static_cast<size_t>(num_registers) * 2) + 2;

  std::vector<uint8_t> buffer(expected + 16); // extra margin
  size_t total_read = 0;
  int retries = 10;
  while (total_read < expected && retries-- > 0) {
    ssize_t n = m_port.read(buffer.data() + total_read,
                            buffer.size() - total_read, 200);
    if (n > 0) {
      total_read += static_cast<size_t>(n);
    } else if (n == 0) {
      // Timeout — try once more.
      continue;
    } else {
      return modbus_response{{}, "serial read error"};
    }
  }

  return modbus_parse_holding_registers(buffer.data(), total_read,
                                        m_device_addr);
}

std::optional<controller_data> renogy_controller::read_data() {
  auto resp =
      read_registers(renogy_data_start_register, renogy_data_num_registers);
  if (!resp.ok()) {
    return std::nullopt;
  }
  return parse_controller_data(resp.registers);
}

std::optional<controller_info> renogy_controller::read_info() {
  auto resp =
      read_registers(renogy_info_start_register, renogy_info_num_registers);
  if (!resp.ok()) {
    return std::nullopt;
  }
  return parse_controller_info(resp.registers);
}
