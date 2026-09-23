idf_component_register(
    SRCS "app_main.c" "pc_link.c" "input_devices.c" "audio_packetizer.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_event esp_netif nvs_flash esp_adc driver esp_http_client
)
