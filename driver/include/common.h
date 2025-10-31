#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdbool.h>

#define SERVICE_UUID        "12345678-1234-1234-1234-123456789abc"
#define CHARACTERISTIC_UUID "87654321-4321-4321-4321-cba987654321"

typedef struct {
    int16_t accel_x, accel_y, accel_z; // Acceleration * 100 (e.g., 1.5g = 150)
    int16_t gyro_x, gyro_y, gyro_z;    // Gyroscope * 10 (e.g., 5.5 deg/s = 55)
    uint8_t button_state;               // 0=none, 1=press, 2=long_press
    uint8_t padding;                    // Padding for alignment
    uint16_t timestamp;                 // Millisecond counter (wraps every 65 seconds)
} __attribute__((packed)) SensorPacket;
// Size: 6*2 + 1 + 1 + 2 = 16 bytes (fits in 20 byte BLE MTU)

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