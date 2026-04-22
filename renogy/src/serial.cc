// Copyright (C) Ben Lewis, 2026.
// SPDX-License-Identifier: MIT

#include "serial.hh"

#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>
#include <utility>

// Map a numeric baud rate to the POSIX speed constant.
static speed_t baud_to_speed(int baud) {
  switch (baud) {
  case 2400:
    return B2400;
  case 4800:
    return B4800;
  case 9600:
    return B9600;
  case 19200:
    return B19200;
  case 38400:
    return B38400;
  case 57600:
    return B57600;
  case 115200:
    return B115200;
  default:
    return B9600;
  }
}

serial_port::~serial_port() { close(); }

serial_port::serial_port(serial_port &&other) noexcept : m_fd(other.m_fd) {
  other.m_fd = -1;
}

serial_port &serial_port::operator=(serial_port &&other) noexcept {
  if (this != &other) {
    close();
    m_fd = other.m_fd;
    other.m_fd = -1;
  }
  return *this;
}

bool serial_port::open(const std::string &device, int baud_rate) {
  close();

  // NOLINT: O_NOCTTY is required to avoid becoming the controlling terminal.
  m_fd = ::open(device.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (m_fd < 0) {
    return false;
  }

  // Clear non-blocking now that we have the fd; we use poll() for timeouts.
  int flags = fcntl(m_fd, F_GETFL, 0);
  if (flags >= 0) {
    fcntl(m_fd, F_SETFL, flags & ~O_NONBLOCK);
  }

  struct termios tio {};
  if (tcgetattr(m_fd, &tio) != 0) {
    close();
    return false;
  }

  // Configure 8N1, no flow control.
  cfmakeraw(&tio);
  tio.c_cflag &= ~(CSIZE | PARENB | CSTOPB | CRTSCTS);
  tio.c_cflag |= CS8 | CLOCAL | CREAD;
  tio.c_iflag = 0;
  tio.c_oflag = 0;
  tio.c_lflag = 0;

  // VMIN=0, VTIME=10: pure timer mode — read() waits up to 1 second for
  // any data, returning whatever arrived (possibly zero bytes on timeout).
  tio.c_cc[VMIN] = 0;
  tio.c_cc[VTIME] = 10; // tenths of a second

  speed_t speed = baud_to_speed(baud_rate);
  cfsetispeed(&tio, speed);
  cfsetospeed(&tio, speed);

  if (tcsetattr(m_fd, TCSANOW, &tio) != 0) {
    close();
    return false;
  }

  tcflush(m_fd, TCIOFLUSH);
  return true;
}

void serial_port::close() {
  if (m_fd >= 0) {
    ::close(m_fd);
    m_fd = -1;
  }
}

bool serial_port::is_open() const { return m_fd >= 0; }

ssize_t serial_port::write(const uint8_t *data, size_t length) {
  if (m_fd < 0) {
    return -1;
  }
  return ::write(m_fd, data, length);
}

ssize_t serial_port::read(uint8_t *buffer, size_t max_length, int timeout_ms) {
  if (m_fd < 0) {
    return -1;
  }

  struct pollfd pfd {};
  pfd.fd = m_fd;
  pfd.events = POLLIN;

  int ret = poll(&pfd, 1, timeout_ms);
  if (ret <= 0) {
    // 0 = timeout, -1 = error
    return ret;
  }

  return ::read(m_fd, buffer, max_length);
}
