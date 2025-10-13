# M5 Stack Atom Matrix Mouse Controller

A wireless mouse controller using M5 Stack Atom Matrix that communicates with Linux over Bluetooth for gesture-based mouse control.

## Features

- **Gesture Control**: Tilt-based cursor movement using accelerometer
- **Click Control**: Short press for left click, long press for right click  
- **Scroll Control**: Z-axis accelerometer for vertical scrolling with noise filtering
- **Bluetooth Communication**: BLE connection with automatic reconnection
- **Configurable Actions**: All sensor data transmitted for flexible action mapping
- **Linux Integration**: Virtual mouse device via uinput
- **LED Feedback**: Visual connection status indication

## Hardware Requirements

- M5 Stack Atom Matrix (ESP32-based with IMU)
- Linux system with Bluetooth capability
- Required Linux packages: `libbluetooth-dev`, `libudev-dev`

## Quick Start

### 1. Build and Flash Firmware

```bash
cd firmware
# Install PlatformIO if not already installed
pip install platformio

# Build and upload to M5 Atom Matrix
pio run --target upload
```

### 2. Build Linux Driver

```bash
cd driver
make

# Install system-wide (optional)
sudo make install
```

### 3. Run the Driver

```bash
# Run in foreground with verbose output
sudo ./m5-mouse-daemon -v

# Or install as system service
sudo make install
sudo systemctl enable m5-mouse.service
sudo systemctl start m5-mouse.service
```

## Configuration

Edit `/etc/m5-mouse.conf` or use a custom config file:

```ini
# Movement sensitivity (multiplier for accelerometer values)
movement_sensitivity = 2.0

# Scroll sensitivity (multiplier for Z-axis scrolling)  
scroll_sensitivity = 1.0

# Dead zone threshold (ignore small movements below this value)
dead_zone = 0.1

# Scroll threshold (minimum Z-axis value to trigger scrolling)
scroll_threshold = 0.3

# Invert axis directions
invert_x = false
invert_y = false
invert_scroll = false

# Number of samples for scroll filtering (1-10)
scroll_filter_samples = 5
```

## Usage

1. **Power on M5 Atom Matrix** - LED will show red (disconnected)
2. **Start Linux daemon** - Device will scan for M5 controller
3. **Connection established** - LED turns green
4. **Control mouse**:
   - Tilt device to move cursor
   - Short button press for left click
   - Long button press (>500ms) for right click
   - Tilt forward/back for vertical scrolling

## Architecture

```
M5 Atom Matrix → BLE → Linux Daemon → uinput → X11/Wayland
```

### Firmware Components

- `main.cpp`: Main loop and BLE setup
- `bluetooth.cpp/h`: BLE communication and data transmission
- `sensor.cpp/h`: IMU data reading and processing

### Driver Components  

- `main.c`: Main daemon with command-line interface
- `bluetooth.c/h`: BLE client and device management
- `uinput.c/h`: Virtual mouse device and input event generation
- `config.c`: Configuration file parsing

## Development

### Firmware Development

```bash
cd firmware
pio run              # Build
pio run -t upload    # Upload to device
pio device monitor   # Serial monitor
```

### Driver Development

```bash
cd driver
make clean && make   # Build
sudo ./m5-mouse-daemon -v -c ../config/m5-mouse.conf  # Test
```

### Testing

```bash
# Monitor input events
sudo evtest

# Test virtual mouse device
xinput list
xinput test "M5 Atom Matrix Mouse"
```

## Troubleshooting

### Common Issues

1. **Permission denied accessing /dev/uinput**
   ```bash
   sudo chmod 666 /dev/uinput
   # Or add user to input group
   sudo usermod -a -G input $USER
   ```

2. **Bluetooth device not found**
   ```bash
   sudo systemctl status bluetooth
   sudo hciconfig hci0 up
   ```

3. **Compilation errors**
   ```bash
   # Install missing dependencies
   sudo apt-get install libbluetooth-dev libudev-dev build-essential
   ```

### Debug Mode

Run with verbose output to see sensor data:

```bash
sudo ./m5-mouse-daemon -v -c config/m5-mouse.conf
```

## Performance

- **Latency**: <50ms end-to-end
- **Update Rate**: 50Hz sensor sampling
- **Battery Life**: >8 hours continuous use
- **Range**: ~10m typical BLE range

## License

Open source - see individual file headers for details.
