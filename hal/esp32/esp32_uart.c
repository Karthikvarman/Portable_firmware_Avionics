#include "hal_uart.h"
#include "hal_common.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include <string.h>
#define ESP32_UART_RX_BUF_SIZE  1024
#define ESP32_UART_TX_BUF_SIZE  512
#define ESP32_UART_MAX_PORTS    3
static const int UART_TX_PINS[3] = {1,  10, 17};
static const int UART_RX_PINS[3] = {3,  9,  16};
typedef struct {
    uart_port_t port;
    bool        initialized;
} esp32_uart_ctx_t;
static esp32_uart_ctx_t s_uart_ctx[ESP32_UART_MAX_PORTS];
hal_status_t hal_uart_init(const hal_uart_config_t *cfg)
{
    if (!cfg || cfg->port >= ESP32_UART_MAX_PORTS) return HAL_PARAM_ERROR;
    esp32_uart_ctx_t *ctx = &s_uart_ctx[cfg->port];
    uart_config_t uart_cfg = {
        .baud_rate  = (int)cfg->baud_rate,
        .data_bits  = UART_DATA_8_BITS,
        .stop_bits  = (cfg->stop_bits == HAL_UART_STOPBITS_2) ?
                       UART_STOP_BITS_2 : UART_STOP_BITS_1,
        .parity     = (cfg->parity == HAL_UART_PARITY_EVEN) ? UART_PARITY_EVEN :
                      (cfg->parity == HAL_UART_PARITY_ODD)  ? UART_PARITY_ODD  :
                                                               UART_PARITY_DISABLE,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    esp_err_t err;
    err = uart_param_config(cfg->port, &uart_cfg);
    if (err != ESP_OK) return HAL_IO_ERROR;
    err = uart_set_pin(cfg->port,
                       UART_TX_PINS[cfg->port],
                       UART_RX_PINS[cfg->port],
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) return HAL_IO_ERROR;
    err = uart_driver_install(cfg->port,
                              ESP32_UART_RX_BUF_SIZE,
                              ESP32_UART_TX_BUF_SIZE,
                              0, NULL, 0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return HAL_IO_ERROR;
    ctx->port        = cfg->port;
    ctx->initialized = true;
    return HAL_OK;
}
hal_status_t hal_uart_write(hal_uart_port_t port, const uint8_t *data, size_t len)
{
    if (port >= ESP32_UART_MAX_PORTS || !data) return HAL_PARAM_ERROR;
    int written = uart_write_bytes(port, (const char *)data, len);
    return (written == (int)len) ? HAL_OK : HAL_IO_ERROR;
}
hal_status_t hal_uart_read(hal_uart_port_t port, uint8_t *buf, size_t len,
                            size_t *bytes_read, uint32_t timeout_ms)
{
    if (port >= ESP32_UART_MAX_PORTS || !buf) return HAL_PARAM_ERROR;
    int n = uart_read_bytes(port, buf, (uint32_t)len, pdMS_TO_TICKS(timeout_ms));
    if (n < 0) return HAL_IO_ERROR;
    if (bytes_read) *bytes_read = (size_t)n;
    return HAL_OK;
}
hal_status_t hal_uart_read_byte(hal_uart_port_t port, uint8_t *byte, uint32_t timeout_ms)
{
    if (port >= ESP32_UART_MAX_PORTS || !byte) return HAL_PARAM_ERROR;
    size_t n;
    hal_status_t rc = hal_uart_read(port, byte, 1, &n, timeout_ms);
    if (rc != HAL_OK || n == 0) return HAL_TIMEOUT;
    return HAL_OK;
}
hal_status_t hal_uart_flush_rx(hal_uart_port_t port)
{
    if (port >= ESP32_UART_MAX_PORTS) return HAL_PARAM_ERROR;
    uart_flush_input(port);
    return HAL_OK;
}
hal_status_t hal_uart_deinit(hal_uart_port_t port)
{
    if (port >= ESP32_UART_MAX_PORTS) return HAL_PARAM_ERROR;
    uart_driver_delete(port);
    s_uart_ctx[port].initialized = false;
    return HAL_OK;
}
bool hal_uart_data_available(hal_uart_port_t port)
{
    if (port >= ESP32_UART_MAX_PORTS) return false;
    size_t available = 0;
    uart_get_buffered_data_len(port, &available);
    return available > 0;
}