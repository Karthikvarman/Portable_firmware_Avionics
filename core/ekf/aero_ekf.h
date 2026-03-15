#ifndef AERO_EKF_H
#define AERO_EKF_H
#include <stdint.h>
#include <stdbool.h>
#include "aero_config.h"
#include "sal_sensor.h"
#include "hal_common.h"
#ifdef __cplusplus
extern "C" {
#endif
#define EKF_N    15  
typedef float aero_mat15x15_t[EKF_N][EKF_N];
typedef float aero_vec15_t[EKF_N];
typedef float aero_quat_t[4];   
typedef float aero_vec3_t[3];
typedef struct {
    float pos[3];       
    float vel[3];       
    float quat[4];      
    float accel_bias[3];
    float gyro_bias[3]; 
    aero_mat15x15_t P;
    aero_mat15x15_t Q;
    float innov_baro;
    float innov_mag[3];
    float innov_gps_pos[3];
    float innov_gps_vel[3];
    float NIS_baro;     
    float NIS_gps;
    float dt;           
    bool  initialized;
    uint32_t update_count;
    hal_tick_t last_imu_tick;
} aero_ekf_t;
void aero_ekf_init(aero_ekf_t *ekf, float dt);
void aero_ekf_set_initial_state(aero_ekf_t *ekf,
                                  float lat_deg, float lon_deg,
                                  float alt_m,
                                  float heading_deg);
void aero_ekf_propagate(aero_ekf_t *ekf, const sal_imu_data_t *imu_data);
void aero_ekf_update_baro(aero_ekf_t *ekf, const sal_baro_data_t *baro);
void aero_ekf_update_mag(aero_ekf_t *ekf, const sal_mag_data_t *mag,
                          const float mag_ref_gauss[3]);
void aero_ekf_update_gnss(aero_ekf_t *ekf, const sal_gnss_data_t *gnss);
void aero_ekf_get_euler(const aero_ekf_t *ekf,
                          float *roll_deg, float *pitch_deg, float *yaw_deg);
void aero_ekf_get_rot_matrix(const aero_ekf_t *ekf, float R[3][3]);
void aero_ekf_get_pos_std(const aero_ekf_t *ekf, float sigma[3]);
bool aero_ekf_is_healthy(const aero_ekf_t *ekf);
void aero_ekf_reset(aero_ekf_t *ekf);
#ifdef __cplusplus
}
#endif
#endif 