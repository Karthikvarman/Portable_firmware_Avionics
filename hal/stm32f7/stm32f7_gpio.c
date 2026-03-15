#include "hal_gpio.h"
#include "stm32f7xx_ll_gpio.h"
#include "stm32f7xx_ll_bus.h"
hal_status_t hal_gpio_init(hal_gpio_pin_t pin, const hal_gpio_config_t *config)
{
    return HAL_OK;
}
hal_status_t hal_gpio_write(hal_gpio_pin_t pin, hal_gpio_state_t state)
{
    if (state == HAL_GPIO_SET) LL_GPIO_SetOutputPin(pin.port, pin.pin);
    else LL_GPIO_ResetOutputPin(pin.port, pin.pin);
    return HAL_OK;
}
hal_status_t hal_gpio_read(hal_gpio_pin_t pin, hal_gpio_state_t *state)
{
    *state = LL_GPIO_IsInputPinSet(pin.port, pin.pin) ? HAL_GPIO_SET : HAL_GPIO_RESET;
    return HAL_OK;
}
hal_status_t hal_gpio_toggle(hal_gpio_pin_t pin)
{
    LL_GPIO_TogglePin(pin.port, pin.pin);
    return HAL_OK;
}