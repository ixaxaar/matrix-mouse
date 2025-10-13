#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/l2cap.h>
#include <bluetooth/uuid.h>
#include "common.h"

typedef struct {
    int socket;
    bdaddr_t device_addr;
    bool connected;
    char device_name[256];
} BLEConnection;

// Function declarations
int init_bluetooth();
int scan_for_device(BLEConnection* conn);
int connect_to_device(BLEConnection* conn);
int read_sensor_data(BLEConnection* conn, SensorPacket* packet);
void disconnect_device(BLEConnection* conn);
void cleanup_bluetooth();

#endif