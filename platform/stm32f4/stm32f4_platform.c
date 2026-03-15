#include "platform.h"
#include "aero_config.h"
#include "hal_common.h"
#include "stm32f4xx_ll_rcc.h"
#include "stm32f4xx_ll_pwr.h"
#include "stm32f4xx_ll_flash.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_utils.h"
#include "stm32f4xx_ll_cortex.h"
static void clock_init(void)
{
    LL_RCC_HSE_Enable();
    while (!LL_RCC_HSE_IsReady());
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
    LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);
    LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSE, 8, 336, LL_RCC_PLLP_DIV_2);
    LL_RCC_PLL_ConfigDomain_48M(LL_RCC_PLLSOURCE_HSE, 8, 336, 7);
    LL_RCC_PLL_Enable();
    while (!LL_RCC_PLL_IsReady());
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_5);
    while (LL_FLASH_GetLatency() != LL_FLASH_LATENCY_5);
    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_4);
    LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_2);
    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
    while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL);
    LL_SetSystemCoreClock(168000000UL);
    LL_InitTick(168000000UL, AERO_RTOS_TICK_RATE_HZ);
}
void platform_init(void)
{
    clock_init();
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA |
                              LL_AHB1_GRP1_PERIPH_GPIOB |
                              LL_AHB1_GRP1_PERIPH_GPIOC);
    AERO_LOGI("PLATFORM", "STM32F4 @ 168 MHz — AeroFirmware v%s", AERO_FW_VERSION_STR);
}
void platform_reset(void)           { NVIC_SystemReset(); }
const char *platform_mcu_name(void) { return "STM32F407"; }
uint32_t platform_sysclk_hz(void)   { return AERO_SYSCLK_HZ; }
void platform_enter_low_power(void) { __WFI(); }
platform_boot_reason_t platform_boot_reason(void)
{
    if (LL_RCC_IsActiveFlag_IWDGRST())  return PLATFORM_BOOT_WATCHDOG;
    if (LL_RCC_IsActiveFlag_SFTRST())   return PLATFORM_BOOT_SOFTRESET;
    if (LL_RCC_IsActiveFlag_PINRST())   return PLATFORM_BOOT_EXTERNAL;
    if (LL_RCC_IsActiveFlag_PORRST())   return PLATFORM_BOOT_POWERON;
    return PLATFORM_BOOT_UNKNOWN;
}
uint32_t platform_free_ram(void)
{
    extern uint32_t _estack, _end;
    return (uint32_t)&_estack - (uint32_t)&_end;
}