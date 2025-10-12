# M5 Stack Atom Matrix Controller

A wireless mouse controller using M5 Stack Atom Matrix that communicates with Linux over WiFi/Bluetooth.

## Features

- Gesture-based mouse control using IMU
- WiFi and Bluetooth connectivity options
- Linux input driver for seamless integration
- LED matrix status indication
- Configurable sensitivity and button mapping

## Quick Start

See [CLAUDE.md](CLAUDE.md) for detailed specifications and implementation plan.

## Components

- `firmware/` - M5 Atom Matrix code (PlatformIO)
- `driver/` - Linux daemon for input handling
- `config/` - Configuration files

## Requirements

- M5 Stack Atom Matrix
- Linux system with WiFi/Bluetooth
- PlatformIO for firmware development