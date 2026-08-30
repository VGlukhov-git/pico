#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include "lwip/netif.h"
#include "lwip/ip4_addr.h"

#include "modules/display.h"
#include "modules/mpu6050.h"

#include "hardware/clocks.h"
#include "hardware/i2c.h"

#include "pico/multicore.h"
#include "web/page.h"
#include "ff.h"
#include "sd_card.h"
#include "hw_config.h"

/* ================= CONFIG ================= */
#define WIFI_SSID "VIVACOM_2C04"
#define WIFI_PASSWORD "4x45ESFRy44b"
#define HTTP_PORT 80

#define SPI_PORT spi1

#define PIN_SCK 10
#define PIN_MOSI 11
#define PIN_CS 9
#define PIN_DC 8
#define PIN_RST 12

/* ==== SD SPI Pins ==== */
#define SPI_SD spi0
#define PIN_SCK_SD 18
#define PIN_MOSI_SD 19
#define PIN_MISO_SD 16
#define PIN_CS_SD 17
/* ===================== */

#define TFT_WIDTH 320
#define TFT_HEIGHT 240

#define ANIM_FPS 24
#define MAX_FRAMES 15
#define ANIM_NAME_LEN 16

/* ===== Animation State ===== */
static volatile bool anim_changed = false;
static char requested_anim[ANIM_NAME_LEN] = "";

/* ===== Gyro Shared State ===== */
volatile float g_pitch = 0.0f;
volatile float g_roll = 0.0f;
volatile float g_yaw = 0.0f;

/* ========================================================= */
/* ================= CORE 1: MPU TASK ====================== */
/* ========================================================= */

void core1_mpu_task()
{
    float pitch = 0.0f, roll = 0.0f, yaw = 0.0f;
    absolute_time_t prev = get_absolute_time();

    while (true)
    {
        absolute_time_t now = get_absolute_time();
        float dt = (float)absolute_time_diff_us(prev, now) / 1000000.0f;
        prev = now;

        mpu6050_compute_angles(&pitch, &roll, &yaw, dt);

        g_pitch = pitch;
        g_roll = roll;
        g_yaw = yaw;

        sleep_ms(10); // ~100 Hz
    }
}

/* ========================================================= */
/* ================= ANIMATION CONTROL ===================== */
/* ========================================================= */

void request_animation(const char *name)
{
    strncpy(requested_anim, name, ANIM_NAME_LEN - 1);
    requested_anim[ANIM_NAME_LEN - 1] = '\0';
    anim_changed = true;
}

void animation_player_tick(void)
{
    static char current_anim[ANIM_NAME_LEN] = "";
    static int frame = 0;
    static absolute_time_t next_frame_time;

    if (anim_changed)
    {
        strcpy(current_anim, requested_anim);
        anim_changed = false;
        frame = 0;
        next_frame_time = get_absolute_time();
    }

    if (current_anim[0] == '\0')
        return;

    if (absolute_time_diff_us(get_absolute_time(), next_frame_time) > 0)
        return;

    char path[64];
    FIL file;
    UINT br;

    snprintf(path, sizeof(path),
             "animations_raw/%s/%s_%02d.raw",
             current_anim, current_anim, frame);

    if (f_open(&file, path, FA_READ) == FR_OK)
    {
        st7789_set_address_window(0, 0, TFT_WIDTH - 1, TFT_HEIGHT - 1);

        uint8_t buffer[512];
        while (f_read(&file, buffer, sizeof(buffer), &br) == FR_OK && br)
        {
            st7789_write_data(buffer, br);
        }
        f_close(&file);
    }

    frame = (frame + 1) % MAX_FRAMES;
    next_frame_time = make_timeout_time_ms(1000 / ANIM_FPS);
}

/* ========================================================= */
/* ================= DISPLAY HELPERS ======================= */
/* ========================================================= */

void st7789_fill_screen(uint16_t color)
{
    uint8_t data[2] = {color >> 8, color & 0xFF};

    st7789_set_address_window(0, 0, TFT_WIDTH - 1, TFT_HEIGHT - 1);

    gpio_put(PIN_DC, 1);
    gpio_put(PIN_CS, 0);

    uint8_t buf[256];
    for (int i = 0; i < 128; i++)
    {
        buf[i * 2] = data[0];
        buf[i * 2 + 1] = data[1];
    }

    for (int i = 0; i < (TFT_WIDTH * TFT_HEIGHT) / 128; i++)
        spi_write_blocking(SPI_PORT, buf, 256);

    gpio_put(PIN_CS, 1);
}

/* ===== SHOW IP ON DISPLAY (NEW FEATURE) ===== */
void display_show_ip(const char *ip)
{
    // Clear screen
    st7789_fill_screen(0x0000); // black

    // Title
    draw_text(40, 60, "WiFi Connected", 0x07E0, 2); // green, scale 2

    // Label
    draw_text(40, 110, "IP Address:", 0xFFFF, 2); // white, scale 2

    // IP itself (bigger)
    draw_text(40, 150, ip, 0x00FF, 2); // cyan, scale 2

    // Keep visible for a few seconds
    sleep_ms(3000);

    // Clear before normal operation
    st7789_fill_screen(0x0000);
}

