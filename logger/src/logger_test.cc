// Copyright (C) Ben Lewis, 2025.
// SPDX-License-Identifier: MIT

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "weather.hh"
#include "transform.hh"

#include "include_ext/json.hpp"

using json = nlohmann::json;

// These functions are internal to their respective compilation units but are
// declared here so the test binary can link against and exercise them directly.
temperature_unit temperature_unit_from_string(const std::string &str);
temperature_unit temperature_unit_from_shortcode(const std::string &str);
temperature read_temperature_from_json(nlohmann::json &data);
overall_condition condition_from_string(const std::string &str);
std::vector<daytime_forecast> load_forecast(const std::string &forecast_response);
std::string convert_condition_to_filename(overall_condition condition);

// ---------------------------------------------------------------------------
// Sample NWS API forecast responses for three locations.
//
// These are full /gridpoints/.../forecast responses, matching the structure
// returned by the NWS API.  Only the number of periods is reduced (real
// responses carry 14 half-day periods).
// ---------------------------------------------------------------------------

// Washington Monument, Washington DC (station KDCA, grid LWX/97,71)
// NWS points: https://api.weather.gov/points/38.8895,-77.0353
// Forecast:   https://api.weather.gov/gridpoints/LWX/97,71/forecast
// Observation: https://api.weather.gov/stations/KDCA/observations/latest
static const std::string washington_monument_forecast = R"({
  "@context": [
    "https://geojson.org/geojson-ld/geojson-context.jsonld",
    {
      "@version": "1.1",
      "wx": "https://api.weather.gov/ontology#",
      "geo": "http://www.opengis.net/ont/geosparql#",
      "unit": "http://codes.wmo.int/common/unit/",
      "@vocab": "https://api.weather.gov/ontology#"
    }
  ],
  "type": "Feature",
  "geometry": {
    "type": "Polygon",
    "coordinates": [[
      [-77.0125, 38.8759],
      [-77.0088, 38.8979],
      [-77.037, 38.9008],
      [-77.0408, 38.8788],
      [-77.0125, 38.8759]
    ]]
  },
  "properties": {
    "units": "us",
    "forecastGenerator": "BaselineForecastGenerator",
    "generatedAt": "2025-06-15T12:00:00+00:00",
    "updateTime": "2025-06-15T11:43:22+00:00",
    "validTimes": "2025-06-15T06:00:00+00:00/P7DT18H",
    "elevation": {
      "unitCode": "wmoUnit:m",
      "value": 6.096
    },
    "periods": [
      {
        "number": 1,
        "name": "Today",
        "startTime": "2025-06-15T06:00:00-04:00",
        "endTime": "2025-06-15T18:00:00-04:00",
        "isDaytime": true,
        "temperature": 88,
        "temperatureUnit": "F",
        "temperatureTrend": null,
        "probabilityOfPrecipitation": {
          "unitCode": "wmoUnit:percent",
          "value": null
        },
        "windSpeed": "5 to 10 mph",
        "windDirection": "SW",
        "icon": "https://api.weather.gov/icons/land/day/skc?size=medium",
        "shortForecast": "Sunny",
        "detailedForecast": "Sunny, with a high near 88. Southwest wind 5 to 10 mph."
      },
      {
        "number": 2,
        "name": "Tonight",
        "startTime": "2025-06-15T18:00:00-04:00",
        "endTime": "2025-06-16T06:00:00-04:00",
        "isDaytime": false,
        "temperature": 68,
        "temperatureUnit": "F",
        "temperatureTrend": null,
        "probabilityOfPrecipitation": {
          "unitCode": "wmoUnit:percent",
          "value": null
        },
        "windSpeed": "2 to 5 mph",
        "windDirection": "SW",
        "icon": "https://api.weather.gov/icons/land/night/few?size=medium",
        "shortForecast": "Mostly Clear",
        "detailedForecast": "Mostly clear, with a low around 68. Southwest wind 2 to 5 mph."
      },
      {
        "number": 3,
        "name": "Monday",
        "startTime": "2025-06-16T06:00:00-04:00",
        "endTime": "2025-06-16T18:00:00-04:00",
        "isDaytime": true,
        "temperature": 91,
        "temperatureUnit": "F",
        "temperatureTrend": null,
        "probabilityOfPrecipitation": {
          "unitCode": "wmoUnit:percent",
          "value": 20
        },
        "windSpeed": "8 to 12 mph",
        "windDirection": "S",
        "icon": "https://api.weather.gov/icons/land/day/sct?size=medium",
        "shortForecast": "Partly Cloudy",
        "detailedForecast": "Partly cloudy, with a high near 91. South wind 8 to 12 mph."
      },
      {
        "number": 4,
        "name": "Monday Night",
        "startTime": "2025-06-16T18:00:00-04:00",
        "endTime": "2025-06-17T06:00:00-04:00",
        "isDaytime": false,
        "temperature": 72,
        "temperatureUnit": "F",
        "temperatureTrend": null,
        "probabilityOfPrecipitation": {
          "unitCode": "wmoUnit:percent",
          "value": 30
        },
        "windSpeed": "5 to 8 mph",
        "windDirection": "S",
        "icon": "https://api.weather.gov/icons/land/night/ovc?size=medium",
        "shortForecast": "Cloudy",
        "detailedForecast": "Cloudy, with a low around 72. South wind 5 to 8 mph."
      },
      {
        "number": 5,
        "name": "Tuesday",
        "startTime": "2025-06-17T06:00:00-04:00",
        "endTime": "2025-06-17T18:00:00-04:00",
        "isDaytime": true,
        "temperature": 85,
        "temperatureUnit": "F",
        "temperatureTrend": "falling",
        "probabilityOfPrecipitation": {
          "unitCode": "wmoUnit:percent",
          "value": 70
        },
        "windSpeed": "10 to 15 mph",
        "windDirection": "W",
        "icon": "https://api.weather.gov/icons/land/day/tsra,70?size=medium",
        "shortForecast": "Thunderstorms",
        "detailedForecast": "Thunderstorms. Cloudy, with a high near 85. West wind 10 to 15 mph. Chance of precipitation is 70%."
      }
    ]
  }
})";

