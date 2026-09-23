#pragma once
#include "esp_err.h"
#include "remote_protocol.h"
esp_err_t remote_link_init(void);
esp_err_t remote_link_send(const remote_packet_t *packet);
