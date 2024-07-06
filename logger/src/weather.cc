#include <iostream>
#include <string>
#include <sstream>

#include <curl/curl.h>

#include "weather.hh"

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

std::string get_weather_data()
{
    CURL *curl = curl_easy_init();
    if (!curl)
    {
        std::cerr << "Error setting up curl." << std::endl;
        return "";
    }
    std::string forecast{};
    CURLcode res;
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "solar-website vNone, https://ben.zen.sdf.org");
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "application/geo+json");
    curl_easy_setopt(curl,
                     CURLOPT_WRITEFUNCTION, 
                     gather_response);
    curl_easy_setopt(curl,
                     CURLOPT_WRITEDATA,
                     &forecast);
    auto forecast_url = get_forecast_url();
    curl_easy_setopt(curl, CURLOPT_URL, forecast_url.c_str());
    std::cout << "Set URL to: " << forecast_url << std::endl;
    res = curl_easy_perform(curl);
    if (res != CURLE_OK)
    {
        std::cerr << "Error calling CURL to load weather: " << res << std::endl;
        return "";
    }

    std::cout << "Got full weather api response: " << std::endl
              << forecast << std::endl;

    // We got JSON. Now let's extract something from it.

    

    curl_easy_cleanup(curl);
    return "69";
}