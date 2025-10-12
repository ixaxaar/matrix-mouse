# M5 Stack Atom Matrix Controller System

## Project Overview
Create a wireless mouse controller using M5 Stack Atom Matrix that communicates with a Linux laptop over WiFi or Bluetooth to provide gesture-based mouse control.

## System Architecture

### Hardware Components
- **M5 Stack Atom Matrix**: ESP32-based device with 5x5 LED matrix and built-in IMU (MPU6886)
- **Linux Laptop**: Target system with WiFi/Bluetooth capability

### Communication Options
1. **WiFi (Primary)**: UDP socket communication over local network
2. **Bluetooth (Alternative)**: BLE with custom service/characteristic

## Technical Specifications

### M5 Atom Matrix Firmware
- **Platform**: Arduino/ESP-IDF
- **Sensors**: IMU (accelerometer/gyroscope) for gesture detection
- **Input**: Physical button for click actions
- **Output**: LED matrix for status indication
- **Communication**: WiFi client or BLE peripheral

#### Gesture Detection
- **Tilt Control**: Use accelerometer X/Y axes for cursor movement
- **Tap Detection**: Use accelerometer Z-axis threshold for left click
- **Button Press**: Physical button for right click
- **Gesture Sensitivity**: Configurable dead zone and scaling

#### Network Protocol (WiFi)
- **Transport**: UDP packets
- **Port**: 8080 (configurable)
- **Packet Format**:
  ```
  struct MousePacket {
    uint8_t type;      // 1=move, 2=click, 3=release
    int16_t dx;        // X movement delta
    int16_t dy;        // Y movement delta
    uint8_t button;    // 0=none, 1=left, 2=right
    uint32_t timestamp;
  };
  ```

#### Bluetooth Protocol (Alternative)
- **Service UUID**: Custom service for mouse control
- **Characteristics**:
  - Movement: X/Y delta values
  - Click: Button state changes
  - Status: Device status/battery

### Linux Driver/Daemon

#### Implementation Options
1. **uinput Driver**: Create virtual mouse device using /dev/uinput
2. **evdev Integration**: Direct event injection
3. **User-space Daemon**: Background service handling communication

#### Core Components
- **Network Listener**: UDP socket server or BLE scanner
- **Input Injection**: Convert packets to Linux input events
- **Device Management**: Handle connection/disconnection
- **Configuration**: Sensitivity, button mapping, calibration

#### Communication Flow
```
M5 Atom Matrix → WiFi/BLE → Linux Daemon → uinput → X11/Wayland
```

## Implementation Plan

### Phase 1: Basic WiFi Implementation
1. M5 Atom Matrix firmware with basic IMU reading
2. WiFi connectivity and UDP packet transmission
3. Linux daemon with UDP listener and uinput integration
4. Basic cursor movement and click detection

### Phase 2: Enhanced Features
1. Gesture recognition improvements
2. LED feedback system
3. Configuration interface
4. Calibration routine

### Phase 3: Bluetooth Option
1. BLE implementation on M5 Atom Matrix
2. Linux BLE client integration
3. Pairing and connection management
4. Fallback between WiFi and Bluetooth

## File Structure
```
m5-matrix/
├── firmware/           # M5 Atom Matrix code
│   ├── src/
│   │   ├── main.cpp
│   │   ├── gesture.cpp
│   │   └── network.cpp
│   └── platformio.ini
├── driver/            # Linux daemon
│   ├── src/
│   │   ├── main.c
│   │   ├── network.c
│   │   └── uinput.c
│   └── Makefile
├── config/            # Configuration files
└── docs/              # Documentation
```

## Configuration
- **WiFi Credentials**: Stored in firmware or configured via AP mode
- **Sensitivity Settings**: Mouse movement scaling factors  
- **Button Mapping**: Configurable button assignments
- **Connection Timeout**: Automatic reconnection handling

## Security Considerations
- Local network only (no internet communication)
- Optional authentication token
- Input validation on Linux daemon
- Limited packet rate to prevent abuse

## Development Tools
- **M5 Stack**: PlatformIO or Arduino IDE
- **Linux**: GCC, make, libudev-dev
- **Testing**: evtest, xinput for input verification

## Performance Requirements
- **Latency**: <50ms end-to-end
- **Battery Life**: >8 hours continuous use
- **Range**: 10m typical WiFi range
- **Reliability**: Auto-reconnection on network drops