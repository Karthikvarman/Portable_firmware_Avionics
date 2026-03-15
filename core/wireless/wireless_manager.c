#include "wireless_manager.h"
#include "hal_timer.h"
#include "os_api.h"
#include <string.h>
#include <stdlib.h>

static wireless_manager_t g_wireless_mgr = {0};
static os_mutex_handle_t g_wireless_mutex = NULL;

hal_status_t wireless_manager_init(void) {
    memset(&g_wireless_mgr, 0, sizeof(wireless_manager_t));
    
    g_wireless_mutex = os_mutex_create();
    if (!g_wireless_mutex) return HAL_IO_ERROR;
    
    g_wireless_mgr.state = WIRELESS_STATE_DISCONNECTED;
    g_wireless_mgr.connection_type = WIRELESS_CONN_NONE;
    g_wireless_mgr.update_interval_ms = 1000;
    g_wireless_mgr.auto_connect = true;
    g_wireless_mgr.auto_switch = true;
    g_wireless_mgr.telemetry_enabled = true;
    g_wireless_mgr.telemetry_port = 8001;
    strcpy(g_wireless_mgr.telemetry_host, "192.168.1.100");
    
    hal_status_t status = rtos_tasks_init();
    if (status != HAL_OK) {
        os_mutex_delete(g_wireless_mutex);
        return status;
    }
    
    return HAL_OK;
}

hal_status_t wireless_manager_deinit(void) {
    wireless_manager_stop();
    rtos_tasks_deinit();
    
    if (g_wireless_mutex) {
        os_mutex_delete(g_wireless_mutex);
        g_wireless_mutex = NULL;
    }
    
    memset(&g_wireless_mgr, 0, sizeof(wireless_manager_t));
    
    return HAL_OK;
}

hal_status_t wireless_manager_start(void) {
    os_mutex_lock(g_wireless_mutex, OS_WAIT_FOREVER);
    
    hal_status_t status = rtos_wifi_task_start();
    if (status != HAL_OK) {
        os_mutex_unlock(g_wireless_mutex);
        return status;
    }
    
    status = rtos_bt_task_start();
    if (status != HAL_OK) {
        rtos_wifi_task_stop();
        os_mutex_unlock(g_wireless_mutex);
        return status;
    }
    
    status = rtos_telemetry_task_start();
    if (status != HAL_OK) {
        rtos_wifi_task_stop();
        rtos_bt_task_stop();
        os_mutex_unlock(g_wireless_mutex);
        return status;
    }
    
    hal_wifi_set_data_callback(&g_task_mgr.wifi, wireless_manager_wifi_data_callback, &g_wireless_mgr);
    hal_bt_set_data_callback(&g_task_mgr.bt, wireless_manager_bt_data_callback, &g_wireless_mgr);
    hal_bt_set_device_callback(&g_task_mgr.bt, wireless_manager_bt_device_callback, &g_wireless_mgr);
    
    os_mutex_unlock(g_wireless_mutex);
    
    return HAL_OK;
}

hal_status_t wireless_manager_stop(void) {
    os_mutex_lock(g_wireless_mutex, OS_WAIT_FOREVER);
    
    rtos_telemetry_task_stop();
    rtos_wifi_task_stop();
    rtos_bt_task_stop();
    
    g_wireless_mgr.state = WIRELESS_STATE_DISCONNECTED;
    g_wireless_mgr.connection_type = WIRELESS_CONN_NONE;
    
    os_mutex_unlock(g_wireless_mutex);
    
    return HAL_OK;
}

hal_status_t wireless_manager_wifi_scan(wireless_wifi_network_t *networks, uint8_t *count) {
    HAL_CHECK_PTR(networks);
    HAL_CHECK_PTR(count);
    
    os_mutex_lock(g_wireless_mutex, OS_WAIT_FOREVER);
    
    hal_wifi_scan_result_t results[WIRELESS_MAX_DEVICES];
    uint8_t result_count = 0;
    
    hal_status_t status = hal_wifi_scan_networks(&g_task_mgr.wifi, results, &result_count);
    if (status == HAL_OK) {
        for (uint8_t i = 0; i < result_count && i < WIRELESS_MAX_DEVICES; i++) {
            strncpy(networks[i].ssid, results[i].ssid, WIRELESS_MAX_SSID_LEN);
            networks[i].ssid[WIRELESS_MAX_SSID_LEN] = '\0';
            networks[i].rssi = results[i].rssi;
            networks[i].connect_time_ms = 0;
            networks[i].active = false;
        }
        *count = result_count;
    }
    
    os_mutex_unlock(g_wireless_mutex);
    
    return status;
}

