#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "AudioALSASpeechPhoneCallController"
#include "AudioALSASpeechPhoneCallController.h"

#include <cutils/properties.h>

#include <pthread.h>
#include <utils/threads.h>

#include <hardware_legacy/power.h>

#include <SpeechUtility.h>

#include "AudioALSAHardwareResourceManager.h"
#include "AudioALSAStreamManager.h"

#include "AudioVolumeFactory.h"
#include "SpeechDriverFactory.h"

#include "SpeechEnhancementController.h"
#include "SpeechPcm2way.h"
#include "SpeechBGSPlayer.h"
#include "SpeechVMRecorder.h"
#include "WCNChipController.h"
#include "AudioALSADriverUtility.h"
#include "AudioALSADeviceParser.h"
#if defined(MTK_AUDIO_HIERARCHICAL_PARAM_SUPPORT)
#include "SpeechParamParser.h"
#endif

#ifdef MTK_AURISYS_PHONE_CALL_SUPPORT
#include <audio_task.h>
#include <AudioMessengerIPI.h>
#endif
#ifdef SPH_VCL_SUPPORT
#include "SpeechVoiceCustomLogger.h"
#endif
#if defined(MTK_SPEECH_ENCRYPTION_SUPPORT)
#include "SpeechDataEncrypter.h"
#endif

#if defined(MTK_USB_PHONECALL)
#include "AudioUSBPhoneCallController.h"
#endif

#include "AudioSCPPhoneCallController.h"


