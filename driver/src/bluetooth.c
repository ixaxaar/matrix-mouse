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

// Add notification handler
static DBusHandlerResult notification_handler(DBusConnection* conn, DBusMessage* msg, void* user_data) {
    (void)conn;
    BLEConnection* ble_conn = (BLEConnection*)user_data;

    // Monitor device connection state
    if (dbus_message_is_signal(msg, "org.freedesktop.DBus.Properties", "PropertiesChanged")) {
        const char* path = dbus_message_get_path(msg);

        // Check if this is our device's connection state
        if (path && strcmp(path, ble_conn->device_path) == 0) {
            DBusMessageIter iter, dict_iter, entry_iter, variant_iter;
            dbus_message_iter_init(msg, &iter);
            dbus_message_iter_next(&iter);
            dbus_message_iter_recurse(&iter, &dict_iter);

            while (dbus_message_iter_get_arg_type(&dict_iter) == DBUS_TYPE_DICT_ENTRY) {
                dbus_message_iter_recurse(&dict_iter, &entry_iter);
                char* prop_name;
                dbus_message_iter_get_basic(&entry_iter, &prop_name);

                if (strcmp(prop_name, "Connected") == 0) {
                    dbus_message_iter_next(&entry_iter);
                    dbus_message_iter_recurse(&entry_iter, &variant_iter);
                    dbus_bool_t connected;
                    dbus_message_iter_get_basic(&variant_iter, &connected);

                    if (!connected && ble_conn->connected) {
                        syslog(LOG_INFO, "Disconnected from device");
                        ble_conn->connected = false;
                    }
                }

                dbus_message_iter_next(&dict_iter);
            }
        }
    }

    if (dbus_message_is_signal(msg, "org.freedesktop.DBus.Properties", "PropertiesChanged")) {
        const char* path = dbus_message_get_path(msg);

        // Check if this is our characteristic
        if (path && strcmp(path, ble_conn->char_path) == 0) {

            DBusMessageIter iter, dict_iter, entry_iter, variant_iter, array_iter;
            dbus_message_iter_init(msg, &iter);

            // Skip interface name
            dbus_message_iter_next(&iter);

            // Get changed properties dict
            dbus_message_iter_recurse(&iter, &dict_iter);

            while (dbus_message_iter_get_arg_type(&dict_iter) == DBUS_TYPE_DICT_ENTRY) {
                dbus_message_iter_recurse(&dict_iter, &entry_iter);

                char* prop_name;
                dbus_message_iter_get_basic(&entry_iter, &prop_name);

                if (strcmp(prop_name, "Value") == 0) {
                    dbus_message_iter_next(&entry_iter);
                    dbus_message_iter_recurse(&entry_iter, &variant_iter);

                    if (dbus_message_iter_get_arg_type(&variant_iter) == DBUS_TYPE_ARRAY) {
                        dbus_message_iter_recurse(&variant_iter, &array_iter);

                        uint8_t buffer[sizeof(SensorPacket)];
                        int idx = 0;

                        while (dbus_message_iter_get_arg_type(&array_iter) == DBUS_TYPE_BYTE &&
                               idx < (int)sizeof(SensorPacket)) {
                            dbus_message_iter_get_basic(&array_iter, &buffer[idx++]);
                            dbus_message_iter_next(&array_iter);
                        }

                        if (idx == sizeof(SensorPacket)) {
                            memcpy(&ble_conn->last_packet, buffer, sizeof(SensorPacket));
                            ble_conn->packet_ready = true;
                            // syslog(LOG_DEBUG, "Full packet received: %d bytes, packet_ready=%d",
                            //        idx, ble_conn->packet_ready);
                        } else if (idx > 0) {
                            syslog(LOG_INFO, "Received partial packet: %d bytes (expected %zu)",
                                   idx, sizeof(SensorPacket));
                        }
                    }
                }

                dbus_message_iter_next(&dict_iter);
            }
        }
    }

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
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

    // Discover services and characteristics
    DBusMessage* msg = dbus_message_new_method_call(
        BLUEZ_SERVICE, "/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
    if (!msg) return -1;

    DBusMessage* reply = dbus_connection_send_with_reply_and_block(dbus_conn, msg, 10000, NULL);
    dbus_message_unref(msg);
    if (!reply) return -1;

    DBusMessageIter iter, dict_iter, entry_iter, iface_dict_iter, iface_entry_iter;
    dbus_message_iter_init(reply, &iter);
    dbus_message_iter_recurse(&iter, &dict_iter);

    bool found_char = false;

    while (dbus_message_iter_get_arg_type(&dict_iter) == DBUS_TYPE_DICT_ENTRY && !found_char) {
        dbus_message_iter_recurse(&dict_iter, &entry_iter);

        char* path;
        dbus_message_iter_get_basic(&entry_iter, &path);

        if (strstr(path, conn->device_path)) {
            dbus_message_iter_next(&entry_iter);
            dbus_message_iter_recurse(&entry_iter, &iface_dict_iter);

            while (dbus_message_iter_get_arg_type(&iface_dict_iter) == DBUS_TYPE_DICT_ENTRY) {
                dbus_message_iter_recurse(&iface_dict_iter, &iface_entry_iter);

                char* interface;
                dbus_message_iter_get_basic(&iface_entry_iter, &interface);

                if (strcmp(interface, "org.bluez.GattCharacteristic1") == 0) {
                    dbus_message_iter_next(&iface_entry_iter);
                    DBusMessageIter props_dict_iter, props_entry_iter;
                    dbus_message_iter_recurse(&iface_entry_iter, &props_dict_iter);

                    while (dbus_message_iter_get_arg_type(&props_dict_iter) == DBUS_TYPE_DICT_ENTRY) {
                        dbus_message_iter_recurse(&props_dict_iter, &props_entry_iter);

                        char* prop_name;
                        dbus_message_iter_get_basic(&props_entry_iter, &prop_name);

                        if (strcmp(prop_name, "UUID") == 0) {
                            dbus_message_iter_next(&props_entry_iter);
                            DBusMessageIter variant_iter;
                            dbus_message_iter_recurse(&props_entry_iter, &variant_iter);

                            char* uuid;
                            dbus_message_iter_get_basic(&variant_iter, &uuid);

                            if (strcasecmp(uuid, CHARACTERISTIC_UUID) == 0) {
                                strncpy(conn->char_path, path, sizeof(conn->char_path) - 1);
                                found_char = true;
                                syslog(LOG_INFO, "Found characteristic at: %s", path);
                                break;
                            }
                        }

                        dbus_message_iter_next(&props_dict_iter);
                    }
                }

                if (found_char) break;
                dbus_message_iter_next(&iface_dict_iter);
            }
        }

        if (found_char) break;
        dbus_message_iter_next(&dict_iter);
    }

    dbus_message_unref(reply);

    if (!found_char) {
        syslog(LOG_ERR, "Failed to find characteristic with UUID: %s", CHARACTERISTIC_UUID);
        call_dbus_method(conn->device_path, "org.bluez.Device1", "Disconnect");
        return -1;
    }

    // Enable notifications
    DBusError error;
    dbus_error_init(&error);
    DBusMessage* notify_msg = dbus_message_new_method_call(
        BLUEZ_SERVICE, conn->char_path, "org.bluez.GattCharacteristic1", "StartNotify");
    if (notify_msg) {
        DBusMessage* notify_reply = dbus_connection_send_with_reply_and_block(
            dbus_conn, notify_msg, 5000, &error);
        dbus_message_unref(notify_msg);
        if (dbus_error_is_set(&error)) {
            syslog(LOG_ERR, "Failed to enable notifications: %s", error.message);
            dbus_error_free(&error);
        } else if (notify_reply) {
            dbus_message_unref(notify_reply);
            syslog(LOG_INFO, "Notifications enabled successfully");
        } else {
            syslog(LOG_WARNING, "Failed to enable notifications (no reply)");
        }
    }

    // Add signal match rules for notifications and device state
    char match_rule[512];
    snprintf(match_rule, sizeof(match_rule),
             "type='signal',interface='org.freedesktop.DBus.Properties',"
             "member='PropertiesChanged',path='%s'", conn->char_path);
    dbus_bus_add_match(dbus_conn, match_rule, NULL);

    // Monitor device connection state
    snprintf(match_rule, sizeof(match_rule),
             "type='signal',interface='org.freedesktop.DBus.Properties',"
             "member='PropertiesChanged',path='%s'", conn->device_path);
    dbus_bus_add_match(dbus_conn, match_rule, NULL);

    // Register message handler
    dbus_connection_add_filter(dbus_conn, notification_handler, conn, NULL);

    conn->connected = true;
    conn->dbus_conn = dbus_conn;
    conn->packet_ready = false;

    syslog(LOG_INFO, "Connected successfully with characteristic path set");
    return 0;
}

int read_sensor_data(BLEConnection* conn, SensorPacket* packet) {
    if (!conn || !packet || !conn->connected || !conn->dbus_conn) {
        syslog(LOG_ERR, "read_sensor_data: invalid params");
        return -1;
    }

    if (strlen(conn->char_path) == 0) {
        syslog(LOG_ERR, "Characteristic path not set");
        return -1;
    }

    // Use timeout of 0 to make it non-blocking and allow signal handling
    dbus_connection_read_write_dispatch(conn->dbus_conn, 0);

    // Check if we have a new packet
    if (conn->packet_ready) {
        memcpy(packet, &conn->last_packet, sizeof(SensorPacket));
        conn->packet_ready = false;
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
