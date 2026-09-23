#pragma once

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef enum {
    ROBOT_CMD_STOP = 0,
    ROBOT_CMD_FORWARD,
    ROBOT_CMD_BACKWARD,
    ROBOT_CMD_LEFT,
    ROBOT_CMD_RIGHT,
} robot_command_t;

typedef struct {
    robot_command_t type;
    int speed;
} robot_command_message_t;

QueueHandle_t robot_command_queue(void);
void robot_transport_init(void);
void robot_transport_publish_command(robot_command_t type, int speed);
void robot_transport_publish_telemetry(float battery_pct, const char *event_name, const char *json_extra);
void robot_transport_task(void *arg);
