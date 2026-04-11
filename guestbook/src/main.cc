// Copyright (C) Ben Lewis, 2024.
// SPDX-License-Identifier: MIT

#include "guestbook.hh"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <map>

#include <unistd.h>

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <argparse.hpp>

auto &err = std::cerr;

std::map<std::string, std::string> get_env_vars() {
    std::map<std::string, std::string> vars{};
    char **c_env = environ;
    while (*c_env != nullptr) {
        auto *separator = strchr(*c_env, '=');
        vars.emplace(std::string{*c_env, separator}, std::string{(separator + 1)});
        c_env++;
    }

    return vars;
}

void write_to_logbook(const std::string &author,
                      const std::string &location,
                      const std::string &message,
                      const std::map<std::string, std::string> &env_vars) {
    if (!env_vars.contains("LOGBOOK")) {
        err << fmt::format("write_to_logbook: LOGBOOK environment variable is not set\n");
        return;
    }

    const auto &logbook_dir = env_vars.at("LOGBOOK");

    if (!std::filesystem::exists(logbook_dir)) {
        err << fmt::format("write_to_logbook: LOGBOOK directory does not exist: {}\n", logbook_dir);
        return;
    }

    std::filesystem::path logbook_path = std::filesystem::path(logbook_dir) / "logbook.current.log";
    std::ofstream logbook_stream(logbook_path, std::ios::app);

    if (!logbook_stream.is_open()) {
        err << fmt::format("write_to_logbook: failed to open logbook file: {}\n", logbook_path.string());
        return;
    }

    format_entry(author, location, message, logbook_stream);

    if (!logbook_stream) {
        err << fmt::format("write_to_logbook: error writing to logbook file: {}\n", logbook_path.string());
    }
}

int main(int argc, char **argv) {

    auto env_vars = get_env_vars();

    // Detect CGI mode via CONTENT_LENGTH environment variable.
    const char *content_length_env = getenv("CONTENT_LENGTH");
    auto content_length = parse_content_length(content_length_env);

    // If CONTENT_LENGTH is set but invalid/too large, return a CGI error.
    if (content_length_env != nullptr && !content_length.has_value()) {
        err << fmt::format("CGI error: invalid or oversized CONTENT_LENGTH: {}\n", content_length_env);
        std::cout << generate_cgi_error(HttpStatus::payload_too_large,
                                        fmt::format("CONTENT_LENGTH value '{}' is invalid or exceeds the {} byte limit",
                                                    content_length_env, max_content_length));
        return 1;
    }

    if (content_length.has_value()) {
        // Running as a CGI script.

        if (content_length.value() == 0) {
            err << fmt::format("CGI error: CONTENT_LENGTH is zero\n");
            std::cout << generate_cgi_error(HttpStatus::bad_request,
                                            "Request body is empty (CONTENT_LENGTH is 0)");
            return 1;
        }

        std::string content = read_cgi_input(std::cin, content_length.value());
        auto form_data = unpack_form_data(content);

        auto validated = validate_form_fields(form_data);
        if (!validated.has_value()) {
            err << fmt::format("CGI error: form validation failed (missing or empty name/message)\n");
            std::cout << generate_cgi_error(HttpStatus::bad_request,
                                            "Missing required fields: both 'name' and 'message' must be provided and non-empty");
            return 1;
        }

        auto &fields = validated.value();
        write_to_logbook(fields["name"], fields["location"], fields["message"], env_vars);

        // Redirect back to the referring page or the site root.
        std::string host = env_vars.contains("HTTP_HOST") ? env_vars["HTTP_HOST"] : "";
        std::string redirect_url = "/";
        if (env_vars.contains("HTTP_REFERER") && !env_vars["HTTP_REFERER"].empty()) {
            redirect_url = sanitize_redirect_target(env_vars["HTTP_REFERER"], host);
        }
        std::cout << generate_cgi_redirect(redirect_url);
        return 0;
    }

    // Not running as CGI — use command-line arguments.
    argparse::ArgumentParser arg_parser{"guestbook", "0.1.0"};
    arg_parser.add_argument("--author").help("The author of the post");
    arg_parser.add_argument<std::string>("--location").help("The author's affiliation or original location");
    arg_parser.add_argument<std::string>("--message").help("The actual message for the guestbook.");
    arg_parser.add_argument<std::string>("-o", "--output").help("The file to write the guestbook entry to.");

    try {
        arg_parser.parse_args(argc, argv);
    } catch (const std::exception &e) {
        err << fmt::format("Exception parsing arguments: {}\n", e.what());
        err << arg_parser << std::endl;
        return 1;
    }

    if (argc > 1) {
        try {
            auto author = truncate_field(arg_parser.get<std::string>("--author"), max_author_length);
            auto location = truncate_field(arg_parser.get<std::string>("--location"), max_location_length);
            auto message = truncate_field(arg_parser.get<std::string>("--message"), max_message_length);

            write_to_logbook(author, location, message, env_vars);
            format_entry(author, location, message, std::cout);
            return 0;
        } catch (const std::exception &e) {
            err << fmt::format("Exception reading variables: {}\n", e.what());
            return 2;
        }
    }

    // Fallback: read from stdin (non-CGI, no arguments).
    std::string content{};
    std::cin >> content;

    if (content.empty()) {
        err << fmt::format("No input provided.\n");
        return 1;
    }

    auto form_data = unpack_form_data(content);
    auto validated = validate_form_fields(form_data);
    if (!validated.has_value()) {
        err << fmt::format("Invalid form data: name and message are required.\n");
        return 1;
    }

    auto &fields = validated.value();
    write_to_logbook(fields["name"], fields["location"], fields["message"], env_vars);
    format_entry(fields["name"], fields["location"], fields["message"], std::cout);

    return 0;
}