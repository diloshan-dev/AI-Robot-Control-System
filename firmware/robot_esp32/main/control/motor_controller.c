#include "motor_controller.h"

#include <stdlib.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "network/robot_transport.h"
#include "robot_config.h"
#include "safety/safety_monitor.h"

static const char *TAG = "motor";

static void set_pwm_channel(ledc_channel_config_t *channel, int gpio, int timer, int channel_id)
{
    channel->gpio_num = gpio;
    channel->speed_mode = LEDC_LOW_SPEED_MODE;
    channel->channel = channel_id;
    channel->timer_sel = timer;
    channel->duty = 0;
    channel->hpoint = 0;
    channel->intr_type = LEDC_INTR_DISABLE;
}

void motor_controller_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << ROBOT_MOTOR_LEFT_FORWARD) |
                       (1ULL << ROBOT_MOTOR_LEFT_REVERSE) |
                       (1ULL << ROBOT_MOTOR_RIGHT_FORWARD) |
                       (1ULL << ROBOT_MOTOR_RIGHT_REVERSE),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&io_conf));

    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 20000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    ledc_channel_config_t left_pwm = {0};
    ledc_channel_config_t right_pwm = {0};
    set_pwm_channel(&left_pwm, ROBOT_MOTOR_PWM_LEFT, LEDC_TIMER_0, LEDC_CHANNEL_0);
    set_pwm_channel(&right_pwm, ROBOT_MOTOR_PWM_RIGHT, LEDC_TIMER_0, LEDC_CHANNEL_1);

    ESP_ERROR_CHECK(ledc_channel_config(&left_pwm));
    ESP_ERROR_CHECK(ledc_channel_config(&right_pwm));

    motor_controller_stop();
    ESP_LOGI(TAG, "Motor controller initialized");
}

void motor_controller_stop(void)
{
    gpio_set_level(ROBOT_MOTOR_LEFT_FORWARD, 0);
    gpio_set_level(ROBOT_MOTOR_LEFT_REVERSE, 0);
    gpio_set_level(ROBOT_MOTOR_RIGHT_FORWARD, 0);
    gpio_set_level(ROBOT_MOTOR_RIGHT_REVERSE, 0);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
}

void motor_controller_set_motion(int left_percent, int right_percent)
{
    int left_clamped = left_percent;
    int right_clamped = right_percent;

    if (left_clamped > 100) left_clamped = 100;
    if (left_clamped < -100) left_clamped = -100;
    if (right_clamped > 100) right_clamped = 100;
    if (right_clamped < -100) right_clamped = -100;

    if (safety_monitor_is_emergency_stop_active()) {
        motor_controller_stop();
        return;
    }

    int left_duty = abs(left_clamped) * 255 / 100;
    int right_duty = abs(right_clamped) * 255 / 100;

    gpio_set_level(ROBOT_MOTOR_LEFT_FORWARD, left_clamped > 0 ? 1 : 0);
    gpio_set_level(ROBOT_MOTOR_LEFT_REVERSE, left_clamped < 0 ? 1 : 0);
    gpio_set_level(ROBOT_MOTOR_RIGHT_FORWARD, right_clamped > 0 ? 1 : 0);
    gpio_set_level(ROBOT_MOTOR_RIGHT_REVERSE, right_clamped < 0 ? 1 : 0);

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, left_duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, right_duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
}

void motor_control_task(void *arg)
{
    (void)arg;
    robot_command_message_t msg = {0};

    while (1) {
        if (xQueueReceive(robot_command_queue(), &msg, portMAX_DELAY) == pdTRUE) {
            switch (msg.type) {
                case ROBOT_CMD_STOP:
                    motor_controller_stop();
                    break;
                case ROBOT_CMD_FORWARD:
                    motor_controller_set_motion(msg.speed, msg.speed);
                    break;
                case ROBOT_CMD_BACKWARD:
                    motor_controller_set_motion(-msg.speed, -msg.speed);
                    break;
                case ROBOT_CMD_LEFT:
                    motor_controller_set_motion(-msg.speed, msg.speed);
                    break;
                case ROBOT_CMD_RIGHT:
                    motor_controller_set_motion(msg.speed, -msg.speed);
                    break;
                default:
                    motor_controller_stop();
                    break;
            }
        }
    }
}
