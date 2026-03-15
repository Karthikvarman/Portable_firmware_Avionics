#include "hal_timer.h"
#include "hal_common.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
hal_tick_t hal_get_tick_ms(void)
{
    return (hal_tick_t)(esp_timer_get_time() / 1000ULL);
}
hal_tick_t hal_elapsed_ms(hal_tick_t since)
{
    return (hal_tick_t)(hal_get_tick_ms() - since);
}
void hal_delay_ms(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}
void hal_delay_us(uint32_t us)
{
    esp_rom_delay_us(us);
}
void hal_wdt_init(uint32_t timeout_ms)
{
    esp_task_wdt_config_t cfg = {
        .timeout_ms      = timeout_ms,
        .idle_core_mask  = 0,
        .trigger_panic   = true,
    };
    esp_task_wdt_init(&cfg);
    esp_task_wdt_add(NULL);   
    AERO_LOGI("HAL_TMR", "Task WDT initialized: %lu ms timeout", timeout_ms);
}
void hal_wdt_kick(void)
{
    esp_task_wdt_reset();
}
bool hal_wdt_was_reset_cause(void)
{
    return false;
}