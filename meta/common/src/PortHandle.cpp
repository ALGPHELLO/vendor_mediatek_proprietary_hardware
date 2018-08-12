#include <assert.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <cutils/properties.h>
#include <dirent.h>
#include <sys/poll.h>
#include <linux/input.h>

#include "PortHandle.h"
#include "PortInterface.h"
#include "LogDefine.h"
#include "SerPort.h"
#include "Context.h"
#include "wifi_api.h"
#include "graphics.h"

#if defined(MTK_TC1_FEATURE)
	#define DEV_USB_PATH	"/dev/ttyGS4"
#else
    #define DEV_USB_PATH    "/dev/ttyGS0"
#endif

#define COM_PORT_TYPE_FILE "/sys/bus/platform/drivers/meta_com_type_info/meta_com_type_info"
#define COM_PORT_TYPE_STR_LEN 1

#define UART_PORT_INFO_FILE "/sys/bus/platform/drivers/meta_uart_port_info/meta_uart_port_info"
#define UART_PORT_INFO_STR_LEN 1

#define MAX_DEVICES 32
#define MAX_LENGTH  1024


class PortHandle
{
private:
	PortHandle(void);
public:
	~PortHandle(void);

	static PortHandle *instance();
	SerPort * createPort();
	void destroyPort();
	SerPort * getPort() const;
	META_COM_TYPE getComType();
	void setComType(META_COM_TYPE comType);
	void querySerPortStatus();
	void FTMuxPrimitiveData(META_RX_DATA *pMuxBuf);
	int WriteDataToPC(void *Local_buf,unsigned short Local_len,void *Peer_buf,unsigned short Peer_len);
	int getMetaUartPort(void);
	void getMetaConnectType();
	int queryPortTypeChange();
	int waitKeyForWiFi();
	int ev_init(void);
	void ev_exit(void);
	int ev_get(struct input_event *ev, unsigned dont_wait);
	void destroy();
	int queryModemModeChange();

private:
	META_COM_TYPE		m_comType;
	SerPort *			m_serPort;
private:
	static PortHandle *	m_myInst;

	unsigned int ev_count;
	unsigned int ev_touch;
	struct pollfd ev_fds[MAX_DEVICES];

};

PortHandle *PortHandle::m_myInst = NULL;

PortHandle::PortHandle(void)
	: m_comType(META_UNKNOWN_COM),
	  m_serPort(NULL)
{
    ev_count = 0;
	ev_touch = 0;
	memset(&ev_fds, 0, sizeof(pollfd)*MAX_DEVICES);
}

PortHandle::~PortHandle(void)
{
	if (m_serPort != NULL)
	{
		delete m_serPort;
		m_serPort = NULL;
	}
}


PortHandle *PortHandle::instance()
{
	return (m_myInst==NULL) ? ((m_myInst=new PortHandle)) : m_myInst;
}

void PortHandle::destroy()
{
	delete m_myInst;
	m_myInst = NULL;
}

void PortHandle::destroyPort()
{
	if(m_serPort != NULL)
	{
		delete m_serPort;
		m_serPort = NULL;			
	}
}

