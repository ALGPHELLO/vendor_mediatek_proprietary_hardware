/*
* Copyright (C) 2014 MediaTek Inc.
* Modification based on code covered by the mentioned copyright
* and/or permission notice(s).
*/
/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <linux/ioctl.h>

#include <cutils/log.h>
#include <cutils/properties.h>

//reboot_meta_flag: 1 means meta mode, other value means use default boot mode.
#define REBOOT_META_FLAG "reboot_meta_flag"
#define REBOOT_META_FLAG_VALUE  "meta"
#define REBOOT_META_FLAG_LENGTH 5     

struct env_ioctl
{
	char *name;
	int name_len;
	char *value;
	int value_len;	
};
#define ENV_MAGIC	 'e'
#define ENV_READ		_IOW(ENV_MAGIC, 1, int)
#define ENV_WRITE 		_IOW(ENV_MAGIC, 2, int)


static int set_env_value(char * name,char * value,unsigned int value_max_len) 
{
	struct env_ioctl env_ioctl_obj;
	int env_fd = 0;
	int ret = 0;
	unsigned long long data_free_size = 0;
	unsigned long long value_new = strtoll(value,NULL,10);
	unsigned int name_len_set=strlen(name)+1;
	unsigned int value_len_set=strlen(value)+1;

	if ((env_fd = open("/proc/lk_env", O_RDWR)) < 0) {
		ALOGE("META[pre_meta] Open env fail for read %s",name);
		goto FAIL_RUTURN;
	}
	if (!(env_ioctl_obj.name = new char[name_len_set])) {
		ALOGE("META[pre_meta] Allocate Memory for env name fail");
		goto FREE_FD;
	} else {
		memset(env_ioctl_obj.name,0x0,name_len_set);
	}
	if (!(env_ioctl_obj.value = new char[value_max_len])){
		ALOGE("META[pre_meta] Allocate Memory for env value fail");
		goto FREE_ALLOCATE_NAME;
	} else {
		memset(env_ioctl_obj.value,0x0,value_max_len);
	}
	env_ioctl_obj.name_len = name_len_set;
	env_ioctl_obj.value_len = value_max_len;
	memcpy(env_ioctl_obj.name, name, name_len_set);
	memcpy(env_ioctl_obj.value, value, value_len_set);
	if ((ret = ioctl(env_fd, ENV_WRITE, &env_ioctl_obj))) {
		ALOGE("META[pre_meta] Set env for %s check fail ret = %d, errno = %d", name,ret, errno);
		goto FREE_ALLOCATE_VALUE;
	}
	delete []env_ioctl_obj.name;
	delete []env_ioctl_obj.value;
	close(env_fd);
	return ret;
FREE_ALLOCATE_VALUE:
	delete []env_ioctl_obj.value;
FREE_ALLOCATE_NAME:
	delete []env_ioctl_obj.name;
FREE_FD:
	close(env_fd);
FAIL_RUTURN:
	return -1;	
}
static int get_env_value(char * name,char * value,unsigned int value_max_len)
{
	struct env_ioctl env_ioctl_obj;
	int env_fd = 0;
	int ret = 0;
	unsigned int name_len=strlen(name)+1;
	if((env_fd = open("/proc/lk_env", O_RDWR)) < 0) {
		ALOGE("META[pre_meta] Open env fail for read %s",name);
		goto FAIL_RUTURN;
	}
	if(!(env_ioctl_obj.name = new char[name_len])) {
		ALOGE("META[pre_meta] Allocate Memory for env name fail");
		goto FREE_FD;
	}else{
		memset(env_ioctl_obj.name,0x0,name_len);
	}
	if(!(env_ioctl_obj.value = new char[value_max_len])){
		ALOGE("META[pre_meta] Allocate Memory for env value fail");
		goto FREE_ALLOCATE_NAME;
	}else{
		memset(env_ioctl_obj.value,0x0,value_max_len);
	}
	env_ioctl_obj.name_len = name_len;
	env_ioctl_obj.value_len = value_max_len;
	memcpy(env_ioctl_obj.name, name, name_len);
	if((ret = ioctl(env_fd, ENV_READ, &env_ioctl_obj))) {
		ALOGE("Get env for %s check fail ret = %d, errno = %d", name,ret, errno);
		goto FREE_ALLOCATE_VALUE;
	}
	if(env_ioctl_obj.value) {
		memcpy(value,env_ioctl_obj.value,env_ioctl_obj.value_len);
		ALOGD("META[pre_meta] %s  = %s", env_ioctl_obj.name,env_ioctl_obj.value);
	} else {
		ALOGE("META[pre_meta] %s is not be set",name);
		goto FREE_ALLOCATE_VALUE;
	}

	delete []env_ioctl_obj.name;
	delete []env_ioctl_obj.value;
	close(env_fd);
	return ret;
FREE_ALLOCATE_VALUE:
	delete []env_ioctl_obj.value;
FREE_ALLOCATE_NAME:
	delete []env_ioctl_obj.name;
FREE_FD:
	close(env_fd);
FAIL_RUTURN:
	return -1;

}

int main(int argc, char** argv)
{
	if(set_env_value(REBOOT_META_FLAG,REBOOT_META_FLAG_VALUE,REBOOT_META_FLAG_LENGTH)) 
	{
	    ALOGE("META[pre_meta] set %s to lk_env fail",REBOOT_META_FLAG);
	}
	sync();
	
	property_set("sys.powerctl","reboot");
	ALOGD("META[pre_meta] reboot target");
    return 0;
}
