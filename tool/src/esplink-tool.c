/*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/* author: wuxx */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <hidapi.h>
#include <libusb.h>

#include <getopt.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

#define readb(addr)         (*( ( volatile uint8_t * )(addr)) )
#define writeb(addr, data)  (*( ( volatile uint8_t * )(addr)) = data)

#define reads(addr)         (*( ( volatile uint16_t * )(addr)) )
#define writes(addr, data)  (*( ( volatile uint16_t * )(addr)) = data)

#define readl(addr)         (*( ( volatile uint32_t * )(addr)) )
#define writel(addr, data)  (*( ( volatile uint32_t * )(addr)) = data)

#define DUMP_STR(s) printf(#s "\t[%s]\n", s)
#define DUMP_VAR4(var) printf(#var "\t0x%08x\n", var)

#define ESPLINK_VID (0x0d28)
#define ESPLINK_PID (0x0204)

#define PACKET_SIZE       (64 + 1)  /* 64 bytes plus report id */
#define USB_TIMEOUT_DEFAULT 1000

#define ID_DAP_Vendor0  0x80U
#define ID_DAP_Vendor13 0x8DU

#define SPI_FLASH_SIZE          flash_size
#define SPI_FLASH_SECTOR_SIZE   (4096)
#define SPI_FLASH_SECTOR_ALIGN_UP(x)     ((x + SPI_FLASH_SECTOR_SIZE - 1) & (~(SPI_FLASH_SECTOR_SIZE - 1)))
#define SPI_FLASH_SECTOR_ALIGN_DOWN(x)   ((x) & (~(SPI_FLASH_SECTOR_SIZE - 1)))

enum CHIP_TYPE_E {
    CT_ESP8266  = 0xfff0c101,
    CT_ESP32    = 0x00f01d83,
    CT_ESP32S2  = 0x000007c6,
    CT_ESP32C3  = 0x1b31506f, /* eco1+2: 0x6921506f, eco3: 0x1b31506f */
    CT_ESP32S3  = 0x9,
};

enum ESPLINK_CMD_E {
    CMD_FLASH_GET_INFO = 0,
    CMD_FLASH_TRANSACTION_START,
    CMD_FLASH_TRANSACTION_END,
    CMD_RAM_WRITE,
    CMD_RAM_READ,
    CMD_FLASH_WRITE_SECTOR,
    CMD_FLASH_READ_SECTOR,
    CMD_FLASH_ERASE_CHIP,

    CMD_SYS_GET_INFO = 0x80, /* chip-type; power-status; flash-offset; baudrate; */
    CMD_SYS_GPIO_MODE,
    CMD_SYS_GPIO_WRITE,
    CMD_SYS_GPIO_READ,
    CMD_SYS_SET_FLASH_OFFSET,
    CMD_SYS_SET_UART_BAUDRATE,
    CMD_SYS_SET_RESET_MODE,
    CMD_SYS_POWER_ON,
    CMD_SYS_POWER_OFF,
    CMD_SYS_ENTER_BOOTLOADER,
    CMD_SYS_ENTER_APP,
};

struct esplink {
    hid_device *dev_handle;
    uint8_t packet_buffer[PACKET_SIZE];
	uint32_t packet_size;
};

static struct esplink esplink_handle;

struct esplink_packet_head {
    uint8_t vendor_cmd;
    uint8_t esplink_cmd;
    uint8_t data[0];
};


struct esp_info_ram {
    uint32_t chip_type;
    uint32_t power_status;
};

struct esp_info_nvm {
    uint32_t flash_offset;
    uint32_t uart_baudrate; /* for drag-n-drop flash */
    uint32_t reset_mode;    /* 0: hot (power off-on cycle); 1: cold (reset with rst) */
};

struct sys_info {
    char buildtime[32];
    struct esp_info_ram ei_ram;
    struct esp_info_nvm ei_nvm;
};

struct sys_info si;

int32_t chip_type = CT_ESP32;

uint32_t flash_id = 0;
uint32_t flash_size = 8 * 1024 * 1024;

