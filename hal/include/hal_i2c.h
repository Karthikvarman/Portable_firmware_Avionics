#ifndef HAL_I2C_H
#define HAL_I2C_H
#include "hal_common.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef uint8_t hal_i2c_bus_t;
typedef enum {
    HAL_I2C_SPEED_STANDARD  = 100000,   
    HAL_I2C_SPEED_FAST      = 400000,   
    HAL_I2C_SPEED_FAST_PLUS = 1000000,  
} hal_i2c_speed_t;
typedef struct {
    hal_i2c_bus_t  bus;
    hal_i2c_speed_t speed;
    uint8_t        sda_gpio_pin;
    uint8_t        scl_gpio_pin;
    uint8_t        sda_gpio_port;   
    uint8_t        scl_gpio_port;
} hal_i2c_config_t;
hal_status_t hal_i2c_init   (const hal_i2c_config_t *cfg);
hal_status_t hal_i2c_deinit (hal_i2c_bus_t bus);
hal_status_t hal_i2c_transfer(hal_i2c_bus_t bus, uint8_t addr,
                               const uint8_t *tx, size_t txlen,
                               uint8_t *rx, size_t rxlen,
                               uint32_t timeout_ms);
hal_status_t hal_i2c_write   (hal_i2c_bus_t bus, uint8_t addr,
                               const uint8_t *data, size_t len, uint32_t timeout_ms);
hal_status_t hal_i2c_read    (hal_i2c_bus_t bus, uint8_t addr,
                               uint8_t *data, size_t len, uint32_t timeout_ms);
hal_status_t hal_i2c_write_reg(hal_i2c_bus_t bus, uint8_t addr,
                                uint8_t reg, const uint8_t *data, size_t len);
hal_status_t hal_i2c_read_reg (hal_i2c_bus_t bus, uint8_t addr,
                                uint8_t reg, uint8_t *data, size_t len);
hal_status_t hal_i2c_probe   (hal_i2c_bus_t bus, uint8_t addr);
#ifdef __cplusplus
}
#endif
#endif 