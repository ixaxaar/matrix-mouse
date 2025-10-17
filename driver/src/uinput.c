#include "uinput.h"
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <syslog.h>
#include <unistd.h>
#include "common.h"

// Minimal event helpers
static inline void emit_event(int fd, int type, int code, int value) {
    struct input_event ie = {0};
    ie.type = type;
    ie.code = code;
    ie.value = value;
    // Kernel will fill in timestamp
    if (write(fd, &ie, sizeof(ie)) < 0) {
        // Keep quiet to avoid log spam if buffer is full
    }
}
static inline void emit_sync(int fd) { emit_event(fd, EV_SYN, SYN_REPORT, 0); }

// CALIBRATION/PROJECTION HOOK:
// Edit this function to convert raw IMU into 2D cursor deltas.
// You can combine gyro (rad/s or deg/s) and accel (g) however you like.
// Return integer pixel deltas in dx, dy for a single sample.
static void compute_cursor_delta(const SensorPacket* s, int* dx, int* dy) {
    // Base mapping (pure gyro, axis swap):
    // cursor X from gyro_y, cursor Y from -gyro_x
    float gx = s->gyro_x;
    float gy = s->gyro_y;

    // Configurable dead-zone (suppress small jitters)
    const float dz = (config.dead_zone > 0.f) ? config.dead_zone : 0.f;
    if (fabsf(gx) < dz) gx = 0.f;
    if (fabsf(gy) < dz) gy = 0.f;

    // Sensitivity scales pixels per unit gyro
    // Tune baseline 120.0f if your sample rate differs from 50 Hz
    const float base_scale = 120.0f * (config.movement_sensitivity > 0.f ? config.movement_sensitivity : 1.0f);

    int out_dx = (int)lroundf(gy * base_scale);
    int out_dy = (int)lroundf(-gx * base_scale);

    // Invert axes if requested
    if (config.invert_x) out_dx = -out_dx;
    if (config.invert_y) out_dy = -out_dy;

    // Placeholder for accel-assisted drift control (mix tilt estimate):
    // float ax = s->accel_x, ay = s->accel_y, az = s->accel_z;
    // TODO: blend gyro integration with accel gravity vector to reduce drift.

    *dx = out_dx;
    *dy = out_dy;
}

int init_uinput_device(UInputDevice* device) {
    if (!device) return -1;

    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        syslog(LOG_ERR, "uinput open failed: %s", strerror(errno));
        return -1;
    }

    // Minimal mouse capabilities: relative X/Y + at least one button
    if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0) goto err;
    if (ioctl(fd, UI_SET_KEYBIT, BTN_LEFT) < 0) goto err;

    if (ioctl(fd, UI_SET_EVBIT, EV_REL) < 0) goto err;
    if (ioctl(fd, UI_SET_RELBIT, REL_X) < 0) goto err;
    if (ioctl(fd, UI_SET_RELBIT, REL_Y) < 0) goto err;

    struct uinput_setup us = {0};
    us.id.bustype = BUS_USB;     // Works well for desktops; BUS_BLUETOOTH also fine
    us.id.vendor  = 0x045E;      // Arbitrary vendor/product (can change)
    us.id.product = 0x0823;
    strncpy(us.name, "M5 Matrix IMU Mouse", sizeof(us.name) - 1);

    if (ioctl(fd, UI_DEV_SETUP, &us) < 0) goto err;
    if (ioctl(fd, UI_DEV_CREATE) < 0) goto err;

    device->fd = fd;
    device->initialized = true;
    syslog(LOG_INFO, "uinput mouse created (relative X/Y, BTN_LEFT)");
    return 0;

err:
    syslog(LOG_ERR, "uinput setup failed: %s", strerror(errno));
    close(fd);
    return -1;
}

void process_sensor_data(UInputDevice* device, const SensorPacket* packet) {
    if (!device || !device->initialized || !packet) return;

    // Optional: handle button here later if you map packet->button_state
    // emit_event(device->fd, EV_KEY, BTN_LEFT, packet->button_state ? 1 : 0);

    int dx = 0, dy = 0;
    compute_cursor_delta(packet, &dx, &dy);

    if (dx || dy) {
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
    }
}
