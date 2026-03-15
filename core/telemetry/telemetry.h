#ifndef TELEMETRY_H
#define TELEMETRY_H
#include "aero_ekf.h"
#include "sal_sensor.h"
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct __attribute__((packed)) {
  uint32_t timestamp_ms;
  float roll_deg;
  float pitch_deg;
  float yaw_deg;
  float pos_n_m;
  float pos_e_m;
  float alt_ekf_m;
  float vel_n_ms;
  float vel_e_ms;
  float vel_d_ms;
  float ax_ms2, ay_ms2, az_ms2;
  float gx_rads, gy_rads, gz_rads;
  float pressure_hpa;
  float alt_baro_m;
  float temp_baro_c;
  float lat_deg;
  float lon_deg;
  float alt_gnss_m;
  float speed_mps;
  uint8_t satellites;
  uint8_t gnss_fix;
  float temperature_c;
  float humidity_pct;
  float co_ppm;
  float roll_setpoint;
  float pitch_setpoint;
  float roll_out;
  float pitch_out;
  float yaw_out;
  float throttle_out;
} telem_frame_t;
#include "stabilization.h"
void telemetry_build_frame(telem_frame_t *frame, const aero_ekf_t *ekf,
                           const sal_data_t *imu, const sal_data_t *baro,
                           const sal_data_t *gnss, const sal_data_t *env,
                           const sal_data_t *co,
                           const stabilization_cmd_t *ctrl);
void telemetry_transmit(const telem_frame_t *frame);
void telemetry_transmit_ascii(const telem_frame_t *frame);
#ifdef __cplusplus
}
#endif
#endif 