SerPort * PortHandle::createPort()
{
    if (m_serPort != NULL)
    {
        assert(false); // repeated create
    }
    else
    {
        META_COM_TYPE eComType = getComType();
        if (eComType == META_USB_COM)
        {
            m_serPort = new UsbPort(DEV_USB_PATH);
        }
        else if (eComType == META_UART_COM)
        {
            char szDevUartPath[256] = {0};
            switch(getMetaUartPort())
            {
                case 1:      //UART1
                    strncpy(szDevUartPath, UART1_PATH, strlen(UART1_PATH));
                    break;
                case 2:      //UART2
                    strncpy(szDevUartPath, UART2_PATH, strlen(UART2_PATH));
                    break;
                case 3:      //UART3
                    strncpy(szDevUartPath, UART3_PATH, strlen(UART3_PATH));
                    break;
                case 4:      //UART4
                    strncpy(szDevUartPath, UART4_PATH, strlen(UART4_PATH));
                    break;
                default:     //default use UART1
                    strncpy(szDevUartPath, UART1_PATH, strlen(UART1_PATH));
                    break;
            }
            META_LOG("[Meta] uart port path: %s", szDevUartPath);
            m_serPort = new UartPort(szDevUartPath);
        }
        else if (eComType == META_SOCKET)
        {
            META_LOG("[Meta] eComType == META_SOCKET");
            char wcn_ready[128] = {0};
            //check WCN driver has ready
            int retry_prop = 20;
            while(retry_prop > 0)
            {
                property_get("service.wcn.formeta.ready", wcn_ready, "no");
                if(!strcmp(wcn_ready, "yes"))
                {
                    META_LOG("[Meta] createPort WCN driver ready");
                    break;;
                }
                else
                {
                    META_LOG("[Meta] createPort get service.wcn.driver.ready fail, retry_prop: %d", retry_prop);
                    usleep(100*1000);
                    retry_prop--;
                }
            }
            char tempPath[64] = {0}; //no use

            m_serPort = new MetaSocket(tempPath);

        }
    }

	assert(m_serPort != NULL);

	return m_serPort;
}

SerPort * PortHandle::getPort() const
{
	return m_serPort;
}

int readSys_int(char const * path)
{
    int fd;
    
    if (path == NULL)
        return -1;

    fd = open(path, O_RDONLY);
    if (fd >= 0) {
        char buffer[20];
        int amt = read(fd, buffer, sizeof(int));          
        close(fd);
        return amt == -1 ? -errno : atoi(buffer);
    }
    META_LOG("[Meta] write_int failed to open %s\n", path);
    return -errno;    
}

int getBootMode_local()
{
    int bootMode;

    bootMode = readSys_int(BOOTMODE_PATH);

    if(NORMAL_BOOT== bootMode)
        META_LOG("[Meta] Normal mode boot!");
    else if(META_BOOT== bootMode)
        META_LOG("[Meta] Meta mode boot!");
    else {
		META_LOG("[Meta] Not Support boot mode! BootMode=%d",bootMode);
        bootMode = -1;       
    }     
    return bootMode;   
}

void PortHandle::setComType(META_COM_TYPE comType)
{
	META_LOG("[META] setComType %d",comType);	
	m_comType = comType;		
}

META_COM_TYPE PortHandle::getComType()
{
	if (m_comType == META_UNKNOWN_COM)
	{
		if(NORMAL_BOOT == getBootMode_local())
		{
			getMetaConnectType();	
		}
		else
		{
			char buf[COM_PORT_TYPE_STR_LEN + 1];
			int bytes_read = 0;
			int res = 0;
			int fd = open(COM_PORT_TYPE_FILE, O_RDONLY);
			if (fd != -1)
			{
				memset(buf, 0, COM_PORT_TYPE_STR_LEN + 1);
				while (bytes_read < COM_PORT_TYPE_STR_LEN)
				{
					res = read(fd, buf + bytes_read, COM_PORT_TYPE_STR_LEN);
					if (res > 0)
						bytes_read += res;
					else
						break;
				}
			    close(fd);
			    m_comType = (META_COM_TYPE)atoi(buf);
			    if (m_comType == META_UNKNOWN_COM)
			    {
                     getMetaConnectType();
			    }
		    }
		    else     //if (fd != -1)
		    {
			    META_LOG("[Meta] Failed to open com port type file %s", COM_PORT_TYPE_FILE);
		    }
		    META_LOG("[Meta] com port type: %d", m_comType);
	    }
    }
	return m_comType;
}