namespace android {

/*==============================================================================
 *                     Property keys
 *============================================================================*/
static const char PROPERTY_KEY_MIC_MUTE_ON[PROPERTY_KEY_MAX] = "af.recovery.mic_mute_on";
static const char PROPERTY_KEY_DL_MUTE_ON[PROPERTY_KEY_MAX] = "af.recovery.dl_mute_on";
static const char PROPERTY_KEY_UL_MUTE_ON[PROPERTY_KEY_MAX] = "af.recovery.ul_mute_on";
static const char PROPERTY_KEY_PHONE1_MD[PROPERTY_KEY_MAX] = "af.recovery.phone1.md";
static const char PROPERTY_KEY_PHONE2_MD[PROPERTY_KEY_MAX] = "af.recovery.phone2.md";
static const char PROPERTY_KEY_FOREGROUND_PHONE_ID[PROPERTY_KEY_MAX] = "af.recovery.phone_id";


static const char WAKELOCK_NAME[] = "EXT_MD_AUDIO_WAKELOCK";


#define DEFAULT_WAIT_SHUTTER_SOUND_UNMUTE_MS (1000) /* 1 sec */
#define DEFAULT_WAIT_ROUTING_UNMUTE_MS (150) /* 150ms */

enum {
    SPH_MUTE_THREAD_STATE_IDLE,
    SPH_MUTE_THREAD_STATE_WAIT
};

enum {
    SPH_MUTE_CTRL_IDLE,
    SPH_MUTE_CTRL_ROUTING_START,
    SPH_MUTE_CTRL_ROUTING_END,
    SPH_MUTE_CTRL_VOLUME_UPDATE
};

enum {
    RTT_CALL_TYPE_CS = 0,
    RTT_CALL_TYPE_RTT = 1,
    RTT_CALL_TYPE_PS = 2,
    RTT_CALL_TYPE_CS_NO_TTY = 3
};

enum {
    AUD_RTT_OFF = 0,
    AUD_RTT_ON = 1
};

static struct mixer *mMixer;

#if !defined (MTK_AUDIO_HIERARCHICAL_PARAM_SUPPORT)
static void setTtyEnhancement(tty_mode_t tty_mode, audio_devices_t routing_device);
#endif

AudioALSASpeechPhoneCallController *AudioALSASpeechPhoneCallController::mSpeechPhoneCallController = NULL;
AudioALSASpeechPhoneCallController *AudioALSASpeechPhoneCallController::getInstance() {
    static AudioLock mGetInstanceLock;
    AL_AUTOLOCK(mGetInstanceLock);

    if (mSpeechPhoneCallController == NULL) {
        mSpeechPhoneCallController = new AudioALSASpeechPhoneCallController();
    }
    ASSERT(mSpeechPhoneCallController != NULL);
    return mSpeechPhoneCallController;
}

AudioALSASpeechPhoneCallController::AudioALSASpeechPhoneCallController() :
    mHardwareResourceManager(AudioALSAHardwareResourceManager::getInstance()),
    mStreamManager(NULL),
    mAudioALSAVolumeController(AudioVolumeFactory::CreateAudioVolumeController()),
    mSpeechDriverFactory(SpeechDriverFactory::GetInstance()),
    mAudioBTCVSDControl(NULL),
    mAudioMode(AUDIO_MODE_NORMAL),
    mMicMute(false),
    mVtNeedOn(false),
    bAudioTaste(false),
    mTtyMode(AUD_TTY_OFF),
    mRoutingForTty(AUDIO_DEVICE_OUT_EARPIECE),
    mPhonecallInputDevice(AUDIO_DEVICE_NONE),
    mPhonecallOutputDevice(AUDIO_DEVICE_NONE),
    mBTMode(0),
    mIdxMD(MODEM_1),
    mPcmIn(NULL),
    mPcmOut(NULL),
#ifdef SPEECH_PMIC_RESET
    mEnable_PMIC_Reset(false),
#endif
    mRfInfo(0),
    mRfMode(0),
    mASRCNeedOn(0),
    mSpeechDVT_SampleRate(0),
    mSpeechDVT_MD_IDX(0),
    mIsSidetoneEnable(false),
    hMuteDlCodecForShutterSoundThread(0),
    mMuteDlCodecForShutterSoundThreadEnable(false),
    mMuteDlCodecForShutterSoundCount(0),
    mDownlinkMuteCodec(false),
    mMuteDlUlForRoutingThread(0),
    mMuteDlUlForRoutingThreadEnable(false),
    mMuteDlUlForRoutingState(SPH_MUTE_THREAD_STATE_IDLE),
    mMuteDlUlForRoutingCtrl(SPH_MUTE_CTRL_IDLE),
    mRttCallType(RTT_CALL_TYPE_CS),
    mRttMode(AUD_RTT_OFF) {

    mLogEnable = __android_log_is_loggable(ANDROID_LOG_DEBUG, LOG_TAG, ANDROID_LOG_INFO);
    // check need mute mic or not after kill mediaserver
    char mic_mute_on[PROPERTY_VALUE_MAX];
    property_get(PROPERTY_KEY_MIC_MUTE_ON, mic_mute_on, "0"); //"0": default off
    mMicMute = (mic_mute_on[0] == '0') ? false : true;

    // Need Mute DL Voice
    char dl_mute_on[PROPERTY_VALUE_MAX];
    property_get(PROPERTY_KEY_DL_MUTE_ON, dl_mute_on, "0"); //"0": default off
    mDlMute = (dl_mute_on[0] == '0') ? false : true;

    // Need Mute UL Voice
    char ul_mute_on[PROPERTY_VALUE_MAX];
    property_get(PROPERTY_KEY_UL_MUTE_ON, ul_mute_on, "0"); //"0": default off
    mUlMute = (ul_mute_on[0] == '0') ? false : true;

    char phone_id[PROPERTY_VALUE_MAX];
    property_get(PROPERTY_KEY_FOREGROUND_PHONE_ID, phone_id, "0"); //"0": default 0
    mPhoneId = (phone_id[0] == '0') ? PHONE_ID_0 : PHONE_ID_1;

    char phone1_md[PROPERTY_VALUE_MAX];
    property_get(PROPERTY_KEY_PHONE1_MD, phone1_md, "0"); //"0": default MD1
    mIdxMDByPhoneId[0] = (phone1_md[0] == '0') ? MODEM_1 : MODEM_EXTERNAL;

    char phone2_md[PROPERTY_VALUE_MAX];
    property_get(PROPERTY_KEY_PHONE2_MD, phone2_md, "0"); //"0": default MD1
    mIdxMDByPhoneId[1] = (phone2_md[0] == '0') ? MODEM_1 : MODEM_EXTERNAL;

    mMixer = AudioALSADriverUtility::getInstance()->getMixer();
    ASSERT(mMixer != NULL);
    // initialize mConfig
    memset((void *)&mConfig, 0, sizeof(pcm_config));

    hMuteDlCodecForShutterSoundThread = 0;
    mMuteDlCodecForShutterSoundCount = 0;
    mMuteDlCodecForShutterSoundThreadEnable = false;
    mDownlinkMuteCodec = false;

    mMuteDlUlForRoutingThread = 0;
    mMuteDlUlForRoutingThreadEnable = false;
    mMuteDlUlForRoutingState = SPH_MUTE_THREAD_STATE_IDLE;
    mMuteDlUlForRoutingCtrl = SPH_MUTE_CTRL_IDLE;

}

AudioALSASpeechPhoneCallController::~AudioALSASpeechPhoneCallController() {

}

bool AudioALSASpeechPhoneCallController::checkTtyNeedOn() const {
    return (mTtyMode != AUD_TTY_OFF && mVtNeedOn == false && mTtyMode != AUD_TTY_ERR &&
#if defined(MTK_RTT_SUPPORT)
            mRttCallType == RTT_CALL_TYPE_CS &&
#endif
            (!audio_is_bluetooth_sco_device(getRoutingForTty())));
}

bool AudioALSASpeechPhoneCallController::checkSideToneFilterNeedOn(const audio_devices_t output_device) const {
    // TTY do not use STMF. Open only for earphone & receiver when side tone != 0.
    return ((checkTtyNeedOn() == false) &&
            //disable the condition, turn on sidetone without check the gain value
            //            (mAudioALSAVolumeController->GetSideToneGain(output_device) != 0) &&
            (output_device == AUDIO_DEVICE_OUT_WIRED_HEADPHONE ||
             output_device == AUDIO_DEVICE_OUT_WIRED_HEADSET ||
             output_device == AUDIO_DEVICE_OUT_EARPIECE));
}

status_t AudioALSASpeechPhoneCallController::init() {
    return NO_ERROR;
}


audio_devices_t AudioALSASpeechPhoneCallController::getInputDeviceForPhoneCall(const audio_devices_t output_devices) {
    audio_devices_t input_device;

    switch (output_devices) {
    case AUDIO_DEVICE_OUT_WIRED_HEADSET: {
        if (mTtyMode == AUD_TTY_VCO) {
            input_device = AUDIO_DEVICE_IN_BUILTIN_MIC;
            ALOGD("%s(), headset, TTY_VCO, input_device(0x%x)", __FUNCTION__, input_device);
        } else {
            input_device = AUDIO_DEVICE_IN_WIRED_HEADSET;
        }
        break;
    }
    case AUDIO_DEVICE_OUT_EARPIECE:
    case AUDIO_DEVICE_OUT_WIRED_HEADPHONE: {
        input_device = AUDIO_DEVICE_IN_BUILTIN_MIC;
        break;
    }
    case AUDIO_DEVICE_OUT_SPEAKER: {
        if (mTtyMode == AUD_TTY_HCO || mTtyMode == AUD_TTY_VCO) {
            input_device = AUDIO_DEVICE_IN_WIRED_HEADSET;
            ALOGD("%s(), speaker, mTtyMode(%d), input_device(0x%x)", __FUNCTION__, mTtyMode,  input_device);
        } else {
            if (USE_REFMIC_IN_LOUDSPK == 1) {
                input_device = AUDIO_DEVICE_IN_BACK_MIC;
            } else {
                input_device = AUDIO_DEVICE_IN_BUILTIN_MIC;
            }
        }
        break;
    }
    case AUDIO_DEVICE_OUT_BLUETOOTH_SCO:
    case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET:
    case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT: {
        input_device = AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET;
        break;
    }
#if defined(MTK_USB_PHONECALL)
    case AUDIO_DEVICE_OUT_USB_DEVICE: {
        input_device = AudioUSBPhoneCallController::getInstance()->getUSBCallInDevice();
        break;
    }
#endif
    default: {
        ALOGW("%s(), no support such output_devices(0x%x), default use AUDIO_DEVICE_IN_BUILTIN_MIC(0x%x) as input_device",
              __FUNCTION__, output_devices, AUDIO_DEVICE_IN_BUILTIN_MIC);
        input_device = AUDIO_DEVICE_IN_BUILTIN_MIC;
        break;
    }
    }

    return input_device;
}

status_t AudioALSASpeechPhoneCallController::open(const audio_mode_t audio_mode, const audio_devices_t output_devices, const audio_devices_t input_device) {
    AL_AUTOLOCK(mLock);
    AL_AUTOLOCK(*AudioALSADriverUtility::getInstance()->getStreamSramDramLock());

    ALOGD("%s(+), mAudioMode: %d => %d, output_devices: 0x%x, input_device: 0x%x",
          __FUNCTION__, mAudioMode, audio_mode, output_devices, input_device);

    int PcmInIdx = 0;
    int PcmOutIdx = 0;
    int CardIndex = 0;
    uint32_t sample_rate = 0;

    mLogEnable = __android_log_is_loggable(ANDROID_LOG_DEBUG, LOG_TAG, ANDROID_LOG_INFO);

    // set speech driver instance
    if (bAudioTaste) {
        mIdxMD = MODEM_1;
    } else {
        modem_index_t modem_index_property = updatePhysicalModemIdx(audio_mode);
        mIdxMD = modem_index_property;
    }

    hMuteDlCodecForShutterSoundThread = 0;
    mMuteDlCodecForShutterSoundCount = 0;
    mMuteDlCodecForShutterSoundThreadEnable = true;
    mDownlinkMuteCodec = false;
    int ret = pthread_create(&hMuteDlCodecForShutterSoundThread, NULL,
                             AudioALSASpeechPhoneCallController::muteDlCodecForShutterSoundThread,
                             (void *)this);
    ASSERT(ret == 0);

    mMuteDlUlForRoutingThread = 0;
    mMuteDlUlForRoutingThreadEnable = true;
    mMuteDlUlForRoutingState = SPH_MUTE_THREAD_STATE_IDLE;
    ret = pthread_create(&mMuteDlUlForRoutingThread, NULL,
                         AudioALSASpeechPhoneCallController::muteDlUlForRoutingThread,
                         (void *)this);
    ASSERT(ret == 0);

    mSpeechDriverFactory->SetActiveModemIndex(mIdxMD);
    char isC2kSupported[PROPERTY_VALUE_MAX];
    property_get("ro.boot.opt_c2k_support", isC2kSupported, "0"); //"0": default not support

    if (strcmp(isC2kSupported, "1") != 0) {
        // wake lock for external modem
        if (mIdxMD == MODEM_EXTERNAL) {
            int ret = acquire_wake_lock(PARTIAL_WAKE_LOCK, WAKELOCK_NAME);
        }
    }
#ifdef MTK_AURISYS_PHONE_CALL_SUPPORT
    /* Load task scene when opening */
    AudioMessengerIPI *pIPI = AudioMessengerIPI::getInstance();
    pIPI->loadTaskScene(TASK_SCENE_PHONE_CALL);
#endif
    SpeechDriverInterface *pSpeechDriver = mSpeechDriverFactory->GetSpeechDriver();
    mPhonecallOutputDevice = output_devices;

    // check BT device
    const bool bt_device_on = audio_is_bluetooth_sco_device(output_devices);
    if (mSpeechDVT_SampleRate != 0) {
        sample_rate = mSpeechDVT_SampleRate;
        ALOGD("%s(), SpeechDVT sample_rate = %d", __FUNCTION__, sample_rate);
    } else {
        sample_rate = calculateSampleRate(bt_device_on);
    }


    //--- here to test pcm interface platform driver_attach
    if (bt_device_on) {
        if (WCNChipController::GetInstance()->IsBTMergeInterfaceSupported() == true) {
            memset(&mConfig, 0, sizeof(mConfig));
            mConfig.channels = 1;
            mConfig.rate = sample_rate;
            mConfig.period_size = 4096;
            mConfig.period_count = 2;
            mConfig.format = PCM_FORMAT_S16_LE;
            mConfig.start_threshold = 0;
            mConfig.stop_threshold = 0;
            mConfig.silence_threshold = 0;
            ALOGE_IF(mPcmOut != NULL, "%s(), mPcmOut = %p", __FUNCTION__, mPcmOut);
            ASSERT(mPcmOut == NULL);

            if (mIdxMD == MODEM_EXTERNAL || mIdxMD == MODEM_2) {
                PcmOutIdx = AudioALSADeviceParser::getInstance()->GetPcmIndexByString(keypcmVoiceMD2BT);
                CardIndex = AudioALSADeviceParser::getInstance()->GetCardIndexByString(keypcmVoiceMD2BT);
                mPcmOut = pcm_open(CardIndex, PcmOutIdx, PCM_OUT, &mConfig);
            } else {
                PcmOutIdx = AudioALSADeviceParser::getInstance()->GetPcmIndexByString(keypcmVoiceMD1BT);
                CardIndex = AudioALSADeviceParser::getInstance()->GetCardIndexByString(keypcmVoiceMD1BT);
                mPcmOut = pcm_open(CardIndex, PcmOutIdx, PCM_OUT, &mConfig);
            }

            ALOGD_IF(mPcmOut == NULL, "%s(), mPcmOut = %p, PcmOutIdx = %d, CardIndex = %d", __FUNCTION__, mPcmOut, PcmOutIdx, CardIndex);
            ASSERT(mPcmOut != NULL);
            pcm_start(mPcmOut);
        }
#if defined(MTK_USB_PHONECALL)
    } else if (AudioUSBPhoneCallController::getInstance()->isForceUSBCall() ||
               output_devices == AUDIO_DEVICE_OUT_USB_DEVICE) {
        AudioUSBPhoneCallController::getInstance()->enable(sample_rate);
#endif
    } else {
        ALOGE_IF(mPcmIn != NULL, "%s(), mPcmIn = %p", __FUNCTION__, mPcmIn);
        ALOGE_IF(mPcmOut != NULL, "%s(), mPcmOut = %p", __FUNCTION__, mPcmOut);
        ASSERT(mPcmIn == NULL && mPcmOut == NULL);

#ifdef MTK_VOICE_ULTRA
        if (output_devices == AUDIO_DEVICE_OUT_EARPIECE) {
            PcmInIdx = AudioALSADeviceParser::getInstance()->GetPcmIndexByString(keypcmVoiceUltra);
            PcmOutIdx = AudioALSADeviceParser::getInstance()->GetPcmIndexByString(keypcmVoiceUltra);
            CardIndex = AudioALSADeviceParser::getInstance()->GetCardIndexByString(keypcmVoiceUltra);
            String8 mixer_ctl_name;
            if (mIdxMD == MODEM_EXTERNAL || mIdxMD == MODEM_2) {
                mixer_ctl_name = "md2";
            } else {
                mixer_ctl_name = "md1";
            }

            if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Ultra_Modem_Select"), mixer_ctl_name.string())) {
                ALOGE("Error: Ultra_Modem_Select invalid value");
            }
            // use pcm out to set memif, ultrasound, downlink
            enum pcm_format memifFormat = PCM_FORMAT_S16_LE;    // or PCM_FORMAT_S32_LE
            unsigned int ultraRate = 96000;
            unsigned int msPerPeriod = 10;  // note: max sram is 48k

            memset(&mConfig, 0, sizeof(mConfig));
            mConfig.channels = 1;
            mConfig.rate = ultraRate;
            mConfig.period_size = (ultraRate * msPerPeriod) / 1000;
            mConfig.period_count = 2;
            mConfig.format = memifFormat;

            mPcmOut = pcm_open(CardIndex, PcmOutIdx, PCM_OUT, &mConfig);

            // use pcm in to set modem, uplink
            memset(&mConfig, 0, sizeof(mConfig));
            mConfig.channels = 2;
            mConfig.rate = sample_rate;
            mConfig.period_size = 1024;
            mConfig.period_count = 2;
            mConfig.format = PCM_FORMAT_S16_LE;
            mConfig.start_threshold = 0;
            mConfig.stop_threshold = 0;
            mConfig.silence_threshold = 0;
            mPcmIn = pcm_open(CardIndex, PcmInIdx, PCM_IN, &mConfig);
        } else
#endif
        {
            if (mIdxMD == MODEM_EXTERNAL || mIdxMD == MODEM_2) {
                PcmInIdx = AudioALSADeviceParser::getInstance()->GetPcmIndexByString(keypcmVoiceMD2);
                PcmOutIdx = AudioALSADeviceParser::getInstance()->GetPcmIndexByString(keypcmVoiceMD2);
                CardIndex = AudioALSADeviceParser::getInstance()->GetCardIndexByString(keypcmVoiceMD2);
            } else {
                PcmInIdx = AudioALSADeviceParser::getInstance()->GetPcmIndexByString(keypcmVoiceMD1);
                PcmOutIdx = AudioALSADeviceParser::getInstance()->GetPcmIndexByString(keypcmVoiceMD1);
                CardIndex = AudioALSADeviceParser::getInstance()->GetCardIndexByString(keypcmVoiceMD1);
            }
            memset(&mConfig, 0, sizeof(mConfig));
            mConfig.channels = mHardwareResourceManager->getNumPhoneMicSupport();
            mConfig.rate = sample_rate;
            mConfig.period_size = 1024;
            mConfig.period_count = 2;
            mConfig.format = PCM_FORMAT_S16_LE;
            mConfig.start_threshold = 0;
            mConfig.stop_threshold = 0;
            mConfig.silence_threshold = 0;
            mPcmIn = pcm_open(CardIndex, PcmInIdx, PCM_IN, &mConfig);
            mConfig.channels = 2;
            mPcmOut = pcm_open(CardIndex, PcmOutIdx, PCM_OUT, &mConfig);
        }
        ALOGD_IF(mPcmIn == NULL, "%s(), mPcmIn = %p, PcmInIdx = %d, CardIndex = %d", __FUNCTION__, mPcmIn, PcmInIdx, CardIndex);
        ALOGD_IF(mPcmOut == NULL, "%s(), mPcmOut = %p, PcmOutIdx = %d, CardIndex = %d", __FUNCTION__, mPcmOut, PcmOutIdx, CardIndex);
        ASSERT(mPcmIn != NULL && mPcmOut != NULL);

        pcm_start(mPcmIn);
        pcm_start(mPcmOut);
        if (AudioSCPPhoneCallController::getInstance()->isSupportPhonecall(output_devices)) {
            AudioSCPPhoneCallController::getInstance()->enable(sample_rate);
        }
    }

