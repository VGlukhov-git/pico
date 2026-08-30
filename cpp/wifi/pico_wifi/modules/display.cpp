#include "display.h"
#include "ff.h"
#include <cstdio>
#include <string>

#define SPI_PORT spi1
#define PIN_SCK 10
#define PIN_MOSI 11
#define PIN_CS 9
#define PIN_DC 8
#define PIN_RST 12

// === ST7789 Commands ===
#define ST77XX_SWRESET 0x01
#define ST77XX_SLPOUT 0x11
#define ST77XX_COLMOD 0x3A
#define ST77XX_MADCTL 0x36
#define ST77XX_CASET 0x2A
#define ST77XX_RASET 0x2B
#define ST77XX_RAMWR 0x2C
#define ST77XX_DISPON 0x29
#define ST77XX_INVON 0x21

void st7789_write_cmd(uint8_t cmd)
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

void st7789_reset()
{
    gpio_put(PIN_RST, 0);
    sleep_ms(50);
    gpio_put(PIN_RST, 1);
    sleep_ms(150);
}

void st7789_init()
{
    st7789_reset();
    st7789_write_cmd(ST77XX_SWRESET);
    sleep_ms(150);
    st7789_write_cmd(ST77XX_SLPOUT);
    sleep_ms(120);
    st7789_write_cmd(ST77XX_COLMOD);
    uint8_t color_mode = 0x55; // 16-bit color
    st7789_write_data(&color_mode, 1);
    sleep_ms(10);
    st7789_write_cmd(ST77XX_MADCTL);
    uint8_t madctl = 0xA0;
    st7789_write_data(&madctl, 1);
    st7789_write_cmd(ST77XX_INVON);
    st7789_write_cmd(ST77XX_DISPON);
    sleep_ms(100);
}

void st7789_set_address_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
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

void show_raw_stream(const std::string &path, int w, int h)
{
    FIL file;
    if (f_open(&file, path.c_str(), FA_READ) != FR_OK)
    {
        printf("Failed to open %s\n", path.c_str());
        return;
    }

    const uint32_t frame_bytes = w * h * 2;
    uint8_t *buffer = new uint8_t[frame_bytes];
    if (!buffer)
    {
        printf("Out of memory for frame buffer!\n");
        f_close(&file);
        return;
    }

    UINT br;
    FRESULT res = f_read(&file, buffer, frame_bytes, &br);
    if (res != FR_OK || br != frame_bytes)
    {
        printf("Read error or incomplete read on %s (%u bytes)\n", path.c_str(), br);
        delete[] buffer;
        f_close(&file);
        return;
    }

    st7789_set_address_window(0, 0, w - 1, h - 1);
    st7789_write_data(buffer, frame_bytes);

    delete[] buffer;
    f_close(&file);
}

static const uint8_t font5x7[][5] = {
#include "font5x7.inc" // You can define or generate this header
};

static void draw_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    st7789_set_address_window(x, y, x, y);
    uint8_t data[2] = {(uint8_t)(color >> 8), (uint8_t)(color & 0xFF)};
    st7789_write_data(data, 2);
}

// Draw a single character using the 5x7 font
static void draw_char_scaled(uint16_t x, uint16_t y, char c, uint16_t color, uint8_t scale)
{
    if (c < 32 || c > 126)
        return; // unsupported char

    const uint8_t *glyph = font5x7[c - 32];

    for (int col = 0; col < 5; col++)
    {
        uint8_t line = glyph[col];
        for (int row = 0; row < 7; row++)
        {
            if (line & 0x01)
            {
                // Draw a scale×scale block instead of a single pixel
                for (int dx = 0; dx < scale; dx++)
                {
                    for (int dy = 0; dy < scale; dy++)
                    {
                        draw_pixel(x + col * scale + dx, y + row * scale + dy, color);
                    }
                }
            }
            line >>= 1;
        }
    }
}

// Draw a string of text
void draw_text(uint16_t x, uint16_t y, const std::string &text, uint16_t color, uint8_t scale)
{
    for (char c : text)
    {
        if (c == '\n')
        {
            y += 8;
            x = 0;
        }
        else
        {
            draw_char_scaled(x, y, c, color, scale);
            x += 6 * scale; // 5 pixels + 1 space
        }
    }
}