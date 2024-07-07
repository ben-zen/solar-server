#pragma once

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

struct daytime_forecast {
    std::string timeframe;
    overall_condition condition;
    float temperature;

    std::string formatted() {
        return std::format("{}, {}, {}", timeframe, (int)condition, temperature);
    }
};

class weather_loader {
public:
  weather_loader(curl_handle &curl);
  std::string get_weather_data();
  std::vector<daytime_forecast> get_forecast();

private:
  curl_handle m_curl;

};
