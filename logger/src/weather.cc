#include <algorithm>
#include <exception>
#include <iostream>
#include <string>

#include <curl/curl.h>
#include <fmt/format.h>

#include "include_ext/json.hpp"
#include "weather.hh"

using json = nlohmann::json;

temperature_unit temperature_unit_from_string(const std::string &str) {
    if (str.compare("wmoUnit:degC") == 0) {
        return temperature_unit::celsius;
    } else if (str.compare("wmoUnit:degF") == 0) {
        return temperature_unit::fahrenheit;
    }

    throw new std::domain_error(fmt::format("Invalid unit string for temperature: {}", str));
}

temperature_unit temperature_unit_from_shortcode(const std::string &str) {
    if (str.compare("C") == 0) {
        return temperature_unit::celsius;
    } else if (str.compare("F") == 0) {
        return temperature_unit::fahrenheit;
    }

    throw new std::domain_error(fmt::format("Unknown short temp code: {}", str));
}

temperature read_temperature_from_json(json &data) {
    temperature_unit unit{temperature_unit_from_string(data["unitCode"].template get<std::string>())};
    float value{data["value"].template get<float>()};

    return {value, unit};
}

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
    std::string *response = (std::string *)userp;

    try {
        response->append((char *)buffer, nmemb);
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

    if (relation.find(str) == relation.end())
    {
        throw new std::domain_error {fmt::format("incorrect condition supplied: {}", str)};
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

        daytime_forecasts.emplace_back(
            forecast_local->at("startTime").template get<std::string>(),
            condition_from_string(forecast_local->at("shortForecast").template get<std::string>()),
            temperature{forecast_local->at("temperature").template get<float>(),
                        temperature_unit_from_shortcode(
                            forecast_local->at("temperatureUnit").template get<std::string>())});

        // skip past the current 
        forecast_iter = ++forecast_local;
    } while (daytime_forecasts.size() < 3 && forecast_iter != forecast.cend());

    return daytime_forecasts;
}

CURLcode weather_loader::nws_api_call(std::string &url_str, std::string *buffer) {
    CURLcode res;

    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, buffer);
    curl_easy_setopt(m_curl, CURLOPT_URL, url_str.c_str());
    std::cout << "Set URL to: " << url_str << std::endl;
    res = curl_easy_perform(m_curl);
    if (res != CURLE_OK)
    {
        std::cerr << "Error calling CURL to load weather: " << res << std::endl;
        return res;
    }

    // Need to figure out how to handle cleaning up CURLOPT_WRITEDATA.

    return res;
}

weather_loader::weather_loader(curl_handle &curl) : m_curl(curl) {
    // Also needs the location of the station.
    curl_easy_setopt(m_curl, CURLOPT_USERAGENT, "solar-website vNone, https://ben.zen.sdf.org");
    curl_easy_setopt(m_curl, CURLOPT_ACCEPT_ENCODING, "application/geo+json");
    curl_easy_setopt(m_curl,
                     CURLOPT_WRITEFUNCTION, 
                     gather_response);
}

daytime_forecast weather_loader::get_current_observation() {
    CURLcode res;
    std::string observation_str{};
    auto observation_url = get_observation_url();
    res = nws_api_call(observation_url, &observation_str);
    if (res != CURLE_OK)
    {
        std::cerr << "CURL error. URL: " << observation_url << " Code: " << res << std::endl;
        return {};
    }

    daytime_forecast observation{};
    json observation_data = json::parse(observation_str);
    try
    {
        observation.condition = condition_from_string(observation_data["properties"]["textDescription"].template get<std::string>());
        observation.temp = read_temperature_from_json(observation_data["properties"]["temperature"]);
        observation.timeframe = observation_data["properties"]["timestamp"].template get<std::string>();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return {};
    }
    
    return observation;
}

std::vector<daytime_forecast> weather_loader::get_forecast() {
    CURLcode res;
    std::string forecast_str{};
    auto forecast_url = get_forecast_url();
    res = nws_api_call(forecast_url, &forecast_str);
    if (res != CURLE_OK)
    {
        std::cerr << "CURL error. URL: " << forecast_url << " Code: " << res << std::endl;
        return {};
    }

    // We got JSON. Now let's extract something from it.
    auto forecast = load_forecast(forecast_str);

    return forecast;
}
