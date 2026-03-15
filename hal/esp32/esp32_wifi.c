#include "hal_wifi.h"
#include "hal_timer.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef TARGET_ESP32
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
} esp32_wifi_platform_t;

static const char *TAG = "hal_wifi";

static hal_status_t esp32_wifi_init(hal_wifi_t *wifi);
static hal_status_t esp32_wifi_deinit(hal_wifi_t *wifi);
static hal_status_t esp32_wifi_set_mode(hal_wifi_t *wifi, hal_wifi_mode_t mode);
static hal_status_t esp32_wifi_get_state(hal_wifi_t *wifi, hal_wifi_state_t *state);
static hal_status_t esp32_wifi_scan_start(hal_wifi_t *wifi);
static hal_status_t esp32_wifi_scan_get_results(hal_wifi_t *wifi, hal_wifi_scan_result_t *results, uint8_t *count);
static hal_status_t esp32_wifi_sta_connect(hal_wifi_t *wifi, const hal_wifi_sta_config_t *config);
static hal_status_t esp32_wifi_sta_disconnect(hal_wifi_t *wifi);
static hal_status_t esp32_wifi_sta_get_config(hal_wifi_t *wifi, hal_wifi_sta_config_t *config);
static hal_status_t esp32_wifi_ap_start(hal_wifi_t *wifi, const hal_wifi_ap_config_t *config);
static hal_status_t esp32_wifi_ap_stop(hal_wifi_t *wifi);
static hal_status_t esp32_wifi_ap_get_config(hal_wifi_t *wifi, hal_wifi_ap_config_t *config);
static hal_status_t esp32_wifi_get_mac(hal_wifi_t *wifi, uint8_t *mac);
static hal_status_t esp32_wifi_set_mac(hal_wifi_t *wifi, const uint8_t *mac);
static hal_status_t esp32_wifi_start_tcp_server(hal_wifi_t *wifi, uint16_t port);
static hal_status_t esp32_wifi_stop_tcp_server(hal_wifi_t *wifi);
static hal_status_t esp32_wifi_tcp_send(hal_wifi_t *wifi, const uint8_t *data, size_t len);
static hal_status_t esp32_wifi_udp_send(hal_wifi_t *wifi, const uint8_t *data, size_t len, const char *host, uint16_t port);
static hal_status_t esp32_wifi_set_data_callback(hal_wifi_t *wifi, void (*callback)(const uint8_t *data, size_t len, void *context), void *context);

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void tcp_server_task(void *pvParameters);

static const hal_wifi_ops_t esp32_wifi_ops = {
    .init = esp32_wifi_init,
    .deinit = esp32_wifi_deinit,
    .set_mode = esp32_wifi_set_mode,
    .get_state = esp32_wifi_get_state,
    .scan_start = esp32_wifi_scan_start,
    .scan_get_results = esp32_wifi_scan_get_results,
    .sta_connect = esp32_wifi_sta_connect,
    .sta_disconnect = esp32_wifi_sta_disconnect,
    .sta_get_config = esp32_wifi_sta_get_config,
    .ap_start = esp32_wifi_ap_start,
    .ap_stop = esp32_wifi_ap_stop,
    .ap_get_config = esp32_wifi_ap_get_config,
    .get_mac = esp32_wifi_get_mac,
    .set_mac = esp32_wifi_set_mac,
    .start_tcp_server = esp32_wifi_start_tcp_server,
    .stop_tcp_server = esp32_wifi_stop_tcp_server,
    .tcp_send = esp32_wifi_tcp_send,
    .udp_send = esp32_wifi_udp_send,
    .set_data_callback = esp32_wifi_set_data_callback
};

hal_status_t hal_esp32_wifi_register(hal_wifi_t *wifi) {
    HAL_CHECK_PTR(wifi);
    return hal_wifi_register_driver(wifi, &esp32_wifi_ops);
}

