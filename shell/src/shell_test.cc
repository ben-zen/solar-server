// Copyright (C) Ben Lewis, 2025.
// SPDX-License-Identifier: MIT

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include "command.hh"
#include "guestbook_io.hh"
#include "renderer.hh"
#include "shell.hh"
#include "text_renderer.hh"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <fmt/format.h>

namespace fs = std::filesystem;

// Standard test timestamp: 2024-07-01T23:25:08 UTC
static const auto test_timestamp =
    std::chrono::sys_days{std::chrono::year{2024}/7/1}
    + std::chrono::hours{23} + std::chrono::minutes{25} + std::chrono::seconds{8};

// ---------------------------------------------------------------------------
// Helper: create a temporary directory that cleans up after itself.
// ---------------------------------------------------------------------------

struct tmp_dir {
    fs::path path;

    tmp_dir() {
        path = fs::temp_directory_path() / "solar-shell-test-XXXXXX";
        // Use a unique name per test run.
        path = fs::temp_directory_path()
             / fmt::format("solar-shell-test-{}", std::chrono::steady_clock::now()
                           .time_since_epoch().count());
        fs::create_directories(path);
    }

    ~tmp_dir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }

    // Non-copyable.
    tmp_dir(const tmp_dir &) = delete;
    tmp_dir &operator=(const tmp_dir &) = delete;
};

// ---------------------------------------------------------------------------
// guestbook_io: write_entry
// ---------------------------------------------------------------------------

TEST_CASE("write_entry creates a file with correct format") {
    tmp_dir dir;
    bool ok = write_entry(dir.path.string(), "Alice", "Seattle", "Hello!", test_timestamp);
    CHECK(ok);

    // Verify a .md file was created.
    int file_count = 0;
    fs::path found;
    for (const auto &entry : fs::directory_iterator(dir.path)) {
        if (entry.path().extension() == ".md") {
            ++file_count;
            found = entry.path();
        }
    }
    CHECK(file_count == 1);

    // Verify the filename contains the timestamp and author.
    auto filename = found.filename().string();
    CHECK(filename.find("2024-07-01T23-25-08") != std::string::npos);
    CHECK(filename.find("alice") != std::string::npos);

    // Verify the content has JSON front matter.
    std::ifstream file(found);
    std::string first_line;
    std::getline(file, first_line);
    CHECK(first_line.find("\"author\"") != std::string::npos);
    CHECK(first_line.find("Alice") != std::string::npos);
    CHECK(first_line.find("\"type\"") != std::string::npos);
    CHECK(first_line.find("guestbook") != std::string::npos);
}

TEST_CASE("write_entry fails for nonexistent directory") {
    bool ok = write_entry("/nonexistent/path", "Bob", "", "Hi", test_timestamp);
    CHECK_FALSE(ok);
}

// ---------------------------------------------------------------------------
// guestbook_io: read_recent_entries
// ---------------------------------------------------------------------------

TEST_CASE("read_recent_entries returns empty for nonexistent directory") {
    auto entries = read_recent_entries("/nonexistent/path", 10);
    CHECK(entries.empty());
}

TEST_CASE("read_recent_entries reads entries written by write_entry") {
    tmp_dir dir;

    // Write two entries at different timestamps.
    auto ts1 = test_timestamp;
    auto ts2 = test_timestamp + std::chrono::hours{1};

    write_entry(dir.path.string(), "Alice", "Seattle", "First!", ts1);
    write_entry(dir.path.string(), "Bob", "Portland", "Second!", ts2);

    auto entries = read_recent_entries(dir.path.string(), 10);
    CHECK(entries.size() == 2);

    // Entries are sorted newest first (by filename).
    CHECK(entries[0].author == "Bob");
    CHECK(entries[0].message == "Second!");
    CHECK(entries[1].author == "Alice");
    CHECK(entries[1].message == "First!");
}

