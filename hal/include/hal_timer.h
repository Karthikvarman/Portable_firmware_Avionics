#ifndef HAL_TIMER_H
#define HAL_TIMER_H
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef uint32_t hal_tick_t;
hal_tick_t hal_get_tick_ms(void);
hal_tick_t hal_elapsed_ms(hal_tick_t since);
void hal_delay_ms(uint32_t ms);
void hal_delay_us(uint32_t us);
#ifdef TARGET_STM32
void hal_systick_inc(void);
#endif
void hal_wdt_init(uint32_t timeout_ms);
void hal_wdt_kick(void);
bool hal_wdt_was_reset_cause(void);
#ifdef __cplusplus
}
#endif
#endif 