#include "aero_ekf.h"
#include "hal_timer.h"
#include "aero_config.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#define AERO_EKF_GRAVITY        9.80665f
#define AERO_EKF_RAD_TO_DEG     (180.0f / 3.14159265358979f)
#define AERO_EKF_DEG_TO_RAD     (3.14159265358979f / 180.0f)
#define AERO_EKF_RE             6378137.0f    
#define NIS_THRESH_BARO         6.0f
#define NIS_THRESH_GPS          18.0f
static inline float fsq(float x) { return x * x; }
static inline float fclamp(float x, float lo, float hi)
    { return x < lo ? lo : (x > hi ? hi : x); }
static void quat_normalize(float q[4])
{
    float n = sqrtf(fsq(q[0]) + fsq(q[1]) + fsq(q[2]) + fsq(q[3]));
    if (n > 1e-6f) { q[0]/=n; q[1]/=n; q[2]/=n; q[3]/=n; }
}
static void quat_integrate(float q[4], const float omega[3], float dt)
{
    float wx = omega[0], wy = omega[1], wz = omega[2];
    float dq0 = 0.5f*(-q[1]*wx - q[2]*wy - q[3]*wz);
    float dq1 = 0.5f*( q[0]*wx + q[2]*wz - q[3]*wy);
    float dq2 = 0.5f*( q[0]*wy - q[1]*wz + q[3]*wx);
    float dq3 = 0.5f*( q[0]*wz + q[1]*wy - q[2]*wx);
    q[0] += dq0 * dt;  q[1] += dq1 * dt;
    q[2] += dq2 * dt;  q[3] += dq3 * dt;
    quat_normalize(q);
}
static void quat_to_rot(const float q[4], float R[3][3])
{
    float w=q[0], x=q[1], y=q[2], z=q[3];
    R[0][0] = 1-2*(fsq(y)+fsq(z)); R[0][1] = 2*(x*y-w*z); R[0][2] = 2*(x*z+w*y);
    R[1][0] = 2*(x*y+w*z); R[1][1] = 1-2*(fsq(x)+fsq(z)); R[1][2] = 2*(y*z-w*x);
    R[2][0] = 2*(x*z-w*y); R[2][1] = 2*(y*z+w*x); R[2][2] = 1-2*(fsq(x)+fsq(y));
}
#define MAT_ZERO(A) memset((A), 0, sizeof(float)*EKF_N*EKF_N)
#define VEC_ZERO(v) memset((v), 0, sizeof(float)*EKF_N)
static void mat_mul(const float A[EKF_N][EKF_N], const float B[EKF_N][EKF_N],
                    float C[EKF_N][EKF_N])
{
    float tmp[EKF_N][EKF_N];
    for (int i=0;i<EKF_N;i++)
        for (int j=0;j<EKF_N;j++) {
            float s=0; for(int k=0;k<EKF_N;k++) s+=A[i][k]*B[k][j]; tmp[i][j]=s;
        }
    memcpy(C, tmp, sizeof(float)*EKF_N*EKF_N);
}
static void mat_add(float A[EKF_N][EKF_N], const float B[EKF_N][EKF_N])
{
    for(int i=0;i<EKF_N;i++) for(int j=0;j<EKF_N;j++) A[i][j]+=B[i][j];
}
static void mat_transpose(const float A[EKF_N][EKF_N], float B[EKF_N][EKF_N])
{
    for(int i=0;i<EKF_N;i++) for(int j=0;j<EKF_N;j++) B[i][j]=A[j][i];
}
static void mat_symmetrize(float P[EKF_N][EKF_N])
{
    for(int i=0;i<EKF_N;i++)
        for(int j=i+1;j<EKF_N;j++) {
            float avg=(P[i][j]+P[j][i])*0.5f;
            P[i][j]=P[j][i]=avg;
        }
}
static void scalar_update(float x[EKF_N], float P[EKF_N][EKF_N],
                           const float H[EKF_N], float z, float R_noise,
                           float *innov_out, float *NIS_out)
{
    float PH[EKF_N] = {0};
    for(int i=0;i<EKF_N;i++) for(int j=0;j<EKF_N;j++) PH[i]+=P[i][j]*H[j];
    float S=R_noise;
    for(int j=0;j<EKF_N;j++) S+=H[j]*PH[j];
    float S_inv = (fabsf(S) > 1e-9f) ? 1.0f/S : 0.0f;
    float y=z;
    for(int j=0;j<EKF_N;j++) y-=H[j]*x[j];
    if(innov_out) *innov_out=y;
    if(NIS_out)   *NIS_out  = fsq(y)*S_inv;
    float K[EKF_N];
    for(int i=0;i<EKF_N;i++) K[i]=PH[i]*S_inv;
    for(int i=0;i<EKF_N;i++) x[i]+=K[i]*y;
    float KH[EKF_N][EKF_N];
    for(int i=0;i<EKF_N;i++) for(int j=0;j<EKF_N;j++) KH[i][j]=(i==j?1.0f:0.0f)-K[i]*H[j];
    float tmp[EKF_N][EKF_N];
    mat_mul(KH, P, tmp);
    memcpy(P, tmp, sizeof(float)*EKF_N*EKF_N);
    mat_symmetrize(P);
}
#define IDX_POS  0
#define IDX_VEL  3
#define IDX_QW   6
#define IDX_QX   7
#define IDX_QY   8
#define IDX_QZ   9
#define IDX_BA   10
#define IDX_BG   13
static void state_to_ekf(aero_ekf_t *e, float x[EKF_N])
{
    for(int i=0;i<3;i++) x[IDX_POS+i]=e->pos[i];
    for(int i=0;i<3;i++) x[IDX_VEL+i]=e->vel[i];
    x[IDX_QW]=e->quat[0]; x[IDX_QX]=e->quat[1];
    x[IDX_QY]=e->quat[2]; x[IDX_QZ]=e->quat[3];
    for(int i=0;i<3;i++) x[IDX_BA+i]=e->accel_bias[i];
    for(int i=0;i<3;i++) x[IDX_BG+i]=e->gyro_bias[i];
}
static void ekf_from_state(aero_ekf_t *e, const float x[EKF_N])
{
    for(int i=0;i<3;i++) e->pos[i]=x[IDX_POS+i];
    for(int i=0;i<3;i++) e->vel[i]=x[IDX_VEL+i];
    e->quat[0]=x[IDX_QW]; e->quat[1]=x[IDX_QX];
    e->quat[2]=x[IDX_QY]; e->quat[3]=x[IDX_QZ];
    for(int i=0;i<3;i++) e->accel_bias[i]=x[IDX_BA+i];
    for(int i=0;i<3;i++) e->gyro_bias[i]=x[IDX_BG+i];
}
void aero_ekf_init(aero_ekf_t *ekf, float dt)
{
    memset(ekf, 0, sizeof(aero_ekf_t));
    ekf->dt = dt;
    ekf->quat[0] = 1.0f; 
    for(int i=0;i<3;i++) ekf->P[IDX_POS+i][IDX_POS+i] = 1.0f;
    for(int i=0;i<3;i++) ekf->P[IDX_VEL+i][IDX_VEL+i] = 0.1f;
    ekf->P[IDX_QW][IDX_QW]=0.01f;
    for(int i=1;i<4;i++) ekf->P[IDX_QW+i][IDX_QW+i] = 0.001f;
    for(int i=0;i<3;i++) ekf->P[IDX_BA+i][IDX_BA+i] = 0.001f;
    for(int i=0;i<3;i++) ekf->P[IDX_BG+i][IDX_BG+i] = 0.0001f;
    float qa = AERO_EKF_Q_ACCEL;
    float qg = AERO_EKF_Q_GYRO;
    float qab= AERO_EKF_Q_ACCEL_BIAS;
    float qgb= AERO_EKF_Q_GYRO_BIAS;
    for(int i=0;i<3;i++) ekf->Q[IDX_VEL+i][IDX_VEL+i] = qa * dt;
    for(int i=0;i<4;i++) ekf->Q[IDX_QW+i][IDX_QW+i]   = qg * dt;
    for(int i=0;i<3;i++) ekf->Q[IDX_BA+i][IDX_BA+i]    = qab * dt;
    for(int i=0;i<3;i++) ekf->Q[IDX_BG+i][IDX_BG+i]    = qgb * dt;
    ekf->initialized = true;
    AERO_LOGI("EKF", "15-state EKF initialized, dt=%.4f s", dt);
}
void aero_ekf_set_initial_state(aero_ekf_t *ekf,
                                  float lat_deg, float lon_deg,
                                  float alt_m, float heading_deg)
{
    ekf->pos[0] = lat_deg * AERO_EKF_DEG_TO_RAD * AERO_EKF_RE;
    ekf->pos[1] = lon_deg * AERO_EKF_DEG_TO_RAD * AERO_EKF_RE;
    ekf->pos[2] = -alt_m;  
    float half_yaw = heading_deg * AERO_EKF_DEG_TO_RAD * 0.5f;
    ekf->quat[0] = cosf(half_yaw);
    ekf->quat[3] = sinf(half_yaw);
    quat_normalize(ekf->quat);
}
void aero_ekf_propagate(aero_ekf_t *ekf, const sal_imu_data_t *imu)
{
    if (!ekf->initialized || !imu) return;
    float dt = ekf->dt;
    float omega[3] = {
        imu->gx - ekf->gyro_bias[0],
        imu->gy - ekf->gyro_bias[1],
        imu->gz - ekf->gyro_bias[2],
    };
    float accel_body[3] = {
        imu->ax - ekf->accel_bias[0],
        imu->ay - ekf->accel_bias[1],
        imu->az - ekf->accel_bias[2],
    };
    quat_integrate(ekf->quat, omega, dt);
    float R[3][3];
    quat_to_rot(ekf->quat, R);
    float a_ned[3] = {0};
    for(int i=0;i<3;i++)
        for(int j=0;j<3;j++)
            a_ned[i] += R[i][j] * accel_body[j];
    a_ned[2] += AERO_EKF_GRAVITY;
    for(int i=0;i<3;i++) {
        ekf->pos[i] += ekf->vel[i] * dt + 0.5f * a_ned[i] * dt * dt;
        ekf->vel[i] += a_ned[i] * dt;
    }
    mat_add(ekf->P, ekf->Q);
    mat_symmetrize(ekf->P);
    ekf->update_count++;
    ekf->last_imu_tick = hal_get_tick_ms();
}
void aero_ekf_update_baro(aero_ekf_t *ekf, const sal_baro_data_t *baro)
{
    if (!ekf->initialized || !baro) return;
    float H[EKF_N];
    memset(H, 0, sizeof(H));
    H[IDX_POS+2] = -1.0f;
    float z_alt = baro->altitude_m;
    float x[EKF_N];
    state_to_ekf(ekf, x);
    scalar_update(x, ekf->P, H, z_alt,
                  AERO_EKF_R_BARO_ALT * AERO_EKF_R_BARO_ALT,
                  &ekf->innov_baro, &ekf->NIS_baro);
    ekf_from_state(ekf, x);
    quat_normalize(ekf->quat);
}
void aero_ekf_update_gnss(aero_ekf_t *ekf, const sal_gnss_data_t *gnss)
{
    if (!ekf->initialized || !gnss || !gnss->valid) return;
    float x[EKF_N];
    state_to_ekf(ekf, x);
    float gps_pos_n = gnss->latitude_deg  * AERO_EKF_DEG_TO_RAD * AERO_EKF_RE;
    float gps_pos_e = gnss->longitude_deg * AERO_EKF_DEG_TO_RAD * AERO_EKF_RE;
    float gps_pos_d = -gnss->altitude_m;
    float course_rad = gnss->course_deg * AERO_EKF_DEG_TO_RAD;
    float gps_vel_n = gnss->speed_mps * cosf(course_rad);
    float gps_vel_e = gnss->speed_mps * sinf(course_rad);
    float H_pos_n[EKF_N]={0}; H_pos_n[IDX_POS+0]=1.0f;
    float H_pos_e[EKF_N]={0}; H_pos_e[IDX_POS+1]=1.0f;
    float H_pos_d[EKF_N]={0}; H_pos_d[IDX_POS+2]=1.0f;
    float H_vel_n[EKF_N]={0}; H_vel_n[IDX_VEL+0]=1.0f;
    float H_vel_e[EKF_N]={0}; H_vel_e[IDX_VEL+1]=1.0f;
    float R_pos = fsq(AERO_EKF_R_GPS_POS);
    float R_vel = fsq(AERO_EKF_R_GPS_VEL);
    float nis = 0;
    scalar_update(x, ekf->P, H_pos_n, gps_pos_n, R_pos, &ekf->innov_gps_pos[0], NULL);
    scalar_update(x, ekf->P, H_pos_e, gps_pos_e, R_pos, &ekf->innov_gps_pos[1], NULL);
    scalar_update(x, ekf->P, H_pos_d, gps_pos_d, R_pos, &ekf->innov_gps_pos[2], NULL);
    scalar_update(x, ekf->P, H_vel_n, gps_vel_n, R_vel, &ekf->innov_gps_vel[0], NULL);
    scalar_update(x, ekf->P, H_vel_e, gps_vel_e, R_vel, &ekf->innov_gps_vel[1], NULL);
    for(int i=0;i<3;i++) nis += fsq(ekf->innov_gps_pos[i]) / R_pos;
    ekf->NIS_gps = nis;
    ekf_from_state(ekf, x);
    quat_normalize(ekf->quat);
}
void aero_ekf_update_mag(aero_ekf_t *ekf, const sal_mag_data_t *mag,
                          const float mag_ref_gauss[3])
{
    if (!ekf->initialized || !mag) return;
    float R[3][3];
    quat_to_rot(ekf->quat, R);
    float pred[3] = {0};
    for(int i=0;i<3;i++)
        for(int j=0;j<3;j++)
            pred[i] += R[j][i] * mag_ref_gauss[j];
    float x[EKF_N];
    state_to_ekf(ekf, x);
    {
        float H[EKF_N]={0};
        float mr=mag_ref_gauss[0], mn=mag_ref_gauss[1];
        H[IDX_QW]= 2*(ekf->quat[0]*mr - ekf->quat[3]*mn);
        H[IDX_QX]= 2*(ekf->quat[1]*mr + ekf->quat[2]*mn);
        H[IDX_QY]= 2*(ekf->quat[2]*mr - ekf->quat[1]*mn);
        H[IDX_QZ]= 2*(ekf->quat[3]*mr + ekf->quat[0]*mn);
        scalar_update(x, ekf->P, H,
                      mag->x, AERO_EKF_R_MAG * AERO_EKF_R_MAG,
                      &ekf->innov_mag[0], NULL);
    }
    ekf_from_state(ekf, x);
    quat_normalize(ekf->quat);
}
void aero_ekf_get_euler(const aero_ekf_t *ekf,
                          float *roll, float *pitch, float *yaw)
{
    float w=ekf->quat[0], x=ekf->quat[1], y=ekf->quat[2], z=ekf->quat[3];
    *roll  = atan2f(2*(w*x+y*z), 1-2*(fsq(x)+fsq(y))) * AERO_EKF_RAD_TO_DEG;
    *pitch = asinf(fclamp(2*(w*y-z*x), -1.0f, 1.0f))  * AERO_EKF_RAD_TO_DEG;
    *yaw   = atan2f(2*(w*z+x*y), 1-2*(fsq(y)+fsq(z))) * AERO_EKF_RAD_TO_DEG;
    if (*yaw < 0) *yaw += 360.0f;
}
void aero_ekf_get_rot_matrix(const aero_ekf_t *ekf, float R[3][3])
{
    quat_to_rot(ekf->quat, R);
}
void aero_ekf_get_pos_std(const aero_ekf_t *ekf, float sigma[3])
{
    for(int i=0;i<3;i++)
        sigma[i] = sqrtf(fabsf(ekf->P[IDX_POS+i][IDX_POS+i]));
}
bool aero_ekf_is_healthy(const aero_ekf_t *ekf)
{
    if (!ekf->initialized) return false;
    if (ekf->NIS_baro > NIS_THRESH_BARO) return false;
    if (ekf->NIS_gps  > NIS_THRESH_GPS)  return false;
    return true;
}
void aero_ekf_reset(aero_ekf_t *ekf)
{
    float dt = ekf->dt;
    aero_ekf_init(ekf, dt);
    AERO_LOGW("EKF", "EKF reset triggered");
}