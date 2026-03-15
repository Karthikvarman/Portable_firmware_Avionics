#include "hal_gps.h"
#include "hal_uart.h"
#include "hal_timer.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    hal_uart_t uart;
    uint32_t last_update_ms;
    bool ublox_configured;
} esp32_gps_platform_t;

static hal_status_t esp32_gps_init(hal_gps_t *gps, uint32_t uart_id, uint32_t baudrate);
static hal_status_t esp32_gps_deinit(hal_gps_t *gps);
static hal_status_t esp32_gps_set_protocol(hal_gps_t *gps, hal_gps_protocol_t protocol);
static hal_status_t esp32_gps_set_update_rate(hal_gps_t *gps, uint16_t rate_hz);
static hal_status_t esp32_gps_configure_ublox(hal_gps_t *gps);
static hal_status_t esp32_gps_read_data(hal_gps_t *gps, uint8_t *data, size_t len);
static hal_status_t esp32_gps_process_nmea(hal_gps_t *gps, const char *nmea);
static hal_status_t esp32_gps_process_ublox(hal_gps_t *gps, const uint8_t *ublox, size_t len);
static hal_status_t esp32_gps_get_position(hal_gps_t *gps, hal_gps_data_t *data);
static hal_status_t esp32_gps_get_time(hal_gps_t *gps, hal_gps_time_t *time);
static hal_status_t esp32_gps_reset(hal_gps_t *gps);
static hal_status_t esp32_gps_sleep(hal_gps_t *gps);
static hal_status_t esp32_gps_wakeup(hal_gps_t *gps);

static const hal_gps_ops_t esp32_gps_ops = {
    .init = esp32_gps_init,
    .deinit = esp32_gps_deinit,
    .set_protocol = esp32_gps_set_protocol,
    .set_update_rate = esp32_gps_set_update_rate,
    .configure_ublox = esp32_gps_configure_ublox,
    .read_data = esp32_gps_read_data,
    .process_nmea = esp32_gps_process_nmea,
    .process_ublox = esp32_gps_process_ublox,
    .get_position = esp32_gps_get_position,
    .get_time = esp32_gps_get_time,
    .reset = esp32_gps_reset,
    .sleep = esp32_gps_sleep,
    .wakeup = esp32_gps_wakeup
};

hal_status_t hal_esp32_gps_register(hal_gps_t *gps) {
    HAL_CHECK_PTR(gps);
    return hal_gps_register_driver(gps, &esp32_gps_ops);
}

static hal_status_t esp32_gps_init(hal_gps_t *gps, uint32_t uart_id, uint32_t baudrate) {
    HAL_CHECK_PTR(gps);
    
    esp32_gps_platform_t *platform = malloc(sizeof(esp32_gps_platform_t));
    if (!platform) return HAL_IO_ERROR;
    
    gps->platform_data = platform;
    platform->ublox_configured = false;
    
    hal_status_t status = hal_uart_init(&platform->uart, uart_id, baudrate);
    if (status != HAL_OK) {
        free(platform);
        return status;
    }
    
    gps->uart_id = uart_id;
    gps->baudrate = baudrate;
    gps->protocol = HAL_GPS_PROTOCOL_AUTO;
    gps->initialized = true;
    
    return HAL_OK;
}

static hal_status_t esp32_gps_deinit(hal_gps_t *gps) {
    HAL_CHECK_PTR(gps);
    
    esp32_gps_platform_t *platform = (esp32_gps_platform_t *)gps->platform_data;
    if (platform) {
        hal_uart_deinit(&platform->uart);
        free(platform);
        gps->platform_data = NULL;
    }
    
    gps->initialized = false;
    return HAL_OK;
}

static hal_status_t esp32_gps_set_protocol(hal_gps_t *gps, hal_gps_protocol_t protocol) {
    HAL_CHECK_PTR(gps);
    gps->protocol = protocol;
    return HAL_OK;
}

static hal_status_t esp32_gps_set_update_rate(hal_gps_t *gps, uint16_t rate_hz) {
    HAL_CHECK_PTR(gps);
    
    esp32_gps_platform_t *platform = (esp32_gps_platform_t *)gps->platform_data;
    if (!platform) return HAL_NOT_INIT;
    
    if (gps->protocol == HAL_GPS_PROTOCOL_UBLOX && !platform->ublox_configured) {
        return esp32_gps_configure_ublox(gps);
    }
    
    return HAL_OK;
}

