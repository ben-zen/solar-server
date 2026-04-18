// Copyright (C) Ben Lewis, 2025.
// SPDX-License-Identifier: MIT

#pragma once

#include "shell.hh"

#include <filesystem>
#include <fstream>
#include <istream>
#include <string>

namespace fs = std::filesystem;

// Example ~/.solarshrc configuration file:
//
//   # Solar shell configuration
//   title = Solar Server BBS
//   info = Powered by sunlight
//   info = ToorCamp 2025
//   logbook = /srv/guestbook/logbook
//   messages = /srv/shell/messages
//   guestbook-bin = /usr/lib/cgi-bin/guestbook
//   location = ToorCamp
//   max-entries = 20
//   width = 80
//
// Lines starting with '#' (including indented comments) are ignored.
// The 'info' key may appear multiple times; values are appended.
// All other keys use the last value if repeated.

// Trim leading and trailing whitespace from a string in place.
inline void trim_config_field(std::string &s) {
    auto start = s.find_first_not_of(" \t\r\n");
    auto end   = s.find_last_not_of(" \t\r\n");
    s = (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

// Parse a simple key = value config file from any input stream.  Lines whose
// first non-whitespace character is '#' are comments.  Keys match CLI
// argument names (without leading dashes).  The 'info' key may appear
// multiple times (values are appended).
inline shell_config parse_config_stream(std::istream &input) {
    shell_config config{};

    std::string line;
    while (std::getline(input, line)) {
        trim_config_field(line);

        // Skip empty lines and comments (including indented comments).
        if (line.empty() || line[0] == '#') continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        auto key   = line.substr(0, eq);
        auto value = line.substr(eq + 1);
        trim_config_field(key);
        trim_config_field(value);

        if (key == "title")              config.banner_title = value;
        else if (key == "info")          config.banner_info.push_back(value);
        else if (key == "logbook")       config.logbook_dir = value;
        else if (key == "messages")      config.messages_dir = value;
        else if (key == "guestbook-bin") config.guestbook_bin = value;
        else if (key == "location")      config.location = value;
        else if (key == "max-entries") {
            try { config.max_entries = std::stoi(value); }
            catch (...) { /* keep default */ }
        }
        else if (key == "width") {
            try { config.width = std::stoi(value); }
            catch (...) { /* keep default */ }
        }
    }

    return config;
}

// Parse a config file from disk by delegating to the stream-based helper.
inline shell_config parse_config_file(const std::string &path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return {};
    }
    return parse_config_stream(file);
}

// Resolve the default config file path from a provided HOME directory.
inline std::string resolve_config_path_from_home(const char *home) {
    if (home == nullptr) {
        return "";
    }
    auto path = fs::path(home) / ".solarshrc";
    if (fs::exists(path)) {
        return path.string();
    }
    return "";
}

// Resolve the config file path: use --config if given, otherwise ~/.solarshrc.
inline std::string resolve_config_path() {
    return resolve_config_path_from_home(std::getenv("HOME"));
}
