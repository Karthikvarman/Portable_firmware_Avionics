#include "lps25hb.h"
#include "sal_sensor.h"
#include "hal_i2c.h"
#include "hal_spi.h"
#include "hal_timer.h"
#include "aero_config.h"
#include <math.h>
#include <string.h>
#define LPS25HB_REG_REF_P_XL        0x08
#define LPS25HB_REG_REF_P_L         0x09
#define LPS25HB_REG_REF_P_H         0x0A
#define LPS25HB_REG_WHO_AM_I        0x0F
#define LPS25HB_REG_RES_CONF        0x10
#define LPS25HB_REG_CTRL_REG1       0x20
#define LPS25HB_REG_CTRL_REG2       0x21
#define LPS25HB_REG_CTRL_REG3       0x22
#define LPS25HB_REG_CTRL_REG4       0x23
#define LPS25HB_REG_INT_CFG         0x24
#define LPS25HB_REG_INT_SRC         0x25
#define LPS25HB_REG_STATUS_REG      0x27
#define LPS25HB_REG_PRESS_OUT_XL    0x28
#define LPS25HB_REG_PRESS_OUT_L     0x29
#define LPS25HB_REG_PRESS_OUT_H     0x2A
#define LPS25HB_REG_TEMP_OUT_L      0x2B
#define LPS25HB_REG_TEMP_OUT_H      0x2C
#define LPS25HB_REG_FIFO_CTRL       0x2E
#define LPS25HB_REG_FIFO_STATUS     0x2F
#define LPS25HB_REG_THS_P_L         0x30
#define LPS25HB_REG_THS_P_H         0x31
#define LPS25HB_REG_RPDS_L          0x39
#define LPS25HB_REG_RPDS_H          0x3A
#define LPS25HB_WHOAMI_VALUE        0xBD
#define LPS25HB_CTRL1_ACTIVE        0xB4  
#define LPS25HB_P0_HPA              1013.25f
typedef struct {
    int          iface;          
    hal_i2c_bus_t i2c_bus;
    hal_spi_bus_t spi_bus;
    uint8_t      i2c_addr;
    float        ref_pressure_hpa;
} lps25hb_priv_t;
static lps25hb_priv_t s_lps25hb_priv;
static hal_status_t reg_write(lps25hb_priv_t *p, uint8_t reg, uint8_t val)
{
    if (p->iface == AERO_IFACE_SPI)
        return hal_spi_write_reg(p->spi_bus, reg, &val, 1);
    return hal_i2c_write_reg(p->i2c_bus, p->i2c_addr, reg, val);
}
static hal_status_t reg_read(lps25hb_priv_t *p, uint8_t reg,
                              uint8_t *buf, size_t len)
{
    if (p->iface == AERO_IFACE_SPI)
        return hal_spi_read_reg(p->spi_bus, reg | 0x80, buf, len);
    return hal_i2c_read(p->i2c_bus, p->i2c_addr, reg, buf, len);
}
static float pressure_to_altitude(float p_hpa, float p0_hpa)
{
    return 44330.0f * (1.0f - powf(p_hpa / p0_hpa, 0.1903f));
}
static hal_status_t lps25hb_init(sal_driver_t *drv)
{
    lps25hb_priv_t *p = (lps25hb_priv_t *)drv->priv;
    uint8_t whoami = 0;
    hal_status_t rc = reg_read(p, LPS25HB_REG_WHO_AM_I, &whoami, 1);
    if (rc != HAL_OK) {
        AERO_LOGE("LPS25HB", "Communication failed (rc=%d)", rc);
        return HAL_IO_ERROR;
    }
    if (whoami != LPS25HB_WHOAMI_VALUE) {
        AERO_LOGE("LPS25HB", "WHO_AM_I mismatch: got 0x%02X, expected 0x%02X",
                  whoami, LPS25HB_WHOAMI_VALUE);
        return HAL_NOT_FOUND;
    }
    reg_write(p, LPS25HB_REG_CTRL_REG2, 0x04);
    hal_delay_ms(5);
    reg_write(p, LPS25HB_REG_RES_CONF, 0x05); 
    reg_write(p, LPS25HB_REG_CTRL_REG1, 0xC4); 
    reg_write(p, LPS25HB_REG_FIFO_CTRL, 0x00);
    p->ref_pressure_hpa = LPS25HB_P0_HPA;
    AERO_LOGI("LPS25HB", "Init OK — WHO_AM_I=0x%02X", whoami);
    return HAL_OK;
}
static hal_status_t lps25hb_self_test(sal_driver_t *drv)
{
    lps25hb_priv_t *p = (lps25hb_priv_t *)drv->priv;
    uint8_t whoami = 0;
    hal_status_t rc = reg_read(p, LPS25HB_REG_WHO_AM_I, &whoami, 1);
    if (rc != HAL_OK) return rc;
    return (whoami == LPS25HB_WHOAMI_VALUE) ? HAL_OK : HAL_ERROR;
}
static hal_status_t lps25hb_read(sal_driver_t *drv, sal_data_t *out)
{
    lps25hb_priv_t *p = (lps25hb_priv_t *)drv->priv;
    uint8_t status = 0;
    reg_read(p, LPS25HB_REG_STATUS_REG, &status, 1);
    uint8_t raw[5];
    hal_status_t rc = reg_read(p, LPS25HB_REG_PRESS_OUT_XL | 0x40, raw, 5);
    if (rc != HAL_OK) return rc;
    int32_t raw_p = (int32_t)((uint32_t)raw[2] << 16 |
                               (uint32_t)raw[1] << 8  |
                               (uint32_t)raw[0]);
    if (raw_p & 0x800000) raw_p |= 0xFF000000; 
    float pressure_hpa = (float)raw_p / 4096.0f;
    int16_t raw_t = (int16_t)((uint16_t)raw[4] << 8 | raw[3]);
    float temperature_c = 42.5f + (float)raw_t / 480.0f;
    out->baro.pressure_hpa   = pressure_hpa;
    out->baro.altitude_m     = pressure_to_altitude(pressure_hpa, p->ref_pressure_hpa);
    out->baro.temperature_c  = temperature_c;
    return HAL_OK;
}
static hal_status_t lps25hb_set_odr(sal_driver_t *drv, uint32_t hz)
{
    lps25hb_priv_t *p = (lps25hb_priv_t *)drv->priv;
    uint8_t odr_bits;
    if      (hz == 0)  { odr_bits = 0x00; }  
    else if (hz <= 1)  { odr_bits = 0x10; }  
    else if (hz <= 7)  { odr_bits = 0x20; }  
    else if (hz <= 12) { odr_bits = 0x30; }  
    else               { odr_bits = 0x40; }  
    return reg_write(p, LPS25HB_REG_CTRL_REG1, 0x80 | odr_bits | 0x04);
}
static hal_status_t lps25hb_sleep(sal_driver_t *drv)
{
    lps25hb_priv_t *p = (lps25hb_priv_t *)drv->priv;
    return reg_write(p, LPS25HB_REG_CTRL_REG1, 0x00); 
}
static hal_status_t lps25hb_wakeup(sal_driver_t *drv)
{
    return lps25hb_init(drv);
}
static sal_driver_t s_lps25hb_driver = {
    .name      = "LPS25HB",
    .id        = SAL_SENSOR_BAROMETER,
    .init      = lps25hb_init,
    .deinit    = NULL,
    .self_test = lps25hb_self_test,
    .read      = lps25hb_read,
    .set_odr   = lps25hb_set_odr,
    .sleep     = lps25hb_sleep,
    .wakeup    = lps25hb_wakeup,
    .priv      = &s_lps25hb_priv,
};
hal_status_t lps25hb_driver_register(int iface, hal_i2c_bus_t i2c_bus,
                                      hal_spi_bus_t spi_bus, uint8_t i2c_addr)
{
    s_lps25hb_priv.i2c_bus  = i2c_bus;
    s_lps25hb_priv.spi_bus  = spi_bus;
    s_lps25hb_priv.i2c_addr = i2c_addr;
    return sal_register(&s_lps25hb_driver);
}
void lps25hb_set_reference_pressure(float p0_hpa)
{
    s_lps25hb_priv.ref_pressure_hpa = p0_hpa;
}