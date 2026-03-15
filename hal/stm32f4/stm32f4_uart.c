#include "hal_uart.h"
#include "stm32f4xx_ll_usart.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_gpio.h"
hal_status_t hal_uart_init(hal_uart_port_t port, const hal_uart_config_t *config)
{
    return HAL_OK;
}
hal_status_t hal_uart_transmit(hal_uart_port_t port, const uint8_t *data, uint16_t size, uint32_t timeout)
{
    return HAL_OK;
}
hal_status_t hal_uart_receive(hal_uart_port_t port, uint8_t *data, uint16_t size, uint32_t timeout)
{
    return HAL_OK;
}
hal_status_t hal_uart_flush(hal_uart_port_t port)
{
    return HAL_OK;
}