static hal_status_t esp32_wifi_init(hal_wifi_t *wifi) {
    HAL_CHECK_PTR(wifi);
    
    esp32_wifi_platform_t *platform = malloc(sizeof(esp32_wifi_platform_t));
    if (!platform) return HAL_IO_ERROR;
    
    memset(platform, 0, sizeof(esp32_wifi_platform_t));
    wifi->platform_data = platform;
    
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    platform->sta_netif = esp_netif_create_default_wifi_sta();
    platform->ap_netif = esp_netif_create_default_wifi_ap();
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    platform->wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, wifi));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, wifi));
    
    wifi->mode = HAL_WIFI_MODE_OFF;
    wifi->state = HAL_WIFI_STATE_DISCONNECTED;
    wifi->initialized = true;
    
    ESP_LOGI(TAG, "WiFi HAL initialized");
    return HAL_OK;
}

static hal_status_t esp32_wifi_deinit(hal_wifi_t *wifi) {
    HAL_CHECK_PTR(wifi);
    
    esp32_wifi_platform_t *platform = (esp32_wifi_platform_t *)wifi->platform_data;
    if (!platform) return HAL_NOT_INIT;
    
    esp32_wifi_stop_tcp_server(wifi);
    esp_wifi_deinit();
    esp_event_loop_delete_default();
    
    if (platform->wifi_event_group) {
        vEventGroupDelete(platform->wifi_event_group);
    }
    
    if (platform->sta_netif) {
        esp_netif_destroy(platform->sta_netif);
    }
    
    if (platform->ap_netif) {
        esp_netif_destroy(platform->ap_netif);
    }
    
    free(platform);
    wifi->platform_data = NULL;
    wifi->initialized = false;
    
    return HAL_OK;
}

static hal_status_t esp32_wifi_set_mode(hal_wifi_t *wifi, hal_wifi_mode_t mode) {
    HAL_CHECK_PTR(wifi);
    
    if (!wifi->initialized) return HAL_NOT_INIT;
    
    wifi_mode_t esp_mode;
    switch (mode) {
        case HAL_WIFI_MODE_OFF: esp_mode = WIFI_MODE_NULL; break;
        case HAL_WIFI_MODE_STA: esp_mode = WIFI_MODE_STA; break;
        case HAL_WIFI_MODE_AP: esp_mode = WIFI_MODE_AP; break;
        case HAL_WIFI_MODE_AP_STA: esp_mode = WIFI_MODE_APSTA; break;
        default: return HAL_PARAM_ERROR;
    }
    
    esp_err_t err = esp_wifi_set_mode(esp_mode);
    if (err != ESP_OK) return HAL_IO_ERROR;
    
    wifi->mode = mode;
    return HAL_OK;
}

static hal_status_t esp32_wifi_get_state(hal_wifi_t *wifi, hal_wifi_state_t *state) {
    HAL_CHECK_PTR(wifi);
    HAL_CHECK_PTR(state);
    
    if (!wifi->initialized) return HAL_NOT_INIT;
    
    *state = wifi->state;
    return HAL_OK;
}

static hal_status_t esp32_wifi_scan_start(hal_wifi_t *wifi) {
    HAL_CHECK_PTR(wifi);
    
    if (!wifi->initialized) return HAL_NOT_INIT;
    
    esp_err_t err = esp_wifi_scan_start(NULL, true);
    if (err != ESP_OK) return HAL_IO_ERROR;
    
    return HAL_OK;
}

