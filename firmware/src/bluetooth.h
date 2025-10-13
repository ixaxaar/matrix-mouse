#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SERVICE_UUID        "12345678-1234-1234-1234-123456789abc"
#define CHARACTERISTIC_UUID "87654321-4321-4321-4321-cba987654321"

struct SensorPacket {
    float accel_x, accel_y, accel_z;
    float gyro_x, gyro_y, gyro_z;
    uint8_t button_state;  // 0=none, 1=short_press, 2=long_press
    uint32_t timestamp;
} __attribute__((packed));

extern BLECharacteristic* pCharacteristic;
extern bool deviceConnected;

void initBluetooth();
void sendSensorData(uint8_t buttonState);

#endif