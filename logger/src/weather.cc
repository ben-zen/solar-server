#include <algorithm>
#include <exception>
#include <format>
#include <iostream>
#include <string>
#include <sstream>

#include <curl/curl.h>

#include "include_ext/json.hpp"
#include "weather.hh"

using json = nlohmann::json;

std::string get_observation_url()
{
    // For testing I'm using the location of the space needle.
    // For realsies I want to first get the location using a call the points endpoint:
    // https://api.weather.gov/points/47.620422,-122.349358
    //
    // This will involve either attaching a GPS module (sure) or some other means of
    // updating location.

    return "https://api.weather.gov/stations/KBFI/observations/latest";

}

std::string get_forecast_url()
{
    // For testing I'm using the location of the space needle.
    // For realsies I want to first get the location using a call the points endpoint:
    // https://api.weather.gov/points/47.620422,-122.349358
    //
    // This will involve either attaching a GPS module (sure) or some other means of
    // updating location.

    return "https://api.weather.gov/gridpoints/SEW/124,69/forecast";
}


extern "C"
size_t gather_response(void *buffer,
                       size_t size,
                       size_t nmemb,
                       void *userp)
{
    std::string_view buffer_data{(const char*)buffer, nmemb};
    std::string *response = (std::string *)userp;

    std::cout << "Received response chunk, size: " << nmemb << " bytes:" << std::endl
              << buffer_data << std::endl;
    try {
        response->append(buffer_data);
    } catch (std::exception &e)
    {
        std::cerr << "Error appending data: " << e.what() << std::endl;
        return CURL_WRITEFUNC_ERROR;
    }

    std::cout << "Returning " << nmemb << " bytes written." << std::endl;
    return nmemb;
}

overall_condition condition_from_string(const std::string &str) {
    static std::map<std::string, overall_condition> relation {
        {"Sunny", overall_condition::clear},
        {"Clear", overall_condition::clear},
    };

    if (!relation.contains(str))
    {
        throw new std::domain_error {std::format("incorrect condition supplied: {}", str)};
    }

    return relation[str];
}

std::vector<daytime_forecast> load_forecast(const std::string &forecast_response)
{
    json forecast_data = json::parse(forecast_response);
    auto forecast = forecast_data["properties"]["periods"];

    auto forecast_iter = forecast.cbegin();
    // where entries have "isDaytime": true, take three.
    std::vector<daytime_forecast> daytime_forecasts{};
    auto is_daytime = [](json j) {
        std::cout << "is_daytime(): " << j.contains("isDaytime") << " , " << j["isDaytime"] << std::endl;
        return j.contains("isDaytime") && j["isDaytime"].template get<bool>();
    };

    do {
        auto forecast_local = std::find_if(forecast_iter, forecast.cend(), is_daytime);

        if (forecast_local == forecast.cend())
        {
            std::cerr << "ran out of daytimes." << std::endl;
            break;
        }

        std::cout << "forecast found: " << *forecast_iter << std::endl;

        daytime_forecasts.emplace_back(forecast_local->at("startTime").template get<std::string>(),
                                       condition_from_string(
                                            forecast_local->at("shortForecast").template get<std::string>()),
                                       forecast_local->at("temperature").template get<float>());

        // skip past the current 
        forecast_iter = ++forecast_local;
    } while (daytime_forecasts.size() < 3 && forecast_iter != forecast.cend());

    return daytime_forecasts;
}

std::string weather_loader::get_weather_data()
{
    return "69";
}

weather_loader::weather_loader(curl_handle &curl) : m_curl(curl) {
    // Also needs the location of the station.
    curl_easy_setopt(m_curl, CURLOPT_USERAGENT, "solar-website vNone, https://ben.zen.sdf.org");
    curl_easy_setopt(m_curl, CURLOPT_ACCEPT_ENCODING, "application/geo+json");
    curl_easy_setopt(m_curl,
                     CURLOPT_WRITEFUNCTION, 
                     gather_response);
}

std::vector<daytime_forecast> weather_loader::get_forecast() {
    CURLcode res;
    std::string forecast_str{};
    auto forecast_url = get_forecast_url();
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &forecast_str);
    curl_easy_setopt(m_curl, CURLOPT_URL, forecast_url.c_str());
    std::cout << "Set URL to: " << forecast_url << std::endl;
    res = curl_easy_perform(m_curl);
    if (res != CURLE_OK)
    {
        std::cerr << "Error calling CURL to load weather: " << res << std::endl;
        return {};
    }

    // We got JSON. Now let's extract something from it.
    auto forecast = load_forecast(forecast_str);

    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, nullptr);

    return forecast;
}