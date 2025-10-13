#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdbool.h>

#define SERVICE_UUID        "12345678-1234-1234-1234-123456789abc"
#define CHARACTERISTIC_UUID "87654321-4321-4321-4321-cba987654321"

typedef struct {
    float accel_x, accel_y, accel_z;
    float gyro_x, gyro_y, gyro_z;
    uint8_t button_state;  // 0=none, 1=short_press, 2=long_press
    uint32_t timestamp;
} __attribute__((packed)) SensorPacket;

typedef struct {
    float movement_sensitivity;
    float scroll_sensitivity;
    float dead_zone;
    float scroll_threshold;
    bool invert_x;
    bool invert_y;
    bool invert_scroll;
    int scroll_filter_samples;
} MouseConfig;

// Global configuration
extern MouseConfig config;
extern bool running;

// Function declarations
void signal_handler(int sig);
void load_config(const char* config_file);
void print_usage(const char* program_name);

#endif