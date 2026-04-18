// Copyright (C) Ben Lewis, 2025.
// SPDX-License-Identifier: MIT

#pragma once

#include "renderer.hh"

#include <string>
#include <vector>

// Read the most recent guestbook entries from a logbook directory.
//
// Entries are Hugo content pages (JSON front matter + markdown body).
// This function scans |logbook_dir| for *.md files, sorts them by filename
// in descending order (newest first), and returns at most |max_entries|
// parsed entries.
std::vector<guestbook_entry> read_recent_entries(const std::string &logbook_dir,
                                                 int max_entries);

// Sign the guestbook by invoking the guestbook binary via fork/exec.
//
// |guestbook_bin| is the path to the guestbook executable.
// |logbook_dir| is passed to the child process via the LOGBOOK environment
// variable.  Returns true if the child process exits successfully.
bool run_guestbook_binary(const std::string &guestbook_bin,
                          const std::string &logbook_dir,
                          const std::string &author,
                          const std::string &location,
                          const std::string &message);
