#include "AudioALSAStreamManager.h"

#include <cutils/properties.h>

#include <tinyalsa/asoundlib.h> // TODO(Harvey): move it

#include "WCNChipController.h"

#include "AudioALSAStreamOut.h"
#include "AudioALSAStreamIn.h"
#include "AudioALSAPlaybackHandlerBase.h"
#include "AudioALSAPlaybackHandlerNormal.h"
#include "AudioALSAPlaybackHandlerFast.h"
#include "AudioALSAPlaybackHandlerVoice.h"
#include "AudioALSAPlaybackHandlerBTSCO.h"
#include "AudioALSAPlaybackHandlerBTCVSD.h"
#include "AudioALSAPlaybackHandlerFMTransmitter.h"
#include "AudioALSAPlaybackHandlerHDMI.h"
#ifdef MTK_MAXIM_SPEAKER_SUPPORT
#include "AudioALSAPlaybackHandlerSpeakerProtection.h"
#endif

#if (defined(MTK_CIRRUS_SPEAKER_SUPPORT) || defined(MTK_DUMMY_SPEAKER_SUPPORT) || defined(MTK_SMARTPA_DUMMY_LIB))
#include "AudioALSAPlaybackHandlerSpeakerProtectionDsp.h"
#endif

#if !defined(MTK_BASIC_PACKAGE)
#include "AudioALSAPlaybackHandlerOffload.h"
#endif

#include "AudioALSACaptureHandlerBase.h"
#include "AudioALSACaptureHandlerNormal.h"
#include "AudioALSACaptureHandlerSyncIO.h"
#include "AudioALSACaptureHandlerVoice.h"
#include "AudioALSACaptureHandlerFMRadio.h"
#include "AudioALSACaptureHandlerBT.h"
#include "AudioALSACaptureHandlerVOW.h"
#include "AudioALSACaptureHandlerAEC.h"
#include "AudioALSACaptureHandlerTDM.h"

#include "AudioALSACaptureHandlerModemDai.h"

#if defined(MTK_SPEAKER_MONITOR_SUPPORT)
#include "AudioALSACaptureHandlerSpkFeed.h"
#endif

#include "AudioALSASpeechPhoneCallController.h"
#include "AudioALSAFMController.h"

//#include "AudioALSAVolumeController.h"
//#include "AudioVolumeInterface.h"
#include "AudioVolumeFactory.h"
#include "AudioDeviceInt.h"


#include "AudioALSAVoiceWakeUpController.h"

#include "AudioALSAHardwareResourceManager.h" // TODO(Harvey): move it
#include "AudioALSASpeechStreamController.h"
#include "AudioALSASampleRateController.h"

#include "AudioCompFltCustParam.h"
#include "SpeechDriverInterface.h"
#include "SpeechDriverFactory.h"
#include "AudioALSADriverUtility.h"
#include "SpeechEnhancementController.h"
#include "SpeechVMRecorder.h"
#include "AudioSmartPaController.h"

#if defined(MTK_AUDIO_HIERARCHICAL_PARAM_SUPPORT)
#include "AudioParamParser.h"
#include "AudioALSAParamTuner.h"
#include "SpeechParamParser.h"
#endif

#if defined(MTK_HYBRID_NLE_SUPPORT)
#include "AudioALSANLEController.h"
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "AudioALSAStreamManager"
//#define FM_HIFI_NOT_CONCURRENT
#define AUDIO_HIFI_RATE_MIN (48000)

static struct pcm_config mLoopbackConfig; // TODO(Harvey): move it to AudioALSAHardwareResourceManager later
static struct pcm *mLoopbackPcm = NULL; // TODO(Harvey): move it to AudioALSAHardwareResourceManager & AudioALSAPlaybackDataDispatcher later
static struct pcm_config mLoopbackUlConfig; // TODO(Harvey): move it to AudioALSAHardwareResourceManager later
static struct pcm *mLoopbackUlPcm = NULL; // TODO(Harvey): move it to AudioALSAHardwareResourceManager & AudioALSAPlaybackDataDispatcher later
static struct pcm *mHdmiPcm = NULL; // TODO(Harvey): move it to AudioALSAHardwareResourceManager & AudioALSAPlaybackDataDispatcher later