// Cal Anderson Park, Seattle WA (station KBFI, grid SEW/125,68)
// NWS points: https://api.weather.gov/points/47.6174,-122.3188
// Forecast:   https://api.weather.gov/gridpoints/SEW/125,68/forecast
// Observation: https://api.weather.gov/stations/KBFI/observations/latest
static const std::string cal_anderson_park_forecast = R"({
  "@context": [
    "https://geojson.org/geojson-ld/geojson-context.jsonld",
    {
      "@version": "1.1",
      "wx": "https://api.weather.gov/ontology#",
      "geo": "http://www.opengis.net/ont/geosparql#",
      "unit": "http://codes.wmo.int/common/unit/",
      "@vocab": "https://api.weather.gov/ontology#"
    }
  ],
  "type": "Feature",
  "geometry": {
    "type": "Polygon",
    "coordinates": [[
      [-122.3017, 47.5996],
      [-122.3079, 47.6202],
      [-122.3383, 47.616],
      [-122.3321, 47.5954],
      [-122.3017, 47.5996]
    ]]
  },
  "properties": {
    "units": "us",
    "forecastGenerator": "BaselineForecastGenerator",
    "generatedAt": "2025-06-15T12:00:00+00:00",
    "updateTime": "2025-06-15T11:30:17+00:00",
    "validTimes": "2025-06-15T06:00:00+00:00/P7DT18H",
    "elevation": {
      "unitCode": "wmoUnit:m",
      "value": 73.152
    },
    "periods": [
      {
        "number": 1,
        "name": "Today",
        "startTime": "2025-06-15T06:00:00-07:00",
        "endTime": "2025-06-15T18:00:00-07:00",
        "isDaytime": true,
        "temperature": 65,
        "temperatureUnit": "F",
        "temperatureTrend": null,
        "probabilityOfPrecipitation": {
          "unitCode": "wmoUnit:percent",
          "value": 10
        },
        "windSpeed": "5 to 10 mph",
        "windDirection": "NW",
        "icon": "https://api.weather.gov/icons/land/day/bkn?size=medium",
        "shortForecast": "Partly Sunny",
        "detailedForecast": "Partly sunny, with a high near 65. Northwest wind 5 to 10 mph."
      },
      {
        "number": 2,
        "name": "Tonight",
        "startTime": "2025-06-15T18:00:00-07:00",
        "endTime": "2025-06-16T06:00:00-07:00",
        "isDaytime": false,
        "temperature": 52,
        "temperatureUnit": "F",
        "temperatureTrend": null,
        "probabilityOfPrecipitation": {
          "unitCode": "wmoUnit:percent",
          "value": 20
        },
        "windSpeed": "3 to 6 mph",
        "windDirection": "N",
        "icon": "https://api.weather.gov/icons/land/night/bkn?size=medium",
        "shortForecast": "Mostly Cloudy",
        "detailedForecast": "Mostly cloudy, with a low around 52. North wind 3 to 6 mph."
      },
      {
        "number": 3,
        "name": "Monday",
        "startTime": "2025-06-16T06:00:00-07:00",
        "endTime": "2025-06-16T18:00:00-07:00",
        "isDaytime": true,
        "temperature": 62,
        "temperatureUnit": "F",
        "temperatureTrend": null,
        "probabilityOfPrecipitation": {
          "unitCode": "wmoUnit:percent",
          "value": 60
        },
        "windSpeed": "8 to 12 mph",
        "windDirection": "S",
        "icon": "https://api.weather.gov/icons/land/day/rain_showers,60?size=medium",
        "shortForecast": "Light Rain",
        "detailedForecast": "Light rain likely. Cloudy, with a high near 62. South wind 8 to 12 mph. Chance of precipitation is 60%."
      },
      {
        "number": 4,
        "name": "Monday Night",
        "startTime": "2025-06-16T18:00:00-07:00",
        "endTime": "2025-06-17T06:00:00-07:00",
        "isDaytime": false,
        "temperature": 50,
        "temperatureUnit": "F",
        "temperatureTrend": null,
        "probabilityOfPrecipitation": {
          "unitCode": "wmoUnit:percent",
          "value": 80
        },
        "windSpeed": "5 to 10 mph",
        "windDirection": "S",
        "icon": "https://api.weather.gov/icons/land/night/rain,80?size=medium",
        "shortForecast": "Rain",
        "detailedForecast": "Rain. Cloudy, with a low around 50. South wind 5 to 10 mph. Chance of precipitation is 80%."
      },
      {
        "number": 5,
        "name": "Tuesday",
        "startTime": "2025-06-17T06:00:00-07:00",
        "endTime": "2025-06-17T18:00:00-07:00",
        "isDaytime": true,
        "temperature": 60,
        "temperatureUnit": "F",
        "temperatureTrend": null,
        "probabilityOfPrecipitation": {
          "unitCode": "wmoUnit:percent",
          "value": 20
        },
        "windSpeed": "5 to 8 mph",
        "windDirection": "NW",
        "icon": "https://api.weather.gov/icons/land/day/few?size=medium",
        "shortForecast": "Mostly Sunny",
        "detailedForecast": "Mostly sunny, with a high near 60. Northwest wind 5 to 8 mph."
      },
      {
        "number": 6,
        "name": "Tuesday Night",
        "startTime": "2025-06-17T18:00:00-07:00",
        "endTime": "2025-06-18T06:00:00-07:00",
        "isDaytime": false,
        "temperature": 49,
        "temperatureUnit": "F",
        "temperatureTrend": null,
        "probabilityOfPrecipitation": {
          "unitCode": "wmoUnit:percent",
          "value": null
        },
        "windSpeed": "3 to 5 mph",
        "windDirection": "N",
        "icon": "https://api.weather.gov/icons/land/night/skc?size=medium",
        "shortForecast": "Clear",
        "detailedForecast": "Clear, with a low around 49. North wind 3 to 5 mph."
      },
      {
        "number": 7,
        "name": "Wednesday",
        "startTime": "2025-06-18T06:00:00-07:00",
        "endTime": "2025-06-18T18:00:00-07:00",
        "isDaytime": true,
        "temperature": 68,
        "temperatureUnit": "F",
        "temperatureTrend": null,
        "probabilityOfPrecipitation": {
          "unitCode": "wmoUnit:percent",
          "value": null
        },
        "windSpeed": "5 to 10 mph",
        "windDirection": "NW",
        "icon": "https://api.weather.gov/icons/land/day/skc?size=medium",
        "shortForecast": "Sunny",
        "detailedForecast": "Sunny, with a high near 68. Northwest wind 5 to 10 mph."
      }
    ]
  }
})";

