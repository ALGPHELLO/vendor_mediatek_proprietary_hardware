#include "AudioALSAHardwareResourceManager.h"

#include <utils/threads.h>

#include "AudioType.h"
#include <AudioLock.h>
#include "AudioAssert.h"

#include "AudioALSADriverUtility.h"
#include "audio_custom_exp.h"

// TMP
#include "AudioALSACodecDeviceOutEarphonePMIC.h"
#include "AudioALSACodecDeviceOutReceiverPMIC.h"
#include "AudioALSACodecDeviceOutSpeakerPMIC.h"
#include "AudioALSACodecDeviceOutSpeakerEarphonePMIC.h"
#include "AudioALSADeviceConfigManager.h"
#include "AudioALSAHardware.h"
#include "AudioALSASampleRateController.h"
#include "AudioALSADeviceParser.h"
#include "AudioVolumeFactory.h"
#include "LoopbackManager.h"
#include "AudioDeviceInt.h"
#include "AudioCustParamClient.h"

#ifdef MTK_AUDIO_HIERARCHICAL_PARAM_SUPPORT
#include "AudioParamParser.h"
#endif

#include "AudioSmartPaController.h"

#ifdef MTK_USB_PHONECALL
#include "AudioUSBPhoneCallController.h"
#endif

#ifdef USING_EXTAMP_HP
#include "AudioALSACodecDeviceOutExtSpeakerAmp.h"
#endif

#ifdef HAVING_RCV_SPK_SWITCH
#include "AudioALSACodecDeviceOutReceiverSpeakerSwitch.h"
#endif

#include "WCNChipController.h"

#ifdef MTK_ANC_SUPPORT
#include "AudioALSAANCController.h"
#endif
#if defined(MTK_HYBRID_NLE_SUPPORT)
#include "AudioALSANLEController.h"
#endif

//awinic add 
#include "AudioALSAPlaybackHandlerNormal.h"
#ifdef awinic_mec
#include<string.h>
#include<cutils/properties.h>
#endif
//awinic add 

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "AudioALSAHardwareResourceManager"

static   const char PROPERTY_KEY_EXTDAC[PROPERTY_KEY_MAX]  = "af.resouce.extdac_support";

namespace android {

static const char *SIDEGEN[] = {
    "I0I1",   "I2",     "I3I4",   "I5I6",
    "I7I8",   "I9",     "I10I11", "I12I13",
    "I14",    "I15I16", "I17I18", "I19I20",
    "I21I22", "O0O1",   "O2",     "O3O4",
    "O5O6",   "O7O8",   "O9O10",  "O11",
    "O12",    "O13O14", "O15O16", "O17O18",
    "O19O20", "O21O22", "O23O24", "OFF",
    "O3",     "O4",     "I25I26", "O25",
    "O28O29", "I23I24", "O32O33", "I34I35"
};

static const char *SIDEGEN_SAMEPLRATE[] = {
    "8K", "11K", "12K", "16K",
    "22K", "24K", "32K", "44K",
    "48K", "88k", "96k", "176k",
    "192k"
};

static AudioLock mGetInstanceLock;
AudioALSAHardwareResourceManager *AudioALSAHardwareResourceManager::mAudioALSAHardwareResourceManager = NULL;
AudioALSAHardwareResourceManager *AudioALSAHardwareResourceManager::getInstance() {
    AL_AUTOLOCK(mGetInstanceLock);

    if (mAudioALSAHardwareResourceManager == NULL) {
        mAudioALSAHardwareResourceManager = new AudioALSAHardwareResourceManager();
    }
    ASSERT(mAudioALSAHardwareResourceManager != NULL);
    return mAudioALSAHardwareResourceManager;
}

status_t AudioALSAHardwareResourceManager::ResetDevice(void) {
    if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_Amp_R_Switch"), "Off")) {
        ALOGE("Error: Audio_Amp_R_Switch invalid value");
    }

    if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_Amp_L_Switch"), "Off")) {
        ALOGE("Error: Audio_Amp_L_Switch invalid value");
    }

    if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Voice_Amp_Switch"), "Off")) {
        ALOGE("Error: Voice_Amp_Switch invalid value");
    }

    if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Speaker_Amp_Switch"), "Off")) {
        ALOGE("Error: Speaker_Amp_Switch invalid value");
    }

    if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Headset_Speaker_Amp_Switch"), "Off")) {
        ALOGE("Error: Headset_Speaker_Amp_Switch invalid value");
    }

    if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Ext_Speaker_Amp_Switch"), "Off")) {
        ALOGE("Error: Ext_Speaker_Amp_Switch invalid value");
    }

    if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Receiver_Speaker_Switch"), "Off")) {
        ALOGE("Error: Receiver_Speaker_Switch invalid value");
    }

    if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_ADC_1_Switch"), "Off")) {
        ALOGE("Error: Headset_Speaker_Amp_Switch invalid value");
    }

    if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_ADC_2_Switch"), "Off")) {
        ALOGE("Error: Headset_Speaker_Amp_Switch invalid value");
    }

    return NO_ERROR;
}

AudioALSAHardwareResourceManager::AudioALSAHardwareResourceManager() :
    mMixer(AudioALSADriverUtility::getInstance()->getMixer()),
    mPcmDL(NULL),
    mOutputDevices(AUDIO_DEVICE_NONE),
    mInputDevice(AUDIO_DEVICE_NONE),
    mOutputDeviceSampleRate(44100),
    mInputDeviceSampleRate(48000),
    mIsChangingInputDevice(false),
    mStartOutputDevicesCount(0),
    mStartInputDeviceCount(0),
#if defined(MTK_AUDIO_MIC_INVERSE) || defined(DEN_PHONE_MIC_INVERSE)
    mMicInverse(true),  // if defined MTK_AUDIO_MIC_INVERSE, default invert main & ref mic
#else
    mMicInverse(false),
#endif
    mBuiltInMicSpecificType(BUILTIN_MIC_DEFAULT),
    mNumHSPole(4),
    mHeadchange(false),
    mSmartPaController(AudioSmartPaController::getInstance()),
    mSpkPcmOut(NULL) {
    ALOGD("%s()", __FUNCTION__);

#ifdef CONFIG_MT_ENG_BUILD
    mLogEnable = 1;
#else
    mLogEnable = __android_log_is_loggable(ANDROID_LOG_DEBUG, LOG_TAG, ANDROID_LOG_INFO);
#endif
    ResetDevice();
    AudioALSACodecDeviceOutEarphonePMIC::getInstance();
    AudioALSACodecDeviceOutSpeakerEarphonePMIC::getInstance();
    mDeviceConfigManager = AudioALSADeviceConfigManager::getInstance();

    memset((void *)&mSpkPcmConfig, 0, sizeof(mSpkPcmConfig));

    mNumPhoneMicSupport = AudioCustParamClient::GetInstance()->getNumMicSupport();

    setMicType();
}


AudioALSAHardwareResourceManager::~AudioALSAHardwareResourceManager() {
    ALOGD("%s()", __FUNCTION__);
    AL_AUTOLOCK(mGetInstanceLock);
    mAudioALSAHardwareResourceManager = NULL;
}

/**
 * output devices
 */
status_t AudioALSAHardwareResourceManager::setOutputDevice(const audio_devices_t new_devices, const uint32_t sample_rate) {
    ALOGD("+%s(), new_devices = 0x%x, mStartOutputDevicesCount = %d", __FUNCTION__, new_devices, mStartOutputDevicesCount);

    ASSERT(mStartOutputDevicesCount == 0);

    mOutputDevices = new_devices;
    mOutputDeviceSampleRate = sample_rate;

    return NO_ERROR;
}

