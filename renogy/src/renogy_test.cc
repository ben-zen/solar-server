// Copyright (C) Ben Lewis, 2026.
// SPDX-License-Identifier: MIT

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <cstdint>
#include <vector>

#include "modbus.hh"
#include "renogy.hh"

// ===================================================================
// Modbus CRC-16
// ===================================================================

TEST_CASE("modbus_crc16 — known vectors") {
  // A standard Modbus example: slave addr 0x01, func 0x03, start 0x0000,
  // qty 0x000A → CRC should be 0xC5CD.
  SUBCASE("read 10 registers from address 0") {
    const uint8_t frame[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
    uint16_t crc = modbus_crc16(frame, sizeof(frame));
    // On the wire the CRC is sent low-byte first: 0xC5, 0xCD.
    // The function returns the raw 16-bit value: 0xCDC5.
    CHECK(crc == 0xCDC5);
  }

  SUBCASE("empty input") {
    uint16_t crc = modbus_crc16(nullptr, 0);
    CHECK(crc == 0xFFFF);
  }

  SUBCASE("single byte") {
    const uint8_t data[] = {0x00};
    uint16_t crc = modbus_crc16(data, 1);
    // CRC of {0x00} with initial 0xFFFF: 0xFFFF ^ 0x00 = 0xFFFF
    // After 8 iterations with polynomial 0xA001:
    CHECK(crc == 0x40BF);
  }
}

// ===================================================================
// Modbus request frame building
// ===================================================================

TEST_CASE("modbus_read_holding_registers_request") {
  SUBCASE("Renogy data registers") {
    auto frame = modbus_read_holding_registers_request(0x01, 0x0100, 35);
    REQUIRE(frame.size() == 8);
    CHECK(frame[0] == 0x01); // device address
    CHECK(frame[1] == 0x03); // function code
    CHECK(frame[2] == 0x01); // start register high
    CHECK(frame[3] == 0x00); // start register low
    CHECK(frame[4] == 0x00); // num registers high
    CHECK(frame[5] == 0x23); // num registers low (35)
    // Verify CRC matches what we compute.
    uint16_t expected_crc = modbus_crc16(frame.data(), 6);
    uint16_t frame_crc =
        static_cast<uint16_t>(frame[6]) |
        (static_cast<uint16_t>(frame[7]) << 8);
    CHECK(frame_crc == expected_crc);
  }

  SUBCASE("Renogy info registers") {
    auto frame = modbus_read_holding_registers_request(0x01, 0x000A, 17);
    REQUIRE(frame.size() == 8);
    CHECK(frame[0] == 0x01);
    CHECK(frame[1] == 0x03);
    CHECK(frame[2] == 0x00);
    CHECK(frame[3] == 0x0A);
    CHECK(frame[4] == 0x00);
    CHECK(frame[5] == 0x11); // 17 registers
  }
}

// ===================================================================
// Modbus response parsing
// ===================================================================

// Helper: build a valid Modbus response frame for given registers.
static std::vector<uint8_t>
build_response(uint8_t addr, const std::vector<uint16_t> &regs) {
  std::vector<uint8_t> frame;
  frame.push_back(addr);
  frame.push_back(0x03); // function code
  auto byte_count = static_cast<uint8_t>(regs.size() * 2);
  frame.push_back(byte_count);
  for (auto r : regs) {
    frame.push_back(static_cast<uint8_t>((r >> 8) & 0xFF));
    frame.push_back(static_cast<uint8_t>(r & 0xFF));
  }
  uint16_t crc = modbus_crc16(frame.data(), frame.size());
  frame.push_back(static_cast<uint8_t>(crc & 0xFF));
  frame.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));
  return frame;
}

