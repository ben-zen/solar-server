// Copyright (C) Ben Lewis, 2024.
// SPDX-License-Identifier: MIT

#pragma once

#include <cctype>
#include <charconv>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/chrono.h>
#include <fmt/format.h>
#include <json.hpp>

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Input limits
// ---------------------------------------------------------------------------

constexpr size_t max_content_length = 8192;
constexpr size_t max_author_length = 200;
constexpr size_t max_location_length = 200;
constexpr size_t max_message_length = 2000;

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
            throw std::out_of_range{fmt::format("Terminated mid-percent encoding: {}", enc_str)};
        }

        // From this, we know that (enc_cursor + 3) is at most enc_str.end(), which is okay for parsing.

        if (!std::isxdigit(*(enc_cursor + 1)) || !std::isxdigit(*(enc_cursor + 2))) {
            throw std::invalid_argument{fmt::format("Non-hex character in percent-encoding: {}", enc_str)};
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

inline int format_entry(std::basic_ostream<char> &output,
                 const std::string &author,
                 const std::string &location,
                 const std::string &message,
                 std::chrono::system_clock::time_point timestamp) {
    // An entry is a Hugo content page with JSON front matter.
    // Two newlines separate the front matter from the message body.

    auto ts_seconds = std::chrono::floor<std::chrono::seconds>(timestamp);

    json front_matter{
        {"author", author},
        {"date", fmt::format("{:%FT%T}", ts_seconds)},
        {"location", location},
        {"type", "guestbook"}
    };

    output << front_matter << std::endl << std::endl << message << std::endl;

    return 0;
}

// Generates a filename for a guestbook entry: YYYY-MM-DDTHH-MM-SS_authorpart.md
// The author part consists of the first 8 alphabetic glyphs from the author name,
// lowercased for filesystem safety.
inline std::string generate_entry_filename(
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

// ---------------------------------------------------------------------------
// CGI helper functions
// ---------------------------------------------------------------------------

// Escapes a string for safe inclusion in HTML content, preventing XSS.
inline std::string html_escape(const std::string &input) {
    std::string escaped;
    escaped.reserve(input.size());
    for (char ch : input) {
        switch (ch) {
            case '&':  escaped += "&amp;";  break;
            case '<':  escaped += "&lt;";   break;
            case '>':  escaped += "&gt;";   break;
            case '"':  escaped += "&quot;";  break;
            case '\'': escaped += "&#39;";  break;
            default:   escaped += ch;       break;
        }
    }
    return escaped;
}

// Returns true if the string contains CR or LF characters, which would
// allow HTTP response splitting / header injection.
inline bool contains_header_breaks(const std::string &value) {
    return value.find('\r') != std::string::npos
        || value.find('\n') != std::string::npos;
}

// Sanitizes a redirect target URL to prevent open redirects and header
// injection.  Accepts relative paths (but not scheme-relative "//…") and
// same-origin absolute URLs (converted to a relative path using |host|).
// Falls back to "/" for anything else.
inline std::string sanitize_redirect_target(const std::string &referer,
                                            const std::string &host = "") {
    if (referer.empty() || contains_header_breaks(referer)) {
        return "/";
    }

    // Accept already-relative paths, but reject scheme-relative URLs ("//…").
    if (referer[0] == '/') {
        if (referer.size() > 1 && referer[1] == '/') {
            return "/";
        }
        return referer;
    }

    // Accept same-origin absolute URLs by converting them to a relative path.
    if (!host.empty()) {
        for (const std::string &scheme : {"http://", "https://"}) {
            if (referer.compare(0, scheme.size(), scheme) != 0) {
                continue;
            }

            auto host_start = scheme.size();
            if (referer.compare(host_start, host.size(), host) != 0) {
                continue;
            }

            auto path_start = host_start + host.size();
            if (path_start == referer.size()) {
                return "/";
            }

            if (referer[path_start] == '/') {
                return referer.substr(path_start);
            }
        }
    }

    return "/";
}

// HTTP status codes used in CGI responses.
enum class HttpStatus {
    ok                  = 200,
    see_other           = 303,
    bad_request         = 400,
    not_found           = 404,
    payload_too_large   = 413,
    enhance_your_calm   = 420,
    internal_error      = 500,
};

// Returns the standard reason phrase for a given HTTP status code.
inline std::string_view status_phrase(HttpStatus code) {
    switch (code) {
        case HttpStatus::ok:                return "OK";
        case HttpStatus::see_other:         return "See Other";
        case HttpStatus::bad_request:       return "Bad Request";
        case HttpStatus::not_found:         return "Not Found";
        case HttpStatus::payload_too_large: return "Payload Too Large";
        case HttpStatus::enhance_your_calm: return "Enhance Your Calm";
        case HttpStatus::internal_error:    return "Internal Server Error";
    }
    return "Unknown";
}

// Parses and validates the CONTENT_LENGTH CGI environment variable.
// Returns std::nullopt if the value is null, empty, non-numeric, negative,
// or exceeds max_content_length.
inline std::optional<size_t> parse_content_length(const char *env_value) {
    if (env_value == nullptr || *env_value == '\0') {
        return std::nullopt;
    }

    std::string_view sv{env_value};

    size_t value{};
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);
    if (ec != std::errc{} || ptr != sv.data() + sv.size()) {
        return std::nullopt;
    }

    if (value > max_content_length) {
        return std::nullopt;
    }

    return value;
}

// Truncates a string to the given maximum length.
inline std::string truncate_field(const std::string &value, size_t max_length) {
    if (value.size() <= max_length) {
        return value;
    }
    return value.substr(0, max_length);
}

// Describes a single field-level validation error.
struct field_error {
    std::string field;   // e.g. "name", "message"
    std::string reason;  // human-readable explanation
};

// Result of form validation.  On success |ok| is true and |fields| contains
// the sanitized values.  On failure |ok| is false and |errors| lists every
// field that failed, while |fields| still holds whatever values were present
// (for display in the error page).
struct form_validation_result {
    bool ok = false;
    std::map<std::string, std::string> fields;
    std::vector<field_error> errors;
};

// Validates required form fields are present and non-empty (name, message),
// truncates all fields to their respective max lengths, and returns the
// sanitized fields. Location is optional (defaults to empty).
// On failure the result contains per-field error descriptions.
inline form_validation_result validate_form_fields(
        const std::map<std::string, std::string> &form_data) {

    form_validation_result result;

    auto name_it = form_data.find("name");
    std::string name_value = (name_it != form_data.end()) ? name_it->second : "";
    if (name_value.empty()) {
        result.errors.push_back({"name", "required, but missing or empty"});
    }

    auto message_it = form_data.find("message");
    std::string message_value = (message_it != form_data.end()) ? message_it->second : "";
    if (message_value.empty()) {
        result.errors.push_back({"message", "required, but missing or empty"});
    }

    std::string location{};
    auto location_it = form_data.find("location");
    if (location_it != form_data.end()) {
        location = location_it->second;
    }

    result.fields["name"] = truncate_field(name_value, max_author_length);
    result.fields["location"] = truncate_field(location, max_location_length);
    result.fields["message"] = truncate_field(message_value, max_message_length);
    result.ok = result.errors.empty();
    return result;
}

// Reads exactly content_length bytes from the given input stream.
inline std::string read_cgi_input(std::istream &input, size_t content_length) {
    if (content_length == 0) {
        return "";
    }
    std::string buffer(content_length, '\0');
    input.read(buffer.data(), static_cast<std::streamsize>(content_length));
    buffer.resize(static_cast<size_t>(input.gcount()));
    return buffer;
}

// Builds a complete CGI response with a Status header, optional extra headers,
// and an optional body.  This is the single response-building primitive in the
// codebase; every other response helper wraps this function.
inline std::string generate_cgi_response(HttpStatus code,
                                         const std::string &extra_headers = "",
                                         const std::string &body = "") {
    std::string response = fmt::format("Status: {} {}\r\n",
                                       static_cast<int>(code),
                                       status_phrase(code));
    if (!extra_headers.empty()) {
        response += extra_headers;
    }
    response += "\r\n";
    if (!body.empty()) {
        response += body;
    }
    return response;
}

// Generates a CGI 303 redirect response to the given URL.
// If the URL contains CR/LF (header injection), returns a 400 error instead.
inline std::string generate_cgi_redirect(const std::string &redirect_url) {
    if (contains_header_breaks(redirect_url)) {
        return generate_cgi_response(HttpStatus::bad_request,
                                     "Content-Type: text/plain\r\n",
                                     "Bad Request: Invalid redirect URL\n");
    }
    return generate_cgi_response(HttpStatus::see_other,
                                 fmt::format("Location: {}\r\n", redirect_url));
}

// Generates a CGI error response.  |detail| lets callers describe what
// specifically went wrong so end users (and server logs) get useful context.
// Only error codes (4xx, 5xx) are permitted.  If a non-error code is provided,
// the code and detail are logged to stderr and a 500 "Internal Server Error"
// is returned instead.
inline std::string generate_cgi_error(HttpStatus code,
                                      const std::string &detail = "") {
    int code_int = static_cast<int>(code);
    if (code_int < 400 || code_int > 599) {
        std::cerr << fmt::format(
            "generate_cgi_error: non-error status {} passed; detail='{}'. Returning 500.\n",
            code_int, detail);
        code = HttpStatus::internal_error;
    }

    auto phrase = status_phrase(code);
    std::string safe_detail = html_escape(detail);
    std::string body_detail = safe_detail.empty()
        ? std::string(phrase)
        : fmt::format("{}: {}", phrase, safe_detail);

    std::string body = fmt::format(
        "<html><body><h1>{}</h1><p>{}</p>"
        "<p><a href=\"/\">Return to site</a></p></body></html>\n",
        phrase, body_detail);

    return generate_cgi_response(code,
                                 "Content-Type: text/html\r\n",
                                 body);
}

// Generates a CGI form-validation error page.  Each submitted field is shown
// in a description list; fields that failed validation are flagged with their
// error reason.  The page uses only basic semantic HTML elements.
inline std::string generate_cgi_form_error(
        const form_validation_result &result) {

    // Build a set of field names that have errors, for quick lookup.
    std::map<std::string, std::string> error_map;
    for (const auto &e : result.errors) {
        error_map[e.field] = e.reason;
    }

    // Construct the list of error summaries.
    std::string error_list;
    for (const auto &e : result.errors) {
        error_list += fmt::format(
            "    <li><strong>{}</strong>: {}</li>\n",
            html_escape(e.field), html_escape(e.reason));
    }

    // Construct description list of submitted values, flagging bad fields.
    // Field display order: name, location, message.
    static const std::vector<std::pair<std::string, std::string>> field_labels = {
        {"name", "Name"},
        {"location", "Location"},
        {"message", "Message"},
    };

    std::string field_rows;
    for (const auto &[key, label] : field_labels) {
        auto value_it = result.fields.find(key);
        std::string safe_value = (value_it != result.fields.end())
            ? html_escape(value_it->second) : "";

        auto err_it = error_map.find(key);
        if (err_it != error_map.end()) {
            field_rows += fmt::format(
                "    <dt>{} <em>(error)</em></dt>\n"
                "    <dd><code>{}</code></dd>\n"
                "    <dd><em>{}</em></dd>\n",
                html_escape(label),
                safe_value.empty() ? std::string("(empty)") : safe_value,
                html_escape(err_it->second));
        } else {
            field_rows += fmt::format(
                "    <dt>{}</dt>\n"
                "    <dd><code>{}</code></dd>\n",
                html_escape(label),
                safe_value.empty() ? std::string("(empty)") : safe_value);
        }
    }

    auto phrase = status_phrase(HttpStatus::bad_request);

    std::string body = fmt::format(
        "<!DOCTYPE html>\n"
        "<html lang=\"en\">\n"
        "<head><meta charset=\"utf-8\"><title>{0}</title></head>\n"
        "<body>\n"
        "  <h1>{0}</h1>\n"
        "  <p>The form could not be submitted. "
        "The following fields have errors:</p>\n"
        "  <ul>\n{1}"
        "  </ul>\n"
        "  <h2>Submitted values</h2>\n"
        "  <dl>\n{2}"
        "  </dl>\n"
        "  <nav><a href=\"/\">Return to site</a></nav>\n"
        "</body>\n"
        "</html>\n",
        phrase, error_list, field_rows);

    return generate_cgi_response(HttpStatus::bad_request,
                                 "Content-Type: text/html\r\n",
                                 body);
}
