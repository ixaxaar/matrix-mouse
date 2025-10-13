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
      Serial.println("🟢 BLE CLIENT CONNECTED!");
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      M5.dis.fillpix(0xff0000); // Red when disconnected
      Serial.println("🔴 BLE CLIENT DISCONNECTED!");
    }
};

void setup() {
  M5.begin(true, false, true);
  Serial.begin(115200);

  // Initialize LED matrix - red initially
  M5.dis.fillpix(0xff0000);

  Serial.println("\n🚀 M5 Atom Matrix Mouse Controller Starting...");
  Serial.println("📱 Device: M5 Stack Atom Matrix");
  Serial.println("🎯 Mode: Bluetooth Mouse Controller");

  // Initialize IMU
  Serial.println("⚡ Initializing IMU sensor...");
  initSensor();
  M5.dis.fillpix(0xffff00); // Yellow during init
  delay(100);

  // Initialize Bluetooth
  Serial.println("🔵 Initializing Bluetooth...");
  initBluetooth();
  M5.dis.fillpix(0xff8000); // Orange during BLE init
  delay(100);

  // Create BLE Device
  Serial.println("📡 Creating BLE device: M5-Mouse-Controller");
  BLEDevice::init("M5-Mouse-Controller");

  // Create BLE Server
  Serial.println("🔧 Setting up BLE server...");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create BLE Service
  Serial.println("🔑 Creating BLE service...");
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create BLE Characteristic
  Serial.println("📊 Setting up sensor data characteristic...");
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_WRITE |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );

  pCharacteristic->addDescriptor(new BLE2902());

  // Start the service
  Serial.println("▶️  Starting BLE service...");
  pService->start();

  // Start advertising
  Serial.println("📢 Starting BLE advertising...");
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);
  BLEDevice::startAdvertising();

  M5.dis.fillpix(0xff0000); // Red = ready/advertising
  Serial.println("✅ Setup complete! Ready for connections.");
  Serial.println("🔴 LED RED = Advertising/Disconnected");
  Serial.println("🟢 LED GREEN = Connected");
  Serial.println("🔘 Button: Short press = Left click, Long press = Right click\n");
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
    Serial.println("🔘 Button pressed - measuring duration...");
    M5.dis.fillpix(0x0000ff); // Blue flash on press
    delay(50);
    M5.dis.fillpix(deviceConnected ? 0x00ff00 : 0xff0000);
  }

  if (M5.Btn.isPressed() && buttonPressed && !longPressHandled) {
    if (millis() - buttonPressStart > 500) { // 500ms for long press
      Serial.println("🖱️  RIGHT CLICK detected (long press)");
      sendSensorData(2); // Right click
      longPressHandled = true;
      M5.dis.fillpix(0xff00ff); // Magenta flash for right click
      delay(100);
      M5.dis.fillpix(deviceConnected ? 0x00ff00 : 0xff0000);
    }
  }

  if (!M5.Btn.isPressed() && buttonPressed) {
    if (!longPressHandled) {
      Serial.println("🖱️  LEFT CLICK detected (short press)");
      sendSensorData(1); // Left click (short press)
      M5.dis.fillpix(0x00ffff); // Cyan flash for left click
      delay(100);
      M5.dis.fillpix(deviceConnected ? 0x00ff00 : 0xff0000);
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
    Serial.println("📢 Restarting BLE advertising after disconnect...");
    oldDeviceConnected = deviceConnected;
  }

  if (deviceConnected && !oldDeviceConnected) {
    Serial.println("🎉 Connection established! Mouse control active.");
    oldDeviceConnected = deviceConnected;
  }

  delay(20); // 50Hz update rate
}