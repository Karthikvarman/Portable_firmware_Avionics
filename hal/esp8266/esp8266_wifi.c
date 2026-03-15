#include "hal_wifi.h"
#include "hal_timer.h"
#include <string.h>
#include <stdlib.h>

#ifdef TARGET_ESP8266
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "lwip/sockets.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

typedef struct {
    esp_netif_t *sta_netif;
    esp_netif_t *ap_netif;
    EventGroupHandle_t wifi_event_group;
    int server_socket;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len;
    bool server_running;
    TaskHandle_t server_task;
} esp8266_wifi_platform_t;
#endif

typedef struct {
    bool initialized;
    hal_wifi_mode_t mode;
    hal_wifi_state_t state;
    uint8_t rx_buffer[HAL_WIFI_BUFFER_SIZE];
    size_t rx_len;
    void (*data_callback)(const uint8_t *data, size_t len, void *context);
    void *callback_context;
} esp8266_wifi_platform_t;

static hal_status_t esp8266_wifi_init(hal_wifi_t *wifi);
static hal_status_t esp8266_wifi_deinit(hal_wifi_t *wifi);
static hal_status_t esp8266_wifi_set_mode(hal_wifi_t *wifi, hal_wifi_mode_t mode);
static hal_status_t esp8266_wifi_get_state(hal_wifi_t *wifi, hal_wifi_state_t *state);
static hal_status_t esp8266_wifi_scan_start(hal_wifi_t *wifi);
static hal_status_t esp8266_wifi_scan_get_results(hal_wifi_t *wifi, hal_wifi_scan_result_t *results, uint8_t *count);
static hal_status_t esp8266_wifi_sta_connect(hal_wifi_t *wifi, const hal_wifi_sta_config_t *config);
static hal_status_t esp8266_wifi_sta_disconnect(hal_wifi_t *wifi);
static hal_status_t esp8266_wifi_ap_start(hal_wifi_t *wifi, const hal_wifi_ap_config_t *config);
static hal_status_t esp8266_wifi_ap_stop(hal_wifi_t *wifi);
static hal_status_t esp8266_wifi_start_tcp_server(hal_wifi_t *wifi, uint16_t port);
static hal_status_t esp8266_wifi_stop_tcp_server(hal_wifi_t *wifi);
static hal_status_t esp8266_wifi_tcp_send(hal_wifi_t *wifi, const uint8_t *data, size_t len);
static hal_status_t esp8266_wifi_set_data_callback(hal_wifi_t *wifi, void (*callback)(const uint8_t *data, size_t len, void *context), void *context);
static hal_status_t esp8266_wifi_get_mac(hal_wifi_t *wifi, uint8_t *mac);

static const hal_wifi_ops_t esp8266_wifi_ops = {
    .init = esp8266_wifi_init,
    .deinit = esp8266_wifi_deinit,
    .set_mode = esp8266_wifi_set_mode,
    .get_state = esp8266_wifi_get_state,
    .scan_start = esp8266_wifi_scan_start,
    .scan_get_results = esp8266_wifi_scan_get_results,
    .sta_connect = esp8266_wifi_sta_connect,
    .sta_disconnect = esp8266_wifi_sta_disconnect,
    .ap_start = esp8266_wifi_ap_start,
    .ap_stop = esp8266_wifi_ap_stop,
    .start_tcp_server = esp8266_wifi_start_tcp_server,
    .stop_tcp_server = esp8266_wifi_stop_tcp_server,
    .tcp_send = esp8266_wifi_tcp_send,
    .set_data_callback = esp8266_wifi_set_data_callback,
    .get_mac = esp8266_wifi_get_mac
};

hal_status_t hal_esp8266_wifi_register(hal_wifi_t *wifi) {
    HAL_CHECK_PTR(wifi);
    return hal_wifi_register_driver(wifi, &esp8266_wifi_ops);
}

