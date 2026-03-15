#include "tgs5141.h"
#include "sal_sensor.h"
#include "hal_adc.h"
#include "hal_timer.h"
#include "aero_config.h"
#include <string.h>
#define TGS5141_DEFAULT_VOUT_ZERO_MV    300.0f   
#define TGS5141_DEFAULT_SENSITIVITY     3.0f      
#define TGS5141_ADC_OVERSAMPLING        16        
#define TGS5141_MAX_PPM                 5000.0f   
#define TGS5141_WARMUP_MS               30000     
typedef struct {
    hal_adc_unit_t  adc_unit;
    uint8_t         adc_channel;
    float           vref_mv;
    float           v_zero_mv;       
    float           sensitivity_mvppm; 
    bool            warmed_up;
    hal_tick_t      start_tick;
} tgs5141_priv_t;
static tgs5141_priv_t s_tgs_priv;
static hal_status_t tgs5141_init(sal_driver_t *drv)
{
    tgs5141_priv_t *p = (tgs5141_priv_t *)drv->priv;
    hal_adc_config_t adc_cfg = {
        .unit           = p->adc_unit,
        .channel        = p->adc_channel,
        .resolution     = HAL_ADC_RES_12BIT,
        .sample_time_ns = 1000,
        .vref_mv        = p->vref_mv,
    };
    hal_status_t rc = hal_adc_init(&adc_cfg);
    if (rc != HAL_OK) {
        AERO_LOGE("TGS5141", "ADC init failed (rc=%d)", rc);
        return rc;
    }
    p->start_tick   = hal_get_tick_ms();
    p->warmed_up    = false;
    AERO_LOGI("TGS5141", "Init OK — ADC unit=%d ch=%d. Warmup: %d s",
              p->adc_unit, p->adc_channel, TGS5141_WARMUP_MS / 1000);
    return HAL_OK;
}
static hal_status_t tgs5141_read(sal_driver_t *drv, sal_data_t *out)
{
    tgs5141_priv_t *p = (tgs5141_priv_t *)drv->priv;
    if (!p->warmed_up) {
        if (hal_elapsed_ms(p->start_tick) >= TGS5141_WARMUP_MS) {
            p->warmed_up = true;
            AERO_LOGI("TGS5141", "Warmup complete — readings valid");
        } else {
            out->co.ppm       = 0.0f;
            out->co.mv        = 0.0f;
            out->co.warmed_up = false;
            return HAL_OK;
        }
    }
    out->co.warmed_up = true;
    float adc_mv = 0.0f;
    hal_status_t rc = hal_adc_read_avg(p->adc_unit, p->adc_channel,
                                        TGS5141_ADC_OVERSAMPLING, &adc_mv);
    if (rc != HAL_OK) return rc;
    float delta_mv = adc_mv - p->v_zero_mv;
    float ppm = delta_mv / p->sensitivity_mvppm;
    if (ppm < 0.0f)                  ppm = 0.0f;
    if (ppm > TGS5141_MAX_PPM)       ppm = TGS5141_MAX_PPM;
    out->co.ppm = ppm;
    out->co.mv  = adc_mv;
    return HAL_OK;
}
static sal_driver_t s_tgs_driver = {
    .name  = "TGS-5141",
    .id    = SAL_SENSOR_CO_GAS,
    .init  = tgs5141_init,
    .read  = tgs5141_read,
    .priv  = &s_tgs_priv,
};
hal_status_t tgs5141_driver_register(hal_adc_unit_t unit, uint8_t channel,
                                      float vref_mv)
{
    s_tgs_priv.adc_unit          = unit;
    s_tgs_priv.adc_channel       = channel;
    s_tgs_priv.vref_mv           = vref_mv;
    s_tgs_priv.v_zero_mv         = TGS5141_DEFAULT_VOUT_ZERO_MV;
    s_tgs_priv.sensitivity_mvppm = TGS5141_DEFAULT_SENSITIVITY;
    return sal_register(&s_tgs_driver);
}
void tgs5141_calibrate(float v_zero_mv, float sensitivity_mvppm)
{
    s_tgs_priv.v_zero_mv         = v_zero_mv;
    s_tgs_priv.sensitivity_mvppm = sensitivity_mvppm;
    AERO_LOGI("TGS5141", "Calibrated: v0=%.2f mV, sens=%.4f mV/ppm",
              v_zero_mv, sensitivity_mvppm);
}