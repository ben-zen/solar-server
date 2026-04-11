// Copyright (C) Ben Lewis, 2024.
// SPDX-License-Identifier: MIT

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include "guestbook.hh"

#include <sstream>
#include <string>

// ---------------------------------------------------------------------------
// is_unencoded
// ---------------------------------------------------------------------------

TEST_CASE("is_unencoded accepts alphanumerics") {
    for (char ch = 'a'; ch <= 'z'; ++ch) CHECK(is_unencoded(ch));
    for (char ch = 'A'; ch <= 'Z'; ++ch) CHECK(is_unencoded(ch));
    for (char ch = '0'; ch <= '9'; ++ch) CHECK(is_unencoded(ch));
}

TEST_CASE("is_unencoded accepts the four exception characters") {
    CHECK(is_unencoded('*'));
    CHECK(is_unencoded('-'));
    CHECK(is_unencoded('.'));
    CHECK(is_unencoded('_'));
}

TEST_CASE("is_unencoded rejects characters that must be percent-encoded") {
    CHECK_FALSE(is_unencoded(' '));
    CHECK_FALSE(is_unencoded('!'));
    CHECK_FALSE(is_unencoded('&'));
    CHECK_FALSE(is_unencoded('='));
    CHECK_FALSE(is_unencoded('+'));
    CHECK_FALSE(is_unencoded('<'));
    CHECK_FALSE(is_unencoded('>'));
    CHECK_FALSE(is_unencoded('/'));
    CHECK_FALSE(is_unencoded('\''));
    CHECK_FALSE(is_unencoded('"'));
}

// ---------------------------------------------------------------------------
// urlencode_serlialize  (note: original spelling preserved)
// ---------------------------------------------------------------------------

TEST_CASE("urlencode_serlialize leaves alphanumerics and exceptions intact") {
    CHECK(urlencode_serlialize("hello") == "hello");
    CHECK(urlencode_serlialize("ABCxyz012") == "ABCxyz012");
    CHECK(urlencode_serlialize("a*b-c.d_e") == "a*b-c.d_e");
}

TEST_CASE("urlencode_serlialize percent-encodes spaces and specials") {
    CHECK(urlencode_serlialize(" ") == "%20");
    CHECK(urlencode_serlialize("a b") == "a%20b");
    CHECK(urlencode_serlialize("&") == "%26");
    CHECK(urlencode_serlialize("=") == "%3D");
}

TEST_CASE("urlencode_serlialize handles an empty string") {
    CHECK(urlencode_serlialize("") == "");
}

// ---------------------------------------------------------------------------
// urlencode_deserialize
// ---------------------------------------------------------------------------

TEST_CASE("urlencode_deserialize decodes alphanumerics unchanged") {
    CHECK(urlencode_deserialize("hello") == "hello");
}

TEST_CASE("urlencode_deserialize converts plus to space") {
    CHECK(urlencode_deserialize("hello+world") == "hello world");
}

TEST_CASE("urlencode_deserialize decodes percent-encoded characters") {
    CHECK(urlencode_deserialize("%20") == " ");
    CHECK(urlencode_deserialize("a%20b") == "a b");
    CHECK(urlencode_deserialize("%26") == "&");
    CHECK(urlencode_deserialize("%3D") == "=");
}

TEST_CASE("urlencode_deserialize handles an empty string") {
    CHECK(urlencode_deserialize("") == "");
}

TEST_CASE("urlencode_deserialize throws on truncated percent encoding") {
    // The implementation throws a pointer (throw new ...); catch std::out_of_range*.
    CHECK_THROWS(urlencode_deserialize("%A"));
    CHECK_THROWS(urlencode_deserialize("abc%"));
    CHECK_THROWS(urlencode_deserialize("%"));
}

TEST_CASE("urlencode_deserialize throws on non-hex characters after percent") {
    CHECK_THROWS(urlencode_deserialize("%GG"));
    CHECK_THROWS(urlencode_deserialize("%ZZ"));
}

// ---------------------------------------------------------------------------
// Roundtrip: serialize then deserialize
// ---------------------------------------------------------------------------

TEST_CASE("urlencode roundtrip preserves simple strings") {
    std::string input = "Hello World!";
    CHECK(urlencode_deserialize(urlencode_serlialize(input)) == input);
}

TEST_CASE("urlencode roundtrip preserves punctuation") {
    std::string input = "key=value&other=data";
    CHECK(urlencode_deserialize(urlencode_serlialize(input)) == input);
}

