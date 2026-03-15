#include "platform.h"
#include "hal_common.h"
#include "hal_timer.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
static uint32_t s_sysclk_hz = 80000000UL;
void platform_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
#ifdef AERO_ESP8266_160MHZ
    s_sysclk_hz = 160000000UL;
#else
    s_sysclk_hz = 80000000UL;
#endif
    AERO_LOGI("ESP8266", "Platform init OK — CPU = %lu MHz", s_sysclk_hz / 1000000UL);
    AERO_LOGI("ESP8266", "Free heap = %lu bytes", (uint32_t)esp_get_free_heap_size());
}
const char *platform_mcu_name(void)  { return "ESP8266"; }
uint32_t    platform_sysclk_hz(void) { return s_sysclk_hz; }
platform_boot_reason_t platform_boot_reason(void)
{
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON:  return PLATFORM_BOOT_POWERON;
        case ESP_RST_SW:       return PLATFORM_BOOT_SOFTRESET;
        case ESP_RST_WDT:
        case ESP_RST_TASK_WDT:
        case ESP_RST_INT_WDT:  return PLATFORM_BOOT_WATCHDOG;
        default:               return PLATFORM_BOOT_UNKNOWN;
    }
}
void platform_reset(void)     { esp_restart(); }
void platform_sleep_wfi(void) {  taskYIELD(); }
void platform_sleep_wfe(void) { taskYIELD(); }
uint32_t platform_free_ram(void)
{
    return (uint32_t)esp_get_free_heap_size();
}