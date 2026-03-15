#include "rtos_tasks.h"
#include "sal_gps.h"
#include "hal_timer.h"
#include "hal_uart.h"
#include <string.h>
#include <stdlib.h>

static rtos_task_manager_t g_task_mgr = {0};

hal_status_t rtos_tasks_init(void) {
    memset(&g_task_mgr, 0, sizeof(rtos_task_manager_t));
    
    g_task_mgr.gps_data_queue = os_queue_create(10, sizeof(sal_gnss_data_t));
    g_task_mgr.wifi_data_queue = os_queue_create(10, 256);
    g_task_mgr.bt_data_queue = os_queue_create(10, 256);
    g_task_mgr.telemetry_queue = os_queue_create(20, 256);
    
    g_task_mgr.gps_mutex = os_mutex_create();
    g_task_mgr.wifi_mutex = os_mutex_create();
    g_task_mgr.bt_mutex = os_mutex_create();
    
    if (!g_task_mgr.gps_data_queue || !g_task_mgr.wifi_data_queue || 
        !g_task_mgr.bt_data_queue || !g_task_mgr.telemetry_queue ||
        !g_task_mgr.gps_mutex || !g_task_mgr.wifi_mutex || !g_task_mgr.bt_mutex) {
        rtos_tasks_deinit();
        return HAL_IO_ERROR;
    }
    
    g_task_mgr.gps_update_rate_ms = 100;
    g_task_mgr.wifi_update_rate_ms = 1000;
    g_task_mgr.bt_update_rate_ms = 1000;
    g_task_mgr.telemetry_update_rate_ms = 50;
    g_task_mgr.telemetry_mode = RTOS_TELEMETRY_MODE_USB;
    
    return HAL_OK;
}

hal_status_t rtos_tasks_deinit(void) {
    rtos_gps_task_stop();
    rtos_wifi_task_stop();
    rtos_bt_task_stop();
    rtos_telemetry_task_stop();
    
    if (g_task_mgr.gps_data_queue) os_queue_delete(g_task_mgr.gps_data_queue);
    if (g_task_mgr.wifi_data_queue) os_queue_delete(g_task_mgr.wifi_data_queue);
    if (g_task_mgr.bt_data_queue) os_queue_delete(g_task_mgr.bt_data_queue);
    if (g_task_mgr.telemetry_queue) os_queue_delete(g_task_mgr.telemetry_queue);
    
    if (g_task_mgr.gps_mutex) os_mutex_delete(g_task_mgr.gps_mutex);
    if (g_task_mgr.wifi_mutex) os_mutex_delete(g_task_mgr.wifi_mutex);
    if (g_task_mgr.bt_mutex) os_mutex_delete(g_task_mgr.bt_mutex);
    
    memset(&g_task_mgr, 0, sizeof(rtos_task_manager_t));
    
    return HAL_OK;
}

hal_status_t rtos_gps_task_start(void) {
    if (g_task_mgr.gps_enabled) return HAL_OK;
    
    hal_status_t status = sal_gps_init(1, 9600);
    if (status != HAL_OK) return status;
    
    status = os_task_create("GPS_Task", rtos_gps_task_fn, NULL, 
                           RTOS_GPS_TASK_PRIORITY, RTOS_GPS_TASK_STACK_SIZE, 
                           &g_task_mgr.gps_task);
    if (status != HAL_OK) {
        sal_gps_deinit();
        return status;
    }
    
    g_task_mgr.gps_enabled = true;
    return HAL_OK;
}

hal_status_t rtos_gps_task_stop(void) {
    if (!g_task_mgr.gps_enabled) return HAL_OK;
    
    if (g_task_mgr.gps_task) {
        os_task_delete(g_task_mgr.gps_task);
        g_task_mgr.gps_task = NULL;
    }
    
    sal_gps_deinit();
    g_task_mgr.gps_enabled = false;
    
    return HAL_OK;
}

hal_status_t rtos_gps_set_update_rate(uint16_t rate_hz) {
    if (rate_hz == 0) return HAL_PARAM_ERROR;
    
    g_task_mgr.gps_update_rate_ms = 1000 / rate_hz;
    return sal_gps_set_update_rate(rate_hz);
}

hal_status_t rtos_wifi_task_start(void) {
    if (g_task_mgr.wifi_enabled) return HAL_OK;
    
    hal_status_t status = hal_esp32_wifi_register(&g_task_mgr.wifi);
    if (status != HAL_OK) return status;
    
    status = hal_wifi_init(&g_task_mgr.wifi);
    if (status != HAL_OK) return status;
    
    status = os_task_create("WiFi_Task", rtos_wifi_task_fn, NULL,
                           RTOS_WIFI_TASK_PRIORITY, RTOS_WIFI_TASK_STACK_SIZE,
                           &g_task_mgr.wifi_task);
    if (status != HAL_OK) {
        hal_wifi_deinit(&g_task_mgr.wifi);
        return status;
    }
    
    g_task_mgr.wifi_enabled = true;
    return HAL_OK;
}

