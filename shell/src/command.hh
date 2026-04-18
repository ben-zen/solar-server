// Copyright (C) Ben Lewis, 2025.
// SPDX-License-Identifier: MIT

#pragma once

#include "renderer.hh"

#include <functional>
#include <string>
#include <vector>

// A single user-facing command in the BBS shell.
//
// Commands encapsulate one unit of user-visible functionality (e.g., "sign
// the guestbook" or "read admin messages").  New features are added by
// creating additional command instances and registering them with the shell.
struct command {
    // Short key the user types to invoke this command (e.g., "1" or "q").
    std::string key;

    // Human-readable label shown in the menu.
    std::string label;

    // The action to perform.  The function receives a reference to the
    // current renderer so it can produce output in the active presentation
    // style.
    std::function<void(renderer &)> execute;
};

// ---------------------------------------------------------------------------
// Built-in command factories.
//
// Each factory returns a fully configured command struct.  The factories
// capture the paths they need by value, so the caller does not need to keep
// the configuration alive after constructing the command list.
// ---------------------------------------------------------------------------

// Sign the guestbook: prompts for author, location, and message, then
// invokes the guestbook binary via fork/exec to write the entry.
// |default_location| pre-fills the location prompt when non-empty.
command make_sign_guestbook_command(const std::string &guestbook_bin,
                                   const std::string &logbook_dir,
                                   const std::string &default_location = "");

// Read recent guestbook entries from |logbook_dir|.
command make_read_guestbook_command(const std::string &logbook_dir,
                                   int max_entries = 10);

// Read admin messages from |messages_dir|.
command make_read_messages_command(const std::string &messages_dir);

// Quit the shell (sets |running| to false).
command make_quit_command(bool &running);