/* ========================================================= */
/* ================= HTTP SERVER ============================ */
/* ========================================================= */

static err_t tcp_write_all(struct tcp_pcb *tpcb, const char *data, size_t len)
{
    size_t sent = 0;

    while (sent < len)
    {
        u16_t space = tcp_sndbuf(tpcb);
        if (space == 0)
        {
            tcp_output(tpcb);
            sleep_ms(1);
            continue;
        }

        size_t chunk = len - sent;
        if (chunk > space)
            chunk = space;
        if (chunk > 1024)
            chunk = 1024;

        err_t e = tcp_write(tpcb, data + sent, (u16_t)chunk, TCP_WRITE_FLAG_COPY);
        if (e == ERR_MEM)
        {
            tcp_output(tpcb);
            sleep_ms(1);
            continue;
        }
        if (e != ERR_OK)
            return e;

        sent += chunk;
    }

    return tcp_output(tpcb);
}

static err_t http_recv(void *arg, struct tcp_pcb *tpcb,
                       struct pbuf *p, err_t err)
{
    if (!p)
    {
        tcp_close(tpcb);
        return ERR_OK;
    }

    tcp_recved(tpcb, p->tot_len);

    char request[128];
    int len = p->tot_len < sizeof(request) - 1 ? p->tot_len : sizeof(request) - 1;
    memcpy(request, p->payload, len);
    request[len] = '\0';

    /* ===== Gyro JSON ===== */
    if (strstr(request, "GET /gyro"))
    {
        char json[128];
        int n = snprintf(json, sizeof(json),
                         "HTTP/1.1 200 OK\r\n"
                         "Content-Type: application/json\r\n"
                         "Connection: close\r\n\r\n"
                         "{\"pitch\":%.2f,\"roll\":%.2f}",
                         g_pitch, g_roll);

        tcp_write_all(tpcb, json, (size_t)n);
        pbuf_free(p);
        tcp_shutdown(tpcb, 0, 1);
        return ERR_OK;
    }

    /* ===== Animation commands ===== */
    if (strstr(request, "GET /red"))
        request_animation("angry");
    if (strstr(request, "GET /green"))
        request_animation("blink");
    if (strstr(request, "GET /blue"))
        request_animation("sleep");
    if (strstr(request, "GET /white"))
        request_animation("wave");

    /* ===== Root page ===== */
    if (strstr(request, "GET / HTTP"))
    {
        tcp_write_all(tpcb, cyberpunk_html, cyberpunk_html_len);
    }
    else
    {
        tcp_write_all(tpcb,
                      "HTTP/1.1 204 No Content\r\nConnection: close\r\n\r\n",
                      55);
    }

    tcp_output(tpcb);
    pbuf_free(p);
    tcp_shutdown(tpcb, 0, 1);
    return ERR_OK;
}

static err_t http_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    tcp_recv(newpcb, http_recv);
    return ERR_OK;
}

static void http_server_init(void)
{
    struct tcp_pcb *pcb = tcp_new();
    tcp_bind(pcb, IP_ADDR_ANY, HTTP_PORT);
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, http_accept);
}

/* ========================================================= */
/* ================= SD ==================================== */
/* ========================================================= */

FATFS fs;

bool mount_sd(void)
{
    hw_config_init();
    if (!sd_init_driver())
        return false;

    if (f_mount(&fs, "", 1) != FR_OK)
        return false;

    spi_set_baudrate(spi0, 20000000);
    return true;
}

/* ========================================================= */
/* ================= MAIN ================================== */
/* ========================================================= */

int main(void)
{
    stdio_init_all();
    sleep_ms(2000);

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

    /* ==== I2C (MPU) ==== */
    i2c_init(i2c_default, 400000);
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
    gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);
    mpu6050_reset();

    /* ==== WiFi ==== */
    if (cyw43_arch_init())
        return 1;

    cyw43_arch_enable_sta_mode();

    int wifi_res = cyw43_arch_wifi_connect_timeout_ms(
        WIFI_SSID, WIFI_PASSWORD,
        CYW43_AUTH_WPA2_AES_PSK, 30000);

    if (wifi_res)
    {
        printf("WiFi failed (%d)\n", wifi_res);
        return 1;
    }

    struct netif *netif = &cyw43_state.netif[CYW43_ITF_STA];
    const char *ip_str = ip4addr_ntoa(netif_ip4_addr(netif));

    printf("WiFi connected, IP=%s\n", ip_str);

    /* === SHOW IP ON TFT === */
    display_show_ip(ip_str);

    http_server_init();

    if (!mount_sd())
        while (1)
            tight_loop_contents();

    multicore_launch_core1(core1_mpu_task);

    while (true)
    {
        animation_player_tick();
        sleep_ms(1);
    }
}
