#include "hal_i2c.h"
#include "hal_gpio.h"
#include "hal_timer.h"
#include "aero_config.h"
hal_status_t hal_i2c_init(hal_i2c_port_t port, const hal_i2c_config_t *config)
{
    return HAL_OK;
}
hal_status_t hal_i2c_master_transmit(hal_i2c_port_t port, uint16_t dev_addr, const uint8_t *data, uint16_t size, uint32_t timeout)
{
    return HAL_OK;
}
hal_status_t hal_i2c_master_receive(hal_i2c_port_t port, uint16_t dev_addr, uint8_t *data, uint16_t size, uint32_t timeout)
{
    return HAL_OK;
}
hal_status_t hal_i2c_mem_write(hal_i2c_port_t port, uint16_t dev_addr, uint16_t mem_addr, uint16_t mem_addr_size, const uint8_t *data, uint16_t size, uint32_t timeout)
{
    return HAL_OK;
}
hal_status_t hal_i2c_mem_read(hal_i2c_port_t port, uint16_t dev_addr, uint16_t mem_addr, uint16_t mem_addr_size, uint8_t *data, uint16_t size, uint32_t timeout)
{
    return HAL_OK;
}