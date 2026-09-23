#pragma once
#include <stdbool.h>
#include "remote_protocol.h"
void remote_audio_init(void);
bool remote_audio_packet(remote_packet_t *packet);
