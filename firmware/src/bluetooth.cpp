#include "bluetooth.h"
#include "sensor.h"
#include <M5Atom.h>

void initBluetooth() {
    Serial.println("🔵 Bluetooth stack initialized!");
    Serial.println("📶 BLE ready for peripheral mode");
}

/**
 * Send sensor data packet over BLE
 * buttonState: 0 = no click, 1 = left click, 2 = right click
 */
void sendSensorData(uint8_t buttonState) {
    if (!deviceConnected || !pCharacteristic) return;

    SensorPacket packet;

    // Get sensor readings as floats
    float accel_x_f, accel_y_f, accel_z_f;
    float gyro_x_f, gyro_y_f, gyro_z_f;
    getSensorData(&accel_x_f, &accel_y_f, &accel_z_f,
                  &gyro_x_f, &gyro_y_f, &gyro_z_f);

    // Convert to scaled integers to fit in 20 bytes
    packet.accel_x = (int16_t)(accel_x_f * 100.0f);
    packet.accel_y = (int16_t)(accel_y_f * 100.0f);
    packet.accel_z = (int16_t)(accel_z_f * 100.0f);
    packet.gyro_x = (int16_t)(gyro_x_f * 10.0f);
    packet.gyro_y = (int16_t)(gyro_y_f * 10.0f);
    packet.gyro_z = (int16_t)(gyro_z_f * 10.0f);

    packet.button_state = buttonState;
    packet.padding = 0;
    packet.timestamp = (uint16_t)(millis() & 0xFFFF);

    // Send via BLE
    pCharacteristic->setValue((uint8_t*)&packet, sizeof(packet));
    pCharacteristic->notify();

    if (buttonState > 0) {
        Serial.printf("📤 Sending button press data: %s\\n",
                     buttonState == 1 ? "BUTTON PRESS" : "LONG PRESS");
    }

    // Periodically show sensor data (every 100 packets ~2 seconds)
    static int packetCount = 0;
    if (buttonState == 0 && ++packetCount % 100 == 0) {
        Serial.printf("📊 Sensor data - Accel: %.2f,%.2f,%.2f | Gyro: %.2f,%.2f,%.2f \n",
                     packet.accel_x / 100.0f, packet.accel_y / 100.0f, packet.accel_z / 100.0f,
                     packet.gyro_x / 10.0f, packet.gyro_y / 10.0f, packet.gyro_z / 10.0f);
    }
}