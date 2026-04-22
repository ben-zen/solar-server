// Copyright (C) Ben Lewis, 2026.
// SPDX-License-Identifier: MIT

#include "modbus.hh"

#include <fmt/format.h>

// Standard Modbus CRC-16 (polynomial 0xA001, initial value 0xFFFF).
uint16_t modbus_crc16(const uint8_t *data, size_t length) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < length; ++i) {
    crc ^= static_cast<uint16_t>(data[i]);
    for (int bit = 0; bit < 8; ++bit) {
      if (crc & 0x0001) {
        crc = (crc >> 1) ^ 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

std::vector<uint8_t> modbus_read_holding_registers_request(
    uint8_t device_addr, uint16_t start_register, uint16_t num_registers) {
  std::vector<uint8_t> frame(8);
  frame[0] = device_addr;
  frame[1] = static_cast<uint8_t>(modbus_function::read_holding_registers);
  frame[2] = static_cast<uint8_t>(start_register >> 8);
  frame[3] = static_cast<uint8_t>(start_register & 0xFF);
  frame[4] = static_cast<uint8_t>(num_registers >> 8);
  frame[5] = static_cast<uint8_t>(num_registers & 0xFF);

  uint16_t crc = modbus_crc16(frame.data(), 6);
  frame[6] = static_cast<uint8_t>(crc & 0xFF);        // CRC low byte
  frame[7] = static_cast<uint8_t>((crc >> 8) & 0xFF); // CRC high byte
  return frame;
}

modbus_response modbus_parse_holding_registers(const uint8_t *data,
                                               size_t length,
                                               uint8_t expected_addr) {
  modbus_response result;

  // Minimum frame: addr(1) + func(1) + byte_count(1) + CRC(2) = 5 bytes.
  if (length < 5) {
    result.error_message =
        fmt::format("response too short ({} bytes, minimum 5)", length);
    return result;
  }

  // Check device address.
  if (data[0] != expected_addr) {
    result.error_message = fmt::format(
        "unexpected device address 0x{:02X} (expected 0x{:02X})", data[0],
        expected_addr);
    return result;
  }

  // Check for Modbus exception response (function code with high bit set).
  if (data[1] & 0x80) {
    uint8_t exception_code = (length >= 3) ? data[2] : 0;
    result.error_message = fmt::format(
        "modbus exception: function 0x{:02X}, code 0x{:02X}", data[1],
        exception_code);
    return result;
  }

  // Verify function code.
  if (data[1] !=
      static_cast<uint8_t>(modbus_function::read_holding_registers)) {
    result.error_message = fmt::format(
        "unexpected function code 0x{:02X} (expected 0x03)", data[1]);
    return result;
  }

  uint8_t byte_count = data[2];

  // The frame must contain addr(1) + func(1) + count(1) + data(byte_count)
  // + CRC(2).
  size_t expected_length =
      static_cast<size_t>(3) + byte_count + 2;
  if (length < expected_length) {
    result.error_message =
        fmt::format("response truncated ({} bytes, expected {})", length,
                    expected_length);
    return result;
  }

  // Verify CRC over the payload (everything except the trailing 2 CRC bytes).
  size_t payload_length = expected_length - 2;
  uint16_t computed_crc = modbus_crc16(data, payload_length);
  uint16_t received_crc = static_cast<uint16_t>(data[payload_length]) |
                          (static_cast<uint16_t>(data[payload_length + 1]) << 8);

  if (computed_crc != received_crc) {
    result.error_message =
        fmt::format("CRC mismatch (computed 0x{:04X}, received 0x{:04X})",
                    computed_crc, received_crc);
    return result;
  }

  // Byte count must be even (each register is 2 bytes).
  if (byte_count % 2 != 0) {
    result.error_message =
        fmt::format("odd byte count {} in register response", byte_count);
    return result;
  }

  size_t num_registers = byte_count / 2;
  result.registers.reserve(num_registers);
  for (size_t i = 0; i < num_registers; ++i) {
    uint16_t hi = data[3 + i * 2];
    uint16_t lo = data[3 + i * 2 + 1];
    result.registers.push_back(static_cast<uint16_t>((hi << 8) | lo));
  }

  return result;
}
