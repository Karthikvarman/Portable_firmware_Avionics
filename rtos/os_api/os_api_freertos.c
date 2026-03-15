#include "os_api.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"
#include "portable.h"
#include <string.h>
hal_status_t os_task_create(const char *name, os_task_fn_t fn, void *arg,
                             uint32_t priority, uint32_t stack_sz,
                             os_task_handle_t *handle)
{
    BaseType_t rc = xTaskCreate((TaskFunction_t)fn, name,
                                 (uint16_t)stack_sz,
                                 arg,
                                 (UBaseType_t)priority,
                                 (TaskHandle_t *)handle);
    return (rc == pdPASS) ? HAL_OK : HAL_NO_MEMORY;
}
void os_task_delete(os_task_handle_t handle)
{
    vTaskDelete((TaskHandle_t)handle);
}
void os_task_delay_ms(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}
void os_task_yield(void)
{
    taskYIELD();
}
void os_task_suspend(os_task_handle_t handle)
{
    vTaskSuspend((TaskHandle_t)handle);
}
void os_task_resume(os_task_handle_t handle)
{
    vTaskResume((TaskHandle_t)handle);
}
uint32_t os_task_stack_free(os_task_handle_t handle)
{
    return (uint32_t)uxTaskGetStackHighWaterMark((TaskHandle_t)handle) * 4;
}
os_queue_handle_t os_queue_create(uint32_t depth, uint32_t item_size)
{
    return (os_queue_handle_t)xQueueCreate(depth, item_size);
}
void os_queue_delete(os_queue_handle_t q)
{
    vQueueDelete((QueueHandle_t)q);
}
bool os_queue_send(os_queue_handle_t q, const void *item, uint32_t timeout_ms)
{
    TickType_t ticks = (timeout_ms == OS_WAIT_FOREVER) ?
                       portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xQueueSend((QueueHandle_t)q, item, ticks) == pdTRUE;
}
bool os_queue_recv(os_queue_handle_t q, void *item, uint32_t timeout_ms)
{
    TickType_t ticks = (timeout_ms == OS_WAIT_FOREVER) ?
                       portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xQueueReceive((QueueHandle_t)q, item, ticks) == pdTRUE;
}
uint32_t os_queue_msgs_waiting(os_queue_handle_t q)
{
    return (uint32_t)uxQueueMessagesWaiting((QueueHandle_t)q);
}
bool os_queue_send_from_isr(os_queue_handle_t q, const void *item)
{
    BaseType_t woken = pdFALSE;
    bool ok = xQueueSendFromISR((QueueHandle_t)q, item, &woken) == pdTRUE;
    portYIELD_FROM_ISR(woken);
    return ok;
}
os_mutex_handle_t os_mutex_create(void)
{
    return (os_mutex_handle_t)xSemaphoreCreateMutex();
}
void os_mutex_delete(os_mutex_handle_t m)
{
    vSemaphoreDelete((SemaphoreHandle_t)m);
}
bool os_mutex_lock(os_mutex_handle_t m, uint32_t timeout_ms)
{
    TickType_t ticks = (timeout_ms == OS_WAIT_FOREVER) ?
                       portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTake((SemaphoreHandle_t)m, ticks) == pdTRUE;
}
void os_mutex_unlock(os_mutex_handle_t m)
{
    xSemaphoreGive((SemaphoreHandle_t)m);
}
os_sem_handle_t os_sem_create_binary(void)
{
    return (os_sem_handle_t)xSemaphoreCreateBinary();
}
os_sem_handle_t os_sem_create_counting(uint32_t max, uint32_t init)
{
    return (os_sem_handle_t)xSemaphoreCreateCounting(max, init);
}
void os_sem_delete(os_sem_handle_t s)   { vSemaphoreDelete((SemaphoreHandle_t)s); }
bool os_sem_take(os_sem_handle_t s, uint32_t timeout_ms)
{
    TickType_t ticks = (timeout_ms == OS_WAIT_FOREVER) ?
                       portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTake((SemaphoreHandle_t)s, ticks) == pdTRUE;
}
void os_sem_give(os_sem_handle_t s) { xSemaphoreGive((SemaphoreHandle_t)s); }
void os_sem_give_from_isr(os_sem_handle_t s)
{
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR((SemaphoreHandle_t)s, &woken);
    portYIELD_FROM_ISR(woken);
}
os_timer_handle_t os_timer_create(const char *name, uint32_t period_ms,
                                   bool auto_reload, os_timer_cb_t cb, void *arg)
{
    return (os_timer_handle_t)xTimerCreate(name,
                                            pdMS_TO_TICKS(period_ms),
                                            auto_reload ? pdTRUE : pdFALSE,
                                            arg,
                                            (TimerCallbackFunction_t)cb);
}
void os_timer_start(os_timer_handle_t t)  { xTimerStart((TimerHandle_t)t, 0); }
void os_timer_stop(os_timer_handle_t t)   { xTimerStop((TimerHandle_t)t, 0); }
void os_timer_delete(os_timer_handle_t t) { xTimerDelete((TimerHandle_t)t, 0); }
void os_start_scheduler(void)   { vTaskStartScheduler(); }
os_tick_t os_get_tick_ms(void)  { return (os_tick_t)pdTICKS_TO_MS(xTaskGetTickCount()); }
uint32_t  os_get_free_heap(void){ return (uint32_t)xPortGetFreeHeapSize(); }
void os_enter_critical(void)    { taskENTER_CRITICAL(); }
void os_exit_critical(void)     { taskEXIT_CRITICAL(); }