// Copyright (C) Ben Lewis, 2025.
// SPDX-License-Identifier: MIT

#include "command.hh"
#include "guestbook_io.hh"
#include "shell.hh"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include <fmt/format.h>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Truncate a string to a maximum length.
// ---------------------------------------------------------------------------

static constexpr size_t max_author_length  = 200;
static constexpr size_t max_location_length = 200;
static constexpr size_t max_message_length  = 2000;

static std::string truncate(const std::string &value, size_t max_len) {
    if (value.size() <= max_len) return value;
    return value.substr(0, max_len);
}

// Helper: extract the string from a prompt_result, or return false to
// signal that the caller should abort (EOF / disconnect).
static bool get_input(prompt_result &result, std::string &out) {
    if (std::holds_alternative<std::string>(result)) {
        out = std::get<std::string>(result);
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Sign the guestbook
// ---------------------------------------------------------------------------

command make_sign_guestbook_command(const std::string &guestbook_bin,
                                   const std::string &logbook_dir,
                                   const std::string &default_location) {
    return command{
        .key   = "g",
        .label = "Sign the guestbook",
        .execute = [guestbook_bin, logbook_dir, default_location](renderer &r) {
            r.show_separator();
            r.show_text("  Sign the Guestbook");
            r.show_separator();

            auto author_result = r.prompt("  Your name:");
            std::string author;
            if (!get_input(author_result, author)) return;
            if (author.empty()) {
                r.show_error("Name is required.");
                return;
            }
            author = truncate(author, max_author_length);

            std::string location_prompt = "  Your location (optional):";
            if (!default_location.empty()) {
                location_prompt = fmt::format("  Your location [{}]:",
                                              default_location);
            }
            auto location_result = r.prompt(location_prompt);
            std::string location;
            if (!get_input(location_result, location)) return;
            if (location.empty() && !default_location.empty()) {
                location = default_location;
            }
            location = truncate(location, max_location_length);

            auto message_result = r.prompt("  Your message:");
            std::string message;
            if (!get_input(message_result, message)) return;
            if (message.empty()) {
                r.show_error("Message is required.");
                return;
            }
            message = truncate(message, max_message_length);

            if (run_guestbook_binary(guestbook_bin, logbook_dir,
                                     author, location, message)) {
                r.show_blank_line();
                r.show_text("  Thank you for signing the guestbook!");
            } else {
                r.show_error("Failed to write guestbook entry.");
            }
        }
    };
}

// ---------------------------------------------------------------------------
// Read recent guestbook entries
// ---------------------------------------------------------------------------

command make_read_guestbook_command(const std::string &logbook_dir,
                                   int max_entries) {
    return command{
        .key   = "r",
        .label = fmt::format("Read recent guestbook entries (last {})", max_entries),
        .execute = [logbook_dir, max_entries](renderer &r) {
            r.show_separator();
            r.show_text("  Recent Guestbook Entries");
            r.show_separator();
            r.show_blank_line();

            auto entries = read_recent_entries(logbook_dir, max_entries);
            r.show_entries(entries);
        }
    };
}

// ---------------------------------------------------------------------------
// Read admin messages
// ---------------------------------------------------------------------------

// Read plain-text or markdown files from the messages directory.
static std::vector<std::pair<std::string, std::string>>
read_messages(const std::string &messages_dir) {
    std::vector<std::pair<std::string, std::string>> messages;

    std::error_code exists_ec;
    if (!fs::exists(messages_dir, exists_ec) || exists_ec) {
        return messages;
    }

    std::error_code is_dir_ec;
    if (!fs::is_directory(messages_dir, is_dir_ec) || is_dir_ec) {
        return messages;
    }

    std::vector<fs::path> paths;
    std::error_code ec;
    for (const auto &entry : fs::directory_iterator(messages_dir, ec)) {
        if (ec) break;
        std::error_code entry_ec;
        if (entry.is_regular_file(entry_ec) && !entry_ec) {
            auto ext = entry.path().extension().string();
            if (ext == ".txt" || ext == ".md") {
                paths.push_back(entry.path());
            }
        }
    }

    // Sort by filename (newest first, assuming timestamp prefixes).
    std::sort(paths.begin(), paths.end(), std::greater<>());

    for (const auto &p : paths) {
        std::ifstream file(p);
        if (!file.is_open()) continue;

        std::ostringstream content;
        content << file.rdbuf();
        messages.emplace_back(p.stem().string(), content.str());
    }

    return messages;
}

command make_read_messages_command(const std::string &messages_dir) {
    return command{
        .key   = "m",
        .label = "Read messages from the sysop",
        .execute = [messages_dir](renderer &r) {
            r.show_separator();
            r.show_text("  Messages from the Sysop");
            r.show_separator();
            r.show_blank_line();

            auto messages = read_messages(messages_dir);
            if (messages.empty()) {
                r.show_text("  (no messages at this time)");
                r.show_blank_line();
                return;
            }

            for (const auto &[title, body] : messages) {
                r.show_text(fmt::format("  --- {} ---", title));
                r.show_text(body);
                r.show_blank_line();
            }
        }
    };
}

// ---------------------------------------------------------------------------
// Quit
// ---------------------------------------------------------------------------

command make_quit_command(bool &running) {
    return command{
        .key   = "q",
        .label = "Quit",
        .execute = [&running](renderer &r) {
            r.show_blank_line();
            r.show_text("  Goodbye! Thanks for visiting the Solar Server.");
            r.show_blank_line();
            running = false;
        }
    };
}

// ---------------------------------------------------------------------------
// Register default commands
// ---------------------------------------------------------------------------

void register_default_commands(shell &sh, const shell_config &config) {
    sh.add_command(make_sign_guestbook_command(config.guestbook_bin,
                                               config.logbook_dir,
                                               config.location));
    sh.add_command(make_read_guestbook_command(config.logbook_dir,
                                               config.max_entries));
    sh.add_command(make_read_messages_command(config.messages_dir));
    sh.add_command(make_quit_command(sh.running_flag()));
}
