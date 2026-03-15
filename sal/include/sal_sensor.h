#ifndef SAL_SENSOR_H
#define SAL_SENSOR_H
#include "hal_common.h"
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    SAL_SENSOR_BAROMETER    = 0,   
    SAL_SENSOR_TEMPERATURE  = 1,   
    SAL_SENSOR_HUMIDITY     = 2,   
    SAL_SENSOR_ACCEL        = 3,   
    SAL_SENSOR_GYRO         = 4,   
    SAL_SENSOR_MAGNETOMETER = 5,   
    SAL_SENSOR_GNSS         = 6,   
    SAL_SENSOR_CO_GAS       = 7,   
    SAL_SENSOR_COUNT        = 8
} sal_sensor_id_t;
typedef struct {
    float pressure_hpa;    
    float altitude_m;      
    float temperature_c;   
} sal_baro_data_t;
typedef struct {
    float temperature_c;   
    float humidity_pct;    
} sal_env_data_t;
typedef struct {
    float ax, ay, az;      
    float gx, gy, gz;      
    float temperature_c;   
} sal_imu_data_t;
typedef struct {
    float x, y, z;         
    float heading_deg;     
} sal_mag_data_t;
typedef struct {
    double latitude_deg;
    double longitude_deg;
    float  altitude_m;
    float  speed_mps;      
    float  course_deg;     
    float  hdop;           
    float  vdop;           
    uint8_t num_satellites;
    uint8_t fix_type;      
    bool    valid;
} sal_gnss_data_t;
typedef struct {
    float ppm;             
    float mv;              
    bool  warmed_up;       
} sal_co_data_t;
typedef union {
    sal_baro_data_t  baro;
    sal_env_data_t   env;
    sal_imu_data_t   imu;    
    sal_mag_data_t   mag;
    sal_gnss_data_t  gnss;
    sal_co_data_t    co;
} sal_data_t;
typedef struct sal_driver {
    const char       *name;           
    sal_sensor_id_t   id;
    hal_status_t (*init)     (struct sal_driver *drv);
    hal_status_t (*deinit)   (struct sal_driver *drv);
    hal_status_t (*self_test)(struct sal_driver *drv);
    hal_status_t (*read)     (struct sal_driver *drv, sal_data_t *out);
    hal_status_t (*set_odr)  (struct sal_driver *drv, uint32_t hz);
    hal_status_t (*sleep)    (struct sal_driver *drv);
    hal_status_t (*wakeup)   (struct sal_driver *drv);
    void        *priv;           
    bool         initialized;
    bool         healthy;
    uint32_t     error_count;
} sal_driver_t;
hal_status_t sal_register(sal_driver_t *drv);
hal_status_t sal_init_all(void);
hal_status_t sal_read(sal_sensor_id_t id, sal_data_t *out);
sal_driver_t *sal_get_driver(sal_sensor_id_t id);
bool sal_sensor_is_healthy(sal_sensor_id_t id);
void sal_run_self_tests(void);
void sal_reinit_unhealthy(void);
void sal_print_summary(void);
#ifdef __cplusplus
}
#endif
#endif 