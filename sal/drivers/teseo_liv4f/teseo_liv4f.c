#include "teseo_liv4f.h"
#include "sal_sensor.h"
#include "hal_uart.h"
#include "hal_i2c.h"
#include "hal_timer.h"
#include "aero_config.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#define NMEA_BUF_LEN    128
#define NMEA_MAX_FIELDS 20
typedef struct {
    int            iface;
    hal_uart_port_t uart_port;
    hal_i2c_bus_t  i2c_bus;
    uint8_t        i2c_addr;
    char           nmea_buf[NMEA_BUF_LEN];
    uint8_t        buf_idx;
    sal_gnss_data_t last_fix;
} teseo_priv_t;
static teseo_priv_t s_teseo_priv;
static uint8_t nmea_checksum(const char *sentence)
{
    uint8_t csum = 0;
    for (const char *p = sentence + 1; *p && *p != '*'; p++)
        csum ^= (uint8_t)*p;
    return csum;
}
static bool nmea_validate(const char *sentence)
{
    const char *star = strchr(sentence, '*');
    if (!star || strlen(star) < 3) return false;
    uint8_t expected = (uint8_t)strtol(star + 1, NULL, 16);
    return nmea_checksum(sentence) == expected;
}
static int nmea_split(char *buf, char *fields[], int max_fields)
{
    int count = 0;
    fields[count++] = buf;
    for (char *p = buf; *p && count < max_fields; p++) {
        if (*p == ',' || *p == '*') {
            *p = '\0';
            fields[count++] = p + 1;
        }
    }
    return count;
}
static double nmea_to_decimal(const char *val, char dir)
{
    if (!val || !*val) return 0.0;
    double raw = atof(val);
    int degrees = (int)(raw / 100);
    double minutes = raw - degrees * 100;
    double result = degrees + minutes / 60.0;
    if (dir == 'S' || dir == 'W') result = -result;
    return result;
}
static void parse_gga(teseo_priv_t *p, char *fields[], int n)
{
    if (n < 10) return;
    int fix = atoi(fields[6]);
    if (fix == 0) {
        p->last_fix.valid = false;
        return;
    }
    p->last_fix.latitude_deg  = nmea_to_decimal(fields[2], fields[3][0]);
    p->last_fix.longitude_deg = nmea_to_decimal(fields[4], fields[5][0]);
    p->last_fix.altitude_m    = (float)atof(fields[9]);
    p->last_fix.num_satellites= (uint8_t)atoi(fields[7]);
    p->last_fix.hdop          = (float)atof(fields[8]);
    p->last_fix.valid         = true;
}
static void parse_rmc(teseo_priv_t *p, char *fields[], int n)
{
    if (n < 8) return;
    if (fields[2][0] != 'A') return; 
    p->last_fix.speed_mps  = (float)atof(fields[7]) * 0.514444f; 
    p->last_fix.course_deg = (float)atof(fields[8]);
}
static void nmea_process_sentence(teseo_priv_t *p, char *sentence)
{
    if (!nmea_validate(sentence)) return;
    char scratch[NMEA_BUF_LEN];
    strncpy(scratch, sentence, NMEA_BUF_LEN - 1);
    char *fields[NMEA_MAX_FIELDS];
    int n = nmea_split(scratch, fields, NMEA_MAX_FIELDS);
    if (n < 1) return;
    if (strncmp(fields[0], "$GPGGA", 6) == 0 ||
        strncmp(fields[0], "$GNGGA", 6) == 0) {
        parse_gga(p, fields, n);
    } else if (strncmp(fields[0], "$GPRMC", 6) == 0 ||
               strncmp(fields[0], "$GNRMC", 6) == 0) {
        parse_rmc(p, fields, n);
    }
}
static hal_status_t teseo_read_sentence(teseo_priv_t *p)
{
    if (p->iface != AERO_IFACE_UART) return HAL_UNSUPPORTED;
    hal_tick_t start = hal_get_tick_ms();
    bool in_sentence = false;
    p->buf_idx = 0;
    while (hal_elapsed_ms(start) < 2000) {
        uint8_t c;
        hal_status_t rc = hal_uart_read_byte(p->uart_port, &c, 100);
        if (rc != HAL_OK) continue;
        if (c == '$') {
            in_sentence = true;
            p->buf_idx  = 0;
        }
        if (!in_sentence) continue;
        if (p->buf_idx < NMEA_BUF_LEN - 1) {
            p->nmea_buf[p->buf_idx++] = (char)c;
        }
        if (c == '\n') {
            p->nmea_buf[p->buf_idx] = '\0';
            nmea_process_sentence(p, p->nmea_buf);
            return HAL_OK;
        }
    }
    return HAL_TIMEOUT;
}
static hal_status_t teseo_init(sal_driver_t *drv)
{
    teseo_priv_t *p = (teseo_priv_t *)drv->priv;
    memset(&p->last_fix, 0, sizeof(sal_gnss_data_t));
    if (p->iface == AERO_IFACE_UART) {
        hal_uart_config_t ucfg = {
            .port       = p->uart_port,
            .baud_rate  = 115200,
            .data_bits  = 8,
            .parity     = HAL_UART_PARITY_NONE,
            .stop_bits  = HAL_UART_STOPBITS_1,
        };
        hal_status_t rc = hal_uart_init(&ucfg);
        if (rc != HAL_OK) return rc;
    }
    hal_tick_t start = hal_get_tick_ms();
    while (hal_elapsed_ms(start) < 3000) {
        if (teseo_read_sentence(p) == HAL_OK) {
            AERO_LOGI("TESEO", "GNSS module responding — NMEA OK");
            return HAL_OK;
        }
    }
    AERO_LOGE("TESEO", "No NMEA data from GNSS module");
    return HAL_TIMEOUT;
}
static hal_status_t teseo_read(sal_driver_t *drv, sal_data_t *out)
{
    teseo_priv_t *p = (teseo_priv_t *)drv->priv;
    for (int i = 0; i < 5; i++) {
        teseo_read_sentence(p);
    }
    memcpy(&out->gnss, &p->last_fix, sizeof(sal_gnss_data_t));
    return HAL_OK;
}
static sal_driver_t s_teseo_driver = {
    .name  = "Teseo-LIV4F",
    .id    = SAL_SENSOR_GNSS,
    .init  = teseo_init,
    .read  = teseo_read,
    .priv  = &s_teseo_priv,
};
hal_status_t teseo_driver_register(int iface, hal_uart_port_t uart_port,
                                    hal_i2c_bus_t i2c_bus, uint8_t i2c_addr)
{
    s_teseo_priv.uart_port = uart_port;
    s_teseo_priv.i2c_bus   = i2c_bus;
    s_teseo_priv.i2c_addr  = i2c_addr;
    return sal_register(&s_teseo_driver);
}