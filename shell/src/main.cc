// Copyright (C) Ben Lewis, 2025.
// SPDX-License-Identifier: MIT

#include "command.hh"
#include "shell.hh"
#include "text_renderer.hh"

#include <iostream>
#include <memory>
#include <string>

#include <fmt/format.h>
#include <argparse.hpp>

int main(int argc, char **argv) {
    argparse::ArgumentParser arg_parser{"solar-shell", "0.1.0",
                                        argparse::default_arguments::help};

    arg_parser.add_argument("--logbook")
        .help("Path to the guestbook logbook directory")
        .default_value(std::string{"/srv/guestbook/logbook"});

    arg_parser.add_argument("--messages")
        .help("Path to the sysop messages directory")
        .default_value(std::string{"/srv/shell/messages"});

    arg_parser.add_argument("--title")
        .help("Banner title")
        .default_value(std::string{"Solar Server BBS"});

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

    shell_config config{
        .banner_title = arg_parser.get<std::string>("--title"),
        .banner_info  = arg_parser.get<std::vector<std::string>>("--info"),
        .logbook_dir  = arg_parser.get<std::string>("--logbook"),
        .messages_dir = arg_parser.get<std::string>("--messages"),
    };

    int width       = arg_parser.get<int>("--width");
    int max_entries = arg_parser.get<int>("--max-entries");

    auto rend = std::make_unique<text_renderer>(std::cout, std::cin, width);

    shell sh{std::move(rend), config};
    sh.add_command(make_sign_guestbook_command(config.logbook_dir));
    sh.add_command(make_read_guestbook_command(config.logbook_dir, max_entries));
    sh.add_command(make_read_messages_command(config.messages_dir));
    sh.add_command(make_quit_command(sh.running_flag()));

    sh.run();

    return 0;
}
