#ifndef HAL_COMMON_H
#define HAL_COMMON_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "aero_config.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    HAL_OK           = 0,    
    HAL_TIMEOUT      = 1,    
    HAL_BUSY         = 2,    
    HAL_IO_ERROR     = 3,    
    HAL_PARAM_ERROR  = 4,    
    HAL_NOT_INIT     = 5,    
    HAL_NOT_FOUND    = 6,    
    HAL_OVERFLOW     = 7,    
    HAL_UNSUPPORTED  = 8,    
} hal_status_t;
#define HAL_RETURN_IF_ERR(expr)               \
    do {                                      \
        hal_status_t _rc = (expr);            \
        if (_rc != HAL_OK) return _rc;        \
    } while (0)
#define HAL_CHECK_PTR(ptr)                    \
    do {                                      \
        if ((ptr) == NULL) return HAL_PARAM_ERROR; \
    } while (0)
#define HAL_DIV_CEIL(n, d)   (((n) + (d) - 1) / (d))
#define HAL_ARRAY_SIZE(a)    (sizeof(a) / sizeof((a)[0]))
#define HAL_CLAMP(x, lo, hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))
#define HAL_UNUSED(x)        (void)(x)
#ifndef LIKELY
    #ifdef __GNUC__
        #define LIKELY(x)   __builtin_expect(!!(x), 1)
        #define UNLIKELY(x) __builtin_expect(!!(x), 0)
    #else
        #define LIKELY(x)   (x)
        #define UNLIKELY(x) (x)
    #endif
#endif
#ifndef PACKED
    #ifdef __GNUC__
        #define PACKED __attribute__((packed))
    #else
        #define PACKED
    #endif
#endif
#ifndef ALIGNED
    #ifdef __GNUC__
        #define ALIGNED(n) __attribute__((aligned(n)))
    #else
        #define ALIGNED(n)
    #endif
#endif
#ifndef RAM_FUNC
    #ifdef __GNUC__
        #define RAM_FUNC __attribute__((section(".RamFunc"), noinline))
    #else
        #define RAM_FUNC
    #endif
#endif
#ifdef AERO_MCU_STM32
    #define AERO_ENTER_CRITICAL()   __disable_irq()
    #define AERO_EXIT_CRITICAL()    __enable_irq()
#else
    #define AERO_ENTER_CRITICAL()   portDISABLE_INTERRUPTS()
    #define AERO_EXIT_CRITICAL()    portENABLE_INTERRUPTS()
#endif
typedef uint32_t hal_tick_t;
hal_tick_t hal_get_tick_ms(void);
hal_tick_t hal_elapsed_ms(hal_tick_t since);
void       hal_delay_ms(uint32_t ms);
#ifdef __cplusplus
}
#endif
#endif 