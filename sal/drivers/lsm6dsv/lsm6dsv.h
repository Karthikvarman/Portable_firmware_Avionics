#ifndef LSM6DSV_H
#define LSM6DSV_H
#include "sal_sensor.h"
#include "hal_i2c.h"
#include "hal_spi.h"
#ifdef __cplusplus
extern "C" {
#endif
hal_status_t lsm6dsv_driver_register(int iface,
                                      hal_i2c_bus_t i2c_bus,
                                      hal_spi_bus_t spi_bus,
                                      uint8_t       i2c_addr);
#ifdef __cplusplus
}
#endif
#endif 