#include <chrono>
#include <format>
#include <iostream>
#include <map>
#include <string>
#include <sys/sysinfo.h>

#include "weather.hh"

std::map<std::string, std::string> generate_report()
{
    std::map<std::string, std::string> report{};
    struct sysinfo s_info{};

    int err = sysinfo(&s_info);
    if (err != 0)
    {
        std::cerr << "Error reading sysinfo: " << err << std::endl;
    }
    else
    {
        std::chrono::seconds uptime{s_info.uptime};
        report.emplace("uptime", std::format("{:%T}", uptime));
    }

    report.emplace("temp", get_weather_data());
    report.emplace("voltage", "12.7");
    report.emplace("charging", "false");
    
    return report;
}

int main(int argc, char **argv)
{
    auto report = generate_report();

    for (auto &&row : report)
    {
        std::cout << row.first << ": " << row.second << std::endl;
    }
}
