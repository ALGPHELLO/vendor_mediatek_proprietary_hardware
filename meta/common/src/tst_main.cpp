#include <assert.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cutils/properties.h>

#include "Modem.h"
#include "SerPort.h"
#include "MSocket.h"
#include "UsbRxWatcher.h"
#include "Context.h"
#include "PortInterface.h"
#include "LogDefine.h"

int main(int argc, char** argv)
{
	META_LOG("[Meta] Enter meta_tst init flow!");

	int count = 0;
	char szVal[128] = {0};
	umask(007);

	UsbRxWatcher hostRx;
    setVirtualRxWatcher(&hostRx);

	SerPort *pPort = NULL;

	if(NORMAL_BOOT == getBootMode())
	{
		META_LOG("[Meta] is normal mode");
		
		queryNormalModeTestFlag();
		if(1 == getNormalModeTestFlag())
		{

			META_LOG("[Meta] To set sys.usb.config");
#ifndef MTK_ATM_METAWIFIONLY
			if(1 == getPropValue("sys.usb.configfs"))
			{
				property_set("sys.usb.config","atm_gs0gs3");
			}
			else
			{
				writePortIndex();
				property_set("sys.usb.config","mtp,adb,dual_acm");
			} 
	
#else
			if(1 == getPropValue("sys.usb.configfs"))
			{
				property_set("sys.usb.config","atm_gs0");
			}
			else
			{
				property_set("sys.usb.config","mtp,adb,acm");
			}
#endif
			sleep(5);
			META_LOG("[Meta] To set persist.meta.connecttype");
			property_set("persist.meta.connecttype", "usb");
			META_LOG("[Meta] To set persist.atm.mdmode");
			property_set("persist.atm.mdmode", "normal");
			setMDMode(1);
    	    
            //queryWifiPara(argc, argv);

			if(getModemHwVersion(0)==MODEM_6293)  //only support one 93 Modem
			{
				if(getLoadType()==2) //eng:1 user:2 user debug:????
				{
					META_LOG("[Meta] To set atci properties");
					//property_set("ctl.start","atci-daemon-u");	//boot up atci
					property_set("persist.service.atci.usermode", "1");
					property_set("persist.service.atci.autostart", "1");
				}
#ifdef ATM_PCSENDAT_SUPPORT
				META_LOG("[Meta] Define ATM_PCSENDAT_SUPPORT");

				MSocket *pSocket = getSocket(SOCKET_ATCI);
				if(pSocket == NULL)
				{
					pSocket = createSocket(SOCKET_ATCI);
					if(pSocket != NULL)
					{
						int bInit = pSocket->init("meta-atci", 1, 1);
						if(bInit == 0)
						{
							delSocket(SOCKET_ATCI);
						}
					}
				}
#else
                META_LOG("[Meta] Not Define ATM_PCSENDAT_SUPPORT");
                
                sleep(5);
	#ifndef MTK_ATM_METAWIFIONLY
                pPort = createSerPort();		
                if (pPort != NULL)
                {
	                pPort->pumpAsync(&hostRx);
				}
                else
                {
                    META_LOG("[Meta] Enter meta_tst init fail");
				}
	#endif
				//createAllModemThread();
#endif

			}
			else // Modem is not 93
			{			
            	sleep(5);
			
				pPort = createSerPort();
			
				if (pPort != NULL)
				{
					pPort->pumpAsync(&hostRx);
				}
				else
				{
					META_LOG("[Meta] Enter meta_tst normal mode init fail");
				}
			}
			
		}
		else
		{
			META_LOG("[Meta] Normal mode flag is not 1,exist!");
			return 0;
		}
	}
	else
	{

		META_LOG("[Meta] is meta mode");
		pPort = createSerPort();		
		if (pPort != NULL)
		{
			pPort->pumpAsync(&hostRx);
		}
		else
		{
			META_LOG("[Meta] Enter meta_tst init fail");
		}
		
		createAllModemThread();
	}

	while (1)
	{
		sleep(5);
		queryPortTypeChange();
		queryModemModeChange();
		querySerPortStatus();
	}

	// infinite loop above; it'll never get here...



	destroyContext();

	return 0;
}
