#include "hal_bt.h"
#include "hal_timer.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef TARGET_ESP32
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "esp_spp_api.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#define SPP_SERVER_NAME "AeroFirmware"
#define SPP_SERVER_CHANNEL 1

typedef struct {
    esp_spp_mode_t mode;
    uint32_t handle;
    bool is_connected;
    uint8_t remote_addr[6];
    SemaphoreHandle_t spp_mutex;
} esp32_bt_platform_t;

static const char *TAG = "hal_bt";

static hal_status_t esp32_bt_init(hal_bt_t *bt);
static hal_status_t esp32_bt_deinit(hal_bt_t *bt);
static hal_status_t esp32_bt_set_mode(hal_bt_t *bt, hal_bt_mode_t mode);
static hal_status_t esp32_bt_get_state(hal_bt_t *bt, hal_bt_state_t *state);
static hal_status_t esp32_bt_set_name(hal_bt_t *bt, const char *name);
static hal_status_t esp32_bt_get_name(hal_bt_t *bt, char *name, size_t len);
static hal_status_t esp32_bt_start_scan(hal_bt_t *bt);
static hal_status_t esp32_bt_stop_scan(hal_bt_t *bt);
static hal_status_t esp32_bt_get_scan_results(hal_bt_t *bt, hal_bt_device_t *devices, uint8_t *count);
static hal_status_t esp32_bt_connect(hal_bt_t *bt, const uint8_t *addr);
static hal_status_t esp32_bt_disconnect(hal_bt_t *bt);
static hal_status_t esp32_bt_is_connected(hal_bt_t *bt, bool *connected);
static hal_status_t esp32_bt_start_advertising(hal_bt_t *bt, const hal_bt_config_t *config);
static hal_status_t esp32_bt_stop_advertising(hal_bt_t *bt);
static hal_status_t esp32_bt_send_data(hal_bt_t *bt, const uint8_t *data, size_t len);
static hal_status_t esp32_bt_set_data_callback(hal_bt_t *bt, void (*callback)(const uint8_t *data, size_t len, void *context), void *context);
static hal_status_t esp32_bt_set_device_callback(hal_bt_t *bt, void (*callback)(const hal_bt_device_t *device, bool discovered, void *context), void *context);

static void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param);
static void esp_spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param);
static void bt_inquiry_scan_result(esp_bt_gap_cb_param_t *param);

static const hal_bt_ops_t esp32_bt_ops = {
    .init = esp32_bt_init,
    .deinit = esp32_bt_deinit,
    .set_mode = esp32_bt_set_mode,
    .get_state = esp32_bt_get_state,
    .set_name = esp32_bt_set_name,
    .get_name = esp32_bt_get_name,
    .start_scan = esp32_bt_start_scan,
    .stop_scan = esp32_bt_stop_scan,
    .get_scan_results = esp32_bt_get_scan_results,
    .connect = esp32_bt_connect,
    .disconnect = esp32_bt_disconnect,
    .is_connected = esp32_bt_is_connected,
    .start_advertising = esp32_bt_start_advertising,
    .stop_advertising = esp32_bt_stop_advertising,
    .send_data = esp32_bt_send_data,
    .set_data_callback = esp32_bt_set_data_callback,
    .set_device_callback = esp32_bt_set_device_callback
};

static hal_bt_t *g_bt_instance = NULL;

hal_status_t hal_esp32_bt_register(hal_bt_t *bt) {
    HAL_CHECK_PTR(bt);
    g_bt_instance = bt;
    return hal_bt_register_driver(bt, &esp32_bt_ops);
}

