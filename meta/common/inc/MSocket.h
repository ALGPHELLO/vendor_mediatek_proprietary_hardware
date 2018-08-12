#ifndef _METASOCKET_H_
#define _METASOCKET_H_

typedef enum
{
	SOCKET_MDLOGGER  = 0,
	SOCKET_MOBILELOG = 1,
	SOCKET_ATCI = 2,
	SOCKET_AT_ATCI = 3,
	SOCKET_END       = 4
}SOCKET_TYPE;



class MSocket
{

public:
	MSocket();
	MSocket(SOCKET_TYPE type);
	virtual ~MSocket(void);
	int init(const char * socket_name, int bListen=0);
	int init(const char * socket_name, int bListen, int server);
	int initReservedClient(const char * socket_name, int bListen);
	void deinit();
	int getSocketID() const
	{
	   return m_socketID;
	}
	void send_msg(const char *msg);

private:
	static void *wait_msg(void*);
	
public:	
	SOCKET_TYPE m_type;
	int m_socketID;
	int m_threadID;
	int m_stop;
	pthread_t  m_thread;
};

#endif