hal_status_t wireless_manager_wifi_connect(const char *ssid, const char *password) {
    HAL_CHECK_PTR(ssid);
    
    os_mutex_lock(g_wireless_mutex, OS_WAIT_FOREVER);
    
    g_wireless_mgr.state = WIRELESS_STATE_CONNECTING;
    
    hal_status_t status = rtos_wifi_connect(ssid, password);
    if (status == HAL_OK) {
        strncpy(g_wireless_mgr.wifi_network.ssid, ssid, WIRELESS_MAX_SSID_LEN);
        g_wireless_mgr.wifi_network.ssid[WIRELESS_MAX_SSID_LEN] = '\0';
        
        if (password) {
            strncpy(g_wireless_mgr.wifi_network.password, password, WIRELESS_MAX_PASS_LEN);
            g_wireless_mgr.wifi_network.password[WIRELESS_MAX_PASS_LEN] = '\0';
        }
        
        g_wireless_mgr.wifi_network.connect_time_ms = hal_timer_get_ms();
        g_wireless_mgr.wifi_network.active = true;
        
        g_wireless_mgr.connection_type = WIRELESS_CONN_WIFI_STA;
        g_wireless_mgr.state = WIRELESS_STATE_CONNECTED;
        
        if (g_wireless_mgr.telemetry_enabled) {
            rtos_telemetry_set_mode(RTOS_TELEMETRY_MODE_WIFI);
        }
    } else {
        g_wireless_mgr.state = WIRELESS_STATE_ERROR;
    }
    
    os_mutex_unlock(g_wireless_mutex);
    
    return status;
}

hal_status_t wireless_manager_wifi_disconnect(void) {
    os_mutex_lock(g_wireless_mutex, OS_WAIT_FOREVER);
    
    hal_status_t status = rtos_wifi_connect(NULL, NULL);
    
    memset(&g_wireless_mgr.wifi_network, 0, sizeof(wireless_wifi_network_t));
    g_wireless_mgr.state = WIRELESS_STATE_DISCONNECTED;
    
    if (g_wireless_mgr.connection_type == WIRELESS_CONN_WIFI_STA) {
        g_wireless_mgr.connection_type = WIRELESS_CONN_NONE;
        if (g_wireless_mgr.auto_switch && g_wireless_mgr.bt_device.connected) {
            g_wireless_mgr.connection_type = WIRELESS_CONN_BT;
            rtos_telemetry_set_mode(RTOS_TELEMETRY_MODE_BT);
        } else {
            rtos_telemetry_set_mode(RTOS_TELEMETRY_MODE_USB);
        }
    }
    
    os_mutex_unlock(g_wireless_mutex);
    
    return status;
}

hal_status_t wireless_manager_wifi_start_ap(const char *ssid, const char *password, uint8_t channel) {
    HAL_CHECK_PTR(ssid);
    
    os_mutex_lock(g_wireless_mutex, OS_WAIT_FOREVER);
    
    hal_status_t status = rtos_wifi_start_ap(ssid, password, channel);
    if (status == HAL_OK) {
        strncpy(g_wireless_mgr.wifi_network.ssid, ssid, WIRELESS_MAX_SSID_LEN);
        g_wireless_mgr.wifi_network.ssid[WIRELESS_MAX_SSID_LEN] = '\0';
        
        if (password) {
            strncpy(g_wireless_mgr.wifi_network.password, password, WIRELESS_MAX_PASS_LEN);
            g_wireless_mgr.wifi_network.password[WIRELESS_MAX_PASS_LEN] = '\0';
        }
        
        g_wireless_mgr.wifi_network.connect_time_ms = hal_timer_get_ms();
        g_wireless_mgr.wifi_network.active = true;
        
        g_wireless_mgr.connection_type = WIRELESS_CONN_WIFI_AP;
        g_wireless_mgr.state = WIRELESS_STATE_CONNECTED;
        
        if (g_wireless_mgr.telemetry_enabled) {
            rtos_telemetry_set_mode(RTOS_TELEMETRY_MODE_WIFI);
        }
    } else {
        g_wireless_mgr.state = WIRELESS_STATE_ERROR;
    }
    
    os_mutex_unlock(g_wireless_mutex);
    
    return status;
}

