#include "camera_service.h"
#include "esp_camera.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "camera";

esp_err_t camera_service_init(void)
{
    camera_config_t config = {
        .pin_pwdn = -1, .pin_reset = -1, .pin_xclk = 0,
        .pin_sscb_sda = 26, .pin_sscb_scl = 27,
        .pin_d7 = 35, .pin_d6 = 34, .pin_d5 = 39, .pin_d4 = 36,
        .pin_d3 = 21, .pin_d2 = 19, .pin_d1 = 18, .pin_d0 = 5,
        .pin_vsync = 25, .pin_href = 23, .pin_pclk = 22,
        .xclk_freq_hz = 20000000, .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0, .pixel_format = PIXFORMAT_JPEG,
        .frame_size = FRAMESIZE_VGA, .jpeg_quality = 12, .fb_count = 2,
        .grab_mode = CAMERA_GRAB_LATEST
    };
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) ESP_LOGE(TAG, "camera init failed: %s", esp_err_to_name(err));
    return err;
}

esp_err_t camera_service_capture_jpeg(uint8_t **data, size_t *length)
{
    if (!data || !length) return ESP_ERR_INVALID_ARG;
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) return ESP_ERR_INVALID_STATE;
    *data = malloc(fb->len);
    if (!*data) { esp_camera_fb_return(fb); return ESP_ERR_NO_MEM; }
    memcpy(*data, fb->buf, fb->len);
    *length = fb->len;
    esp_camera_fb_return(fb);
    return ESP_OK;
}

void camera_service_release_jpeg(uint8_t *data) { free(data); }
