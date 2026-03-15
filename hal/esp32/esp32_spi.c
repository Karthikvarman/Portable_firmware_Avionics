#include "hal_spi.h"
#include "hal_common.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include <string.h>
#define ESP32_SPI_MAX_BUSES     3   
#define ESP32_SPI_MAX_FREQ_HZ   80000000
typedef struct {
    spi_host_device_t   host;
    spi_device_handle_t dev;
    bool                initialized;
    int                 cs_pin;
} esp32_spi_ctx_t;
static esp32_spi_ctx_t s_spi_ctx[ESP32_SPI_MAX_BUSES];
static const int SPI_MOSI_DEFAULT = 23;
static const int SPI_MISO_DEFAULT = 19;
static const int SPI_CLK_DEFAULT  = 18;
hal_status_t hal_spi_init(const hal_spi_config_t *cfg)
{
    if (!cfg || cfg->bus >= ESP32_SPI_MAX_BUSES) return HAL_PARAM_ERROR;
    esp32_spi_ctx_t *ctx = &s_spi_ctx[cfg->bus];
    const spi_host_device_t hosts[] = {SPI1_HOST, SPI2_HOST, SPI3_HOST};
    ctx->host   = hosts[cfg->bus];
    ctx->cs_pin = cfg->cs_gpio_pin;
    spi_bus_config_t bus_cfg = {
        .mosi_io_num    = SPI_MOSI_DEFAULT,
        .miso_io_num    = SPI_MISO_DEFAULT,
        .sclk_io_num    = SPI_CLK_DEFAULT,
        .quadwp_io_num  = -1,
        .quadhd_io_num  = -1,
        .max_transfer_sz= 4096,
    };
    esp_err_t err = spi_bus_initialize(ctx->host, &bus_cfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return HAL_IO_ERROR;
    uint32_t mode = 0;
    if (cfg->mode == HAL_SPI_MODE_1) mode = 1;
    else if (cfg->mode == HAL_SPI_MODE_2) mode = 2;
    else if (cfg->mode == HAL_SPI_MODE_3) mode = 3;
    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = (int)cfg->clock_hz,
        .mode           = (uint8_t)mode,
        .spics_io_num   = cfg->cs_gpio_pin,
        .queue_size     = 8,
        .flags          = (cfg->bit_order == HAL_SPI_BITORDER_LSB) ? SPI_DEVICE_BIT_LSBFIRST : 0,
    };
    err = spi_bus_add_device(ctx->host, &dev_cfg, &ctx->dev);
    if (err != ESP_OK) return HAL_IO_ERROR;
    ctx->initialized = true;
    return HAL_OK;
}
hal_status_t hal_spi_transfer(hal_spi_bus_t bus, const uint8_t *tx, uint8_t *rx,
                               uint16_t len, uint32_t timeout_ms)
{
    if (bus >= ESP32_SPI_MAX_BUSES || !s_spi_ctx[bus].initialized) return HAL_NOT_INIT;
    if (!tx && !rx) return HAL_PARAM_ERROR;
    static uint8_t s_dummy[1024];
    if (!tx) { memset(s_dummy, 0xFF, len < 1024 ? len : 1024); tx = s_dummy; }
    spi_transaction_t t = {
        .length    = len * 8,   
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    esp_err_t err = spi_device_polling_transmit(s_spi_ctx[bus].dev, &t);
    return (err == ESP_OK) ? HAL_OK : HAL_IO_ERROR;
}
hal_status_t hal_spi_write(hal_spi_bus_t bus, const uint8_t *data, uint16_t len,
                            uint32_t timeout_ms)
{
    return hal_spi_transfer(bus, data, NULL, len, timeout_ms);
}
hal_status_t hal_spi_read(hal_spi_bus_t bus, uint8_t *data, uint16_t len,
                           uint32_t timeout_ms)
{
    return hal_spi_transfer(bus, NULL, data, len, timeout_ms);
}
hal_status_t hal_spi_read_reg(hal_spi_bus_t bus, uint8_t reg, uint8_t *data, uint8_t len)
{
    if (bus >= ESP32_SPI_MAX_BUSES || !s_spi_ctx[bus].initialized) return HAL_NOT_INIT;
    uint8_t tx[len + 1];
    uint8_t rx[len + 1];
    tx[0] = reg | 0x80;
    for (int i = 1; i <= len; i++) tx[i] = 0x00;
    hal_status_t rc = hal_spi_transfer(bus, tx, rx, len + 1, 50);
    if (rc == HAL_OK && data) {
        for (int i = 0; i < len; i++) data[i] = rx[i + 1];
    }
    return rc;
}
hal_status_t hal_spi_write_reg(hal_spi_bus_t bus, uint8_t reg, const uint8_t *data, uint8_t len)
{
    if (bus >= ESP32_SPI_MAX_BUSES || !s_spi_ctx[bus].initialized) return HAL_NOT_INIT;
    uint8_t tx[len + 1];
    tx[0] = reg & 0x7F;
    for (int i = 0; i < len; i++) tx[i + 1] = data[i];
    return hal_spi_transfer(bus, tx, NULL, len + 1, 50);
}
hal_status_t hal_spi_deinit(hal_spi_bus_t bus)
{
    if (bus >= ESP32_SPI_MAX_BUSES) return HAL_PARAM_ERROR;
    esp32_spi_ctx_t *ctx = &s_spi_ctx[bus];
    if (ctx->initialized) {
        spi_bus_remove_device(ctx->dev);
        spi_bus_free(ctx->host);
        ctx->initialized = false;
    }
    return HAL_OK;
}