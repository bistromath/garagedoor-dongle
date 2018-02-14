#include <sys/socket.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"

#include "wifi.h"

EventGroupHandle_t tcp_event_group;

static int server_socket = 0;
static struct sockaddr_in server_addr;
static struct sockaddr_in client_addr;
static unsigned int socklen = sizeof(client_addr);
static int connect_socket = 0;

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id) {
        case SYSTEM_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            esp_wifi_connect();
            xEventGroupClearBits(tcp_event_group, WIFI_CONNECTED_BIT);
            break;
        case SYSTEM_EVENT_STA_CONNECTED:
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            ESP_LOGI("wifi:", "got ip: %s\n",
                    ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
            xEventGroupSetBits(tcp_event_group, WIFI_CONNECTED_BIT);
            break;
        case SYSTEM_EVENT_AP_STACONNECTED:
            ESP_LOGI("wifi:", "station:"MACSTR" join,AID=%d\n",
                    MAC2STR(event->event_info.sta_connected.mac),
                    event->event_info.sta_connected.aid);
            xEventGroupSetBits(tcp_event_group, WIFI_CONNECTED_BIT);
            break;
        case SYSTEM_EVENT_AP_STADISCONNECTED:
            ESP_LOGI("wifi:", "station:"MACSTR"leave,AID=%d\n",
                    MAC2STR(event->event_info.sta_disconnected.mac),
                    event->event_info.sta_disconnected.aid);
            xEventGroupClearBits(tcp_event_group, WIFI_CONNECTED_BIT);
            break;
        default:
            break;
    }
    return ESP_OK;
}

void gd_wifi_init() {
    tcp_event_group = xEventGroupCreate();
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL) );

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_GARAGEDOOR_WIFI_SSID,
            .password = CONFIG_GARAGEDOOR_WIFI_PASSWORD
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI("wifi:", "wifi_init finished");
    ESP_LOGI("wifi:", "connect to AP SSID:%s password:%s\n",
            CONFIG_GARAGEDOOR_WIFI_SSID, CONFIG_GARAGEDOOR_WIFI_PASSWORD);
}
