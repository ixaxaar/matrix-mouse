#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SERVICE_UUID        "12345678-1234-1234-1234-123456789abc"
#define CHARACTERISTIC_UUID "87654321-4321-4321-4321-cba987654321"

/**
 * @brief Structure to hold sensor data for BLE transmission.
 * Limited to 20 bytes to fit in default BLE MTU (23 bytes - 3 bytes overhead = 20 bytes payload)
 */
struct SensorPacket {
    int16_t accel_x, accel_y, accel_z; ///< Acceleration * 100 (e.g., 1.5g = 150)
    int16_t gyro_x, gyro_y, gyro_z;    ///< Gyroscope * 10 (e.g., 5.5 deg/s = 55)
    uint8_t button_state;               ///< 0=none, 1=press, 2=long_press
    uint8_t padding;                    ///< Padding for alignment
    uint16_t timestamp;                 ///< Millisecond counter (wraps every 65 seconds)
} __attribute__((packed));
// Size: 6*2 + 1 + 1 + 2 = 16 bytes (well under 20 byte limit)

extern BLECharacteristic* pCharacteristic; ///< Pointer to the BLE characteristic used for sending data.
extern bool deviceConnected;               ///< Flag to indicate if a BLE client is connected.

/**
 * @brief Initializes the Bluetooth Low Energy (BLE) server, service, and characteristic.
 */
void initBluetooth();

/**
 * @brief Packages and sends sensor data over BLE.
 *
 * @param buttonState The current state of the button to be included in the packet.
 */
void sendSensorData(uint8_t buttonState);

#endif