static hal_status_t esp8266_wifi_init(hal_wifi_t *wifi) {
    HAL_CHECK_PTR(wifi);
    
    esp8266_wifi_platform_t *platform = malloc(sizeof(esp8266_wifi_platform_t));
    if (!platform) return HAL_IO_ERROR;
    
    memset(platform, 0, sizeof(esp8266_wifi_platform_t));
    wifi->platform_data = platform;
    
#ifdef TARGET_ESP8266
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK) return HAL_IO_ERROR;
    
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) return HAL_IO_ERROR;
    
    platform->sta_netif = esp_netif_create_default_wifi_sta();
    platform->ap_netif = esp_netif_create_default_wifi_ap();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) return HAL_IO_ERROR;
    
    platform->wifi_event_group = xEventGroupCreate();
    if (!platform->wifi_event_group) return HAL_IO_ERROR;
#endif
    
    platform->initialized = true;
    wifi->mode = HAL_WIFI_MODE_OFF;
    wifi->state = HAL_WIFI_STATE_OFF;
    
    return HAL_OK;
}

static hal_status_t esp8266_wifi_deinit(hal_wifi_t *wifi) {
    HAL_CHECK_PTR(wifi);
    
    esp8266_wifi_platform_t *platform = (esp8266_wifi_platform_t *)wifi->platform_data;
    if (!platform) return HAL_NOT_INIT;
    
#ifdef TARGET_ESP8266
    esp_wifi_deinit();
    esp_netif_destroy(platform->sta_netif);
    esp_netif_destroy(platform->ap_netif);
    vEventGroupDelete(platform->wifi_event_group);
#endif
    
    free(platform);
    wifi->platform_data = NULL;
    platform->initialized = false;
    
    return HAL_OK;
}

static hal_status_t esp8266_wifi_set_mode(hal_wifi_t *wifi, hal_wifi_mode_t mode) {
    HAL_CHECK_PTR(wifi);
    
    esp8266_wifi_platform_t *platform = (esp8266_wifi_platform_t *)wifi->platform_data;
    if (!platform || !platform->initialized) return HAL_NOT_INIT;
    
#ifdef TARGET_ESP8266
    wifi_mode_t esp_mode;
    switch (mode) {
        case HAL_WIFI_MODE_OFF:
            esp_mode = WIFI_MODE_NULL;
            break;
        case HAL_WIFI_MODE_STA:
            esp_mode = WIFI_MODE_STA;
            break;
        case HAL_WIFI_MODE_AP:
            esp_mode = WIFI_MODE_AP;
            break;
        case HAL_WIFI_MODE_AP_STA:
            esp_mode = WIFI_MODE_APSTA;
            break;
        default:
            return HAL_INVALID_PARAM;
    }
    
    esp_err_t ret = esp_wifi_set_mode(esp_mode);
    if (ret != ESP_OK) return HAL_IO_ERROR;
    
    if (mode != HAL_WIFI_MODE_OFF) {
        ret = esp_wifi_start();
        if (ret != ESP_OK) return HAL_IO_ERROR;
    } else {
        ret = esp_wifi_stop();
        if (ret != ESP_OK) return HAL_IO_ERROR;
    }
#endif
    
    wifi->mode = mode;
    return HAL_OK;
}

static hal_status_t esp8266_wifi_get_state(hal_wifi_t *wifi, hal_wifi_state_t *state) {
    HAL_CHECK_PTR(wifi);
    HAL_CHECK_PTR(state);
    
    esp8266_wifi_platform_t *platform = (esp8266_wifi_platform_t *)wifi->platform_data;
    if (!platform || !platform->initialized) return HAL_NOT_INIT;
    
    *state = wifi->state;
    return HAL_OK;
}

static hal_status_t esp8266_wifi_scan_start(hal_wifi_t *wifi) {
    HAL_CHECK_PTR(wifi);
    
    esp8266_wifi_platform_t *platform = (esp8266_wifi_platform_t *)wifi->platform_data;
    if (!platform || !platform->initialized) return HAL_NOT_INIT;
    
#ifdef TARGET_ESP8266
    esp_err_t ret = esp_wifi_scan_start(NULL, false);
    if (ret != ESP_OK) return HAL_IO_ERROR;
#endif
    
    return HAL_OK;
}

