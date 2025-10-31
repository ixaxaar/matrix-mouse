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

    // Get sensor readings
    getSensorData(&packet.accel_x, &packet.accel_y, &packet.accel_z,
                  &packet.gyro_x, &packet.gyro_y, &packet.gyro_z);

    packet.button_state = buttonState;
    packet.timestamp = millis();

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
                     packet.accel_x, packet.accel_y, packet.accel_z,
                     packet.gyro_x, packet.gyro_y, packet.gyro_z);
    }
}