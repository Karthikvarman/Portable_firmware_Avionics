#ifndef HAL_GPS_H
#define HAL_GPS_H
#include "hal_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HAL_GPS_NMEA_MAX_LEN      256
#define HAL_GPS_UBLOX_MAX_LEN     512
#define HAL_GPS_BUFFER_SIZE       1024

typedef enum {
    HAL_GPS_PROTOCOL_NMEA = 0,
    HAL_GPS_PROTOCOL_UBLOX = 1,
    HAL_GPS_PROTOCOL_AUTO = 2
} hal_gps_protocol_t;

typedef enum {
    HAL_GPS_FIX_NONE = 0,
    HAL_GPS_FIX_2D = 1,
    HAL_GPS_FIX_3D = 2,
    HAL_GPS_FIX_RTCM = 3
} hal_gps_fix_type_t;

typedef struct {
    uint32_t timestamp_ms;
    hal_gps_fix_type_t fix_type;
    double latitude;
    double longitude;
    float altitude_msl;
    float altitude_geoid;
    float ground_speed_kph;
    float ground_course_deg;
    float hdop;
    float vdop;
    uint8_t satellites_used;
    uint8_t satellites_visible;
    bool valid;
} hal_gps_data_t;

typedef struct {
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    bool valid;
} hal_gps_time_t;

typedef struct hal_gps_s hal_gps_t;

typedef struct {
    hal_status_t (*init)(hal_gps_t *gps, uint32_t uart_id, uint32_t baudrate);
    hal_status_t (*deinit)(hal_gps_t *gps);
    hal_status_t (*set_protocol)(hal_gps_t *gps, hal_gps_protocol_t protocol);
    hal_status_t (*set_update_rate)(hal_gps_t *gps, uint16_t rate_hz);
    hal_status_t (*configure_ublox)(hal_gps_t *gps);
    hal_status_t (*read_data)(hal_gps_t *gps, uint8_t *data, size_t len);
    hal_status_t (*process_nmea)(hal_gps_t *gps, const char *nmea);
    hal_status_t (*process_ublox)(hal_gps_t *gps, const uint8_t *ublox, size_t len);
    hal_status_t (*get_position)(hal_gps_t *gps, hal_gps_data_t *data);
    hal_status_t (*get_time)(hal_gps_t *gps, hal_gps_time_t *time);
    hal_status_t (*reset)(hal_gps_t *gps);
    hal_status_t (*sleep)(hal_gps_t *gps);
    hal_status_t (*wakeup)(hal_gps_t *gps);
} hal_gps_ops_t;

struct hal_gps_s {
    const hal_gps_ops_t *ops;
    uint32_t uart_id;
    uint32_t baudrate;
    hal_gps_protocol_t protocol;
    hal_gps_data_t last_data;
    hal_gps_time_t last_time;
    uint8_t rx_buffer[HAL_GPS_BUFFER_SIZE];
    size_t rx_len;
    bool initialized;
    void *platform_data;
};

hal_status_t hal_gps_init(hal_gps_t *gps, uint32_t uart_id, uint32_t baudrate);
hal_status_t hal_gps_deinit(hal_gps_t *gps);
hal_status_t hal_gps_register_driver(hal_gps_t *gps, const hal_gps_ops_t *ops);
hal_status_t hal_gps_set_protocol(hal_gps_t *gps, hal_gps_protocol_t protocol);
hal_status_t hal_gps_set_update_rate(hal_gps_t *gps, uint16_t rate_hz);
hal_status_t hal_gps_configure_neo_f10(hal_gps_t *gps);
hal_status_t hal_gps_process_data(hal_gps_t *gps, const uint8_t *data, size_t len);
hal_status_t hal_gps_get_position(hal_gps_t *gps, hal_gps_data_t *data);
hal_status_t hal_gps_get_time(hal_gps_t *gps, hal_gps_time_t *time);
hal_status_t hal_gps_reset(hal_gps_t *gps);
hal_status_t hal_gps_sleep(hal_gps_t *gps);
hal_status_t hal_gps_wakeup(hal_gps_t *gps);

#ifdef __cplusplus
}
#endif

#endif