status_t AudioALSAHardwareResourceManager::startOutputDevice(const audio_devices_t new_devices, const uint32_t SampleRate) {
    ALOGD("+%s(), new_devices = 0x%x, mOutputDevices = 0x%x, mStartOutputDevicesCount = %d SampleRate = %d",
          __FUNCTION__, new_devices, mOutputDevices, mStartOutputDevicesCount, SampleRate);

    AL_AUTOLOCK(mLock);

    if (new_devices == mOutputDevices) {
        // don't need to do anything
    } else if (AUDIO_DEVICE_NONE != mOutputDevices) {
        changeOutputDevice_l(new_devices, SampleRate);
    } else {
        startOutputDevice_l(new_devices, SampleRate);
    }

    mStartOutputDevicesCount++;

    ALOGD("-%s(), mOutputDevices = 0x%x, mStartOutputDevicesCount = %d", __FUNCTION__, mOutputDevices, mStartOutputDevicesCount);
    return NO_ERROR;
}


status_t AudioALSAHardwareResourceManager::stopOutputDevice() {
    ALOGD("+%s(), mOutputDevices = 0x%x, mStartOutputDevicesCount = %d", __FUNCTION__, mOutputDevices, mStartOutputDevicesCount);

    AL_AUTOLOCK(mLock);

    if (mStartOutputDevicesCount > 1) {
        // don't need to do anything
    } else {
        if (mStartOutputDevicesCount == 1 && mOutputDevices != AUDIO_DEVICE_NONE) {
            stopOutputDevice_l();
        }
        else {
            ASSERT(0);
            if (mStartOutputDevicesCount < 0) {
                mStartOutputDevicesCount = 0;
            }
        }
    }

    if (mStartOutputDevicesCount > 0) {
        mStartOutputDevicesCount--;
    }

    if (mHeadchange == true && mStartOutputDevicesCount == 0) {
#ifdef MTK_NEW_VOL_CONTROL
        AudioMTKGainController::getInstance()->GetHeadPhoneImpedance();
#else
        AudioALSAVolumeController::getInstance()->GetHeadPhoneImpedance();
#endif
        mHeadchange = false;
    }

    ALOGD("-%s(), mOutputDevices = 0x%x, mStartOutputDevicesCount = %d", __FUNCTION__, mOutputDevices, mStartOutputDevicesCount);
    return NO_ERROR;
}


status_t AudioALSAHardwareResourceManager::changeOutputDevice(const audio_devices_t new_devices) {
    ALOGD("+%s(), mOutputDevices: 0x%x => 0x%x", __FUNCTION__, mOutputDevices, new_devices);

    AL_AUTOLOCK(mLock);
    changeOutputDevice_l(new_devices, mOutputDeviceSampleRate);

    ALOGD("-%s(), mOutputDevices: 0x%x", __FUNCTION__, mOutputDevices);
    return NO_ERROR;
}


status_t AudioALSAHardwareResourceManager::startOutputDevice_l(const audio_devices_t new_devices, const uint32_t SampleRate) {
    mOutputDevices = new_devices;
    mOutputDeviceSampleRate = SampleRate;

    if (mSmartPaController->isSmartPAUsed() &&
        (new_devices & AUDIO_DEVICE_OUT_SPEAKER && mSmartPaController->isAlsaCodec())) {
        int pcmindex = AudioALSADeviceParser::getInstance()->GetPcmIndexByString(keypcmExtSpkMeida);
        int cardindex = AudioALSADeviceParser::getInstance()->GetCardIndexByString(keypcmExtSpkMeida);

        memset(&mSpkPcmConfig, 0, sizeof(mSpkPcmConfig));
        mSpkPcmConfig.channels = 2;
        mSpkPcmConfig.rate = SampleRate;
        mSpkPcmConfig.period_size = 1024;
        mSpkPcmConfig.period_count = 2;
        mSpkPcmConfig.format = PCM_FORMAT_S32_LE;
        mSpkPcmConfig.stop_threshold = ~(0U);  // TODO: KC: check if needed

        ASSERT(mSpkPcmOut == NULL);

        if (mSmartPaController->getI2sSetStage() & SPK_I2S_BEFORE_PCM_OPEN) {
            mSmartPaController->dspOnBoardSpeakerOn(SampleRate);
        }

        mSpkPcmOut = pcm_open(cardindex, pcmindex, PCM_OUT | PCM_MONOTONIC, &mSpkPcmConfig);

        if (pcm_is_ready(mSpkPcmOut) == false) {
            ALOGE("%s(), pcm_is_ready(%p) == false due to %s, close pcm.", __FUNCTION__, mSpkPcmOut, pcm_get_error(mSpkPcmOut));
            pcm_close(mSpkPcmOut);
            mSpkPcmOut = NULL;
        } else {
            if (pcm_start(mSpkPcmOut) != 0) {
                ALOGE("%s(), pcm_start(%p) fail due to %s", __FUNCTION__, mSpkPcmOut, pcm_get_error(mSpkPcmOut));
            }
        }
        ASSERT(mSpkPcmOut != NULL);
        ALOGV("%s(), mSpkPcmOut = %p", __FUNCTION__, mSpkPcmOut);
    }

    if (new_devices == (AUDIO_DEVICE_OUT_SPEAKER | AUDIO_DEVICE_OUT_WIRED_HEADSET) ||
        new_devices == (AUDIO_DEVICE_OUT_SPEAKER | AUDIO_DEVICE_OUT_WIRED_HEADPHONE)) {
#ifdef MTK_ANC_SUPPORT
        AudioALSAANCController::getInstance()->setHeadphoneSpeakerEnabled(true);
#endif
        OpenHeadphoneSpeakerPath(SampleRate);
    } else if (new_devices == AUDIO_DEVICE_OUT_EARPIECE) {
#ifdef MTK_ANC_SUPPORT
        AudioALSAANCController::getInstance()->setReceiverEnabled(true);
#endif
        OpenReceiverPath(SampleRate);
    }  else if (new_devices == AUDIO_DEVICE_OUT_WIRED_HEADSET ||
                new_devices == AUDIO_DEVICE_OUT_WIRED_HEADPHONE) {
        OpenHeadphonePath(SampleRate);
    } else if (new_devices & AUDIO_DEVICE_OUT_SPEAKER) {
#ifdef MTK_ANC_SUPPORT
        AudioALSAANCController::getInstance()->setSpeakerEnabled(true);
#endif
        OpenSpeakerPath(SampleRate);
    }

    return NO_ERROR;
}


status_t AudioALSAHardwareResourceManager::stopOutputDevice_l() {
    if (mOutputDevices == (AUDIO_DEVICE_OUT_SPEAKER | AUDIO_DEVICE_OUT_WIRED_HEADSET) ||
        mOutputDevices == (AUDIO_DEVICE_OUT_SPEAKER | AUDIO_DEVICE_OUT_WIRED_HEADPHONE)) {
        CloseHeadphoneSpeakerPath();
#ifdef MTK_ANC_SUPPORT
        AudioALSAANCController::getInstance()->setHeadphoneSpeakerEnabled(false);
#endif
    } else if (mOutputDevices == AUDIO_DEVICE_OUT_EARPIECE) {
        CloseReceiverPath();
#ifdef MTK_ANC_SUPPORT
        AudioALSAANCController::getInstance()->setReceiverEnabled(false);
#endif
    } else if (mOutputDevices == AUDIO_DEVICE_OUT_WIRED_HEADSET ||
               mOutputDevices == AUDIO_DEVICE_OUT_WIRED_HEADPHONE) {
        CloseHeadphonePath();
    } else if (mOutputDevices & AUDIO_DEVICE_OUT_SPEAKER) {
        CloseSpeakerPath();
#ifdef MTK_ANC_SUPPORT
        AudioALSAANCController::getInstance()->setSpeakerEnabled(false);
#endif
    }

    if (mSmartPaController->isSmartPAUsed() &&
        (mOutputDevices & AUDIO_DEVICE_OUT_SPEAKER) &&
        mSmartPaController->isAlsaCodec()) {
        if (mSpkPcmOut != NULL) {
            pcm_stop(mSpkPcmOut);
            pcm_close(mSpkPcmOut);
            mSpkPcmOut = NULL;
        }

        if (mSmartPaController->getI2sSetStage() & SPK_I2S_BEFORE_PCM_OPEN) {
            mSmartPaController->dspOnBoardSpeakerOff();
        }
    }

    mOutputDevices = AUDIO_DEVICE_NONE;

    return NO_ERROR;
}