// ---------------------------------------------------------------------------
// unpack_form_data
// ---------------------------------------------------------------------------

TEST_CASE("unpack_form_data parses a standard form submission") {
    std::string form = "name=Alice&location=Seattle&message=Hello";
    auto data = unpack_form_data(form);

    CHECK(data["name"] == "Alice");
    CHECK(data["location"] == "Seattle");
    CHECK(data["message"] == "Hello");
}

TEST_CASE("unpack_form_data decodes URL-encoded values") {
    std::string form = "name=Bob+Smith&location=New%20York&message=Hi%21";
    auto data = unpack_form_data(form);

    CHECK(data["name"] == "Bob Smith");
    CHECK(data["location"] == "New York");
    CHECK(data["message"] == "Hi!");
}

TEST_CASE("unpack_form_data handles a single key-value pair") {
    auto data = unpack_form_data("key=value");
    CHECK(data.size() == 1);
    CHECK(data["key"] == "value");
}

TEST_CASE("unpack_form_data handles empty value") {
    auto data = unpack_form_data("name=&location=here&message=test");
    CHECK(data["name"] == "");
    CHECK(data["location"] == "here");
    CHECK(data["message"] == "test");
}

// ---------------------------------------------------------------------------
// format_entry  — safe serialization of user input
// ---------------------------------------------------------------------------

TEST_CASE("format_entry produces valid JSON front matter") {
    std::ostringstream out;
    format_entry("Alice", "Wonderland", "Hello there!", out);

    std::string result = out.str();

    // The output should contain valid JSON on the first line(s) followed by the message.
    auto double_newline = result.find("\n\n");
    REQUIRE(double_newline != std::string::npos);

    std::string json_part = result.substr(0, double_newline);
    auto parsed = json::parse(json_part);

    CHECK(parsed["params"]["author"] == "Alice");
    CHECK(parsed["params"]["location"] == "Wonderland");
    CHECK(parsed.contains("time"));
}

TEST_CASE("format_entry includes the message body after front matter") {
    std::ostringstream out;
    format_entry("Bob", "Mars", "Greetings from Mars!", out);

    std::string result = out.str();
    auto double_newline = result.find("\n\n");
    std::string body = result.substr(double_newline + 2);

    CHECK(body.find("Greetings from Mars!") != std::string::npos);
}

TEST_CASE("format_entry returns zero on success") {
    std::ostringstream out;
    CHECK(format_entry("A", "B", "C", out) == 0);
}

// ---------------------------------------------------------------------------
// Malicious input — XSS / HTML injection
// ---------------------------------------------------------------------------

TEST_CASE("format_entry safely serializes HTML/script injection in author field") {
    std::ostringstream out;
    std::string xss = "<script>alert('xss')</script>";
    format_entry(xss, "safe", "safe", out);

    std::string result = out.str();
    auto double_newline = result.find("\n\n");
    std::string json_part = result.substr(0, double_newline);

    // nlohmann/json must escape angle brackets and quotes in the JSON string.
    auto parsed = json::parse(json_part);
    CHECK(parsed["params"]["author"] == xss);

    // The raw JSON text must not contain unescaped angle brackets that could be
    // interpreted as HTML.  nlohmann/json escapes < and > as Unicode escapes or
    // keeps them literal inside JSON strings (which is safe in a JSON context).
    // Verify the JSON is re-parseable (not corrupted by the injection).
    CHECK_NOTHROW(static_cast<void>(json::parse(json_part)));
}

TEST_CASE("format_entry safely serializes HTML/script injection in message field") {
    std::ostringstream out;
    std::string xss = "<img src=x onerror=alert(1)>";
    format_entry("safe", "safe", xss, out);

    std::string result = out.str();
    auto double_newline = result.find("\n\n");
    std::string json_part = result.substr(0, double_newline);

    // JSON front matter must remain valid.
    CHECK_NOTHROW(static_cast<void>(json::parse(json_part)));

    // Message body should appear after the front matter.
    std::string body = result.substr(double_newline + 2);
    CHECK(body.find(xss) != std::string::npos);
}

TEST_CASE("urlencode roundtrip preserves XSS payload safely") {
    std::string xss = "<script>alert('xss')</script>";
    std::string encoded = urlencode_serlialize(xss);

    // The encoded form must not contain raw angle brackets.
    CHECK(encoded.find('<') == std::string::npos);
    CHECK(encoded.find('>') == std::string::npos);

    // Roundtrip should recover the original string.
    CHECK(urlencode_deserialize(encoded) == xss);
}

