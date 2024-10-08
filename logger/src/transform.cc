
#include "transform.hh"
#include "weather.hh"

#include <fmt/chrono.h>

#include "include_ext/json.hpp"

using json = nlohmann::json;

/* example CSS:

.battery-icon {
    content: url("/svg/battery-charging-half.svg");
    height: 14px;
}

.battery-status::after {
    content:'45%';
    height: 24px;
}


.weather-icon {
    content: url("/svg/partly-sunny.svg");
    height: 14px;
}

.weather::after {
    content: '67°F';
    height: 24px;
}

*/

std::string convert_condition_to_filename(overall_condition condition) {
    switch (condition) {
    case overall_condition::clear:
        return "sunny.svg";

    case overall_condition::cloudy:
        return "cloudy.svg";

    case overall_condition::partly_cloudy:
        return "partly-sunny.svg";

    case overall_condition::misting:
        return "light-rain.svg";

    case overall_condition::raining:
        return "raining.svg";

    case overall_condition::snowing:
        return "snowing.svg";
    }

    return "";
};

int report_to_css(const status_report &report, std::ostream &out) {
    out << fmt::format(R"(.weather-icon {{ content: url("/svg/{}"); height: 14px; }})",
                       convert_condition_to_filename(report.current_weather.condition)) << std::endl
        << fmt::format(R"(.weather::after {{ content: "{}"; height: 24px; }})",
                       report.current_weather.temp) << std::endl
        << fmt::format(R"(.forecast-tomorrow-icon {{ content: url("/svg/{}"); height: 14px; }})",
                       convert_condition_to_filename(report.upcoming_days[0].condition)) << std::endl
        << fmt::format(R"(.forecast-tomorrow::after {{ content: "{}"; height: 24px; }})",
                       report.upcoming_days[0].temp) << std::endl
        << fmt::format(R"(.forecast-overmorrow-icon {{ content: url("/svg/{}"); height: 14px; }})",
                       convert_condition_to_filename(report.upcoming_days[1].condition)) << std::endl
        << fmt::format(R"(.forecast-overmorrow::after {{ content: "{}"; height: 24px; }})",
                       report.upcoming_days[1].temp) << std::endl
        << fmt::format(R"(.battery-icon {{ content: url("/svg/{}"); height: 14px; }})",
                       report.is_charging ? "battery-charging-half.svg" : "battery-half.svg") << std::endl
        << fmt::format(R"(.battery-status::after {{ content: "{}"; }})", report.battery_voltage) << std::endl
        << fmt::format(R"(.uptime::after {{ content: "{:%T}"; }})", report.uptime)
        << std::endl;

    return 0;
}

int report_to_json(const status_report &/* report */, std::ostream &/* out */) {
    return 0;
}