#ifndef SAL_GPS_H
#define SAL_GPS_H

#include "hal_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// GPS update rate configuration
typedef enum {
    SAL_GPS_RATE_1HZ   = 1,
    SAL_GPS_RATE_2HZ   = 2,
    SAL_GPS_RATE_5HZ   = 5,
    SAL_GPS_RATE_10HZ  = 10,
    SAL_GPS_RATE_20HZ  = 20
} sal_gps_rate_t;

// GPS fix types
typedef enum {
    SAL_GPS_FIX_NONE = 0,
    SAL_GPS_FIX_2D   = 1,
    SAL_GPS_FIX_3D   = 2,
    SAL_GPS_FIX_RTK  = 3
} sal_gps_fix_type_t;

// GPS data structure (compatible with existing sal_gnss_data_t)
typedef struct {
    double latitude_deg;
    double longitude_deg;
    float  altitude_m;
    float  speed_mps;
    float  course_deg;
    float  hdop;
    float  vdop;
    uint8_t num_satellites;
    uint8_t fix_type;
    bool    valid;
    uint32_t timestamp_ms;
} sal_gps_data_t;

// GPS configuration structure
typedef struct {
    sal_gps_rate_t update_rate;
    uint32_t uart_baudrate;
    bool enable_rtcm;
    uint8_t min_sats_for_3d;
    float min_hdop_for_3d;
} sal_gps_config_t;

// SAL GPS functions
hal_status_t sal_gps_init(const sal_gps_config_t *config);
hal_status_t sal_gps_deinit(void);
hal_status_t sal_gps_read(sal_gps_data_t *data);
hal_status_t sal_gps_set_update_rate(sal_gps_rate_t rate);
hal_status_t sal_gps_process_raw(const uint8_t *data, size_t len);
hal_status_t sal_gps_get_config(sal_gps_config_t *config);
bool sal_gps_is_healthy(void);

#ifdef __cplusplus
}
#endif

#endif // SAL_GPS_H