TEST_CASE("modbus_parse_holding_registers") {
  SUBCASE("valid single register response") {
    std::vector<uint16_t> expected_regs = {0x1234};
    auto frame = build_response(0x01, expected_regs);
    auto resp =
        modbus_parse_holding_registers(frame.data(), frame.size(), 0x01);
    CHECK(resp.ok());
    REQUIRE(resp.registers.size() == 1);
    CHECK(resp.registers[0] == 0x1234);
  }

  SUBCASE("valid multi-register response") {
    std::vector<uint16_t> expected = {0xAAAA, 0xBBBB, 0xCCCC};
    auto frame = build_response(0x01, expected);
    auto resp =
        modbus_parse_holding_registers(frame.data(), frame.size(), 0x01);
    CHECK(resp.ok());
    REQUIRE(resp.registers.size() == 3);
    CHECK(resp.registers[0] == 0xAAAA);
    CHECK(resp.registers[1] == 0xBBBB);
    CHECK(resp.registers[2] == 0xCCCC);
  }

  SUBCASE("wrong device address") {
    std::vector<uint16_t> regs = {0x0001};
    auto frame = build_response(0x02, regs);
    auto resp =
        modbus_parse_holding_registers(frame.data(), frame.size(), 0x01);
    CHECK_FALSE(resp.ok());
    CHECK(resp.error_message.find("unexpected device address") !=
          std::string::npos);
  }

  SUBCASE("too short") {
    uint8_t data[] = {0x01, 0x03};
    auto resp = modbus_parse_holding_registers(data, sizeof(data), 0x01);
    CHECK_FALSE(resp.ok());
    CHECK(resp.error_message.find("too short") != std::string::npos);
  }

  SUBCASE("CRC mismatch") {
    std::vector<uint16_t> regs = {0x0042};
    auto frame = build_response(0x01, regs);
    // Corrupt one CRC byte.
    frame.back() ^= 0xFF;
    auto resp =
        modbus_parse_holding_registers(frame.data(), frame.size(), 0x01);
    CHECK_FALSE(resp.ok());
    CHECK(resp.error_message.find("CRC mismatch") != std::string::npos);
  }

  SUBCASE("modbus exception response") {
    // Exception: addr 0x01, function 0x83 (0x03 | 0x80), exception code 0x02
    uint8_t frame_data[] = {0x01, 0x83, 0x02, 0x00, 0x00};
    uint16_t crc = modbus_crc16(frame_data, 3);
    frame_data[3] = static_cast<uint8_t>(crc & 0xFF);
    frame_data[4] = static_cast<uint8_t>((crc >> 8) & 0xFF);
    auto resp =
        modbus_parse_holding_registers(frame_data, sizeof(frame_data), 0x01);
    CHECK_FALSE(resp.ok());
    CHECK(resp.error_message.find("exception") != std::string::npos);
  }
}

// ===================================================================
// Renogy controller data parsing
// ===================================================================

// Build a 35-register vector matching the Renogy data register layout,
// using values that exercise the scaling logic.
static std::vector<uint16_t> make_sample_data_registers() {
  std::vector<uint16_t> regs(35, 0);
  regs[0] = 96;    // battery capacity 96%
  regs[1] = 131;   // battery voltage = 131 * 0.1 = 13.1 V
  regs[2] = 212;   // charge current = 212 * 0.01 = 2.12 A
  regs[3] = 0x1218; // controller temp 18°C (high), battery temp 24°C (low)
  regs[4] = 0;     // load voltage = 0 (Wanderer has no switched load)
  regs[5] = 0;     // load current = 0
  regs[6] = 0;     // load power = 0
  regs[7] = 131;   // solar voltage = 13.1 V
  regs[8] = 212;   // solar current = 2.12 A
  regs[9] = 27;    // solar power = 27 W
  regs[10] = 0;    // 0x10A (write-only, skipped)
  regs[11] = 121;  // min batt V today = 12.1 V
  regs[12] = 134;  // max batt V today = 13.4 V
  regs[13] = 351;  // max charge current today = 3.51 A
  regs[14] = 0;    // max discharge current today = 0 A
  regs[15] = 46;   // max charge power today = 46 W
  regs[16] = 0;    // max discharge power today = 0 W
  regs[17] = 4;    // charge Ah today = 4
  regs[18] = 0;    // discharge Ah today = 0
  regs[19] = 50;   // charge Wh today = 50
  regs[20] = 0;    // discharge Wh today = 0
  regs[21] = 15;   // controller uptime = 15 days
  regs[22] = 9;    // total battery over-discharges = 9
  regs[23] = 3;    // total battery full charges = 3
  regs[24] = 0;    // total charge Ah (high)
  regs[25] = 500;  // total charge Ah (low) = 500 Ah
  regs[26] = 0;    // total discharge Ah (high)
  regs[27] = 200;  // total discharge Ah (low) = 200 Ah
  regs[28] = 0;    // cumulative power generated (high)
  regs[29] = 42;   // cumulative power generated (low) = 42 kWh
  regs[30] = 0;    // cumulative power consumed (high)
  regs[31] = 18;   // cumulative power consumed (low) = 18 kWh
  // 0x120: load status (high byte, bits reversed) | charging state (low byte)
  // Load status raw = 0x00, charging state = 5 (floating)
  regs[32] = 0x0005;
  regs[33] = 0;    // fault codes (high)
  regs[34] = 0;    // fault codes (low)
  return regs;
}

