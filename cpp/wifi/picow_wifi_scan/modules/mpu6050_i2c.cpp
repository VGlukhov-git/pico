/**
 * @file mpu6050.cpp
 * @brief MPU6050 accelerometer/gyroscope I2C driver for Raspberry Pi Pico
 * @license BSD-3-Clause
 */

#include "mpu6050.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include <stdio.h>
#include <cmath>

// Default I2C address for MPU6050
constexpr uint8_t MPU6050_ADDR = 0x68;

// MPU6050 Register Addresses
constexpr uint8_t REG_PWR_MGMT_1 = 0x6B;
constexpr uint8_t REG_ACCEL_XOUT_H = 0x3B;
constexpr uint8_t REG_TEMP_OUT_H = 0x41;
constexpr uint8_t REG_GYRO_XOUT_H = 0x43;

#ifdef i2c_default

/**
 * @brief Write multiple bytes to MPU6050 register
 */
static void mpu6050_write(uint8_t reg, const uint8_t *data, size_t len)
{
    uint8_t buf[len + 1];
    buf[0] = reg;
    for (size_t i = 0; i < len; i++)
        buf[i + 1] = data[i];
    i2c_write_blocking(i2c_default, MPU6050_ADDR, buf, len + 1, false);
}

/**
 * @brief Read multiple bytes from MPU6050 register
 */
static void mpu6050_read(uint8_t reg, uint8_t *buf, size_t len)
{
    i2c_write_blocking(i2c_default, MPU6050_ADDR, &reg, 1, true);
    i2c_read_blocking(i2c_default, MPU6050_ADDR, buf, len, false);
}

void mpu6050_reset()
{
    uint8_t reset_cmd = 0x80;
    mpu6050_write(REG_PWR_MGMT_1, &reset_cmd, 1);
    sleep_ms(100);

    uint8_t wake_cmd = 0x00;
    mpu6050_write(REG_PWR_MGMT_1, &wake_cmd, 1);
    sleep_ms(10);
}

void mpu6050_read_raw(int16_t accel[3], int16_t gyro[3], int16_t *temp)
{
    uint8_t buffer[6];

    // Read accelerometer data
    mpu6050_read(REG_ACCEL_XOUT_H, buffer, 6);
    for (int i = 0; i < 3; i++)
        accel[i] = (buffer[i * 2] << 8) | buffer[i * 2 + 1];

    // Read temperature data
    mpu6050_read(REG_TEMP_OUT_H, buffer, 2);
    *temp = (buffer[0] << 8) | buffer[1];

    // Read gyroscope data
    mpu6050_read(REG_GYRO_XOUT_H, buffer, 6);
    for (int i = 0; i < 3; i++)
        gyro[i] = (buffer[i * 2] << 8) | buffer[i * 2 + 1];
}

void mpu6050_read_scaled(float accel[3], float gyro[3], float *temp_c)
{
    int16_t accel_raw[3], gyro_raw[3], temp_raw;
    mpu6050_read_raw(accel_raw, gyro_raw, &temp_raw);

    // Convert raw values to meaningful units
    for (int i = 0; i < 3; i++)
    {
        accel[i] = accel_raw[i] / 16384.0f; // g units
        gyro[i] = gyro_raw[i] / 131.0f;     // deg/s
    }
    *temp_c = (temp_raw / 340.0f) + 36.53f; // °C
}

void mpu6050_compute_angles(float *pitch, float *roll, float *yaw, float dt)
{
    float accel[3], gyro[3], temp;
    mpu6050_read_scaled(accel, gyro, &temp);

    // Compute pitch and roll from accelerometer (in degrees)
    float accel_pitch = atan2(accel[1], sqrt(accel[0] * accel[0] + accel[2] * accel[2])) * 180.0f / M_PI;
    float accel_roll = atan2(-accel[0], accel[2]) * 180.0f / M_PI;

    static float pitch_est = 0, roll_est = 0, yaw_est = 0; // persistent filter state

    // Complementary filter to combine gyro and accel
    const float alpha = 0.98f;

    pitch_est = alpha * (pitch_est + gyro[0] * dt) + (1 - alpha) * accel_pitch;
    roll_est = alpha * (roll_est + gyro[1] * dt) + (1 - alpha) * accel_roll;
    yaw_est += gyro[2] * dt; // integrate gyro Z for yaw (will drift)

    *pitch = pitch_est;
    *roll = roll_est;
    *yaw = yaw_est;
}

#endif // i2c_default
