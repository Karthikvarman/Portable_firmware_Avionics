#include "FreeRTOSConfig.h"
#include "hal_common.h"
#include "hal_timer.h"
#include <stdio.h>
void vApplicationStackOverflowHook(void *task, char *name)
{
    (void)task;
    AERO_LOGE("RTOS", "!!! STACK OVERFLOW in task: %s !!!", name ? name : "UNKNOWN");
    for (volatile uint32_t i = 0; i < 500000UL; i++);
    volatile uint32_t *p = (volatile uint32_t *)0xDEADBEEF;
    (void)(*p);
}
void vApplicationMallocFailedHook(void)
{
    AERO_LOGE("RTOS", "!!! MALLOC FAILED — heap exhausted (heap_4) !!!");
    while (1) { hal_wdt_kick(); }
}
void platform_freertos_assert(const char *file, unsigned line)
{
    AERO_LOGE("ASSERT", "FreeRTOS assertion failed: %s:%u", file, line);
    __disable_irq();
    while (1);
}