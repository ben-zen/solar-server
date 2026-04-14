// Copyright (C) Ben Lewis, 2025.
// SPDX-License-Identifier: MIT

#include "shell.hh"

#include <algorithm>
#include <cctype>

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
    renderer_->clear_screen();
    renderer_->show_banner(config_.banner_title, config_.banner_info);

    while (running_) {
        // Build the menu from the current command list.
        std::vector<std::pair<std::string, std::string>> menu_items;
        for (const auto &cmd : commands_) {
            menu_items.emplace_back(cmd.key, cmd.label);
        }

        renderer_->show_menu(menu_items);

        auto choice = renderer_->prompt(">");

        if (choice.empty()) {
            if (renderer_->at_eof()) {
                running_ = false;
                break;
            }
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
