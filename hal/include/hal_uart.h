#ifndef HAL_UART_H
#define HAL_UART_H
#include "hal_common.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef uint8_t hal_uart_port_t;
typedef enum {
    HAL_UART_STOPBITS_1 = 0,
    HAL_UART_STOPBITS_2 = 1,
} hal_uart_stopbits_t;
typedef enum {
    HAL_UART_PARITY_NONE = 0,
    HAL_UART_PARITY_EVEN = 1,
    HAL_UART_PARITY_ODD  = 2,
} hal_uart_parity_t;
typedef enum {
    HAL_UART_FLOW_NONE    = 0,
    HAL_UART_FLOW_RTS_CTS = 1,
} hal_uart_flow_t;
typedef struct {
    hal_uart_port_t      port;
    uint32_t             baud_rate;
    hal_uart_stopbits_t  stop_bits;
    hal_uart_parity_t    parity;
    hal_uart_flow_t      flow_control;
    uint32_t             rx_buf_size;  
} hal_uart_config_t;
hal_status_t hal_uart_init      (const hal_uart_config_t *cfg);
hal_status_t hal_uart_deinit    (hal_uart_port_t port);
hal_status_t hal_uart_write     (hal_uart_port_t port, const uint8_t *data, size_t len);
hal_status_t hal_uart_read      (hal_uart_port_t port, uint8_t *buf, size_t len,
                                  size_t *bytes_read, uint32_t timeout_ms);
hal_status_t hal_uart_read_byte (hal_uart_port_t port, uint8_t *byte, uint32_t timeout_ms);
hal_status_t hal_uart_flush_rx  (hal_uart_port_t port);
bool         hal_uart_data_available(hal_uart_port_t port);
#ifdef __cplusplus
}
#endif
#endif 