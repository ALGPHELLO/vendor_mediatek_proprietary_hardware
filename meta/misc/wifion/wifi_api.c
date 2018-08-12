#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <cutils/log.h>


#include "ifaddrs.h"


//#define LOGD	printf
//#define LOGE	printf

#define LOGE ALOGE
#define LOGD ALOGD


#define TAG "[WIFI-META]"
#define META_CONNECT_WIFI 	"/system/etc/wifi/meta_connect_wifi.sh"
#define META_DISCONNECT_WIFI 	"/system/etc/wifi/meta_poll_disconnect_wifi.sh"
#define WIFI_POWER_PATH		"/dev/wmtWifi"


static int wifi_set_power(int on) 
{
    int sz;
    int fd = -1;
    int ret = -1;
    const char buffer = (on ? '1' : '0');
    	
    LOGD(TAG"wifi_set_power, %d",on);

    fd = open(WIFI_POWER_PATH, O_WRONLY);
    LOGD(TAG"wifi_set_power,%s", WIFI_POWER_PATH);    
    if (fd < 0) {
        LOGE(TAG"open(%s) for write failed: %s (%d)", WIFI_POWER_PATH,
             strerror(errno), errno);
        goto out;
    }
    sz = write(fd, &buffer, 1);
    if (sz < 0) {
        LOGE(TAG"write(%s) failed: %s (%d)", WIFI_POWER_PATH, strerror(errno),
             errno);
        goto out;
    }
    ret = 0;

out:
    if (fd >= 0) close(fd);
    return ret;
}

int meta_open_wifi(void){
	return wifi_set_power(1);
}

int meta_close_wifi(void){
	return wifi_set_power(0);
}


int meta_connect_wifi(void){
	pid_t pid;
	int err;
	int child_stat = 0;
	
	if ((pid = fork()) < 0) 
	{
		LOGD(TAG"fork fails: %d (%s)\n", errno, strerror(errno));
		return (-1);
	} 
	else if (pid == 0)  /*child process*/
	{

		LOGD(TAG"child process %s %s before\n","/system/bin/sh",META_CONNECT_WIFI);
		err = execl("/system/bin/sh","sh",META_CONNECT_WIFI,(char *)NULL);

		LOGD(TAG"child process %s %s after\n","/system/bin/sh",META_CONNECT_WIFI);
		exit(-2);
	} 
	else  /*parent process*/
	{

		LOGD(TAG"parent pid = %d, before execl %s %s\n", pid,"/system/bin/sh",META_CONNECT_WIFI);

		waitpid(pid, &child_stat, 0) ;
		if (WIFEXITED(child_stat)) {
			LOGE(TAG "%s: terminated by exit(%d)\n", __FUNCTION__, WEXITSTATUS(child_stat));
			return WEXITSTATUS(child_stat);
		} else {
			LOGE(TAG "%s: execl error, %d (%s)\n", __FUNCTION__, errno, strerror(errno));
			return -1;
		}
		
		return -1;
	}
}

int meta_disconnect_wifi(void){
	pid_t pid;
	int err;
	int child_stat = 0;

	if ((pid = fork()) < 0)
	{
		LOGD(TAG"fork fails: %d (%s)\n", errno, strerror(errno));
		return (-1);
	}
	else if (pid == 0)  /*child process*/
	{

		LOGD(TAG"child process %s %s before\n","/system/bin/sh",META_DISCONNECT_WIFI);
		err = execl("/system/bin/sh","sh",META_DISCONNECT_WIFI,(char *)NULL);

		LOGD(TAG"child process %s %s after\n","/system/bin/sh",META_DISCONNECT_WIFI);
		exit(-2);
	}
	else  /*parent process*/
	{

		LOGD(TAG"parent pid = %d, before execl %s %s\n", pid,"/system/bin/sh",META_DISCONNECT_WIFI);

		waitpid(pid, &child_stat, 0);
		if (WIFEXITED(child_stat)) {
			LOGE(TAG "%s: terminated by exit(%d)\n", __FUNCTION__, WEXITSTATUS(child_stat));
			return WEXITSTATUS(child_stat);
		} else {
			LOGE(TAG "%s: execl error, %d (%s)\n", __FUNCTION__, errno, strerror(errno));
			return -1;
		}

		return -1;
	}
}

int get_ip_address(char * ip, int *len){

	struct ifaddrs * ifAddrStruct=NULL;
	void * tmpAddrPtr=NULL;
	char addressBuffer[INET_ADDRSTRLEN];
	int ret = 0;

	if ((NULL == ip)||(NULL == len))
		return -1;

	ret = getifaddrs(&ifAddrStruct);
	if (-1 == ret)
		return -1;

	while (ifAddrStruct!=NULL)
	{
		if (ifAddrStruct->ifa_addr->sa_family==AF_INET)
		{	// check it is IP4
			// is a valid IP4 Address
			tmpAddrPtr = &((struct sockaddr_in *)ifAddrStruct->ifa_addr)->sin_addr;

			inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
			//printf("%s IPV4 Address %s\n", ifAddrStruct->ifa_name, addressBuffer);
			if(0 == strncmp(ifAddrStruct->ifa_name,"wlan0",5)){
				*len = strlen(addressBuffer);
				if(*len > 15){
					LOGE(TAG"IP len invalid %d \n",*len);
					return -1;
				}

				memset(ip,0,*len+1);

				if(NULL == strncpy(ip,addressBuffer,*len)){
					 LOGE(TAG"strncpy fail.\n");
				}

				LOGD(TAG"IPV4 Address %s, len %d\n",ip,*len);

				return 0;
			}

		}
#if 0
		else if (ifAddrStruct->ifa_addr->sa_family==AF_INET6)
		{	// check it is IP6
			// is a valid IP6 Address
			tmpAddrPtr=&((struct sockaddr_in *)ifAddrStruct->ifa_addr)->sin_addr;
			char addressBuffer[INET6_ADDRSTRLEN];
			inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN);
			printf("%s IPV6 Address %s\n", ifAddrStruct->ifa_name, addressBuffer); 
		} 
#endif
		ifAddrStruct = ifAddrStruct->ifa_next;
	}


	return -1;
}


