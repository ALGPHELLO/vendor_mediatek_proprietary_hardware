#include "meta_sdcard.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <utils/Log.h>


#define FREEIF(p)	do { if(p) free(p); (p) = NULL; } while(0)
#define CHKERR(x)	do { if((x) < 0) goto error; } while(0)

#define UNUSED(x) (void)(x)

static char *path_data = NULL;
static SDCARD_CNF_CB cnf_cb = NULL;

static int meta_sdcard_read_info(const char *filename, void *buf, ssize_t bufsz)
{
	int fd, rsize;
	if ((fd = open(filename, O_RDONLY)) < 0) {
		META_SDCARD_LOG("Open %s failed errno(%s)",filename,(char*)strerror(errno));
		return -1;
	}

	rsize = read(fd, buf, bufsz);
	if(rsize < 0)
		META_SDCARD_LOG("read %s failed errno(%s)",filename,(char*)strerror(errno));
	close(fd);

	return rsize;
}

static void meta_sdcard_send_resp(SDCARD_CNF *cnf)
{
	if (cnf_cb)
		cnf_cb(cnf);
	else
		WriteDataToPC(cnf, sizeof(SDCARD_CNF), NULL, 0);
}

void Meta_SDcard_Register(SDCARD_CNF_CB callback)
{
	cnf_cb = callback;
}

bool Meta_SDcard_Init(SDCARD_REQ *req)
{
	int id = (int)req->dwSDHCIndex;
	char name[20];
	char *ptr;
	DIR *dp;
	struct dirent *dirp;
	int fd;

	META_SDCARD_LOG("meta_sdcard_init, ID(%d)\n",id);
#ifdef MTK_EMMC_SUPPORT
	if ((id < 0) || (id > (MAX_NUM_SDCARDS - 1))){
#else
	if ((id < 1) || (id > (MAX_NUM_SDCARDS - 1))){
#endif
		META_SDCARD_LOG("ID error(%d)\n",id);
		return false;
	}

	/* init path_data of /dev/block/mmcblkx */
	if (!path_data && NULL == (path_data = malloc(512))) {
		META_SDCARD_LOG("No memory\n");
		return false;
	}
#ifdef MTK_EMMC_SUPPORT
	sprintf(name, "mmcblk%d", id);
#else
	sprintf(name, "mmcblk%d", id - 1);
#endif
	ptr = path_data;
	ptr += sprintf(ptr, "/dev/block/%s", name);

	META_SDCARD_LOG("[META_SD] path_data: %s/\n", path_data);

#ifdef MTK_EMMC_SUPPORT
	sprintf(name, "mmc%d", id);
#else
	sprintf(name, "mmc%d", id - 1);
#endif

	return true;
}

bool Meta_SDcard_Deinit(void)
{
	FREEIF(path_data);
	return true;
}


void Meta_SDcard_OP(SDCARD_REQ *req, char *peer_buf, unsigned short peer_len)
{
	SDCARD_CNF cnf;
	int bufsz = 512;
	char fname[512];
	char buf[512];
	unsigned char cid[16];
	unsigned char csd[16];

	UNUSED(peer_buf);
	UNUSED(peer_len);

	memset(&cnf, 0, sizeof(SDCARD_CNF));

	cnf.header.id = FT_SDCARD_CNF_ID;
	cnf.header.token = req->header.token;
	cnf.status = META_SUCCESS;

	META_SDCARD_LOG("[META_SD] read %s \n", path_data);
	sprintf(fname, "%s", path_data);
	CHKERR(meta_sdcard_read_info(fname, buf, bufsz));
	memcpy(csd,buf,16*sizeof(unsigned char));

	META_SDCARD_LOG("[META_SD] send resp, pass\n");
	meta_sdcard_send_resp(&cnf);
	return;

error:
	META_SDCARD_LOG("[META_SD] send resp, failed\n");
	cnf.status = META_FAILED;
	meta_sdcard_send_resp(&cnf);
	return;
}

