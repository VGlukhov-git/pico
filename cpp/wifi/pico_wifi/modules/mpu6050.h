#pragma once
#include <stdint.h>

/**
 * @brief Reset and wake up the MPU6050.
 */
void mpu6050_reset();

/**
 * @brief Read raw sensor values from the MPU6050.
 * @param accel Array of 3 values for X, Y, Z accelerometer data
 * @param gyro  Array of 3 values for X, Y, Z gyroscope data
 * @param temp  Pointer to store raw temperature
 */
void mpu6050_read_raw(int16_t accel[3], int16_t gyro[3], int16_t *temp);

/**
 * @brief Read scaled (converted) values from the MPU6050.
 * @param accel Array of 3 accelerometer readings in g
 * @param gyro  Array of 3 gyroscope readings in °/s
 * @param temp_c Pointer to temperature in °C
 */
void mpu6050_read_scaled(float accel[3], float gyro[3], float *temp_c);
/**
 * @brief Compute pitch, roll, and yaw angles using sensor fusion
 * @param pitch Pointer to store pitch angle (degrees)
 * @param roll  Pointer to store roll angle (degrees)
 * @param yaw   Pointer to store yaw angle (degrees)
 * @param dt    Time step between samples (seconds)
 */
void mpu6050_compute_angles(float *pitch, float *roll, float *yaw, float dt);
