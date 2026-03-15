#ifndef STM32H7XX_LL_SYSTEM_H
#define STM32H7XX_LL_SYSTEM_H
#include <stdint.h>
#define LL_FLASH_LATENCY_4 4
#define LL_FLASH_LATENCY_7 7
void LL_FLASH_SetLatency(uint32_t Latency);
uint32_t LL_FLASH_GetLatency(void);
void LL_SetSystemCoreClock(uint32_t HCLK_Frequency);
#endif