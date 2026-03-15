#ifndef SENSOR_CAL_H
#define SENSOR_CAL_H
#include "hal_common.h"
#include "sal_sensor.h"
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
  CAL_STATE_IDLE = 0,
  CAL_STATE_COLLECTING,
  CAL_STATE_SUCCESS,
  CAL_STATE_FAILED
} sensor_cal_state_t;
void sensor_cal_init(uint16_t sample_count);
void sensor_cal_start(void);
bool sensor_cal_update(const sal_imu_data_t *imu);
sensor_cal_state_t sensor_cal_get_state(void);
float sensor_cal_get_progress(void);
void sensor_cal_get_offsets(float accel_bias[3], float gyro_bias[3]);
void sensor_cal_reset(void);
hal_status_t sensor_cal_save(void);
bool sensor_cal_load(void);
#ifdef __cplusplus
}
#endif
#endif 