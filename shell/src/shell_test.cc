// Copyright (C) Ben Lewis, 2025.
// SPDX-License-Identifier: MIT

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include "command.hh"
#include "config.hh"
#include "guestbook_io.hh"
#include "renderer.hh"
#include "shell.hh"
#include "text_renderer.hh"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include <fmt/format.h>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Helper: create a temporary directory that cleans up after itself.
// ---------------------------------------------------------------------------

struct tmp_dir {
    fs::path path;

    tmp_dir() {
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
// Helper: write a guestbook entry file manually for testing.
// This mirrors the format produced by the guestbook binary without
// depending on it.
// ---------------------------------------------------------------------------

static void create_test_entry(const fs::path &dir,
                              const std::string &filename,
                              const std::string &author,
                              const std::string &location,
                              const std::string &date,
                              const std::string &message) {
    std::ofstream out(dir / filename);
    out << fmt::format(R"({{"author":"{}","date":"{}","location":"{}","type":"guestbook"}})",
                       author, date, location)
        << "\n\n" << message << "\n";
}

// ---------------------------------------------------------------------------
// guestbook_io: read_recent_entries
// ---------------------------------------------------------------------------

TEST_CASE("read_recent_entries returns empty for nonexistent directory") {
    auto entries = read_recent_entries("/nonexistent/path", 10);
    CHECK(entries.empty());
}

TEST_CASE("read_recent_entries reads manually-created entries") {
    tmp_dir dir;

    create_test_entry(dir.path, "2024-07-01T23-25-08_alice.md",
                      "Alice", "Seattle", "2024-07-01T23:25:08", "First!");
    create_test_entry(dir.path, "2024-07-02T00-25-08_bob.md",
                      "Bob", "Portland", "2024-07-02T00:25:08", "Second!");

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
        create_test_entry(dir.path,
                          fmt::format("2024-07-0{}T12-00-00_user.md", i + 1),
                          fmt::format("User{}", i), "", fmt::format("2024-07-0{}T12:00:00", i + 1),
                          fmt::format("Msg{}", i));
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
    REQUIRE(std::holds_alternative<std::string>(result));
    CHECK(std::get<std::string>(result) == "hello");
    CHECK(out.str().find("Name:") != std::string::npos);
}

TEST_CASE("text_renderer::prompt returns prompt_eof on stream exhaustion") {
    std::ostringstream out;
    std::istringstream in{""};
    text_renderer r{out, in, 40};

    auto result = r.prompt(">");
    CHECK(std::holds_alternative<prompt_eof>(result));
}

// ---------------------------------------------------------------------------
// text_renderer: show_entries
// ---------------------------------------------------------------------------

TEST_CASE("text_renderer::show_entries displays entries with separators") {
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
    // Visual separator after each entry.
    CHECK(output.find("---") != std::string::npos);
}

TEST_CASE("text_renderer::show_entries handles empty list") {
    std::ostringstream out;
    std::istringstream in;
    text_renderer r{out, in, 40};

    r.show_entries({});
    CHECK(out.str().find("no entries yet") != std::string::npos);
}

TEST_CASE("text_renderer::show_entries strips ANSI escape sequences from entries") {
    std::ostringstream out;
    std::istringstream in;
    text_renderer r{out, in, 40};

    // Entry with embedded ESC sequence (ANSI injection attempt).
    std::vector<guestbook_entry> entries{
        {"Evil\033[31mUser", "", "2024-07-01", "Injected\033[0m text"},
    };

    r.show_entries(entries);
    auto output = out.str();
    // The ESC character (0x1B) should be stripped from the untrusted fields.
    // The only ESC sequences should be from our own ANSI formatting.
    CHECK(output.find("EvilUser") != std::string::npos);
    CHECK(output.find("Injected text") != std::string::npos);
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

TEST_CASE("make_sign_guestbook_command rejects empty name") {
    auto cmd = make_sign_guestbook_command("/nonexistent/bin", "/tmp");
    CHECK(cmd.key == "g");

    std::ostringstream out;
    std::istringstream in{"\nTestCity\nHello\n"};
    text_renderer r{out, in, 40};

    cmd.execute(r);
    CHECK(out.str().find("Error") != std::string::npos);
    CHECK(out.str().find("Name") != std::string::npos);
}

TEST_CASE("make_sign_guestbook_command rejects empty message") {
    auto cmd = make_sign_guestbook_command("/nonexistent/bin", "/tmp");

    std::ostringstream out;
    std::istringstream in{"TestUser\nTestCity\n\n"};
    text_renderer r{out, in, 40};

    cmd.execute(r);
    CHECK(out.str().find("Error") != std::string::npos);
    CHECK(out.str().find("Message") != std::string::npos);
}

// ---------------------------------------------------------------------------
// command: make_read_guestbook_command
// ---------------------------------------------------------------------------

TEST_CASE("make_read_guestbook_command displays entries") {
    tmp_dir dir;
    create_test_entry(dir.path, "2024-07-01T23-25-08_alice.md",
                      "Alice", "Seattle", "2024-07-01T23:25:08", "Hello!");

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

TEST_CASE("shell handles case-insensitive commands") {
    std::ostringstream out;
    std::istringstream in{"Q\n"};
    auto rend = std::make_unique<text_renderer>(out, in, 40);

    shell_config config{.banner_title = "Test"};

    shell sh{std::move(rend), config};
    sh.add_command(make_quit_command(sh.running_flag()));

    sh.run();

    CHECK(out.str().find("Goodbye") != std::string::npos);
}

TEST_CASE("shell exits cleanly on EOF") {
    std::ostringstream out;
    std::istringstream in{""};
    auto rend = std::make_unique<text_renderer>(out, in, 40);

    shell_config config{.banner_title = "Test"};

    shell sh{std::move(rend), config};
    sh.add_command(make_quit_command(sh.running_flag()));

    sh.run();

    // Should not spin — should exit cleanly via prompt_eof.
    CHECK_FALSE(sh.running_flag());
}

TEST_CASE("shell prompts user to enter a letter on empty input") {
    std::ostringstream out;
    // Empty line followed by quit.
    std::istringstream in{"\nq\n"};
    auto rend = std::make_unique<text_renderer>(out, in, 40);

    shell_config config{.banner_title = "Test"};

    shell sh{std::move(rend), config};
    sh.add_command(make_quit_command(sh.running_flag()));

    sh.run();

    auto output = out.str();
    CHECK(output.find("enter a letter") != std::string::npos);
    CHECK(output.find("Goodbye") != std::string::npos);
}

// ---------------------------------------------------------------------------
// config: parse_config_stream
// ---------------------------------------------------------------------------

TEST_CASE("parse_config_stream parses basic key=value pairs") {
    std::istringstream input{
        "title = My BBS\n"
        "logbook = /var/log/guestbook\n"
        "messages = /var/sysop/messages\n"
        "guestbook-bin = /usr/local/bin/guestbook\n"
    };

    auto config = parse_config_stream(input);
    CHECK(config.banner_title == "My BBS");
    CHECK(config.logbook_dir == "/var/log/guestbook");
    CHECK(config.messages_dir == "/var/sysop/messages");
    CHECK(config.guestbook_bin == "/usr/local/bin/guestbook");
}

TEST_CASE("parse_config_stream handles repeated info keys") {
    std::istringstream input{
        "info = Line one\n"
        "info = Line two\n"
        "info = Line three\n"
    };

    auto config = parse_config_stream(input);
    REQUIRE(config.banner_info.size() == 3);
    CHECK(config.banner_info[0] == "Line one");
    CHECK(config.banner_info[1] == "Line two");
    CHECK(config.banner_info[2] == "Line three");
}

TEST_CASE("parse_config_stream ignores comments and blank lines") {
    std::istringstream input{
        "# This is a comment\n"
        "\n"
        "  # Indented comment\n"
        "title = Visible\n"
    };

    auto config = parse_config_stream(input);
    CHECK(config.banner_title == "Visible");
}

TEST_CASE("parse_config_stream ignores unknown keys") {
    std::istringstream input{
        "title = Known\n"
        "unknown_key = some value\n"
        "also-unknown = other value\n"
    };

    auto config = parse_config_stream(input);
    CHECK(config.banner_title == "Known");
    // Unknown keys should not affect defaults.
    CHECK(config.logbook_dir == "/srv/guestbook/logbook");
}

TEST_CASE("parse_config_stream trims whitespace around keys and values") {
    std::istringstream input{
        "  title  =  Trimmed Title  \n"
        "  logbook  =  /some/path  \n"
    };

    auto config = parse_config_stream(input);
    CHECK(config.banner_title == "Trimmed Title");
    CHECK(config.logbook_dir == "/some/path");
}

TEST_CASE("resolve_config_path_from_home returns empty for null home") {
    CHECK(resolve_config_path_from_home(nullptr).empty());
}

TEST_CASE("resolve_config_path_from_home returns empty for nonexistent dotfile") {
    tmp_dir dir;
    // No .solarshrc exists in the temp directory.
    CHECK(resolve_config_path_from_home(dir.path.string().c_str()).empty());
}

TEST_CASE("resolve_config_path_from_home finds existing dotfile") {
    tmp_dir dir;
    std::ofstream dotfile(dir.path / ".solarshrc");
    dotfile << "title = Found\n";
    dotfile.close();

    auto path = resolve_config_path_from_home(dir.path.string().c_str());
    CHECK_FALSE(path.empty());
    CHECK(path.find(".solarshrc") != std::string::npos);
}

// ---------------------------------------------------------------------------
// config: location, max-entries, width keys
// ---------------------------------------------------------------------------

TEST_CASE("parse_config_stream parses location key") {
    std::istringstream input{"location = ToorCamp\n"};
    auto config = parse_config_stream(input);
    CHECK(config.location == "ToorCamp");
}

TEST_CASE("parse_config_stream parses max-entries key") {
    std::istringstream input{"max-entries = 25\n"};
    auto config = parse_config_stream(input);
    CHECK(config.max_entries == 25);
}

TEST_CASE("parse_config_stream parses width key") {
    std::istringstream input{"width = 80\n"};
    auto config = parse_config_stream(input);
    CHECK(config.width == 80);
}

TEST_CASE("parse_config_stream handles invalid numeric values gracefully") {
    std::istringstream input{
        "max-entries = not_a_number\n"
        "width = abc\n"
    };
    auto config = parse_config_stream(input);
    // Should keep defaults on parse failure.
    CHECK(config.max_entries == 10);
    CHECK(config.width == 0);
}

// ---------------------------------------------------------------------------
// command: make_sign_guestbook_command with default location
// ---------------------------------------------------------------------------

TEST_CASE("make_sign_guestbook_command shows default location in prompt") {
    auto cmd = make_sign_guestbook_command("/nonexistent/bin", "/tmp",
                                           "ToorCamp");

    std::ostringstream out;
    std::istringstream in{"TestUser\n\nHello!\n"};
    text_renderer r{out, in, 40};

    cmd.execute(r);
    // The location prompt should show the default.
    CHECK(out.str().find("ToorCamp") != std::string::npos);
}

// ---------------------------------------------------------------------------
// prompt_result variant: EOF handling in sign command
// ---------------------------------------------------------------------------

TEST_CASE("make_sign_guestbook_command aborts on EOF during name prompt") {
    auto cmd = make_sign_guestbook_command("/nonexistent/bin", "/tmp");

    std::ostringstream out;
    std::istringstream in{""};  // EOF immediately.
    text_renderer r{out, in, 40};

    cmd.execute(r);
    // Should not crash or show "Thank you" — just abort silently.
    CHECK(out.str().find("Thank you") == std::string::npos);
}