static hal_status_t esp32_bt_init(hal_bt_t *bt) {
    HAL_CHECK_PTR(bt);
    
    esp32_bt_platform_t *platform = malloc(sizeof(esp32_bt_platform_t));
    if (!platform) return HAL_IO_ERROR;
    
    memset(platform, 0, sizeof(esp32_bt_platform_t));
    platform->spp_mutex = xSemaphoreCreateMutex();
    if (!platform->spp_mutex) {
        free(platform);
        return HAL_IO_ERROR;
    }
    
    bt->platform_data = platform;
    
    esp_err_t ret;
    
    ret = esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "BLE controller mem release failed: %s", esp_err_to_name(ret));
    }
    
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "initialize controller failed: %s", esp_err_to_name(ret));
        vSemaphoreDelete(platform->spp_mutex);
        free(platform);
        return HAL_IO_ERROR;
    }
    
    ret = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
    if (ret) {
        ESP_LOGE(TAG, "enable controller failed: %s", esp_err_to_name(ret));
        vSemaphoreDelete(platform->spp_mutex);
        free(platform);
        return HAL_IO_ERROR;
    }
    
    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(TAG, "initialize bluedroid failed: %s", esp_err_to_name(ret));
        vSemaphoreDelete(platform->spp_mutex);
        free(platform);
        return HAL_IO_ERROR;
    }
    
    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG, "enable bluedroid failed: %s", esp_err_to_name(ret));
        vSemaphoreDelete(platform->spp_mutex);
        free(platform);
        return HAL_IO_ERROR;
    }
    
    ret = esp_bt_gap_register_callback(esp_bt_gap_cb);
    if (ret) {
        ESP_LOGE(TAG, "gap register failed: %s", esp_err_to_name(ret));
        vSemaphoreDelete(platform->spp_mutex);
        free(platform);
        return HAL_IO_ERROR;
    }
    
    esp_spp_init(ESP_SPP_MODE_CB);
    ret = esp_spp_register_callback(esp_spp_cb);
    if (ret) {
        ESP_LOGE(TAG, "spp register failed: %s", esp_err_to_name(ret));
        vSemaphoreDelete(platform->spp_mutex);
        free(platform);
        return HAL_IO_ERROR;
    }
    
    esp_bt_dev_set_device_name("AeroFirmware");
    
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
    
    bt->mode = HAL_BT_MODE_CLASSIC;
    bt->state = HAL_BT_STATE_READY;
    bt->initialized = true;
    
    ESP_LOGI(TAG, "Bluetooth HAL initialized");
    return HAL_OK;
}

static hal_status_t esp32_bt_deinit(hal_bt_t *bt) {
    HAL_CHECK_PTR(bt);
    
    esp32_bt_platform_t *platform = (esp32_bt_platform_t *)bt->platform_data;
    if (!platform) return HAL_NOT_INIT;
    
    esp_spp_deinit();
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    
    if (platform->spp_mutex) {
        vSemaphoreDelete(platform->spp_mutex);
    }
    
    free(platform);
    bt->platform_data = NULL;
    bt->initialized = false;
    g_bt_instance = NULL;
    
    return HAL_OK;
}

static hal_status_t esp32_bt_set_mode(hal_bt_t *bt, hal_bt_mode_t mode) {
    HAL_CHECK_PTR(bt);
    
    if (!bt->initialized) return HAL_NOT_INIT;
    
    esp_bt_mode_t esp_mode;
    switch (mode) {
        case HAL_BT_MODE_OFF: esp_mode = ESP_BT_MODE_IDLE; break;
        case HAL_BT_MODE_CLASSIC: esp_mode = ESP_BT_MODE_CLASSIC_BT; break;
        case HAL_BT_MODE_BLE: esp_mode = ESP_BT_MODE_BLE; break;
        case HAL_BT_MODE_DUAL: esp_mode = ESP_BT_MODE_BTDM; break;
        default: return HAL_PARAM_ERROR;
    }
    
    esp_err_t ret = esp_bt_controller_enable(esp_mode);
    if (ret != ESP_OK) return HAL_IO_ERROR;
    
    bt->mode = mode;
    return HAL_OK;
}

static hal_status_t esp32_bt_get_state(hal_bt_t *bt, hal_bt_state_t *state) {
    HAL_CHECK_PTR(bt);
    HAL_CHECK_PTR(state);
    
    if (!bt->initialized) return HAL_NOT_INIT;
    
    *state = bt->state;
    return HAL_OK;
}

static hal_status_t esp32_bt_set_name(hal_bt_t *bt, const char *name) {
    HAL_CHECK_PTR(bt);
    HAL_CHECK_PTR(name);
    
    if (!bt->initialized) return HAL_NOT_INIT;
    
    esp_err_t ret = esp_bt_dev_set_device_name(name);
    if (ret != ESP_OK) return HAL_IO_ERROR;
    
    strncpy(bt->config.name, name, HAL_BT_MAX_NAME_LEN);
    bt->config.name[HAL_BT_MAX_NAME_LEN] = '\0';
    
    return HAL_OK;
}

static hal_status_t esp32_bt_get_name(hal_bt_t *bt, char *name, size_t len) {
    HAL_CHECK_PTR(bt);
    HAL_CHECK_PTR(name);
    
    if (!bt->initialized) return HAL_NOT_INIT;
    
    const char *device_name = esp_bt_dev_get_name();
    if (!device_name) return HAL_IO_ERROR;
    
    strncpy(name, device_name, len - 1);
    name[len - 1] = '\0';
    
    return HAL_OK;
}

