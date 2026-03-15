#ifndef PLATFORM_H
#define PLATFORM_H
#include <stdint.h>
#include <stdbool.h>
#include "hal_common.h"
#ifdef __cplusplus
extern "C" {
#endif
void platform_init(void);
void platform_reset(void);
const char *platform_mcu_name(void);
uint32_t platform_sysclk_hz(void);
void platform_enter_low_power(void);
typedef enum {
    PLATFORM_BOOT_POWERON  = 0,
    PLATFORM_BOOT_WATCHDOG = 1,
    PLATFORM_BOOT_SOFTRESET= 2,
    PLATFORM_BOOT_EXTERNAL = 3,
    PLATFORM_BOOT_UNKNOWN  = 4,
} platform_boot_reason_t;
platform_boot_reason_t platform_boot_reason(void);
uint32_t platform_free_ram(void);
#ifdef __cplusplus
}
#endif
#endif 