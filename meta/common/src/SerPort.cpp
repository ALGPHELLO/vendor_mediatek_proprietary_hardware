#include <stdlib.h>
#include <unistd.h>
#include <termios.h> 
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <cutils/properties.h>
#include <linux/ioctl.h>
#include <netinet/tcp.h>
#include <sys/mman.h>

#include "SerPort.h"
#include "LogDefine.h"
#include "wifi_api.h"
#include "graphics.h"
#include "Context.h"

extern "C" {
#include "hardware/ccci_intf.h"
}
static const int META_SOCKET_PORT = 9000;
static const int BACKLOG = 32;


//////////////////////////////////////////////////////////////////////////
CCCI::CCCI(const char *path)
{
	m_fd = open(path);
}

signed int CCCI::open(const char *path)
{
	int retry = 100;
	signed int fd = NULL_FILE_DESCRIPTOR;
	
	while(fd == NULL_FILE_DESCRIPTOR && retry != 0)
	{
		fd = ::open(path, O_RDWR|O_NOCTTY|O_NONBLOCK);
	    META_LOG("[Meta]Open modem. m_fd = %d", fd);
	    if (fd != NULL_FILE_DESCRIPTOR)
	    {
		    META_LOG("[Meta] Open modem port:(%s) success.", path);
			break;
	    }
	    else
	    {
		    META_LOG("[Meta] Open modem port:(%s) fail. errno = %d", path, errno);
			usleep(100*1000);
			retry--;
	    }
	}
	
	return fd;
}

signed int CCCI::read(unsigned char *buf, unsigned int len)
{
	int tmpLen = 0;
	while(1)
	{
		tmpLen = ::read(m_fd, buf, len);
		if(tmpLen <= 0)
		{
            if(errno == EAGAIN)
            {
                usleep(10*1000);
                continue;
            }
			//META_LOG("[META]read data error: fd = %d", m_fd);
            return -1;
		}
		return tmpLen;
	}
	//META_LOG("[Meta] read data from device: len =%d , m_fd = %d", tmpLen, m_fd);
}