static hal_status_t esp32_wifi_scan_get_results(hal_wifi_t *wifi, hal_wifi_scan_result_t *results, uint8_t *count) {
    HAL_CHECK_PTR(wifi);
    HAL_CHECK_PTR(results);
    HAL_CHECK_PTR(count);
    
    if (!wifi->initialized) return HAL_NOT_INIT;
    
    uint16_t ap_num = 0;
    esp_err_t err = esp_wifi_scan_get_ap_num(&ap_num);
    if (err != ESP_OK) return HAL_IO_ERROR;
    
    wifi_ap_record_t *ap_records = malloc(sizeof(wifi_ap_record_t) * ap_num);
    if (!ap_records) return HAL_IO_ERROR;
    
    err = esp_wifi_scan_get_ap_records(&ap_num, ap_records);
    if (err != ESP_OK) {
        free(ap_records);
        return HAL_IO_ERROR;
    }
    
    uint8_t result_count = (ap_num < HAL_WIFI_MAX_SCAN_RESULTS) ? ap_num : HAL_WIFI_MAX_SCAN_RESULTS;
    for (uint8_t i = 0; i < result_count; i++) {
        strncpy(results[i].ssid, (char *)ap_records[i].ssid, HAL_WIFI_MAX_SSID_LEN);
        results[i].ssid[HAL_WIFI_MAX_SSID_LEN] = '\0';
        memcpy(results[i].bssid, ap_records[i].bssid, HAL_WIFI_MAX_BSSID_LEN);
        results[i].rssi = ap_records[i].rssi;
        results[i].channel = ap_records[i].primary;
        results[i].auth = (hal_wifi_auth_t)ap_records[i].authmode;
        results[i].last_seen_ms = hal_timer_get_ms();
    }
    
    *count = result_count;
    free(ap_records);
    
    return HAL_OK;
}

static hal_status_t esp32_wifi_sta_connect(hal_wifi_t *wifi, const hal_wifi_sta_config_t *config) {
    HAL_CHECK_PTR(wifi);
    HAL_CHECK_PTR(config);
    
    if (!wifi->initialized) return HAL_NOT_INIT;
    
    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, config->ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, config->password, sizeof(wifi_config.sta.password) - 1);
    
    if (config->use_bssid) {
        memcpy(wifi_config.sta.bssid, config->bssid, 6);
        wifi_config.sta.bssid_set = true;
    }
    
    esp_err_t err = esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    if (err != ESP_OK) return HAL_IO_ERROR;
    
    wifi->state = HAL_WIFI_STATE_CONNECTING;
    err = esp_wifi_start();
    if (err != ESP_OK) {
        wifi->state = HAL_WIFI_STATE_ERROR;
        return HAL_IO_ERROR;
    }
    
    err = esp_wifi_connect();
    if (err != ESP_OK) {
        wifi->state = HAL_WIFI_STATE_ERROR;
        return HAL_IO_ERROR;
    }
    
    esp32_wifi_platform_t *platform = (esp32_wifi_platform_t *)wifi->platform_data;
    EventBits_t bits = xEventGroupWaitBits(platform->wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(10000));
    
    if (bits & WIFI_CONNECTED_BIT) {
        wifi->state = HAL_WIFI_STATE_CONNECTED;
        wifi->sta_config = *config;
        return HAL_OK;
    } else {
        wifi->state = HAL_WIFI_STATE_ERROR;
        return HAL_TIMEOUT;
    }
}

static hal_status_t esp32_wifi_sta_disconnect(hal_wifi_t *wifi) {
    HAL_CHECK_PTR(wifi);
    
    if (!wifi->initialized) return HAL_NOT_INIT;
    
    wifi->state = HAL_WIFI_STATE_DISCONNECTING;
    esp_err_t err = esp_wifi_disconnect();
    if (err != ESP_OK) return HAL_IO_ERROR;
    
    wifi->state = HAL_WIFI_STATE_DISCONNECTED;
    return HAL_OK;
}

static hal_status_t esp32_wifi_sta_get_config(hal_wifi_t *wifi, hal_wifi_sta_config_t *config) {
    HAL_CHECK_PTR(wifi);
    HAL_CHECK_PTR(config);
    
    if (!wifi->initialized) return HAL_NOT_INIT;
    
    *config = wifi->sta_config;
    return HAL_OK;
}

