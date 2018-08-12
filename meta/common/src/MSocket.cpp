#include <string.h>
#include <cutils/sockets.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

#include <errno.h>
#include "Context.h"
#include "LogDefine.h"
#include "MSocket.h"

MSocket::MSocket()
{
	m_socketID = -1;
	m_threadID = -1;
	m_stop = 0;
	memset(&m_thread, 0, sizeof(pthread_t));
	m_type = SOCKET_END;
	signal(SIGPIPE,SIG_IGN);
}

MSocket::MSocket(SOCKET_TYPE type)
{
	m_socketID = -1;
	m_threadID = -1;
	m_stop = 0;
	memset(&m_thread, 0, sizeof(pthread_t));
	m_type = type;

	signal(SIGPIPE,SIG_IGN);
}


MSocket::~MSocket(void)
{
	deinit();
}

int MSocket::init(const char * socket_name, int bListen, int server)
{
	if(server == 1)
	{
		META_LOG("[META][Socket] Creat Socket Server:(%s)",socket_name);
	}
	else
	{
		META_LOG("[META][Socket] Creat Socket Client:(%s)",socket_name);
		return init(socket_name,bListen);
	}
	
	m_socketID = socket_local_server(socket_name, ANDROID_SOCKET_NAMESPACE_RESERVED, SOCK_STREAM);
	
	META_LOG("[Meta][Socket] m_socketID = %d errno = %d", m_socketID,errno);	

	listen(m_socketID,4);

	while((m_socketID = accept(m_socketID,NULL,NULL))>0)
	{
		break;
		META_LOG("Meta][Socket]Accept Connection");
	}

	META_LOG("Meta][Socket]Meta Should Block Heer!");

	if(bListen)
	{
		m_threadID = pthread_create(&m_thread, NULL, wait_msg,  this);
		if(m_threadID)
		{
			META_LOG("[Meta][Socket] Failed to create socket thread!");
			return 0;
		}
	}
	
	return 1;	
		
}

int MSocket::initReservedClient(const char * socket_name, int bListen)
{
	int count = 0;
	META_LOG("[Meta][Socket] To connect server:(%s)", socket_name);
	while(m_socketID < 0) 
	{
		count++;
		m_socketID = socket_local_client(socket_name, ANDROID_SOCKET_NAMESPACE_RESERVED, SOCK_STREAM);
		META_LOG("[Meta][Socket] m_socketID = %d", m_socketID);
        META_LOG("[Meta][Socket] errno = %d, string = %s", errno, strerror(errno));
		usleep(200*1000);
		if(count == 5)
			return 0;		
	}

	META_LOG("[Meta][Socket] connect successful");
	//if bListen is true, we will create thread to read socket data.
	if(bListen)
	{
		m_threadID = pthread_create(&m_thread, NULL, wait_msg,  this);
		if(m_threadID)
		{
			META_LOG("[Meta][Socket] Failed to create socket thread!");
			return 0;
		}
	}
	
	return 1;

}

int MSocket::init(const char * socket_name, int bListen)
{
	int count = 0;
	int val = 0;
	
	META_LOG("[Meta][Socket] To connect server:(%s)", socket_name);
	while(m_socketID < 0) 
	{
		count++;
		m_socketID = socket_local_client(socket_name, ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_STREAM);
		META_LOG("[Meta][Socket] m_socketID = %d", m_socketID);
        META_LOG("[Meta][Socket] errno = %d, string = %s", errno, strerror(errno));
		usleep(200*1000);
		if(count == 5)
			return 0;		
	}

	META_LOG("[Meta][Socket] connect successful");
	//if bListen is true, we will create thread to read socket data.
	if(bListen)
	{
		m_threadID = pthread_create(&m_thread, NULL, wait_msg,  this);
		if(m_threadID)
		{
			META_LOG("[Meta][Socket] Failed to create socket thread!");
			return 0;
		}
	}
	
	if(0 == setsockopt(m_socketID, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val)))
	{
		META_LOG("[Meta][Socket] set socket to option to TCP_NODELAY!");
	}
	
	return 1;
}

void MSocket::deinit()
{
	if(m_threadID == 0)
	{
		m_stop = 1;
		pthread_join(m_thread, NULL);
	}

    if (m_socketID > 0)
    {
       	close (m_socketID);
        m_socketID = -1;
    }
}

void MSocket::send_msg(const char *msg)
{
	int nWritten = 0;

	META_LOG("[Meta][Socket] send mssage (%s) - socket id = %d", msg,  m_socketID);
	if((nWritten = write(m_socketID, msg, strlen(msg))) < 0)
	{
		META_LOG("[Meta][Socket] socket write error: %s", strerror(errno));
	}
	else
	{
		META_LOG("[Meta][Socket] write %d Bytes, total = %zd", nWritten, strlen(msg));
	}
}

void *MSocket::wait_msg(void *p)
{
	const char *msg = "calibration";
	const char *msg_ok = "OK";
	if(p == NULL)
	{
		META_LOG("[Meta][Socket] socket thread parameter error!");
		return NULL;
	}
	
	MSocket *pSocket = (MSocket *)p;
	char data[32] = {0};
	int ret = 0;

	META_LOG("[Meta][Socket] pSocket->m_socketID = %d", pSocket->m_socketID);

	while(pSocket->m_stop == 0)
	{
		ret = read(pSocket->m_socketID, data, 31);
		if(ret >0)
		{
			META_LOG("[Meta][Socket] data len = %d, rawdata = %s!", ret, data);
			char *pos = strstr(data, msg);
			if(pos != NULL)
			{
				createSerPortThread();
				createAllModemThread();
				continue;
			}
			pos = strstr(data, msg_ok);
			if(pos != NULL)
			{
				META_LOG("[Meta][Socket][DEBUG] got OK from modem");
				if(getATRespFlag()==1)
				{
					META_LOG("[Meta][Socket][DEBUG] setATRespFlag to 0");
					setATRespFlag(0);
				}
				continue;
			}
			
			setATRespFlag(-1);
		}
		else
		{
		
			usleep(100000); // wake up every 0.1sec   
		}		
	}
	return NULL;
}


