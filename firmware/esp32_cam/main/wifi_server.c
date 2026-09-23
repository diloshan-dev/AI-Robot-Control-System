#include "wifi_server.h"
#include "camera_service.h"
#include "servo.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "cam_http";
static EventGroupHandle_t wifi_events;
#define WIFI_READY BIT0

static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) esp_wifi_connect();
    else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) esp_wifi_connect();
    else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) xEventGroupSetBits(wifi_events, WIFI_READY);
}
static esp_err_t snapshot(httpd_req_t *req)
{
    uint8_t *jpg = NULL; size_t len = 0;
    esp_err_t err = camera_service_capture_jpeg(&jpg, &len);
    if (err != ESP_OK) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "camera unavailable");
    httpd_resp_set_type(req, "image/jpeg");
    err = httpd_resp_send(req, (const char *)jpg, len);
    camera_service_release_jpeg(jpg);
    return err;
}
static esp_err_t servo(httpd_req_t *req)
{
    char query[32]; char value[8];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, "angle", value, sizeof(value)) != ESP_OK) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "angle required");
    char *end; long angle = strtol(value, &end, 10);
    if (*end || angle < 0 || angle > 180 || servo_set_angle((uint8_t)angle) != ESP_OK) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid angle");
    return httpd_resp_sendstr(req, "ok");
}
esp_err_t wifi_server_start(void)
{
    wifi_events = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init()); ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT(); ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event, NULL));
    wifi_config_t cfg = {.sta = {.threshold.authmode = WIFI_AUTH_WPA2_PSK}};
    strncpy((char *)cfg.sta.ssid, CONFIG_CAM_WIFI_SSID, sizeof(cfg.sta.ssid));
    strncpy((char *)cfg.sta.password, CONFIG_CAM_WIFI_PASSWORD, sizeof(cfg.sta.password));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA)); ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg)); ESP_ERROR_CHECK(esp_wifi_start());
    xEventGroupWaitBits(wifi_events, WIFI_READY, pdFALSE, pdTRUE, portMAX_DELAY);
    httpd_config_t hc = HTTPD_DEFAULT_CONFIG(); hc.server_port = CONFIG_CAM_HTTP_PORT;
    httpd_handle_t server; ESP_ERROR_CHECK(httpd_start(&server, &hc));
    httpd_uri_t snap = {.uri="/snapshot",.method=HTTP_GET,.handler=snapshot};
    httpd_uri_t move = {.uri="/servo",.method=HTTP_GET,.handler=servo};
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &snap)); ESP_ERROR_CHECK(httpd_register_uri_handler(server, &move));
    ESP_LOGI(TAG, "HTTP server ready"); return ESP_OK;
}
