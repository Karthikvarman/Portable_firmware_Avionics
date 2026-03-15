#include "hal_spi.h"
#include "esp_system.h"
hal_status_t hal_spi_init(hal_spi_port_t port, const hal_spi_config_t *config)
{
    return HAL_OK;
}
hal_status_t hal_spi_transmit(hal_spi_port_t port, const uint8_t *data, uint16_t size, uint32_t timeout)
{
    return HAL_OK;
}
hal_status_t hal_spi_receive(hal_spi_port_t port, uint8_t *data, uint16_t size, uint32_t timeout)
{
    return HAL_OK;
}
hal_status_t hal_spi_transmit_receive(hal_spi_port_t port, const uint8_t *tx_data, uint8_t *rx_data, uint16_t size, uint32_t timeout)
{
    return HAL_OK;
}