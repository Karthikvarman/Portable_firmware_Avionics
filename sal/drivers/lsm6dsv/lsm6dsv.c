#include "lsm6dsv.h"
#include "sal_sensor.h"
#include "hal_i2c.h"
#include "hal_spi.h"
#include "hal_timer.h"
#include "aero_config.h"
#include <math.h>
#include <string.h>
#define LSM_REG_FUNC_CFG_ACCESS     0x01
#define LSM_REG_FIFO_CTRL1          0x07
#define LSM_REG_FIFO_CTRL2          0x08
#define LSM_REG_FIFO_CTRL3          0x09
#define LSM_REG_FIFO_CTRL4          0x0A
#define LSM_REG_COUNTER_BDR_REG1    0x0B
#define LSM_REG_INT1_CTRL           0x0D
#define LSM_REG_INT2_CTRL           0x0E
#define LSM_REG_WHO_AM_I            0x0F
#define LSM_REG_CTRL1               0x10   
#define LSM_REG_CTRL2               0x11   
#define LSM_REG_CTRL3               0x12
#define LSM_REG_CTRL4               0x13
#define LSM_REG_CTRL5               0x14
#define LSM_REG_CTRL6               0x15
#define LSM_REG_CTRL7               0x16
#define LSM_REG_CTRL8               0x17
#define LSM_REG_CTRL9               0x18
#define LSM_REG_CTRL10              0x19
#define LSM_REG_STATUS_REG          0x1E
#define LSM_REG_OUT_TEMP_L          0x20
#define LSM_REG_OUT_TEMP_H          0x21
#define LSM_REG_OUTX_L_G            0x22
#define LSM_REG_OUTX_H_G            0x23
#define LSM_REG_OUTY_L_G            0x24
#define LSM_REG_OUTY_H_G            0x25
#define LSM_REG_OUTZ_L_G            0x26
#define LSM_REG_OUTZ_H_G            0x27
#define LSM_REG_OUTX_L_A            0x28
#define LSM_REG_OUTX_H_A            0x29
#define LSM_REG_OUTY_L_A            0x2A
#define LSM_REG_OUTY_H_A            0x2B
#define LSM_REG_OUTZ_L_A            0x2C
#define LSM_REG_OUTZ_H_A            0x2D
#define LSM_WHOAMI_VALUE            0x70   
#define LSM_ACCEL_SENS_4G           0.000122f  
#define LSM_ACCEL_SENS_8G           0.000244f
#define LSM_ACCEL_SENS_16G          0.000488f
#define LSM_ACCEL_SENS_32G          0.000976f
#define LSM_GYRO_SENS_125DPS        0.004375f  
#define LSM_GYRO_SENS_250DPS        0.008750f
#define LSM_GYRO_SENS_500DPS        0.017500f
#define LSM_GYRO_SENS_1000DPS       0.035000f
#define LSM_GYRO_SENS_2000DPS       0.070000f
#define LSM_GYRO_SENS_4000DPS       0.140000f
#define LSM_G_TO_MS2                9.80665f
#define LSM_DPS_TO_RADS             (3.14159265f / 180.0f)
typedef struct {
    int            iface;
    hal_i2c_bus_t  i2c_bus;
    hal_spi_bus_t  spi_bus;
    uint8_t        i2c_addr;
    float          accel_sens;   
    float          gyro_sens;    
} lsm6dsv_priv_t;
static lsm6dsv_priv_t s_lsm_priv;
static hal_status_t reg_write(lsm6dsv_priv_t *p, uint8_t reg, uint8_t val)
{
    if (p->iface == AERO_IFACE_SPI)
        return hal_spi_write_reg(p->spi_bus, reg, &val, 1);
    return hal_i2c_write_reg(p->i2c_bus, p->i2c_addr, reg, val);
}
static hal_status_t reg_read(lsm6dsv_priv_t *p, uint8_t reg,
                              uint8_t *buf, size_t len)
{
    if (p->iface == AERO_IFACE_SPI)
        return hal_spi_read_reg(p->spi_bus, reg | 0x80, buf, len);
    return hal_i2c_read(p->i2c_bus, p->i2c_addr, reg, buf, len);
}
static hal_status_t lsm6dsv_init(sal_driver_t *drv)
{
    lsm6dsv_priv_t *p = (lsm6dsv_priv_t *)drv->priv;
    uint8_t whoami = 0;
    hal_status_t rc = reg_read(p, LSM_REG_WHO_AM_I, &whoami, 1);
    if (rc != HAL_OK || whoami != LSM_WHOAMI_VALUE) {
        AERO_LOGE("LSM6DSV", "WHO_AM_I: 0x%02X (expected 0x%02X)", whoami, LSM_WHOAMI_VALUE);
        return HAL_NOT_FOUND;
    }
    reg_write(p, LSM_REG_CTRL3, 0x01);
    hal_delay_ms(15);
    reg_write(p, LSM_REG_CTRL1, 0x44);  
    p->accel_sens = LSM_ACCEL_SENS_8G * LSM_G_TO_MS2;
    reg_write(p, LSM_REG_CTRL2, 0x4C);  
    p->gyro_sens = LSM_GYRO_SENS_2000DPS * LSM_DPS_TO_RADS;
    reg_write(p, LSM_REG_CTRL3, 0x44);
    reg_write(p, LSM_REG_FIFO_CTRL4, 0x00);
    AERO_LOGI("LSM6DSV", "Init OK — accel ±8g @ 104Hz, gyro ±2000dps @ 104Hz");
    return HAL_OK;
}
static hal_status_t lsm6dsv_self_test(sal_driver_t *drv)
{
    lsm6dsv_priv_t *p = (lsm6dsv_priv_t *)drv->priv;
    uint8_t whoami = 0;
    hal_status_t rc = reg_read(p, LSM_REG_WHO_AM_I, &whoami, 1);
    if (rc != HAL_OK) return rc;
    return (whoami == LSM_WHOAMI_VALUE) ? HAL_OK : HAL_IO_ERROR;
}
static hal_status_t lsm6dsv_read(sal_driver_t *drv, sal_data_t *out)
{
    lsm6dsv_priv_t *p = (lsm6dsv_priv_t *)drv->priv;
    uint8_t raw[14];
    hal_status_t rc = reg_read(p, LSM_REG_OUT_TEMP_L | 0x40, raw, 14);
    if (rc != HAL_OK) return rc;
    int16_t raw_temp = (int16_t)((uint16_t)raw[1] << 8 | raw[0]);
    float temp_c = 25.0f + (float)raw_temp / 256.0f;
    int16_t gx = (int16_t)((uint16_t)raw[3] << 8 | raw[2]);
    int16_t gy = (int16_t)((uint16_t)raw[5] << 8 | raw[4]);
    int16_t gz = (int16_t)((uint16_t)raw[7] << 8 | raw[6]);
    int16_t ax = (int16_t)((uint16_t)raw[9]  << 8 | raw[8]);
    int16_t ay = (int16_t)((uint16_t)raw[11] << 8 | raw[10]);
    int16_t az = (int16_t)((uint16_t)raw[13] << 8 | raw[12]);
    out->imu.ax = (float)ax * p->accel_sens;
    out->imu.ay = (float)ay * p->accel_sens;
    out->imu.az = (float)az * p->accel_sens;
    out->imu.gx = (float)gx * p->gyro_sens;
    out->imu.gy = (float)gy * p->gyro_sens;
    out->imu.gz = (float)gz * p->gyro_sens;
    out->imu.temperature_c = temp_c;
    return HAL_OK;
}
static hal_status_t lsm6dsv_set_odr(sal_driver_t *drv, uint32_t hz)
{
    lsm6dsv_priv_t *p = (lsm6dsv_priv_t *)drv->priv;
    uint8_t odr;
    if      (hz == 0)    odr = 0x00;
    else if (hz <= 12)   odr = 0x01;
    else if (hz <= 26)   odr = 0x02;
    else if (hz <= 52)   odr = 0x03;
    else if (hz <= 104)  odr = 0x04;
    else if (hz <= 208)  odr = 0x05;
    else if (hz <= 416)  odr = 0x06;
    else if (hz <= 833)  odr = 0x07;
    else                 odr = 0x08; 
    reg_write(p, LSM_REG_CTRL1, (odr << 4) | 0x04); 
    reg_write(p, LSM_REG_CTRL2, (odr << 4) | 0x0C); 
    return HAL_OK;
}
static hal_status_t lsm6dsv_sleep(sal_driver_t *drv)
{
    lsm6dsv_priv_t *p = (lsm6dsv_priv_t *)drv->priv;
    reg_write(p, LSM_REG_CTRL1, 0x00); 
    reg_write(p, LSM_REG_CTRL2, 0x00); 
    return HAL_OK;
}
static sal_driver_t s_lsm_driver = {
    .name      = "LSM6DSV32XTR",
    .id        = SAL_SENSOR_ACCEL,
    .init      = lsm6dsv_init,
    .self_test = lsm6dsv_self_test,
    .read      = lsm6dsv_read,
    .set_odr   = lsm6dsv_set_odr,
    .sleep     = lsm6dsv_sleep,
    .priv      = &s_lsm_priv,
};
hal_status_t lsm6dsv_driver_register(int iface, hal_i2c_bus_t i2c_bus,
                                      hal_spi_bus_t spi_bus, uint8_t i2c_addr)
{
    s_lsm_priv.i2c_bus  = i2c_bus;
    s_lsm_priv.spi_bus  = spi_bus;
    s_lsm_priv.i2c_addr = i2c_addr;
    return sal_register(&s_lsm_driver);
}