static hal_status_t esp32_bt_start_scan(hal_bt_t *bt) {
    HAL_CHECK_PTR(bt);
    
    if (!bt->initialized) return HAL_NOT_INIT;
    
    esp_err_t ret = esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 10, 0);
    if (ret != ESP_OK) return HAL_IO_ERROR;
    
    bt->state = HAL_BT_STATE_SCANNING;
    bt->device_count = 0;
    
    return HAL_OK;
}

static hal_status_t esp32_bt_stop_scan(hal_bt_t *bt) {
    HAL_CHECK_PTR(bt);
    
    if (!bt->initialized) return HAL_NOT_INIT;
    
    esp_err_t ret = esp_bt_gap_cancel_discovery();
    if (ret != ESP_OK) return HAL_IO_ERROR;
    
    if (bt->state == HAL_BT_STATE_SCANNING) {
        bt->state = HAL_BT_STATE_READY;
    }
    
    return HAL_OK;
}

static hal_status_t esp32_bt_get_scan_results(hal_bt_t *bt, hal_bt_device_t *devices, uint8_t *count) {
    HAL_CHECK_PTR(bt);
    HAL_CHECK_PTR(devices);
    HAL_CHECK_PTR(count);
    
    if (!bt->initialized) return HAL_NOT_INIT;
    
    uint8_t result_count = (bt->device_count < HAL_BT_MAX_DEVICES) ? bt->device_count : HAL_BT_MAX_DEVICES;
    for (uint8_t i = 0; i < result_count; i++) {
        devices[i] = bt->devices[i];
    }
    
    *count = result_count;
    return HAL_OK;
}

static hal_status_t esp32_bt_connect(hal_bt_t *bt, const uint8_t *addr) {
    HAL_CHECK_PTR(bt);
    HAL_CHECK_PTR(addr);
    
    if (!bt->initialized) return HAL_NOT_INIT;
    
    esp_err_t ret = esp_spp_start_srv(ESP_SPP_SEC_NONE, ESP_SPP_ROLE_SLAVE, SPP_SERVER_CHANNEL, SPP_SERVER_NAME);
    if (ret != ESP_OK) return HAL_IO_ERROR;
    
    return HAL_OK;
}

static hal_status_t esp32_bt_disconnect(hal_bt_t *bt) {
    HAL_CHECK_PTR(bt);
    
    if (!bt->initialized) return HAL_NOT_INIT;
    
    esp32_bt_platform_t *platform = (esp32_bt_platform_t *)bt->platform_data;
    if (!platform) return HAL_NOT_INIT;
    
    xSemaphoreTake(platform->spp_mutex, portMAX_DELAY);
    
    if (platform->handle != 0) {
        esp_spp_disconnect(platform->handle);
        platform->handle = 0;
    }
    
    platform->is_connected = false;
    bt->state = HAL_BT_STATE_READY;
    
    xSemaphoreGive(platform->spp_mutex);
    
    return HAL_OK;
}

static hal_status_t esp32_bt_is_connected(hal_bt_t *bt, bool *connected) {
    HAL_CHECK_PTR(bt);
    HAL_CHECK_PTR(connected);
    
    if (!bt->initialized) return HAL_NOT_INIT;
    
    esp32_bt_platform_t *platform = (esp32_bt_platform_t *)bt->platform_data;
    if (!platform) return HAL_NOT_INIT;
    
    *connected = platform->is_connected;
    return HAL_OK;
}

static hal_status_t esp32_bt_start_advertising(hal_bt_t *bt, const hal_bt_config_t *config) {
    HAL_CHECK_PTR(bt);
    HAL_CHECK_PTR(config);
    
    if (!bt->initialized) return HAL_NOT_INIT;
    
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
    
    bt->config = *config;
    bt->state = HAL_BT_STATE_ADVERTISING;
    
    return HAL_OK;
}

static hal_status_t esp32_bt_stop_advertising(hal_bt_t *bt) {
    HAL_CHECK_PTR(bt);
    
    if (!bt->initialized) return HAL_NOT_INIT;
    
    esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
    
    if (bt->state == HAL_BT_STATE_ADVERTISING) {
        bt->state = HAL_BT_STATE_READY;
    }
    
    return HAL_OK;
}

