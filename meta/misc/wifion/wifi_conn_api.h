#ifndef _WIFI_CONN_API_H_
#define _WIFI_CONN_API_H_

#define AP_SSID "oppometa"

#ifdef __cplusplus
extern "C" {

#endif

int get_rssi_init(void);
int get_rssi_deinit(void);

int get_rssi_connected(void);
int get_rssi_unconnected(void);


#ifdef __cplusplus
};
#endif


#endif
