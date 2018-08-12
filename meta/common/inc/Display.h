#ifndef _DISPLAY_H_
#define _DISPLAY_H_

#include <pthread.h>


#define NULL_FILE_DESCRIPTOR (-1)	// 0 or -1 ???

class Display
{
public:
	Display(void);
	virtual ~Display(void);

public:
    void setExitFlag(unsigned int exitFlag);
    void setConnectedFlag(unsigned int connectedFlag);
	signed int pump();
	signed int pumpAsync();
	

private:
	static void *ThreadProc(void*);
	unsigned int m_exitFlag;
	unsigned int m_connectedFlag;
protected:
	pthread_t m_thread;
};

#endif	//_DISPLAY_H_