static uint8_t cook(uint8_t c)
{
    /* please check the ascii code table */
    if (c >= 0x20 && c <= 0x7E) {
        return c;
    } else {
        return '.';
    }
}

void dumpb(uint8_t *p, uint32_t byte_nr)
{
    uint32_t i = 0, x;
    uint8_t buf[16];
    uint32_t count, left;

    count = byte_nr / 16;
    left  = byte_nr % 16;

    fprintf(stdout,"[0x%08x]: ", i);
    for(i = 0; i < count; i++) {
        for(x = 0; x < 16; x++) {
            buf[x] = p[i * 16 + x];
            fprintf(stdout,"%02x ", buf[x]);
        }
        fprintf(stdout,"  ");
        for(x = 0; x < 16; x++) {
            fprintf(stdout,"%c", cook(buf[x]));
        }

        fprintf(stdout,"\n[0x%08x]: ", (i + 1) * 16);
    }

    if (left != 0) {
        for(x = 0; x < 16; x++) {
            if (x < left) {
                buf[x] = p[i * 16 + x];
                fprintf(stdout,"%02x ", buf[x]);
            } else {
                buf[x] = ' ';
                fprintf(stdout,"   ");
            }
        }
        fprintf(stdout,"  ");
        for(x = 0; x < 16; x++) {
            fprintf(stdout,"%c", cook(buf[x]));
        }

    }

    fprintf(stdout,"\n");

}

int esplink_usb_xfer(int txlen)
{
    int32_t retval;

    /* Pad the rest of the TX buffer with 0's */
    memset(esplink_handle.packet_buffer + txlen, 0, esplink_handle.packet_size - txlen);

    /* write data to device */
    retval = hid_write(esplink_handle.dev_handle,
            esplink_handle.packet_buffer, esplink_handle.packet_size);
    if (retval == -1) {
        fprintf(stderr, "error writing data: %ls", hid_error(esplink_handle.dev_handle));
        return -1;
    }

    /* get reply */
    retval = hid_read_timeout(esplink_handle.dev_handle, esplink_handle.packet_buffer,
            esplink_handle.packet_size, USB_TIMEOUT_DEFAULT);
    if (retval == -1 || retval == 0) {
        fprintf(stderr, "error reading data: %ls", hid_error(esplink_handle.dev_handle));
        return -1;
    }

    return 0;
}

int esplink_usb_xfer_wait(int txlen)
{
    int32_t retval;

    /* Pad the rest of the TX buffer with 0's */
    memset(esplink_handle.packet_buffer + txlen, 0, esplink_handle.packet_size - txlen);

    /* write data to device */
    retval = hid_write(esplink_handle.dev_handle,
            esplink_handle.packet_buffer, esplink_handle.packet_size);
    if (retval == -1) {
        fprintf(stderr, "error writing data: %ls", hid_error(esplink_handle.dev_handle));
        return -1;
    }

    /* get reply */
    retval = hid_read_timeout(esplink_handle.dev_handle, esplink_handle.packet_buffer,
            esplink_handle.packet_size, -1);
    if (retval == -1 || retval == 0) {
        fprintf(stderr, "error reading data: %ls", hid_error(esplink_handle.dev_handle));
        return -1;
    }

    return 0;
}

int esplink_flash_transaction_start()
{
    struct esplink_packet_head *ph;
    ph = (struct esplink_packet_head *)&esplink_handle.packet_buffer[1];

    ph->vendor_cmd  = ID_DAP_Vendor13;
    ph->esplink_cmd = CMD_FLASH_TRANSACTION_START;

    if (esplink_usb_xfer(3) != 0) {
        fprintf(stderr, "ESPLink CMD_FLASH_TRANSACTION_START failed.");
        exit(-1);
    }

    return 0;
}

int esplink_flash_transaction_end()
{
    struct esplink_packet_head *ph;
    ph = (struct esplink_packet_head *)&esplink_handle.packet_buffer[1];

    ph->vendor_cmd  = ID_DAP_Vendor13;
    ph->esplink_cmd = CMD_FLASH_TRANSACTION_END;

    if (esplink_usb_xfer(3) != 0) {
        fprintf(stderr, "ESPLink CMD_FLASH_TRANSACTION_END failed.\n");
        exit(-1);
    }

    return 0;
}

