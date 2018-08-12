#ifndef _WIFI_API_H_
#define _WIFI_API_H_

#ifdef __cplusplus
extern "C" {

#endif


int meta_open_wifi(void);

int meta_close_wifi(void);

/***************************************
meta_connect_wifi will execute /etc/wifi/meta_connect_wifi.sh

make sure 2 files in folder /etc/wifi
1. meta_connect_wifi.sh
2. meta_wpa_supplicant.conf 

a>how to set static ip?

open meta_connect_wifi.sh and modify below line. 
ifconfig wlan0 192.168.43.108 netmask 255.255.255.0

b>how to set ssid?

open meta_wpa_supplicant.conf and modify below line. 
ssid="oppometa"
***************************************/
int meta_connect_wifi(void);

/***************************************
meta_disconnect_wifi will execute /etc/wifi/meta_poll_disconnect_wifi.sh

make sure the following file in folder /etc/wifi:
meta_poll_disconnect_wifi.sh
***************************************/
int meta_disconnect_wifi(void);

int get_ip_address(char * ip, int *len);

#ifdef __cplusplus
};
#endif


#endif
