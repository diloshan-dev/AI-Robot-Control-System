#pragma once
#include "esp_err.h"
#include <stdbool.h>
typedef struct { int8_t x; int8_t y; uint8_t buttons; } remote_input_t;
esp_err_t remote_input_init(void);
void remote_input_read(remote_input_t *input);
bool remote_input_ptt(void);