/* max read count: 65 - 7 = 58 */
int esplink_ram_read(uint16_t ram_addr, uint8_t *buf, uint16_t count)
{
    struct esplink_packet_head *ph;
    ph = (struct esplink_packet_head *)&esplink_handle.packet_buffer[1];

    ph->vendor_cmd  = ID_DAP_Vendor13;
    ph->esplink_cmd = CMD_RAM_READ;
    writes(&ph->data[0], ram_addr);
    writes(&ph->data[2], count);

    //dumpb(esplink_handle.packet_buffer, 16);

    if (esplink_usb_xfer(7) != 0) {
        fprintf(stderr, "ESPLink CMD_RAM_READ failed.\n");
        return -1;
    }

    memcpy(buf, &(esplink_handle.packet_buffer[2]), count);

    return 0;
}

#define RAM_RW_SIZE   (58)

int esplink_ram_read_sector(uint8_t *buf)
{
    uint32_t i, offset = 0;
    uint32_t count, left_size;
    count     = SPI_FLASH_SECTOR_SIZE / RAM_RW_SIZE;
    left_size = SPI_FLASH_SECTOR_SIZE % RAM_RW_SIZE;

    for(i = 0; i < count; i++) {
	    if (esplink_ram_read(offset, &buf[offset], RAM_RW_SIZE) != 0) {
            fprintf(stderr, " esplink_ram_read 0x%x failed.\n", offset);
        }

        offset += RAM_RW_SIZE;
    }

    /* left */
    if (esplink_ram_read(offset, &buf[offset], left_size) != 0) {
        fprintf(stderr, " esplink_ram_read 0x%x failed.\n", left_size);
    }

    return 0;
}

int esplink_ram_dump()
{
	uint8_t flash_sector_ram[SPI_FLASH_SECTOR_SIZE] = {0};
    uint32_t roffset;

    for(roffset = 0; roffset < SPI_FLASH_SECTOR_SIZE; roffset += 32) {
	    if (esplink_ram_read(roffset, &flash_sector_ram[roffset], 32) != 0) {
            fprintf(stderr, " esplink_ram_read 0x%x failed.\n", roffset);
        }

    }

    dumpb(flash_sector_ram, SPI_FLASH_SECTOR_SIZE);

    return 0;
}

/* max write count: 65 - 7 = 58 */
int esplink_ram_write(uint16_t ram_addr, uint8_t *buf, uint16_t count)
{
    struct esplink_packet_head *ph;
    ph = (struct esplink_packet_head *)&esplink_handle.packet_buffer[1];

    ph->vendor_cmd  = ID_DAP_Vendor13;
    ph->esplink_cmd = CMD_RAM_WRITE;
    writes(&ph->data[0], ram_addr);
    writes(&ph->data[2], count);

    memcpy(&ph->data[4], buf, count);

    if (esplink_usb_xfer(7 + count) != 0) {
        fprintf(stderr, "ESPLink CMD_FLASH_TRANSACTION_END failed.\n");
        return -1;
    }

    return 0;
}

int esplink_ram_write_sector(uint8_t *buf)
{
    uint32_t raddr;

    uint32_t i, count, left_size;

    count     = SPI_FLASH_SECTOR_SIZE / RAM_RW_SIZE;
    left_size = SPI_FLASH_SECTOR_SIZE % RAM_RW_SIZE;
    raddr     = 0;

    for(i = 0; i < count; i++) {
	    if (esplink_ram_write(raddr, &buf[raddr], RAM_RW_SIZE) != 0) {
            fprintf(stderr, " esplink_ram_write 0x%x failed.\n", raddr);
        }
        raddr += RAM_RW_SIZE;
    }

    /* left */
    if (esplink_ram_write(raddr, &buf[raddr], left_size) != 0) {
        fprintf(stderr, " esplink_ram_write 0x%x failed.\n", raddr);
    }

    return 0;
}