signed int CCCI::write(const unsigned char *p, unsigned int len)
{
	int bytes_written = -1;
	int remain_size = len;
	pthread_mutex_lock(&(Device::m_wMutex));
	while(remain_size > 0)
	{
		bytes_written = ::write(m_fd, p, remain_size);
		if (bytes_written < 0)
		{
            if(errno == 11)  //modem is busy,AP send data too fast and modem is not able to process it, need to retry.
            {
                META_LOG("[Meta] Write data to CCCI device failed, modem is busy, retry to write, m_fd=%d", m_fd);
                usleep(50*1000);
                continue;
            }
            META_LOG("[Meta] Write data to CCCI device failed, return %d, errno=%d, m_fd=%d", bytes_written, errno, m_fd);
            pthread_mutex_unlock(&(Device::m_wMutex));
            return bytes_written;
		}
		else
		{
            META_LOG("[Meta] Write %d bytes to CCCI device: m_fd = %d, ", bytes_written, m_fd);
		}
		remain_size -= bytes_written;
		p += bytes_written;
	}
	pthread_mutex_unlock(&(Device::m_wMutex));
	return (len - remain_size);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CCB::CCB()
{
	init();
}

int CCB::init()
{
	
	while(0 == checkMdStatus())
	{
		META_LOG("[Meta] To check modem status");
		usleep(100*1000);
	}

	META_LOG("[Meta] To init CCB");
	// Initialize CCB related stuffs
    m_fd = ccci_ccb_register(MD_SYS1, USR_SMEM_CCB_META/*USR_SMEM_CCB_DHL*/);
    if (m_fd <= 0) {
        META_LOG("[Meta] ccb register failed, %d", m_fd);
        return 0;
    }

    int ccb_ready = 0;
    int retry = 0;
    // check whether CCB state is ready
    while ((ccb_ready = ccci_ccb_query_status()) != 0 && retry < META_CCB_INIT_MAX_RETRY) {
        META_LOG("[Meta] ccb not ready, ret %d, retry %d", ccb_ready, retry);
        usleep(100*1000);
        retry++;
    }
    
    if (retry == META_CCB_INIT_MAX_RETRY) {
        META_LOG("[Meta] ccb not retry after %d retries, ret %d", META_CCB_INIT_MAX_RETRY, ccb_ready);
        return 0;
    }
    META_LOG("[Meta] CCB is ready");

	return m_fd;

}

signed int CCB::read(unsigned char* buf, unsigned int len)
{
	META_LOG("[Meta] CCB to read data from device");
	
	int bitmask = ccci_ccb_poll(META_CCB_POOL_BITMASK);
	 if (bitmask < 0) {
        META_LOG("[Meta] CCB ccci_ccb_poll error, ret %d", bitmask);
        return -1;
    }

	unsigned char *ccb_data_buf = NULL;
    unsigned int ccb_data_len = 0; 
	len = 0;
	int ret = ccci_ccb_read_get(META_CCB_BUFFER_ID, &ccb_data_buf, &ccb_data_len);

	META_LOG("[Meta] CCB to read data, ret = %d, len = %d", ret, ccb_data_len);
	
	if ((ret < 0) || (ccb_data_len == 0))
		return 0;

	//add mux header
	buf[0] = 0xAC;
	buf[1] = 0xCA;
	buf[2] = 0x00;
	buf[3] = 0xFF;
	buf[4] = (ccb_data_len & 0x000000ff);
	buf[5] = (ccb_data_len >> 8) & 0x000000ff;

	//fill raw data- ccb buffer need 8byte align.
	ccb_data_copy(buf+META_CCB_MUX_HEADER_LEN, ccb_data_buf, ccb_data_len, ccb_data_buf);
		
	if ((ret = ccci_ccb_read_done(META_CCB_BUFFER_ID)) < 0) {
            META_LOG("[Meta] CCB ccb_read_done failed, ret %d", ret);
    }
		
	return ccb_data_len+META_CCB_MUX_HEADER_LEN;
}

signed int CCB::write(const unsigned char* buf, unsigned int len)
{
	META_LOG("[Meta] CCB: to call ccci_ccb_write_alloc");
		
	unsigned char *write_buf = NULL;

    int retry = 0;
    while ((write_buf = ccci_ccb_write_alloc(META_CCB_BUFFER_ID)) == NULL && retry < META_CCB_TX_MAX_RETRY)
	{
        META_LOG("[Meta] CCB ccb_buffer_write: cannot alloc ccb buf, retry %d", retry);
        usleep(10000);
        retry++;
    }

	META_LOG("[Meta] CCB: end call ccci_ccb_write_alloc");
	
	if (retry >= META_CCB_TX_MAX_RETRY) {
        META_LOG("[Meta] CCB ccb_buffer_write: cannot alloc ccb buf!");
        return 0;
    }

	//ccb buffer need 8byte align.
	ccb_data_copy(write_buf, (char*)buf, len, write_buf);

	META_LOG("[Meta] CCB: to call ccci_ccb_write_done");
	
	int ret = ccci_ccb_write_done(META_CCB_BUFFER_ID, write_buf, len);
    if (ret < 0)
	{
        META_LOG("[Meta] CCB ccb_buffer_write: ccb_write_done error, ret %d", ret);
		return 0;
    }

	META_LOG("[Meta] CCB Write %d bytes to device: m_fd = %d, ", len, m_fd);
		
	return len;
}

unsigned int CCB::checkMdStatus()
{
	char status[128]={0};
    property_get("mtk.md1.status",status, "0");

	META_LOG("[Meta] modem status = %s", status);
	if(0 ==	strcmp(status, "ready")) //ccb owner tell us to check this property.
		return 1;

	return 0;
}
void* CCB::ccb_memcpy(void *dst,void *src, size_t n) 
{
	long *p1 = (long*)dst;
	long *p2 = (long*)src;
    for (unsigned int idx = 0; idx < n/sizeof(long); idx++)
        *p1++ = *p2++;
	
    return dst;
}
void CCB::ccb_data_copy(void* dst, void* src, unsigned int length, void* alignment_addr)
{ 
	unsigned int i=0,c=(0x8-(((long)(alignment_addr))&0x7)); 
	
	for(; i<c && i<(unsigned int)length; i++) 
		*(((char *)(dst))+i) = *(((char *)(src))+i); 

	c = (length-i)&(~0x7); 
	ccb_memcpy(((char *)(dst))+i, ((char *)(src))+i, c); 

	for(i+=c; i<(unsigned int)length; i++) 
		*(((char *)(dst))+i) = *(((char *)(src))+i); 
}


//////////////////////////////////////////////////////////////////////////

SerPort::SerPort(const char *path)
{
	m_fd = open(path);	
}

SerPort::SerPort()
{
}


SerPort::~SerPort()
{
	if(m_fd > 0)
	{
		META_LOG("[Meta] Close serPort m_fd = %d", m_fd );
	    ::close(m_fd);
		m_fd = NULL_FILE_DESCRIPTOR;
	}
}

signed int SerPort::open(const char *path)
{
	signed int fd = ::open(path, O_RDWR|O_NOCTTY);

	META_LOG("[Meta] Open serPort. m_fd = %d", fd);

	if (fd != NULL_FILE_DESCRIPTOR)
	{
		META_LOG("[Meta] Open serport:(%s) success.", path);
		initTermIO(fd);
	}
	else
	{
		META_LOG("[Meta] Open serport:(%s) fail, error code = %d", path, errno);
	}
	
	return fd;
}

void SerPort::initTermIO(int portFd)
{
	struct termios termOptions;
	if (fcntl(portFd, F_SETFL, 0) == -1)
	{
	    META_LOG("[Meta] initTermIO call fcntl fail");
	}
	// Get the current options:
	tcgetattr(portFd, &termOptions);

	// Set 8bit data, No parity, stop 1 bit (8N1):
	termOptions.c_cflag &= ~PARENB;
	termOptions.c_cflag &= ~CSTOPB;
	termOptions.c_cflag &= ~CSIZE;
	termOptions.c_cflag |= CS8 | CLOCAL | CREAD;

	// Raw mode
	termOptions.c_iflag &= ~(INLCR | ICRNL | IXON | IXOFF | IXANY);
	termOptions.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);  /*raw input*/
	termOptions.c_oflag &= ~OPOST;  /*raw output*/


	tcflush(portFd,TCIFLUSH);//clear input buffer
	termOptions.c_cc[VTIME] = (1 == getNormalModeTestFlag())? 10:100; /* inter-character timer unused */
	termOptions.c_cc[VMIN] = 0; /* blocking read until 0 character arrives */


	cfsetispeed(&termOptions, B921600);
    cfsetospeed(&termOptions, B921600);
	/*
	* Set the new options for the port...
	*/
	tcsetattr(portFd, TCSANOW, &termOptions);
}

