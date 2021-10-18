#!/bin/bash

if  [ ${#1} -eq 0 ] || 
    [ ${#2} -eq 0 ] || 
    [ ${#3} -eq 0 ] || 
    [ ${#4} -eq 0 ] || 
    [ ${#5} -eq 0 ] ; then
    echo "usage: esppad.sh esp8266/esp32/esp32s2/esp32c3/esp32s3 bootloader.bin partition-table.bin app.bin flash_image.bin"
    exit 0
fi

if [ "$1" == "esp32c3" ] || [ "$1" == "esp32s3" ]; then
    #ESP32-C3/ESP32-S3
    ((BOOTLOADER_ADDR=0x0000))
    ((PTABLE_ADDR=0x8000))
    ((APPIMAGE_ADDR=0x10000))

    PAD_FILE=pad_0.bin
    dd if=/dev/zero of=${PAD_FILE} bs=512 count=0

elif [ "$1" == "esp32" ] || [ "$1" == "esp32s2" ] ; then
    #ESP32/ESP32-S2
    ((BOOTLOADER_ADDR=0x1000))
    ((PTABLE_ADDR=0x8000))
    ((APPIMAGE_ADDR=0x10000))

    PAD_FILE=pad_4k.bin
    dd if=/dev/zero bs=512 count=8 | tr "\000" "\377" > ${PAD_FILE}

fi

BOOTLOADER_FILE=$2
PTABLE_FILE=$3
APPIMAGE_FILE=$4

OFILE=$5

BOOTLOADER_SECTION_SIZE=$(($PTABLE_ADDR - $BOOTLOADER_ADDR))
PTABLE_SECTION_SIZE=$(($APPIMAGE_ADDR - $PTABLE_ADDR))

truncate -s $BOOTLOADER_SECTION_SIZE $BOOTLOADER_FILE
truncate -s $PTABLE_SECTION_SIZE     $PTABLE_FILE

cat ${PAD_FILE} $BOOTLOADER_FILE $PTABLE_FILE $APPIMAGE_FILE > $OFILE

#
#
