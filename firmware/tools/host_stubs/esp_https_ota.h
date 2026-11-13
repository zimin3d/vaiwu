/* Host-check stub: mirrors the ESP-IDF 5.x esp_https_ota API shape. */
#pragma once
#include "esp_err.h"
#include "esp_http_client.h"

typedef void* esp_https_ota_handle_t;

typedef struct {
    const esp_http_client_config_t* http_config;
} esp_https_ota_config_t;

#define ESP_ERR_HTTPS_OTA_IN_PROGRESS (0x7001)

esp_err_t esp_https_ota_begin(const esp_https_ota_config_t* config, esp_https_ota_handle_t* handle);
esp_err_t esp_https_ota_perform(esp_https_ota_handle_t handle);
esp_err_t esp_https_ota_finish(esp_https_ota_handle_t handle);
esp_err_t esp_https_ota_abort(esp_https_ota_handle_t handle);
int esp_https_ota_get_image_size(esp_https_ota_handle_t handle);
int esp_https_ota_get_image_len_read(esp_https_ota_handle_t handle);
