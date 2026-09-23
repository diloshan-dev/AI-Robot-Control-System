#pragma once
#include <stdint.h>
/*
 * Wire-compatible with home_esp32/home_protocol.h. The PC server is the
 * bridge between this remote (walkie/karaoke controller) and the robot.
 * Keep field order, packing, magic, type values, and 96-sample audio blocks
 * unchanged when extending the protocol.
 */
#define REMOTE_PACKET_MAGIC 0x4B4E
typedef enum { REMOTE_DATA_CONTROL=1, REMOTE_DATA_AUDIO=2, REMOTE_DATA_MODE=3 } remote_data_type_t;
typedef enum { REMOTE_MODE_NORMAL=0, REMOTE_MODE_KARAOKE=1 } remote_mode_t;
typedef struct __attribute__((packed)) { uint16_t magic; uint8_t type; uint8_t seq; int8_t joystick_x; int8_t joystick_y; uint8_t buttons; uint8_t mode; uint16_t samples; int16_t audio[96]; } remote_packet_t;