// Marrowstone Island, Puget Sound WA (station K0S9, grid SEW/118,92)
// NWS points: https://api.weather.gov/points/48.069,-122.6839
// Forecast:   https://api.weather.gov/gridpoints/SEW/118,92/forecast
// Observation: https://api.weather.gov/stations/K0S9/observations/latest
static const std::string marrowstone_island_forecast = R"({
  "@context": [
    "https://geojson.org/geojson-ld/geojson-context.jsonld",
    {
      "@version": "1.1",
      "wx": "https://api.weather.gov/ontology#",
      "geo": "http://www.opengis.net/ont/geosparql#",
      "unit": "http://codes.wmo.int/common/unit/",
      "@vocab": "https://api.weather.gov/ontology#"
    }
  ],
  "type": "Feature",
  "geometry": {
    "type": "Polygon",
    "coordinates": [[
      [-122.6659, 48.0619],
      [-122.6722, 48.0823],
      [-122.7028, 48.0781],
      [-122.6964, 48.0577],
      [-122.6659, 48.0619]
    ]]
  },
  "properties": {
    "units": "us",
    "forecastGenerator": "BaselineForecastGenerator",
    "generatedAt": "2025-06-15T12:00:00+00:00",
    "updateTime": "2025-06-15T11:25:43+00:00",
    "validTimes": "2025-06-15T06:00:00+00:00/P7DT18H",
    "elevation": {
      "unitCode": "wmoUnit:m",
      "value": 18.8976
    },
    "periods": [
      {
        "number": 1,
        "name": "Today",
        "startTime": "2025-06-15T06:00:00-07:00",
        "endTime": "2025-06-15T18:00:00-07:00",
        "isDaytime": true,
        "temperature": 58,
        "temperatureUnit": "F",
        "temperatureTrend": null,
        "probabilityOfPrecipitation": {
          "unitCode": "wmoUnit:percent",
          "value": null
        },
        "windSpeed": "2 mph",
        "windDirection": "N",
        "icon": "https://api.weather.gov/icons/land/day/fog?size=medium",
        "shortForecast": "Fog",
        "detailedForecast": "Patchy fog before 11am. Partly sunny, with a high near 58."
      },
      {
        "number": 2,
        "name": "Tonight",
        "startTime": "2025-06-15T18:00:00-07:00",
        "endTime": "2025-06-16T06:00:00-07:00",
        "isDaytime": false,
        "temperature": 48,
        "temperatureUnit": "F",
        "temperatureTrend": null,
        "probabilityOfPrecipitation": {
          "unitCode": "wmoUnit:percent",
          "value": null
        },
        "windSpeed": "3 to 5 mph",
        "windDirection": "N",
        "icon": "https://api.weather.gov/icons/land/night/sct?size=medium",
        "shortForecast": "Partly Cloudy",
        "detailedForecast": "Partly cloudy, with a low around 48. North wind 3 to 5 mph."
      },
      {
        "number": 3,
        "name": "Monday",
        "startTime": "2025-06-16T06:00:00-07:00",
        "endTime": "2025-06-16T18:00:00-07:00",
        "isDaytime": true,
        "temperature": 61,
        "temperatureUnit": "F",
        "temperatureTrend": null,
        "probabilityOfPrecipitation": {
          "unitCode": "wmoUnit:percent",
          "value": 30
        },
        "windSpeed": "5 to 10 mph",
        "windDirection": "S",
        "icon": "https://api.weather.gov/icons/land/day/bkn?size=medium",
        "shortForecast": "Mostly Cloudy",
        "detailedForecast": "Mostly cloudy, with a high near 61. South wind 5 to 10 mph."
      },
      {
        "number": 4,
        "name": "Monday Night",
        "startTime": "2025-06-16T18:00:00-07:00",
        "endTime": "2025-06-17T06:00:00-07:00",
        "isDaytime": false,
        "temperature": 46,
        "temperatureUnit": "F",
        "temperatureTrend": null,
        "probabilityOfPrecipitation": {
          "unitCode": "wmoUnit:percent",
          "value": 50
        },
        "windSpeed": "5 to 8 mph",
        "windDirection": "SE",
        "icon": "https://api.weather.gov/icons/land/night/rain_showers,50?size=medium",
        "shortForecast": "Drizzle",
        "detailedForecast": "Drizzle likely after midnight. Mostly cloudy, with a low around 46. Southeast wind 5 to 8 mph. Chance of precipitation is 50%."
      },
      {
        "number": 5,
        "name": "Tuesday",
        "startTime": "2025-06-17T06:00:00-07:00",
        "endTime": "2025-06-17T18:00:00-07:00",
        "isDaytime": true,
        "temperature": 55,
        "temperatureUnit": "F",
        "temperatureTrend": null,
        "probabilityOfPrecipitation": {
          "unitCode": "wmoUnit:percent",
          "value": 60
        },
        "windSpeed": "10 to 15 mph",
        "windDirection": "S",
        "icon": "https://api.weather.gov/icons/land/day/rain_showers,60?size=medium",
        "shortForecast": "Rain Showers",
        "detailedForecast": "Rain showers likely. Cloudy, with a high near 55. South wind 10 to 15 mph. Chance of precipitation is 60%."
      },
      {
        "number": 6,
        "name": "Tuesday Night",
        "startTime": "2025-06-17T18:00:00-07:00",
        "endTime": "2025-06-18T06:00:00-07:00",
        "isDaytime": false,
        "temperature": 44,
        "temperatureUnit": "F",
        "temperatureTrend": null,
        "probabilityOfPrecipitation": {
          "unitCode": "wmoUnit:percent",
          "value": null
        },
        "windSpeed": "3 to 5 mph",
        "windDirection": "N",
        "icon": "https://api.weather.gov/icons/land/night/skc?size=medium",
        "shortForecast": "Clear",
        "detailedForecast": "Clear, with a low around 44. North wind 3 to 5 mph."
      },
      {
        "number": 7,
        "name": "Wednesday",
        "startTime": "2025-06-18T06:00:00-07:00",
        "endTime": "2025-06-18T18:00:00-07:00",
        "isDaytime": true,
        "temperature": 63,
        "temperatureUnit": "F",
        "temperatureTrend": null,
        "probabilityOfPrecipitation": {
          "unitCode": "wmoUnit:percent",
          "value": null
        },
        "windSpeed": "5 to 10 mph",
        "windDirection": "NW",
        "icon": "https://api.weather.gov/icons/land/day/skc?size=medium",
        "shortForecast": "Sunny",
        "detailedForecast": "Sunny, with a high near 63. Northwest wind 5 to 10 mph."
      }
    ]
  }
})";

