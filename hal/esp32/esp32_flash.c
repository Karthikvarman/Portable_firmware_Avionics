#include "hal_flash.h"
#include "esp_partition.h"
#include "esp_log.h"
#include <string.h>
static const char *TAG = "HAL_FLASH_ESP32";
static const esp_partition_t* find_partition(void) {
    return esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "storage");
}
hal_status_t hal_flash_erase_sector(uint32_t sector_addr) {
    const esp_partition_t* part = find_partition();
    if (!part) return HAL_ERROR;
    esp_err_t err = esp_partition_erase_range(part, sector_addr, 4096);
    return (err == ESP_OK) ? HAL_OK : HAL_ERROR;
}
hal_status_t hal_flash_write(uint32_t addr, const uint8_t *data, uint32_t len) {
    const esp_partition_t* part = find_partition();
    if (!part) return HAL_ERROR;
    esp_err_t err = esp_partition_write(part, addr, data, len);
    return (err == ESP_OK) ? HAL_OK : HAL_ERROR;
}
hal_status_t hal_flash_read(uint32_t addr, uint8_t *buf, uint32_t len) {
    const esp_partition_t* part = find_partition();
    if (!part) return HAL_ERROR;
    esp_err_t err = esp_partition_read(part, addr, buf, len);
    return (err == ESP_OK) ? HAL_OK : HAL_ERROR;
}
hal_status_t hal_flash_verify(uint32_t addr, const uint8_t *expected, uint32_t len) {
    uint8_t buf[256];
    uint32_t remaining = len;
    uint32_t current_addr = addr;
    while (remaining > 0) {
        uint32_t chunk = (remaining > sizeof(buf)) ? sizeof(buf) : remaining;
        if (hal_flash_read(current_addr, buf, chunk) != HAL_OK) return HAL_ERROR;
        if (memcmp(buf, expected + (current_addr - addr), chunk) != 0) return HAL_ERROR;
        remaining -= chunk;
        current_addr += chunk;
    }
    return HAL_OK;
}
uint32_t hal_flash_sector_size(uint32_t addr) {
    return 4096; 
}
uint32_t hal_flash_get_base(void) {
    return 0x0; 
}