TEST_CASE("urlencode_serlialize encodes HTML-significant characters") {
    CHECK(urlencode_serlialize("<").find('%') != std::string::npos);
    CHECK(urlencode_serlialize(">").find('%') != std::string::npos);
    CHECK(urlencode_serlialize("\"").find('%') != std::string::npos);
    CHECK(urlencode_serlialize("'").find('%') != std::string::npos);
}

TEST_CASE("unpack_form_data with XSS in values") {
    // Simulates a form submission where the message contains a script tag,
    // URL-encoded as a browser would send it.
    std::string xss_encoded = urlencode_serlialize("<script>alert('xss')</script>");
    std::string form = "name=safe&location=safe&message=" + xss_encoded;
    auto data = unpack_form_data(form);

    // The decoded message should be the original XSS string (the guestbook
    // should store input faithfully; output escaping is Hugo's responsibility).
    CHECK(data["message"] == "<script>alert('xss')</script>");
}

// ---------------------------------------------------------------------------
// Malicious input — SQL-style injection (should survive as literal text)
// ---------------------------------------------------------------------------

TEST_CASE("format_entry handles SQL injection attempt in author") {
    std::ostringstream out;
    std::string sqli = "'; DROP TABLE entries; --";
    format_entry(sqli, "safe", "safe", out);

    std::string result = out.str();
    auto double_newline = result.find("\n\n");
    std::string json_part = result.substr(0, double_newline);

    auto parsed = json::parse(json_part);
    // The injection string must be stored as a literal JSON string value.
    CHECK(parsed["params"]["author"] == sqli);
}

// ---------------------------------------------------------------------------
// Malicious input — null bytes and control characters
// ---------------------------------------------------------------------------

TEST_CASE("urlencode roundtrip handles null byte") {
    std::string with_null{"before\0after", 12};
    std::string encoded = urlencode_serlialize(with_null);

    // Null byte must be percent-encoded.
    CHECK(encoded.find('\0') == std::string::npos);
    CHECK(encoded.find("%00") != std::string::npos);

    // Roundtrip should recover the original.
    CHECK(urlencode_deserialize(encoded) == with_null);
}

TEST_CASE("urlencode_serlialize encodes control characters") {
    // Tab, newline, carriage return should all be percent-encoded.
    CHECK(urlencode_serlialize("\t").find('%') != std::string::npos);
    CHECK(urlencode_serlialize("\n").find('%') != std::string::npos);
    CHECK(urlencode_serlialize("\r").find('%') != std::string::npos);
}

// ---------------------------------------------------------------------------
// Malicious input — path traversal
// ---------------------------------------------------------------------------

TEST_CASE("urlencode roundtrip preserves path traversal attempts literally") {
    std::string traversal = "../../etc/passwd";
    std::string encoded = urlencode_serlialize(traversal);
    CHECK(urlencode_deserialize(encoded) == traversal);
}

TEST_CASE("format_entry preserves path traversal as literal text in JSON") {
    std::ostringstream out;
    format_entry("../../etc/passwd", "safe", "safe", out);

    std::string result = out.str();
    auto double_newline = result.find("\n\n");
    auto parsed = json::parse(result.substr(0, double_newline));
    CHECK(parsed["params"]["author"] == "../../etc/passwd");
}

// ---------------------------------------------------------------------------
// Malicious input — excessively long input
// ---------------------------------------------------------------------------

TEST_CASE("urlencode roundtrip handles long input") {
    std::string long_input(10000, 'A');
    CHECK(urlencode_deserialize(urlencode_serlialize(long_input)) == long_input);
}

TEST_CASE("format_entry handles long field values") {
    std::ostringstream out;
    std::string long_name(5000, 'X');
    format_entry(long_name, "loc", "msg", out);

    std::string result = out.str();
    auto double_newline = result.find("\n\n");
    auto parsed = json::parse(result.substr(0, double_newline));
    CHECK(parsed["params"]["author"] == long_name);
}

// ---------------------------------------------------------------------------
// Malicious input — JSON injection
// ---------------------------------------------------------------------------

TEST_CASE("format_entry safely handles JSON injection in author field") {
    std::ostringstream out;
    std::string json_inject = R"(","evil":"injected"})";
    format_entry(json_inject, "safe", "safe", out);

    std::string result = out.str();
    auto double_newline = result.find("\n\n");
    std::string json_part = result.substr(0, double_newline);

    auto parsed = json::parse(json_part);
    // The injected JSON must not create a new key; it must be a literal string.
    CHECK(parsed["params"]["author"] == json_inject);
    CHECK_FALSE(parsed.contains("evil"));
}