    if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Speech_MD_USAGE"), "On")) {
        ALOGE("Error: Speech_MD invalid value");
    }

    if (checkTtyNeedOn() == true) {
        setTtyInOutDevice(getRoutingForTty());
    } else {
        // Set PMIC digital/analog part - uplink has pop, open first
        mPhonecallInputDevice = input_device;

        // Set PMIC digital/analog part - uplink has pop, open first
#if defined(MTK_USB_PHONECALL)
        if (!AudioUSBPhoneCallController::getInstance()->isEnable()) {
            mHardwareResourceManager->startInputDevice(mPhonecallInputDevice);
        }
#else
        mHardwareResourceManager->startInputDevice(mPhonecallInputDevice);
#endif
    }

    // start Side Tone Filter
    if (checkSideToneFilterNeedOn(output_devices) == true) {
        mHardwareResourceManager->EnableSideToneFilter(true);
        mIsSidetoneEnable = true;
    }
#if defined(MTK_SPEECH_ENCRYPTION_SUPPORT)
    if (SpeechDataEncrypter::GetInstance()->GetEnableStatus()) {
        SpeechDataEncrypter::GetInstance()->Start();
    }
#endif

    // Set MD side sampling rate
    pSpeechDriver->SetModemSideSamplingRate(sample_rate);

    // Set speech mode
    if (checkTtyNeedOn() == false) {
#if defined(MTK_USB_PHONECALL)
        if (AudioUSBPhoneCallController::getInstance()->isForceUSBCall()) {
            pSpeechDriver->SetSpeechMode(mPhonecallInputDevice, AUDIO_DEVICE_OUT_USB_DEVICE);
        } else {
            pSpeechDriver->SetSpeechMode(mPhonecallInputDevice, output_devices);
        }

        if (!AudioUSBPhoneCallController::getInstance()->isEnable()) {
            mHardwareResourceManager->startOutputDevice(output_devices, sample_rate);
        }
#else
        pSpeechDriver->SetSpeechMode(mPhonecallInputDevice, output_devices);
        mHardwareResourceManager->startOutputDevice(output_devices, sample_rate);
#endif
    }

    // Speech/VT on
    if (mVtNeedOn == true) {
        pSpeechDriver->VideoTelephonyOn();

        // trun on P2W for Video Telephony
        bool wideband_on = false; // VT default use Narrow Band (8k), modem side will SRC to 16K
        pSpeechDriver->PCM2WayOn(wideband_on);
    } else {
        pSpeechDriver->SpeechOn();

        // turn on TTY
        if (checkTtyNeedOn() == true) {
            pSpeechDriver->TtyCtmOn(mTtyMode);
#if !defined (MTK_AUDIO_HIERARCHICAL_PARAM_SUPPORT)
            setTtyEnhancement(mTtyMode, getRoutingForTty());
#endif
        }
    }

#if defined(MTK_RTT_SUPPORT)
    pSpeechDriver->RttConfig(mRttMode);
#endif

#ifdef SPH_VCL_SUPPORT
    // check VCL need open
    SpeechVoiceCustomLogger *pSpeechVoiceCustomLogger = SpeechVoiceCustomLogger::GetInstance();
    if (pSpeechVoiceCustomLogger->UpdateVCLSwitch() == true) {
        pSpeechVoiceCustomLogger->Open();
    }
#endif

    // check VM need open
    SpeechVMRecorder *pSpeechVMRecorder = SpeechVMRecorder::GetInstance();
    if (pSpeechVMRecorder->GetVMRecordCapability() == true) {
        pSpeechVMRecorder->Open();
    }

    mAudioMode = audio_mode;

    ALOGD("%s(-), mAudioMode: %d, mIdxMD: %d, bt_device_on: %d, sample_rate: %u"
          ", isC2kSupported: %s"
          ", CardIndex: %d, PcmInIdx: %d, PcmOutIdx: %d, mPcmIn: %p, mPcmOut: %p",
          __FUNCTION__, mAudioMode, mIdxMD, bt_device_on, sample_rate,
          isC2kSupported,
          CardIndex, PcmInIdx, PcmOutIdx, mPcmIn, mPcmOut);

    return NO_ERROR;
}


status_t AudioALSASpeechPhoneCallController::close() {
    AL_AUTOLOCK(mLock);
    AL_AUTOLOCK(*AudioALSADriverUtility::getInstance()->getStreamSramDramLock());
    ALOGD("%s(), mAudioMode: %d => 0", __FUNCTION__, mAudioMode);

    const modem_index_t modem_index = mSpeechDriverFactory->GetActiveModemIndex();
    const audio_devices_t old_output_device = mHardwareResourceManager->getOutputDevice();

#ifdef SPEECH_PMIC_RESET
    mEnable_PMIC_Reset = false;//close thread_PMIC_Reset
#endif
    // Get current active speech driver
    SpeechDriverInterface *pSpeechDriver = mSpeechDriverFactory->GetSpeechDriver();
    pSpeechDriver->SetUplinkMute(true);
#if defined(MTK_SPEECH_ENCRYPTION_SUPPORT)
    if (SpeechDataEncrypter::GetInstance()->GetStartStatus()) {
        SpeechDataEncrypter::GetInstance()->Stop();
    }
#endif

    // check VM need close
    SpeechVMRecorder *pSpeechVMRecorder = SpeechVMRecorder::GetInstance();
    if (pSpeechVMRecorder->GetVMRecordStatus() == true) {
        pSpeechVMRecorder->Close();
    }

#ifdef SPH_VCL_SUPPORT
    // check VCL need open
    SpeechVoiceCustomLogger *pSpeechVoiceCustomLogger = SpeechVoiceCustomLogger::GetInstance();
    if (pSpeechVoiceCustomLogger->GetVCLRecordStatus() == true) {
        pSpeechVoiceCustomLogger->Close();
    }
#endif

    struct mixer_ctl *ctl;
    enum mixer_ctl_type type;
    unsigned int num_values;

#if defined(MTK_USB_PHONECALL)
    if (!AudioUSBPhoneCallController::getInstance()->isEnable())
#endif
    {
        mHardwareResourceManager->stopOutputDevice();

        // Stop Side Tone Filter
        if (mIsSidetoneEnable) {
            mHardwareResourceManager->EnableSideToneFilter(false);
            mIsSidetoneEnable = false;
        }
    }
    // Stop MODEM_PCM
    if (mPcmIn != NULL) {
        pcm_stop(mPcmIn);
        pcm_close(mPcmIn);
        mPcmIn = NULL;
    }

    if (mPcmOut != NULL) {
        pcm_stop(mPcmOut);
        pcm_close(mPcmOut);
        mPcmOut = NULL;
    }

#if defined(MTK_USB_PHONECALL)
    if (!AudioUSBPhoneCallController::getInstance()->isEnable()) {
        mHardwareResourceManager->stopInputDevice(mPhonecallInputDevice);
        mPhonecallInputDevice = AUDIO_DEVICE_NONE;
    } else {
        AudioUSBPhoneCallController::getInstance()->disable();
    }
#else
    mHardwareResourceManager->stopInputDevice(mPhonecallInputDevice);
    mPhonecallInputDevice = AUDIO_DEVICE_NONE;
#endif
    mPhonecallOutputDevice = AUDIO_DEVICE_NONE;

    if (AudioSCPPhoneCallController::getInstance()->isEnable()) {
        AudioSCPPhoneCallController::getInstance()->disable();
    }

    // terminated shutter sound thread
    if (mMuteDlCodecForShutterSoundThreadEnable == true) {
        AL_LOCK(mMuteDlCodecForShutterSoundLock);
        mMuteDlCodecForShutterSoundThreadEnable = false;
        AL_SIGNAL(mMuteDlCodecForShutterSoundLock);
        AL_UNLOCK(mMuteDlCodecForShutterSoundLock);

        pthread_join(hMuteDlCodecForShutterSoundThread, NULL);
    }

    // terminated mute for routing thread
    if (mMuteDlUlForRoutingThreadEnable == true) {
        AL_LOCK(mMuteDlUlForRoutingLock);
        mMuteDlUlForRoutingThreadEnable = false;
        AL_SIGNAL(mMuteDlUlForRoutingLock);
        AL_UNLOCK(mMuteDlUlForRoutingLock);

        pthread_join(mMuteDlUlForRoutingThread, NULL);
    }

#if defined(MTK_RTT_SUPPORT)
    pSpeechDriver->RttConfig(AUD_RTT_OFF);
#endif

    // Speech/VT off
    if (pSpeechDriver->GetApSideModemStatus(VT_STATUS_MASK) == true) {
        pSpeechDriver->PCM2WayOff();
        pSpeechDriver->VideoTelephonyOff();
    } else if (pSpeechDriver->GetApSideModemStatus(SPEECH_STATUS_MASK) == true) {
        if (pSpeechDriver->GetApSideModemStatus(TTY_STATUS_MASK) == true) {
            pSpeechDriver->TtyCtmOff();

#if defined(MTK_AUDIO_HIERARCHICAL_PARAM_SUPPORT)
            char paramTtyInfo[50] = {0};
            snprintf(paramTtyInfo, sizeof(paramTtyInfo), "ParamSphTty=%d;", TTY_PARAM_OFF);
            SpeechParamParser::getInstance()->SetParamInfo(String8(paramTtyInfo));
#else
            pSpeechDriver->SetSpeechEnhancement(true);
            pSpeechDriver->SetSpeechEnhancementMask(SpeechEnhancementController::GetInstance()->GetSpeechEnhancementMask());
#endif
        }
        pSpeechDriver->SpeechOff();
    } else {
        ALOGE("%s(), mAudioMode = %d, Speech & VT are already closed!!", __FUNCTION__, mAudioMode);
        ASSERT(pSpeechDriver->GetApSideModemStatus(VT_STATUS_MASK)     == true ||
               pSpeechDriver->GetApSideModemStatus(SPEECH_STATUS_MASK) == true);
    }

    // clean VT status
    if (mVtNeedOn == true) {
        ALOGD("%s(), Set mVtNeedOn = false", __FUNCTION__);
        mVtNeedOn = false;
    }
    pSpeechDriver->SetUplinkMute(mMicMute);

    char isC2kSupported[PROPERTY_VALUE_MAX];
    property_get("ro.boot.opt_c2k_support", isC2kSupported, "0"); //"0": default not support

    if (strcmp(isC2kSupported, "1") != 0) {
        // wake lock for external modem
        if (mIdxMD == MODEM_EXTERNAL) {
            int ret = release_wake_lock(WAKELOCK_NAME);
        }
    }

    if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Speech_MD_USAGE"), "Off")) {
        ALOGE("Error: Speech_MD_USAGE invalid value");
    }

    mAudioMode = AUDIO_MODE_NORMAL; // TODO(Harvey): default value? VoIP?

    return NO_ERROR;
}


