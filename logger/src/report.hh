#pragma once

#include <format>
#include <chrono>
#include <string>
#include <vector>

#include "curl.hh"
#include "unit.hh"
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

status_report generate_report(curl_handle &curl);