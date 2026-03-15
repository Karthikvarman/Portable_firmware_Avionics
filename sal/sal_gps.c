#include "sal_sensor.h"
#include "hal_gps.h"
#include "hal_timer.h"
#include <string.h>
#include <stdlib.h>

typedef struct {
    hal_gps_t gps;
    uint32_t last_update_ms;
    uint32_t update_interval_ms;
    bool initialized;
    sal_gnss_data_t last_data;
} sal_gps_context_t;

static sal_gps_context_t g_gps_ctx = {0};

hal_status_t sal_gps_init(uint32_t uart_id, uint32_t baudrate) {
    if (g_gps_ctx.initialized) return HAL_OK;
    
    hal_status_t status = hal_esp32_gps_register(&g_gps_ctx.gps);
    if (status != HAL_OK) return status;
    
    status = hal_gps_init(&g_gps_ctx.gps, uart_id, baudrate);
    if (status != HAL_OK) return status;
    
    status = hal_gps_configure_neo_f10(&g_gps_ctx.gps);
    if (status != HAL_OK) return status;
    
    status = hal_gps_set_update_rate(&g_gps_ctx.gps, 10);
    if (status != HAL_OK) return status;
    
    g_gps_ctx.update_interval_ms = 100;
    g_gps_ctx.last_update_ms = 0;
    g_gps_ctx.initialized = true;
    
    memset(&g_gps_ctx.last_data, 0, sizeof(sal_gnss_data_t));
    
    return HAL_OK;
}

hal_status_t sal_gps_deinit(void) {
    if (!g_gps_ctx.initialized) return HAL_OK;
    
    hal_gps_deinit(&g_gps_ctx.gps);
    g_gps_ctx.initialized = false;
    
    return HAL_OK;
}

hal_status_t sal_gps_read(sal_gnss_data_t *data) {
    HAL_CHECK_PTR(data);
    
    if (!g_gps_ctx.initialized) return HAL_NOT_INIT;
    
    uint32_t current_time = hal_timer_get_ms();
    if (current_time - g_gps_ctx.last_update_ms < g_gps_ctx.update_interval_ms) {
        *data = g_gps_ctx.last_data;
        return HAL_OK;
    }
    
    hal_gps_data_t gps_data;
    hal_status_t status = hal_gps_get_position(&g_gps_ctx.gps, &gps_data);
    if (status != HAL_OK) return status;
    
    g_gps_ctx.last_data.latitude_deg = gps_data.latitude;
    g_gps_ctx.last_data.longitude_deg = gps_data.longitude;
    g_gps_ctx.last_data.altitude_m = gps_data.altitude_msl;
    g_gps_ctx.last_data.speed_mps = gps_data.ground_speed_kph / 3.6f;
    g_gps_ctx.last_data.course_deg = gps_data.ground_course_deg;
    g_gps_ctx.last_data.hdop = gps_data.hdop;
    g_gps_ctx.last_data.vdop = gps_data.vdop;
    g_gps_ctx.last_data.num_satellites = gps_data.satellites_used;
    g_gps_ctx.last_data.fix_type = (uint8_t)gps_data.fix_type;
    g_gps_ctx.last_data.valid = gps_data.valid;
    
    g_gps_ctx.last_update_ms = current_time;
    *data = g_gps_ctx.last_data;
    
    return HAL_OK;
}

hal_status_t sal_gps_process_data(const uint8_t *data, size_t len) {
    HAL_CHECK_PTR(data);
    
    if (!g_gps_ctx.initialized) return HAL_NOT_INIT;
    
    return hal_gps_process_data(&g_gps_ctx.gps, data, len);
}

hal_status_t sal_gps_set_update_rate(uint16_t rate_hz) {
    if (!g_gps_ctx.initialized) return HAL_NOT_INIT;
    
    if (rate_hz == 0) return HAL_PARAM_ERROR;
    
    g_gps_ctx.update_interval_ms = 1000 / rate_hz;
    
    return hal_gps_set_update_rate(&g_gps_ctx.gps, rate_hz);
}

hal_status_t sal_gps_reset(void) {
    if (!g_gps_ctx.initialized) return HAL_NOT_INIT;
    
    hal_status_t status = hal_gps_reset(&g_gps_ctx.gps);
    if (status != HAL_OK) return status;
    
    status = hal_gps_configure_neo_f10(&g_gps_ctx.gps);
    if (status != HAL_OK) return status;
    
    memset(&g_gps_ctx.last_data, 0, sizeof(sal_gnss_data_t));
    g_gps_ctx.last_update_ms = 0;
    
    return HAL_OK;
}

hal_status_t sal_gps_sleep(void) {
    if (!g_gps_ctx.initialized) return HAL_NOT_INIT;
    
    return hal_gps_sleep(&g_gps_ctx.gps);
}

hal_status_t sal_gps_wakeup(void) {
    if (!g_gps_ctx.initialized) return HAL_NOT_INIT;
    
    return hal_gps_wakeup(&g_gps_ctx.gps);
}

bool sal_gps_is_initialized(void) {
    return g_gps_ctx.initialized;
}

uint32_t sal_gps_get_last_update_time(void) {
    return g_gps_ctx.last_update_ms;
}
