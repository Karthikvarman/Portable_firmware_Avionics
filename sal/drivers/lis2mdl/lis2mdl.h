#ifndef LIS2MDL_H
#define LIS2MDL_H
#include "sal_sensor.h"
#include "hal_i2c.h"
#include "hal_spi.h"
#ifdef __cplusplus
extern "C" {
#endif
hal_status_t lis2mdl_driver_register(int iface,
                                      hal_i2c_bus_t i2c_bus,
                                      hal_spi_bus_t spi_bus,
                                      uint8_t       i2c_addr);
void lis2mdl_set_hard_iron_bias(float bx, float by, float bz);
#ifdef __cplusplus
}
#endif
#endif 