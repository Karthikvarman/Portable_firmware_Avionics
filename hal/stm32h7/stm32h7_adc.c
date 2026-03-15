#include "hal_adc.h"
#include "hal_common.h"
#include "stm32h7xx_ll_adc.h"
#include "stm32h7xx_ll_bus.h"
#include "stm32h7xx_ll_gpio.h"
#include <math.h>
#define STM32H7_ADC_RESOLUTION   4096.0f   
#define STM32H7_ADC_TIMEOUT_MS   10
typedef struct {
    ADC_TypeDef *instance;
    bool         initialized;
    float        vref_mv;
} stm32h7_adc_ctx_t;
static stm32h7_adc_ctx_t s_adc_ctx[2] = {
    { .instance = ADC1 },
    { .instance = ADC3 },
};
hal_status_t hal_adc_init(const hal_adc_config_t *cfg)
{
    if (!cfg || cfg->unit >= 2) return HAL_PARAM_ERROR;
    stm32h7_adc_ctx_t *ctx = &s_adc_ctx[cfg->unit];
    ctx->vref_mv = cfg->vref_mv > 0 ? cfg->vref_mv : 3300.0f;
    if (cfg->unit == HAL_ADC_UNIT_1)
        LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_ADC12);
    else
        LL_AHB4_GRP1_EnableClock(LL_AHB4_GRP1_PERIPH_ADC3);
    LL_ADC_InitTypeDef adc_init = {
        .Resolution         = LL_ADC_RESOLUTION_12B,
        .LowPowerMode       = LL_ADC_LP_MODE_NONE,
    };
    LL_ADC_Init(ctx->instance, &adc_init);
    LL_ADC_REG_InitTypeDef reg_init = {
        .TriggerSource      = LL_ADC_REG_TRIG_SOFTWARE,
        .SequencerLength    = LL_ADC_REG_SEQ_SCAN_DISABLE,
        .ContinuousMode     = LL_ADC_REG_CONV_SINGLE,
        .DataTransferMode   = LL_ADC_REG_DR_TRANSFER,
        .Overrun            = LL_ADC_REG_OVR_DATA_OVERWRITTEN,
    };
    LL_ADC_REG_Init(ctx->instance, &reg_init);
    LL_ADC_EnableInternalRegulator(ctx->instance);
    hal_delay_ms(1); 
    LL_ADC_StartCalibration(ctx->instance, LL_ADC_CALIB_OFFSET, LL_ADC_SINGLE_ENDED);
    hal_tick_t start = hal_get_tick_ms();
    while (LL_ADC_IsCalibrationOnGoing(ctx->instance)) {
        if (hal_elapsed_ms(start) > 100) return HAL_TIMEOUT;
    }
    LL_ADC_Enable(ctx->instance);
    ctx->initialized = true;
    return HAL_OK;
}
hal_status_t hal_adc_read_raw(hal_adc_unit_t unit, uint8_t ch, uint16_t *raw)
{
    if (unit >= 2 || !raw) return HAL_PARAM_ERROR;
    stm32h7_adc_ctx_t *ctx = &s_adc_ctx[unit];
    if (!ctx->initialized) return HAL_NOT_INIT;
    LL_ADC_REG_SetSequencerRanks(ctx->instance, LL_ADC_REG_RANK_1,
        __LL_ADC_DECIMAL_NB_TO_CHANNEL(ch));
    LL_ADC_SetChannelSamplingTime(ctx->instance,
        __LL_ADC_DECIMAL_NB_TO_CHANNEL(ch), LL_ADC_SAMPLINGTIME_810CYCLES_5);
    LL_ADC_REG_StartConversion(ctx->instance);
    hal_tick_t start = hal_get_tick_ms();
    while (!LL_ADC_IsActiveFlag_EOC(ctx->instance)) {
        if (hal_elapsed_ms(start) > STM32H7_ADC_TIMEOUT_MS) return HAL_TIMEOUT;
    }
    *raw = (uint16_t)LL_ADC_REG_ReadConversionData12(ctx->instance);
    LL_ADC_ClearFlag_EOC(ctx->instance);
    return HAL_OK;
}
hal_status_t hal_adc_read_voltage(hal_adc_unit_t unit, uint8_t ch, float *mv)
{
    uint16_t raw = 0;
    hal_status_t rc = hal_adc_read_raw(unit, ch, &raw);
    if (rc != HAL_OK) return rc;
    *mv = (float)raw / STM32H7_ADC_RESOLUTION * s_adc_ctx[unit].vref_mv;
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
    if (unit >= 2) return HAL_PARAM_ERROR;
    LL_ADC_Disable(s_adc_ctx[unit].instance);
    s_adc_ctx[unit].initialized = false;
    return HAL_OK;
}