#include <SpeechDriverNormal.h>

#include <string.h>

#include <errno.h>

#include <pthread.h>

#include <utils/threads.h> /*  for ANDROID_PRIORITY_AUDIO */

#include <cutils/properties.h> /* for PROPERTY_KEY_MAX */

#include <system/audio.h>

#include <audio_time.h>

#include <AudioLock.h>

#include <SpeechUtility.h>

#include <SpeechMessageID.h>


#include <SpeechMessageQueue.h>
#include <SpeechMessengerNormal.h>

#ifdef MTK_AURISYS_PHONE_CALL_SUPPORT
#include <AudioMessengerIPI.h>
#endif


#include <AudioVolumeFactory.h>
#include <SpeechBGSPlayer.h>
#include <SpeechVMRecorder.h>
#include <SpeechPcm2way.h>
#include <SpeechDataProcessingHandler.h>

#include <SpeechParamParser.h>
#include <SpeechEnhancementController.h>

#include <WCNChipController.h>

#include <AudioSmartPaController.h>

#include "AudioALSAHardwareResourceManager.h"

#include <AudioVIBSPKControl.h>



#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "SpeechDriverNormal"




namespace android {


/*
 * =============================================================================
 *                     typedef
 * =============================================================================
 */

#define USE_DEDICATED_LOOPBACK_DELAY_FRAMES (true)
#define MAX_LOOPBACK_DELAY_FRAMES (64)
#define DEFAULT_LOOPBACK_DELAY_FRAMES (12) /* 12 frames => 240 ms */

#define MAX_SPEECH_AUTO_LOCK_TIMEOUT_MS (1000)

#define TEMP_CCCI_MD_PAYLOAD_SYNC (0x1234)


#define MAX_VM_RECORD_SIZE      (0x4000) // 7500 * 2 => 16K
#define MAX_RAW_RECORD_SIZE     (0x1000) // 1924 * 2 => 4K
#define MAX_PNW_UL_SIZE         (0x800)  //  960 * 2 => 2K
#define MAX_TTY_DEBUG_SIZE      (0x200)  //  160 * 2 => 512 bytes

#define MAX_PARSED_RECORD_SIZE  (MAX_RAW_RECORD_SIZE)


#define MAX_SPEECH_PARAM_CONCATE_SIZE   (SPEECH_SHM_SPEECH_PARAM_SIZE)

static const uint16_t kSphParamSize[NUM_AUDIO_TYPE_SPEECH_TYPE] = {
    0x3000, // AUDIO_TYPE_SPEECH,             18 + (2 + 352 * 2) * 12 => 12K
    0x400,  // AUDIO_TYPE_SPEECH_DMNR,        18 +      488 * 2       =>  1K
    128,    // AUDIO_TYPE_SPEECH_GENERAL,     18 +       28 * 2       => 128
    128,    // AUDIO_TYPE_SPEECH_MAGICLARITY, 18 +       32 * 2       => 128
    0,      // AUDIO_TYPE_SPEECH_NETWORK,     unused                  =>   0
    32      // AUDIO_TYPE_SPEECH_ECHOREF,     18 +   3 * 2            =>  32
};


#define MAX_MSG_PROCESS_TIME_MS (10)


/*
 * =============================================================================
 *                     global
 * =============================================================================
 */

/* keep modem status to recovery when audioserver die */
static const char kPropertyKeyModemStatus[PROPERTY_KEY_MAX] = "af.modem_1.status";
static const char kPropertyKeyModemEPOF[PROPERTY_KEY_MAX]   = "af.modem_1.epof";


/* from MSG_M2A_NETWORK_STATUS_NOTIFY */
static const char kPropertyKeyRfMode[PROPERTY_KEY_MAX] = "af.rf_mode";

/* from MSG_M2A_NW_CODEC_INFO_NOTIFY */
static const char kPropertyKeyRilSphCodecInfo[PROPERTY_KEY_MAX] = "af.ril.speech.codec.info";
static const char kPropertyKeyRilHdVoiceStatus[PROPERTY_KEY_MAX] = "af.ril.hd.voice.status";


/*
 * =============================================================================
 *                     Macro
 * =============================================================================
 */



/*
 * =============================================================================
 *                     Singleton Pattern
 * =============================================================================
 */

SpeechDriverNormal *SpeechDriverNormal::mSpeechDriver = NULL;

SpeechDriverNormal *SpeechDriverNormal::GetInstance(modem_index_t modem_index) {
    static AudioLock mGetInstanceLock;

    AL_AUTOLOCK(mGetInstanceLock);

    if (modem_index != MODEM_1) {
        ALOGE("%s(), modem_index %d not support!!", __FUNCTION__, modem_index);
        ASSERT(modem_index == MODEM_1);
        return NULL;
    }

    if (mSpeechDriver == NULL) {
        mSpeechDriver = new SpeechDriverNormal(modem_index);
    }
    return mSpeechDriver;
}



/*==============================================================================
 *                     Constructor / Destructor / Init / Deinit
 *============================================================================*/

SpeechDriverNormal::SpeechDriverNormal(modem_index_t modem_index) {
    mModemIndex = modem_index;

    mSpeechMessenger = new SpeechMessengerNormal(mModemIndex);
    if (mSpeechMessenger == NULL) {
        ALOGE("%s(), mSpeechMessenger == NULL!!", __FUNCTION__);
    } else {
        if (get_uint32_from_property(kPropertyKeyModemEPOF) != 0) {
            if (mSpeechMessenger->checkModemAlive() == true) {
                ALOGD("%s(), md alive, reset EPOF", __FUNCTION__);
                set_uint32_to_property(kPropertyKeyModemEPOF, 0);
            }
        }

        kMaxApPayloadDataSize = mSpeechMessenger->getMaxApPayloadDataSize();
        kMaxMdPayloadDataSize = mSpeechMessenger->getMaxMdPayloadDataSize();

        AUDIO_ALLOC_BUFFER(mBgsBuf, kMaxApPayloadDataSize);
        AUDIO_ALLOC_BUFFER(mVmRecBuf, MAX_VM_RECORD_SIZE);
        AUDIO_ALLOC_BUFFER(mRawRecBuf, MAX_RAW_RECORD_SIZE);
        AUDIO_ALLOC_BUFFER(mParsedRecBuf, MAX_PARSED_RECORD_SIZE);
        AUDIO_ALLOC_BUFFER(mP2WUlBuf, MAX_PNW_UL_SIZE);
        AUDIO_ALLOC_BUFFER(mP2WDlBuf, kMaxApPayloadDataSize);
        AUDIO_ALLOC_BUFFER(mTtyDebugBuf, MAX_TTY_DEBUG_SIZE);
    }

    mSampleRateEnum = SPH_SAMPLE_RATE_32K;

    mApplication = SPH_APPLICATION_INVALID;
    mSpeechMode = SPEECH_MODE_NORMAL;
    mInputDevice = AUDIO_DEVICE_IN_BUILTIN_MIC;
    mOutputDevice = AUDIO_DEVICE_OUT_EARPIECE;

    mModemLoopbackDelayFrames = DEFAULT_LOOPBACK_DELAY_FRAMES;


    // Record capability
    mRecordSampleRateType = RECORD_SAMPLE_RATE_08K;
    mRecordChannelType    = RECORD_CHANNEL_MONO;
    mRecordType = RECORD_TYPE_MIX;
    mVolumeIndex = 0x3;

    mTtyDebugEnable = false;
    mApResetDuringSpeech = false;
    mModemResetDuringSpeech = false;


    // init var
    mEnableThread = false;
    mEnableThreadDuringSpeech = false;

    hReadSpeechMessageThread = 0;
    hModemStatusMonitorThread = 0;


    // BT Headset NREC
    mBtHeadsetNrecOn = SpeechEnhancementController::GetInstance()->GetBtHeadsetNrecOn();

    // RTT
    mRttMode = 0;

    memset(&mSpeechParam, 0, sizeof(sph_param_buf_t));
    for (int i = 0; i < NUM_AUDIO_TYPE_SPEECH_TYPE; i++) {
        if (kSphParamSize[i] > 0) {
            mSpeechParam[i].memory_size = kSphParamSize[i];
            mSpeechParam[i].data_size   = 0;
            AUDIO_ALLOC_BUFFER(mSpeechParam[i].p_buffer, mSpeechParam[i].memory_size);
        } else {
            mSpeechParam[i].memory_size = 0;
            mSpeechParam[i].data_size   = 0;
            mSpeechParam[i].p_buffer    = NULL;
        }
    }
    AUDIO_ALLOC_BUFFER(mSpeechParamConcat, MAX_SPEECH_PARAM_CONCATE_SIZE);

    mSpeechMessageQueue = new SpeechMessageQueue(sendSpeechMessageToModemWrapper,
                                                 errorHandleSpeechMessageWrapper,
                                                 this);

    createThreads();

    // initial modem side modem status
    mModemSideModemStatus = get_uint32_from_property(kPropertyKeyModemStatus);
    RecoverModemSideStatusToInitState();
}


SpeechDriverNormal::~SpeechDriverNormal() {
    joinThreads();

    if (mSpeechMessageQueue) {
        delete mSpeechMessageQueue;
        mSpeechMessageQueue = NULL;
    }

    AUDIO_FREE_POINTER(mBgsBuf);
    AUDIO_FREE_POINTER(mVmRecBuf);
    AUDIO_FREE_POINTER(mRawRecBuf);
    AUDIO_FREE_POINTER(mParsedRecBuf);
    AUDIO_FREE_POINTER(mP2WUlBuf);
    AUDIO_FREE_POINTER(mP2WDlBuf);
    AUDIO_FREE_POINTER(mTtyDebugBuf);

    for (int i = 0; i < NUM_AUDIO_TYPE_SPEECH_TYPE; i++) {
        AUDIO_FREE_POINTER(mSpeechParam[i].p_buffer);
        mSpeechParam[i].memory_size = 0;
        mSpeechParam[i].data_size   = 0;
    }
    AUDIO_FREE_POINTER(mSpeechParamConcat);
}


/*==============================================================================
 *                     modem status
 *============================================================================*/

bool SpeechDriverNormal::getModemSideModemStatus(
    const modem_status_mask_t modem_status_mask) const {
    return ((mModemSideModemStatus & modem_status_mask) > 0);
}


void SpeechDriverNormal::setModemSideModemStatus(const modem_status_mask_t modem_status_mask) {
    AL_AUTOLOCK(mModemSideModemStatusLock);

    if (getModemSideModemStatus(modem_status_mask) == true) {
        ALOGE("%s(), modem_status_mask: 0x%x already enabled!!", __FUNCTION__, modem_status_mask);
        ASSERT(getModemSideModemStatus(modem_status_mask) == false);
        return;
    }

    mModemSideModemStatus |= modem_status_mask;

    // save mModemSideModemStatus in property to avoid medieserver die
    set_uint32_to_property(kPropertyKeyModemStatus, mModemSideModemStatus);
}


void SpeechDriverNormal::resetModemSideModemStatus(const modem_status_mask_t modem_status_mask) {
    AL_AUTOLOCK(mModemSideModemStatusLock);

    if (getModemSideModemStatus(modem_status_mask) == false) {
        ALOGE("%s(), modem_status_mask: 0x%x not enabled!!", __FUNCTION__, modem_status_mask);
        ASSERT(getModemSideModemStatus(modem_status_mask) == true);
        return;
    }

    mModemSideModemStatus &= (~modem_status_mask);

    // save mModemSideModemStatus in property to avoid medieserver die
    set_uint32_to_property(kPropertyKeyModemStatus, mModemSideModemStatus);
}


void SpeechDriverNormal::cleanAllModemSideModemStatus() {
    AL_AUTOLOCK(mModemSideModemStatusLock);

    ALOGD("%s(), mModemSideModemStatus: 0x%x to be clean", __FUNCTION__, mModemSideModemStatus);
    mModemSideModemStatus = 0;

    set_uint32_to_property(kPropertyKeyModemStatus, mModemSideModemStatus);
}



/*==============================================================================
 *                     msg
 *============================================================================*/

int SpeechDriverNormal::configSpeechInfo(sph_info_t *p_sph_info) {
    int retval = 0;

    if (p_sph_info == NULL) {
        return -EFAULT;
    }

    ASSERT(sizeof(sph_info_t) == SIZE_OF_SPH_INFO);
    memset(p_sph_info, 0, sizeof(sph_info_t));

    /* application */
    p_sph_info->application = mApplication;

    /* bt_info */
    const bool bt_device_on = audio_is_bluetooth_sco_device(mOutputDevice);
    if (bt_device_on == false) {
        p_sph_info->bt_info = SPH_BT_OFF;
    } else {
        if (WCNChipController::GetInstance()->IsBTMergeInterfaceSupported() == true) {
            p_sph_info->bt_info = SPH_BT_PCM;
        } else {
            p_sph_info->bt_info = SPH_BT_CVSD_MSBC;
        }
    }

    /* sample_rate_enum */
    p_sph_info->sample_rate_enum = mSampleRateEnum;

    /* param */
#if defined(MTK_AURISYS_PHONE_CALL_SUPPORT)
    p_sph_info->opendsp_flag = true;
    p_sph_info->sph_param_path = SPH_PARAM_VIA_PAYLOAD;
    p_sph_info->sph_param_valid = false; /* bypass sph param for opendsp */
    p_sph_info->sph_param_length = 0;
    p_sph_info->sph_param_index = 0;
#else
    p_sph_info->opendsp_flag = false;

    retval = writeAllSpeechParametersToModem(&p_sph_info->sph_param_length, &p_sph_info->sph_param_index);
    if (retval == 0) {
        p_sph_info->sph_param_path = SPH_PARAM_VIA_SHM;
        p_sph_info->sph_param_valid = true;
    } else {
        p_sph_info->sph_param_path = SPH_PARAM_VIA_PAYLOAD;
        p_sph_info->sph_param_valid = false;
        p_sph_info->sph_param_length = 0;
        p_sph_info->sph_param_index = 0;
    }
#endif

    /* ext_dev_info */
    switch (mOutputDevice) {
#ifdef MTK_AUDIO_SPEAKER_PATH_3_IN_1
    case AUDIO_DEVICE_OUT_EARPIECE:
        p_sph_info->ext_dev_info = SPH_EXT_DEV_INFO_VIBRATION_RECEIVER;
        break;
#endif
    case AUDIO_DEVICE_OUT_SPEAKER:
        if (AudioSmartPaController::getInstance()->isSmartPAUsed()) {
#if defined(MTK_AUDIO_SPEAKER_PATH_2_IN_1) || defined(MTK_AUDIO_SPEAKER_PATH_3_IN_1)
            p_sph_info->ext_dev_info = SPH_EXT_DEV_INFO_SMARTPA_VIBRATION_SPEAKER;
#else
            p_sph_info->ext_dev_info = SPH_EXT_DEV_INFO_SMARTPA_SPEAKER;
#endif
        } else {
#if defined(MTK_AUDIO_SPEAKER_PATH_2_IN_1) || defined(MTK_AUDIO_SPEAKER_PATH_3_IN_1)
            p_sph_info->ext_dev_info = SPH_EXT_DEV_INFO_VIBRATION_SPEAKER;
#else
            p_sph_info->ext_dev_info = SPH_EXT_DEV_INFO_DEFULAT;
#endif /* end of MTK_AUDIO_SPEAKER_PATH_2_IN_1 || MTK_AUDIO_SPEAKER_PATH_3_IN_1 */
        }
        break;
#ifdef MTK_USB_PHONECALL
    case AUDIO_DEVICE_OUT_USB_DEVICE:
        p_sph_info->ext_dev_info = SPH_EXT_DEV_INFO_USB_AUDIO;
        break;
#endif
    case AUDIO_DEVICE_OUT_WIRED_HEADSET:
    case AUDIO_DEVICE_OUT_WIRED_HEADPHONE:
        p_sph_info->ext_dev_info = SPH_EXT_DEV_INFO_EARPHONE;
        break;
    default:
        p_sph_info->ext_dev_info = SPH_EXT_DEV_INFO_DEFULAT;
        break;
    }


    /* loopback */
    if (p_sph_info->application != SPH_APPLICATION_LOOPBACK) {
        p_sph_info->loopback_flag  = 0;
        p_sph_info->loopback_delay = 0;
    } else {
        p_sph_info->loopback_flag = 0;
        /* bt codec */
        if (mUseBtCodec == false) {
            p_sph_info->loopback_flag |= SPH_LOOPBACK_INFO_FLAG_DISABLE_BT_CODEC;
        }
        /* delay ms */
        if (USE_DEDICATED_LOOPBACK_DELAY_FRAMES == true) {
            p_sph_info->loopback_flag |= SPH_LOOPBACK_INFO_FLAG_DELAY_SETTING;
            p_sph_info->loopback_delay = mModemLoopbackDelayFrames;
        } else {
            p_sph_info->loopback_delay = 0;
        }
    }


    /* echo_ref_delay_ms */
    if (p_sph_info->bt_info == SPH_BT_CVSD_MSBC) {
        if (mBtHeadsetNrecOn == false) {
            p_sph_info->echo_ref_delay_ms = 0;
        } else {
            getBtDelayTime(&p_sph_info->echo_ref_delay_ms);
        }
    }
    ASSERT(p_sph_info->echo_ref_delay_ms <= 256); /* modem limitation */

    /* mic_delay_ms */
    switch (p_sph_info->ext_dev_info) {
    case SPH_EXT_DEV_INFO_SMARTPA_SPEAKER:
    case SPH_EXT_DEV_INFO_SMARTPA_VIBRATION_SPEAKER:
        if (AudioSmartPaController::getInstance()->isSmartPAUsed()) {
            p_sph_info->mic_delay_ms = AudioSmartPaController::getInstance()->getSmartPaDelayUs() / 1000;
        } else {
            p_sph_info->mic_delay_ms = 0;
        }
        break;
#ifdef MTK_USB_PHONECALL
    case SPH_EXT_DEV_INFO_USB_AUDIO:
        getUsbDelayTime(&p_sph_info->mic_delay_ms);
        break;
#endif
    default:
        p_sph_info->mic_delay_ms = 0;
    }
    ASSERT(p_sph_info->mic_delay_ms <= 64); /* modem limitation */


    /* speech enhancement function dynamic mask */
    sph_enh_mask_struct_t mask = SpeechEnhancementController::GetInstance()->GetSpeechEnhancementMask();
    p_sph_info->enh_dynamic_ctrl = speechEnhancementMaskWrapper(mask.dynamic_func);


    /* dump info */
    ALOGD("%s(), app: %d, bt: %d, rate enum: %d, opendsp: %d, path: %d, param emi valid: %d, param size: 0x%x, param index: 0x%x"
          ", ext_dev_info: %d, loopback_flag: 0x%x, loopback_delay: %d, aec delay: %d, mic delay: %d, enh_dynamic_ctrl: 0x%x",
          __FUNCTION__,
          p_sph_info->application,
          p_sph_info->bt_info,
          p_sph_info->sample_rate_enum,
          p_sph_info->opendsp_flag,
          p_sph_info->sph_param_path,
          p_sph_info->sph_param_valid,
          p_sph_info->sph_param_length,
          p_sph_info->sph_param_index,
          p_sph_info->ext_dev_info,
          p_sph_info->loopback_flag,
          p_sph_info->loopback_delay,
          p_sph_info->echo_ref_delay_ms,
          p_sph_info->mic_delay_ms,
          p_sph_info->enh_dynamic_ctrl);


    return 0;
}


int SpeechDriverNormal::configMailBox(
    sph_msg_t *p_sph_msg,
    uint16_t msg_id,
    uint16_t param_16bit,
    uint32_t param_32bit) {

    if (p_sph_msg == NULL) {
        return -EFAULT;
    }

    memset(p_sph_msg, 0, sizeof(sph_msg_t));

    p_sph_msg->buffer_type = SPH_MSG_BUFFER_TYPE_MAILBOX;
    p_sph_msg->msg_id = msg_id;
    p_sph_msg->param_16bit = param_16bit;
    p_sph_msg->param_32bit = param_32bit;

    return 0;
}


int SpeechDriverNormal::configPayload(
    sph_msg_t *p_sph_msg,
    uint16_t msg_id,
    uint16_t data_type,
    void    *data_addr,
    uint16_t data_size) {

    if (p_sph_msg == NULL) {
        return -EFAULT;
    }

    memset(p_sph_msg, 0, sizeof(sph_msg_t));

    p_sph_msg->buffer_type = SPH_MSG_BUFFER_TYPE_PAYLOAD;
    p_sph_msg->msg_id = msg_id;

    p_sph_msg->payload_data_type = data_type;
    p_sph_msg->payload_data_size = data_size;
    p_sph_msg->payload_data_addr = data_addr;

    return 0;
}


int SpeechDriverNormal::sendMailbox(sph_msg_t *p_sph_msg,
                                    uint16_t msg_id,
                                    uint16_t param_16bit,
                                    uint32_t param_32bit) {
    configMailBox(p_sph_msg, msg_id, param_16bit, param_32bit);
    if (isApMsgBypassQueue(p_sph_msg) == true) {
        return sendSpeechMessageToModem(p_sph_msg);
    } else {
        return sendSpeechMessageToQueue(p_sph_msg);
    }
}


int SpeechDriverNormal::sendPayload(sph_msg_t *p_sph_msg,
                                    uint16_t msg_id,
                                    uint16_t data_type,
                                    void    *data_buf,
                                    uint16_t data_size) {
    configPayload(p_sph_msg, msg_id, data_type, data_buf, data_size);
    if (isApMsgBypassQueue(p_sph_msg) == true) {
        return sendSpeechMessageToModem(p_sph_msg);
    } else {
        return sendSpeechMessageToQueue(p_sph_msg);
    }
}



/*==============================================================================
 *                     queue
 *============================================================================*/

int SpeechDriverNormal::sendSpeechMessageToQueue(sph_msg_t *p_sph_msg) {

    /* error handling */
    if (p_sph_msg == NULL) {
        ALOGW("%s(), p_sph_msg == NULL!! return", __FUNCTION__);
        return -EFAULT;
    }

    if (mSpeechMessageQueue == NULL) {
        ALOGW("%s(), mSpeechMessageQueue == NULL!! return", __FUNCTION__);
        return -EFAULT;
    }

    uint32_t block_thread_ms = getBlockThreadTimeMsByID(p_sph_msg);
    return mSpeechMessageQueue->sendSpeechMessageToQueue(p_sph_msg, block_thread_ms);
}


int SpeechDriverNormal::sendSpeechMessageAckToQueue(sph_msg_t *p_sph_msg) {

    /* error handling */
    if (p_sph_msg == NULL) {
        ALOGW("%s(), p_sph_msg == NULL!! return", __FUNCTION__);
        return -EFAULT;
    }

    if (isMdAckBack(p_sph_msg) == false) {
        ALOGW("%s(), isMdAckBack(0x%x) failed!! return", __FUNCTION__, p_sph_msg->msg_id);
        return -EFAULT;
    }

    if (mSpeechMessageQueue == NULL) {
        ALOGW("%s(), mSpeechMessageQueue == NULL!! return", __FUNCTION__);
        return -EFAULT;
    }

    return mSpeechMessageQueue->sendSpeechMessageAckToQueue(p_sph_msg);
}


int SpeechDriverNormal::sendSpeechMessageToModemWrapper(void *arg, sph_msg_t *p_sph_msg) {
    SpeechDriverNormal *pSpeechDriver = static_cast<SpeechDriverNormal *>(arg);

    if (pSpeechDriver == NULL) {
        ALOGE("%s(), static_cast failed!!", __FUNCTION__);
        return -EMEDIUMTYPE;
    }

    if (p_sph_msg == NULL) {
        ALOGW("%s(), p_sph_msg == NULL!! return", __FUNCTION__);
        return -EFAULT;
    }

    return pSpeechDriver->sendSpeechMessageToModem(p_sph_msg);
}


int SpeechDriverNormal::sendSpeechMessageToModem(sph_msg_t *p_sph_msg) {
    /* only config modem error state here to using lock to protect it */
    static AudioLock send_message_lock;
    static bool b_epof = (get_uint32_from_property(kPropertyKeyModemEPOF) != 0);
    static bool b_during_call = false;
    static bool b_modem_crash_during_call = false;

    int retval = 0;

    AL_AUTOLOCK_MS(send_message_lock, MAX_SPEECH_AUTO_LOCK_TIMEOUT_MS);

    /* error handling */
    if (p_sph_msg == NULL) {
        ALOGW("%s(), p_sph_msg == NULL!! return", __FUNCTION__);
        return -EFAULT;
    }

    if (mSpeechMessenger == NULL) {
        ALOGW("%s(), mSpeechMessenger == NULL!! return", __FUNCTION__);
        return -EFAULT;
    }


    /* send message to modem */
    if ((b_epof == true || b_modem_crash_during_call == true || mModemResetDuringSpeech == true) &&
        p_sph_msg->msg_id != MSG_A2M_MD_ALIVE_ACK_BACK) {
        ALOGW("%s(), b_epof: %d, b_modem_crash_during_call: %d, mModemResetDuringSpeech: %d!! bypass msg 0x%x", __FUNCTION__,
              b_epof, b_modem_crash_during_call, mModemResetDuringSpeech, p_sph_msg->msg_id);
        retval = -EPIPE;
    } else {
        retval = mSpeechMessenger->sendSpeechMessage(p_sph_msg);
    }

    /* config modem state for error handling */
    switch (p_sph_msg->msg_id) {
    case MSG_A2M_SPH_ON:
        b_during_call = true;
        break;
    case MSG_A2M_SPH_OFF:
        /* this call is end, suppose modem will be recovered before next call */
        b_modem_crash_during_call = false;
        b_during_call = false;
        break;
    case MSG_A2M_EPOF_ACK:
        /* enable EPOF only after EPOF ack is sent to modem!! */
        b_epof = true;
        set_uint32_to_property(kPropertyKeyModemEPOF, b_epof);
        break;
    case MSG_A2M_MD_ALIVE_ACK_BACK:
        /* disable EPOF */
        b_epof = false;
        set_uint32_to_property(kPropertyKeyModemEPOF, b_epof);
        break;
    default:
        break;
    }

    if (retval == 0) {
        if (isNeedDumpMsg(p_sph_msg) == true) {
            PRINT_SPH_MSG(ALOGD, "send msg done", p_sph_msg);
        } else {
            PRINT_SPH_MSG(SPH_LOG_D, "send msg done", p_sph_msg);
        }
    } else if (retval != 0) {
        PRINT_SPH_MSG(ALOGE, "send msg failed!!", p_sph_msg);
        if (b_during_call == true) {
            /* notate whether modem crashed during phone call or not */
            /* cannot use GetApSideModemStatus because need lock protect it */
            b_modem_crash_during_call = true;
        }
    }

    return retval;
}


int SpeechDriverNormal::errorHandleSpeechMessageWrapper(void *arg, sph_msg_t *p_sph_msg) {
    SpeechDriverNormal *pSpeechDriver = static_cast<SpeechDriverNormal *>(arg);

    if (pSpeechDriver == NULL) {
        ALOGE("%s(), static_cast failed!!", __FUNCTION__);
        return -EMEDIUMTYPE;
    }

    if (p_sph_msg == NULL) {
        ALOGW("%s(), p_sph_msg == NULL!! return", __FUNCTION__);
        return -EFAULT;
    }

    return pSpeechDriver->errorHandleSpeechMessage(p_sph_msg);
}


int SpeechDriverNormal::errorHandleSpeechMessage(sph_msg_t *p_sph_msg) {
    /* error handling */
    if (p_sph_msg == NULL) {
        ALOGW("%s(), p_sph_msg == NULL!! return", __FUNCTION__);
        return -EFAULT;
    }

    int retval = 0;

    if (getSyncType(p_sph_msg->msg_id) != SPH_MSG_HANDSHAKE_AP_CTRL_NEED_ACK) {
        PRINT_SPH_MSG(ALOGD, "no need ack. return", p_sph_msg);
        return 0;
    }

    retval = makeFakeMdAckMsgFromApMsg(p_sph_msg);
    if (retval != 0) {
        PRINT_SPH_MSG(ALOGW, "make fake modem ack error!! return", p_sph_msg);
        return retval;
    }

    PRINT_SPH_MSG(ALOGD, "make fake modem ack", p_sph_msg);
    retval = processModemAckMessage(p_sph_msg);

    return retval;
}


int SpeechDriverNormal::readSpeechMessageFromModem(sph_msg_t *p_sph_msg) {
    int retval = 0;

    AL_AUTOLOCK_MS(mReadMessageLock, MAX_SPEECH_AUTO_LOCK_TIMEOUT_MS);

    /* error handling */
    if (p_sph_msg == NULL) {
        ALOGW("%s(), p_sph_msg == NULL!! return", __FUNCTION__);
        return -EFAULT;
    }

    if (mSpeechMessenger == NULL) {
        ALOGW("%s(), mSpeechMessenger == NULL!! return", __FUNCTION__);
        return -EFAULT;
    }

    SPH_LOG_D("%s(+)", __FUNCTION__);
    retval = mSpeechMessenger->readSpeechMessage(p_sph_msg);
    SPH_LOG_D("%s(-), msg id 0x%x", __FUNCTION__, p_sph_msg->msg_id);

    return retval;
}


/*==============================================================================
 *                     thread
 *============================================================================*/

void SpeechDriverNormal::createThreads() {
    int ret = 0;

    mEnableThread = true;
    ret = pthread_create(&hReadSpeechMessageThread, NULL,
                         SpeechDriverNormal::readSpeechMessageThread,
                         (void *)this);
    ASSERT(ret == 0);
}


void SpeechDriverNormal::joinThreads() {
    mEnableThread = false;

    pthread_join(hReadSpeechMessageThread, NULL);
}


void *SpeechDriverNormal::readSpeechMessageThread(void *arg) {
    SpeechDriverNormal *pSpeechDriver = NULL;
    sph_msg_t sph_msg;
    int retval = 0;

    char thread_name[128] = {0};
    CONFIG_THREAD(thread_name, ANDROID_PRIORITY_AUDIO);

    pSpeechDriver = static_cast<SpeechDriverNormal *>(arg);
    if (pSpeechDriver == NULL) {
        ALOGE("%s(), NULL!! pSpeechDriver %p", __FUNCTION__, pSpeechDriver);
        goto READ_MSG_THREAD_DONE;
    }

    while (pSpeechDriver->mEnableThread == true) {
        /* wait until modem message comes */
        memset(&sph_msg, 0, sizeof(sph_msg_t));
        retval = pSpeechDriver->readSpeechMessageFromModem(&sph_msg);
        if (retval != 0) {
            ALOGV("%s(), readSpeechMessageFromModem failed!!", __FUNCTION__);
            usleep(100 * 1000);
            continue;
        }

        pSpeechDriver->processModemMessage(&sph_msg);
    }


READ_MSG_THREAD_DONE:
    ALOGV("%s terminated", thread_name);
    pthread_exit(NULL);
    return NULL;
}


void SpeechDriverNormal::createThreadsDuringSpeech() {
    int ret = 0;

    mEnableThreadDuringSpeech = true;
    ret = pthread_create(&hModemStatusMonitorThread, NULL,
                         SpeechDriverNormal::modemStatusMonitorThread,
                         (void *)this);
    ASSERT(ret == 0);
}


void SpeechDriverNormal::joinThreadsDuringSpeech() {
    if (mEnableThreadDuringSpeech == true) {
        AL_LOCK_MS(mModemStatusMonitorThreadLock, MAX_SPEECH_AUTO_LOCK_TIMEOUT_MS);
        mEnableThreadDuringSpeech = false;
        AL_SIGNAL(mModemStatusMonitorThreadLock);
        AL_UNLOCK(mModemStatusMonitorThreadLock);

        pthread_join(hModemStatusMonitorThread, NULL);
    }
}


void *SpeechDriverNormal::modemStatusMonitorThread(void *arg) {
    SpeechDriverNormal *pSpeechDriver = NULL;
    SpeechMessageQueue *pSpeechMessageQueue = NULL;

    int retval = 0;

    char thread_name[128] = {0};
    CONFIG_THREAD(thread_name, ANDROID_PRIORITY_AUDIO);

    pSpeechDriver = static_cast<SpeechDriverNormal *>(arg);

    if (pSpeechDriver == NULL) {
        ALOGE("%s(), NULL!! pSpeechDriver %p", __FUNCTION__, pSpeechDriver);
        goto MODEM_STATUS_MONITOR_THREAD_DONE;
    }

    pSpeechMessageQueue = pSpeechDriver->mSpeechMessageQueue;
    if (pSpeechMessageQueue == NULL) {
        ALOGE("%s(), NULL!! pSpeechMessageQueue %p", __FUNCTION__, pSpeechMessageQueue);
        goto MODEM_STATUS_MONITOR_THREAD_DONE;
    }

    while (pSpeechDriver->mEnableThreadDuringSpeech == true) {
        if (pSpeechDriver->CheckModemIsReady() == false) {
            ALOGW("%s(), modem status error!! notify queue", __FUNCTION__);
            pSpeechDriver->mModemResetDuringSpeech = true;
            pSpeechMessageQueue->notifyQueueToStopWaitingAck();
            break;
        }

        AL_LOCK_MS(pSpeechDriver->mModemStatusMonitorThreadLock, MAX_SPEECH_AUTO_LOCK_TIMEOUT_MS);
        AL_WAIT_MS(pSpeechDriver->mModemStatusMonitorThreadLock, 200); // check status each 200 ms
        AL_UNLOCK(pSpeechDriver->mModemStatusMonitorThreadLock);
    }


MODEM_STATUS_MONITOR_THREAD_DONE:
    ALOGV("%s terminated", thread_name);
    pthread_exit(NULL);
    return NULL;
}


/*==============================================================================
 *                     process msg
 *============================================================================*/

int SpeechDriverNormal::processModemMessage(sph_msg_t *p_sph_msg) {
    struct timespec ts_start;
    struct timespec ts_stop;
    uint64_t time_diff_msg = 0;

    int retval = 0;

    /* error handling */
    if (p_sph_msg == NULL) {
        ALOGW("%s(), p_sph_msg == NULL!! return", __FUNCTION__);
        return -EFAULT;
    }

    /* get time for start */
    audio_get_timespec_monotonic(&ts_start);

    /* process modem message */
    switch (getSyncType(p_sph_msg->msg_id)) {
    case SPH_MSG_HANDSHAKE_MD_ACK_BACK_AP_CTRL:
        /* send ack to queue first */
        retval = processModemAckMessage(p_sph_msg);
        sendSpeechMessageAckToQueue(p_sph_msg);
        break;
    case SPH_MSG_HANDSHAKE_MD_CTRL_BYPASS_ACK:
    case SPH_MSG_HANDSHAKE_MD_CTRL_NEED_ACK:
        retval = processModemControlMessage(p_sph_msg);
        break;
    case SPH_MSG_HANDSHAKE_MD_REQUEST_DATA:
    case SPH_MSG_HANDSHAKE_MD_NOTIFY_DATA:
        retval = processModemDataMessage(p_sph_msg);
        break;
    default:
        ALOGW("%s(), p_sph_msg->msg_id 0x%x not support!!", __FUNCTION__, p_sph_msg->msg_id);
        retval = -EINVAL;
    }

    /* get time for stop */
    audio_get_timespec_monotonic(&ts_stop);
    time_diff_msg = get_time_diff_ms(&ts_start, &ts_stop);
    if (time_diff_msg >= MAX_MSG_PROCESS_TIME_MS) {
        ALOGW("%s(), msg 0x%x process time %ju ms is too long", __FUNCTION__,
              p_sph_msg->msg_id, time_diff_msg);
    }

    /* NOTICE: Must copy payload/modem data before return!! */
    return retval;
}


int SpeechDriverNormal::processModemAckMessage(sph_msg_t *p_sph_msg) {
    /* config modem status */
    switch (p_sph_msg->msg_id) {
    case MSG_M2A_MUTE_SPH_UL_ACK:
        break;
    case MSG_M2A_MUTE_SPH_DL_ACK:
        break;
    case MSG_M2A_MUTE_SPH_UL_SOURCE_ACK:
        break;
    case MSG_M2A_SPH_ON_ACK:
        setModemSideModemStatus(SPEECH_STATUS_MASK);
        break;
    case MSG_M2A_SPH_OFF_ACK:
        if (mSpeechMessenger != NULL) { mSpeechMessenger->resetShareMemoryIndex(); }
        joinThreadsDuringSpeech();
        resetModemSideModemStatus(SPEECH_STATUS_MASK);
        break;
    case MSG_M2A_SPH_DEV_CHANGE_ACK:
        break;
    case MSG_M2A_PNW_ON_ACK:
        setModemSideModemStatus(P2W_STATUS_MASK);
        break;
    case MSG_M2A_PNW_OFF_ACK:
        resetModemSideModemStatus(P2W_STATUS_MASK);
        break;
    case MSG_M2A_VM_REC_ON_ACK:
        setModemSideModemStatus(VM_RECORD_STATUS_MASK);
        break;
    case MSG_M2A_VM_REC_OFF_ACK:
        resetModemSideModemStatus(VM_RECORD_STATUS_MASK);
        break;
    case MSG_M2A_RECORD_RAW_PCM_ON_ACK:
        setModemSideModemStatus(RAW_RECORD_STATUS_MASK);
        break;
    case MSG_M2A_RECORD_RAW_PCM_OFF_ACK:
        resetModemSideModemStatus(RAW_RECORD_STATUS_MASK);
        break;
    case MSG_M2A_CTM_ON_ACK:
        setModemSideModemStatus(TTY_STATUS_MASK);
        break;
    case MSG_M2A_CTM_OFF_ACK:
        resetModemSideModemStatus(TTY_STATUS_MASK);
        break;
    case MSG_M2A_BGSND_ON_ACK:
        setModemSideModemStatus(BGS_STATUS_MASK);
        break;
    case MSG_M2A_BGSND_OFF_ACK:
        resetModemSideModemStatus(BGS_STATUS_MASK);
        break;
    case MSG_M2A_EM_DYNAMIC_SPH_ACK:
        break;
    case MSG_M2A_DYNAMIC_PAR_IN_STRUCT_SHM_ACK:
        break;
    case MSG_M2A_VIBSPK_PARAMETER_ACK:
        break;
    case MSG_M2A_SMARTPA_PARAMETER_ACK:
        break;
    default:
        ALOGE("%s(), not supported msg_id 0x%x", __FUNCTION__, p_sph_msg->msg_id);
    }

    return 0;
}


void SpeechDriverNormal::processModemEPOF() {
#ifdef MTK_AURISYS_PHONE_CALL_SUPPORT
    /* turn off modem clock for scp before EPOF done */
    if (isSpeechApplicationOn()) {
        AudioMessengerIPI::getInstance()->setSpmApMdSrcReq(false);
    }
#endif

    /* send EPOF ack to modem */
    sph_msg_t sph_msg;
    sendMailbox(&sph_msg, MSG_A2M_EPOF_ACK, 0, 0);

    /* notify queue */
    if (mSpeechMessageQueue != NULL) { mSpeechMessageQueue->notifyQueueToStopWaitingAck(); }
}


void SpeechDriverNormal::processModemAlive() {
    /* send alive ack to modem */
    sph_msg_t sph_msg;
    sendMailbox(&sph_msg, MSG_A2M_MD_ALIVE_ACK_BACK, 0, 0);
}


void SpeechDriverNormal::processNetworkCodecInfo(sph_msg_t *p_sph_msg) {
    spcCodecInfoStruct codec_info;
    spcCodecInfoStruct *p_codec_info = NULL;

    uint16_t data_type = 0;
    uint16_t data_size = 0;

    int retval = 0;

    /* error handling */
    if (p_sph_msg == NULL) {
        ALOGW("%s(), p_sph_msg == NULL!! return", __FUNCTION__);
        return;
    }

    if (mApResetDuringSpeech == true) {
        PRINT_SPH_MSG(ALOGW, "mApResetDuringSpeech == true!! drop md data", p_sph_msg);
        return;
    }


    if (p_sph_msg->buffer_type == SPH_MSG_BUFFER_TYPE_MAILBOX) { // via share memory
        data_size = sizeof(spcCodecInfoStruct);
        retval = mSpeechMessenger->readMdDataFromShareMemory(
                     &codec_info,
                     &data_type,
                     &data_size,
                     p_sph_msg->length,
                     p_sph_msg->rw_index);
        if (retval != 0) {
            PRINT_SPH_MSG(ALOGW, "get share memory md data failed!! drop it", p_sph_msg);
            return;
        }

        p_codec_info = &codec_info;
    } else if (p_sph_msg->buffer_type == SPH_MSG_BUFFER_TYPE_PAYLOAD) { // via payload
        ASSERT(p_sph_msg->payload_data_idx == p_sph_msg->payload_data_total_idx);

        p_codec_info = (spcCodecInfoStruct *)p_sph_msg->payload_data_addr;
        data_type = p_sph_msg->payload_data_type;
        data_size = p_sph_msg->payload_data_size;
    } else {
        PRINT_SPH_MSG(ALOGW, "bad buffer_type!!", p_sph_msg);
        return;
    }

    /* check value */
    if (data_type != SHARE_BUFF_DATA_TYPE_CCCI_NW_CODEC_INFO) {
        PRINT_SPH_MSG(ALOGE, "bad data_type!!", p_sph_msg);
        WARNING("bad data_type!!");
        return;
    }

    if (data_size != sizeof(spcCodecInfoStruct)) {
        PRINT_SPH_MSG(ALOGE, "bad data_size!!", p_sph_msg);
        WARNING("bad data_size!!");
        return;
    }

    /* set network info in property */
    ALOGD("%s(), length: 0x%x, rw_index: 0x%x, %s: \"%s\", %s: \"%s\"",
          __FUNCTION__,
          p_sph_msg->length,
          p_sph_msg->rw_index,
          kPropertyKeyRilSphCodecInfo, p_codec_info->codecInfo,
          kPropertyKeyRilHdVoiceStatus, p_codec_info->codecOp);
    set_string_to_property(kPropertyKeyRilSphCodecInfo, p_codec_info->codecInfo);
    set_string_to_property(kPropertyKeyRilHdVoiceStatus, p_codec_info->codecOp);


    /* send read ack to modem */
    sph_msg_t sph_msg;
    sendMailbox(&sph_msg, MSG_A2M_NW_CODEC_INFO_READ_ACK, 0, 0);
}


int SpeechDriverNormal::processModemControlMessage(sph_msg_t *p_sph_msg) {
    switch (p_sph_msg->msg_id) {
    case MSG_M2A_EPOF_NOTIFY: /* need ack */
        PRINT_SPH_MSG(ALOGD, "EPOF!!", p_sph_msg);
        processModemEPOF();
        break;
    case MSG_M2A_MD_ALIVE: /* need ack */
        PRINT_SPH_MSG(ALOGD, "MD Alive", p_sph_msg);
        processModemAlive();
        break;
    case MSG_M2A_EM_DATA_REQUEST: /* bypass ack */
        break; /* lagecy control, do nothing after 93 modem */
    case MSG_M2A_NETWORK_STATUS_NOTIFY: /* bypass ack */
        ALOGV("%s(), %s: %d", __FUNCTION__, kPropertyKeyRfMode, p_sph_msg->param_16bit);
        set_uint32_to_property(kPropertyKeyRfMode, p_sph_msg->param_16bit);
#ifdef MTK_AUDIO_GAIN_TABLE // speech network type change
        AudioVolumeFactory::CreateAudioVolumeController()->speechNetworkChange(p_sph_msg->param_16bit);
#endif
        break;
    case MSG_M2A_NW_CODEC_INFO_NOTIFY: /* need ack */
        processNetworkCodecInfo(p_sph_msg);
        break;
    default:
        ALOGE("%s(), not supported msg_id 0x%x", __FUNCTION__, p_sph_msg->msg_id);
    }

    return 0;
}


int SpeechDriverNormal::parseRawRecordPcmBuffer(void *raw_buf, void *parsed_buf, uint16_t *p_data_size) {
    spcRAWPCMBufInfo header_RawPcmBufInfo;
    spcApRAWPCMBufHdr header_ApRawPcmBuf;

    uint16_t BytesCopied = 0;
    uint16_t BytesToCopy = 0;

    char *PtrTarget = NULL;
    char *PtrSource = NULL;

    int retval = 0;

    // share buffer header
    memcpy(&header_RawPcmBufInfo, raw_buf, sizeof(spcRAWPCMBufInfo));
    PtrTarget = (char *)parsed_buf;

    AL_AUTOLOCK(mRecordTypeLock);
    switch (mRecordType) {
    case RECORD_TYPE_UL:
        header_ApRawPcmBuf.u16SyncWord = TEMP_CCCI_MD_PAYLOAD_SYNC;
        header_ApRawPcmBuf.u16RawPcmDir = RECORD_TYPE_UL;
        header_ApRawPcmBuf.u16Freq = sph_sample_rate_enum_to_value(header_RawPcmBufInfo.u16ULFreq);
        header_ApRawPcmBuf.u16Length = header_RawPcmBufInfo.u16ULLength;
        header_ApRawPcmBuf.u16Channel = 1;
        header_ApRawPcmBuf.u16BitFormat = AUDIO_FORMAT_PCM_16_BIT;

        // uplink raw pcm header
        memcpy(PtrTarget, &header_ApRawPcmBuf, sizeof(spcApRAWPCMBufHdr));
        BytesCopied = sizeof(spcApRAWPCMBufHdr);

        //uplink raw pcm
        PtrTarget = (char *)parsed_buf + BytesCopied;
        PtrSource = (char *)raw_buf + sizeof(spcRAWPCMBufInfo);
        BytesToCopy = header_RawPcmBufInfo.u16ULLength;
        memcpy(PtrTarget, PtrSource, BytesToCopy);
        BytesCopied += BytesToCopy;
        break;
    case RECORD_TYPE_DL:
        header_ApRawPcmBuf.u16SyncWord = TEMP_CCCI_MD_PAYLOAD_SYNC;
        header_ApRawPcmBuf.u16RawPcmDir = RECORD_TYPE_DL;
        header_ApRawPcmBuf.u16Freq = sph_sample_rate_enum_to_value(header_RawPcmBufInfo.u16DLFreq);
        header_ApRawPcmBuf.u16Length = header_RawPcmBufInfo.u16DLLength;
        header_ApRawPcmBuf.u16Channel = 1;
        header_ApRawPcmBuf.u16BitFormat = AUDIO_FORMAT_PCM_16_BIT;

        // downlink raw pcm header
        memcpy(PtrTarget, &header_ApRawPcmBuf, sizeof(spcApRAWPCMBufHdr));
        BytesCopied = sizeof(spcApRAWPCMBufHdr);

        // downlink raw pcm
        PtrTarget = (char *)parsed_buf + BytesCopied;
        PtrSource = (char *)raw_buf + sizeof(spcRAWPCMBufInfo) + header_RawPcmBufInfo.u16ULLength;
        BytesToCopy = header_RawPcmBufInfo.u16DLLength;
        memcpy(PtrTarget, PtrSource, BytesToCopy);
        BytesCopied += BytesToCopy;
        break;
    case RECORD_TYPE_MIX:
        header_ApRawPcmBuf.u16SyncWord = TEMP_CCCI_MD_PAYLOAD_SYNC;
        header_ApRawPcmBuf.u16RawPcmDir = RECORD_TYPE_UL;
        header_ApRawPcmBuf.u16Freq = sph_sample_rate_enum_to_value(header_RawPcmBufInfo.u16ULFreq);
        header_ApRawPcmBuf.u16Length = header_RawPcmBufInfo.u16ULLength;
        header_ApRawPcmBuf.u16Channel = 1;
        header_ApRawPcmBuf.u16BitFormat = AUDIO_FORMAT_PCM_16_BIT;

        //uplink raw pcm header
        memcpy(PtrTarget, &header_ApRawPcmBuf, sizeof(spcApRAWPCMBufHdr));
        BytesCopied = sizeof(spcApRAWPCMBufHdr);

        //uplink raw pcm
        PtrTarget = (char *)parsed_buf + BytesCopied;
        PtrSource = (char *)raw_buf + sizeof(spcRAWPCMBufInfo);
        BytesToCopy = header_RawPcmBufInfo.u16ULLength;
        memcpy(PtrTarget, PtrSource, BytesToCopy);
        BytesCopied += BytesToCopy;

        PtrTarget = (char *)parsed_buf + BytesCopied;

        //downlink raw pcm header
        header_ApRawPcmBuf.u16RawPcmDir = RECORD_TYPE_DL;
        header_ApRawPcmBuf.u16Freq = sph_sample_rate_enum_to_value(header_RawPcmBufInfo.u16DLFreq);
        header_ApRawPcmBuf.u16Length = header_RawPcmBufInfo.u16DLLength;
        memcpy(PtrTarget, &header_ApRawPcmBuf, sizeof(spcApRAWPCMBufHdr));
        BytesCopied += sizeof(spcApRAWPCMBufHdr);

        //downlink raw pcm
        PtrTarget = (char *)parsed_buf + BytesCopied;
        PtrSource = (char *)raw_buf + sizeof(spcRAWPCMBufInfo) + header_RawPcmBufInfo.u16ULLength;
        BytesToCopy = header_RawPcmBufInfo.u16DLLength;
        memcpy(PtrTarget, PtrSource, BytesToCopy);
        BytesCopied += BytesToCopy;
        break;
    default:
        ALOGW("%s(), mRecordType %d error!!", __FUNCTION__, mRecordType);
        retval = -EINVAL;
        BytesCopied = 0;
        break;
    }

    if (BytesCopied > *p_data_size) {
        ALOGW("%s(), BytesCopied %u > parsed_buf size %u!!", __FUNCTION__,
              BytesCopied, *p_data_size);
        *p_data_size = 0;
        WARNING("-EOVERFLOW");
        return -EOVERFLOW;
    }


    *p_data_size = BytesCopied;

    return retval;
}


static void dropMdDataInShareMemory(SpeechMessengerNormal *messenger, sph_msg_t *p_sph_msg) {
    uint8_t dummy_md_data[SPEECH_SHM_MD_DATA_SIZE];
    uint16_t data_type = 0;
    uint16_t data_size = 0;

    int retval = 0;

    if (p_sph_msg->buffer_type == SPH_MSG_BUFFER_TYPE_MAILBOX) { // via share memory
        data_size = SPEECH_SHM_MD_DATA_SIZE;
        retval = messenger->readMdDataFromShareMemory(
                     dummy_md_data,
                     &data_type,
                     &data_size,
                     p_sph_msg->length,
                     p_sph_msg->rw_index);
        if (retval != 0) {
            PRINT_SPH_MSG(ALOGW, "get share memory md data failed!!", p_sph_msg);
        }
    }
}


int SpeechDriverNormal::processModemDataMessage(sph_msg_t *p_sph_msg) {
    /* error handling */
    if (p_sph_msg == NULL) {
        ALOGW("%s(), p_sph_msg == NULL!! return", __FUNCTION__);
        return -EFAULT;
    }

    if (mSpeechMessenger == NULL) {
        ALOGW("%s(), mSpeechMessenger == NULL!! return", __FUNCTION__);
        return -EFAULT;
    }


    if (mApResetDuringSpeech == true) {
        PRINT_SPH_MSG(ALOGW, "mApResetDuringSpeech == true!! drop md data", p_sph_msg);
        return -ERESTART;
    }

    static BGSPlayer *pBGSPlayer = BGSPlayer::GetInstance();
    static SpeechVMRecorder *pSpeechVMRecorder = SpeechVMRecorder::GetInstance();
    static SpeechDataProcessingHandler *pSpeechDataProcessingHandler = SpeechDataProcessingHandler::getInstance();
    static Record2Way *pRecord2Way = Record2Way::GetInstance();
    static Play2Way *pPlay2Way = Play2Way::GetInstance();

    struct timespec ts_start;
    struct timespec ts_stop;

    uint64_t time_diff_shm = 0;
    uint64_t time_diff_vm = 0;

    sph_msg_t sph_msg;

    uint16_t num_data_request = 0;

    uint16_t data_type = 0;
    uint16_t data_size = 0;
    uint16_t payload_length = 0;
    uint32_t write_idx = 0;

    RingBuf ringbuf;

    int retval = 0;


    /* TODO: add class */
    switch (p_sph_msg->msg_id) {
    case MSG_M2A_BGSND_DATA_REQUEST: {
        ASSERT(getModemSideModemStatus(BGS_STATUS_MASK) == true);

        // avoid stream out standby to destroy BGSPlayBuffer during MSG_M2A_BGSND_DATA_REQUEST
        AL_LOCK(pBGSPlayer->mBGSMutex);

        // fill playback data
        if (GetApSideModemStatus(BGS_STATUS_MASK) == false) {
            PRINT_SPH_MSG(ALOGW, "ap bgs off now!! break", p_sph_msg);
            AL_UNLOCK(pBGSPlayer->mBGSMutex);
            break;
        } else if (!mBgsBuf) {
            PRINT_SPH_MSG(ALOGW, "mBgsBuf NULL!! break", p_sph_msg);
            AL_UNLOCK(pBGSPlayer->mBGSMutex);
            break;
        } else {
            PRINT_SPH_MSG(SPH_LOG_D, "bgs data request", p_sph_msg);
            num_data_request = p_sph_msg->length;
            if (num_data_request > kMaxApPayloadDataSize) {
                num_data_request = kMaxApPayloadDataSize;
            }
            data_size = (uint16_t)pBGSPlayer->PutDataToSpeaker((char *)mBgsBuf, num_data_request);
            AL_UNLOCK(pBGSPlayer->mBGSMutex);
        }

        // share memory
        retval = mSpeechMessenger->writeApDataToShareMemory(mBgsBuf,
                                                            SHARE_BUFF_DATA_TYPE_CCCI_BGS_TYPE,
                                                            data_size,
                                                            &payload_length,
                                                            &write_idx);
        // send data notify to modem side
        if (retval == 0) { // via share memory
            retval = sendMailbox(&sph_msg, MSG_A2M_BGSND_DATA_NOTIFY, payload_length, write_idx);
        } else { // via payload
            retval = sendPayload(&sph_msg, MSG_A2M_BGSND_DATA_NOTIFY,
                                 SHARE_BUFF_DATA_TYPE_CCCI_BGS_TYPE,
                                 mBgsBuf, data_size);
        }

        break;
    }
    case MSG_M2A_VM_REC_DATA_NOTIFY: {
        ASSERT(getModemSideModemStatus(VM_RECORD_STATUS_MASK) == true);

        if (GetApSideModemStatus(VM_RECORD_STATUS_MASK) == false) {
            PRINT_SPH_MSG(ALOGW, "ap vm rec off now!! drop it", p_sph_msg);
            dropMdDataInShareMemory(mSpeechMessenger, p_sph_msg);
            break;
        } else if (!mVmRecBuf) {
            PRINT_SPH_MSG(ALOGW, "mVmRecBuf NULL!! drop it", p_sph_msg);
            dropMdDataInShareMemory(mSpeechMessenger, p_sph_msg);
            break;
        } else {
            PRINT_SPH_MSG(SPH_LOG_V, "vm rec data notify", p_sph_msg);
            time_diff_shm = 0;
            time_diff_vm = 0;

            /* get vm data */
            if (p_sph_msg->buffer_type == SPH_MSG_BUFFER_TYPE_MAILBOX) { // via share memory
                data_size = MAX_VM_RECORD_SIZE;

                audio_get_timespec_monotonic(&ts_start);
                retval = mSpeechMessenger->readMdDataFromShareMemory(
                             mVmRecBuf,
                             &data_type,
                             &data_size,
                             p_sph_msg->length,
                             p_sph_msg->rw_index);
                audio_get_timespec_monotonic(&ts_stop);
                time_diff_shm = get_time_diff_ms(&ts_start, &ts_stop);

                if (retval != 0) {
                    PRINT_SPH_MSG(ALOGW, "get share memory md data failed!! drop it", p_sph_msg);
                } else {
                    sendMailbox(&sph_msg, MSG_A2M_VM_REC_DATA_READ_ACK,
                                p_sph_msg->length, p_sph_msg->rw_index);
                }
            } else if (p_sph_msg->buffer_type == SPH_MSG_BUFFER_TYPE_PAYLOAD) { // via payload
                if (p_sph_msg->payload_data_size > kMaxMdPayloadDataSize) {
                    PRINT_SPH_MSG(ALOGW, "get share memory md data failed!! drop it", p_sph_msg);
                    retval = -ENOMEM;
                } else {
                    memcpy(mVmRecBuf,
                           p_sph_msg->payload_data_addr,
                           p_sph_msg->payload_data_size);
                    data_type = p_sph_msg->payload_data_type;
                    data_size = p_sph_msg->payload_data_size;
                    if (p_sph_msg->payload_data_idx == p_sph_msg->payload_data_total_idx) {
                        sendMailbox(&sph_msg, MSG_A2M_VM_REC_DATA_READ_ACK, 0, 0);
                    }
                }
            } else {
                PRINT_SPH_MSG(ALOGW, "bad buffer_type!!", p_sph_msg);
                retval = -EINVAL;
            }

            /* copy vm data */
            if (retval == 0) {
                if (data_type != SHARE_BUFF_DATA_TYPE_CCCI_VM_TYPE) {
                    PRINT_SPH_MSG(ALOGW, "wrong data_type. drop it", p_sph_msg);
                    retval = -EINVAL;
                } else if (data_size > 0) {
                    ringbuf.pBufBase = (char *)mVmRecBuf;
                    ringbuf.bufLen   = data_size + 1; // +1: avoid pRead == pWrite
                    ringbuf.pRead    = ringbuf.pBufBase;
                    ringbuf.pWrite   = ringbuf.pBufBase + data_size;
                    audio_get_timespec_monotonic(&ts_start);
                    pSpeechVMRecorder->CopyBufferToVM(ringbuf);
                    audio_get_timespec_monotonic(&ts_stop);
                    time_diff_vm = get_time_diff_ms(&ts_start, &ts_stop);
                }
            }
            if ((time_diff_shm + time_diff_vm) >= MAX_MSG_PROCESS_TIME_MS) {
                ALOGW("%s(), time_diff_shm %ju, time_diff_vm %ju", __FUNCTION__, time_diff_shm, time_diff_vm);
            }
        }
        break;
    }
    case MSG_M2A_RAW_PCM_REC_DATA_NOTIFY: {
        ASSERT(getModemSideModemStatus(RAW_RECORD_STATUS_MASK) == true);

        if (GetApSideModemStatus(RAW_RECORD_STATUS_MASK) == false) {
            PRINT_SPH_MSG(ALOGW, "ap raw rec off now!! drop it", p_sph_msg);
            dropMdDataInShareMemory(mSpeechMessenger, p_sph_msg);
            break;
        } else if (!mRawRecBuf || !mParsedRecBuf) {
            PRINT_SPH_MSG(ALOGW, "mRawRecBuf or mParsedRecBuf NULL!! drop it", p_sph_msg);
            dropMdDataInShareMemory(mSpeechMessenger, p_sph_msg);
            break;
        } else {
            PRINT_SPH_MSG(SPH_LOG_V, "raw rec data notify", p_sph_msg);

            /* get rec data */
            if (p_sph_msg->buffer_type == SPH_MSG_BUFFER_TYPE_MAILBOX) { // via share memory
                data_size = MAX_RAW_RECORD_SIZE;
                retval = mSpeechMessenger->readMdDataFromShareMemory(
                             mRawRecBuf,
                             &data_type,
                             &data_size,
                             p_sph_msg->length,
                             p_sph_msg->rw_index);
                if (retval != 0) {
                    PRINT_SPH_MSG(ALOGW, "get share memory md data failed!! drop it", p_sph_msg);
                } else {
                    sendMailbox(&sph_msg, MSG_A2M_RAW_PCM_REC_DATA_READ_ACK,
                                p_sph_msg->length, p_sph_msg->rw_index);
                }
            } else if (p_sph_msg->buffer_type == SPH_MSG_BUFFER_TYPE_PAYLOAD) { // via payload
                if (p_sph_msg->payload_data_size > kMaxMdPayloadDataSize) {
                    PRINT_SPH_MSG(ALOGW, "get share memory md data failed!! drop it", p_sph_msg);
                    retval = -ENOMEM;
                } else {
                    memcpy(mRawRecBuf,
                           p_sph_msg->payload_data_addr,
                           p_sph_msg->payload_data_size);
                    data_type = p_sph_msg->payload_data_type;
                    data_size = p_sph_msg->payload_data_size;
                    if (p_sph_msg->payload_data_idx == p_sph_msg->payload_data_total_idx) {
                        sendMailbox(&sph_msg, MSG_A2M_RAW_PCM_REC_DATA_READ_ACK, 0, 0);
                    }
                }
            } else {
                PRINT_SPH_MSG(ALOGW, "bad buffer_type!!", p_sph_msg);
                retval = -EINVAL;
            }

            /* copy raw rec data */
            if (retval == 0) {
#if 0 // check
                if (data_type != SHARE_BUFF_DATA_TYPE_CCCI_RAW_PCM_TYPE) {
                    PRINT_SPH_MSG(ALOGW, "wrong data_type. drop it", p_sph_msg);
                    retval = -EINVAL;
                } else
#endif
                    if (data_size > 0) {
                        data_size = MAX_PARSED_RECORD_SIZE;
                        retval = parseRawRecordPcmBuffer(mRawRecBuf, mParsedRecBuf, &data_size);
                        if (retval == 0) {
                            ringbuf.pBufBase = (char *)mParsedRecBuf;
                            ringbuf.bufLen   = data_size + 1; // +1: avoid pRead == pWrite
                            ringbuf.pRead    = ringbuf.pBufBase;
                            ringbuf.pWrite   = ringbuf.pBufBase + data_size;

                            pSpeechDataProcessingHandler->provideModemRecordDataToProvider(ringbuf);
                        }
                    }
            }
        }
        break;
    }
    case MSG_M2A_PNW_UL_DATA_NOTIFY: {
        ASSERT(getModemSideModemStatus(P2W_STATUS_MASK) == true);

        if (GetApSideModemStatus(P2W_STATUS_MASK) == false) {
            PRINT_SPH_MSG(ALOGW, "ap p2w off now!! drop it", p_sph_msg);
            dropMdDataInShareMemory(mSpeechMessenger, p_sph_msg);
            break;
        } else if (!mP2WUlBuf) {
            PRINT_SPH_MSG(ALOGW, "mP2WUlBuf NULL!! drop it", p_sph_msg);
            dropMdDataInShareMemory(mSpeechMessenger, p_sph_msg);
            break;
        } else {
            PRINT_SPH_MSG(SPH_LOG_V, "p2w ul data notify", p_sph_msg);

            /* get p2w ul data */
            if (p_sph_msg->buffer_type == SPH_MSG_BUFFER_TYPE_MAILBOX) { // via share memory
                data_size = MAX_PNW_UL_SIZE;
                retval = mSpeechMessenger->readMdDataFromShareMemory(
                             mP2WUlBuf,
                             &data_type,
                             &data_size,
                             p_sph_msg->length,
                             p_sph_msg->rw_index);
                if (retval != 0) {
                    PRINT_SPH_MSG(ALOGW, "get share memory md data failed!! drop it", p_sph_msg);
                } else {
                    sendMailbox(&sph_msg, MSG_A2M_PNW_UL_DATA_READ_ACK,
                                p_sph_msg->length, p_sph_msg->rw_index);
                }
            } else if (p_sph_msg->buffer_type == SPH_MSG_BUFFER_TYPE_PAYLOAD) { // via payload
                if (p_sph_msg->payload_data_size > kMaxMdPayloadDataSize) {
                    PRINT_SPH_MSG(ALOGW, "get share memory md data failed!! drop it", p_sph_msg);
                    retval = -ENOMEM;
                } else {
                    memcpy(mP2WUlBuf,
                           p_sph_msg->payload_data_addr,
                           p_sph_msg->payload_data_size);
                    data_type = p_sph_msg->payload_data_type;
                    data_size = p_sph_msg->payload_data_size;
                    if (p_sph_msg->payload_data_idx == p_sph_msg->payload_data_total_idx) {
                        sendMailbox(&sph_msg, MSG_A2M_PNW_UL_DATA_READ_ACK, 0, 0);
                    }
                }
            } else {
                PRINT_SPH_MSG(ALOGW, "bad buffer_type!!", p_sph_msg);
                retval = -EINVAL;
            }

            /* copy p2w ul data */
            if (retval == 0) {
#if 0 // check
                if (data_type != SHARE_BUFF_DATA_TYPE_PCM_GetFromMic) {
                    PRINT_SPH_MSG(ALOGW, "wrong data_type. drop it", p_sph_msg);
                    retval = -EINVAL;
                } else
#endif
                    if (data_size > 0) {
                        ringbuf.pBufBase = (char *)mP2WUlBuf;
                        ringbuf.bufLen   = data_size + 1; // +1: avoid pRead == pWrite
                        ringbuf.pRead    = ringbuf.pBufBase;
                        ringbuf.pWrite   = ringbuf.pBufBase + data_size;
                        pRecord2Way->GetDataFromMicrophone(ringbuf);

#if 0 // PCM2WAY: UL -> DL Loopback
                        // Used for debug and Speech DVT
                        uint16_t size_bytes = 320;
                        char buffer[320];
                        pRecord2Way->Read(buffer, size_bytes);
                        pPlay2Way->Write(buffer, size_bytes);
#endif
                    }
            }
        }

        break;
    }
    case MSG_M2A_PNW_DL_DATA_REQUEST: {
        ASSERT(getModemSideModemStatus(P2W_STATUS_MASK) == true);

        // fill p2w dl data
        if (GetApSideModemStatus(P2W_STATUS_MASK) == false) {
            PRINT_SPH_MSG(ALOGW, "ap p2w off now!! break", p_sph_msg);
            break;
        } else if (!mP2WDlBuf) {
            PRINT_SPH_MSG(ALOGW, "mP2WDlBuf NULL!! break", p_sph_msg);
            break;
        } else {
            PRINT_SPH_MSG(SPH_LOG_D, "p2w dl data request", p_sph_msg);
            num_data_request = p_sph_msg->length;
            if (num_data_request > kMaxApPayloadDataSize) {
                num_data_request = kMaxApPayloadDataSize;
            }
            data_size = (uint16_t)pPlay2Way->PutDataToSpeaker((char *)mP2WDlBuf, num_data_request);
            if (data_size == 0) {
                PRINT_SPH_MSG(ALOGW, "data_size == 0", p_sph_msg);
#if 0
                break;
#endif
            }
        }

        // share memory
        retval = mSpeechMessenger->writeApDataToShareMemory(mP2WDlBuf,
                                                            SHARE_BUFF_DATA_TYPE_PCM_FillSpk,
                                                            data_size,
                                                            &payload_length,
                                                            &write_idx);
        // send data notify to modem side
        if (retval == 0) { // via share memory
            retval = sendMailbox(&sph_msg, MSG_A2M_PNW_DL_DATA_NOTIFY, payload_length, write_idx);
        } else { // via payload
            retval = sendPayload(&sph_msg, MSG_A2M_PNW_DL_DATA_NOTIFY,
                                 SHARE_BUFF_DATA_TYPE_PCM_FillSpk,
                                 mP2WDlBuf, data_size);
        }
        break;
    }
    case MSG_M2A_CTM_DEBUG_DATA_NOTIFY: {
        ASSERT(getModemSideModemStatus(TTY_STATUS_MASK) == true);

        if (GetApSideModemStatus(TTY_STATUS_MASK) == false) {
            PRINT_SPH_MSG(ALOGW, "ap tty off now!! drop it", p_sph_msg);
            dropMdDataInShareMemory(mSpeechMessenger, p_sph_msg);
            break;
        } else if (!mTtyDebugBuf) {
            PRINT_SPH_MSG(ALOGW, "mTtyDebugBuf NULL!! drop it", p_sph_msg);
            dropMdDataInShareMemory(mSpeechMessenger, p_sph_msg);
            break;
        } else {
            PRINT_SPH_MSG(SPH_LOG_V, "tty debug data notify", p_sph_msg);

            /* get tty debug data */
            if (p_sph_msg->buffer_type == SPH_MSG_BUFFER_TYPE_MAILBOX) { // via share memory
                data_size = MAX_TTY_DEBUG_SIZE;
                retval = mSpeechMessenger->readMdDataFromShareMemory(
                             mTtyDebugBuf,
                             &data_type,
                             &data_size,
                             p_sph_msg->length,
                             p_sph_msg->rw_index);

                if (retval != 0) {
                    PRINT_SPH_MSG(ALOGW, "get share memory md data failed!! drop it", p_sph_msg);
                } else {
                    sendMailbox(&sph_msg, MSG_A2M_CTM_DEBUG_DATA_READ_ACK,
                                p_sph_msg->length, p_sph_msg->rw_index);
                }
            } else if (p_sph_msg->buffer_type == SPH_MSG_BUFFER_TYPE_PAYLOAD) { // via payload
                if (p_sph_msg->payload_data_size > kMaxMdPayloadDataSize) {
                    PRINT_SPH_MSG(ALOGW, "get share memory md data failed!! drop it", p_sph_msg);
                    retval = -ENOMEM;
                } else {
                    memcpy(mTtyDebugBuf,
                           p_sph_msg->payload_data_addr,
                           p_sph_msg->payload_data_size);
                    data_type = p_sph_msg->payload_data_type;
                    data_size = p_sph_msg->payload_data_size;
                    if (p_sph_msg->payload_data_idx == p_sph_msg->payload_data_total_idx) {
                        sendMailbox(&sph_msg, MSG_A2M_CTM_DEBUG_DATA_READ_ACK, 0, 0);
                    }
                }
            } else {
                PRINT_SPH_MSG(ALOGW, "bad buffer_type!!", p_sph_msg);
                retval = -EINVAL;
            }

            /* copy tty debug data */
            if (retval == 0) {
                ringbuf.pBufBase = (char *)mTtyDebugBuf;
                ringbuf.bufLen   = data_size + 1; // +1: avoid pRead == pWrite
                ringbuf.pRead    = ringbuf.pBufBase;
                ringbuf.pWrite   = ringbuf.pBufBase + data_size;

                switch (data_type) {
                case SHARE_BUFF_DATA_TYPE_CCCI_CTM_UL_IN:
                    pSpeechVMRecorder->GetCtmDebugDataFromModem(ringbuf, pSpeechVMRecorder->pCtmDumpFileUlIn);
                    break;
                case SHARE_BUFF_DATA_TYPE_CCCI_CTM_DL_IN:
                    pSpeechVMRecorder->GetCtmDebugDataFromModem(ringbuf, pSpeechVMRecorder->pCtmDumpFileDlIn);
                    break;
                case SHARE_BUFF_DATA_TYPE_CCCI_CTM_UL_OUT:
                    pSpeechVMRecorder->GetCtmDebugDataFromModem(ringbuf, pSpeechVMRecorder->pCtmDumpFileUlOut);
                    break;
                case SHARE_BUFF_DATA_TYPE_CCCI_CTM_DL_OUT:
                    pSpeechVMRecorder->GetCtmDebugDataFromModem(ringbuf, pSpeechVMRecorder->pCtmDumpFileDlOut);
                    break;
                default:
                    PRINT_SPH_MSG(ALOGW, "wrong data_type. drop it", p_sph_msg);
                    retval = -EINVAL;
                    ASSERT(0);
                }
            }
        }
        break;
    }

    default:
        ALOGE("%s(), not supported msg_id 0x%x", __FUNCTION__, p_sph_msg->msg_id);
    }


    return 0;
}



/*==============================================================================
 *                     Speech Control
 *============================================================================*/

static speech_mode_t GetSpeechModeByOutputDevice(const audio_devices_t output_device) {
    speech_mode_t speech_mode = SPEECH_MODE_NORMAL;
    if (audio_is_bluetooth_sco_device(output_device)) {
        speech_mode = SPEECH_MODE_BT_EARPHONE;
    } else if (output_device == AUDIO_DEVICE_OUT_SPEAKER) {
        if (SpeechEnhancementController::GetInstance()->GetMagicConferenceCallOn() == true) {
            speech_mode = SPEECH_MODE_MAGIC_CON_CALL;
        } else {
            speech_mode = SPEECH_MODE_LOUD_SPEAKER;
        }
    } else if (output_device == AUDIO_DEVICE_OUT_WIRED_HEADSET) {
        speech_mode = SPEECH_MODE_EARPHONE;
#if defined(MTK_AUDIO_HIERARCHICAL_PARAM_SUPPORT)
        SpeechParamParser::getInstance()->SetParamInfo(String8("ParamHeadsetPole=4;"));
#endif
    } else if (output_device == AUDIO_DEVICE_OUT_WIRED_HEADPHONE) {
        speech_mode = SPEECH_MODE_EARPHONE;
#if defined(MTK_AUDIO_HIERARCHICAL_PARAM_SUPPORT)
        SpeechParamParser::getInstance()->SetParamInfo(String8("ParamHeadsetPole=3;"));
#endif
    }
#ifdef MTK_USB_PHONECALL
    else if (output_device == AUDIO_DEVICE_OUT_USB_DEVICE) {
        speech_mode = SPEECH_MODE_USB_AUDIO;
    }
#endif
    else if (output_device == AUDIO_DEVICE_OUT_EARPIECE) {
#if defined(MTK_HAC_SUPPORT)
        if (SpeechEnhancementController::GetInstance()->GetHACOn() == true) {
            speech_mode = SPEECH_MODE_HAC;
        } else
#endif
        {
            speech_mode = SPEECH_MODE_NORMAL;
        }
    }

    return speech_mode;
}


status_t SpeechDriverNormal::SetSpeechMode(const audio_devices_t input_device, const audio_devices_t output_device) {
    speech_mode_t speech_mode = GetSpeechModeByOutputDevice(output_device);
    sph_msg_t sph_msg;
    sph_info_t sph_info;

    int retval = 0;

    ALOGD("%s(), input_device: 0x%x, output_device: 0x%x, speech_mode: %d", __FUNCTION__, input_device, output_device, speech_mode);

    mSpeechMode = speech_mode;
    mInputDevice = input_device;
    mOutputDevice = output_device;

    // set a unreasonable gain value s.t. the reasonable gain can be set to modem next time
    mDownlinkGain   = kUnreasonableGainValue;
    mDownlinkenh1Gain = kUnreasonableGainValue;
    mUplinkGain     = kUnreasonableGainValue;
    mSideToneGain   = kUnreasonableGainValue;

    int param_arg[4];
    param_arg[0] = (int)mSpeechMode;
    param_arg[1] = mVolumeIndex;
    param_arg[2] = mBtHeadsetNrecOn;
    param_arg[3] = 0; // bit 0: customized profile, bit 1: single band, bit 4~7: band number

    AL_AUTOLOCK_MS(mSpeechParamLock, MAX_SPEECH_AUTO_LOCK_TIMEOUT_MS); // atomic: write shm & send msg
    parseSpeechParam(AUDIO_TYPE_SPEECH, param_arg);

    if (isSpeechApplicationOn()) {
        configSpeechInfo(&sph_info);
        retval = sendPayload(&sph_msg, MSG_A2M_SPH_DEV_CHANGE,
                             SHARE_BUFF_DATA_TYPE_CCCI_SPH_INFO,
                             &sph_info, sizeof(sph_info_t));
    }

    return 0;
}


status_t SpeechDriverNormal::setMDVolumeIndex(int stream, int device, int index) {
    int param_arg[4];

    //Android M Voice volume index: available index 1~7, 0 for mute
    //Android L Voice volume index: available index 0~6
    if (index <= 0) {
        return 0;
    } else {
        mVolumeIndex = index - 1;
    }

    if (isSpeechApplicationOn() == false) {
        ALOGD("%s(), stream: %d, device: 0x%x, index: %d, sph off, return",
              __FUNCTION__, stream, device, index);
    } else {
        ALOGD("%s(), stream: %d, device: 0x%x, index: %d",
              __FUNCTION__, stream, device, index);
        param_arg[0] = (int)mSpeechMode;
        param_arg[1] = mVolumeIndex;
        param_arg[2] = mBtHeadsetNrecOn;
        param_arg[3] = 0; // bit 0: customized profile, bit 1: single band, bit 4~7: band number

        SetDynamicSpeechParameters(AUDIO_TYPE_SPEECH, param_arg);
    }

    return 0;
}


int SpeechDriverNormal::SpeechOnByApplication(const uint8_t application) {
    sph_msg_t sph_msg;
    sph_info_t sph_info;

    SLOG_ENG("SpeechOn(), application: %d", application);

    // reset mute/gain status
    CleanGainValueAndMuteStatus();

    // reset modem status
    mModemResetDuringSpeech = false;

    // vibration speaker param
    if (IsAudioSupportFeature(AUDIO_SUPPORT_VIBRATION_SPEAKER)) {
        PARAM_VIBSPK eVibSpkParam;
        GetVibSpkParam((void *)&eVibSpkParam);
        SetVibSpkParam((void *)&eVibSpkParam);
    }

    // speech param
    AL_AUTOLOCK_MS(mSpeechParamLock, MAX_SPEECH_AUTO_LOCK_TIMEOUT_MS); // atomic: write shm & send msg
    parseSpeechParam(AUDIO_TYPE_SPEECH_GENERAL, NULL);
    parseSpeechParam(AUDIO_TYPE_SPEECH_MAGICLARITY, NULL);
    parseSpeechParam(AUDIO_TYPE_SPEECH_DMNR, NULL);

    mApplication = application;
    configSpeechInfo(&sph_info);

    int retval = sendPayload(&sph_msg, MSG_A2M_SPH_ON,
                             SHARE_BUFF_DATA_TYPE_CCCI_SPH_INFO,
                             &sph_info, sizeof(sph_info_t));

    createThreadsDuringSpeech();

    return retval;
}


int SpeechDriverNormal::SpeechOffByApplication(const uint8_t application) {
    sph_msg_t sph_msg;

    SLOG_ENG("SpeechOff(), application: %d, mApplication: %d", application, mApplication);
    if (application != mApplication) {
        WARNING("speech off not in pair!!");
    }

    int retval = sendMailbox(&sph_msg, MSG_A2M_SPH_OFF, 0, 0);

    CleanGainValueAndMuteStatus();

    mApplication = SPH_APPLICATION_INVALID;

    mModemResetDuringSpeech = false;

    return retval;
}


status_t SpeechDriverNormal::SpeechOn() {
    CheckApSideModemStatusAllOffOrDie();
    SetApSideModemStatus(SPEECH_STATUS_MASK);

    return SpeechOnByApplication(SPH_APPLICATION_NORMAL);
}


status_t SpeechDriverNormal::SpeechOff() {
    /* should send sph off first and then clean state */
    int retval = SpeechOffByApplication(SPH_APPLICATION_NORMAL);

    ResetApSideModemStatus(SPEECH_STATUS_MASK);
    CheckApSideModemStatusAllOffOrDie();

    return retval;
}


status_t SpeechDriverNormal::VideoTelephonyOn() {
    CheckApSideModemStatusAllOffOrDie();
    SetApSideModemStatus(VT_STATUS_MASK);

    return SpeechOnByApplication(SPH_APPLICATION_VT_CALL);
}


status_t SpeechDriverNormal::VideoTelephonyOff() {
    /* should send sph off first and then clean state */
    int retval = SpeechOffByApplication(SPH_APPLICATION_VT_CALL);

    ResetApSideModemStatus(VT_STATUS_MASK);
    CheckApSideModemStatusAllOffOrDie();

    return retval;
}


status_t SpeechDriverNormal::SpeechRouterOn() {
    CheckApSideModemStatusAllOffOrDie();
    SetApSideModemStatus(SPEECH_ROUTER_STATUS_MASK);

    return SpeechOnByApplication(SPH_APPLICATION_ROUTER);
}


status_t SpeechDriverNormal::SpeechRouterOff() {
    /* should send sph off first and then clean state */
    int retval = SpeechOffByApplication(SPH_APPLICATION_ROUTER);

    ResetApSideModemStatus(SPEECH_ROUTER_STATUS_MASK);
    CheckApSideModemStatusAllOffOrDie();

    return retval;
}



/*==============================================================================
 *                     Recording Control
 *============================================================================*/

status_t SpeechDriverNormal::RecordOn(record_type_t type_record) {
    AL_AUTOLOCK(mRecordTypeLock);
    SLOG_ENG("%s(), mRecordSampleRateType: %d, mRecordChannelType: %d, mRecordType: %d => %d",
             __FUNCTION__, mRecordSampleRateType, mRecordChannelType, mRecordType, type_record);

    SetApSideModemStatus(RAW_RECORD_STATUS_MASK);

    mRecordType = type_record;
    sph_msg_t sph_msg;
    uint16_t param_16bit = mRecordSampleRateType | (mRecordChannelType << 4);
    return sendMailbox(&sph_msg, MSG_A2M_RECORD_RAW_PCM_ON, param_16bit, 0);
}


status_t SpeechDriverNormal::RecordOff(record_type_t type_record) {
    AL_AUTOLOCK(mRecordTypeLock);
    SLOG_ENG("%s(), mRecordType: %d => %d", __FUNCTION__, mRecordType, type_record);

    sph_msg_t sph_msg;
    int retval = 0;

    retval = sendMailbox(&sph_msg, MSG_A2M_RECORD_RAW_PCM_OFF, 0, 0);

    ResetApSideModemStatus(RAW_RECORD_STATUS_MASK);
    mRecordType = type_record;
    return retval;
}


status_t SpeechDriverNormal::SetPcmRecordType(record_type_t type_record) {
    AL_AUTOLOCK(mRecordTypeLock);
    ALOGD("%s(), mRecordType: %d => %d", __FUNCTION__, mRecordType, type_record);
    mRecordType = type_record;
    return 0;
}


status_t SpeechDriverNormal::VoiceMemoRecordOn() {
    SetApSideModemStatus(VM_RECORD_STATUS_MASK);
    sph_msg_t sph_msg;
    return sendMailbox(&sph_msg, MSG_A2M_VM_REC_ON, 0, 0);
}


status_t SpeechDriverNormal::VoiceMemoRecordOff() {
    sph_msg_t sph_msg;
    int retval = 0;
    retval = sendMailbox(&sph_msg, MSG_A2M_VM_REC_OFF, 0, 0);

    ResetApSideModemStatus(VM_RECORD_STATUS_MASK);
    return retval;
}


uint16_t SpeechDriverNormal::GetRecordSampleRate() const {
    uint16_t num_sample_rate = 0;

    switch (mRecordSampleRateType) {
    case RECORD_SAMPLE_RATE_08K:
        num_sample_rate = 8000;
        break;
    case RECORD_SAMPLE_RATE_16K:
        num_sample_rate = 16000;
        break;
    case RECORD_SAMPLE_RATE_32K:
        num_sample_rate = 32000;
        break;
    case RECORD_SAMPLE_RATE_48K:
        num_sample_rate = 48000;
        break;
    default:
        num_sample_rate = 8000;
        break;
    }

    return num_sample_rate;
}


uint16_t SpeechDriverNormal::GetRecordChannelNumber() const {
    uint16_t num_channel = 0;

    switch (mRecordChannelType) {
    case RECORD_CHANNEL_MONO:
        num_channel = 1;
        break;
    case RECORD_CHANNEL_STEREO:
        num_channel = 2;
        break;
    default:
        num_channel = 1;
        break;
    }

    return num_channel;
}

/*==============================================================================
 *                     Background Sound
 *============================================================================*/

status_t SpeechDriverNormal::BGSoundOn() {
    SetApSideModemStatus(BGS_STATUS_MASK);
    sph_msg_t sph_msg;
    return sendMailbox(&sph_msg, MSG_A2M_BGSND_ON, 0, 0);
}


status_t SpeechDriverNormal::BGSoundConfig(uint8_t ul_gain, uint8_t dl_gain) {
    sph_msg_t sph_msg;
    uint16_t param_16bit = (ul_gain << 8) | dl_gain;
    return sendMailbox(&sph_msg, MSG_A2M_BGSND_CONFIG, param_16bit, 0);
}


status_t SpeechDriverNormal::BGSoundOff() {
    sph_msg_t sph_msg;
    int retval = 0;
    retval = sendMailbox(&sph_msg, MSG_A2M_BGSND_OFF, 0, 0);

    ResetApSideModemStatus(BGS_STATUS_MASK);
    return retval;
}



/*==============================================================================
 *                     PCM 2 Way
 *============================================================================*/

status_t SpeechDriverNormal::PCM2WayOn(const bool wideband_on) {
    SetApSideModemStatus(P2W_STATUS_MASK);

    mPCM2WayState = (SPC_PNW_MSG_BUFFER_SPK | SPC_PNW_MSG_BUFFER_MIC | (wideband_on << 4));
    ALOGD("%s(), wideband_on: %d, mPCM2WayState: 0x%x", __FUNCTION__, wideband_on, mPCM2WayState);

    sph_msg_t sph_msg;
    return sendMailbox(&sph_msg, MSG_A2M_PNW_ON, mPCM2WayState, 0);
}


status_t SpeechDriverNormal::PCM2WayOff() {
    ALOGD("%s(), mPCM2WayState: 0x%x => 0", __FUNCTION__, mPCM2WayState);
    mPCM2WayState = 0;

    sph_msg_t sph_msg;
    int retval = 0;
    retval = sendMailbox(&sph_msg, MSG_A2M_PNW_OFF, 0, 0);

    ResetApSideModemStatus(P2W_STATUS_MASK);
    return retval;
}


/*==============================================================================
 *                     TTY-CTM Control
 *============================================================================*/

status_t SpeechDriverNormal::TtyCtmOn(tty_mode_t ttyMode) {
    SpeechVMRecorder *pSpeechVMRecorder = SpeechVMRecorder::GetInstance();
    const bool uplink_mute_on_copy = mUplinkMuteOn;

    ALOGD("%s(), ttyMode: %d", __FUNCTION__, ttyMode);

    SetApSideModemStatus(TTY_STATUS_MASK);

    SetUplinkMute(true);
    TtyCtmDebugOn(pSpeechVMRecorder->GetVMRecordCapabilityForCTM4Way());

    sph_msg_t sph_msg;
    int retval = sendMailbox(&sph_msg, MSG_A2M_CTM_ON, ttyMode, 0);

    SetUplinkMute(uplink_mute_on_copy);

    return retval;
}


status_t SpeechDriverNormal::TtyCtmOff() {
    ALOGD("%s()", __FUNCTION__);

    sph_msg_t sph_msg;
    int retval = 0;

    if (mTtyDebugEnable == true) {
        TtyCtmDebugOn(false);
    }
    retval = sendMailbox(&sph_msg, MSG_A2M_CTM_OFF, 0, 0);

    ResetApSideModemStatus(TTY_STATUS_MASK);
    return retval;
}


status_t SpeechDriverNormal::TtyCtmDebugOn(bool tty_debug_flag) {
    SpeechVMRecorder *pSpeechVMRecorder = SpeechVMRecorder::GetInstance();

    ALOGD("%s(), tty_debug_flag: %d", __FUNCTION__, tty_debug_flag);

    if (tty_debug_flag == true) {
        mTtyDebugEnable = true;
        pSpeechVMRecorder->StartCtmDebug();
    } else {
        pSpeechVMRecorder->StopCtmDebug();
        mTtyDebugEnable = false;
    }

    sph_msg_t sph_msg;
    return sendMailbox(&sph_msg, MSG_A2M_CTM_DUMP_DEBUG_FILE, tty_debug_flag, 0);
}

/*==============================================================================
 *                     RTT
 *============================================================================*/

int SpeechDriverNormal::RttConfig(int rttMode) {
    ALOGD("%s(), rttMode = %d, old mRttMode = %d", __FUNCTION__, rttMode, mRttMode);

    if (rttMode == mRttMode) { return NO_ERROR; }
    mRttMode = rttMode;
    sph_msg_t sph_msg;
    return sendMailbox(&sph_msg, MSG_A2M_RTT_CONFIG, (uint16_t)mRttMode, 0);
}


/*==============================================================================
 *                     Modem Audio DVT and Debug
 *============================================================================*/

status_t SpeechDriverNormal::SetModemLoopbackPoint(uint16_t loopback_point) {
    ALOGD("%s(), loopback_point: %d", __FUNCTION__, loopback_point);

    sph_msg_t sph_msg;
    return sendMailbox(&sph_msg, MSG_A2M_SET_LPBK_POINT_DVT, loopback_point, 0);
}


/*==============================================================================
 *                     Acoustic Loopback
 *============================================================================*/

status_t SpeechDriverNormal::SetAcousticLoopback(bool loopback_on) {
    ALOGD("%s(), loopback_on: %d, mModemLoopbackDelayFrames: %d, mUseBtCodec: %d",
          __FUNCTION__, loopback_on, mModemLoopbackDelayFrames, mUseBtCodec);

    int retval = 0;

    if (loopback_on == true) {
        CheckApSideModemStatusAllOffOrDie();
        SetApSideModemStatus(LOOPBACK_STATUS_MASK);

        retval = SpeechOnByApplication(SPH_APPLICATION_LOOPBACK);
    } else {
        mUseBtCodec = true;

        /* should send sph off first and then clean state */
        retval = SpeechOffByApplication(SPH_APPLICATION_LOOPBACK);

        ResetApSideModemStatus(LOOPBACK_STATUS_MASK);
        CheckApSideModemStatusAllOffOrDie();

#if defined(MODEM_DYNAMIC_PARAM) && defined(MTK_AUDIO_SPH_LPBK_PARAM)
        SpeechParamParser::getInstance()->SetParamInfo(String8("ParamSphLpbk=0;"));
#endif
    }

    return retval;
}


status_t SpeechDriverNormal::SetAcousticLoopbackBtCodec(bool enable_codec) {
    ALOGD("%s(), mUseBtCodec: %d => %d", __FUNCTION__, mUseBtCodec, enable_codec);
    mUseBtCodec = enable_codec;
    return 0;
}


status_t SpeechDriverNormal::SetAcousticLoopbackDelayFrames(int32_t delay_frames) {
    ALOGD("%s(), mModemLoopbackDelayFrames: %d => %d", __FUNCTION__,
          mModemLoopbackDelayFrames, delay_frames);

    if (delay_frames < 0) {
        ALOGE("%s(), delay_frames(%d) < 0!! set 0 instead", __FUNCTION__, delay_frames);
        delay_frames = 0;
    }

    mModemLoopbackDelayFrames = (uint8_t)delay_frames;
    if (mModemLoopbackDelayFrames > MAX_LOOPBACK_DELAY_FRAMES) {
        ALOGE("%s(), delay_frames(%d) > %d!! set %d instead.", __FUNCTION__,
              mModemLoopbackDelayFrames, MAX_LOOPBACK_DELAY_FRAMES, MAX_LOOPBACK_DELAY_FRAMES);
        mModemLoopbackDelayFrames = MAX_LOOPBACK_DELAY_FRAMES;
    }

    if (mApplication == SPH_APPLICATION_LOOPBACK) {
        ALOGW("Loopback is enabled now! The new delay_frames will be applied next time");
    }

    return 0;
}



/*==============================================================================
 *                     Volume Control
 *============================================================================*/

status_t SpeechDriverNormal::SetDownlinkGain(int16_t gain) {
    static AudioLock gainLock;
    AL_AUTOLOCK(gainLock);

    if (isSpeechApplicationOn() == false) {
        return 0;
    }

    ALOGD("%s(), mDownlinkGain: 0x%x => 0x%x", __FUNCTION__, mDownlinkGain, gain);
    if (gain == mDownlinkGain) { return 0; }
    mDownlinkGain = gain;
    sph_msg_t sph_msg;
    return sendMailbox(&sph_msg, MSG_A2M_SPH_DL_DIGIT_VOLUME, gain, 0);
}


status_t SpeechDriverNormal::SetEnh1DownlinkGain(int16_t gain) {
    static AudioLock gainLock;
    AL_AUTOLOCK(gainLock);

    if (isSpeechApplicationOn() == false) {
        return 0;
    }

    ALOGD("%s(), mDownlinkenh1Gain: 0x%x => 0x%x", __FUNCTION__, mDownlinkenh1Gain, gain);
    if (gain == mDownlinkenh1Gain) { return 0; }
    mDownlinkenh1Gain = gain;
    sph_msg_t sph_msg;
    return sendMailbox(&sph_msg, MSG_A2M_SPH_DL_ENH_REF_DIGIT_VOLUME, gain, 0);
}


status_t SpeechDriverNormal::SetUplinkGain(int16_t gain) {
    static AudioLock gainLock;
    AL_AUTOLOCK(gainLock);

    if (isSpeechApplicationOn() == false) {
        return 0;
    }

    ALOGD("%s(), mUplinkGain: 0x%x => 0x%x", __FUNCTION__, mUplinkGain, gain);
    if (gain == mUplinkGain) { return 0; }
    mUplinkGain = gain;
    sph_msg_t sph_msg;
    return sendMailbox(&sph_msg, MSG_A2M_SPH_UL_DIGIT_VOLUME, gain, 0);
}


status_t SpeechDriverNormal::SetDownlinkMute(bool mute_on) {
    static AudioLock muteLock;
    AL_AUTOLOCK(muteLock);

    if (isSpeechApplicationOn() == false) {
        return 0;
    }

    ALOGD("%s(), mDownlinkMuteOn: %d => %d", __FUNCTION__, mDownlinkMuteOn, mute_on);
    if (mute_on == mDownlinkMuteOn) { return 0; }
    mDownlinkMuteOn = mute_on;
    sph_msg_t sph_msg;
    return sendMailbox(&sph_msg, MSG_A2M_MUTE_SPH_DL, mute_on, 0);
}


status_t SpeechDriverNormal::SetDownlinkMuteCodec(bool mute_on) {
    static AudioLock muteLock;
    AL_AUTOLOCK(muteLock);

    if (isSpeechApplicationOn() == false) {
        return 0;
    }

    ALOGD("%s(), mute_on: %d", __FUNCTION__, mute_on);
    sph_msg_t sph_msg;
    return sendMailbox(&sph_msg, MSG_A2M_MUTE_SPH_DL_CODEC, mute_on, 0);
}


status_t SpeechDriverNormal::SetUplinkMute(bool mute_on) {
    static AudioLock muteLock;
    AL_AUTOLOCK(muteLock);

    if (isSpeechApplicationOn() == false) {
        return 0;
    }

    ALOGD("%s(), mUplinkMuteOn: %d => %d", __FUNCTION__, mUplinkMuteOn, mute_on);
    if (mute_on == mUplinkMuteOn) { return 0; }
    mUplinkMuteOn = mute_on;
    sph_msg_t sph_msg;
    return sendMailbox(&sph_msg, MSG_A2M_MUTE_SPH_UL, mute_on, 0);
}


status_t SpeechDriverNormal::SetUplinkSourceMute(bool mute_on) {
    static AudioLock muteLock;
    AL_AUTOLOCK(muteLock);

    if (isSpeechApplicationOn() == false) {
        return 0;
    }

    ALOGD("%s(), mUplinkSourceMuteOn: %d => %d", __FUNCTION__, mUplinkSourceMuteOn, mute_on);
    if (mute_on == mUplinkSourceMuteOn) { return 0; }
    mUplinkSourceMuteOn = mute_on;
    sph_msg_t sph_msg;
    return sendMailbox(&sph_msg, MSG_A2M_MUTE_SPH_UL_SOURCE, mute_on, 0);
}



/*==============================================================================
 *                     Device related Config
 *============================================================================*/

status_t SpeechDriverNormal::SetModemSideSamplingRate(uint16_t sample_rate) {
    mSampleRateEnum = sph_sample_rate_value_to_enum(sample_rate);
    return 0;
}


/*==============================================================================
 *                     Speech Enhancement Control
 *============================================================================*/

status_t SpeechDriverNormal::SetSpeechEnhancement(bool enhance_on) {
    ALOGD("%s(), enhance_on = %d ", __FUNCTION__, enhance_on);
    sph_msg_t sph_msg;
    return sendMailbox(&sph_msg, MSG_A2M_CTRL_SPH_ENH, enhance_on, 0);
}


status_t SpeechDriverNormal::SetSpeechEnhancementMask(const sph_enh_mask_struct_t &mask) {
    sph_msg_t sph_msg;

    uint16_t enh_dynamic_ctrl = speechEnhancementMaskWrapper(mask.dynamic_func);

    ALOGD("%s(), enh_dynamic_ctrl mask 0x%x", __FUNCTION__, enh_dynamic_ctrl);
    return sendMailbox(&sph_msg, MSG_A2M_ENH_CTRL_SUPPORT, enh_dynamic_ctrl, 0);
}

/*
 * from: sph_enh_dynamic_mask_t
 * to:   sph_enh_dynamic_ctrl_t
 */
uint16_t SpeechDriverNormal::speechEnhancementMaskWrapper(const uint32_t enh_dynamic_mask) {
    uint16_t enh_dynamic_ctrl = 0;

    /* DMNR */
    if (enh_dynamic_mask & SPH_ENH_DYNAMIC_MASK_DMNR) {
        enh_dynamic_ctrl |= SPH_ENH_DYNAMIC_CTRL_MASK_DMNR;
    }

    /* TDNC */
    enh_dynamic_ctrl |= SPH_ENH_DYNAMIC_CTRL_MASK_TDNC; /* always on */

    /* MAGIC CONFERENCE */
    if (enh_dynamic_mask & SPH_ENH_DYNAMIC_MASK_LSPK_DMNR) {
        enh_dynamic_ctrl |= SPH_ENH_DYNAMIC_CTRL_MASK_MAGIC_CONFERENCE;
    }

    return enh_dynamic_ctrl;
}


status_t SpeechDriverNormal::SetBtHeadsetNrecOn(const bool bt_headset_nrec_on) {
    ALOGD("%s(), mBtHeadsetNrecOn: %d => %d", __FUNCTION__, mBtHeadsetNrecOn, bt_headset_nrec_on);
    mBtHeadsetNrecOn = bt_headset_nrec_on; /* will be applied later in SetSpeechMode() */
    return 0;
}



/*==============================================================================
 *                     Speech Enhancement Parameters
 *============================================================================*/

int SpeechDriverNormal::parseSpeechParam(const int type, int *param_arg) {
    if (type < 0 || type >= NUM_AUDIO_TYPE_SPEECH_TYPE) {
        ALOGW("%s(), not support type %d", __FUNCTION__, type);
        return -EINVAL;
    }

    if (type == AUDIO_TYPE_SPEECH && !param_arg) {
        ALOGW("%s(), AUDIO_TYPE_SPEECH param_arg == NULL!! return", __FUNCTION__);
        return -EINVAL;
    }


    char *param_buf = (char *)mSpeechParam[type].p_buffer;
    uint16_t parsed_size = 0;
    int retval = 0;

    switch (type) {
    case AUDIO_TYPE_SPEECH:
        parsed_size = SpeechParamParser::getInstance()->GetSpeechParamUnit(param_buf, param_arg);
        break;
    case AUDIO_TYPE_SPEECH_GENERAL:
        parsed_size = SpeechParamParser::getInstance()->GetGeneralParamUnit(param_buf);
        break;
#ifdef MTK_SPH_MAGICLARITY_SHAPEFIR_SUPPORT
    case AUDIO_TYPE_SPEECH_MAGICLARITY:
        parsed_size = SpeechParamParser::getInstance()->GetMagiClarityParamUnit(param_buf);
        break;
#endif
    case AUDIO_TYPE_SPEECH_DMNR:
        if (AudioALSAHardwareResourceManager::getInstance()->getNumPhoneMicSupport() >= 2) {
            parsed_size = SpeechParamParser::getInstance()->GetDmnrParamUnit(param_buf);
        } else {
            ALOGW("%s(), not support type %d", __FUNCTION__, type);
            parsed_size = 0;
            retval = -EINVAL;
        }
        break;
    default:
        ALOGW("%s(), not support type %d", __FUNCTION__, type);
        parsed_size = 0;
        retval = -EINVAL;
        break;
    }

    if (parsed_size > mSpeechParam[type].memory_size) {
        ALOGW("%s(), type: %d, parsed_size %u > memory_size %u",
              __FUNCTION__, type, parsed_size, mSpeechParam[type].memory_size);
        WARNING("overflow!!");
        retval = -ENOMEM;
    }

    if (retval == 0) {
        mSpeechParam[type].data_size = parsed_size;
    }

    ALOGV("%s(), type: %d, parsed_size: %d", __FUNCTION__, type, parsed_size);
    return retval;
}


int SpeechDriverNormal::writeAllSpeechParametersToModem(uint16_t *p_length, uint16_t *p_index) {
    sph_msg_t sph_msg;

    uint16_t concat_size = 0;
    int retval = 0;

    // concat all param
    for (int i = 0; i < NUM_AUDIO_TYPE_SPEECH_TYPE; i++) {
        if (mSpeechParam[i].data_size > 0) {
            if (concat_size + mSpeechParam[i].data_size > MAX_SPEECH_PARAM_CONCATE_SIZE) {
                ALOGW("%s(), type: %d, data_size %u, concat_size %u overflow!!",
                      __FUNCTION__, i, mSpeechParam[i].data_size, concat_size);
                mSpeechParam[i].data_size = 0;
                retval = -EOVERFLOW;
                break;
            }
            memcpy((uint8_t *)mSpeechParamConcat + concat_size,
                   mSpeechParam[i].p_buffer,
                   mSpeechParam[i].data_size);
            concat_size += mSpeechParam[i].data_size;
            ALOGV("%s(), type: %d, data_size: %d, concat_size: %d", __FUNCTION__,
                  i, mSpeechParam[i].data_size, concat_size);
            mSpeechParam[i].data_size = 0;
        }
    }

    // write to share memory
    uint16_t write_idx = 0;
    if (retval == 0) {
        retval = mSpeechMessenger->writeSphParamToShareMemory(mSpeechParamConcat,
                                                              concat_size,
                                                              &write_idx);

        if (retval != 0) { // shm fail => use payload
            if (concat_size <= kMaxApPayloadDataSize) {
                sendPayload(&sph_msg, MSG_A2M_EM_DYNAMIC_SPH,
                            SHARE_BUFF_DATA_TYPE_CCCI_DYNAMIC_PARAM_TYPE,
                            mSpeechParamConcat, concat_size);
            }
        }
    }

    // update length & index
    if (retval == 0) {
        *p_length = concat_size;
        *p_index  = write_idx;
    }
    return retval;
}


status_t SpeechDriverNormal::SetDynamicSpeechParameters(const int type, const void *param_arg) {
    if (type < 0 || type >= NUM_AUDIO_TYPE_SPEECH_TYPE) {
        ALOGW("%s(), not support type %d", __FUNCTION__, type);
        return -EINVAL;
    }
    if (type == AUDIO_TYPE_SPEECH && !param_arg) {
        ALOGW("%s(), AUDIO_TYPE_SPEECH param_arg == NULL!! return", __FUNCTION__);
        return -EINVAL;
    }

    AL_AUTOLOCK_MS(mSpeechParamLock, MAX_SPEECH_AUTO_LOCK_TIMEOUT_MS); // atomic: write shm & send msg

    sph_msg_t sph_msg;
    int retval = 0;

    // parse
    retval = parseSpeechParam(type, (int *)param_arg);

    // share memory
    uint16_t write_idx = 0;
    if (retval == 0) {
        retval = mSpeechMessenger->writeSphParamToShareMemory(mSpeechParam[type].p_buffer,
                                                              mSpeechParam[type].data_size,
                                                              &write_idx);
    }

    // send sph param to modem side
    if (retval == 0) { // via share memory
        retval = sendMailbox(&sph_msg, MSG_A2M_DYNAMIC_PAR_IN_STRUCT_SHM,
                             mSpeechParam[type].data_size, write_idx);
    } else { // via payload
        if (mSpeechParam[type].data_size <= kMaxApPayloadDataSize) {
            retval = sendPayload(&sph_msg, MSG_A2M_EM_DYNAMIC_SPH,
                                 SHARE_BUFF_DATA_TYPE_CCCI_DYNAMIC_PARAM_TYPE,
                                 mSpeechParam[type].p_buffer,
                                 mSpeechParam[type].data_size);
        }
    }

    ALOGV("%s(), type: %d, data_size: %d", __FUNCTION__, type, mSpeechParam[type].data_size);
    return retval;
}


status_t SpeechDriverNormal::GetVibSpkParam(void *eVibSpkParam) {
    /* error handling */
    if (eVibSpkParam == NULL) {
        ALOGW("%s(), eVibSpkParam == NULL!! return", __FUNCTION__);
        return -EFAULT;
    }

    int32_t frequency;
    AUDIO_ACF_CUSTOM_PARAM_STRUCT audioParam;
    getAudioCompFltCustParam(AUDIO_COMP_FLT_VIBSPK, &audioParam);
    PARAM_VIBSPK *pParamVibSpk = (PARAM_VIBSPK *)eVibSpkParam;
    int dTableIndex;

    if (audioParam.bes_loudness_WS_Gain_Max != VIBSPK_CALIBRATION_DONE && audioParam.bes_loudness_WS_Gain_Max != VIBSPK_SETDEFAULT_VALUE) {
        frequency = VIBSPK_DEFAULT_FREQ;
    } else {
        frequency = audioParam.bes_loudness_WS_Gain_Min;
    }

    if (frequency < VIBSPK_FREQ_LOWBOUND) {
        dTableIndex = 0;
    } else {
        dTableIndex = (frequency - VIBSPK_FREQ_LOWBOUND + 1) / VIBSPK_FILTER_FREQSTEP;
    }

    if (dTableIndex < VIBSPK_FILTER_NUM && dTableIndex >= 0) {
        memcpy(pParamVibSpk->pParam, &SPH_VIBR_FILTER_COEF_Table[dTableIndex], sizeof(uint16_t)*VIBSPK_SPH_PARAM_SIZE);
    }

    if (IsAudioSupportFeature(AUDIO_SUPPORT_2IN1_SPEAKER)) {
        pParamVibSpk->flag2in1 = false;
    } else {
        pParamVibSpk->flag2in1 = true;
    }

    return 0;
}


status_t SpeechDriverNormal::SetVibSpkParam(void *eVibSpkParam) {
    sph_msg_t sph_msg;

    /* error handling */
    if (eVibSpkParam == NULL) {
        ALOGW("%s(), eVibSpkParam == NULL!! return", __FUNCTION__);
        return -EFAULT;
    }

    /* payload (keep lagacy code) */
    return sendPayload(&sph_msg, MSG_A2M_VIBSPK_PARAMETER,
                       SHARE_BUFF_DATA_TYPE_CCCI_VIBSPK_PARAM,
                       eVibSpkParam, sizeof(PARAM_VIBSPK));
}


status_t SpeechDriverNormal::GetSmartpaParam(void *eParamSmartpa) {
    /* error handling */
    if (eParamSmartpa == NULL) {
        ALOGW("%s(), eParamSmartpa == NULL!! return", __FUNCTION__);
        return -EFAULT;
    }

    return 0;
}


status_t SpeechDriverNormal::SetSmartpaParam(void *eParamSmartpa) {
    /* error handling */
    if (eParamSmartpa == NULL) {
        ALOGW("%s(), eParamSmartpa == NULL!! return", __FUNCTION__);
        return -EFAULT;
    }

    return 0;
}



/*==============================================================================
 *                     Recover State
 *============================================================================*/

void SpeechDriverNormal::RecoverModemSideStatusToInitState() {
    if (mModemSideModemStatus != 0) {
        ALOGD("%s(), mModemIndex: %d, mModemSideModemStatus: 0x%x", __FUNCTION__,
              mModemIndex, mModemSideModemStatus);
        mApResetDuringSpeech = true;
    }

    // Raw Record
    if (getModemSideModemStatus(RAW_RECORD_STATUS_MASK) == true) {
        ALOGD("%s(), mModemIndex = %d, raw_record_on = true",  __FUNCTION__, mModemIndex);
        SetApSideModemStatus(RAW_RECORD_STATUS_MASK);
        RecordOff(mRecordType);
    }

    // VM Record
    if (getModemSideModemStatus(VM_RECORD_STATUS_MASK) == true) {
        ALOGD("%s(), mModemIndex = %d, vm_on = true",  __FUNCTION__, mModemIndex);
        SetApSideModemStatus(VM_RECORD_STATUS_MASK);
        VoiceMemoRecordOff();
    }

    // BGS
    if (getModemSideModemStatus(BGS_STATUS_MASK) == true) {
        ALOGD("%s(), mModemIndex = %d, bgs_on = true", __FUNCTION__, mModemIndex);
        SetApSideModemStatus(BGS_STATUS_MASK);
        BGSoundOff();
    }

    // TTY
    if (getModemSideModemStatus(TTY_STATUS_MASK) == true) {
        ALOGD("%s(), mModemIndex = %d, tty_on = true", __FUNCTION__, mModemIndex);
        SetApSideModemStatus(TTY_STATUS_MASK);
        TtyCtmOff();
    }

    // P2W
    if (getModemSideModemStatus(P2W_STATUS_MASK) == true) {
        ALOGD("%s(), mModemIndex = %d, p2w_on = true", __FUNCTION__, mModemIndex);
        SetApSideModemStatus(P2W_STATUS_MASK);
        PCM2WayOff();
    }

    // SPH (Phone Call / VT / Loopback / ...)
    if (getModemSideModemStatus(SPEECH_STATUS_MASK) == true) {
        ALOGD("%s(), mModemIndex = %d, speech_on = true", __FUNCTION__, mModemIndex);
        SetApSideModemStatus(SPEECH_STATUS_MASK);
        mApplication = SPH_APPLICATION_NORMAL;
        SpeechOff();
    }
    mApResetDuringSpeech = false;
}



/*==============================================================================
 *                     Check Modem Status
 *============================================================================*/

bool SpeechDriverNormal::CheckModemIsReady() {
    if (mSpeechMessenger == NULL) {
        return false;
    }

    return (mSpeechMessenger->checkModemReady() == true &&
            mModemResetDuringSpeech == false);
}



/*==============================================================================
 *                     Delay sync
 *============================================================================*/

int SpeechDriverNormal::getBtDelayTime(uint16_t *p_bt_delay_ms) {
    if (p_bt_delay_ms == NULL) {
        ALOGW("%s(), p_bt_delay_ms == NULL!! return", __FUNCTION__);
        return -EFAULT;
    }

    if (strlen(mBtHeadsetName) == 0) {
        ALOGW("%s(), mBtHeadsetName invalid!!", __FUNCTION__);
        *p_bt_delay_ms = 0;
        return -ENODEV;
    }

    *p_bt_delay_ms = SpeechParamParser::getInstance()->GetBtDelayTime(mBtHeadsetName);

    return 0;
}


int SpeechDriverNormal::getUsbDelayTime(uint8_t *p_usb_delay_ms) {
    if (p_usb_delay_ms == NULL) {
        ALOGW("%s(), p_usb_delay_ms == NULL!! return", __FUNCTION__);
        return -EFAULT;
    }

#ifndef MTK_USB_PHONECALL
    *p_usb_delay_ms = 0;
    return 0;
#else

    SPEECH_ECHOREF_PARAM_STRUCT *p_echo_ref_param = NULL;

    char *param_buf = (char *)mSpeechParam[AUDIO_TYPE_SPEECH_ECHOREF].p_buffer;
    uint16_t parsed_size = 0;

    uint32_t echo_ref_param_offset = sizeof(SPEECH_DYNAMIC_PARAM_UNIT_HDR_STRUCT) + sizeof(uint16_t);
    uint32_t echo_ref_param_unit_size = echo_ref_param_offset + sizeof(SPEECH_ECHOREF_PARAM_STRUCT);

    /* SpeechEchoRef_AudioParam.xml */
    parsed_size = SpeechParamParser::getInstance()->GetEchoRefParamUnit(param_buf);
    if (parsed_size != echo_ref_param_unit_size) {
        ALOGW("%s(), parsed_size %u != echo_ref_param_unit_size %u!!", __FUNCTION__,
              parsed_size, echo_ref_param_unit_size);
        ASSERT(parsed_size == echo_ref_param_unit_size);
        *p_usb_delay_ms = 0;
        return -EINVAL;
    }

    p_echo_ref_param = (SPEECH_ECHOREF_PARAM_STRUCT *)(param_buf + echo_ref_param_offset);
    ALOGV("%s(), %d, 0x%x, %d", __FUNCTION__,
          p_echo_ref_param->speech_common_para[0],
          p_echo_ref_param->speech_common_para[1],
          p_echo_ref_param->speech_common_para[2]);

    *p_usb_delay_ms = p_echo_ref_param->speech_common_para[1] & 0xFF;

    return 0;
#endif
}



} /* end of namespace android */

