// Copyright (C) Ben Lewis, 2025.
// SPDX-License-Identifier: MIT

#include "shell.hh"

#include <algorithm>
#include <cctype>
#include <variant>

// Case-insensitive string comparison.
static std::string to_lower(const std::string &s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

shell::shell(std::unique_ptr<renderer> rend, shell_config config)
    : renderer_{std::move(rend)}, config_{std::move(config)} {}

void shell::add_command(command cmd) {
    commands_.push_back(std::move(cmd));
}

void shell::run() {
    while (running_) {
        renderer_->clear_screen();
        renderer_->show_banner(config_.banner_title, config_.banner_info);

        // Build the menu from the current command list.
        std::vector<std::pair<std::string, std::string>> menu_items;
        for (const auto &cmd : commands_) {
            menu_items.emplace_back(cmd.key, cmd.label);
        }

        // Push the menu towards the bottom of the screen.
        // Banner lines: 2 borders + 1 title + info lines.
        // Menu lines: 1 blank + items + 1 blank.
        // Prompt line: 1.
        if (config_.height > 0) {
            int banner_lines = 3 + static_cast<int>(config_.banner_info.size());
            int menu_lines = static_cast<int>(commands_.size()) + 2;
            int prompt_lines = 1;
            int used = banner_lines + menu_lines + prompt_lines;
            int padding = std::max(0, config_.height - used);
            for (int i = 0; i < padding; ++i) {
                renderer_->show_blank_line();
            }
        }

        renderer_->show_menu(menu_items);

        auto result = renderer_->prompt(">");

        if (std::holds_alternative<prompt_interrupt>(result)) {
            running_ = false;
            break;
        }

        if (std::holds_alternative<prompt_eof>(result) ||
            std::holds_alternative<prompt_disconnect>(result)) {
            running_ = false;
            break;
        }

        auto choice = std::get<std::string>(result);

        // Trim leading/trailing whitespace so " q " still matches "q".
        auto start = choice.find_first_not_of(" \t");
        auto end   = choice.find_last_not_of(" \t");
        choice = (start == std::string::npos) ? "" : choice.substr(start, end - start + 1);

        if (choice.empty()) {
            renderer_->show_error("Please enter a letter to choose a command.");
            continue;
        }

        auto lower_choice = to_lower(choice);
        auto it = std::find_if(commands_.begin(), commands_.end(),
            [&lower_choice](const command &cmd) {
                return to_lower(cmd.key) == lower_choice;
            });

        if (it != commands_.end()) {
            it->execute(*renderer_);
        } else {
            renderer_->show_error("Unknown command. Please try again.");
        }
    }
}
