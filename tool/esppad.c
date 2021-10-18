#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>



struct esp_image_info
{
    char *name;
    uint32_t bootloader_addr;
    uint32_t ptable_addr;
    uint32_t app_addr;
};


struct esp_image_info ei_map[] = {
    {
        .name = "esp8266",
        .bootloader_addr = 0x0,
        .ptable_addr     = 0x8000,
        .app_addr        = 0x10000,
    },
    {
        .name = "esp32",
        .bootloader_addr = 0x1000,
        .ptable_addr     = 0x8000,
        .app_addr        = 0x10000,
    },
    {
        .name = "esp32s2",
        .bootloader_addr = 0x1000,
        .ptable_addr     = 0x8000,
        .app_addr        = 0x10000,
    },
    {
        .name = "esp32c3",
        .bootloader_addr = 0x0,
        .ptable_addr     = 0x8000,
        .app_addr        = 0x10000,
    },
    {
        .name = "esp32s3",
        .bootloader_addr = 0x0,
        .ptable_addr     = 0x8000,
        .app_addr        = 0x10000,
    },
};


int main(int argc, char **argv)
{
    int i;
    int fd_bl, fd_pt, fd_app, fd_out;
    struct stat st_bl, st_pt, st_app;
    int size_max_bl, size_max_pt;

    uint8_t *out;
    uint32_t outsize;

    if (argc != 5) {
        fprintf(stderr, 
                "usage: %s esp8266/esp32/esp32s2/esp32c3/esp32s3 bootloader.bin partition-table.bin app.bin flash_image.bin\n", 
                argv[0]);
        return (-1);

    }

    for(i = 0; i < (sizeof(ei_map) / sizeof(ei_map[0])); i++) {
        if (strcmp(argv[1], ei_map[i].name) == 0) {
            break;
        }
    }

    if (i == (sizeof(ei_map) / sizeof(ei_map[0]))) {
        fprintf(stderr, "unknown chip name [%s]\n", argv[1]);
        return (-1);
    }

    if ((fd_bl = open(argv[2], O_RDWR)) == -1) {
        perror("open");
        return (-1);
    }

    if ((fstat(fd_bl, &st_bl)) == -1) {
        perror("fstat");
        return (-1);
    }

    if ((fd_pt = open(argv[3], O_RDWR)) == -1) {
        perror("open");
        return (-1);
    }

    if ((fstat(fd_pt, &st_pt)) == -1) {
        perror("fstat");
        return (-1);
    }

    if ((fd_app = open(argv[4], O_RDWR)) == -1) {
        perror("open");
        return (-1);
    }

    if ((fstat(fd_app, &st_app)) == -1) {
        perror("fstat");
        return (-1);
    }

    if((fd_out = open(argv[5], O_CREAT | O_RDWR | O_TRUNC)) == -1) {
        perror("fstat");
        return (-1);
    }


    size_max_bl = ei_map[i].ptable_addr - ei_map[i].bootloader_addr;
    size_max_pt = ei_map[i].app_addr    - ei_map[i].ptable_addr;

    if (st_bl.st_size > size_max_bl) {
        fprintf(stderr, "bootloader size invalid (%d > %d)\n", st_bl.st_size, size_max_bl);
        return (-1);
    }

    if (st_pt.st_size > size_max_pt) {
        fprintf(stderr, "ptable size invalid (%d > %d)\n", st_pt.st_size, size_max_pt);
        return (-1);
    }

    outsize = ei_map[i].app_addr + st_app.st_size;
    if ((out = malloc(outsize)) == NULL) {
        perror("malloc");
        return (-1);
    }

    memset(out, 0xff, outsize);

    if (read(fd_bl, &out[ei_map[i].bootloader_addr], st_bl.st_size) != st_bl.st_size) {
        perror("read");
        return (-1);
    }

    if (read(fd_pt, &out[ei_map[i].ptable_addr], st_pt.st_size) != st_pt.st_size) {
        perror("read");
        return (-1);
    }

    if (read(fd_app, &out[ei_map[i].app_addr], st_app.st_size) != st_app.st_size) {
        perror("read");
        return (-1);
    }

    if (write(fd_out, out, outsize) != outsize) {
        perror("write");
        return (-1);
    }

    close(fd_bl);
    close(fd_pt);
    close(fd_app);

    close(fd_out);

    fprintf(stdout, "done\n");

    return 0;
}