TEST_CASE("format_entry safely handles JSON injection in location field") {
    std::ostringstream out;
    std::string json_inject = R"("},"params":{"author":"hacked","location":"hacked"})";
    format_entry("safe", json_inject, "safe", out);

    std::string result = out.str();
    auto double_newline = result.find("\n\n");
    auto parsed = json::parse(result.substr(0, double_newline));
    CHECK(parsed["params"]["author"] == "safe");
    CHECK(parsed["params"]["location"] == json_inject);
}

// ---------------------------------------------------------------------------
// Malicious input — Unicode and multi-byte sequences
// ---------------------------------------------------------------------------

TEST_CASE("urlencode roundtrip handles UTF-8 sequences") {
    // A snowman: U+2603 → UTF-8 bytes 0xE2 0x98 0x83
    std::string snowman = "\xE2\x98\x83";
    CHECK(urlencode_deserialize(urlencode_serlialize(snowman)) == snowman);
}

TEST_CASE("format_entry handles UTF-8 in all fields") {
    std::ostringstream out;
    std::string emoji = "\xF0\x9F\x8C\x8D"; // U+1F30D globe emoji
    format_entry(emoji, emoji, emoji, out);

    std::string result = out.str();
    auto double_newline = result.find("\n\n");
    auto parsed = json::parse(result.substr(0, double_newline));
    CHECK(parsed["params"]["author"] == emoji);
    CHECK(parsed["params"]["location"] == emoji);

    std::string body = result.substr(double_newline + 2);
    CHECK(body.find(emoji) != std::string::npos);
}

// ---------------------------------------------------------------------------
// Edge: form data with encoded newlines (as browsers send them)
// ---------------------------------------------------------------------------

TEST_CASE("unpack_form_data handles encoded CRLF in message") {
    std::string form = "name=test&location=place&message=line1%0D%0Aline2";
    auto data = unpack_form_data(form);
    CHECK(data["message"] == "line1\r\nline2");
}

// ---------------------------------------------------------------------------
// parse_content_length — validates CGI CONTENT_LENGTH env var
// ---------------------------------------------------------------------------

TEST_CASE("parse_content_length returns nullopt for nullptr") {
    CHECK_FALSE(parse_content_length(nullptr).has_value());
}

TEST_CASE("parse_content_length returns nullopt for empty string") {
    CHECK_FALSE(parse_content_length("").has_value());
}

TEST_CASE("parse_content_length parses valid small values") {
    auto result = parse_content_length("124");
    REQUIRE(result.has_value());
    CHECK(result.value() == 124);
}

TEST_CASE("parse_content_length parses zero") {
    auto result = parse_content_length("0");
    REQUIRE(result.has_value());
    CHECK(result.value() == 0);
}

TEST_CASE("parse_content_length rejects values exceeding max") {
    // Max is 8192; anything larger should be rejected.
    CHECK_FALSE(parse_content_length("10000").has_value());
    CHECK_FALSE(parse_content_length("99999").has_value());
}

TEST_CASE("parse_content_length accepts value at the maximum") {
    auto result = parse_content_length("8192");
    REQUIRE(result.has_value());
    CHECK(result.value() == 8192);
}

TEST_CASE("parse_content_length rejects non-numeric input") {
    CHECK_FALSE(parse_content_length("abc").has_value());
    CHECK_FALSE(parse_content_length("12abc").has_value());
    CHECK_FALSE(parse_content_length("-1").has_value());
}

// ---------------------------------------------------------------------------
// truncate_field — enforces max length on user input
// ---------------------------------------------------------------------------

TEST_CASE("truncate_field returns short string unchanged") {
    CHECK(truncate_field("hello", 200) == "hello");
}

TEST_CASE("truncate_field truncates to max length") {
    std::string long_str(300, 'A');
    auto result = truncate_field(long_str, 200);
    CHECK(result.size() == 200);
    CHECK(result == std::string(200, 'A'));
}

TEST_CASE("truncate_field handles empty string") {
    CHECK(truncate_field("", 200) == "");
}

TEST_CASE("truncate_field handles string exactly at limit") {
    std::string exact(200, 'B');
    CHECK(truncate_field(exact, 200) == exact);
}

// ---------------------------------------------------------------------------
// validate_form_fields — checks required fields are present and within limits
// ---------------------------------------------------------------------------

