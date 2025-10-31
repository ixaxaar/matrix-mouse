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

- **Transport**: BLE characteristics with optimized packet size
- **MTU Handling**: 512-byte MTU requested, packets sized to fit in default 20-byte payload
- **Data Format** (16 bytes total, fits in default BLE MTU):
  ```c
  struct SensorPacket {
    int16_t accel_x, accel_y, accel_z; // Acceleration * 100 (e.g., 1.5g = 150)
    int16_t gyro_x, gyro_y, gyro_z;    // Gyroscope * 10 (e.g., 5.5 deg/s = 55)
    uint8_t button_state;               // 0=none, 1=press, 2=long_press
    uint8_t padding;                    // Alignment padding
    uint16_t timestamp;                 // Millisecond counter (wraps every 65s)
  } __attribute__((packed));
  // Total: 16 bytes (fits in 20-byte BLE MTU limit)
  ```
- **Data Encoding**: Integer scaling used to reduce packet size while maintaining precision

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

#### Implementation

**Chosen Approach**: User-space daemon with uinput virtual device
- Creates virtual mouse device via `/dev/uinput`
- D-Bus integration for BlueZ communication
- Non-blocking event loop for responsive signal handling

#### Core Components

1. **BLE Client** (`bluetooth.c`):
   - D-Bus-based BlueZ integration
   - Device scanning and connection management
   - Notification handler for real-time sensor data
   - Automatic reconnection on disconnect
   - Connection state monitoring

2. **3D Orientation Tracking** (`uinput.c`):
   - **Complementary Filter**: Fuses gyroscope (98%) and accelerometer (2%) data
   - Tracks roll, pitch, yaw in 3D space
   - Gyroscope integration for fast response
   - Accelerometer correction to prevent drift

3. **Orientation-Independent Cursor Control**:
   - **Yaw Compensation**: Rotates movement vector by device yaw angle
   - Movement in world-space coordinates (not device-local)
   - Example: Moving device "forward" moves cursor up, even if device rotates 180°
   - Maintains consistent movement direction regardless of device rotation

4. **Input Injection**:
   - Sub-pixel precision cursor movement (fractional accumulation)
   - Button event handling (left/right click)
   - Configurable sensitivity and dead zones
   - Movement clamping to prevent cursor jumping

5. **Configuration**:
   - Movement sensitivity scaling
   - Dead zone filtering
   - Axis inversion support
   - YAML-based configuration loading

#### Communication Flow

```
M5 Atom Matrix → BLE Notify → BlueZ (D-Bus) → Daemon
                                                  ↓
                                    3D Orientation Tracking
                                                  ↓
                                    Yaw-Compensated 2D Delta
                                                  ↓
                                    uinput → X11/Wayland
```

## Implementation Status

### ✅ Completed (Phase 1)

1. **Firmware**:
   - IMU sensor reading (accelerometer + gyroscope)
   - BLE peripheral with optimized 16-byte packets
   - Button handling (press/release detection)
   - LED status indicators
   - Auto-reconnection

2. **Driver**:
   - D-Bus/BlueZ BLE client
   - 3D orientation tracking with sensor fusion
   - Yaw-compensated cursor control
   - uinput virtual mouse device
   - Non-blocking event loop with signal handling

3. **Features**:
   - Orientation-independent movement (handles device rotation)
   - Left/right mouse button support
   - Configurable sensitivity and dead zones
   - Sub-pixel precision movement

### 🚧 In Progress (Phase 2)

1. Fine-tuning orientation tracking parameters
2. Testing movement accuracy and responsiveness
3. Optimizing sensor fusion coefficients

### 📋 Planned (Phase 3)

1. Scroll wheel support using Z-axis tilt
2. Custom gesture recognition (double-tap, shake, etc.)
3. YAML configuration file support
4. Calibration interface
5. Multiple device support
6. Power management optimization

## File Structure

```
m5-matrix/
├── firmware/              # M5 Atom Matrix code
│   ├── src/
│   │   ├── main.cpp       # Main loop, BLE server, connection callbacks
│   │   ├── sensor.cpp     # IMU sensor reading (MPU6886)
│   │   ├── sensor.h       # Sensor interface definitions
│   │   ├── bluetooth.cpp  # BLE data transmission with int16 encoding
│   │   └── bluetooth.h    # BLE protocol, SensorPacket structure
│   └── platformio.ini
├── driver/                # Linux daemon
│   ├── include/
│   │   ├── bluetooth.h    # BLE connection management
│   │   ├── common.h       # SensorPacket, config structures
│   │   └── uinput.h       # Virtual device interface
│   ├── src/
│   │   ├── main.c         # Main loop, signal handling, device scanning
│   │   ├── bluetooth.c    # D-Bus/BlueZ integration, notification handler
│   │   ├── uinput.c       # 3D orientation tracking, cursor control
│   │   └── config.c       # YAML configuration loading
│   └── Makefile
├── config/                # Configuration files
└── CLAUDE.md             # This documentation
```

## Configuration

### Mouse Configuration (driver/src/main.c)

```c
MouseConfig config = {
    .movement_sensitivity = 2.0f,      // Multiplier for orientation → cursor movement
    .scroll_sensitivity = 1.0f,        // (Reserved for future scroll implementation)
    .dead_zone = 0.1f,                 // Ignore movements below 0.1 radians (~5.7°)
    .scroll_threshold = 0.3f,          // (Reserved)
    .invert_x = false,                 // Reverse X-axis movement
    .invert_y = false,                 // Reverse Y-axis movement
    .invert_scroll = false,            // (Reserved)
    .scroll_filter_samples = 5         // (Reserved)
};
```

### Runtime Parameters

- **BLE Scanning**: 5-second discovery window
- **Update Rate**: 50Hz (20ms between reads)
- **Orientation Filter**: 98% gyro / 2% accel (complementary filter)
- **Movement Clamping**: ±50 pixels per frame max
- **Signal Timeout**: 2-second forced exit on Ctrl-C

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

- **Latency**: <50ms end-to-end (target)
- **Update Rate**: 50Hz sensor sampling
- **Battery Life**: >8 hours continuous use (target)
- **Range**: 10m typical BLE range
- **Reliability**: Auto-reconnection on BLE drops
- **Orientation Accuracy**: ±2° typical with complementary filter

## Known Issues & Limitations

1. **Yaw Drift**: No magnetometer means yaw accumulates drift over time
   - Mitigation: Periodic recalibration or use pitch/roll only

2. **BLE MTU**: Limited to 20-byte packets on some systems
   - Solution: Implemented 16-byte compressed packet format

3. **First Frame Skip**: Orientation initialization requires one frame
   - Impact: Minimal, occurs only at startup

## Troubleshooting

### Mouse Not Moving
- Check orientation values in logs: `Orientation R:<roll>° P:<pitch>° Y:<yaw>°`
- Verify `movement_sensitivity` is not too low (default: 2.0)
- Ensure device is tilted beyond `dead_zone` threshold (default: 0.1 rad = ~5.7°)

### Cursor Drifting
- Reduce `movement_sensitivity`
- Increase `dead_zone` to filter small movements
- Check for gyroscope bias in logs

### Connection Drops
- Verify M5 device shows green LED when connected
- Check system Bluetooth logs: `journalctl -f | grep m5-mouse-daemon`
- Ensure BlueZ version supports BLE notifications

### Ctrl-C Not Working
- Wait 2 seconds for alarm timeout
- Force kill: `sudo killall -9 m5-mouse-daemon`
