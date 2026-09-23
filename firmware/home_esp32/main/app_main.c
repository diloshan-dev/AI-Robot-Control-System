#include "pc_link.h"
#include "input_devices.h"
#include "audio_packetizer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include <string.h>
static home_mode_t mode=HOME_MODE_NORMAL; static int64_t last_rx;
static void command_received(const char *name, bool enabled) {
    last_rx=esp_timer_get_time();
    if (!strcmp(name, "relay") || !strcmp(name, "light")) gpio_set_level(CONFIG_HOME_RELAY_GPIO, enabled ? 1 : 0);
}
static void control_task(void *arg)
{
    home_packet_t out={0}; home_input_t input; uint8_t seq=0;
    for (;;) {
        input_devices_read(&input); out.magic=HOME_PACKET_MAGIC; out.type=1; out.sequence=seq++; out.x=input.x; out.y=(mode==HOME_MODE_KARAOKE)?0:input.y; out.buttons=input.buttons; out.mode=mode;
        out.audio_len=0;
        if ((esp_timer_get_time()-last_rx)>((int64_t)CONFIG_HOME_COMMAND_TIMEOUT_MS*1000)) { out.x=0; out.y=0; out.buttons=0; }
        if (pc_link_send(&out, mode) != ESP_OK) {
            out.x=0; out.y=0; out.buttons=0;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
void app_main(void) { ESP_ERROR_CHECK(nvs_flash_init()); ESP_ERROR_CHECK(input_devices_init()); gpio_set_direction(CONFIG_HOME_RELAY_GPIO, GPIO_MODE_OUTPUT); gpio_set_level(CONFIG_HOME_RELAY_GPIO, 0); audio_packetizer_init(); ESP_ERROR_CHECK(pc_link_init(command_received)); last_rx=esp_timer_get_time(); xTaskCreate(control_task,"home_control",4096,NULL,5,NULL); ESP_LOGI("home","WiFi PC client ready; karaoke disables movement"); }
