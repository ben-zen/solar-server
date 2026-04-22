// Copyright (C) Ben Lewis, 2026.
// SPDX-License-Identifier: MIT
//
// Renogy solar controller register definitions and parsing logic.
// Derived from sophienyaa/NodeRenogy (MIT licensed).
// See https://github.com/sophienyaa/NodeRenogy

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <fmt/format.h>

#include "modbus.hh"
#include "serial.hh"

// ---------------------------------------------------------------------------
// Modbus register addresses
// ---------------------------------------------------------------------------

/// Starting register for controller identification data.
constexpr uint16_t renogy_info_start_register = 0x00A;
/// Number of registers to read for controller identification.
constexpr uint16_t renogy_info_num_registers = 17;

/// Starting register for live state / telemetry data.
constexpr uint16_t renogy_data_start_register = 0x100;
/// Number of registers to read for live state data.
constexpr uint16_t renogy_data_num_registers = 35;

// ---------------------------------------------------------------------------
// Charging state enumeration
// ---------------------------------------------------------------------------

/// Charging states reported by register 0x120 (low byte).
enum class charging_state : uint8_t {
  deactivated = 0,
  activated = 1,
  mppt = 2,
  equalizing = 3,
  boost = 4,
  floating = 5,
  current_limiting = 6,
};

template <>
struct fmt::formatter<charging_state> : fmt::formatter<std::string_view> {
  template <typename Context>
  auto format(charging_state state, Context &ctx) const {
    std::string_view name;
    switch (state) {
    case charging_state::deactivated:
      name = "deactivated";
      break;
    case charging_state::activated:
      name = "activated";
      break;
    case charging_state::mppt:
      name = "MPPT";
      break;
    case charging_state::equalizing:
      name = "equalizing";
      break;
    case charging_state::boost:
      name = "boost";
      break;
    case charging_state::floating:
      name = "floating";
      break;
    case charging_state::current_limiting:
      name = "current limiting";
      break;
    default:
      name = "unknown";
      break;
    }
    return fmt::formatter<std::string_view>::format(name, ctx);
  }
};

// ---------------------------------------------------------------------------
// Live controller data (registers 0x100–0x122)
// ---------------------------------------------------------------------------

/// Telemetry snapshot read from the Renogy controller's data registers.
struct controller_data {
  // --- Instantaneous readings ---
  uint8_t battery_capacity_pct;  // 0x100  SOC (%)
  float battery_voltage;         // 0x101  (raw × 0.1 V)
  float battery_charge_current;  // 0x102  (raw × 0.01 A)
  int8_t controller_temperature; // 0x103  high byte (°C, signed)
  int8_t battery_temperature;    // 0x103  low byte  (°C, signed)
  float load_voltage;            // 0x104  (raw × 0.1 V)
  float load_current;            // 0x105  (raw × 0.01 A)
  uint16_t load_power;           // 0x106  (W)
  float solar_voltage;           // 0x107  (raw × 0.1 V)
  float solar_current;           // 0x108  (raw × 0.01 A)
  uint16_t solar_power;          // 0x109  (W)

  // --- Daily statistics ---
  float min_battery_voltage_today;    // 0x10B  (raw × 0.1 V)
  float max_battery_voltage_today;    // 0x10C  (raw × 0.1 V)
  float max_charge_current_today;     // 0x10D  (raw × 0.01 A)
  float max_discharge_current_today;  // 0x10E  (raw × 0.01 A)
  uint16_t max_charge_power_today;    // 0x10F  (W)
  uint16_t max_discharge_power_today; // 0x110  (W)
  uint16_t charge_ah_today;           // 0x111  (Ah)
  uint16_t discharge_ah_today;        // 0x112  (Ah)
  uint16_t charge_wh_today;           // 0x113  (Wh)
  uint16_t discharge_wh_today;        // 0x114  (Wh)

