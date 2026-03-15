#include "platform.h"
#include "hal_common.h"
#include "hal_timer.h"
#include "stm32f7xx_ll_rcc.h"
#include "stm32f7xx_ll_system.h"
#include "stm32f7xx_ll_pwr.h"
#include "stm32f7xx_ll_bus.h"
#include "stm32f7xx_ll_cortex.h"
#include "stm32f7xx_ll_gpio.h"
static uint32_t s_sysclk_hz = 16000000UL;  
static void clock_config(void)
{
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
    LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);
    LL_RCC_HSE_Enable();
    while (!LL_RCC_HSE_IsReady());
    LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSE, 8, 432,
                                  LL_RCC_PLLP_DIV_2);
    LL_RCC_PLL_Enable();
    while (!LL_RCC_PLL_IsReady());
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_7);
    LL_FLASH_EnablePrefetch();
    LL_FLASH_EnableInstCache();
    LL_FLASH_EnableDataCache();
    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_4);
    LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_2);
    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
    while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL);
    LL_PWR_EnableOverDriveMode();
    while (!LL_PWR_IsActiveFlag_OD());
    LL_PWR_EnableOverDriveSwitching();
    while (!LL_PWR_IsActiveFlag_ODSW());
    s_sysclk_hz = 216000000UL;
    SystemCoreClock = s_sysclk_hz;
}
void platform_init(void)
{
    SCB->CPACR |= (3UL << 20) | (3UL << 22);
    SCB_EnableICache();
    SCB_EnableDCache();
    clock_config();
    SysTick_Config(SystemCoreClock / 1000);
    AERO_LOGI("STM32F7", "Platform init OK — SYSCLK = %lu MHz",
              s_sysclk_hz / 1000000UL);
}
const char *platform_mcu_name(void)  { return "STM32F767ZI"; }
uint32_t    platform_sysclk_hz(void) { return s_sysclk_hz; }
platform_boot_reason_t platform_boot_reason(void)
{
    if (LL_RCC_IsActiveFlag_IWDGRST())  { LL_RCC_ClearResetFlags(); return PLATFORM_BOOT_WATCHDOG; }
    if (LL_RCC_IsActiveFlag_SFTRST())   { LL_RCC_ClearResetFlags(); return PLATFORM_BOOT_SOFTRESET; }
    if (LL_RCC_IsActiveFlag_PORRST())   { LL_RCC_ClearResetFlags(); return PLATFORM_BOOT_POWERON; }
    LL_RCC_ClearResetFlags();
    return PLATFORM_BOOT_UNKNOWN;
}
void platform_reset(void)     { NVIC_SystemReset(); }
void platform_sleep_wfi(void) { __WFI(); }
void platform_sleep_wfe(void) { __WFE(); }
uint32_t platform_free_ram(void)
{
    extern uint32_t _ebss;
    extern uint32_t _estack;
    return (uint32_t)&_estack - (uint32_t)&_ebss;
}