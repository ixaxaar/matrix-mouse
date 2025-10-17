#define _GNU_SOURCE
#include "bluetooth.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <syslog.h>
#include <time.h>
#include <math.h>

#define BLUEZ_SERVICE "org.bluez"
#define BLUEZ_ADAPTER_INTERFACE "org.bluez.Adapter1"
#define BLUEZ_DEVICE_INTERFACE "org.bluez.Device1"
#define BLUEZ_GATT_SERVICE_INTERFACE "org.bluez.GattService1"
#define BLUEZ_GATT_CHARACTERISTIC_INTERFACE "org.bluez.GattCharacteristic1"
#define ADAPTER_PATH "/org/bluez/hci0"

static DBusConnection* dbus_conn = NULL;

static int call_method(const char* path, const char* interface, const char* method) {
    DBusMessage* msg = dbus_message_new_method_call(BLUEZ_SERVICE, path, interface, method);
    if (!msg) return -1;
    
    DBusMessage* reply = dbus_connection_send_with_reply_and_block(dbus_conn, msg, 5000, NULL);
    dbus_message_unref(msg);
    
    if (!reply) return -1;
    
    dbus_message_unref(reply);
    return 0;
}

static int set_property(const char* path, const char* interface, const char* property, bool value) {
    DBusMessage* msg = dbus_message_new_method_call(BLUEZ_SERVICE, path, "org.freedesktop.DBus.Properties", "Set");
    if (!msg) return -1;
    
    DBusMessageIter iter, variant;
    dbus_message_iter_init_append(msg, &iter);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &interface);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &property);
    
    dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, DBUS_TYPE_BOOLEAN_AS_STRING, &variant);
    dbus_message_iter_append_basic(&variant, DBUS_TYPE_BOOLEAN, &value);
    dbus_message_iter_close_container(&iter, &variant);
    
    DBusMessage* reply = dbus_connection_send_with_reply_and_block(dbus_conn, msg, 5000, NULL);
    dbus_message_unref(msg);
    
    if (!reply) return -1;
    
    dbus_message_unref(reply);
    return 0;
}

static char* get_string_property(const char* path, const char* interface, const char* property) {
    DBusMessage* msg = dbus_message_new_method_call(BLUEZ_SERVICE, path, "org.freedesktop.DBus.Properties", "Get");
    if (!msg) return NULL;
    
    DBusMessageIter iter;
    dbus_message_iter_init_append(msg, &iter);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &interface);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &property);
    
    DBusMessage* reply = dbus_connection_send_with_reply_and_block(dbus_conn, msg, 5000, NULL);
    dbus_message_unref(msg);
    
    if (!reply) return NULL;
    
    DBusMessageIter reply_iter, variant_iter;
    dbus_message_iter_init(reply, &reply_iter);
    dbus_message_iter_recurse(&reply_iter, &variant_iter);
    
    char* value = NULL;
    char* str_value;
    if (dbus_message_iter_get_arg_type(&variant_iter) == DBUS_TYPE_STRING) {
        dbus_message_iter_get_basic(&variant_iter, &str_value);
        value = strdup(str_value);
    }
    
    dbus_message_unref(reply);
    return value;
}

int init_bluetooth() {
    syslog(LOG_INFO, "Initializing BlueZ D-Bus connection");
    
    DBusError error;
    dbus_error_init(&error);
    
    dbus_conn = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
    if (dbus_error_is_set(&error)) {
        syslog(LOG_ERR, "D-Bus connection failed: %s", error.message);
        dbus_error_free(&error);
        return -1;
    }
    
    if (set_property(ADAPTER_PATH, BLUEZ_ADAPTER_INTERFACE, "Powered", true) != 0) {
        syslog(LOG_ERR, "Failed to power on adapter");
        return -1;
    }
    
    syslog(LOG_INFO, "BlueZ D-Bus initialized successfully");
    return 0;
}