void PortHandle::getMetaConnectType()
{
	char tempstr[128]={0};
	char crypto_state[128] = {0};
	char vold_decrypt[128] = {0};
	int retry = 20;

	//Before get property value, we need wait for data decryption ready.
	property_get("ro.crypto.state", crypto_state, "");
	if (!strcmp(crypto_state, "unencrypted") || !strcmp(crypto_state, "unsupported"))
	{
	     META_LOG("[Meta] getMetaConnectType data ready, unencrypted --");
         goto DATA_READY;
	}
	else if(!strcmp(crypto_state, "encrypted"))
	{
         while(retry > 0)
         {
             property_get("vold.decrypt", vold_decrypt, "");
			 if(!strcmp(vold_decrypt, "trigger_restart_framework"))
			 {
                 META_LOG("[Meta] getMetaConnectType data ready, retry: %d", retry);
                 goto DATA_READY;
             }
			 else
			 {
			     usleep(1000 * 1000);
			     retry--;
			 }
         }
		 META_LOG("[Meta] getMetaConnectType check data ready timeout, vold_decrypt: %s", vold_decrypt);
		 m_comType = META_UNKNOWN_COM;
		 return;
     }
	 else
	 {
	     META_LOG("[Meta] getMetaConnectType error crypto_state: %s", crypto_state);
		 m_comType = META_UNKNOWN_COM;
		 return;
	 }

DATA_READY:
    int ret = property_get("persist.meta.connecttype",tempstr,"unknown");
    META_LOG("[Meta] ret:%d, persist.meta.connecttype: %s", ret,tempstr);
	if (strcmp(tempstr,"uart") == 0)
	{
	    m_comType = META_UART_COM;  //UART
	}
	else if (strcmp(tempstr,"usb") == 0)
	{
	    m_comType = META_USB_COM;  //USB
	}
	else if (strcmp(tempstr,"wifi") == 0)
	{
	    m_comType = META_SOCKET;  //WiFi
	}

}

int PortHandle::waitKeyForWiFi()
{
    META_LOG("[Meta] waitKeyForWiFi ev_init ");
    ev_init();
    // wait for the next key event
    struct input_event ev;
    while(1)
	{
        ev_get(&ev, 0);
		META_LOG("[Meta] waitKeyForWiFi ev.type :%d, ev.code:%d, EV_KEY:%d, KEY_VOLUMEUP:%d",ev.type, ev.code,EV_KEY,KEY_VOLUMEUP);
        if (ev.type == EV_KEY && ev.code == KEY_VOLUMEUP)
		{
		    META_LOG("[Meta] detect EV_KEY KEY_VOLUMEUP ");
            break;
        }
    }
	ev_exit();
	return 1;
}


int PortHandle::ev_init(void)
{
    DIR *dir;
    struct dirent *de;
    char name[MAX_LENGTH];
    int fd;
	dir = opendir("/dev/input");
    if(dir != 0)
	{
        while((de = readdir(dir)))
		{
//            fprintf(stderr,"/dev/input/%s\n", de->d_name);
            if(strncmp(de->d_name,"event",5)) continue;
            fd = openat(dirfd(dir), de->d_name, O_RDONLY);
            if(fd < 0) continue;

            ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name);
			//LOGD(TAG "NAME = %s\n", name);
            if (!strncmp(name, "mtk-tpd", 7))
			{
                ev_touch = ev_count;
			}
            ev_fds[ev_count].fd = fd;
            ev_fds[ev_count].events = POLLIN;
            ev_count++;
            if(ev_count == MAX_DEVICES) break;
        }
        closedir(dir);
	}

    return 0;
}

void PortHandle::ev_exit(void)
{
    while (ev_count > 0) {
        close(ev_fds[--ev_count].fd);
    }
}

int PortHandle::ev_get(struct input_event *ev, unsigned dont_wait)
{
    int r;
    unsigned n;
    static unsigned idx = 0;

    do {
        r = poll(ev_fds, ev_count, dont_wait ? 0 : -1);

        if(r > 0) {
            n = idx;
            do {
                if(ev_fds[n].revents & POLLIN) {
                    r = read(ev_fds[n].fd, ev, sizeof(*ev));
                    if(r == sizeof(*ev)) {
                        idx = ((n+1)%ev_count);
                        return 0;
                    }
                }
                n = ((n+1)%ev_count);
            } while(n!=idx);
        }
    } while(dont_wait == 0);

    return -1;
}

