#include "pico/stdlib.h"
#include "hardware/spi.h"

// === Pin configuration ===
#define SPI_PORT spi1
#define PIN_SCK 10
#define PIN_MOSI 11
#define PIN_CS 9
#define PIN_DC 8
#define PIN_RST 12

// === Display resolution ===
#define TFT_WIDTH 320
#define TFT_HEIGHT 240

// === Colors ===
#define BLACK 0x0000
#define RED 0xF800
#define GREEN 0x07E0
#define BLUE 0x001F
#define WHITE 0xFFFF
#define CYAN 0x07FF
#define MAGENTA 0xF81F
#define YELLOW 0xFFE0

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

// === Low-level SPI ===
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

// === Set address window ===
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

// === Fill screen ===
void fill_color(uint16_t color)
{
    uint8_t data[2] = {color >> 8, color & 0xFF};
    st7789_set_address_window(0, 0, TFT_WIDTH - 1, TFT_HEIGHT - 1);

    gpio_put(PIN_DC, 1);
    gpio_put(PIN_CS, 0);
    for (int i = 0; i < TFT_WIDTH * TFT_HEIGHT; i++)
    {
        spi_write_blocking(SPI_PORT, data, 2);
    }
    gpio_put(PIN_CS, 1);
}

// === Helper: Extract RGB from 565 ===
static inline void rgb565_to_components(uint16_t color, uint8_t *r, uint8_t *g, uint8_t *b)
{
    *r = (color >> 11) & 0x1F;
    *g = (color >> 5) & 0x3F;
    *b = color & 0x1F;
}

// === Helper: Combine RGB into 565 ===
static inline uint16_t components_to_rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return (r << 11) | (g << 5) | b;
}

// === Fade between two colors ===
void fade_color(uint16_t color1, uint16_t color2, int steps, int delay_ms)
{
    uint8_t r1, g1, b1, r2, g2, b2;
    rgb565_to_components(color1, &r1, &g1, &b1);
    rgb565_to_components(color2, &r2, &g2, &b2);

    for (int i = 0; i <= steps; i++)
    {
        uint8_t r = r1 + (r2 - r1) * i / steps;
        uint8_t g = g1 + (g2 - g1) * i / steps;
        uint8_t b = b1 + (b2 - b1) * i / steps;
        uint16_t color = components_to_rgb565(r, g, b);
        fill_color(color);
        sleep_ms(delay_ms);
    }
}

// === Main ===
int main()
{
    stdio_init_all();

    spi_init(SPI_PORT, 80000000);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    gpio_init(PIN_CS);
    gpio_set_dir(PIN_CS, GPIO_OUT);
    gpio_put(PIN_CS, 1);
    gpio_init(PIN_DC);
    gpio_set_dir(PIN_DC, GPIO_OUT);
    gpio_init(PIN_RST);
    gpio_set_dir(PIN_RST, GPIO_OUT);

    st7789_init();

    uint16_t colors[] = {RED, GREEN, BLUE, YELLOW, MAGENTA, CYAN, WHITE, BLACK};
    int color_count = sizeof(colors) / sizeof(colors[0]);

    while (true)
    {
        for (int i = 0; i < color_count - 1; i++)
        {
            fade_color(colors[i], colors[i + 1], 50, 2); // 50 steps, 20ms delay each
        }
        fade_color(colors[color_count - 1], colors[0], 10, 20); // loop back
    }
}
