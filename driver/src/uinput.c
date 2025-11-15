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
#include "Fusion.h"

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

// IMU state with Fusion AHRS
typedef struct {
    FusionAhrs ahrs;              // Fusion AHRS algorithm
    double t_last;
    float cursor_x, cursor_y;     // Virtual cursor position (accumulated)
    float smoothed_vel_x, smoothed_vel_y;  // Smoothed cursor velocity
    int initialized;
} FusionFilterState;

static FusionFilterState fusion_state = {0};

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

    // Handle button events (unchanged)
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

    // Convert int16 sensor data to float
    FusionVector gyroscope = {
        .axis.x = packet->gyro_x / 10.0f,  // Convert back to degrees/s
        .axis.y = packet->gyro_y / 10.0f,
        .axis.z = packet->gyro_z / 10.0f
    };

    FusionVector accelerometer = {
        .axis.x = packet->accel_x / 100.0f,  // Convert back to g
        .axis.y = packet->accel_y / 100.0f,
        .axis.z = packet->accel_z / 100.0f
    };

    // Get time delta
    double t_now = now_s();
    float dt = fusion_state.initialized ? (float)(t_now - fusion_state.t_last) : 0.02f;
    fusion_state.t_last = t_now;

    if (!fusion_state.initialized) {
        // Initialize Fusion AHRS
        FusionAhrsInitialise(&fusion_state.ahrs);

        // Set AHRS settings optimized for tilt-based mouse control
        FusionAhrsSettings settings = {
            .convention = FusionConventionNwu,        // North-West-Up coordinate system
            .gain = 0.5f,                            // Moderate gain (balance speed vs stability)
            .gyroscopeRange = 2000.0f,               // ±2000 degrees/s range
            .accelerationRejection = 20.0f,          // Moderate rejection (filter fast movements)
            .magneticRejection = 0.0f,               // No magnetometer
            .recoveryTriggerPeriod = 5 * 50          // 5 seconds at 50Hz (don't recover too fast)
        };
        FusionAhrsSetSettings(&fusion_state.ahrs, &settings);

        fusion_state.cursor_x = 0.0f;
        fusion_state.cursor_y = 0.0f;
        fusion_state.initialized = 1;
        return;  // Skip first frame
    }

    // Update AHRS with sensor data (no magnetometer)
    FusionAhrsUpdateNoMagnetometer(&fusion_state.ahrs, gyroscope, accelerometer, dt);

    // Get current orientation as Euler angles (roll, pitch, yaw)
    FusionQuaternion quaternion = FusionAhrsGetQuaternion(&fusion_state.ahrs);
    FusionEuler euler = FusionQuaternionToEuler(quaternion);

    // Use orientation angles directly for cursor control
    // Roll (tilting left/right) → X movement
    // Pitch (tilting forward/back) → Y movement
    // Yaw ignored (no magnetometer, drifts anyway)
    float roll = euler.angle.roll;    // degrees
    float pitch = euler.angle.pitch;  // degrees

    // Apply dead zone to angles (filter small unintentional tilts)
    float dead_zone_deg = config.dead_zone;  // e.g., 3.0 degrees
    if (fabsf(roll) < dead_zone_deg) roll = 0.0f;
    if (fabsf(pitch) < dead_zone_deg) pitch = 0.0f;

    // Map tilt angles to cursor velocity (degrees → pixels/second)
    // Larger tilt = faster cursor movement
    float raw_vel_x = roll * config.movement_sensitivity;    // Roll → horizontal velocity
    float raw_vel_y = pitch * config.movement_sensitivity;   // Pitch → vertical velocity

    // Apply exponential smoothing to reduce jitter (alpha = 0.3 = 30% new, 70% old)
    // Lower alpha = smoother but more lag, higher alpha = more responsive but jittery
    float alpha = 0.3f;
    fusion_state.smoothed_vel_x = alpha * raw_vel_x + (1.0f - alpha) * fusion_state.smoothed_vel_x;
    fusion_state.smoothed_vel_y = alpha * raw_vel_y + (1.0f - alpha) * fusion_state.smoothed_vel_y;

    // Integrate smoothed velocity to cursor position
    fusion_state.cursor_x += fusion_state.smoothed_vel_x * dt;
    fusion_state.cursor_y += fusion_state.smoothed_vel_y * dt;

    // Debug: log orientation and velocity
    static int debug_count = 0;
    if (++debug_count % 10 == 0) {  // Every 10 frames (~200ms at 50Hz)
        syslog(LOG_INFO, "Angles: Roll:%.1f° Pitch:%.1f° Yaw:%.1f° | Vel(%.1f, %.1f) px/s | dt:%.4f",
               euler.angle.roll, euler.angle.pitch, euler.angle.yaw,
               fusion_state.smoothed_vel_x, fusion_state.smoothed_vel_y, dt);
    }

    // Extract integer deltas for mouse movement
    int dx = (int)(fusion_state.cursor_x);
    int dy = (int)(fusion_state.cursor_y);

    // Subtract integer part from accumulated position (keep fractional part for smoothness)
    fusion_state.cursor_x -= (float)dx;
    fusion_state.cursor_y -= (float)dy;

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
        syslog(LOG_INFO, "Movement: Vel(%.1f, %.1f) px/s | Accum(%.2f, %.2f) | Delta dx:%d dy:%d",
               fusion_state.smoothed_vel_x, fusion_state.smoothed_vel_y,
               fusion_state.cursor_x, fusion_state.cursor_y,
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
