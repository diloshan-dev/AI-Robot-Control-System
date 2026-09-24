#include "input_devices.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "sdkconfig.h"
#include <stdlib.h>
#define JOY_X GPIO_NUM_34
#define JOY_Y GPIO_NUM_35
#define BUTTON GPIO_NUM_17
#define MODE_BUTTON GPIO_NUM_16
#define PTT GPIO_NUM_4
esp_err_t input_devices_init(void) { gpio_set_direction(BUTTON, GPIO_MODE_INPUT); gpio_set_pull_up(BUTTON, GPIO_PULLUP_ONLY); gpio_set_direction(MODE_BUTTON, GPIO_MODE_INPUT); gpio_set_pull_up(MODE_BUTTON, GPIO_PULLUP_ONLY); gpio_set_direction(PTT, GPIO_MODE_INPUT); gpio_set_pull_up(PTT, GPIO_PULLUP_ONLY); adc1_config_width(ADC_WIDTH_BIT_12); adc1_config_channel_atten(CONFIG_HOME_PTT_ADC_CHANNEL, ADC_ATTEN_DB_11); return ESP_OK; }
void input_devices_read(home_input_t *i) { int x=0,y=0; adc1_get_raw(ADC1_CHANNEL_6,&x); adc1_get_raw(ADC1_CHANNEL_7,&y); i->x=(int8_t)((x-2048)*127/2048); i->y=(int8_t)((y-2048)*127/2048); i->buttons=(gpio_get_level(BUTTON)==0?1:0)|(gpio_get_level(MODE_BUTTON)==0?2:0); }
bool input_devices_ptt_pressed(void) { return gpio_get_level(PTT)==0; }
