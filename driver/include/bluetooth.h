#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include <stdbool.h>
#include <stdint.h>
#include <dbus/dbus.h>
#include "common.h"

#define SERVICE_UUID "12345678-1234-1234-1234-123456789abc"
#define CHARACTERISTIC_UUID "87654321-4321-4321-4321-cba987654321"

typedef struct {
    char device_path[256];
    char device_name[128];
    char service_path[256];
    char char_path[256];
    bool connected;
    bool scanning;
    DBusConnection* dbus_conn;
    SensorPacket last_packet;
    bool packet_ready;
} BLEConnection;

// Function declarations
int init_bluetooth();
int scan_for_device(BLEConnection* conn);
int connect_to_device(BLEConnection* conn);
int read_sensor_data(BLEConnection* conn, SensorPacket* packet);
void disconnect_device(BLEConnection* conn);
void cleanup_bluetooth();

#endif
