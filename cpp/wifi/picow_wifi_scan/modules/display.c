#include "display.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "ff.h"
/* ==== Pin config ==== */
#define SPI_PORT spi1
#define PIN_SCK 10
#define PIN_MOSI 11
#define PIN_CS 9
#define PIN_DC 8
#define PIN_RST 12

/* ==== ST7789 Commands ==== */
#define ST77XX_SWRESET 0x01
#define ST77XX_SLPOUT 0x11
#define ST77XX_COLMOD 0x3A
#define ST77XX_MADCTL 0x36
#define ST77XX_CASET 0x2A
#define ST77XX_RASET 0x2B
#define ST77XX_RAMWR 0x2C
#define ST77XX_DISPON 0x29
#define ST77XX_INVON 0x21

/* ==== Low-level I/O ==== */

static void st7789_write_cmd(uint8_t cmd)
{
    gpio_put(PIN_DC, 0);
    gpio_put(PIN_CS, 0);
    spi_write_blocking(SPI_PORT, &cmd, 1);
    gpio_put(PIN_CS, 1);
}

void st7789_write_data(const uint8_t *data, size_t len)
{
    gpio_put(PIN_DC, 1);
    gpio_put(PIN_CS, 0);
    spi_write_blocking(SPI_PORT, data, len);
    gpio_put(PIN_CS, 1);
}

static void st7789_reset(void)
{
    gpio_put(PIN_RST, 0);
    sleep_ms(50);
    gpio_put(PIN_RST, 1);
    sleep_ms(150);
}

/* ==== Init ==== */

void st7789_init(void)
{
    spi_init(SPI_PORT, 40 * 1000 * 1000);

    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    gpio_init(PIN_CS);
    gpio_init(PIN_DC);
    gpio_init(PIN_RST);

    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_set_dir(PIN_DC, GPIO_OUT);
    gpio_set_dir(PIN_RST, GPIO_OUT);

    gpio_put(PIN_CS, 1);

    st7789_reset();

    st7789_write_cmd(ST77XX_SWRESET);
    sleep_ms(150);

    st7789_write_cmd(ST77XX_SLPOUT);
    sleep_ms(120);

    uint8_t colmod = 0x55; // RGB565
    st7789_write_cmd(ST77XX_COLMOD);
    st7789_write_data(&colmod, 1);

    uint8_t madctl = 0xA0; // Landscape + BGR
    st7789_write_cmd(ST77XX_MADCTL);
    st7789_write_data(&madctl, 1);

    st7789_write_cmd(ST77XX_INVON);
    st7789_write_cmd(ST77XX_DISPON);
    sleep_ms(100);
}

/* ==== Address Window ==== */

void st7789_set_address_window(uint16_t x0, uint16_t y0,
                               uint16_t x1, uint16_t y1)
{
    uint8_t data[4];

    st7789_write_cmd(ST77XX_CASET);
    data[0] = x0 >> 8;
    data[1] = x0 & 0xFF;
    data[2] = x1 >> 8;
    data[3] = x1 & 0xFF;
    st7789_write_data(data, 4);

    st7789_write_cmd(ST77XX_RASET);
    data[0] = y0 >> 8;
    data[1] = y0 & 0xFF;
    data[2] = y1 >> 8;
    data[3] = y1 & 0xFF;
    st7789_write_data(data, 4);

    st7789_write_cmd(ST77XX_RAMWR);
}

/* File reading*/
void show_raw_stream(const char *path, int w, int h)
{
    FIL file;
    FRESULT res;

    res = f_open(&file, path, FA_READ);
    if (res != FR_OK)
    {
        printf("Failed to open %s\n", path);
        return;
    }

    uint32_t frame_bytes = (uint32_t)w * (uint32_t)h * 2;

    uint8_t *buffer = (uint8_t *)malloc(frame_bytes);
    if (!buffer)
    {
        printf("Out of memory for frame buffer!\n");
        f_close(&file);
        return;
    }

    UINT br = 0;
    res = f_read(&file, buffer, frame_bytes, &br);
    if (res != FR_OK || br != frame_bytes)
    {
        printf("Read error or incomplete read on %s (%u bytes)\n", path, br);
        free(buffer);
        f_close(&file);
        return;
    }

    st7789_set_address_window(0, 0, w - 1, h - 1);
    st7789_write_data(buffer, frame_bytes);

    free(buffer);
    f_close(&file);
}

/* ==== Font ==== */
static const uint8_t font5x7[][5] = {
#include "font5x7.inc"
};

/* ==== Pixel ==== */

static void draw_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    uint8_t px[2] = {color >> 8, color & 0xFF};
    st7789_set_address_window(x, y, x, y);
    st7789_write_data(px, 2);
}

/* ==== Character ==== */

static void draw_char_scaled(uint16_t x, uint16_t y,
                             char c, uint16_t color, uint8_t scale)
{
    if (c < 32 || c > 126)
        return;

    const uint8_t *glyph = font5x7[c - 32];

    for (int col = 0; col < 5; col++)
    {
        uint8_t bits = glyph[col];
        for (int row = 0; row < 7; row++)
        {
            if (bits & 1)
            {
                for (int dx = 0; dx < scale; dx++)
                    for (int dy = 0; dy < scale; dy++)
                        draw_pixel(x + col * scale + dx,
                                   y + row * scale + dy,
                                   color);
            }
            bits >>= 1;
        }
    }
}

/* ==== Text ==== */

void draw_text(uint16_t x, uint16_t y,
               const char *text,
               uint16_t color,
               uint8_t scale)
{
    while (*text)
    {
        char c = *text++;

        if (c == '\n')
        {
            y += 8 * scale;
            x = 0;
        }
        else
        {
            draw_char_scaled(x, y, c, color, scale);
            x += 6 * scale;
        }
    }
}