status_t AudioALSAHardwareResourceManager::changeOutputDevice_l(const audio_devices_t new_devices, const uint32_t SampleRate) {
    stopOutputDevice_l();
    startOutputDevice_l(new_devices, SampleRate);

    return NO_ERROR;
}

bool AudioALSAHardwareResourceManager::isSharedOutDevice(audio_devices_t device) {
    // these devices cannot exist at the same time
    audio_devices_t sharedDevice = AUDIO_DEVICE_OUT_EARPIECE |
                                   AUDIO_DEVICE_OUT_SPEAKER |
                                   AUDIO_DEVICE_OUT_WIRED_HEADSET |
                                   AUDIO_DEVICE_OUT_WIRED_HEADPHONE |
                                   AUDIO_DEVICE_OUT_BLUETOOTH_SCO |
                                   AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET |
                                   AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT;

    if (isBtSpkDevice(device)) {
        return true;
    }

    if ((device & ~sharedDevice) != 0) {
        return false;
    }

    return true;
}

/**
 * input devices
 */
status_t AudioALSAHardwareResourceManager::setInputDevice(const audio_devices_t new_devices) {
    ALOGD("+%s(), new_devices = 0x%x, mStartInputDeviceCount = %d", __FUNCTION__, new_devices, mStartInputDeviceCount);

    return NO_ERROR;
}

void AudioALSAHardwareResourceManager::setMIC1Mode(bool isphonemic) {
    uint32_t micmode;

    if (isphonemic == false) {
        micmode = mPhoneMicMode;
    } else {
        micmode = mHeadsetMicMode;
    }

    ALOGD_IF(mLogEnable, "%s(), isphonemic %d, micmode %d", __FUNCTION__, isphonemic, micmode);

    if (micmode == AUDIO_MIC_MODE_ACC) {
        mDeviceConfigManager->ApplyDeviceSettingByName(AUDIOMIC1_TYPE_ACCMODE);
    } else if (micmode == AUDIO_MIC_MODE_DCC) {
        mDeviceConfigManager->ApplyDeviceSettingByName(AUDIOMIC1_TYPE_DCCMODE);
    } else if ((micmode == AUDIO_MIC_MODE_DMIC) || (micmode == AUDIO_MIC_MODE_DMIC_LP) || (micmode == AUDIO_MIC_MODE_DMIC_VENDOR01)) {
        if (IsAudioSupportFeature(AUDIO_SUPPORT_DMIC)) {
            mDeviceConfigManager->ApplyDeviceSettingByName(AUDIOMIC1_TYPE_DMICMODE);
        } else {
            mDeviceConfigManager->ApplyDeviceSettingByName(AUDIOMIC1_TYPE_ACCMODE);
        }
    } else if (micmode == AUDIO_MIC_MODE_DCCECMDIFF) {
        mDeviceConfigManager->ApplyDeviceSettingByName(AUDIOMIC1_TYPE_DCCECMDIFFMODE);
    } else if (micmode == AUDIO_MIC_MODE_DCCECMSINGLE) {
        mDeviceConfigManager->ApplyDeviceSettingByName(AUDIOMIC1_TYPE_DCCECMSINGLEMODE);
    }
}

void AudioALSAHardwareResourceManager::setMIC2Mode(bool isphonemic) {
    uint32_t micmode;

    if (isphonemic == false) {
        micmode = mPhoneMicMode;
    } else {
        micmode = mHeadsetMicMode;
    }

    ALOGD_IF(mLogEnable, "%s(), isphonemic %d, micmode %d", __FUNCTION__, isphonemic, micmode);

    if (micmode == AUDIO_MIC_MODE_ACC) {
        mDeviceConfigManager->ApplyDeviceSettingByName(AUDIOMIC2_TYPE_ACCMODE);
    } else if (micmode == AUDIO_MIC_MODE_DCC) {
        mDeviceConfigManager->ApplyDeviceSettingByName(AUDIOMIC2_TYPE_DCCMODE);
    } else if ((micmode == AUDIO_MIC_MODE_DMIC) || (micmode == AUDIO_MIC_MODE_DMIC_LP) || (micmode == AUDIO_MIC_MODE_DMIC_VENDOR01)) {
        if (IsAudioSupportFeature(AUDIO_SUPPORT_DMIC)) {
            mDeviceConfigManager->ApplyDeviceSettingByName(AUDIOMIC2_TYPE_DMICMODE);
        } else {
            mDeviceConfigManager->ApplyDeviceSettingByName(AUDIOMIC2_TYPE_ACCMODE);
        }
    } else if (micmode == AUDIO_MIC_MODE_DCCECMDIFF) {
        mDeviceConfigManager->ApplyDeviceSettingByName(AUDIOMIC2_TYPE_DCCECMDIFFMODE);
    } else if (micmode == AUDIO_MIC_MODE_DCCECMSINGLE) {
        mDeviceConfigManager->ApplyDeviceSettingByName(AUDIOMIC2_TYPE_DCCECMSINGLEMODE);
    }
}

void AudioALSAHardwareResourceManager::setMIC3Mode(bool isphonemic) {
    uint32_t micmode = isphonemic ? mHeadsetMicMode : mPhoneMicMode;

    ALOGD_IF(mLogEnable, "%s(), isphonemic %d, micmode %d", __FUNCTION__, isphonemic, micmode);

    if (micmode == AUDIO_MIC_MODE_ACC) {
        mDeviceConfigManager->ApplyDeviceSettingByName(AUDIOMIC3_TYPE_ACCMODE);
    } else if (micmode == AUDIO_MIC_MODE_DCC) {
        mDeviceConfigManager->ApplyDeviceSettingByName(AUDIOMIC3_TYPE_DCCMODE);
    } else if (micmode == AUDIO_MIC_MODE_DCCECMDIFF) {
        mDeviceConfigManager->ApplyDeviceSettingByName(AUDIOMIC3_TYPE_DCCECMDIFFMODE);
    } else if (micmode == AUDIO_MIC_MODE_DCCECMSINGLE) {
        mDeviceConfigManager->ApplyDeviceSettingByName(AUDIOMIC3_TYPE_DCCECMSINGLEMODE);
    }
}

static void setSingleDmicLoopbackType(builtin_mic_specific_type mBuiltInMicSpecificType,
                                      bool mMicInverse, bool enable) {
    bool use_single_input = IsAudioSupportFeature(AUDIO_SUPPORT_DMIC) &
                            LoopbackManager::GetInstance()->GetLoopbackType();
    AudioALSADeviceConfigManager *mDeviceConfigManager = AudioALSADeviceConfigManager::getInstance();

    if (use_single_input) {
        ALOGD("%s(), enable = %d, mBuiltInMicSpecificType = %d, mMicInverse = %d",
              __FUNCTION__, enable, mBuiltInMicSpecificType, mMicInverse);
        if (enable) {
            switch (mBuiltInMicSpecificType) {
            case BUILTIN_MIC_MIC1_ONLY:
                if (mMicInverse) {
                    mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_LOOPBACK_USE_SINGLE_MIC_R_CH);
                } else {
                    mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_LOOPBACK_USE_SINGLE_MIC_L_CH);
                }

                break;
            case BUILTIN_MIC_MIC2_ONLY:
                if (mMicInverse) {
                    mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_LOOPBACK_USE_SINGLE_MIC_L_CH);
                } else {
                    mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_LOOPBACK_USE_SINGLE_MIC_R_CH);
                }

                break;
            default:
                break;
            }
        } else {
            switch (mBuiltInMicSpecificType) {
            case BUILTIN_MIC_MIC1_ONLY:
                if (mMicInverse) {
                    mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_LOOPBACK_USE_SINGLE_MIC_R_CH);
                } else {
                    mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_LOOPBACK_USE_SINGLE_MIC_L_CH);
                }

                break;
            case BUILTIN_MIC_MIC2_ONLY:
                if (mMicInverse) {
                    mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_LOOPBACK_USE_SINGLE_MIC_L_CH);
                } else {
                    mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_LOOPBACK_USE_SINGLE_MIC_R_CH);
                }

                break;
            default:
                break;
            }
        }
    }

}