void SerPort::setSerPortExitFlag()
{
	//Do nothing here
}

//////////////////////////////////////////////////////////////////////////

UartPort::UartPort(const char *path)
	: SerPort(path)
{
}

//////////////////////////////////////////////////////////////////////////

UsbPort::UsbPort(const char *path)
	: SerPort(path)
{
	m_devPath = strdup(path);
	m_usbFlag = 1;
}

UsbPort::~UsbPort()
{
	// it'll never get here
	// so it doesn't make much sense...
	free((char*)m_devPath);
}

signed int UsbPort::read(unsigned char *buf, unsigned int len)
{
	// try to reopen USB if it was unplugged
/*	if (NULL_FILE_DESCRIPTOR == m_fd && !update())
	{
		return -1;
	}
*/
	
	signed int ret = SerPort::read(buf, len);

	// in case of error, see if USB is unplugged


	// it doesn't make sense to do PnP check if 'read' succeeds

	return ret;
}

signed int UsbPort::write(const unsigned char *buf, unsigned int len)
{
	// try to reopen USB if it was unplugged
	if (NULL_FILE_DESCRIPTOR == m_fd)	//&& !update())
	{
		return -1;
	}
	signed int ret = SerPort::write(buf, len);

	// it doesn't make sense to do PnP check if 'write' succeeds

	return ret;
}

