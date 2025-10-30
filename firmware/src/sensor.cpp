#include "sensor.h"
#include <M5Atom.h>

void initSensor() {
    Serial.println("🎯 IMU sensor initialized successfully!");
    Serial.println("📊 Accelerometer & Gyroscope ready for data");
    // M5.IMU is initialized in M5.begin()
}

/**
 * Get accelerometer and gyroscope data
 */
void getSensorData(float* accel_x, float* accel_y, float* accel_z,
                   float* gyro_x, float* gyro_y, float* gyro_z) {

    // Read accelerometer data
    M5.IMU.getAccelData(accel_x, accel_y, accel_z);

    // Read gyroscope data
    M5.IMU.getGyroData(gyro_x, gyro_y, gyro_z);
}