/* read flash to ram */
int esplink_flash_read_sector(uint32_t flash_addr, uint8_t *buf)
{
    struct esplink_packet_head *ph;
    ph = (struct esplink_packet_head *)&esplink_handle.packet_buffer[1];

    ph->vendor_cmd  = ID_DAP_Vendor13;
    ph->esplink_cmd = CMD_FLASH_READ_SECTOR;
    writel(&ph->data[0], flash_addr);

    if (esplink_usb_xfer(7) != 0) {
        fprintf(stderr, "ESPLink CMD_FLASH_READ_SECTOR failed.\n");
        return -1;
    }

    esplink_ram_read_sector(buf);

    return 0;
}

int esplink_flash_read_sectors(uint32_t flash_addr, uint32_t sector_num, uint8_t *buf)
{
    uint32_t i, faddr;

    esplink_flash_transaction_start();

    for(i = 0; i < sector_num; i++) {
        faddr = flash_addr + i * SPI_FLASH_SECTOR_SIZE;
        if ((faddr & 0xffff) == 0) { fprintf(stdout, "read 0x%08x\r\n", faddr); }
        esplink_flash_read_sector(faddr, &buf[i * SPI_FLASH_SECTOR_SIZE]);
    }

    esplink_flash_transaction_end();

    return 0;
}

int esplink_flash_dump_sector(uint32_t flash_addr)
{
    uint8_t flash_sector[SPI_FLASH_SECTOR_SIZE];

    esplink_flash_transaction_start();

    esplink_flash_read_sector(flash_addr, flash_sector);

    esplink_flash_transaction_end();

    dumpb(flash_sector, SPI_FLASH_SECTOR_SIZE);

    return 0;
}

int esplink_flash_write_sector(uint32_t flash_addr, uint8_t *buf)
{
    struct esplink_packet_head *ph;

    esplink_ram_write_sector(buf);

    ph = (struct esplink_packet_head *)&esplink_handle.packet_buffer[1];

    ph->vendor_cmd  = ID_DAP_Vendor13;
    ph->esplink_cmd = CMD_FLASH_WRITE_SECTOR;
    writel(&ph->data[0], flash_addr);

    if (esplink_usb_xfer(7) != 0) {
        fprintf(stderr, "ESPLink CMD_FLASH_READ_SECTOR failed.\n");
        return -1;
    }
    return 0;
}

int esplink_flash_write_sectors(uint32_t flash_addr, uint32_t sector_num, uint8_t *buf)
{
    uint32_t i, faddr;

    esplink_flash_transaction_start();

    for(i = 0; i < sector_num; i++) {
        faddr = flash_addr + i * SPI_FLASH_SECTOR_SIZE;
        if ((faddr & 0xffff) == 0) { fprintf(stdout, "write 0x%08x\r\n", faddr); }
        esplink_flash_write_sector(faddr, &buf[i * SPI_FLASH_SECTOR_SIZE]);
    }

    esplink_flash_transaction_end();

    return 0;
}

int esplink_flash_erase_chip()
{
    struct esplink_packet_head *ph;

    esplink_flash_transaction_start();

    ph = (struct esplink_packet_head *)&esplink_handle.packet_buffer[1];

    ph->vendor_cmd  = ID_DAP_Vendor13;
    ph->esplink_cmd = CMD_FLASH_ERASE_CHIP;

    if (esplink_usb_xfer_wait(3) != 0) {
        fprintf(stderr, "ESPLink CMD_FLASH_ERASE_CHIP failed.\n");
        return -1;
    }

    esplink_flash_transaction_end();

    return 0;
}

