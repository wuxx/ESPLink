#!/bin/bash

#FIXME: please modify the path to your target dir
DIR=./

ESPTOOL=esptool.py

#SER_PORT=COM32 #on windows with git-bash
#SER_PORT=/dev/ttyUSB1
SER_PORT=/dev/ttyACM0

BAUDRATE=460800

${ESPTOOL} \
        --chip esp32 \
        -p ${SER_PORT} \
        -b ${BAUDRATE} \
        --before=default_reset \
        --after=hard_reset \
        write_flash \
        --flash_mode dio \
        --flash_freq 40m \
        --flash_size 2MB \
        0x8000  ${DIR}/partition-table.bin \
        0x1000  ${DIR}/bootloader.bin \
        0x10000 ${DIR}/dap-host-esp32.bin
