.PHONY: all firmware driver clean install uninstall test help

# Default target
all: firmware driver

# Build firmware
firmware:
	@echo "Building M5 Atom Matrix firmware..."
	cd firmware && pio run

# Flash firmware to device
firmware-upload:
	@echo "Uploading firmware to M5 Atom Matrix..."
	cd firmware && pio run --target upload

# Monitor serial output from firmware
firmware-monitor:
	@echo "Starting serial monitor..."
	cd firmware && pio device monitor

# Build Linux driver
driver:
	@echo "Building Linux driver..."
	$(MAKE) -C driver

# Clean all build artifacts
clean:
	@echo "Cleaning build artifacts..."
	cd firmware && pio run --target clean
	$(MAKE) -C driver clean

# Install driver system-wide (requires sudo)
install: driver
	@echo "Installing M5 mouse daemon system-wide..."
	$(MAKE) -C driver install

# Uninstall driver (requires sudo)
uninstall:
	@echo "Uninstalling M5 mouse daemon..."
	$(MAKE) -C driver uninstall

# Run driver in foreground with verbose output
test: driver
	@echo "Running M5 mouse daemon in test mode..."
	@echo "Note: This requires sudo privileges"
	sudo ./driver/m5-mouse-daemon -v -c config/m5-mouse.conf

# Check dependencies
deps-check:
	@echo "Checking dependencies..."
	@which pio > /dev/null || echo "ERROR: PlatformIO not found. Install with: pip install platformio"
	@which gcc > /dev/null || echo "ERROR: GCC not found. Install with: sudo apt install build-essential"
	@pkg-config --exists bluez || echo "ERROR: Bluetooth development headers not found. Install with: sudo apt install libbluetooth-dev"
	@echo "Dependency check complete."

# Install development dependencies
deps-install:
	@echo "Installing development dependencies..."
	pip install platformio
	sudo apt update
	sudo apt install build-essential libbluetooth-dev libudev-dev

# Start/stop/status systemd service
service-start:
	sudo systemctl start m5-mouse.service

service-stop:
	sudo systemctl stop m5-mouse.service

service-status:
	sudo systemctl status m5-mouse.service

service-enable:
	sudo systemctl enable m5-mouse.service

service-disable:
	sudo systemctl disable m5-mouse.service

service-logs:
	sudo journalctl -u m5-mouse.service -f

# Development helpers
dev-firmware: firmware firmware-upload firmware-monitor

dev-driver: driver
	sudo ./driver/m5-mouse-daemon -v -c config/m5-mouse.conf

# Show help
help:
	@echo "M5 Stack Atom Matrix Mouse Controller - Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all              - Build firmware and driver (default)"
	@echo "  firmware         - Build M5 Atom Matrix firmware"
	@echo "  firmware-upload  - Flash firmware to device"
	@echo "  firmware-monitor - Monitor serial output"
	@echo "  driver           - Build Linux driver"
	@echo "  clean            - Clean all build artifacts"
	@echo "  install          - Install driver system-wide (requires sudo)"
	@echo "  uninstall        - Uninstall driver (requires sudo)"
	@echo "  test             - Run driver in test mode (requires sudo)"
	@echo ""
	@echo "Dependencies:"
	@echo "  deps-check       - Check for required dependencies"
	@echo "  deps-install     - Install development dependencies"
	@echo ""
	@echo "Service Management:"
	@echo "  service-start    - Start systemd service"
	@echo "  service-stop     - Stop systemd service"
	@echo "  service-status   - Show service status"
	@echo "  service-enable   - Enable service autostart"
	@echo "  service-disable  - Disable service autostart"
	@echo "  service-logs     - Follow service logs"
	@echo ""
	@echo "Development:"
	@echo "  dev-firmware     - Build, upload and monitor firmware"
	@echo "  dev-driver       - Build and run driver in verbose mode"
	@echo ""
	@echo "Quick Start:"
	@echo "  1. make deps-install   # Install dependencies"
	@echo "  2. make dev-firmware   # Build and flash firmware"
	@echo "  3. make dev-driver     # Test driver connection"
	@echo "  4. make install        # Install as system service"