int32_t esplink_sys_dump_info(struct sys_info *psi)
{


    switch (psi->ei_ram.chip_type) {
        case (CT_ESP8266):
            fprintf(stdout, "chip:\t\t[ESP8266]\n");
            break;
        case (CT_ESP32):
            fprintf(stdout, "chip:\t\t[ESP32]\n");
            break;
        case (CT_ESP32S2):
            fprintf(stdout, "chip:\t\t[ESP32-S2]\n");
            break;
        case (CT_ESP32C3):
            fprintf(stdout, "chip:\t\t[ESP32-C3]\n");
            break;
        case (CT_ESP32S3):
            fprintf(stdout, "chip:\t\t[ESP32-S3]\n");
            break;
        default:
            fprintf(stdout, "chip:\t\t[Unknown](0x%x)\n", psi->ei_ram.chip_type);
            break;
    }

    fprintf(stdout, "power:\t\t[%s]\n", psi->ei_ram.power_status == 0 ? "off" : "on");
    fprintf(stdout, "reset_mode:     [%s]\n", si.ei_nvm.reset_mode == 0 ? "hot" : "cold");
    fprintf(stdout, "flash_offset:   [0x%x]\n", si.ei_nvm.flash_offset);
    fprintf(stdout, "uart_baudrate:  [%d]\n", si.ei_nvm.uart_baudrate);

    /* buildtime info already in ESPLink/DETAIL.TXT */
    //fprintf(stdout, "ESPLink buildtime: [%s]\n", psi->buildtime);
    return 0;
}

int32_t esplink_sys_get_info()
{
    struct esplink_packet_head *ph;

    ph = (struct esplink_packet_head *)&esplink_handle.packet_buffer[1];

    ph->vendor_cmd  = ID_DAP_Vendor13;
    ph->esplink_cmd = CMD_SYS_GET_INFO;

    if (esplink_usb_xfer_wait(3) != 0) {
        fprintf(stderr, "ESPLink CMD_SYS_GET_INFO failed.\n");
        return -1;
    }

    memcpy(&si, &(esplink_handle.packet_buffer[2]), sizeof(si));

    esplink_sys_dump_info(&si);

#if 0
    DUMP_STR(si.buildinfo);
    DUMP_VAR4(si.ei_ram.chip_type);
    DUMP_VAR4(si.ei_ram.power_status);
    DUMP_VAR4(si.ei_nvm.flash_offset);
    DUMP_VAR4(si.ei_nvm.uart_baudrate);
#endif

    return 0;

}

int32_t esplink_sys_set_flash_offset(uint32_t offset)
{

    struct esplink_packet_head *ph;

    ph = (struct esplink_packet_head *)&esplink_handle.packet_buffer[1];

    ph->vendor_cmd  = ID_DAP_Vendor13;
    ph->esplink_cmd = CMD_SYS_SET_FLASH_OFFSET;

    writel(&(ph->data[0]), offset);

    if (esplink_usb_xfer_wait(3 + 4) != 0) {
        fprintf(stderr, "ESPLink CMD_SYS_SET_FLASH_OFFSET failed.\n");
        return -1;
    }

    return 0;

}

int32_t esplink_sys_set_uart_baudrate(uint32_t baudrate)
{

    struct esplink_packet_head *ph;

    ph = (struct esplink_packet_head *)&esplink_handle.packet_buffer[1];

    ph->vendor_cmd  = ID_DAP_Vendor13;
    ph->esplink_cmd = CMD_SYS_SET_UART_BAUDRATE;

    writel(&(ph->data[0]), baudrate);

    if (esplink_usb_xfer_wait(3 + 4) != 0) {
        fprintf(stderr, "ESPLink CMD_SYS_SET_UART_BAUDRATE failed.\n");
        return -1;
    }

    return 0;

}

int32_t esplink_sys_set_reset_mode(uint32_t mode)
{

    struct esplink_packet_head *ph;

    ph = (struct esplink_packet_head *)&esplink_handle.packet_buffer[1];

    ph->vendor_cmd  = ID_DAP_Vendor13;
    ph->esplink_cmd = CMD_SYS_SET_RESET_MODE;

    writel(&(ph->data[0]), mode);

    if (esplink_usb_xfer_wait(3 + 4) != 0) {
        fprintf(stderr, "ESPLink CMD_SYS_SET_RESET_MODE failed.\n");
        return -1;
    }

    return 0;

}

