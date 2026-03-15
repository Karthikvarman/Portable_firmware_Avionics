#include "hal_uart.h"
#include "hal_timer.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "stm32h7xx_ll_usart.h"
#include "stm32h7xx_ll_bus.h"
#include "stm32h7xx_ll_dma.h"
#define UART_DMA_RX_BUF_SIZE    512
typedef struct {
    USART_TypeDef            *instance;
    bool                      initialized;
    hal_uart_config_t         cfg;
    uint8_t                   dma_rx_buf[UART_DMA_RX_BUF_SIZE];
    volatile uint32_t         dma_rx_head;
} stm32h7_uart_state_t;
static stm32h7_uart_state_t s_uart[HAL_UART_PORT_MAX] = {
    [HAL_UART_PORT_1] = { .instance = USART1 },
    [HAL_UART_PORT_2] = { .instance = USART2 },
    [HAL_UART_PORT_3] = { .instance = USART3 },
    [HAL_UART_PORT_4] = { .instance = UART4  },
};
static void uart_enable_clock(hal_uart_port_t port)
{
    switch (port) {
        case HAL_UART_PORT_1: LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_USART1); break;
        case HAL_UART_PORT_2: LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART2); break;
        case HAL_UART_PORT_3: LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART3); break;
        case HAL_UART_PORT_4: LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_UART4);  break;
        default: break;
    }
}
hal_status_t hal_uart_init(const hal_uart_config_t *cfg)
{
    if (!cfg || cfg->port >= HAL_UART_PORT_MAX) return HAL_INVALID;
    stm32h7_uart_state_t *st = &s_uart[cfg->port];
    uart_enable_clock(cfg->port);
    memcpy(&st->cfg, cfg, sizeof(hal_uart_config_t));
    LL_USART_Disable(st->instance);
    uint32_t parity, stop;
    switch (cfg->parity) {
        case HAL_UART_PARITY_ODD:  parity = LL_USART_PARITY_ODD;  break;
        case HAL_UART_PARITY_EVEN: parity = LL_USART_PARITY_EVEN; break;
        default:                   parity = LL_USART_PARITY_NONE;  break;
    }
    stop = (cfg->stop_bits == HAL_UART_STOPBITS_2) ?
            LL_USART_STOPBITS_2 : LL_USART_STOPBITS_1;
    LL_USART_InitTypeDef init = {
        .PrescalerValue      = LL_USART_PRESCALER_DIV1,
        .BaudRate            = cfg->baud_rate,
        .DataWidth           = (cfg->data_bits == 7) ?
                                LL_USART_DATAWIDTH_7B : LL_USART_DATAWIDTH_8B,
        .StopBits            = stop,
        .Parity              = parity,
        .TransferDirection   = LL_USART_DIRECTION_TX_RX,
        .HardwareFlowControl = cfg->flow_control ?
                                LL_USART_HWCONTROL_RTS_CTS : LL_USART_HWCONTROL_NONE,
        .OverSampling        = LL_USART_OVERSAMPLING_16,
    };
    if (LL_USART_Init(st->instance, &init) != SUCCESS) return HAL_ERROR;
    LL_USART_Enable(st->instance);
    hal_tick_t start = hal_get_tick_ms();
    while (!LL_USART_IsActiveFlag_TEACK(st->instance) ||
           !LL_USART_IsActiveFlag_REACK(st->instance)) {
        if (hal_elapsed_ms(start) > 100) return HAL_TIMEOUT;
    }
    st->initialized = true;
    AERO_LOGI("HAL_UART", "Port %d init @ %lu baud", cfg->port, cfg->baud_rate);
    return HAL_OK;
}
hal_status_t hal_uart_deinit(hal_uart_port_t port)
{
    if (port >= HAL_UART_PORT_MAX) return HAL_INVALID;
    LL_USART_Disable(s_uart[port].instance);
    LL_USART_DeInit(s_uart[port].instance);
    s_uart[port].initialized = false;
    return HAL_OK;
}
hal_status_t hal_uart_write(hal_uart_port_t port, const uint8_t *data, size_t len)
{
    if (port >= HAL_UART_PORT_MAX || !s_uart[port].initialized) return HAL_INVALID;
    USART_TypeDef *u = s_uart[port].instance;
    for (size_t i = 0; i < len; i++) {
        hal_tick_t start = hal_get_tick_ms();
        while (!LL_USART_IsActiveFlag_TXE_TXFNF(u)) {
            if (hal_elapsed_ms(start) > 100) return HAL_TIMEOUT;
        }
        LL_USART_TransmitData8(u, data[i]);
    }
    hal_tick_t start = hal_get_tick_ms();
    while (!LL_USART_IsActiveFlag_TC(u)) {
        if (hal_elapsed_ms(start) > 200) return HAL_TIMEOUT;
    }
    return HAL_OK;
}
hal_status_t hal_uart_read(hal_uart_port_t port, uint8_t *data,
                            size_t len, uint32_t timeout_ms)
{
    if (port >= HAL_UART_PORT_MAX || !s_uart[port].initialized) return HAL_INVALID;
    USART_TypeDef *u = s_uart[port].instance;
    for (size_t i = 0; i < len; i++) {
        hal_tick_t start = hal_get_tick_ms();
        while (!LL_USART_IsActiveFlag_RXNE_RXFNE(u)) {
            if (hal_elapsed_ms(start) > timeout_ms) return HAL_TIMEOUT;
        }
        data[i] = LL_USART_ReceiveData8(u);
    }
    return HAL_OK;
}
hal_status_t hal_uart_write_byte(hal_uart_port_t port, uint8_t byte)
{
    return hal_uart_write(port, &byte, 1);
}
hal_status_t hal_uart_read_byte(hal_uart_port_t port, uint8_t *byte,
                                 uint32_t timeout_ms)
{
    return hal_uart_read(port, byte, 1, timeout_ms);
}
uint32_t hal_uart_rx_available(hal_uart_port_t port)
{
    if (port >= HAL_UART_PORT_MAX || !s_uart[port].initialized) return 0;
    return LL_USART_IsActiveFlag_RXNE_RXFNE(s_uart[port].instance) ? 1 : 0;
}
hal_status_t hal_uart_flush_tx(hal_uart_port_t port)
{
    if (port >= HAL_UART_PORT_MAX) return HAL_INVALID;
    USART_TypeDef *u = s_uart[port].instance;
    hal_tick_t start = hal_get_tick_ms();
    while (!LL_USART_IsActiveFlag_TC(u)) {
        if (hal_elapsed_ms(start) > 500) return HAL_TIMEOUT;
    }
    return HAL_OK;
}
void hal_uart_printf(hal_uart_port_t port, const char *fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (len > 0) hal_uart_write(port, (uint8_t *)buf, (size_t)len);
}