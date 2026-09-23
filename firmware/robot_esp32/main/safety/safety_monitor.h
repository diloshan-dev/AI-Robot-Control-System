#pragma once

#include <stdbool.h>

void safety_monitor_init(void);
bool safety_monitor_is_emergency_stop_active(void);
void safety_monitor_task(void *arg);