void PortHandle::querySerPortStatus()
{
	/*
	if(queryPortTypeChange() == 1)
    {
		META_LOG("[Meta] Port type changed");
		return; 
	}
	if(queryModemModeChange() == 1)
	{
		META_LOG("[Meta] Modem mode changed");
		return; 
	}
	*/
    if (m_comType == META_USB_COM)
    {
        SerPort * pPort = getSerPort();
		if (pPort != NULL)
		{
		    pPort->update();
		}
    }
}

int PortHandle::getMetaUartPort(void)
{
    int nPort = 1;
    if (m_comType == META_UART_COM)
    {
	    char buf[UART_PORT_INFO_STR_LEN + 1] = {0};
	    int fd = open(UART_PORT_INFO_FILE, O_RDONLY);
	    if (fd != -1)
	    {
			if (read(fd, buf, sizeof(char)*COM_PORT_TYPE_STR_LEN) <= 0)
			{
			    META_LOG("[Meta] ERROR can not read meta uart port ");
		    }
			else
			{
			    nPort = atoi(buf);
			}
		    close(fd);

	    }
	    else
	    {
		    META_LOG("[Meta] Failed to open meta uart port file %s", UART_PORT_INFO_FILE);
	    }
	    META_LOG("[Meta] uart com port: %d", nPort);
    }
	else
	{
	    META_LOG("[Meta] com port type is not uart");
	}
	return nPort;
}

int PortHandle::queryPortTypeChange()
{
	if(1 == getNormalModeTestFlag())
	{
		char tempstr[128] = {0};
		int nComType = META_UNKNOWN_COM;
		property_get("persist.meta.connecttype",tempstr,"unknown");
		
		if (strcmp(tempstr,"uart") == 0)
		{
			return 0;
		}
		else if (strcmp(tempstr,"usb") == 0)
		{
			nComType = META_USB_COM;  //USB
		}
		else if (strcmp(tempstr,"wifi") == 0)
		{
			nComType = META_SOCKET;  //WiFi
		}
		if(m_comType != nComType) //Got command to switch connect type
		{
			META_LOG("[Meta] Change connect type from %d to %d", m_comType, nComType);
			destroyVirtualRxThread();
			usleep(100*1000); //sleep 100 ms
			setComType((META_COM_TYPE)nComType);
#ifndef MTK_ATM_METAWIFIONLY
			createVirtualRxThread();
#else
			if(META_SOCKET == nComType)
			{
				createVirtualRxThread();
			}
#endif
			return 1;
		}
    }
    return 0;
}

int PortHandle::queryModemModeChange()
{
	if((1 == getNormalModeTestFlag()) && (0 == getModemModeSwitching()))
	{
		char tempstr[128] = {0};
		property_get("persist.atm.mdmode",tempstr,"unknown");
        
        //META_LOG("[Meta] persist.atm.mdmode = %s, modem mode = %d", tempstr, getMDMode());
		
		if ((strcmp(tempstr,"normal") == 0) && (getMDMode() == 2))//normal= 1 meta=2
		{
			META_LOG("[Meta] persist.atm.mdmode = %s. switch modem from meta to normal mode", tempstr);
			setMDMode(1);
			ChangeModemMode(1); 
			return 1;
		}
		
		if ((strcmp(tempstr,"meta") == 0) && (getMDMode() == 1))//normal= 1 meta=2
		{
			META_LOG("[Meta] persist.atm.mdmode = %s. switch modem from normal to meta mode", tempstr);
			setMDMode(2);
			ChangeModemMode(2);
			return 1;
		}
    }
    return 0;
}

