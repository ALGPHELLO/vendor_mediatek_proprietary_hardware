/*
 * lxScribo.h
 *
 *  Created on: Mar 14, 2012
 *      Author: nlv02095
 */

#ifndef LXSCRIBO_H_
#define LXSCRIBO_H_

#define LXSCRIBO_VERSION 	0.1
#define SERIALDEV 			"/dev/ttyUSB0" // "/dev/Scribo"
#define LXSOCKET 			"9887" 		// note: in ascii for api alignment with serial

int lxScriboRegister(char *dev);	// register target and return opened file desc.
int lxScriboGetFd(void);			// return active file desc.

int lxScriboVersion(int fd, char *buffer);
int lxScriboWrite(int fd, int size, unsigned char *buffer, unsigned int *pError);
int lxScriboWriteRead(int fd, int wsize, const unsigned char *wbuffer
										   , int rsize, unsigned char *rbuffer, unsigned int *pError);
int lxScriboPrintTargetRev(int fd);
int lxScriboSerialInit(char *dev);
int lxScriboSocketInit(char *dev);
void lxScriboSocketExit(void);
int lxScriboSetPin(int fd, int pin, int value);

int lxI2cInit(char *dev);
int lxI2cWrite(int fd, int size, unsigned char *buffer, unsigned int *pError);
int lxI2cWriteRead(int fd, int wsize, const unsigned char *wbuffer
										   , int rsize, unsigned char *rbuffer, unsigned int *pError);

int lxDummyInit(char *dev);
int lxDummyWrite(int fd, int size, unsigned char *buffer, unsigned int *pError);
int lxDummyWriteRead(int fd, int wsize, const unsigned char *wbuffer
										   , int rsize, unsigned char *rbuffer, unsigned int *pError);

int (*lxScriboInit)(char *dev);
int (*lxWrite)(int fd, int size, unsigned char *buffer, unsigned int *pError);
int (*lxWriteRead)(int fd, int wsize, const unsigned char *wbuffer
										   , int rsize, unsigned char *rbuffer, unsigned int *pError);

int lxScriboListenSocketInit(char *socketnr);
int lxScriboSetPin(int fd, int pin, int value);
int lxScriboGetPin(int fd, int pin);
int lxScriboVersion(int fd, char *buffer);
int lxScriboWrite(int fd, int size, uint8_t *buffer, uint32_t *pError);
int lxScriboWriteRead(int fd, int wsize, const uint8_t *wbuffer, int rsize,
		uint8_t *rbuffer, uint32_t *pError) ;
int lxScriboRegister(char *dev);
int lxScriboGetFd(void);
int lxScriboPrintTargetRev(int fd);
int lxScriboGetRev(int fd, char *str, int max);

extern int lxScribo_verbose;
extern int i2c_trace;
extern int NXP_I2C_verbose;

// for dummy
#define rprintf printf

//from gpio.h
#define I2CBUS 0
//from gui.h
/* IDs of menu items */
typedef enum
{
  ITEM_ID_VOLUME = 100,
  ITEM_ID_PRESET,
  ITEM_ID_EQUALIZER,

  ITEM_ID_STANDALONE,

  ITEM_ID_ENABLESCREENDUMP,
} menuElement_t;

int i2c_GetSpeed(int bus);
void i2c_SetSpeed(int bus, int bitrate);
int i2c_WriteRead(int bus, int addrWr, void* dataWr, int sizeWr, int* nWr, void* dataRd, int sizeRd, int* nRd);
_Bool i2c_Write(int bus, int addrWr, void* dataWr, int sizeWr);
_Bool i2c_Read(int bus, int addr, void* data, int size, int* nr);;
_Bool gui_GetValue(menuElement_t item, int* val);
_Bool gui_SetValue(menuElement_t item, int* val);

#endif /* LXSCRIBO_H_ */
