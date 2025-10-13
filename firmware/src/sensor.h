#ifndef SENSOR_H
#define SENSOR_H

void initSensor();
void getSensorData(float* accel_x, float* accel_y, float* accel_z,
                   float* gyro_x, float* gyro_y, float* gyro_z);

#endif