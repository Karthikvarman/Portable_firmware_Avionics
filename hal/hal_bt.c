#include "hal_bt.h"
#include <string.h>
#include <stdlib.h>

hal_status_t hal_bt_init(hal_bt_t *bt) {
    HAL_CHECK_PTR(bt);
    HAL_CHECK_PTR(bt->ops);
    
    memset(bt, 0, sizeof(hal_bt_t));
    
    return bt->ops->init(bt);
}

hal_status_t hal_bt_deinit(hal_bt_t *bt) {
    HAL_CHECK_PTR(bt);
    
    if (bt->ops && bt->ops->deinit) {
        return bt->ops->deinit(bt);
    }
    
    return HAL_OK;
}

hal_status_t hal_bt_register_driver(hal_bt_t *bt, const hal_bt_ops_t *ops) {
    HAL_CHECK_PTR(bt);
    HAL_CHECK_PTR(ops);
    
    bt->ops = ops;
    return HAL_OK;
}

hal_status_t hal_bt_set_mode(hal_bt_t *bt, hal_bt_mode_t mode) {
    HAL_CHECK_PTR(bt);
    
    if (!bt->initialized) return HAL_NOT_INIT;
    if (!bt->ops->set_mode) return HAL_UNSUPPORTED;
    
    return bt->ops->set_mode(bt, mode);
}

hal_status_t hal_bt_get_state(hal_bt_t *bt, hal_bt_state_t *state) {
    HAL_CHECK_PTR(bt);
    HAL_CHECK_PTR(state);
    
    if (!bt->initialized) return HAL_NOT_INIT;
    if (!bt->ops->get_state) return HAL_UNSUPPORTED;
    
    return bt->ops->get_state(bt, state);
}

hal_status_t hal_bt_set_name(hal_bt_t *bt, const char *name) {
    HAL_CHECK_PTR(bt);
    HAL_CHECK_PTR(name);
    
    if (!bt->initialized) return HAL_NOT_INIT;
    if (!bt->ops->set_name) return HAL_UNSUPPORTED;
    
    return bt->ops->set_name(bt, name);
}

hal_status_t hal_bt_scan_devices(hal_bt_t *bt, hal_bt_device_t *devices, uint8_t *count) {
    HAL_CHECK_PTR(bt);
    
    if (!bt->initialized) return HAL_NOT_INIT;
    if (!bt->ops->start_scan || !bt->ops->get_scan_results) return HAL_UNSUPPORTED;
    
    hal_status_t status = bt->ops->start_scan(bt);
    if (status != HAL_OK) return status;
    
    return bt->ops->get_scan_results(bt, devices, count);
}

hal_status_t hal_bt_connect_device(hal_bt_t *bt, const uint8_t *addr) {
    HAL_CHECK_PTR(bt);
    HAL_CHECK_PTR(addr);
    
    if (!bt->initialized) return HAL_NOT_INIT;
    if (!bt->ops->connect) return HAL_UNSUPPORTED;
    
    return bt->ops->connect(bt, addr);
}

hal_status_t hal_bt_disconnect(hal_bt_t *bt) {
    HAL_CHECK_PTR(bt);
    
    if (!bt->initialized) return HAL_NOT_INIT;
    if (!bt->ops->disconnect) return HAL_UNSUPPORTED;
    
    return bt->ops->disconnect(bt);
}

hal_status_t hal_bt_start_telemetry_service(hal_bt_t *bt) {
    HAL_CHECK_PTR(bt);
    
    if (!bt->initialized) return HAL_NOT_INIT;
    if (!bt->ops->start_advertising) return HAL_UNSUPPORTED;
    
    hal_bt_config_t config = {0};
    strncpy(config.name, "AeroFirmware", HAL_BT_MAX_NAME_LEN);
    config.discoverable = true;
    config.connectable = true;
    config.advertising_interval_ms = 100;
    
    return bt->ops->start_advertising(bt, &config);
}

hal_status_t hal_bt_send_telemetry(hal_bt_t *bt, const uint8_t *data, size_t len) {
    HAL_CHECK_PTR(bt);
    HAL_CHECK_PTR(data);
    
    if (!bt->initialized) return HAL_NOT_INIT;
    if (!bt->ops->send_data) return HAL_UNSUPPORTED;
    
    return bt->ops->send_data(bt, data, len);
}

hal_status_t hal_bt_set_data_callback(hal_bt_t *bt, void (*callback)(const uint8_t *data, size_t len, void *context), void *context) {
    HAL_CHECK_PTR(bt);
    
    if (!bt->initialized) return HAL_NOT_INIT;
    if (!bt->ops->set_data_callback) return HAL_UNSUPPORTED;
    
    bt->data_callback = callback;
    bt->data_context = context;
    
    return bt->ops->set_data_callback(bt, callback, context);
}
