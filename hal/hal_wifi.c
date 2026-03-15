#include "hal_wifi.h"
#include <string.h>
#include <stdlib.h>

hal_status_t hal_wifi_init(hal_wifi_t *wifi) {
    HAL_CHECK_PTR(wifi);
    HAL_CHECK_PTR(wifi->ops);
    
    memset(wifi, 0, sizeof(hal_wifi_t));
    
    return wifi->ops->init(wifi);
}

hal_status_t hal_wifi_deinit(hal_wifi_t *wifi) {
    HAL_CHECK_PTR(wifi);
    
    if (wifi->ops && wifi->ops->deinit) {
        return wifi->ops->deinit(wifi);
    }
    
    return HAL_OK;
}

hal_status_t hal_wifi_register_driver(hal_wifi_t *wifi, const hal_wifi_ops_t *ops) {
    HAL_CHECK_PTR(wifi);
    HAL_CHECK_PTR(ops);
    
    wifi->ops = ops;
    return HAL_OK;
}

hal_status_t hal_wifi_set_mode(hal_wifi_t *wifi, hal_wifi_mode_t mode) {
    HAL_CHECK_PTR(wifi);
    
    if (!wifi->initialized) return HAL_NOT_INIT;
    if (!wifi->ops->set_mode) return HAL_UNSUPPORTED;
    
    return wifi->ops->set_mode(wifi, mode);
}

hal_status_t hal_wifi_get_state(hal_wifi_t *wifi, hal_wifi_state_t *state) {
    HAL_CHECK_PTR(wifi);
    HAL_CHECK_PTR(state);
    
    if (!wifi->initialized) return HAL_NOT_INIT;
    if (!wifi->ops->get_state) return HAL_UNSUPPORTED;
    
    return wifi->ops->get_state(wifi, state);
}

hal_status_t hal_wifi_scan_networks(hal_wifi_t *wifi, hal_wifi_scan_result_t *results, uint8_t *count) {
    HAL_CHECK_PTR(wifi);
    
    if (!wifi->initialized) return HAL_NOT_INIT;
    if (!wifi->ops->scan_start || !wifi->ops->scan_get_results) return HAL_UNSUPPORTED;
    
    hal_status_t status = wifi->ops->scan_start(wifi);
    if (status != HAL_OK) return status;
    
    return wifi->ops->scan_get_results(wifi, results, count);
}

hal_status_t hal_wifi_sta_connect(hal_wifi_t *wifi, const char *ssid, const char *password) {
    HAL_CHECK_PTR(wifi);
    HAL_CHECK_PTR(ssid);
    
    if (!wifi->initialized) return HAL_NOT_INIT;
    if (!wifi->ops->sta_connect) return HAL_UNSUPPORTED;
    
    hal_wifi_sta_config_t config = {0};
    strncpy(config.ssid, ssid, HAL_WIFI_MAX_SSID_LEN);
    config.ssid[HAL_WIFI_MAX_SSID_LEN] = '\0';
    
    if (password) {
        strncpy(config.password, password, HAL_WIFI_MAX_PASS_LEN);
        config.password[HAL_WIFI_MAX_PASS_LEN] = '\0';
    }
    
    return wifi->ops->sta_connect(wifi, &config);
}

hal_status_t hal_wifi_sta_disconnect(hal_wifi_t *wifi) {
    HAL_CHECK_PTR(wifi);
    
    if (!wifi->initialized) return HAL_NOT_INIT;
    if (!wifi->ops->sta_disconnect) return HAL_UNSUPPORTED;
    
    return wifi->ops->sta_disconnect(wifi);
}

hal_status_t hal_wifi_ap_start(hal_wifi_t *wifi, const char *ssid, const char *password, uint8_t channel) {
    HAL_CHECK_PTR(wifi);
    HAL_CHECK_PTR(ssid);
    
    if (!wifi->initialized) return HAL_NOT_INIT;
    if (!wifi->ops->ap_start) return HAL_UNSUPPORTED;
    
    hal_wifi_ap_config_t config = {0};
    strncpy(config.ssid, ssid, HAL_WIFI_MAX_SSID_LEN);
    config.ssid[HAL_WIFI_MAX_SSID_LEN] = '\0';
    
    if (password) {
        strncpy(config.password, password, HAL_WIFI_MAX_PASS_LEN);
        config.password[HAL_WIFI_MAX_PASS_LEN] = '\0';
        config.auth = HAL_WIFI_AUTH_WPA2_PSK;
    } else {
        config.auth = HAL_WIFI_AUTH_OPEN;
    }
    
    config.channel = channel;
    config.max_connections = 4;
    config.hidden = false;
    
    return wifi->ops->ap_start(wifi, &config);
}

hal_status_t hal_wifi_ap_stop(hal_wifi_t *wifi) {
    HAL_CHECK_PTR(wifi);
    
    if (!wifi->initialized) return HAL_NOT_INIT;
    if (!wifi->ops->ap_stop) return HAL_UNSUPPORTED;
    
    return wifi->ops->ap_stop(wifi);
}

hal_status_t hal_wifi_start_telemetry_server(hal_wifi_t *wifi, uint16_t port) {
    HAL_CHECK_PTR(wifi);
    
    if (!wifi->initialized) return HAL_NOT_INIT;
    if (!wifi->ops->start_tcp_server) return HAL_UNSUPPORTED;
    
    return wifi->ops->start_tcp_server(wifi, port);
}

hal_status_t hal_wifi_send_telemetry(hal_wifi_t *wifi, const uint8_t *data, size_t len) {
    HAL_CHECK_PTR(wifi);
    HAL_CHECK_PTR(data);
    
    if (!wifi->initialized) return HAL_NOT_INIT;
    if (!wifi->ops->tcp_send) return HAL_UNSUPPORTED;
    
    return wifi->ops->tcp_send(wifi, data, len);
}

hal_status_t hal_wifi_set_data_callback(hal_wifi_t *wifi, void (*callback)(const uint8_t *data, size_t len, void *context), void *context) {
    HAL_CHECK_PTR(wifi);
    
    if (!wifi->initialized) return HAL_NOT_INIT;
    if (!wifi->ops->set_data_callback) return HAL_UNSUPPORTED;
    
    wifi->data_callback = callback;
    wifi->callback_context = context;
    
    return wifi->ops->set_data_callback(wifi, callback, context);
}