int32_t esplink_sys_set_power(uint32_t p)
{

    struct esplink_packet_head *ph;

    ph = (struct esplink_packet_head *)&esplink_handle.packet_buffer[1];

    ph->vendor_cmd  = ID_DAP_Vendor13;
    if (p) {
        ph->esplink_cmd = CMD_SYS_POWER_ON;
    } else {
        ph->esplink_cmd = CMD_SYS_POWER_OFF;
    }

    if (esplink_usb_xfer_wait(3 + 0) != 0) {
        fprintf(stderr, "ESPLink CMD_SYS_POWER ON/OFF failed.\n");
        return -1;
    }

    return 0;

}

int32_t esplink_sys_enter_bootloader()
{

    struct esplink_packet_head *ph;

    ph = (struct esplink_packet_head *)&esplink_handle.packet_buffer[1];

    ph->vendor_cmd  = ID_DAP_Vendor13;
    ph->esplink_cmd = CMD_SYS_ENTER_BOOTLOADER;

    if (esplink_usb_xfer_wait(3 + 0) != 0) {
        fprintf(stderr, "ESPLink CMD_SYS_ENTER_BOOTLOADER failed.\n");
        return -1;
    }

    return 0;

}

int32_t esplink_sys_enter_app()
{

    struct esplink_packet_head *ph;

    ph = (struct esplink_packet_head *)&esplink_handle.packet_buffer[1];

    ph->vendor_cmd  = ID_DAP_Vendor13;
    ph->esplink_cmd = CMD_SYS_ENTER_APP;

    if (esplink_usb_xfer_wait(3 + 0) != 0) {
        fprintf(stderr, "ESPLink CMD_SYS_ENTER_APP failed.\n");
        return -1;
    }

    return 0;

}

int32_t esplink_sys_get_id(char *id)
{
    int i;
    struct esplink_packet_head *ph;
    uint8_t *pb;
    uint32_t size;

    pb = (uint8_t *)&esplink_handle.packet_buffer[1];

    //pb[0] = ID_DAP_Vendor0;

    pb[0] = 0x00; /* ID_DAP_Info */
    pb[1] = 0x03; /* DAP_ID_SER_NUM */

    if (esplink_usb_xfer(3) != 0) {
        fprintf(stderr, "ESPLink ID_DAP_Vendor0 failed.\n");
        return -1;
    }

    pb = &(esplink_handle.packet_buffer[0]);
    size = pb[1];
#if 0
    for(i = 0; i < 8; i++) {
        fprintf(stdout, "pb[%d]: 0x%x\n", i, pb[i]);
    }
#endif

    memcpy(id, &pb[2], size);

    return 0;

}

int esplink_gpio_mode(uint32_t gpio_port, uint32_t gpio_index, uint32_t gpio_mode)
{
    struct esplink_packet_head *ph;

    ph = (struct esplink_packet_head *)&esplink_handle.packet_buffer[1];

    ph->vendor_cmd  = ID_DAP_Vendor13;
    ph->esplink_cmd = CMD_SYS_GPIO_MODE;

    writel(&(ph->data[0]), gpio_port);
    writel(&(ph->data[4]), gpio_index);
    writel(&(ph->data[8]), gpio_mode);

    if (esplink_usb_xfer_wait(15) != 0) {
        fprintf(stderr, "ESPLink CMD_SYS_GPIO_MODE failed.\n");
        return -1;
    }

    return 0;
}


int32_t esplink_gpio_read(uint32_t gpio_port, uint32_t gpio_index)
{
    struct esplink_packet_head *ph;

    ph = (struct esplink_packet_head *)&esplink_handle.packet_buffer[1];

    ph->vendor_cmd  = ID_DAP_Vendor13;
    ph->esplink_cmd = CMD_SYS_GPIO_READ;

    writel(&(ph->data[0]), gpio_port);
    writel(&(ph->data[4]), gpio_index);

    if (esplink_usb_xfer_wait(11) != 0) {
        fprintf(stderr, "ESPLink CMD_SYS_GPIO_MODE failed.\n");
        return -1;
    }

    return readl(&(esplink_handle.packet_buffer[2]));

}

