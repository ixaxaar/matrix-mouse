#include "uinput.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <math.h>

int init_uinput_device(UInputDevice* device) {
    if (!device) return -1;

    device->fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (device->fd < 0) {
        syslog(LOG_ERR, "Failed to open /dev/uinput: %s", strerror(errno));
        return -1;
    }

    // Enable event types
    ioctl(device->fd, UI_SET_EVBIT, EV_KEY);
    ioctl(device->fd, UI_SET_EVBIT, EV_REL);

    // Enable only left mouse button
    ioctl(device->fd, UI_SET_KEYBIT, BTN_LEFT);

    // Enable relative axes for movement only
    ioctl(device->fd, UI_SET_RELBIT, REL_X);
    ioctl(device->fd, UI_SET_RELBIT, REL_Y);

    // Setup device info
    struct uinput_setup usetup = {0};
    usetup.id.bustype = BUS_BLUETOOTH;
    usetup.id.vendor = 0x1234;
    usetup.id.product = 0x5678;
    strcpy(usetup.name, "M5 Atom Matrix Mouse");

    if (ioctl(device->fd, UI_DEV_SETUP, &usetup) < 0) {
        syslog(LOG_ERR, "Failed to setup uinput device: %s", strerror(errno));
        close(device->fd);
        return -1;
    }

    if (ioctl(device->fd, UI_DEV_CREATE) < 0) {
        syslog(LOG_ERR, "Failed to create uinput device: %s", strerror(errno));
        close(device->fd);
        return -1;
    }

    device->initialized = true;
    syslog(LOG_INFO, "Simple virtual mouse device created");

    return 0;
}

static void emit_event(int fd, int type, int code, int value) {
    struct input_event ie = {0};
    ie.type = type;
    ie.code = code;
    ie.value = value;

    write(fd, &ie, sizeof(ie));
}

static void emit_sync(int fd) {
    emit_event(fd, EV_SYN, SYN_REPORT, 0);
}

void process_sensor_data(UInputDevice* device, const SensorPacket* packet) {
    if (!device || !device->initialized || !packet) return;

    static uint8_t last_button_state = 0;

    // Process button events (any button = left click)
    if (packet->button_state != last_button_state) {
        if (packet->button_state > 0 && last_button_state == 0) {
            // Any button press = Left click press
            emit_event(device->fd, EV_KEY, BTN_LEFT, 1);
            emit_sync(device->fd);
        } else if (packet->button_state == 0 && last_button_state > 0) {
            // Button release = Left click release
            emit_event(device->fd, EV_KEY, BTN_LEFT, 0);
            emit_sync(device->fd);
        }
        last_button_state = packet->button_state;
    }

    // Process gyro movement (always active)
    float gyro_x = packet->gyro_x;
    float gyro_y = packet->gyro_y;

    // Apply dead zone to gyro
    if (fabs(gyro_x) < config.dead_zone) gyro_x = 0;
    if (fabs(gyro_y) < config.dead_zone) gyro_y = 0;

    // Calculate movement deltas using gyro
    int dx = 0, dy = 0;

    if (gyro_x != 0 || gyro_y != 0) {
        // Scale gyro values for mouse movement
        dx = (int)(gyro_x * config.movement_sensitivity * 50.0f);
        dy = (int)(gyro_y * config.movement_sensitivity * 50.0f);

        if (config.invert_x) dx = -dx;
        if (config.invert_y) dy = -dy;
    }

    // Emit movement events
    if (dx != 0 || dy != 0) {
        emit_event(device->fd, EV_REL, REL_X, dx);
        emit_event(device->fd, EV_REL, REL_Y, dy);
        emit_sync(device->fd);
    }
}

void cleanup_uinput_device(UInputDevice* device) {
    if (!device) return;

    if (device->initialized && device->fd >= 0) {
        ioctl(device->fd, UI_DEV_DESTROY);
        close(device->fd);
        device->fd = -1;
        device->initialized = false;
        syslog(LOG_INFO, "Simple virtual mouse device destroyed");
    }
}