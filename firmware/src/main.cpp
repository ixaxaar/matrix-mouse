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

  // Handle button press (simple click only)
  static bool lastButtonState = false;
  bool currentButtonState = M5.Btn.isPressed();

  if (currentButtonState && !lastButtonState) {
    // Button pressed
    Serial.println("🖱️ LEFT CLICK pressed");
    sendSensorData(1); // Left click
    M5.dis.fillpix(0x00ffff); // Cyan flash for click
    delay(50);
    M5.dis.fillpix(deviceConnected ? 0x00ff00 : 0xff0000);
  } else if (!currentButtonState && lastButtonState) {
    // Button released  
    Serial.println("🖱️ LEFT CLICK released");
    sendSensorData(0); // Release
  }

  lastButtonState = currentButtonState;

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