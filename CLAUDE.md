# M5 Stack Atom Matrix Controller System

## Project Overview

Create a wireless mouse controller using M5 Stack Atom Matrix that communicates with a Linux laptop over Bluetooth to provide gesture-based mouse control.

## System Architecture

### Hardware Components

- **M5 Stack Atom Matrix**: ESP32-based device with 5x5 LED matrix and built-in IMU (MPU6886)
- **Linux Laptop**: Target system with Bluetooth capability

### Communication

- **Bluetooth**: BLE with custom service/characteristic for sensor data transmission

## Technical Specifications

### M5 Atom Matrix Firmware

- **Platform**: Arduino/ESP-IDF
- **Sensors**: IMU (accelerometer/gyroscope) for gesture detection
- **Input**: Physical button for click actions
- **Output**: LED matrix for status indication
- **Communication**: BLE peripheral

#### Sensor Data and Controls

- **Tilt Control**: Accelerometer X/Y axes for cursor movement
- **Scroll Control**: Accelerometer Z-axis for vertical scrolling (with noise filtering)
- **Left Click**: Physical button short press
- **Right Click**: Physical button long press
- **Sensor Reporting**: All IMU sensor values transmitted for configurable actions on Linux side
- **Gesture Sensitivity**: Configurable dead zone and scaling

#### Bluetooth Protocol

- **Transport**: BLE characteristics
- **Data Format**:
  ```
  struct SensorPacket {
    float accel_x, accel_y, accel_z;    // Accelerometer values
    float gyro_x, gyro_y, gyro_z;      // Gyroscope values
    uint8_t button_state;               // 0=none, 1=short_press, 2=long_press
    uint32_t timestamp;
  };
  ```

#### BLE Service Structure

- **Service UUID**: `12345678-1234-1234-1234-123456789abc`
- **Characteristic UUID**: `87654321-4321-4321-4321-cba987654321`
- **Device Name**: `M5-Mouse-Controller`
- **Characteristics**:
  - Sensor Data: Complete IMU readings and button state (READ, WRITE, NOTIFY properties)
  - Supports BLE notifications for real-time sensor streaming

#### LED Status Indicators

The 5x5 LED matrix displays connection and activity status:

- **Red**: Advertising/Disconnected (ready for connection)
- **Orange**: BLE initialization in progress
- **Yellow**: IMU sensor initialization
- **Green**: Connected to BLE client
- **Cyan**: Button press detected (brief flash)
- **Magenta**: Long button press detected (brief flash)

#### Firmware Implementation Details

- **Update Rate**: 50Hz (20ms delay between sensor packets)
- **BLE Auto-Reconnect**: Automatically restarts advertising when client disconnects
- **Sensor Data Logging**: Periodic sensor readings printed every 100 packets (~2 seconds)
- **Connection Callbacks**: Visual and serial feedback on connection/disconnection events

### Linux Driver/Daemon

#### Implementation Options

1. **uinput Driver**: Create virtual mouse device using /dev/uinput
2. **evdev Integration**: Direct event injection
3. **User-space Daemon**: Background service handling communication

#### Core Components

- **BLE Client**: Bluetooth scanner and connection management
- **Input Injection**: Convert sensor data to Linux input events
- **Device Management**: Handle BLE connection/disconnection
- **Configuration**: Configurable action mapping, sensitivity, noise filtering

#### Communication Flow

```
M5 Atom Matrix → BLE → Linux Daemon → uinput → X11/Wayland
```

## Implementation Plan

### Phase 1: Basic Bluetooth Implementation

1. M5 Atom Matrix firmware with complete IMU reading
2. BLE peripheral setup and sensor data transmission
3. Linux daemon with BLE client and uinput integration
4. Configurable action mapping (cursor, scroll, clicks)

### Phase 2: Enhanced Features

1. Advanced sensor filtering and noise reduction
2. LED feedback system for connection status
3. Runtime configuration interface
4. Calibration and sensitivity tuning

### Phase 3: Advanced Features

1. Custom gesture recognition
2. Multiple device support
3. Power management optimization
4. Advanced configuration profiles

## File Structure

```
m5-matrix/
├── firmware/           # M5 Atom Matrix code
│   ├── src/
│   │   ├── main.cpp       # Main application loop and BLE server setup
│   │   ├── sensor.cpp     # IMU sensor reading functions
│   │   ├── sensor.h       # Sensor interface definitions
│   │   ├── bluetooth.cpp  # BLE data transmission
│   │   └── bluetooth.h    # BLE protocol definitions and SensorPacket structure
│   └── platformio.ini
├── driver/            # Linux daemon
│   ├── src/
│   │   ├── main.c
│   │   ├── bluetooth.c
│   │   └── uinput.c
│   └── Makefile
├── config/            # Configuration files
└── docs/              # Documentation
```

## Configuration

- **BLE Pairing**: Device discovery and pairing management
- **Action Mapping**: Configurable sensor-to-action assignments
- **Sensitivity Settings**: Movement scaling and dead zones
- **Noise Filtering**: Z-axis scroll filtering parameters
- **Connection Timeout**: Automatic BLE reconnection handling

## Security Considerations

- BLE local pairing only (no internet communication)
- BLE authentication and encryption
- Input validation on Linux daemon
- Rate limiting to prevent input flooding

## Development Tools

- **M5 Stack**: PlatformIO or Arduino IDE
- **Linux**: GCC, make, libbluetooth-dev, libudev-dev
- **Testing**: evtest, xinput for input verification

## Performance Requirements

- **Latency**: <50ms end-to-end
- **Battery Life**: >8 hours continuous use
- **Range**: 10m typical BLE range
- **Reliability**: Auto-reconnection on BLE drops
