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

// Lightweight IMU filter state
typedef struct {
    double t_last;
    float roll, pitch, yaw;
    float gxf, gyf, gzf;
    int initialized;
} ImuFilterState;

static ImuFilterState imu = {0};

static inline double now_s() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

// CALIBRATION/PROJECTION HOOK:
// Edit this function to convert raw IMU into 2D cursor deltas.
// You can combine gyro (rad/s or deg/s) and accel (g) however you like.
// Return integer pixel deltas in dx, dy for a single sample.
static void compute_cursor_delta(const SensorPacket* s, int* dx, int* dy) {
    double t = now_s();
    if (!imu.initialized) {
        imu.t_last = t;
        imu.roll = atan2f(s->accel_y, s->accel_z);
        imu.pitch = atan2f(-s->accel_x, hypotf(s->accel_y, s->accel_z));
        imu.yaw = 0.f;
        imu.gxf = s->gyro_x;
        imu.gyf = s->gyro_y;
        imu.gzf = s->gyro_z;
        imu.initialized = 1;
        *dx = *dy = 0;
        return;
    }

    double dt = t - imu.t_last;
    if (dt <= 0.0 || dt > 0.2) dt = 0.02;  // guard
    imu.t_last = t;

    // Low-pass gyro
    const float lpf = 0.2f;
    imu.gxf += lpf * (s->gyro_x - imu.gxf);
    imu.gyf += lpf * (s->gyro_y - imu.gyf);
    imu.gzf += lpf * (s->gyro_z - imu.gzf);

    float gx = imu.gxf, gy = imu.gyf, gz = imu.gzf;

    // Complementary filter for roll/pitch, yaw is integrated only
    const float alpha = 0.98f;
    float acc_roll  = atan2f(s->accel_y, s->accel_z);
    float acc_pitch = atan2f(-s->accel_x, hypotf(s->accel_y, s->accel_z));
    imu.roll  = alpha * (imu.roll  + gx * (float)dt) + (1.f - alpha) * acc_roll;
    imu.pitch = alpha * (imu.pitch + gy * (float)dt) + (1.f - alpha) * acc_pitch;
    imu.yaw  += gz * (float)dt;

    // Dead zone on rate
    float rate_mag = fmaxf(fabsf(gx), fmaxf(fabsf(gy), fabsf(gz)));
    if (rate_mag < (config.dead_zone > 0.f ? config.dead_zone : 0.f)) {
        *dx = *dy = 0;
        return;
    }

    // Map to cursor: horizontal from yaw rate (with a touch of gy), vertical from -gx
    float sens = config.movement_sensitivity > 0.f ? config.movement_sensitivity : 1.f;
    const float rate_gain = 1000.f; // pixels per rad/s, scaled by dt

    float dx_rate = 0.85f * gz + 0.15f * gy;
    float dy_rate = -gx;

    int out_dx = (int)lroundf(dx_rate * (float)dt * rate_gain * sens);
    int out_dy = (int)lroundf(dy_rate * (float)dt * rate_gain * sens);

    // Optional small posture damping to reduce drift when still
    const float angle_damp = 0.0f; // set >0 to blend angle feedback if needed
    if (angle_damp > 0.f) {
        out_dx += (int)lroundf(imu.yaw * angle_damp);
        out_dy += (int)lroundf(imu.pitch * angle_damp);
    }

    if (config.invert_x) out_dx = -out_dx;
    if (config.invert_y) out_dy = -out_dy;

    // Clamp step size
    const int max_step = 60;
    if (out_dx >  max_step) out_dx =  max_step;
    if (out_dx < -max_step) out_dx = -max_step;
    if (out_dy >  max_step) out_dy =  max_step;
    if (out_dy < -max_step) out_dy = -max_step;

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
