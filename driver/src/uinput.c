#define _GNU_SOURCE
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
#include <time.h>

// Emit a single input event
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
static uint8_t last_button_state = 0;

// IMU filter state
typedef struct {
    double t_last;
    float roll, pitch, yaw;
    float gxf, gyf, gzf;
    int initialized;
} ImuFilterState;

static inline double now_s() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

int init_uinput_device(UInputDevice* device) {
    if (!device) return -1;

    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        syslog(LOG_ERR, "uinput open failed: %s", strerror(errno));
        return -1;
    }

    // Minimal mouse capabilities: relative X/Y + right and left mouse buttons
    if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0) goto err;
    if (ioctl(fd, UI_SET_KEYBIT, BTN_LEFT) < 0) goto err;
    if (ioctl(fd, UI_SET_KEYBIT, BTN_RIGHT) < 0) goto err;

    if (ioctl(fd, UI_SET_EVBIT, EV_REL) < 0) goto err;
    if (ioctl(fd, UI_SET_RELBIT, REL_X) < 0) goto err;
    if (ioctl(fd, UI_SET_RELBIT, REL_Y) < 0) goto err;

    struct uinput_setup us = {0};
    us.id.bustype = BUS_USB;     // Works well for desktops; BUS_BLUETOOTH also fine
    us.id.vendor  = VENDOR_ID;
    us.id.product = PRODUCT_ID;
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

    // Convert int16 back to floats
    float accel_x = packet->accel_x / 100.0f;
    float accel_z = packet->accel_z / 100.0f;

    // Handle button events
    if (packet->button_state != last_button_state) {
        // Reset last button state
        if (last_button_state == 1)
            emit_event(device->fd, EV_KEY, BTN_LEFT, 0);
        else if (last_button_state == 2)
            emit_event(device->fd, EV_KEY, BTN_RIGHT, 0);
        emit_sync(device->fd);

        if (packet->button_state == 1) {
            // Button pressed - left click down
            emit_event(device->fd, EV_KEY, BTN_LEFT, 1);
            emit_sync(device->fd);
            syslog(LOG_INFO, "Left button pressed");
        } else if (packet->button_state == 2) {
            // Right click down
            emit_event(device->fd, EV_KEY, BTN_RIGHT, 1);
            emit_sync(device->fd);
            syslog(LOG_INFO, "Right button pressed");
        } else if (packet->button_state == 0) {
            // Button released
            if (last_button_state == 1)
                emit_event(device->fd, EV_KEY, BTN_LEFT, 0);
            else if (last_button_state == 2)
                emit_event(device->fd, EV_KEY, BTN_RIGHT, 0);
            emit_sync(device->fd);
            syslog(LOG_INFO, "Button released");
        }

        last_button_state = packet->button_state;
    }

    // Calculate tilt angles from accelerometer
    // When device is flat: accel_y ≈ -1.0g (gravity), accel_x ≈ 0, accel_z ≈ 0
    // Tilt left/right changes accel_x, tilt forward/back changes accel_z

    // Remove the gravity component from Y axis
    float tilt_x = accel_x;  // Left/Right tilt (positive = tilt right)
    float tilt_z = accel_z;  // Forward/Back tilt (positive = tilt forward)

    // Apply dead zone to reduce jitter
    if (fabsf(tilt_x) < config.dead_zone) tilt_x = 0.0f;
    if (fabsf(tilt_z) < config.dead_zone) tilt_z = 0.0f;

    // Calculate mouse movement deltas
    // Map tilt to cursor movement: tilt right = move right, tilt forward = move up
    int dx = 0, dy = 0;

    if (tilt_x != 0.0f || tilt_z != 0.0f) {
        // Apply sensitivity and invert if configured
        dx = (int)(tilt_x * config.movement_sensitivity * (config.invert_x ? -1 : 1));
        dy = (int)(-tilt_z * config.movement_sensitivity * (config.invert_y ? -1 : 1));

        // Clamp to reasonable values to prevent jumping
        if (dx > 50) dx = 50;
        if (dx < -50) dx = -50;
        if (dy > 50) dy = 50;
        if (dy < -50) dy = -50;
    }

    // Log periodically (every 50 packets ~1 second)
    static int log_count = 0;
    if (++log_count % 50 == 0) {
        syslog(LOG_INFO, "Tilt X: %.2f Z: %.2f -> dx: %d dy: %d",
               tilt_x, tilt_z, dx, dy);
    }

    // Send mouse movement if there's any delta
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
    }
}