static hal_status_t esp32_gps_configure_ublox(hal_gps_t *gps) {
    HAL_CHECK_PTR(gps);
    
    esp32_gps_platform_t *platform = (esp32_gps_platform_t *)gps->platform_data;
    if (!platform) return HAL_NOT_INIT;
    
    uint8_t cfg_rate[] = {
        0xB5, 0x62, 0x06, 0x08, 0x06, 0x00, 0x64, 0x32, 0x01, 0x00, 0x01, 0x00, 0x7A, 0x12
    };
    
    uint8_t cfg_msg[] = {
        0xB5, 0x62, 0x06, 0x01, 0x08, 0x00, 0xF0, 0x00, 0x00, 0x01, 0x00, 0x01, 0x01, 0x01, 0x31, 0xE0
    };
    
    hal_uart_write(&platform->uart, cfg_rate, sizeof(cfg_rate));
    hal_timer_delay_ms(100);
    hal_uart_write(&platform->uart, cfg_msg, sizeof(cfg_msg));
    hal_timer_delay_ms(100);
    
    platform->ublox_configured = true;
    return HAL_OK;
}

static hal_status_t esp32_gps_read_data(hal_gps_t *gps, uint8_t *data, size_t len) {
    HAL_CHECK_PTR(gps);
    HAL_CHECK_PTR(data);
    
    esp32_gps_platform_t *platform = (esp32_gps_platform_t *)gps->platform_data;
    if (!platform) return HAL_NOT_INIT;
    
    size_t read = hal_uart_read(&platform->uart, data, len);
    return (read > 0) ? HAL_OK : HAL_TIMEOUT;
}

static hal_status_t esp32_gps_process_nmea(hal_gps_t *gps, const char *nmea) {
    HAL_CHECK_PTR(gps);
    HAL_CHECK_PTR(nmea);
    
    if (strncmp(nmea, "$GPGGA", 6) == 0) {
        char *token;
        char *rest = (char *)nmea;
        int field = 0;
        
        while ((token = strtok_r(rest, ",", &rest)) && field <= 9) {
            switch (field) {
                case 1: if (strlen(token) > 0) {
                    gps->last_data.timestamp_ms = hal_timer_get_ms();
                    gps->last_data.valid = true;
                } break;
                case 2: if (strlen(token) > 0) {
                    double lat = atof(token);
                    int deg = (int)(lat / 100);
                    double min = lat - (deg * 100);
                    gps->last_data.latitude = deg + (min / 60.0);
                } break;
                case 3: if (strlen(token) > 0 && token[0] == 'S') {
                    gps->last_data.latitude = -gps->last_data.latitude;
                } break;
                case 4: if (strlen(token) > 0) {
                    double lon = atof(token);
                    int deg = (int)(lon / 100);
                    double min = lon - (deg * 100);
                    gps->last_data.longitude = deg + (min / 60.0);
                } break;
                case 5: if (strlen(token) > 0 && token[0] == 'W') {
                    gps->last_data.longitude = -gps->last_data.longitude;
                } break;
                case 6: if (strlen(token) > 0) {
                    int fix = atoi(token);
                    gps->last_data.fix_type = (fix > 0) ? HAL_GPS_FIX_3D : HAL_GPS_FIX_NONE;
                } break;
                case 7: if (strlen(token) > 0) {
                    gps->last_data.satellites_used = (uint8_t)atoi(token);
                } break;
                case 8: if (strlen(token) > 0) {
                    gps->last_data.hdop = (float)atof(token);
                } break;
                case 9: if (strlen(token) > 0) {
                    gps->last_data.altitude_msl = (float)atof(token);
                } break;
            }
            field++;
        }
    }
    else if (strncmp(nmea, "$GPRMC", 6) == 0) {
        char *token;
        char *rest = (char *)nmea;
        int field = 0;
        
        while ((token = strtok_r(rest, ",", &rest)) && field <= 7) {
            switch (field) {
                case 7: if (strlen(token) > 0) {
                    gps->last_data.ground_speed_kph = (float)atof(token) * 1.852f;
                } break;
                case 8: if (strlen(token) > 0) {
                    gps->last_data.ground_course_deg = (float)atof(token);
                } break;
            }
            field++;
        }
    }
    
    return HAL_OK;
}

