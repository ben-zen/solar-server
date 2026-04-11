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
    if (env_vars.contains("LOGBOOK") && std::filesystem::exists(env_vars.at("LOGBOOK"))) {
        std::filesystem::path logbook_path{env_vars.at("LOGBOOK") + "/logbook.current.log"};
        std::fstream logbook_stream{};
        logbook_stream.open(logbook_path, std::ios_base::in | std::ios_base::out | std::ios_base::app);
        format_entry(author, location, message, logbook_stream);
    }
}

int main(int argc, char **argv) {

    auto env_vars = get_env_vars();

    // Detect CGI mode via CONTENT_LENGTH environment variable.
    const char *content_length_env = getenv("CONTENT_LENGTH");
    auto content_length = parse_content_length(content_length_env);

    // If CONTENT_LENGTH is set but invalid/too large, return a CGI error.
    if (content_length_env != nullptr && !content_length.has_value()) {
        std::cout << generate_cgi_error(413, "Payload Too Large");
        return 1;
    }

    if (content_length.has_value()) {
        // Running as a CGI script.

        if (content_length.value() == 0) {
            std::cout << generate_cgi_error(400, "Bad Request");
            return 1;
        }

        std::string content = read_cgi_input(std::cin, content_length.value());
        auto form_data = unpack_form_data(content);

        auto validated = validate_form_fields(form_data);
        if (!validated.has_value()) {
            std::cout << generate_cgi_error(400, "Bad Request");
            return 1;
        }

        auto &fields = validated.value();
        write_to_logbook(fields["name"], fields["location"], fields["message"], env_vars);

        // Redirect back to the referring page or the site root.
        std::string redirect_url = "/";
        if (env_vars.contains("HTTP_REFERER") && !env_vars["HTTP_REFERER"].empty()) {
            redirect_url = env_vars["HTTP_REFERER"];
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
        std::cerr << "Exception parsing arguments: " << e.what() << std::endl;
        std::cerr << arg_parser << std::endl;
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
            std::cerr << "Exception reading variables: " << e.what() << std::endl;
            return 2;
        }
    }

    // Fallback: read from stdin (non-CGI, no arguments).
    std::string content{};
    std::cin >> content;

    if (content.empty()) {
        std::cerr << "No input provided." << std::endl;
        return 1;
    }

    auto form_data = unpack_form_data(content);
    auto validated = validate_form_fields(form_data);
    if (!validated.has_value()) {
        std::cerr << "Invalid form data: name and message are required." << std::endl;
        return 1;
    }

    auto &fields = validated.value();
    write_to_logbook(fields["name"], fields["location"], fields["message"], env_vars);
    format_entry(fields["name"], fields["location"], fields["message"], std::cout);

    return 0;
}