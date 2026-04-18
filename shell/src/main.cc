// Copyright (C) Ben Lewis, 2025.
// SPDX-License-Identifier: MIT

#include "command.hh"
#include "config.hh"
#include "shell.hh"
#include "text_renderer.hh"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

#include <sys/ioctl.h>
#include <unistd.h>

#include <fmt/format.h>
#include <argparse.hpp>

namespace fs = std::filesystem;

// Query the terminal width.  Returns 0 if the width cannot be determined
// (e.g., when stdout is not a terminal).
static int detect_terminal_width() {
    struct winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        return ws.ws_col;
    }
    return 0;
}

// Resolve the effective terminal width.  Priority:
//   1. Explicit CLI/config value (> 0)
//   2. Auto-detected terminal width
//   3. Fallback default (72)
static int resolve_width(int configured) {
    if (configured > 0) return configured;
    int detected = detect_terminal_width();
    return detected > 0 ? detected : 72;
}

// Apply CLI argument overrides to a shell_config.  Only values that were
// explicitly provided on the command line replace the config-file defaults.
static void apply_cli_overrides(shell_config &config,
                                const argparse::ArgumentParser &parser) {
    auto cli_title = parser.get<std::string>("--title");
    if (!cli_title.empty()) config.banner_title = cli_title;

    auto cli_info = parser.get<std::vector<std::string>>("--info");
    if (!cli_info.empty()) config.banner_info = cli_info;

    auto cli_logbook = parser.get<std::string>("--logbook");
    if (!cli_logbook.empty()) config.logbook_dir = cli_logbook;

    auto cli_messages = parser.get<std::string>("--messages");
    if (!cli_messages.empty()) config.messages_dir = cli_messages;

    auto cli_guestbook = parser.get<std::string>("--guestbook-bin");
    if (!cli_guestbook.empty()) config.guestbook_bin = cli_guestbook;

    auto cli_location = parser.get<std::string>("--location");
    if (!cli_location.empty()) config.location = cli_location;

    if (parser.is_used("--max-entries")) {
        config.max_entries = parser.get<int>("--max-entries");
    }

    if (parser.is_used("--width")) {
        config.width = parser.get<int>("--width");
    }
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

    arg_parser.add_argument("--location")
        .help("Default location pre-filled in the guestbook sign prompt")
        .default_value(std::string{""});

    arg_parser.add_argument("--width")
        .help("Terminal width for formatting (0 = auto-detect)")
        .default_value(0)
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

    apply_cli_overrides(config, arg_parser);

    int width = resolve_width(config.width);

    auto rend = std::make_unique<text_renderer>(std::cout, std::cin, width);

    shell sh{std::move(rend), config};
    sh.add_command(make_sign_guestbook_command(config.guestbook_bin,
                                               config.logbook_dir,
                                               config.location));
    sh.add_command(make_read_guestbook_command(config.logbook_dir,
                                               config.max_entries));
    sh.add_command(make_read_messages_command(config.messages_dir));
    sh.add_command(make_quit_command(sh.running_flag()));

    sh.run();

    return 0;
}
