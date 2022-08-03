ESPLink v1.0 ([v1.2](./README_v1.2.md))
-----------
[中文](./README_cn.md)
* [ESPLink Introduce](#ESPLink-Introduce) 
* [Features](#Features)
* [Pin Description](#Pin-Description)
* [esplink-tool](#esplink-tool)
* [Product Link](#Product-Link)
* [Reference](#Reference)


# ESPLink Introduce
ESPLink is a debug tool build for Expressif's ESP chips made by MuseLab, all Espressif's ESP chips are supported, currently supports ESP8266/ESP32/ESP32-S2/ESP32-C3/ESP32-S3 (also can update firmware to support Espressif's latest chips), with usb-to-serial (compatible with traditional use), drag-n-drop program, and jtag debug, made it very convenient for development and test.  

<div align=center>
<img src="https://github.com/wuxx/ESPLink/blob/master/doc/esplink-1.png" width = "400" alt="" align=center />
<img src="https://github.com/wuxx/ESPLink/blob/master/doc/esplink-2.png" width = "400" alt="" align=center />
</div>

# Features
## USB-to-Serial
compatible with the traditional use, can used to program with esptool.py or monitor the serial output message. examples for reference:  
```
$idf.py -p /dev/ttyACM0 flash monitor
$esptool.py --chip esp32c3 \
           -p /dev/ttyACM0 \
           -b 115200 \
           --before=default_reset \
           --after=hard_reset \
           --no-stub \
           write_flash \
           --flash_mode dio \
           --flash_freq 80m \
           --flash_size 2MB \
           0x0     esp32c3/bootloader.bin \
           0x8000  esp32c3/partition-table.bin \
           0x10000 esp32c3/blink_100.bin
```
  
## Drag-and-Drop Program
the ESPLink support drag-n-drop program, after power on the board, a virtual USB Disk named `ESPLink` will appear, just drag the flash image into the ESPLink, wait for some seconds, then the ESPLink will automatically complete the program work. it's very convenient since it's no need to rely on external tools and work well on any platform (Win/Linux/Mac .etc), some typical usage scenarios: quickly verification, compile on cloud server and program on any PC, firmware upgrade when used on commercial products .etc  
note: the flash image is a splicing of three files (bootloader.bin/partition-table.bin/app.bin), just expand the `bootloader.bin`, expand the `partition-table.bin`, and concatenate these three files, use the script `esppad.sh` or `esppad` under tools directory to make it, example for reference:    
```
$./tools/esppad.sh
usage: esppad.sh esp8266/esp32/esp32s2/esp32c3/esp32s3 bootloader.bin partition-table.bin app.bin flash_image.bin
$./tools/esppad.sh esp32c3 bootloader.bin partition-table.bin app.bin flash_image.bin
$cp flash_image.bin /media/pi/ESPLink/
```

## JTAG Debug
ESPLink support jtag interface to debug the ESP32-C3, it's useful for product developer to fix the bug when system crash, here are the instructions for reference (target chip is ESP32-C3 here as an example)
### Openocd Install  
```
$git clone https://github.com/espressif/openocd-esp32.git
$cd openocd-esp32
$./bootstrap
$./configure --enable-cmsis-dap
$make -j
$sudo make install
```

### Burn the efuse
the efuse JTAG_SEL_ENABLE should be burned to enable the jtag function.
```
$espefuse.py -p /dev/ttyACM0 burn_efuse JTAG_SEL_ENABLE
```

### Attach to ESP32-C3
set GPIO10 to 0 for choose the GPIO function to JTAG, then power on the board and execute the attach script  
```
$sudo openocd -f tcl/interface/cmsis-dap.cfg -f tcl/target/esp32c3.cfg
Open On-Chip Debugger  v0.10.0-esp32-20201202-30-gddf07692 (2021-03-22-16:48)
Licensed under GNU GPL v2
For bug reports, read
        http://openocd.org/doc/doxygen/bugs.html
adapter speed: 10000 kHz

force hard breakpoints
Info : Listening on port 6666 for tcl connections
Info : Listening on port 4444 for telnet connections
Info : CMSIS-DAP: SWD  Supported
Info : CMSIS-DAP: JTAG Supported
Info : CMSIS-DAP: FW Version = 0255
Info : CMSIS-DAP: Serial# = 0800000100430028430000014e504332a5a5a5a597969908
Info : CMSIS-DAP: Interface Initialised (JTAG)
Info : SWCLK/TCK = 1 SWDIO/TMS = 1 TDI = 1 TDO = 1 nTRST = 0 nRESET = 1
Info : CMSIS-DAP: Interface ready
Info : High speed (adapter_khz 10000) may be limited by adapter firmware.
Info : clock speed 10000 kHz
Info : cmsis-dap JTAG TLR_RESET
Info : cmsis-dap JTAG TLR_RESET
Info : JTAG tap: esp32c3.cpu tap/device found: 0x00005c25 (mfg: 0x612 (Espressif Systems), part: 0x0005, ver: 0x0)
Info : datacount=2 progbufsize=16
Info : Examined RISC-V core; found 1 harts
Info :  hart 0: XLEN=32, misa=0x40101104
Info : Listening on port 3333 for gdb connections
```

### Debug
when attach success, open another ternimal to debug, you can use gdb or telnet, explained separately as follows
#### Debug with Gdb
```
$riscv32-esp-elf-gdb -ex 'target remote 127.0.0.1:3333' ./build/blink.elf
(gdb) info reg
(gdb) s
(gdb) continue
```

#### Debug with telnet
```
$telnet localhost 4444
>reset
>halt
>reg
>step
>reg pc
>resume
```

# Pin Description
## Power
`GND`,`3V3`,`5V`  
note: the 3V3 power can controled by software for apply a `cold reset`, i.e. power-off-on cycle  
```
$./tools/esplink-tool -c power=1 #power-on
$./tools/esplink-tool -c power=0 #power-off
```
## JTAG
`TCK`,`TMS`,`TDI`,`TDO`  
## UART
`TX`,`RX`  
USB-Serial TX and RX
## BOOT 
`BOOT`  
set low, and reset the board, then the board will enter to bootloader for flash program, note that `BOOT` and `RST` is controled by ESPLink USB-Serial `DTR` and `RTS`, compatible with other USB-Serial chip like CP2102 and CH340.
## Reset
`RST`  
for `hot reset` the chip
note: ESPLink support two reset type: `cold reset` (with power-off-on on 3V3 pin) and `hot reset` (with RST pin low-to-high), if you use 3V3 pin to power your board, then you may like the `cold reset`, which no need to connect the RST pin, it may be more more convenient. a command can switch between `cold reset` and `hot reset`. note that the config is non-volatile, it's stored in ESPLink internal flash memory.
```
$./tools/esplink-tool -c reset_mode=hot
$./tools/esplink-tool -c reset_mode=cold
```
# esplink-tool
esplink-tool is a program to do some control, like probe the chip, enter bootloader/app, config or just control ESPLink GPIO.
```
usage: /home/pi/oss/ESPLink/tool/esplink-tool.arm [CONFIG]
             -p | --probe                   probe chip
             -e | --enter bootloader/app    enter bootloader or app
             -c | --config                  esplink config
             -g | --gpio                    esplink gpio write/read
             -w | --write                   write gpio
             -r | --read                    read  gpio
             -m | --mode                    set   gpio mode
             -h | --help                    display help info

             -- version 1.0 --
```


# Product Link
[ESPLink Board](https://www.aliexpress.com/item/1005003618669159.html?spm=5261.ProductManageOnline.0.0.7b284edfMi8J4H)

# Reference
### esp-idf
https://github.com/espressif/esp-idf
### esp32-c3 get-started
https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/get-started/
### esp32-c3
https://www.espressif.com/zh-hans/products/socs/esp32-c3
