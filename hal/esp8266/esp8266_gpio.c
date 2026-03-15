#include "hal_gpio.h"
#include "hal_common.h"
#include "driver/gpio.h"
hal_status_t hal_gpio_init(const hal_gpio_config_t *cfg)
{
    if (!cfg) return HAL_PARAM_ERROR;
    gpio_config_t conf = {
        .pin_bit_mask = (1ULL << cfg->pin),
        .intr_type    = GPIO_INTR_DISABLE,
    };
    switch (cfg->mode) {
        case HAL_GPIO_MODE_INPUT:
            conf.mode = GPIO_MODE_INPUT; break;
        case HAL_GPIO_MODE_OUTPUT_PP:
            conf.mode = GPIO_MODE_OUTPUT; break;
        case HAL_GPIO_MODE_OUTPUT_OD:
            conf.mode = GPIO_MODE_OUTPUT_OD; break;
        case HAL_GPIO_MODE_ANALOG:
            return HAL_UNSUPPORTED; 
        default:
            return HAL_PARAM_ERROR;
    }
    switch (cfg->pull) {
        case HAL_GPIO_PULL_NONE:
            conf.pull_up_en = 0; conf.pull_down_en = 0; break;
        case HAL_GPIO_PULL_UP:
            conf.pull_up_en = 1; conf.pull_down_en = 0; break;
        case HAL_GPIO_PULL_DOWN:
            conf.pull_up_en = 0; conf.pull_down_en = 1; break;
    }
    if (gpio_config(&conf) != ESP_OK) return HAL_IO_ERROR;
    if (cfg->mode == HAL_GPIO_MODE_OUTPUT_PP || cfg->mode == HAL_GPIO_MODE_OUTPUT_OD) {
        hal_gpio_write(0, cfg->pin, cfg->initial_state);
    }
    return HAL_OK;
}
hal_status_t hal_gpio_write(uint8_t port, uint8_t pin, hal_gpio_state_t state)
{
    (void)port;
    if (gpio_set_level(pin, (uint32_t)state) != ESP_OK) return HAL_IO_ERROR;
    return HAL_OK;
}
hal_status_t hal_gpio_read(uint8_t port, uint8_t pin, hal_gpio_state_t *out)
{
    (void)port;
    if (!out) return HAL_PARAM_ERROR;
    *out = (hal_gpio_state_t)gpio_get_level(pin);
    return HAL_OK;
}
hal_status_t hal_gpio_toggle(uint8_t port, uint8_t pin)
{
    hal_gpio_state_t current;
    hal_status_t rc = hal_gpio_read(port, pin, &current);
    if (rc != HAL_OK) return rc;
    return hal_gpio_write(port, pin, (current == HAL_GPIO_HIGH) ? HAL_GPIO_LOW : HAL_GPIO_HIGH);
}
hal_status_t hal_gpio_deinit(uint8_t port, uint8_t pin)
{
    (void)port;
    return hal_gpio_init(&(hal_gpio_config_t){
        .pin = pin,
        .mode = HAL_GPIO_MODE_INPUT,
        .pull = HAL_GPIO_PULL_NONE
    });
}