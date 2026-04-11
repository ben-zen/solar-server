// Copyright (C) Ben Lewis, 2024.
// SPDX-License-Identifier: MIT

#pragma once

#include <cctype>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <string_view>

#include <fmt/chrono.h>
#include <fmt/format.h>
#include <json.hpp>

using json = nlohmann::json;

// The |application/x-www-form-urlencoded percent-encode set| contains all code points except:
// * Alphanumerics
// * U+002A (*), U+002D (-), U+002E (.), and U+005F (_)
inline bool is_unencoded(unsigned char ch) {
    static std::string exceptions{"*-._"};
    return std::isalnum(ch) || (exceptions.find(ch) != std::string::npos); 
}

// Implements x-www-form-urlencoded from RFC3986 and https://url.spec.whatwg.org/
// with a focus on UTF-8.
inline std::string urlencode_serlialize(std::string const &str) {
    std::string encoded{};

    for (auto &ch : str) {
        if (is_unencoded(ch)) {
            encoded.push_back(ch);
            continue;
        }
        
        // Percent-encode anything else.
        encoded.append(fmt::format("%{:02X}", static_cast<unsigned char>(ch)));
    }

    return encoded;
}

inline std::string urlencode_deserialize(std::string const &enc_str) {
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

inline std::map<std::string, std::string> unpack_form_data(const std::string_view &form_text) {
    std::map<std::string, std::string> form_data{};

    size_t index = 0;
    while (index < form_text.length()) {
        auto sep_index = form_text.find("&", index);
        if (sep_index == std::string::npos) {
            sep_index = form_text.length();
        }

        auto form_row = form_text.substr(index, (sep_index - index));

        auto row_sep = form_row.find("=", 0);
        std::string enc_name {form_row.substr(0, row_sep)};
        std::string enc_data {form_row.substr(row_sep + 1)};

        form_data.emplace(urlencode_deserialize(enc_name),
                          urlencode_deserialize(enc_data));

        index = sep_index + 1;
    }

    return form_data;
}

inline int format_entry(const std::string &author,
                 const std::string &location,
                 const std::string &message,
                 std::basic_ostream<char> &output) {
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
