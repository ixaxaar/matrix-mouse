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
#define ADAPTER_PATH "/org/bluez/hci0"

static DBusConnection* dbus_conn = NULL;

// Simplified D-Bus method call with better error handling
static int call_dbus_method(const char* path, const char* interface, const char* method) {
    DBusMessage* msg = dbus_message_new_method_call(BLUEZ_SERVICE, path, interface, method);
    if (!msg) {
        syslog(LOG_ERR, "Failed to create D-Bus message for %s", method);
        return -1;
    }

    DBusError error;
    dbus_error_init(&error);
    DBusMessage* reply = dbus_connection_send_with_reply_and_block(dbus_conn, msg, 5000, &error);
    dbus_message_unref(msg);

    int result = 0;
    if (dbus_error_is_set(&error)) {
        syslog(LOG_ERR, "D-Bus error calling %s: %s", method, error.message);
        dbus_error_free(&error);
        result = -1;
    }

    if (reply) dbus_message_unref(reply);
    return result;
}

// Simplified property setter
static int set_adapter_powered(bool powered) {
    DBusMessage* msg = dbus_message_new_method_call(
        BLUEZ_SERVICE, ADAPTER_PATH, "org.freedesktop.DBus.Properties", "Set");
    if (!msg) return -1;

    const char* interface = "org.bluez.Adapter1";
    const char* property = "Powered";

    DBusMessageIter iter, variant;
    dbus_message_iter_init_append(msg, &iter);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &interface);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &property);
    dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "b", &variant);
    dbus_bool_t value = powered;
    dbus_message_iter_append_basic(&variant, DBUS_TYPE_BOOLEAN, &value);
    dbus_message_iter_close_container(&iter, &variant);

    DBusMessage* reply = dbus_connection_send_with_reply_and_block(dbus_conn, msg, 5000, NULL);
    dbus_message_unref(msg);

    int result = reply ? 0 : -1;
    if (reply) dbus_message_unref(reply);
    return result;
}

