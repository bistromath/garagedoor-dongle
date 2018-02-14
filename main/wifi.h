#ifndef __WIFI_H__
#define __WIFI_H__

extern EventGroupHandle_t tcp_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define MESSAGE_SENT_BIT BIT1

void gd_wifi_init(void);

#endif //__WIFI_H__
