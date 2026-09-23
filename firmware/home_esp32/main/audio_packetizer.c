#include "audio_packetizer.h"
#include "input_devices.h"
#include "driver/adc.h"
#include "sdkconfig.h"
void audio_packetizer_init(void) {}
bool audio_packetizer_fill(home_packet_t *p, uint8_t seq)
{
    if (!input_devices_ptt_pressed()) return false;
    p->magic=HOME_PACKET_MAGIC; p->type=2; p->sequence=seq; p->audio_len=96;
    for (int i=0;i<96;i++) { int raw=adc1_get_raw(ADC1_CHANNEL_6); p->audio[i]=(int16_t)((raw-2048)<<4); }
    return true;
}
