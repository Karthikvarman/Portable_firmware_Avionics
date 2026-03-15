#ifndef STM32_LL_RCC_H
#define STM32_LL_RCC_H
#include <stdint.h>
typedef struct {
  uint32_t SYSCLK_Frequency;
  uint32_t HCLK_Frequency;
  uint32_t PCLK1_Frequency;
  uint32_t PCLK2_Frequency;
} LL_RCC_ClocksTypeDef;
#define LL_RCC_SYS_CLK_FREQ (480000000U)
void LL_RCC_HSE_Enable(void);
uint32_t LL_RCC_HSE_IsReady(void);
#define LL_RCC_PLLSOURCE_HSE 0x1
void LL_RCC_PLL_SetSource(uint32_t source);
void LL_RCC_PLL1P_Enable(void);
void LL_RCC_PLL1Q_Enable(void);
void LL_RCC_PLL1R_Enable(void);
void LL_RCC_PLL1_SetM(uint32_t m);
void LL_RCC_PLL1_SetN(uint32_t n);
void LL_RCC_PLL1_SetP(uint32_t p);
void LL_RCC_PLL1_SetQ(uint32_t q);
void LL_RCC_PLL1_SetR(uint32_t r);
void LL_RCC_PLL1_SetVCOInputRange(uint32_t range);
void LL_RCC_PLL1_SetVCOOutputRange(uint32_t range);
void LL_RCC_PLL1_Enable(void);
uint32_t LL_RCC_PLL1_IsReady(void);
#define LL_RCC_PLLINPUTRANGE_4_8 0x2
#define LL_RCC_PLLVCORANGE_WIDE 0x3
#define LL_RCC_AHB_DIV_2 0x1
#define LL_RCC_APB1_DIV_2 0x1
#define LL_RCC_APB2_DIV_2 0x1
void LL_RCC_SetAHBPrescaler(uint32_t prescaler);
void LL_RCC_SetAPB1Prescaler(uint32_t prescaler);
void LL_RCC_SetAPB2Prescaler(uint32_t prescaler);
#define LL_RCC_SYS_CLKSOURCE_PLL1 0x2
#define LL_RCC_SYS_CLKSOURCE_STATUS_PLL1 0x2
void LL_RCC_SetSysClkSource(uint32_t source);
uint32_t LL_RCC_GetSysClkSource(void);
void LL_RCC_GetSystemClocksFreq(LL_RCC_ClocksTypeDef *freqs);
#endif