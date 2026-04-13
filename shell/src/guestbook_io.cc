// Copyright (C) Ben Lewis, 2025.
// SPDX-License-Identifier: MIT

#include "guestbook_io.hh"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <fmt/chrono.h>
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
// Writing entries
// ---------------------------------------------------------------------------

// Generates a filename for a guestbook entry: YYYY-MM-DDTHH-MM-SS_authorpart.md
// The author part consists of the first 8 alphabetic glyphs from the author
// name, lowercased for filesystem safety.
static std::string generate_entry_filename(
    const std::string &author,
    std::chrono::system_clock::time_point timestamp) {

    std::string author_part;
    for (char ch : author) {
        if (author_part.size() >= 8) break;
        if (std::isalpha(static_cast<unsigned char>(ch))) {
            author_part.push_back(
                static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        }
    }

    auto ts_seconds = std::chrono::floor<std::chrono::seconds>(timestamp);
    return fmt::format("{:%Y-%m-%dT%H-%M-%S}_{}.md", ts_seconds, author_part);
}

bool write_entry(const std::string &logbook_dir,
                 const std::string &author,
                 const std::string &location,
                 const std::string &message,
                 std::chrono::system_clock::time_point timestamp) {
    if (!fs::exists(logbook_dir) || !fs::is_directory(logbook_dir)) {
        std::cerr << fmt::format("write_entry: logbook directory does not exist: {}\n",
                                 logbook_dir);
        return false;
    }

    auto filename = generate_entry_filename(author, timestamp);
    fs::path entry_path = fs::path(logbook_dir) / filename;

    std::ofstream out(entry_path);
    if (!out.is_open()) {
        std::cerr << fmt::format("write_entry: failed to open file: {}\n",
                                 entry_path.string());
        return false;
    }

    auto ts_seconds = std::chrono::floor<std::chrono::seconds>(timestamp);
    json front_matter{
        {"author",   author},
        {"date",     fmt::format("{:%FT%T}", ts_seconds)},
        {"location", location},
        {"type",     "guestbook"}
    };

    out << front_matter << std::endl << std::endl << message << std::endl;

    if (!out) {
        std::cerr << fmt::format("write_entry: error writing to file: {}\n",
                                 entry_path.string());
        return false;
    }

    return true;
}
