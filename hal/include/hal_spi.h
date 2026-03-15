#ifndef HAL_SPI_H
#define HAL_SPI_H
#include "hal_common.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef uint8_t hal_spi_bus_t;
typedef enum {
    HAL_SPI_MODE_0 = 0,   
    HAL_SPI_MODE_1 = 1,   
    HAL_SPI_MODE_2 = 2,   
    HAL_SPI_MODE_3 = 3,   
} hal_spi_mode_t;
typedef enum {
    HAL_SPI_BITORDER_MSB = 0,
    HAL_SPI_BITORDER_LSB = 1,
} hal_spi_bitorder_t;
typedef struct {
    hal_spi_bus_t      bus;
    uint32_t           clock_hz;       
    hal_spi_mode_t     mode;
    hal_spi_bitorder_t bit_order;
    int8_t             cs_gpio_pin;    
    uint8_t            cs_gpio_port;   
} hal_spi_config_t;
hal_status_t hal_spi_init      (const hal_spi_config_t *cfg);
hal_status_t hal_spi_deinit    (hal_spi_bus_t bus);
hal_status_t hal_spi_transfer  (hal_spi_bus_t bus, const uint8_t *tx, uint8_t *rx,
                                 uint16_t len, uint32_t timeout_ms);
hal_status_t hal_spi_write     (hal_spi_bus_t bus, const uint8_t *data, uint16_t len,
                                 uint32_t timeout_ms);
hal_status_t hal_spi_read      (hal_spi_bus_t bus, uint8_t *data, uint16_t len,
                                 uint32_t timeout_ms);
hal_status_t hal_spi_read_reg  (hal_spi_bus_t bus, uint8_t reg, uint8_t *data, uint8_t len);
hal_status_t hal_spi_write_reg (hal_spi_bus_t bus, uint8_t reg, const uint8_t *data, uint8_t len);
#ifdef __cplusplus
}
#endif
#endif 