void UsbPort::close()
{
	if (m_fd != NULL_FILE_DESCRIPTOR)
	{
		::close(m_fd);
		m_fd = NULL_FILE_DESCRIPTOR;
	}
}
int UsbPort::isReady() const
{
	int type = 0;
    char buf[11];
    int bytes_read = 0;
    int res = 0;
    int fd = ::open("/sys/class/android_usb/android0/state", O_RDONLY);
    if (fd != -1)
    {
        memset(buf, 0, 11);
        while (bytes_read < 10)
        {
            res = ::read(fd, buf + bytes_read, 10);
            if (res > 0)
                bytes_read += res;
            else
                break;
        }
        ::close(fd);
        type = strcmp(buf,"CONFIGURED");

        META_LOG("[Meta] Query usb state OK.");
    }
    else
    {
        META_LOG("[Meta] Failed to open:/sys/class/android_usb/android0/state");
    }
         
	return (type == 0);  
}

pthread_mutex_t META_USBPort_Mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t META_ComPortMD_Mutex = PTHREAD_MUTEX_INITIALIZER;

void UsbPort::update()
{
	if (!isReady())
	{
		if(m_usbFlag)
		{
			close();	
		}
		m_usbFlag = 0;
		META_LOG("[Meta] USB cable plus out!");
	}
	else
	{
		if(!m_usbFlag)
		{
			sleep(1);
			if (pthread_mutex_lock (&META_USBPort_Mutex))
			{
				META_LOG( "[Meta] META_MAIN META_USBPort_Mutex lock error!\n"); 
			}
			m_fd = open(m_devPath);

			if(m_fd != NULL_FILE_DESCRIPTOR)
			{
				m_usbFlag = 1;
			}

			if (pthread_mutex_unlock (&META_USBPort_Mutex))
			{
				META_LOG( "[Meta] META_Main META_USBPort_Mutex unlock error!\n"); 
			}
		}
	}

}

MetaSocket::MetaSocket(const char *path)
{
	m_fd = open(path);
	m_nClientFd = NULL_FILE_DESCRIPTOR;
	m_bConnect = false;
    m_nSocketConnectExitFlag = 0;
}

MetaSocket::~MetaSocket()
{
    close();
}

