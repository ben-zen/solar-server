// Copyright (C) Ben Lewis, 2025.
// SPDX-License-Identifier: MIT

#include "command.hh"
#include "shell.hh"
#include "text_renderer.hh"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include <fmt/format.h>
#include <argparse.hpp>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// .solarshrc configuration file parser
// ---------------------------------------------------------------------------

// Parse a simple key = value config file.  Lines starting with '#' are
// comments.  Keys match CLI argument names (without leading dashes).
// The 'info' key may appear multiple times (values are appended).
static shell_config parse_config_file(const std::string &path) {
    shell_config config{};

    std::ifstream file(path);
    if (!file.is_open()) {
        return config;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments.
        if (line.empty() || line[0] == '#') continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        // Extract and trim key/value.
        auto key = line.substr(0, eq);
        auto value = line.substr(eq + 1);

        // Trim whitespace.
        auto trim = [](std::string &s) {
            auto start = s.find_first_not_of(" \t");
            auto end   = s.find_last_not_of(" \t");
            s = (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
        };
        trim(key);
        trim(value);

        if (key == "title")         config.banner_title = value;
        else if (key == "info")     config.banner_info.push_back(value);
        else if (key == "logbook")  config.logbook_dir = value;
        else if (key == "messages") config.messages_dir = value;
        else if (key == "guestbook-bin") config.guestbook_bin = value;
    }

    return config;
}

// Resolve the config file path: use --config if given, otherwise ~/.solarshrc.
static std::string resolve_config_path() {
    const char *home = std::getenv("HOME");
    if (home != nullptr) {
        auto path = fs::path(home) / ".solarshrc";
        if (fs::exists(path)) {
            return path.string();
        }
    }
    return "";
}

int main(int argc, char **argv) {
    argparse::ArgumentParser arg_parser{"solar-shell", "0.1.0",
                                        argparse::default_arguments::help};

    arg_parser.add_argument("--config")
        .help("Path to configuration file (default: ~/.solarshrc)")
        .default_value(std::string{""});

    arg_parser.add_argument("--logbook")
        .help("Path to the guestbook logbook directory")
        .default_value(std::string{""});

    arg_parser.add_argument("--messages")
        .help("Path to the sysop messages directory")
        .default_value(std::string{""});

    arg_parser.add_argument("--guestbook-bin")
        .help("Path to the guestbook binary")
        .default_value(std::string{""});

    arg_parser.add_argument("--title")
        .help("Banner title")
        .default_value(std::string{""});

    arg_parser.add_argument("--info")
        .help("Extra info line for the banner (may be repeated)")
        .append()
        .default_value(std::vector<std::string>{});

    arg_parser.add_argument("--width")
        .help("Terminal width for formatting")
        .default_value(72)
        .scan<'i', int>();

    arg_parser.add_argument("--max-entries")
        .help("Maximum number of recent guestbook entries to display")
        .default_value(10)
        .scan<'i', int>();

    try {
        arg_parser.parse_args(argc, argv);
    } catch (const std::exception &e) {
        std::cerr << fmt::format("Error: {}\n", e.what());
        std::cerr << arg_parser << std::endl;
        return 1;
    }

    // Load configuration: defaults → dotfile → CLI overrides.
    auto config_path = arg_parser.get<std::string>("--config");
    if (config_path.empty()) {
        config_path = resolve_config_path();
    }

    shell_config config{};
    if (!config_path.empty()) {
        config = parse_config_file(config_path);
    }

    // CLI arguments override dotfile values (only when explicitly provided).
    auto cli_title = arg_parser.get<std::string>("--title");
    if (!cli_title.empty()) config.banner_title = cli_title;

    auto cli_info = arg_parser.get<std::vector<std::string>>("--info");
    if (!cli_info.empty()) config.banner_info = cli_info;

    auto cli_logbook = arg_parser.get<std::string>("--logbook");
    if (!cli_logbook.empty()) config.logbook_dir = cli_logbook;

    auto cli_messages = arg_parser.get<std::string>("--messages");
    if (!cli_messages.empty()) config.messages_dir = cli_messages;

    auto cli_guestbook = arg_parser.get<std::string>("--guestbook-bin");
    if (!cli_guestbook.empty()) config.guestbook_bin = cli_guestbook;

    int width       = arg_parser.get<int>("--width");
    int max_entries = arg_parser.get<int>("--max-entries");

    auto rend = std::make_unique<text_renderer>(std::cout, std::cin, width);

    shell sh{std::move(rend), config};
    sh.add_command(make_sign_guestbook_command(config.guestbook_bin,
                                               config.logbook_dir));
    sh.add_command(make_read_guestbook_command(config.logbook_dir, max_entries));
    sh.add_command(make_read_messages_command(config.messages_dir));
    sh.add_command(make_quit_command(sh.running_flag()));

    sh.run();

    return 0;
}