static hal_status_t esp8266_wifi_scan_get_results(hal_wifi_t *wifi, hal_wifi_scan_result_t *results, uint8_t *count) {
    HAL_CHECK_PTR(wifi);
    HAL_CHECK_PTR(results);
    HAL_CHECK_PTR(count);
    
    esp8266_wifi_platform_t *platform = (esp8266_wifi_platform_t *)wifi->platform_data;
    if (!platform || !platform->initialized) return HAL_NOT_INIT;
    
#ifdef TARGET_ESP8266
    uint16_t ap_num = 0;
    wifi_ap_record_t *ap_records = malloc(sizeof(wifi_ap_record_t) * (*count));
    if (!ap_records) return HAL_IO_ERROR;
    
    esp_err_t ret = esp_wifi_scan_get_ap_records(&ap_num, ap_records);
    if (ret == ESP_OK) {
        uint8_t actual_count = (ap_num < *count) ? ap_num : *count;
        for (uint8_t i = 0; i < actual_count; i++) {
            strncpy(results[i].ssid, (char*)ap_records[i].ssid, sizeof(results[i].ssid) - 1);
            results[i].ssid[sizeof(results[i].ssid) - 1] = '\0';
            results[i].rssi = ap_records[i].rssi;
            results[i].channel = ap_records[i].primary;
            results[i].auth = (hal_wifi_auth_t)ap_records[i].authmode;
            memcpy(results[i].bssid, ap_records[i].bssid, 6);
        }
        *count = actual_count;
    }
    
    free(ap_records);
    if (ret != ESP_OK) return HAL_IO_ERROR;
#endif
    
    return HAL_OK;
}

static hal_status_t esp8266_wifi_sta_connect(hal_wifi_t *wifi, const hal_wifi_sta_config_t *config) {
    HAL_CHECK_PTR(wifi);
    HAL_CHECK_PTR(config);
    
    esp8266_wifi_platform_t *platform = (esp8266_wifi_platform_t *)wifi->platform_data;
    if (!platform || !platform->initialized) return HAL_NOT_INIT;
    
#ifdef TARGET_ESP8266
    wifi_config_t wifi_config = {0};
    strncpy((char*)wifi_config.sta.ssid, config->ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, config->password, sizeof(wifi_config.sta.password) - 1);
    
    esp_err_t ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) return HAL_IO_ERROR;
    
    ret = esp_wifi_connect();
    if (ret != ESP_OK) return HAL_IO_ERROR;
#endif
    
    wifi->state = HAL_WIFI_STATE_CONNECTING;
    return HAL_OK;
}

static hal_status_t esp8266_wifi_sta_disconnect(hal_wifi_t *wifi) {
    HAL_CHECK_PTR(wifi);
    
    esp8266_wifi_platform_t *platform = (esp8266_wifi_platform_t *)wifi->platform_data;
    if (!platform || !platform->initialized) return HAL_NOT_INIT;
    
#ifdef TARGET_ESP8266
    esp_err_t ret = esp_wifi_disconnect();
    if (ret != ESP_OK) return HAL_IO_ERROR;
#endif
    
    wifi->state = HAL_WIFI_STATE_DISCONNECTED;
    return HAL_OK;
}

static hal_status_t esp8266_wifi_ap_start(hal_wifi_t *wifi, const hal_wifi_ap_config_t *config) {
    HAL_CHECK_PTR(wifi);
    HAL_CHECK_PTR(config);
    
    esp8266_wifi_platform_t *platform = (esp8266_wifi_platform_t *)wifi->platform_data;
    if (!platform || !platform->initialized) return HAL_NOT_INIT;
    
#ifdef TARGET_ESP8266
    wifi_config_t wifi_config = {0};
    strncpy((char*)wifi_config.ap.ssid, config->ssid, sizeof(wifi_config.ap.ssid) - 1);
    strncpy((char*)wifi_config.ap.password, config->password, sizeof(wifi_config.ap.password) - 1);
    wifi_config.ap.channel = config->channel;
    wifi_config.ap.authmode = (wifi_auth_mode_t)config->auth;
    wifi_config.ap.max_connection = config->max_connections;
    
    esp_err_t ret = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    if (ret != ESP_OK) return HAL_IO_ERROR;
#endif
    
    wifi->state = HAL_WIFI_STATE_AP_STARTED;
    return HAL_OK;
}