signed int MetaSocket::open(const char *path)
{
	int ret = -1;
	int sock_opt = 1;
	int enable = 1;
	path = NULL;
    int fd = NULL_FILE_DESCRIPTOR;
    char serverIP[16] = {0};
    int nPort = 0;
	int ipLength = 16;
	
    if(1 == getNormalModeTestFlag())
    {
		WIFI_PARA wifi_para = getWifiPara();
		strncpy(serverIP, wifi_para.ip_addr, sizeof(serverIP)-1);
		nPort = wifi_para.port;
		META_LOG("[Meta] Socket get server IP address:%s Port:%d", serverIP, nPort);
    }
    else
    {
#ifndef MTK_META_RSSITRIGGER_SUPPORT
		createDisplay();
#endif
		ret = meta_connect_wifi();
#ifndef MTK_META_RSSITRIGGER_SUPPORT
		//destoryDisplay();
		if(ret == 0)
		{
			setConnectedFlag();
		}
		else
		{
			destoryDisplay();
		}
#endif
		if(ret == 0)
		{
			META_LOG("[Meta] Socket Open connect wifi success.");
		}
		else
		{
			META_LOG("[Meta] Socket Open connect wifi fail.");
			display_string(COLOR_YELLOW, "WiFi network invalid in meta mode");
		}
		nPort = META_SOCKET_PORT;
		if(get_ip_address(serverIP, &ipLength) == 0)
		{
			META_LOG("[Meta] Socket get server IP address:%s  Port:%d success", serverIP, nPort);
		}
		else
		{
			META_LOG("[Meta] Socket get server IP address fail");
			return -1;
		}
		display_string(COLOR_BLUE, "WiFi network OK in meta mode, please connect");
	}

	//Create socket
    if((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        META_LOG("[META] Socket created fail. errno=%d", errno);
		return -1;
    }

    META_LOG("[META] Socket created success fd:%d",fd);

	// SET SOCKET REUSE Address
    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void*)&sock_opt, sizeof(sock_opt)) < 0)
    {
        META_LOG("[META] Socket setsockopt failed. errno=%d", errno);
		return -1;
    }
    // SET SOCKET RECEIVE TIMEOUT
    // Set socket to nonblock to avoid release problem
    struct timeval timeout = {1,0};
    if(setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeval)) < 0)
    {
        META_LOG("[META] Socket set receive timeout failed. errno=%d", errno);
		return -1;
    }
	//SET TCP_NODELAY
	if(setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void*)&enable, sizeof(enable)) < 0)
    {
        META_LOG("[META] Socket setsockopt TCP_NODELAY failed. errno=%d", errno);
    }
	//Prepare the sockaddr_in structure
	struct sockaddr_in* serverAddr = new struct sockaddr_in;
	if(serverAddr == NULL)
	{
	    META_LOG("[Meta] Socket new server addr failed. errno=%d", errno);
		::close(fd);
		fd = NULL_FILE_DESCRIPTOR;
	    return -1;
	}

	serverAddr->sin_family = AF_INET;
    serverAddr->sin_port = htons(nPort);
    serverAddr->sin_addr.s_addr = inet_addr(serverIP);

	//Bind
    if(bind(fd,(struct sockaddr*)serverAddr, sizeof(struct sockaddr)) < 0)
    {
        META_LOG("[META] Socket bind failed. errno=%d", errno);
        goto errout;
    }
    META_LOG("[META] Socket bind done");

    //Listen
    if (listen(fd, BACKLOG) == -1)
    {
        META_LOG("[META] Socket Failed to listen Socket port, errno=%d", errno);
        goto errout;
	}
	META_LOG("[META] Socket listen done");

	delete serverAddr;
	serverAddr = NULL;
	return fd;

errout:
	delete serverAddr;
	serverAddr = NULL;
	::close(fd);
	fd = NULL_FILE_DESCRIPTOR;
	return fd;
}

signed int MetaSocket::read(unsigned char *buf, unsigned int len)
	{

	if(!m_bConnect)
	{
	    if(connect() == -1)
	    {
	        META_LOG("[Meta] Socket::read connect fail");
			return -1;
	    }
	}
    
    if(m_nClientFd < 0)
    {
        return -1;
    }

	//META_LOG("[META] Socket enter read, connect success");
	int tmpLen = 0;
	tmpLen = recv(m_nClientFd, buf, len, 0);
	if(tmpLen == 0)
	{
	    META_LOG("[META] Socket recv data len is 0, network interrupt, need to reconnect - m_nClientFd = %d, len = %d", m_nClientFd, tmpLen);
	    disconnect();
	}
	else
	{
	    META_LOG("[META] Socket recv data from socket client - m_nClientFd = %d, len = %d", m_nClientFd, tmpLen);
	}
	return tmpLen;
}

