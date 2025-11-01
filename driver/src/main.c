#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <getopt.h>
#include <syslog.h>
#include <sys/stat.h>
#include "common.h"
#include "bluetooth.h"
#include "uinput.h"

MouseConfig config = {
    .movement_sensitivity = 2.0f,
    .scroll_sensitivity = 1.0f,
    .dead_zone = 0.1f,
    .scroll_threshold = 0.3f,
    .invert_x = false,
    .invert_y = false,
    .invert_scroll = false,
    .scroll_filter_samples = 5
};

bool running = true;

void signal_handler(int sig) {
    if (sig == SIGALRM) {
        syslog(LOG_ERR, "Forced exit due to timeout");
        exit(1);
    }
    running = false;
    syslog(LOG_INFO, "Received signal %d, shutting down...", sig);
    // Force exit if we can't gracefully shutdown in 2 seconds
    alarm(2);
}

void print_usage(const char* program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("Options:\n");
    printf("  -c, --config FILE    Configuration file path\n");
    printf("  -d, --daemon         Run as daemon\n");
    printf("  -v, --verbose        Verbose output\n");
    printf("  -h, --help           Show this help\n");
}

int main(int argc, char* argv[]) {
    bool daemon_mode = false;
    bool verbose = false;
    char* config_file = "/etc/m5-mouse.yaml";

    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"daemon", no_argument, 0, 'd'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "c:dvh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'c':
                config_file = optarg;
                break;
            case 'd':
                daemon_mode = true;
                break;
            case 'v':
                verbose = true;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    // Load configuration
    load_config(config_file);

    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGALRM, signal_handler);

    if (daemon_mode) {
        // Daemonize
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return 1;
        }
        if (pid > 0) {
            return 0; // Parent exits
        }

        setsid();
        chdir("/");
        umask(0);

        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        openlog("m5-mouse-daemon", LOG_PID, LOG_DAEMON);
    } else {
        openlog("m5-mouse-daemon", LOG_PID | LOG_PERROR, LOG_USER);
    }

    syslog(LOG_INFO, "M5 Mouse Daemon starting...");

    // Initialize Bluetooth
    if (init_bluetooth() < 0) {
        syslog(LOG_ERR, "Failed to initialize Bluetooth");
        return 1;
    }

    // Initialize uinput device
    UInputDevice uinput_device;
    if (init_uinput_device(&uinput_device) < 0) {
        syslog(LOG_ERR, "Failed to initialize uinput device");
        cleanup_bluetooth();
        return 1;
    }

    syslog(LOG_INFO, "Scanning for M5 device...");

    BLEConnection connection = {0};

    while (running) {
        // Scan for device
        if (scan_for_device(&connection) < 0) {
            syslog(LOG_WARNING, "Device scan failed, retrying in 5 seconds...");
            sleep(5);
            continue;
        }

        syslog(LOG_INFO, "Found M5 device: %s", connection.device_name);

        // Connect to device
        if (connect_to_device(&connection) < 0) {
            syslog(LOG_WARNING, "Connection failed, retrying in 5 seconds...");
            sleep(5);
            continue;
        }

        syslog(LOG_INFO, "Connected to M5 device");

        // Main data processing loop
        while (running && connection.connected) {
            SensorPacket packet;
            int result = read_sensor_data(&connection, &packet);

            if (result < 0) {
                syslog(LOG_WARNING, "Lost connection to device");
                break;
            }

            if (result > 0) {
                process_sensor_data(&uinput_device, &packet);

                if (verbose && !daemon_mode) {
                    printf("Accel: %.2f,%.2f,%.2f Gyro: %.2f,%.2f,%.2f Btn: %d\n",
                           packet.accel_x / 100.0f, packet.accel_y / 100.0f, packet.accel_z / 100.0f,
                           packet.gyro_x / 10.0f, packet.gyro_y / 10.0f, packet.gyro_z / 10.0f,
                           packet.button_state);
                }
            }

            usleep(20000); // 50Hz
        }

        disconnect_device(&connection);
        syslog(LOG_INFO, "Disconnected from device, will retry...");
        sleep(2);
    }

    cleanup_uinput_device(&uinput_device);
    cleanup_bluetooth();

    syslog(LOG_INFO, "M5 Mouse Daemon stopped");
    closelog();

    return 0;
}
