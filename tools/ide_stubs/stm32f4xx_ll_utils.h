#ifndef STM32_LL_UTILS_H
#define STM32_LL_UTILS_H
#include <stdint.h>
void LL_Init1msTick(uint32_t HCLKFrequency);
void LL_mDelay(uint32_t Delay);
#endif