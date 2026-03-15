#ifndef CMSIS_COMPILER_H
#define CMSIS_COMPILER_H
#include <stdint.h>
#define __STATIC_INLINE static inline
#define __WEAK __attribute__((weak))
#define __ALIGN_BEGIN
#define __ALIGN_END __attribute__((aligned(4)))
void __disable_irq(void);
void __enable_irq(void);
void __WFI(void);
void __WFE(void);
void NVIC_SystemReset(void);
#define MPU_CTRL_PRIVDEFENA_Msk (1UL << 2)
#define MPU_RBAR_VALID_Msk (1UL << 4)
#define MPU_RBAR_REGION_Pos 0
#define MPU_RASR_ENABLE_Pos 0
#define MPU_RASR_SIZE_Pos 1
#define MPU_RASR_AP_Pos 24
#define MPU_RASR_TEX_Pos 19
#define MPU_RASR_C_Pos 18
#define MPU_RASR_B_Pos 17
typedef struct {
  uint32_t RBAR;
  uint32_t RASR;
  uint32_t CTRL;
} MPU_Type;
typedef struct {
  uint32_t CPACR;
} SCB_Type;
#define MPU ((MPU_Type *)0xE000ED90)
#define SCB ((SCB_Type *)0xE000ED00)
void SCB_EnableICache(void);
void SCB_EnableDCache(void);
void ARM_MPU_Disable(void);
void ARM_MPU_Enable(uint32_t MPU_Control);
void SysTick_Config(uint32_t ticks);
extern uint32_t SystemCoreClock;
#endif