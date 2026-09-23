#include "safety_monitor.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "robot_config.h"

static const char *TAG = "safety";
static volatile bool emergency_stop = false;

void safety_monitor_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << ROBOT_EMERGENCY_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };

    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_LOGI(TAG, "Emergency stop input configured");
}

bool safety_monitor_is_emergency_stop_active(void)
{
    return emergency_stop || (gpio_get_level(ROBOT_EMERGENCY_GPIO) == 0);
}

void safety_monitor_task(void *arg)
{
    (void)arg;

    while (1) {
        if (gpio_get_level(ROBOT_EMERGENCY_GPIO) == 0) {
            emergency_stop = true;
            ESP_LOGW(TAG, "Emergency stop triggered. Motors disabled.");
        }

        if (emergency_stop) {
            ESP_LOGW(TAG, "Local safety override is active");
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
