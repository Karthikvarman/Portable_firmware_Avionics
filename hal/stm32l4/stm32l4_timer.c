#include "hal_timer.h"
#include <string.h>
#include <stdlib.h>

#ifdef TARGET_STM32L4
#include "stm32l4xx_hal.h"
#include "stm32l4xx_ll_cortex.h"
#endif

typedef struct {
    bool initialized;
    uint32_t tick_rate_hz;
    uint32_t tick_count;
} stm32l4_timer_platform_t;

static hal_status_t stm32l4_timer_init(hal_timer_t *timer);
static hal_status_t stm32l4_timer_deinit(hal_timer_t *timer);
static uint32_t stm32l4_timer_get_tick_rate_hz(hal_timer_t *timer);
static uint32_t stm32l4_timer_get_tick_count_ms(hal_timer_t *timer);
static uint32_t stm32l4_timer_get_tick_count_us(hal_timer_t *timer);
static void stm32l4_timer_delay_ms(hal_timer_t *timer, uint32_t ms);
static void stm32l4_timer_delay_us(hal_timer_t *timer, uint32_t us);

static const hal_timer_ops_t stm32l4_timer_ops = {
    .init = stm32l4_timer_init,
    .deinit = stm32l4_timer_deinit,
    .get_tick_rate_hz = stm32l4_timer_get_tick_rate_hz,
    .get_tick_count_ms = stm32l4_timer_get_tick_count_ms,
    .get_tick_count_us = stm32l4_timer_get_tick_count_us,
    .delay_ms = stm32l4_timer_delay_ms,
    .delay_us = stm32l4_timer_delay_us
};

hal_status_t hal_stm32l4_timer_register(hal_timer_t *timer) {
    HAL_CHECK_PTR(timer);
    return hal_timer_register_driver(timer, &stm32l4_timer_ops);
}

static hal_status_t stm32l4_timer_init(hal_timer_t *timer) {
    HAL_CHECK_PTR(timer);
    
    stm32l4_timer_platform_t *platform = malloc(sizeof(stm32l4_timer_platform_t));
    if (!platform) return HAL_IO_ERROR;
    
    memset(platform, 0, sizeof(stm32l4_timer_platform_t));
    timer->platform_data = platform;
    
    platform->initialized = true;
    platform->tick_rate_hz = 1000;
    platform->tick_count = 0;
    
    return HAL_OK;
}

static hal_status_t stm32l4_timer_deinit(hal_timer_t *timer) {
    HAL_CHECK_PTR(timer);
    
    stm32l4_timer_platform_t *platform = (stm32l4_timer_platform_t *)timer->platform_data;
    if (!platform) return HAL_NOT_INIT;
    
    free(platform);
    timer->platform_data = NULL;
    platform->initialized = false;
    
    return HAL_OK;
}

static uint32_t stm32l4_timer_get_tick_rate_hz(hal_timer_t *timer) {
    HAL_CHECK_PTR(timer, 0);
    
    stm32l4_timer_platform_t *platform = (stm32l4_timer_platform_t *)timer->platform_data;
    if (!platform || !platform->initialized) return 0;
    
    return platform->tick_rate_hz;
}

static uint32_t stm32l4_timer_get_tick_count_ms(hal_timer_t *timer) {
    HAL_CHECK_PTR(timer, 0);
    
    stm32l4_timer_platform_t *platform = (stm32l4_timer_platform_t *)timer->platform_data;
    if (!platform || !platform->initialized) return 0;
    
    return platform->tick_count;
}

static uint32_t stm32l4_timer_get_tick_count_us(hal_timer_t *timer) {
    HAL_CHECK_PTR(timer, 0);
    
    stm32l4_timer_platform_t *platform = (stm32l4_timer_platform_t *)timer->platform_data;
    if (!platform || !platform->initialized) return 0;
    
    return platform->tick_count * 1000;
}

static void stm32l4_timer_delay_ms(hal_timer_t *timer, uint32_t ms) {
    HAL_CHECK_PTR(timer);
    
    for (volatile uint32_t i = 0; i < ms * 1000; i++) {
        __NOP();
    }
}

static void stm32l4_timer_delay_us(hal_timer_t *timer, uint32_t us) {
    HAL_CHECK_PTR(timer);
    
    for (volatile uint32_t i = 0; i < us; i++) {
        __NOP();
    }
}
