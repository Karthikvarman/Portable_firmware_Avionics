#ifndef HAL_FLASH_H
#define HAL_FLASH_H
#include "hal_common.h"
#ifdef __cplusplus
extern "C" {
#endif
hal_status_t hal_flash_erase_sector(uint32_t sector_addr);
hal_status_t hal_flash_write       (uint32_t addr, const uint8_t *data, uint32_t len);
hal_status_t hal_flash_read        (uint32_t addr, uint8_t *buf, uint32_t len);
hal_status_t hal_flash_verify      (uint32_t addr, const uint8_t *expected, uint32_t len);
uint32_t     hal_flash_sector_size (uint32_t addr);
uint32_t     hal_flash_get_base    (void);
#ifdef __cplusplus
}
#endif
#endif 