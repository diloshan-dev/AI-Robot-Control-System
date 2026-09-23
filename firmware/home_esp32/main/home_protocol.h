#pragma once
#include <stdint.h>
#define HOME_PACKET_MAGIC 0x4B4E
typedef enum { HOME_CMD_STOP, HOME_CMD_MOVE, HOME_CMD_RELAY, HOME_CMD_MODE } home_command_t;
typedef enum { HOME_MODE_NORMAL, HOME_MODE_KARAOKE } home_mode_t;
typedef struct __attribute__((packed)) {
    uint16_t magic; uint8_t type; uint8_t sequence; int8_t x; int8_t y;
    uint8_t buttons; uint8_t mode; uint8_t relay; uint16_t audio_len; int16_t audio[96];
} home_packet_t;
