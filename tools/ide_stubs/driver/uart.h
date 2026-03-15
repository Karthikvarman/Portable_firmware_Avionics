#ifndef ESP_DRIVER_UART_H
#define ESP_DRIVER_UART_H
#include "../esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef int uart_port_t;
typedef enum {
  UART_DATA_8_BITS = 8,
} uart_word_length_t;
typedef enum {
  UART_STOP_BITS_1 = 1,
  UART_STOP_BITS_2 = 2,
} uart_stop_bits_t;
typedef enum {
  UART_PARITY_DISABLE = 0,
  UART_PARITY_EVEN = 1,
  UART_PARITY_ODD = 2,
} uart_parity_t;
typedef enum {
  UART_HW_FLOWCTRL_DISABLE = 0,
} uart_hw_flowcontrol_t;
typedef enum {
  UART_SCLK_APB = 0,
} uart_sclk_t;
typedef struct {
  int baud_rate;
  uart_word_length_t data_bits;
  uart_parity_t parity;
  uart_stop_bits_t stop_bits;
  uart_hw_flowcontrol_t flow_ctrl;
  uart_sclk_t source_clk;
} uart_config_t;
esp_err_t uart_param_config(uart_port_t uart_num,
                            const uart_config_t *uart_config);
esp_err_t uart_set_pin(uart_port_t uart_num, int tx_io_num, int rx_io_num,
                       int rts_io_num, int cts_io_num);
esp_err_t uart_driver_install(uart_port_t uart_num, int rx_buffer_size,
                              int tx_buffer_size, int queue_size,
                              void *uart_queue, int intr_alloc_flags);
int uart_write_bytes(uart_port_t uart_num, const void *src, size_t size);
int uart_read_bytes(uart_port_t uart_num, void *buf, uint32_t length,
                    uint32_t ticks_to_wait);
#endif