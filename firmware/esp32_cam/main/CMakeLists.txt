idf_component_register(
    SRCS "app_main.c" "camera_service.c" "servo.c" "wifi_server.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_event esp_netif nvs_flash esp_http_server esp32-camera driver json
)
