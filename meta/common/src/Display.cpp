#include <string.h>
#include <cutils/properties.h>

#include "Display.h"
#include "LogDefine.h"
#include "graphics.h"
#include "wifi_conn_api.h"
#include "wifi_api.h"
#include "Context.h"


Display::Display(void)
	: m_exitFlag(0),m_connectedFlag(0)
{
	memset(&m_thread, 0, sizeof(pthread_t));
	init_display();
	get_rssi_init();
}

Display::~Display(void)
{
    get_rssi_deinit();
	pthread_join(m_thread, NULL);
}

signed int Display::pump()
{
    int rssi = -9999;
    char strRssi[128] = {0};

    while (m_exitFlag == 0)
    {
        if (m_connectedFlag != 1)
        {
            rssi = get_rssi_unconnected();
            if (rssi == -9999)
            {
                sprintf(strRssi,"WIFI is opening...");
            }
            else
            {
                sprintf(strRssi,"WIFI RSSI: %d",rssi);
            }
            display_string(COLOR_BLUE, strRssi);
            if (m_exitFlag == 0)
            {
                usleep(800*1000);
            }
        }
        else
        {
            //disconnect wifi if RSSI is low and power off device.
            meta_disconnect_wifi();
            if(META_BOOT == getBootMode())
            {
				//Only power off device in META mode
                property_set("sys.powerctl","reboot");
			}
            return 0;
        }
    }
    return 0;
}

void Display::setExitFlag(unsigned int exitFlag)
{
	m_exitFlag = exitFlag;	
}

void Display::setConnectedFlag(unsigned int connectedFlag)
{
    m_connectedFlag = connectedFlag;
}


signed int Display::pumpAsync()
{
    pthread_create(&m_thread, NULL, ThreadProc, this);
    return 0;
}

void *Display::ThreadProc(void *p)
{
    Display *inst = (Display*)p;
    inst->pump();
    return NULL;
}


