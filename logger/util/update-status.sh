#!/bin/sh

# Location arguments for NWS weather data.
# Override these environment variables to configure for a different location.
# Look up your location at: https://api.weather.gov/points/{latitude},{longitude}
NWS_STATION="${NWS_STATION:-KBFI}"
NWS_GRID_OFFICE="${NWS_GRID_OFFICE:-SEW}"
NWS_GRID_X="${NWS_GRID_X:-124}"
NWS_GRID_Y="${NWS_GRID_Y:-69}"

mkdir /temp/update-status && pushd /temp/update-status
/srv/logger/logger \
    --station "$NWS_STATION" \
    --grid-office "$NWS_GRID_OFFICE" \
    --grid-x "$NWS_GRID_X" \
    --grid-y "$NWS_GRID_Y"

cp ./status.css /www/solar-site/css/status.css
echo "Updated status.css"
