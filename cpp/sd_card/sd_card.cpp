#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "pico/multicore.h"
#include "ff.h"
#include "sd_card.h"
#include "hw_config.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>
#include "modules/display.h"
#include "modules/mpu6050.h"
#include "hardware/clocks.h"

// ==== Button Pins ====
#define BTN1 2
#define BTN2 14
#define BTN3 3
#define BTN4 15

// ==== SD SPI Pins ====
#define SPI_SD spi0
#define PIN_SCK_SD 18
#define PIN_MOSI_SD 19
#define PIN_MISO_SD 16
#define PIN_CS_SD 17

// ==== Display SPI ====
#define SPI_PORT spi1
#define PIN_SCK 10
#define PIN_MOSI 11
#define PIN_CS 9
#define PIN_DC 8
#define PIN_RST 12

#define TFT_WIDTH 320
#define TFT_HEIGHT 240

#define SERVO1_PIN 20
#define SERVO2_PIN 21
#define SERVO3_PIN 22
#define SERVO4_PIN 7

// ==== Globals ====
FATFS fs;
std::vector<std::string> animFolders;
std::vector<std::string> frames;
int animIndex = 0;

std::string currentAnimationName;
// ==== Shared flags ====
volatile bool next_anim_flag = false;
volatile bool prev_anim_flag = false;

// ==== Shared orientation ====
struct Orientation
{
    float pitch, roll, yaw;
};
volatile Orientation latest_angles = {0, 0, 0};

// Typical 50 Hz servo parameters
#define SERVO_PWM_FREQ 50
#define SERVO_MIN_US 500      // 0°  → 0.5 ms pulse
#define SERVO_MAX_US 1900     // 180° → 2.5 ms pulse
#define SERVO_PERIOD_US 20000 // 20 ms period (1 / 50 Hz)

// Store PWM slice numbers for each servo
uint pwm_slices[4];

// === Convert angle to duty cycle ===
uint16_t angle_to_duty(float angle)
{
    angle = std::clamp(angle, 0.0f, 180.0f);
    const float range = SERVO_MAX_US - SERVO_MIN_US;
    float pulse = SERVO_MIN_US + (angle / 180.0f) * range;
    float duty = (pulse / SERVO_PERIOD_US) * 65535.0f;
    return static_cast<uint16_t>(duty + 0.5f);
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

void load_frames()
{
    std::string folder = std::string("/animations_raw/") + animFolders[animIndex];
    frames = list_raw_files(folder);
    draw_text(10, 20, folder, 0xffff, 2);
    printf("Loaded animation: %s\n", folder.c_str());
}

void load_frames_from_name(std::string folderName)
{
    std::string folder = std::string("/animations_raw/") + folderName;
    frames = list_raw_files(folder);
    printf("Loaded animation: %s\n", folder.c_str());
}

// === Initialize servo PWM ===
void servo_init()
{
    int servo_pins[4] = {SERVO1_PIN, SERVO2_PIN, SERVO3_PIN, SERVO4_PIN};

    for (int i = 0; i < 4; i++)
    {
        gpio_set_function(servo_pins[i], GPIO_FUNC_PWM);
        uint slice = pwm_gpio_to_slice_num(servo_pins[i]);
        pwm_slices[i] = slice;

        pwm_config config = pwm_get_default_config();
        pwm_config_set_clkdiv(&config, 64.f); // Scale down clock to reach ~50 Hz
        pwm_init(slice, &config, true);

        uint32_t sys_clk = clock_get_hz(clk_sys);
        uint32_t wrap = sys_clk / (SERVO_PWM_FREQ * 64) - 1;
        pwm_set_wrap(slice, wrap);

        pwm_set_enabled(slice, true);
        pwm_set_gpio_level(servo_pins[i], angle_to_duty(90)); // Center
    }
}

// === Set servo angle (0–180°) ===
void servo_write(int servo, float angle)
{
    if (servo < 1 || servo > 4)
        return;
    uint pin = SERVO1_PIN + (servo - 1);
    uint slice = pwm_slices[servo - 1];
    pwm_set_gpio_level(pin, angle_to_duty(angle));
}

// ================== MPU6050 THREAD ==================
void core1_mpu_task()
{
    float pitch = 0, roll = 0, yaw = 0;
    bool prev1 = false, prev2 = false, prev3 = false, prev4 = false;
    absolute_time_t prev = get_absolute_time();

    float value_pair_1 = 90;
    bool frameLoaded = false;
    while (true)
    {
        absolute_time_t now = get_absolute_time();
        float dt = absolute_time_diff_us(prev, now) / 1e6f;
        prev = now;

        // Read new orientation via I2C
        mpu6050_compute_angles(&pitch, &roll, &yaw, dt);

        // Update shared variable atomically
        latest_angles.pitch = pitch;
        latest_angles.roll = roll;
        latest_angles.yaw = yaw;

        bool b1 = gpio_get(BTN1) == 0;
        bool b2 = gpio_get(BTN2) == 0;
        bool b3 = gpio_get(BTN3) == 0;
        bool b4 = gpio_get(BTN4) == 0;

        if (b1 && !prev1)
            next_anim_flag = true;
        if (b2 && !prev2)
            next_anim_flag = true;
        if (b3 && !prev3)
            prev_anim_flag = true;
        if (b4 && !prev4)
            next_anim_flag = true;

        if (pitch < -2 && !prev1)
        {
            prev1 = true;
            next_anim_flag = true;
        }
        if (pitch > 0 && prev1)
        {
            prev1 = false;
        }
        float dA = fabs(roll / 10);

        if (roll < -0.2)
        {
            if (!frameLoaded)
            {
                // load_frames_from_name("angry");
                frameLoaded = true;
            }
            value_pair_1 += dA;
        }

        if (roll > 0.2)
        {
            if (!frameLoaded)
            {
                // load_frames_from_name("angry");
                frameLoaded = true;
            }
            value_pair_1 -= dA;
        }

        if (roll > -0.2 && roll < 0.2)
        {
            frameLoaded = false;
        }
        if (value_pair_1 > 180)
        {
            value_pair_1 = 180;
        }

        if (value_pair_1 < 0)
        {
            value_pair_1 = 0;
        }

        if (value_pair_1 <= 90)
        {
            servo_write(1, value_pair_1);
            servo_write(2, 180 - value_pair_1);
        }
        if (value_pair_1 >= 90)
        {
            servo_write(3, 180 - value_pair_1);
            servo_write(4, value_pair_1);
        }

        prev2 = b2;
        prev3 = b3;
        prev4 = b4;
        sleep_ms(1);
    }
}

// ================== Utility functions ==================
void fill_color(uint16_t color)
{
    uint8_t data[2] = {uint8_t(color >> 8), uint8_t(color & 0xFF)};
    st7789_set_address_window(0, 0, TFT_WIDTH - 1, TFT_HEIGHT - 1);

    gpio_put(PIN_DC, 1);
    gpio_put(PIN_CS, 0);
    const int chunk = 128;
    uint8_t buf[chunk * 2];
    for (int i = 0; i < chunk; i++)
    {
        buf[i * 2] = data[0];
        buf[i * 2 + 1] = data[1];
    }
    for (int i = 0; i < (TFT_WIDTH * TFT_HEIGHT) / chunk; i++)
    {
        spi_write_blocking(SPI_PORT, buf, sizeof(buf));
    }
    gpio_put(PIN_CS, 1);
}

bool mount_sd()
{
    hw_config_init();
    if (!sd_init_driver())
    {
        printf("SD init failed.\n");
        fill_color(0x001F);
        return false;
    }

    FRESULT fr = f_mount(&fs, "", 1);
    if (fr != FR_OK)
    {
        fill_color(0xF800);
        printf("f_mount failed (%d)\n", fr);
        return false;
    }

    spi_set_baudrate(spi0, 20000000);
    printf("SD mounted!\n");
    return true;
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
            if (fno.fattrib & AM_DIR && fno.fname[0] != '.')
                result.push_back(std::string(fno.fname));
        }
        f_closedir(&dir);
    }
    std::sort(result.begin(), result.end());
    return result;
}