hal_status_t wireless_manager_wifi_stop_ap(void) {
    os_mutex_lock(g_wireless_mutex, OS_WAIT_FOREVER);
    
    hal_status_t status = rtos_wifi_connect(NULL, NULL);
    
    memset(&g_wireless_mgr.wifi_network, 0, sizeof(wireless_wifi_network_t));
    g_wireless_mgr.state = WIRELESS_STATE_DISCONNECTED;
    
    if (g_wireless_mgr.connection_type == WIRELESS_CONN_WIFI_AP) {
        g_wireless_mgr.connection_type = WIRELESS_CONN_NONE;
        rtos_telemetry_set_mode(RTOS_TELEMETRY_MODE_USB);
    }
    
    os_mutex_unlock(g_wireless_mutex);
    
    return status;
}

hal_status_t wireless_manager_bt_scan(wireless_bt_device_t *devices, uint8_t *count) {
    HAL_CHECK_PTR(devices);
    HAL_CHECK_PTR(count);
    
    os_mutex_lock(g_wireless_mutex, OS_WAIT_FOREVER);
    
    hal_bt_device_t results[WIRELESS_MAX_DEVICES];
    uint8_t result_count = 0;
    
    hal_status_t status = hal_bt_scan_devices(&g_task_mgr.bt, results, &result_count);
    if (status == HAL_OK) {
        for (uint8_t i = 0; i < result_count && i < WIRELESS_MAX_DEVICES; i++) {
            memcpy(devices[i].addr, results[i].addr, 6);
            strncpy(devices[i].name, results[i].name, 31);
            devices[i].name[31] = '\0';
            devices[i].rssi = results[i].rssi;
            devices[i].connected = false;
            devices[i].connect_time_ms = 0;
        }
        *count = result_count;
    }
    
    os_mutex_unlock(g_wireless_mutex);
    
    return status;
}

hal_status_t wireless_manager_bt_connect(const uint8_t *addr) {
    HAL_CHECK_PTR(addr);
    
    os_mutex_lock(g_wireless_mutex, OS_WAIT_FOREVER);
    
    g_wireless_mgr.state = WIRELESS_STATE_CONNECTING;
    
    hal_status_t status = hal_bt_connect_device(&g_task_mgr.bt, addr);
    if (status == HAL_OK) {
        memcpy(g_wireless_mgr.bt_device.addr, addr, 6);
        g_wireless_mgr.bt_device.connect_time_ms = hal_timer_get_ms();
        g_wireless_mgr.bt_device.connected = true;
        
        g_wireless_mgr.connection_type = WIRELESS_CONN_BT;
        g_wireless_mgr.state = WIRELESS_STATE_CONNECTED;
        
        if (g_wireless_mgr.telemetry_enabled) {
            rtos_telemetry_set_mode(RTOS_TELEMETRY_MODE_BT);
        }
    } else {
        g_wireless_mgr.state = WIRELESS_STATE_ERROR;
    }
    
    os_mutex_unlock(g_wireless_mutex);
    
    return status;
}

hal_status_t wireless_manager_bt_disconnect(void) {
    os_mutex_lock(g_wireless_mutex, OS_WAIT_FOREVER);
    
    hal_status_t status = hal_bt_disconnect(&g_task_mgr.bt);
    
    memset(&g_wireless_mgr.bt_device, 0, sizeof(wireless_bt_device_t));
    g_wireless_mgr.state = WIRELESS_STATE_DISCONNECTED;
    
    if (g_wireless_mgr.connection_type == WIRELESS_CONN_BT) {
        g_wireless_mgr.connection_type = WIRELESS_CONN_NONE;
        if (g_wireless_mgr.auto_switch && g_wireless_mgr.wifi_network.active) {
            g_wireless_mgr.connection_type = WIRELESS_CONN_WIFI_STA;
            rtos_telemetry_set_mode(RTOS_TELEMETRY_MODE_WIFI);
        } else {
            rtos_telemetry_set_mode(RTOS_TELEMETRY_MODE_USB);
        }
    }
    
    os_mutex_unlock(g_wireless_mutex);
    
    return status;
}

hal_status_t wireless_manager_bt_start_telemetry(void) {
    os_mutex_lock(g_wireless_mutex, OS_WAIT_FOREVER);
    
    hal_status_t status = rtos_bt_start_telemetry_service();
    
    os_mutex_unlock(g_wireless_mutex);
    
    return status;
}

