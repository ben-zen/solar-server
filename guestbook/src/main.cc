// Copyright (C) Ben Lewis, 2024.
// SPDX-License-Identifier: MIT

#include <chrono>
#include <format>
#include <iostream>
#include <ranges>
#include <string>

#include <json.hpp>

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
        {"time", std::format("{:%F, %T}", timestamp)}
    };

    output << front_matter << std::endl << std::endl << message << std::endl;

    return 0;
}

int main(int argc, char **argv) {

    // auto content_length = getenv("CONTENT_LENGTH");
    // if (content_length == nullptr) {
    //     // Not running as a CGI script.
    //     std::cerr << "Not running as CGI. Exit for now." << std::endl;
    //     return 1;
    // }

    // std::cerr << "Running as CGI. Continue." << std::endl;

    std::cerr << std::format("Received {} arguments: ", argc);
    for (int i = 0; i < argc; i++) {
        std::cerr << argv[i] << ", ";
    }
    std::cerr << std::endl;

    if (argc < 4) {
        std::cerr << "Needs three arguments (author, location, message) to build a guestbook entry." << std::endl;
        return 1;
    }

    format_entry(argv[1], argv[2], argv[3], std::cout);

    std::cerr << "If the message is incomplete, try wrapping it in quotes!" << std::endl;

    return 0;
}