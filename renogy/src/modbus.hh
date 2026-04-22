// Copyright (C) Ben Lewis, 2026.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

/// Modbus RTU function codes used by Renogy controllers.
enum class modbus_function : uint8_t {
  read_holding_registers = 0x03,
};

/// Compute the Modbus CRC-16 checksum over \p length bytes starting at
/// \p data.
uint16_t modbus_crc16(const uint8_t *data, size_t length);

/// Build a Modbus RTU "Read Holding Registers" (0x03) request frame
/// addressed to \p device_addr, starting at \p start_register and reading
/// \p num_registers consecutive registers.
std::vector<uint8_t> modbus_read_holding_registers_request(
    uint8_t device_addr, uint16_t start_register, uint16_t num_registers);

/// The result of parsing a Modbus RTU response frame.  On success the
/// \c registers vector holds the decoded 16-bit register values.  On failure
/// \c error_message describes what went wrong.
struct modbus_response {
  std::vector<uint16_t> registers;
  std::string error_message;

  [[nodiscard]] bool ok() const { return error_message.empty(); }
};

/// Parse a Modbus RTU response to a "Read Holding Registers" request.
/// \p data / \p length is the raw byte buffer read from the serial port.
/// \p expected_addr is the device address we expect to see in the frame.
modbus_response modbus_parse_holding_registers(const uint8_t *data,
                                               size_t length,
                                               uint8_t expected_addr);
