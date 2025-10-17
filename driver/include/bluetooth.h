#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include <stdbool.h>
#include <stdint.h>
#include <dbus/dbus.h>
#include "common.h"

typedef struct {
    DBusConnection* dbus_conn;
    char device_path[256];
    char device_address[18];
    char device_name[256];
    char service_path[256];
    char char_path[256];
    bool connected;
    bool scanning;
} BLEConnection;

// Function declarations
int init_bluetooth();
int scan_for_device(BLEConnection* conn);
int connect_to_device(BLEConnection* conn);
int read_sensor_data(BLEConnection* conn, SensorPacket* packet);
void disconnect_device(BLEConnection* conn);
void cleanup_bluetooth();

#endif