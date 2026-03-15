#ifndef STM32L4XX_LL_GPIO_H
#define STM32L4XX_LL_GPIO_H
#include <stdint.h>
void LL_GPIO_SetOutputPin(uint8_t port, uint8_t pin);
void LL_GPIO_ResetOutputPin(uint8_t port, uint8_t pin);
void LL_GPIO_TogglePin(uint8_t port, uint8_t pin);
uint32_t LL_GPIO_IsInputPinSet(uint8_t port, uint8_t pin);
#endif