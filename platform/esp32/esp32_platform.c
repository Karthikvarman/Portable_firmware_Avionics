#include "../../config/aero_config.h"
#include "../../hal/include/hal_common.h"
#include "../platform.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
static const char *TAG = "DEVKITV1";
void platform_init(void) {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    nvs_flash_init();
  }
  AERO_LOGI(TAG, "ESP32 DevKitV1 Booting @ 240MHz");
  AERO_LOGI(TAG, "AeroFirmware Avionics v%s", AERO_FW_VERSION_STR);
}
void platform_reset(void) { esp_restart(); }
const char *platform_mcu_name(void) { return "ESP32-DevKitV1"; }
uint32_t platform_sysclk_hz(void) { return 240000000UL; }
void platform_enter_low_power(void) { vTaskDelay(pdMS_TO_TICKS(1)); }
platform_boot_reason_t platform_boot_reason(void) {
  esp_reset_reason_t reason = esp_reset_reason();
  switch (reason) {
  case ESP_RST_POWERON:
    return PLATFORM_BOOT_POWERON;
  case ESP_RST_WDT:
    return PLATFORM_BOOT_WATCHDOG;
  case ESP_RST_SW:
    return PLATFORM_BOOT_SOFTRESET;
  default:
    return PLATFORM_BOOT_UNKNOWN;
  }
}
uint32_t platform_free_ram(void) { return (uint32_t)esp_get_free_heap_size(); }