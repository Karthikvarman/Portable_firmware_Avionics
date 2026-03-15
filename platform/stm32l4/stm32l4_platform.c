#include "platform.h"
#include "aero_config.h"
#include "hal_common.h"
#include "stm32l4xx_ll_rcc.h"
#include "stm32l4xx_ll_pwr.h"
#include "stm32l4xx_ll_flash.h"
#include "stm32l4xx_ll_bus.h"
#include "stm32l4xx_ll_utils.h"
static void clock_init(void)
{
    LL_RCC_MSI_Enable();
    while (!LL_RCC_MSI_IsReady());
    LL_RCC_MSI_SetRange(LL_RCC_MSIRANGE_6); 
    LL_RCC_MSI_SetCalibTrimming(0);
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
    LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);
    LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_MSI, LL_RCC_PLLM_DIV_1, 40, LL_RCC_PLLR_DIV_2);
    LL_RCC_PLL_EnableDomain_SYS();
    LL_RCC_PLL_Enable();
    while (!LL_RCC_PLL_IsReady());
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_4);
    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
    LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_1);
    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
    while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL);
    LL_SetSystemCoreClock(80000000UL);
    LL_InitTick(80000000UL, AERO_RTOS_TICK_RATE_HZ);
}
void platform_init(void)
{
    clock_init();
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA |
                              LL_AHB2_GRP1_PERIPH_GPIOB |
                              LL_AHB2_GRP1_PERIPH_GPIOC);
    AERO_LOGI("PLATFORM", "STM32L4 @ 80 MHz — AeroFirmware v%s", AERO_FW_VERSION_STR);
}
void platform_reset(void)           { NVIC_SystemReset(); }
const char *platform_mcu_name(void) { return "STM32L476"; }
uint32_t platform_sysclk_hz(void)   { return AERO_SYSCLK_HZ; }
void platform_enter_low_power(void) { __WFI(); }
platform_boot_reason_t platform_boot_reason(void)
{
    if (LL_RCC_IsActiveFlag_IWDGRST())  return PLATFORM_BOOT_WATCHDOG;
    if (LL_RCC_IsActiveFlag_SFTRST())   return PLATFORM_BOOT_SOFTRESET;
    if (LL_RCC_IsActiveFlag_PINRST())   return PLATFORM_BOOT_EXTERNAL;
    if (LL_RCC_IsActiveFlag_BORRST())   return PLATFORM_BOOT_POWERON;
    return PLATFORM_BOOT_UNKNOWN;
}
uint32_t platform_free_ram(void)
{
    extern uint32_t _estack, _end;
    return (uint32_t)&_estack - (uint32_t)&_end;
}