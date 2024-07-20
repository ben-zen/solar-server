// Copyright (C) Ben Lewis, 2024.
// SPDX-License-Identifier: MIT

#include <chrono>
#include <iostream>
#include <ranges>
#include <string>

#include <unistd.h>

#include <fmt/chrono.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <json.hpp>
#include <argparse.hpp>

using json = nlohmann::json;

int format_entry(const std::string &author,
                 const std::string &location,
                 const std::string &message,
                 std::ostream &output) {
    // An entry consists of front matter, two newlines, the message body, and a final newline.
    // If this resembles a Hugo document to you ... yes.
    // To that end, we should collect system status (ideally by loading & calling, but I'm not picky)
    
    auto timestamp = std::chrono::system_clock::now();
    
    json front_matter{
        {"params", json{{"author", author},{"location", location}}},
        {"time", fmt::format("{:%F, %T}", timestamp)}
    };

    output << front_matter << std::endl << std::endl << message << std::endl;

    return 0;
}

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
    for(char **env_var = environ; *env_var != nullptr; env_var++)
    {
        auto env_value = std::getenv(*env_var);
        err << '\t' << std::string{*env_var} << ": "
                  << ((env_value != nullptr) ? env_value : "NULL") << std::endl;
    }
    err << std::endl;

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

    std::exit(0);
}