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

// IMU state for position tracking
typedef struct {
    double t_last;
    float cursor_x, cursor_y;     // Virtual cursor position (accumulated)
    float accel_x_smooth, accel_y_smooth;  // Low-pass filtered accelerometer
    int initialized;
} ImuFilterState;

static ImuFilterState imu_state = {0};

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

    // Convert int16 back to floats
    float accel_x = packet->accel_x / 100.0f;  // g
    float accel_y = packet->accel_y / 100.0f;  // g
    // accel_z ignored for now (could be used for scroll or 3D tracking later)

    // Get time delta
    double t_now = now_s();
    float dt = imu_state.initialized ? (float)(t_now - imu_state.t_last) : 0.02f;
    imu_state.t_last = t_now;

    if (!imu_state.initialized) {
        // Initialize state
        imu_state.cursor_x = 0.0f;
        imu_state.cursor_y = 0.0f;
        imu_state.accel_x_smooth = accel_x;
        imu_state.accel_y_smooth = accel_y;
        imu_state.initialized = 1;
        return;  // Skip first frame
    }

    // Low-pass filter to smooth accelerometer readings
    const float ACCEL_SMOOTH = 0.8f;  // 0.8 = keep 80% of old value, add 20% new
    imu_state.accel_x_smooth = ACCEL_SMOOTH * imu_state.accel_x_smooth + (1.0f - ACCEL_SMOOTH) * accel_x;
    imu_state.accel_y_smooth = ACCEL_SMOOTH * imu_state.accel_y_smooth + (1.0f - ACCEL_SMOOTH) * accel_y;

    // Use smoothed accelerometer as direct input (tilt-based control)
    // When device tilts right, accel_x increases (gravity component shifts)
    // This is more stable than trying to track translation
    float tilt_x = imu_state.accel_x_smooth;
    float tilt_y = imu_state.accel_y_smooth;

    // Debug: log raw values
    static int debug_count = 0;
    if (++debug_count % 10 == 0) {  // Every 10 frames (~200ms)
        syslog(LOG_INFO, "RAW: accel(%.3f, %.3f) smooth(%.3f, %.3f) deadzone:%.3f",
               accel_x, accel_y, tilt_x, tilt_y, config.dead_zone);
    }

    // Apply dead zone to filter small tilts
    if (fabsf(tilt_x) < config.dead_zone) tilt_x = 0.0f;
    if (fabsf(tilt_y) < config.dead_zone) tilt_y = 0.0f;

    // Map tilt directly to cursor velocity (simpler and more stable)
    // Tilt right (+X) → cursor moves right
    // Tilt forward (+Y) → cursor moves up
    float scale = config.movement_sensitivity * 50.0f;  // Much lower scale
    imu_state.cursor_x += tilt_x * scale * dt;
    imu_state.cursor_y += tilt_y * scale * dt;

    // Debug cursor accumulation
    if (debug_count % 10 == 0) {
        syslog(LOG_INFO, "TILT: (%.3f, %.3f) cursor_accum: (%.2f, %.2f) scale:%.1f dt:%.4f",
               tilt_x, tilt_y, imu_state.cursor_x, imu_state.cursor_y, scale, dt);
    }

    // Extract integer deltas for mouse movement
    int dx = (int)(imu_state.cursor_x);
    int dy = (int)(imu_state.cursor_y);

    // Subtract integer part from accumulated position (keep fractional part for smoothness)
    imu_state.cursor_x -= (float)dx;
    imu_state.cursor_y -= (float)dy;

    // Apply invert settings
    if (config.invert_x) dx = -dx;
    if (config.invert_y) dy = -dy;

    // Clamp to reasonable values to prevent jumping
    if (dx > 50) dx = 50;
    if (dx < -50) dx = -50;
    if (dy > 50) dy = 50;
    if (dy < -50) dy = -50;

    // Log periodically (every 50 packets ~1 second)
    static int log_count = 0;
    if (++log_count % 50 == 0) {
        syslog(LOG_INFO, "Tilt: (%.2f, %.2f) | Cursor: (%.2f, %.2f) -> dx:%d dy:%d",
               tilt_x, tilt_y,
               imu_state.cursor_x, imu_state.cursor_y,
               dx, dy);
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