signed int MetaSocket::write(const unsigned char *buf, unsigned int len)
{
    if (NULL_FILE_DESCRIPTOR == m_nClientFd)
	{
		return -1;
	}

	int bytes_written = -1;
	int remain_size = len;
	pthread_mutex_lock(&(Device::m_wMutex));
	//META_LOG("[Meta] Socket enter write");
	while(remain_size > 0)
    {
		bytes_written = send(m_nClientFd, buf, remain_size, 0);
		if (bytes_written < 0)
		{
		    META_LOG("[Meta] Socket write data by socket failed, return %d, errno=%d, m_nClientFd=%d", bytes_written, errno, m_nClientFd);
			pthread_mutex_unlock(&(Device::m_wMutex));
			return bytes_written;
		}
		else
		{
			META_LOG("[Meta] Socket write %d bytes by socket: m_nClientFd = %d, ", bytes_written, m_nClientFd);
		}
		remain_size -= bytes_written;
		buf += bytes_written;
	}
    pthread_mutex_unlock(&(Device::m_wMutex));
    return (len - remain_size);

}

void MetaSocket::close()
{
    disconnect();
	if (m_fd != NULL_FILE_DESCRIPTOR)
	{
		::close(m_fd);
		m_fd = NULL_FILE_DESCRIPTOR;
	}
	if(1 != getNormalModeTestFlag())
	{
		display_string(COLOR_WHITE, "USB MODE");
		meta_close_wifi();
		exit_display();
    }
}

signed int MetaSocket::connect()
{
    if (NULL_FILE_DESCRIPTOR == m_fd)
	{
		return -1;
	}

	if (m_bConnect)
	{
	    return 0;
	}

	struct sockaddr_in* clientAddr = new struct sockaddr_in;
	if (clientAddr == NULL)
	{
	    META_LOG("[Meta] Socket new client addr failed. errno=%d", errno);
		m_bConnect = false;
		return -1;
	}
	memset(clientAddr,0,sizeof(struct sockaddr_in));
	socklen_t alen = sizeof(struct sockaddr);
	
    META_LOG("[Meta] Socket connect, accept the connection");
	while(m_nSocketConnectExitFlag == 0)
	{    
	    if ((m_nClientFd = accept(m_fd, (struct sockaddr*)clientAddr, &alen)) == -1)
	    {
			if(errno == EAGAIN || errno == EINVAL)
			{
				usleep(200*1000);
		        continue;
			}
			META_LOG("Socket accept error, errno=%d", errno);
			
			m_bConnect = false;

            if(1 != getNormalModeTestFlag())
            {
			    display_string(COLOR_YELLOW, "WiFi connect fail in meta mode");
			}
			delete clientAddr;
			clientAddr = NULL;
			return -1;
	    }
	    else
	    {
			m_bConnect = true;
            if(1 != getNormalModeTestFlag())
            {
			    display_string(COLOR_BLUE, "WiFi connect OK in meta mode");
			}
		    META_LOG("[Meta] Socket connect, Received a connection from %s, m_nClientFd = %d",
				(char*)inet_ntoa(clientAddr->sin_addr), m_nClientFd);
			delete clientAddr;
			clientAddr = NULL;
			return 0;
	    }
	}
	delete clientAddr;
	clientAddr = NULL;
    return -1;
}

void MetaSocket::disconnect()
{
    if (m_bConnect)
    {
        if (m_nClientFd != NULL_FILE_DESCRIPTOR)
	    {
	        m_bConnect = false;
	        ::close(m_nClientFd);
	        m_nClientFd = NULL_FILE_DESCRIPTOR;
		}
    }
}

void MetaSocket::createDisplay()
{
    m_display = new Display();
	if (m_display != NULL)
	{
	    m_display ->pumpAsync();
	}
}


void MetaSocket::destoryDisplay()
{
    if(m_display != NULL)
	{
	    m_display->setExitFlag(1);
	    delete m_display;
		m_display = NULL;
	}
}

void MetaSocket::setConnectedFlag()
{
    if(m_display != NULL)
    {
        m_display->setConnectedFlag(1);
    }
}

void MetaSocket::setSerPortExitFlag()
{
    m_nSocketConnectExitFlag = 1;
}

