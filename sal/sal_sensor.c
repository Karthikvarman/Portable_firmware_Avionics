#include "sal_sensor.h"
#include "hal_timer.h"
#include "aero_config.h"
#include <string.h>
#include <stdio.h>
static sal_driver_t *s_drivers[SAL_SENSOR_COUNT];
static bool          s_sal_ready = false;
hal_status_t sal_init(void)
{
    memset(s_drivers, 0, sizeof(s_drivers));
    s_sal_ready = true;
    AERO_LOGI("SAL", "Sensor Abstraction Layer initialized");
    return HAL_OK;
}
hal_status_t sal_register_driver(sal_driver_t *drv)
{
    if (!s_sal_ready || !drv) return HAL_PARAM_ERROR;
    if (drv->id >= SAL_SENSOR_COUNT) return HAL_PARAM_ERROR;
    s_drivers[drv->id] = drv;
    AERO_LOGI("SAL", "Driver '%s' registered for sensor %d", drv->name, drv->id);
    return HAL_OK;
}
hal_status_t sal_sensor_init(sal_sensor_id_t id)
{
    if (id >= SAL_SENSOR_COUNT || !s_drivers[id]) return HAL_NOT_FOUND;
    sal_driver_t *drv = s_drivers[id];
    if (!drv->init) return HAL_UNSUPPORTED;
    hal_status_t rc = drv->init(drv);
    if (rc == HAL_OK) {
        drv->initialized = true;
        drv->healthy     = true;
        AERO_LOGI("SAL", "Sensor '%s' initialized OK", drv->name);
    } else {
        drv->initialized = false;
        drv->healthy     = false;
        drv->error_count++;
        AERO_LOGE("SAL", "Sensor '%s' init FAILED (rc=%d)", drv->name, rc);
    }
    return rc;
}
hal_status_t sal_sensor_self_test(sal_sensor_id_t id)
{
    if (id >= SAL_SENSOR_COUNT || !s_drivers[id]) return HAL_NOT_FOUND;
    sal_driver_t *drv = s_drivers[id];
    if (!drv->initialized) return HAL_NOT_INIT;
    if (!drv->self_test)   return HAL_OK; 
    hal_status_t rc = drv->self_test(drv);
    drv->healthy = (rc == HAL_OK);
    if (rc != HAL_OK) {
        drv->error_count++;
        AERO_LOGE("SAL", "Self-test FAILED for '%s' (rc=%d)", drv->name, rc);
    } else {
        AERO_LOGI("SAL", "Self-test PASS for '%s'", drv->name);
    }
    return rc;
}
hal_status_t sal_sensor_read(sal_sensor_id_t id, sal_data_t *data)
{
    if (id >= SAL_SENSOR_COUNT || !s_drivers[id]) return HAL_NOT_FOUND;
    sal_driver_t *drv = s_drivers[id];
    if (!drv->initialized || !drv->read) return HAL_NOT_INIT;
    hal_status_t rc = drv->read(drv, data);
    if (rc != HAL_OK) {
        drv->error_count++;
        drv->healthy = (drv->error_count < 5);  
    } else {
        drv->error_count = 0;
        drv->healthy = true;
    }
    return rc;
}
hal_status_t sal_sensor_set_odr(sal_sensor_id_t id, uint32_t hz)
{
    if (id >= SAL_SENSOR_COUNT || !s_drivers[id]) return HAL_NOT_FOUND;
    sal_driver_t *drv = s_drivers[id];
    if (!drv->set_odr) return HAL_UNSUPPORTED;
    return drv->set_odr(drv, hz);
}
hal_status_t sal_sensor_sleep(sal_sensor_id_t id)
{
    if (id >= SAL_SENSOR_COUNT || !s_drivers[id]) return HAL_NOT_FOUND;
    sal_driver_t *drv = s_drivers[id];
    if (!drv->sleep) return HAL_UNSUPPORTED;
    return drv->sleep(drv);
}
hal_status_t sal_sensor_wakeup(sal_sensor_id_t id)
{
    if (id >= SAL_SENSOR_COUNT || !s_drivers[id]) return HAL_NOT_FOUND;
    sal_driver_t *drv = s_drivers[id];
    if (!drv->wakeup) return HAL_UNSUPPORTED;
    return drv->wakeup(drv);
}
bool sal_sensor_is_healthy(sal_sensor_id_t id)
{
    if (id >= SAL_SENSOR_COUNT || !s_drivers[id]) return false;
    return s_drivers[id]->healthy;
}
sal_driver_t *sal_get_driver(sal_sensor_id_t id)
{
    if (id >= SAL_SENSOR_COUNT) return NULL;
    return s_drivers[id];
}
hal_status_t sal_init_all_sensors(void)
{
    hal_status_t overall = HAL_OK;
    for (int i = 0; i < SAL_SENSOR_COUNT; i++) {
        if (!s_drivers[i]) continue;
        hal_status_t rc = sal_sensor_init((sal_sensor_id_t)i);
        if (rc != HAL_OK) overall = rc;
        else sal_sensor_self_test((sal_sensor_id_t)i);
    }
    sal_print_status();
    return overall;
}
void sal_print_status(void)
{
    AERO_LOGI("SAL", "=== Sensor Status ===");
    for (int i = 0; i < SAL_SENSOR_COUNT; i++) {
        if (!s_drivers[i]) continue;
        sal_driver_t *drv = s_drivers[i];
        AERO_LOGI("SAL", "  [%d] %-16s  init=%d  healthy=%d  errors=%lu",
                  i, drv->name, drv->initialized, drv->healthy, drv->error_count);
    }
    AERO_LOGI("SAL", "=====================");
}
hal_status_t sal_register(sal_driver_t *drv)
{
    if (!drv) return HAL_PARAM_ERROR;
    if (!s_sal_ready) sal_init();
    return sal_register_driver(drv);
}
hal_status_t sal_init_all(void)
{
    return sal_init_all_sensors();
}
hal_status_t sal_read(sal_sensor_id_t id, sal_data_t *out)
{
    return sal_sensor_read(id, out);
}
void sal_run_self_tests(void)
{
    for (int i = 0; i < SAL_SENSOR_COUNT; i++)
        sal_sensor_self_test((sal_sensor_id_t)i);
}
void sal_reinit_unhealthy(void)
{
    for (int i = 0; i < SAL_SENSOR_COUNT; i++) {
        sal_driver_t *d = s_drivers[i];
        if (!d || d->healthy) continue;
        AERO_LOGW("SAL", "Re-initializing unhealthy sensor: %s", d->name);
        if (d->deinit) d->deinit(d);
        hal_delay_ms(10);
        hal_status_t rc = d->init ? d->init(d) : HAL_UNSUPPORTED;
        if (rc == HAL_OK) {
            d->initialized = true;
            d->healthy     = true;
            d->error_count = 0;
            AERO_LOGI("SAL", "  %s recovered OK", d->name);
        }
    }
}
void sal_print_summary(void)
{
    sal_print_status();
}