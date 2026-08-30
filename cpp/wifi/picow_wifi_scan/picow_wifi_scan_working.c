#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "hardware/spi.h"
#include "hardware/gpio.h"

#include "lwip/tcp.h"
#include "lwip/pbuf.h"

#include "modules/display.h"

/* ================= WIFI / TCP ================= */

#define WIFI_SSID "VIVACOM_2C04"
#define WIFI_PASSWORD "4x45ESFRy44b"

#define SERVER_IP "192.168.1.5"
#define SERVER_PORT 4242

#define DEBUG_printf printf

/* ================= TFT (ST7789) ================= */

#define SPI_PORT spi1

#define PIN_SCK 10
#define PIN_MOSI 11
#define PIN_CS 9
#define PIN_DC 8
#define PIN_RST 12

#define TFT_WIDTH 320
#define TFT_HEIGHT 240

/* ST7789 320x240 OFFSETS */
#define ST7789_X_OFFSET 0
#define ST7789_Y_OFFSET 80

/* ================= COLORS (RGB565) ================= */

#define COLOR_BLACK 0x0000
#define COLOR_RED 0xF800
#define COLOR_GREEN 0x07E0
#define COLOR_BLUE 0x001F
#define COLOR_YELLOW 0xFFE0

void st7789_fill_screen(uint16_t color)
{
    uint8_t data[2] = {
        (uint8_t)(color >> 8),
        (uint8_t)(color & 0xFF)};

    st7789_set_address_window(0, 0, TFT_WIDTH - 1, TFT_HEIGHT - 1);

    gpio_put(PIN_DC, 1);
    gpio_put(PIN_CS, 0);

    const int chunk = 128;
    uint8_t buf[chunk * 2];

    // Prepare one chunk of pixels
    for (int i = 0; i < chunk; i++)
    {
        buf[i * 2] = data[0];
        buf[i * 2 + 1] = data[1];
    }

    int total_pixels = TFT_WIDTH * TFT_HEIGHT;
    int full_chunks = total_pixels / chunk;
    int remainder = total_pixels % chunk;

    // Send full chunks
    for (int i = 0; i < full_chunks; i++)
    {
        spi_write_blocking(SPI_PORT, buf, chunk * 2);
    }

    // Send remaining pixels (if any)
    if (remainder > 0)
    {
        spi_write_blocking(SPI_PORT, buf, remainder * 2);
    }

    gpio_put(PIN_CS, 1);
}

void handle_display_command(const char *cmd)
{
    if (strcmp(cmd, "up\n") == 0)
        st7789_fill_screen(COLOR_RED);
    else if (strcmp(cmd, "down\n") == 0)
        st7789_fill_screen(COLOR_BLUE);
    else if (strcmp(cmd, "left\n") == 0)
        st7789_fill_screen(COLOR_GREEN);
    else if (strcmp(cmd, "right\n") == 0)
        st7789_fill_screen(COLOR_YELLOW);
    else
        st7789_fill_screen(COLOR_BLACK);
}

/* ================= TCP ================= */

static err_t tcp_recv_cb(void *arg, struct tcp_pcb *pcb,
                         struct pbuf *p, err_t err)
{
    if (!p)
        return ERR_OK;

    char buf[32];
    uint16_t n = p->tot_len;
    if (n > sizeof(buf) - 1)
        n = sizeof(buf) - 1;

    pbuf_copy_partial(p, buf, n, 0);
    buf[n] = 0;

    tcp_recved(pcb, p->tot_len);
    pbuf_free(p);

    DEBUG_printf("RX: %s", buf);
    handle_display_command(buf);

    return ERR_OK;
}

static err_t tcp_connected_cb(void *arg, struct tcp_pcb *pcb, err_t err)
{
    if (err != ERR_OK)
        return err;
    DEBUG_printf("TCP connected\n");
    tcp_recv(pcb, tcp_recv_cb);
    return ERR_OK;
}

static bool tcp_connect_server(ip_addr_t *addr)
{
    struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_V4);
    if (!pcb)
        return false;

    cyw43_arch_lwip_begin();
    err_t err = tcp_connect(pcb, addr, SERVER_PORT, tcp_connected_cb);
    cyw43_arch_lwip_end();

    return err == ERR_OK;
}

/* ================= MAIN ================= */

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
    st7789_fill_screen(0x0000);

    sleep_ms(2000);

    if (cyw43_arch_init())
        return 1;
    cyw43_arch_enable_sta_mode();

    DEBUG_printf("Connecting Wi-Fi...\n");
    if (cyw43_arch_wifi_connect_timeout_ms(
            WIFI_SSID, WIFI_PASSWORD,
            CYW43_AUTH_WPA2_AES_PSK, 30000))
    {
        DEBUG_printf("Wi-Fi failed\n");
        return 1;
    }

    DEBUG_printf("Wi-Fi connected\n");

    ip_addr_t server_addr;
    ip4addr_aton(SERVER_IP, &server_addr);
    tcp_connect_server(&server_addr);

    while (true)
    {
#if PICO_CYW43_ARCH_POLL
        cyw43_arch_poll();
        cyw43_arch_wait_for_work_until(make_timeout_time_ms(100));
#else
        sleep_ms(100);
#endif
    }
}
