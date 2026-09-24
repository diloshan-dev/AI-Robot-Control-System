#include "remote_input.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#define X_CH ADC1_CHANNEL_6
#define Y_CH ADC1_CHANNEL_7
#define PTT_GPIO GPIO_NUM_4
#define BUTTON_GPIO GPIO_NUM_17
#define MODE_GPIO GPIO_NUM_16
esp_err_t remote_input_init(void) { adc1_config_width(ADC_WIDTH_BIT_12); adc1_config_channel_atten(X_CH,ADC_ATTEN_DB_11); adc1_config_channel_atten(Y_CH,ADC_ATTEN_DB_11); gpio_set_direction(PTT_GPIO,GPIO_MODE_INPUT); gpio_set_pull_up(PTT_GPIO,GPIO_PULLUP_ONLY); gpio_set_direction(BUTTON_GPIO,GPIO_MODE_INPUT); gpio_set_pull_up(BUTTON_GPIO,GPIO_PULLUP_ONLY); gpio_set_direction(MODE_GPIO,GPIO_MODE_INPUT); gpio_set_pull_up(MODE_GPIO,GPIO_PULLUP_ONLY); return ESP_OK; }
void remote_input_read(remote_input_t *i) { int x=adc1_get_raw(X_CH),y=adc1_get_raw(Y_CH); i->x=(int8_t)((x-2048)*127/2048); i->y=(int8_t)((y-2048)*127/2048); i->buttons=(gpio_get_level(BUTTON_GPIO)==0?1:0)|(gpio_get_level(MODE_GPIO)==0?2:0); }
bool remote_input_ptt(void) { return gpio_get_level(PTT_GPIO)==0; }
