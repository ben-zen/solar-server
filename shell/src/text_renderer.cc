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

// Strip control characters and terminal escape sequences from untrusted
// input to prevent terminal injection.  Handles CSI (ESC [), OSC (ESC ]),
// DCS (ESC P), PM (ESC ^), APC (ESC _), SOS (ESC X), SS3 (ESC O), and
// other two-byte ESC-prefixed functions.  Preserves tab and newline.
static std::string sanitize_terminal_output(const std::string &input) {
    std::string result;
    result.reserve(input.size());

    for (size_t i = 0; i < input.size(); ++i) {
        auto ch = static_cast<unsigned char>(input[i]);

        if (ch == 0x1B) {
            if (i + 1 >= input.size()) {
                continue;  // Drop a trailing bare ESC.
            }

            auto next = static_cast<unsigned char>(input[i + 1]);

            if (next == '[') {
                // CSI: ESC [ parameters/intermediates final-byte
                i += 2;
                while (i < input.size()) {
                    auto seq = static_cast<unsigned char>(input[i]);
                    if (seq >= 0x40 && seq <= 0x7E) {
                        break;  // Consume the final byte as part of the skip.
                    }
                    ++i;
                }
                continue;
            }

            if (next == ']') {
                // OSC: ESC ] ... BEL or ST (ESC \)
                i += 2;
                while (i < input.size()) {
                    auto seq = static_cast<unsigned char>(input[i]);
                    if (seq == 0x07) {
                        break;  // BEL terminator.
                    }
                    if (seq == 0x1B && i + 1 < input.size() &&
                        static_cast<unsigned char>(input[i + 1]) == '\\') {
                        ++i;  // Consume the '\' from ST.
                        break;
                    }
                    ++i;
                }
                continue;
            }

            if (next == 'P' || next == '^' || next == '_' || next == 'X') {
                // DCS / PM / APC / SOS: ESC <type> ... ST (ESC \)
                i += 2;
                while (i < input.size()) {
                    auto seq = static_cast<unsigned char>(input[i]);
                    if (seq == 0x1B && i + 1 < input.size() &&
                        static_cast<unsigned char>(input[i + 1]) == '\\') {
                        ++i;  // Consume the '\' from ST.
                        break;
                    }
                    ++i;
                }
                continue;
            }

            // Consume other ESC-prefixed terminal controls conservatively,
            // including SS3 (ESC O) and other two-byte escape functions.
            ++i;
            continue;
        }

        if (ch >= 0x20 || ch == '\t' || ch == '\n') {
            result.push_back(static_cast<char>(ch));
        }
        // Drop other control characters.
    }
    return result;
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

bool text_renderer::at_eof() const {
    return in_.eof() || in_.fail();
}

void text_renderer::show_entries(const std::vector<guestbook_entry> &entries) {
    if (entries.empty()) {
        out_ << fmt::format("  {}(no entries yet){}\n", ansi::dim, ansi::reset);
        return;
    }

    for (const auto &entry : entries) {
        auto safe_author   = sanitize_terminal_output(entry.author);
        auto safe_location = sanitize_terminal_output(entry.location);
        auto safe_date     = sanitize_terminal_output(entry.date);
        auto safe_message  = sanitize_terminal_output(entry.message);

        std::string header = fmt::format("{} from {}", safe_date, safe_author);
        if (!safe_location.empty()) {
            header += fmt::format(" ({})", safe_location);
        }
        out_ << fmt::format("  {}{}{}\n", ansi::dim, header, ansi::reset);
        out_ << fmt::format("  {}\n", safe_message);
        out_ << fmt::format("  {}---{}\n", ansi::dim, ansi::reset);
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
