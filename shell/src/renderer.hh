// Copyright (C) Ben Lewis, 2025.
// SPDX-License-Identifier: MIT

#pragma once

#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

// A guestbook entry as read from a logbook file.
struct guestbook_entry {
    std::string author;
    std::string location;
    std::string date;
    std::string message;
};

// Response states returned by renderer::prompt().
struct prompt_eof {};        // Clean end-of-input (e.g., Ctrl-D).
struct prompt_disconnect {}; // I/O error or broken connection.
struct prompt_interrupt {};  // Signal interrupt (e.g., Ctrl-C / SIGINT).

// The result of a prompt operation: either the user's input string or a
// terminal state indicating that the session has ended.
using prompt_result = std::variant<std::string, prompt_eof, prompt_disconnect,
                                   prompt_interrupt>;

// Abstract renderer interface.
//
// This separates the presentation layer from command logic so that the
// terminal implementation can be replaced (e.g., with a full-screen TUI
// library) without touching any business logic.
class renderer {
public:
    virtual ~renderer() = default;

    // Clear the terminal screen.
    virtual void clear_screen() = 0;

    // Display a welcome banner with a title and optional info lines.
    virtual void show_banner(const std::string &title,
                             const std::vector<std::string> &info_lines) = 0;

    // Display a numbered menu of items.  Each item is a (key, description)
    // pair shown to the user.
    virtual void show_menu(
        const std::vector<std::pair<std::string, std::string>> &items) = 0;

    // Prompt the user for a single line of input.  Returns the input string
    // on success, prompt_eof on a clean end-of-input, or prompt_disconnect
    // if the underlying stream encountered an I/O error.
    virtual prompt_result prompt(const std::string &prompt_text) = 0;

    // Display a list of guestbook entries.
    virtual void show_entries(const std::vector<guestbook_entry> &entries) = 0;

    // Display a block of plain text (e.g., an admin message).
    virtual void show_text(const std::string &text) = 0;

    // Display an error message.
    virtual void show_error(const std::string &error) = 0;

    // Print a visual separator line.
    virtual void show_separator() = 0;

    // Print a blank line for spacing.
    virtual void show_blank_line() = 0;
};