TEST_CASE("parse_controller_data") {
  SUBCASE("valid sample data matching NodeRenogy output") {
    auto regs = make_sample_data_registers();
    auto result = parse_controller_data(regs);
    REQUIRE(result.has_value());
    auto &d = *result;

    CHECK(d.battery_capacity_pct == 96);
    CHECK(d.battery_voltage == doctest::Approx(13.1f).epsilon(0.01));
    CHECK(d.battery_charge_current == doctest::Approx(2.12f).epsilon(0.01));
    CHECK(d.controller_temperature == 18);
    CHECK(d.battery_temperature == 24);
    CHECK(d.load_voltage == doctest::Approx(0.0f));
    CHECK(d.load_current == doctest::Approx(0.0f));
    CHECK(d.load_power == 0);
    CHECK(d.solar_voltage == doctest::Approx(13.1f).epsilon(0.01));
    CHECK(d.solar_current == doctest::Approx(2.12f).epsilon(0.01));
    CHECK(d.solar_power == 27);
    CHECK(d.min_battery_voltage_today == doctest::Approx(12.1f).epsilon(0.01));
    CHECK(d.max_battery_voltage_today == doctest::Approx(13.4f).epsilon(0.01));
    CHECK(d.max_charge_current_today == doctest::Approx(3.51f).epsilon(0.01));
    CHECK(d.max_discharge_current_today == doctest::Approx(0.0f));
    CHECK(d.max_charge_power_today == 46);
    CHECK(d.max_discharge_power_today == 0);
    CHECK(d.charge_ah_today == 4);
    CHECK(d.discharge_ah_today == 0);
    CHECK(d.charge_wh_today == 50);
    CHECK(d.discharge_wh_today == 0);
    CHECK(d.controller_uptime_days == 15);
    CHECK(d.total_battery_over_discharges == 9);
    CHECK(d.total_battery_full_charges == 3);
    CHECK(d.total_charge_ah == 500);
    CHECK(d.total_discharge_ah == 200);
    CHECK(d.cumulative_power_generated_kwh == 42);
    CHECK(d.cumulative_power_consumed_kwh == 18);
    CHECK(d.load_status == 0);
    CHECK(d.charge_state == charging_state::floating);
    CHECK(d.fault_codes == 0);
  }

  SUBCASE("too few registers returns nullopt") {
    std::vector<uint16_t> regs(10, 0);
    auto result = parse_controller_data(regs);
    CHECK_FALSE(result.has_value());
  }

  SUBCASE("too many registers returns nullopt") {
    auto regs = make_sample_data_registers();
    regs.push_back(0);
    auto result = parse_controller_data(regs);
    CHECK_FALSE(result.has_value());
  }

  SUBCASE("32-bit register combination") {
    auto regs = make_sample_data_registers();
    // Set total charge Ah to 0x00010002 = 65538
    regs[24] = 0x0001;
    regs[25] = 0x0002;
    auto result = parse_controller_data(regs);
    REQUIRE(result.has_value());
    CHECK(result->total_charge_ah == 65538);
  }

  SUBCASE("charging states") {
    auto regs = make_sample_data_registers();

    regs[32] = 0x0000;
    auto r0 = parse_controller_data(regs);
    REQUIRE(r0.has_value());
    CHECK(r0->charge_state == charging_state::deactivated);

    regs[32] = 0x0002;
    auto r2 = parse_controller_data(regs);
    REQUIRE(r2.has_value());
    CHECK(r2->charge_state == charging_state::mppt);

    regs[32] = 0x0004;
    auto r4 = parse_controller_data(regs);
    REQUIRE(r4.has_value());
    CHECK(r4->charge_state == charging_state::boost);
  }

  SUBCASE("temperature sign handling") {
    auto regs = make_sample_data_registers();
    // Negative temperatures: controller = -5°C (0xFB), battery = -10°C (0xF6)
    regs[3] = 0xFBF6;
    auto result = parse_controller_data(regs);
    REQUIRE(result.has_value());
    CHECK(result->controller_temperature == -5);
    CHECK(result->battery_temperature == -10);
  }
}