void PortHandle::FTMuxPrimitiveData(META_RX_DATA *pMuxBuf)
{
    /* This primitive is logged by TST */
    unsigned char *pTempBuf = NULL;
    unsigned char *pTempDstBuf = NULL;
    unsigned char *pMamptrBase = NULL;
    unsigned char *pDestptrBase = NULL;
    int iCheckNum = 0;
    int dest_index=0;
    unsigned char cCheckSum = 0;
    int cbWriten = 0;
    int cbTxBuffer = 0;
    int i=0;
	SerPort * pPort = getSerPort();

    if(pMuxBuf == NULL)
    {
        META_LOG("[Meta] (FTMuxPrimitiveData) Err: pMuxBuf is NULL");
        return;
    }

    cbTxBuffer = pMuxBuf->LocalLen + pMuxBuf->PeerLen + 9;
    if (cbTxBuffer>FRAME_MAX_LEN)
    {
        META_LOG("[Meta] (FTMuxPrimitiveData) error frame size is too big!! ");
        return;
    }
    else
        META_LOG("[Meta] (FTMuxPrimitiveData) Type = %d Local_len = %d, Peer_len = %d", pMuxBuf->eFrameType, pMuxBuf->LocalLen, pMuxBuf->PeerLen);

    //META_LOG("[Meta] (FTMuxPrimitiveData) total size = %d", cbTxBuffer);
    pMamptrBase = (unsigned char *)malloc(cbTxBuffer);

    if(pMamptrBase == NULL)
    {
        META_LOG("[Meta] (FTMuxPrimitiveData) Err: malloc pMamptrBase Fail");
        return;
    }
    pDestptrBase = (unsigned char *)malloc(FRAME_MAX_LEN);
    if(pDestptrBase == NULL)
    {
        META_LOG("[Meta] (FTMuxPrimitiveData) Err: malloc pDestptrBase Fail");
        free(pMamptrBase);
        return;
    }


    pTempDstBuf = pDestptrBase;
    pTempBuf = pMamptrBase;

    /* fill the frameheader */
    *pTempBuf++ = 0x55;
    *pTempBuf++=((pMuxBuf->LocalLen + pMuxBuf->PeerLen +5)&0xff00)>>8;
    *pTempBuf++= (pMuxBuf->LocalLen + pMuxBuf->PeerLen +5)&0xff;
    *pTempBuf++ = 0x60;

    /*fill the local and peer data u16Length and its data */
    *pTempBuf++ = ((pMuxBuf->LocalLen)&0xff); /// pMuxBuf->LocalLen ;
    *pTempBuf++ = ((pMuxBuf->LocalLen)&0xff00)>>8;
    *pTempBuf++ = (pMuxBuf->PeerLen )&0xff;   ///pMuxBuf->PeerLen ;
    *pTempBuf++ = ((pMuxBuf->PeerLen)&0xff00)>>8;

    memcpy((pTempBuf), pMuxBuf->pData, pMuxBuf->LocalLen + pMuxBuf->PeerLen);

    pTempBuf = pMamptrBase;

    /* 0x5a is start data, so we use 0x5a and 0x01 inidcate 0xa5, use 0x5a and 0x5a indicate 0x5a
    the escape is just for campatiable with feature phone */
    while (iCheckNum != (cbTxBuffer-1))
    {
        cCheckSum ^= *pTempBuf;
        *pTempDstBuf = *pTempBuf;
        iCheckNum++;

        if (*pTempBuf ==0xA5 )
        {
            *pTempDstBuf++ = 0x5A;
            *pTempDstBuf++ = 0x01;
            dest_index++;		//do the escape, dest_index should add for write to uart or usb
        }
        else if (*pTempBuf ==0x5A )
        {
            *pTempDstBuf++ = 0x5A;
            *pTempDstBuf++ = 0x5A;
            dest_index++;		//do the escape, dest_index should add for write to uart or usb
        }
        else
            pTempDstBuf++;

        dest_index++;
        pTempBuf++;
    }

    /* 0x5a is start data, so we use 0x5a and 0x01 inidcate 0xa5 for check sum, use 0x5a and 0x5a indicate 0x5a
    the escape is just for campatiable with feature phone */
    if ( cCheckSum ==0xA5 )
    {
        dest_index++;		//do the escape, dest_index should add for write to uart or usb
        //Wayne replace 2048 with MAX_TST_RECEIVE_BUFFER_LENGTH
        if ((dest_index) > FRAME_MAX_LEN)//2048)
        {
            META_LOG("[Meta] (FTMuxPrimitiveData) Data is too big: index = %d cbTxBuffer = %d ",dest_index, cbTxBuffer);
            goto TSTMuxError;
        }

        *pTempDstBuf++= 0x5A;
        *pTempDstBuf = 0x01;
    }
    else if ( cCheckSum ==0x5A )
    {
        dest_index++;		//do the escape, dest_index should add for write to uart or usb
        if ((dest_index) > FRAME_MAX_LEN)
        {
            META_LOG("[Meta] (FTMuxPrimitiveData) Data is too big: index = %d cbTxBuffer = %d ",dest_index, cbTxBuffer);
            goto TSTMuxError;
        }
        *pTempDstBuf++= 0x5A;
        *pTempDstBuf = 0x5A;
    }
    else
        *pTempDstBuf =(char )cCheckSum;

    dest_index++;

    //write to PC
    //cbWriten = write(getPort(), (void *)pDestptrBase, dest_index);

	pPort->write(pDestptrBase, dest_index);
    pTempDstBuf = pDestptrBase;

    META_LOG("[Meta] FTMuxPrimitiveData: %d  %d %d  cChecksum: %d ",cbWriten, cbTxBuffer, dest_index,cCheckSum);

    TSTMuxError:

    free(pMamptrBase);
    free(pDestptrBase);
}


