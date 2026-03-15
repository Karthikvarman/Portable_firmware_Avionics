#include "hal_gpio.h"
#include "hal_common.h"
#include "driver/gpio.h"
#include "hal/gpio_types.h"
hal_status_t hal_gpio_init(const hal_gpio_config_t *cfg)
{
    if (!cfg) return HAL_PARAM_ERROR;
    gpio_config_t io_cfg = {
        .pin_bit_mask = (1ULL << cfg->pin),
        .pull_up_en   = (cfg->pull == HAL_GPIO_PULL_UP)   ? GPIO_PULLUP_ENABLE   : GPIO_PULLUP_DISABLE,
        .pull_down_en = (cfg->pull == HAL_GPIO_PULL_DOWN) ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    switch (cfg->mode) {
        case HAL_GPIO_MODE_INPUT:
            io_cfg.mode = GPIO_MODE_INPUT; break;
        case HAL_GPIO_MODE_OUTPUT_PP:
            io_cfg.mode = GPIO_MODE_OUTPUT; break;
        case HAL_GPIO_MODE_OUTPUT_OD:
            io_cfg.mode = GPIO_MODE_OUTPUT_OD; break;
        case HAL_GPIO_MODE_ANALOG:
            io_cfg.mode = GPIO_MODE_DISABLE; break;
        default:
            return HAL_PARAM_ERROR;
    }
    return (gpio_config(&io_cfg) == ESP_OK) ? HAL_OK : HAL_IO_ERROR;
}
hal_status_t hal_gpio_write(uint8_t port, uint8_t pin, hal_gpio_state_t state)
{
    (void)port;
    return (gpio_set_level((gpio_num_t)pin, state == HAL_GPIO_HIGH ? 1 : 0) == ESP_OK)
            ? HAL_OK : HAL_IO_ERROR;
}
hal_status_t hal_gpio_read(uint8_t port, uint8_t pin, hal_gpio_state_t *out)
{
    (void)port;
    if (!out) return HAL_PARAM_ERROR;
    *out = gpio_get_level((gpio_num_t)pin) ? HAL_GPIO_HIGH : HAL_GPIO_LOW;
    return HAL_OK;
}
hal_status_t hal_gpio_toggle(uint8_t port, uint8_t pin)
{
    (void)port;
    int lvl = gpio_get_level((gpio_num_t)pin);
    return (gpio_set_level((gpio_num_t)pin, 1 - lvl) == ESP_OK) ? HAL_OK : HAL_IO_ERROR;
}
hal_status_t hal_gpio_deinit(uint8_t port, uint8_t pin)
{
    (void)port;
    gpio_reset_pin((gpio_num_t)pin);
    return HAL_OK;
}