// ===================================================================
// Renogy controller info parsing
// ===================================================================

static std::vector<uint16_t> make_sample_info_registers() {
  std::vector<uint16_t> regs(17, 0);
  // 0x00A: voltage rating 12V (high byte) / current 30A (low byte)
  regs[0] = 0x0C1E; // 12, 30
  // 0x00B: discharge current 20A (high) / type 0=Controller (low)
  regs[1] = 0x1400;
  // 0x00C–0x013: model string " RNG-CTRL-WND30 " (8 registers)
  // Each register holds two ASCII characters.
  const char *model = " RNG-CTRL-WND30 ";
  for (int i = 0; i < 8; ++i) {
    regs[2 + i] = static_cast<uint16_t>(
        (static_cast<uint8_t>(model[i * 2]) << 8) |
         static_cast<uint8_t>(model[i * 2 + 1]));
  }
  // 0x014–0x015: software version V1.0.4
  // The NodeRenogy code reads: x14[1]=major, x14[2]=minor, x14[3]=patch
  // across two 16-bit registers.
  // regs[10] high byte = unused (0), low byte = major (1)
  // regs[11] high byte = minor (0), low byte = patch (4)
  regs[10] = 0x0001;
  regs[11] = 0x0004;
  // 0x016–0x017: hardware version V1.0.3
  regs[12] = 0x0001;
  regs[13] = 0x0003;
  // 0x018–0x019: serial number = 1116960
  // 1116960 = 0x00110B20 → regs[14] = 0x0011, regs[15] = 0x0B20
  regs[14] = 0x0011;
  regs[15] = 0x0B20;
  // 0x01A: MODBUS address = 1
  regs[16] = 0x0001;
  return regs;
}

TEST_CASE("parse_controller_info") {
  SUBCASE("valid Wanderer controller info") {
    auto regs = make_sample_info_registers();
    auto result = parse_controller_info(regs);
    REQUIRE(result.has_value());
    auto &ci = *result;

    CHECK(ci.voltage_rating == 12);
    CHECK(ci.current_rating == 30);
    CHECK(ci.discharge_current_rating == 20);
    CHECK(ci.controller_type == "Controller");
    CHECK(ci.model == "RNG-CTRL-WND30");
    CHECK(ci.software_version == "V1.0.4");
    CHECK(ci.hardware_version == "V1.0.3");
    CHECK(ci.serial_number == 1116960);
    CHECK(ci.modbus_address == 1);
  }

  SUBCASE("inverter type") {
    auto regs = make_sample_info_registers();
    regs[1] = 0x1401; // type = 1 → Inverter
    auto result = parse_controller_info(regs);
    REQUIRE(result.has_value());
    CHECK(result->controller_type == "Inverter");
  }

  SUBCASE("too few registers returns nullopt") {
    std::vector<uint16_t> regs(5, 0);
    auto result = parse_controller_info(regs);
    CHECK_FALSE(result.has_value());
  }

  SUBCASE("too many registers returns nullopt") {
    auto regs = make_sample_info_registers();
    regs.push_back(0);
    auto result = parse_controller_info(regs);
    CHECK_FALSE(result.has_value());
  }
}

// ===================================================================
// End-to-end: Modbus frame → parsed Renogy data
// ===================================================================

TEST_CASE("end-to-end: build response frame, parse, then decode") {
  auto regs = make_sample_data_registers();
  auto frame = build_response(0x01, regs);

  // Parse the Modbus frame.
  auto modbus_resp =
      modbus_parse_holding_registers(frame.data(), frame.size(), 0x01);
  REQUIRE(modbus_resp.ok());
  REQUIRE(modbus_resp.registers.size() == regs.size());

  // Decode into Renogy data.
  auto data = parse_controller_data(modbus_resp.registers);
  REQUIRE(data.has_value());
  CHECK(data->battery_capacity_pct == 96);
  CHECK(data->solar_power == 27);
  CHECK(data->charge_state == charging_state::floating);
}
