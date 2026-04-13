// Copyright (C) Ben Lewis, 2025.
// SPDX-License-Identifier: MIT

#pragma once

#include "renderer.hh"

#include <chrono>
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

// Write a new guestbook entry to |logbook_dir|.
//
// The entry is written in the same Hugo-compatible format as the guestbook
// CGI binary (JSON front matter with author, date, location, type keys,
// followed by the message body).  Returns true on success.
bool write_entry(const std::string &logbook_dir,
                 const std::string &author,
                 const std::string &location,
                 const std::string &message,
                 std::chrono::system_clock::time_point timestamp);
