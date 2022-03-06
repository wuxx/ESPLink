#!/bin/bash

#FIXME: please modify the path to your target dir
DIR=.

TARGET=${DIR}/test.bin

#SER_PORT=COM32 #on windows with git-bash
#SER_PORT=/dev/ttyUSB0
SER_PORT=/dev/ttyACM0

#BAUDRATE=460800
BAUDRATE=921600

#esp8266 32Mbit/4MByte
esptool.py --port ${SER_PORT} \
        --baud ${BAUDRATE} write_flash \
        -fs 32m -ff 80m \
        0x00000 ${TARGET}
