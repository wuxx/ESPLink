ESPLink v1.0 ([v1.2](./README_v1.2.md))
-----------
[English](./README.md)
* [ESPLink 介绍](#ESPLink-介绍) 
* [特性](#特性)
* [引脚说明](#引脚说明)
* [上位机软件](#esplink-tool)
* [产品链接](#产品链接)
* [参考](#参考)


# ESPLink 介绍
ESPLink 是 Muselab推出的专用于乐鑫的ESP系列芯片的调试工具，支持所有乐鑫的ESP系列芯片(ESP8266/ESP32/ESP32-S2/ESP32-C3/ESP32-S3等)，支持串口烧录调试、拖拽烧录、JTAG调试，可极大提高开发测试的效率  
<div align=center>
<img src="https://github.com/wuxx/ESPLink/blob/master/doc/esplink-1.png" width = "400" alt="" align=center />
<img src="https://github.com/wuxx/ESPLink/blob/master/doc/esplink-2.png" width = "400" alt="" align=center />
</div>

# 特性
## USB转串口
兼容传统的串口使用，可使用esptool.py进行下载调试，举例如下：
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
  
## 拖拽烧录
板载的下载器ESPLink支持拖拽烧录，将开发板上电之后，PC将会出现一个名为ESPLink的虚拟U盘，此时只需将flash镜像文件拖拽至ESPLink虚拟U盘中，稍等片刻，即可自动完成烧录。此功能使得烧录无需依赖任何外部工具以及操作系统， 典型的使用场景列举如下：快速的原型验证、在云端服务器进行编译、然后在任意PC上烧录、或者在商业的产品中实现固件的快速升级。
注意：flash镜像文件为三个文件(bootloader.bin/partition-table.bin/app.bin)拼接而成，tools目录下提供了一个脚本以供使用，举例如下：
```
$./tools/esppad.sh
usage: esppad.sh esp8266/esp32/esp32s2/esp32c3/esp32s3 bootloader.bin partition-table.bin app.bin flash_image.bin
$./tools/esppad.sh esp32c3 bootloader.bin partition-table.bin app.bin flash_image.bin
$cp flash_image.bin /media/pi/ESPLink/
```

## JTAG Debug
ESPLink 支持使用jtag调试ESP系列芯片, 可以方便的让您在系统崩溃时查找原因。具体使用说明如下：（以下以ESP32-C3为例）
### Openocd 安装
  
```
$git clone https://github.com/espressif/openocd-esp32.git
$cd openocd-esp32
$./bootstrap
$./configure --enable-cmsis-dap
$make -j
$sudo make install
```

### 烧录efuse
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

# 引脚说明
## Power
`GND`,`3V3`,`5V`  
注意：3V3电源可使用软件控制上下电，以实现`冷复位`，即将芯片完全下电再重新上电，命令如下
```
$./tools/esplink-tool -c power=1 #power-on
$./tools/esplink-tool -c power=0 #power-off
```
## JTAG
`TCK`,`TMS`,`TDI`,`TDO`
## UART
`TX`,`RX`

## BOOT 
`BOOT`
用于令芯片进入Bootloader，拉低此引脚，然后复位芯片(冷复位或者热复位)，即可进入Bootloader

## Reset
`RST`  
用于芯片的`热复位`
注意：ESPLink支持`冷复位`(通过控制3V3电源)和`热复位`(通过控制RST引脚)
```
$./tools/esplink-tool -c reset_mode=hot
$./tools/esplink-tool -c reset_mode=cold
```
# esplink-tool
esplink-tool 是为ESPLink设计的上位机程序，可实现目标芯片的探测、令目标芯片进入bootloader或者app，以及ESPLink的参数配置
```
$esplink-tool -h
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


# 产品链接
[ESPLink](https://item.taobao.com/item.htm?spm=a1z10.5-c.w4002-21349689069.17.a93f128cnQKQTi&id=659813902021)

# 参考
### esp-idf
https://github.com/espressif/esp-idf
