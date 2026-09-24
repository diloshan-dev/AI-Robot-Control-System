#include "suspension_controller.h"

#include <math.h>

#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "robot_config.h"

static const char *TAG = "suspension";
static int corner_angles[4] = {90, 90, 90, 90};
static bool auto_level_enabled = false;

#define SUSPENSION_CHANNEL_COUNT 4
#define SUSPENSION_PWM_FREQ 50
#define SUSPENSION_MIN_DEG 0
#define SUSPENSION_MAX_DEG 180

static const int PCA9685_SERVO_CHANNELS[4] = {0, 1, 2, 3};

static int clamp_angle(int angle)
{
    if (angle < SUSPENSION_MIN_DEG) return SUSPENSION_MIN_DEG;
    if (angle > SUSPENSION_MAX_DEG) return SUSPENSION_MAX_DEG;
    return angle;
}

static uint32_t angle_to_duty(int angle_deg)
{
    const int clamped = clamp_angle(angle_deg);
    const uint32_t min_duty = 500;
    const uint32_t max_duty = 2500;
    return min_duty + ((max_duty - min_duty) * (uint32_t)(clamped)) / 180;
}

static void apply_servo_angle(int corner, int angle_deg)
{
    if (corner < 0 || corner >= 4) {
        return;
    }
    corner_angles[corner] = clamp_angle(angle_deg);
    ESP_LOGD(TAG, "corner %d angle=%d", corner, corner_angles[corner]);
}

void suspension_controller_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_14_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = SUSPENSION_PWM_FREQ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    for (int i = 0; i < SUSPENSION_CHANNEL_COUNT; ++i) {
        ledc_channel_config_t ch = {
            .gpio_num = GPIO_NUM_32 + i,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = i,
            .timer_sel = LEDC_TIMER_0,
            .duty = 0,
            .hpoint = 0,
        };
        ESP_ERROR_CHECK(ledc_channel_config(&ch));
    }

    suspension_level_all(90);
    ESP_LOGI(TAG, "Suspension controller initialized (4 corners via LEDC channels)");
}

void suspension_set_corner(suspension_corner_id_t corner, int angle_deg)
{
    apply_servo_angle((int)corner, angle_deg);
    const int channel = PCA9685_SERVO_CHANNELS[corner];
    const uint32_t duty = angle_to_duty(corner_angles[corner]);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, channel);
}

void suspension_level_all(int height_deg)
{
    for (int i = 0; i < 4; ++i) {
        suspension_set_corner((suspension_corner_id_t)i, height_deg);
    }
}

void suspension_stand_tall(void)
{
    suspension_level_all(120);
}

void suspension_crouch(void)
{
    suspension_level_all(60);
}

void suspension_tilt_correct(float pitch_deg, float roll_deg)
{
    if (!auto_level_enabled) {
        return;
    }
    const float pitch_adj = pitch_deg * 0.7f;
    const float roll_adj = roll_deg * 0.7f;
    const int front = (int)(90 + pitch_adj + roll_adj);
    const int rear = (int)(90 - pitch_adj + roll_adj);
    const int left = (int)(90 + roll_adj - pitch_adj);
    const int right = (int)(90 - roll_adj - pitch_adj);

    suspension_set_corner(SUSPENSION_CORNER_FRONT_LEFT, front);
    suspension_set_corner(SUSPENSION_CORNER_FRONT_RIGHT, left);
    suspension_set_corner(SUSPENSION_CORNER_BACK_LEFT, rear);
    suspension_set_corner(SUSPENSION_CORNER_BACK_RIGHT, right);
}

void suspension_hold(void)
{
    for (int i = 0; i < 4; ++i) {
        suspension_set_corner((suspension_corner_id_t)i, corner_angles[i]);
    }
}

bool suspension_auto_level_enabled(void)
{
    return auto_level_enabled;
}

void suspension_auto_level_set(bool enabled)
{
    auto_level_enabled = enabled;
}
