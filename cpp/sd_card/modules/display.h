#pragma once
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include <cstdint>
#include <string>

// ==== Display Pin Definitions ====
#define SPI_DISPLAY spi1
#define PIN_SCK_DISP 10
#define PIN_MOSI_DISP 11
#define PIN_CS_DISP 9
#define PIN_DC_DISP 8
#define PIN_RST_DISP 12

// ==== Display API ====
void st7789_init();
void st7789_set_address_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void show_raw_stream(const std::string &path, int w = 320, int h = 240);
void draw_text(uint16_t x, uint16_t y, const std::string &text, uint16_t color, uint8_t scale);