status_t AudioALSASpeechPhoneCallController::routing(const audio_devices_t new_output_devices, const audio_devices_t new_input_device) {
    AL_AUTOLOCK(mLock);
    AL_AUTOLOCK(*AudioALSADriverUtility::getInstance()->getStreamSramDramLock());

    ALOGD("%s(+), mAudioMode: %d, mIdxMD: %d, new_output_devices: 0x%x, new_input_device: 0x%x",
          __FUNCTION__, mAudioMode, mIdxMD, new_output_devices, new_input_device);

    const modem_index_t modem_index = mSpeechDriverFactory->GetActiveModemIndex();
    const audio_devices_t old_output_device =  mHardwareResourceManager->getOutputDevice();

    // Get current active speech driver
    SpeechDriverInterface *pSpeechDriver = mSpeechDriverFactory->GetSpeechDriver();

    int PcmInIdx = 0;
    int PcmOutIdx = 0;
    int CardIndex = 0;

    // Mute during device change.
    muteDlUlForRouting(SPH_MUTE_CTRL_ROUTING_START);

#if defined(MTK_USB_PHONECALL)
    if (!AudioUSBPhoneCallController::getInstance()->isEnable())
#endif
    {
        // Stop PMIC digital/analog part - downlink
        mHardwareResourceManager->stopOutputDevice();

        // Stop Side Tone Filter
        if (mIsSidetoneEnable) {
            mHardwareResourceManager->EnableSideToneFilter(false);
            mIsSidetoneEnable = false;
        }

        // Stop MODEM_PCM
        //mAudioDigitalInstance->SetModemPcmEnable(modem_index, false);

        // Stop PMIC digital/analog part - uplink
        mHardwareResourceManager->stopInputDevice(mPhonecallInputDevice);
        mPhonecallInputDevice = AUDIO_DEVICE_NONE;

    }

    // Stop AP side digital part
    if (pSpeechDriver->GetApSideModemStatus(TTY_STATUS_MASK) == true) {
        pSpeechDriver->TtyCtmOff();
#if defined(MTK_AUDIO_HIERARCHICAL_PARAM_SUPPORT)
        char paramTtyInfo[50] = {0};
        snprintf(paramTtyInfo, sizeof(paramTtyInfo), "ParamSphTty=%d;", TTY_PARAM_OFF);
        SpeechParamParser::getInstance()->SetParamInfo(String8(paramTtyInfo));
#endif
    }

    // Get new device
    const audio_devices_t output_device = new_output_devices; //(audio_devices_t)mAudioResourceManager->getDlOutputDevice();
    const audio_devices_t input_device  = new_input_device; //(audio_devices_t)mAudioResourceManager->getUlInputDevice();

    mPhonecallOutputDevice = new_output_devices;


    // Check BT device
    const bool bt_device_on = audio_is_bluetooth_sco_device(new_output_devices);
    uint32_t sample_rate = 0;
    if (mSpeechDVT_SampleRate != 0) {
        sample_rate = mSpeechDVT_SampleRate;
        ALOGD("%s(), SpeechDVT sample_rate = %d", __FUNCTION__, sample_rate);
    } else {
        sample_rate = calculateSampleRate(bt_device_on);
    }

    //close previous device
    if (mPcmIn != NULL) {
        pcm_stop(mPcmIn);
        pcm_close(mPcmIn);
        mPcmIn = NULL;
    }

    if (mPcmOut != NULL) {
        pcm_stop(mPcmOut);
        pcm_close(mPcmOut);
        mPcmOut = NULL;
    }

#if defined(MTK_USB_PHONECALL)
    if (AudioUSBPhoneCallController::getInstance()->isEnable()) {
        AudioUSBPhoneCallController::getInstance()->disable();
    }
#endif
    if (AudioSCPPhoneCallController::getInstance()->isEnable()) {
        AudioSCPPhoneCallController::getInstance()->disable();
    }
    if (bt_device_on) {
        if (WCNChipController::GetInstance()->IsBTMergeInterfaceSupported() == true) {
            //open bt sco device
            memset(&mConfig, 0, sizeof(mConfig));

            mConfig.channels = 1;
            mConfig.rate = sample_rate;
            mConfig.period_size = 4096;
            mConfig.period_count = 2;
            mConfig.format = PCM_FORMAT_S16_LE;
            mConfig.start_threshold = 0;
            mConfig.stop_threshold = 0;
            mConfig.silence_threshold = 0;

            ALOGE_IF(mPcmOut != NULL, "%s(), mPcmOut = %p", __FUNCTION__, mPcmOut);
            ASSERT(mPcmOut == NULL);

            if (mIdxMD == MODEM_EXTERNAL || mIdxMD == MODEM_2) {
                PcmOutIdx = AudioALSADeviceParser::getInstance()->GetPcmIndexByString(keypcmVoiceMD2BT);
                CardIndex = AudioALSADeviceParser::getInstance()->GetCardIndexByString(keypcmVoiceMD2BT);
                mPcmOut = pcm_open(CardIndex, PcmOutIdx, PCM_OUT, &mConfig);
            } else {
                PcmOutIdx = AudioALSADeviceParser::getInstance()->GetPcmIndexByString(keypcmVoiceMD1BT);
                CardIndex = AudioALSADeviceParser::getInstance()->GetCardIndexByString(keypcmVoiceMD1BT);
                mPcmOut = pcm_open(CardIndex, PcmOutIdx, PCM_OUT, &mConfig);
            }
            ALOGD_IF(mPcmOut == NULL, "%s(), mPcmOut = %p, PcmOutIdx = %d, CardIndex = %d", __FUNCTION__, mPcmOut, PcmOutIdx, CardIndex);
            ASSERT(mPcmOut != NULL);

            pcm_start(mPcmOut);
        }
#if defined(MTK_USB_PHONECALL)
    } else if (AudioUSBPhoneCallController::getInstance()->isForceUSBCall() ||
               output_device == AUDIO_DEVICE_OUT_USB_DEVICE) {
        AudioUSBPhoneCallController::getInstance()->enable(sample_rate);
#endif
    } else {
#ifdef MTK_VOICE_ULTRA
        if (output_device == AUDIO_DEVICE_OUT_EARPIECE) {
            PcmInIdx = AudioALSADeviceParser::getInstance()->GetPcmIndexByString(keypcmVoiceUltra);
            PcmOutIdx = AudioALSADeviceParser::getInstance()->GetPcmIndexByString(keypcmVoiceUltra);
            CardIndex = AudioALSADeviceParser::getInstance()->GetCardIndexByString(keypcmVoiceUltra);
            String8 mixer_ctl_name;
            if (mIdxMD == MODEM_EXTERNAL || mIdxMD == MODEM_2) {
                mixer_ctl_name = "md2";
            } else {
                mixer_ctl_name = "md1";
            }

            if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Ultra_Modem_Select"), mixer_ctl_name.string())) {
                ALOGE("Error: Ultra_Modem_Select invalid value");
            }

            ALOGE_IF(mPcmIn != NULL, "%s(), mPcmIn = %p", __FUNCTION__, mPcmIn);
            ALOGE_IF(mPcmOut != NULL, "%s(), mPcmOut = %p", __FUNCTION__, mPcmOut);
            ASSERT(mPcmIn == NULL && mPcmOut == NULL);

            // use pcm out to set memif, ultrasound, downlink
            enum pcm_format memifFormat = PCM_FORMAT_S16_LE;    // or PCM_FORMAT_S32_LE
            unsigned int ultraRate = 96000;
            unsigned int msPerPeriod = 10;  // note: max sram is 48k

            memset(&mConfig, 0, sizeof(mConfig));
            mConfig.channels = 1;
            mConfig.rate = ultraRate;
            mConfig.period_size = (ultraRate * msPerPeriod) / 1000;
            mConfig.period_count = 2;
            mConfig.format = memifFormat;

            mPcmOut = pcm_open(CardIndex, PcmOutIdx, PCM_OUT, &mConfig);

            // use pcm in to set modem, uplink
            memset(&mConfig, 0, sizeof(mConfig));
            mConfig.channels = 2;
            mConfig.rate = sample_rate; // modem rate
            mConfig.period_size = 1024;
            mConfig.period_count = 2;
            mConfig.format = PCM_FORMAT_S16_LE;

            mPcmIn = pcm_open(CardIndex, PcmInIdx, PCM_IN, &mConfig);
        } else
#endif
        {
            if (mIdxMD == MODEM_EXTERNAL || mIdxMD == MODEM_2) {
                PcmInIdx = AudioALSADeviceParser::getInstance()->GetPcmIndexByString(keypcmVoiceMD2);
                PcmOutIdx = AudioALSADeviceParser::getInstance()->GetPcmIndexByString(keypcmVoiceMD2);
                CardIndex = AudioALSADeviceParser::getInstance()->GetCardIndexByString(keypcmVoiceMD2);
            } else {
                PcmInIdx = AudioALSADeviceParser::getInstance()->GetPcmIndexByString(keypcmVoiceMD1);
                PcmOutIdx = AudioALSADeviceParser::getInstance()->GetPcmIndexByString(keypcmVoiceMD1);
                CardIndex = AudioALSADeviceParser::getInstance()->GetCardIndexByString(keypcmVoiceMD1);
            }
            memset(&mConfig, 0, sizeof(mConfig));
            mConfig.channels = mHardwareResourceManager->getNumPhoneMicSupport();
            mConfig.rate = sample_rate;
            mConfig.period_size = 1024;
            mConfig.period_count = 2;
            mConfig.format = PCM_FORMAT_S16_LE;
            mConfig.start_threshold = 0;
            mConfig.stop_threshold = 0;
            mConfig.silence_threshold = 0;
            ALOGE_IF(mPcmIn != NULL, "%s(), mPcmIn = %p", __FUNCTION__, mPcmIn);
            ALOGE_IF(mPcmOut != NULL, "%s(), mPcmOut = %p", __FUNCTION__, mPcmOut);
            ASSERT(mPcmIn == NULL && mPcmOut == NULL);
            mPcmIn = pcm_open(CardIndex, PcmInIdx, PCM_IN, &mConfig);

            mConfig.channels = 2;
            mPcmOut = pcm_open(CardIndex, PcmOutIdx, PCM_OUT, &mConfig);
        }
        ALOGD_IF(mPcmIn == NULL, "%s(), mPcmIn = %p, PcmInIdx = %d, CardIndex = %d", __FUNCTION__, mPcmIn, PcmInIdx, CardIndex);
        ALOGD_IF(mPcmOut == NULL, "%s(), mPcmOut = %p, PcmOutIdx = %d, CardIndex = %d", __FUNCTION__, mPcmOut, PcmOutIdx, CardIndex);
        ASSERT(mPcmIn != NULL && mPcmOut != NULL);

        pcm_start(mPcmIn);
        pcm_start(mPcmOut);

        if (AudioSCPPhoneCallController::getInstance()->isSupportPhonecall(new_output_devices)) {
            AudioSCPPhoneCallController::getInstance()->enable(sample_rate);
        }
    }

    // Set new device
    if (checkTtyNeedOn() == true) {
        setTtyInOutDevice(getRoutingForTty());
#if !defined (MTK_AUDIO_HIERARCHICAL_PARAM_SUPPORT)
        setTtyEnhancement(mTtyMode, getRoutingForTty());
#endif
    } else {
        // Set PMIC digital/analog part - uplink has pop, open first
        mPhonecallInputDevice = input_device;

#if defined(MTK_USB_PHONECALL)
        if (!AudioUSBPhoneCallController::getInstance()->isEnable())
#endif
        {
            // Set PMIC digital/analog part - uplink has pop, open first
            mHardwareResourceManager->startInputDevice(mPhonecallInputDevice);

            // Set PMIC digital/analog part - DL need trim code.
            mHardwareResourceManager->startOutputDevice(output_device, sample_rate);
        }
    }

    // start Side Tone Filter
    if (checkSideToneFilterNeedOn(output_device) == true) {
        mHardwareResourceManager->EnableSideToneFilter(true);
        mIsSidetoneEnable = true;
    }

    // Set MD side sampling rate
    pSpeechDriver->SetModemSideSamplingRate(sample_rate);

    // Set speech mode
    if (checkTtyNeedOn() == false) {
        pSpeechDriver->SetSpeechMode(mPhonecallInputDevice, output_device);
    } else {
        pSpeechDriver->TtyCtmOn(mTtyMode);
    }

    // Need recover mute state, trigger to wait for timeout unmute
    muteDlUlForRouting(SPH_MUTE_CTRL_ROUTING_END);

    ALOGD("%s(-), mHardwareResourceManager output_devices: 0x%x, input_device: 0x%x"
          ", bt_device_on: %d, sample_rate: %u"
          ", CardIndex: %d, PcmInIdx: %d, PcmOutIdx: %d, mPcmIn: %p, mPcmOut: %p",
          __FUNCTION__,
          mHardwareResourceManager->getOutputDevice(),
          mHardwareResourceManager->getInputDevice(),
          bt_device_on, sample_rate,
          CardIndex, PcmInIdx, PcmOutIdx, mPcmIn, mPcmOut);

    return NO_ERROR;
}

