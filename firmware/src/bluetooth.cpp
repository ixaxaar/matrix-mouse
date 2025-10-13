#include "bluetooth.h"
#include "sensor.h"
#include <M5Atom.h>

void initBluetooth() {
    Serial.println("Initializing Bluetooth...");
}

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
        Serial.printf("Button: %d\n", buttonState);
    }
}