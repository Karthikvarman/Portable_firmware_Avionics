#include "hal_flash.h"
#include "hal_common.h"
#include "stm32h7xx.h"
#include <string.h>
#define FLASH_SECTOR_SIZE       (128 * 1024UL)   
#define FLASH_BANK1_BASE        0x08000000UL
#define FLASH_BANK2_BASE        0x08100000UL
#define FLASH_PROGRAM_WORD_SIZE 32               
#define FLASH_KEY1              0x45670123UL
#define FLASH_KEY2              0xCDEF89ABUL
#define FLASH_TIMEOUT_MS        10000
static bool s_unlocked = false;
static void flash_unlock(void)
{
    if (!s_unlocked) {
        FLASH->KEYR1 = FLASH_KEY1;
        FLASH->KEYR1 = FLASH_KEY2;
        s_unlocked = true;
    }
}
static void flash_lock(void)
{
    FLASH->CR1 |= FLASH_CR1_LOCK;
    s_unlocked = false;
}
static hal_status_t flash_wait_busy(uint8_t bank, uint32_t timeout_ms)
{
    hal_tick_t start = hal_get_tick_ms();
    volatile uint32_t *sr = (bank == 1) ? &FLASH->SR1 : &FLASH->SR2;
    while (*sr & FLASH_SR1_BSY) {
        if (hal_elapsed_ms(start) > timeout_ms) return HAL_TIMEOUT;
    }
    if (*sr & (FLASH_SR1_WRPERR | FLASH_SR1_PGSERR | FLASH_SR1_OPERR))
        return HAL_IO_ERROR;
    return HAL_OK;
}
hal_status_t hal_flash_erase_sector(uint32_t sector_addr)
{
    flash_unlock();
    uint8_t  bank;
    uint32_t sector_idx;
    if (sector_addr >= FLASH_BANK2_BASE) {
        bank = 2;
        sector_idx = (sector_addr - FLASH_BANK2_BASE) / FLASH_SECTOR_SIZE;
    } else {
        bank = 1;
        sector_idx = (sector_addr - FLASH_BANK1_BASE) / FLASH_SECTOR_SIZE;
    }
    hal_status_t rc = flash_wait_busy(bank, FLASH_TIMEOUT_MS);
    if (rc != HAL_OK) { flash_lock(); return rc; }
    if (bank == 1) {
        FLASH->CR1 &= ~FLASH_CR1_SNB;
        FLASH->CR1 |= (sector_idx << FLASH_CR1_SNB_Pos) | FLASH_CR1_SER | FLASH_CR1_PSIZE_1;
        FLASH->CR1 |= FLASH_CR1_START;
    } else {
        FLASH->CR2 &= ~FLASH_CR2_SNB;
        FLASH->CR2 |= (sector_idx << FLASH_CR2_SNB_Pos) | FLASH_CR2_SER | FLASH_CR2_PSIZE_1;
        FLASH->CR2 |= FLASH_CR2_START;
    }
    rc = flash_wait_busy(bank, FLASH_TIMEOUT_MS);
    if (bank == 1) FLASH->CR1 &= ~(FLASH_CR1_SER | FLASH_CR1_PSIZE);
    else           FLASH->CR2 &= ~(FLASH_CR2_SER | FLASH_CR2_PSIZE);
    flash_lock();
    return rc;
}
hal_status_t hal_flash_write(uint32_t addr, const uint8_t *data, uint32_t len)
{
    if (!data || (addr % FLASH_PROGRAM_WORD_SIZE) != 0) return HAL_PARAM_ERROR;
    flash_unlock();
    uint8_t bank = (addr >= FLASH_BANK2_BASE) ? 2 : 1;
    if (bank == 1) {
        FLASH->CR1 &= ~FLASH_CR1_PSIZE;
        FLASH->CR1 |=  FLASH_CR1_PG | FLASH_CR1_PSIZE_1;
    } else {
        FLASH->CR2 &= ~FLASH_CR2_PSIZE;
        FLASH->CR2 |=  FLASH_CR2_PG | FLASH_CR2_PSIZE_1;
    }
    uint32_t offset = 0;
    static uint8_t s_pad[32];
    while (offset < len) {
        hal_status_t rc = flash_wait_busy(bank, FLASH_TIMEOUT_MS);
        if (rc != HAL_OK) { flash_lock(); return rc; }
        uint32_t chunk = (len - offset >= 32) ? 32 : (len - offset);
        memcpy(s_pad, data + offset, chunk);
        if (chunk < 32) memset(s_pad + chunk, 0xFF, 32 - chunk);
        volatile uint32_t *dest = (volatile uint32_t *)(addr + offset);
        const uint32_t    *src  = (const uint32_t *)s_pad;
        for (int i = 0; i < 8; i++) dest[i] = src[i];
        __DSB();  
        offset += 32;
    }
    if (bank == 1) FLASH->CR1 &= ~FLASH_CR1_PG;
    else           FLASH->CR2 &= ~FLASH_CR2_PG;
    flash_lock();
    return HAL_OK;
}
hal_status_t hal_flash_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    if (!buf) return HAL_PARAM_ERROR;
    memcpy(buf, (const void *)addr, len);
    return HAL_OK;
}
hal_status_t hal_flash_verify(uint32_t addr, const uint8_t *expected, uint32_t len)
{
    if (!expected) return HAL_PARAM_ERROR;
    return (memcmp((const void *)addr, expected, len) == 0) ? HAL_OK : HAL_IO_ERROR;
}
uint32_t hal_flash_sector_size(uint32_t addr)
{
    (void)addr;
    return FLASH_SECTOR_SIZE;
}
uint32_t hal_flash_get_base(void)
{
    return FLASH_BANK1_BASE;
}