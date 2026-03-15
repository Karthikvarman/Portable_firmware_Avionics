#ifndef STM32_LL_BUS_H
#define STM32_LL_BUS_H
#include <stdint.h>
#define LL_APB1_GRP1_PERIPH_USART2 (1 << 17)
#define LL_APB1_GRP1_PERIPH_USART3 (1 << 18)
#define LL_APB1_GRP1_PERIPH_UART4 (1 << 19)
#define LL_APB2_GRP1_PERIPH_USART1 (1 << 14)
#define LL_AHB4_GRP1_PERIPH_GPIOA (1 << 0)
#define LL_AHB4_GRP1_PERIPH_GPIOB (1 << 1)
#define LL_AHB4_GRP1_PERIPH_GPIOC (1 << 2)
#define LL_AHB4_GRP1_PERIPH_GPIOD (1 << 3)
#define LL_AHB4_GRP1_PERIPH_GPIOE (1 << 4)
#define LL_AHB4_GRP1_PERIPH_GPIOF (1 << 5)
#define LL_AHB4_GRP1_PERIPH_GPIOG (1 << 6)
#define LL_AHB4_GRP1_PERIPH_GPIOH (1 << 7)
#define LL_AHB4_GRP1_PERIPH_GPIOI (1 << 8)
#define LL_AHB4_GRP1_PERIPH_GPIOJ (1 << 9)
#define LL_AHB4_GRP1_PERIPH_GPIOK (1 << 10)
void LL_APB1_GRP1_EnableClock(uint32_t periph);
void LL_APB2_GRP1_EnableClock(uint32_t periph);
void LL_AHB4_GRP1_EnableClock(uint32_t periph);
#endif