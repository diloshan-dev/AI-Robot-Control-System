#include "remote_audio.h"
#include "remote_input.h"
#include "driver/adc.h"
#define AUDIO_CH ADC1_CHANNEL_6
void remote_audio_init(void) {}
bool remote_audio_packet(remote_packet_t *p)
{
    if (!remote_input_ptt()) return false;
    p->type=REMOTE_DATA_AUDIO; p->samples=96;
    for (int i=0;i<96;i++) p->audio[i]=(int16_t)((adc1_get_raw(AUDIO_CH)-2048)<<4);
    return true;
}
