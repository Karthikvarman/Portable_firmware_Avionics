#include "platform.h"
#include "hal_timer.h"
#include "hal_wdt.h"
#include "sal_sensor.h"
#include "protocol_manager.h"
#include "app_tasks.h"
#include "aero_config.h"
int main(void)
{
    platform_init();
#if AERO_WDT_ENABLE
    hal_wdt_init(AERO_WDT_TIMEOUT_MS);
#endif
    AERO_LOGI("MAIN", "============================================");
    AERO_LOGI("MAIN", "  AeroFirmware v%s — %s", AERO_FW_VERSION_STR, platform_mcu_name());
    AERO_LOGI("MAIN", "  Build: %s", AERO_FW_BUILD_DATE);
    AERO_LOGI("MAIN", "  Clock: %lu MHz", platform_sysclk_hz() / 1000000UL);
    AERO_LOGI("MAIN", "============================================");
    switch (platform_boot_reason()) {
        case PLATFORM_BOOT_WATCHDOG:  AERO_LOGW("MAIN", "Reset: WATCHDOG"); break;
        case PLATFORM_BOOT_SOFTRESET: AERO_LOGI("MAIN", "Reset: SOFTWARE");  break;
        case PLATFORM_BOOT_POWERON:   AERO_LOGI("MAIN", "Reset: POWER-ON");  break;
        default: break;
    }
    protocol_manager_init();
    hal_status_t rc = sal_init_all();
    if (rc != HAL_OK) {
        AERO_LOGW("MAIN", "One or more sensors failed init — continuing");
    }
    app_tasks_start();
    for (;;) {
        hal_wdt_kick();
        hal_delay_ms(1000);
    }
    return 0;
}