#ifndef STM32H7XX_LL_USART_H
#define STM32H7XX_LL_USART_H
#include <stdint.h>
#define LL_USART_PRESCALER_DIV1 0
#define LL_USART_STOPBITS_1 0
#define LL_USART_STOPBITS_2 1
#define LL_USART_PARITY_NONE 0
#define LL_USART_PARITY_EVEN 1
#define LL_USART_PARITY_ODD 2
#define LL_USART_HWCONTROL_NONE 0
#define LL_USART_HWCONTROL_RTS_CTS 1
#define LL_USART_DIRECTION_TX_RX 0
#define LL_USART_DATAWIDTH_7B 0
#define LL_USART_DATAWIDTH_8B 1
#define LL_USART_OVERSAMPLING_16 0
#define SUCCESS 0
#define ERROR 1
typedef struct {
  void *dummy;
} USART_TypeDef;
#define USART1 ((USART_TypeDef *)1)
#define USART2 ((USART_TypeDef *)2)
#define USART3 ((USART_TypeDef *)3)
#define UART4 ((USART_TypeDef *)4)
typedef struct {
  uint32_t BaudRate;
  uint32_t DataWidth;
  uint32_t StopBits;
  uint32_t Parity;
  uint32_t TransferDirection;
  uint32_t HardwareFlowControl;
  uint32_t OverSampling;
} LL_USART_InitTypeDef;
void LL_USART_Init(void *USARTx, LL_USART_InitTypeDef *USART_InitStruct);
void LL_USART_DeInit(void *USARTx);
void LL_USART_Enable(void *USARTx);
void LL_USART_Disable(void *USARTx);
void LL_USART_TransmitData8(void *USARTx, uint8_t Value);
uint8_t LL_USART_ReceiveData8(void *USARTx);
uint32_t LL_USART_IsActiveFlag_TXE_TXFNF(void *USARTx);
uint32_t LL_USART_IsActiveFlag_RXNE_RXFNE(void *USARTx);
uint32_t LL_USART_IsActiveFlag_TC(void *USARTx);
uint32_t LL_USART_IsActiveFlag_TEACK(void *USARTx);
uint32_t LL_USART_IsActiveFlag_REACK(void *USARTx);
#endif