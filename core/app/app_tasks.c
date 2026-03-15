#include "app_tasks.h"
#include "aero_config.h"
#include "aero_ekf.h"
#include "hal_timer.h"
#include "os_api.h"
#include "protocol_manager.h"
#include "sal_sensor.h"
#include "sensor_cal.h"
#include "stabilization.h"
#include "telemetry.h"
#include "wireless_manager.h"
#include <stdio.h>
#include <string.h>

static aero_ekf_t g_ekf;
static sal_data_t g_imu_data;
static sal_data_t g_baro_data;
static sal_data_t g_mag_data;
static sal_data_t g_gnss_data;
static sal_data_t g_env_data;
static sal_data_t g_co_data;
static os_mutex_handle_t g_ekf_mutex;
static os_mutex_handle_t g_data_mutex;
static const float g_mag_ref[3] = {22.5f, 0.0f, -42.0f}; 

static void task_sensor_imu(void *arg) {
  (void)arg;
  os_tick_t last_wake = os_get_tick_ms();
  for (;;) {
    sal_data_t data;
    if (sal_sensor_read(SAL_SENSOR_ACCEL, &data) == HAL_OK) {
      os_mutex_lock(g_data_mutex, OS_WAIT_FOREVER);
      g_imu_data = data;
      os_mutex_unlock(g_data_mutex);
      if (sensor_cal_get_state() == CAL_STATE_COLLECTING) {
        if (sensor_cal_update(&data.imu)) {
#if AERO_CAL_AUTO_SAVE
          sensor_cal_save();
#endif
        }
      } else {
        float ba[3], bg[3];
        sensor_cal_get_offsets(ba, bg);
        data.imu.ax -= ba[0];
        data.imu.ay -= ba[1];
        data.imu.az -= ba[2];
        data.imu.gx -= bg[0];
        data.imu.gy -= bg[1];
        data.imu.gz -= bg[2];
        os_mutex_lock(g_ekf_mutex, OS_WAIT_FOREVER);
        aero_ekf_propagate(&g_ekf, &data.imu);
        float r, p, y, h;
        aero_ekf_get_euler(&g_ekf, &r, &p, &y);
        h = g_ekf.pos[2]; 
        stabilization_update(r, p, data.imu.gz, h,
                             (float)AERO_TASK_IMU_PERIOD_MS / 1000.0f);
        os_mutex_unlock(g_ekf_mutex);
      }
    } else {
      AERO_LOGW("IMU_TASK", "IMU read failed");
    }
    os_task_delay_ms(AERO_TASK_IMU_PERIOD_MS);
  }
}

static void task_sensor_baro(void *arg) {
  (void)arg;
  for (;;) {
    sal_data_t data;
    if (sal_sensor_read(SAL_SENSOR_BAROMETER, &data) == HAL_OK) {
      os_mutex_lock(g_data_mutex, OS_WAIT_FOREVER);
      g_baro_data = data;
      os_mutex_unlock(g_data_mutex);
      os_mutex_lock(g_ekf_mutex, OS_WAIT_FOREVER);
      aero_ekf_update_baro(&g_ekf, &data.baro);
      os_mutex_unlock(g_ekf_mutex);
    }
    os_task_delay_ms(AERO_TASK_BARO_PERIOD_MS);
  }
}

static void task_sensor_mag(void *arg) {
  (void)arg;
  for (;;) {
    sal_data_t data;
    if (sal_sensor_read(SAL_SENSOR_MAGNETOMETER, &data) == HAL_OK) {
      os_mutex_lock(g_data_mutex, OS_WAIT_FOREVER);
      g_mag_data = data;
      os_mutex_unlock(g_data_mutex);
      os_mutex_lock(g_ekf_mutex, OS_WAIT_FOREVER);
      aero_ekf_update_mag(&g_ekf, &data.mag, g_mag_ref);
      os_mutex_unlock(g_ekf_mutex);
    }
    os_task_delay_ms(AERO_TASK_MAG_PERIOD_MS);
  }
}