audio_devices_t AudioALSASpeechPhoneCallController::getPhonecallInputDevice() {
    AL_AUTOLOCK(mLock);
    return mPhonecallInputDevice;
}

audio_devices_t AudioALSASpeechPhoneCallController::getPhonecallOutputDevice() {
    AL_AUTOLOCK(mLock);
    return mPhonecallOutputDevice;
}

status_t AudioALSASpeechPhoneCallController::setTtyMode(const tty_mode_t tty_mode) {
    ALOGD("+%s(), mTtyMode = %d, new tty mode = %d", __FUNCTION__, mTtyMode, tty_mode);

#if defined(MTK_TTY_SUPPORT)
    SpeechDriverInterface *pSpeechDriver = mSpeechDriverFactory->GetSpeechDriver();
    tty_mode_t preTtyMode = mTtyMode;
    if (mTtyMode != tty_mode) {
        mTtyMode = tty_mode;
    }
#if defined(MTK_RTT_SUPPORT)
    if (mRttCallType != RTT_CALL_TYPE_CS) {
        if ((pSpeechDriver->GetApSideModemStatus(SPEECH_STATUS_MASK) == true) &&
            (pSpeechDriver->GetApSideModemStatus(TTY_STATUS_MASK) == true)) {
            //force disable TTY during call
            pSpeechDriver->SetUplinkMute(true);
            pSpeechDriver->TtyCtmOff();
            audio_devices_t output_devices = getRoutingForTty();//routing to original non-tty output device
            routing(output_devices, mPhonecallInputDevice);
            mAudioALSAVolumeController->setVoiceVolume(mAudioALSAVolumeController->getVoiceVolume(),
                                                       AudioALSAStreamManager::getInstance()->getModeForGain(),
                                                       output_devices);
            pSpeechDriver->SetUplinkMute(mMicMute);
            ALOGD("-%s(), mRttCallType =%d, mTtyMode = %d, force TTY_OFF", __FUNCTION__, mRttCallType, mTtyMode);
        }
    } else if ((pSpeechDriver->GetApSideModemStatus(SPEECH_STATUS_MASK) == true) &&
               (mTtyMode != AUD_TTY_OFF) && (mTtyMode != AUD_TTY_ERR) &&
               (pSpeechDriver->GetApSideModemStatus(TTY_STATUS_MASK) == false)) {
        //recover TTY during call
        pSpeechDriver->SetUplinkMute(true);
        pSpeechDriver->TtyCtmOn(mTtyMode);
        audio_devices_t output_devices = getRoutingForTty();//routing to original non-tty output device
        routing(output_devices, mPhonecallInputDevice);
        mAudioALSAVolumeController->setVoiceVolume(mAudioALSAVolumeController->getVoiceVolume(),
                                                   AudioALSAStreamManager::getInstance()->getModeForGain(),
                                                   output_devices);
        pSpeechDriver->SetUplinkMute(mMicMute);
        ALOGD("-%s(), mRttCallType =%d, mTtyMode = %d, recover TTY from TTY_OFF", __FUNCTION__, mRttCallType, mTtyMode);
    } else
#endif
    {
        if (preTtyMode != tty_mode) {
            if (pSpeechDriver->GetApSideModemStatus(VT_STATUS_MASK) == false &&
                pSpeechDriver->GetApSideModemStatus(SPEECH_STATUS_MASK) == true) {
                pSpeechDriver->SetUplinkMute(true);
                if (pSpeechDriver->GetApSideModemStatus(TTY_STATUS_MASK) == true) {
                    pSpeechDriver->TtyCtmOff();
                }
                audio_devices_t output_devices = getRoutingForTty();//"NG:mHardwareResourceManager->getOutputDevice()->HCO->off use main mic
                routing(output_devices, mPhonecallInputDevice);

                if ((mTtyMode != AUD_TTY_OFF) && (mTtyMode != AUD_TTY_ERR) &&
                    (pSpeechDriver->GetApSideModemStatus(TTY_STATUS_MASK) == false)) {
                    pSpeechDriver->TtyCtmOn(mTtyMode);
#if defined(MTK_AUDIO_HIERARCHICAL_PARAM_SUPPORT)
                } else {
                    mAudioALSAVolumeController->setVoiceVolume(mAudioALSAVolumeController->getVoiceVolume(),
                                                               AudioALSAStreamManager::getInstance()->getModeForGain(),
                                                               output_devices);
                }
#else
                }
                setTtyEnhancement(mTtyMode, output_devices);
#endif
                pSpeechDriver->SetUplinkMute(mMicMute);
            }
        }
        ALOGD("-%s(), mTtyMode = %d", __FUNCTION__, mTtyMode);
    }
#endif
    return NO_ERROR;
}

void AudioALSASpeechPhoneCallController::setTtyInOutDevice(audio_devices_t routing_device) {
    ALOGV("+%s(), routing_device(out) = 0x%x, mTtyMode = %d", __FUNCTION__, routing_device, mTtyMode);

    audio_devices_t PhonecallOutputDevice = routing_device;
    SpeechDriverInterface *pSpeechDriver = mSpeechDriverFactory->GetSpeechDriver();
#ifdef MTK_TTY_SUPPORT
    char paramTtyInfo[50] = {0};
    if (mTtyMode == AUD_TTY_OFF) {
#if defined(MTK_AUDIO_HIERARCHICAL_PARAM_SUPPORT)
        snprintf(paramTtyInfo, sizeof(paramTtyInfo), "ParamSphTty=%d;", TTY_PARAM_OFF);
        SpeechParamParser::getInstance()->SetParamInfo(String8(paramTtyInfo));
#endif
        mPhonecallInputDevice = getInputDeviceForPhoneCall(PhonecallOutputDevice);
        mHardwareResourceManager->startOutputDevice(PhonecallOutputDevice, 16000);
        mHardwareResourceManager->startInputDevice(mPhonecallInputDevice);
        pSpeechDriver->SetSpeechMode(mPhonecallInputDevice, PhonecallOutputDevice);
    } else {
#if defined(MTK_AUDIO_HIERARCHICAL_PARAM_SUPPORT)
        if ((mTtyMode == AUD_TTY_FULL) || (mTtyMode == AUD_TTY_HCO)) {
            snprintf(paramTtyInfo, sizeof(paramTtyInfo), "ParamSphTty=%d;", TTY_PARAM_HCO);
            SpeechParamParser::getInstance()->SetParamInfo(String8(paramTtyInfo));
        } else if (mTtyMode == AUD_TTY_VCO) {
            snprintf(paramTtyInfo, sizeof(paramTtyInfo), "ParamSphTty=%d;", TTY_PARAM_VCO);
            SpeechParamParser::getInstance()->SetParamInfo(String8(paramTtyInfo));
        }
#endif
        if (routing_device == AUDIO_DEVICE_NONE) {
            PhonecallOutputDevice = AUDIO_DEVICE_OUT_DEFAULT;
            mPhonecallInputDevice = getInputDeviceForPhoneCall(PhonecallOutputDevice);
            mHardwareResourceManager->startOutputDevice(PhonecallOutputDevice, 16000);
            mHardwareResourceManager->startInputDevice(mPhonecallInputDevice);
#if defined(MTK_AUDIO_HIERARCHICAL_PARAM_SUPPORT)
            mAudioALSAVolumeController->ApplyMicGain(Normal_Mic, mAudioMode);
#endif
            pSpeechDriver->SetSpeechMode(mPhonecallInputDevice, PhonecallOutputDevice);
        } else if (routing_device & AUDIO_DEVICE_OUT_SPEAKER) {
            if (mTtyMode == AUD_TTY_VCO) {
                ALOGD("%s(), speaker, TTY_VCO", __FUNCTION__);
#if defined(ENABLE_EXT_DAC) || defined(ALL_USING_VOICEBUFFER_INCALL)
                PhonecallOutputDevice = AUDIO_DEVICE_OUT_EARPIECE;
#else
                PhonecallOutputDevice = AUDIO_DEVICE_OUT_WIRED_HEADSET;
#endif
                mPhonecallInputDevice = AUDIO_DEVICE_IN_BUILTIN_MIC;
                mHardwareResourceManager->startOutputDevice(PhonecallOutputDevice, 16000);
                mHardwareResourceManager->startInputDevice(mPhonecallInputDevice);
                mAudioALSAVolumeController->ApplyMicGain(Handfree_Mic, mAudioMode);
                pSpeechDriver->SetSpeechMode(mPhonecallInputDevice, AUDIO_DEVICE_OUT_SPEAKER);
            } else if (mTtyMode == AUD_TTY_HCO) {
                ALOGD("%s(), speaker, TTY_HCO", __FUNCTION__);
#if defined(ENABLE_EXT_DAC) || defined(ALL_USING_VOICEBUFFER_INCALL)
                PhonecallOutputDevice = AUDIO_DEVICE_OUT_EARPIECE;
#else
                PhonecallOutputDevice = AUDIO_DEVICE_OUT_SPEAKER;
#endif
                mPhonecallInputDevice = AUDIO_DEVICE_IN_WIRED_HEADSET;
                mHardwareResourceManager->startOutputDevice(PhonecallOutputDevice, 16000);
                mHardwareResourceManager->startInputDevice(mPhonecallInputDevice);
                mAudioALSAVolumeController->ApplyMicGain(TTY_CTM_Mic, mAudioMode);
                pSpeechDriver->SetSpeechMode(mPhonecallInputDevice, AUDIO_DEVICE_OUT_SPEAKER);
            } else if (mTtyMode == AUD_TTY_FULL) {
                ALOGD("%s(), speaker, TTY_FULL", __FUNCTION__);
#if defined(ENABLE_EXT_DAC) || defined(ALL_USING_VOICEBUFFER_INCALL)
                PhonecallOutputDevice = AUDIO_DEVICE_OUT_EARPIECE;
#else
                PhonecallOutputDevice = AUDIO_DEVICE_OUT_WIRED_HEADSET;
#endif
                mPhonecallInputDevice = AUDIO_DEVICE_IN_WIRED_HEADSET;
                mHardwareResourceManager->startOutputDevice(PhonecallOutputDevice, 16000);
                mHardwareResourceManager->startInputDevice(mPhonecallInputDevice);
                mAudioALSAVolumeController->ApplyMicGain(TTY_CTM_Mic, mAudioMode);
                pSpeechDriver->SetSpeechMode(mPhonecallInputDevice, AUDIO_DEVICE_OUT_WIRED_HEADSET);
            }
        } else if ((routing_device == AUDIO_DEVICE_OUT_WIRED_HEADSET) ||
                   (routing_device == AUDIO_DEVICE_OUT_WIRED_HEADPHONE)) {
            if (mTtyMode == AUD_TTY_VCO) {
                ALOGD("%s(), headset, TTY_VCO", __FUNCTION__);
#if defined(ENABLE_EXT_DAC) || defined(ALL_USING_VOICEBUFFER_INCALL)
                PhonecallOutputDevice = AUDIO_DEVICE_OUT_EARPIECE;
#else
                PhonecallOutputDevice = AUDIO_DEVICE_OUT_WIRED_HEADSET;
#endif
                mPhonecallInputDevice = AUDIO_DEVICE_IN_BUILTIN_MIC;
                mHardwareResourceManager->startOutputDevice(PhonecallOutputDevice, 16000);
                mHardwareResourceManager->startInputDevice(mPhonecallInputDevice);
                mAudioALSAVolumeController->ApplyMicGain(Normal_Mic, mAudioMode);
                pSpeechDriver->SetSpeechMode(mPhonecallInputDevice, AUDIO_DEVICE_OUT_EARPIECE);
            } else if (mTtyMode == AUD_TTY_HCO) {
                ALOGD("%s(), headset, TTY_HCO", __FUNCTION__);
                PhonecallOutputDevice = AUDIO_DEVICE_OUT_EARPIECE;
                mPhonecallInputDevice = AUDIO_DEVICE_IN_WIRED_HEADSET;
                mHardwareResourceManager->startOutputDevice(PhonecallOutputDevice, 16000);
                mHardwareResourceManager->startInputDevice(mPhonecallInputDevice);
                mAudioALSAVolumeController->ApplyMicGain(TTY_CTM_Mic, mAudioMode);
                pSpeechDriver->SetSpeechMode(mPhonecallInputDevice, PhonecallOutputDevice);
            } else if (mTtyMode == AUD_TTY_FULL) {
                ALOGD("%s(), headset, TTY_FULL", __FUNCTION__);
#if defined(ENABLE_EXT_DAC) || defined(ALL_USING_VOICEBUFFER_INCALL)
                PhonecallOutputDevice = AUDIO_DEVICE_OUT_EARPIECE;
#else
                PhonecallOutputDevice = AUDIO_DEVICE_OUT_WIRED_HEADSET;
#endif
                mPhonecallInputDevice = AUDIO_DEVICE_IN_WIRED_HEADSET;
                mHardwareResourceManager->startOutputDevice(PhonecallOutputDevice, 16000);
                mHardwareResourceManager->startInputDevice(mPhonecallInputDevice);
                mAudioALSAVolumeController->ApplyMicGain(TTY_CTM_Mic, mAudioMode);
                pSpeechDriver->SetSpeechMode(mPhonecallInputDevice, AUDIO_DEVICE_OUT_WIRED_HEADSET);
            }
        } else if (routing_device == AUDIO_DEVICE_OUT_EARPIECE) {
            // tty device is removed. TtyCtm already off in CloseMD.
            mPhonecallInputDevice = getInputDeviceForPhoneCall(PhonecallOutputDevice);
            mHardwareResourceManager->startOutputDevice(PhonecallOutputDevice, 16000);
            mHardwareResourceManager->startInputDevice(mPhonecallInputDevice);
#if defined(MTK_AUDIO_HIERARCHICAL_PARAM_SUPPORT)
            mAudioALSAVolumeController->ApplyMicGain(Normal_Mic, mAudioMode);
#endif
            pSpeechDriver->SetSpeechMode(mPhonecallInputDevice, PhonecallOutputDevice);
            ALOGD("%s(), receiver", __FUNCTION__);
        } else {
            mPhonecallInputDevice = getInputDeviceForPhoneCall(PhonecallOutputDevice);
            mHardwareResourceManager->startOutputDevice(PhonecallOutputDevice, 16000);
            mHardwareResourceManager->startInputDevice(mPhonecallInputDevice);
#if defined(MTK_AUDIO_HIERARCHICAL_PARAM_SUPPORT)
            mAudioALSAVolumeController->ApplyMicGain(Normal_Mic, mAudioMode);
#endif
            pSpeechDriver->SetSpeechMode(mPhonecallInputDevice, PhonecallOutputDevice);
        }
#if !defined(MTK_AUDIO_HIERARCHICAL_PARAM_SUPPORT)
        mAudioALSAVolumeController->setVoiceVolume(mAudioALSAVolumeController->getVoiceVolume(),
                                                   AudioALSAStreamManager::getInstance()->getModeForGain(),
                                                   PhonecallOutputDevice);
#endif
    }
#endif
    ALOGD("-%s(), mTtyMode(%d), routing_device(out) = 0x%x, mPhonecallInputDevice(0x%x), PhonecallOutputDevice(0x%x)", __FUNCTION__, mTtyMode, routing_device, mPhonecallInputDevice, PhonecallOutputDevice);

}

