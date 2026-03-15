#include "hal_spi.h"
#include "hal_gpio.h"
#include "hal_timer.h"
#include <string.h>
#include "stm32h7xx_ll_spi.h"
#include "stm32h7xx_ll_bus.h"
#include "stm32h7xx_ll_gpio.h"
typedef struct {
    SPI_TypeDef *instance;
    bool         initialized;
    uint8_t      cs_port;
    uint8_t      cs_pin;
} stm32h7_spi_state_t;
static stm32h7_spi_state_t s_spi[HAL_SPI_BUS_MAX] = {
    [HAL_SPI_BUS_1] = { .instance = SPI1 },
    [HAL_SPI_BUS_2] = { .instance = SPI2 },
    [HAL_SPI_BUS_3] = { .instance = SPI3 },
};
static void spi_enable_clock(hal_spi_bus_t bus)
{
    switch (bus) {
        case HAL_SPI_BUS_1: LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SPI1); break;
        case HAL_SPI_BUS_2: LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_SPI2); break;
        case HAL_SPI_BUS_3: LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_SPI3); break;
        default: break;
    }
}
hal_status_t hal_spi_init(const hal_spi_config_t *cfg)
{
    if (!cfg || cfg->bus >= HAL_SPI_BUS_MAX) return HAL_INVALID;
    stm32h7_spi_state_t *st = &s_spi[cfg->bus];
    spi_enable_clock(cfg->bus);
    LL_SPI_Disable(st->instance);
    uint32_t phase = (cfg->mode & 1) ? LL_SPI_PHASE_2EDGE  : LL_SPI_PHASE_1EDGE;
    uint32_t polarity = (cfg->mode & 2) ? LL_SPI_POLARITY_HIGH : LL_SPI_POLARITY_LOW;
    uint32_t apb_clk = AERO_APB2_HZ;  
    uint32_t prescaler = LL_SPI_BAUDRATEPRESCALER_DIV2;
    uint32_t div = 2;
    while (div < 256 && (apb_clk / div) > cfg->clock_hz) {
        div <<= 1;
        prescaler += LL_SPI_BAUDRATEPRESCALER_DIV4 - LL_SPI_BAUDRATEPRESCALER_DIV2;
    }
    LL_SPI_InitTypeDef init = {
        .TransferDirection = LL_SPI_FULL_DUPLEX,
        .Mode              = LL_SPI_MODE_MASTER,
        .DataWidth         = LL_SPI_DATAWIDTH_8BIT,
        .ClockPolarity     = polarity,
        .ClockPhase        = phase,
        .NSS               = LL_SPI_NSS_SOFT,
        .BaudRate          = prescaler,
        .BitOrder          = (cfg->bit_order == HAL_SPI_BITORDER_MSB) ?
                              LL_SPI_MSB_FIRST : LL_SPI_LSB_FIRST,
        .CRCCalculation    = LL_SPI_CRCCALCULATION_DISABLE,
        .CRCPoly           = 7,
    };
    if (LL_SPI_Init(st->instance, &init) != SUCCESS) return HAL_ERROR;
    st->cs_port = cfg->cs_gpio_port;
    st->cs_pin  = cfg->cs_gpio_pin;
    st->initialized = true;
    hal_gpio_config_t gpio_cs = {
        .port  = cfg->cs_gpio_port,
        .pin   = cfg->cs_gpio_pin,
        .dir   = HAL_GPIO_OUTPUT,
        .pull  = HAL_GPIO_PULL_NONE,
        .speed = HAL_GPIO_SPEED_HIGH,
    };
    hal_gpio_init(&gpio_cs);
    hal_gpio_write(cfg->cs_gpio_port, cfg->cs_gpio_pin, HAL_GPIO_STATE_HIGH);
    LL_SPI_Enable(st->instance);
    AERO_LOGI("HAL_SPI", "Bus %d init OK, mode=%d", cfg->bus, cfg->mode);
    return HAL_OK;
}
hal_status_t hal_spi_deinit(hal_spi_bus_t bus)
{
    if (bus >= HAL_SPI_BUS_MAX) return HAL_INVALID;
    LL_SPI_Disable(s_spi[bus].instance);
    LL_SPI_DeInit(s_spi[bus].instance);
    s_spi[bus].initialized = false;
    return HAL_OK;
}
void hal_spi_cs_assert(hal_spi_bus_t bus)
{
    if (bus < HAL_SPI_BUS_MAX)
        hal_gpio_write(s_spi[bus].cs_port, s_spi[bus].cs_pin, HAL_GPIO_STATE_LOW);
}
void hal_spi_cs_deassert(hal_spi_bus_t bus)
{
    if (bus < HAL_SPI_BUS_MAX)
        hal_gpio_write(s_spi[bus].cs_port, s_spi[bus].cs_pin, HAL_GPIO_STATE_HIGH);
}
static uint8_t spi_transfer_byte(SPI_TypeDef *spi, uint8_t tx)
{
    hal_tick_t start = hal_get_tick_ms();
    while (!LL_SPI_IsActiveFlag_TXP(spi)) {
        if (hal_elapsed_ms(start) > 100) return 0xFF;
    }
    LL_SPI_TransmitData8(spi, tx);
    start = hal_get_tick_ms();
    while (!LL_SPI_IsActiveFlag_RXP(spi)) {
        if (hal_elapsed_ms(start) > 100) return 0xFF;
    }
    return LL_SPI_ReceiveData8(spi);
}
hal_status_t hal_spi_transfer(hal_spi_bus_t bus, const uint8_t *tx_buf,
                               uint8_t *rx_buf, size_t len)
{
    if (bus >= HAL_SPI_BUS_MAX || !s_spi[bus].initialized) return HAL_INVALID;
    SPI_TypeDef *spi = s_spi[bus].instance;
    LL_SPI_StartMasterTransfer(spi);
    for (size_t i = 0; i < len; i++) {
        uint8_t tx = tx_buf ? tx_buf[i] : 0xFF;
        uint8_t rx = spi_transfer_byte(spi, tx);
        if (rx_buf) rx_buf[i] = rx;
    }
    while (LL_SPI_IsActiveFlag_BSY(spi));
    LL_SPI_SuspendMasterTransfer(spi);
    return HAL_OK;
}
hal_status_t hal_spi_write(hal_spi_bus_t bus, const uint8_t *data, size_t len)
{
    return hal_spi_transfer(bus, data, NULL, len);
}
hal_status_t hal_spi_read(hal_spi_bus_t bus, uint8_t *data, size_t len)
{
    return hal_spi_transfer(bus, NULL, data, len);
}
hal_status_t hal_spi_write_reg(hal_spi_bus_t bus, uint8_t reg_addr,
                                const uint8_t *data, size_t len)
{
    hal_spi_cs_assert(bus);
    uint8_t cmd = reg_addr & 0x7F; 
    hal_spi_transfer(bus, &cmd, NULL, 1);
    hal_spi_transfer(bus, data, NULL, len);
    hal_spi_cs_deassert(bus);
    return HAL_OK;
}
hal_status_t hal_spi_read_reg(hal_spi_bus_t bus, uint8_t reg_addr,
                               uint8_t *data, size_t len)
{
    hal_spi_cs_assert(bus);
    uint8_t cmd = reg_addr | 0x80; 
    hal_spi_transfer(bus, &cmd, NULL, 1);
    hal_spi_transfer(bus, NULL, data, len);
    hal_spi_cs_deassert(bus);
    return HAL_OK;
}