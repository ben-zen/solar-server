// Copyright (C) Ben Lewis, 2025.
// SPDX-License-Identifier: MIT

#pragma once

#include <csignal>

// Global SIGINT flag.  Set by the signal handler, checked and cleared by
// prompt implementations.
inline volatile sig_atomic_t g_sigint_received = 0;