status_t AudioALSAHardwareResourceManager::startInputDevice(const audio_devices_t new_device) {
    ALOGD("+%s(), new_device = 0x%x, mInputDevice = 0x%x, mStartInputDeviceCount = %d, mMicInverse = %d, mNumPhoneMicSupport = %d",
          __FUNCTION__, new_device, mInputDevice, mStartInputDeviceCount, mMicInverse, mNumPhoneMicSupport);

    AL_AUTOLOCK(mLockin);
    bool needTurnonSequence = (new_device & ~AUDIO_DEVICE_BIT_IN) & (AUDIO_DEVICE_IN_BUILTIN_MIC | AUDIO_DEVICE_IN_BACK_MIC | AUDIO_DEVICE_IN_WIRED_HEADSET | AUDIO_DEVICE_IN_SPK_FEED);

    if (needTurnonSequence == false) {
        ALOGD("%s(), no need to config analog part", __FUNCTION__);
        return NO_ERROR;
    }

#ifdef FORCE_ROUTING_REF_MIC
    mBuiltInMicSpecificType = BUILTIN_MIC_MIC2_ONLY; /* For bring up */
#endif

    if (((mInputDevice & new_device) & ~AUDIO_DEVICE_BIT_IN) != 0) {
        ALOGW("%s(), input device already opened", __FUNCTION__);
        if (new_device != AUDIO_DEVICE_IN_SPK_FEED) {
            mStartInputDeviceCount++;
        }
        ALOGD("-%s(), mInputDevice = 0x%x, mStartInputDeviceCount = %d", __FUNCTION__, mInputDevice, mStartInputDeviceCount);
        return NO_ERROR;
    }

    int retval = 0;
    if (new_device == AUDIO_DEVICE_IN_BUILTIN_MIC) {
        setMIC1Mode(false);
        setMIC2Mode(false);
        setSingleDmicLoopbackType(mBuiltInMicSpecificType, mMicInverse, true);

        if (mBuiltInMicSpecificType == BUILTIN_MIC_MIC1_ONLY) {
            if (mMicInverse == true) {
                mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_DEVICE_BUILTIN_MIC_MIC1_INVERSE);
            } else {
                mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_DEVICE_BUILTIN_MIC_MIC1);
            }
        } else if (mBuiltInMicSpecificType == BUILTIN_MIC_MIC2_ONLY) {
            if (mMicInverse == true) {
                mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_DEVICE_BUILTIN_MIC_MIC2_INVERSE);
            } else {
                mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_DEVICE_BUILTIN_MIC_MIC2);
            }
        } else if (mBuiltInMicSpecificType == BUILTIN_MIC_MIC3_ONLY) {
            // ADC3 using DCC mode with 3rd mic input
            mDeviceConfigManager->ApplyDeviceSettingByName(AUDIOMIC3_TYPE_DCCMODE);
            mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_DEVICE_BUILTIN_MIC_MIC3);
        } else if (mBuiltInMicSpecificType == BUILTIN_MIC_MIC4_ONLY) {
            // ADC3 using DCC mode with 4th mic input
            mDeviceConfigManager->ApplyDeviceSettingByName(AUDIOMIC3_TYPE_DCCMODE);
            mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_DEVICE_BUILTIN_MIC_MIC4);
        } else if (mBuiltInMicSpecificType == BUILTIN_MIC_MIC5_ONLY) {
            // ADC3 using DCC mode with 5th mic input
            mDeviceConfigManager->ApplyDeviceSettingByName(AUDIOMIC3_TYPE_DCCMODE);
            mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_DEVICE_BUILTIN_MIC_MIC5);
        } else if (mBuiltInMicSpecificType == BUILTIN_MIC_DEFAULT) {
            switch (mNumPhoneMicSupport) {
            case 3:
                mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_DEVICE_BUILTIN_TRIPLE_MIC);
                break;
            case 2:
                mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_DEVICE_BUILTIN_DUAL_MIC);
                break;
            case 1:
                mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_DEVICE_BUILTIN_SINGLE_MIC);
                break;
            default:
                break;
            }

            if (mMicInverse == true && mNumPhoneMicSupport > 1) {
                ALOGD("%s(), need MicInverse", __FUNCTION__);
                mDeviceConfigManager->ApplyDeviceSettingByName(AUDIO_MIC_INVERSE);
            }
        }
    } else if (new_device == AUDIO_DEVICE_IN_BACK_MIC) {
        if (mNumPhoneMicSupport >= 2) {
            setMIC1Mode(false);
            setMIC2Mode(false);

            if (mMicInverse == true) {
                mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_DEVICE_BUILTIN_BACK_MIC_INVERSE);
            } else {
                mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_DEVICE_BUILTIN_BACK_MIC);
            }
        } else {
            ALOGW("%s(), mic < 2, not support", __FUNCTION__);
        }
    } else if (new_device == AUDIO_DEVICE_IN_WIRED_HEADSET) {
        setMIC1Mode(true);
        setMIC2Mode(true);
        mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_DEVICE_HEADSET_MIC);

        if (mNumHSPole == 5) {
            retval = mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_UL_LR_Swap"), "On");
        }
        ASSERT(retval == 0);
    }

    mInputDevice |= new_device;
    if (new_device != AUDIO_DEVICE_IN_SPK_FEED) {
        mStartInputDeviceCount++;
    }

    ALOGD("-%s(), mInputDevice = 0x%x, mStartInputDeviceCount = %d", __FUNCTION__, mInputDevice, mStartInputDeviceCount);
    return NO_ERROR;
}


