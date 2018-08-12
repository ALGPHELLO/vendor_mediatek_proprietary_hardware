#include <SpeechMessengerNormal.h>

#include <stdlib.h>
#include <string.h>

#include <errno.h>

#include <pthread.h>

#include <cutils/properties.h> /* for PROPERTY_KEY_MAX */

#include <log/log.h>

extern "C" {
#include <hardware/ccci_intf.h>
}


#include <audio_memory_control.h>

#include <AudioAssert.h>

#include <SpeechType.h>

#include <SpeechUtility.h>

#include <SpeechMessageID.h>




#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "SpeechMessengerNormal"


namespace android {


/*
 * =============================================================================
 *                     global value
 * =============================================================================
 */

static const char kPropertyKeyShareMemoryInit[PROPERTY_KEY_MAX] = "af.speech.shm_init";


/*
 * =============================================================================
 *                     utility for modem
 * =============================================================================
 */

static CCCI_MD get_ccci_md_number(const modem_index_t mModemIndex) {
    CCCI_MD ccci_md = MD_SYS1;

    switch (mModemIndex) {
    case MODEM_1:
        ccci_md = MD_SYS1;
        break;
    case MODEM_2:
        ccci_md = MD_SYS2;
        break;
    default:
        ccci_md = MD_SYS1;
    }
    return ccci_md;
}


/*
 * =============================================================================
 *                     MACRO
 * =============================================================================
 */

#define CCCI_MAX_BUF_SIZE               (3456)

#define CCCI_MAILBOX_SIZE               (16) /* sizeof(ccci_mail_box_t) */
#define CCCI_MAX_AP_PAYLOAD_HEADER_SIZE (6)
#define CCCI_MAX_MD_PAYLOAD_HEADER_SIZE (10)
#define CCCI_MAX_AP_PAYLOAD_DATA_SIZE   (CCCI_MAX_BUF_SIZE - CCCI_MAILBOX_SIZE - CCCI_MAX_AP_PAYLOAD_HEADER_SIZE)
#define CCCI_MAX_MD_PAYLOAD_DATA_SIZE   (CCCI_MAX_BUF_SIZE - CCCI_MAILBOX_SIZE - CCCI_MAX_MD_PAYLOAD_HEADER_SIZE)

#define CCCI_MAILBOX_MAGIC              (0xFFFFFFFF)

#define CCCI_AP_PAYLOAD_SYNC            (0xA2A2)
#define CCCI_MD_PAYLOAD_SYNC            (0x1234) /* 0x2A2A is old lagecy code */


#define MAX_SIZE_OF_ONE_FRAME           (16) /* 32-bits * 4ch */


/*
 * =============================================================================
 *                     typedef
 * =============================================================================
 */

typedef uint32_t ccci_channel_t;

enum { /* ccci_channel_t */
#if 0 /* 80020004 or 0x80030004 */
    CCCI_M2A_CHANNEL_NUMBER = 4,
#endif
    CCCI_A2M_CHANNEL_NUMBER = 5
};


typedef uint8_t ccci_msg_buffer_t;

enum { /* ccci_msg_buffer_t */
    CCCI_MSG_BUFFER_TYPE_MAILBOX       = 0,
    CCCI_MSG_BUFFER_TYPE_AP_PAYLOAD    = 1,
    CCCI_MSG_BUFFER_TYPE_MD_PAYLOAD    = 2
};


typedef struct ccci_mail_box_t {
    uint32_t magic;
    uint16_t param_16bit;
    uint16_t msg_id;        /* sph_msg_id_t */
    uint32_t ch;            /* ccci_channel_t */
    uint32_t param_32bit;
} ccci_mail_box_t;


typedef struct ccci_ap_payload_t {
    uint32_t offset;        /* always 0 */
    uint32_t payload_size;
    uint32_t ch;
    uint16_t param_16bit;
    uint16_t msg_id;        /* sph_msg_id_t */

    uint16_t data_sync;     /* CCCI_AP_PAYLOAD_SYNC */
    uint16_t data_type;     /* share_buff_data_type_t */
    uint16_t data_size;

    uint8_t  payload_data[CCCI_MAX_AP_PAYLOAD_DATA_SIZE];
} ccci_ap_payload_t;


typedef struct ccci_md_payload_t {
    uint32_t offset;
    uint32_t message_size;
    uint32_t ch;
    uint16_t param_16bit;
    uint16_t msg_id;        /* sph_msg_id_t */

    uint16_t data_sync;     /* CCCI_MD_PAYLOAD_SYNC */
    uint16_t data_type;     /* share_buff_data_type_t */
    uint16_t data_size;
    uint16_t idx;
    uint16_t total_idx;

    uint8_t  data[CCCI_MAX_MD_PAYLOAD_DATA_SIZE];
} ccci_md_payload_t;


typedef struct ccci_msg_t {
    union {
        uint8_t           buffer[CCCI_MAX_BUF_SIZE]; /* for ccci read/write */
        ccci_mail_box_t   mail_box;
        ccci_ap_payload_t ap_payload;
        ccci_md_payload_t md_payload;
    };

    uint8_t               buffer_type; /* ccci_msg_buffer_t */
    uint16_t              buffer_size;
} ccci_msg_t;



/*
 * =============================================================================
 *                     class implementation
 * =============================================================================
 */

SpeechMessengerNormal::SpeechMessengerNormal(const modem_index_t modem_index) {
    int retval = 0;

    ALOGV("%s(), modem_index: %d", __FUNCTION__, modem_index);
    mModemIndex = modem_index;

    mCcciDeviceHandler = -1;
    mCcciShareMemoryHandler = -1;

    mShareMemoryBase = NULL;
    mShareMemoryLength = 0;
    mShareMemory = NULL;

    hFormatShareMemoryThread = 0;

    AUDIO_ALLOC_STRUCT(ccci_msg_t, mCcciMsgSend);
    AUDIO_ALLOC_STRUCT(ccci_msg_t, mCcciMsgRead);

    retval = checkCcciStatusAndRecovery();
    if (retval != 0) {
        ALOGW("%s(), ccci open fail!!", __FUNCTION__);
    }
}


SpeechMessengerNormal::~SpeechMessengerNormal() {
#ifdef MTK_CCCI_SHARE_BUFFER_SUPPORT
    AL_LOCK(mShareMemoryHandlerLock);
    closeShareMemory();
    AL_UNLOCK(mShareMemoryHandlerLock);
#endif

    AL_LOCK(mCcciHandlerLock);
    closeCcciDriver();
    AL_UNLOCK(mCcciHandlerLock);

    AUDIO_FREE_POINTER(mCcciMsgSend);
    AUDIO_FREE_POINTER(mCcciMsgRead);
}


int SpeechMessengerNormal::openCcciDriver() {
    CCCI_USER ccci_user = USR_AUDIO_RX;
    CCCI_MD   ccci_md   = get_ccci_md_number(mModemIndex);

    const uint32_t k_max_device_name_size = 32;
    char dev_name[k_max_device_name_size];
    memset(dev_name, 0, sizeof(dev_name));

    if (mCcciDeviceHandler >= 0) {
        ALOGD("%s(), ccci_md: %d, mCcciDeviceHandler: %d already open",
              __FUNCTION__, ccci_md, mCcciDeviceHandler);
        return 0;
    }

    // open ccci device driver
    strncpy(dev_name,
            ccci_get_node_name(ccci_user, ccci_md),
            sizeof(dev_name) - 1);

    mCcciDeviceHandler = open(dev_name, O_RDWR);
    if (mCcciDeviceHandler < 0) {
        ALOGE("%s(), open(%s) fail!! mCcciDeviceHandler: %d, errno: %d", __FUNCTION__,
              dev_name, (int32_t)mCcciDeviceHandler, errno);
        return -ENODEV;
    }

    ALOGD("%s(), ccci_md: %d, dev_name: \"%s\", mCcciDeviceHandler: %d",
          __FUNCTION__, ccci_md, dev_name, mCcciDeviceHandler);
    return 0;
}


int SpeechMessengerNormal::closeCcciDriver() {
    ALOGD("%s(), mCcciDeviceHandler: %d", __FUNCTION__, mCcciDeviceHandler);

    if (mCcciDeviceHandler >= 0) {
        close(mCcciDeviceHandler);
        mCcciDeviceHandler = -1;
    }
    return 0;
}


int SpeechMessengerNormal::openShareMemory() {
#ifndef MTK_CCCI_SHARE_BUFFER_SUPPORT
    return 0;
#endif

    CCCI_MD ccci_md = get_ccci_md_number(mModemIndex);

    if (mCcciShareMemoryHandler >= 0 &&
        mShareMemoryBase != NULL &&
        mShareMemoryLength >= sizeof(sph_shm_t)) {
        ALOGD("%s(), ccci_md: %d, mCcciShareMemoryHandler: %d, mShareMemoryBase: %p, mShareMemoryLength: %u already open",
              __FUNCTION__, ccci_md, mCcciShareMemoryHandler, mShareMemoryBase, (uint32_t)mShareMemoryLength);
        return 0;
    }

    // get share momoey address
    mCcciShareMemoryHandler = ccci_smem_get(ccci_md, USR_SMEM_RAW_AUDIO, &mShareMemoryBase, &mShareMemoryLength);
    if (mCcciShareMemoryHandler < 0) {
        ALOGE("%s(), ccci_smem_get(%d) fail!! mCcciShareMemoryHandler: %d, errno: %d", __FUNCTION__,
              ccci_md, (int32_t)mCcciShareMemoryHandler, errno);
        return -ENODEV;
    }

    if (mShareMemoryBase == NULL || mShareMemoryLength == 0) {
        ALOGE("%s(), mShareMemoryBase(%p) == NULL || mShareMemoryLength(%u) == 0", __FUNCTION__,
              mShareMemoryBase, (uint32_t)mShareMemoryLength);
        closeShareMemory();
        return -EFAULT;
    }

    if (mShareMemoryLength < sizeof(sph_shm_t)) {
        ALOGE("%s(), mShareMemoryLength(%u) < sizeof(sph_shm_t): %u", __FUNCTION__,
              (uint32_t)mShareMemoryLength, (uint32_t)sizeof(sph_shm_t));
        closeShareMemory();
        return -ENOMEM;
    }

    int retval = 0;
    if (get_uint32_from_property(kPropertyKeyShareMemoryInit) != 0) {
        mShareMemory = (sph_shm_t *)mShareMemoryBase;

        ALOGD("%s(), mShareMemory: %p, ap_flag: 0x%x, md_flag: 0x%x, struct_checksum: %u",
              __FUNCTION__,
              mShareMemory,
              mShareMemory->ap_flag,
              mShareMemory->md_flag,
              mShareMemory->struct_checksum);
        ALOGD("%s(), [sph_param] offset: %u, size: %u, [ap_data] offset: %u, size: %u, [md_data] offset: %u, size: %u",
              __FUNCTION__,
              mShareMemory->region.sph_param_region.offset,
              mShareMemory->region.sph_param_region.size,
              mShareMemory->region.ap_data_region.offset,
              mShareMemory->region.ap_data_region.size,
              mShareMemory->region.md_data_region.offset,
              mShareMemory->region.md_data_region.size);
    } else {
        if (checkModemReady() == true) {
            retval = formatShareMemory();
        } else {
            retval = pthread_create(&hFormatShareMemoryThread, NULL,
                                    SpeechMessengerNormal::formatShareMemoryThread,
                                    (void *)this);
            ASSERT(retval == 0);
        }
    }

    return retval;
}


int SpeechMessengerNormal::closeShareMemory() {
#ifndef MTK_CCCI_SHARE_BUFFER_SUPPORT
    return 0;
#endif

    ALOGD("%s(), mCcciShareMemoryHandler: %d, mShareMemoryBase: %p, mShareMemoryLength: %u",
          __FUNCTION__, mCcciShareMemoryHandler, mShareMemoryBase, mShareMemoryLength);

    if (mCcciShareMemoryHandler >= 0) {
        ccci_smem_put(mCcciShareMemoryHandler, mShareMemoryBase, mShareMemoryLength);
        mCcciShareMemoryHandler = -1;
        mShareMemoryBase = NULL;
        mShareMemoryLength = 0;
        mShareMemory = NULL;
    }

    return 0;
}


int SpeechMessengerNormal::checkCcciStatusAndRecovery() {
    const uint8_t k_max_try_cnt = 200;
    uint8_t try_cnt = 0;

    int retval = 0;

    for (try_cnt = 0; try_cnt < k_max_try_cnt; try_cnt++) {
        AL_LOCK(mCcciHandlerLock);
        if (mCcciDeviceHandler >= 0) {
            retval = 0;
        } else {
            retval = openCcciDriver();
        }
        AL_UNLOCK(mCcciHandlerLock);

        if (retval == 0) {
            break;
        } else {
            ALOGW("%s(), #%d, sleep 10 ms & retry openCcciDriver", __FUNCTION__, try_cnt);
            usleep(10 * 1000); /* 10 ms */
        }
    }

    if (retval != 0) {
        ALOGW("%s(), ccci driver not init!!", __FUNCTION__);
        return retval;
    }

#ifdef MTK_CCCI_SHARE_BUFFER_SUPPORT
    for (try_cnt = 0; try_cnt < k_max_try_cnt; try_cnt++) {
        AL_LOCK(mShareMemoryHandlerLock);
        if (mCcciShareMemoryHandler >= 0 &&
            mShareMemoryBase != NULL &&
            mShareMemoryLength >= sizeof(sph_shm_t)) {
            retval = 0;
        } else {
            retval = openShareMemory();
        }
        AL_UNLOCK(mShareMemoryHandlerLock);

        if (retval == 0) {
            break;
        } else {
            ALOGW("%s(), #%d, sleep 10 ms & retry openShareMemory", __FUNCTION__, try_cnt);
            usleep(10 * 1000); /* 10 ms */
        }
    }

    if (retval != 0) {
        ALOGW("%s(), ccci share memory not init", __FUNCTION__);
        return retval;
    }
#endif

    return 0;
}


void *SpeechMessengerNormal::formatShareMemoryThread(void *arg) {
    SpeechMessengerNormal *pSpeechMessenger = NULL;

    uint32_t try_cnt = 0;
    const uint32_t k_max_try_cnt = 3000; /* (5min == 300sec) / 100 ms */

    /* detach pthread */
    pthread_detach(pthread_self());

    pSpeechMessenger = static_cast<SpeechMessengerNormal *>(arg);
    if (pSpeechMessenger == NULL) {
        ALOGE("%s(), NULL!! pSpeechMessenger %p", __FUNCTION__, pSpeechMessenger);
        WARNING("cast fail!!");
        goto FORMAT_SHM_THREAD_DONE;
    }

    /* wait until modem ready */
    for (try_cnt = 0; try_cnt < k_max_try_cnt; try_cnt++) {
        if (pSpeechMessenger->checkModemReady() == true) {
            break;
        }

        ALOGW("%s(), #%u checkModemReady fail...", __FUNCTION__, try_cnt);
        usleep(100 * 1000); /* 100 ms */
    }


    /* format share memory */
    pSpeechMessenger->formatShareMemory();



FORMAT_SHM_THREAD_DONE:
    ALOGD("%s(), terminated", __FUNCTION__);
    pthread_exit(NULL);
    return NULL;
}


int SpeechMessengerNormal::formatShareMemory() {
#ifndef MTK_CCCI_SHARE_BUFFER_SUPPORT
    return 0;
#endif

    AL_AUTOLOCK(mShareMemoryLock);

    if (mShareMemoryBase == NULL || mShareMemoryLength < sizeof(sph_shm_t)) {
        ALOGE("%s(), mShareMemoryBase(%p) == NULL || mShareMemoryLength(%u) < sizeof(sph_shm_t): %u", __FUNCTION__,
              mShareMemoryBase, (uint32_t)mShareMemoryLength, (uint32_t)sizeof(sph_shm_t));
        WARNING("EFAULT");
        return -EFAULT;
    }

    if (checkModemReady() == false) {
        ALOGW("%s(), checkModemReady fail...", __FUNCTION__);
    }


    mShareMemory = (sph_shm_t *)mShareMemoryBase;

    /* only format share memory once after boot */
    if (get_uint32_from_property(kPropertyKeyShareMemoryInit) != 0) {
        goto FORMAT_SHARE_MEMORY_DONE;
    }


    /* 32 bytes gurard region */
    sph_memset(mShareMemory->guard_region_pre, 0x0A, SPEECH_SHM_GUARD_REGION_SIZE);

    /* ap_flag */
    mShareMemory->ap_flag = 0;

    /* md_flag: modem should not read at thie stage */
    if (checkModemReady() == true &&
        mShareMemory->md_flag & SPH_SHM_MD_FLAG_SPH_PARAM_READ) {
        ALOGE("%s(), modem still read!! md_flag: 0x%x", __FUNCTION__, mShareMemory->md_flag);
        mShareMemory->md_flag = 0;
        //WARNING("modem read when format shm");
    }

    /* sph_param region */
    mShareMemory->region.sph_param_region.offset = (uint8_t *)mShareMemory->sph_param - (uint8_t *)mShareMemory;
    mShareMemory->region.sph_param_region.size = SPEECH_SHM_SPEECH_PARAM_SIZE;
    mShareMemory->region.sph_param_region.read_idx = 0;
    mShareMemory->region.sph_param_region.write_idx = 0;

    /* ap_data region */
    mShareMemory->region.ap_data_region.offset = (uint8_t *)mShareMemory->ap_data - (uint8_t *)mShareMemory;
    mShareMemory->region.ap_data_region.size = SPEECH_SHM_AP_DATA_SIZE;
    mShareMemory->region.ap_data_region.read_idx = 0;
    mShareMemory->region.ap_data_region.write_idx = 0;

    /* md_data region */
    mShareMemory->region.md_data_region.offset = (uint8_t *)mShareMemory->md_data - (uint8_t *)mShareMemory;
    mShareMemory->region.md_data_region.size = SPEECH_SHM_MD_DATA_SIZE;
    mShareMemory->region.md_data_region.read_idx = 0;
    mShareMemory->region.md_data_region.write_idx = 0;

    /* reserve_1 region */
    mShareMemory->region.reserve_1.offset = 0;
    mShareMemory->region.reserve_1.size = 0;
    mShareMemory->region.reserve_1.read_idx = 0;
    mShareMemory->region.reserve_1.write_idx = 0;

    /* reserve_2 region */
    mShareMemory->region.reserve_2.offset = 0;
    mShareMemory->region.reserve_2.size = 0;
    mShareMemory->region.reserve_2.read_idx = 0;
    mShareMemory->region.reserve_2.write_idx = 0;

    /* reserve */
    mShareMemory->reserve = 0;

    /* checksum */
    mShareMemory->struct_checksum = (uint8_t *)(&mShareMemory->struct_checksum) - (uint8_t *)mShareMemory;

    /* sph_param */
    sph_memset(mShareMemory->sph_param, 0, SPEECH_SHM_SPEECH_PARAM_SIZE);

    /* ap_data */
    sph_memset(mShareMemory->ap_data, 0, SPEECH_SHM_AP_DATA_SIZE);

    /* md_data */
    sph_memset(mShareMemory->md_data, 0, SPEECH_SHM_MD_DATA_SIZE);

    /* 32 bytes gurard region */
    sph_memset(mShareMemory->guard_region_post, 0x0A, SPEECH_SHM_GUARD_REGION_SIZE);


    /* share memory init ready */
    mShareMemory->ap_flag |= SPH_SHM_AP_FLAG_READY;

    /* init done flag */
    set_uint32_to_property(kPropertyKeyShareMemoryInit, 1);



FORMAT_SHARE_MEMORY_DONE:
    ALOGD("%s(), mShareMemory: %p, ap_flag: 0x%x, md_flag: 0x%x, struct_checksum: %u",
          __FUNCTION__,
          mShareMemory,
          mShareMemory->ap_flag,
          mShareMemory->md_flag,
          mShareMemory->struct_checksum);

    ALOGD("%s(), [sph_param] offset: %u, size: %u, [ap_data] offset: %u, size: %u, [md_data] offset: %u, size: %u",
          __FUNCTION__,
          mShareMemory->region.sph_param_region.offset,
          mShareMemory->region.sph_param_region.size,
          mShareMemory->region.ap_data_region.offset,
          mShareMemory->region.ap_data_region.size,
          mShareMemory->region.md_data_region.offset,
          mShareMemory->region.md_data_region.size);

    return 0;
}


bool SpeechMessengerNormal::checkModemReady() {
    modem_status_t modem_status = MODEM_STATUS_INVALID;
    unsigned int status_value = 0;
    int retval = 0;

    if (mCcciDeviceHandler < 0) {
        ALOGW("%s(), ccci not init!!", __FUNCTION__);
        return false; // -ENODEV;
    }

    retval = ::ioctl(mCcciDeviceHandler, CCCI_IOC_GET_MD_STATE, &status_value);
    if (retval < 0) {
        ALOGW("%s(), ioctl CCCI_IOC_GET_MD_STATE fail!! retval: %d, errno: %d",
              __FUNCTION__, retval, errno);
        return false; // retval;
    }

    if (status_value >= MODEM_STATUS_INVALID &&
        status_value <= MODEM_STATUS_EXPT) {
        modem_status = (modem_status_t)(status_value & 0xFF);
    }

    static bool dump_modem_fail_log = false; /* avoid to dump too much error log */
    if (modem_status == MODEM_STATUS_READY) {
        dump_modem_fail_log = false;
    } else {
        if (dump_modem_fail_log == false) {
            ALOGW("%s(), modem_status %d != MODEM_STATUS_READY", __FUNCTION__, modem_status);
            dump_modem_fail_log = true;
        }
    }

    return (modem_status == MODEM_STATUS_READY);
}


bool SpeechMessengerNormal::checkModemAlive() {
    if (!mShareMemory) {
        ALOGW("%s(), mShareMemory NULL!! return false", __FUNCTION__);
        return false;
    }

    return ((mShareMemory->md_flag & SPH_SHM_MD_FLAG_ALIVE) > 0);
}


uint32_t SpeechMessengerNormal::getMaxApPayloadDataSize() {
    return CCCI_MAX_AP_PAYLOAD_DATA_SIZE;
}


uint32_t SpeechMessengerNormal::getMaxMdPayloadDataSize() {
    return CCCI_MAX_MD_PAYLOAD_DATA_SIZE;
}


int SpeechMessengerNormal::sendSpeechMessage(sph_msg_t *p_sph_msg) {
    AL_AUTOLOCK(mCcciMsgSendLock);

    int length_write = 0;
    int retval = 0;
    int try_cnt = 0;
    const int k_max_try_cnt = 20;

    if (p_sph_msg == NULL) {
        ALOGE("%s(), p_sph_msg = NULL, return", __FUNCTION__);
        return -EFAULT;
    }

    retval = checkCcciStatusAndRecovery();
    if (retval != 0) {
        PRINT_SPH_MSG(ALOGE, "send msg failed!! ccci not ready", p_sph_msg);
        return retval;
    }

    if (checkModemReady() == false) {
        PRINT_SPH_MSG(ALOGE, "send msg failed!! modem not ready", p_sph_msg);
        return -EPIPE;
    }


    /* parsing speech message */
    memset(mCcciMsgSend, 0, sizeof(ccci_msg_t));
    retval = speechMessageToCcciMessage(p_sph_msg, mCcciMsgSend);
    if (retval != 0) {
        ALOGE("%s(), speechMessageToCcciMessage fail!! return", __FUNCTION__);
        return retval;
    }


    /* send message */
    for (try_cnt = 0; try_cnt < k_max_try_cnt; try_cnt++) {
        length_write = write(mCcciDeviceHandler,
                             mCcciMsgSend->buffer,
                             mCcciMsgSend->buffer_size);
        if (length_write == mCcciMsgSend->buffer_size) {
            retval = 0;
            break;
        }

        if (checkModemReady() == false) {
            PRINT_SPH_MSG(ALOGE, "write msg failed!! modem not ready", p_sph_msg);
            retval = -EPIPE;
            break;
        }

        retval = -EBADMSG;
        ALOGW("%s(), try_cnt: #%d, msg_id: 0x%x, length_write: %d, errno: %d",
              __FUNCTION__, try_cnt, p_sph_msg->msg_id, length_write, errno);
        usleep(2 * 1000);
    }

    return retval;
}


int SpeechMessengerNormal::readSpeechMessage(sph_msg_t *p_sph_msg) {
    AL_AUTOLOCK(mCcciMsgReadLock);

    int length_read = 0;
    int retval = 0;

    if (p_sph_msg == NULL) {
        ALOGE("%s(), p_sph_msg = NULL, return", __FUNCTION__);
        return -EFAULT;
    }

    retval = checkCcciStatusAndRecovery();
    if (retval != 0) {
        PRINT_SPH_MSG(ALOGE, "read msg failed!! ccci not ready", p_sph_msg);
        return retval;
    }

    /* read message */
    memset(mCcciMsgRead->buffer, 0, sizeof(mCcciMsgRead->buffer));
    length_read = read(mCcciDeviceHandler, mCcciMsgRead->buffer, sizeof(mCcciMsgRead->buffer));
    if (length_read < CCCI_MAILBOX_SIZE) { /* at least one mailbox at once */
        if (checkModemReady() == true) {
            ALOGV("%s(), read ccci fail!! modem ready, length_read: %d, errno: %d",
                  __FUNCTION__, (int32_t)length_read, errno);
            return -ETIMEDOUT;
        } else {
            ALOGW("%s(), read ccci fail!! modem invalid, length_read: %d, errno: %d",
                  __FUNCTION__, (int32_t)length_read, errno);
            return -EPIPE;
        }
    }

    mCcciMsgRead->buffer_size = length_read;

    /* parsing ccci message */
    if (mCcciMsgRead->mail_box.magic == CCCI_MAILBOX_MAGIC) {
        mCcciMsgRead->buffer_type = CCCI_MSG_BUFFER_TYPE_MAILBOX;
    } else {
        mCcciMsgRead->buffer_type = CCCI_MSG_BUFFER_TYPE_MD_PAYLOAD;
    }
    retval = ccciMessageToSpeechMessage(mCcciMsgRead, p_sph_msg);

    return retval;
}


int SpeechMessengerNormal::speechMessageToCcciMessage(
    sph_msg_t *p_sph_msg, ccci_msg_t *p_ccci_msg) {

    int retval = 0;

    if (!p_ccci_msg || !p_sph_msg) {
        ALOGW("%s(), NULL!! return", __FUNCTION__);
        return -EFAULT;
    }

    ccci_mail_box_t   *p_mail_box   = &p_ccci_msg->mail_box;
    ccci_ap_payload_t *p_ap_payload = &p_ccci_msg->ap_payload;

    switch (p_sph_msg->buffer_type) {
    case SPH_MSG_BUFFER_TYPE_MAILBOX:
        p_mail_box->magic = CCCI_MAILBOX_MAGIC;
        p_mail_box->param_16bit = p_sph_msg->param_16bit;
        p_mail_box->msg_id = p_sph_msg->msg_id;
        p_mail_box->ch = CCCI_A2M_CHANNEL_NUMBER;
        p_mail_box->param_32bit = p_sph_msg->param_32bit;

        /* date size to be writed */
        p_ccci_msg->buffer_size = CCCI_MAILBOX_SIZE;
        retval = 0;
        break;
    case SPH_MSG_BUFFER_TYPE_PAYLOAD:
        p_ap_payload->offset = 0; /* always 0 */
        p_ap_payload->payload_size = CCCI_MAX_AP_PAYLOAD_HEADER_SIZE + p_sph_msg->payload_data_size;
        p_ap_payload->ch = CCCI_A2M_CHANNEL_NUMBER;
        p_ap_payload->param_16bit  = CCCI_MAX_AP_PAYLOAD_HEADER_SIZE + p_sph_msg->payload_data_size;
        p_ap_payload->msg_id = p_sph_msg->msg_id;


        p_ap_payload->data_sync = CCCI_AP_PAYLOAD_SYNC;
        p_ap_payload->data_type = p_sph_msg->payload_data_type;
        p_ap_payload->data_size = p_sph_msg->payload_data_size;

        if (p_sph_msg->payload_data_addr == NULL) {
            ALOGE("%s(), payload_data_addr == NULL!!", __FUNCTION__);
            retval = -ENODEV;
            break;
        }

        if (p_sph_msg->payload_data_size > CCCI_MAX_AP_PAYLOAD_DATA_SIZE) {
            ALOGE("%s(), payload_data_size %d > %d!!", __FUNCTION__,
                  p_sph_msg->payload_data_size,
                  CCCI_MAX_AP_PAYLOAD_DATA_SIZE);
            retval = -ENOMEM;
            break;
        }
        memcpy(p_ap_payload->payload_data,
               p_sph_msg->payload_data_addr,
               p_sph_msg->payload_data_size);

        /* date size to be writed */
        p_ccci_msg->buffer_size = CCCI_MAILBOX_SIZE + p_ap_payload->payload_size;
        retval = 0;
        break;
    default:
        ALOGW("%s(), not support type %d!!", __FUNCTION__, p_sph_msg->buffer_type);
        retval = -EINVAL;
    }

    return retval;
}


int SpeechMessengerNormal::ccciMessageToSpeechMessage(
    ccci_msg_t *p_ccci_msg, sph_msg_t *p_sph_msg) {

    int retval = 0;

    if (!p_ccci_msg || !p_sph_msg) {
        ALOGW("%s(), NULL!! return", __FUNCTION__);
        return -EFAULT;
    }

    ccci_mail_box_t   *p_mail_box   = &p_ccci_msg->mail_box;
    ccci_md_payload_t *p_md_payload = &p_ccci_msg->md_payload;

    switch (p_ccci_msg->buffer_type) {
    case CCCI_MSG_BUFFER_TYPE_MAILBOX:
        ALOGV("%s(), buffer_size: %d, ch:%d", __FUNCTION__, p_ccci_msg->buffer_size, p_mail_box->ch);
        ASSERT(p_ccci_msg->buffer_size == CCCI_MAILBOX_SIZE);

        p_sph_msg->buffer_type = SPH_MSG_BUFFER_TYPE_MAILBOX;
        p_sph_msg->msg_id = p_mail_box->msg_id;
        p_sph_msg->param_16bit = p_mail_box->param_16bit;
        p_sph_msg->param_32bit = p_mail_box->param_32bit;
        retval = 0;
        break;
    case CCCI_MSG_BUFFER_TYPE_MD_PAYLOAD:
        ALOGV("%s(), buffer_size: %d, ch:%d", __FUNCTION__, p_ccci_msg->buffer_size, p_md_payload->ch);
        ASSERT(p_ccci_msg->buffer_size == p_md_payload->message_size);
        ASSERT(p_md_payload->message_size == (CCCI_MAILBOX_SIZE + CCCI_MAX_MD_PAYLOAD_HEADER_SIZE + p_md_payload->data_size));
        ASSERT(p_md_payload->data_sync == CCCI_MD_PAYLOAD_SYNC);
        ASSERT(p_md_payload->data_size <= CCCI_MAX_MD_PAYLOAD_DATA_SIZE);

        p_sph_msg->buffer_type = SPH_MSG_BUFFER_TYPE_PAYLOAD;
        p_sph_msg->msg_id = p_md_payload->msg_id;

        p_sph_msg->payload_data_type        = p_md_payload->data_type;
        p_sph_msg->payload_data_size        = p_md_payload->data_size;
        p_sph_msg->payload_data_addr        = p_md_payload->data;
        p_sph_msg->payload_data_idx         = p_md_payload->idx;
        p_sph_msg->payload_data_total_idx   = p_md_payload->total_idx;

        retval = 0;
        break;

    default:
        ALOGW("%s(), not support type %d!!", __FUNCTION__, p_ccci_msg->buffer_type);
        retval = -EINVAL;
    }

    return retval;
}


int SpeechMessengerNormal::resetShareMemoryIndex() {
#ifndef MTK_CCCI_SHARE_BUFFER_SUPPORT
    return 0;
#endif

    AL_AUTOLOCK(mShareMemoryLock);

    if (!mShareMemory) {
        ALOGW("%s(), NULL!! return", __FUNCTION__);
        return -EFAULT;
    }

    /* enable ap write flag */
    mShareMemory->ap_flag |= SPH_SHM_AP_FLAG_SPH_PARAM_WRITE;

    if (mShareMemory->md_flag & SPH_SHM_MD_FLAG_SPH_PARAM_READ) {
        ALOGE("%s(), modem still read!! md_flag: 0x%x", __FUNCTION__,
              mShareMemory->md_flag);
        WARNING("md_flag error!!");
        mShareMemory->ap_flag &= (~SPH_SHM_AP_FLAG_SPH_PARAM_WRITE);
        return -EBUSY;
    }


    /* sph_param */
    mShareMemory->region.sph_param_region.read_idx = 0;
    mShareMemory->region.sph_param_region.write_idx = 0;

    /* ap data */
    mShareMemory->region.ap_data_region.read_idx = 0;
    mShareMemory->region.ap_data_region.write_idx = 0;

    /* md data */
    mShareMemory->region.md_data_region.read_idx = 0;
    mShareMemory->region.md_data_region.write_idx = 0;

    /* disable ap write flag */
    mShareMemory->ap_flag &= (~SPH_SHM_AP_FLAG_SPH_PARAM_WRITE);

    return 0;
}


uint32_t SpeechMessengerNormal::shm_region_data_count(region_info_t *p_region) {
    if (!p_region) {
        return 0;
    }

    if (p_region->read_idx >= p_region->size) {
        ALOGE("%s(), offset: 0x%x, size: 0x%x, read_idx : 0x%x, write_idx: 0x%x", __FUNCTION__,
              p_region->offset, p_region->size, p_region->read_idx, p_region->write_idx);
        WARNING("read idx error");
        p_region->read_idx %= p_region->size;
    } else if (p_region->write_idx >= p_region->size) {
        ALOGE("%s(), offset: 0x%x, size: 0x%x, read_idx : 0x%x, write_idx: 0x%x", __FUNCTION__,
              p_region->offset, p_region->size, p_region->read_idx, p_region->write_idx);
        WARNING("write idx error");
        p_region->write_idx %= p_region->size;
    }


    uint32_t count = 0;

    if (p_region->write_idx >= p_region->read_idx) {
        count = p_region->write_idx - p_region->read_idx;
    } else {
        count = p_region->size - (p_region->read_idx - p_region->write_idx);
    }

    return count;
}


uint32_t SpeechMessengerNormal::shm_region_free_space(region_info_t *p_region) {
    if (!p_region) {
        return 0;
    }

    uint32_t count = p_region->size - shm_region_data_count(p_region);

    if (count >= MAX_SIZE_OF_ONE_FRAME) {
        count -= MAX_SIZE_OF_ONE_FRAME;
    } else {
        count = 0;
    }

    return count;
}


void SpeechMessengerNormal::shm_region_write_from_linear(region_info_t *p_region,
                                                         const void *linear_buf,
                                                         uint32_t count) {
    if (!p_region || !linear_buf || !mShareMemory) {
        return;
    }

    if (p_region->read_idx >= p_region->size) {
        ALOGE("%s(), offset: 0x%x, size: 0x%x, read_idx : 0x%x, write_idx: 0x%x, count: 0x%x", __FUNCTION__,
              p_region->offset, p_region->size, p_region->read_idx, p_region->write_idx, count);
        WARNING("read idx error");
        p_region->read_idx %= p_region->size;
    } else if (p_region->write_idx >= p_region->size) {
        ALOGE("%s(), offset: 0x%x, size: 0x%x, read_idx : 0x%x, write_idx: 0x%x, count: 0x%x", __FUNCTION__,
              p_region->offset, p_region->size, p_region->read_idx, p_region->write_idx, count);
        WARNING("write idx error");
        p_region->write_idx %= p_region->size;
    }


    SPH_LOG_V("%s(+), offset: 0x%x, size: 0x%x, read_idx : 0x%x, write_idx: 0x%x, count: 0x%x", __FUNCTION__,
              p_region->offset, p_region->size, p_region->read_idx, p_region->write_idx, count);

    uint32_t free_space = shm_region_free_space(p_region);
    uint8_t *p_buf = ((uint8_t *)mShareMemory) + p_region->offset;

    ASSERT(free_space >= count);

    if (p_region->read_idx <= p_region->write_idx) {
        uint32_t w2e = p_region->size - p_region->write_idx;
        if (count <= w2e) {
            sph_memcpy(p_buf + p_region->write_idx, linear_buf, count);
            p_region->write_idx += count;
            if (p_region->write_idx == p_region->size) {
                p_region->write_idx = 0;
            }
        } else {
            sph_memcpy(p_buf + p_region->write_idx, linear_buf, w2e);
            sph_memcpy(p_buf, (uint8_t *)linear_buf + w2e, count - w2e);
            p_region->write_idx = count - w2e;
        }
    } else {
        sph_memcpy(p_buf + p_region->write_idx, linear_buf, count);
        p_region->write_idx += count;
    }

    SPH_LOG_V("%s(-), offset: 0x%x, size: 0x%x, read_idx : 0x%x, write_idx: 0x%x, count: 0x%x", __FUNCTION__,
              p_region->offset, p_region->size, p_region->read_idx, p_region->write_idx, count);
}




void SpeechMessengerNormal::shm_region_read_to_linear(void *linear_buf,
                                                      region_info_t *p_region,
                                                      uint32_t count) {
    if (!p_region || !linear_buf || !mShareMemory) {
        return;
    }

    if (p_region->read_idx >= p_region->size) {
        ALOGE("%s(), offset: 0x%x, size: 0x%x, read_idx : 0x%x, write_idx: 0x%x, count: 0x%x", __FUNCTION__,
              p_region->offset, p_region->size, p_region->read_idx, p_region->write_idx, count);
        WARNING("read idx error");
        p_region->read_idx %= p_region->size;
    } else if (p_region->write_idx >= p_region->size) {
        ALOGE("%s(), offset: 0x%x, size: 0x%x, read_idx : 0x%x, write_idx: 0x%x, count: 0x%x", __FUNCTION__,
              p_region->offset, p_region->size, p_region->read_idx, p_region->write_idx, count);
        WARNING("write idx error");
        p_region->write_idx %= p_region->size;
    }


    SPH_LOG_V("%s(+), offset: 0x%x, size: 0x%x, read_idx : 0x%x, write_idx: 0x%x, count: 0x%x", __FUNCTION__,
              p_region->offset, p_region->size, p_region->read_idx, p_region->write_idx, count);

    uint32_t available_count = shm_region_data_count(p_region);
    uint8_t *p_buf = ((uint8_t *)mShareMemory) + p_region->offset;

    ASSERT(count <= available_count);

    if (p_region->read_idx <= p_region->write_idx) {
        sph_memcpy(linear_buf, p_buf + p_region->read_idx, count);
        p_region->read_idx += count;
    } else {
        uint32_t r2e = p_region->size - p_region->read_idx;
        if (r2e >= count) {
            sph_memcpy(linear_buf, p_buf + p_region->read_idx, count);
            p_region->read_idx += count;
            if (p_region->read_idx == p_region->size) {
                p_region->read_idx = 0;
            }
        } else {
            sph_memcpy(linear_buf, p_buf + p_region->read_idx, r2e);
            sph_memcpy((uint8_t *)linear_buf + r2e, p_buf, count - r2e);
            p_region->read_idx = count - r2e;
        }
    }

    SPH_LOG_V("%s(-), offset: 0x%x, size: 0x%x, read_idx : 0x%x, write_idx: 0x%x, count: 0x%x", __FUNCTION__,
              p_region->offset, p_region->size, p_region->read_idx, p_region->write_idx, count);
}



int SpeechMessengerNormal::writeSphParamToShareMemory(const void *p_sph_param,
                                                      uint16_t sph_param_length,
                                                      uint16_t *p_write_idx) {
#ifndef MTK_CCCI_SHARE_BUFFER_SUPPORT
    return -ENODEV;
#endif

    int retval = 0;

    AL_AUTOLOCK(mShareMemoryLock);

    if (!p_sph_param || !p_write_idx || !mShareMemory) {
        ALOGW("%s(), NULL!! return", __FUNCTION__);
        return -EFAULT;
    }

    mShareMemory->ap_flag |= SPH_SHM_AP_FLAG_SPH_PARAM_WRITE;

    if (mShareMemory->md_flag & SPH_SHM_MD_FLAG_SPH_PARAM_READ) {
        ALOGW("%s(), modem still read!! md_flag: 0x%x", __FUNCTION__,
              mShareMemory->md_flag);
        mShareMemory->ap_flag &= (~SPH_SHM_AP_FLAG_SPH_PARAM_WRITE);
        return -EBUSY;
    }

    region_info_t *p_region = &mShareMemory->region.sph_param_region;
    uint16_t free_space = (uint16_t)shm_region_free_space(p_region);

    if (sph_param_length > free_space) {
        ALOGW("%s(), sph_param_length %u > free_space %u!!", __FUNCTION__,
              sph_param_length, free_space);
        return -ENOMEM;
    }

    /* keep the data index before write */
    *p_write_idx = (uint16_t)p_region->write_idx;

    /* write sph param */
    shm_region_write_from_linear(p_region, p_sph_param, sph_param_length);

    mShareMemory->ap_flag &= (~SPH_SHM_AP_FLAG_SPH_PARAM_WRITE);
    return 0;
}


int SpeechMessengerNormal::writeApDataToShareMemory(const void *p_data_buf,
                                                    uint16_t data_type,
                                                    uint16_t data_size,
                                                    uint16_t *p_payload_length,
                                                    uint32_t *p_write_idx) {
#ifndef MTK_CCCI_SHARE_BUFFER_SUPPORT
    return -ENODEV;
#endif

    int retval = 0;

    AL_AUTOLOCK(mShareMemoryLock);

    if (!p_data_buf || !p_payload_length || !p_write_idx || !mShareMemory) {
        ALOGW("%s(), NULL!! return", __FUNCTION__);
        return -EFAULT;
    }

    region_info_t *p_region = &mShareMemory->region.ap_data_region;

    uint16_t payload_length = CCCI_MAX_AP_PAYLOAD_HEADER_SIZE + data_size;
    uint16_t free_space = (uint16_t)shm_region_free_space(p_region);

    if (payload_length > free_space) {
        ALOGW("%s(), payload_length %u > free_space %u!!", __FUNCTION__,
              payload_length, free_space);
        *p_payload_length = 0;
        return -ENOMEM;
    }

    /* keep the data index before write */
    *p_write_idx = p_region->write_idx;

    /* write header */
    uint16_t header[3];
    header[0] = CCCI_AP_PAYLOAD_SYNC;
    header[1] = data_type;
    header[2] = data_size;
    shm_region_write_from_linear(p_region, header, CCCI_MAX_AP_PAYLOAD_HEADER_SIZE);

    /* write data */
    shm_region_write_from_linear(p_region, p_data_buf, data_size);

    *p_payload_length = payload_length;
    return 0;
}


int SpeechMessengerNormal::readMdDataFromShareMemory(void *p_data_buf,
                                                     uint16_t *p_data_type,
                                                     uint16_t *p_data_size,
                                                     uint16_t payload_length,
                                                     uint32_t read_idx) {
#ifndef MTK_CCCI_SHARE_BUFFER_SUPPORT
    return -ENODEV;
#endif

    int retval = 0;

    AL_AUTOLOCK(mShareMemoryLock);

    if (!p_data_buf || !p_data_type || !p_data_size || !mShareMemory) {
        ALOGW("%s(), NULL!! return", __FUNCTION__);
        return -EFAULT;
    }

    region_info_t *p_region = &mShareMemory->region.md_data_region;

    uint16_t data_size = payload_length - CCCI_MAX_MD_PAYLOAD_HEADER_SIZE;
    uint32_t available_count = shm_region_data_count(p_region);

    if (data_size > *p_data_size) {
        ALOGW("%s(), data_size %u > p_data_buf size %u!!", __FUNCTION__,
              data_size, *p_data_size);
        *p_data_size = 0;
        WARNING("-ENOMEM");
        return -ENOMEM;
    }

    if (payload_length > available_count) {
        ALOGW("%s(), payload_length %u > available_count %u!!", __FUNCTION__,
              payload_length, available_count);
        *p_data_size = 0;
        return -ENOMEM;
    }

    /* check read index */
    if (read_idx != p_region->read_idx) {
        ALOGW("%s(), read_idx 0x%x != p_region->read_idx 0x%x!!", __FUNCTION__,
              read_idx, p_region->read_idx);
        WARNING("bad read_idx!!");
        p_region->read_idx = read_idx;
    }

    /* read header */
    uint16_t header[5];
    shm_region_read_to_linear(header, p_region, CCCI_MAX_MD_PAYLOAD_HEADER_SIZE);

    if (header[0] != CCCI_MD_PAYLOAD_SYNC ||
        header[2] != data_size ||
        header[3] != header[4]) {
        ALOGE("%s(), sync: 0x%x, type: %d, size: 0x%x, idx: %d, total_idx: %d",
              __FUNCTION__, header[0], header[1], header[2], header[3], header[4]);
        WARNING("md data header error");
        *p_data_size = 0;
        return -EINVAL;
    }

    *p_data_type = header[1];


    /* read data */
    shm_region_read_to_linear(p_data_buf, p_region, data_size);

    *p_data_size = data_size;
    return 0;

}


} // end of namespace android

