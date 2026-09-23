#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "control/motor_controller.h"
#include "audio/audio_output.h"
#include "network/robot_transport.h"
#include "safety/safety_monitor.h"

static const char *TAG = "robot_app";

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing Kaniye companion robot core...");

    safety_monitor_init();
    motor_controller_init();
    audio_output_init();
    robot_transport_init();

    xTaskCreatePinnedToCore(safety_monitor_task, "safety_monitor", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(motor_control_task, "motor_control", 4096, NULL, 4, NULL, 1);
    xTaskCreatePinnedToCore(robot_transport_task, "robot_transport", 4096, NULL, 3, NULL, 0);

    ESP_LOGI(TAG, "Robot core booted successfully. Local safety logic active.");
}