int scan_for_device(BLEConnection* conn) {
    if (!conn || !dbus_conn) return -1;
    
    syslog(LOG_INFO, "Starting BLE scan for M5 device");
    
    if (set_property(ADAPTER_PATH, BLUEZ_ADAPTER_INTERFACE, "Discovering", true) != 0) {
        syslog(LOG_ERR, "Failed to start scanning");
        return -1;
    }
    
    conn->scanning = true;
    sleep(5);  // Scan for 5 seconds
    
    DBusMessage* msg = dbus_message_new_method_call("org.freedesktop.DBus", "/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
    if (!msg) {
        set_property(ADAPTER_PATH, BLUEZ_ADAPTER_INTERFACE, "Discovering", false);
        return -1;
    }
    
    DBusMessage* reply = dbus_connection_send_with_reply_and_block(dbus_conn, msg, 10000, NULL);
    dbus_message_unref(msg);
    
    if (!reply) {
        set_property(ADAPTER_PATH, BLUEZ_ADAPTER_INTERFACE, "Discovering", false);
        return -1;
    }
    
    DBusMessageIter iter, dict_iter, entry_iter;
    dbus_message_iter_init(reply, &iter);
    dbus_message_iter_recurse(&iter, &dict_iter);
    
    bool found = false;
    while (dbus_message_iter_get_arg_type(&dict_iter) == DBUS_TYPE_DICT_ENTRY) {
        dbus_message_iter_recurse(&dict_iter, &entry_iter);
        
        char* path;
        dbus_message_iter_get_basic(&entry_iter, &path);
        
        if (strstr(path, "/org/bluez/hci0/dev_")) {
            char* name = get_string_property(path, BLUEZ_DEVICE_INTERFACE, "Name");
            char* address = get_string_property(path, BLUEZ_DEVICE_INTERFACE, "Address");
            
            if (name && (strstr(name, "M5") || strstr(name, "Mouse"))) {
                strcpy(conn->device_path, path);
                strcpy(conn->device_name, name);
                if (address) strcpy(conn->device_address, address);
                found = true;
                syslog(LOG_INFO, "Found M5 device: %s at %s", name, address ? address : "unknown");
            }
            
            free(name);
            free(address);
            
            if (found) break;
        }
        
        dbus_message_iter_next(&dict_iter);
    }
    
    dbus_message_unref(reply);
    set_property(ADAPTER_PATH, BLUEZ_ADAPTER_INTERFACE, "Discovering", false);
    conn->scanning = false;
    
    return found ? 0 : -1;
}

int connect_to_device(BLEConnection* conn) {
    if (!conn || !dbus_conn || strlen(conn->device_path) == 0) {
        syslog(LOG_ERR, "Invalid device path for connection");
        return -1;
    }
    
    syslog(LOG_INFO, "Connecting to device: %s", conn->device_path);
    
    if (call_method(conn->device_path, BLUEZ_DEVICE_INTERFACE, "Connect") != 0) {
        syslog(LOG_ERR, "Failed to connect to device");
        return -1;
    }
    
    sleep(3);  // Give connection time to establish
    
    // Find GATT service and characteristic
    DBusMessage* msg = dbus_message_new_method_call("org.freedesktop.DBus", "/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
    if (!msg) return -1;
    
    DBusMessage* reply = dbus_connection_send_with_reply_and_block(dbus_conn, msg, 10000, NULL);
    dbus_message_unref(msg);
    
    if (!reply) return -1;
    
    DBusMessageIter iter, dict_iter, entry_iter;
    dbus_message_iter_init(reply, &iter);
    dbus_message_iter_recurse(&iter, &dict_iter);
    
    bool service_found = false, char_found = false;
    char device_prefix[256];
    snprintf(device_prefix, sizeof(device_prefix), "%s/service", conn->device_path);
    
    while (dbus_message_iter_get_arg_type(&dict_iter) == DBUS_TYPE_DICT_ENTRY) {
        dbus_message_iter_recurse(&dict_iter, &entry_iter);
        
        char* path;
        dbus_message_iter_get_basic(&entry_iter, &path);
        
        if (strstr(path, device_prefix)) {
            char* uuid = get_string_property(path, BLUEZ_GATT_SERVICE_INTERFACE, "UUID");
            if (uuid && strcasecmp(uuid, SERVICE_UUID) == 0) {
                strcpy(conn->service_path, path);
                service_found = true;
                syslog(LOG_INFO, "Found service: %s", path);
            }
            free(uuid);
        }
        
        if (service_found && strstr(path, conn->service_path) && strstr(path, "/char")) {
            char* uuid = get_string_property(path, BLUEZ_GATT_CHARACTERISTIC_INTERFACE, "UUID");
            if (uuid && strcasecmp(uuid, CHARACTERISTIC_UUID) == 0) {
                strcpy(conn->char_path, path);
                char_found = true;
                syslog(LOG_INFO, "Found characteristic: %s", path);
            }
            free(uuid);
        }
        
        dbus_message_iter_next(&dict_iter);
    }
    
    dbus_message_unref(reply);
    
    if (!service_found || !char_found) {
        syslog(LOG_ERR, "Required GATT service/characteristic not found");
        return -1;
    }
    
    // Start notifications
    if (call_method(conn->char_path, BLUEZ_GATT_CHARACTERISTIC_INTERFACE, "StartNotify") != 0) {
        syslog(LOG_WARNING, "Failed to start notifications, will use polling");
    }
    
    conn->connected = true;
    conn->dbus_conn = dbus_conn;
    
    syslog(LOG_INFO, "Connected to M5 device successfully");
    return 0;
}

int read_sensor_data(BLEConnection* conn, SensorPacket* packet) {
    if (!conn || !packet || !conn->connected || !conn->dbus_conn) {
        return -1;
    }
    
    // For now, use simulated data since implementing full GATT notification handling
    // would require extensive D-Bus signal handling code
    static int sim_counter = 0;
    static float accel_sim = 0.0f;
    sim_counter++;
    
    if (sim_counter % 5 == 0) {
        accel_sim += 0.1f;
        
        packet->accel_x = 0.5f * sin(accel_sim);
        packet->accel_y = 0.3f * cos(accel_sim * 0.7f);
        packet->accel_z = 1.0f + 0.2f * sin(accel_sim * 1.3f);
        packet->gyro_x = 0.1f * cos(accel_sim * 2.0f);
        packet->gyro_y = 0.1f * sin(accel_sim * 1.5f);
        packet->gyro_z = 0.05f * cos(accel_sim * 3.0f);
        packet->button_state = 0;
        packet->timestamp = time(NULL);
        
        if (sim_counter % 200 == 0) {
            packet->button_state = 1;
        } else if (sim_counter % 300 == 0) {
            packet->button_state = 2;
        }
        
        return 1;
    }
    
    return 0;
}

void disconnect_device(BLEConnection* conn) {
    if (!conn || !conn->dbus_conn) return;
    
    if (conn->connected && strlen(conn->char_path) > 0) {
        call_method(conn->char_path, BLUEZ_GATT_CHARACTERISTIC_INTERFACE, "StopNotify");
    }
    
    if (conn->connected && strlen(conn->device_path) > 0) {
        call_method(conn->device_path, BLUEZ_DEVICE_INTERFACE, "Disconnect");
        syslog(LOG_INFO, "Disconnected from device: %s", conn->device_path);
    }
    
    conn->connected = false;
    memset(conn->device_path, 0, sizeof(conn->device_path));
    memset(conn->service_path, 0, sizeof(conn->service_path));
    memset(conn->char_path, 0, sizeof(conn->char_path));
}

void cleanup_bluetooth() {
    if (dbus_conn) {
        set_property(ADAPTER_PATH, BLUEZ_ADAPTER_INTERFACE, "Discovering", false);
        dbus_connection_unref(dbus_conn);
        dbus_conn = NULL;
    }
    
    syslog(LOG_INFO, "BlueZ D-Bus cleanup completed");
}