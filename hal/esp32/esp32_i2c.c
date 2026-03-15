#include "hal_i2c.h"
#include "hal_timer.h"
#include "aero_config.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
static const char *TAG = "HAL_I2C_ESP32";
static i2c_master_bus_handle_t  s_bus[HAL_I2C_BUS_MAX];
static bool                     s_init[HAL_I2C_BUS_MAX];
hal_status_t hal_i2c_init(const hal_i2c_config_t *cfg)
{
    if (!cfg || cfg->bus >= HAL_I2C_BUS_MAX) return HAL_INVALID;
    i2c_master_bus_config_t bus_cfg = {
        .clk_source        = I2C_CLK_SRC_DEFAULT,
        .i2c_port          = (i2c_port_num_t)cfg->bus,
        .scl_io_num        = (gpio_num_t)22,  
        .sda_io_num        = (gpio_num_t)21,  
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    if (i2c_new_master_bus(&bus_cfg, &s_bus[cfg->bus]) != ESP_OK) {
        ESP_LOGE(TAG, "Bus %d init failed", cfg->bus);
        return HAL_ERROR;
    }
    s_init[cfg->bus] = true;
    ESP_LOGI(TAG, "Bus %d initialized", cfg->bus);
    return HAL_OK;
}
hal_status_t hal_i2c_deinit(hal_i2c_bus_t bus)
{
    if (bus >= HAL_I2C_BUS_MAX || !s_init[bus]) return HAL_INVALID;
    i2c_del_master_bus(s_bus[bus]);
    s_init[bus] = false;
    return HAL_OK;
}
hal_status_t hal_i2c_write(hal_i2c_bus_t bus, uint8_t dev_addr,
                            uint8_t reg_addr, const uint8_t *data, size_t len)
{
    if (!s_init[bus]) return HAL_INVALID;
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = dev_addr,
        .scl_speed_hz    = 400000,
    };
    i2c_master_dev_handle_t dev;
    if (i2c_master_bus_add_device(s_bus[bus], &dev_cfg, &dev) != ESP_OK)
        return HAL_ERROR;
    uint8_t tx_buf[len + 1];
    tx_buf[0] = reg_addr;
    for (size_t i = 0; i < len; i++) tx_buf[i+1] = data[i];
    esp_err_t err = i2c_master_transmit(dev, tx_buf, len + 1, 100);
    i2c_master_bus_rm_device(dev);
    return (err == ESP_OK) ? HAL_OK : HAL_ERROR;
}
hal_status_t hal_i2c_read(hal_i2c_bus_t bus, uint8_t dev_addr,
                           uint8_t reg_addr, uint8_t *data, size_t len)
{
    if (!s_init[bus]) return HAL_INVALID;
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = dev_addr,
        .scl_speed_hz    = 400000,
    };
    i2c_master_dev_handle_t dev;
    if (i2c_master_bus_add_device(s_bus[bus], &dev_cfg, &dev) != ESP_OK)
        return HAL_ERROR;
    esp_err_t err = i2c_master_transmit_receive(dev, &reg_addr, 1, data, len, 100);
    i2c_master_bus_rm_device(dev);
    return (err == ESP_OK) ? HAL_OK : HAL_ERROR;
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
    if (!s_init[bus]) return HAL_INVALID;
    esp_err_t err = i2c_master_probe(s_bus[bus], dev_addr, 50);
    return (err == ESP_OK) ? HAL_OK : HAL_NOT_FOUND;
}
hal_status_t hal_i2c_scan(hal_i2c_bus_t bus, uint8_t *found,
                           uint8_t max_found, uint8_t *count)
{
    *count = 0;
    for (uint8_t addr = 0x08; addr < 0x78 && *count < max_found; addr++) {
        if (hal_i2c_probe(bus, addr) == HAL_OK) {
            found[(*count)++] = addr;
        }
    }
    return HAL_OK;
}