#ifndef HAL_GPIO_H
#define HAL_GPIO_H
#include "hal_common.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    HAL_GPIO_MODE_INPUT      = 0,
    HAL_GPIO_MODE_OUTPUT_PP  = 1,   
    HAL_GPIO_MODE_OUTPUT_OD  = 2,   
    HAL_GPIO_MODE_ANALOG     = 3,
} hal_gpio_mode_t;
typedef enum {
    HAL_GPIO_PULL_NONE = 0,
    HAL_GPIO_PULL_UP   = 1,
    HAL_GPIO_PULL_DOWN = 2,
} hal_gpio_pull_t;
typedef enum {
    HAL_GPIO_LOW  = 0,
    HAL_GPIO_HIGH = 1,
} hal_gpio_state_t;
typedef struct {
    uint8_t          port;   
    uint8_t          pin;    
    hal_gpio_mode_t  mode;
    hal_gpio_pull_t  pull;
    hal_gpio_state_t initial_state;   
} hal_gpio_config_t;
hal_status_t hal_gpio_init  (const hal_gpio_config_t *cfg);
hal_status_t hal_gpio_write (uint8_t port, uint8_t pin, hal_gpio_state_t state);
hal_status_t hal_gpio_read  (uint8_t port, uint8_t pin, hal_gpio_state_t *out);
hal_status_t hal_gpio_toggle(uint8_t port, uint8_t pin);
hal_status_t hal_gpio_deinit(uint8_t port, uint8_t pin);
#ifdef __cplusplus
}
#endif
#endif 