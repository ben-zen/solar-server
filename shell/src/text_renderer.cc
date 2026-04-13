// Copyright (C) Ben Lewis, 2025.
// SPDX-License-Identifier: MIT

#include "text_renderer.hh"

#include <algorithm>
#include <string>

#include <fmt/format.h>

// ANSI escape helpers.
namespace ansi {
    constexpr auto reset  = "\033[0m";
    constexpr auto bold   = "\033[1m";
    constexpr auto dim    = "\033[2m";
    constexpr auto cyan   = "\033[36m";
    constexpr auto yellow = "\033[33m";
    constexpr auto red    = "\033[31m";
    constexpr auto clear  = "\033[2J\033[H";
}

text_renderer::text_renderer(std::ostream &out, std::istream &in, int width)
    : out_{out}, in_{in}, width_{width} {}

void text_renderer::clear_screen() {
    out_ << ansi::clear;
    out_.flush();
}

void text_renderer::show_banner(const std::string &title,
                                const std::vector<std::string> &info_lines) {
    std::string border(static_cast<size_t>(width_), '=');
    out_ << fmt::format("{}{}{}\n", ansi::bold, border, ansi::reset);

    // Centre the title within the border width.
    int padding = std::max(0, (width_ - static_cast<int>(title.size())) / 2);
    out_ << fmt::format("{}{}{:>{}}{}\n",
                        ansi::bold, ansi::cyan,
                        title, padding + static_cast<int>(title.size()),
                        ansi::reset);

    for (const auto &line : info_lines) {
        int lpad = std::max(0, (width_ - static_cast<int>(line.size())) / 2);
        out_ << fmt::format("{}{:>{}}{}\n",
                            ansi::dim,
                            line, lpad + static_cast<int>(line.size()),
                            ansi::reset);
    }

    out_ << fmt::format("{}{}{}\n", ansi::bold, border, ansi::reset);
}

void text_renderer::show_menu(
        const std::vector<std::pair<std::string, std::string>> &items) {
    out_ << '\n';
    for (const auto &[key, desc] : items) {
        out_ << fmt::format("  {}{}[{}]{} {}\n",
                            ansi::bold, ansi::yellow,
                            key, ansi::reset, desc);
    }
    out_ << '\n';
}

std::string text_renderer::prompt(const std::string &prompt_text) {
    out_ << fmt::format("{}{}{} ", ansi::cyan, prompt_text, ansi::reset);
    out_.flush();

    std::string line;
    if (!std::getline(in_, line)) {
        return "";
    }
    return line;
}

void text_renderer::show_entries(const std::vector<guestbook_entry> &entries) {
    if (entries.empty()) {
        out_ << fmt::format("  {}(no entries yet){}\n", ansi::dim, ansi::reset);
        return;
    }

    for (const auto &entry : entries) {
        std::string header = fmt::format("{} from {}", entry.date, entry.author);
        if (!entry.location.empty()) {
            header += fmt::format(" ({})", entry.location);
        }
        out_ << fmt::format("  {}{}{}\n", ansi::dim, header, ansi::reset);
        out_ << fmt::format("  {}\n\n", entry.message);
    }
}

void text_renderer::show_text(const std::string &text) {
    out_ << text << '\n';
}

void text_renderer::show_error(const std::string &error) {
    out_ << fmt::format("{}{}Error: {}{}\n",
                        ansi::bold, ansi::red, error, ansi::reset);
}

void text_renderer::show_separator() {
    std::string line(static_cast<size_t>(width_), '-');
    out_ << fmt::format("{}{}{}\n", ansi::dim, line, ansi::reset);
}

void text_renderer::show_blank_line() {
    out_ << '\n';
}
