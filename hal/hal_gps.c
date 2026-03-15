#include "hal_gps.h"
#include <string.h>
#include <stdlib.h>

static hal_status_t hal_gps_process_buffer(hal_gps_t *gps);

hal_status_t hal_gps_init(hal_gps_t *gps, uint32_t uart_id, uint32_t baudrate) {
    HAL_CHECK_PTR(gps);
    HAL_CHECK_PTR(gps->ops);
    
    memset(gps, 0, sizeof(hal_gps_t));
    gps->uart_id = uart_id;
    gps->baudrate = baudrate;
    gps->protocol = HAL_GPS_PROTOCOL_AUTO;
    
    return gps->ops->init(gps, uart_id, baudrate);
}

hal_status_t hal_gps_deinit(hal_gps_t *gps) {
    HAL_CHECK_PTR(gps);
    
    if (gps->ops && gps->ops->deinit) {
        return gps->ops->deinit(gps);
    }
    
    return HAL_OK;
}

hal_status_t hal_gps_register_driver(hal_gps_t *gps, const hal_gps_ops_t *ops) {
    HAL_CHECK_PTR(gps);
    HAL_CHECK_PTR(ops);
    
    gps->ops = ops;
    return HAL_OK;
}

hal_status_t hal_gps_set_protocol(hal_gps_t *gps, hal_gps_protocol_t protocol) {
    HAL_CHECK_PTR(gps);
    
    if (!gps->initialized) return HAL_NOT_INIT;
    if (!gps->ops->set_protocol) return HAL_UNSUPPORTED;
    
    gps->protocol = protocol;
    return gps->ops->set_protocol(gps, protocol);
}

hal_status_t hal_gps_set_update_rate(hal_gps_t *gps, uint16_t rate_hz) {
    HAL_CHECK_PTR(gps);
    
    if (!gps->initialized) return HAL_NOT_INIT;
    if (!gps->ops->set_update_rate) return HAL_UNSUPPORTED;
    
    return gps->ops->set_update_rate(gps, rate_hz);
}

hal_status_t hal_gps_configure_neo_f10(hal_gps_t *gps) {
    HAL_CHECK_PTR(gps);
    
    if (!gps->initialized) return HAL_NOT_INIT;
    
    gps->protocol = HAL_GPS_PROTOCOL_UBLOX;
    
    if (gps->ops->configure_ublox) {
        return gps->ops->configure_ublox(gps);
    }
    
    return HAL_OK;
}

hal_status_t hal_gps_process_data(hal_gps_t *gps, const uint8_t *data, size_t len) {
    HAL_CHECK_PTR(gps);
    HAL_CHECK_PTR(data);
    
    if (!gps->initialized) return HAL_NOT_INIT;
    
    for (size_t i = 0; i < len; i++) {
        if (gps->rx_len < HAL_GPS_BUFFER_SIZE - 1) {
            gps->rx_buffer[gps->rx_len++] = data[i];
        } else {
            gps->rx_len = 0;
        }
        
        if (data[i] == '\n' || gps->rx_len >= HAL_GPS_BUFFER_SIZE - 1) {
            hal_gps_process_buffer(gps);
        }
    }
    
    return HAL_OK;
}

static hal_status_t hal_gps_process_buffer(hal_gps_t *gps) {
    if (gps->rx_len == 0) return HAL_OK;
    
    gps->rx_buffer[gps->rx_len] = '\0';
    
    if (gps->protocol == HAL_GPS_PROTOCOL_AUTO) {
        if (gps->rx_buffer[0] == '$' && gps->rx_len > 6) {
            gps->protocol = HAL_GPS_PROTOCOL_NMEA;
        } else if (gps->rx_len > 2 && gps->rx_buffer[0] == 0xB5 && gps->rx_buffer[1] == 0x62) {
            gps->protocol = HAL_GPS_PROTOCOL_UBLOX;
        }
    }
    
    if (gps->protocol == HAL_GPS_PROTOCOL_NMEA && gps->ops->process_nmea) {
        gps->ops->process_nmea(gps, (const char *)gps->rx_buffer);
    } else if (gps->protocol == HAL_GPS_PROTOCOL_UBLOX && gps->ops->process_ublox) {
        gps->ops->process_ublox(gps, gps->rx_buffer, gps->rx_len);
    }
    
    gps->rx_len = 0;
    return HAL_OK;
}

hal_status_t hal_gps_get_position(hal_gps_t *gps, hal_gps_data_t *data) {
    HAL_CHECK_PTR(gps);
    HAL_CHECK_PTR(data);
    
    if (!gps->initialized) return HAL_NOT_INIT;
    if (!gps->ops->get_position) return HAL_UNSUPPORTED;
    
    return gps->ops->get_position(gps, data);
}

hal_status_t hal_gps_get_time(hal_gps_t *gps, hal_gps_time_t *time) {
    HAL_CHECK_PTR(gps);
    HAL_CHECK_PTR(time);
    
    if (!gps->initialized) return HAL_NOT_INIT;
    if (!gps->ops->get_time) return HAL_UNSUPPORTED;
    
    return gps->ops->get_time(gps, time);
}

hal_status_t hal_gps_reset(hal_gps_t *gps) {
    HAL_CHECK_PTR(gps);
    
    if (!gps->initialized) return HAL_NOT_INIT;
    if (!gps->ops->reset) return HAL_UNSUPPORTED;
    
    return gps->ops->reset(gps);
}

hal_status_t hal_gps_sleep(hal_gps_t *gps) {
    HAL_CHECK_PTR(gps);
    
    if (!gps->initialized) return HAL_NOT_INIT;
    if (!gps->ops->sleep) return HAL_UNSUPPORTED;
    
    return gps->ops->sleep(gps);
}

hal_status_t hal_gps_wakeup(hal_gps_t *gps) {
    HAL_CHECK_PTR(gps);
    
    if (!gps->initialized) return HAL_NOT_INIT;
    if (!gps->ops->wakeup) return HAL_UNSUPPORTED;
    
    return gps->ops->wakeup(gps);
}
