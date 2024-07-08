#include <chrono>
#include <format>
#include <iostream>
#include <map>
#include <string>
#include <sys/sysinfo.h>

#include "curl.hh"
#include "weather.hh"

std::map<std::string, std::string> generate_report(curl_handle &curl)
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

    weather_loader weather{curl};

    report.emplace("temp", weather.get_weather_data());

    auto forecast = weather.get_forecast();

    for (auto &&f : forecast)
    {
        std::cout << std::format("{}, {}, {}", (int)f.condition, f.temperature, f.timeframe) << "," << std::endl;
    }

    auto observation = weather.get_current_observation();

    std::cout << std::format("{}, {}, {}", (int)observation.condition, observation.temperature, observation.timeframe) << std::endl;

    report.emplace("voltage", "12.7");
    report.emplace("charging", "false");
    
    return report;
}

int main(int argc, char **argv)
{
    curl_handle curl = curl_handle::create_handle();

    auto report = generate_report(curl);

    for (auto &&row : report)
    {
        std::cout << row.first << ": " << row.second << std::endl;
    }
}
