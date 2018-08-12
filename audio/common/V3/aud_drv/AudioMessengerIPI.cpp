#include "AudioMessengerIPI.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <AudioLock.h>


#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "AudioMessengerIPI"


/*#define ENABLE_DUMP_IPI_MSG*/
#define MAX_IPI_MSG_QUEUE_SIZE (8)


#define AUDIO_IPI_DEVICE_PATH "/dev/audio_ipi"
#define AUDIO_IPI_IOC_MAGIC 'i'

#define AUDIO_IPI_IOCTL_SEND_MSG_ONLY _IOW(AUDIO_IPI_IOC_MAGIC, 0, unsigned int)
#define AUDIO_IPI_IOCTL_SEND_PAYLOAD  _IOW(AUDIO_IPI_IOC_MAGIC, 1, unsigned int)
#define AUDIO_IPI_IOCTL_SEND_DRAM     _IOW(AUDIO_IPI_IOC_MAGIC, 2, unsigned int)

#define AUDIO_IPI_IOCTL_LOAD_SCENE    _IOW(AUDIO_IPI_IOC_MAGIC, 10, unsigned int)

#define AUDIO_IPI_IOCTL_DUMP_PCM      _IOW(AUDIO_IPI_IOC_MAGIC, 97, unsigned int)
#define AUDIO_IPI_IOCTL_REG_FEATURE   _IOW(AUDIO_IPI_IOC_MAGIC, 98, unsigned int)
#define AUDIO_IPI_IOCTL_SPM_MDSRC_ON  _IOW(AUDIO_IPI_IOC_MAGIC, 99, unsigned int)