static void task_sensor_gnss(void *arg) {
  (void)arg;
  for (;;) {
    sal_data_t data;
    if (sal_sensor_read(SAL_SENSOR_GNSS, &data) == HAL_OK) {
      os_mutex_lock(g_data_mutex, OS_WAIT_FOREVER);
      g_gnss_data = data;
      os_mutex_unlock(g_data_mutex);
      if (data.gnss.valid) {
        os_mutex_lock(g_ekf_mutex, OS_WAIT_FOREVER);
        aero_ekf_update_gnss(&g_ekf, &data.gnss);
        os_mutex_unlock(g_ekf_mutex);
      }
    }
    os_task_delay_ms(AERO_TASK_GPS_PERIOD_MS);
  }
}

static void task_sensor_env(void *arg) {
  (void)arg;
  uint32_t co_counter = 0;
  for (;;) {
    sal_data_t data;
    if (sal_sensor_read(SAL_SENSOR_TEMPERATURE, &data) == HAL_OK) {
      os_mutex_lock(g_data_mutex, OS_WAIT_FOREVER);
      g_env_data = data;
      os_mutex_unlock(g_data_mutex);
    }
    co_counter++;
    if (co_counter & 1) {
      sal_data_t co;
      if (sal_sensor_read(SAL_SENSOR_CO_GAS, &co) == HAL_OK) {
        os_mutex_lock(g_data_mutex, OS_WAIT_FOREVER);
        g_co_data = co;
        os_mutex_unlock(g_data_mutex);
        if (co.co.ppm > 35.0f) {
          AERO_LOGW("CO_TASK", "CO ALERT: %.1f ppm", co.co.ppm);
        }
      }
    }
    os_task_delay_ms(500);
  }
}

static void task_telemetry(void *arg) {
  (void)arg;
  for (;;) {
    aero_ekf_t ekf_snap;
    os_mutex_lock(g_ekf_mutex, OS_WAIT_FOREVER);
    ekf_snap = g_ekf;
    os_mutex_unlock(g_ekf_mutex);
    sal_data_t imu, baro, gnss, env, co;
    os_mutex_lock(g_data_mutex, OS_WAIT_FOREVER);
    imu = g_imu_data;
    baro = g_baro_data;
    gnss = g_gnss_data;
    env = g_env_data;
    co = g_co_data;
    os_mutex_unlock(g_data_mutex);
    stabilization_cmd_t ctrl;
    stabilization_get_commands(&ctrl);
    telem_frame_t frame;
    telemetry_build_frame(&frame, &ekf_snap, &imu, &baro, &gnss, &env, &co,
                          &ctrl);
    telemetry_transmit(&frame);
    os_task_delay_ms(AERO_TASK_TELEM_PERIOD_MS);
  }
}

static void task_monitor(void *arg) {
  (void)arg;
  for (;;) {
    for (int i = 0; i < SAL_SENSOR_COUNT; i++) {
      if (!sal_sensor_is_healthy((sal_sensor_id_t)i)) {
        sal_driver_t *drv = sal_get_driver((sal_sensor_id_t)i);
        if (drv) {
          AERO_LOGE("MONITOR", "Sensor '%s' UNHEALTHY — reinit attempt",
                    drv->name);
          sal_sensor_init((sal_sensor_id_t)i);
        }
      }
    }
    os_mutex_lock(g_ekf_mutex, 50);
    bool ekf_ok = aero_ekf_is_healthy(&g_ekf);
    os_mutex_unlock(g_ekf_mutex);
    if (!ekf_ok) {
      AERO_LOGW("MONITOR", "EKF NIS exceeded threshold — solution degraded");
    }
    uint32_t free_heap = os_get_free_heap();
    AERO_LOGI("MONITOR", "Free heap: %lu bytes | EKF: %s", free_heap,
              ekf_ok ? "OK" : "DEGRADED");
    hal_wdt_kick();
    os_task_delay_ms(1000);
  }
}

