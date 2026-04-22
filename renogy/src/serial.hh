// Copyright (C) Ben Lewis, 2026.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <sys/types.h>

/// RAII wrapper around a POSIX serial port configured for Modbus RTU
/// communication (8N1, no flow control).
class serial_port {
  int m_fd = -1;

public:
  serial_port() = default;
  ~serial_port();

  serial_port(const serial_port &) = delete;
  serial_port &operator=(const serial_port &) = delete;

  serial_port(serial_port &&other) noexcept;
  serial_port &operator=(serial_port &&other) noexcept;

  /// Open \p device at \p baud_rate (default 9600).  Returns true on success.
  bool open(const std::string &device, int baud_rate = 9600);

  /// Close the port (also called by the destructor).
  void close();

  /// True when the port file descriptor is valid.
  [[nodiscard]] bool is_open() const;

  /// Write \p length bytes from \p data.  Returns the number of bytes written,
  /// or -1 on error.
  ssize_t write(const uint8_t *data, size_t length);

  /// Read up to \p max_length bytes into \p buffer, waiting at most
  /// \p timeout_ms milliseconds.  Returns bytes read, 0 on timeout, or -1 on
  /// error.
  ssize_t read(uint8_t *buffer, size_t max_length, int timeout_ms = 500);
};
