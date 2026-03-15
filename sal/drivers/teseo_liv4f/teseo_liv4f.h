#ifndef TESSEO_LIV4F_H
#define TESSEO_LIV4F_H

#include "hal_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Teseo-LIV4F configuration
typedef struct {
    uint32_t uart_baudrate;
    uint8_t update_rate_hz;
    bool enable_nmea;
    bool enable_ubx;
} teseo_config_t;

// Teseo-LIV4F functions
hal_status_t teseo_init(const teseo_config_t *config);
hal_status_t teseo_deinit(void);
hal_status_t teseo_read(uint8_t *data, size_t *len);
hal_status_t teseo_process_nmea(const char *nmea);
hal_status_t teseo_set_update_rate(uint8_t rate_hz);

#ifdef __cplusplus
}
#endif

#endif
