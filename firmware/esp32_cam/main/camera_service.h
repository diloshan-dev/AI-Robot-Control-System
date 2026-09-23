#pragma once
#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>
esp_err_t camera_service_init(void);
esp_err_t camera_service_capture_jpeg(uint8_t **data, size_t *length);
void camera_service_release_jpeg(uint8_t *data);
