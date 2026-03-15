#ifndef WIRELESS_MANAGER_H
#define WIRELESS_MANAGER_H
#include "hal_wifi.h"
#include "hal_bt.h"
#include "rtos_tasks.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WIRELESS_MAX_SSID_LEN      32
#define WIRELESS_MAX_PASS_LEN      64
#define WIRELESS_MAX_DEVICES       10
#define WIRELESS_BUFFER_SIZE       2048

typedef enum {
    WIRELESS_CONN_NONE = 0,
    WIRELESS_CONN_WIFI_STA = 1,
    WIRELESS_CONN_WIFI_AP = 2,
    WIRELESS_CONN_BT = 3,
    WIRELESS_CONN_WIFI_BT = 4
} wireless_connection_type_t;

typedef enum {
    WIRELESS_STATE_DISCONNECTED = 0,
    WIRELESS_STATE_CONNECTING = 1,
    WIRELESS_STATE_CONNECTED = 2,
    WIRELESS_STATE_ERROR = 3
} wireless_state_t;

typedef struct {
    char ssid[WIRELESS_MAX_SSID_LEN + 1];
    char password[WIRELESS_MAX_PASS_LEN + 1];
    int8_t rssi;
    uint32_t connect_time_ms;
    bool active;
} wireless_wifi_network_t;

typedef struct {
    uint8_t addr[6];
    char name[32];
    int8_t rssi;
    bool connected;
    uint32_t connect_time_ms;
} wireless_bt_device_t;

typedef struct {
    wireless_state_t state;
    wireless_connection_type_t connection_type;
    
    wireless_wifi_network_t wifi_network;
    wireless_bt_device_t bt_device;
    
    hal_wifi_scan_result_t wifi_scan_results[WIRELESS_MAX_DEVICES];
    uint8_t wifi_scan_count;
    
    wireless_bt_device_t bt_scan_results[WIRELESS_MAX_DEVICES];
    uint8_t bt_scan_count;
    
    uint32_t last_update_ms;
    uint32_t update_interval_ms;
    
    bool auto_connect;
    bool auto_switch;
    bool telemetry_enabled;
    
    uint16_t telemetry_port;
    char telemetry_host[64];
    
} wireless_manager_t;

hal_status_t wireless_manager_init(void);
hal_status_t wireless_manager_deinit(void);

hal_status_t wireless_manager_start(void);
hal_status_t wireless_manager_stop(void);

hal_status_t wireless_manager_wifi_scan(wireless_wifi_network_t *networks, uint8_t *count);
hal_status_t wireless_manager_wifi_connect(const char *ssid, const char *password);
hal_status_t wireless_manager_wifi_disconnect(void);
hal_status_t wireless_manager_wifi_start_ap(const char *ssid, const char *password, uint8_t channel);
hal_status_t wireless_manager_wifi_stop_ap(void);

hal_status_t wireless_manager_bt_scan(wireless_bt_device_t *devices, uint8_t *count);
hal_status_t wireless_manager_bt_connect(const uint8_t *addr);
hal_status_t wireless_manager_bt_disconnect(void);
hal_status_t wireless_manager_bt_start_telemetry(void);
hal_status_t wireless_manager_bt_stop_telemetry(void);

hal_status_t wireless_manager_get_state(wireless_state_t *state);
hal_status_t wireless_manager_get_connection_type(wireless_connection_type_t *type);
hal_status_t wireless_manager_get_wifi_status(wireless_wifi_network_t *network);
hal_status_t wireless_manager_get_bt_status(wireless_bt_device_t *device);

hal_status_t wireless_manager_set_auto_connect(bool enable);
hal_status_t wireless_manager_set_auto_switch(bool enable);
hal_status_t wireless_manager_set_telemetry_enabled(bool enable);

hal_status_t wireless_manager_send_telemetry(const uint8_t *data, size_t len);
hal_status_t wireless_manager_send_command(const char *command);

void wireless_manager_wifi_data_callback(const uint8_t *data, size_t len, void *context);
void wireless_manager_bt_data_callback(const uint8_t *data, size_t len, void *context);
void wireless_manager_bt_device_callback(const hal_bt_device_t *device, bool discovered, void *context);

#ifdef __cplusplus
}
#endif

#endif
