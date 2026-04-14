// Copyright (C) Ben Lewis, 2025.
// SPDX-License-Identifier: MIT

#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

// A guestbook entry as read from a logbook file.
struct guestbook_entry {
    std::string author;
    std::string location;
    std::string date;
    std::string message;
};

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

    // Prompt the user for a single line of input and return it.
    // Returns an empty string on EOF / disconnect.
    virtual std::string prompt(const std::string &prompt_text) = 0;

    // Returns true when the underlying input stream has reached EOF.
    virtual bool at_eof() const = 0;

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