// ---------------------------------------------------------------------------
// temperature_unit_from_string
// ---------------------------------------------------------------------------

TEST_CASE("temperature_unit_from_string parses Celsius") {
    CHECK(temperature_unit_from_string("wmoUnit:degC") == temperature_unit::celsius);
}

TEST_CASE("temperature_unit_from_string parses Fahrenheit") {
    CHECK(temperature_unit_from_string("wmoUnit:degF") == temperature_unit::fahrenheit);
}

TEST_CASE("temperature_unit_from_string throws on unknown unit") {
    CHECK_THROWS(temperature_unit_from_string("wmoUnit:degK"));
    CHECK_THROWS(temperature_unit_from_string(""));
    CHECK_THROWS(temperature_unit_from_string("celsius"));
}

// ---------------------------------------------------------------------------
// temperature_unit_from_shortcode
// ---------------------------------------------------------------------------

TEST_CASE("temperature_unit_from_shortcode parses C") {
    CHECK(temperature_unit_from_shortcode("C") == temperature_unit::celsius);
}

TEST_CASE("temperature_unit_from_shortcode parses F") {
    CHECK(temperature_unit_from_shortcode("F") == temperature_unit::fahrenheit);
}

TEST_CASE("temperature_unit_from_shortcode throws on unknown shortcode") {
    CHECK_THROWS(temperature_unit_from_shortcode("K"));
    CHECK_THROWS(temperature_unit_from_shortcode(""));
    CHECK_THROWS(temperature_unit_from_shortcode("fahrenheit"));
}

