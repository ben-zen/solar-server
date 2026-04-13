// Copyright (C) Ben Lewis, 2025.
// SPDX-License-Identifier: MIT

#include "shell.hh"

#include <algorithm>

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
            continue;
        }

        // Find the matching command (case-insensitive single-key match).
        auto it = std::find_if(commands_.begin(), commands_.end(),
            [&choice](const command &cmd) {
                // Compare only the first character if the key is a single char.
                if (cmd.key.size() == 1 && choice.size() >= 1) {
                    return std::tolower(static_cast<unsigned char>(cmd.key[0]))
                        == std::tolower(static_cast<unsigned char>(choice[0]));
                }
                return cmd.key == choice;
            });

        if (it != commands_.end()) {
            it->execute(*renderer_);
        } else {
            renderer_->show_error("Unknown command. Please try again.");
        }
    }
}
