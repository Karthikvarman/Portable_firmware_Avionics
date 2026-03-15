#include "hal_i2c.h"
#include "hal_gpio.h"
#include "hal_timer.h"
#include "aero_config.h"
#include <string.h>
#include "stm32h7xx_ll_i2c.h"
#include "stm32h7xx_ll_rcc.h"
#include "stm32h7xx_ll_bus.h"
#include "stm32h7xx_ll_gpio.h"
typedef struct {
    I2C_TypeDef  *instance;
    bool          initialized;
    uint32_t      timeout_ms;
} stm32h7_i2c_state_t;
static stm32h7_i2c_state_t s_i2c[HAL_I2C_BUS_MAX] = {
    [HAL_I2C_BUS_1] = { .instance = I2C1 },
    [HAL_I2C_BUS_2] = { .instance = I2C2 },
    [HAL_I2C_BUS_3] = { .instance = I2C3 },
};
static hal_status_t wait_flag(I2C_TypeDef *i2c, uint32_t flag,
                               uint32_t state, uint32_t timeout_ms)
{
    hal_tick_t start = hal_get_tick_ms();
    while (LL_I2C_IsActiveFlag(i2c, flag) != state) {
        if (hal_elapsed_ms(start) >= timeout_ms) return HAL_TIMEOUT;
        if (LL_I2C_IsActiveFlag_NACK(i2c)) {
            LL_I2C_ClearFlag_NACK(i2c);
            return HAL_NOT_FOUND;
        }
        if (LL_I2C_IsActiveFlag_BERR(i2c) ||
            LL_I2C_IsActiveFlag_ARLO(i2c)) {
            LL_I2C_ClearFlag_BERR(i2c);
            LL_I2C_ClearFlag_ARLO(i2c);
            return HAL_ERROR;
        }
    }
    return HAL_OK;
}
static void enable_clock(hal_i2c_bus_t bus)
{
    switch (bus) {
        case HAL_I2C_BUS_1: LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C1); break;
        case HAL_I2C_BUS_2: LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C2); break;
        case HAL_I2C_BUS_3: LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C3); break;
        default: break;
    }
}
hal_status_t hal_i2c_init(const hal_i2c_config_t *cfg)
{
    if (!cfg || cfg->bus >= HAL_I2C_BUS_MAX) return HAL_INVALID;
    stm32h7_i2c_state_t *st = &s_i2c[cfg->bus];
    enable_clock(cfg->bus);
    uint32_t timing;
    switch (cfg->speed) {
        case HAL_I2C_SPEED_FAST_PLUS: timing = 0x00300F38; break; 
        case HAL_I2C_SPEED_FAST:      timing = 0x10805E89; break; 
        default:                       timing = 0x307075B1; break; 
    }
    LL_I2C_InitTypeDef init = {
        .PeripheralMode  = LL_I2C_MODE_I2C,
        .Timing          = timing,
        .AnalogFilter    = LL_I2C_ANALOGFILTER_ENABLE,
        .DigitalFilter   = 0,
        .OwnAddress1     = 0,
        .TypeAcknowledge = LL_I2C_ACK,
        .OwnAddrSize     = LL_I2C_OWNADDRESS1_7BIT,
    };
    if (LL_I2C_Init(st->instance, &init) != SUCCESS) return HAL_ERROR;
    LL_I2C_Enable(st->instance);
    st->initialized = true;
    st->timeout_ms  = cfg->timeout_ms ? cfg->timeout_ms : 100;
    AERO_LOGI("HAL_I2C", "Bus %d initialized @ %lu Hz", cfg->bus, cfg->speed);
    return HAL_OK;
}
hal_status_t hal_i2c_deinit(hal_i2c_bus_t bus)
{
    if (bus >= HAL_I2C_BUS_MAX) return HAL_INVALID;
    LL_I2C_Disable(s_i2c[bus].instance);
    LL_I2C_DeInit(s_i2c[bus].instance);
    s_i2c[bus].initialized = false;
    return HAL_OK;
}
hal_status_t hal_i2c_write(hal_i2c_bus_t bus, uint8_t dev_addr,
                            uint8_t reg_addr, const uint8_t *data, size_t len)
{
    if (bus >= HAL_I2C_BUS_MAX || !s_i2c[bus].initialized) return HAL_INVALID;
    I2C_TypeDef *i2c = s_i2c[bus].instance;
    uint32_t     tmo = s_i2c[bus].timeout_ms;
    hal_status_t rc;
    LL_I2C_HandleTransfer(i2c, (dev_addr << 1), LL_I2C_ADDRSLAVE_7BIT,
                          (uint32_t)(len + 1), LL_I2C_MODE_AUTOEND,
                          LL_I2C_GENERATE_START_WRITE);
    rc = wait_flag(i2c, I2C_ISR_TXIS, 1, tmo);
    if (rc != HAL_OK) return rc;
    LL_I2C_TransmitData8(i2c, reg_addr);
    for (size_t i = 0; i < len; i++) {
        rc = wait_flag(i2c, I2C_ISR_TXIS, 1, tmo);
        if (rc != HAL_OK) return rc;
        LL_I2C_TransmitData8(i2c, data[i]);
    }
    rc = wait_flag(i2c, I2C_ISR_STOPF, 1, tmo);
    LL_I2C_ClearFlag_STOP(i2c);
    return rc;
}
hal_status_t hal_i2c_read(hal_i2c_bus_t bus, uint8_t dev_addr,
                           uint8_t reg_addr, uint8_t *data, size_t len)
{
    if (bus >= HAL_I2C_BUS_MAX || !s_i2c[bus].initialized) return HAL_INVALID;
    I2C_TypeDef *i2c = s_i2c[bus].instance;
    uint32_t     tmo = s_i2c[bus].timeout_ms;
    hal_status_t rc;
    LL_I2C_HandleTransfer(i2c, (dev_addr << 1), LL_I2C_ADDRSLAVE_7BIT,
                          1, LL_I2C_MODE_SOFTEND,
                          LL_I2C_GENERATE_START_WRITE);
    rc = wait_flag(i2c, I2C_ISR_TXIS, 1, tmo);
    if (rc != HAL_OK) return rc;
    LL_I2C_TransmitData8(i2c, reg_addr);
    rc = wait_flag(i2c, I2C_ISR_TC, 1, tmo);
    if (rc != HAL_OK) return rc;
    LL_I2C_HandleTransfer(i2c, (dev_addr << 1) | 1, LL_I2C_ADDRSLAVE_7BIT,
                          (uint32_t)len, LL_I2C_MODE_AUTOEND,
                          LL_I2C_GENERATE_START_READ);
    for (size_t i = 0; i < len; i++) {
        rc = wait_flag(i2c, I2C_ISR_RXNE, 1, tmo);
        if (rc != HAL_OK) return rc;
        data[i] = LL_I2C_ReceiveData8(i2c);
    }
    rc = wait_flag(i2c, I2C_ISR_STOPF, 1, tmo);
    LL_I2C_ClearFlag_STOP(i2c);
    return rc;
}
hal_status_t hal_i2c_write_reg(hal_i2c_bus_t bus, uint8_t dev_addr,
                                uint8_t reg_addr, uint8_t value)
{
    return hal_i2c_write(bus, dev_addr, reg_addr, &value, 1);
}
hal_status_t hal_i2c_read_reg(hal_i2c_bus_t bus, uint8_t dev_addr,
                               uint8_t reg_addr, uint8_t *value)
{
    return hal_i2c_read(bus, dev_addr, reg_addr, value, 1);
}
hal_status_t hal_i2c_probe(hal_i2c_bus_t bus, uint8_t dev_addr)
{
    if (bus >= HAL_I2C_BUS_MAX || !s_i2c[bus].initialized) return HAL_INVALID;
    I2C_TypeDef *i2c = s_i2c[bus].instance;
    LL_I2C_HandleTransfer(i2c, (dev_addr << 1), LL_I2C_ADDRSLAVE_7BIT,
                          0, LL_I2C_MODE_AUTOEND,
                          LL_I2C_GENERATE_START_WRITE);
    hal_status_t rc = wait_flag(i2c, I2C_ISR_STOPF, 1, 5);
    LL_I2C_ClearFlag_STOP(i2c);
    return rc;
}
hal_status_t hal_i2c_scan(hal_i2c_bus_t bus, uint8_t *found,
                           uint8_t max_found, uint8_t *count)
{
    *count = 0;
    for (uint8_t addr = 0x08; addr < 0x78 && *count < max_found; addr++) {
        if (hal_i2c_probe(bus, addr) == HAL_OK) {
            found[(*count)++] = addr;
            AERO_LOGI("HAL_I2C", "Found device at 0x%02X", addr);
        }
        hal_delay_ms(1);
    }
    return HAL_OK;
}