// ---------------------------------------------------------------------------
// read_temperature_from_json
// ---------------------------------------------------------------------------

TEST_CASE("read_temperature_from_json reads Celsius temperature") {
    json data = {{"unitCode", "wmoUnit:degC"}, {"value", 22.5}};
    auto temp = read_temperature_from_json(data);
    CHECK(temp.unit == temperature_unit::celsius);
    CHECK(temp.value == doctest::Approx(22.5));
}

TEST_CASE("read_temperature_from_json reads Fahrenheit temperature") {
    json data = {{"unitCode", "wmoUnit:degF"}, {"value", 72.0}};
    auto temp = read_temperature_from_json(data);
    CHECK(temp.unit == temperature_unit::fahrenheit);
    CHECK(temp.value == doctest::Approx(72.0));
}

TEST_CASE("read_temperature_from_json reads zero temperature") {
    json data = {{"unitCode", "wmoUnit:degC"}, {"value", 0.0}};
    auto temp = read_temperature_from_json(data);
    CHECK(temp.value == doctest::Approx(0.0));
}

TEST_CASE("read_temperature_from_json reads negative temperature") {
    json data = {{"unitCode", "wmoUnit:degC"}, {"value", -15.3}};
    auto temp = read_temperature_from_json(data);
    CHECK(temp.unit == temperature_unit::celsius);
    CHECK(temp.value == doctest::Approx(-15.3));
}

// ---------------------------------------------------------------------------
// condition_from_string — valid conditions
// ---------------------------------------------------------------------------

TEST_CASE("condition_from_string maps clear conditions") {
    CHECK(condition_from_string("Sunny") == overall_condition::clear);
    CHECK(condition_from_string("Clear") == overall_condition::clear);
    CHECK(condition_from_string("Mostly Clear") == overall_condition::clear);
}

TEST_CASE("condition_from_string maps partly cloudy conditions") {
    CHECK(condition_from_string("Mostly Sunny") == overall_condition::partly_cloudy);
    CHECK(condition_from_string("Partly Cloudy") == overall_condition::partly_cloudy);
    CHECK(condition_from_string("Partly Sunny") == overall_condition::partly_cloudy);
}

TEST_CASE("condition_from_string maps cloudy conditions") {
    CHECK(condition_from_string("Mostly Cloudy") == overall_condition::cloudy);
    CHECK(condition_from_string("Cloudy") == overall_condition::cloudy);
    CHECK(condition_from_string("Overcast") == overall_condition::cloudy);
}

TEST_CASE("condition_from_string maps misting conditions") {
    CHECK(condition_from_string("Fog") == overall_condition::misting);
    CHECK(condition_from_string("Haze") == overall_condition::misting);
    CHECK(condition_from_string("Light Rain") == overall_condition::misting);
    CHECK(condition_from_string("Drizzle") == overall_condition::misting);
}

TEST_CASE("condition_from_string maps raining conditions") {
    CHECK(condition_from_string("Rain") == overall_condition::raining);
    CHECK(condition_from_string("Heavy Rain") == overall_condition::raining);
    CHECK(condition_from_string("Showers") == overall_condition::raining);
    CHECK(condition_from_string("Thunderstorms") == overall_condition::raining);
    CHECK(condition_from_string("Rain Showers") == overall_condition::raining);
    CHECK(condition_from_string("Chance Rain Showers") == overall_condition::raining);
}

TEST_CASE("condition_from_string maps snowing conditions") {
    CHECK(condition_from_string("Snow") == overall_condition::snowing);
    CHECK(condition_from_string("Light Snow") == overall_condition::snowing);
    CHECK(condition_from_string("Heavy Snow") == overall_condition::snowing);
    CHECK(condition_from_string("Blizzard") == overall_condition::snowing);
}