hal_status_t wireless_manager_bt_stop_telemetry(void) {
    os_mutex_lock(g_wireless_mutex, OS_WAIT_FOREVER);
    
    hal_status_t status = hal_bt_disconnect(&g_task_mgr.bt);
    
    os_mutex_unlock(g_wireless_mutex);
    
    return status;
}

hal_status_t wireless_manager_get_state(wireless_state_t *state) {
    HAL_CHECK_PTR(state);
    
    os_mutex_lock(g_wireless_mutex, OS_WAIT_FOREVER);
    *state = g_wireless_mgr.state;
    os_mutex_unlock(g_wireless_mutex);
    
    return HAL_OK;
}

hal_status_t wireless_manager_get_connection_type(wireless_connection_type_t *type) {
    HAL_CHECK_PTR(type);
    
    os_mutex_lock(g_wireless_mutex, OS_WAIT_FOREVER);
    *type = g_wireless_mgr.connection_type;
    os_mutex_unlock(g_wireless_mutex);
    
    return HAL_OK;
}

hal_status_t wireless_manager_get_wifi_status(wireless_wifi_network_t *network) {
    HAL_CHECK_PTR(network);
    
    os_mutex_lock(g_wireless_mutex, OS_WAIT_FOREVER);
    *network = g_wireless_mgr.wifi_network;
    os_mutex_unlock(g_wireless_mutex);
    
    return HAL_OK;
}

hal_status_t wireless_manager_get_bt_status(wireless_bt_device_t *device) {
    HAL_CHECK_PTR(device);
    
    os_mutex_lock(g_wireless_mutex, OS_WAIT_FOREVER);
    *device = g_wireless_mgr.bt_device;
    os_mutex_unlock(g_wireless_mutex);
    
    return HAL_OK;
}

hal_status_t wireless_manager_set_auto_connect(bool enable) {
    os_mutex_lock(g_wireless_mutex, OS_WAIT_FOREVER);
    g_wireless_mgr.auto_connect = enable;
    os_mutex_unlock(g_wireless_mutex);
    
    return HAL_OK;
}

hal_status_t wireless_manager_set_auto_switch(bool enable) {
    os_mutex_lock(g_wireless_mutex, OS_WAIT_FOREVER);
    g_wireless_mgr.auto_switch = enable;
    os_mutex_unlock(g_wireless_mutex);
    
    return HAL_OK;
}

hal_status_t wireless_manager_set_telemetry_enabled(bool enable) {
    os_mutex_lock(g_wireless_mutex, OS_WAIT_FOREVER);
    g_wireless_mgr.telemetry_enabled = enable;
    os_mutex_unlock(g_wireless_mutex);
    
    return HAL_OK;
}

hal_status_t wireless_manager_send_telemetry(const uint8_t *data, size_t len) {
    HAL_CHECK_PTR(data);
    
    if (!g_wireless_mgr.telemetry_enabled) return HAL_NOT_INIT;
    
    return rtos_telemetry_send_data(data, len);
}

hal_status_t wireless_manager_send_command(const char *command) {
    HAL_CHECK_PTR(command);
    
    return wireless_manager_send_telemetry((const uint8_t *)command, strlen(command));
}

void wireless_manager_wifi_data_callback(const uint8_t *data, size_t len, void *context) {
    wireless_manager_t *mgr = (wireless_manager_t *)context;
    
    if (len > 0 && data[0] == '$') {
        wireless_manager_send_telemetry(data, len);
    }
}

void wireless_manager_bt_data_callback(const uint8_t *data, size_t len, void *context) {
    wireless_manager_t *mgr = (wireless_manager_t *)context;
    
    if (len > 0 && data[0] == '$') {
        wireless_manager_send_telemetry(data, len);
    }
}

void wireless_manager_bt_device_callback(const hal_bt_device_t *device, bool discovered, void *context) {
    wireless_manager_t *mgr = (wireless_manager_t *)context;
    
    if (discovered && mgr->auto_connect) {
        for (uint8_t i = 0; i < mgr->bt_scan_count; i++) {
            if (memcmp(mgr->bt_scan_results[i].addr, device->addr, 6) == 0) {
                if (strcmp(mgr->bt_scan_results[i].name, "AeroFirmware") == 0) {
                    wireless_manager_bt_connect(device->addr);
                    break;
                }
            }
        }
    }
}
