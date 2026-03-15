#ifndef TGS5141_H
#define TGS5141_H
#include "hal_common.h"
#include "hal_adc.h"
#ifdef __cplusplus
extern "C" {
#endif
hal_status_t tgs5141_driver_register(hal_adc_unit_t unit, uint8_t channel,
                                      float vref_mv);
void tgs5141_calibrate(float v_zero_mv, float sensitivity_mvppm);
#ifdef __cplusplus
}
#endif
#endif 