#include "lis2mdl.h"
#include "sal_sensor.h"
#include "hal_i2c.h"
#include "hal_spi.h"
#include "hal_timer.h"
#include "aero_config.h"
#include <math.h>
#include <string.h>
#define LIS_REG_OFFSET_X_REG_L      0x45
#define LIS_REG_OFFSET_X_REG_H      0x46
#define LIS_REG_OFFSET_Y_REG_L      0x47
#define LIS_REG_OFFSET_Y_REG_H      0x48
#define LIS_REG_OFFSET_Z_REG_L      0x49
#define LIS_REG_OFFSET_Z_REG_H      0x4A
#define LIS_REG_WHO_AM_I            0x4F
#define LIS_REG_CFG_REG_A           0x60
#define LIS_REG_CFG_REG_B           0x61
#define LIS_REG_CFG_REG_C           0x62
#define LIS_REG_INT_CRTL_REG        0x63
#define LIS_REG_STATUS_REG          0x67
#define LIS_REG_OUTX_L              0x68
#define LIS_REG_OUTX_H              0x69
#define LIS_REG_OUTY_L              0x6A
#define LIS_REG_OUTY_H              0x6B
#define LIS_REG_OUTZ_L              0x6C
#define LIS_REG_OUTZ_H              0x6D
#define LIS_REG_TEMP_OUT_L          0x6E
#define LIS_REG_TEMP_OUT_H          0x6F
#define LIS_WHOAMI_VALUE            0x40
#define LIS_SENSITIVITY_GAUSS       0.0015f  
typedef struct {
    int            iface;
    hal_i2c_bus_t  i2c_bus;
    hal_spi_bus_t  spi_bus;
    uint8_t        i2c_addr;
    float          bias_x, bias_y, bias_z;
} lis2mdl_priv_t;
static lis2mdl_priv_t s_lis_priv;
static hal_status_t reg_write(lis2mdl_priv_t *p, uint8_t reg, uint8_t val)
{
    if (p->iface == AERO_IFACE_SPI)
        return hal_spi_write_reg(p->spi_bus, reg, &val, 1);
    return hal_i2c_write_reg(p->i2c_bus, p->i2c_addr, reg, val);
}
static hal_status_t reg_read(lis2mdl_priv_t *p, uint8_t reg,
                              uint8_t *buf, size_t len)
{
    if (p->iface == AERO_IFACE_SPI)
        return hal_spi_read_reg(p->spi_bus, reg | 0x40, buf, len);
    return hal_i2c_read(p->i2c_bus, p->i2c_addr, reg, buf, len);
}
static hal_status_t lis2mdl_init(sal_driver_t *drv)
{
    lis2mdl_priv_t *p = (lis2mdl_priv_t *)drv->priv;
    uint8_t whoami = 0;
    hal_status_t rc = reg_read(p, LIS_REG_WHO_AM_I, &whoami, 1);
    if (rc != HAL_OK || whoami != LIS_WHOAMI_VALUE) {
        AERO_LOGE("LIS2MDL", "WHO_AM_I=0x%02X (expected 0x%02X)", whoami, LIS_WHOAMI_VALUE);
        return HAL_NOT_FOUND;
    }
    reg_write(p, LIS_REG_CFG_REG_A, 0x20);
    hal_delay_ms(5);
    reg_write(p, LIS_REG_CFG_REG_A, 0x8C); 
    reg_write(p, LIS_REG_CFG_REG_B, 0x02);
    reg_write(p, LIS_REG_CFG_REG_C, 0x01);
    AERO_LOGI("LIS2MDL", "Init OK — ODR=100Hz, temp-comp enabled");
    return HAL_OK;
}
static hal_status_t lis2mdl_self_test(sal_driver_t *drv)
{
    lis2mdl_priv_t *p = (lis2mdl_priv_t *)drv->priv;
    uint8_t whoami = 0;
    hal_status_t rc = reg_read(p, LIS_REG_WHO_AM_I, &whoami, 1);
    return (rc == HAL_OK && whoami == LIS_WHOAMI_VALUE) ? HAL_OK : HAL_IO_ERROR;
}
static hal_status_t lis2mdl_read(sal_driver_t *drv, sal_data_t *out)
{
    lis2mdl_priv_t *p = (lis2mdl_priv_t *)drv->priv;
    uint8_t raw[8];
    hal_status_t rc = reg_read(p, LIS_REG_OUTX_L, raw, 8);
    if (rc != HAL_OK) return rc;
    int16_t mx = (int16_t)((uint16_t)raw[1] << 8 | raw[0]);
    int16_t my = (int16_t)((uint16_t)raw[3] << 8 | raw[2]);
    int16_t mz = (int16_t)((uint16_t)raw[5] << 8 | raw[4]);
    int16_t temp_raw = (int16_t)((uint16_t)raw[7] << 8 | raw[6]);
    out->mag.x = (float)mx * LIS_SENSITIVITY_GAUSS - p->bias_x;
    out->mag.y = (float)my * LIS_SENSITIVITY_GAUSS - p->bias_y;
    out->mag.z = (float)mz * LIS_SENSITIVITY_GAUSS - p->bias_z;
    out->mag.temperature_c = 25.0f + (float)temp_raw / 8.0f;
    out->mag.timestamp_ms  = hal_get_tick_ms();
    return HAL_OK;
}
static hal_status_t lis2mdl_set_odr(sal_driver_t *drv, uint32_t hz)
{
    lis2mdl_priv_t *p = (lis2mdl_priv_t *)drv->priv;
    uint8_t odr;
    if      (hz <= 10)  odr = 0x00;
    else if (hz <= 20)  odr = 0x04;
    else if (hz <= 50)  odr = 0x08;
    else                odr = 0x0C; 
    return reg_write(p, LIS_REG_CFG_REG_A, 0x80 | odr);
}
static hal_status_t lis2mdl_sleep(sal_driver_t *drv)
{
    lis2mdl_priv_t *p = (lis2mdl_priv_t *)drv->priv;
    return reg_write(p, LIS_REG_CFG_REG_A, 0x03); 
}
static sal_driver_t s_lis_driver = {
    .name      = "LIS2MDLTR",
    .id        = SAL_SENSOR_MAGNETOMETER,
    .init      = lis2mdl_init,
    .self_test = lis2mdl_self_test,
    .read      = lis2mdl_read,
    .set_odr   = lis2mdl_set_odr,
    .sleep     = lis2mdl_sleep,
    .priv      = &s_lis_priv,
};
hal_status_t lis2mdl_driver_register(int iface, hal_i2c_bus_t i2c_bus,
                                      hal_spi_bus_t spi_bus, uint8_t i2c_addr)
{
    s_lis_priv.i2c_bus  = i2c_bus;
    s_lis_priv.spi_bus  = spi_bus;
    s_lis_priv.i2c_addr = i2c_addr;
    return sal_register(&s_lis_driver);
}
void lis2mdl_set_hard_iron_bias(float bx, float by, float bz)
{
    s_lis_priv.bias_x = bx;
    s_lis_priv.bias_y = by;
    s_lis_priv.bias_z = bz;
}