status_t AudioALSAHardwareResourceManager::stopInputDevice(const audio_devices_t stop_device) {
    ALOGD("+%s(), mInputDevice = 0x%x, stop_device = 0x%x, mStartInputDeviceCount = %d, mMicInverse = %d, mNumPhoneMicSupport = %d",
          __FUNCTION__, mInputDevice, stop_device, mStartInputDeviceCount, mMicInverse, mNumPhoneMicSupport);

    AL_AUTOLOCK(mLockin);

    bool needTurnoffSequence = (stop_device & ~AUDIO_DEVICE_BIT_IN) & (AUDIO_DEVICE_IN_BUILTIN_MIC | AUDIO_DEVICE_IN_BACK_MIC | AUDIO_DEVICE_IN_WIRED_HEADSET | AUDIO_DEVICE_IN_SPK_FEED);

    if (needTurnoffSequence == false) {
        ALOGD("%s(), no need to config analog part", __FUNCTION__);
        return NO_ERROR;
    }

    if (((mInputDevice & stop_device) & ~AUDIO_DEVICE_BIT_IN) == 0) {
        ALOGW("%s(), input device not opened yet, do nothing", __FUNCTION__);
        return NO_ERROR;
    }

    if (stop_device != AUDIO_DEVICE_IN_SPK_FEED) {
        if (mStartInputDeviceCount > 0) {
            mStartInputDeviceCount--;
        }

        if (mStartInputDeviceCount < 0) {
            ALOGE("%s(), mStartInputDeviceCount negative %d, reset to 0!", __FUNCTION__, mStartInputDeviceCount);
            mStartInputDeviceCount = 0;
        }

        if (mStartInputDeviceCount > 0) {
            ALOGD("-%s(), mInputDevice = 0x%x, mStartInputDeviceCount = %d,return", __FUNCTION__, mInputDevice, mStartInputDeviceCount);
            return NO_ERROR;
        }
    }

    int retval = 0;

    if (stop_device == AUDIO_DEVICE_IN_BUILTIN_MIC) {
        setSingleDmicLoopbackType(mBuiltInMicSpecificType, mMicInverse, false);

        if (mBuiltInMicSpecificType == BUILTIN_MIC_MIC1_ONLY) {
            if (mMicInverse == true) {
                mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_DEVICE_BUILTIN_MIC_MIC1_INVERSE);
            } else {
                mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_DEVICE_BUILTIN_MIC_MIC1);
            }
        } else if (mBuiltInMicSpecificType == BUILTIN_MIC_MIC2_ONLY) {
            if (mMicInverse == true) {
                mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_DEVICE_BUILTIN_MIC_MIC2_INVERSE);
            } else {
                mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_DEVICE_BUILTIN_MIC_MIC2);
            }
        } else if (mBuiltInMicSpecificType == BUILTIN_MIC_MIC3_ONLY) {
            mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_DEVICE_BUILTIN_MIC_MIC3);
        } else if (mBuiltInMicSpecificType == BUILTIN_MIC_MIC4_ONLY) {
            mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_DEVICE_BUILTIN_MIC_MIC4);
        } else if (mBuiltInMicSpecificType == BUILTIN_MIC_MIC5_ONLY) {
            mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_DEVICE_BUILTIN_MIC_MIC5);
        } else if (mBuiltInMicSpecificType == BUILTIN_MIC_DEFAULT) {
            setMIC1Mode(false);
            setMIC2Mode(false);

            switch (mNumPhoneMicSupport) {
            case 3:
                mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_DEVICE_BUILTIN_TRIPLE_MIC);
                break;
            case 2:
                mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_DEVICE_BUILTIN_DUAL_MIC);
                break;
            case 1:
                mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_DEVICE_BUILTIN_SINGLE_MIC);
                break;
            default:
                break;
            }
        }
    } else if (stop_device == AUDIO_DEVICE_IN_BACK_MIC) {
        if (mNumPhoneMicSupport >= 2) {
            setMIC1Mode(false);
            setMIC2Mode(false);
            if (mMicInverse == true) {
                mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_DEVICE_BUILTIN_BACK_MIC_INVERSE);
            } else {
                mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_DEVICE_BUILTIN_BACK_MIC);
            }
        } else {
            ALOGW("%s(), mic < 2, not support", __FUNCTION__);
        }
    } else if (stop_device == AUDIO_DEVICE_IN_WIRED_HEADSET) {
        setMIC1Mode(true);
        mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_DEVICE_HEADSET_MIC);

        if (mNumHSPole == 5) {
            retval = mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_UL_LR_Swap"), "Off");
        }
        ASSERT(retval == 0);
    }

    mInputDevice &= ((~stop_device) | AUDIO_DEVICE_BIT_IN);
    if (mInputDevice == AUDIO_DEVICE_BIT_IN) { mInputDevice = AUDIO_DEVICE_NONE; }

    ALOGD("-%s(), mInputDevice = 0x%x, mStartInputDeviceCount = %d", __FUNCTION__, mInputDevice, mStartInputDeviceCount);
    return NO_ERROR;
}


status_t AudioALSAHardwareResourceManager::changeInputDevice(const audio_devices_t new_device) {
    ALOGD("+%s(), mInputDevice: 0x%x => 0x%x", __FUNCTION__, mInputDevice, new_device);

    stopInputDevice(getInputDevice());
    startInputDevice(new_device);

    ALOGD("-%s(), mInputDevice: 0x%x", __FUNCTION__, mInputDevice);
    return NO_ERROR;
}



status_t AudioALSAHardwareResourceManager::setHWGain2DigitalGain(const uint32_t gain) {
    ALOGD("%s(), gain = 0x%x", __FUNCTION__, gain);

    const uint32_t kMaxAudioHWGainValue = 0x80000;

    if (gain > kMaxAudioHWGainValue) {
        ALOGE("%s(), gain(0x%x) > kMaxAudioHWGainValue(0x%x)!! return!!", __FUNCTION__, gain, kMaxAudioHWGainValue);
        return BAD_VALUE;
    }

    int retval = 0;
    if (WCNChipController::GetInstance()->IsFMMergeInterfaceSupported() == true) {
        retval = mixer_ctl_set_value(mixer_get_ctl_by_name(mMixer, "Audio Mrgrx Volume"), 0, gain);
    } else {
        retval = mixer_ctl_set_value(mixer_get_ctl_by_name(mMixer, "Audio FM I2S Volume"), 0, gain);
    }

    if (retval != 0) {
        ALOGE("%s(), retval = %d", __FUNCTION__, retval);
        ASSERT(retval == 0);
    }

    return NO_ERROR;
}



