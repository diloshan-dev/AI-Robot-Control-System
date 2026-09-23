#include "remote_link.h"
#include "remote_input.h"
#include "remote_audio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
static remote_mode_t mode=REMOTE_MODE_NORMAL;
static void remote_task(void *arg)
{
    remote_packet_t packet={.magic=REMOTE_PACKET_MAGIC}; remote_input_t input; uint8_t seq=0; int64_t last_send=0; uint8_t previous_buttons=0;
    for (;;) {
        remote_input_read(&input);
        if ((input.buttons & 2) && !(previous_buttons & 2)) mode=(mode==REMOTE_MODE_NORMAL)?REMOTE_MODE_KARAOKE:REMOTE_MODE_NORMAL;
        previous_buttons=input.buttons;
        packet.magic=REMOTE_PACKET_MAGIC; packet.seq=seq++; packet.type=REMOTE_DATA_CONTROL; packet.joystick_x=input.x; packet.joystick_y=(mode==REMOTE_MODE_KARAOKE)?0:input.y; packet.buttons=input.buttons; packet.mode=mode;
        if (remote_audio_packet(&packet)) packet.type=REMOTE_DATA_AUDIO;
        if (esp_timer_get_time()-last_send>500000) { remote_link_send(&packet); last_send=esp_timer_get_time(); }
        else remote_link_send(&packet);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
void app_main(void) { ESP_ERROR_CHECK(remote_input_init()); remote_audio_init(); ESP_ERROR_CHECK(remote_link_init()); xTaskCreate(remote_task,"remote",4096,NULL,5,NULL); ESP_LOGI("remote","remote controller ready; karaoke mode locks movement"); }