static hal_status_t esp32_gps_process_ublox(hal_gps_t *gps, const uint8_t *ublox, size_t len) {
    HAL_CHECK_PTR(gps);
    HAL_CHECK_PTR(ublox);
    
    if (len < 8) return HAL_PARAM_ERROR;
    
    if (ublox[0] == 0xB5 && ublox[1] == 0x62) {
        uint8_t class_id = ublox[2];
        uint8_t msg_id = ublox[3];
        uint16_t payload_len = ublox[4] | (ublox[5] << 8);
        
        if (class_id == 0x01 && msg_id == 0x07) {
            if (payload_len >= 92 && len >= (8 + payload_len)) {
                gps->last_data.timestamp_ms = hal_timer_get_ms();
                gps->last_data.fix_type = (hal_gps_fix_type_t)ublox[20];
                gps->last_data.longitude = ((int32_t)ublox[24] | ((int32_t)ublox[25] << 8) | ((int32_t)ublox[26] << 16) | ((int32_t)ublox[27] << 24)) * 1e-7;
                gps->last_data.latitude = ((int32_t)ublox[28] | ((int32_t)ublox[29] << 8) | ((int32_t)ublox[30] << 16) | ((int32_t)ublox[31] << 24)) * 1e-7;
                gps->last_data.altitude_msl = ((int32_t)ublox[36] | ((int32_t)ublox[37] << 8) | ((int32_t)ublox[38] << 16) | ((int32_t)ublox[39] << 24)) * 1e-3;
                gps->last_data.ground_speed_kph = ((int32_t)ublox[60] | ((int32_t)ublox[61] << 8) | ((int32_t)ublox[62] << 16) | ((int32_t)ublox[63] << 24)) * 1e-3 * 3.6f;
                gps->last_data.ground_course_deg = ((int32_t)ublox[64] | ((int32_t)ublox[65] << 8) | ((int32_t)ublox[66] << 16) | ((int32_t)ublox[67] << 24)) * 1e-5;
                gps->last_data.hdop = ((uint32_t)ublox[76] | ((uint32_t)ublox[77] << 8) | ((uint32_t)ublox[78] << 16) | ((uint32_t)ublox[79] << 24)) * 1e-2;
                gps->last_data.satellites_used = ublox[23];
                gps->last_data.valid = (gps->last_data.fix_type > HAL_GPS_FIX_NONE);
            }
        }
    }
    
    return HAL_OK;
}

static hal_status_t esp32_gps_get_position(hal_gps_t *gps, hal_gps_data_t *data) {
    HAL_CHECK_PTR(gps);
    HAL_CHECK_PTR(data);
    
    if (!gps->initialized) return HAL_NOT_INIT;
    
    *data = gps->last_data;
    return HAL_OK;
}

static hal_status_t esp32_gps_get_time(hal_gps_t *gps, hal_gps_time_t *time) {
    HAL_CHECK_PTR(gps);
    HAL_CHECK_PTR(time);
    
    if (!gps->initialized) return HAL_NOT_INIT;
    
    *time = gps->last_time;
    return HAL_OK;
}

static hal_status_t esp32_gps_reset(hal_gps_t *gps) {
    HAL_CHECK_PTR(gps);
    
    esp32_gps_platform_t *platform = (esp32_gps_platform_t *)gps->platform_data;
    if (!platform) return HAL_NOT_INIT;
    
    uint8_t reset_cmd[] = {0xB5, 0x62, 0x06, 0x04, 0x04, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x0C, 0x6D};
    
    hal_uart_write(&platform->uart, reset_cmd, sizeof(reset_cmd));
    hal_timer_delay_ms(1000);
    
    platform->ublox_configured = false;
    return HAL_OK;
}

static hal_status_t esp32_gps_sleep(hal_gps_t *gps) {
    HAL_CHECK_PTR(gps);
    
    esp32_gps_platform_t *platform = (esp32_gps_platform_t *)gps->platform_data;
    if (!platform) return HAL_NOT_INIT;
    
    uint8_t sleep_cmd[] = {0xB5, 0x62, 0x02, 0x41, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x4C, 0x37};
    
    hal_uart_write(&platform->uart, sleep_cmd, sizeof(sleep_cmd));
    return HAL_OK;
}

static hal_status_t esp32_gps_wakeup(hal_gps_t *gps) {
    HAL_CHECK_PTR(gps);
    
    esp32_gps_platform_t *platform = (esp32_gps_platform_t *)gps->platform_data;
    if (!platform) return HAL_NOT_INIT;
    
    uint8_t wakeup_cmd[] = {0xB5, 0x62, 0x02, 0x41, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x4B, 0x35};
    
    hal_uart_write(&platform->uart, wakeup_cmd, sizeof(wakeup_cmd));
    hal_timer_delay_ms(200);
    
    return HAL_OK;
}
