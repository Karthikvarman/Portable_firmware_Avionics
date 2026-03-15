#ifndef STM32H7XX_LL_PWR_H
#define STM32H7XX_LL_PWR_H
#include <stdint.h>
#define LL_PWR_REGU_VOLTAGE_SCALE0 0
#define LL_PWR_REGU_VOLTAGE_SCALE1 1
void LL_PWR_SetRegulVoltageScaling(uint32_t PowerVoltageScaling);
uint32_t LL_PWR_IsActiveFlag_VOS(void);
void LL_PWR_EnableOverDriveMode(void);
uint32_t LL_PWR_IsActiveFlag_OD(void);
void LL_PWR_EnableOverDriveSwitching(void);
uint32_t LL_PWR_IsActiveFlag_ODSW(void);
#endif