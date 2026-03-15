#ifndef HAL_BT_H
#define HAL_BT_H
#include "hal_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HAL_BT_MAX_NAME_LEN         32
#define HAL_BT_MAX_PIN_LEN          16
#define HAL_BT_MAX_DEVICES          10
#define HAL_BT_BUFFER_SIZE          1024

typedef enum {
    HAL_BT_STATE_OFF = 0,
    HAL_BT_STATE_INITIALIZING = 1,
    HAL_BT_STATE_READY = 2,
    HAL_BT_STATE_SCANNING = 3,
    HAL_BT_STATE_ADVERTISING = 4,
    HAL_BT_STATE_CONNECTED = 5,
    HAL_BT_STATE_ERROR = 6
} hal_bt_state_t;

typedef enum {
    HAL_BT_MODE_OFF = 0,
    HAL_BT_MODE_CLASSIC = 1,
    HAL_BT_MODE_BLE = 2,
    HAL_BT_MODE_DUAL = 3
} hal_bt_mode_t;

typedef enum {
    HAL_BT_ROLE_PERIPHERAL = 0,
    HAL_BT_ROLE_CENTRAL = 1
} hal_bt_role_t;

typedef struct {
    uint8_t addr[6];
    char name[HAL_BT_MAX_NAME_LEN + 1];
    int8_t rssi;
    bool connectable;
    uint32_t last_seen_ms;
} hal_bt_device_t;

typedef struct {
    char name[HAL_BT_MAX_NAME_LEN + 1];
    char pin[HAL_BT_MAX_PIN_LEN + 1];
    bool discoverable;
    bool connectable;
    uint16_t advertising_interval_ms;
    uint8_t advertising_data[31];
    uint8_t advertising_data_len;
} hal_bt_config_t;

typedef struct hal_bt_s hal_bt_t;

typedef struct {
    hal_status_t (*init)(hal_bt_t *bt);
    hal_status_t (*deinit)(hal_bt_t *bt);
    hal_status_t (*set_mode)(hal_bt_t *bt, hal_bt_mode_t mode);
    hal_status_t (*get_state)(hal_bt_t *bt, hal_bt_state_t *state);
    hal_status_t (*set_name)(hal_bt_t *bt, const char *name);
    hal_status_t (*get_name)(hal_bt_t *bt, char *name, size_t len);
    hal_status_t (*start_scan)(hal_bt_t *bt);
    hal_status_t (*stop_scan)(hal_bt_t *bt);
    hal_status_t (*get_scan_results)(hal_bt_t *bt, hal_bt_device_t *devices, uint8_t *count);
    hal_status_t (*connect)(hal_bt_t *bt, const uint8_t *addr);
    hal_status_t (*disconnect)(hal_bt_t *bt);
    hal_status_t (*is_connected)(hal_bt_t *bt, bool *connected);
    hal_status_t (*start_advertising)(hal_bt_t *bt, const hal_bt_config_t *config);
    hal_status_t (*stop_advertising)(hal_bt_t *bt);
    hal_status_t (*send_data)(hal_bt_t *bt, const uint8_t *data, size_t len);
    hal_status_t (*set_data_callback)(hal_bt_t *bt, void (*callback)(const uint8_t *data, size_t len, void *context), void *context);
    hal_status_t (*set_device_callback)(hal_bt_t *bt, void (*callback)(const hal_bt_device_t *device, bool discovered, void *context), void *context);
} hal_bt_ops_t;

struct hal_bt_s {
    const hal_bt_ops_t *ops;
    hal_bt_mode_t mode;
    hal_bt_state_t state;
    hal_bt_role_t role;
    hal_bt_config_t config;
    hal_bt_device_t devices[HAL_BT_MAX_DEVICES];
    uint8_t device_count;
    uint8_t rx_buffer[HAL_BT_BUFFER_SIZE];
    size_t rx_len;
    bool initialized;
    void (*data_callback)(const uint8_t *data, size_t len, void *context);
    void *data_context;
    void (*device_callback)(const hal_bt_device_t *device, bool discovered, void *context);
    void *device_context;
    void *platform_data;
};

hal_status_t hal_bt_init(hal_bt_t *bt);
hal_status_t hal_bt_deinit(hal_bt_t *bt);
hal_status_t hal_bt_register_driver(hal_bt_t *bt, const hal_bt_ops_t *ops);
hal_status_t hal_bt_set_mode(hal_bt_t *bt, hal_bt_mode_t mode);
hal_status_t hal_bt_get_state(hal_bt_t *bt, hal_bt_state_t *state);
hal_status_t hal_bt_set_name(hal_bt_t *bt, const char *name);
hal_status_t hal_bt_scan_devices(hal_bt_t *bt, hal_bt_device_t *devices, uint8_t *count);
hal_status_t hal_bt_connect_device(hal_bt_t *bt, const uint8_t *addr);
hal_status_t hal_bt_disconnect(hal_bt_t *bt);
hal_status_t hal_bt_start_telemetry_service(hal_bt_t *bt);
hal_status_t hal_bt_send_telemetry(hal_bt_t *bt, const uint8_t *data, size_t len);
hal_status_t hal_bt_set_data_callback(hal_bt_t *bt, void (*callback)(const uint8_t *data, size_t len, void *context), void *context);

#ifdef __cplusplus
}
#endif

#endif
