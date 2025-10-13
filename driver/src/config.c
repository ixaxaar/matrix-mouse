#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

void load_config(const char* config_file) {
    FILE* file = fopen(config_file, "r");
    if (!file) {
        syslog(LOG_WARNING, "Config file %s not found, using defaults", config_file);
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        // Remove newline
        line[strcspn(line, "\n")] = 0;

        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\0') continue;

        char key[64], value[64];
        if (sscanf(line, "%63[^=]=%63s", key, value) == 2) {
            // Trim whitespace
            char* k = key;
            while (*k == ' ') k++;
            char* ke = k + strlen(k) - 1;
            while (ke > k && *ke == ' ') *ke-- = '\0';

            char* v = value;
            while (*v == ' ') v++;
            char* ve = v + strlen(v) - 1;
            while (ve > v && *ve == ' ') *ve-- = '\0';

            // Parse configuration values
            if (strcmp(k, "movement_sensitivity") == 0) {
                config.movement_sensitivity = atof(v);
            } else if (strcmp(k, "scroll_sensitivity") == 0) {
                config.scroll_sensitivity = atof(v);
            } else if (strcmp(k, "dead_zone") == 0) {
                config.dead_zone = atof(v);
            } else if (strcmp(k, "scroll_threshold") == 0) {
                config.scroll_threshold = atof(v);
            } else if (strcmp(k, "invert_x") == 0) {
                config.invert_x = (strcmp(v, "true") == 0 || strcmp(v, "1") == 0);
            } else if (strcmp(k, "invert_y") == 0) {
                config.invert_y = (strcmp(v, "true") == 0 || strcmp(v, "1") == 0);
            } else if (strcmp(k, "invert_scroll") == 0) {
                config.invert_scroll = (strcmp(v, "true") == 0 || strcmp(v, "1") == 0);
            } else if (strcmp(k, "scroll_filter_samples") == 0) {
                config.scroll_filter_samples = atoi(v);
                if (config.scroll_filter_samples < 1) config.scroll_filter_samples = 1;
                if (config.scroll_filter_samples > 10) config.scroll_filter_samples = 10;
            }
        }
    }

    fclose(file);
    syslog(LOG_INFO, "Configuration loaded from %s", config_file);
}