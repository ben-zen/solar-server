// Copyright (C) Ben Lewis, 2025.
// SPDX-License-Identifier: MIT

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

    nws_location location{
        .station = arg_parser.get<std::string>("--station"),
        .grid_office = arg_parser.get<std::string>("--grid-office"),
        .grid_x = arg_parser.get<int>("--grid-x"),
        .grid_y = arg_parser.get<int>("--grid-y"),
    };

    std::filesystem::path destination{arg_parser.get<std::string>("--output")};

    // Acquire all major resources before running.
    curl_handle curl = curl_handle::create_handle();
    std::fstream out_file{destination, std::ios::out | std::ios::trunc};
    
    auto report = generate_report(curl, location);
    std::cout << fmt::format("{}", report) << std::endl;
    report_to_css(report, out_file);
}
