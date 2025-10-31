#ifndef UINPUT_H
#define UINPUT_H

#include <linux/uinput.h>
#include "common.h"

#define VENDOR_ID  0x045E
#define PRODUCT_ID 0x0823

typedef struct {
    int fd;
    bool initialized;
} UInputDevice;

// Function declarations
int init_uinput_device(UInputDevice* device);
void process_sensor_data(UInputDevice* device, const SensorPacket* packet);
void cleanup_uinput_device(UInputDevice* device);

#endif