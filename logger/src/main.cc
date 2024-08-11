
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <iostream>
#include <string>

#include "curl.hh"
#include "report.hh"
#include "transform.hh"
#include "weather.hh"



int main(int argc, char **argv)
{
    std::filesystem::path destination{"./status.css"};

    // parse args. for now, we write to a local file. Look into p-ranav/argparse
    // for a solution here.

    // Acquire all major resources before running.
    // load configuration.

    curl_handle curl = curl_handle::create_handle();
    std::fstream out_file{destination, std::ios::out | std::ios::trunc};
    
    auto report = generate_report(curl);
    std::cout << fmt::format("{}", report) << std::endl;
    report_to_css(report, out_file);
}