// Simplified device finder
static bool find_m5_device(BLEConnection* conn) {
    DBusMessage* msg = dbus_message_new_method_call(
        BLUEZ_SERVICE, "/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
    if (!msg) return false;

    DBusMessage* reply = dbus_connection_send_with_reply_and_block(dbus_conn, msg, 10000, NULL);
    dbus_message_unref(msg);
    if (!reply) return false;

    DBusMessageIter iter, dict_iter, entry_iter;
    dbus_message_iter_init(reply, &iter);
    dbus_message_iter_recurse(&iter, &dict_iter);

    bool found = false;
    while (dbus_message_iter_get_arg_type(&dict_iter) == DBUS_TYPE_DICT_ENTRY && !found) {
        dbus_message_iter_recurse(&dict_iter, &entry_iter);

        char* path;
        dbus_message_iter_get_basic(&entry_iter, &path);

        // Only check device paths
        if (strstr(path, "/org/bluez/hci0/dev_")) {
            // Get device name property
            DBusMessage* prop_msg = dbus_message_new_method_call(
                BLUEZ_SERVICE, path, "org.freedesktop.DBus.Properties", "Get");
            if (prop_msg) {
                const char* interface = "org.bluez.Device1";
                const char* property = "Name";

                DBusMessageIter prop_iter;
                dbus_message_iter_init_append(prop_msg, &prop_iter);
                dbus_message_iter_append_basic(&prop_iter, DBUS_TYPE_STRING, &interface);
                dbus_message_iter_append_basic(&prop_iter, DBUS_TYPE_STRING, &property);

                DBusMessage* prop_reply = dbus_connection_send_with_reply_and_block(
                    dbus_conn, prop_msg, 5000, NULL);
                dbus_message_unref(prop_msg);

                if (prop_reply) {
                    DBusMessageIter reply_iter, variant_iter;
                    dbus_message_iter_init(prop_reply, &reply_iter);
                    dbus_message_iter_recurse(&reply_iter, &variant_iter);

                    if (dbus_message_iter_get_arg_type(&variant_iter) == DBUS_TYPE_STRING) {
                        char* name;
                        dbus_message_iter_get_basic(&variant_iter, &name);

                        if (name && (strstr(name, "M5") || strstr(name, "Mouse"))) {
                            strncpy(conn->device_path, path, sizeof(conn->device_path) - 1);
                            strncpy(conn->device_name, name, sizeof(conn->device_name) - 1);
                            found = true;
                            syslog(LOG_INFO, "Found M5 device: %s", name);
                        }
                    }
                    dbus_message_unref(prop_reply);
                }
            }
        }

        dbus_message_iter_next(&dict_iter);
    }

    dbus_message_unref(reply);
    return found;
}

int init_bluetooth() {
    syslog(LOG_INFO, "Initializing Bluetooth");

    DBusError error;
    dbus_error_init(&error);

    dbus_conn = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
    if (dbus_error_is_set(&error)) {
        syslog(LOG_ERR, "D-Bus connection failed: %s", error.message);
        dbus_error_free(&error);
        return -1;
    }

    if (set_adapter_powered(true) != 0) {
        syslog(LOG_WARNING, "Failed to power on adapter, continuing anyway");
    }

    return 0;
}

int scan_for_device(BLEConnection* conn) {
    if (!conn || !dbus_conn) return -1;

    syslog(LOG_INFO, "Scanning for M5 device...");

    // Start discovery
    if (call_dbus_method(ADAPTER_PATH, "org.bluez.Adapter1", "StartDiscovery") != 0) {
        syslog(LOG_ERR, "Failed to start discovery");
        return -1;
    }

    conn->scanning = true;
    sleep(5);  // Scan duration

    // Find device
    bool found = find_m5_device(conn);

    // Stop discovery
    call_dbus_method(ADAPTER_PATH, "org.bluez.Adapter1", "StopDiscovery");
    conn->scanning = false;

    return found ? 0 : -1;
}

int connect_to_device(BLEConnection* conn) {
    if (!conn || !dbus_conn || strlen(conn->device_path) == 0) {
        return -1;
    }

    syslog(LOG_INFO, "Connecting to device...");

    if (call_dbus_method(conn->device_path, "org.bluez.Device1", "Connect") != 0) {
        return -1;
    }

    sleep(2);  // Connection establishment time
    conn->connected = true;
    conn->dbus_conn = dbus_conn;

    syslog(LOG_INFO, "Connected successfully");
    return 0;
}

int read_sensor_data(BLEConnection* conn, SensorPacket* packet) {
    if (!conn || !packet || !conn->connected || !conn->dbus_conn) {
        return -1;
    }

    // Read actual BLE characteristic value
    if (strlen(conn->char_path) == 0) {
        // Need to find the characteristic path first
        syslog(LOG_ERR, "Characteristic path not set");
        return -1;
    }

    DBusMessage* msg = dbus_message_new_method_call(
        BLUEZ_SERVICE, conn->char_path,
        "org.freedesktop.DBus.Properties", "Get");
    if (!msg) return -1;

    const char* interface = "org.bluez.GattCharacteristic1";
    const char* property = "Value";

    DBusMessageIter iter;
    dbus_message_iter_init_append(msg, &iter);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &interface);
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &property);

    DBusError error;
    dbus_error_init(&error);
    DBusMessage* reply = dbus_connection_send_with_reply_and_block(
        conn->dbus_conn, msg, 5000, &error);
    dbus_message_unref(msg);

    if (dbus_error_is_set(&error)) {
        syslog(LOG_ERR, "Failed to read characteristic: %s", error.message);
        dbus_error_free(&error);
        return -1;
    }

    if (!reply) return -1;

    // Parse the byte array from D-Bus
    DBusMessageIter reply_iter, variant_iter, array_iter;
    dbus_message_iter_init(reply, &reply_iter);
    dbus_message_iter_recurse(&reply_iter, &variant_iter);

    if (dbus_message_iter_get_arg_type(&variant_iter) != DBUS_TYPE_ARRAY) {
        dbus_message_unref(reply);
        return -1;
    }

    dbus_message_iter_recurse(&variant_iter, &array_iter);

    uint8_t buffer[sizeof(SensorPacket)];
    int idx = 0;

    while (dbus_message_iter_get_arg_type(&array_iter) == DBUS_TYPE_BYTE &&
           idx < (int)sizeof(SensorPacket)) {
        dbus_message_iter_get_basic(&array_iter, &buffer[idx++]);
        dbus_message_iter_next(&array_iter);
    }

    dbus_message_unref(reply);

    if (idx == sizeof(SensorPacket)) {
        memcpy(packet, buffer, sizeof(SensorPacket));
        return 1;
    }

    return 0;
}

void disconnect_device(BLEConnection* conn) {
    if (!conn || !conn->dbus_conn) return;

    if (conn->connected && strlen(conn->device_path) > 0) {
        call_dbus_method(conn->device_path, "org.bluez.Device1", "Disconnect");
        syslog(LOG_INFO, "Disconnected from device");
    }

    conn->connected = false;
    memset(conn->device_path, 0, sizeof(conn->device_path));
    memset(conn->service_path, 0, sizeof(conn->service_path));
    memset(conn->char_path, 0, sizeof(conn->char_path));
}

void cleanup_bluetooth() {
    if (dbus_conn) {
        call_dbus_method(ADAPTER_PATH, "org.bluez.Adapter1", "StopDiscovery");
        dbus_connection_unref(dbus_conn);
        dbus_conn = NULL;
    }
    syslog(LOG_INFO, "Bluetooth cleanup completed");
}