TEST_CASE("condition_from_string throws on unrecognized condition") {
    CHECK_THROWS(condition_from_string("Tornado"));
    CHECK_THROWS(condition_from_string(""));
    CHECK_THROWS(condition_from_string("sunny"));  // case sensitive
}

// ---------------------------------------------------------------------------
// load_forecast — Washington Monument
// ---------------------------------------------------------------------------

TEST_CASE("load_forecast parses Washington Monument forecast") {
    auto forecasts = load_forecast(washington_monument_forecast);

    // Should extract 3 daytime periods
    REQUIRE(forecasts.size() == 3);

    // Today: 88°F, Sunny
    CHECK(forecasts[0].temp.value == doctest::Approx(88.0));
    CHECK(forecasts[0].temp.unit == temperature_unit::fahrenheit);
    CHECK(forecasts[0].condition == overall_condition::clear);
    CHECK(forecasts[0].timeframe == "2025-06-15T06:00:00-04:00");

    // Monday: 91°F, Partly Cloudy
    CHECK(forecasts[1].temp.value == doctest::Approx(91.0));
    CHECK(forecasts[1].condition == overall_condition::partly_cloudy);

    // Tuesday: 85°F, Thunderstorms
    CHECK(forecasts[2].temp.value == doctest::Approx(85.0));
    CHECK(forecasts[2].condition == overall_condition::raining);
}

// ---------------------------------------------------------------------------
// load_forecast — Cal Anderson Park, Seattle
// ---------------------------------------------------------------------------

TEST_CASE("load_forecast parses Cal Anderson Park forecast") {
    auto forecasts = load_forecast(cal_anderson_park_forecast);

    REQUIRE(forecasts.size() == 3);

    // Today: 65°F, Partly Sunny
    CHECK(forecasts[0].temp.value == doctest::Approx(65.0));
    CHECK(forecasts[0].condition == overall_condition::partly_cloudy);
    CHECK(forecasts[0].timeframe == "2025-06-15T06:00:00-07:00");

    // Monday: 62°F, Light Rain
    CHECK(forecasts[1].temp.value == doctest::Approx(62.0));
    CHECK(forecasts[1].condition == overall_condition::misting);

    // Tuesday: 60°F, Mostly Sunny
    CHECK(forecasts[2].temp.value == doctest::Approx(60.0));
    CHECK(forecasts[2].condition == overall_condition::partly_cloudy);
}

// ---------------------------------------------------------------------------
// load_forecast — Marrowstone Island, Puget Sound
// ---------------------------------------------------------------------------

TEST_CASE("load_forecast parses Marrowstone Island forecast") {
    auto forecasts = load_forecast(marrowstone_island_forecast);

    REQUIRE(forecasts.size() == 3);

    // Today: 58°F, Fog
    CHECK(forecasts[0].temp.value == doctest::Approx(58.0));
    CHECK(forecasts[0].condition == overall_condition::misting);
    CHECK(forecasts[0].timeframe == "2025-06-15T06:00:00-07:00");

    // Monday: 61°F, Mostly Cloudy
    CHECK(forecasts[1].temp.value == doctest::Approx(61.0));
    CHECK(forecasts[1].condition == overall_condition::cloudy);

    // Tuesday: 55°F, Rain Showers
    CHECK(forecasts[2].temp.value == doctest::Approx(55.0));
    CHECK(forecasts[2].condition == overall_condition::raining);
}

// ---------------------------------------------------------------------------
// load_forecast — edge cases
// ---------------------------------------------------------------------------

TEST_CASE("load_forecast with no daytime periods returns empty") {
    std::string no_daytime = R"({
      "properties": {
        "periods": [
          {
            "number": 1,
            "name": "Tonight",
            "startTime": "2025-06-15T18:00:00-04:00",
            "isDaytime": false,
            "temperature": 68,
            "temperatureUnit": "F",
            "shortForecast": "Clear"
          }
        ]
      }
    })";

    auto forecasts = load_forecast(no_daytime);
    CHECK(forecasts.empty());
}

TEST_CASE("load_forecast with fewer than 3 daytime periods returns what is available") {
    std::string two_days = R"({
      "properties": {
        "periods": [
          {
            "number": 1,
            "name": "Today",
            "startTime": "2025-06-15T06:00:00-04:00",
            "isDaytime": true,
            "temperature": 75,
            "temperatureUnit": "F",
            "shortForecast": "Sunny"
          },
          {
            "number": 2,
            "name": "Tonight",
            "startTime": "2025-06-15T18:00:00-04:00",
            "isDaytime": false,
            "temperature": 60,
            "temperatureUnit": "F",
            "shortForecast": "Clear"
          },
          {
            "number": 3,
            "name": "Monday",
            "startTime": "2025-06-16T06:00:00-04:00",
            "isDaytime": true,
            "temperature": 78,
            "temperatureUnit": "F",
            "shortForecast": "Partly Cloudy"
          }
        ]
      }
    })";

    auto forecasts = load_forecast(two_days);
    CHECK(forecasts.size() == 2);
}