int AudioALSASpeechPhoneCallController::setRttCallType(const int rttCallType) {
#if defined(MTK_RTT_SUPPORT)
    ALOGD("+%s(), mRttCallType = %d, new rttCallType = %d", __FUNCTION__, mRttCallType, rttCallType);
    if (mRttCallType != rttCallType) {
        SpeechDriverInterface *pSpeechDriver = mSpeechDriverFactory->GetSpeechDriver();
        switch (rttCallType) {
        case RTT_CALL_TYPE_RTT:
            mRttMode = AUD_RTT_ON;
            mRttCallType = rttCallType;
            setTtyMode((const tty_mode_t) mTtyMode);
            if (pSpeechDriver->GetApSideModemStatus(SPEECH_STATUS_MASK)) {
                pSpeechDriver->RttConfig(mRttMode);
            }
            break;

        case RTT_CALL_TYPE_CS:
        case RTT_CALL_TYPE_PS:
        case RTT_CALL_TYPE_CS_NO_TTY:
            mRttMode = AUD_RTT_OFF;
            mRttCallType = rttCallType;
            setTtyMode((const tty_mode_t) mTtyMode);
            if (pSpeechDriver->GetApSideModemStatus(SPEECH_STATUS_MASK)) {
                pSpeechDriver->RttConfig(mRttMode);
            }
            break;

        default:
            ALOGE("%s(): Invalid rttCallType(%d)", __FUNCTION__, rttCallType);
            break;
        }
    }
    ALOGD("-%s(): mRttCallType = %d, mRttMode = %d", __FUNCTION__, mRttCallType, mRttMode);
    return NO_ERROR;
#else
    ALOGW("%s(), rttCallType = %d, NOT Supported!", __FUNCTION__, rttCallType);
    return INVALID_OPERATION;
#endif

}

void AudioALSASpeechPhoneCallController::setVtNeedOn(const bool vt_on) {
    ALOGD("%s(), new vt_on = %d, old mVtNeedOn = %d", __FUNCTION__, vt_on, mVtNeedOn);
    AL_AUTOLOCK(mLock);

    mVtNeedOn = vt_on;
}

void AudioALSASpeechPhoneCallController::setMicMute(const bool mute_on) {
    ALOGD("%s(), mMicMute: %d => %d", __FUNCTION__, mMicMute, mute_on);
    AL_AUTOLOCK(mLock);

    mSpeechDriverFactory->GetSpeechDriver()->SetUplinkMute(mute_on);

    property_set(PROPERTY_KEY_MIC_MUTE_ON, (mute_on == false) ? "0" : "1");
    mMicMute = mute_on;
}

void AudioALSASpeechPhoneCallController::setDlMute(const bool mute_on) {
    ALOGD("%s(), mDlMute: %d => %d", __FUNCTION__, mDlMute, mute_on);
    AL_AUTOLOCK(mLock);

    mSpeechDriverFactory->GetSpeechDriver()->SetDownlinkMute(mute_on);

    property_set(PROPERTY_KEY_DL_MUTE_ON, (mute_on == false) ? "0" : "1");
    mDlMute = mute_on;
}

void AudioALSASpeechPhoneCallController::setUlMute(const bool mute_on) {
    ALOGD("%s(), mUlMute: %d => %d", __FUNCTION__, mUlMute, mute_on);
    AL_AUTOLOCK(mLock);

    mSpeechDriverFactory->GetSpeechDriver()->SetUplinkSourceMute(mute_on);

    property_set(PROPERTY_KEY_UL_MUTE_ON, (mute_on == false) ? "0" : "1");
    mUlMute = mute_on;
}

void AudioALSASpeechPhoneCallController::setBTMode(const int mode) {
    ALOGD("%s(), mBTMode: %d => %d", __FUNCTION__, mBTMode, mode);
    AL_AUTOLOCK(mLock);
    mBTMode = mode;
}

void AudioALSASpeechPhoneCallController::getRFInfo() {
    WARNING("Not implement yet!!");
}

#ifdef SPEECH_PMIC_RESET
//call by speech driver
bool AudioALSASpeechPhoneCallController::StartPMIC_Reset() {
    status_t retval = NO_ERROR;
    ALOGD("%s()", __FUNCTION__);
    // create reading thread
    if (mEnable_PMIC_Reset != true) {
        //UlData: I2S0->
        mEnable_PMIC_Reset = true;

        retval = AL_LOCK_MS(mThreadLock, 200);
        if (retval != 0) {
            ALOGE("%s() create thread fail!!lock timeout 180ms", __FUNCTION__);
            return UNKNOWN_ERROR;
        }

        int ret = pthread_create(&hThread_PMIC_Reset, NULL, AudioALSASpeechPhoneCallController::thread_PMIC_Reset, (void *)this);
        if (ret != 0) {
            ALOGE("%s() create thread fail!!", __FUNCTION__);
            return UNKNOWN_ERROR;
        }
    }
    return true;
}

void *AudioALSASpeechPhoneCallController::thread_PMIC_Reset(void *arg) {
    prctl(PR_SET_NAME, (unsigned long)__FUNCTION__, 0, 0, 0);
    AudioALSASpeechPhoneCallController *pAudioALSASpeechPhoneCallController = (AudioALSASpeechPhoneCallController *)arg;
#if 1
    // Adjust thread priority
    int rtnPrio = setpriority(PRIO_PROCESS, 0, ANDROID_PRIORITY_AUDIO);
    if (0 != rtnPrio) {
        ALOGE("[%s] failed, errno: %d, return=%d", __FUNCTION__, errno, rtnPrio);
    } else {
        ALOGD("%s setpriority ok, priority: %d", __FUNCTION__, ANDROID_PRIORITY_AUDIO);
    }
#else

    // force to set priority
    struct sched_param sched_p;
    sched_getparam(0, &sched_p);
    sched_p.sched_priority = RTPM_PRIO_AUDIO_RECORD + 1;
    if (0 != sched_setscheduler(0, SCHED_RR, &sched_p)) {
        ALOGE("[%s] failed, errno: %d", __FUNCTION__, errno);
    } else {
        sched_p.sched_priority = RTPM_PRIO_AUDIO_CCCI_THREAD;
        sched_getparam(0, &sched_p);
        ALOGD("sched_setscheduler ok, priority: %d", sched_p.sched_priority);
    }
#endif

    ALOGD("+%s(), lock pid: %d, tid: %d", __FUNCTION__, getpid(), gettid());
    if (pAudioALSASpeechPhoneCallController->mEnable_PMIC_Reset == true) {
        /*
        if (pAudioALSASpeechPhoneCallController->mEnable_PMIC_Reset == false)
        {
            break;
        }*/
        usleep((1000) * 1000); //1s
        ALOGD("%s(), wait 1s....", __FUNCTION__);
#if 1
        struct mixer_ctl *ctl;
        ALOGE("%s(), PMIC_Reset !!!  PMIC_REG_CLEAR", __FUNCTION__);
        ctl = mixer_get_ctl_by_name(mMixer, "PMIC_REG_CLEAR"); //workaround
        mixer_ctl_set_enum_by_string(ctl, "On");
#endif

    }
    pAudioALSASpeechPhoneCallController->mEnable_PMIC_Reset = false;

    AL_UNLOCK(pAudioALSASpeechPhoneCallController->mThreadLock);
    ALOGD("-%s(), unlock pid: %d, tid: %d", __FUNCTION__, getpid(), gettid());
    pthread_exit(NULL);
    return NULL;
}
#endif


