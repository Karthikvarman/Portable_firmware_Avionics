#ifndef HAL_ADC_H
#define HAL_ADC_H
#include "hal_common.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    HAL_ADC_UNIT_1 = 0,
    HAL_ADC_UNIT_2 = 1,
    HAL_ADC_UNIT_3 = 2,
} hal_adc_unit_t;
typedef enum {
    HAL_ADC_RES_8BIT  = 8,
    HAL_ADC_RES_10BIT = 10,
    HAL_ADC_RES_12BIT = 12,
    HAL_ADC_RES_14BIT = 14,
    HAL_ADC_RES_16BIT = 16,
} hal_adc_resolution_t;
typedef struct {
    hal_adc_unit_t        unit;
    uint8_t               channel;
    hal_adc_resolution_t  resolution;
    uint32_t              sample_time_ns;  
    float                 vref_mv;         
} hal_adc_config_t;
hal_status_t hal_adc_init(const hal_adc_config_t *cfg);
hal_status_t hal_adc_read_raw(hal_adc_unit_t unit, uint8_t channel, uint16_t *raw);
hal_status_t hal_adc_read_voltage(hal_adc_unit_t unit, uint8_t channel, float *mv);
hal_status_t hal_adc_read_avg(hal_adc_unit_t unit, uint8_t channel,
                               uint32_t samples, float *mv_avg);
hal_status_t hal_adc_deinit(hal_adc_unit_t unit);
#ifdef __cplusplus
}
#endif
#endif 