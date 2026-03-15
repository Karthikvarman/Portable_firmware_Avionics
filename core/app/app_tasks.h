#ifndef APP_TASKS_H
#define APP_TASKS_H
#include "aero_ekf.h"
#include "sal_sensor.h"
#ifdef __cplusplus
extern "C" {
#endif
void app_tasks_start(void);
void app_get_ekf_state(aero_ekf_t *out);
void app_get_sensor_data(sal_sensor_id_t id, sal_data_t *out);
#ifdef __cplusplus
}
#endif
#endif 