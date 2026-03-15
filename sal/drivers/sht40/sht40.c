#include "sht40.h"
#include "sal_sensor.h"
#include "hal_i2c.h"
#include "hal_timer.h"
#include "aero_config.h"
#include <string.h>
#define SHT40_CMD_MEAS_HIGH         0xFD  
#define SHT40_CMD_MEAS_MED          0xF6  
#define SHT40_CMD_MEAS_LOW          0xE0  
#define SHT40_CMD_HEAT_200mW_1s     0x39
#define SHT40_CMD_HEAT_200mW_100ms  0x32
#define SHT40_CMD_HEAT_110mW_1s     0x2F
#define SHT40_CMD_HEAT_110mW_100ms  0x24
#define SHT40_CMD_HEAT_20mW_1s      0x1E
#define SHT40_CMD_HEAT_20mW_100ms   0x15
#define SHT40_CMD_SOFT_RESET        0x94
#define SHT40_CMD_READ_SERIAL       0x89
#define SHT40_MEAS_DELAY_HIGH_MS    20   
typedef struct {
    hal_i2c_bus_t i2c_bus;
    uint8_t       i2c_addr;
    uint32_t      serial;
} sht40_priv_t;
static sht40_priv_t s_sht40_priv;
static uint8_t sht40_crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : (crc << 1);
        }
    }
    return crc;
}
static hal_status_t sht40_send_cmd(sht40_priv_t *p, uint8_t cmd)
{
    return hal_i2c_write(p->i2c_bus, p->i2c_addr, cmd, NULL, 0);
}
static hal_status_t sht40_init(sal_driver_t *drv)
{
    sht40_priv_t *p = (sht40_priv_t *)drv->priv;
    hal_status_t rc = sht40_send_cmd(p, SHT40_CMD_SOFT_RESET);
    if (rc != HAL_OK) {
        AERO_LOGE("SHT40", "Not responding on I2C addr 0x%02X", p->i2c_addr);
        return HAL_NOT_FOUND;
    }
    hal_delay_ms(2);
    rc = sht40_send_cmd(p, SHT40_CMD_READ_SERIAL);
    if (rc == HAL_OK) {
        hal_delay_ms(2);
        uint8_t buf[6];
        rc = hal_i2c_read(p->i2c_bus, p->i2c_addr, 0, buf, 6);
        if (rc == HAL_OK) {
            p->serial = (uint32_t)buf[0] << 24 | buf[1] << 16 |
                        (uint32_t)buf[3] << 8  | buf[4];
            AERO_LOGI("SHT40", "Serial: 0x%08lX", p->serial);
        }
    }
    AERO_LOGI("SHT40", "Init OK on addr 0x%02X", p->i2c_addr);
    return HAL_OK;
}
static hal_status_t sht40_read(sal_driver_t *drv, sal_data_t *out)
{
    sht40_priv_t *p = (sht40_priv_t *)drv->priv;
    hal_status_t rc = sht40_send_cmd(p, SHT40_CMD_MEAS_HIGH);
    if (rc != HAL_OK) return rc;
    hal_delay_ms(SHT40_MEAS_DELAY_HIGH_MS);
    uint8_t buf[6];
    rc = hal_i2c_read(p->i2c_bus, p->i2c_addr, 0, buf, 6);
    if (rc != HAL_OK) return rc;
    if (sht40_crc8(buf, 2) != buf[2]) {
        AERO_LOGW("SHT40", "Temperature CRC mismatch");
        return HAL_IO_ERROR;
    }
    if (sht40_crc8(buf + 3, 2) != buf[5]) {
        AERO_LOGW("SHT40", "Humidity CRC mismatch");
        return HAL_IO_ERROR;
    }
    uint16_t t_raw  = (uint16_t)buf[0] << 8 | buf[1];
    uint16_t rh_raw = (uint16_t)buf[3] << 8 | buf[4];
    float temp_c = -45.0f + 175.0f * (float)t_raw  / 65535.0f;
    float rh_pct = -6.0f  + 125.0f * (float)rh_raw / 65535.0f;
    if (rh_pct < 0.0f)   rh_pct = 0.0f;
    if (rh_pct > 100.0f) rh_pct = 100.0f;
    out->env.temperature_c = temp_c;
    out->env.humidity_pct  = rh_pct;
    return HAL_OK;
}
static hal_status_t sht40_sleep(sal_driver_t *drv)
{
    return HAL_OK;
}
static sal_driver_t s_sht40_accel_driver = {
    .name      = "SHT40-AD1B-R2",
    .id        = SAL_SENSOR_TEMPERATURE,
    .init      = sht40_init,
    .self_test = NULL,
    .read      = sht40_read,
    .set_odr   = NULL,
    .sleep     = sht40_sleep,
    .priv      = &s_sht40_priv,
};
    s_sht40_priv.i2c_bus  = bus;
    s_sht40_priv.i2c_addr = addr;
    return sal_register(&s_sht40_accel_driver);
}