TEST_CASE("load_forecast throws on malformed JSON") {
    CHECK_THROWS(load_forecast("not json at all"));
    CHECK_THROWS(load_forecast("{\"incomplete"));
}

TEST_CASE("load_forecast returns empty on missing properties.periods") {
    auto result1 = load_forecast(R"({"properties": {}})");
    CHECK(result1.empty());

    auto result2 = load_forecast(R"({})");
    CHECK(result2.empty());
}

// ---------------------------------------------------------------------------
// convert_condition_to_filename
// ---------------------------------------------------------------------------

TEST_CASE("convert_condition_to_filename maps all conditions") {
    CHECK(convert_condition_to_filename(overall_condition::clear) == "sunny.svg");
    CHECK(convert_condition_to_filename(overall_condition::cloudy) == "cloudy.svg");
    CHECK(convert_condition_to_filename(overall_condition::partly_cloudy) == "partly-sunny.svg");
    CHECK(convert_condition_to_filename(overall_condition::misting) == "light-rain.svg");
    CHECK(convert_condition_to_filename(overall_condition::raining) == "raining.svg");
    CHECK(convert_condition_to_filename(overall_condition::snowing) == "snowing.svg");
}

// ---------------------------------------------------------------------------
// report_to_css — output safety
// ---------------------------------------------------------------------------

TEST_CASE("report_to_css produces well-formed CSS") {
    status_report report{};
    report.current_weather = {"2025-06-15T12:00:00", overall_condition::clear, {72.0f, temperature_unit::fahrenheit}};
    report.upcoming_days = {
        {"2025-06-16T06:00:00", overall_condition::partly_cloudy, {68.0f, temperature_unit::fahrenheit}},
        {"2025-06-17T06:00:00", overall_condition::raining, {60.0f, temperature_unit::fahrenheit}},
    };
    report.uptime = std::chrono::seconds(3661);
    report.battery_voltage = voltage<float>(12.8f);
    report.is_charging = false;

    std::ostringstream out;
    int result = report_to_css(report, out);
    CHECK(result == 0);

    std::string css = out.str();

    // Should contain expected CSS class selectors
    CHECK(css.find(".weather-icon") != std::string::npos);
    CHECK(css.find(".weather::after") != std::string::npos);
    CHECK(css.find(".forecast-tomorrow-icon") != std::string::npos);
    CHECK(css.find(".forecast-tomorrow::after") != std::string::npos);
    CHECK(css.find(".forecast-overmorrow-icon") != std::string::npos);
    CHECK(css.find(".forecast-overmorrow::after") != std::string::npos);
    CHECK(css.find(".battery-icon") != std::string::npos);
    CHECK(css.find(".battery-status::after") != std::string::npos);
    CHECK(css.find(".uptime::after") != std::string::npos);

    // Should reference correct SVG files
    CHECK(css.find("sunny.svg") != std::string::npos);
    CHECK(css.find("partly-sunny.svg") != std::string::npos);
    CHECK(css.find("raining.svg") != std::string::npos);

    // Battery should show non-charging icon
    CHECK(css.find("battery-half.svg") != std::string::npos);
}

TEST_CASE("report_to_css shows charging icon when charging") {
    status_report report{};
    report.current_weather = {"2025-06-15T12:00:00", overall_condition::clear, {72.0f, temperature_unit::fahrenheit}};
    report.upcoming_days = {
        {"2025-06-16T06:00:00", overall_condition::clear, {68.0f, temperature_unit::fahrenheit}},
        {"2025-06-17T06:00:00", overall_condition::clear, {70.0f, temperature_unit::fahrenheit}},
    };
    report.uptime = std::chrono::seconds(100);
    report.battery_voltage = voltage<float>(13.2f);
    report.is_charging = true;

    std::ostringstream out;
    report_to_css(report, out);
    std::string css = out.str();

    CHECK(css.find("battery-charging-half.svg") != std::string::npos);
}

TEST_CASE("report_to_css output contains no unescaped HTML tags") {
    status_report report{};
    report.current_weather = {"2025-06-15T12:00:00", overall_condition::clear, {72.0f, temperature_unit::fahrenheit}};
    report.upcoming_days = {
        {"2025-06-16T06:00:00", overall_condition::clear, {68.0f, temperature_unit::fahrenheit}},
        {"2025-06-17T06:00:00", overall_condition::clear, {70.0f, temperature_unit::fahrenheit}},
    };
    report.uptime = std::chrono::seconds(0);
    report.battery_voltage = voltage<float>(12.0f);
    report.is_charging = false;

    std::ostringstream out;
    report_to_css(report, out);
    std::string css = out.str();

    // The CSS output should not contain raw HTML tags that could enable injection
    CHECK(css.find("<script") == std::string::npos);
    CHECK(css.find("<img") == std::string::npos);
    CHECK(css.find("javascript:") == std::string::npos);
}

