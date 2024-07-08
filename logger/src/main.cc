#include <chrono>
#include <format>
#include <iostream>
#include <map>
#include <string>
#include <sys/sysinfo.h>

#include "curl.hh"
#include "weather.hh"

struct status_report {
    daytime_forecast current_weather;
    std::vector<daytime_forecast> upcoming_days;
    std::chrono::seconds uptime;
    voltage<float> battery_voltage;
    wattage<float> current_wattage;
    wattage<float> charging_wattage;
    bool is_charging;
};

std::string report_format{ 
};

template <>
struct std::formatter<status_report> : std::formatter<std::string_view> {
    template <typename Context>
    auto format(const status_report &report, Context &ctx) const {
        return format_to(ctx.out(),
R"(report:
  current weather: {}
  upcoming daytime weather: {}
  uptime: {:%T}
  battery voltage: {}
  current usage: {}
  charging wattage: {}
  is charging: {}
)",
                         report.current_weather,
                         report.upcoming_days,
                         report.uptime,
                         report.battery_voltage,
                         report.current_wattage,
                         report.charging_wattage,
                         report.is_charging);
    }
};

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
        std::cout << std::format("{}, {}, {}", f.condition, f.temp, f.timeframe) << "," << std::endl;
    }

    report.upcoming_days = std::move(forecast);

    auto observation = weather.get_current_observation();

    std::cout << std::format("{}, {}, {}", observation.condition, observation.temp, observation.timeframe) << std::endl;

    report.current_weather = observation;

    // battery statistics
    report.battery_voltage = voltage<float>(12.8);

    // solar array details
    report.is_charging = false;
    
    return report;
}

int main(int argc, char **argv)
{
    curl_handle curl = curl_handle::create_handle();

    auto report = generate_report(curl);

    std::cout << std::format("{}", report) << std::endl;
}
