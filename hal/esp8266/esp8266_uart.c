#include "hal_uart.h"
#include "hal_common.h"
#include "driver/uart.h"
hal_status_t hal_uart_init(const hal_uart_config_t *cfg)
{
    if (!cfg) return HAL_PARAM_ERROR;
    uart_config_t uart_cfg = {
        .baud_rate = cfg->baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity    = (cfg->parity == HAL_UART_PARITY_EVEN) ? UART_PARITY_EVEN :
                     (cfg->parity == HAL_UART_PARITY_ODD)  ? UART_PARITY_ODD  :
                                                             UART_PARITY_DISABLE,
        .stop_bits = (cfg->stop_bits == HAL_UART_STOPBITS_2) ? UART_STOP_BITS_2 :
                                                              UART_STOP_BITS_1,
        .flow_ctrl = (cfg->flow_control == HAL_UART_FLOW_RTS_CTS) ? UART_HW_FLOWCTRL_CTS_RTS :
                                                                   UART_HW_FLOWCTRL_DISABLE,
    };
    if (uart_param_config(cfg->port, &uart_cfg) != ESP_OK) return HAL_IO_ERROR;
    if (uart_driver_install(cfg->port, cfg->rx_buf_size ? cfg->rx_buf_size : 256, 0, 0, NULL, 0) != ESP_OK)
        return HAL_IO_ERROR;
    return HAL_OK;
}
hal_status_t hal_uart_deinit(hal_uart_port_t port)
{
    if (uart_driver_delete(port) != ESP_OK) return HAL_IO_ERROR;
    return HAL_OK;
}
hal_status_t hal_uart_write(hal_uart_port_t port, const uint8_t *data, size_t len)
{
    if (!data) return HAL_PARAM_ERROR;
    int written = uart_write_bytes(port, (const char*)data, len);
    return (written >= 0) ? HAL_OK : HAL_IO_ERROR;
}
hal_status_t hal_uart_read(hal_uart_port_t port, uint8_t *buf, size_t len,
                             size_t *bytes_read, uint32_t timeout_ms)
{
    if (!buf) return HAL_PARAM_ERROR;
    int read = uart_read_bytes(port, buf, len, pdMS_TO_TICKS(timeout_ms));
    if (bytes_read) *bytes_read = (read >= 0) ? (size_t)read : 0;
    return (read >= 0) ? HAL_OK : HAL_TIMEOUT;
}
hal_status_t hal_uart_read_byte(hal_uart_port_t port, uint8_t *byte, uint32_t timeout_ms)
{
    size_t n = 0;
    return hal_uart_read(port, byte, 1, &n, timeout_ms);
}
hal_status_t hal_uart_flush_rx(hal_uart_port_t port)
{
    if (uart_flush(port) != ESP_OK) return HAL_IO_ERROR;
    return HAL_OK;
}
bool hal_uart_data_available(hal_uart_port_t port)
{
    size_t buffered = 0;
    uart_get_buffered_data_len(port, &buffered);
    return (buffered > 0);
}