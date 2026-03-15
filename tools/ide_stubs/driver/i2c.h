#ifndef ESP_DRIVER_I2C_H
#define ESP_DRIVER_I2C_H
#include "../esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef int i2c_port_t;
typedef enum {
  I2C_MODE_MASTER = 0,
  I2C_MODE_SLAVE = 1,
} i2c_mode_t;
typedef struct {
  i2c_mode_t mode;
  int sda_io_num;
  int scl_io_num;
  bool sda_pullup_en;
  bool scl_pullup_en;
  struct {
    uint32_t clk_speed;
  } master;
} i2c_config_t;
esp_err_t i2c_param_config(i2c_port_t i2c_num, const i2c_config_t *i2c_conf);
esp_err_t i2c_driver_install(i2c_port_t i2c_num, i2c_mode_t mode,
                             size_t slv_rx_buf_len, size_t slv_tx_buf_len,
                             int intr_alloc_flags);
esp_err_t i2c_master_write_to_device(i2c_port_t i2c_num, uint8_t device_address,
                                     const uint8_t *write_buffer,
                                     size_t write_size, uint32_t ticks_to_wait);
esp_err_t i2c_master_read_from_device(i2c_port_t i2c_num,
                                      uint8_t device_address,
                                      uint8_t *read_buffer, size_t read_size,
                                      uint32_t ticks_to_wait);
#endif