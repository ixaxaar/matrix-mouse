#ifndef SENSOR_H
#define SENSOR_H

/**
 * @brief Initializes the sensor.
 *
 */
void initSensor();

/**
 * @brief Gets the sensor data from the accelerometer and gyroscope.
 *
 * @param accel_x Pointer to a float to store the X-axis acceleration.
 * @param accel_y Pointer to a float to store the Y-axis acceleration.
 * @param accel_z Pointer to a float to store the Z-axis acceleration.
 * @param gyro_x Pointer to a float to store the X-axis gyroscope data.
 * @param gyro_y Pointer to a float to store the Y-axis gyroscope data.
 * @param gyro_z Pointer to a float to store the Z-axis gyroscope data.
 */
void getSensorData(float* accel_x, float* accel_y, float* accel_z,
                   float* gyro_x, float* gyro_y, float* gyro_z);

#endif