static hal_status_t esp32_wifi_ap_start(hal_wifi_t *wifi, const hal_wifi_ap_config_t *config) {
    HAL_CHECK_PTR(wifi);
    HAL_CHECK_PTR(config);
    
    if (!wifi->initialized) return HAL_NOT_INIT;
    
    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.ap.ssid, config->ssid, sizeof(wifi_config.ap.ssid) - 1);
    strncpy((char *)wifi_config.ap.password, config->password, sizeof(wifi_config.ap.password) - 1);
    wifi_config.ap.channel = config->channel;
    wifi_config.ap.authmode = (wifi_auth_mode_t)config->auth;
    wifi_config.ap.max_connection = config->max_connections;
    wifi_config.ap.ssid_hidden = config->hidden;
    
    esp_err_t err = esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config);
    if (err != ESP_OK) return HAL_IO_ERROR;
    
    err = esp_wifi_start();
    if (err != ESP_OK) return HAL_IO_ERROR;
    
    wifi->ap_config = *config;
    return HAL_OK;
}

static hal_status_t esp32_wifi_ap_stop(hal_wifi_t *wifi) {
    HAL_CHECK_PTR(wifi);
    
    if (!wifi->initialized) return HAL_NOT_INIT;
    
    esp_err_t err = esp_wifi_stop();
    if (err != ESP_OK) return HAL_IO_ERROR;
    
    return HAL_OK;
}

static hal_status_t esp32_wifi_ap_get_config(hal_wifi_t *wifi, hal_wifi_ap_config_t *config) {
    HAL_CHECK_PTR(wifi);
    HAL_CHECK_PTR(config);
    
    if (!wifi->initialized) return HAL_NOT_INIT;
    
    *config = wifi->ap_config;
    return HAL_OK;
}

static hal_status_t esp32_wifi_get_mac(hal_wifi_t *wifi, uint8_t *mac) {
    HAL_CHECK_PTR(wifi);
    HAL_CHECK_PTR(mac);
    
    if (!wifi->initialized) return HAL_NOT_INIT;
    
    esp_err_t err = esp_wifi_get_mac(ESP_IF_WIFI_STA, mac);
    if (err != ESP_OK) return HAL_IO_ERROR;
    
    return HAL_OK;
}

static hal_status_t esp32_wifi_set_mac(hal_wifi_t *wifi, const uint8_t *mac) {
    HAL_CHECK_PTR(wifi);
    HAL_CHECK_PTR(mac);
    
    if (!wifi->initialized) return HAL_NOT_INIT;
    
    esp_err_t err = esp_wifi_set_mac(ESP_IF_WIFI_STA, (uint8_t *)mac);
    if (err != ESP_OK) return HAL_IO_ERROR;
    
    return HAL_OK;
}

