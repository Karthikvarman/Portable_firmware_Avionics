#ifndef FREERTOS_SEMPHR_H
#define FREERTOS_SEMPHR_H
#include "FreeRTOS.h"
typedef void *SemaphoreHandle_t;
#define xSemaphoreCreateMutex() ((SemaphoreHandle_t)0x1)
#define xSemaphoreTake(x, y) (1)
#define xSemaphoreGive(x) (1)
#endif