modem_index_t AudioALSASpeechPhoneCallController::updatePhysicalModemIdx(const audio_mode_t audio_mode) {
#if defined(MTK_COMBO_MODEM_SUPPORT)
    (void)audio_mode;
    return MODEM_1;
#else
    modem_index_t modem_index = MODEM_1;

    if (mSpeechDVT_MD_IDX == 0) {
        modem_index = mIdxMDByPhoneId[mPhoneId];
        ALOGD("%s(), audio_mode(%d), mPhoneId(%d), modem_index=%d", __FUNCTION__, audio_mode, mPhoneId, modem_index);
    } else {
        switch (mSpeechDVT_MD_IDX) {
        case 1: {
            modem_index = MODEM_1;
            break;
        }
        case 2: {
            modem_index = MODEM_EXTERNAL;
            break;
        }
        default: {
            modem_index = MODEM_1;
            break;
        }
        }
        ALOGD("%s(), SpeechDVT_MD_IDX = %d, modem_index=%d", __FUNCTION__, mSpeechDVT_MD_IDX, modem_index);
    }
    return modem_index;
#endif
}

int AudioALSASpeechPhoneCallController::setPhoneId(const phone_id_t phoneId) {
#if !defined(MTK_COMBO_MODEM_SUPPORT)
    ALOGD("+%s(), mPhoneId = %d, new phoneId = %d", __FUNCTION__, mPhoneId, phoneId);
#endif
    if (phoneId != mPhoneId) {
        if (phoneId == PHONE_ID_0 || phoneId == PHONE_ID_1) {
            mPhoneId = phoneId;
            char property_value[PROPERTY_VALUE_MAX] = {0};
            snprintf(property_value, sizeof(property_value), "%d", mPhoneId);
            property_set(PROPERTY_KEY_FOREGROUND_PHONE_ID, property_value);
#if !defined(MTK_COMBO_MODEM_SUPPORT)
            ALOGD("-%s(), mPhoneId = %d", __FUNCTION__, mPhoneId);
#endif
        } else {
            ALOGW("-%s(), Invalid %d. return. mPhoneId = %d", __FUNCTION__, phoneId, mPhoneId);
        }
    }
    return NO_ERROR;
}

/**
 * check if Phone Call need reopen according to RIL mapped modem
 */
bool AudioALSASpeechPhoneCallController::checkReopen(const modem_index_t rilMappedMDIdx) {
    bool needReopen = false;
    bool isSpeechOpen = mSpeechDriverFactory->GetSpeechDriver()->GetApSideModemStatus(SPEECH_STATUS_MASK);
    modem_index_t activeMDIdx = mSpeechDriverFactory->GetActiveModemIndex();
    if (isSpeechOpen) {
        //check modem index
        if (activeMDIdx != rilMappedMDIdx) {
            needReopen = true;
        }
    }
    ALOGD("%s(), needReopen(%d), MDIdx(%d->%d), isSpeechOpen(%d)",
          __FUNCTION__, needReopen, activeMDIdx, rilMappedMDIdx, isSpeechOpen);
    return needReopen;
}

status_t AudioALSASpeechPhoneCallController::setParam(const String8 &keyParamPairs) {
    ALOGD("+%s(): %s", __FUNCTION__, keyParamPairs.string());
    AudioParameter param = AudioParameter(keyParamPairs);
    int value;
    String8 ValueParam;
    char property_value[PROPERTY_VALUE_MAX] = {0};

    if (param.getInt(String8("AudioTaste"), value) == NO_ERROR) {
        param.remove(String8("AudioTaste"));
        bAudioTaste = (value == 1) ? true : false;

        ALOGD("%s(): bAudioTaste = %d", __FUNCTION__, bAudioTaste);
    } else if (param.getInt(String8("SpeechDVT_SampleRate"), value) == NO_ERROR) {
        param.remove(String8("SpeechDVT_SampleRate"));
        mSpeechDVT_SampleRate = value;

        ALOGD("%s(): mSpeechDVT_SampleRate = %d", __FUNCTION__, mSpeechDVT_SampleRate);
    } else if (param.getInt(String8("SpeechDVT_MD_IDX"), value) == NO_ERROR) {
        param.remove(String8("SpeechDVT_MD_IDX"));
        mSpeechDVT_MD_IDX = value;

        ALOGD("%s(): mSpeechDVT_MD_IDX = %d", __FUNCTION__, mSpeechDVT_MD_IDX);
    } else if (param.get(String8("Phone1Modem"), ValueParam) == NO_ERROR) {
        param.remove(String8("Phone1Modem"));
        if (ValueParam.string() != NULL) {
            if (strcmp(ValueParam.string(), "MD1") == 0) {
                mIdxMDByPhoneId[0] = MODEM_1;
            } else if (strcmp(ValueParam.string(), "MD3") == 0) {
                mIdxMDByPhoneId[0] = MODEM_EXTERNAL;
            } else {
                ALOGW("%s(), %s, Invalid MD Index. return", __FUNCTION__, ValueParam.string());
            }
            snprintf(property_value, sizeof(property_value), "%d", mIdxMDByPhoneId[0]);
            property_set(PROPERTY_KEY_PHONE1_MD, property_value);
        }
    } else if (param.get(String8("Phone2Modem"), ValueParam) == NO_ERROR) {
        param.remove(String8("Phone2Modem"));
        if (ValueParam.string() != NULL) {
            if (strcmp(ValueParam.string(), "MD1") == 0) {
                mIdxMDByPhoneId[1] = MODEM_1;
            } else if (strcmp(ValueParam.string(), "MD3") == 0) {
                mIdxMDByPhoneId[1] = MODEM_EXTERNAL;
            } else {
                ALOGW("%s(), %s, Invalid MD Index. return", __FUNCTION__, ValueParam.string());
            }
            snprintf(property_value, sizeof(property_value), "%d", mIdxMDByPhoneId[1]);
            property_set(PROPERTY_KEY_PHONE2_MD, property_value);
        }
    }
    ALOGD("-%s(): %s", __FUNCTION__, keyParamPairs.string());
    return NO_ERROR;
}

#if !defined(MTK_AUDIO_HIERARCHICAL_PARAM_SUPPORT)
void setTtyEnhancement(tty_mode_t tty_mode, audio_devices_t routing_device) {
    ALOGD("+%s, tty_mode=%d, routing=%d", __FUNCTION__, tty_mode, routing_device);
    SpeechDriverInterface *pSpeechDriver = SpeechDriverFactory::GetInstance()->GetSpeechDriver();
    sph_enh_mask_struct_t sphMask;
    if (tty_mode == AUD_TTY_VCO) {
        if (routing_device == AUDIO_DEVICE_OUT_SPEAKER) {
            //handfree mic
            sphMask.main_func = SPH_ENH_MAIN_MASK_AEC |
                                SPH_ENH_MAIN_MASK_EES |
                                SPH_ENH_MAIN_MASK_ULNR |
                                SPH_ENH_MAIN_MASK_TDNC |
                                SPH_ENH_MAIN_MASK_DMNR |
                                SPH_ENH_MAIN_MASK_AGC;
            sphMask.dynamic_func = (SPH_ENH_DYNAMIC_MASK_ALL  & (~SPH_ENH_DYNAMIC_MASK_SIDEKEY_DGAIN));
            pSpeechDriver->SetSpeechMode(AUDIO_DEVICE_IN_DEFAULT, AUDIO_DEVICE_OUT_SPEAKER);
        } else if ((routing_device == AUDIO_DEVICE_OUT_WIRED_HEADSET) ||
                   (routing_device == AUDIO_DEVICE_OUT_WIRED_HEADPHONE)) {
            //handset mic
            sphMask.main_func = SPH_ENH_MAIN_MASK_AEC |
                                SPH_ENH_MAIN_MASK_EES |
                                SPH_ENH_MAIN_MASK_ULNR |
                                SPH_ENH_MAIN_MASK_TDNC |
                                SPH_ENH_MAIN_MASK_DMNR |
                                SPH_ENH_MAIN_MASK_AGC;
            sphMask.dynamic_func = (SPH_ENH_DYNAMIC_MASK_ALL  & (~SPH_ENH_DYNAMIC_MASK_SIDEKEY_DGAIN));
            pSpeechDriver->SetSpeechMode(AUDIO_DEVICE_IN_DEFAULT, AUDIO_DEVICE_OUT_EARPIECE);
        }
    } else if (tty_mode == AUD_TTY_HCO) {
        if (routing_device == AUDIO_DEVICE_OUT_SPEAKER) {
            // handfree speaker
            sphMask.main_func = SPH_ENH_MAIN_MASK_DLNR;
            sphMask.dynamic_func = (SPH_ENH_DYNAMIC_MASK_ALL  & (~SPH_ENH_DYNAMIC_MASK_SIDEKEY_DGAIN));
            pSpeechDriver->SetSpeechMode(AUDIO_DEVICE_IN_DEFAULT, AUDIO_DEVICE_OUT_SPEAKER);
        } else if ((routing_device == AUDIO_DEVICE_OUT_WIRED_HEADSET) ||
                   (routing_device == AUDIO_DEVICE_OUT_WIRED_HEADPHONE)) {
            // handset receiver
            sphMask.main_func = SPH_ENH_MAIN_MASK_DLNR;
            sphMask.dynamic_func = (SPH_ENH_DYNAMIC_MASK_ALL  & (~SPH_ENH_DYNAMIC_MASK_SIDEKEY_DGAIN));
            pSpeechDriver->SetSpeechMode(AUDIO_DEVICE_IN_DEFAULT, AUDIO_DEVICE_OUT_EARPIECE);
        }
    } else if (tty_mode == AUD_TTY_FULL) {
        pSpeechDriver->SetSpeechEnhancement(false);
        ALOGD("-%s, disable speech enhancement", __FUNCTION__);
        return;
    } else if (tty_mode == AUD_TTY_OFF) {
        pSpeechDriver->SetSpeechEnhancement(true);
        pSpeechDriver->SetSpeechEnhancementMask(SpeechEnhancementController::GetInstance()->GetSpeechEnhancementMask());
        ALOGD("-%s, recover all speech enhancement", __FUNCTION__);
        return;
    }

    pSpeechDriver->SetSpeechEnhancement(true);
    pSpeechDriver->SetSpeechEnhancementMask(sphMask);
    ALOGD("-%s, main_func=0x%x, dynamic_func=0x%x", __FUNCTION__, sphMask.main_func, sphMask.dynamic_func);

}
#endif


