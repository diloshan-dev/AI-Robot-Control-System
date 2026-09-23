#include "robot_transport.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "transport";
static QueueHandle_t command_queue;

QueueHandle_t robot_command_queue(void)
{
    return command_queue;
}

void robot_transport_init(void)
{
    command_queue = xQueueCreate(8, sizeof(robot_command_message_t));
    if (command_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create robot command queue");
    }
}

void robot_transport_publish_command(robot_command_t type, int speed)
{
    if (command_queue == NULL) {
        return;
    }

    robot_command_message_t msg = {
        .type = type,
        .speed = speed,
    };

    xQueueSend(command_queue, &msg, portMAX_DELAY);
}

void robot_transport_task(void *arg)
{
    (void)arg;

    while (1) {
        robot_command_message_t msg = {0};

        if (xQueueReceive(command_queue, &msg, pdMS_TO_TICKS(200)) == pdTRUE) {
            ESP_LOGI(TAG, "Received command: %d speed=%d", msg.type, msg.speed);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
