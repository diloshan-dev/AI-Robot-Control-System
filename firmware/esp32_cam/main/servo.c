#include "servo.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_log.h"
#include "sdkconfig.h"

esp_err_t servo_init(void)
{
    ESP_RETURN_ON_ERROR(ledc_timer_config(&(ledc_timer_config_t){.speed_mode=LEDC_LOW_SPEED_MODE,.timer_num=LEDC_TIMER_1,.duty_resolution=LEDC_TIMER_13_BIT,.freq_hz=50,.clk_cfg=LEDC_AUTO_CLK}), "servo", "timer");
    return ledc_channel_config(&(ledc_channel_config_t){.gpio_num=CONFIG_CAM_SERVO_GPIO,.speed_mode=LEDC_LOW_SPEED_MODE,.channel=LEDC_CHANNEL_1,.timer_sel=LEDC_TIMER_1,.duty=0,.hpoint=0});
}
esp_err_t servo_set_angle(uint8_t angle)
{
    if (angle > 180) return ESP_ERR_INVALID_ARG;
    uint32_t duty = 410 + ((uint32_t)angle * 1638U / 180U);
    return ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty, 0);
}
