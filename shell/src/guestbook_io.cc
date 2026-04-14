// Copyright (C) Ben Lewis, 2025.
// SPDX-License-Identifier: MIT

#include "guestbook_io.hh"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>

#include <sys/wait.h>
#include <unistd.h>

#include <fmt/format.h>
#include <json.hpp>

using json = nlohmann::json;
namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Reading entries
// ---------------------------------------------------------------------------

// Parse a single Hugo content file with JSON front matter.
// Expected layout:
//   {"author":"…","date":"…","location":"…","type":"guestbook"}
//
//   Message body text…
static guestbook_entry parse_entry(const fs::path &path) {
    guestbook_entry entry{};

    std::ifstream file(path);
    if (!file.is_open()) {
        return entry;
    }

    // First line is JSON front matter.
    std::string front_matter_line;
    if (!std::getline(file, front_matter_line)) {
        return entry;
    }

    try {
        auto j = json::parse(front_matter_line);
        if (j.contains("author"))   entry.author   = j["author"].get<std::string>();
        if (j.contains("date"))     entry.date     = j["date"].get<std::string>();
        if (j.contains("location")) entry.location = j["location"].get<std::string>();
    } catch (const json::exception &) {
        // Malformed front matter — return what we have.
    }

    // Skip the blank separator line.
    std::string blank;
    std::getline(file, blank);

    // Read the remaining lines as the message body.
    std::ostringstream body;
    std::string line;
    bool first = true;
    while (std::getline(file, line)) {
        if (!first) body << '\n';
        body << line;
        first = false;
    }
    entry.message = body.str();

    return entry;
}

std::vector<guestbook_entry> read_recent_entries(const std::string &logbook_dir,
                                                 int max_entries) {
    std::vector<guestbook_entry> entries;

    if (!fs::exists(logbook_dir) || !fs::is_directory(logbook_dir)) {
        return entries;
    }

    // Collect all .md files in the logbook directory.
    std::vector<fs::path> paths;
    for (const auto &dir_entry : fs::directory_iterator(logbook_dir)) {
        if (dir_entry.is_regular_file() && dir_entry.path().extension() == ".md") {
            paths.push_back(dir_entry.path());
        }
    }

    // Sort descending by filename (filenames are timestamped).
    std::sort(paths.begin(), paths.end(), std::greater<>());

    int count = 0;
    for (const auto &p : paths) {
        if (count >= max_entries) break;
        auto entry = parse_entry(p);
        if (!entry.author.empty() || !entry.message.empty()) {
            entries.push_back(std::move(entry));
            ++count;
        }
    }

    return entries;
}

// ---------------------------------------------------------------------------
// Writing entries via the guestbook binary
// ---------------------------------------------------------------------------

bool run_guestbook_binary(const std::string &guestbook_bin,
                          const std::string &logbook_dir,
                          const std::string &author,
                          const std::string &location,
                          const std::string &message) {
    pid_t pid = fork();

    if (pid < 0) {
        std::cerr << fmt::format("run_guestbook_binary: fork failed\n");
        return false;
    }

    if (pid == 0) {
        // Child process: set LOGBOOK env var and exec the guestbook binary.
        setenv("LOGBOOK", logbook_dir.c_str(), 1);

        execl(guestbook_bin.c_str(), guestbook_bin.c_str(),
              "--author", author.c_str(),
              "--location", location.c_str(),
              "--message", message.c_str(),
              nullptr);

        // If execl returns, it failed.
        std::cerr << fmt::format("run_guestbook_binary: exec failed: {}\n",
                                 guestbook_bin);
        _exit(127);
    }

    // Parent process: wait for the child.
    int status = 0;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        return true;
    }

    std::cerr << fmt::format("run_guestbook_binary: child exited with status {}\n",
                             WIFEXITED(status) ? WEXITSTATUS(status) : -1);
    return false;
}
