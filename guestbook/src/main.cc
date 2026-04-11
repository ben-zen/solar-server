// Copyright (C) Ben Lewis, 2024.
// SPDX-License-Identifier: MIT

#include "guestbook.hh"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <ranges>
#include <string>
#include <string_view>
#include <map>

#include <strings.h>
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

const char *http_content_type{"CONTENT_TYPE"};

int main(int argc, char **argv) {

    auto &err = std::cout;

    std::string author{};
    std::string location{};
    std::string message{};

    // auto content_length = getenv("CONTENT_LENGTH");
    // if (content_length == nullptr) {
    //     // Not running as a CGI script.
    //     err << "Not running as CGI. Exit for now." << std::endl;
    //     return 1;
    // }

    // err << "Running as CGI. Continue." << std::endl;

    std::vector<const char*> arg_strs{argv, argv + argc};

    std::cout << fmt::format("{}", arg_strs) << std::endl << std::endl;

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
        std::exit(1);
    }

    err << "Environment variables:" << std::endl;
    auto env_vars = get_env_vars();
    std::cout << fmt::format("{}", fmt::join(env_vars, "\n")) << std::endl << std::endl;

    if (argc > 1)
    {
        try {
            author = arg_parser.get<std::string>("--author");
            location = arg_parser.get<std::string>("--location");
            message = arg_parser.get<std::string>("--message");
            
            format_entry(author, location, message, std::cout);
            
            err << "If the message is incomplete, try wrapping it in quotes!" << std::endl << std::endl;

            std::exit(0);
        } catch (const std::exception &e) {
            std::cerr << "Exception reading variables: " << e.what() << std::endl;
            std::exit(2);
        }
    }

    err << "Not using arguments, trying std::cin" << std::endl;

    std::string content{};

    std::cin >> content;

    err << content << std::endl;

    //if (env_vars.contains(http_content_type) && env_vars[http_content_type].compare("application/x-www-form-urlencoded") == 0)
    {
        err << "Parse input into a useful structure." << std::endl;
        auto form_data = unpack_form_data(content);

        err << fmt::format("{}", fmt::join(form_data, "\n")) << std::endl << std::endl;

        author = form_data["name"];
        location = form_data["location"];
        message = form_data["message"];
    }

    // Open the log file.

    if (env_vars.contains("LOGBOOK") && std::filesystem::exists(env_vars["LOGBOOK"])) {
        err << "Writing to the logbook under " << env_vars["LOGBOOK"] << std::endl;
        std::filesystem::path logbook_path {env_vars["LOGBOOK"]+"/logbook.current.log"};
        std::fstream logbook_stream{};
        logbook_stream.open(logbook_path, std::ios_base::in | std::ios_base::out | std::ios_base::app);

        format_entry(author, location, message, logbook_stream);
    }

    format_entry(author, location, message, std::cout);

    std::exit(0);
}