void AudioALSASpeechPhoneCallController::muteDlCodecForShutterSound(const bool mute_on) {
    ALOGD("%s(), mMuteDlCodecForShutterSoundCount: %u, do mute_on: %d",
          __FUNCTION__, mMuteDlCodecForShutterSoundCount, mute_on);

    SpeechDriverInterface *pSpeechDriver = mSpeechDriverFactory->GetSpeechDriver();

    if (pSpeechDriver->GetApSideModemStatus(SPEECH_STATUS_MASK) == false &&
        pSpeechDriver->GetApSideModemStatus(VT_STATUS_MASK) == false) {
        ALOGW("%s(), speech off!! do nothing!!", __FUNCTION__);
        return;
    }

    AL_LOCK(mMuteDlCodecForShutterSoundLock);

    if (mute_on == true) {
        if (mMuteDlCodecForShutterSoundCount == 0) {
            if (mDownlinkMuteCodec == false) {
                pSpeechDriver->SetDownlinkMuteCodec(true);
                mDownlinkMuteCodec = true;
            } else {
                AL_SIGNAL(mMuteDlCodecForShutterSoundLock); // cancel wait & mute
            }
        }
        mMuteDlCodecForShutterSoundCount++;
    } else { // unmute
        if (mMuteDlCodecForShutterSoundCount == 0) {
            WARNING("BGS unmute DL Codec not in pair!!");
        } else {
            mMuteDlCodecForShutterSoundCount--;
            if (mMuteDlCodecForShutterSoundCount == 0) {
                AL_SIGNAL(mMuteDlCodecForShutterSoundLock); // notify to wait & mute
            }
        }
    }

    AL_UNLOCK(mMuteDlCodecForShutterSoundLock);

}


void *AudioALSASpeechPhoneCallController::muteDlCodecForShutterSoundThread(void *arg) {
    AudioALSASpeechPhoneCallController *call_controller = NULL;
    SpeechDriverInterface *pSpeechDriver = NULL;
    AudioLock *lock = NULL;

    char thread_name[128] = {0};
    int retval = 0;

    CONFIG_THREAD(thread_name, ANDROID_PRIORITY_AUDIO);

    call_controller = static_cast<AudioALSASpeechPhoneCallController *>(arg);
    if (call_controller == NULL) {
        ALOGE("%s(), call_controller is NULL!!", __FUNCTION__);
        goto MUTE_DL_CODEC_FOR_SHUTTER_SOUND_THREAD_DONE;
    }

    pSpeechDriver = call_controller->mSpeechDriverFactory->GetSpeechDriver();
    lock = &call_controller->mMuteDlCodecForShutterSoundLock;

    AL_LOCK(lock);

    while (call_controller->mMuteDlCodecForShutterSoundThreadEnable == true) {
        // sleep until signal comes
        AL_WAIT_NO_TIMEOUT(lock);

        // debug
        ALOGD("%s(), count: %u, mute: %d, start to wait & mute", __FUNCTION__,
              call_controller->mMuteDlCodecForShutterSoundCount,
              call_controller->mDownlinkMuteCodec);

        // wait and then unmute
        if (call_controller->mMuteDlCodecForShutterSoundCount == 0 &&
            call_controller->mDownlinkMuteCodec == true) {
            retval = AL_WAIT_MS(lock, DEFAULT_WAIT_SHUTTER_SOUND_UNMUTE_MS);
            if (call_controller->mMuteDlCodecForShutterSoundCount == 0 &&
                call_controller->mDownlinkMuteCodec == true) { // double check
                ALOGD("%s(), count: %u, mute: %d, do mute DL codec", __FUNCTION__,
                      call_controller->mMuteDlCodecForShutterSoundCount,
                      call_controller->mDownlinkMuteCodec);
                pSpeechDriver->SetDownlinkMuteCodec(false);
                call_controller->mDownlinkMuteCodec = false;
            } else {
                ALOGD("%s(), count: %u, mute: %d, mute canceled, retval: %d", __FUNCTION__,
                      call_controller->mMuteDlCodecForShutterSoundCount,
                      call_controller->mDownlinkMuteCodec, retval);
            }
        }

    }

    AL_UNLOCK(lock);

MUTE_DL_CODEC_FOR_SHUTTER_SOUND_THREAD_DONE:
    ALOGV("%s terminated", thread_name);
    pthread_exit(NULL);
    return NULL;
}

void AudioALSASpeechPhoneCallController::updateVolume() {
    muteDlUlForRouting(SPH_MUTE_CTRL_VOLUME_UPDATE);
}

void AudioALSASpeechPhoneCallController::muteDlUlForRouting(const int muteCtrl) {
    ALOGD_IF(mLogEnable, "%s(), do mute_ctrl: %d, mMuteDlUlForRoutingState: %d, routing output device = 0x%x",
          __FUNCTION__, muteCtrl, mMuteDlUlForRoutingState, mHardwareResourceManager->getOutputDevice());

    SpeechDriverInterface *pSpeechDriver = mSpeechDriverFactory->GetSpeechDriver();

    if (pSpeechDriver->GetApSideModemStatus(SPEECH_STATUS_MASK) == false) {
        ALOGW("%s(), speech off!! do nothing!!", __FUNCTION__);
        return;
    }

    AL_LOCK(mMuteDlUlForRoutingLock);
    mMuteDlUlForRoutingCtrl = muteCtrl;

    switch (mMuteDlUlForRoutingCtrl) {
    case SPH_MUTE_CTRL_ROUTING_START:
        if (mMuteDlUlForRoutingState != SPH_MUTE_THREAD_STATE_WAIT) {
            pSpeechDriver->SetDownlinkMute(true);
            pSpeechDriver->SetUplinkMute(true);
            pSpeechDriver->SetUplinkSourceMute(true); // avoid hw pop
            ALOGD_IF(mLogEnable, "%s(), mMuteDlUlForRoutingCtrl = %d, mMuteDlUlForRoutingState = %d, do mute only",
                  __FUNCTION__, mMuteDlUlForRoutingCtrl, mMuteDlUlForRoutingState);
        } else {
            ALOGD_IF(mLogEnable, "%s(), mMuteDlUlForRoutingCtrl = %d, mMuteDlUlForRoutingState = %d, do mute and stop waiting",
                  __FUNCTION__, mMuteDlUlForRoutingCtrl, mMuteDlUlForRoutingState);
            AL_SIGNAL(mMuteDlUlForRoutingLock);
        }
        break;

    case SPH_MUTE_CTRL_ROUTING_END:
        ALOGD_IF(mLogEnable, "%s(), mMuteDlUlForRoutingCtrl = %d, trigger thread, routing output device = 0x%x",
              __FUNCTION__, mMuteDlUlForRoutingCtrl, mHardwareResourceManager->getOutputDevice());
        AL_SIGNAL(mMuteDlUlForRoutingLock); // notify to wait & mute
        break;

    case SPH_MUTE_CTRL_VOLUME_UPDATE:
        if (mMuteDlUlForRoutingState == SPH_MUTE_THREAD_STATE_WAIT) {
            ALOGD_IF(mLogEnable, "%s(), mMuteDlUlForRoutingCtrl = %d, mMuteDlUlForRoutingState = %d, do unmute directly",
                  __FUNCTION__, mMuteDlUlForRoutingCtrl, mMuteDlUlForRoutingState);
            AL_SIGNAL(mMuteDlUlForRoutingLock); // notify to mute directly
        }
        break;

    case SPH_MUTE_CTRL_IDLE:
    default:
        ALOGD_IF(mLogEnable, "%s(), mMuteDlUlForRoutingCtrl = %d, mMuteDlUlForRoutingState = %d",
              __FUNCTION__, mMuteDlUlForRoutingCtrl, mMuteDlUlForRoutingState);
        break;
    }

    AL_UNLOCK(mMuteDlUlForRoutingLock);
}

void *AudioALSASpeechPhoneCallController::muteDlUlForRoutingThread(void *arg) {
    AudioALSASpeechPhoneCallController *call_controller = NULL;
    SpeechDriverInterface *pSpeechDriver = NULL;
    AudioLock *lock = NULL;

    char thread_name[128] = {0};
    int retvalWait = 0;

    CONFIG_THREAD(thread_name, ANDROID_PRIORITY_AUDIO);

    call_controller = static_cast<AudioALSASpeechPhoneCallController *>(arg);
    if (call_controller == NULL) {
        ALOGE("%s(), call_controller is NULL!!", __FUNCTION__);
        goto MUTE_DL_UL_FOR_ROUTING_THREAD_DONE;
    }

    pSpeechDriver = call_controller->mSpeechDriverFactory->GetSpeechDriver();
    lock = &call_controller->mMuteDlUlForRoutingLock;
    call_controller->mMuteDlUlForRoutingState = SPH_MUTE_THREAD_STATE_IDLE;

    AL_LOCK(lock);

    while (call_controller->mMuteDlUlForRoutingThreadEnable == true) {
        // sleep until signal comes
        AL_WAIT_NO_TIMEOUT(lock);

        // debug
        ALOGD_IF(call_controller->mLogEnable, "%s(), Ctrl: %d, State: %d, start to wait & mute", __FUNCTION__,
              call_controller->mMuteDlUlForRoutingCtrl,
              call_controller->mMuteDlUlForRoutingState);

        // wait and then recover to current mute status
        if (call_controller->mMuteDlUlForRoutingCtrl == SPH_MUTE_CTRL_ROUTING_END) {
            call_controller->mMuteDlUlForRoutingState = SPH_MUTE_THREAD_STATE_WAIT;
            retvalWait = AL_WAIT_MS(lock, DEFAULT_WAIT_ROUTING_UNMUTE_MS);
            call_controller->mMuteDlUlForRoutingState = SPH_MUTE_THREAD_STATE_IDLE;

            if (retvalWait == -ETIMEDOUT) { //time out, do unmute
                pSpeechDriver->SetUplinkSourceMute(call_controller->mUlMute);
                pSpeechDriver->SetUplinkMute(call_controller->mMicMute);
                pSpeechDriver->SetDownlinkMute(call_controller->mDlMute);
                ALOGD("%s(), Ctrl: %d, State: %d, wait retval(%d), wait %dms and unmute", __FUNCTION__,
                      call_controller->mMuteDlUlForRoutingCtrl,
                      call_controller->mMuteDlUlForRoutingState,
                      retvalWait, DEFAULT_WAIT_ROUTING_UNMUTE_MS);

            } else {//disturb wait
                if (call_controller->mMuteDlUlForRoutingCtrl == SPH_MUTE_CTRL_ROUTING_START) {
                    //break wait
                    ALOGD_IF(call_controller->mLogEnable, "%s(), Ctrl: %d, State: %d, wait retval(%d), break waiting, keep routing mute", __FUNCTION__,
                          call_controller->mMuteDlUlForRoutingCtrl,
                          call_controller->mMuteDlUlForRoutingState, retvalWait);
                } else if (call_controller->mMuteDlUlForRoutingCtrl == SPH_MUTE_CTRL_VOLUME_UPDATE) {
                    pSpeechDriver->SetUplinkSourceMute(call_controller->mUlMute);
                    pSpeechDriver->SetUplinkMute(call_controller->mMicMute);
                    pSpeechDriver->SetDownlinkMute(call_controller->mDlMute);
                    ALOGD("%s(), Ctrl: %d, State: %d, wait retval(%d), unmute directly", __FUNCTION__,
                          call_controller->mMuteDlUlForRoutingCtrl,
                          call_controller->mMuteDlUlForRoutingState, retvalWait);
                }
            }
        }
    }

    AL_UNLOCK(lock);

MUTE_DL_UL_FOR_ROUTING_THREAD_DONE:
    ALOGV("%s terminated", thread_name);
    pthread_exit(NULL);
    return NULL;
}

} // end of namespace android
