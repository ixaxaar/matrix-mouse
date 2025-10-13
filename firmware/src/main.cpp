#include <M5Atom.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "bluetooth.h"
#include "sensor.h"

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      M5.dis.fillpix(0x00ff00); // Green when connected
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      M5.dis.fillpix(0xff0000); // Red when disconnected
    }
};

void setup() {
  M5.begin(true, false, true);
  Serial.begin(115200);

  // Initialize LED matrix - red initially
  M5.dis.fillpix(0xff0000);

  Serial.println("M5 Atom Matrix Mouse Controller Starting...");

  // Initialize IMU
  initSensor();

  // Initialize Bluetooth
  initBluetooth();

  // Create BLE Device
  BLEDevice::init("M5-Mouse-Controller");

  // Create BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_WRITE |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );

  pCharacteristic->addDescriptor(new BLE2902());

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);
  BLEDevice::startAdvertising();

  Serial.println("Waiting for client connection to notify...");
}

void loop() {
  M5.update();

  // Handle button press
  static unsigned long buttonPressStart = 0;
  static bool buttonPressed = false;
  static bool longPressHandled = false;

  if (M5.Btn.isPressed() && !buttonPressed) {
    buttonPressed = true;
    buttonPressStart = millis();
    longPressHandled = false;
  }

  if (M5.Btn.isPressed() && buttonPressed && !longPressHandled) {
    if (millis() - buttonPressStart > 500) { // 500ms for long press
      sendSensorData(2); // Right click
      longPressHandled = true;
    }
  }

  if (!M5.Btn.isPressed() && buttonPressed) {
    if (!longPressHandled) {
      sendSensorData(1); // Left click (short press)
    }
    buttonPressed = false;
    longPressHandled = false;
  }

  // Send sensor data continuously when connected
  if (deviceConnected) {
    sendSensorData(0); // Normal sensor data
  }

  // Handle disconnection
  if (!deviceConnected && oldDeviceConnected) {
    delay(500);
    pServer->startAdvertising();
    Serial.println("Start advertising");
    oldDeviceConnected = deviceConnected;
  }

  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }

  delay(20); // 50Hz update rate
}