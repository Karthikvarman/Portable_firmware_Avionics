#include "platform.h"
#include "hal_timer.h"
#include "hal_gpio.h"
#include "aero_config.h"
#include "stm32h7xx_ll_rcc.h"
#include "stm32h7xx_ll_pwr.h"
#include "stm32h7xx_ll_system.h"
#include "stm32h7xx_ll_bus.h"
#include "stm32h7xx_ll_utils.h"
#include "stm32h7xx_ll_cortex.h"
static void clock_init(void)
{
    LL_APB4_GRP1_EnableClock(LL_APB4_GRP1_PERIPH_SYSCFG);
    LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE0);
    while (!LL_PWR_IsActiveFlag_VOS());
    LL_RCC_HSE_Enable();
    while (!LL_RCC_HSE_IsReady());
    LL_RCC_PLL_SetSource(LL_RCC_PLLSOURCE_HSE);
    LL_RCC_PLL1P_Enable();
    LL_RCC_PLL1Q_Enable();
    LL_RCC_PLL1R_Enable();
    LL_RCC_PLL1_SetM(5);
    LL_RCC_PLL1_SetN(192);
    LL_RCC_PLL1_SetP(2);
    LL_RCC_PLL1_SetQ(2);
    LL_RCC_PLL1_SetR(2);
    LL_RCC_PLL1_SetVCOInputRange(LL_RCC_PLLINPUTRANGE_4_8);
    LL_RCC_PLL1_SetVCOOutputRange(LL_RCC_PLLVCORANGE_WIDE);
    LL_RCC_PLL1_Enable();
    while (!LL_RCC_PLL1_IsReady());
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_4);
    while (LL_FLASH_GetLatency() != LL_FLASH_LATENCY_4);
    LL_RCC_SetAHBPrescaler(LL_RCC_AHB_DIV_2);  
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_2); 
    LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_2); 
    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL1);
    while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL1);
    LL_SetSystemCoreClock(480000000UL);
    LL_InitTick(480000000UL, AERO_RTOS_TICK_RATE_HZ);
}
static void gpio_init(void)
{
    LL_AHB4_GRP1_EnableClock(LL_AHB4_GRP1_PERIPH_GPIOA |
                              LL_AHB4_GRP1_PERIPH_GPIOB |
                              LL_AHB4_GRP1_PERIPH_GPIOC |
                              LL_AHB4_GRP1_PERIPH_GPIOD |
                              LL_AHB4_GRP1_PERIPH_GPIOE);
    hal_gpio_config_t led = {
        .port  = 1, 
        .pin   = 0,
        .mode  = HAL_GPIO_MODE_OUTPUT_PP,
        .pull  = HAL_GPIO_PULL_NONE,
        .initial_state = HAL_GPIO_HIGH,
    };
    hal_gpio_init(&led);
}
static void mpu_init(void)
{
    ARM_MPU_Disable();
    MPU->RBAR = (0x08000000U) | MPU_RBAR_VALID_Msk | (0U << MPU_RBAR_REGION_Pos);
    MPU->RASR = (1U  << MPU_RASR_ENABLE_Pos)   |
                (28U << MPU_RASR_SIZE_Pos)       |  
                (0b000110 << MPU_RASR_AP_Pos)    |  
                (1U << MPU_RASR_TEX_Pos)         |
                (1U << MPU_RASR_C_Pos)            |
                (1U << MPU_RASR_B_Pos);
    MPU->RBAR = (0x24000000U) | MPU_RBAR_VALID_Msk | (1U << MPU_RBAR_REGION_Pos);
    MPU->RASR = (1U  << MPU_RASR_ENABLE_Pos)   |
                (20U << MPU_RASR_SIZE_Pos)       |  
                (0b000011 << MPU_RASR_AP_Pos)    |  
                (1U << MPU_RASR_TEX_Pos)         |
                (1U << MPU_RASR_C_Pos)            |
                (1U << MPU_RASR_B_Pos);
    ARM_MPU_Enable(MPU_CTRL_PRIVDEFENA_Msk);
}
static void cache_init(void)
{
    SCB_EnableICache();
    SCB_EnableDCache();
}
void platform_init(void)
{
    __disable_irq();
    cache_init();
    mpu_init();
    clock_init();
    gpio_init();
    __enable_irq();
    AERO_LOGI("PLATFORM", "STM32H7 @ 480 MHz — AeroFirmware v%s",
              AERO_FW_VERSION_STR);
}
void platform_reset(void)
{
    NVIC_SystemReset();
}
const char *platform_mcu_name(void)
{
    return "STM32H743";
}
uint32_t platform_sysclk_hz(void)
{
    return AERO_SYSCLK_HZ;
}
void platform_enter_low_power(void)
{
    __WFI();
}