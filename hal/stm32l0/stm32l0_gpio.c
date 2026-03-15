#include "hal_gpio.h"
#include "stm32l0xx_ll_bus.h"
#include "stm32l0xx_ll_gpio.h"
hal_status_t hal_gpio_init(const hal_gpio_config_t *config) {
  return HAL_OK;
}
hal_status_t hal_gpio_write(uint8_t port, uint8_t pin, hal_gpio_state_t state) {
  if (state == HAL_GPIO_HIGH)
    LL_GPIO_SetOutputPin(port, pin);
  else
    LL_GPIO_ResetOutputPin(port, pin);
  return HAL_OK;
}
hal_status_t hal_gpio_read(uint8_t port, uint8_t pin, hal_gpio_state_t *state) {
  *state = LL_GPIO_IsInputPinSet(port, pin) ? HAL_GPIO_HIGH : HAL_GPIO_LOW;
  return HAL_OK;
}
hal_status_t hal_gpio_toggle(uint8_t port, uint8_t pin) {
  LL_GPIO_TogglePin(port, pin);
  return HAL_OK;
}
hal_status_t hal_gpio_deinit(uint8_t port, uint8_t pin) { return HAL_OK; }