TEST_CASE("read_recent_entries respects max_entries limit") {
    tmp_dir dir;

    for (int i = 0; i < 5; ++i) {
        auto ts = test_timestamp + std::chrono::hours{i};
        write_entry(dir.path.string(),
                    fmt::format("User{}", i), "", fmt::format("Msg{}", i), ts);
    }

    auto entries = read_recent_entries(dir.path.string(), 3);
    CHECK(entries.size() == 3);
}

// ---------------------------------------------------------------------------
// text_renderer: banner
// ---------------------------------------------------------------------------

TEST_CASE("text_renderer::show_banner outputs title and info lines") {
    std::ostringstream out;
    std::istringstream in;
    text_renderer r{out, in, 40};

    r.show_banner("Test BBS", {"Line 1", "Line 2"});
    auto output = out.str();

    CHECK(output.find("Test BBS") != std::string::npos);
    CHECK(output.find("Line 1") != std::string::npos);
    CHECK(output.find("Line 2") != std::string::npos);
}

// ---------------------------------------------------------------------------
// text_renderer: show_menu
// ---------------------------------------------------------------------------

TEST_CASE("text_renderer::show_menu outputs all items") {
    std::ostringstream out;
    std::istringstream in;
    text_renderer r{out, in, 40};

    r.show_menu({{"g", "Sign guestbook"}, {"q", "Quit"}});
    auto output = out.str();

    CHECK(output.find("[g]") != std::string::npos);
    CHECK(output.find("Sign guestbook") != std::string::npos);
    CHECK(output.find("[q]") != std::string::npos);
    CHECK(output.find("Quit") != std::string::npos);
}

// ---------------------------------------------------------------------------
// text_renderer: prompt
// ---------------------------------------------------------------------------

TEST_CASE("text_renderer::prompt reads a line from input") {
    std::ostringstream out;
    std::istringstream in{"hello\n"};
    text_renderer r{out, in, 40};

    auto result = r.prompt("Name:");
    CHECK(result == "hello");
    CHECK(out.str().find("Name:") != std::string::npos);
}

// ---------------------------------------------------------------------------
// text_renderer: show_entries
// ---------------------------------------------------------------------------

TEST_CASE("text_renderer::show_entries displays entries") {
    std::ostringstream out;
    std::istringstream in;
    text_renderer r{out, in, 40};

    std::vector<guestbook_entry> entries{
        {"Alice", "Seattle", "2024-07-01", "Hello!"},
        {"Bob", "", "2024-07-02", "Hi there"},
    };

    r.show_entries(entries);
    auto output = out.str();
    CHECK(output.find("Alice") != std::string::npos);
    CHECK(output.find("Seattle") != std::string::npos);
    CHECK(output.find("Hello!") != std::string::npos);
    CHECK(output.find("Bob") != std::string::npos);
    CHECK(output.find("Hi there") != std::string::npos);
}

TEST_CASE("text_renderer::show_entries handles empty list") {
    std::ostringstream out;
    std::istringstream in;
    text_renderer r{out, in, 40};

    r.show_entries({});
    CHECK(out.str().find("no entries yet") != std::string::npos);
}

// ---------------------------------------------------------------------------
// text_renderer: show_error
// ---------------------------------------------------------------------------

TEST_CASE("text_renderer::show_error displays error text") {
    std::ostringstream out;
    std::istringstream in;
    text_renderer r{out, in, 40};

    r.show_error("something broke");
    CHECK(out.str().find("something broke") != std::string::npos);
    CHECK(out.str().find("Error") != std::string::npos);
}

// ---------------------------------------------------------------------------
// command: make_quit_command
// ---------------------------------------------------------------------------

TEST_CASE("make_quit_command sets running to false") {
    bool running = true;
    auto cmd = make_quit_command(running);
    CHECK(cmd.key == "q");

    std::ostringstream out;
    std::istringstream in;
    text_renderer r{out, in};
    cmd.execute(r);

    CHECK_FALSE(running);
    CHECK(out.str().find("Goodbye") != std::string::npos);
}

// ---------------------------------------------------------------------------
// command: make_sign_guestbook_command
// ---------------------------------------------------------------------------