status_t AudioALSAHardwareResourceManager::setInterruptRate(const audio_output_flags_t flag,
                                                            const uint32_t rate) {
    if (rate <= 0 || rate >= 65535) {
        ALOGE("%s, rate is not in range", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    int retval;
    if (isIsolatedDeepBuffer(flag)) {
        retval = mixer_ctl_set_value(mixer_get_ctl_by_name(mMixer, "deep_buffer_irq_cnt"), 0, rate);
    } else {
        retval = mixer_ctl_set_value(mixer_get_ctl_by_name(mMixer, "Audio IRQ1 CNT"), 0, rate);
    }

    if (retval != 0) {
        ALOGE("%s(), retval = %d", __FUNCTION__, retval);
        ASSERT(retval == 0);
    }

    return NO_ERROR;
}

status_t AudioALSAHardwareResourceManager::setInterruptRate2(const uint32_t rate) {
    if (rate <= 0 || rate >= 65535) {
        ALOGE("%s, rate is not in range", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    //int retval = mixer_ctl_set_value(mixer_get_ctl_by_name(mMixer, "Audio IRQ2 CNT"), 0, rate);
    //ASSERT(retval == 0);

    //return (retval == 0) ? NO_ERROR : UNKNOWN_ERROR;
    return NO_ERROR;
}

status_t AudioALSAHardwareResourceManager::setULInterruptRate(const uint32_t rate) {
    if (rate <= 0 || rate >= 65535) {
        ALOGE("%s, rate is not in range", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    int retval = mixer_ctl_set_value(mixer_get_ctl_by_name(mMixer, "Audio IRQ2 CNT"), 0, rate);

    if (retval != 0) {
        ALOGE("%s(), retval = %d", __FUNCTION__, retval);
        //ASSERT(retval == 0);
    }

    return NO_ERROR;
}

status_t AudioALSAHardwareResourceManager::openAddaOutput(const uint32_t sample_rate) {
    ALOGD("+%s(), sample_rate = 0x%x", __FUNCTION__, sample_rate);
    AL_AUTOLOCK(*AudioALSADriverUtility::getInstance()->getStreamSramDramLock());
    struct pcm_config mConfig;

    // DL  setting
    memset(&mConfig, 0, sizeof(mConfig));
    mConfig.channels = 2;
    mConfig.rate = sample_rate;
    mConfig.period_size = 1024;
    mConfig.period_count = 2;
    mConfig.format = PCM_FORMAT_S16_LE;
    mConfig.start_threshold = 0;
    mConfig.stop_threshold = 0;
    mConfig.silence_threshold = 0;

    ASSERT(mPcmDL == NULL);
    int pcmIdx = AudioALSADeviceParser::getInstance()->GetPcmIndexByString(keypcmI2S0Dl1Playback);
    int cardIdx = AudioALSADeviceParser::getInstance()->GetCardIndexByString(keypcmI2S0Dl1Playback);
    mPcmDL = pcm_open(cardIdx, pcmIdx, PCM_OUT, &mConfig);
    ASSERT(mPcmDL != NULL);
    ALOGV("%s(), mPcmDL = %p", __FUNCTION__, mPcmDL);

    pcm_start(mPcmDL);
    return NO_ERROR;
}


status_t AudioALSAHardwareResourceManager::closeAddaOutput() {
    ALOGD("+%s()", __FUNCTION__);
    AL_AUTOLOCK(*AudioALSADriverUtility::getInstance()->getStreamSramDramLock());
    pcm_stop(mPcmDL);
    pcm_close(mPcmDL);
    mPcmDL = NULL;

    ALOGD("-%s()", __FUNCTION__);
    return NO_ERROR;
}


status_t AudioALSAHardwareResourceManager::setSgenMode(const sgen_mode_t sgen_mode) {
    if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_SineGen_Switch"), SIDEGEN[sgen_mode])) {
        ALOGE("Error: Audio_SineGen_Switch invalid value");
    }

    return NO_ERROR;
}

status_t AudioALSAHardwareResourceManager::setSgenSampleRate(const sgen_mode_samplerate_t sample_rate) {
    ALOGD("setSgenSampleRate = %d", sample_rate);
    if (sample_rate < 0 || sample_rate >= 9) {
        ALOGE("%s, sgen_rate is not in range", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_SineGen_SampleRate"), SIDEGEN_SAMEPLRATE[sample_rate])) {
        ALOGE("Error: Audio_SineGen_SampleRate invalid value");
    }

    return NO_ERROR;
}

status_t AudioALSAHardwareResourceManager::EnableSideToneFilter(const bool enable) {
#ifdef ENABLE_SW_SIDETONE
    ALOGD("%s(), SW Sidetone Applied, skip setting HW Sidetone", __FUNCTION__);

#else
    ALOGV("EnableSideToneFilter enable = %d", enable);
    if (enable) {
        mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_DEVICE_SIDETONE);
    } else {
        mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_DEVICE_SIDETONE);
    }
#endif

    return NO_ERROR;
}

bool AudioALSAHardwareResourceManager::GetExtDacPropertyEnable() {
    return AudioALSADriverUtility::getInstance()->GetPropertyValue(PROPERTY_KEY_EXTDAC);
}

status_t AudioALSAHardwareResourceManager::SetExtDacGpioEnable(bool bEnable) {
    int retval = 0;
    // check for property first
    if (GetExtDacPropertyEnable() == true) {
        ALOGD("%s GetExtDacPropertyEnable bEnable = %d", __FUNCTION__, bEnable);
        if (bEnable == true) {
            retval = mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_I2S1_Setting"), "On");
            ASSERT(retval == 0);
        } else {
            retval = mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_I2S1_Setting"), "Off");
            ASSERT(retval == 0);
        }
        return NO_ERROR;
    }

    // if supprot for extdac
#ifdef EXT_DAC_SUPPORT
    if (bEnable == true) {
        retval = mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_I2S1_Setting"), "On");
        ASSERT(retval == 0);
    } else {
        retval = mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_I2S1_Setting"), "Off");
        ASSERT(retval == 0);
    }
#endif
    return NO_ERROR;
}


status_t AudioALSAHardwareResourceManager::setMicType(void) {
    char value[PROPERTY_VALUE_MAX];
    int ret, bflag;

    mPhoneMicMode = GetMicDeviceMode(0);
    mHeadsetMicMode = GetMicDeviceMode(1);

#ifdef UPDATE_PHONE_MIC_MODE_BY_PCB
    if (mPhoneMicMode == AUDIO_MIC_MODE_DCC) {
        mPhoneMicMode = updatePhoneMicMode();
        if (mPhoneMicMode == AUDIO_MIC_MODE_ACC) {
            mHeadsetMicMode = AUDIO_MIC_MODE_ACC;
        }
    }
#endif

    // init using phone mic mode
    setMIC1Mode(false);
    setMIC2Mode(false);

    if (mNumPhoneMicSupport >= 3) {
        setMIC3Mode(false);
    }

    return NO_ERROR;
}


status_t AudioALSAHardwareResourceManager::setSPKCurrentSensor(bool bSwitch) {
    struct mixer_ctl *ctl;
    struct mixer_ctl *ctl2;

    enum mixer_ctl_type type;
    unsigned int num_values;
    static AUDIO_SPEAKER_MODE eSpeakerMode;
    ALOGD("%s(), bSwitch = %d", __FUNCTION__, bSwitch);
    ctl = mixer_get_ctl_by_name(mMixer, "Audio_Speaker_CurrentSensing");

    if (ctl == NULL) {
        ALOGE("Kernel unsupport Audio_Speaker_CurrentSensing");
        return UNKNOWN_ERROR;
    }

    if (mixer_ctl_set_enum_by_string(ctl, bSwitch ? "On" : "Off")) {
        ALOGE("Error: Audio_Speaker_CurrentSensing invalid value : On");
    }


    ctl2 = mixer_get_ctl_by_name(mMixer, "Audio_Speaker_class_Switch");

    if (true == bSwitch) {
        //Save Speaker Mode
        ALOGD("Enable+ value [%d] [%s]", mixer_ctl_get_value(ctl2, 0), mixer_ctl_get_enum_string(ctl2, mixer_ctl_get_value(ctl2, 0)));

        if (strcmp(mixer_ctl_get_enum_string(ctl2, mixer_ctl_get_value(ctl2, 0)), "CLASSAB")) {
            eSpeakerMode = AUDIO_SPEAKER_MODE_D;
        } else {
            eSpeakerMode = AUDIO_SPEAKER_MODE_AB;
        }

        ALOGD("Current Mode [%d]", eSpeakerMode);

        if (mixer_ctl_set_enum_by_string(ctl2, "CLASSAB")) {
            ALOGE("Error: Audio_Speaker_CurrentPeakDetector invalid value");
        }

        ALOGD("Enable- [%s]", mixer_ctl_get_enum_string(ctl2, mixer_ctl_get_value(ctl2, 0)));

    } else {
        //restore Speaker Mode

        if (mixer_ctl_set_enum_by_string(ctl2, eSpeakerMode ? "CLASSAB" : "CALSSD")) {
            ALOGE("Error: Audio_Speaker_CurrentPeakDetector invalid value");
        }

        ALOGD("RollBack to [%s]", mixer_ctl_get_enum_string(ctl2, 0));
    }


    ALOGD("Audio_Speaker_CurrentSensing Get value [%d] [%s]", mixer_ctl_get_value(ctl, 0), mixer_ctl_get_enum_string(ctl, mixer_ctl_get_value(ctl, 0)));

    return NO_ERROR;
}

status_t AudioALSAHardwareResourceManager::setSPKCurrentSensorPeakDetectorReset(bool bSwitch) {
    struct mixer_ctl *ctl;
    enum mixer_ctl_type type;
    unsigned int num_values;
    ALOGD("%s(), bSwitch = %d", __FUNCTION__, bSwitch);
    ctl = mixer_get_ctl_by_name(mMixer, "Audio_Speaker_CurrentPeakDetector");

    if (ctl == NULL) {
        ALOGE("Kernel unsupport Audio_Speaker_CurrentPeakDetector");
        return UNKNOWN_ERROR;
    }

    type = mixer_ctl_get_type(ctl);
    num_values = mixer_ctl_get_num_values(ctl);
    if (mixer_ctl_set_enum_by_string(ctl, bSwitch ? "On" : "Off")) {
        ALOGE("Error: Audio_Speaker_CurrentPeakDetector invalid value");
    }

    return NO_ERROR;
}

uint32_t AudioALSAHardwareResourceManager::updatePhoneMicMode() {
    struct mixer_ctl *ctl;
    uint32_t mic_mode = mPhoneMicMode;
    unsigned int ctl_value = 0;
    ALOGD("%s()", __FUNCTION__);
    ctl = mixer_get_ctl_by_name(mMixer, "Audio_MIC_Mode");

    if (ctl == NULL) {
        ALOGD("Kernel unsupport Audio_MIC_Mode");
        return mic_mode;
    }

    ctl_value = mixer_ctl_get_value(ctl, 0);

    ALOGD("%s(), new mic mode = %d,  old mic_mode = %d", __FUNCTION__, ctl_value, mic_mode);

    return ctl_value;
}

audio_devices_t AudioALSAHardwareResourceManager::getOutputDevice() {
    return mOutputDevices;
}

audio_devices_t AudioALSAHardwareResourceManager::getInputDevice() {
    return mInputDevice & ((~AUDIO_DEVICE_IN_SPK_FEED) | AUDIO_DEVICE_BIT_IN);
}


status_t AudioALSAHardwareResourceManager::setMicInverse(bool bMicInverse) {
    ALOGD_IF(mLogEnable, "%s(), bMicInverse = %d", __FUNCTION__, bMicInverse);
    if (getNumPhoneMicSupport() >= 2) {
#if defined(MTK_AUDIO_MIC_INVERSE) || defined(DEN_PHONE_MIC_INVERSE)
        mMicInverse = !bMicInverse;
#else
        mMicInverse = bMicInverse;
#endif
    } else {
        ALOGD("%s(), not support", __FUNCTION__);
    }
    return NO_ERROR;
}

void AudioALSAHardwareResourceManager::EnableAudBufClk(bool bEanble) {
    ALOGD("%s(), bEanble = %d, mStartOutputDevicesCount %d", __FUNCTION__, bEanble, mStartOutputDevicesCount);

    if (mStartOutputDevicesCount > 0) {
        return;
    }

    enum mixer_ctl_type type;
    struct mixer_ctl *ctl;
    int retval = 0;
    ctl = mixer_get_ctl_by_name(mMixer, "AUD_CLK_BUF_Switch");

    if (ctl == NULL) {
        ALOGE("EnableAudBufClk not support");
        return ;
    }

    if (bEanble == true) {
        retval = mixer_ctl_set_enum_by_string(ctl, "On");
        ASSERT(retval == 0);
    } else {
        retval = mixer_ctl_set_enum_by_string(ctl, "Off");
        ASSERT(retval == 0);
    }
}


bool AudioALSAHardwareResourceManager::getMicInverse(void) {
#if defined(MTK_AUDIO_MIC_INVERSE) || defined(DEN_PHONE_MIC_INVERSE)
    ALOGD("%s(), mMicInverse = %d", __FUNCTION__, !mMicInverse);
    return !mMicInverse;
#else
    ALOGD("%s(), mMicInverse = %d", __FUNCTION__, mMicInverse);
    return mMicInverse;
#endif
}


void AudioALSAHardwareResourceManager::setAudioDebug(const bool enable) {
    if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_Debug_Setting"), enable ? "On" : "Off")) {
        ALOGE("Error: Audio_Debug_Setting invalid value");
    }
}

unsigned int AudioALSAHardwareResourceManager::getNonSmartPAType()
{
    return EXTAMP_LO;
}

status_t AudioALSAHardwareResourceManager::OpenReceiverPath(const uint32_t SampleRate __unused) {
    //AudioALSACodecDeviceOutReceiverPMIC::getInstance()->open();
    if (IsAudioSupportFeature(AUDIO_SUPPORT_2IN1_SPEAKER)) {
        mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_DEVICE_2IN1_SPEAKER);
    } else {
        mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_DEVICE_RECEIVER);
    }
#ifdef HAVING_RCV_SPK_SWITCH
    AudioALSACodecDeviceOutReceiverSpeakerSwitch::getInstance()->open();
#endif

    return NO_ERROR;
}

status_t AudioALSAHardwareResourceManager::CloseReceiverPath() {
    //AudioALSACodecDeviceOutReceiverPMIC::getInstance()->close();

    if (IsAudioSupportFeature(AUDIO_SUPPORT_2IN1_SPEAKER)) {
        mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_DEVICE_2IN1_SPEAKER);
    } else {
        mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_DEVICE_RECEIVER);
    }
