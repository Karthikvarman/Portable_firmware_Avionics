#ifndef STM32F4_LL_RCC_H
#define STM32F4_LL_RCC_H
#include <stdint.h>
void LL_RCC_HSE_Enable(void);
uint32_t LL_RCC_HSE_IsReady(void);
void LL_RCC_PLL_Enable(void);
#define LL_RCC_PLLSOURCE_HSE (1U)
#define LL_RCC_PLLP_DIV_2 (2U)
void LL_RCC_PLL_ConfigDomain_SYS(uint32_t source, uint32_t m, uint32_t n,
                                 uint32_t p);
#endif