static hal_status_t esp8266_wifi_ap_stop(hal_wifi_t *wifi) {
    HAL_CHECK_PTR(wifi);
    
    esp8266_wifi_platform_t *platform = (esp8266_wifi_platform_t *)wifi->platform_data;
    if (!platform || !platform->initialized) return HAL_NOT_INIT;
    
    wifi->state = HAL_WIFI_STATE_OFF;
    return HAL_OK;
}

static hal_status_t esp8266_wifi_start_tcp_server(hal_wifi_t *wifi, uint16_t port) {
    HAL_CHECK_PTR(wifi);
    
    esp8266_wifi_platform_t *platform = (esp8266_wifi_platform_t *)wifi->platform_data;
    if (!platform || !platform->initialized) return HAL_NOT_INIT;
    
#ifdef TARGET_ESP8266
    platform->server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (platform->server_socket < 0) return HAL_IO_ERROR;
    
    int opt = 1;
    setsockopt(platform->server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    platform->server_addr.sin_family = AF_INET;
    platform->server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    platform->server_addr.sin_port = htons(port);
    
    if (bind(platform->server_socket, (struct sockaddr*)&platform->server_addr, sizeof(platform->server_addr)) < 0) {
        close(platform->server_socket);
        return HAL_IO_ERROR;
    }
    
    if (listen(platform->server_socket, 1) < 0) {
        close(platform->server_socket);
        return HAL_IO_ERROR;
    }
    
    platform->server_running = true;
#endif
    
    return HAL_OK;
}

static hal_status_t esp8266_wifi_stop_tcp_server(hal_wifi_t *wifi) {
    HAL_CHECK_PTR(wifi);
    
    esp8266_wifi_platform_t *platform = (esp8266_wifi_platform_t *)wifi->platform_data;
    if (!platform || !platform->initialized) return HAL_NOT_INIT;
    
#ifdef TARGET_ESP8266
    if (platform->server_socket >= 0) {
        close(platform->server_socket);
        platform->server_socket = -1;
    }
    platform->server_running = false;
#endif
    
    return HAL_OK;
}

static hal_status_t esp8266_wifi_tcp_send(hal_wifi_t *wifi, const uint8_t *data, size_t len) {
    HAL_CHECK_PTR(wifi);
    HAL_CHECK_PTR(data);
    
    esp8266_wifi_platform_t *platform = (esp8266_wifi_platform_t *)wifi->platform_data;
    if (!platform || !platform->initialized) return HAL_NOT_INIT;
    
#ifdef TARGET_ESP8266
    if (platform->client_addr_len > 0) {
        sendto(platform->server_socket, data, len, 0, (struct sockaddr*)&platform->client_addr, platform->client_addr_len);
    }
#endif
    
    return HAL_OK;
}

static hal_status_t esp8266_wifi_set_data_callback(hal_wifi_t *wifi, void (*callback)(const uint8_t *data, size_t len, void *context), void *context) {
    HAL_CHECK_PTR(wifi);
    
    esp8266_wifi_platform_t *platform = (esp8266_wifi_platform_t *)wifi->platform_data;
    if (!platform || !platform->initialized) return HAL_NOT_INIT;
    
    platform->data_callback = callback;
    platform->callback_context = context;
    
    return HAL_OK;
}

static hal_status_t esp8266_wifi_get_mac(hal_wifi_t *wifi, uint8_t *mac) {
    HAL_CHECK_PTR(wifi);
    HAL_CHECK_PTR(mac);
    
    esp8266_wifi_platform_t *platform = (esp8266_wifi_platform_t *)wifi->platform_data;
    if (!platform || !platform->initialized) return HAL_NOT_INIT;
    
#ifdef TARGET_ESP8266
    esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, mac);
    if (ret != ESP_OK) return HAL_IO_ERROR;
#else
    memset(mac, 0, 6);
#endif
    
    return HAL_OK;
}