static void task_uplink(void *arg) {
  (void)arg;
  uint8_t buffer[64];
  int pos = 0;
  AERO_LOGI("UPLINK", "Uplink task started on UART port %d",
            AERO_TELEM_UART_PORT);
  for (;;) {
    uint8_t c;
    if (hal_uart_read_byte(AERO_TELEM_UART_PORT, &c, 100) == HAL_OK) {
      if (c == '\n' || c == '\r') {
        buffer[pos] = '\0';
        if (pos > 0) {
          if (strstr((char *)buffer, "$CMD,CAL") != NULL) {
            AERO_LOGI("UPLOAD", "Remote Command Received: RECALIBRATE");
            sensor_cal_start();
          }
        }
        pos = 0;
      } else if (pos < sizeof(buffer) - 1) {
        if (c >= 32 && c <= 126) { 
          buffer[pos++] = c;
        }
      }
    }
    os_task_delay_ms(10);
  }
}

static void task_wifi(void *arg) {
  (void)arg;
  for (;;) {
    // WiFi task implementation
  }
}

static void task_bluetooth(void *arg) {
  (void)arg;
  for (;;) {
    // Bluetooth task implementation
  }
}

void app_tasks_start(void) {
  aero_ekf_init(&g_ekf, (float)AERO_TASK_IMU_PERIOD_MS / 1000.0f);
  sensor_cal_init(AERO_CAL_SAMPLE_COUNT);
  if (!sensor_cal_load()) {
    sensor_cal_start(); 
  }
  stabilization_init();
  
  // Initialize wireless manager
  wireless_manager_init();
  
  g_ekf_mutex = os_mutex_create();
  g_data_mutex = os_mutex_create();
  os_task_create("tsk_imu", task_sensor_imu, NULL, AERO_PRIO_SENSOR,
                 AERO_TASK_SENSOR_STACK, NULL);
  os_task_create("tsk_baro", task_sensor_baro, NULL, AERO_PRIO_SENSOR,
                 AERO_TASK_SENSOR_STACK, NULL);
  os_task_create("tsk_mag", task_sensor_mag, NULL, AERO_PRIO_SENSOR,
                 AERO_TASK_SENSOR_STACK, NULL);
  os_task_create("tsk_gnss", task_sensor_gnss, NULL, AERO_PRIO_SENSOR,
                 AERO_TASK_SENSOR_STACK, NULL);
  os_task_create("tsk_env", task_sensor_env, NULL, AERO_PRIO_SENSOR,
                 AERO_TASK_SENSOR_STACK, NULL);
  os_task_create("tsk_wifi", task_wifi, NULL, AERO_PRIO_WIRELESS,
                 AERO_TASK_WIRELESS_STACK, NULL);
  os_task_create("tsk_bt", task_bluetooth, NULL, AERO_PRIO_WIRELESS,
                 AERO_TASK_WIRELESS_STACK, NULL);
  os_task_create("tsk_telem", task_telemetry, NULL, AERO_PRIO_TELEMETRY,
                 AERO_TASK_TELEMETRY_STACK, NULL);
  os_task_create("tsk_mon", task_monitor, NULL, AERO_PRIO_MONITOR,
                 AERO_TASK_MONITOR_STACK, NULL);
  os_task_create("tsk_up", task_uplink, NULL, AERO_PRIO_TELEMETRY,
                 AERO_TASK_TELEMETRY_STACK, NULL);
  AERO_LOGI("APP", "All tasks created — starting FreeRTOS scheduler");
  os_start_scheduler();
  for (;;)
    ;
}

void app_get_ekf_state(aero_ekf_t *out) {
  os_mutex_lock(g_ekf_mutex, OS_WAIT_FOREVER);
  *out = g_ekf;
  os_mutex_unlock(g_ekf_mutex);
}

void app_get_sensor_data(sal_sensor_id_t id, sal_data_t *out) {
  os_mutex_lock(g_data_mutex, OS_WAIT_FOREVER);
  switch (id) {
  case SAL_SENSOR_ACCEL:
    *out = g_imu_data;
    break;
  case SAL_SENSOR_BAROMETER:
    *out = g_baro_data;
    break;
  case SAL_SENSOR_MAGNETOMETER:
    *out = g_mag_data;
    break;
  case SAL_SENSOR_GNSS:
    *out = g_gnss_data;
    break;
  case SAL_SENSOR_TEMPERATURE:
    *out = g_env_data;
    break;
  case SAL_SENSOR_CO_GAS:
    *out = g_co_data;
    break;
  default:
    break;
  }
  os_mutex_unlock(g_data_mutex);
}