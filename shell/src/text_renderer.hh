// Copyright (C) Ben Lewis, 2025.
// SPDX-License-Identifier: MIT

#pragma once

#include "renderer.hh"

#include <iostream>
#include <string>

// A plain-text terminal renderer using ANSI escape codes for basic styling.
//
// This implementation targets classic BBS / telnet use cases where the
// connection may be a serial line or a bare telnet session.  It writes to
// the provided output stream (defaults to stdout) and reads a single line
// at a time from the provided input stream (defaults to stdin).
class text_renderer : public renderer {
public:
    explicit text_renderer(std::ostream &out = std::cout,
                           std::istream &in  = std::cin,
                           int width = 72,
                           int height = 24);

    void clear_screen() override;
    void show_banner(const std::string &title,
                     const std::vector<std::string> &info_lines) override;
    void show_menu(
        const std::vector<std::pair<std::string, std::string>> &items) override;
    prompt_result prompt(const std::string &prompt_text) override;
    void show_entries(const std::vector<guestbook_entry> &entries) override;
    void show_text(const std::string &text) override;
    void show_error(const std::string &error) override;
    void show_separator() override;
    void show_blank_line() override;

    // Update the terminal width (e.g., after a SIGWINCH).
    // Values below 1 are ignored to prevent undefined behavior from
    // signed→unsigned conversion when constructing separator strings.
    void set_width(int w) { if (w >= 1) width_ = w; }

    // Update the terminal height.  Values below 1 are ignored.
    void set_height(int h) { if (h >= 1) height_ = h; }

    // Return the current terminal height.
    int height() const { return height_; }

private:
    std::ostream &out_;
    std::istream &in_;
    int width_;
    int height_;
};