TEST_CASE("make_sign_guestbook_command writes an entry") {
    tmp_dir dir;
    auto cmd = make_sign_guestbook_command(dir.path.string());
    CHECK(cmd.key == "g");

    std::ostringstream out;
    std::istringstream in{"TestUser\nTestCity\nHello world\n"};
    text_renderer r{out, in, 40};

    cmd.execute(r);

    // Verify a file was created.
    int count = 0;
    for (const auto &entry : fs::directory_iterator(dir.path)) {
        if (entry.path().extension() == ".md") ++count;
    }
    CHECK(count == 1);
    CHECK(out.str().find("Thank you") != std::string::npos);
}

TEST_CASE("make_sign_guestbook_command rejects empty name") {
    tmp_dir dir;
    auto cmd = make_sign_guestbook_command(dir.path.string());

    std::ostringstream out;
    std::istringstream in{"\nTestCity\nHello\n"};
    text_renderer r{out, in, 40};

    cmd.execute(r);
    CHECK(out.str().find("Error") != std::string::npos);
    CHECK(out.str().find("Name") != std::string::npos);
}

// ---------------------------------------------------------------------------
// command: make_read_guestbook_command
// ---------------------------------------------------------------------------

TEST_CASE("make_read_guestbook_command displays entries") {
    tmp_dir dir;
    write_entry(dir.path.string(), "Alice", "Seattle", "Hello!", test_timestamp);

    auto cmd = make_read_guestbook_command(dir.path.string(), 10);
    CHECK(cmd.key == "r");

    std::ostringstream out;
    std::istringstream in;
    text_renderer r{out, in, 40};

    cmd.execute(r);
    CHECK(out.str().find("Alice") != std::string::npos);
    CHECK(out.str().find("Hello!") != std::string::npos);
}

// ---------------------------------------------------------------------------
// command: make_read_messages_command
// ---------------------------------------------------------------------------

TEST_CASE("make_read_messages_command displays message files") {
    tmp_dir dir;

    // Create a test message.
    std::ofstream msg(dir.path / "welcome.txt");
    msg << "Welcome to the solar server!" << std::endl;
    msg.close();

    auto cmd = make_read_messages_command(dir.path.string());
    CHECK(cmd.key == "m");

    std::ostringstream out;
    std::istringstream in;
    text_renderer r{out, in, 40};

    cmd.execute(r);
    CHECK(out.str().find("welcome") != std::string::npos);
    CHECK(out.str().find("Welcome to the solar server!") != std::string::npos);
}

TEST_CASE("make_read_messages_command handles empty directory") {
    tmp_dir dir;
    auto cmd = make_read_messages_command(dir.path.string());

    std::ostringstream out;
    std::istringstream in;
    text_renderer r{out, in, 40};

    cmd.execute(r);
    CHECK(out.str().find("no messages") != std::string::npos);
}

// ---------------------------------------------------------------------------
// shell: full loop with quit
// ---------------------------------------------------------------------------

TEST_CASE("shell runs and quits on 'q' input") {
    std::ostringstream out;
    std::istringstream in{"q\n"};
    auto rend = std::make_unique<text_renderer>(out, in, 40);

    shell_config config{
        .banner_title = "Test BBS",
        .banner_info  = {"info line"},
        .logbook_dir  = "/tmp",
        .messages_dir = "/tmp",
    };

    shell sh{std::move(rend), config};
    sh.add_command(make_quit_command(sh.running_flag()));

    sh.run();

    auto output = out.str();
    CHECK(output.find("Test BBS") != std::string::npos);
    CHECK(output.find("Goodbye") != std::string::npos);
}

TEST_CASE("shell handles unknown commands gracefully") {
    std::ostringstream out;
    std::istringstream in{"x\nq\n"};
    auto rend = std::make_unique<text_renderer>(out, in, 40);

    shell_config config{.banner_title = "Test"};

    shell sh{std::move(rend), config};
    sh.add_command(make_quit_command(sh.running_flag()));

    sh.run();

    auto output = out.str();
    CHECK(output.find("Unknown command") != std::string::npos);
    CHECK(output.find("Goodbye") != std::string::npos);
}
