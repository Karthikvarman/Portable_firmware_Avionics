#ifndef STM32F4_LL_FLASH_H
#define STM32F4_LL_FLASH_H
#include <stdint.h>
#define LL_FLASH_LATENCY_5 (5U)
void LL_FLASH_SetLatency(uint32_t latency);
#endif