#include "protocol_manager.h"
#include "hal_i2c.h"
#include "hal_spi.h"
#include "hal_uart.h"
#include "hal_adc.h"
#include "hal_timer.h"
#include "aero_config.h"
#include "lps25hb.h"
#include "lsm6dsv.h"
#include "lis2mdl.h"
#include "sht40.h"
#include "teseo_liv4f.h"
#include "tgs5141.h"
#include "sal_sensor.h"
#include <string.h>
static const char *TAG = "PROTO_MGR";
static void init_i2c_bus(void)
{
    hal_i2c_config_t cfg = {
        .bus        = HAL_I2C_BUS_1,
        .speed      = HAL_I2C_SPEED_FAST,  
        .timeout_ms = 50,
    };
    hal_status_t rc = hal_i2c_init(&cfg);
    if (rc != HAL_OK) {
        AERO_LOGE(TAG, "I2C bus 1 init failed");
    }
}
static void init_spi_bus(void)
{
    hal_spi_config_t cfg = {
        .bus         = HAL_SPI_BUS_1,
        .mode        = HAL_SPI_MODE_3,    
        .bit_order   = HAL_SPI_BITORDER_MSB,
        .clock_hz    = 8000000,            
        .cs_gpio_pin = 4,                  
        .cs_gpio_port= 0,
    };
    hal_status_t rc = hal_spi_init(&cfg);
    if (rc != HAL_OK) {
        AERO_LOGE(TAG, "SPI bus 1 init failed");
    }
}
static bool detect_i2c_device(hal_i2c_bus_t bus, uint8_t addr)
{
    return hal_i2c_probe(bus, addr) == HAL_OK;
}
static bool detect_spi_whoami(hal_spi_bus_t bus, uint8_t expected_whoami)
{
    uint8_t whoami = 0;
    hal_status_t rc = hal_spi_read_reg(bus, 0x0F | 0x80, &whoami, 1);
    return (rc == HAL_OK && whoami == expected_whoami);
}
static bool detect_uart_nmea(hal_uart_port_t port)
{
    uint8_t c = 0;
    hal_tick_t start = hal_get_tick_ms();
    while (hal_elapsed_ms(start) < 1000) {
        if (hal_uart_read_byte(port, &c, 10) == HAL_OK && c == '$')
            return true;
    }
    return false;
}
void protocol_manager_init(void)
{
    AERO_LOGI(TAG, "=== Protocol Auto-Detection ===");
    init_i2c_bus();
    init_spi_bus();
#if AERO_SENSOR_LPS25HB_ENABLE
    if (detect_i2c_device(HAL_I2C_BUS_1, AERO_LPS25HB_I2C_ADDR)) {
        AERO_LOGI(TAG, "LPS25HB: detected on I2C addr 0x%02X", AERO_LPS25HB_I2C_ADDR);
        lps25hb_driver_register(AERO_IFACE_I2C, HAL_I2C_BUS_1,
                                 HAL_SPI_BUS_1, AERO_LPS25HB_I2C_ADDR);
    } else if (detect_spi_whoami(HAL_SPI_BUS_1, 0xBD)) {
        AERO_LOGI(TAG, "LPS25HB: detected on SPI");
        lps25hb_driver_register(AERO_IFACE_SPI, HAL_I2C_BUS_1,
                                 HAL_SPI_BUS_1, 0);
    } else {
        AERO_LOGE(TAG, "LPS25HB: NOT FOUND on I2C or SPI");
    }
#endif
#if AERO_SENSOR_SHT40_ENABLE
    if (detect_i2c_device(HAL_I2C_BUS_1, AERO_SHT40_I2C_ADDR)) {
        AERO_LOGI(TAG, "SHT40: detected on I2C addr 0x%02X", AERO_SHT40_I2C_ADDR);
        sht40_driver_register(HAL_I2C_BUS_1, AERO_SHT40_I2C_ADDR);
    } else {
        AERO_LOGE(TAG, "SHT40: NOT FOUND");
    }
#endif
#if AERO_SENSOR_LSM6DSV_ENABLE
    if (detect_i2c_device(HAL_I2C_BUS_1, AERO_LSM6DSV_I2C_ADDR)) {
        AERO_LOGI(TAG, "LSM6DSV: detected on I2C addr 0x%02X", AERO_LSM6DSV_I2C_ADDR);
        lsm6dsv_driver_register(AERO_IFACE_I2C, HAL_I2C_BUS_1,
                                 HAL_SPI_BUS_1, AERO_LSM6DSV_I2C_ADDR);
    } else if (detect_spi_whoami(HAL_SPI_BUS_1, 0x70)) {
        AERO_LOGI(TAG, "LSM6DSV: detected on SPI");
        lsm6dsv_driver_register(AERO_IFACE_SPI, HAL_I2C_BUS_1,
                                 HAL_SPI_BUS_1, 0);
    } else {
        AERO_LOGE(TAG, "LSM6DSV: NOT FOUND");
    }
#endif
#if AERO_SENSOR_LIS2MDL_ENABLE
    if (detect_i2c_device(HAL_I2C_BUS_1, AERO_LIS2MDL_I2C_ADDR)) {
        AERO_LOGI(TAG, "LIS2MDL: detected on I2C addr 0x%02X", AERO_LIS2MDL_I2C_ADDR);
        lis2mdl_driver_register(AERO_IFACE_I2C, HAL_I2C_BUS_1,
                                  HAL_SPI_BUS_1, AERO_LIS2MDL_I2C_ADDR);
    } else {
        AERO_LOGE(TAG, "LIS2MDL: NOT FOUND");
    }
#endif
#if AERO_SENSOR_TESEO_LIV4F_ENABLE
    {
        hal_uart_config_t gnss_uart = {
            .port      = HAL_UART_PORT_2,
            .baud_rate = 115200,
            .data_bits = 8,
            .parity    = HAL_UART_PARITY_NONE,
            .stop_bits = HAL_UART_STOPBITS_1,
        };
        hal_uart_init(&gnss_uart);
        if (detect_uart_nmea(HAL_UART_PORT_2)) {
            AERO_LOGI(TAG, "Teseo-LIV4F: detected on UART2 (NMEA)");
            teseo_driver_register(AERO_IFACE_UART, HAL_UART_PORT_2,
                                   HAL_I2C_BUS_1, AERO_TESEO_I2C_ADDR);
        } else if (detect_i2c_device(HAL_I2C_BUS_1, AERO_TESEO_I2C_ADDR)) {
            AERO_LOGI(TAG, "Teseo-LIV4F: detected on I2C addr 0x%02X", AERO_TESEO_I2C_ADDR);
            teseo_driver_register(AERO_IFACE_I2C, HAL_UART_PORT_2,
                                   HAL_I2C_BUS_1, AERO_TESEO_I2C_ADDR);
        } else {
            AERO_LOGE(TAG, "Teseo-LIV4F: NOT FOUND on UART or I2C");
        }
    }
#endif
#if AERO_SENSOR_TGS5141_ENABLE
    AERO_LOGI(TAG, "TGS-5141: registering on ADC unit=1 ch=0 vref=3300mV");
    tgs5141_driver_register(HAL_ADC_UNIT_1, 0, 3300.0f);
#endif
    AERO_LOGI(TAG, "=== Protocol Detection Complete ===");
}