  // --- Lifetime counters ---
  uint16_t controller_uptime_days;         // 0x115
  uint16_t total_battery_over_discharges;  // 0x116
  uint16_t total_battery_full_charges;     // 0x117
  uint32_t total_charge_ah;                // 0x118–0x119
  uint32_t total_discharge_ah;             // 0x11A–0x11B
  uint32_t cumulative_power_generated_kwh; // 0x11C–0x11D
  uint32_t cumulative_power_consumed_kwh;  // 0x11E–0x11F

  // --- Status ---
  uint8_t load_status;              // 0x120  high byte
  charging_state charge_state;      // 0x120  low byte
  uint32_t fault_codes;             // 0x121–0x122
};

template <>
struct fmt::formatter<controller_data> : fmt::formatter<std::string_view> {
  template <typename Context>
  auto format(const controller_data &d, Context &ctx) const {
    return fmt::format_to(
        ctx.out(),
        "battery: {}% {:.2f}V {:.2f}A | solar: {:.2f}V {:.2f}A {}W | "
        "load: {:.2f}V {:.2f}A {}W | charging: {}",
        d.battery_capacity_pct, d.battery_voltage, d.battery_charge_current,
        d.solar_voltage, d.solar_current, d.solar_power, d.load_voltage,
        d.load_current, d.load_power, d.charge_state);
  }
};

// ---------------------------------------------------------------------------
// Controller identification (registers 0x00A–0x01A)
// ---------------------------------------------------------------------------

/// Static identification data read once at startup.
struct controller_info {
  uint8_t voltage_rating;           // 0x00A  high byte (V)
  uint8_t current_rating;           // 0x00A  low byte  (A)
  uint8_t discharge_current_rating; // 0x00B  high byte (A)
  std::string controller_type;      // 0x00B  low byte: 0=Controller, 1=Inverter
  std::string model;                // 0x00C–0x013 (ASCII)
  std::string software_version;     // 0x014–0x015
  std::string hardware_version;     // 0x016–0x017
  uint32_t serial_number;           // 0x018–0x019
  uint8_t modbus_address;           // 0x01A
};

template <>
struct fmt::formatter<controller_info> : fmt::formatter<std::string_view> {
  template <typename Context>
  auto format(const controller_info &ci, Context &ctx) const {
    return fmt::format_to(ctx.out(),
                          "{} ({}V/{}A) sw:{} hw:{} sn:{} addr:{}",
                          ci.model, ci.voltage_rating, ci.current_rating,
                          ci.software_version, ci.hardware_version,
                          ci.serial_number, ci.modbus_address);
  }
};

// ---------------------------------------------------------------------------
// Parsing helpers — convert a vector of raw Modbus register values into
// the corresponding struct.
// ---------------------------------------------------------------------------

/// Parse \p registers (expected length: \ref renogy_data_num_registers) into
/// a \ref controller_data struct.  Returns \c std::nullopt when the register
/// vector has the wrong size.
std::optional<controller_data>
parse_controller_data(const std::vector<uint16_t> &registers);

/// Parse \p registers (expected length: \ref renogy_info_num_registers) into
/// a \ref controller_info struct.  Returns \c std::nullopt when the register
/// vector has the wrong size.
std::optional<controller_info>
parse_controller_info(const std::vector<uint16_t> &registers);

// ---------------------------------------------------------------------------
// High-level controller interface
// ---------------------------------------------------------------------------

/// Communicates with a Renogy solar charge controller over Modbus RTU.
class renogy_controller {
  serial_port m_port;
  uint8_t m_device_addr;

  /// Read \p num_registers starting from \p start_register.  Sends the
  /// request, reads the response, and parses the Modbus frame.
  modbus_response read_registers(uint16_t start_register,
                                 uint16_t num_registers);

public:
  /// Construct a controller handle that will talk to \p device at
  /// \p baud_rate (default 9600), with Modbus device address \p device_addr
  /// (default 1).
  explicit renogy_controller(uint8_t device_addr = 1);

  /// Open the serial port.  Returns true on success.
  bool open(const std::string &device, int baud_rate = 9600);

  /// True when the serial port is open.
  [[nodiscard]] bool is_open() const;

  /// Read live telemetry from the controller.
  std::optional<controller_data> read_data();

  /// Read identification / static information from the controller.
  std::optional<controller_info> read_info();
};
