// Copyright (C) Ben Lewis, 2025.
// SPDX-License-Identifier: MIT

#include "command.hh"
#include "guestbook_io.hh"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
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

// ---------------------------------------------------------------------------
// Sign the guestbook
// ---------------------------------------------------------------------------

command make_sign_guestbook_command(const std::string &guestbook_bin,
                                   const std::string &logbook_dir) {
    return command{
        .key   = "g",
        .label = "Sign the guestbook",
        .execute = [guestbook_bin, logbook_dir](renderer &r) {
            r.show_separator();
            r.show_text("  Sign the Guestbook");
            r.show_separator();

            auto author = r.prompt("  Your name:");
            if (author.empty()) {
                r.show_error("Name is required.");
                return;
            }
            author = truncate(author, max_author_length);

            auto location = r.prompt("  Your location (optional):");
            location = truncate(location, max_location_length);

            auto message = r.prompt("  Your message:");
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

    if (!fs::exists(messages_dir) || !fs::is_directory(messages_dir)) {
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