namespace android {

/*==============================================================================
 *                     Property keys
 *============================================================================*/

const char PROPERTY_KEY_VOICE_WAKE_UP_NEED_ON[PROPERTY_KEY_MAX] = "persist.af.vw_need_on";
const char PROPERTY_KEY_SET_BT_NREC[PROPERTY_KEY_MAX] = "persist.debug.set_bt_aec";
const char PROPERTY_KEY_HIFI_DAC_STATE[PROPERTY_KEY_MAX] = "persist.af.hifi_dac_state";


/*==============================================================================
 *                     Singleton Pattern
 *============================================================================*/

AudioALSAStreamManager *AudioALSAStreamManager::mStreamManager = NULL;
AudioALSAStreamManager *AudioALSAStreamManager::getInstance() {
    static AudioLock mGetInstanceLock;
    AL_AUTOLOCK(mGetInstanceLock);

    if (mStreamManager == NULL) {
        mStreamManager = new AudioALSAStreamManager();
    }
    ASSERT(mStreamManager != NULL);
    return mStreamManager;
}

/*==============================================================================
 *                     Callback Function
 *============================================================================*/

#if defined(MTK_AUDIO_HIERARCHICAL_PARAM_SUPPORT)
void CallbackAudioXmlChanged(AppHandle *appHandle, const char *audioTypeName) {
    ALOGD("+%s(), audioType = %s", __FUNCTION__, audioTypeName);
    // reload XML file
    AppOps *appOps = appOpsGetInstance();
    if (appOps == NULL) {
        ALOGE("%s(), Error: AppOps == NULL", __FUNCTION__);
        ASSERT(0);
        return;
    }

    if (appOps->appHandleReloadAudioType(appHandle, audioTypeName) == APP_ERROR) {
        ALOGE("%s(), Reload xml fail!(audioType = %s)", __FUNCTION__, audioTypeName);
    } else {
        if (strcmp(audioTypeName, audioTypeNameList[AUDIO_TYPE_SPEECH]) == 0) {
            //"Speech"
            AudioALSAStreamManager::getInstance()->UpdateSpeechParams((int)AUDIO_TYPE_SPEECH);
        } else if (strcmp(audioTypeName, audioTypeNameList[AUDIO_TYPE_SPEECH_DMNR]) == 0) {
            //"SpeechDMNR"
            AudioALSAStreamManager::getInstance()->UpdateSpeechParams((int)AUDIO_TYPE_SPEECH_DMNR);
        } else if (strcmp(audioTypeName, audioTypeNameList[AUDIO_TYPE_SPEECH_GENERAL]) == 0) {
            //"SpeechGeneral"
            AudioALSAStreamManager::getInstance()->UpdateSpeechParams((int)AUDIO_TYPE_SPEECH_GENERAL);
        }
    }
}
#endif


/*==============================================================================
 *                     Constructor / Destructor / Init / Deinit
 *============================================================================*/

AudioALSAStreamManager::AudioALSAStreamManager() :
    mStreamOutIndex(0),
    mStreamInIndex(0),
    mPlaybackHandlerIndex(0),
    mCaptureHandlerIndex(0),
    mSpeechPhoneCallController(AudioALSASpeechPhoneCallController::getInstance()),
    mPhoneCallSpeechOpen(false),
    mFMController(AudioALSAFMController::getInstance()),
    mAudioALSAVolumeController(AudioVolumeFactory::CreateAudioVolumeController()),
    mSpeechDriverFactory(SpeechDriverFactory::GetInstance()),
    mMicMute(false),
    mAudioMode(AUDIO_MODE_NORMAL),
    mEnterPhoneCallMode(false),
    mPhoneWithVoip(false),
    mVoipToRingTone(false),
    mPhoneCallControllerStatus(false),
    mResumeAllStreamsAtRouting(false),
    mIsNeedResumeStreamOut(false),
    mLoopbackEnable(false),
    mHdmiEnable(false),
    mFilterManagerNumber(0),
    mBesLoudnessStatus(false),
    mBesLoudnessControlCallback(NULL),
    mAudioSpeechEnhanceInfoInstance(AudioSpeechEnhanceInfo::getInstance()),
    mHeadsetChange(false),
    //#ifdef MTK_VOW_SUPPORT
    mAudioALSAVoiceWakeUpController(AudioALSAVoiceWakeUpController::getInstance()),
    //#endif
    mVoiceWakeUpNeedOn(false),
    mForceDisableVoiceWakeUpForSetMode(false),
    mBypassPostProcessDL(false),
    mBGSDlGain(0xFF),
    mBGSUlGain(0),
    mBypassDualMICProcessUL(false),
    mBtHeadsetName(NULL),
    mAvailableOutputDevices(AUDIO_DEVICE_OUT_SPEAKER | AUDIO_DEVICE_OUT_EARPIECE),
    mHiFiEnable(false),
    mHiFiSampleRate(AUDIO_HIFI_RATE_MIN),
    mCustScene("") {
    ALOGD("%s()", __FUNCTION__);

#ifdef CONFIG_MT_ENG_BUILD
    mLogEnable = 1;
#else
    mLogEnable = __android_log_is_loggable(ANDROID_LOG_DEBUG, LOG_TAG, ANDROID_LOG_INFO);
#endif

    mStreamOutVector.clear();
    mStreamInVector.clear();

    mPlaybackHandlerVector.clear();
    mCaptureHandlerVector.clear();

    mFilterManagerVector.clear();

    // Get hifi status from  System propery
    mHiFiEnable = getHiFiStatus();

    // resume voice wake up need on
    char property_value[PROPERTY_VALUE_MAX];
    property_get(PROPERTY_KEY_VOICE_WAKE_UP_NEED_ON, property_value, "0"); //"0": default off
    const bool bVoiceWakeUpNeedOn = (property_value[0] == '0') ? false : true;
    //the default on setting will control by framework due to init model need be set first by framework. But still need to handle mediaserver died case
    if (bVoiceWakeUpNeedOn == true) {
        if (mAudioALSAVoiceWakeUpController->getVoiceWakeUpStateFromKernel()) {
            setVoiceWakeUpNeedOn(true);
        }
    }
    mAudioCustParamClient = NULL;
    mAudioCustParamClient = AudioCustParamClient::GetInstance();

#ifdef MTK_BESLOUDNESS_SUPPORT
    unsigned int result = 0 ;
    bool firstBootup;
    AUDIO_AUDENH_CONTROL_OPTION_STRUCT audioParam;
    if (mAudioCustParamClient->GetBesLoudnessControlOptionParamFromNV(&audioParam)) {
        result = audioParam.u32EnableFlg;
    } else {
        ALOGW("%s(), get NVRAM data error!\n", __FUNCTION__);
    }
    mBesLoudnessStatus = (result & 0x1 ? true : false);
    firstBootup = (result & 0x2 ? true : false);

    if (firstBootup && AudioSmartPaController::getInstance()->isSmartPAUsed()) {
        audioParam.u32EnableFlg &= (~0x03);
        AudioCustParamClient::GetInstance()->SetBesLoudnessControlOptionParamToNV(&audioParam);
        mBesLoudnessStatus = false;
        ALOGD("%s(), fisrt bootup\n", __FUNCTION__);
    }

    ALOGD("%s(), mBesLoudnessStatus [%d] (From NvRam) \n", __FUNCTION__, mBesLoudnessStatus);
#else
    mBesLoudnessStatus = false;
    ALOGD("%s(), mBesLoudnessStatus [%d] (Always) \n", __FUNCTION__, mBesLoudnessStatus);
#endif

#if defined(MTK_AUDIO_HIERARCHICAL_PARAM_SUPPORT)
    AppHandle *mAppHandle;
    AppOps *appOps = appOpsGetInstance();
    if (appOps == NULL) {
        ALOGE("%s(), Error: AppOps == NULL", __FUNCTION__);
        ASSERT(0);
        return;
    }

    /* Init AppHandle */
    ALOGD("%s() appHandleGetInstance", __FUNCTION__);
    mAppHandle = appOps->appHandleGetInstance();
    ALOGD("%s() appHandleRegXmlChangedCb", __FUNCTION__);

    /* XML changed callback process */
    appOps->appHandleRegXmlChangedCb(mAppHandle, CallbackAudioXmlChanged);
#endif

    // get power hal service first to reduce time for later usage of power hal
    initPowerHal();
}


AudioALSAStreamManager::~AudioALSAStreamManager() {
    ALOGD("%s()", __FUNCTION__);
    if (mBtHeadsetName) {
        free((void *)mBtHeadsetName);
        mBtHeadsetName = NULL;
    }

    if (mAudioMode != AUDIO_MODE_NORMAL) {
        setMode(AUDIO_MODE_NORMAL);
    }
    mStreamManager = NULL;
}


/*==============================================================================
 *                     Implementations
 *============================================================================*/

AudioMTKStreamOutInterface *AudioALSAStreamManager::openOutputStream(
    uint32_t devices,
    int *format,
    uint32_t *channels,
    uint32_t *sampleRate,
    status_t *status,
    uint32_t output_flag) {
    ALOGD("+%s()", __FUNCTION__);
    AL_AUTOLOCK(mStreamVectorLock);
    AL_AUTOLOCK(mLock);

    if (format == NULL || channels == NULL || sampleRate == NULL || status == NULL) {
        ALOGE("%s(), NULL pointer!! format = %p, channels = %p, sampleRate = %p, status = %p",
              __FUNCTION__, format, channels, sampleRate, status);
        if (status != NULL) { *status = INVALID_OPERATION; }
        return NULL;
    }

    ALOGD("%s(), devices = 0x%x, format = 0x%x, channels = 0x%x, sampleRate = %d, status = 0x%x",
          __FUNCTION__, devices, *format, *channels, *sampleRate, *status);

    // stream out flags
#if 1 // TODO(Harvey): why.........
    mStreamOutIndex = (uint32_t)(*status);
#endif
    //const uint32_t flags = 0; //(uint32_t)(*status);

    // create stream out
    AudioALSAStreamOut *pAudioALSAStreamOut = new AudioALSAStreamOut();
    pAudioALSAStreamOut->set(devices, format, channels, sampleRate, status, output_flag);
    if (*status != NO_ERROR) {
        ALOGE("-%s(), set fail, return NULL", __FUNCTION__);
        delete pAudioALSAStreamOut;
        pAudioALSAStreamOut = NULL;
        return NULL;
    }

    // save stream out object in vector
#if 0 // TODO(Harvey): why.........
    pAudioALSAStreamOut->setIdentity(mStreamOutIndex);
    mStreamOutVector.add(mStreamOutIndex, pAudioALSAStreamOut);
    mStreamOutIndex++;
#else
    pAudioALSAStreamOut->setIdentity(mStreamOutIndex);
    mStreamOutVector.add(mStreamOutIndex, pAudioALSAStreamOut);
#endif

    // setup Filter for ACF/HCF/AudEnh/VibSPK // TODO Check return status of pAudioALSAStreamOut->set
    AudioMTKFilterManager *pAudioFilterManagerHandler = new AudioMTKFilterManager(*sampleRate, popcount(*channels), *format, pAudioALSAStreamOut->bufferSize());
    if (pAudioFilterManagerHandler != NULL) {
        if (pAudioFilterManagerHandler->init(output_flag) == NO_ERROR) {
            mFilterManagerVector.add(mStreamOutIndex, pAudioFilterManagerHandler);
        } else {
            delete pAudioFilterManagerHandler;
        }
    }
    //mFilterManagerNumber++;

    ALOGD("-%s(), out = %p, status = 0x%x, mStreamOutVector.size() = %zu",
          __FUNCTION__, pAudioALSAStreamOut, *status, mStreamOutVector.size());


    return pAudioALSAStreamOut;
}

void AudioALSAStreamManager::closeOutputStream(AudioMTKStreamOutInterface *out) {
    ALOGD("+%s(), out = %p, mStreamOutVector.size() = %zu", __FUNCTION__, out, mStreamOutVector.size());
    AL_AUTOLOCK(mStreamVectorLock);
    AL_AUTOLOCK(mLock);

    if (out == NULL) {
        ALOGE("%s(), Cannot close null output stream!! return", __FUNCTION__);
        return;
    }

    AudioALSAStreamOut *pAudioALSAStreamOut = static_cast<AudioALSAStreamOut *>(out);
    ASSERT(pAudioALSAStreamOut != 0);

    uint32_t streamOutId = pAudioALSAStreamOut->getIdentity();

    mStreamOutVector.removeItem(streamOutId);
    delete pAudioALSAStreamOut;

    uint32_t dFltMngindex = mFilterManagerVector.indexOfKey(streamOutId);

    if (dFltMngindex < mFilterManagerVector.size()) {
        AudioMTKFilterManager *pAudioFilterManagerHandler = static_cast<AudioMTKFilterManager *>(mFilterManagerVector[dFltMngindex]);
        ALOGD("%s, remove mFilterManagerVector Success [%d]/[%zu] [%d], pAudioFilterManagerHandler=%p",
              __FUNCTION__, dFltMngindex, mFilterManagerVector.size(), streamOutId, pAudioFilterManagerHandler);
        ASSERT(pAudioFilterManagerHandler != 0);
        mFilterManagerVector.removeItem(streamOutId);
        delete pAudioFilterManagerHandler;
    } else {
        ALOGD("%s, Remove mFilterManagerVector Error [%d]/[%zu]", __FUNCTION__, dFltMngindex, mFilterManagerVector.size());
    }

    ALOGD("-%s(), mStreamOutVector.size() = %zu", __FUNCTION__, mStreamOutVector.size());
}


AudioMTKStreamInInterface *AudioALSAStreamManager::openInputStream(
    uint32_t devices,
    int *format,
    uint32_t *channels,
    uint32_t *sampleRate,
    status_t *status,
    audio_in_acoustics_t acoustics,
    uint32_t input_flag) {
    AL_AUTOLOCK(mStreamVectorLock);
    AL_AUTOLOCK(mLock);

    if (format == NULL || channels == NULL || sampleRate == NULL || status == NULL) {
        ALOGE("%s(), NULL pointer!! format = %p, channels = %p, sampleRate = %p, status = %p",
              __FUNCTION__, format, channels, sampleRate, status);
        if (status != NULL) { *status = INVALID_OPERATION; }
        return NULL;
    }

    ALOGD("%s(), devices = 0x%x, format = 0x%x, channels = 0x%x, sampleRate = %d, status = %d, acoustics = 0x%x",
          __FUNCTION__, devices, *format, *channels, *sampleRate, *status, acoustics);

#if 1 // TODO(Harvey): why.........
    mStreamInIndex = (uint32_t)(*status);
#endif

    // create stream in
    AudioALSAStreamIn *pAudioALSAStreamIn = new AudioALSAStreamIn();
    audio_devices_t input_device = static_cast<audio_devices_t>(devices);
    bool sharedDevice = (input_device & ~AUDIO_DEVICE_BIT_IN) & (AUDIO_DEVICE_IN_BUILTIN_MIC | AUDIO_DEVICE_IN_BACK_MIC | AUDIO_DEVICE_IN_WIRED_HEADSET);

    //In PhonecallMode and the new input device is sharedDevice,we may do some check
    if ((isModeInPhoneCall() == true) && (sharedDevice == true)) {
        input_device = mSpeechPhoneCallController->getPhonecallInputDevice();
        sharedDevice = (input_device & ~AUDIO_DEVICE_BIT_IN) & (AUDIO_DEVICE_IN_BUILTIN_MIC | AUDIO_DEVICE_IN_BACK_MIC | AUDIO_DEVICE_IN_WIRED_HEADSET);
        if (sharedDevice == true) { //if phonecall_device also use sharedDevice, set the input_device = phonecall_device
            devices = static_cast<uint32_t>(input_device);
            ALOGD("+%s(), isModeInPhoneCall, force to set input_device = 0x%x", __FUNCTION__, input_device);
        }
    } else {
        if ((sharedDevice == true) && (mStreamInVector.size() > 0)) {
            input_device = CheckInputDevicePriority(input_device);
            devices = static_cast<uint32_t>(input_device);
            for (size_t i = 0; i < mStreamInVector.size(); i++) {
                sharedDevice = ((mStreamInVector[i]->getStreamAttribute()->input_device) & ~AUDIO_DEVICE_BIT_IN) & (AUDIO_DEVICE_IN_BUILTIN_MIC | AUDIO_DEVICE_IN_BACK_MIC | AUDIO_DEVICE_IN_WIRED_HEADSET);
                if ((sharedDevice == true) && ((mStreamInVector[i]->getStreamAttribute()->input_device) != input_device)) {
                    mStreamInVector[i]->routing(input_device);
                }
            }
        }
    }

#ifdef UPLINK_LOW_LATENCY
    if ((input_flag == 1) && (mStreamInVector.size() > 0)) {
        for (size_t i = 0; i < mStreamInVector.size(); i++) {
            if (mStreamInVector[i]->getStreamAttribute()->mAudioInputFlags != AUDIO_INPUT_FLAG_FAST) {
                input_flag = 0;
                ALOGD("+%s(), Fast Record Reject by HAL, because Normal Record is using, force to set input_flag = %d", __FUNCTION__, input_flag);
                break;
            }
        }
    }

    pAudioALSAStreamIn->set(devices, format, channels, sampleRate, status, acoustics, input_flag);
#else
    pAudioALSAStreamIn->set(devices, format, channels, sampleRate, status, acoustics);
#endif
    if (*status != NO_ERROR) {
        ALOGE("-%s(), set fail, return NULL", __FUNCTION__);
        delete pAudioALSAStreamIn;
        pAudioALSAStreamIn = NULL;
        return NULL;
    }

    // save stream in object in vector
#if 0 // TODO(Harvey): why.........
    pAudioALSAStreamIn->setIdentity(mStreamInIndex);
    mStreamInVector.add(mStreamInIndex, pAudioALSAStreamIn);
    mStreamInIndex++;
#else
    pAudioALSAStreamIn->setIdentity(mStreamInIndex);
    mStreamInVector.add(mStreamInIndex, pAudioALSAStreamIn);
#endif

    ALOGD("-%s(), in = %p, status = 0x%x, mStreamInVector.size() = %zu",
          __FUNCTION__, pAudioALSAStreamIn, *status, mStreamInVector.size());
    return pAudioALSAStreamIn;
}


void AudioALSAStreamManager::closeInputStream(AudioMTKStreamInInterface *in) {
    ALOGD("+%s(), in = %p, size() = %zu", __FUNCTION__, in, mStreamInVector.size());
    AL_AUTOLOCK(mStreamVectorLock);
    AL_AUTOLOCK(mLock);

    if (in == NULL) {
        ALOGE("%s(), Cannot close null input stream!! return", __FUNCTION__);
        return;
    }

    AudioALSAStreamIn *pAudioALSAStreamIn = static_cast<AudioALSAStreamIn *>(in);
    ASSERT(pAudioALSAStreamIn != 0);

    mStreamInVector.removeItem(pAudioALSAStreamIn->getIdentity());
    delete pAudioALSAStreamIn;


    // make sure voice wake up is resume when all capture stream stop if need
    if (mVoiceWakeUpNeedOn == true &&
        mForceDisableVoiceWakeUpForSetMode == false) {
        ALOGD("%s(), resume voice wake up", __FUNCTION__);
        //ASSERT(mAudioALSAVoiceWakeUpController->getVoiceWakeUpEnable() == false); // TODO(Harvey): double check, remove it later
        if (mAudioALSAVoiceWakeUpController->getVoiceWakeUpEnable() == false) {
            mAudioALSAVoiceWakeUpController->setVoiceWakeUpEnable(true);
        }
    }

    if (mStreamInVector.size() == 0) {
        mAudioSpeechEnhanceInfoInstance->SetHifiRecord(false);
    } else {
        bool bClear = true;
        for (size_t i = 0; i < mStreamInVector.size(); i++) {
            pAudioALSAStreamIn = mStreamInVector[i];

            if (pAudioALSAStreamIn->getStreamInCaptureHandler() == NULL) {
                ALOGD("%s(), mStreamInVector[%zu] capture handler close already", __FUNCTION__, i);
                continue;
            }

            if (pAudioALSAStreamIn->getStreamInCaptureHandler()->getCaptureHandlerType() == CAPTURE_HANDLER_NORMAL) {
                bClear = false;
                break;
            }
        }
        if (bClear) { //if still has Normal capture handler, not to reset hifi record status.
            mAudioSpeechEnhanceInfoInstance->SetHifiRecord(false);
        }
    }

    ALOGD("-%s(), mStreamInVector.size() = %zu", __FUNCTION__, mStreamInVector.size());
}

void dlStreamAttributeSourceCustomization(stream_attribute_t *streamAttribute) {
    if (!strcmp(streamAttribute->mCustScene, "App1")) {
        /* App1 Scene customization */
    } if (!strcmp(streamAttribute->mCustScene, "App2")) {
        /* App2 Scene customization: Music playback will using VoIP DL processing */
        streamAttribute->mVoIPEnable = true;
        ALOGD("%s(), Scene is App2, enable VoIP DL processing", __FUNCTION__);
    }
}

AudioALSAPlaybackHandlerBase *AudioALSAStreamManager::createPlaybackHandler(
    stream_attribute_t *stream_attribute_source) {
    ALOGD("+%s(), mAudioMode = %d, output_devices = 0x%x", __FUNCTION__, mAudioMode, stream_attribute_source->output_devices);
    AL_AUTOLOCK(mAudioModeLock);

    // Init input stream attribute here
    stream_attribute_source->audio_mode = mAudioMode; // set mode to stream attribute for mic gain setting
    stream_attribute_source->mVoIPEnable = isModeInVoipCall();

    // just use what stream out is ask to use
    //stream_attribute_source->sample_rate = AudioALSASampleRateController::getInstance()->getPrimaryStreamOutSampleRate();

    //for DMNR tuning
    stream_attribute_source->BesRecord_Info.besrecord_dmnr_tuningEnable = mAudioSpeechEnhanceInfoInstance->IsAPDMNRTuningEnable();
    stream_attribute_source->bBypassPostProcessDL = mBypassPostProcessDL;
    stream_attribute_source->u8BGSDlGain = mBGSDlGain;
    stream_attribute_source->u8BGSUlGain = mBGSUlGain;
    strncpy(stream_attribute_source->mCustScene, mCustScene.string(), SCENE_NAME_LEN_MAX - 1);

    //todo:: enable ACF if support
    if (stream_attribute_source->sample_rate > 48000) {
        stream_attribute_source->bBypassPostProcessDL = true;
    }

    dlStreamAttributeSourceCustomization(stream_attribute_source);

    // create
    AudioALSAPlaybackHandlerBase *pPlaybackHandler = NULL;
    if (isModeInPhoneCall() == true) {
        switch (stream_attribute_source->output_devices) {
#ifdef MTK_MAXIM_SPEAKER_SUPPORT
        case AUDIO_DEVICE_OUT_SPEAKER_SAFE: {
            stream_attribute_source->bBypassPostProcessDL = true;
            stream_attribute_source->sample_rate = 16000;
            pPlaybackHandler = new AudioALSAPlaybackHandlerSpeakerProtection(stream_attribute_source);
            uint32_t dFltMngindex = mFilterManagerVector.indexOfKey(stream_attribute_source->mStreamOutIndex);
            ALOGD("%s(), ApplyFilter [%u]/[%zu] Device [0x%x]", __FUNCTION__, dFltMngindex, mFilterManagerVector.size(), stream_attribute_source->output_devices);
            if (dFltMngindex < mFilterManagerVector.size()) {
                pPlaybackHandler->setFilterMng(static_cast<AudioMTKFilterManager *>(mFilterManagerVector[dFltMngindex]));
                mFilterManagerVector[dFltMngindex]->setDevice(stream_attribute_source->output_devices);
            }
            break;
        }
#endif
        case AUDIO_DEVICE_OUT_AUX_DIGITAL:
            pPlaybackHandler = new AudioALSAPlaybackHandlerHDMI(stream_attribute_source);
            break;
        default:
            pPlaybackHandler = new AudioALSAPlaybackHandlerVoice(stream_attribute_source);
            break;
        }
    } else {
        switch (stream_attribute_source->output_devices) {
        case AUDIO_DEVICE_OUT_BLUETOOTH_SCO:
        case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET:
        case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT: {
            if (WCNChipController::GetInstance()->IsBTMergeInterfaceSupported() == true) {
                pPlaybackHandler = new AudioALSAPlaybackHandlerBTSCO(stream_attribute_source);
            } else {
                pPlaybackHandler = new AudioALSAPlaybackHandlerBTCVSD(stream_attribute_source);
            }
            break;
        }
        case AUDIO_DEVICE_OUT_AUX_DIGITAL: {
            pPlaybackHandler = new AudioALSAPlaybackHandlerHDMI(stream_attribute_source);
            break;
        }
        case AUDIO_DEVICE_OUT_FM: {
            pPlaybackHandler = new AudioALSAPlaybackHandlerFMTransmitter(stream_attribute_source);
            break;
        }
        case AUDIO_DEVICE_OUT_EARPIECE:
        case AUDIO_DEVICE_OUT_WIRED_HEADSET:
        case AUDIO_DEVICE_OUT_WIRED_HEADPHONE:
        case AUDIO_DEVICE_OUT_SPEAKER:
        default: {
#if !defined(MTK_BASIC_PACKAGE)
            if (AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD & stream_attribute_source->mAudioOutputFlags) {
                pPlaybackHandler = new AudioALSAPlaybackHandlerOffload(stream_attribute_source);
            } else
#endif
            {
#ifdef DOWNLINK_LOW_LATENCY
                if (AUDIO_OUTPUT_FLAG_FAST & stream_attribute_source->mAudioOutputFlags &&
                    !(AUDIO_OUTPUT_FLAG_PRIMARY & stream_attribute_source->mAudioOutputFlags)) {
                    pPlaybackHandler = new AudioALSAPlaybackHandlerFast(stream_attribute_source);
                } else
#endif
                {
#if (defined(MTK_MAXIM_SPEAKER_SUPPORT) || defined(MTK_CIRRUS_SPEAKER_SUPPORT) || defined(MTK_DUMMY_SPEAKER_SUPPORT) || defined(MTK_SMARTPA_DUMMY_LIB))
                    if (stream_attribute_source->output_devices & AUDIO_DEVICE_OUT_SPEAKER) {
#if defined(MTK_MAXIM_SPEAKER_SUPPORT)
                        pPlaybackHandler = new AudioALSAPlaybackHandlerSpeakerProtection(stream_attribute_source);
#elif  (defined(MTK_CIRRUS_SPEAKER_SUPPORT) || defined(MTK_DUMMY_SPEAKER_SUPPORT) || defined(MTK_SMARTPA_DUMMY_LIB))
                        if (stream_attribute_source->mAudioOutputFlags & AUDIO_OUTPUT_FLAG_PRIMARY)
                           pPlaybackHandler = new AudioALSAPlaybackHandlerSpeakerProtectionDsp(stream_attribute_source);
                        else
                           pPlaybackHandler = new AudioALSAPlaybackHandlerNormal(stream_attribute_source);
#endif
                    } else
#endif
                    {
                        pPlaybackHandler = new AudioALSAPlaybackHandlerNormal(stream_attribute_source);
                    }
                }
            }
            break;
        }
        }
        uint32_t dFltMngindex = mFilterManagerVector.indexOfKey(stream_attribute_source->mStreamOutIndex);
        ALOGV("%s(), ApplyFilter [%u]/[%zu] Device [0x%x]", __FUNCTION__, dFltMngindex, mFilterManagerVector.size(), stream_attribute_source->output_devices);

        if (dFltMngindex < mFilterManagerVector.size()) {
            pPlaybackHandler->setFilterMng(static_cast<AudioMTKFilterManager *>(mFilterManagerVector[dFltMngindex]));
            mFilterManagerVector[dFltMngindex]->setDevice(stream_attribute_source->output_devices);
        }
    }

    // save playback handler object in vector
    ASSERT(pPlaybackHandler != NULL);
    pPlaybackHandler->setIdentity(mPlaybackHandlerIndex);

    AL_LOCK(mPlaybackHandlerVectorLock);
    mPlaybackHandlerVector.add(mPlaybackHandlerIndex, pPlaybackHandler);
    AL_UNLOCK(mPlaybackHandlerVectorLock);

    mPlaybackHandlerIndex++;

#if defined(MTK_AUDIO_SW_DRE) && defined(MTK_NEW_VOL_CONTROL)
    AudioMTKGainController::getInstance()->registerPlaybackHandler(pPlaybackHandler->getIdentity());
#endif

    ALOGD("-%s(), mPlaybackHandlerVector.size() = %zu", __FUNCTION__, mPlaybackHandlerVector.size());
    return pPlaybackHandler;
}


status_t AudioALSAStreamManager::destroyPlaybackHandler(AudioALSAPlaybackHandlerBase *pPlaybackHandler) {
    ALOGV("+%s(), mode = %d, pPlaybackHandler = %p", __FUNCTION__, mAudioMode, pPlaybackHandler);
    //AL_AUTOLOCK(mLock); // TODO(Harvey): setparam -> routing -> close -> destroy deadlock

    status_t status = NO_ERROR;

#if defined(MTK_AUDIO_SW_DRE) && defined(MTK_NEW_VOL_CONTROL)
    AudioMTKGainController::getInstance()->removePlaybackHandler(pPlaybackHandler->getIdentity());
#endif

    AL_LOCK(mPlaybackHandlerVectorLock);
    mPlaybackHandlerVector.removeItem(pPlaybackHandler->getIdentity());
    AL_UNLOCK(mPlaybackHandlerVectorLock);

    ALOGD("-%s(), mode = %d, pPlaybackHandler = %p, mPlaybackHandlerVector.size() = %zu",
          __FUNCTION__, mAudioMode, pPlaybackHandler, mPlaybackHandlerVector.size());

    delete pPlaybackHandler;

    return status;
}

void ulStreamAttributeTargetCustomization(stream_attribute_t *streamAttribute) {
    if (!strcmp((char*)streamAttribute->mCustScene, "App1")) {
        /* App1 Scene customization */
    } else if (!strcmp((char*)streamAttribute->mCustScene, "App2")) {
        /* App2 Scene customization: normal record will using VoIP processing */
        if (streamAttribute->input_source == AUDIO_SOURCE_MIC) {
            streamAttribute->input_source = AUDIO_SOURCE_VOICE_COMMUNICATION;
            ALOGD("%s(), Scene is App2, replace MIC input source with communication", __FUNCTION__);
        }
    }
}

AudioALSACaptureHandlerBase *AudioALSAStreamManager::createCaptureHandler(
    stream_attribute_t *stream_attribute_target) {
    ALOGD("+%s(), mAudioMode = %d, input_source = %d, input_device = 0x%x, mBypassDualMICProcessUL=%d, sample_rate=%d",
          __FUNCTION__, mAudioMode, stream_attribute_target->input_source, stream_attribute_target->input_device, mBypassDualMICProcessUL, stream_attribute_target->sample_rate);
    //AL_AUTOLOCK(mLock);
    status_t retval = AL_LOCK_MS(mLock, 1000);
    if (retval != NO_ERROR) {
        ALOGD("mLock timeout : 1s , return NULL");
        return NULL;
    }

    // use primary stream out device
    audio_devices_t current_output_devices = (mStreamOutVector.size() > 0) ?
                                             mStreamOutVector[0]->getStreamAttribute()->output_devices :
                                             AUDIO_DEVICE_NONE;

    if (isBtSpkDevice(current_output_devices)) {
        // use SPK setting for BTSCO + SPK
        current_output_devices = (audio_devices_t)(current_output_devices & (~AUDIO_DEVICE_OUT_ALL_SCO));
    }

    // Init input stream attribute here
    stream_attribute_target->audio_mode = mAudioMode; // set mode to stream attribute for mic gain setting
    stream_attribute_target->output_devices = current_output_devices; // set output devices to stream attribute for mic gain setting and BesRecord parameter
    stream_attribute_target->micmute = mMicMute;
    strncpy(stream_attribute_target->mCustScene, mCustScene.string(), SCENE_NAME_LEN_MAX - 1);

    // BesRecordInfo
    stream_attribute_target->BesRecord_Info.besrecord_enable = false; // default set besrecord off
    stream_attribute_target->BesRecord_Info.besrecord_bypass_dualmicprocess = mBypassDualMICProcessUL;  // bypass dual MIC preprocess
    stream_attribute_target->BesRecord_Info.besrecord_voip_enable = false;

    /* StreamAttribute customization for scene */
    ulStreamAttributeTargetCustomization(stream_attribute_target);

    // create
    AudioALSACaptureHandlerBase *pCaptureHandler = NULL;
#if 0
#if defined(MTK_SPEAKER_MONITOR_SPEECH_SUPPORT)
    if (stream_attribute_target->input_device == AUDIO_DEVICE_IN_SPK_FEED) {
        pCaptureHandler = new AudioALSACaptureHandlerModemDai(stream_attribute_target);
    } else
#endif
#endif
    {
        if (stream_attribute_target->input_source == AUDIO_SOURCE_FM_TUNER) {
            if (isEchoRefUsing() == true) {
                ALOGD("%s(), not support FM record in VoIP mode, return NULL", __FUNCTION__);
                AL_UNLOCK(mLock);
                return NULL;
            }

            pCaptureHandler = new AudioALSACaptureHandlerFMRadio(stream_attribute_target);
        } else if (stream_attribute_target->input_device == AUDIO_DEVICE_IN_BUS) {
            pCaptureHandler = new AudioALSACaptureHandlerVoice(stream_attribute_target);
        } else if (stream_attribute_target->input_source == AUDIO_SOURCE_HOTWORD) {
            if (mAudioALSAVoiceWakeUpController->getVoiceWakeUpEnable() == false) {
                mAudioALSAVoiceWakeUpController->setVoiceWakeUpEnable(true);
            }
            pCaptureHandler = new AudioALSACaptureHandlerVOW(stream_attribute_target);
        } else if (stream_attribute_target->input_source == AUDIO_SOURCE_VOICE_UNLOCK) {
            pCaptureHandler = new AudioALSACaptureHandlerSyncIO(stream_attribute_target);
        }
        //else if (isModeInPhoneCall() == true)
        else if ((stream_attribute_target->input_source == AUDIO_SOURCE_VOICE_UPLINK) || (stream_attribute_target->input_source == AUDIO_SOURCE_VOICE_DOWNLINK) ||
                 (stream_attribute_target->input_source == AUDIO_SOURCE_VOICE_CALL) ||
                 ((isModeInPhoneCall() == true) && (WCNChipController::GetInstance()->IsBTMergeInterfaceSupported() == false) && (stream_attribute_target->input_device == AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET))) {
            if ((isModeInPhoneCall() == false) ||
                (SpeechDriverFactory::GetInstance()->GetSpeechDriver()->GetApSideModemStatus(SPEECH_STATUS_MASK) == false)) {
                ALOGD("Can not open PhoneCall Record now !! Because it is not PhoneCallMode or speech driver is not ready, return NULL");
                AL_UNLOCK(mLock);
                return NULL;
            }
            pCaptureHandler = new AudioALSACaptureHandlerVoice(stream_attribute_target);
        } else if ((mPhoneWithVoip == false) && ((isModeInVoipCall() == true) || (stream_attribute_target->NativePreprocess_Info.PreProcessEffect_AECOn == true)
                   || (stream_attribute_target->input_source == AUDIO_SOURCE_VOICE_COMMUNICATION)
                   || (stream_attribute_target->input_source == AUDIO_SOURCE_CUSTOMIZATION1) //MagiASR enable AEC
                   || (stream_attribute_target->input_source == AUDIO_SOURCE_CUSTOMIZATION2))) { //Normal REC with AEC
            stream_attribute_target->BesRecord_Info.besrecord_enable = EnableBesRecord();
            if (mStreamInVector.size() > 1) {
                for (size_t i = 0; i < mStreamInVector.size(); i++) {
                    if (mStreamInVector[i]->getStreamAttribute()->input_source == AUDIO_SOURCE_FM_TUNER) {
                        mStreamInVector[i]->standby();
                    }
                }
            }
            if (isModeInVoipCall() == true || (stream_attribute_target->input_source == AUDIO_SOURCE_VOICE_COMMUNICATION)) {
                stream_attribute_target->BesRecord_Info.besrecord_voip_enable = true;
                stream_attribute_target->mVoIPEnable = true;
#if 0
                if (current_output_devices == AUDIO_DEVICE_OUT_SPEAKER) {
                    if (stream_attribute_target->input_device == AUDIO_DEVICE_IN_BUILTIN_MIC) {
                        if (USE_REFMIC_IN_LOUDSPK == 1) {
                            ALOGD("%s(), routing changed!! input_device: 0x%x => 0x%x",
                                  __FUNCTION__, stream_attribute_target->input_device, AUDIO_DEVICE_IN_BACK_MIC);
                            stream_attribute_target->input_device = AUDIO_DEVICE_IN_BACK_MIC;
                        }
                    }
                }
#endif
            }
#if defined(MTK_AURISYS_FRAMEWORK_SUPPORT) && defined(MTK_CUS2_AUDIO_SOURCE_REPLACE_AEC_EFFECT)
            else if (stream_attribute_target->NativePreprocess_Info.PreProcessEffect_AECOn == true
                     && stream_attribute_target->input_source == AUDIO_SOURCE_MIC) {
                // Not VoIP/AEC input source but AEC effect enabled, using customization2 to do AEC processing
                stream_attribute_target->input_source = AUDIO_SOURCE_CUSTOMIZATION2;
                ALOGD("Normal record && AEC enabled, set the input source with %d", stream_attribute_target->input_source);
            }
#endif
            switch (stream_attribute_target->input_device) {
            case AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET: {
#if defined(MTK_AURISYS_FRAMEWORK_SUPPORT)
                /* Only BT ALSA+Aurisys arch support AEC processing */
                if (stream_attribute_target->output_devices & AUDIO_DEVICE_OUT_ALL_SCO) {
                    pCaptureHandler = new AudioALSACaptureHandlerAEC(stream_attribute_target);
                } else {
                    pCaptureHandler = new AudioALSACaptureHandlerBT(stream_attribute_target);
                }
#else
                pCaptureHandler = new AudioALSACaptureHandlerBT(stream_attribute_target);
#endif
                break;
            }
            default: {
                pCaptureHandler = new AudioALSACaptureHandlerAEC(stream_attribute_target);
                break;
            }
            }
        } else {
            //enable BesRecord if not these input sources
            if ((stream_attribute_target->input_source != AUDIO_SOURCE_VOICE_UNLOCK) &&
                (stream_attribute_target->input_source != AUDIO_SOURCE_FM_TUNER)) { // TODO(Harvey, Yu-Hung): never go through here?
#if 0   //def UPLINK_LOW_LATENCY
                if ((stream_attribute_target->mAudioInputFlags & AUDIO_INPUT_FLAG_FAST) || (stream_attribute_target->sample_rate > 48000)) {
                    stream_attribute_target->BesRecord_Info.besrecord_enable = false;
                } else {
                    stream_attribute_target->BesRecord_Info.besrecord_enable = EnableBesRecord();
                }
#else
                if (stream_attribute_target->sample_rate > 48000) { //no uplink preprocess for sample rate higher than 48k
                    stream_attribute_target->BesRecord_Info.besrecord_enable = false;
                } else {
                    stream_attribute_target->BesRecord_Info.besrecord_enable = EnableBesRecord();
                }
#endif
            }

            bool bReCreate = false;
            if ((stream_attribute_target->sample_rate > 48000) && !mAudioSpeechEnhanceInfoInstance->GetHifiRecord()) { //no HifiRecord ongoing, and need to create HiFiRecord
                mAudioSpeechEnhanceInfoInstance->SetHifiRecord(true);
                if (mCaptureHandlerVector.size() > 0) { //already has another streamin ongoing with CAPTURE_HANDLER_NORMAL
                    for (size_t i = 0; i < mCaptureHandlerVector.size(); i++) {
                        if (mCaptureHandlerVector[i]->getCaptureHandlerType() == CAPTURE_HANDLER_NORMAL) {
                            bReCreate = true;
                            break;
                        }
                    }
                }
                if (bReCreate) { //need to re-create related capture handler for dataprovider reopen and dataclient SRC set.
                    ALOGD("%s(), reCreate streamin for hifi record +", __FUNCTION__);
                    //only suspend and standby CAPTURE_HANDLER_NORMAL streamin
                    setAllInputStreamsSuspend(true, false, CAPTURE_HANDLER_NORMAL);
                    standbyAllInputStreams(false, CAPTURE_HANDLER_NORMAL);
                    setAllInputStreamsSuspend(false, false, CAPTURE_HANDLER_NORMAL);
                    ALOGD("%s(), reCreate streamin for hifi record -", __FUNCTION__);
                }
            }

            switch (stream_attribute_target->input_device) {
#if defined(MTK_SPEAKER_MONITOR_SUPPORT)
            case  AUDIO_DEVICE_IN_SPK_FEED: {
                pCaptureHandler = new AudioALSACaptureHandlerSpkFeed(stream_attribute_target);
                break;
            }
#endif
            case AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET: {
                pCaptureHandler = new AudioALSACaptureHandlerBT(stream_attribute_target);
                break;
            }
#if defined(MTK_TDM_SUPPORT)
            case AUDIO_DEVICE_IN_HDMI: {
                pCaptureHandler = new AudioALSACaptureHandlerTDM(stream_attribute_target);
                break;
            }
#endif
            case AUDIO_DEVICE_IN_BUILTIN_MIC:
            case AUDIO_DEVICE_IN_BACK_MIC:
            case AUDIO_DEVICE_IN_WIRED_HEADSET:
            default: {
                pCaptureHandler = new AudioALSACaptureHandlerNormal(stream_attribute_target);
                break;
            }
            }
        }
    }

    if (stream_attribute_target->input_source != AUDIO_SOURCE_HOTWORD) {
        if (mAudioALSAVoiceWakeUpController->getVoiceWakeUpEnable() == true) {
            ALOGI("Not Hotword Record,Actually Force Close VOW");
            mAudioALSAVoiceWakeUpController->setVoiceWakeUpEnable(false);
        }
    }

    // save capture handler object in vector
    ASSERT(pCaptureHandler != NULL);
    pCaptureHandler->setIdentity(mCaptureHandlerIndex);
    mCaptureHandlerVector.add(mCaptureHandlerIndex, pCaptureHandler);
    mCaptureHandlerIndex++;
    AL_UNLOCK(mLock);
    ALOGD("-%s(), mCaptureHandlerVector.size() = %zu", __FUNCTION__, mCaptureHandlerVector.size());
    return pCaptureHandler;
}


status_t AudioALSAStreamManager::destroyCaptureHandler(AudioALSACaptureHandlerBase *pCaptureHandler) {
    ALOGD("+%s(), mode = %d, pCaptureHandler = %p", __FUNCTION__, mAudioMode, pCaptureHandler);
    //AL_AUTOLOCK(mLock); // TODO(Harvey): setparam -> routing -> close -> destroy deadlock

    status_t status = NO_ERROR;

    mCaptureHandlerVector.removeItem(pCaptureHandler->getIdentity());
    delete pCaptureHandler;

    ALOGD("-%s(), mCaptureHandlerVector.size() = %zu", __FUNCTION__, mCaptureHandlerVector.size());
    return status;
}


status_t AudioALSAStreamManager::setVoiceVolume(float volume) {
    ALOGD("%s(), volume = %f", __FUNCTION__, volume);

    if (volume < 0.0 || volume > 1.0) {
        ALOGE("-%s(), strange volume level %f, something wrong!!", __FUNCTION__, volume);
        return BAD_VALUE;
    }

    AL_AUTOLOCK(mLock);

    if (mAudioALSAVolumeController) {
        // use primary stream out device
        const audio_devices_t current_output_devices = (mStreamOutVector.size() > 0)
                                                       ? mStreamOutVector[0]->getStreamAttribute()->output_devices
                                                       : AUDIO_DEVICE_NONE;
        mAudioALSAVolumeController->setVoiceVolume(volume, getModeForGain(), current_output_devices);
        AudioALSASpeechPhoneCallController::getInstance()->updateVolume();
    }

    return NO_ERROR;
}

#ifdef MTK_AUDIO_GAIN_TABLE
status_t AudioALSAStreamManager::setAnalogVolume(int stream, int device, int index, bool force_incall) {
    ALOGV("%s(),stream=%d, device=%d, index=%d", __FUNCTION__, stream, device, index);

    AL_AUTOLOCK(mLock);

    if (mAudioALSAVolumeController) {
        if (force_incall == 0) {
            mAudioALSAVolumeController->setAnalogVolume(stream, device, index, getModeForGain());
        } else {
            mAudioALSAVolumeController->setAnalogVolume(stream, device, index, AUDIO_MODE_IN_CALL);
        }
    }

    return NO_ERROR;
}

int AudioALSAStreamManager::SetCaptureGain(void) {
    const stream_attribute_t *mStreamAttributeTarget;
    uint32_t i;
    ALOGD("%s(), mStreamInVector.size() = %zu", __FUNCTION__, mStreamInVector.size());

    for (i = 0; i < mStreamInVector.size(); i++) {
        //if(mStreamInVector[i]->getStreamAttribute()->output_devices == output_devices)
        {
            mStreamAttributeTarget = mStreamInVector[i]->getStreamAttribute();
            if (mAudioALSAVolumeController != NULL) {
                mAudioALSAVolumeController->SetCaptureGain(mAudioMode, mStreamAttributeTarget->input_source, mStreamAttributeTarget->input_device, mStreamAttributeTarget->output_devices);
            }
        }
    }
    return 0;
}

#endif
float AudioALSAStreamManager::getMasterVolume(void) {
    return mAudioALSAVolumeController->getMasterVolume();
}

uint32_t AudioALSAStreamManager::GetOffloadGain(float vol_f) {
    if (mAudioALSAVolumeController != NULL) {
        return mAudioALSAVolumeController->GetOffloadGain(vol_f);
    }
    ALOGE("%s(), VolumeController Null", __FUNCTION__);
    return -1;
}

status_t AudioALSAStreamManager::setMasterVolume(float volume, uint32_t iohandle) {
    ALOGD("%s(), volume = %f", __FUNCTION__, volume);

    if (volume < 0.0 || volume > 1.0) {
        ALOGE("-%s(), strange volume level %f, something wrong!!", __FUNCTION__, volume);
        return BAD_VALUE;
    }

    AL_AUTOLOCK(mLock);
    if (mAudioALSAVolumeController) {
        audio_devices_t current_output_devices;
        uint32_t index = mStreamOutVector.indexOfKey(iohandle);
        if (index < mStreamOutVector.size()) {
            current_output_devices = mStreamOutVector[index]->getStreamAttribute()->output_devices;
        } else {
            current_output_devices  = (mStreamOutVector.size() > 0)
                                      ? mStreamOutVector[0]->getStreamAttribute()->output_devices
                                      : AUDIO_DEVICE_NONE;
        }
        mAudioALSAVolumeController->setMasterVolume(volume, getModeForGain(), current_output_devices);
    }

    return NO_ERROR;
}

status_t AudioALSAStreamManager::setHeadsetVolumeMax() {
    ALOGD("%s()", __FUNCTION__);
    mAudioALSAVolumeController->setAudioBufferGain(0);
    return NO_ERROR;
}

status_t AudioALSAStreamManager::setFmVolume(float volume) {
    ALOGV("+%s(), volume = %f", __FUNCTION__, volume);

    if (volume < 0.0 || volume > 1.0) {
        ALOGE("-%s(), strange volume level %f, something wrong!!", __FUNCTION__, volume);
        return BAD_VALUE;
    }

    AL_AUTOLOCK(mLock);
    mFMController->setFmVolume(volume);

    return NO_ERROR;
}

status_t AudioALSAStreamManager::setMicMute(bool state) {
    ALOGD("%s(), mMicMute: %d => %d", __FUNCTION__, mMicMute, state);
    AL_AUTOLOCK(mLock);
    AudioALSASpeechPhoneCallController::getInstance()->setMicMute(state);
    if (isModeInPhoneCall() == false) {
        SetInputMute(state);
    }
    mMicMute = state;
    return NO_ERROR;
}


bool AudioALSAStreamManager::getMicMute() {
    ALOGD("%s(), mMicMute = %d", __FUNCTION__, mMicMute);
    AL_AUTOLOCK(mLock);

    return mMicMute;
}

void AudioALSAStreamManager::SetInputMute(bool bEnable) {
    ALOGD("+%s(), %d", __FUNCTION__, bEnable);
    if (mStreamInVector.size() > 0) {
        for (size_t i = 0; i < mStreamInVector.size(); i++) { // TODO(Harvey): Mic+FM !?
            mStreamInVector[i]->SetInputMute(bEnable);
        }
    }
    ALOGD("-%s(), %d", __FUNCTION__, bEnable);
}

status_t AudioALSAStreamManager::setVtNeedOn(const bool vt_on) {
    ALOGD("%s(), setVtNeedOn: %d", __FUNCTION__, vt_on);
    AudioALSASpeechPhoneCallController::getInstance()->setVtNeedOn(vt_on);

    return NO_ERROR;
}

status_t AudioALSAStreamManager::setBGSDlMute(const bool mute_on) {
    if (mute_on) {
        mBGSDlGain = 0;
    } else {
        mBGSDlGain = 0xFF;
    }
    ALOGD("%s(), mute_on: %d, mBGSDlGain=0x%x", __FUNCTION__, mute_on, mBGSDlGain);

    return NO_ERROR;
}

status_t AudioALSAStreamManager::setBGSUlMute(const bool mute_on) {
    if (mute_on) {
        mBGSUlGain = 0;
    } else {
        mBGSUlGain = 0xFF;
    }
    ALOGD("%s(), mute_on: %d, mBGSUlGain=0x%x", __FUNCTION__, mute_on, mBGSUlGain);

    return NO_ERROR;
}

status_t AudioALSAStreamManager::setMode(audio_mode_t new_mode) {
    bool resumeAllStreamsAtSetMode = false;
    bool updateModeToStreamOut = false;
    int ret = 0;

    AL_AUTOLOCK(mStreamVectorLock);

    // check value
    if ((new_mode < AUDIO_MODE_NORMAL) || (new_mode > AUDIO_MODE_MAX)) {
        ALOGW("%s(), new_mode: %d is BAD_VALUE, return", __FUNCTION__, new_mode);
        return BAD_VALUE;
    }

    // TODO(Harvey): modem 1 / modem 2 check

    if (new_mode == mAudioMode) {
        ALOGW("%s(), mAudioMode: %d == %d, return", __FUNCTION__, mAudioMode, new_mode);
        return NO_ERROR;
    }

    // make sure voice wake up is closed before leaving normal mode
    if (new_mode != AUDIO_MODE_NORMAL) {
        mForceDisableVoiceWakeUpForSetMode = true;
        if (mAudioALSAVoiceWakeUpController->getVoiceWakeUpEnable() == true) {
            ALOGD("%s(), force close voice wake up", __FUNCTION__);
            mAudioALSAVoiceWakeUpController->setVoiceWakeUpEnable(false);
        }
    } else if (isModeInPhoneCall(mAudioMode) == true) {
        //check if any stream out is playing during leaving IN_CALL mode
        int DelayCount = 0;
        bool IsStreamActive;
        do {
            IsStreamActive = false;
            for (size_t i = 0; i < mStreamOutVector.size(); i++) {
                if (mStreamOutVector[i]->isOutPutStreamActive() == true) {
                    IsStreamActive = true;
                }
            }
            if (IsStreamActive) {
                usleep(20 * 1000);
                ALOGD_IF(mLogEnable, "%s(), delay 20ms x(%d) for active stream out playback", __FUNCTION__, DelayCount);
            }
            DelayCount++;
        } while ((DelayCount <= 10) && (IsStreamActive));
    }

    mPhoneWithVoip = false;
    mEnterPhoneCallMode = (isModeInPhoneCall(new_mode) == true) ? true : false;

    if (((isModeInPhoneCall(mAudioMode) == true) && (isModeInVoipCall(new_mode) == true)) ||  //Phone -> VOIP
        ((isModeInVoipCall(mAudioMode) == true) && (isModeInPhoneCall(new_mode) == true)) ||  //VOIP -> Phone
        ((mVoipToRingTone == true) && (isModeInPhoneCall(new_mode) == true))) {               //VOIP -> Ring Tone -> Phone
        mPhoneWithVoip = true;
        mVoipToRingTone = false;
    } else if ((isModeInVoipCall(mAudioMode) == true) && (new_mode == AUDIO_MODE_RINGTONE)) { //VOIP -> Ring Tone
        mVoipToRingTone = true;
    } else {
        mVoipToRingTone = false;
    }

    ALOGD("+%s(), mAudioMode: %d => %d, mPhoneCallControllerStatus = %d , mPhoneWithVoip : %d , mVoipToRingTone : %d",
          __FUNCTION__, mAudioMode, new_mode, mPhoneCallControllerStatus, mPhoneWithVoip, mVoipToRingTone);

    if (isModeInPhoneCall(new_mode) == true || isModeInPhoneCall(mAudioMode) == true ||
        isModeInVoipCall(new_mode)  == true || isModeInVoipCall(mAudioMode) == true) {
        setAllInputStreamsSuspend(true, true);
        standbyAllInputStreams(true);
        //Need to reset MicInverse when phone/VOIP call
        AudioALSAHardwareResourceManager::getInstance()->setMicInverse(0);

        if ((mAudioMode != AUDIO_MODE_IN_CALL && // non-phone call --> voip mode
             new_mode == AUDIO_MODE_IN_COMMUNICATION) ||
            (mAudioMode == AUDIO_MODE_IN_COMMUNICATION && // leave voip, not enter phone call, and not 2->3->0
             isModeInPhoneCall(new_mode) == false &&
             mPhoneCallControllerStatus == false)) {
            mIsNeedResumeStreamOut = false;
            updateModeToStreamOut = true;
        } else {
            setAllOutputStreamsSuspend(true, true);
            standbyAllOutputStreams(true);
            mIsNeedResumeStreamOut = true;
        }

        /* Only change mode to non-call need resume streams at the end of setMode().
           Otherwise, resume streams when get the routing command. */
        /* Not use isModeInPhoneCall() because 2->3 case need to resume in routing command.*/
        if (new_mode == AUDIO_MODE_IN_CALL) {
            mResumeAllStreamsAtRouting = true;
        } else {
            resumeAllStreamsAtSetMode = true;
        }
    }

    // TODO(Harvey): // close mATV when mode swiching

    {
        AL_AUTOLOCK(mLock);
        AL_AUTOLOCK(mAudioModeLock);

        // use primary stream out device // TODO(Harvey): add a function? get from hardware?
#ifdef FORCE_ROUTING_RECEIVER
        const audio_devices_t current_output_devices = AUDIO_DEVICE_OUT_EARPIECE;
#else
        const audio_devices_t current_output_devices = (mStreamOutVector.size() > 0)
                                                       ? mStreamOutVector[0]->getStreamAttribute()->output_devices
                                                       : AUDIO_DEVICE_NONE;
#endif

        // close previous call if needed
        /*if ((isModeInPhoneCall(mAudioMode) == true)
            && (isModeInPhoneCall(new_mode) == false))*/
        if ((isModeInPhoneCall(new_mode) == false) && (mPhoneCallControllerStatus == true)) {
            mSpeechPhoneCallController->close();
            mPhoneCallControllerStatus = false;
            ALOGD("%s(), force unmute mic after phone call closed", __FUNCTION__);
            mSpeechPhoneCallController->setMicMute(false);
        }
        // open next call if needed
        //if (isModeInPhoneCall(new_mode) == true)
        if ((isModeInPhoneCall(new_mode) == true) && (mPhoneCallControllerStatus == false)) {
            mPhoneCallSpeechOpen = true;
        } else {
            mPhoneCallSpeechOpen = false;
        }

        mAudioMode = new_mode;

        if (isModeInPhoneCall() != true) {
            mAudioALSAVolumeController->setMasterVolume(mAudioALSAVolumeController->getMasterVolume(),
                                                        getModeForGain(), current_output_devices);
        }

        // make sure voice wake up is resume when go back to normal mode
        if (mAudioMode == AUDIO_MODE_NORMAL) {
            mForceDisableVoiceWakeUpForSetMode = false;
            if (mVoiceWakeUpNeedOn == true &&
                mStreamInVector.size() == 0) {
                ALOGD("%s(), resume voice wake up", __FUNCTION__);
                ASSERT(mAudioALSAVoiceWakeUpController->getVoiceWakeUpEnable() == false); // TODO(Harvey): double check, remove it later
                mAudioALSAVoiceWakeUpController->setVoiceWakeUpEnable(true);
            }
        }
    }

    // update audio mode to stream out if not suspend/standby streamout
    if (updateModeToStreamOut) {
        for (size_t i = 0; i < mStreamOutVector.size(); i++) {
            ret = mStreamOutVector[i]->updateAudioMode(mAudioMode);
            ASSERT(ret == 0);
        }
    }

    if (resumeAllStreamsAtSetMode == true) {
        if (mIsNeedResumeStreamOut) {
            mIsNeedResumeStreamOut = false;
            setAllOutputStreamsSuspend(false, true);
        }
        setAllInputStreamsSuspend(false, true);
    }

#if defined(MTK_HYBRID_NLE_SUPPORT)
    AudioALSAHyBridNLEManager::getInstance()->setAudioMode(mAudioMode);
#endif

    ALOGD("-%s(), mAudioMode = %d, mPhoneCallSpeechOpen = %d, mResumeAllStreamsAtRouting = %d, resumeAllStreamsAtSetMode = %d",
          __FUNCTION__, mAudioMode, mPhoneCallSpeechOpen, mResumeAllStreamsAtRouting, resumeAllStreamsAtSetMode);

    return NO_ERROR;
}

audio_mode_t AudioALSAStreamManager::getMode() {
    AL_AUTOLOCK(mAudioModeLock);
    ALOGD("%s(), mAudioMode = %d", __FUNCTION__, mAudioMode);

    return mAudioMode;
}

status_t AudioALSAStreamManager::syncSharedOutDevice(audio_devices_t routingSharedOutDevice,
                                                     AudioALSAStreamOut *currentStreamOut) {
    ALOGD("+%s(), routingSharedOutDevice: %d",
          __FUNCTION__, routingSharedOutDevice);
    AL_AUTOLOCK(mLock);

    status_t status = NO_ERROR;
    Vector<AudioALSAStreamOut *> streamOutToRoute;

    // Check if shared device
    AudioALSAHardwareResourceManager *hwResMng = AudioALSAHardwareResourceManager::getInstance();
    if (!hwResMng->isSharedOutDevice(routingSharedOutDevice)) {
        ALOGD("-%s(), this stream out is not shared out device, return.", __FUNCTION__);
        return NO_ERROR;
    }

    // suspend before routing & check which streamout need routing
    for (size_t i = 0; i < mStreamOutVector.size(); i++) {
        if (isOutputNeedRouting(mStreamOutVector[i], currentStreamOut, routingSharedOutDevice)) {
            mStreamOutVector[i]->setSuspend(true);
            streamOutToRoute.add(mStreamOutVector[i]);
        }
    }

    // routing
    for (size_t i = 0; i < streamOutToRoute.size(); i++) {
        status = streamOutToRoute[i]->routing(routingSharedOutDevice);
        ASSERT(status == NO_ERROR);
        if (streamOutToRoute[i] != currentStreamOut) {
            streamOutToRoute[i]->setMuteForRouting(true);
        }
    }

    // resume suspend
    for (size_t i = 0; i < streamOutToRoute.size(); i++) {
        streamOutToRoute[i]->setSuspend(false);
    }

    if (streamOutToRoute.size() > 0) {
        updateOutputDeviceForAllStreamIn_l(routingSharedOutDevice);

        // volume control
        if (!isModeInPhoneCall()) {
            mAudioALSAVolumeController->setMasterVolume(mAudioALSAVolumeController->getMasterVolume(),
                                                        getModeForGain(), routingSharedOutDevice);
        }
    }

    ALOGV("-%s()", __FUNCTION__);
    return status;
}

bool AudioALSAStreamManager::isOutputNeedRouting(AudioALSAStreamOut *eachStreamOut,
                                                 AudioALSAStreamOut *currentStreamOut,
                                                 audio_devices_t routingSharedOutDevice) {
    audio_devices_t streamOutDevice = eachStreamOut->getStreamAttribute()->output_devices;
    bool isSharedStreamOutDevice = AudioALSAHardwareResourceManager::getInstance()->isSharedOutDevice(streamOutDevice);
    bool isSharedRoutingDevice = AudioALSAHardwareResourceManager::getInstance()->isSharedOutDevice(routingSharedOutDevice);

    if (streamOutDevice == routingSharedOutDevice) {
        return false;
    }

    if (eachStreamOut->isOutPutStreamActive()) {

        // active currentStreamOut always need routing
        if (currentStreamOut == eachStreamOut) {
            return true;
        }

        if (isSharedStreamOutDevice && isSharedRoutingDevice) {
            return true;
        }
    }

    return false;
}

status_t AudioALSAStreamManager::DeviceNoneUpdate() {
    ALOGD("+%s()", __FUNCTION__);
    AL_AUTOLOCK(mLock);
    status_t status = NO_ERROR;

    // update the output device info for voice wakeup (even when "routing=0")
    mAudioALSAVoiceWakeUpController->updateDeviceInfoForVoiceWakeUp();
    ALOGD("-%s()", __FUNCTION__);

    return status;
}

status_t AudioALSAStreamManager::routingOutputDevice(AudioALSAStreamOut *pAudioALSAStreamOut, const audio_devices_t current_output_devices, audio_devices_t output_devices) {
    AL_AUTOLOCK(mLock);

    status_t status = NO_ERROR;
    audio_devices_t streamOutDevice = pAudioALSAStreamOut->getStreamAttribute()->output_devices;

    // TODO(Harvey, Hochi): Sometimes AUDIO_DEVICE_NONE might need to transferred to other device?

    // set original routing device to TTY
    mSpeechPhoneCallController->setRoutingForTty((audio_devices_t)output_devices);

    // update the output device info for voice wakeup (even when "routing=0")
    mAudioALSAVoiceWakeUpController->updateDeviceInfoForVoiceWakeUp();

    // When FM + (WFD, A2DP, SCO(44.1K -> 8/16K), ...), Policy will routing to AUDIO_DEVICE_NONE
    // Hence, use other device like AUDIO_DEVICE_OUT_REMOTE_SUBMIX instead to achieve FM routing.
    if (output_devices == AUDIO_DEVICE_NONE && mFMController->getFmEnable() == true) {
        ALOGD("%s(), flag: %d, Replace AUDIO_DEVICE_NONE with AUDIO_DEVICE_OUT_REMOTE_SUBMIX for AP-path FM routing",
              __FUNCTION__, pAudioALSAStreamOut->getStreamAttribute()->mAudioOutputFlags);
        output_devices = AUDIO_DEVICE_OUT_REMOTE_SUBMIX;
    }

    if (output_devices == AUDIO_DEVICE_NONE) {
        ALOGW("%s(), flag: %d, output_devices == AUDIO_DEVICE_NONE(0x%x), return",
              __FUNCTION__, pAudioALSAStreamOut->getStreamAttribute()->mAudioOutputFlags, AUDIO_DEVICE_NONE);
        return NO_ERROR;
    } else if (output_devices == streamOutDevice) {
        if ((mPhoneCallSpeechOpen == true) || (mResumeAllStreamsAtRouting == true)) {
            ALOGD("+%s(), flag: %d, output_devices = current_devices(0x%x), mPhoneCallSpeechOpen = %d, mResumeAllStreamsAtRouting = %d",
                  __FUNCTION__, pAudioALSAStreamOut->getStreamAttribute()->mAudioOutputFlags,
                  current_output_devices, mPhoneCallSpeechOpen, mResumeAllStreamsAtRouting);
        }
#ifdef MTK_AUDIO_TTY_SPH_ENH_SUPPORT
        else if ((isModeInPhoneCall() == true) && (mSpeechPhoneCallController->checkTtyNeedOn() == true)) {
             ALOGW("+%s(), flag: %d, output_devices == current_output_devices(0x%x), but TTY call is enabled",
                  __FUNCTION__, pAudioALSAStreamOut->getStreamAttribute()->mAudioOutputFlags, streamOutDevice);
        }
#endif
        else {
             ALOGW("%s(), flag: %d, mPhoneCallSpeechOpen = %d, output_devices == current_output_devices(0x%x), return",
                  __FUNCTION__, pAudioALSAStreamOut->getStreamAttribute()->mAudioOutputFlags,
                  mPhoneCallSpeechOpen, streamOutDevice);
            return NO_ERROR;
        }
    } else {
            ALOGD("+%s(), flag: %d, output_devices: 0x%x => 0x%x, mPhoneCallSpeechOpen = %d",
                  __FUNCTION__, pAudioALSAStreamOut->getStreamAttribute()->mAudioOutputFlags,
                  streamOutDevice, output_devices, mPhoneCallSpeechOpen);
    }

    // close FM when mode swiching
    if (mFMController->getFmEnable() &&
        (mPhoneCallSpeechOpen)) {
        //setFmEnable(false);
        mFMController->setFmEnable(false, current_output_devices, false, false, true);
    }


    // do routing
    if (isModeInPhoneCall() &&
        pAudioALSAStreamOut->getStreamAttribute()->mAudioOutputFlags & AUDIO_OUTPUT_FLAG_PRIMARY) {
        bool checkrouting = CheckStreaminPhonecallRouting(mSpeechPhoneCallController->getInputDeviceForPhoneCall(output_devices), false);
        bool isFirstRoutingInCall = false;

        if (mPhoneCallSpeechOpen == true) {
#ifdef FORCE_ROUTING_RECEIVER
            output_devices = AUDIO_DEVICE_OUT_EARPIECE;
#endif
            mSpeechPhoneCallController->open(
                mAudioMode,
                output_devices,
                mSpeechPhoneCallController->getInputDeviceForPhoneCall(output_devices));
#ifdef SPEECH_PMIC_RESET
            mSpeechPhoneCallController->StartPMIC_Reset();
#endif

            mPhoneCallSpeechOpen = false;
            isFirstRoutingInCall = true;
            mPhoneCallControllerStatus = true;
        }

        if (output_devices == streamOutDevice) {
            if (mSpeechPhoneCallController->checkTtyNeedOn() == true) {
                ALOGW("-%s(), output_devices == current_output_devices(0x%x), but TTY call is enabled",
                      __FUNCTION__, current_output_devices);
                mSpeechPhoneCallController->routing(
                    output_devices,
                    mSpeechPhoneCallController->getInputDeviceForPhoneCall(output_devices));
            }
        } else if (!isFirstRoutingInCall) {
            mSpeechPhoneCallController->routing(
                output_devices,
                mSpeechPhoneCallController->getInputDeviceForPhoneCall(output_devices));
        }
#ifdef SPEECH_PMIC_RESET
        mSpeechPhoneCallController->StartPMIC_Reset();
#endif

        //Need to resume the streamin
        if (checkrouting == true) {
            CheckStreaminPhonecallRouting(mSpeechPhoneCallController->getInputDeviceForPhoneCall(output_devices), true);
        }

        // volume control
        mAudioALSAVolumeController->setVoiceVolume(mAudioALSAVolumeController->getVoiceVolume(),
                                                   getModeForGain(), output_devices);

        for (size_t i = 0; i < mStreamOutVector.size(); i++) {
            mStreamOutVector[i]->syncPolicyDevice();
        }
    }

    if (mResumeAllStreamsAtRouting == true) {
        setAllStreamsSuspend(false, true);
        mResumeAllStreamsAtRouting = false;
    }


    Vector<AudioALSAStreamOut *> streamOutToRoute;

    // Check if non active streamout device
    if (!pAudioALSAStreamOut->isOutPutStreamActive()) {
        ALOGD("-%s(), stream out not active, route itself and return", __FUNCTION__);
        pAudioALSAStreamOut->routing(output_devices);
        return NO_ERROR;
    }

    // suspend before routing & check if other streamouts need routing
    for (size_t i = 0; i < mStreamOutVector.size(); i++) {
        if (isOutputNeedRouting(mStreamOutVector[i], pAudioALSAStreamOut, output_devices)) {
            mStreamOutVector[i]->setSuspend(true);
            streamOutToRoute.add(mStreamOutVector[i]);
        }
    }

    // routing
    for (size_t i = 0; i < streamOutToRoute.size(); i++) {
        status = streamOutToRoute[i]->routing(output_devices);
        ASSERT(status == NO_ERROR);
        if (streamOutToRoute[i] != pAudioALSAStreamOut) {
            streamOutToRoute[i]->setMuteForRouting(true);
        }
    }

    // resume suspend
    for (size_t i = 0; i < streamOutToRoute.size(); i++) {
        streamOutToRoute[i]->setSuspend(false);
    }

    if (streamOutToRoute.size() > 0) {
        updateOutputDeviceForAllStreamIn_l(output_devices);

        // volume control
        if (!isModeInPhoneCall()) {
            mAudioALSAVolumeController->setMasterVolume(mAudioALSAVolumeController->getMasterVolume(),
                                                        getModeForGain(), output_devices);
        }
    }

    ALOGD("-%s(), output_devices = 0x%x", __FUNCTION__, output_devices);
    return status;
}


status_t AudioALSAStreamManager::routingInputDevice(AudioALSAStreamIn *pAudioALSAStreamIn, const audio_devices_t current_input_device, audio_devices_t input_device) {
    ALOGD("+%s(), input_device: 0x%x => 0x%x", __FUNCTION__, current_input_device, input_device);
    AL_AUTOLOCK(mLock);

    status_t status = NO_ERROR;

    bool sharedDevice = (input_device & ~AUDIO_DEVICE_BIT_IN) & (AUDIO_DEVICE_IN_BUILTIN_MIC | AUDIO_DEVICE_IN_BACK_MIC | AUDIO_DEVICE_IN_WIRED_HEADSET);
    //In PhonecallMode and the new input_device / phonecall_device are both sharedDevice,we may change the input_device = phonecall_device
    if ((isModeInPhoneCall() == true) && (sharedDevice == true)) {
        audio_devices_t phonecall_device = mSpeechPhoneCallController->getPhonecallInputDevice();
        sharedDevice = (phonecall_device & ~AUDIO_DEVICE_BIT_IN) & (AUDIO_DEVICE_IN_BUILTIN_MIC | AUDIO_DEVICE_IN_BACK_MIC | AUDIO_DEVICE_IN_WIRED_HEADSET);
        if (sharedDevice == true) {
            input_device = phonecall_device;
        }
        ALOGD("+%s(), isModeInPhoneCall, input_device = 0x%x", __FUNCTION__, input_device);
    } else if ((sharedDevice == true) && (mStreamInVector.size() > 1)) {
        input_device = CheckInputDevicePriority(input_device);
    }

    if (input_device == AUDIO_DEVICE_NONE) {
        ALOGW("-%s(), input_device == AUDIO_DEVICE_NONE(0x%x), return", __FUNCTION__, AUDIO_DEVICE_NONE);
        return NO_ERROR;
    } else if (input_device == current_input_device) {
        ALOGW("-%s(), input_device == current_input_device(0x%x), return", __FUNCTION__, current_input_device);
        return NO_ERROR;
    }

    if (mStreamInVector.size() > 0) {
        for (size_t i = 0; i < mStreamInVector.size(); i++) {
            if ((input_device == AUDIO_DEVICE_IN_FM_TUNER) || (current_input_device == AUDIO_DEVICE_IN_FM_TUNER)) {
                if (pAudioALSAStreamIn == mStreamInVector[i]) {
                    status = mStreamInVector[i]->routing(input_device);
                    ASSERT(status == NO_ERROR);
                }
            } else {
                status = mStreamInVector[i]->routing(input_device);
                ASSERT(status == NO_ERROR);
            }
        }
    }
    return status;
}

// check if headset has changed
bool AudioALSAStreamManager::CheckHeadsetChange(const audio_devices_t current_output_devices, audio_devices_t output_device) {
    ALOGD("+%s(), current_output_devices = %d output_device = %d ", __FUNCTION__, current_output_devices, output_device);
    if (current_output_devices == output_device) {
        return false;
    }
    if (current_output_devices == AUDIO_DEVICE_NONE || output_device == AUDIO_DEVICE_NONE) {
        return true;
    }
    if (current_output_devices == AUDIO_DEVICE_OUT_WIRED_HEADSET || current_output_devices == AUDIO_DEVICE_OUT_WIRED_HEADPHONE
        || output_device == AUDIO_DEVICE_OUT_WIRED_HEADSET || output_device == AUDIO_DEVICE_OUT_WIRED_HEADPHONE) {
        return true;
    }
    return false;
}

status_t AudioALSAStreamManager::setFmEnable(const bool enable, bool bForceControl, bool bForce2DirectConn, audio_devices_t output_device) { // TODO(Harvey)
    //AL_AUTOLOCK(mLock);

    // Reject set fm enable during phone call mode
    if (isModeInPhoneCall(mAudioMode)) {
        ALOGW("-%s(), mAudioMode(%d) is in phone call mode, return.", __FUNCTION__, mAudioMode);
        return INVALID_OPERATION;
    }

    // use primary stream out device // TODO(Harvey): add a function? get from hardware?
    const audio_devices_t current_output_devices = output_device == AUDIO_DEVICE_NONE ? ((mStreamOutVector.size() > 0)
                                                                                         ? mStreamOutVector[0]->getStreamAttribute()->output_devices
                                                                                         : AUDIO_DEVICE_NONE) : output_device;

    mFMController->setFmEnable(enable, current_output_devices, bForceControl, bForce2DirectConn);
    return NO_ERROR;
}

status_t AudioALSAStreamManager::setHdmiEnable(const bool enable) { // TODO(George): tmp, add a class to do it
    ALOGD("+%s(), enable = %d", __FUNCTION__, enable);
    AL_AUTOLOCK(mLock);
    AL_AUTOLOCK(*AudioALSADriverUtility::getInstance()->getStreamSramDramLock());

    if (enable == mHdmiEnable) {
        return ALREADY_EXISTS;
    }
    mHdmiEnable = enable;

    if (enable == true) {
        int pcmIdx = AudioALSADeviceParser::getInstance()->GetPcmIndexByString(keypcmI2S0Dl1Playback);
        int cardIdx = AudioALSADeviceParser::getInstance()->GetCardIndexByString(keypcmI2S0Dl1Playback);

        // DL loopback setting
        mLoopbackConfig.channels = 2;
        mLoopbackConfig.rate = 44100;
        mLoopbackConfig.period_size = 512;
        mLoopbackConfig.period_count = 4;
        mLoopbackConfig.format = PCM_FORMAT_S32_LE;
        mLoopbackConfig.start_threshold = 0;
        mLoopbackConfig.stop_threshold = 0;
        mLoopbackConfig.silence_threshold = 0;
        if (mHdmiPcm == NULL) {
            mHdmiPcm = pcm_open(cardIdx, pcmIdx, PCM_OUT, &mLoopbackConfig);
            ALOGD("pcm_open mHdmiPcm = %p", mHdmiPcm);
        }
        if (!mHdmiPcm || !pcm_is_ready(mHdmiPcm)) {
            ALOGD("Unable to open mHdmiPcm device %u (%s)", pcmIdx, pcm_get_error(mHdmiPcm));
        }

        ALOGD("pcm_start(mHdmiPcm)");
        pcm_start(mHdmiPcm);
    } else {
        ALOGD("pcm_close");
        if (mHdmiPcm != NULL) {
            pcm_close(mHdmiPcm);
            mHdmiPcm = NULL;
        }
        ALOGD("pcm_close done");
    }


    ALOGD("-%s(), enable = %d", __FUNCTION__, enable);
    return NO_ERROR;
}

status_t AudioALSAStreamManager::setLoopbackEnable(const bool enable) { // TODO(Harvey): tmp, add a class to do it
    ALOGD("+%s(), enable = %d", __FUNCTION__, enable);
    AL_AUTOLOCK(mLock);
    AL_AUTOLOCK(*AudioALSADriverUtility::getInstance()->getStreamSramDramLock());

    if (enable == mLoopbackEnable) {
        return ALREADY_EXISTS;
    }
    mLoopbackEnable = enable;

    if (enable == true) {
        int pcmIdx = AudioALSADeviceParser::getInstance()->GetPcmIndexByString(keypcmUlDlLoopback);
        int cardIdx = AudioALSADeviceParser::getInstance()->GetCardIndexByString(keypcmUlDlLoopback);

        // DL loopback setting
        mLoopbackConfig.channels = 2;
        mLoopbackConfig.rate = 48000;
        mLoopbackConfig.period_size = 512;
        mLoopbackConfig.period_count = 4;
        mLoopbackConfig.format = PCM_FORMAT_S16_LE;
        mLoopbackConfig.start_threshold = 0;
        mLoopbackConfig.stop_threshold = 0;
        mLoopbackConfig.silence_threshold = 0;
        if (mLoopbackPcm == NULL) {
            mLoopbackPcm = pcm_open(cardIdx, pcmIdx, PCM_OUT, &mLoopbackConfig);
            ALOGD("pcm_open mLoopbackPcm = %p", mLoopbackPcm);
        }
        if (!mLoopbackPcm || !pcm_is_ready(mLoopbackPcm)) {
            ALOGD("Unable to open mLoopbackPcm device %u (%s)", pcmIdx, pcm_get_error(mLoopbackPcm));
        }

        ALOGD("pcm_start(mLoopbackPcm)");
        pcm_start(mLoopbackPcm);

        //UL loopback setting
        mLoopbackUlConfig.channels = 2;
        mLoopbackUlConfig.rate = 48000;
        mLoopbackUlConfig.period_size = 512;
        mLoopbackUlConfig.period_count = 4;
        mLoopbackUlConfig.format = PCM_FORMAT_S16_LE;
        mLoopbackUlConfig.start_threshold = 0;
        mLoopbackUlConfig.stop_threshold = 0;
        mLoopbackUlConfig.silence_threshold = 0;
        if (mLoopbackUlPcm == NULL) {
            mLoopbackUlPcm = pcm_open(cardIdx, pcmIdx, PCM_IN, &mLoopbackUlConfig);
            ALOGD("pcm_open mLoopbackPcm = %p", mLoopbackUlPcm);
        }
        if (!mLoopbackUlPcm || !pcm_is_ready(mLoopbackUlPcm)) {
            ALOGD("Unable to open mLoopbackUlPcm device %u (%s)", pcmIdx, pcm_get_error(mLoopbackUlPcm));
        }
        ALOGD("pcm_start(mLoopbackUlPcm)");
        pcm_start(mLoopbackUlPcm);
    } else {
        ALOGD("pcm_close");
        if (mLoopbackPcm != NULL) {
            pcm_close(mLoopbackPcm);
            mLoopbackPcm = NULL;
        }
        if (mLoopbackUlPcm != NULL) {
            pcm_close(mLoopbackUlPcm);
            mLoopbackUlPcm = NULL;
        }
        ALOGD("pcm_close done");
    }


    ALOGD("-%s(), enable = %d", __FUNCTION__, enable);
    return NO_ERROR;
}

bool AudioALSAStreamManager::getFmEnable() {
    AL_AUTOLOCK(mLock);
    return mFMController->getFmEnable();
}

status_t AudioALSAStreamManager::setAllOutputStreamsSuspend(const bool suspend_on, const bool setModeRequest __unused) {
    for (size_t i = 0; i < mStreamOutVector.size(); i++) {
        ASSERT(mStreamOutVector[i]->setSuspend(suspend_on) == NO_ERROR);
    }

    return NO_ERROR;
}

status_t AudioALSAStreamManager::setAllInputStreamsSuspend(const bool suspend_on, const bool setModeRequest, const capture_handler_t caphandler) {
    ALOGV("%s()", __FUNCTION__);

    status_t status = NO_ERROR;

    AudioALSAStreamIn *pAudioALSAStreamIn = NULL;

    for (size_t i = 0; i < mStreamInVector.size(); i++) {
        pAudioALSAStreamIn = mStreamInVector[i];

        if ((setModeRequest == true) && (mEnterPhoneCallMode == true) && (mStreamInVector[i]->getStreamInCaptureHandler() != NULL)) {
            //No need to do reopen when mode change
            if ((pAudioALSAStreamIn->isSupportConcurrencyInCall()) == true) {
                ALOGD("%s(), Enter phone call mode, mStreamInVector[%zu] support concurrency!!", __FUNCTION__, i);
                continue;
            }
        }

        if (pAudioALSAStreamIn->getStreamInCaptureHandler() == NULL) {
            ALOGD("%s(), this streamin does not have capture handler, just set suspend", __FUNCTION__);
            status = pAudioALSAStreamIn->setSuspend(suspend_on);
            continue;
        }
        if (pAudioALSAStreamIn->getCaptureHandlerType() & caphandler) {
            ALOGD("%s(), find corresponding streamin, suspend it", __FUNCTION__);
            status = pAudioALSAStreamIn->setSuspend(suspend_on);
        }

        if (status != NO_ERROR) {
            ALOGE("%s(), mStreamInVector[%zu] setSuspend() fail!!", __FUNCTION__, i);
        }
    }

    ALOGV("%s()-", __FUNCTION__);
    return status;
}

status_t AudioALSAStreamManager::setAllStreamsSuspend(const bool suspend_on, const bool setModeRequest) {
    ALOGD("%s(), suspend_on = %d", __FUNCTION__, suspend_on);

    status_t status = NO_ERROR;

    status = setAllOutputStreamsSuspend(suspend_on, setModeRequest);
    status = setAllInputStreamsSuspend(suspend_on, setModeRequest);

    return status;
}


status_t AudioALSAStreamManager::standbyAllOutputStreams(const bool setModeRequest __unused) {
    ALOGD_IF(mLogEnable, "%s()", __FUNCTION__);
    status_t status = NO_ERROR;

    AudioALSAStreamOut *pAudioALSAStreamOut = NULL;

    for (size_t i = 0; i < mStreamOutVector.size(); i++) {
        pAudioALSAStreamOut = mStreamOutVector[i];
        status = pAudioALSAStreamOut->standbyStreamOut();
        if (status != NO_ERROR) {
            ALOGE("%s(), mStreamOutVector[%zu] standbyStreamOut() fail!!", __FUNCTION__, i);
        }
    }

    return status;
}

status_t AudioALSAStreamManager::standbyAllInputStreams(const bool setModeRequest, capture_handler_t caphandler) {
    ALOGD_IF(mLogEnable, "%s()", __FUNCTION__);
    status_t status = NO_ERROR;

    AudioALSAStreamIn *pAudioALSAStreamIn = NULL;

    for (size_t i = 0; i < mStreamInVector.size(); i++) {
        pAudioALSAStreamIn = mStreamInVector[i];

        if ((setModeRequest == true) && (mEnterPhoneCallMode == true) && (mStreamInVector[i]->getStreamInCaptureHandler() != NULL)) {
            //No need to do reopen when mode change
            if ((pAudioALSAStreamIn->isSupportConcurrencyInCall()) == true) {
                ALOGD("%s(), Enter phone call mode, mStreamInVector[%zu] support concurrency!!", __FUNCTION__, i);
                continue;
            }
        }

        if (pAudioALSAStreamIn->getStreamInCaptureHandler() == NULL) {
            ALOGD("%s(), mStreamInVector[%zu] capture handler not created yet, pAudioALSAStreamIn=%p, this=%p", __FUNCTION__, i, pAudioALSAStreamIn, this);
            continue;
        }

        if ((pAudioALSAStreamIn->getStreamInCaptureHandler() != NULL) && (pAudioALSAStreamIn->getStreamInCaptureHandler()->getCaptureHandlerType() & caphandler)) {
            ALOGD("%s(), find corresponding streamin, standby it", __FUNCTION__);
            status = pAudioALSAStreamIn->standby();
        }

        if (status != NO_ERROR) {
            ALOGE("%s(), mStreamInVector[%zu] standby() fail!!", __FUNCTION__, i);
        }
    }

    ALOGV("%s()-", __FUNCTION__);
    return status;
}

status_t AudioALSAStreamManager::standbyAllStreams(const bool setModeRequest) {
    ALOGD_IF("%s()", __FUNCTION__);

    status_t status = NO_ERROR;

    status = standbyAllOutputStreams(setModeRequest);
    status = standbyAllInputStreams(setModeRequest);

    return status;
}

bool AudioALSAStreamManager::setHiFiStatus(bool enable) {

    ALOGD("%s(), enable = %d", __FUNCTION__, enable);
    property_set(PROPERTY_KEY_HIFI_DAC_STATE, (enable == false) ? "0" : "1");
    return true;
}

bool AudioALSAStreamManager::getHiFiStatus() {
    bool ret = false;
    char property_value[PROPERTY_VALUE_MAX];
    property_get(PROPERTY_KEY_HIFI_DAC_STATE, property_value, "0"); //"0": default off
    ret = (property_value[0] == '0') ? false : true;;
    return ret;
}

int AudioALSAStreamManager::setAllStreamHiFi(AudioALSAStreamOut *pAudioALSAStreamOut, uint32_t sampleRate) {
    int status = 0;
    const audio_devices_t streamOutDevice = pAudioALSAStreamOut->getStreamAttribute()->output_devices;
    ALOGD("%s(), previous mHiFiEnable = %s sampleRate = %u",
          __FUNCTION__, mHiFiEnable ? "true" : "false", sampleRate);
#if defined(MTK_HIFIAUDIO_SUPPORT)
    AL_AUTOLOCK(mLock);
    /* HIHI enable*/
    if (sampleRate > AUDIO_HIFI_RATE_MIN) {
        if (!mHiFiEnable) {
            ALOGD("%s(), enable HiFi, first suspend all streams", __FUNCTION__);

            // close FM when mode swiching
            if (mFMController->getFmEnable()) {
#ifndef FM_HIFI_NOT_CONCURRENT
                mFMController->setFmEnable(false, streamOutDevice, false, false, true);
#else
                sampleRate = AudioALSASampleRateController::getInstance()->getPrimaryStreamOutSampleRate();
#endif
            }

            setAllStreamsSuspend(true, true);
            standbyAllStreams(true);

            for (size_t i = 0; i < mStreamOutVector.size(); i++) {
                mStreamOutVector[i]->setStreamOutSampleRate(sampleRate);
            }

            // Update sample rate
            AudioALSASampleRateController::getInstance()->setPrimaryStreamOutSampleRate(sampleRate);

            setAllStreamsSuspend(false, true);
            mHiFiEnable = true;
        }

    } else {
        ALOGD("%s(), sampleRate < %d, disable HiFi", __FUNCTION__, AUDIO_HIFI_RATE_MIN);

        if (mHiFiEnable == true) {
            ALOGD("%s(), HIFI disable frome enable state. disable all stream", __FUNCTION__);

            // close FM when mode swiching
            if (mFMController->getFmEnable()) {
#ifndef FM_HIFI_NOT_CONCURRENT
                mFMController->setFmEnable(false, streamOutDevice, false, false, true);
#endif
            }

            setAllStreamsSuspend(true, true);
            standbyAllStreams(true);

            for (size_t i = 0; i < mStreamOutVector.size(); i++) {
                mStreamOutVector[i]->setStreamOutSampleRate(sampleRate);
            }

            // Update sample rate
            AudioALSASampleRateController::getInstance()->setPrimaryStreamOutSampleRate(sampleRate);

            setAllStreamsSuspend(false, true);
            mHiFiEnable = false;
        }
    }
    setHiFiStatus(mHiFiEnable);
#else
    status = -ENOSYS;
    ALOGD("%s(),status = %d", __FUNCTION__, status);
#endif
    return status;
}

audio_devices_t AudioALSAStreamManager::CheckInputDevicePriority(audio_devices_t input_device) {
    for (size_t i = 0; i < mStreamInVector.size(); i++) {
        if (setUsedDevice(input_device) == 0) {
            break;
        }
        audio_devices_t old_device = mStreamInVector[i]->getStreamAttribute()->input_device;
        bool sharedDevice = (old_device & ~AUDIO_DEVICE_BIT_IN) & (AUDIO_DEVICE_IN_BUILTIN_MIC | AUDIO_DEVICE_IN_BACK_MIC | AUDIO_DEVICE_IN_WIRED_HEADSET);
        if (sharedDevice == false) {
            continue;
        }
        if ((old_device != input_device) && (setUsedDevice(old_device) < setUsedDevice(input_device))) {
            input_device = old_device;
        }
    }
    ALOGD("%s(),input_device = 0x%x", __FUNCTION__, input_device);
    return input_device;
}

uint32_t AudioALSAStreamManager::setUsedDevice(const audio_devices_t used_device) {
    uint32_t usedInputDeviceIndex = 0;
    switch (used_device) {
    case AUDIO_DEVICE_IN_BUILTIN_MIC: {
        usedInputDeviceIndex = 0;
        break;
    }
    case AUDIO_DEVICE_IN_WIRED_HEADSET: {
        usedInputDeviceIndex = 1;
        break;
    }
    case AUDIO_DEVICE_IN_BACK_MIC: {
        usedInputDeviceIndex = 2;
        break;
    }
    }
    return usedInputDeviceIndex;
}

bool AudioALSAStreamManager::CheckStreaminPhonecallRouting(audio_devices_t new_phonecall_device, bool checkrouting) {
    if (checkrouting == true) { //Already Routing, Need to resume streamin
        setAllInputStreamsSuspend(false, false);
    } else { //Need to check the streamin to do routing
        bool newsharedDevice = ((new_phonecall_device & ~AUDIO_DEVICE_BIT_IN) & (AUDIO_DEVICE_IN_BUILTIN_MIC | AUDIO_DEVICE_IN_BACK_MIC | AUDIO_DEVICE_IN_WIRED_HEADSET));
        if ((mStreamInVector.size() > 0) && (newsharedDevice == true)) {
            status_t status = NO_ERROR;
            bool oldsharedDevice = 0;
            audio_devices_t old_device;
            for (size_t i = 0; i < mStreamInVector.size(); i++) {
                old_device = mStreamInVector[i]->getStreamAttribute()->input_device;
                oldsharedDevice = (old_device & ~AUDIO_DEVICE_BIT_IN) & (AUDIO_DEVICE_IN_BUILTIN_MIC | AUDIO_DEVICE_IN_BACK_MIC | AUDIO_DEVICE_IN_WIRED_HEADSET);
                if ((oldsharedDevice == true) && (old_device != new_phonecall_device)) {
                    if (checkrouting == false) {
                        setAllInputStreamsSuspend(true, false);
                        standbyAllInputStreams(false);
                        checkrouting = true;
                    }
                    ALOGD("+%s(),old_device = 0x%x -> new_phonecall_device = 0x%x", __FUNCTION__, oldsharedDevice, new_phonecall_device);
                    status = mStreamInVector[i]->routing(new_phonecall_device);
                    ASSERT(status == NO_ERROR);
                }
            }
        }
    }
    return checkrouting;
}

bool AudioALSAStreamManager::getPhoncallOutputDevice() {
#ifdef FORCE_ROUTING_RECEIVER
    const audio_devices_t current_output_devices = AUDIO_DEVICE_OUT_EARPIECE;
#else
    const audio_devices_t current_output_devices = (mStreamOutVector.size() > 0)
                                                   ? mStreamOutVector[0]->getStreamAttribute()->output_devices
                                                   : AUDIO_DEVICE_NONE;
#endif
    ALOGD("%s(),current_output_devices = %d ", __FUNCTION__, current_output_devices);
    bool bt_device_on = audio_is_bluetooth_sco_device(current_output_devices);
    ALOGD("%s(),bt_device_on = %d ", __FUNCTION__, bt_device_on);
    return bt_device_on;
}

size_t AudioALSAStreamManager::getInputBufferSize(uint32_t sampleRate, audio_format_t format, uint32_t channelCount) {
    size_t wordSize = 0;
    switch (format) {
    case AUDIO_FORMAT_PCM_8_BIT: {
        wordSize = sizeof(int8_t);
        break;
    }
    case AUDIO_FORMAT_PCM_16_BIT: {
        wordSize = sizeof(int16_t);
        break;
    }
    case AUDIO_FORMAT_PCM_8_24_BIT:
    case AUDIO_FORMAT_PCM_32_BIT: {
        wordSize = sizeof(int32_t);
        break;
    }
    default: {
        ALOGW("%s(), wrong format(0x%x), default use wordSize = %zu", __FUNCTION__, format, sizeof(int16_t));
        wordSize = sizeof(int16_t);
        break;
    }
    }

    size_t bufferSize = ((sampleRate * channelCount * wordSize) * 20) / 1000; // TODO (Harvey): why 20 ms here?

    ALOGD("%s(), sampleRate = %u, format = 0x%x, channelCount = %d, bufferSize = %zu",
          __FUNCTION__, sampleRate, format, channelCount, bufferSize);
    return bufferSize;
}

status_t AudioALSAStreamManager::updateOutputDeviceForAllStreamIn_l(audio_devices_t output_devices) {
    status_t status = NO_ERROR;

    if (mStreamInVector.size() > 0) {
        // update the output device info for input stream
        // (ex:for BesRecord parameters update or mic device change)
        ALOGD("%s(), mStreamInVector.size() = %zu", __FUNCTION__, mStreamInVector.size());
        for (size_t i = 0; i < mStreamInVector.size(); i++) {
            status = mStreamInVector[i]->updateOutputDeviceInfoForInputStream(output_devices);
            ASSERT(status == NO_ERROR);
        }
    }

    return status;
}

status_t AudioALSAStreamManager::updateOutputDeviceForAllStreamIn(audio_devices_t output_devices)
{
    AL_AUTOLOCK(mLock);

    return updateOutputDeviceForAllStreamIn_l(output_devices);
}

// set musicplus to streamout
status_t AudioALSAStreamManager::SetMusicPlusStatus(bool bEnable) {

    for (size_t i = 0; i < mFilterManagerVector.size() ; i++) {
        AudioMTKFilterManager  *pTempFilter = mFilterManagerVector[i];
        pTempFilter->setParamFixed(bEnable ? true : false);
    }

    return NO_ERROR;
}

bool AudioALSAStreamManager::GetMusicPlusStatus() {

    for (size_t i = 0; i < mFilterManagerVector.size() ; i++) {
        AudioMTKFilterManager  *pTempFilter = mFilterManagerVector[i];
        bool musicplus_status = pTempFilter->isParamFixed();
        if (musicplus_status) {
            return true;
        }
    }

    return false;
}

status_t AudioALSAStreamManager::SetBesLoudnessStatus(bool bEnable) {
    ALOGD("mBesLoudnessStatus() flag %d", bEnable);

#ifdef MTK_BESLOUDNESS_SUPPORT
    mBesLoudnessStatus = bEnable;
    AUDIO_AUDENH_CONTROL_OPTION_STRUCT audioParam;
    audioParam.u32EnableFlg = bEnable ? 1 : 0;
    mAudioCustParamClient->SetBesLoudnessControlOptionParamToNV(&audioParam);
    if (mBesLoudnessControlCallback != NULL) {
        mBesLoudnessControlCallback((void *)mBesLoudnessStatus);
    }
#else
    ALOGD("Unsupport set mBesLoudnessStatus()");
#endif
    return NO_ERROR;
}

bool AudioALSAStreamManager::GetBesLoudnessStatus() {
    return mBesLoudnessStatus;
}

status_t AudioALSAStreamManager::SetBesLoudnessControlCallback(const BESLOUDNESS_CONTROL_CALLBACK_STRUCT *callback_data) {
    if (callback_data == NULL) {
        mBesLoudnessControlCallback = NULL;
    } else {
        mBesLoudnessControlCallback = callback_data->callback;
        ASSERT(mBesLoudnessControlCallback != NULL);
        mBesLoudnessControlCallback((void *)mBesLoudnessStatus);
    }

    return NO_ERROR;
}

status_t AudioALSAStreamManager::UpdateACFHCF(int value) {
    ALOGD("%s()", __FUNCTION__);

    AUDIO_ACF_CUSTOM_PARAM_STRUCT sACFHCFParam;

    for (size_t i = 0; i < mFilterManagerVector.size() ; i++) {
        AudioMTKFilterManager  *pTempFilter = mFilterManagerVector[i];
        if (value == 0) {
            ALOGD("setParameters Update ACF Parames");
            getAudioCompFltCustParam(AUDIO_COMP_FLT_AUDIO, &sACFHCFParam);
            pTempFilter->setParameter(AUDIO_COMP_FLT_AUDIO, &sACFHCFParam);

        } else if (value == 1) {
            ALOGD("setParameters Update HCF Parames");
            getAudioCompFltCustParam(AUDIO_COMP_FLT_HEADPHONE, &sACFHCFParam);
            pTempFilter->setParameter(AUDIO_COMP_FLT_HEADPHONE, &sACFHCFParam);

        } else if (value == 2) {
            ALOGD("setParameters Update ACFSub Parames");
            getAudioCompFltCustParam(AUDIO_COMP_FLT_AUDIO_SUB, &sACFHCFParam);
            pTempFilter->setParameter(AUDIO_COMP_FLT_AUDIO_SUB, &sACFHCFParam);

        }
    }
    return NO_ERROR;
}

// ACF Preview parameter
status_t AudioALSAStreamManager::SetACFPreviewParameter(void *ptr, int len __unused) {
    ALOGD("%s()", __FUNCTION__);

    for (size_t i = 0; i < mFilterManagerVector.size() ; i++) {
        AudioMTKFilterManager  *pTempFilter = mFilterManagerVector[i];
        pTempFilter->setParameter(AUDIO_COMP_FLT_AUDIO, (AUDIO_ACF_CUSTOM_PARAM_STRUCT *)ptr);
    }

    return NO_ERROR;
}

status_t AudioALSAStreamManager::SetHCFPreviewParameter(void *ptr, int len __unused) {
    ALOGD("%s()", __FUNCTION__);

    for (size_t i = 0; i < mFilterManagerVector.size() ; i++) {
        AudioMTKFilterManager  *pTempFilter = mFilterManagerVector[i];
        pTempFilter->setParameter(AUDIO_COMP_FLT_HEADPHONE, (AUDIO_ACF_CUSTOM_PARAM_STRUCT *)ptr);
    }

    return NO_ERROR;
}

status_t AudioALSAStreamManager::setSpkOutputGain(int32_t gain, uint32_t ramp_sample_cnt) {
    ALOGD("%s(), gain = %d, ramp_sample_cnt = %u", __FUNCTION__, gain, ramp_sample_cnt);

#if 0 //K2 mark temp
    for (size_t i = 0; i < mFilterManagerVector.size() ; i++) {
        AudioMTKFilterManager  *pTempFilter = mFilterManagerVector[i];
        pTempFilter->setSpkOutputGain(gain, ramp_sample_cnt);
    }
#endif

    return NO_ERROR;
}

status_t AudioALSAStreamManager::setSpkFilterParam(uint32_t fc, uint32_t bw, int32_t th) {
    ALOGD("%s(), fc %d, bw %d, th %d", __FUNCTION__, fc, bw, th);

#if 0 //K2 mark temp
    for (size_t i = 0; i < mFilterManagerVector.size() ; i++) {
        AudioMTKFilterManager  *pTempFilter = mFilterManagerVector[i];
        pTempFilter->setSpkFilterParam(fc, bw, th);
    }
#endif
    return NO_ERROR;
}
status_t AudioALSAStreamManager::SetSpeechVmEnable(const int Type_VM) {
    ALOGV("%s(), Type_VM %d", __FUNCTION__, Type_VM);
#ifndef MTK_AUDIO_HIERARCHICAL_PARAM_SUPPORT
    ALOGD("%s(), Type_VM=%d, only Normal VM", __FUNCTION__, Type_VM);

    AUDIO_CUSTOM_PARAM_STRUCT eSphParamNB;
    mAudioCustParamClient->GetNBSpeechParamFromNVRam(&eSphParamNB);
    if (Type_VM == 0) { // normal VM
        eSphParamNB.debug_info[0] = 0;
    } else { // EPL
        eSphParamNB.debug_info[0] = 3;
        if (eSphParamNB.speech_common_para[0] == 0) { // if not assign EPL debug type yet, set a default one
            eSphParamNB.speech_common_para[0] = 6;
        }
    }

    mAudioCustParamClient->SetNBSpeechParamToNVRam(&eSphParamNB);
    SpeechEnhancementController::GetInstance()->SetNBSpeechParametersToAllModem(&eSphParamNB);
#endif

    return NO_ERROR;
}

status_t AudioALSAStreamManager::SetEMParameter(AUDIO_CUSTOM_PARAM_STRUCT *pSphParamNB) {
    ALOGD("%s()", __FUNCTION__);

    mAudioCustParamClient->SetNBSpeechParamToNVRam(pSphParamNB);
    SpeechEnhancementController::GetInstance()->SetNBSpeechParametersToAllModem(pSphParamNB);
    // Speech Enhancement, VM, Speech Driver
    // update VM/EPL/TTY record capability & enable if needed
    SpeechVMRecorder::GetInstance()->SetVMRecordCapability(pSphParamNB);


    return NO_ERROR;
}

status_t AudioALSAStreamManager::UpdateSpeechParams(const int speech_band) {
#ifdef MTK_AUDIO_HIERARCHICAL_PARAM_SUPPORT
    speech_type_dynamic_param_t typeSpeechparam = (speech_type_dynamic_param_t)speech_band;
    bool bParamTunerPlaying = AudioALSAParamTuner::getInstance()->isPlaying();
    ALOGD("+%s() typeSpeechparam=%d, bParamTunerPlaying=%d", __FUNCTION__, typeSpeechparam, bParamTunerPlaying);
    switch (typeSpeechparam) {
    case AUDIO_TYPE_SPEECH_DMNR:
        mSpeechDriverFactory->GetSpeechDriver()->SetDynamicSpeechParameters(typeSpeechparam, &typeSpeechparam);
        break;
    case AUDIO_TYPE_SPEECH_GENERAL:
        mSpeechDriverFactory->GetSpeechDriver()->SetDynamicSpeechParameters(typeSpeechparam, &typeSpeechparam);
        break;

    default:
        break;

    }
    if (isModeInPhoneCall() == true || bParamTunerPlaying == true) { // get output device for in_call, and set speech mode
        UpdateSpeechMode();
    }

    return NO_ERROR;

#else
    ALOGD("%s(), speech_band=%d", __FUNCTION__, speech_band);

    //speech_band: 0:Narrow Band, 1: Wide Band, 2: Super Wideband, ..., 8: All
    if (speech_band == 0) { //Narrow Band
        AUDIO_CUSTOM_PARAM_STRUCT eSphParamNB;
        mAudioCustParamClient->GetNBSpeechParamFromNVRam(&eSphParamNB);
        SpeechEnhancementController::GetInstance()->SetNBSpeechParametersToAllModem(&eSphParamNB);
        ALOGD("JT:================================");
        for (int i = 0; i < SPEECH_COMMON_NUM ; i++) {
            ALOGD("JT:speech_common_para[%d] = %d", i, eSphParamNB.speech_common_para[i]);
        }
        for (int i = 0; i < SPEECH_PARA_MODE_NUM; i++) {
            for (int j = 0; j < SPEECH_PARA_NUM; j++) {
                ALOGD("JT:speech_mode_para[%d][%d] = %d", i, j, eSphParamNB.speech_mode_para[i][j]);
            }
        }
        for (int i = 0; i < 4; i++) {
            ALOGD("JT:speech_volume_para[%d] = %d", i, eSphParamNB.speech_volume_para[i]);
        }
    }
#if defined(MTK_WB_SPEECH_SUPPORT)
    else if (speech_band == 1) { //Wide Band
        AUDIO_CUSTOM_WB_PARAM_STRUCT eSphParamWB;
        mAudioCustParamClient->GetWBSpeechParamFromNVRam(&eSphParamWB);
        SpeechEnhancementController::GetInstance()->SetWBSpeechParametersToAllModem(&eSphParamWB);
    }
#endif
    else if (speech_band == 8) { //set all mode parameters
        AUDIO_CUSTOM_PARAM_STRUCT eSphParamNB;
        AUDIO_CUSTOM_WB_PARAM_STRUCT eSphParamWB;
        mAudioCustParamClient->GetNBSpeechParamFromNVRam(&eSphParamNB);
        SpeechEnhancementController::GetInstance()->SetNBSpeechParametersToAllModem(&eSphParamNB);
#if defined(MTK_WB_SPEECH_SUPPORT)
        mAudioCustParamClient->GetWBSpeechParamFromNVRam(&eSphParamWB);
        SpeechEnhancementController::GetInstance()->SetWBSpeechParametersToAllModem(&eSphParamWB);
#endif
    }

    if (isModeInPhoneCall() == true) { // get output device for in_call, and set speech mode
        UpdateSpeechMode();
    }

    return NO_ERROR;
#endif
}

status_t AudioALSAStreamManager::UpdateSpeechLpbkParams() {
    ALOGD("%s()", __FUNCTION__);
#ifdef MTK_AUDIO_HIERARCHICAL_PARAM_SUPPORT
    SpeechParamParser::getInstance()->SetParamInfo(String8("ParamSphLpbk=1;"));
#else
    AUDIO_CUSTOM_PARAM_STRUCT eSphParamNB;
    AUDIO_CUSTOM_SPEECH_LPBK_PARAM_STRUCT  eSphParamNBLpbk;
    mAudioCustParamClient->GetNBSpeechParamFromNVRam(&eSphParamNB);
    mAudioCustParamClient->GetNBSpeechLpbkParamFromNVRam(&eSphParamNBLpbk);
    SpeechEnhancementController::GetInstance()->SetNBSpeechLpbkParametersToAllModem(&eSphParamNB, &eSphParamNBLpbk);
    //no need to set speech mode, only for loopback parameters update
#endif

    return NO_ERROR;
}

status_t AudioALSAStreamManager::UpdateMagiConParams() {
    if (AudioALSAHardwareResourceManager::getInstance()->getNumPhoneMicSupport() < 2) {
        ALOGW("-%s(), MagiConference Not Support", __FUNCTION__);
        return INVALID_OPERATION;
    }

    ALOGD("%s()", __FUNCTION__);
#if defined(MTK_MAGICONFERENCE_SUPPORT)
#ifndef MTK_AUDIO_HIERARCHICAL_PARAM_SUPPORT
    AUDIO_CUSTOM_MAGI_CONFERENCE_STRUCT eSphParamMagiCon;
    mAudioCustParamClient->GetMagiConSpeechParamFromNVRam(&eSphParamMagiCon);
    SpeechEnhancementController::GetInstance()->SetMagiConSpeechParametersToAllModem(&eSphParamMagiCon);

    if (isModeInPhoneCall() == true) { // get output device for in_call, and set speech mode
        UpdateSpeechMode();
    }

#endif
    return NO_ERROR;
#else
    ALOGW("-%s(), MagiConference Not Support", __FUNCTION__);
    return INVALID_OPERATION;
#endif
}

status_t AudioALSAStreamManager::UpdateHACParams() {
    ALOGD("%s()", __FUNCTION__);
#if defined(MTK_HAC_SUPPORT)
#ifndef MTK_AUDIO_HIERARCHICAL_PARAM_SUPPORT
    AUDIO_CUSTOM_HAC_PARAM_STRUCT eSphParamHAC;
    mAudioCustParamClient->GetHACSpeechParamFromNVRam(&eSphParamHAC);
    SpeechEnhancementController::GetInstance()->SetHACSpeechParametersToAllModem(&eSphParamHAC);

    if (isModeInPhoneCall() == true) { // get output device for in_call, and set speech mode
        UpdateSpeechMode();
    }

#endif
    return NO_ERROR;
#else
    ALOGW("-%s(), HAC Not Support", __FUNCTION__);
    return INVALID_OPERATION;

#endif
}

status_t AudioALSAStreamManager::UpdateDualMicParams() {
    ALOGD("%s()", __FUNCTION__);
#ifndef MTK_AUDIO_HIERARCHICAL_PARAM_SUPPORT
    AUDIO_CUSTOM_EXTRA_PARAM_STRUCT eSphParamDualMic;
    mAudioCustParamClient->GetDualMicSpeechParamFromNVRam(&eSphParamDualMic);
    if (AudioALSAHardwareResourceManager::getInstance()->getNumPhoneMicSupport() >= 2) {
        SpeechEnhancementController::GetInstance()->SetDualMicSpeechParametersToAllModem(&eSphParamDualMic);
    }

    if (isModeInPhoneCall() == true) { // get output device for in_call, and set speech mode
        UpdateSpeechMode();
    }

#endif
    return NO_ERROR;
}

status_t AudioALSAStreamManager::setMDVolumeIndex(int stream, int device, int index) {
    ALOGV("+%s() stream= %x, device= %x, index= %x", __FUNCTION__, stream, device, index);
    if (stream == 0) { //stream voice call/voip call
        if (isModeInPhoneCall() == true) {
            mSpeechDriverFactory->GetSpeechDriver()->setMDVolumeIndex(stream, device, index);
        } else {
            SpeechDriverInterface *pSpeechDriver = NULL;
            for (int modem_index = MODEM_1; modem_index < NUM_MODEM; modem_index++) {
                pSpeechDriver = mSpeechDriverFactory->GetSpeechDriverByIndex((modem_index_t)modem_index);
                if (pSpeechDriver != NULL) { // Might be single talk and some speech driver is NULL
                    pSpeechDriver->setMDVolumeIndex(stream, device, index);
                }
            }
        }
    }
    return NO_ERROR;
}

status_t AudioALSAStreamManager::UpdateSpeechMode() {
    ALOGD("%s()", __FUNCTION__);
    //tina todo
    const audio_devices_t output_device = (audio_devices_t)AudioALSAHardwareResourceManager::getInstance()->getOutputDevice();
    const audio_devices_t input_device  = (audio_devices_t)AudioALSAHardwareResourceManager::getInstance()->getInputDevice();
    mSpeechDriverFactory->GetSpeechDriver()->SetSpeechMode(input_device, output_device);

    return NO_ERROR;
}

status_t AudioALSAStreamManager::UpdateSpeechVolume() {
    ALOGD("%s()", __FUNCTION__);
    mAudioALSAVolumeController->initVolumeController();

    if (isModeInPhoneCall() == true) {
        //TINA TODO GET DEVICE
        int32_t outputDevice = (audio_devices_t)AudioALSAHardwareResourceManager::getInstance()->getOutputDevice();
        AudioALSASpeechPhoneCallController *pSpeechPhoneCallController = AudioALSASpeechPhoneCallController::getInstance();
#ifndef MTK_AUDIO_GAIN_TABLE
        mAudioALSAVolumeController->setVoiceVolume(mAudioALSAVolumeController->getVoiceVolume(),
                                                   getModeForGain(), (uint32)outputDevice);
#endif
        switch (outputDevice) {
        case AUDIO_DEVICE_OUT_WIRED_HEADSET : {
#ifdef  MTK_TTY_SUPPORT
            if (pSpeechPhoneCallController->getTtyMode() == AUD_TTY_VCO) {
                mAudioALSAVolumeController->ApplyMicGain(Normal_Mic, mAudioMode);
            } else if (pSpeechPhoneCallController->getTtyMode() == AUD_TTY_HCO || pSpeechPhoneCallController->getTtyMode() == AUD_TTY_FULL) {
                mAudioALSAVolumeController->ApplyMicGain(TTY_CTM_Mic, mAudioMode);
            } else {
                mAudioALSAVolumeController->ApplyMicGain(Headset_Mic, mAudioMode);
            }
#else
            mAudioALSAVolumeController->ApplyMicGain(Headset_Mic, mAudioMode);
#endif
            break;
        }
        case AUDIO_DEVICE_OUT_WIRED_HEADPHONE : {
#ifdef  MTK_TTY_SUPPORT
            if (pSpeechPhoneCallController->getTtyMode() == AUD_TTY_VCO) {
                mAudioALSAVolumeController->ApplyMicGain(Normal_Mic, mAudioMode);
            } else if (pSpeechPhoneCallController->getTtyMode() == AUD_TTY_HCO || pSpeechPhoneCallController->getTtyMode() == AUD_TTY_FULL) {
                mAudioALSAVolumeController->ApplyMicGain(TTY_CTM_Mic, mAudioMode);
            } else {
                mAudioALSAVolumeController->ApplyMicGain(Headset_Mic, mAudioMode);
            }
#else
            mAudioALSAVolumeController->ApplyMicGain(Headset_Mic, mAudioMode);
#endif
            break;
        }
        case AUDIO_DEVICE_OUT_SPEAKER: {
#ifdef  MTK_TTY_SUPPORT
            if (pSpeechPhoneCallController->getTtyMode() == AUD_TTY_VCO) {
                mAudioALSAVolumeController->ApplyMicGain(Normal_Mic, mAudioMode);
            } else if (pSpeechPhoneCallController->getTtyMode() == AUD_TTY_HCO || pSpeechPhoneCallController->getTtyMode() == AUD_TTY_FULL) {
                mAudioALSAVolumeController->ApplyMicGain(TTY_CTM_Mic, mAudioMode);
            } else {
                mAudioALSAVolumeController->ApplyMicGain(Handfree_Mic, mAudioMode);
            }
#else
            mAudioALSAVolumeController->ApplyMicGain(Handfree_Mic, mAudioMode);
#endif
            break;
        }
        case AUDIO_DEVICE_OUT_EARPIECE: {
            mAudioALSAVolumeController->ApplyMicGain(Normal_Mic, mAudioMode);
            break;
        }
        default: {
            break;
        }
        }
    } else {
        setMasterVolume(mAudioALSAVolumeController->getMasterVolume());
    }
    return NO_ERROR;

}

status_t AudioALSAStreamManager::SetVCEEnable(bool bEnable) {
    ALOGD("%s()", __FUNCTION__);
    SpeechEnhancementController::GetInstance()->SetDynamicMaskOnToAllModem(SPH_ENH_DYNAMIC_MASK_VCE, bEnable);
    return NO_ERROR;

}

status_t AudioALSAStreamManager::SetSpeechParamEnable(const char *ParamName, bool enable) {
#if defined(MTK_AUDIO_HIERARCHICAL_PARAM_SUPPORT)
    if (strcmp(ParamName, "ParamSphSV") == 0) {
        if (SpeechParamParser::getInstance()->GetSpeechParamSupport("SPH_PARAM_SV")) {
            bool SuperVolumeStatus = SpeechParamParser::getInstance()->GetParamStatus(ParamName);
            ALOGD("%s(), %s(%d -> %d)", __FUNCTION__, ParamName, SuperVolumeStatus, enable);
            if (SuperVolumeStatus != enable) {
                // enable/disable flag
                if (enable) {
                    SpeechParamParser::getInstance()->SetParamInfo(String8("ParamSphSV=1;"));
                } else {
                    SpeechParamParser::getInstance()->SetParamInfo(String8("ParamSphSV=0;"));
                }
            }
            return NO_ERROR;
        } else {
            ALOGD("%s() %s NOT Supported!", __FUNCTION__, ParamName);
            return INVALID_OPERATION;
        }
    } else {
        return INVALID_OPERATION;
    }
#else
    (void)ParamName;
    (void)enable;
    ALOGD("%s() NOT Supported!", __FUNCTION__);
    return INVALID_OPERATION;
#endif
}

bool AudioALSAStreamManager::GetSpeechParamEnable(const char *ParamName) {
#if defined(MTK_AUDIO_HIERARCHICAL_PARAM_SUPPORT)
    if (strcmp(ParamName, "ParamSphSV") == 0) {
        if (SpeechParamParser::getInstance()->GetSpeechParamSupport("SPH_PARAM_SV")) {
            bool SuperVolumeStatus = SpeechParamParser::getInstance()->GetParamStatus(ParamName);
            ALOGD("%s(), SuperVolumeStatus=%d", __FUNCTION__, SuperVolumeStatus);
            return SuperVolumeStatus;
        } else {
            ALOGV("%s() NOT Supported!", __FUNCTION__);
            return false;
        }
    } else {
        return false;
    }
#else
    (void)ParamName;
    ALOGD("%s() NOT Supported!", __FUNCTION__);
    return false;
#endif
}

status_t AudioALSAStreamManager::SetMagiConCallEnable(bool bEnable) {
    ALOGD("%s(), bEnable=%d", __FUNCTION__, bEnable);

    // enable/disable flag
    SpeechEnhancementController::GetInstance()->SetMagicConferenceCallOn(bEnable);
    if (isModeInPhoneCall() == true) { // get output device for in_call, and set speech mode
        UpdateSpeechMode();
    }

    return NO_ERROR;

}

bool AudioALSAStreamManager::GetMagiConCallEnable(void) {
    bool bEnable = SpeechEnhancementController::GetInstance()->GetMagicConferenceCallOn();
    ALOGD("-%s(), bEnable=%d", __FUNCTION__, bEnable);

    return bEnable;
}

status_t AudioALSAStreamManager::SetHACEnable(bool bEnable) {
    ALOGD("%s(), bEnable=%d", __FUNCTION__, bEnable);

    // enable/disable flag
    SpeechEnhancementController::GetInstance()->SetHACOn(bEnable);
    if (isModeInPhoneCall() == true) { // get output device for in_call, and set speech mode
        UpdateSpeechMode();
    }

    return NO_ERROR;

}

bool AudioALSAStreamManager::GetHACEnable(void) {
    bool bEnable = SpeechEnhancementController::GetInstance()->GetHACOn();
    ALOGD("-%s(), bEnable=%d", __FUNCTION__, bEnable);

    return bEnable;
}

status_t AudioALSAStreamManager::SetVMLogConfig(unsigned short mVMConfig) {
    ALOGD("+%s(), mVMConfig=%d", __FUNCTION__, mVMConfig);

#ifdef MTK_AUDIO_HIERARCHICAL_PARAM_SUPPORT
    if (GetVMLogConfig() == mVMConfig) {
        ALOGD("%s(), mVMConfig(%d) the same, retrun directly.", __FUNCTION__, mVMConfig);
        return NO_ERROR;
    }
    // Speech Enhancement, VM, Speech Driver
    // update VM/EPL/TTY record capability & enable if needed
    SpeechVMRecorder::GetInstance()->SetVMRecordCapability(mVMConfig);
#endif

    return NO_ERROR;

}

unsigned short AudioALSAStreamManager::GetVMLogConfig(void) {
    unsigned short mVMConfig;

    AUDIO_CUSTOM_AUDIO_FUNC_SWITCH_PARAM_STRUCT eParaAudioFuncSwitch;
    mAudioCustParamClient->GetAudioFuncSwitchParamFromNV(&eParaAudioFuncSwitch);
    mVMConfig = eParaAudioFuncSwitch.vmlog_dump_config;

    ALOGD("-%s(), mVMConfig=%d", __FUNCTION__, mVMConfig);

    return mVMConfig;
}

status_t AudioALSAStreamManager::SetCustXmlEnable(unsigned short enable) {
    ALOGD("+%s(), enable = %d", __FUNCTION__, enable);

#ifdef MTK_AUDIO_HIERARCHICAL_PARAM_SUPPORT
    AUDIO_CUSTOM_AUDIO_FUNC_SWITCH_PARAM_STRUCT eParaAudioFuncSwitch;
    mAudioCustParamClient->GetAudioFuncSwitchParamFromNV(&eParaAudioFuncSwitch);
    if (eParaAudioFuncSwitch.cust_xml_enable == enable) {
        ALOGD("%s(), enable(%d) the same, retrun directly.", __FUNCTION__, enable);
        return NO_ERROR;
    } else {
        eParaAudioFuncSwitch.cust_xml_enable = enable;
        mAudioCustParamClient->SetAudioFuncSwitchParamToNV(&eParaAudioFuncSwitch);
        ALOGD("%s(), set CustXmlEnabl = %d\n", __FUNCTION__, enable);
    }
#endif

    return NO_ERROR;
}

unsigned short AudioALSAStreamManager::GetCustXmlEnable(void) {
    unsigned short enable;

    AUDIO_CUSTOM_AUDIO_FUNC_SWITCH_PARAM_STRUCT eParaAudioFuncSwitch;
    mAudioCustParamClient->GetAudioFuncSwitchParamFromNV(&eParaAudioFuncSwitch);
    enable = eParaAudioFuncSwitch.cust_xml_enable;

    ALOGD("-%s(), enable = %d", __FUNCTION__, enable);

    return enable;
}

/**
 * reopen Phone Call audio path according to RIL mapped modem
 */
int AudioALSAStreamManager::phoneCallRefreshModem(const char *rilMappedMDName) {
    AL_AUTOLOCK(mStreamVectorLock);
    AL_AUTOLOCK(mLock);
    bool isPhoneCallNeedReopen = false;
    modem_index_t rilMappedMDIdx = MODEM_1;
    //check if Phone Call need reopen according to RIL mapped modem
    if (rilMappedMDName != NULL) {
        if (isModeInPhoneCall(mAudioMode)) {
            if (strcmp("MD1", rilMappedMDName) == 0) {
                rilMappedMDIdx = MODEM_1;
            } else if (strcmp("MD3", rilMappedMDName) == 0) {
                rilMappedMDIdx = MODEM_EXTERNAL;
            } else {
                ALOGW("%s(), Invalid rilMappedMDName=%s, mAudioMode(%d)", __FUNCTION__, rilMappedMDName, mAudioMode);
                return -EINVAL;
            }
            isPhoneCallNeedReopen = mSpeechPhoneCallController->checkReopen(rilMappedMDIdx);
        }
        ALOGV("%s(), rilMappedMDName=%s, mAudioMode(%d), isPhoneCallNeedReopen(%d)",
              __FUNCTION__,rilMappedMDName, mAudioMode, isPhoneCallNeedReopen);
    } else {
        ALOGW("%s(), Invalid rilMappedMDName=NULL, mAudioMode(%d)", __FUNCTION__, mAudioMode);
        return -EINVAL;
    }
    if (isPhoneCallNeedReopen) {
        ALOGD("+%s(), rilMappedMDIdx(%d), mAudioMode(%d), start to reopen", __FUNCTION__, rilMappedMDIdx, mAudioMode);
        setAllStreamsSuspend(true, true);
        standbyAllStreams(true);
        mSpeechPhoneCallController->setMicMute(true);
        const audio_devices_t currentPhonecallOutputDevice = mSpeechPhoneCallController->getPhonecallOutputDevice();
        const audio_devices_t currentPhonecallInputputDevice = mSpeechPhoneCallController->getPhonecallInputDevice();

        mSpeechPhoneCallController->close();
        mSpeechPhoneCallController->open(mAudioMode, currentPhonecallOutputDevice, currentPhonecallInputputDevice);

        mAudioALSAVolumeController->setVoiceVolume(mAudioALSAVolumeController->getVoiceVolume(),
                                                   getModeForGain(), currentPhonecallOutputDevice);
        mSpeechPhoneCallController->setMicMute(mMicMute);
        setAllStreamsSuspend(false, true);
        ALOGD("-%s(), mAudioMode(%d), currentPhonecallOutputDevice(0x%x), reopen end",
              __FUNCTION__, mAudioMode, currentPhonecallOutputDevice);
    } else {
        ALOGD("-%s(), rilMappedMDName=%s, mAudioMode(%d), no need to reopen", __FUNCTION__, rilMappedMDName, mAudioMode);
    }
    return 0;
}

/**
 * update Phone Call phone id
 */
int AudioALSAStreamManager::phoneCallUpdatePhoneId(const phone_id_t phoneId) {
    if (phoneId != PHONE_ID_0 && phoneId != PHONE_ID_1) {
        return -EINVAL;
    }
    if (isModeInPhoneCall(mAudioMode)) {
        phone_id_t currentPhoneId = mSpeechPhoneCallController->getPhoneId();

        if (phoneId != currentPhoneId) {
            ALOGD("%s(), phoneId(%d->%d), mAudioMode(%d)", __FUNCTION__, currentPhoneId, phoneId, mAudioMode);
            mSpeechPhoneCallController->setPhoneId(phoneId);
            modem_index_t newMDIdx = mSpeechPhoneCallController->getIdxMDByPhoneId(phoneId);
            if (newMDIdx == MODEM_EXTERNAL) {
                phoneCallRefreshModem("MD3");
            } else {
                phoneCallRefreshModem("MD1");
            }
        }
    } else {
        mSpeechPhoneCallController->setPhoneId(phoneId);
    }
    return 0;
}

status_t AudioALSAStreamManager::SetBtHeadsetName(const char *btHeadsetName) {
    if (mBtHeadsetName) {
        free((void *)mBtHeadsetName);
        mBtHeadsetName = NULL;
    }
    if (btHeadsetName) {
        mBtHeadsetName = strdup(btHeadsetName);
        ALOGV("%s(), mBtHeadsetName = %s", __FUNCTION__, mBtHeadsetName);
    }
    return NO_ERROR;
}

const char *AudioALSAStreamManager::GetBtHeadsetName() {
    return mBtHeadsetName;
}

status_t AudioALSAStreamManager::SetBtHeadsetNrec(bool bEnable) {
#if defined(CONFIG_MT_ENG_BUILD)
    // Used for testing the BT_NREC_OFF case
    char property_value[PROPERTY_VALUE_MAX];
    property_get(PROPERTY_KEY_SET_BT_NREC, property_value, "-1");
    int btNrecProp = atoi(property_value);
    if (btNrecProp != -1) {
        bEnable = btNrecProp;
        ALOGD("%s(), force set the BT headset NREC = %d", __FUNCTION__, bEnable);
    }
#endif

    ALOGV("%s(), bEnable=%d", __FUNCTION__, bEnable);

    // enable/disable flag
    if (SpeechEnhancementController::GetInstance()->GetBtHeadsetNrecOn() != bEnable) {
        SpeechEnhancementController::GetInstance()->SetBtHeadsetNrecOnToAllModem(bEnable);
    }

    return NO_ERROR;

}

bool AudioALSAStreamManager::GetBtHeadsetNrecStatus(void) {
    bool bEnable = SpeechEnhancementController::GetInstance()->GetBtHeadsetNrecOn();
    ALOGD("-%s(), bEnable=%d", __FUNCTION__, bEnable);

    return bEnable;
}

status_t AudioALSAStreamManager::Enable_DualMicSettng(sph_enh_dynamic_mask_t sphMask, bool bEnable) {
    ALOGD("%s(), bEnable=%d", __FUNCTION__, bEnable);

    SpeechEnhancementController::GetInstance()->SetDynamicMaskOnToAllModem(sphMask, bEnable);
    return NO_ERROR;

}

status_t AudioALSAStreamManager::Set_LSPK_DlMNR_Enable(sph_enh_dynamic_mask_t sphMask, bool bEnable) {
    ALOGD("%s(), bEnable=%d", __FUNCTION__, bEnable);

    Enable_DualMicSettng(sphMask, bEnable);

    if (SpeechEnhancementController::GetInstance()->GetMagicConferenceCallOn() == true &&
        SpeechEnhancementController::GetInstance()->GetDynamicMask(sphMask) == true) {
        ALOGE("Cannot open MagicConCall & LoudSpeaker DMNR at the same time!!");
    }
    return NO_ERROR;

}


bool AudioALSAStreamManager::getVoiceWakeUpNeedOn() {
    AL_AUTOLOCK(mLock);
    return mVoiceWakeUpNeedOn;
}

status_t AudioALSAStreamManager::setVoiceWakeUpNeedOn(const bool enable) {
    ALOGD("+%s(), mVoiceWakeUpNeedOn: %d => %d ", __FUNCTION__, mVoiceWakeUpNeedOn, enable);
    AL_AUTOLOCK(mLock);

    if (enable == mVoiceWakeUpNeedOn) {
        ALOGW("-%s(), enable(%d) == mVoiceWakeUpNeedOn(%d), return", __FUNCTION__, enable, mVoiceWakeUpNeedOn);
        return INVALID_OPERATION;
    }

    if (enable == true) {
        if (mStreamInVector.size() != 0 || mForceDisableVoiceWakeUpForSetMode == true) {
            ALOGD("-%s(), mStreamInVector.size() = %zu, mForceDisableVoiceWakeUpForSetMode = %d, return", __FUNCTION__, mStreamInVector.size(), mForceDisableVoiceWakeUpForSetMode);
        } else {
            if (mAudioALSAVoiceWakeUpController->getVoiceWakeUpEnable() == false) {
                mAudioALSAVoiceWakeUpController->setVoiceWakeUpEnable(true);
            }
        }
    } else {
        if (mAudioALSAVoiceWakeUpController->getVoiceWakeUpEnable() == true) {
            mAudioALSAVoiceWakeUpController->setVoiceWakeUpEnable(false);
        }
    }


    property_set(PROPERTY_KEY_VOICE_WAKE_UP_NEED_ON, (enable == false) ? "0" : "1");
    mVoiceWakeUpNeedOn = enable;

    ALOGD("-%s(), mVoiceWakeUpNeedOn: %d", __FUNCTION__, mVoiceWakeUpNeedOn);
    return NO_ERROR;
}

void AudioALSAStreamManager::UpdateDynamicFunctionMask(void) {
    ALOGD("+%s()", __FUNCTION__);
    if (mStreamInVector.size() > 0) {
        for (size_t i = 0; i < mStreamInVector.size(); i++) {
            mStreamInVector[i]->UpdateDynamicFunctionMask();
        }
    }
    ALOGD("-%s()", __FUNCTION__);
}

bool AudioALSAStreamManager::EnableBesRecord(void) {
    bool bRet = false;
    if ((mAudioCustParamClient->QueryFeatureSupportInfo()& SUPPORT_HD_RECORD) > 0) {
        bRet = true;
    }
    ALOGD("-%s(), %x", __FUNCTION__, bRet);

    return bRet;
}

status_t AudioALSAStreamManager::setScreenState(bool mode) {
    AL_AUTOLOCK(mStreamVectorLock);
    AudioALSAStreamOut *pAudioALSAStreamOut = NULL;
    AudioALSAStreamIn *pAudioALSAStreamIn = NULL;

    for (size_t i = 0; i < mStreamOutVector.size(); i++) {
        pAudioALSAStreamOut = mStreamOutVector[i];
        pAudioALSAStreamOut->setScreenState(mode);
    }

    for (size_t i = 0; i < mStreamInVector.size(); i++) {
        pAudioALSAStreamIn = mStreamInVector[i];
        // Update IRQ period when all streamin are Normal Record
        if ((pAudioALSAStreamIn->getStreamInCaptureHandler() != NULL) && ((pAudioALSAStreamIn->getInputFlags() & AUDIO_INPUT_FLAG_FAST) != 0) &&
            (pAudioALSAStreamIn->getStreamInCaptureHandler()->getCaptureHandlerType() != CAPTURE_HANDLER_NORMAL)) {
            break;
        }
        if (i == (mStreamInVector.size() - 1)) {
            ALOGE("%s, mStreamInVector[%zu]->getInputFlags() = 0x%x\n", __FUNCTION__, i, mStreamInVector[i]->getInputFlags());
            pAudioALSAStreamIn->setLowLatencyMode(mode);
        }

    }
    return NO_ERROR;
}

status_t AudioALSAStreamManager::setBypassDLProcess(bool flag) {
    AL_AUTOLOCK(mLock);
    AudioALSAStreamOut *pAudioALSAStreamOut = NULL;

    mBypassPostProcessDL = flag;

    return NO_ERROR;
}

status_t AudioALSAStreamManager::EnableSphStrmByDevice(audio_devices_t output_devices) {
    AudioALSASpeechStreamController::getInstance()->SetStreamOutputDevice(output_devices);
    if (isModeInPhoneCall(mAudioMode) == true) {
        if ((output_devices & AUDIO_DEVICE_OUT_SPEAKER) != 0) {
            AudioALSASpeechStreamController::getInstance()->EnableSpeechStreamThread(true);
        }
    }
    return NO_ERROR;
}

status_t AudioALSAStreamManager::DisableSphStrmByDevice(audio_devices_t output_devices) {
    AudioALSASpeechStreamController::getInstance()->SetStreamOutputDevice(output_devices);
    if (isModeInPhoneCallSupportEchoRef(mAudioMode) == true) {
        if (AudioALSASpeechStreamController::getInstance()->IsSpeechStreamThreadEnable() == true) {
            AudioALSASpeechStreamController::getInstance()->EnableSpeechStreamThread(false);
        }
    }
    return NO_ERROR;
}

status_t AudioALSAStreamManager::EnableSphStrm(audio_mode_t new_mode) {
    ALOGD("%s new_mode = %d", __FUNCTION__, new_mode);
    if ((new_mode < AUDIO_MODE_NORMAL) || (new_mode > AUDIO_MODE_MAX)) {
        return BAD_VALUE;
    }

    if (isModeInPhoneCall(new_mode) == true) {
        if ((AudioALSASpeechStreamController::getInstance()->GetStreamOutputDevice() & AUDIO_DEVICE_OUT_SPEAKER) != 0 &&
            (AudioALSASpeechStreamController::getInstance()->IsSpeechStreamThreadEnable() == false)) {
            AudioALSASpeechStreamController::getInstance()->EnableSpeechStreamThread(true);
        }
    }
    return NO_ERROR;
}

status_t AudioALSAStreamManager::DisableSphStrm(audio_mode_t new_mode) {
    ALOGD("%s new_mode = %d", __FUNCTION__, new_mode);
    if ((new_mode < AUDIO_MODE_NORMAL) || (new_mode > AUDIO_MODE_MAX)) {
        return BAD_VALUE;
    }
    if (new_mode == mAudioMode) {
        ALOGW("-%s(), mAudioMode: %d == %d, return", __FUNCTION__, mAudioMode, new_mode);
        return BAD_VALUE;
    }

    if (isModeInPhoneCallSupportEchoRef(mAudioMode) == true) {
        if (AudioALSASpeechStreamController::getInstance()->IsSpeechStreamThreadEnable() == true) {
            AudioALSASpeechStreamController::getInstance()->EnableSpeechStreamThread(false);
        }
    }
    return NO_ERROR;
}

bool AudioALSAStreamManager::IsSphStrmSupport(void) {
    char property_value[PROPERTY_VALUE_MAX];
    bool Currentsupport = false;
    property_get("streamout.speech_stream.enable", property_value, "1");
    int speech_stream = atoi(property_value);
#if defined(MTK_MAXIM_SPEAKER_SUPPORT)&&defined(MTK_SPEAKER_MONITOR_SPEECH_SUPPORT)
    Currentsupport = true;
#endif
    ALOGD("%s = %d Currentsupport = %d", __FUNCTION__, speech_stream, Currentsupport);
    return (speech_stream & Currentsupport);
}

bool AudioALSAStreamManager::isModeInPhoneCallSupportEchoRef(const audio_mode_t audio_mode) {
    if (audio_mode == AUDIO_MODE_IN_CALL) {
        return true;
    } else {
        return false;
    }
}

status_t AudioALSAStreamManager::setParametersToStreamOut(const String8 &keyValuePairs) { // TODO(Harvey
    if (mStreamOutVector.size() == 0) {
        return INVALID_OPERATION;
    }

    AudioALSAStreamOut *pAudioALSAStreamOut = NULL;
    for (size_t i = 0; i < mStreamOutVector.size() ; i++) {
        pAudioALSAStreamOut = mStreamOutVector[i];
        pAudioALSAStreamOut->setParameters(keyValuePairs);
    }

    return NO_ERROR;
}

status_t AudioALSAStreamManager::setParameters(const String8 &keyValuePairs, int IOport) { // TODO(Harvey)
    status_t status = PERMISSION_DENIED;
    ssize_t index = -1;

    ALOGD("+%s(), IOport = %d, keyValuePairs = %s", __FUNCTION__, IOport, keyValuePairs.string());

    index = mStreamOutVector.indexOfKey(IOport);
    if (index >= 0) {
        ALOGV("Send to mStreamOutVector[%zu]", index);
        AudioALSAStreamOut *pAudioALSAStreamOut = mStreamOutVector.valueAt(index);
        status = pAudioALSAStreamOut->setParameters(keyValuePairs);
        ALOGV("-%s()", __FUNCTION__);
        return status;
    }

    index = mStreamInVector.indexOfKey(IOport);
    if (index >= 0) {
        ALOGV("Send to mStreamInVector [%zu]", index);
        AudioALSAStreamIn *pAudioALSAStreamIn = mStreamInVector.valueAt(index);
        status = pAudioALSAStreamIn->setParameters(keyValuePairs);
        ALOGV("-%s()", __FUNCTION__);
        return status;
    }

    ALOGE("-%s(), do nothing, return", __FUNCTION__);
    return status;
}

void AudioALSAStreamManager::updateDeviceConnectionState(audio_devices_t device, bool connect) {
    if ((device & AUDIO_DEVICE_BIT_IN) == false) {
        mAvailableOutputDevices = connect ? mAvailableOutputDevices | device : mAvailableOutputDevices & !device;
    }
}

bool AudioALSAStreamManager::getDeviceConnectionState(audio_devices_t device) {
    if ((device & AUDIO_DEVICE_BIT_IN) == false) {
        return !!(mAvailableOutputDevices & device);
    }
    return false;
}

void AudioALSAStreamManager::setCustScene(const String8 scene) {
    mCustScene = scene;
#if defined(MTK_NEW_VOL_CONTROL)
    AudioMTKGainController::getInstance()->setScene(scene.string());
#endif
}

bool AudioALSAStreamManager::isEchoRefUsing() {
    if (isModeInVoipCall() == true) {
        return true;
    }
    if (mStreamInVector.size() > 1) {
        for (size_t i = 0; i < mStreamInVector.size(); i++) {
            if ((mStreamInVector[i]->getStreamAttribute()->input_source == AUDIO_SOURCE_VOICE_COMMUNICATION)
                || (mStreamInVector[i]->getStreamAttribute()->NativePreprocess_Info.PreProcessEffect_AECOn == true)
                || (mStreamInVector[i]->getStreamAttribute()->input_source == AUDIO_SOURCE_CUSTOMIZATION1) //MagiASR enable AEC
                || (mStreamInVector[i]->getStreamAttribute()->input_source == AUDIO_SOURCE_CUSTOMIZATION2)) {
                return true;
            }
        }
    }
    return false;
}

} // end of namespace android
