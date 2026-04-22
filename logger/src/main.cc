// Copyright (C) Ben Lewis, 2025.
// SPDX-License-Identifier: MIT

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <iostream>
#include <string>

#include <argparse.hpp>

#include "curl.hh"
#include "report.hh"
#include "transform.hh"
#include "weather.hh"

// NWS station identifiers are alphanumeric (e.g., KBFI, K0S9).
// Grid office codes are alphabetic (e.g., SEW, LWX).
static bool is_valid_station(const std::string &s) {
    return !s.empty() && std::all_of(s.begin(), s.end(),
        [](unsigned char c) { return std::isalnum(c); });
}

static bool is_valid_grid_office(const std::string &s) {
    return !s.empty() && std::all_of(s.begin(), s.end(),
        [](unsigned char c) { return std::isalpha(c); });
}

int main(int argc, char **argv)
{
    argparse::ArgumentParser arg_parser{"logger", "0.1.0"};
    arg_parser.add_argument("--station")
        .help("NWS station identifier (e.g., KBFI)")
        .required();
    arg_parser.add_argument("--grid-office")
        .help("NWS forecast grid office (e.g., SEW)")
        .required();
    arg_parser.add_argument("--grid-x")
        .help("NWS forecast grid X coordinate (e.g., 124)")
        .scan<'i', int>()
        .required();
    arg_parser.add_argument("--grid-y")
        .help("NWS forecast grid Y coordinate (e.g., 69)")
        .scan<'i', int>()
        .required();
    arg_parser.add_argument("-o", "--output")
        .help("Output file path for the generated CSS")
        .default_value(std::string{"./status.css"});

    try {
        arg_parser.parse_args(argc, argv);
    } catch (const std::exception &e) {
        std::cerr << fmt::format("Error parsing arguments: {}\n", e.what());
        std::cerr << arg_parser << std::endl;
        return 1;
    }

    auto station = arg_parser.get<std::string>("--station");
    auto grid_office = arg_parser.get<std::string>("--grid-office");
    auto grid_x = arg_parser.get<int>("--grid-x");
    auto grid_y = arg_parser.get<int>("--grid-y");

    if (!is_valid_station(station)) {
        std::cerr << fmt::format("Error: --station must be non-empty and alphanumeric, got '{}'\n", station);
        return 1;
    }
    if (!is_valid_grid_office(grid_office)) {
        std::cerr << fmt::format("Error: --grid-office must be non-empty and alphabetic, got '{}'\n", grid_office);
        return 1;
    }
    if (grid_x < 0) {
        std::cerr << fmt::format("Error: --grid-x must be non-negative, got {}\n", grid_x);
        return 1;
    }
    if (grid_y < 0) {
        std::cerr << fmt::format("Error: --grid-y must be non-negative, got {}\n", grid_y);
        return 1;
    }

    nws_location location{
        .station = std::move(station),
        .grid_office = std::move(grid_office),
        .grid_x = grid_x,
        .grid_y = grid_y,
    };

    std::filesystem::path destination{arg_parser.get<std::string>("--output")};

    // Acquire all major resources before running.
    curl_handle curl = curl_handle::create_handle();
    
    auto report = generate_report(curl, location);
    std::cout << fmt::format("{}", report) << std::endl;

    return report_to_css(report, destination);
}
