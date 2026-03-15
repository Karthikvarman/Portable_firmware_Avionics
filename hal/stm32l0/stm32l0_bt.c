#include "hal_bt.h"
#include "hal_timer.h"
#include <string.h>
#include <stdlib.h>

#ifdef TARGET_STM32L0
#include "stm32l0xx_hal.h"
#include "stm32l0xx_ll_usart.h"
#include "stm32l0xx_ll_gpio.h"
#endif

typedef struct {
    bool initialized;
    hal_bt_mode_t mode;
    hal_bt_state_t state;
    uint8_t rx_buffer[HAL_BT_BUFFER_SIZE];
    size_t rx_len;
    void (*data_callback)(const uint8_t *data, size_t len, void *context);
    void *callback_context;
    void (*device_callback)(const hal_bt_device_t *device, bool discovered, void *context);
    void *device_context;
} stm32l0_bt_platform_t;

static hal_status_t stm32l0_bt_init(hal_bt_t *bt);
static hal_status_t stm32l0_bt_deinit(hal_bt_t *bt);
static hal_status_t stm32l0_bt_set_mode(hal_bt_t *bt, hal_bt_mode_t mode);
static hal_status_t stm32l0_bt_get_state(hal_bt_t *bt, hal_bt_state_t *state);
static hal_status_t stm32l0_bt_set_name(hal_bt_t *bt, const char *name);
static hal_status_t stm32l0_bt_get_name(hal_bt_t *bt, char *name, size_t len);
static hal_status_t stm32l0_bt_start_scan(hal_bt_t *bt);
static hal_status_t stm32l0_bt_stop_scan(hal_bt_t *bt);
static hal_status_t stm32l0_bt_get_scan_results(hal_bt_t *bt, hal_bt_device_t *devices, uint8_t *count);
static hal_status_t stm32l0_bt_connect(hal_bt_t *bt, const uint8_t *addr);
static hal_status_t stm32l0_bt_disconnect(hal_bt_t *bt);
static hal_status_t stm32l0_bt_is_connected(hal_bt_t *bt, bool *connected);
static hal_status_t stm32l0_bt_start_advertising(hal_bt_t *bt, const hal_bt_config_t *config);
static hal_status_t stm32l0_bt_stop_advertising(hal_bt_t *bt);
static hal_status_t stm32l0_bt_send_data(hal_bt_t *bt, const uint8_t *data, size_t len);
static hal_status_t stm32l0_bt_set_data_callback(hal_bt_t *bt, void (*callback)(const uint8_t *data, size_t len, void *context), void *context);
static hal_status_t stm32l0_bt_set_device_callback(hal_bt_t *bt, void (*callback)(const hal_bt_device_t *device, bool discovered, void *context), void *context);

static const hal_bt_ops_t stm32l0_bt_ops = {
    .init = stm32l0_bt_init,
    .deinit = stm32l0_bt_deinit,
    .set_mode = stm32l0_bt_set_mode,
    .get_state = stm32l0_bt_get_state,
    .set_name = stm32l0_bt_set_name,
    .get_name = stm32l0_bt_get_name,
    .start_scan = stm32l0_bt_start_scan,
    .stop_scan = stm32l0_bt_stop_scan,
    .get_scan_results = stm32l0_bt_get_scan_results,
    .connect = stm32l0_bt_connect,
    .disconnect = stm32l0_bt_disconnect,
    .is_connected = stm32l0_bt_is_connected,
    .start_advertising = stm32l0_bt_start_advertising,
    .stop_advertising = stm32l0_bt_stop_advertising,
    .send_data = stm32l0_bt_send_data,
    .set_data_callback = stm32l0_bt_set_data_callback,
    .set_device_callback = stm32l0_bt_set_device_callback
};

hal_status_t hal_stm32l0_bt_register(hal_bt_t *bt) {
    HAL_CHECK_PTR(bt);
    return hal_bt_register_driver(bt, &stm32l0_bt_ops);
}

static hal_status_t stm32l0_bt_init(hal_bt_t *bt) {
    HAL_CHECK_PTR(bt);
    
    stm32l0_bt_platform_t *platform = malloc(sizeof(stm32l0_bt_platform_t));
    if (!platform) return HAL_IO_ERROR;
    
    memset(platform, 0, sizeof(stm32l0_bt_platform_t));
    bt->platform_data = platform;
    
    platform->initialized = true;
    bt->mode = HAL_BT_MODE_OFF;
    bt->state = HAL_BT_STATE_OFF;
    
    return HAL_OK;
}

static hal_status_t stm32l0_bt_deinit(hal_bt_t *bt) {
    HAL_CHECK_PTR(bt);
    
    stm32l0_bt_platform_t *platform = (stm32l0_bt_platform_t *)bt->platform_data;
    if (!platform) return HAL_NOT_INIT;
    
    free(platform);
    bt->platform_data = NULL;
    platform->initialized = false;
    
    return HAL_OK;
}

static hal_status_t stm32l0_bt_set_mode(hal_bt_t *bt, hal_bt_mode_t mode) {
    HAL_CHECK_PTR(bt);
    
    stm32l0_bt_platform_t *platform = (stm32l0_bt_platform_t *)bt->platform_data;
    if (!platform || !platform->initialized) return HAL_NOT_INIT;
    
    bt->mode = mode;
    return HAL_OK;
}

static hal_status_t stm32l0_bt_get_state(hal_bt_t *bt, hal_bt_state_t *state) {
    HAL_CHECK_PTR(bt);
    HAL_CHECK_PTR(state);
    
    stm32l0_bt_platform_t *platform = (stm32l0_bt_platform_t *)bt->platform_data;
    if (!platform || !platform->initialized) return HAL_NOT_INIT;
    
    *state = bt->state;
    return HAL_OK;
}

