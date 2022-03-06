#!/bin/bash

if [ ${#1} -eq 0 ]; then
    echo "$0 ESP8266/ESP32/ESP32-C3/ESP32-S2/ESP32-S3"
    exit 1
fi

TARGET=$1

esplink-tool -p | grep ${TARGET}
if [ $? -eq 0 ]; then
   exit 0
else
   exit 1
fi
