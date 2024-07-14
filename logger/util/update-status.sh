#!/bin/sh

mkdir /temp/update-status && pushd /temp/update-status
/srv/logger/logger

cp ./status.css /www/solar-site/css/status.css
echo "Updated status.css"
