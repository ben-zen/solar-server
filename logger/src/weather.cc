#include <iostream>
#include <string>

#include <curl/curl.h>

std::string get_weather_data()
{
    CURL *curl = curl_easy_init();
    if (!curl)
    {
        std::cerr << "Error setting up curl." << std::endl;
        return "";
    }

    CURLcode res;
    curl_easy_setopt(curl, CURLOPT_URL, "https://api.weather.gov/gridpoints/SEW/124,69/forecast");
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "solar-website vNone, https://ben.zen.sdf.org");
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "application/geo+json");
    // For testing I'm using the location of the space needle.
    // For realsies I want to first get the location using a call to:
    // https://api.weather.gov/points/47.620422,-122.349358

}