#include "hal_adc.h"
#include "hal_common.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include <math.h>
#define ESP32_ADC_MAX_UNITS     2
#define ESP32_ADC_SAMPLES       16   
typedef struct {
    adc_oneshot_unit_handle_t handle;
    adc_cali_handle_t         cali;
    bool                      initialized;
    float                     vref_mv;
} esp32_adc_ctx_t;
static esp32_adc_ctx_t s_adc_ctx[ESP32_ADC_MAX_UNITS];
hal_status_t hal_adc_init(const hal_adc_config_t *cfg)
{
    if (!cfg || cfg->unit >= ESP32_ADC_MAX_UNITS) return HAL_PARAM_ERROR;
    esp32_adc_ctx_t *ctx = &s_adc_ctx[cfg->unit];
    const adc_unit_t units[] = { ADC_UNIT_1, ADC_UNIT_2 };
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = units[cfg->unit],
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    if (adc_oneshot_new_unit(&init_cfg, &ctx->handle) != ESP_OK)
        return HAL_IO_ERROR;
    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten    = ADC_ATTEN_DB_11,   
    };
    if (adc_oneshot_config_channel(ctx->handle,
            (adc_channel_t)cfg->channel, &chan_cfg) != ESP_OK)
        return HAL_IO_ERROR;
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id  = units[cfg->unit],
        .chan     = (adc_channel_t)cfg->channel,
        .atten    = ADC_ATTEN_DB_11,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    esp_err_t err = adc_cali_create_scheme_curve_fitting(&cali_cfg, &ctx->cali);
    if (err != ESP_OK) ctx->cali = NULL;  
    ctx->vref_mv     = cfg->vref_mv > 0 ? cfg->vref_mv : 3300.0f;
    ctx->initialized = true;
    return HAL_OK;
}
hal_status_t hal_adc_read_raw(hal_adc_unit_t unit, uint8_t ch, uint16_t *raw)
{
    if (unit >= ESP32_ADC_MAX_UNITS || !raw) return HAL_PARAM_ERROR;
    esp32_adc_ctx_t *ctx = &s_adc_ctx[unit];
    if (!ctx->initialized) return HAL_NOT_INIT;
    int val = 0;
    if (adc_oneshot_read(ctx->handle, (adc_channel_t)ch, &val) != ESP_OK)
        return HAL_IO_ERROR;
    *raw = (uint16_t)val;
    return HAL_OK;
}
hal_status_t hal_adc_read_voltage(hal_adc_unit_t unit, uint8_t ch, float *mv)
{
    if (unit >= ESP32_ADC_MAX_UNITS || !mv) return HAL_PARAM_ERROR;
    esp32_adc_ctx_t *ctx = &s_adc_ctx[unit];
    if (!ctx->initialized) return HAL_NOT_INIT;
    int raw = 0;
    if (adc_oneshot_read(ctx->handle, (adc_channel_t)ch, &raw) != ESP_OK)
        return HAL_IO_ERROR;
    if (ctx->cali) {
        int cali_mv = 0;
        adc_cali_raw_to_voltage(ctx->cali, raw, &cali_mv);
        *mv = (float)cali_mv;
    } else {
        *mv = (float)raw / 4095.0f * ctx->vref_mv;
    }
    return HAL_OK;
}
hal_status_t hal_adc_read_avg(hal_adc_unit_t unit, uint8_t ch,
                               uint32_t samples, float *mv_avg)
{
    float sum = 0;
    for (uint32_t i = 0; i < samples; i++) {
        float mv = 0;
        hal_status_t rc = hal_adc_read_voltage(unit, ch, &mv);
        if (rc != HAL_OK) return rc;
        sum += mv;
    }
    *mv_avg = sum / (float)samples;
    return HAL_OK;
}
hal_status_t hal_adc_deinit(hal_adc_unit_t unit)
{
    if (unit >= ESP32_ADC_MAX_UNITS) return HAL_PARAM_ERROR;
    esp32_adc_ctx_t *ctx = &s_adc_ctx[unit];
    if (ctx->cali)   { adc_cali_delete_scheme_curve_fitting(ctx->cali); ctx->cali = NULL; }
    if (ctx->handle) { adc_oneshot_del_unit(ctx->handle); ctx->handle = NULL; }
    ctx->initialized = false;
    return HAL_OK;
}