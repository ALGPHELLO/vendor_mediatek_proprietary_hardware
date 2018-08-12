#ifndef ANDROID_MESSENGER_IPI_H
#define ANDROID_MESSENGER_IPI_H

#include "AudioType.h"
#include <AudioLock.h>


namespace android {

#define IPI_MSG_HEADER_SIZE      (16)
#define MAX_IPI_MSG_PAYLOAD_SIZE (32)
#define MAX_IPI_MSG_BUF_SIZE     (IPI_MSG_HEADER_SIZE + MAX_IPI_MSG_PAYLOAD_SIZE)

#define IPI_MSG_MAGIC_NUMBER     (0x8888)


enum { /* audio_ipi_msg_layer_t */
    AUDIO_IPI_LAYER_HAL_TO_KERNEL,  /* HAL    -> kernel */
    AUDIO_IPI_LAYER_HAL_TO_SCP,     /* HAL    -> SCP */

    AUDIO_IPI_LAYER_KERNEL_TO_HAL,  /* kernel -> HAL */
    AUDIO_IPI_LAYER_KERNEL_TO_SCP,  /* kernel -> SCP */

    AUDIO_IPI_LAYER_SCP_TO_HAL,     /* SCP    -> HAL */
    AUDIO_IPI_LAYER_SCP_TO_KERNEL,  /* SCP    -> kernel */

    AUDIO_IPI_LAYER_MODEM_TO_SCP,   /* MODEM  -> SCP */
};


enum { /* audio_ipi_msg_data_t */
    AUDIO_IPI_MSG_ONLY, /* param1: defined by user,       param2: defined by user */
    AUDIO_IPI_PAYLOAD,  /* param1: payload length (<=32), param2: defined by user */
    AUDIO_IPI_DMA,      /* param1: dma data length,       param2: defined by user */
};



enum { /* audio_ipi_msg_ack_t */
    AUDIO_IPI_MSG_BYPASS_ACK = 0,
    AUDIO_IPI_MSG_NEED_ACK   = 1,
    AUDIO_IPI_MSG_ACK_BACK   = 2,
    AUDIO_IPI_MSG_CANCELED   = 8
};


struct ipi_msg_t {
    uint16_t magic;      /* IPI_MSG_MAGIC_NUMBER */
    uint8_t  task_scene; /* see task_scene_t */
    uint8_t  msg_layer;  /* see audio_ipi_msg_layer_t */
    uint8_t  data_type;  /* see audio_ipi_msg_data_t */
    uint8_t  ack_type;   /* see audio_ipi_msg_ack_t */
    uint16_t msg_id;     /* defined by user */
    uint32_t param1;     /* see audio_ipi_msg_data_t */
    uint32_t param2;     /* see audio_ipi_msg_data_t */
    union {
        char payload[MAX_IPI_MSG_PAYLOAD_SIZE];
        char *dma_addr;  /* for AUDIO_IPI_DMA only */
    };
};


class AudioMessengerIPI {
public:
    virtual ~AudioMessengerIPI();

    static AudioMessengerIPI *getInstance();

    virtual void     loadTaskScene(const uint8_t task_scene);
    virtual void     configDumpPcmEnable(const bool enable);
    virtual void     registerScpFeature(const bool enable);
    virtual void     setSpmApMdSrcReq(const bool enable);

    virtual status_t sendIpiMsg(
        struct ipi_msg_t *p_ipi_msg,
        uint8_t task_scene, /* task_scene_t */
        uint8_t msg_layer, /* audio_ipi_msg_layer_t */
        uint8_t data_type, /* see audio_ipi_msg_data_t */
        uint8_t ack_type,  /* see audio_ipi_msg_ack_t */
        uint16_t msg_id,
        uint32_t param1,
        uint32_t param2,
        char    *data_buffer);

    virtual status_t recvIpiMsg(struct ipi_msg_t *p_ipi_msg);



protected:
    AudioMessengerIPI();


    /**
     * open/close audio ipi driver
     */
    virtual status_t openDriver();
    virtual status_t closeDriver();

    virtual uint16_t getMessageBufSize(const struct ipi_msg_t *ipi_msg);
    virtual void     dumpMsg(const struct ipi_msg_t *ipi_msg);
    virtual void     checkMsgFormat(const struct ipi_msg_t *ipi_msg, unsigned int len);


    int mDeviceDriver;
    AudioLock mLock;



private:
    /**
     * singleton pattern
     */
    static AudioMessengerIPI *mAudioMessengerIPI;
};

} // end namespace android

#endif // end of ANDROID_MESSENGER_IPI_H