#ifdef HAVING_RCV_SPK_SWITCH
    AudioALSACodecDeviceOutReceiverSpeakerSwitch::getInstance()->close();
#endif

    return NO_ERROR;
}

status_t AudioALSAHardwareResourceManager::OpenHeadphonePath(const uint32_t SampleRate __unused) {
#if defined(MTK_DPD_SUPPORT)
    setDPDModule(true);
#endif
    SetExtDacGpioEnable(true);
    mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_DEVICE_HEADPHONE);
#if defined(MTK_HYBRID_NLE_SUPPORT)
    AudioALSAHyBridNLEManager::getInstance()->setEnableNLE(true);
#endif
    return NO_ERROR;
}

status_t AudioALSAHardwareResourceManager::CloseHeadphonePath() {
#if defined(MTK_HYBRID_NLE_SUPPORT)
    AudioALSAHyBridNLEManager::getInstance()->setEnableNLE(false);
#endif
    mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_DEVICE_HEADPHONE);
#if defined(MTK_DPD_SUPPORT)
    setDPDModule(false);
#endif
    return NO_ERROR;
}


status_t AudioALSAHardwareResourceManager::OpenSpeakerPath(const uint32_t SampleRate) {
    SetExtDacGpioEnable(true);

    if (mSmartPaController->isSmartPAUsed()) {
        mSmartPaController->speakerOn(SampleRate, mOutputDevices);
    } else {
#if defined(SMARTPA_DYNAMIC_DETECT)
        unsigned int spkType = getNonSmartPAType();
        switch (spkType) {
        case INTAMP:
            mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_DEVICE_SPEAKER);
            break;
        case EXTAMP_LO:
            mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_DEVICE_SPEAKER);
            mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_DEVICE_EXT_SPEAKER);
            break;
        case EXTAMP_HP:
            mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_DEVICE_HEADPHONE);
            mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_DEVICE_EXT_SPEAKER);
            break;
        }
#else
#if defined(USING_EXTAMP_HP)

#if defined(USING_EXTAMP_HP_FLAG)
        if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Ext_Speaker_Use"), "On")) {
            ALOGE("Error: Ext_Speaker_Used invalid value");
        }
#endif

        mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_DEVICE_HEADPHONE);
        mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_DEVICE_EXT_SPEAKER);
#elif defined(USING_EXTAMP_LO)
        mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_DEVICE_SPEAKER);
        mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_DEVICE_EXT_SPEAKER);
//awinic add 
#ifdef awinic_mec
	property_set("af.awinic.support","true");
#endif
//awinic add 
#else
        mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_DEVICE_SPEAKER);
#endif
#endif  /* end of #if defined(SMARTPA_DYNAMIC_DETECT) */
    }

    return NO_ERROR;
}

status_t  AudioALSAHardwareResourceManager::CloseSpeakerPath() {
    if (mSmartPaController->isSmartPAUsed()) {
        mSmartPaController->speakerOff();
    } else {
#if defined(SMARTPA_DYNAMIC_DETECT)
        unsigned int spkType = getNonSmartPAType();
        switch (spkType) {
        case INTAMP:
            mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_DEVICE_SPEAKER);
            break;
        case EXTAMP_LO:
            mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_DEVICE_EXT_SPEAKER);
            mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_DEVICE_SPEAKER);
            break;
        case EXTAMP_HP:
            mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_DEVICE_EXT_SPEAKER);
            mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_DEVICE_HEADPHONE);
            break;
        }
#else
#if defined(USING_EXTAMP_HP)
        mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_DEVICE_EXT_SPEAKER);
        mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_DEVICE_HEADPHONE);
#if defined(USING_EXTAMP_HP_FLAG)
        if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Ext_Speaker_Use"), "Off")) {
            ALOGE("Error: Ext_Speaker_Used invalid value");
        }
#endif
#elif defined(USING_EXTAMP_LO)
        mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_DEVICE_EXT_SPEAKER);
        mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_DEVICE_SPEAKER);
//awinic add 
#ifdef awinic_mec
	property_set("af.awinic.support","false");
#endif
//awinic add 
#else
        mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_DEVICE_SPEAKER);
