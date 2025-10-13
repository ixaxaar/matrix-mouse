#define _GNU_SOURCE
#include "bluetooth.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <fcntl.h>
#include <math.h>

static char device_mac[18] = {0};
static int notification_fd = -1;

int init_bluetooth() {
    syslog(LOG_INFO, "Initializing Bluetooth using modern BlueZ 5");

    // Check if bluetoothctl is available
    if (system("which bluetoothctl > /dev/null 2>&1") != 0) {
        syslog(LOG_ERR, "bluetoothctl not found. BlueZ not properly installed");
        return -1;
    }

    // Power on bluetooth and make discoverable
    system("bluetoothctl power on > /dev/null 2>&1");
    system("bluetoothctl agent NoInputNoOutput > /dev/null 2>&1");
    system("bluetoothctl default-agent > /dev/null 2>&1");
    sleep(2);

    syslog(LOG_INFO, "Modern BlueZ initialized");
    return 0;
}

int scan_for_device(BLEConnection* conn) {
    if (!conn) return -1;

    syslog(LOG_INFO, "Scanning for M5-Mouse-Controller...");

    // Start scanning
    system("bluetoothctl scan on > /dev/null 2>&1 &");
    sleep(5); // Scan for 5 seconds

    // Look for our device
    FILE* fp = popen("bluetoothctl devices", "r");
    if (!fp) {
        syslog(LOG_ERR, "Failed to list devices");
        return -1;
    }

    char line[256];
    bool found = false;

    while (fgets(line, sizeof(line), fp)) {
        // Parse line like: "Device 64:B7:08:80:E2:E8 M5-Mouse-Controller"
        if (strstr(line, "M5") || strstr(line, "Mouse")) {
            char* mac_start = strstr(line, "Device ");
            if (mac_start) {
                mac_start += 7; // Skip "Device "
                char* mac_end = strchr(mac_start, ' ');
                if (mac_end && (mac_end - mac_start) == 17) {
                    strncpy(device_mac, mac_start, 17);
                    device_mac[17] = '\0';
                    str2ba(device_mac, &conn->device_addr);
                    strcpy(conn->device_name, "M5-Mouse-Controller");
                    found = true;
                    break;
                }
            }
        }
    }
    pclose(fp);

    // Stop scanning
    system("bluetoothctl scan off > /dev/null 2>&1");

    if (found) {
        syslog(LOG_INFO, "Found M5 device at MAC: %s", device_mac);
        return 0;
    } else {
        syslog(LOG_WARNING, "M5-Mouse-Controller not found in scan");
        return -1;
    }
}

int connect_to_device(BLEConnection* conn) {
    if (!conn || strlen(device_mac) == 0) {
        syslog(LOG_ERR, "No device MAC address available");
        return -1;
    }

    syslog(LOG_INFO, "Connecting to device %s", device_mac);

    // Try to connect using bluetoothctl
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "bluetoothctl connect %s", device_mac);

    int result = system(cmd);
    if (result != 0) {
        syslog(LOG_ERR, "Failed to connect via bluetoothctl");
        return -1;
    }

    sleep(3); // Give connection time to establish

    // Check if we're actually connected
    snprintf(cmd, sizeof(cmd), "bluetoothctl info %s | grep -q 'Connected: yes'", device_mac);
    if (system(cmd) != 0) {
        syslog(LOG_ERR, "Device connection verification failed");
        return -1;
    }

    // Create a simple pipe for simulating data reception
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        syslog(LOG_ERR, "Failed to create notification pipe");
        return -1;
    }

    notification_fd = pipefd[0];

    // Make read end non-blocking
    int flags = fcntl(notification_fd, F_GETFL);
    fcntl(notification_fd, F_SETFL, flags | O_NONBLOCK);

    conn->connected = true;
    conn->socket = notification_fd;

    syslog(LOG_INFO, "Connected to M5 device successfully");
    return 0;
}

int read_sensor_data(BLEConnection* conn, SensorPacket* packet) {
    if (!conn || !packet || !conn->connected) {
        return -1;
    }

    // This is a simplified implementation that generates test data
    // In a real implementation, you would:
    // 1. Use D-Bus to communicate with BlueZ
    // 2. Subscribe to GATT characteristic notifications
    // 3. Parse incoming BLE notification data
    // 4. Convert to SensorPacket format

    static int sim_counter = 0;
    static float accel_sim = 0.0f;
    sim_counter++;

    // Generate realistic simulated sensor data
    if (sim_counter % 5 == 0) { // Every 5th call (~10Hz)
        accel_sim += 0.1f;

        packet->accel_x = 0.5f * sin(accel_sim);
        packet->accel_y = 0.3f * cos(accel_sim * 0.7f);
        packet->accel_z = 1.0f + 0.2f * sin(accel_sim * 1.3f);
        packet->gyro_x = 0.1f * cos(accel_sim * 2.0f);
        packet->gyro_y = 0.1f * sin(accel_sim * 1.5f);
        packet->gyro_z = 0.05f * cos(accel_sim * 3.0f);
        packet->button_state = 0;
        packet->timestamp = sim_counter;

        // Simulate occasional button presses
        if (sim_counter % 200 == 0) {
            packet->button_state = 1; // Left click
        } else if (sim_counter % 300 == 0) {
            packet->button_state = 2; // Right click
        }

        return 1; // Data received
    }

    return 0; // No data this time
}

void disconnect_device(BLEConnection* conn) {
    if (!conn) return;

    if (strlen(device_mac) > 0) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "bluetoothctl disconnect %s > /dev/null 2>&1", device_mac);
        system(cmd);
        syslog(LOG_INFO, "Disconnected from device %s", device_mac);
    }

    if (notification_fd >= 0) {
        close(notification_fd);
        notification_fd = -1;
    }

    conn->connected = false;
    conn->socket = -1;
}

void cleanup_bluetooth() {
    if (notification_fd >= 0) {
        close(notification_fd);
        notification_fd = -1;
    }

    // Stop any ongoing scans
    system("bluetoothctl scan off > /dev/null 2>&1");

    syslog(LOG_INFO, "Modern Bluetooth cleanup completed");
}