int PortHandle::WriteDataToPC(void *Local_buf,unsigned short Local_len,void *Peer_buf,unsigned short Peer_len)
{
	META_RX_DATA metaRxData;
	memset(&metaRxData,0, sizeof(META_RX_DATA));
	unsigned int dataLen = Local_len+Peer_len+8+1;
	unsigned char *metaRxbuf = (unsigned char *)malloc(dataLen);
	memset(metaRxbuf,0, dataLen);
	unsigned char *cPeerbuf = &metaRxbuf[Local_len+8];

	metaRxData.eFrameType = AP_FRAME;
	metaRxData.pData = metaRxbuf;
	metaRxData.LocalLen = Local_len;
	metaRxData.PeerLen = Peer_len >0 ? Peer_len+8 : Peer_len;

    if (((Local_len + Peer_len)> FT_MAX_LEN)||(Peer_len >PEER_BUF_MAX_LEN))
    {
        META_LOG("[Meta] (WriteDataToPC) Err: Local_len = %hu, Peer_len = %hu", Local_len,Peer_len);
        return 0;
    }

    if ((Local_len == 0) && (Local_buf == NULL))
    {
        META_LOG("[Meta] (WriteDataToPC) Err: Local_len = %hu, Peer_len = %hu", Local_len,Peer_len);
        return 0;
    }

    // copy to the temp buffer, and send it to the tst task.
    memcpy(metaRxbuf, Local_buf, Local_len);
    if ((Peer_len >0)&&(Peer_buf !=NULL))
        memcpy(cPeerbuf, Peer_buf, Peer_len);

    FTMuxPrimitiveData(&metaRxData);

    return 1;
}


/////////////////////////////////////////////////////////////////////////////////

void destroyPortHandle()
{
	return PortHandle::instance()->destroy();
}

META_COM_TYPE getComType()
{
	return PortHandle::instance()->getComType();
}

SerPort * createSerPort()
{
	return PortHandle::instance()->createPort();
}

void destroySerPort()
{
	return PortHandle::instance()->destroyPort();
}

SerPort * getSerPort()
{
	return PortHandle::instance()->getPort();
}


void querySerPortStatus()
{
     return PortHandle::instance()->querySerPortStatus();
}

int queryPortTypeChange()
{
	return PortHandle::instance()->queryPortTypeChange();
}
int queryModemModeChange()
{
	return PortHandle::instance()->queryModemModeChange();
}

int WriteDataToPC(void *Local_buf,unsigned short Local_len,void *Peer_buf,unsigned short Peer_len)
{
	return PortHandle::instance()->WriteDataToPC(Local_buf,Local_len,Peer_buf,Peer_len);
}

void setComType(META_COM_TYPE comType)
{
	return 	PortHandle::instance()->setComType(comType);
}







