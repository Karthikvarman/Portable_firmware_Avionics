#ifndef FREERTOS_H
#define FREERTOS_H
#include <stddef.h>
#include <stdint.h>
#define pdMS_TO_TICKS(x) (x)
typedef void *TaskHandle_t;
typedef uint32_t TickType_t;
#define portDISABLE_INTERRUPTS()
#define portENABLE_INTERRUPTS()
#define portENTER_CRITICAL()
#define portEXIT_CRITICAL()
#define portTICK_PERIOD_MS 1
#endif