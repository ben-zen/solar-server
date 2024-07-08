#pragma once

#include <format>
#include <string>

#include <curl/curl.h>

#include "curl.hh"

enum class overall_condition {
    clear,
    partly_cloudy,
    cloudy,
    misting,
    raining,
    snowing
};

template<>
struct std::formatter<overall_condition> : std::formatter<std::string_view> {
  template <typename Context>
  auto format(const overall_condition condition, Context &context) const -> auto {
    switch (condition) {
    case overall_condition::clear:
      return formatter<std::string_view>::format("clear", context);

    case overall_condition::cloudy:
      return formatter<std::string_view>::format("cloudy", context);

    case overall_condition::misting:
      return formatter<std::string_view>::format("misting", context);

    case overall_condition::partly_cloudy:
      return formatter<std::string_view>::format("partly cloudy", context);

    case overall_condition::raining:
      return formatter<std::string_view>::format("raining", context);

    case overall_condition::snowing:
      return formatter<std::string_view>::format("snowing", context);
    }
  
    // unreachable
    return context.out();
  }
};

enum class temperature_unit {
  fahrenheit,
  celsius
};

template<>
struct std::formatter<temperature_unit> : std::formatter<std::string_view> {
  // Bonus to make this support long and short forms.

  template <typename Context>
  auto format(const temperature_unit unit, Context &context) const -> auto {
    switch (unit) {
      case temperature_unit::celsius:
        return formatter<std::string_view>::format("C", context);

      case temperature_unit::fahrenheit:
        return formatter<std::string_view>::format("F", context);
    }

    // Unreachable
    return context.out();
  }
};

struct temperature {
  temperature_unit unit;
  float value;
};

struct daytime_forecast {
    std::string timeframe;
    overall_condition condition;
    temperature temp;

    std::string formatted() {
        return std::format("{}, {}, {} °{}", timeframe, (int)condition, temp.value, temp.unit);
    }
};

class weather_loader {
public:
  weather_loader(curl_handle &curl);
  std::string get_weather_data();
  std::vector<daytime_forecast> get_forecast();
  daytime_forecast get_current_observation();

private:
  curl_handle m_curl;

  CURLcode nws_api_call(std::string &url_str, std::string *buffer);

};
