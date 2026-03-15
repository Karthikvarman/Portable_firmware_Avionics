#include "hal_gpio.h"
#include "hal_common.h"
#include "stm32f4xx_ll_gpio.h"
#include "stm32f4xx_ll_bus.h"
static GPIO_TypeDef * const s_gpio_ports[] = { GPIOA, GPIOB, GPIOC, GPIOD, GPIOE, GPIOF, GPIOG, GPIOH };
#define NUM_PORTS (sizeof(s_gpio_ports)/sizeof(s_gpio_ports[0]))
static void gpio_clk_enable(uint8_t port)
{
    const uint32_t masks[] = {
        LL_AHB1_GRP1_PERIPH_GPIOA, LL_AHB1_GRP1_PERIPH_GPIOB,
        LL_AHB1_GRP1_PERIPH_GPIOC, LL_AHB1_GRP1_PERIPH_GPIOD,
        LL_AHB1_GRP1_PERIPH_GPIOE, LL_AHB1_GRP1_PERIPH_GPIOF,
        LL_AHB1_GRP1_PERIPH_GPIOG, LL_AHB1_GRP1_PERIPH_GPIOH,
    };
    if (port < NUM_PORTS)
        LL_AHB1_GRP1_EnableClock(masks[port]);
}
hal_status_t hal_gpio_init(const hal_gpio_config_t *cfg)
{
    if (!cfg || cfg->port >= NUM_PORTS) return HAL_PARAM_ERROR;
    gpio_clk_enable(cfg->port);
    GPIO_TypeDef *GPIOx = s_gpio_ports[cfg->port];
    LL_GPIO_InitTypeDef init = {
        .Pin        = (1u << cfg->pin),
        .Speed      = LL_GPIO_SPEED_FREQ_HIGH,
        .OutputType = (cfg->mode == HAL_GPIO_MODE_OUTPUT_OD) ? LL_GPIO_OUTPUT_OPENDRAIN : LL_GPIO_OUTPUT_PUSHPULL,
        .Pull       = (cfg->pull == HAL_GPIO_PULL_UP) ? LL_GPIO_PULL_UP : (cfg->pull == HAL_GPIO_PULL_DOWN ? LL_GPIO_PULL_DOWN : LL_GPIO_PULL_NO),
    };
    switch (cfg->mode) {
        case HAL_GPIO_MODE_INPUT:  init.Mode = LL_GPIO_MODE_INPUT; break;
        case HAL_GPIO_MODE_ANALOG: init.Mode = LL_GPIO_MODE_ANALOG; break;
        default:                   init.Mode = LL_GPIO_MODE_OUTPUT; break;
    }
    LL_GPIO_Init(GPIOx, &init);
    if (init.Mode == LL_GPIO_MODE_OUTPUT) hal_gpio_write(cfg->port, cfg->pin, cfg->initial_state);
    return HAL_OK;
}
hal_status_t hal_gpio_write(uint8_t port, uint8_t pin, hal_gpio_state_t state)
{
    if (port >= NUM_PORTS) return HAL_PARAM_ERROR;
    if (state == HAL_GPIO_HIGH) LL_GPIO_SetOutputPin(s_gpio_ports[port], 1u << pin);
    else                        LL_GPIO_ResetOutputPin(s_gpio_ports[port], 1u << pin);
    return HAL_OK;
}
hal_status_t hal_gpio_read(uint8_t port, uint8_t pin, hal_gpio_state_t *out)
{
    if (port >= NUM_PORTS || !out) return HAL_PARAM_ERROR;
    *out = LL_GPIO_IsInputPinSet(s_gpio_ports[port], 1u << pin) ? HAL_GPIO_HIGH : HAL_GPIO_LOW;
    return HAL_OK;
}
hal_status_t hal_gpio_toggle(uint8_t port, uint8_t pin)
{
    if (port >= NUM_PORTS) return HAL_PARAM_ERROR;
    LL_GPIO_TogglePin(s_gpio_ports[port], 1u << pin);
    return HAL_OK;
}
hal_status_t hal_gpio_deinit(uint8_t port, uint8_t pin)
{
    if (port >= NUM_PORTS) return HAL_PARAM_ERROR;
    LL_GPIO_InitTypeDef d = { .Pin=1u<<pin, .Mode=LL_GPIO_MODE_ANALOG, .Pull=LL_GPIO_PULL_NO };
    LL_GPIO_Init(s_gpio_ports[port], &d);
    return HAL_OK;
}