int32_t esplink_gpio_write(uint32_t gpio_port, uint32_t gpio_index, uint32_t gpio_value)
{
    struct esplink_packet_head *ph;

    ph = (struct esplink_packet_head *)&esplink_handle.packet_buffer[1];

    ph->vendor_cmd  = ID_DAP_Vendor13;
    ph->esplink_cmd = CMD_SYS_GPIO_WRITE;

    writel(&(ph->data[0]), gpio_port);
    writel(&(ph->data[4]), gpio_index);
    writel(&(ph->data[8]), gpio_value);

    if (esplink_usb_xfer_wait(15) != 0) {
        fprintf(stderr, "ESPLink CMD_SYS_GPIO_WRITE failed.\n");
        return -1;
    }

    return 0;

}


void esplink_close()
{
    hid_close(esplink_handle.dev_handle);
    hid_exit();
}

int esplink_open()
{
    hid_device *dev = NULL;
    int i;
    struct hid_device_info *devs, *cur_dev;
    unsigned short target_vid, target_pid;
    wchar_t *target_serial = NULL;

    if (hid_init() != 0) {
        fprintf(stderr, "hid_init fail!\n");
        exit(-1);
    }

    if ((dev = hid_open(ESPLINK_VID, ESPLINK_PID, NULL)) == NULL) {
        fprintf(stderr, "ESPLink open fail!\n");
        exit(-1);
    }

    esplink_handle.dev_handle  = dev;
	esplink_handle.packet_size = PACKET_SIZE;
    esplink_handle.packet_buffer[0] = 0; /* report number */

    return 0;
}

void usage(char *program_name)
{
    printf("usage: %s [CONFIG] \n", program_name);
    fprintf (stdout, ("\
             -p | --probe                   probe chip                       \n\
             -e | --enter bootloader/app    enter bootloader or app          \n\
             -c | --config                  esplink config                   \n\
             -g | --gpio                    esplink gpio write/read          \n\
             -w | --write                   write gpio                       \n\
             -r | --read                    read  gpio                       \n\
             -m | --mode                    set   gpio mode                  \n\
             -h | --help                    display help info                \n\n\
             -- version 1.0 --\n\
"));
    exit(0);
}

static struct option const long_options[] =
{
  {"write",    no_argument,        NULL, 'w'},
  {"read",     no_argument,        NULL, 'r'},
  {"probe",    no_argument,        NULL, 'p'},
  {"enter",    required_argument,  NULL, 'e'},
  {"config",   required_argument,  NULL, 'c'},

  /* gpio */
  {"gpio",     required_argument,  NULL, 'g'},
  {"mode",     required_argument,  NULL, 'm'},

  {"help",     no_argument,        NULL, 'h'},
  {NULL,       0,                  NULL,  0 },
};

/* stm32f1xx_hal_gpio.h */
uint32_t gpio_mode_map(char *s)
{
    if (strcmp(s, "in") == 0) {
        return 0; /* GPIO_MODE_INPUT */
    } else if (strcmp(s, "out") == 0) {
        return 1; /* GPIO_MODE_OUTPUT_PP */
    } else {
        fprintf(stderr, "invalid gpio mode %s\n", s);
        exit(-1);
    }
}

