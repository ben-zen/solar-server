#!/bin/sh

# Location arguments for NWS weather data.
# Override these environment variables to configure for a different location.
# Look up your location at: https://api.weather.gov/points/{latitude},{longitude}
NWS_STATION="${NWS_STATION:-KBFI}"
NWS_GRID_OFFICE="${NWS_GRID_OFFICE:-SEW}"
NWS_GRID_X="${NWS_GRID_X:-124}"
NWS_GRID_Y="${NWS_GRID_Y:-69}"

mkdir -p /temp/update-status && cd /temp/update-status || exit 1
/srv/logger/logger \
    --station "$NWS_STATION" \
    --grid-office "$NWS_GRID_OFFICE" \
    --grid-x "$NWS_GRID_X" \
    --grid-y "$NWS_GRID_Y" || exit 1

cp ./status.css /www/solar-site/css/status.css || exit 1
echo "Updated status.css"
