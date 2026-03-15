#include "hal_timer.h"
#include "hal_common.h"
#include "stm32f4xx_ll_utils.h"
#include "stm32f4xx_ll_iwdg.h"
#include "stm32f4xx_ll_rcc.h"
static volatile uint32_t g_tick_ms = 0;
void hal_systick_inc(void) { g_tick_ms++; }
hal_tick_t hal_get_tick_ms(void) { return (hal_tick_t)g_tick_ms; }
hal_tick_t hal_elapsed_ms(hal_tick_t since) { return (hal_tick_t)(g_tick_ms - since); }
void hal_delay_ms(uint32_t ms)
{
    uint32_t start = g_tick_ms;
    while ((g_tick_ms - start) < ms) __WFI();
}
void hal_delay_us(uint32_t us)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    uint32_t cycles = us * (SystemCoreClock / 1000000UL);
    while (DWT->CYCCNT < cycles);
}
void hal_wdt_init(uint32_t timeout_ms)
{
    LL_IWDG_Enable(IWDG);
    LL_IWDG_EnableWriteAccess(IWDG);
    LL_IWDG_SetPrescaler(IWDG, LL_IWDG_PRESCALER_256);
    uint32_t reload = (timeout_ms * 32) / 256;
    if (reload > 0xFFF) reload = 0xFFF;
    LL_IWDG_SetReloadCounter(IWDG, reload);
    while (!LL_IWDG_IsReady(IWDG));
    LL_IWDG_ReloadCounter(IWDG);
}
void hal_wdt_kick(void) { LL_IWDG_ReloadCounter(IWDG); }
bool hal_wdt_was_reset_cause(void) { return LL_RCC_IsActiveFlag_IWDGRST(); }