int main(int argc, char **argv)
{
    char *ifile = NULL;

    int32_t c;
    int32_t option_index;
    int32_t fd;

    struct stat st;

    uint32_t gpio_port = 0, gpio_index, gpio_value; 
    int32_t  gpio_mode = -1;

    uint32_t esp_flash_offset, esp_uart_baudrate, esp_power, esp_reset_mode;

    char config[128] = {0};

    int op_mode = 1;
    int rw = 1; /* read: 0; write: 1; */

    if (argc == 1) {
        usage(argv[0]);
        exit(-1);
    }

    while ((c = getopt_long (argc, argv, "wrpe:c:g:m:h",
                long_options, &option_index)) != -1) {
        switch (c) {
            case ('c'):
                op_mode = 0;    /* config esplink */
                memcpy(config, optarg, strlen(optarg));
                break;
            case ('r'):
                rw = 0;
                break;
            case ('w'):
                rw = 1;
                break;
            case ('e'):
                op_mode = 2;
                memcpy(config, optarg, strlen(optarg));
                break;
            case ('p'):
                op_mode = 3;    /* chip probe */
                break;
            case ('g'):
                //gpio_num = strtoul(&optarg[1], NULL, 16);
                op_mode = 4;    /* esplink gpio access */
                gpio_port  = 0xA + (optarg[1] - 'A');
                gpio_index = strtoul(&optarg[2], NULL, 0);
                break;
            case ('m'):
                gpio_mode = gpio_mode_map(optarg);
                break;
            case ('h'):
                usage(argv[0]);
                exit(0);
            default:
                usage(argv[0]);
                exit(0);
        }
    }

    ifile = argv[argc - 1];

    //printf("ifile: %s\r\n", ifile);
    esplink_open();

    if (op_mode == 0) { /* esplink config */
        if (memcmp(config, "flash_offset=", 13) == 0) { /* FIXME: hardcode */

            esp_flash_offset = strtoul(&config[13], NULL, 0);
            DUMP_VAR4(esp_flash_offset);

            esplink_sys_set_flash_offset(esp_flash_offset);

        } else if (memcmp(config, "uart_baudrate=", 14) == 0) {
            esp_uart_baudrate = strtoul(&config[14], NULL, 0);
            DUMP_VAR4(esp_uart_baudrate);

            esplink_sys_set_uart_baudrate(esp_uart_baudrate);

        } else if (memcmp(config, "power=", 6) == 0) {
            esp_power = strtoul(&config[6], NULL, 0);
            DUMP_VAR4(esp_power);

            esplink_sys_set_power(esp_power);
        } else if (memcmp(config, "reset_mode=", 11) == 0) {
            if (strcmp(&config[11], "hot") == 0) {
                esp_reset_mode = 0;
            } else if(strcmp(&config[11], "cold") == 0) {
                esp_reset_mode = 1;
            } else {
                fprintf(stderr, "invalid reset_mode %s\r\n", &config[11]);
                exit(-1);
            }

            DUMP_VAR4(esp_reset_mode);

            esplink_sys_set_reset_mode(esp_reset_mode);
        }

    } else if (op_mode == 2) { /* enter bootloader or app */
        if (strcmp(config, "bootloader") == 0) {

            fprintf(stdout, "enter bootloader\n");

            esplink_sys_enter_bootloader();
        } else if (strcmp(config, "app") == 0) {

            fprintf(stdout, "enter app\n");

            esplink_sys_enter_app();
        }

    } else if (op_mode == 3) { /* probe chip */
        fprintf(stdout, "probe chip\n");

        esplink_sys_get_info();
    } else if (op_mode == 4) {
        /* gpio control */
        //fprintf(stdout, "gpio_port  P%X\r\n", gpio_port);
        //fprintf(stdout, "gpio_index %d\r\n", gpio_index);
        if ((gpio_port >= 0xA && gpio_port <=0xF) && (gpio_index >= 0 && gpio_index <= 15)) {
            if (gpio_mode != -1) { /* gpio mode */
                fprintf(stdout, "gpio mode P%X%d 0x%x\r\n", gpio_port, gpio_index, gpio_mode);
                esplink_gpio_mode(gpio_port, gpio_index, gpio_mode);
            } else if (rw == 0) { /* gpio read */
                gpio_value = esplink_gpio_read(gpio_port, gpio_index);
                fprintf(stdout, "gpio read P%X%d return %d\r\n", gpio_port, gpio_index, gpio_value);
            } else if (rw == 1) { /* gpio write */
                gpio_value = strtoul(argv[argc - 1], NULL, 0);
                fprintf(stdout, "gpio write P%X%d %d\r\n", gpio_port, gpio_index, gpio_value);
                esplink_gpio_write(gpio_port, gpio_index, gpio_value);
            }

        } else {
            fprintf(stderr, "invalid gpio_num %X%d\r\n", gpio_port, gpio_index);
            exit(-1);
        }

    } else {
            fprintf(stderr, "invalid op_mode %d\r\n", op_mode);
            exit(-1);

    }

    fprintf(stdout, "done\n");

    //esplink_ram_dump();
    esplink_close();

    return 0;
}
