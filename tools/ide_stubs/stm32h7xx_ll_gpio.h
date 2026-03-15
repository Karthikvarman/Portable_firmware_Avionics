#ifndef STM32_LL_GPIO_H
#define STM32_LL_GPIO_H
#include <stdint.h>
typedef struct {
  void *dummy;
} GPIO_TypeDef;
#define GPIOA ((GPIO_TypeDef *)0x40020000)
#define GPIOB ((GPIO_TypeDef *)0x40020400)
#define GPIOC ((GPIO_TypeDef *)0x40020800)
#define GPIOD ((GPIO_TypeDef *)0x40020C00)
#define GPIOE ((GPIO_TypeDef *)0x40021000)
#define GPIOF ((GPIO_TypeDef *)0x40021400)
#define GPIOG ((GPIO_TypeDef *)0x40021800)
#define GPIOH ((GPIO_TypeDef *)0x40021C00)
#define GPIOI ((GPIO_TypeDef *)0x40022000)
#define GPIOJ ((GPIO_TypeDef *)0x40022400)
#define GPIOK ((GPIO_TypeDef *)0x40022800)
typedef struct {
  uint32_t Pin;
  uint32_t Mode;
  uint32_t Speed;
  uint32_t OutputType;
  uint32_t Pull;
  uint32_t Alternate;
} LL_GPIO_InitTypeDef;
#define LL_GPIO_MODE_INPUT (0U)
#define LL_GPIO_MODE_OUTPUT (1U)
#define LL_GPIO_MODE_ALTERNATE (2U)
#define LL_GPIO_MODE_ANALOG (3U)
#define LL_GPIO_SPEED_FREQ_LOW (0U)
#define LL_GPIO_SPEED_FREQ_MEDIUM (1U)
#define LL_GPIO_SPEED_FREQ_HIGH (2U)
#define LL_GPIO_SPEED_FREQ_VERY_HIGH (3U)
#define LL_GPIO_OUTPUT_PUSHPULL (0U)
#define LL_GPIO_OUTPUT_OPENDRAIN (1U)
#define LL_GPIO_PULL_NO (0U)
#define LL_GPIO_PULL_UP (1U)
#define LL_GPIO_PULL_DOWN (2U)
void LL_GPIO_Init(GPIO_TypeDef *GPIOx, LL_GPIO_InitTypeDef *GPIO_InitStruct);
void LL_GPIO_SetOutputPin(GPIO_TypeDef *GPIOx, uint32_t PinMask);
void LL_GPIO_ResetOutputPin(GPIO_TypeDef *GPIOx, uint32_t PinMask);
void LL_GPIO_TogglePin(GPIO_TypeDef *GPIOx, uint32_t PinMask);
uint32_t LL_GPIO_IsInputPinSet(GPIO_TypeDef *GPIOx, uint32_t PinMask);
#endif