#endif
#endif  /* end of #if defined(SMARTPA_DYNAMIC_DETECT) */
    }

    SetExtDacGpioEnable(false);
    return NO_ERROR;
}

status_t  AudioALSAHardwareResourceManager::OpenHeadphoneSpeakerPath(const uint32_t SampleRate) {
    SetExtDacGpioEnable(true);

    if (mSmartPaController->isSmartPAUsed()) {
        mSmartPaController->speakerOn(SampleRate, mOutputDevices);
        mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_DEVICE_HEADPHONE);
    } else {
#if defined(SMARTPA_DYNAMIC_DETECT)
        unsigned int spkType = getNonSmartPAType();
        switch (spkType) {
        case INTAMP:
            mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_DEVICE_EARPHONE_SPEAKER);
            break;
        case EXTAMP_LO:
            mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_DEVICE_EARPHONE_SPEAKER);
            mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_DEVICE_EXT_SPEAKER);
            break;
        case EXTAMP_HP:
            mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_DEVICE_HEADPHONE);
            mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_DEVICE_EXT_SPEAKER);
            break;
        }
#else
#if defined(USING_EXTAMP_HP)
        mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_DEVICE_HEADPHONE);
        mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_DEVICE_EXT_SPEAKER);
#elif defined(USING_EXTAMP_LO)
        mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_DEVICE_EARPHONE_SPEAKER);
        mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_DEVICE_EXT_SPEAKER);
#else
        mDeviceConfigManager->ApplyDeviceTurnonSequenceByName(AUDIO_DEVICE_EARPHONE_SPEAKER);
#endif
#endif  /* end of #if defined(SMARTPA_DYNAMIC_DETECT) */
    }

    return NO_ERROR;
}

status_t  AudioALSAHardwareResourceManager::CloseHeadphoneSpeakerPath() {
    if (mSmartPaController->isSmartPAUsed()) {
        mSmartPaController->speakerOff();
        mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_DEVICE_HEADPHONE);
    } else {
#if defined(SMARTPA_DYNAMIC_DETECT)
        unsigned int spkType = getNonSmartPAType();
        switch (spkType) {
        case INTAMP:
            mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_DEVICE_EARPHONE_SPEAKER);
            break;
        case EXTAMP_LO:
            mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_DEVICE_EXT_SPEAKER);
            mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_DEVICE_EARPHONE_SPEAKER);
            break;
        case EXTAMP_HP:
            mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_DEVICE_EXT_SPEAKER);
            mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_DEVICE_HEADPHONE);
            break;
        }
#else
#if defined(USING_EXTAMP_HP)
        mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_DEVICE_EXT_SPEAKER);
        mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_DEVICE_HEADPHONE);
#elif defined(USING_EXTAMP_LO)
        mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_DEVICE_EXT_SPEAKER);
        mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_DEVICE_EARPHONE_SPEAKER);
#else
        mDeviceConfigManager->ApplyDeviceTurnoffSequenceByName(AUDIO_DEVICE_EARPHONE_SPEAKER);
#endif
#endif  /* end of #if defined(SMARTPA_DYNAMIC_DETECT) */
    }

    SetExtDacGpioEnable(false);
    return NO_ERROR;
}

status_t  AudioALSAHardwareResourceManager::OpenBuiltInMicPath() {
    return NO_ERROR;
}

status_t  AudioALSAHardwareResourceManager::CloseBuiltInMicPath() {
    return NO_ERROR;
}

status_t  AudioALSAHardwareResourceManager::OpenBackMicPath() {
    return NO_ERROR;
}

status_t  AudioALSAHardwareResourceManager::CloseBackMicPath() {
    return NO_ERROR;
}

status_t  AudioALSAHardwareResourceManager::OpenWiredHeadsetMicPath() {
    return NO_ERROR;
}

status_t  AudioALSAHardwareResourceManager::CloseWiredHeadsetMicPath() {
    return NO_ERROR;
}

void AudioALSAHardwareResourceManager::HpImpeDanceDetect(void) {
    ALOGD("+HpImpeDanceDetect");
#if defined(MTK_AUDIO_GAIN_TABLE) == defined(MTK_NEW_VOL_CONTROL)

    AL_AUTOLOCK(mLock);

    /* notify codec hp just plugged in, call get_accdet_auxadc() during accdet on */
    struct mixer_ctl *ctl = mixer_get_ctl_by_name(mMixer, "Headphone Plugged In");
    if (ctl) {
        int ret = 0;
        ret = mixer_ctl_set_enum_by_string(ctl, "On");
        if (ret) {
            ALOGE("%s(), ret %d, set Headphone Plugged In failed", __FUNCTION__, ret);
        }
    } else {
        ALOGW("%s(), Headphone Plugged In not support", __FUNCTION__);
    }

    if (mStartOutputDevicesCount != 0) {
        ALOGD("-HpImpeDanceDetect : output device Busy");
        mHeadchange = true;
        return;
    }

#ifdef MTK_NEW_VOL_CONTROL
    AudioMTKGainController::getInstance()->GetHeadPhoneImpedance();
#else
    AudioALSAVolumeController::getInstance()->GetHeadPhoneImpedance();
#endif

    mHeadchange = false;
#endif
    ALOGD("-HpImpeDanceDetect");
}

void AudioALSAHardwareResourceManager::setDPDModule(bool enable) {
    ALOGD("%s() enable = %d", __FUNCTION__, enable);

    enum mixer_ctl_type type;
    struct mixer_ctl *ctl;
    int retval = 0;

    ctl = mixer_get_ctl_by_name(mMixer, "Audio_DPD_Switch");

    if (ctl == NULL) {
        ALOGE("Audio_DPD_Switch not support");
        return;
    }

    if (enable) {
        retval = mixer_ctl_set_enum_by_string(ctl, "On");
        ASSERT(retval == 0);
    } else {
        retval = mixer_ctl_set_enum_by_string(ctl, "Off");
        ASSERT(retval == 0);
    }
    return;
}

void AudioALSAHardwareResourceManager::setHeadphoneLowPowerMode(bool enable) {
    ALOGD("%s() enable = %d", __FUNCTION__, enable);

    enum mixer_ctl_type type;
    struct mixer_ctl *ctl;
    int retval = 0;

    ctl = mixer_get_ctl_by_name(mMixer, "Audio_Power_Mode");

    if (ctl == NULL) {
        ALOGE("Audio_Power_Mode not support");
        return;
    }

    if (enable) {
        retval = mixer_ctl_set_enum_by_string(ctl, "Low_Power");
        ASSERT(retval == 0);
    } else {
        retval = mixer_ctl_set_enum_by_string(ctl, "Hifi");
        ASSERT(retval == 0);
    }
    return;
}

void AudioALSAHardwareResourceManager::setCodecSampleRate(const uint32_t sample_rate) {
    ALOGD("+%s(), sample_rate = %d", __FUNCTION__, sample_rate);

    int retval = mixer_ctl_set_value(mixer_get_ctl_by_name(mMixer, "Codec_ADC_SampleRate"), 0, sample_rate);
    if (retval != 0) {
        ALOGE("%s(), Set mixer Codec_ADC_SampleRate error(), ret = %d!!!", __FUNCTION__, retval);
    }

    retval = mixer_ctl_set_value(mixer_get_ctl_by_name(mMixer, "Codec_DAC_SampleRate"), 0, sample_rate);
    if (retval != 0) {
        ALOGE("%s(), Set mixer Codec_DAC_SampleRate error(), ret = %d!!!", __FUNCTION__, retval);
    }

    retval = mixer_ctl_set_value(mixer_get_ctl_by_name(mMixer, "Codec_ADC2_SampleRate"), 0, sample_rate);
    if (retval != 0) {
        ALOGE("%s(), Set mixer Codec_ADC2_SampleRate error(), ret = %d!!!", __FUNCTION__, retval);
    }

    ALOGD("-%s()", __FUNCTION__);
}

} // end of namespace android
