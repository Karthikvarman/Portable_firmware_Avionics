#include "hal_timer.h"
#include "hal_common.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
hal_tick_t hal_get_tick_ms(void)
{
    return (hal_tick_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}
hal_tick_t hal_elapsed_ms(hal_tick_t since)
{
    hal_tick_t now = hal_get_tick_ms();
    if (now >= since) return (now - since);
    return (0xFFFFFFFFUL - since + now + 1);
}
void hal_delay_ms(uint32_t ms)
{
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        vTaskDelay(pdMS_TO_TICKS(ms));
    } else {
        extern void ets_delay_us(uint32_t us);
        ets_delay_us(ms * 1000);
    }
}
void hal_delay_us(uint32_t us)
{
    extern void ets_delay_us(uint32_t us);
    ets_delay_us(us);
}
hal_status_t hal_wdt_init(uint32_t timeout_ms)
{
    (void)timeout_ms;
    return HAL_OK;
}
void hal_wdt_kick(void)
{
}