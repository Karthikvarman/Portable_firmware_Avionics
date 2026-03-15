#ifndef OS_API_H
#define OS_API_H
#include <stdint.h>
#include <stdbool.h>
#include "hal_common.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef void *os_task_handle_t;
typedef void *os_queue_handle_t;
typedef void *os_mutex_handle_t;
typedef void *os_sem_handle_t;
typedef void *os_timer_handle_t;
typedef uint32_t os_tick_t;
#define OS_WAIT_FOREVER    0xFFFFFFFFUL
#define OS_NO_WAIT         0UL
typedef void (*os_task_fn_t)(void *arg);
hal_status_t os_task_create(const char *name, os_task_fn_t fn, void *arg,
                             uint32_t priority, uint32_t stack_sz,
                             os_task_handle_t *handle);
void os_task_delete(os_task_handle_t handle);
void os_task_delay_ms(uint32_t ms);
void os_task_yield(void);
void os_task_suspend(os_task_handle_t handle);
void os_task_resume(os_task_handle_t handle);
uint32_t os_task_stack_free(os_task_handle_t handle);
os_queue_handle_t os_queue_create(uint32_t depth, uint32_t item_size);
void              os_queue_delete(os_queue_handle_t q);
bool              os_queue_send(os_queue_handle_t q, const void *item, uint32_t timeout_ms);
bool              os_queue_recv(os_queue_handle_t q, void *item, uint32_t timeout_ms);
uint32_t          os_queue_msgs_waiting(os_queue_handle_t q);
bool              os_queue_send_from_isr(os_queue_handle_t q, const void *item);
os_mutex_handle_t os_mutex_create(void);
void              os_mutex_delete(os_mutex_handle_t m);
bool              os_mutex_lock(os_mutex_handle_t m, uint32_t timeout_ms);
void              os_mutex_unlock(os_mutex_handle_t m);
os_sem_handle_t os_sem_create_binary(void);
os_sem_handle_t os_sem_create_counting(uint32_t max, uint32_t init);
void            os_sem_delete(os_sem_handle_t s);
bool            os_sem_take(os_sem_handle_t s, uint32_t timeout_ms);
void            os_sem_give(os_sem_handle_t s);
void            os_sem_give_from_isr(os_sem_handle_t s);
typedef void (*os_timer_cb_t)(os_timer_handle_t timer, void *arg);
os_timer_handle_t os_timer_create(const char *name, uint32_t period_ms,
                                   bool auto_reload, os_timer_cb_t cb, void *arg);
void os_timer_start(os_timer_handle_t t);
void os_timer_stop(os_timer_handle_t t);
void os_timer_delete(os_timer_handle_t t);
void     os_start_scheduler(void);
os_tick_t os_get_tick_ms(void);
uint32_t  os_get_free_heap(void);
void      os_enter_critical(void);
void      os_exit_critical(void);
#ifdef __cplusplus
}
#endif
#endif 