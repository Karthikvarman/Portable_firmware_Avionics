#ifndef HAL_WIFI_H
#define HAL_WIFI_H
#include "hal_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HAL_WIFI_MAX_SSID_LEN       32
#define HAL_WIFI_MAX_PASS_LEN       64
#define HAL_WIFI_MAX_BSSID_LEN       6
#define HAL_WIFI_MAX_SCAN_RESULTS    20
#define HAL_WIFI_BUFFER_SIZE         2048

typedef enum {
    HAL_WIFI_MODE_OFF = 0,
    HAL_WIFI_MODE_STA = 1,
    HAL_WIFI_MODE_AP = 2,
    HAL_WIFI_MODE_AP_STA = 3
} hal_wifi_mode_t;

typedef enum {
    HAL_WIFI_STATE_DISCONNECTED = 0,
    HAL_WIFI_STATE_CONNECTING = 1,
    HAL_WIFI_STATE_CONNECTED = 2,
    HAL_WIFI_STATE_DISCONNECTING = 3,
    HAL_WIFI_STATE_ERROR = 4
} hal_wifi_state_t;

typedef enum {
    HAL_WIFI_AUTH_OPEN = 0,
    HAL_WIFI_AUTH_WEP = 1,
    HAL_WIFI_AUTH_WPA_PSK = 2,
    HAL_WIFI_AUTH_WPA2_PSK = 3,
    HAL_WIFI_AUTH_WPA_WPA2_PSK = 4,
    HAL_WIFI_AUTH_WPA2_ENTERPRISE = 5,
    HAL_WIFI_AUTH_WPA3_PSK = 6,
    HAL_WIFI_AUTH_WPA2_WPA3_PSK = 7
} hal_wifi_auth_t;

typedef struct {
    char ssid[HAL_WIFI_MAX_SSID_LEN + 1];
    uint8_t bssid[HAL_WIFI_MAX_BSSID_LEN];
    int8_t rssi;
    uint8_t channel;
    hal_wifi_auth_t auth;
    uint32_t last_seen_ms;
} hal_wifi_scan_result_t;

typedef struct {
    char ssid[HAL_WIFI_MAX_SSID_LEN + 1];
    char password[HAL_WIFI_MAX_PASS_LEN + 1];
    uint8_t bssid[HAL_WIFI_MAX_BSSID_LEN];
    bool use_bssid;
    uint32_t ip_addr;
    uint32_t netmask;
    uint32_t gateway;
    uint32_t dns1;
    uint32_t dns2;
    int8_t rssi;
    uint32_t connect_time_ms;
} hal_wifi_sta_config_t;

typedef struct {
    char ssid[HAL_WIFI_MAX_SSID_LEN + 1];
    char password[HAL_WIFI_MAX_PASS_LEN + 1];
    uint8_t channel;
    hal_wifi_auth_t auth;
    uint32_t ip_addr;
    uint32_t netmask;
    uint32_t gateway;
    uint8_t max_connections;
    bool hidden;
} hal_wifi_ap_config_t;

typedef struct hal_wifi_s hal_wifi_t;

typedef struct {
    hal_status_t (*init)(hal_wifi_t *wifi);
    hal_status_t (*deinit)(hal_wifi_t *wifi);
    hal_status_t (*set_mode)(hal_wifi_t *wifi, hal_wifi_mode_t mode);
    hal_status_t (*get_state)(hal_wifi_t *wifi, hal_wifi_state_t *state);
    hal_status_t (*scan_start)(hal_wifi_t *wifi);
    hal_status_t (*scan_get_results)(hal_wifi_t *wifi, hal_wifi_scan_result_t *results, uint8_t *count);
    hal_status_t (*sta_connect)(hal_wifi_t *wifi, const hal_wifi_sta_config_t *config);
    hal_status_t (*sta_disconnect)(hal_wifi_t *wifi);
    hal_status_t (*sta_get_config)(hal_wifi_t *wifi, hal_wifi_sta_config_t *config);
    hal_status_t (*ap_start)(hal_wifi_t *wifi, const hal_wifi_ap_config_t *config);
    hal_status_t (*ap_stop)(hal_wifi_t *wifi);
    hal_status_t (*ap_get_config)(hal_wifi_t *wifi, hal_wifi_ap_config_t *config);
    hal_status_t (*get_mac)(hal_wifi_t *wifi, uint8_t *mac);
    hal_status_t (*set_mac)(hal_wifi_t *wifi, const uint8_t *mac);
    hal_status_t (*start_tcp_server)(hal_wifi_t *wifi, uint16_t port);
    hal_status_t (*stop_tcp_server)(hal_wifi_t *wifi);
    hal_status_t (*tcp_send)(hal_wifi_t *wifi, const uint8_t *data, size_t len);
    hal_status_t (*udp_send)(hal_wifi_t *wifi, const uint8_t *data, size_t len, const char *host, uint16_t port);
    hal_status_t (*set_data_callback)(hal_wifi_t *wifi, void (*callback)(const uint8_t *data, size_t len, void *context), void *context);
} hal_wifi_ops_t;

struct hal_wifi_s {
    const hal_wifi_ops_t *ops;
    hal_wifi_mode_t mode;
    hal_wifi_state_t state;
    hal_wifi_sta_config_t sta_config;
    hal_wifi_ap_config_t ap_config;
    hal_wifi_scan_result_t scan_results[HAL_WIFI_MAX_SCAN_RESULTS];
    uint8_t scan_count;
    uint8_t rx_buffer[HAL_WIFI_BUFFER_SIZE];
    size_t rx_len;
    bool initialized;
    void (*data_callback)(const uint8_t *data, size_t len, void *context);
    void *callback_context;
    void *platform_data;
};

hal_status_t hal_wifi_init(hal_wifi_t *wifi);
hal_status_t hal_wifi_deinit(hal_wifi_t *wifi);
hal_status_t hal_wifi_register_driver(hal_wifi_t *wifi, const hal_wifi_ops_t *ops);
hal_status_t hal_wifi_set_mode(hal_wifi_t *wifi, hal_wifi_mode_t mode);
hal_status_t hal_wifi_get_state(hal_wifi_t *wifi, hal_wifi_state_t *state);
hal_status_t hal_wifi_scan_networks(hal_wifi_t *wifi, hal_wifi_scan_result_t *results, uint8_t *count);
hal_status_t hal_wifi_sta_connect(hal_wifi_t *wifi, const char *ssid, const char *password);
hal_status_t hal_wifi_sta_disconnect(hal_wifi_t *wifi);
hal_status_t hal_wifi_ap_start(hal_wifi_t *wifi, const char *ssid, const char *password, uint8_t channel);
hal_status_t hal_wifi_ap_stop(hal_wifi_t *wifi);
hal_status_t hal_wifi_start_telemetry_server(hal_wifi_t *wifi, uint16_t port);
hal_status_t hal_wifi_send_telemetry(hal_wifi_t *wifi, const uint8_t *data, size_t len);
hal_status_t hal_wifi_set_data_callback(hal_wifi_t *wifi, void (*callback)(const uint8_t *data, size_t len, void *context), void *context);

#ifdef __cplusplus
}
#endif

#endif
