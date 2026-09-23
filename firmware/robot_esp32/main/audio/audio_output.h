#pragma once

#include <stddef.h>
#include <stdint.h>

void audio_output_init(void);
void audio_output_play_pcm16(const uint8_t *data, size_t length, uint8_t volume_hint);
void audio_output_stop(void);
