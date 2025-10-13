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

static float scroll_buffer[10] = {0};
static int scroll_index = 0;
static uint32_t last_timestamp = 0;

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
    
    // Enable mouse buttons
    ioctl(device->fd, UI_SET_KEYBIT, BTN_LEFT);
    ioctl(device->fd, UI_SET_KEYBIT, BTN_RIGHT);
    ioctl(device->fd, UI_SET_KEYBIT, BTN_MIDDLE);
    
    // Enable relative axes
    ioctl(device->fd, UI_SET_RELBIT, REL_X);
    ioctl(device->fd, UI_SET_RELBIT, REL_Y);
    ioctl(device->fd, UI_SET_RELBIT, REL_WHEEL);
    
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
    syslog(LOG_INFO, "Virtual mouse device created");
    
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

static float filter_scroll(float new_value) {
    scroll_buffer[scroll_index] = new_value;
    scroll_index = (scroll_index + 1) % config.scroll_filter_samples;
    
    float sum = 0;
    for (int i = 0; i < config.scroll_filter_samples; i++) {
        sum += scroll_buffer[i];
    }
    
    return sum / config.scroll_filter_samples;
}

void process_sensor_data(UInputDevice* device, const SensorPacket* packet) {
    if (!device || !device->initialized || !packet) return;
    
    static uint8_t last_button_state = 0;
    
    // Calculate time delta
    uint32_t dt = (last_timestamp > 0) ? (packet->timestamp - last_timestamp) : 20;
    last_timestamp = packet->timestamp;
    
    // Clamp dt to reasonable values
    if (dt > 100) dt = 20;
    
    // Process button events
    if (packet->button_state != last_button_state) {
        if (packet->button_state == 1 && last_button_state == 0) {
            // Left click press
            emit_event(device->fd, EV_KEY, BTN_LEFT, 1);
            emit_sync(device->fd);
        } else if (packet->button_state == 2 && last_button_state == 0) {
            // Right click press
            emit_event(device->fd, EV_KEY, BTN_RIGHT, 1);
            emit_sync(device->fd);
        } else if (packet->button_state == 0 && last_button_state > 0) {
            // Button release
            if (last_button_state == 1) {
                emit_event(device->fd, EV_KEY, BTN_LEFT, 0);
            } else if (last_button_state == 2) {
                emit_event(device->fd, EV_KEY, BTN_RIGHT, 0);
            }
            emit_sync(device->fd);
        }
        last_button_state = packet->button_state;
    }
    
    // Process movement (only when no button is pressed)
    if (packet->button_state == 0) {
        float accel_x = packet->accel_x;
        float accel_y = packet->accel_y;
        float accel_z = packet->accel_z;
        
        // Apply dead zone
        if (fabs(accel_x) < config.dead_zone) accel_x = 0;
        if (fabs(accel_y) < config.dead_zone) accel_y = 0;
        
        // Calculate movement deltas
        int dx = 0, dy = 0;
        
        if (accel_x != 0 || accel_y != 0) {
            dx = (int)(accel_x * config.movement_sensitivity * dt / 20.0f);
            dy = (int)(accel_y * config.movement_sensitivity * dt / 20.0f);
            
            if (config.invert_x) dx = -dx;
            if (config.invert_y) dy = -dy;
        }
        
        // Process scrolling (Z-axis)
        int scroll = 0;
        float filtered_z = filter_scroll(accel_z);
        
        if (fabs(filtered_z) > config.scroll_threshold) {
            scroll = (int)(filtered_z * config.scroll_sensitivity);
            if (config.invert_scroll) scroll = -scroll;
        }
        
        // Emit movement events
        if (dx != 0 || dy != 0 || scroll != 0) {
            if (dx != 0) emit_event(device->fd, EV_REL, REL_X, dx);
            if (dy != 0) emit_event(device->fd, EV_REL, REL_Y, dy);
            if (scroll != 0) emit_event(device->fd, EV_REL, REL_WHEEL, scroll);
            emit_sync(device->fd);
        }
    }
}

void cleanup_uinput_device(UInputDevice* device) {
    if (!device) return;
    
    if (device->initialized && device->fd >= 0) {
        ioctl(device->fd, UI_DEV_DESTROY);
        close(device->fd);
        device->fd = -1;
        device->initialized = false;
        syslog(LOG_INFO, "Virtual mouse device destroyed");
    }
}