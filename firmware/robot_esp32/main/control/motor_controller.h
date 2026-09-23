#pragma once

#include <stdbool.h>

void motor_controller_init(void);
void motor_controller_stop(void);
void motor_controller_set_motion(int left_percent, int right_percent);
void motor_control_task(void *arg);
