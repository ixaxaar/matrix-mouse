#include "bluetooth.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <sys/socket.h>

static int hci_dev_id = -1;

int init_bluetooth() {
    hci_dev_id = hci_get_route(NULL);
    if (hci_dev_id < 0) {
        syslog(LOG_ERR, "No Bluetooth adapter found");
        return -1;
    }
    
    syslog(LOG_INFO, "Using Bluetooth adapter hci%d", hci_dev_id);
    return 0;
}

int scan_for_device(BLEConnection* conn) {
    if (!conn) return -1;
    
    int dev_id = hci_dev_id;
    int sock = hci_open_dev(dev_id);
    if (sock < 0) {
        syslog(LOG_ERR, "Failed to open HCI device: %s", strerror(errno));
        return -1;
    }
    
    // Simple scan implementation - in a real implementation,
    // you would use hci_le_set_scan_parameters and hci_le_set_scan_enable
    // For now, we'll simulate finding the device
    
    // This is a simplified version - real BLE scanning requires more complex HCI commands
    syslog(LOG_INFO, "Scanning for BLE devices...");
    
    // Simulate device discovery
    str2ba("00:00:00:00:00:00", &conn->device_addr); // Placeholder
    strcpy(conn->device_name, "M5-Mouse-Controller");
    
    close(sock);
    return 0;
}

int connect_to_device(BLEConnection* conn) {
    if (!conn) return -1;
    
    // Create L2CAP socket for BLE connection
    int sock = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
    if (sock < 0) {
        syslog(LOG_ERR, "Failed to create L2CAP socket: %s", strerror(errno));
        return -1;
    }
    
    struct sockaddr_l2 addr = {0};
    addr.l2_family = AF_BLUETOOTH;
    addr.l2_psm = htobs(0x1001); // ATT PSM
    bacpy(&addr.l2_bdaddr, &conn->device_addr);
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        syslog(LOG_ERR, "Failed to connect to device: %s", strerror(errno));
        close(sock);
        return -1;
    }
    
    conn->socket = sock;
    conn->connected = true;
    
    return 0;
}

int read_sensor_data(BLEConnection* conn, SensorPacket* packet) {
    if (!conn || !packet || !conn->connected) return -1;
    
    // Read data from BLE characteristic
    // This is a simplified implementation - real GATT operations require
    // proper ATT protocol handling
    
    ssize_t bytes_read = recv(conn->socket, packet, sizeof(SensorPacket), MSG_DONTWAIT);
    
    if (bytes_read < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0; // No data available
        }
        syslog(LOG_ERR, "Failed to read from device: %s", strerror(errno));
        conn->connected = false;
        return -1;
    }
    
    if (bytes_read != sizeof(SensorPacket)) {
        syslog(LOG_WARNING, "Received incomplete packet");
        return 0;
    }
    
    return 1; // Data received
}

void disconnect_device(BLEConnection* conn) {
    if (!conn) return;
    
    if (conn->connected && conn->socket >= 0) {
        close(conn->socket);
        conn->socket = -1;
        conn->connected = false;
    }
}

void cleanup_bluetooth() {
    // Cleanup any global Bluetooth resources
    syslog(LOG_INFO, "Bluetooth cleanup completed");
}