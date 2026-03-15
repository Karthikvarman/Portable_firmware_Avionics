#ifndef AERO_CONFIG_H
#define AERO_CONFIG_H
#define AERO_FW_VERSION_MAJOR 1
#define AERO_FW_VERSION_MINOR 0
#define AERO_FW_VERSION_PATCH 0
#define AERO_FW_VERSION_STR "1.0.0"
#define AERO_FW_BUILD_DATE __DATE__ " " __TIME__
#if !defined(TARGET_STM32H7) && !defined(TARGET_STM32F4) &&                    \
    !defined(TARGET_STM32F7) && !defined(TARGET_STM32L4) &&                    \
    !defined(TARGET_STM32L0) && !defined(TARGET_ESP32) &&                      \
    !defined(TARGET_ESP32_S2) && !defined(TARGET_ESP32_S3) &&                  \
    !defined(TARGET_ESP32_C3) && !defined(TARGET_ESP32_C6) &&                  \
    !defined(TARGET_ESP32_H2) && !defined(TARGET_ESP8266) &&                   \
    !defined(TARGET_ESP32_DEVKITV1) && !defined(TARGET_ESP32_DEV_MODULE)
#define TARGET_STM32H7 
#endif
#if defined(TARGET_STM32H7) || defined(TARGET_STM32F4) ||                      \
    defined(TARGET_STM32F7) || defined(TARGET_STM32L4) ||                      \
    defined(TARGET_STM32L0)
#define AERO_MCU_STM32
#elif defined(TARGET_ESP32) || defined(TARGET_ESP32_S2) ||                     \
    defined(TARGET_ESP32_S3) || defined(TARGET_ESP32_C3) ||                    \
    defined(TARGET_ESP32_C6) || defined(TARGET_ESP32_H2) ||                    \
    defined(TARGET_ESP32_DEVKITV1) || defined(TARGET_ESP32_DEV_MODULE)
#define AERO_MCU_ESP32
#define AERO_MCU_ESP
#elif defined(TARGET_ESP8266)
#define AERO_MCU_ESP8266
#define AERO_MCU_ESP
#endif
#if defined(TARGET_STM32H7)
#define AERO_SYSCLK_HZ 480000000UL
#elif defined(TARGET_STM32F4)
#define AERO_SYSCLK_HZ 168000000UL
#elif defined(TARGET_STM32F7)
#define AERO_SYSCLK_HZ 216000000UL
#elif defined(TARGET_STM32L4)
#define AERO_SYSCLK_HZ 80000000UL
#elif defined(TARGET_STM32L0)
#define AERO_SYSCLK_HZ 32000000UL
#elif defined(TARGET_ESP32) || defined(TARGET_ESP32_S2) ||                     \
    defined(TARGET_ESP32_S3) || defined(TARGET_ESP32_DEVKITV1) ||              \
    defined(TARGET_ESP32_DEV_MODULE)