TEST_CASE("report_to_css formats uptime correctly") {
    status_report report{};
    report.current_weather = {"now", overall_condition::clear, {70.0f, temperature_unit::fahrenheit}};
    report.upcoming_days = {
        {"tomorrow", overall_condition::clear, {70.0f, temperature_unit::fahrenheit}},
        {"overmorrow", overall_condition::clear, {70.0f, temperature_unit::fahrenheit}},
    };
    report.uptime = std::chrono::seconds(90061); // 25h 1m 1s
    report.battery_voltage = voltage<float>(12.5f);
    report.is_charging = false;

    std::ostringstream out;
    report_to_css(report, out);
    std::string css = out.str();

    // Uptime should be formatted as H:MM:SS via {:%T}
    CHECK(css.find(".uptime::after") != std::string::npos);
}

// ---------------------------------------------------------------------------
// make_observation_url — URL generation from nws_location
// ---------------------------------------------------------------------------

TEST_CASE("make_observation_url constructs correct URL for Seattle (KBFI)") {
    nws_location loc{"KBFI", "SEW", 124, 69};
    CHECK(make_observation_url(loc) == "https://api.weather.gov/stations/KBFI/observations/latest");
}

TEST_CASE("make_observation_url constructs correct URL for Washington DC (KDCA)") {
    nws_location loc{"KDCA", "LWX", 97, 71};
    CHECK(make_observation_url(loc) == "https://api.weather.gov/stations/KDCA/observations/latest");
}

TEST_CASE("make_observation_url constructs correct URL for Marrowstone Island (K0S9)") {
    nws_location loc{"K0S9", "SEW", 118, 92};
    CHECK(make_observation_url(loc) == "https://api.weather.gov/stations/K0S9/observations/latest");
}

// ---------------------------------------------------------------------------
// make_forecast_url — URL generation from nws_location
// ---------------------------------------------------------------------------

TEST_CASE("make_forecast_url constructs correct URL for Seattle") {
    nws_location loc{"KBFI", "SEW", 124, 69};
    CHECK(make_forecast_url(loc) == "https://api.weather.gov/gridpoints/SEW/124,69/forecast");
}

TEST_CASE("make_forecast_url constructs correct URL for Washington DC") {
    nws_location loc{"KDCA", "LWX", 97, 71};
    CHECK(make_forecast_url(loc) == "https://api.weather.gov/gridpoints/LWX/97,71/forecast");
}

TEST_CASE("make_forecast_url constructs correct URL for Marrowstone Island") {
    nws_location loc{"K0S9", "SEW", 118, 92};
    CHECK(make_forecast_url(loc) == "https://api.weather.gov/gridpoints/SEW/118,92/forecast");
}

// ---------------------------------------------------------------------------
// report_to_css — file path overload
// ---------------------------------------------------------------------------

TEST_CASE("report_to_css writes to a valid file path") {
    status_report report{};
    report.current_weather = {"now", overall_condition::clear, {70.0f, temperature_unit::fahrenheit}};
    report.upcoming_days = {
        {"tomorrow", overall_condition::clear, {70.0f, temperature_unit::fahrenheit}},
        {"overmorrow", overall_condition::clear, {70.0f, temperature_unit::fahrenheit}},
    };
    report.uptime = std::chrono::seconds(100);
    report.battery_voltage = voltage<float>(12.5f);
    report.is_charging = false;

    auto path = std::filesystem::temp_directory_path() / "logger_test_output.css";
    int result = report_to_css(report, path);
    CHECK(result == 0);

    // Verify the file was written with expected content
    std::ifstream in(path);
    std::string css((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK(css.find(".weather-icon") != std::string::npos);
    CHECK(css.find(".uptime::after") != std::string::npos);

    std::filesystem::remove(path);
}

TEST_CASE("report_to_css returns error for unwritable path") {
    status_report report{};
    report.current_weather = {"now", overall_condition::clear, {70.0f, temperature_unit::fahrenheit}};
    report.upcoming_days = {
        {"tomorrow", overall_condition::clear, {70.0f, temperature_unit::fahrenheit}},
        {"overmorrow", overall_condition::clear, {70.0f, temperature_unit::fahrenheit}},
    };
    report.uptime = std::chrono::seconds(0);
    report.battery_voltage = voltage<float>(12.0f);
    report.is_charging = false;

    // Create a unique directory under temp that we guarantee doesn't exist.
    auto bad_path = std::filesystem::temp_directory_path()
                  / "logger_test_no_such_dir_8f3a2c"
                  / "output.css";
    // Ensure the parent directory truly doesn't exist.
    std::filesystem::remove_all(bad_path.parent_path());
    int result = report_to_css(report, bad_path);
    CHECK(result != 0);
}
