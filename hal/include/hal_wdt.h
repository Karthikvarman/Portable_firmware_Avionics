#ifndef HAL_WDT_H
#define HAL_WDT_H
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
void hal_wdt_init(uint32_t timeout_ms);
void hal_wdt_kick(void);
bool hal_wdt_was_reset_cause(void);
#ifdef __cplusplus
}
#endif
#endif 