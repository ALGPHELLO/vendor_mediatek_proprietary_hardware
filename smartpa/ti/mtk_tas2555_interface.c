#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "ti_extamp"

#include <fcntl.h>
#include <cutils/log.h>

#include "mtk_tas2555_interface.h"

static int ti_extamp_get_dev_file_desc()
{
    int fd = open(TI_DRV2555_I2CDEVICE, O_RDWR | O_NONBLOCK, 0);

    if (fd < 0)
        ALOGW("Can't open i2c bus:%s\n", TI_DRV2555_I2CDEVICE);

    return fd;
}

static int ti_extamp_set_samplerate(int samplerate, int fd)
{
    int ret = 0;
    unsigned char buf[5] = {TIAUDIO_CMD_SAMPLERATE, 0};

    buf[1] = (unsigned char)((samplerate & 0xff000000)>>24);
    buf[2] = (unsigned char)((samplerate & 0x00ff0000)>>16);
    buf[3] = (unsigned char)((samplerate & 0x0000ff00)>>8);
    buf[4] = (unsigned char)((samplerate & 0x000000ff));

    ret = write(fd, buf, 5);

    return ret;
}

static int ti_extamp_init(struct SmartPa *smart_pa)
{
    int fd = ti_extamp_get_dev_file_desc();
    (void *) smart_pa;

    if (fd < 0)
        return fd;
    else
        close(fd);

    return 0;
}

static int ti_extamp_deinit()
{
    return 0;
}

static int ti_extamp_speakerOn(struct SmartPaRuntime *runtime)
{
    int ret = 0;
    unsigned char buf[2] = {TIAUDIO_CMD_SPEAKER, 1};
    int fd = ti_extamp_get_dev_file_desc();

    if (fd < 0)
        return fd;

    ret = ti_extamp_set_samplerate(runtime->sampleRate, fd);
    if (ret < 0)
        return ret;

    ret = write(fd, buf, 2);
    close(fd);

    return ret;
}

static int ti_extamp_speakerOff()
{
    int ret = 0;
    unsigned char buf[2] = {TIAUDIO_CMD_SPEAKER, 0};
    int fd = ti_extamp_get_dev_file_desc();

    if (fd < 0)
        return fd;

    ret = write(fd, buf, 2);
    close(fd);

    return ret;
}

int mtk_smartpa_init(struct SmartPa *smart_pa)
{
    ALOGD("%s\n", __FUNCTION__);

    smart_pa->ops.init = (void *) ti_extamp_init;
    smart_pa->ops.deinit = (void *) ti_extamp_deinit;
    smart_pa->ops.speakerOn = (void *) ti_extamp_speakerOn;
    smart_pa->ops.speakerOff = (void *) ti_extamp_speakerOff;

    return 0;
}
