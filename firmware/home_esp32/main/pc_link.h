#pragma once
#include "esp_err.h"
#include "home_protocol.h"
#include <stdbool.h>
typedef void (*home_command_handler_t)(const char *name, bool enabled);
esp_err_t pc_link_init(home_command_handler_t handler);
esp_err_t pc_link_send(const home_packet_t *packet, home_mode_t mode);