#define AERO_SYSCLK_HZ 240000000UL
#elif defined(TARGET_ESP32_C3) || defined(TARGET_ESP32_C6)
#define AERO_SYSCLK_HZ 160000000UL
#elif defined(TARGET_ESP32_H2)
#define AERO_SYSCLK_HZ 96000000UL
#elif defined(TARGET_ESP8266)
#define AERO_SYSCLK_HZ 80000000UL
#endif
#ifndef AERO_RTOS_TICK_RATE_HZ
#define AERO_RTOS_TICK_RATE_HZ 1000
#endif
#define AERO_SENSOR_LPS25HB_ENABLE 1     
#define AERO_SENSOR_SHT40_ENABLE 1       
#define AERO_SENSOR_LSM6DSV_ENABLE 1     
#define AERO_SENSOR_LIS2MDL_ENABLE 1     
#define AERO_SENSOR_TESEO_LIV4F_ENABLE 1 
#define AERO_SENSOR_TGS5141_ENABLE 1     
#define AERO_IFACE_AUTO 0
#define AERO_IFACE_I2C 1
#define AERO_IFACE_SPI 2
#define AERO_IFACE_UART 3
#define AERO_IFACE_ADC 4
#define AERO_LPS25HB_IFACE AERO_IFACE_AUTO
#define AERO_SHT40_IFACE AERO_IFACE_I2C 
#define AERO_LSM6DSV_IFACE AERO_IFACE_AUTO
#define AERO_LIS2MDL_IFACE AERO_IFACE_AUTO
#define AERO_TESEO_LIV4F_IFACE AERO_IFACE_AUTO
#define AERO_TGS5141_IFACE AERO_IFACE_ADC 
#define AERO_LPS25HB_I2C_ADDR 0x5D 
#define AERO_SHT40_I2C_ADDR 0x44   
#define AERO_LSM6DSV_I2C_ADDR 0x6B 
#define AERO_LIS2MDL_I2C_ADDR 0x1E 
#define AERO_TESEO_I2C_ADDR 0x3A   
#define AERO_TASK_IMU_PERIOD_MS 5    
#define AERO_TASK_BARO_PERIOD_MS 20  
#define AERO_TASK_MAG_PERIOD_MS 20   
#define AERO_TASK_GPS_PERIOD_MS 100  
#define AERO_TASK_ENV_PERIOD_MS 1000 
#define AERO_TASK_TELEM_PERIOD_MS 50 
#define AERO_TASK_MON_PERIOD_MS 1000 
#define AERO_TASK_SENSOR_STACK 512    
#define AERO_TASK_EKF_STACK 1024      
#define AERO_TASK_TELEMETRY_STACK 512 
#define AERO_TASK_MONITOR_STACK 384   
#define AERO_TASK_WIRELESS_STACK 1024
#define AERO_PRIO_IDLE 0
#define AERO_PRIO_MONITOR 2
#define AERO_PRIO_WIRELESS 3
#define AERO_PRIO_TELEMETRY 4
#define AERO_PRIO_SENSOR 6
#define AERO_PRIO_CRITICAL 7
#define AERO_CAL_SAMPLE_COUNT 500 
#define AERO_CAL_AUTO_SAVE 1      
#define AERO_EKF_Q_ACCEL 1.0e-3f      
#define AERO_EKF_Q_GYRO 1.0e-4f       
#define AERO_EKF_Q_ACCEL_BIAS 1.0e-7f 
#define AERO_EKF_Q_GYRO_BIAS 1.0e-8f  
#define AERO_EKF_R_BARO_ALT 0.5f 
#define AERO_EKF_R_GPS_POS 3.0f  
#define AERO_EKF_R_GPS_VEL 0.2f  
#define AERO_EKF_R_MAG 0.05f     
#define AERO_TELEM_UART_PORT 1  
#define AERO_TELEM_BUF_SIZE 256 
#define AERO_TELEM_FRAME_HEADER 0xAE
#define AERO_TELEM_FRAME_FOOTER 0xEA
#define AERO_TELEM_ASCII_ENABLE 1
#define AERO_WDT_ENABLE 1
#define AERO_WDT_TIMEOUT_MS 5000 
#define AERO_DATA_FLASH_ENABLE 1
#define AERO_DATA_FLASH_ADDR 0x081E0000UL 
#define AERO_DATA_FLASH_SIZE (128 * 1024UL)
#define AERO_LOG_LEVEL_NONE 0
#define AERO_LOG_LEVEL_ERROR 1
#define AERO_LOG_LEVEL_WARN 2
#define AERO_LOG_LEVEL_INFO 3
#define AERO_LOG_LEVEL_DEBUG 4
#ifndef AERO_LOG_LEVEL
#define AERO_LOG_LEVEL AERO_LOG_LEVEL_INFO
#endif
#include <stdio.h>
#if AERO_LOG_LEVEL >= AERO_LOG_LEVEL_ERROR
#define AERO_LOGE(tag, fmt, ...)                                               \
  printf("[E][%s] " fmt "\r\n", tag, ##__VA_ARGS__)
#else
#define AERO_LOGE(tag, fmt, ...)                                               \
  do {                                                                         \
  } while (0)
#endif
#if AERO_LOG_LEVEL >= AERO_LOG_LEVEL_WARN
#define AERO_LOGW(tag, fmt, ...)                                               \
  printf("[W][%s] " fmt "\r\n", tag, ##__VA_ARGS__)
#else
#define AERO_LOGW(tag, fmt, ...)                                               \
  do {                                                                         \
  } while (0)
#endif
#if AERO_LOG_LEVEL >= AERO_LOG_LEVEL_INFO
#define AERO_LOGI(tag, fmt, ...)                                               \
  printf("[I][%s] " fmt "\r\n", tag, ##__VA_ARGS__)
#else
#define AERO_LOGI(tag, fmt, ...)                                               \
  do {                                                                         \
  } while (0)
#endif
#if AERO_LOG_LEVEL >= AERO_LOG_LEVEL_DEBUG
#define AERO_LOGD(tag, fmt, ...)                                               \
  printf("[D][%s] " fmt "\r\n", tag, ##__VA_ARGS__)
#else
#define AERO_LOGD(tag, fmt, ...)                                               \
  do {                                                                         \
  } while (0)
#endif
#ifdef AERO_MCU_ESP
#include "esp_log.h"
#undef AERO_LOGE
#undef AERO_LOGW
#undef AERO_LOGI
#undef AERO_LOGD
#define AERO_LOGE(tag, fmt, ...) ESP_LOGE(tag, fmt, ##__VA_ARGS__)
#define AERO_LOGW(tag, fmt, ...) ESP_LOGW(tag, fmt, ##__VA_ARGS__)
#define AERO_LOGI(tag, fmt, ...) ESP_LOGI(tag, fmt, ##__VA_ARGS__)
#define AERO_LOGD(tag, fmt, ...) ESP_LOGD(tag, fmt, ##__VA_ARGS__)
#endif
#endif 