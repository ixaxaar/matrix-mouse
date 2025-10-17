#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <yaml.h>

static void parse_yaml_value(const char* key, yaml_node_t* value_node) {
    if (value_node->type == YAML_SCALAR_NODE) {
        char* value = (char*)value_node->data.scalar.value;
        
        if (strcmp(key, "movement_sensitivity") == 0) {
            config.movement_sensitivity = atof(value);
        } else if (strcmp(key, "scroll_sensitivity") == 0) {
            config.scroll_sensitivity = atof(value);
        } else if (strcmp(key, "dead_zone") == 0) {
            config.dead_zone = atof(value);
        } else if (strcmp(key, "scroll_threshold") == 0) {
            config.scroll_threshold = atof(value);
        } else if (strcmp(key, "invert_x") == 0) {
            config.invert_x = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
        } else if (strcmp(key, "invert_y") == 0) {
            config.invert_y = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
        } else if (strcmp(key, "invert_scroll") == 0) {
            config.invert_scroll = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
        } else if (strcmp(key, "scroll_filter_samples") == 0) {
            config.scroll_filter_samples = atoi(value);
            if (config.scroll_filter_samples < 1) config.scroll_filter_samples = 1;
            if (config.scroll_filter_samples > 10) config.scroll_filter_samples = 10;
        }
    }
}

void load_config(const char* config_file) {
    FILE* file = fopen(config_file, "r");
    if (!file) {
        syslog(LOG_WARNING, "Config file %s not found, using defaults", config_file);
        return;
    }

    yaml_parser_t parser;
    yaml_document_t document;
    
    if (!yaml_parser_initialize(&parser)) {
        syslog(LOG_ERR, "Failed to initialize YAML parser");
        fclose(file);
        return;
    }

    yaml_parser_set_input_file(&parser, file);

    if (!yaml_parser_load(&parser, &document)) {
        syslog(LOG_ERR, "Failed to parse YAML config file %s", config_file);
        yaml_parser_delete(&parser);
        fclose(file);
        return;
    }

    yaml_node_t* root = yaml_document_get_root_node(&document);
    if (root && root->type == YAML_MAPPING_NODE) {
        yaml_node_pair_t* pair;
        for (pair = root->data.mapping.pairs.start; 
             pair < root->data.mapping.pairs.top; pair++) {
            
            yaml_node_t* key_node = yaml_document_get_node(&document, pair->key);
            yaml_node_t* value_node = yaml_document_get_node(&document, pair->value);
            
            if (key_node->type == YAML_SCALAR_NODE) {
                char* key = (char*)key_node->data.scalar.value;
                parse_yaml_value(key, value_node);
            }
        }
    }

    yaml_document_delete(&document);
    yaml_parser_delete(&parser);
    fclose(file);
    
    syslog(LOG_INFO, "Configuration loaded from %s", config_file);
}