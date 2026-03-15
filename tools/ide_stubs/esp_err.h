#ifndef ESP_ERR_H
#define ESP_ERR_H
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_NVS_NO_FREE_PAGES 0x1101
#define ESP_ERR_NVS_NEW_VERSION_FOUND 0x1102
#endif