namespace android {

AudioMessengerIPI *AudioMessengerIPI::mAudioMessengerIPI = NULL;
AudioMessengerIPI *AudioMessengerIPI::getInstance() {
    static AudioLock mGetInstanceLock;
    AL_AUTOLOCK(mGetInstanceLock);

    if (mAudioMessengerIPI == NULL) {
        mAudioMessengerIPI = new AudioMessengerIPI();
    }
    ASSERT(mAudioMessengerIPI != NULL);
    return mAudioMessengerIPI;
}


AudioMessengerIPI::AudioMessengerIPI() :
    mDeviceDriver(-1) {
    ALOGV("%s()", __FUNCTION__);
    openDriver();
}


AudioMessengerIPI::~AudioMessengerIPI() {
    ALOGV("%s()", __FUNCTION__);
    closeDriver();
}


status_t AudioMessengerIPI::openDriver() {
    ALOGD("%s()", __FUNCTION__);
    AL_AUTOLOCK(mLock);

    mDeviceDriver = open(AUDIO_IPI_DEVICE_PATH, O_RDONLY);
    if (mDeviceDriver < 0) {
        ALOGE("%s() fail to open %s, errno: %d", __FUNCTION__, AUDIO_IPI_DEVICE_PATH, errno);
        return UNKNOWN_ERROR;
    }

    return NO_ERROR;
}


status_t AudioMessengerIPI::closeDriver() {
    ALOGD("%s()", __FUNCTION__);
    AL_AUTOLOCK(mLock);

    if (mDeviceDriver >= 0) {
        close(mDeviceDriver);
        mDeviceDriver = -1;
    }

    return NO_ERROR;
}


uint16_t AudioMessengerIPI::getMessageBufSize(const struct ipi_msg_t *ipi_msg) {
    if (ipi_msg->data_type == AUDIO_IPI_MSG_ONLY) {
        return IPI_MSG_HEADER_SIZE;
    } else if (ipi_msg->data_type == AUDIO_IPI_PAYLOAD) {
        return (IPI_MSG_HEADER_SIZE + ipi_msg->param1);
    } else if (ipi_msg->data_type == AUDIO_IPI_DMA) {
        return (IPI_MSG_HEADER_SIZE + 8); /* 64-bits addr */
    } else {
        return 0;
    }
}


void AudioMessengerIPI::dumpMsg(const struct ipi_msg_t *ipi_msg __unused) {
#ifdef ENABLE_DUMP_IPI_MSG
    int i = 0;
    int payload_size = 0;

    ALOGD("%s(), sizeof(ipi_msg_t) = %d", __FUNCTION__, sizeof(ipi_msg_t));

    ALOGD("%s(), magic = 0x%x", __FUNCTION__, ipi_msg->magic);
    ALOGD("%s(), task_scene = 0x%x", __FUNCTION__, ipi_msg->task_scene);
    ALOGD("%s(), msg_layer = 0x%x", __FUNCTION__, ipi_msg->msg_layer);
    ALOGD("%s(), data_type = 0x%x", __FUNCTION__, ipi_msg->data_type);
    ALOGD("%s(), ack_type = 0x%x", __FUNCTION__, ipi_msg->ack_type);
    ALOGD("%s(), msg_id = 0x%x", __FUNCTION__, ipi_msg->msg_id);
    ALOGD("%s(), param1 = 0x%x", __FUNCTION__, ipi_msg->param1);
    ALOGD("%s(), param2 = 0x%x", __FUNCTION__, ipi_msg->param2);

    if (ipi_msg->data_type == AUDIO_IPI_PAYLOAD) {
        payload_size = ipi_msg->param1;
        for (i = 0; i < payload_size; i++) {
            ALOGD("%s(), payload[%d] = 0x%x", __FUNCTION__, i, ipi_msg->payload[i]);
        }
    } else if (ipi_msg->data_type == AUDIO_IPI_DMA) {
        ALOGD("%s(), dma_addr = %p\n", __FUNCTION__, ipi_msg->dma_addr);
    }
#endif
}


void AudioMessengerIPI::checkMsgFormat(const struct ipi_msg_t *ipi_msg, unsigned int len) {
    dumpMsg(ipi_msg);

    ASSERT(ipi_msg->magic == IPI_MSG_MAGIC_NUMBER);
    ASSERT(getMessageBufSize(ipi_msg) == len);
}


void AudioMessengerIPI::loadTaskScene(const uint8_t task_scene) {
    int retval = ioctl(mDeviceDriver, AUDIO_IPI_IOCTL_LOAD_SCENE, task_scene);
    if (retval != 0) {
        ALOGE("%s() ioctl fail! retval = %d, errno: %d", __FUNCTION__, retval, errno);
    }
}


void AudioMessengerIPI::configDumpPcmEnable(const bool enable) {
    int retval = ioctl(mDeviceDriver, AUDIO_IPI_IOCTL_DUMP_PCM, enable);
    if (retval != 0) {
        ALOGE("%s() ioctl fail! retval = %d, errno: %d", __FUNCTION__, retval, errno);
    }
}


void AudioMessengerIPI::registerScpFeature(const bool enable) {
    int retval = ioctl(mDeviceDriver, AUDIO_IPI_IOCTL_REG_FEATURE, enable);
    if (retval != 0) {
        ALOGE("%s() ioctl fail! retval = %d, errno: %d", __FUNCTION__, retval, errno);
    }
}


void AudioMessengerIPI::setSpmApMdSrcReq(const bool enable) {
    int retval = ioctl(mDeviceDriver, AUDIO_IPI_IOCTL_SPM_MDSRC_ON, enable);
    if (retval != 0) {
        ALOGE("%s() ioctl fail! retval = %d, errno: %d", __FUNCTION__, retval, errno);
    }
}


status_t AudioMessengerIPI::recvIpiMsg(struct ipi_msg_t *p_ipi_msg) {
    int length_read = read(mDeviceDriver, (void *)p_ipi_msg, sizeof(ipi_msg_t));
    if (length_read != sizeof(ipi_msg_t)) {
        ALOGW("%s(), length_read = %d, return", __FUNCTION__, mDeviceDriver);
        return UNKNOWN_ERROR;
    }

    return NO_ERROR;
}


status_t AudioMessengerIPI::sendIpiMsg(
    struct ipi_msg_t *p_ipi_msg,
    uint8_t task_scene, /* task_scene_t */
    uint8_t msg_layer, /* audio_ipi_msg_layer_t */
    uint8_t data_type, /* see audio_ipi_msg_data_t */
    uint8_t ack_type,  /* see audio_ipi_msg_ack_t */
    uint16_t msg_id,
    uint32_t param1,
    uint32_t param2,
    char    *data_buffer) {
    ALOGD("%s(), task_scene = %d, msg_id = 0x%x, param1 = 0x%x, param2 = 0x%x",
          __FUNCTION__, task_scene, msg_id, param1, param2);
    AL_AUTOLOCK(mLock);

    if (mDeviceDriver < 0) {
        ALOGW("%s(), mDeviceDriver = %d, return", __FUNCTION__, mDeviceDriver);
        return UNKNOWN_ERROR;
    }


    uint32_t ipi_msg_len = 0;

    memset(p_ipi_msg, 0, MAX_IPI_MSG_BUF_SIZE);

    p_ipi_msg->magic      = IPI_MSG_MAGIC_NUMBER;
    p_ipi_msg->task_scene = task_scene;
    p_ipi_msg->msg_layer  = msg_layer;
    p_ipi_msg->data_type  = data_type;
    p_ipi_msg->ack_type   = ack_type;
    p_ipi_msg->msg_id     = msg_id;
    p_ipi_msg->param1     = param1;
    p_ipi_msg->param2     = param2;

    if (p_ipi_msg->data_type == AUDIO_IPI_PAYLOAD) {
        ASSERT(data_buffer != NULL);
        ASSERT(p_ipi_msg->param1 <= MAX_IPI_MSG_PAYLOAD_SIZE);
        memcpy(p_ipi_msg->payload, data_buffer, p_ipi_msg->param1);
    } else if (p_ipi_msg->data_type == AUDIO_IPI_DMA) {
        ASSERT(data_buffer != NULL);
        p_ipi_msg->dma_addr = data_buffer;
    }


    ipi_msg_len = getMessageBufSize(p_ipi_msg);
    checkMsgFormat(p_ipi_msg, ipi_msg_len);


    int retval = 0;
    if (p_ipi_msg->data_type == AUDIO_IPI_MSG_ONLY) {
        retval = ioctl(mDeviceDriver, AUDIO_IPI_IOCTL_SEND_MSG_ONLY, p_ipi_msg);
    } else if (p_ipi_msg->data_type == AUDIO_IPI_PAYLOAD) {
        retval = ioctl(mDeviceDriver, AUDIO_IPI_IOCTL_SEND_PAYLOAD, p_ipi_msg);
    } else if (p_ipi_msg->data_type == AUDIO_IPI_DMA) {
        retval = ioctl(mDeviceDriver, AUDIO_IPI_IOCTL_SEND_DRAM, p_ipi_msg);
    }
    if (retval != 0) {
        ALOGE("%s() ioctl fail! retval = %d, errno: %d", __FUNCTION__, retval, errno);
    }

    return (retval == 0) ? NO_ERROR : UNKNOWN_ERROR;
}




} // end of namespace android