static hal_status_t esp32_wifi_start_tcp_server(hal_wifi_t *wifi, uint16_t port) {
    HAL_CHECK_PTR(wifi);
    
    if (!wifi->initialized) return HAL_NOT_INIT;
    
    esp32_wifi_platform_t *platform = (esp32_wifi_platform_t *)wifi->platform_data;
    if (!platform) return HAL_NOT_INIT;
    
    if (platform->server_running) {
        esp32_wifi_stop_tcp_server(wifi);
    }
    
    platform->server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (platform->server_socket < 0) return HAL_IO_ERROR;
    
    int opt = 1;
    setsockopt(platform->server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    platform->server_addr.sin_family = AF_INET;
    platform->server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    platform->server_addr.sin_port = htons(port);
    
    if (bind(platform->server_socket, (struct sockaddr *)&platform->server_addr, sizeof(platform->server_addr)) < 0) {
        close(platform->server_socket);
        return HAL_IO_ERROR;
    }
    
    if (listen(platform->server_socket, 5) < 0) {
        close(platform->server_socket);
        return HAL_IO_ERROR;
    }
    
    platform->server_running = true;
    xTaskCreate(tcp_server_task, "tcp_server", 4096, wifi, 5, &platform->server_task);
    
    ESP_LOGI(TAG, "TCP server started on port %d", port);
    return HAL_OK;
}

static hal_status_t esp32_wifi_stop_tcp_server(hal_wifi_t *wifi) {
    HAL_CHECK_PTR(wifi);
    
    esp32_wifi_platform_t *platform = (esp32_wifi_platform_t *)wifi->platform_data;
    if (!platform) return HAL_NOT_INIT;
    
    platform->server_running = false;
    
    if (platform->server_socket >= 0) {
        close(platform->server_socket);
        platform->server_socket = -1;
    }
    
    if (platform->server_task) {
        vTaskDelete(platform->server_task);
        platform->server_task = NULL;
    }
    
    return HAL_OK;
}

static hal_status_t esp32_wifi_tcp_send(hal_wifi_t *wifi, const uint8_t *data, size_t len) {
    HAL_CHECK_PTR(wifi);
    HAL_CHECK_PTR(data);
    
    esp32_wifi_platform_t *platform = (esp32_wifi_platform_t *)wifi->platform_data;
    if (!platform || platform->server_socket < 0) return HAL_NOT_INIT;
    
    int client_sock = accept(platform->server_socket, (struct sockaddr *)&platform->client_addr, &platform->client_addr_len);
    if (client_sock < 0) return HAL_IO_ERROR;
    
    int sent = send(client_sock, data, len, 0);
    close(client_sock);
    
    return (sent == (int)len) ? HAL_OK : HAL_IO_ERROR;
}

static hal_status_t esp32_wifi_udp_send(hal_wifi_t *wifi, const uint8_t *data, size_t len, const char *host, uint16_t port) {
    HAL_CHECK_PTR(wifi);
    HAL_CHECK_PTR(data);
    HAL_CHECK_PTR(host);
    
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) return HAL_IO_ERROR;
    
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &dest_addr.sin_addr);
    
    int sent = sendto(sock, data, len, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    close(sock);
    
    return (sent == (int)len) ? HAL_OK : HAL_IO_ERROR;
}

static hal_status_t esp32_wifi_set_data_callback(hal_wifi_t *wifi, void (*callback)(const uint8_t *data, size_t len, void *context), void *context) {
    HAL_CHECK_PTR(wifi);
    
    wifi->data_callback = callback;
    wifi->callback_context = context;
    
    return HAL_OK;
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    hal_wifi_t *wifi = (hal_wifi_t *)arg;
    esp32_wifi_platform_t *platform = (esp32_wifi_platform_t *)wifi->platform_data;
    
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi station started");
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGI(TAG, "WiFi station disconnected");
                wifi->state = HAL_WIFI_STATE_DISCONNECTED;
                xEventGroupSetBits(platform->wifi_event_group, WIFI_FAIL_BIT);
                break;
            default:
                break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
            wifi->state = HAL_WIFI_STATE_CONNECTED;
            wifi->sta_config.ip_addr = event->ip_info.ip.addr;
            wifi->sta_config.netmask = event->ip_info.netmask.addr;
            wifi->sta_config.gateway = event->ip_info.gw.addr;
            xEventGroupSetBits(platform->wifi_event_group, WIFI_CONNECTED_BIT);
        }
    }
}

static void tcp_server_task(void *pvParameters) {
    hal_wifi_t *wifi = (hal_wifi_t *)pvParameters;
    esp32_wifi_platform_t *platform = (esp32_wifi_platform_t *)wifi->platform_data;
    
    uint8_t rx_buffer[HAL_WIFI_BUFFER_SIZE];
    
    while (platform->server_running) {
        platform->client_addr_len = sizeof(platform->client_addr);
        int client_sock = accept(platform->server_socket, (struct sockaddr *)&platform->client_addr, &platform->client_addr_len);
        
        if (client_sock >= 0) {
            int len = recv(client_sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
            if (len > 0) {
                rx_buffer[len] = 0;
                ESP_LOGI(TAG, "Received %d bytes", len);
                
                if (wifi->data_callback) {
                    wifi->data_callback(rx_buffer, len, wifi->callback_context);
                }
            }
            close(client_sock);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    vTaskDelete(NULL);
}

#else

hal_status_t hal_esp32_wifi_register(hal_wifi_t *wifi) {
    return HAL_UNSUPPORTED;
}

#endif
