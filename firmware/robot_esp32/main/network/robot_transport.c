#include "robot_transport.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_websocket_client.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

static const char *TAG = "transport";
static QueueHandle_t command_queue;
static esp_websocket_client_handle_t websocket_client = NULL;
static bool wifi_connected = false;
static int reconnect_attempt = 0;

static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)handler_args;
    (void)base;
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;

    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "WebSocket connected to robot server");
            reconnect_attempt = 0;
            break;
        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "WebSocket disconnected, will reconnect with backoff");
            break;
        case WEBSOCKET_EVENT_DATA:
            if (data && data->data_len > 0) {
                ESP_LOGI(TAG, "Received server message: %.*s", data->data_len, (char *)data->data_ptr);
            }
            break;
        default:
            break;
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_data;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "WiFi station starting");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_connected = false;
        ESP_LOGW(TAG, "WiFi disconnected, retrying connection");
        if (reconnect_attempt < 5) {
            reconnect_attempt++;
            vTaskDelay(pdMS_TO_TICKS(1000 * reconnect_attempt));
            esp_wifi_connect();
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "WiFi connected with IP: " IPSTR, IP2STR(&event->ip_info.ip));
        wifi_connected = true;
    }
}

static void connect_to_server(void)
{
    if (websocket_client != NULL) {
        esp_websocket_client_close(websocket_client, portMAX_DELAY);
        esp_websocket_client_destroy(websocket_client);
        websocket_client = NULL;
    }

    char uri[256];
    snprintf(uri, sizeof(uri), "ws://%s:%d/ws/robot", CONFIG_ROBOT_SERVER_HOST, CONFIG_ROBOT_SERVER_PORT);

    esp_websocket_client_config_t ws_cfg = {
        .uri = uri,
        .buffer_size = 4096,
        .pingpong_timeout_sec = 10,
        .reconnect_timeout_ms = 2000,
        .disable_auto_reconnect = false,
    };

    websocket_client = esp_websocket_client_init(&ws_cfg);
    esp_websocket_client_register_events(websocket_client, WEBSOCKET_EVENT_ANY, websocket_event_handler, NULL);
    esp_websocket_client_start(websocket_client);
}

QueueHandle_t robot_command_queue(void)
{
    return command_queue;
}

void robot_transport_init(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_t *netif = esp_netif_create_default_wifi_sta();
    if (netif == NULL) {
        ESP_LOGE(TAG, "Failed to create WiFi station netif");
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_cfg = {
        .sta = {
            .ssid = CONFIG_ROBOT_WIFI_SSID,
            .password = CONFIG_ROBOT_WIFI_PASSWORD,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    command_queue = xQueueCreate(8, sizeof(robot_command_message_t));
    if (command_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create robot command queue");
    }

    connect_to_server();
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

void robot_transport_publish_telemetry(float battery_pct, const char *event_name, const char *json_extra)
{
    if (websocket_client == NULL || !wifi_connected) {
        return;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "telemetry");
    cJSON_AddNumberToObject(root, "battery_pct", battery_pct);
    if (event_name != NULL) {
        cJSON_AddStringToObject(root, "event", event_name);
    }
    if (json_extra != NULL) {
        cJSON *extra = cJSON_Parse(json_extra);
        if (extra != NULL) {
            cJSON_AddItemToObject(root, "sensors", extra);
        }
    }

    char *serialized = cJSON_PrintUnformatted(root);
    if (serialized != NULL) {
        esp_websocket_client_send_text(websocket_client, serialized, strlen(serialized), portMAX_DELAY);
        free(serialized);
    }
    cJSON_Delete(root);
}

void robot_transport_task(void *arg)
{
    (void)arg;

    while (1) {
        robot_command_message_t msg = {0};

        if (xQueueReceive(command_queue, &msg, pdMS_TO_TICKS(200)) == pdTRUE) {
            ESP_LOGI(TAG, "Received command: %d speed=%d", msg.type, msg.speed);
            if (websocket_client != NULL && wifi_connected) {
                cJSON *root = cJSON_CreateObject();
                cJSON_AddStringToObject(root, "type", "command");
                cJSON_AddStringToObject(root, "action", msg.type == ROBOT_CMD_STOP ? "stop" : "move");
                cJSON *params = cJSON_CreateObject();
                if (msg.type == ROBOT_CMD_FORWARD) {
                    cJSON_AddStringToObject(params, "direction", "forward");
                } else if (msg.type == ROBOT_CMD_BACKWARD) {
                    cJSON_AddStringToObject(params, "direction", "backward");
                } else if (msg.type == ROBOT_CMD_LEFT) {
                    cJSON_AddStringToObject(params, "direction", "left");
                } else if (msg.type == ROBOT_CMD_RIGHT) {
                    cJSON_AddStringToObject(params, "direction", "right");
                }
                cJSON_AddNumberToObject(params, "speed", msg.speed);
                cJSON_AddItemToObject(root, "params", params);

                char *serialized = cJSON_PrintUnformatted(root);
                if (serialized != NULL) {
                    esp_websocket_client_send_text(websocket_client, serialized, strlen(serialized), portMAX_DELAY);
                    free(serialized);
                }
                cJSON_Delete(root);
            }
        }

        if (!wifi_connected && websocket_client != NULL) {
            ESP_LOGI(TAG, "WiFi disconnected, reconnecting WebSocket");
            connect_to_server();
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
