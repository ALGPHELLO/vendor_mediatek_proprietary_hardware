
#ifndef _PORTINTERFACE_H_
#define _PORTINTERFACE_H_

#include "MetaPub.h"

class SerPort;

void destroyPortHandle();
META_COM_TYPE getComType();
void setComType(META_COM_TYPE comType);
SerPort * createSerPort();
SerPort * getSerPort();
void querySerPortStatus();
int queryPortTypeChange();
int queryModemModeChange();
void destroySerPort();


#endif	// _PORTINTERFACE_H_