TEST_CASE("validate_form_fields accepts valid form data") {
    std::map<std::string, std::string> form = {
        {"name", "Alice"}, {"location", "Seattle"}, {"message", "Hello!"}
    };
    auto result = validate_form_fields(form);
    REQUIRE(result.has_value());
    CHECK(result->at("name") == "Alice");
    CHECK(result->at("location") == "Seattle");
    CHECK(result->at("message") == "Hello!");
}

TEST_CASE("validate_form_fields rejects missing name") {
    std::map<std::string, std::string> form = {
        {"location", "Seattle"}, {"message", "Hello!"}
    };
    CHECK_FALSE(validate_form_fields(form).has_value());
}

TEST_CASE("validate_form_fields rejects missing message") {
    std::map<std::string, std::string> form = {
        {"name", "Alice"}, {"location", "Seattle"}
    };
    CHECK_FALSE(validate_form_fields(form).has_value());
}

TEST_CASE("validate_form_fields allows missing location") {
    std::map<std::string, std::string> form = {
        {"name", "Alice"}, {"message", "Hello!"}
    };
    auto result = validate_form_fields(form);
    REQUIRE(result.has_value());
    CHECK(result->at("location") == "");
}

TEST_CASE("validate_form_fields rejects empty name") {
    std::map<std::string, std::string> form = {
        {"name", ""}, {"location", "here"}, {"message", "Hello!"}
    };
    CHECK_FALSE(validate_form_fields(form).has_value());
}

TEST_CASE("validate_form_fields rejects empty message") {
    std::map<std::string, std::string> form = {
        {"name", "Alice"}, {"location", "here"}, {"message", ""}
    };
    CHECK_FALSE(validate_form_fields(form).has_value());
}

TEST_CASE("validate_form_fields truncates long fields") {
    std::map<std::string, std::string> form = {
        {"name", std::string(300, 'X')},
        {"location", std::string(300, 'Y')},
        {"message", std::string(3000, 'Z')}
    };
    auto result = validate_form_fields(form);
    REQUIRE(result.has_value());
    CHECK(result->at("name").size() == max_author_length);
    CHECK(result->at("location").size() == max_location_length);
    CHECK(result->at("message").size() == max_message_length);
}

// ---------------------------------------------------------------------------
// read_cgi_input — reads exactly content_length bytes from a stream
// ---------------------------------------------------------------------------

TEST_CASE("read_cgi_input reads exact number of bytes") {
    std::istringstream input("name=Alice&location=Seattle&message=Hi");
    auto result = read_cgi_input(input, 38);
    CHECK(result == "name=Alice&location=Seattle&message=Hi");
}

TEST_CASE("read_cgi_input reads partial content when stream is shorter") {
    std::istringstream input("short");
    auto result = read_cgi_input(input, 100);
    CHECK(result == "short");
}

TEST_CASE("read_cgi_input reads zero bytes") {
    std::istringstream input("anything");
    auto result = read_cgi_input(input, 0);
    CHECK(result == "");
}

// ---------------------------------------------------------------------------
// generate_cgi_redirect — produces a proper CGI redirect response
// ---------------------------------------------------------------------------

TEST_CASE("generate_cgi_redirect produces valid CGI headers") {
    auto response = generate_cgi_redirect("/");
    CHECK(response.find("Status: 303 See Other") != std::string::npos);
    CHECK(response.find("Location: /") != std::string::npos);
    // Must end with blank line (double CRLF or double LF).
    CHECK(response.find("\r\n\r\n") != std::string::npos);
}

TEST_CASE("generate_cgi_redirect uses the provided URL") {
    auto response = generate_cgi_redirect("/guestbook");
    CHECK(response.find("Location: /guestbook") != std::string::npos);
}

// ---------------------------------------------------------------------------
// generate_cgi_error — produces a proper CGI error response
// ---------------------------------------------------------------------------

TEST_CASE("generate_cgi_error produces 400 response") {
    auto response = generate_cgi_error(400, "Bad Request");
    CHECK(response.find("Status: 400 Bad Request") != std::string::npos);
    CHECK(response.find("Content-Type: text/html") != std::string::npos);
    CHECK(response.find("Bad Request") != std::string::npos);
}

TEST_CASE("generate_cgi_error produces 413 response") {
    auto response = generate_cgi_error(413, "Payload Too Large");
    CHECK(response.find("Status: 413 Payload Too Large") != std::string::npos);
}
