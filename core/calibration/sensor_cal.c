#include "sensor_cal.h"
#include "aero_config.h"
#include "hal_flash.h"
#include <math.h>
#include <string.h>
#define GRAVITY_MSS 9.80665f
typedef struct {
  sensor_cal_state_t state;
  uint16_t target_samples;
  uint16_t current_samples;
  double sum_accel[3];
  double sum_gyro[3];
  float accel_bias[3];
  float gyro_bias[3];
} sensor_cal_ctx_t;
static sensor_cal_ctx_t s_cal;
void sensor_cal_init(uint16_t sample_count) {
  memset(&s_cal, 0, sizeof(s_cal));
  s_cal.state = CAL_STATE_IDLE;
  s_cal.target_samples = sample_count;
}
void sensor_cal_start(void) {
  s_cal.current_samples = 0;
  for (int i = 0; i < 3; i++) {
    s_cal.sum_accel[i] = 0;
    s_cal.sum_gyro[i] = 0;
  }
  s_cal.state = CAL_STATE_COLLECTING;
  AERO_LOGI("CAL", "Starting automatic IMU calibration...");
}
bool sensor_cal_update(const sal_imu_data_t *imu) {
  if (s_cal.state != CAL_STATE_COLLECTING)
    return false;
  s_cal.sum_accel[0] += imu->ax;
  s_cal.sum_accel[1] += imu->ay;
  s_cal.sum_accel[2] += imu->az;
  s_cal.sum_gyro[0] += imu->gx;
  s_cal.sum_gyro[1] += imu->gy;
  s_cal.sum_gyro[2] += imu->gz;
  s_cal.current_samples++;
  if (s_cal.current_samples >= s_cal.target_samples) {
    float inv_n = 1.0f / (float)s_cal.target_samples;
    s_cal.accel_bias[0] = (float)(s_cal.sum_accel[0] * inv_n);
    s_cal.accel_bias[1] = (float)(s_cal.sum_accel[1] * inv_n);
    s_cal.accel_bias[2] = (float)(s_cal.sum_accel[2] * inv_n) - GRAVITY_MSS;
    s_cal.gyro_bias[0] = (float)(s_cal.sum_gyro[0] * inv_n);
    s_cal.gyro_bias[1] = (float)(s_cal.sum_gyro[1] * inv_n);
    s_cal.gyro_bias[2] = (float)(s_cal.sum_gyro[2] * inv_n);
    s_cal.state = CAL_STATE_SUCCESS;
    AERO_LOGI("CAL",
              "Calibration complete! AccelBias: [%.3f, %.3f, %.3f] m/s^2",
              s_cal.accel_bias[0], s_cal.accel_bias[1], s_cal.accel_bias[2]);
    AERO_LOGI("CAL",
              "Calibration complete! GyroBias:  [%.3f, %.3f, %.3f] rad/s",
              s_cal.gyro_bias[0], s_cal.gyro_bias[1], s_cal.gyro_bias[2]);
    return true;
  }
  return false;
}
sensor_cal_state_t sensor_cal_get_state(void) { return s_cal.state; }
float sensor_cal_get_progress(void) {
  if (s_cal.target_samples == 0)
    return 0;
  return (float)s_cal.current_samples / (float)s_cal.target_samples;
}
void sensor_cal_get_offsets(float accel_bias[3], float gyro_bias[3]) {
  memcpy(accel_bias, s_cal.accel_bias, sizeof(s_cal.accel_bias));
  memcpy(gyro_bias, s_cal.gyro_bias, sizeof(s_cal.gyro_bias));
}
void sensor_cal_reset(void) {
  s_cal.state = CAL_STATE_IDLE;
  s_cal.current_samples = 0;
  memset(s_cal.accel_bias, 0, sizeof(s_cal.accel_bias));
  memset(s_cal.gyro_bias, 0, sizeof(s_cal.gyro_bias));
}
hal_status_t sensor_cal_save(void) {
  uint32_t addr = AERO_DATA_FLASH_ADDR;
  uint8_t buffer[sizeof(float) * 6];
  memcpy(buffer, s_cal.accel_bias, sizeof(float) * 3);
  memcpy(buffer + sizeof(float) * 3, s_cal.gyro_bias, sizeof(float) * 3);
  hal_flash_erase_sector(addr);
  return hal_flash_write(addr, buffer, sizeof(buffer));
}
bool sensor_cal_load(void) {
  uint32_t addr = AERO_DATA_FLASH_ADDR;
  uint8_t buffer[sizeof(float) * 6];
  if (hal_flash_read(addr, buffer, sizeof(buffer)) != HAL_OK)
    return false;
  if (buffer[0] == 0xFF && buffer[1] == 0xFF)
    return false;
  memcpy(s_cal.accel_bias, buffer, sizeof(float) * 3);
  memcpy(s_cal.gyro_bias, buffer + sizeof(float) * 3, sizeof(float) * 3);
  AERO_LOGI("CAL", "Loaded offsets from storage.");
  return true;
}