hal_status_t rtos_wifi_task_stop(void) {
    if (!g_task_mgr.wifi_enabled) return HAL_OK;
    
    if (g_task_mgr.wifi_task) {
        os_task_delete(g_task_mgr.wifi_task);
        g_task_mgr.wifi_task = NULL;
    }
    
    hal_wifi_deinit(&g_task_mgr.wifi);
    g_task_mgr.wifi_enabled = false;
    
    return HAL_OK;
}

hal_status_t rtos_wifi_connect(const char *ssid, const char *password) {
    if (!g_task_mgr.wifi_enabled) return HAL_NOT_INIT;
    
    os_mutex_lock(g_task_mgr.wifi_mutex, OS_WAIT_FOREVER);
    hal_status_t status = hal_wifi_sta_connect(&g_task_mgr.wifi, ssid, password);
    os_mutex_unlock(g_task_mgr.wifi_mutex);
    
    return status;
}

hal_status_t rtos_wifi_start_ap(const char *ssid, const char *password, uint8_t channel) {
    if (!g_task_mgr.wifi_enabled) return HAL_NOT_INIT;
    
    os_mutex_lock(g_task_mgr.wifi_mutex, OS_WAIT_FOREVER);
    hal_status_t status = hal_wifi_ap_start(&g_task_mgr.wifi, ssid, password, channel);
    os_mutex_unlock(g_task_mgr.wifi_mutex);
    
    return status;
}

hal_status_t rtos_bt_task_start(void) {
    if (g_task_mgr.bt_enabled) return HAL_OK;
    
    hal_status_t status = hal_esp32_bt_register(&g_task_mgr.bt);
    if (status != HAL_OK) return status;
    
    status = hal_bt_init(&g_task_mgr.bt);
    if (status != HAL_OK) return status;
    
    status = os_task_create("BT_Task", rtos_bt_task_fn, NULL,
                           RTOS_BT_TASK_PRIORITY, RTOS_BT_TASK_STACK_SIZE,
                           &g_task_mgr.bt_task);
    if (status != HAL_OK) {
        hal_bt_deinit(&g_task_mgr.bt);
        return status;
    }
    
    g_task_mgr.bt_enabled = true;
    return HAL_OK;
}

hal_status_t rtos_bt_task_stop(void) {
    if (!g_task_mgr.bt_enabled) return HAL_OK;
    
    if (g_task_mgr.bt_task) {
        os_task_delete(g_task_mgr.bt_task);
        g_task_mgr.bt_task = NULL;
    }
    
    hal_bt_deinit(&g_task_mgr.bt);
    g_task_mgr.bt_enabled = false;
    
    return HAL_OK;
}

hal_status_t rtos_bt_start_telemetry_service(void) {
    if (!g_task_mgr.bt_enabled) return HAL_NOT_INIT;
    
    return hal_bt_start_telemetry_service(&g_task_mgr.bt);
}

hal_status_t rtos_telemetry_task_start(void) {
    if (g_task_mgr.telemetry_enabled) return HAL_OK;
    
    hal_status_t status = os_task_create("Telemetry_Task", rtos_telemetry_task_fn, NULL,
                                        RTOS_TELEMETRY_TASK_PRIORITY, RTOS_TELEMETRY_TASK_STACK_SIZE,
                                        &g_task_mgr.telemetry_task);
    if (status != HAL_OK) return status;
    
    g_task_mgr.telemetry_enabled = true;
    return HAL_OK;
}

hal_status_t rtos_telemetry_task_stop(void) {
    if (!g_task_mgr.telemetry_enabled) return HAL_OK;
    
    if (g_task_mgr.telemetry_task) {
        os_task_delete(g_task_mgr.telemetry_task);
        g_task_mgr.telemetry_task = NULL;
    }
    
    g_task_mgr.telemetry_enabled = false;
    return HAL_OK;
}

hal_status_t rtos_telemetry_set_mode(rtos_telemetry_mode_t mode) {
    g_task_mgr.telemetry_mode = mode;
    return HAL_OK;
}

hal_status_t rtos_telemetry_send_data(const uint8_t *data, size_t len) {
    if (!g_task_mgr.telemetry_enabled) return HAL_NOT_INIT;
    
    uint8_t buffer[256];
    if (len > sizeof(buffer) - 1) len = sizeof(buffer) - 1;
    
    memcpy(buffer, data, len);
    buffer[len] = 0;
    
    return os_queue_send(g_task_mgr.telemetry_queue, buffer, OS_NO_WAIT) ? HAL_OK : HAL_IO_ERROR;
}

void rtos_gps_task_fn(void *arg) {
    sal_gnss_data_t gps_data;
    uint32_t last_update = 0;
    
    while (g_task_mgr.gps_enabled) {
        uint32_t current_time = os_get_tick_ms();
        
        if (current_time - last_update >= g_task_mgr.gps_update_rate_ms) {
            if (sal_gps_read(&gps_data) == HAL_OK) {
                os_queue_send(g_task_mgr.gps_data_queue, &gps_data, OS_NO_WAIT);
            }
            last_update = current_time;
        }
        
        os_task_delay_ms(10);
    }
    
    os_task_delete(NULL);
}