static hal_status_t stm32l0_bt_set_name(hal_bt_t *bt, const char *name) {
    HAL_CHECK_PTR(bt);
    HAL_CHECK_PTR(name);
    
    stm32l0_bt_platform_t *platform = (stm32l0_bt_platform_t *)bt->platform_data;
    if (!platform || !platform->initialized) return HAL_NOT_INIT;
    
    return HAL_OK;
}

static hal_status_t stm32l0_bt_get_name(hal_bt_t *bt, char *name, size_t len) {
    HAL_CHECK_PTR(bt);
    HAL_CHECK_PTR(name);
    
    stm32l0_bt_platform_t *platform = (stm32l0_bt_platform_t *)bt->platform_data;
    if (!platform || !platform->initialized) return HAL_NOT_INIT;
    
    strncpy(name, "AeroFirmware", len - 1);
    name[len - 1] = '\0';
    
    return HAL_OK;
}

static hal_status_t stm32l0_bt_start_scan(hal_bt_t *bt) {
    HAL_CHECK_PTR(bt);
    
    stm32l0_bt_platform_t *platform = (stm32l0_bt_platform_t *)bt->platform_data;
    if (!platform || !platform->initialized) return HAL_NOT_INIT;
    
    return HAL_OK;
}

static hal_status_t stm32l0_bt_stop_scan(hal_bt_t *bt) {
    HAL_CHECK_PTR(bt);
    
    stm32l0_bt_platform_t *platform = (stm32l0_bt_platform_t *)bt->platform_data;
    if (!platform || !platform->initialized) return HAL_NOT_INIT;
    
    return HAL_OK;
}

static hal_status_t stm32l0_bt_get_scan_results(hal_bt_t *bt, hal_bt_device_t *devices, uint8_t *count) {
    HAL_CHECK_PTR(bt);
    HAL_CHECK_PTR(devices);
    HAL_CHECK_PTR(count);
    
    stm32l0_bt_platform_t *platform = (stm32l0_bt_platform_t *)bt->platform_data;
    if (!platform || !platform->initialized) return HAL_NOT_INIT;
    
    *count = 0;
    return HAL_OK;
}

static hal_status_t stm32l0_bt_connect(hal_bt_t *bt, const uint8_t *addr) {
    HAL_CHECK_PTR(bt);
    HAL_CHECK_PTR(addr);
    
    stm32l0_bt_platform_t *platform = (stm32l0_bt_platform_t *)bt->platform_data;
    if (!platform || !platform->initialized) return HAL_NOT_INIT;
    
    return HAL_OK;
}

static hal_status_t stm32l0_bt_disconnect(hal_bt_t *bt) {
    HAL_CHECK_PTR(bt);
    
    stm32l0_bt_platform_t *platform = (stm32l0_bt_platform_t *)bt->platform_data;
    if (!platform || !platform->initialized) return HAL_NOT_INIT;
    
    return HAL_OK;
}

static hal_status_t stm32l0_bt_is_connected(hal_bt_t *bt, bool *connected) {
    HAL_CHECK_PTR(bt);
    HAL_CHECK_PTR(connected);
    
    stm32l0_bt_platform_t *platform = (stm32l0_bt_platform_t *)bt->platform_data;
    if (!platform || !platform->initialized) return HAL_NOT_INIT;
    
    *connected = (bt->state == HAL_BT_STATE_CONNECTED);
    return HAL_OK;
}

static hal_status_t stm32l0_bt_start_advertising(hal_bt_t *bt, const hal_bt_config_t *config) {
    HAL_CHECK_PTR(bt);
    HAL_CHECK_PTR(config);
    
    stm32l0_bt_platform_t *platform = (stm32l0_bt_platform_t *)bt->platform_data;
    if (!platform || !platform->initialized) return HAL_NOT_INIT;
    
    return HAL_OK;
}

static hal_status_t stm32l0_bt_stop_advertising(hal_bt_t *bt) {
    HAL_CHECK_PTR(bt);
    
    stm32l0_bt_platform_t *platform = (stm32l0_bt_platform_t *)bt->platform_data;
    if (!platform || !platform->initialized) return HAL_NOT_INIT;
    
    return HAL_OK;
}

static hal_status_t stm32l0_bt_send_data(hal_bt_t *bt, const uint8_t *data, size_t len) {
    HAL_CHECK_PTR(bt);
    HAL_CHECK_PTR(data);
    
    stm32l0_bt_platform_t *platform = (stm32l0_bt_platform_t *)bt->platform_data;
    if (!platform || !platform->initialized) return HAL_NOT_INIT;
    
    return HAL_OK;
}

static hal_status_t stm32l0_bt_set_data_callback(hal_bt_t *bt, void (*callback)(const uint8_t *data, size_t len, void *context), void *context) {
    HAL_CHECK_PTR(bt);
    
    stm32l0_bt_platform_t *platform = (stm32l0_bt_platform_t *)bt->platform_data;
    if (!platform || !platform->initialized) return HAL_NOT_INIT;
    
    platform->data_callback = callback;
    platform->callback_context = context;
    
    return HAL_OK;
}

static hal_status_t stm32l0_bt_set_device_callback(hal_bt_t *bt, void (*callback)(const hal_bt_device_t *device, bool discovered, void *context), void *context) {
    HAL_CHECK_PTR(bt);
    
    stm32l0_bt_platform_t *platform = (stm32l0_bt_platform_t *)bt->platform_data;
    if (!platform || !platform->initialized) return HAL_NOT_INIT;
    
    platform->device_callback = callback;
    platform->device_context = context;
    
    return HAL_OK;
}
