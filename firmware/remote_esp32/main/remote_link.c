#include "remote_link.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "sdkconfig.h"
#include <string.h>
static const uint8_t robot_mac[6]={0xff,0xff,0xff,0xff,0xff,0xff};
esp_err_t remote_link_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init()); ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg=WIFI_INIT_CONFIG_DEFAULT(); ESP_ERROR_CHECK(esp_wifi_init(&cfg)); ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA)); ESP_ERROR_CHECK(esp_wifi_start()); ESP_ERROR_CHECK(esp_wifi_set_channel(CONFIG_REMOTE_ESPNOW_CHANNEL,WIFI_SECOND_CHAN_NONE)); ESP_ERROR_CHECK(esp_now_init());
    esp_now_peer_info_t peer={.channel=CONFIG_REMOTE_ESPNOW_CHANNEL,.ifidx=WIFI_IF_STA,.encrypt=false}; memcpy(peer.peer_addr,robot_mac,6); return esp_now_add_peer(&peer);
}
esp_err_t remote_link_send(const remote_packet_t *p) { return esp_now_send(robot_mac,(const uint8_t *)p,sizeof(*p)); }