void rtos_wifi_task_fn(void *arg) {
    uint8_t data[256];
    hal_wifi_state_t state;
    
    while (g_task_mgr.wifi_enabled) {
        os_mutex_lock(g_task_mgr.wifi_mutex, OS_WAIT_FOREVER);
        
        if (hal_wifi_get_state(&g_task_mgr.wifi, &state) == HAL_OK) {
            if (state == HAL_WIFI_STATE_CONNECTED) {
                if (os_queue_recv(g_task_mgr.wifi_data_queue, data, 0)) {
                    hal_wifi_send_telemetry(&g_task_mgr.wifi, data, strlen((char *)data));
                }
            }
        }
        
        os_mutex_unlock(g_task_mgr.wifi_mutex);
        os_task_delay_ms(g_task_mgr.wifi_update_rate_ms);
    }
    
    os_task_delete(NULL);
}

void rtos_bt_task_fn(void *arg) {
    uint8_t data[256];
    bool connected;
    
    while (g_task_mgr.bt_enabled) {
        os_mutex_lock(g_task_mgr.bt_mutex, OS_WAIT_FOREVER);
        
        if (hal_bt_is_connected(&g_task_mgr.bt, &connected) == HAL_OK) {
            if (connected) {
                if (os_queue_recv(g_task_mgr.bt_data_queue, data, 0)) {
                    hal_bt_send_telemetry(&g_task_mgr.bt, data, strlen((char *)data));
                }
            }
        }
        
        os_mutex_unlock(g_task_mgr.bt_mutex);
        os_task_delay_ms(g_task_mgr.bt_update_rate_ms);
    }
    
    os_task_delete(NULL);
}

void rtos_telemetry_task_fn(void *arg) {
    uint8_t data[256];
    sal_gnss_data_t gps_data;
    
    while (g_task_mgr.telemetry_enabled) {
        uint32_t current_time = os_get_tick_ms();
        
        if (os_queue_recv(g_task_mgr.gps_data_queue, &gps_data, 0)) {
            snprintf((char *)data, sizeof(data), 
                    "$AERO,GPS,%.6f,%.6f,%.2f,%.2f,%.2f,%u,%u,%.1f,%.1f*%02X\n",
                    gps_data.latitude_deg, gps_data.longitude_deg, gps_data.altitude_m,
                    gps_data.speed_mps, gps_data.course_deg, gps_data.num_satellites,
                    gps_data.fix_type, gps_data.hdop, gps_data.vdop, 0);
            
            switch (g_task_mgr.telemetry_mode) {
                case RTOS_TELEMETRY_MODE_WIFI:
                    if (g_task_mgr.wifi_enabled) {
                        os_queue_send(g_task_mgr.wifi_data_queue, data, OS_NO_WAIT);
                    }
                    break;
                    
                case RTOS_TELEMETRY_MODE_BT:
                    if (g_task_mgr.bt_enabled) {
                        os_queue_send(g_task_mgr.bt_data_queue, data, OS_NO_WAIT);
                    }
                    break;
                    
                case RTOS_TELEMETRY_MODE_AUTO:
                    if (g_task_mgr.wifi_enabled) {
                        os_queue_send(g_task_mgr.wifi_data_queue, data, OS_NO_WAIT);
                    } else if (g_task_mgr.bt_enabled) {
                        os_queue_send(g_task_mgr.bt_data_queue, data, OS_NO_WAIT);
                    }
                    break;
                    
                case RTOS_TELEMETRY_MODE_USB:
                default:
                    hal_uart_write(&g_uart1, data, strlen((char *)data));
                    break;
            }
        }
        
        if (os_queue_recv(g_task_mgr.telemetry_queue, data, 0)) {
            switch (g_task_mgr.telemetry_mode) {
                case RTOS_TELEMETRY_MODE_WIFI:
                    if (g_task_mgr.wifi_enabled) {
                        os_queue_send(g_task_mgr.wifi_data_queue, data, OS_NO_WAIT);
                    }
                    break;
                    
                case RTOS_TELEMETRY_MODE_BT:
                    if (g_task_mgr.bt_enabled) {
                        os_queue_send(g_task_mgr.bt_data_queue, data, OS_NO_WAIT);
                    }
                    break;
                    
                case RTOS_TELEMETRY_MODE_AUTO:
                    if (g_task_mgr.wifi_enabled) {
                        os_queue_send(g_task_mgr.wifi_data_queue, data, OS_NO_WAIT);
                    } else if (g_task_mgr.bt_enabled) {
                        os_queue_send(g_task_mgr.bt_data_queue, data, OS_NO_WAIT);
                    }
                    break;
                    
                case RTOS_TELEMETRY_MODE_USB:
                default:
                    hal_uart_write(&g_uart1, data, strlen((char *)data));
                    break;
            }
        }
        
        os_task_delay_ms(g_task_mgr.telemetry_update_rate_ms);
    }
    
    os_task_delete(NULL);
}
