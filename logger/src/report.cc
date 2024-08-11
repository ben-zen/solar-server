
#include <iostream>
#include <sys/sysinfo.h>

#include "report.hh"

status_report generate_report(curl_handle &curl)
{
    status_report report{};
    struct sysinfo s_info{};

    int err = sysinfo(&s_info);
    if (err != 0)
    {
        std::cerr << "Error reading sysinfo: " << err << std::endl;
    }
    else
    {
        std::chrono::seconds uptime{s_info.uptime};
        report.uptime = std::move(uptime);
    }

    weather_loader weather{curl};

    auto forecast = weather.get_forecast();

    for (auto &&f : forecast)
    {
        std::cout << fmt::format("{}, {}, {}", f.condition, f.temp, f.timeframe) << "," << std::endl;
    }

    report.upcoming_days = std::move(forecast);

    auto observation = weather.get_current_observation();

    std::cout << fmt::format("{}, {}, {}", observation.condition, observation.temp, observation.timeframe) << std::endl;

    report.current_weather = observation;

    // battery statistics
    report.battery_voltage = voltage<float>(12.8);

    // solar array details
    report.is_charging = false;
    
    return report;
}