#ifndef LPS25HB_H
#define LPS25HB_H
#include "sal_sensor.h"
#include "hal_i2c.h"
#include "hal_spi.h"
#ifdef __cplusplus
extern "C" {
#endif
hal_status_t lps25hb_driver_register(int iface,
                                      hal_i2c_bus_t i2c_bus,
                                      hal_spi_bus_t spi_bus,
                                      uint8_t       i2c_addr);
void lps25hb_set_reference_pressure(float p0_hpa);
#ifdef __cplusplus
}
#endif
#endif 