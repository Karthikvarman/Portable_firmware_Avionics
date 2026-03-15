#ifndef RTOS_TASKS_H
#define RTOS_TASKS_H
#include "os_api.h"
#include "sal_sensor.h"
#include "hal_wifi.h"
#include "hal_bt.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RTOS_GPS_TASK_STACK_SIZE    4096
#define RTOS_WIFI_TASK_STACK_SIZE   4096
#define RTOS_BT_TASK_STACK_SIZE     4096
#define RTOS_TELEMETRY_TASK_STACK_SIZE 4096

#define RTOS_GPS_TASK_PRIORITY      5
#define RTOS_WIFI_TASK_PRIORITY     4
#define RTOS_BT_TASK_PRIORITY       4
#define RTOS_TELEMETRY_TASK_PRIORITY 6

typedef enum {
    RTOS_TELEMETRY_MODE_USB = 0,
    RTOS_TELEMETRY_MODE_WIFI = 1,
    RTOS_TELEMETRY_MODE_BT = 2,
    RTOS_TELEMETRY_MODE_AUTO = 3
} rtos_telemetry_mode_t;

typedef struct {
    os_task_handle_t gps_task;
    os_task_handle_t wifi_task;
    os_task_handle_t bt_task;
    os_task_handle_t telemetry_task;
    
    os_queue_handle_t gps_data_queue;
    os_queue_handle_t wifi_data_queue;
    os_queue_handle_t bt_data_queue;
    os_queue_handle_t telemetry_queue;
    
    os_mutex_handle_t gps_mutex;
    os_mutex_handle_t wifi_mutex;
    os_mutex_handle_t bt_mutex;
    
    bool gps_enabled;
    bool wifi_enabled;
    bool bt_enabled;
    bool telemetry_enabled;
    
    rtos_telemetry_mode_t telemetry_mode;
    
    uint32_t gps_update_rate_ms;
    uint32_t wifi_update_rate_ms;
    uint32_t bt_update_rate_ms;
    uint32_t telemetry_update_rate_ms;
    
    hal_wifi_t wifi;
    hal_bt_t bt;
    
} rtos_task_manager_t;

hal_status_t rtos_tasks_init(void);
hal_status_t rtos_tasks_deinit(void);

hal_status_t rtos_gps_task_start(void);
hal_status_t rtos_gps_task_stop(void);
hal_status_t rtos_gps_set_update_rate(uint16_t rate_hz);

hal_status_t rtos_wifi_task_start(void);
hal_status_t rtos_wifi_task_stop(void);
hal_status_t rtos_wifi_connect(const char *ssid, const char *password);
hal_status_t rtos_wifi_start_ap(const char *ssid, const char *password, uint8_t channel);

hal_status_t rtos_bt_task_start(void);
hal_status_t rtos_bt_task_stop(void);
hal_status_t rtos_bt_start_telemetry_service(void);

hal_status_t rtos_telemetry_task_start(void);
hal_status_t rtos_telemetry_task_stop(void);
hal_status_t rtos_telemetry_set_mode(rtos_telemetry_mode_t mode);
hal_status_t rtos_telemetry_send_data(const uint8_t *data, size_t len);

void rtos_gps_task_fn(void *arg);
void rtos_wifi_task_fn(void *arg);
void rtos_bt_task_fn(void *arg);
void rtos_telemetry_task_fn(void *arg);

#ifdef __cplusplus
}
#endif

#endif
