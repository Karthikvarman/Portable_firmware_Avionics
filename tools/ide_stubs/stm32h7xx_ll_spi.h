#ifndef STM32_LL_SPI_H
#define STM32_LL_SPI_H
#include <stdint.h>
typedef struct {
  uint32_t Instance;
} SPI_TypeDef;
#define LL_SPI_MODE_MASTER (1U << 0)
#define LL_SPI_BAUDRATEPRESCALER_DIV2 (0U)
void LL_SPI_Enable(SPI_TypeDef *SPIx);
#endif