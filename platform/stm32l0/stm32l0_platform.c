#include "platform.h"
#include "aero_config.h"
#include "hal_common.h"
#include "stm32l0xx_ll_rcc.h"
#include "stm32l0xx_ll_bus.h"
#include "stm32l0xx_ll_pwr.h"
#include "stm32l0xx_ll_flash.h"
#include "stm32l0xx_ll_utils.h"
static void clock_init(void)
{
    LL_RCC_HSI_Enable();
    while (!LL_RCC_HSI_IsReady());
    LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSI, LL_RCC_PLL_MUL_4, LL_RCC_PLL_DIV_2);
    LL_RCC_PLL_Enable();
    while (!LL_RCC_PLL_IsReady());
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_1);
    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
    LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_1);
    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
    while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL);
    LL_SetSystemCoreClock(32000000UL);
    LL_InitTick(32000000UL, AERO_RTOS_TICK_RATE_HZ);
}
void platform_init(void)
{
    clock_init();
    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA |
                             LL_IOP_GRP1_PERIPH_GPIOB);
    AERO_LOGI("PLATFORM", "STM32L0 @ 32 MHz — AeroFirmware v%s", AERO_FW_VERSION_STR);
}
void platform_reset(void)           { NVIC_SystemReset(); }
const char *platform_mcu_name(void) { return "STM32L073"; }
uint32_t platform_sysclk_hz(void)   { return AERO_SYSCLK_HZ; }
void platform_enter_low_power(void) { __WFI(); }
platform_boot_reason_t platform_boot_reason(void) { return PLATFORM_BOOT_UNKNOWN; }
uint32_t platform_free_ram(void)
{
    extern uint32_t _estack, _end;
    return (uint32_t)&_estack - (uint32_t)&_end;
}