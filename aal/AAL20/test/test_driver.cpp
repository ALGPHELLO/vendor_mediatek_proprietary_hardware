#define LOG_TAG "AALService"

#include <stdio.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <ddp_drv.h>
#include <sys/ioctl.h>

int main(int argc, char *argv[])
{
    int fd = open("/dev/mtk_disp_mgr", O_RDONLY, 0);
    int ret;

    int enable = 1;
    ret = ioctl(fd, DISP_IOCTL_AAL_EVENTCTL, &enable);
    if (ret != 0) {
        printf("ioctl failed: %d", ret);
    }

    return 0;
}

