#ifndef FREERTOS_QUEUE_H
#define FREERTOS_QUEUE_H
#include "FreeRTOS.h"
typedef void *QueueHandle_t;
QueueHandle_t xQueueCreate(uint32_t uxQueueLength, uint32_t uxItemSize);
#endif