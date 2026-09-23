#pragma once
#include <stdbool.h>
#include "home_protocol.h"
void audio_packetizer_init(void);
bool audio_packetizer_fill(home_packet_t *packet, uint8_t sequence);
