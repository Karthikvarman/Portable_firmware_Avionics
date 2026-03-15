#include "telemetry.h"
#include "aero_config.h"
#include "hal_timer.h"
#include "hal_uart.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static uint16_t crc16_ccitt(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (int b = 0; b < 8; b++)
      crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
  }
  return crc;
}
static uint8_t nmea_xor(const char *s) {
  uint8_t csum = 0;
  for (const char *p = s + 1; *p && *p != '*'; p++)
    csum ^= (uint8_t)*p;
  return csum;
}
void telemetry_build_frame(telem_frame_t *frame, const aero_ekf_t *ekf,
                           const sal_data_t *imu, const sal_data_t *baro,
                           const sal_data_t *gnss, const sal_data_t *env,
                           const sal_data_t *co,
                           const stabilization_cmd_t *ctrl) {
  if (!frame)
    return;
  memset(frame, 0, sizeof(telem_frame_t));
  float roll = 0, pitch = 0, yaw = 0;
  if (ekf && ekf->initialized) {
    aero_ekf_get_euler(ekf, &roll, &pitch, &yaw);
    frame->pos_n_m = ekf->pos[0];
    frame->pos_e_m = ekf->pos[1];
    frame->alt_ekf_m = -ekf->pos[2];
    frame->vel_n_ms = ekf->vel[0];
    frame->vel_e_ms = ekf->vel[1];
    frame->vel_d_ms = ekf->vel[2];
  }
  frame->timestamp_ms = hal_get_tick_ms();
  frame->roll_deg = roll;
  frame->pitch_deg = pitch;
  frame->yaw_deg = yaw;
  if (imu) {
    frame->ax_ms2 = imu->imu.ax;
    frame->ay_ms2 = imu->imu.ay;
    frame->az_ms2 = imu->imu.az;
    frame->gx_rads = imu->imu.gx;
    frame->gy_rads = imu->imu.gy;
    frame->gz_rads = imu->imu.gz;
  }
  if (baro) {
    frame->pressure_hpa = baro->baro.pressure_hpa;
    frame->alt_baro_m = baro->baro.altitude_m;
    frame->temp_baro_c = baro->baro.temperature_c;
  }
  if (gnss) {
    frame->lat_deg = (float)gnss->gnss.latitude_deg;
    frame->lon_deg = (float)gnss->gnss.longitude_deg;
    frame->alt_gnss_m = gnss->gnss.altitude_m;
    frame->speed_mps = gnss->gnss.speed_mps;
    frame->satellites = gnss->gnss.num_satellites;
    frame->gnss_fix = gnss->gnss.valid;
  }
  if (env) {
    frame->temperature_c = env->env.temperature_c;
    frame->humidity_pct = env->env.humidity_pct;
  }
  if (co) {
    frame->co_ppm = co->co.ppm;
  }
  if (ctrl) {
    frame->roll_setpoint = ctrl->roll_setpoint;
    frame->pitch_setpoint = ctrl->pitch_setpoint;
    frame->roll_out = ctrl->roll_out;
    frame->pitch_out = ctrl->pitch_out;
    frame->yaw_out = ctrl->yaw_out;
    frame->throttle_out = ctrl->throttle_out;
  }
}
static uint8_t s_tx_buf[AERO_TELEM_BUF_SIZE];
void telemetry_transmit(const telem_frame_t *frame) {
  if (!frame)
    return;
  uint8_t *p = s_tx_buf;
  *p++ = AERO_TELEM_FRAME_HEADER;
  const uint8_t *payload = (const uint8_t *)frame;
  uint16_t payload_len = sizeof(telem_frame_t);
  *p++ = (uint8_t)(payload_len & 0xFF);
  *p++ = (uint8_t)(payload_len >> 8);
  *p++ = 0x01;
  memcpy(p, payload, payload_len);
  p += payload_len;
  uint16_t crc = crc16_ccitt(s_tx_buf + 1, (size_t)(p - s_tx_buf - 1));
  *p++ = (uint8_t)(crc >> 8);
  *p++ = (uint8_t)(crc & 0xFF);
  *p++ = AERO_TELEM_FRAME_FOOTER;
  hal_uart_write(HAL_UART_PORT_1, s_tx_buf, (size_t)(p - s_tx_buf));
}
void telemetry_transmit_ascii(const telem_frame_t *frame) {
  if (!frame)
    return;
  char buf[256];
  int n = snprintf(
      buf, sizeof(buf),
      "$AERO,%lu,%.2f,%.2f,%.2f,%.2f,%.6f,%.6f,%u,%.1f,%.2f,%.2f,%.2f,%.2f",
      frame->timestamp_ms, frame->roll_deg, frame->pitch_deg, frame->yaw_deg,
      frame->alt_ekf_m, frame->lat_deg, frame->lon_deg, frame->satellites,
      frame->co_ppm, frame->roll_setpoint, frame->pitch_setpoint,
      frame->roll_out, frame->pitch_out);
  uint8_t csum = nmea_xor(buf);
  n += snprintf(buf + n, sizeof(buf) - n, "*%02X\r\n", csum);
  hal_uart_write(HAL_UART_PORT_1, (uint8_t *)buf, (size_t)n);
}