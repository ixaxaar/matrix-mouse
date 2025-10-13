#ifndef UINPUT_H
#define UINPUT_H

#include <linux/uinput.h>
#include "common.h"

typedef struct {
    int fd;
    bool initialized;
} UInputDevice;

// Function declarations
int init_uinput_device(UInputDevice* device);
void process_sensor_data(UInputDevice* device, const SensorPacket* packet);
void cleanup_uinput_device(UInputDevice* device);

#endif