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
 */
struct SensorPacket {
    float accel_x, accel_y, accel_z; ///< X, Y, and Z-axis acceleration data.
    float gyro_x, gyro_y, gyro_z;   ///< X, Y, and Z-axis gyroscope data.
    uint8_t button_state;  // 0=none, 1=short_press, 2=long_press ///< State of the button (0=none, 1=short press, 2=long press).
    uint32_t timestamp;             ///< Timestamp of the sensor reading.
} __attribute__((packed));

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