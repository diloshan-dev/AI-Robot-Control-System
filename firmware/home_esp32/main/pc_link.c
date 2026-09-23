#include "pc_link.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"
#include "sdkconfig.h"
#include "string.h"
#include "stdio.h"

static const char *TAG = "home_pc";
static EventGroupHandle_t wifi_events;
static home_command_handler_t command_handler;
static char response[512];
static size_t response_length;
#define WIFI_READY BIT0

static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) esp_wifi_connect();
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) esp_wifi_connect();
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) xEventGroupSetBits(wifi_events, WIFI_READY);
}
static esp_err_t response_cb(esp_http_client_event_t *event)
{
    if (event->event_id == HTTP_EVENT_ON_DATA && response_length + event->data_len < sizeof(response)) {
        memcpy(response + response_length, event->data, event->data_len);
        response_length += event->data_len; response[response_length] = '\0';
    }
    return ESP_OK;
}
static void dispatch_commands(void)
{
    const char *keys[] = {"relay", "light"};
    for (size_t i = 0; i < 2; ++i) {
        char needle[32]; snprintf(needle, sizeof(needle), "\"%s\"", keys[i]);
        char *p = strstr(response, needle);
        if (p) {
            p = strchr(p + strlen(needle), ':');
            if (p) {
                do { ++p; } while (*p == ' ' || *p == '\t');
                if (command_handler && (!strncmp(p, "true", 4) || !strncmp(p, "false", 5)))
                    command_handler(keys[i], strncmp(p, "true", 4) == 0);
            }
        }
    }
}
esp_err_t pc_link_init(home_command_handler_t handler)
{
    command_handler = handler; wifi_events = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init()); ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT(); ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event, NULL));
    wifi_config_t config = {0};
    strncpy((char *)config.sta.ssid, CONFIG_HOME_WIFI_SSID, sizeof(config.sta.ssid));
    strncpy((char *)config.sta.password, CONFIG_HOME_WIFI_PASSWORD, sizeof(config.sta.password));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA)); ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &config)); ESP_ERROR_CHECK(esp_wifi_start());
    xEventGroupWaitBits(wifi_events, WIFI_READY, pdFALSE, pdTRUE, portMAX_DELAY);
    return ESP_OK;
}
esp_err_t pc_link_send(const home_packet_t *packet, home_mode_t mode)
{
    char body[1800];
    int length = snprintf(body, sizeof(body),
        "{\"room\":\"%s\",\"device\":\"%s\",\"type\":\"%s\",\"x\":%d,\"y\":%d,\"buttons\":%u,\"mode\":\"%s\",\"audio_samples\":%u",
        CONFIG_HOME_ROOM_ID, CONFIG_HOME_DEVICE_ID, packet->type == 2 ? "audio" : "control",
        packet->x, packet->y, packet->buttons, mode == HOME_MODE_KARAOKE ? "karaoke" : "normal", packet->audio_len);
    if (packet->type == 2 && length > 0 && (size_t)length < sizeof(body) - 20) {
        length += snprintf(body + length, sizeof(body) - (size_t)length, ",\"samples\":[");
        for (uint16_t i = 0; i < packet->audio_len && i < 96 && (size_t)length < sizeof(body) - 12; ++i)
            length += snprintf(body + length, sizeof(body) - (size_t)length, "%s%d", i ? "," : "", packet->audio[i]);
        length += snprintf(body + length, sizeof(body) - (size_t)length, "]}");
    } else if (length > 0) {
        length += snprintf(body + length, sizeof(body) - (size_t)length, "}");
    }
    esp_http_client_config_t config = {.url=CONFIG_HOME_SERVER_URL, .method=HTTP_METHOD_POST, .event_handler=response_cb, .timeout_ms=1000};
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return ESP_ERR_NO_MEM;
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body, length);
    response_length = 0; esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK && esp_http_client_get_status_code(client) >= 200 && esp_http_client_get_status_code(client) < 300) dispatch_commands();
    else if (err != ESP_OK) ESP_LOGW(TAG, "PC server unavailable: %s", esp_err_to_name(err));
    esp_http_client_cleanup(client); return err;
}
