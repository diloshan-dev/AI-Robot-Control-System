#pragma once
#include "esp_err.h"
#include <stdbool.h>
typedef struct { int8_t x; int8_t y; uint8_t buttons; } home_input_t;
esp_err_t input_devices_init(void);
void input_devices_read(home_input_t *input);
bool input_devices_ptt_pressed(void);
