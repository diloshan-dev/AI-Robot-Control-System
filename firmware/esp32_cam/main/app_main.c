#include "camera_service.h"
#include "servo.h"
#include "wifi_server.h"
#include "esp_log.h"
#include "nvs_flash.h"
void app_main(void)
{
    esp_err_t nvs = nvs_flash_init();
    if (nvs == ESP_ERR_NVS_NO_FREE_PAGES || nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) { ESP_ERROR_CHECK(nvs_flash_erase()); ESP_ERROR_CHECK(nvs_flash_init()); }
    ESP_ERROR_CHECK(camera_service_init()); ESP_ERROR_CHECK(servo_init()); ESP_ERROR_CHECK(servo_set_angle(90));
    ESP_ERROR_CHECK(wifi_server_start()); ESP_LOGI("cam_app", "camera node ready");
}
