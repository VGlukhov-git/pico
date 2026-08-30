#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "ff.h" // FatFs
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>

// ==== Pin Definitions ====
#define BTN1 2
#define BTN3 3
#define BTN2 17
#define BTN4 15

#define SPI_DISPLAY spi1
#define PIN_SCK_DISP 10
#define PIN_MOSI_DISP 11
#define PIN_CS_DISP 9
#define PIN_DC_DISP 8
#define PIN_RST_DISP 12

#define SPI_SD spi0
#define PIN_SCK_SD 18
#define PIN_MOSI_SD 19
#define PIN_MISO_SD 16
#define PIN_CS_SD 17

// ==== Globals ====
FATFS fs;
std::vector<std::string> animFolders;
std::vector<std::string> frames;
int animIndex = 0;

// ==== SPI Helpers ====
void spi_write_cmd(uint8_t cmd)
{
    gpio_put(PIN_DC_DISP, 0);
    gpio_put(PIN_CS_DISP, 0);
    spi_write_blocking(SPI_DISPLAY, &cmd, 1);
    gpio_put(PIN_CS_DISP, 1);
}

void spi_write_data(const uint8_t *data, size_t len)
{
    gpio_put(PIN_DC_DISP, 1);
    gpio_put(PIN_CS_DISP, 0);
    spi_write_blocking(SPI_DISPLAY, data, len);
    gpio_put(PIN_CS_DISP, 1);
}

void display_reset()
{
    gpio_put(PIN_RST_DISP, 0);
    sleep_ms(50);
    gpio_put(PIN_RST_DISP, 1);
    sleep_ms(50);
}

void init_display()
{
    display_reset();
    spi_write_cmd(0x01);
    sleep_ms(10);
    spi_write_cmd(0x11);
    sleep_ms(100);
    spi_write_cmd(0x36);
    {
        uint8_t d[] = {0x60};
        spi_write_data(d, 1);
    }
    spi_write_cmd(0x3A);
    {
        uint8_t d[] = {0x55};
        spi_write_data(d, 1);
    }
    spi_write_cmd(0x21);
    spi_write_cmd(0x13);
    spi_write_cmd(0x29);
    sleep_ms(100);
}

void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t data[4];
    spi_write_cmd(0x2A);
    data[0] = x0 >> 8;
    data[1] = x0 & 0xFF;
    data[2] = x1 >> 8;
    data[3] = x1 & 0xFF;
    spi_write_data(data, 4);

    spi_write_cmd(0x2B);
    data[0] = y0 >> 8;
    data[1] = y0 & 0xFF;
    data[2] = y1 >> 8;
    data[3] = y1 & 0xFF;
    spi_write_data(data, 4);

    spi_write_cmd(0x2C);
}

// ==== SD Card + FAT FS ====
bool mount_sd()
{
    FRESULT fr = f_mount(&fs, "", 1);
    return (fr == FR_OK);
}

std::vector<std::string> list_dir(const char *path)
{
    std::vector<std::string> result;
    DIR dir;
    FILINFO fno;
    if (f_opendir(&dir, path) == FR_OK)
    {
        while (true)
        {
            if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == 0)
                break;
            if (fno.fattrib & AM_DIR)
            {
                if (fno.fname[0] != '.')
                    result.push_back(std::string(fno.fname));
            }
        }
        f_closedir(&dir);
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::vector<std::string> list_raw_files(const std::string &folder)
{
    std::vector<std::string> files;
    DIR dir;
    FILINFO fno;
    if (f_opendir(&dir, folder.c_str()) == FR_OK)
    {
        while (true)
        {
            if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == 0)
                break;
            if (strstr(fno.fname, ".raw"))
                files.push_back(std::string(fno.fname));
        }
        f_closedir(&dir);
    }
    std::sort(files.begin(), files.end());
    return files;
}

// ==== Button ====
bool button_pressed(uint pin)
{
    return gpio_get(pin) == 0;
}

// ==== Display Animation ====
void show_raw_stream(const std::string &path, int w = 320, int h = 240, int chunk = 242)
{
    FIL file;
    if (f_open(&file, path.c_str(), FA_READ) != FR_OK)
    {
        printf("Failed to open %s\n", path.c_str());
        return;
    }

    set_window(0, 0, w - 1, h - 1);
    const int bufsize = w * 2 * chunk;
    uint8_t *buffer = new uint8_t[bufsize];

    for (int y = 0; y < h; y += chunk)
    {
        UINT br;
        f_read(&file, buffer, bufsize, &br);
        if (br > 0)
            spi_write_data(buffer, br);
    }
    delete[] buffer;
    f_close(&file);
}

void load_frames()
{
    std::string folder = std::string("/sd/animations_raw/") + animFolders[animIndex];
    frames = list_raw_files(folder);
    printf("Loaded animation: %s\n", folder.c_str());
}

int main()
{
    stdio_init_all();

    // ==== GPIO Init ====
    gpio_init(BTN1);
    gpio_set_dir(BTN1, GPIO_IN);
    gpio_pull_up(BTN1);
    gpio_init(BTN2);
    gpio_set_dir(BTN2, GPIO_IN);
    gpio_pull_up(BTN2);
    gpio_init(BTN3);
    gpio_set_dir(BTN3, GPIO_IN);
    gpio_pull_up(BTN3);
    gpio_init(BTN4);
    gpio_set_dir(BTN4, GPIO_IN);
    gpio_pull_up(BTN4);

    // ==== SPI Display Init ====
    spi_init(SPI_DISPLAY, 40000000);
    gpio_set_function(PIN_SCK_DISP, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI_DISP, GPIO_FUNC_SPI);
    gpio_init(PIN_CS_DISP);
    gpio_set_dir(PIN_CS_DISP, GPIO_OUT);
    gpio_init(PIN_DC_DISP);
    gpio_set_dir(PIN_DC_DISP, GPIO_OUT);
    gpio_init(PIN_RST_DISP);
    gpio_set_dir(PIN_RST_DISP, GPIO_OUT);
    gpio_put(PIN_CS_DISP, 1);

    init_display();

    // ==== Mount SD ====
    if (!mount_sd())
    {
        printf("Failed to mount SD card.\n");
        while (1)
            tight_loop_contents();
    }
    printf("Mounted SD card.\n");

    animFolders = list_dir("/sd/animations_raw");
    if (animFolders.empty())
    {
        printf("No animation folders found.\n");
        while (1)
            tight_loop_contents();
    }

    load_frames();

    // ==== Main Loop ====
    while (true)
    {
        std::string folder = std::string("/sd/animations_raw/") + animFolders[animIndex];
        for (auto &frame : frames)
        {
            std::string path = folder + "/" + frame;
            show_raw_stream(path);
            sleep_ms(50);
        }

        // Check buttons to switch animation
        if (button_pressed(BTN2) || button_pressed(BTN4))
        {
            animIndex = (animIndex + 1) % animFolders.size();
            load_frames();
        }
    }
}
