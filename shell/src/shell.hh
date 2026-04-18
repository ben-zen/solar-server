// Copyright (C) Ben Lewis, 2025.
// SPDX-License-Identifier: MIT

#pragma once

#include "command.hh"
#include "renderer.hh"

#include <memory>
#include <string>
#include <vector>

// Configuration for the BBS shell.
struct shell_config {
    // Banner title displayed at the top of the screen.
    std::string banner_title = "Solar Server BBS";

    // Extra info lines shown below the banner title.
    std::vector<std::string> banner_info{};

    // Path to the guestbook logbook directory.
    std::string logbook_dir = "/srv/guestbook/logbook";

    // Path to the sysop messages directory.
    std::string messages_dir = "/srv/shell/messages";

    // Path to the guestbook binary (invoked via fork/exec for signing).
    std::string guestbook_bin = "guestbook";

    // Default location pre-filled in the guestbook sign prompt.
    std::string location{};

    // Maximum number of recent guestbook entries to display.
    int max_entries = 10;

    // Default terminal width for formatting (0 = auto-detect).
    int width = 0;
};

// The main BBS shell.
//
// Owns a renderer (presentation) and a set of commands (business logic).
// The run() method enters the interactive command loop.
class shell {
public:
    shell(std::unique_ptr<renderer> rend, shell_config config);

    // Register an additional command.  Commands are displayed in the order
    // they are added.
    void add_command(command cmd);

    // Enter the interactive command loop.  Returns when the user quits.
    void run();

    // Returns a reference to the running flag.  Intended for constructing
    // a quit command that can stop the shell from outside.
    bool &running_flag() { return running_; }

private:
    std::unique_ptr<renderer> renderer_;
    shell_config config_;
    std::vector<command> commands_;
    bool running_ = true;
};