static hal_status_t esp32_bt_send_data(hal_bt_t *bt, const uint8_t *data, size_t len) {
    HAL_CHECK_PTR(bt);
    HAL_CHECK_PTR(data);
    
    if (!bt->initialized) return HAL_NOT_INIT;
    
    esp32_bt_platform_t *platform = (esp32_bt_platform_t *)bt->platform_data;
    if (!platform || !platform->is_connected || platform->handle == 0) return HAL_NOT_INIT;
    
    xSemaphoreTake(platform->spp_mutex, portMAX_DELAY);
    
    esp_err_t ret = esp_spp_write(platform->handle, len, data, ESP_SPP_WRITE_TYPE_NO_RSP);
    
    xSemaphoreGive(platform->spp_mutex);
    
    return (ret == ESP_OK) ? HAL_OK : HAL_IO_ERROR;
}

static hal_status_t esp32_bt_set_data_callback(hal_bt_t *bt, void (*callback)(const uint8_t *data, size_t len, void *context), void *context) {
    HAL_CHECK_PTR(bt);
    
    bt->data_callback = callback;
    bt->data_context = context;
    
    return HAL_OK;
}

static hal_status_t esp32_bt_set_device_callback(hal_bt_t *bt, void (*callback)(const hal_bt_device_t *device, bool discovered, void *context), void *context) {
    HAL_CHECK_PTR(bt);
    
    bt->device_callback = callback;
    bt->device_context = context;
    
    return HAL_OK;
}

static void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
    if (!g_bt_instance) return;
    
    switch (event) {
        case ESP_BT_GAP_DISC_RES_EVT:
            bt_inquiry_scan_result(param);
            break;
        case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
            if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
                if (g_bt_instance->state == HAL_BT_STATE_SCANNING) {
                    g_bt_instance->state = HAL_BT_STATE_READY;
                }
            }
            break;
        default:
            break;
    }
}

static void esp_spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
    if (!g_bt_instance) return;
    
    esp32_bt_platform_t *platform = (esp32_bt_platform_t *)g_bt_instance->platform_data;
    if (!platform) return;
    
    switch (event) {
        case ESP_SPP_INIT_EVT:
            ESP_LOGI(TAG, "SPP initialized");
            break;
        case ESP_SPP_DISCOVERY_COMP_EVT:
            ESP_LOGI(TAG, "SPP discovery complete");
            break;
        case ESP_SPP_OPEN_EVT:
            ESP_LOGI(TAG, "SPP connection opened, handle %d", param->open.handle);
            xSemaphoreTake(platform->spp_mutex, portMAX_DELAY);
            platform->handle = param->open.handle;
            platform->is_connected = true;
            memcpy(platform->remote_addr, param->open.bda, 6);
            xSemaphoreGive(platform->spp_mutex);
            g_bt_instance->state = HAL_BT_STATE_CONNECTED;
            break;
        case ESP_SPP_CLOSE_EVT:
            ESP_LOGI(TAG, "SPP connection closed");
            xSemaphoreTake(platform->spp_mutex, portMAX_DELAY);
            platform->handle = 0;
            platform->is_connected = false;
            xSemaphoreGive(platform->spp_mutex);
            g_bt_instance->state = HAL_BT_STATE_READY;
            break;
        case ESP_SPP_DATA_IND_EVT:
            ESP_LOGI(TAG, "SPP data received, len %d", param->data_ind.len);
            if (g_bt_instance->data_callback) {
                g_bt_instance->data_callback(param->data_ind.data, param->data_ind.len, g_bt_instance->data_context);
            }
            break;
        default:
            break;
    }
}

static void bt_inquiry_scan_result(esp_bt_gap_cb_param_t *param) {
    if (!g_bt_instance || g_bt_instance->device_count >= HAL_BT_MAX_DEVICES) return;
    
    hal_bt_device_t *device = &g_bt_instance->devices[g_bt_instance->device_count];
    
    memcpy(device->addr, param->disc_res.bda, 6);
    device->rssi = param->disc_res.rssi;
    device->connectable = (param->disc_res.eir[0] & 0x04) != 0;
    device->last_seen_ms = hal_timer_get_ms();
    
    char *name = (char *)param->disc_res.eir;
    if (name && strlen(name) > 0) {
        strncpy(device->name, name, HAL_BT_MAX_NAME_LEN);
        device->name[HAL_BT_MAX_NAME_LEN] = '\0';
    } else {
        device->name[0] = '\0';
    }
    
    g_bt_instance->device_count++;
    
    if (g_bt_instance->device_callback) {
        g_bt_instance->device_callback(device, true, g_bt_instance->device_context);
    }
    
    ESP_LOGI(TAG, "Found device: %s, RSSI: %d", device->name, device->rssi);
}

#else

hal_status_t hal_esp32_bt_register(hal_bt_t *bt) {
    return HAL_UNSUPPORTED;
}

#endif
