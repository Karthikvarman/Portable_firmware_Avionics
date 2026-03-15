#ifndef ESP_DRIVER_GPIO_H
#define ESP_DRIVER_GPIO_H
#include "../esp_err.h"
#include <stdbool.h>
#include <stdint.h>
typedef int gpio_num_t;
typedef enum {
  GPIO_MODE_DISABLE = 0,
  GPIO_MODE_INPUT = 1,
  GPIO_MODE_OUTPUT = 2,
  GPIO_MODE_OUTPUT_OD = 3,
} gpio_mode_t;
typedef enum {
  GPIO_INTR_DISABLE = 0,
} gpio_int_type_t;
typedef enum {
  GPIO_PULLUP_DISABLE = 0,
  GPIO_PULLUP_ENABLE = 1,
} gpio_pullup_t;
typedef enum {
  GPIO_PULLDOWN_DISABLE = 0,
  GPIO_PULLDOWN_ENABLE = 1,
} gpio_pulldown_t;
typedef struct {
  uint64_t pin_bit_mask;
  gpio_mode_t mode;
  gpio_pullup_t pull_up_en;
  gpio_pulldown_t pull_down_en;
  gpio_int_type_t intr_type;
} gpio_config_t;
esp_err_t gpio_config(const gpio_config_t *pGPIOConfig);
esp_err_t gpio_reset_pin(gpio_num_t gpio_num);
esp_err_t gpio_set_level(gpio_num_t gpio_num, uint32_t level);
int gpio_get_level(gpio_num_t gpio_num);
#endif