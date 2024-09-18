// Copyright (C) Ben Lewis, 2024.
// SPDX-License-Identifier: MIT

#include <cctype>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <ranges>
#include <string>
#include <string_view>
#include <map>

#include <strings.h>
#include <unistd.h>

#include <fmt/chrono.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <json.hpp>
#include <argparse.hpp>

using json = nlohmann::json;

// The |application/x-www-form-urlencoded percent-encode set| contains all code points except:
// * Alphanumerics
// * U+002A (*), U+002D (-), U+002E (.), and U+005F (_)
bool is_unencoded(unsigned char ch) {
    static std::string exceptions{"*-._"};
    return std::isalnum(ch) || (exceptions.find(ch) != std::string::npos); 
}

// Implements x-www-form-urlencoded from RFC3986 and https://url.spec.whatwg.org/
// with a focus on UTF-8.
std::string urlencode_serlialize(std::string const &str) {
    std::string encoded{};

    for (auto &ch : str) {
        if (is_unencoded(ch)) {
            encoded.push_back(ch);
            continue;
        }
        
        // Percent-encode anything else.
        encoded.append(fmt::format("%{:X}", ch));
    }

    return encoded;
}

std::string urlencode_deserialize(std::string const &enc_str) {
    std::string decoded{};
    auto enc_cursor = enc_str.begin();

    while (enc_cursor != enc_str.end()) {
        if (*enc_cursor != '%') {
            if (is_unencoded(*enc_cursor)){
                decoded.push_back(*enc_cursor);
            } else if (*enc_cursor == '+') {
                decoded.push_back(' ');
            } else {
                std::cerr << "urlencode_deserialize: Encountered unexpected character: " << *enc_cursor << " Writing & continuing."<< std::endl;
                decoded.push_back(*enc_cursor);
            }
            
            enc_cursor++;
            continue;
        }

        // Percent-decode time!
        if ((enc_cursor + 1) == enc_str.end() || (enc_cursor + 2) == enc_str.end()) {
            throw new std::out_of_range{fmt::format("Terminated mid-percent encoding: {}", enc_str)};
        }

        // From this, we know that (enc_cursor + 3) is at most enc_str.end(), which is okay for parsing.

        if (!std::isxdigit(*(enc_cursor + 1)) || !std::isxdigit(*(enc_cursor + 2))) {
            throw new std::invalid_argument{fmt::format("Non-hex character in percent-encoding: {}", enc_str)};
        }

        std::string decode{(enc_cursor + 1), (enc_cursor + 3)};
        std::istringstream decoder{decode};
        short decoded_short{};

        decoder >> std::setbase(16) >> decoded_short;
        decoded.push_back((char) decoded_short);
        enc_cursor += 3;
    }

    return decoded;
}

// packing form data is also needed.

std::map<std::string, std::string> unpack_form_data(const std::string_view &form_text) {
    std::map<std::string, std::string> form_data{};

    std::cout << "String length: " << form_text.length() << std::endl;
    size_t index = 0;
    while (index < form_text.length()) {
        auto sep_index = form_text.find("&", index);
        if (sep_index == std::string::npos) {
            std::cout << "index is at " << index << " and there's no further &s to be had." << std::endl;
            sep_index = form_text.length();
        } else {
            std::cout << "Found an & at " << sep_index << std::endl;
        }

        auto form_row = form_text.substr(index, (sep_index - index));
        std::cout << "Got row: " << form_row << std::endl;

        auto row_sep = form_row.find("=", 0);
        std::string enc_name {form_row.substr(0, row_sep)};
        std::string enc_data {form_row.substr(row_sep + 1)};
        std::cout << "name: " << enc_name << " data: " << enc_data << std::endl;
        std::cout << urlencode_deserialize(enc_name) << ": " << urlencode_deserialize(enc_data) << std::endl;

        form_data.emplace(urlencode_deserialize(enc_name),
                          urlencode_deserialize(enc_data));

        index = sep_index + 1;
    }

    return form_data;
}

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

    format_entry(author, location, message, std::cout);

    std::exit(0);
}