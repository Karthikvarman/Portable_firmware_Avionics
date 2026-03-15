#ifndef STM32L4_LL_RCC_H
#define STM32L4_LL_RCC_H
#include <stdint.h>
void LL_RCC_MSI_Enable(void);
uint32_t LL_RCC_MSI_IsReady(void);
#define LL_RCC_MSIRANGE_6 (6U)
void LL_RCC_MSI_SetRange(uint32_t range);
void LL_RCC_MSI_SetCalibTrimming(uint32_t trim);
#define LL_RCC_PLLSOURCE_MSI (1U)
#define LL_RCC_PLLM_DIV_1 (1U)
#define LL_RCC_PLLR_DIV_2 (2U)
void LL_RCC_PLL_ConfigDomain_SYS(uint32_t source, uint32_t m, uint32_t n,
                                 uint32_t r);
void LL_RCC_PLL_EnableDomain_SYS(void);
void LL_RCC_PLL_Enable(void);
#endif