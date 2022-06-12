ESPLink v1.2
-----------
[English](./README.md)
* [ESPLink 介绍](#ESPLink-介绍) 
* [特性](#特性)
* [产品链接](#产品链接)
* [参考](#参考)


# ESPLink 介绍
ESPLink 是 Muselab推出的专用于乐鑫的ESP系列芯片的调试工具，支持所有乐鑫的ESP系列芯片(ESP8266/ESP32/ESP32-S2/ESP32-C3/ESP32-S3等)，支持串口烧录调试、拖拽烧录、JTAG调试，可极大提高开发测试的效率  
<div align=center>
<img src="https://github.com/wuxx/ESPLink/blob/master/doc/ESPLink-v1.2-1.jpg" width = "400" alt="" align=center />
<img src="https://github.com/wuxx/ESPLink/blob/master/doc/ESPLink-v1.2-2.jpg" width = "400" alt="" align=center />
<img src="https://github.com/wuxx/ESPLink/blob/master/doc/ESPLink-v1.2-4.jpg" width = "400" alt="" align=center />
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
注意：flash镜像文件为uf2格式，可使用乐鑫官方工具idf.py生成，举例如下
```
$idf.py uf2
$cp build/uf2.bin /media/pi/ESPLink/
```

## JTAG Debug
ESPLink 支持使用jtag调试ESP系列芯片, 可以方便的让您在系统崩溃时查找原因。具体使用说明如下：（以下以ESP32-S2为例）
### Openocd 安装
  
```
$git clone https://github.com/espressif/openocd-esp32.git
$cd openocd-esp32
$./bootstrap
$./configure --enable-cmsis-dap
$make -j
$sudo make install
```

### Attach to ESP32-S3
```
pi@raspberrypi:~/oss/openocd-esp32 $ sudo ./src/openocd -s /home/pi/oss/openocd-esp32/tcl -f tcl/interface/esp_usb_bridge.cfg -f tcl/target/esp32s2.cfg
Open On-Chip Debugger  v0.11.0-esp32-20220411-5-g03cd2031 (2022-04-25-09:45)
Licensed under GNU GPL v2
For bug reports, read
        http://openocd.org/doc/doxygen/bugs.html
Info : only one transport option; autoselect 'jtag'
Info : esp_usb_jtag: VID set to 0x303a and PID to 0x1002
Info : esp_usb_jtag: capabilities descriptor set to 0x30a
adapter speed: 4000 kHz
Warn : Transport "jtag" was already selected
Info : Listening on port 6666 for tcl connections
Info : Listening on port 4444 for telnet connections
Info : esp_usb_jtag: serial (84F703D20134)
Info : esp_usb_jtag: Device found. Base speed 750KHz, div range 1 to 1
Info : clock speed 750 kHz
Info : JTAG tap: esp32s2.cpu tap/device found: 0x120034e5 (mfg: 0x272 (Tensilica), part: 0x2003, ver: 0x1)
Info : esp32s2: Debug controller was reset.
Info : esp32s2: Core was reset.
Info : starting gdb server for esp32s2 on 3333
Info : Listening on port 3333 for gdb connections
```


### Debug
attach 成功之后，可以打开另一个终端窗口进行操作，可使用gdb或者telnet，分别举例说明如下
#### Debug with Gdb
```
$xtensa-esp32s2-elf-gdb -ex 'target remote 127.0.0.1:3333' ./build/blink.elf
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
## JTAG
`TCK`,`TMS`,`TDI`,`TDO`
## UART
`TX`,`RX`

## BOOT 
`BOOT`
用于令芯片进入Bootloader，拉低此引脚，然后复位芯片，即可进入Bootloader

## Reset
`RST`  
用于芯片的`热复位`

# 产品链接
[ESPLink](https://item.taobao.com/item.htm?spm=a1z10.5-c.w4002-21349689069.17.a93f128cnQKQTi&id=659813902021)

# 参考
### ESP USB Bridge
https://github.com/espressif/esp-usb-bridge
### esp-idf
https://github.com/espressif/esp-idf
### esp32-c3 get-started
https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/get-started/
### esp32-c3
https://www.espressif.com/zh-hans/products/socs/esp32-c3

