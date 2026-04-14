// Copyright (C) Ben Lewis, 2025.
// SPDX-License-Identifier: MIT

#pragma once

#include "shell.hh"

#include <filesystem>
#include <fstream>
#include <istream>
#include <string>

namespace fs = std::filesystem;

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
