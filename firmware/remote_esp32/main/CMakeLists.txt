idf_component_register(
    SRCS "app_main.c" "remote_link.c" "remote_input.c" "remote_audio.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_event esp_netif nvs_flash esp_adc driver
)
