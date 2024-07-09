
#include <format>
#include <iostream>
#include <string>

#include "curl.hh"
#include "report.hh"
#include "transform.hh"
#include "weather.hh"



int main(int argc, char **argv)
{
    curl_handle curl = curl_handle::create_handle();

    auto report = generate_report(curl);

    std::cout << std::format("{}", report) << std::endl;

    report_to_css(report, std::cout);
}