// ================== MAIN ==================
int main()
{
    stdio_init_all();

    // ==== Buttons ====
    for (int pin : {BTN1, BTN2, BTN3, BTN4})
    {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        gpio_pull_up(pin);
    }

    // ==== Display SPI ====
    spi_init(SPI_PORT, 80000000);
    servo_init();
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
    fill_color(0x0000);

    // ==== I2C (MPU) ====
    i2c_init(i2c_default, 400000);
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
    gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);
    mpu6050_reset();

    // ==== SD ====
    if (!mount_sd())
        while (true)
            tight_loop_contents();
    spi_set_baudrate(spi0, 30000000);

    // ==== Load animations ====
    animFolders = list_dir("/animations_raw");
    if (animFolders.empty())
    {
        printf("No animation folders found.\n");
        fill_color(0x07E0);
        while (true)
            tight_loop_contents();
    }
    load_frames();
    // load_frames_from_name("angry");

    // ==== Launch tasks ====
    multicore_launch_core1(core1_mpu_task);

    // ==== Main display loop ====
    while (true)
    {
        std::string folder = std::string("/animations_raw/") + animFolders[animIndex];

        for (auto &frame : frames)
        {
            std::string path = folder + "/" + frame;
            show_raw_stream(path);

            // Read latest orientation safely
            Orientation o;
            o.pitch = latest_angles.pitch;
            o.roll = latest_angles.roll;
            o.yaw = latest_angles.yaw;
            char buf[64];
            snprintf(buf, sizeof(buf), "P: %.1f  R: %.1f  Y: %.1f", o.pitch, o.roll, o.yaw);
            draw_text(0, 0, buf, 0x3334, 2);

            sleep_ms(30);

            if (next_anim_flag || prev_anim_flag)
            {
                if (next_anim_flag)
                {
                    animIndex = (animIndex + 1) % animFolders.size();
                    next_anim_flag = false;
                }
                if (prev_anim_flag)
                {
                    animIndex = (animIndex - 1 + animFolders.size()) % animFolders.size();
                    prev_anim_flag = false;
                }
                load_frames();
                break;
            }
        }
    }
}
