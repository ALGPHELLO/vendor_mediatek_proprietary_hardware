#include "AudioSmartPaController.h"
#include "SpeechDriverFactory.h"
#include "AudioParamParser.h"
#include "AudioUtility.h"
#include "AudioALSAStreamManager.h"
#include "AudioALSASpeechPhoneCallController.h"
#include "LoopbackManager.h"
#include <system/audio.h>

#include <string>
#include <dlfcn.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "AudioSmartPaController"

namespace android {

typedef enum {
    PLAYBACK_MODE_NORMAL,
    PLAYBACK_MODE_IN_CALL,
    PLAYBACK_MODE_VOIP,
    PLAYBACK_MODE_NUM
} playback_mode_t;

typedef enum {
    PLAYBACK_DEVICE_NONE = -1,
    PLAYBACK_DEVICE_SPEAKER,
    PLAYBACK_DEVICE_RECEIVER,
    PLAYBACK_DEVICE_NUM
} playback_device_t;

/*
 * singleton
 */
AudioSmartPaController *AudioSmartPaController::mAudioSmartPaController = NULL;
AudioSmartPaController *AudioSmartPaController::getInstance() {
    static AudioLock mGetInstanceLock;
    AL_AUTOLOCK(mGetInstanceLock);

    if (mAudioSmartPaController == NULL) {
        mAudioSmartPaController = new AudioSmartPaController();
    }

    ASSERT(mAudioSmartPaController != NULL);
    return mAudioSmartPaController;
}

/*
 * constructor / destructor
 */
AudioSmartPaController::AudioSmartPaController() :
    mMixer(AudioALSADriverUtility::getInstance()->getMixer()),
    mLibHandle(NULL),
    mtk_smartpa_init(NULL) {
    // init variables
    memset(&mSmartPa, 0, sizeof(mSmartPa));

    // init process
    init();
};

AudioSmartPaController::~AudioSmartPaController() {
    deinit();

    if (mLibHandle) {
        if (dlclose(mLibHandle)) {
            ALOGE("%s(), dlclose failed, dlerror = %s", __FUNCTION__, dlerror());
        }
    }
};

/*
 * function implementations
 */
int AudioSmartPaController::init() {
    int ret;

    ret = initSmartPaAttribute();
    if (ret) {
        ALOGE("%s(), initSmartPaAttribute failed, ret = %d", __FUNCTION__, ret);
        return ret;
    }

    if (!mSmartPa.attribute.isSmartPAUsed) {
        return 0;
    }

    ret = initSmartPaRuntime();
    if (ret) {
        ALOGE("%s(), initSmartPaRuntime failed, ret = %d", __FUNCTION__, ret);
        ASSERT(ret != 0);
        return ret;
    }

    // load lib
    ALOGD("%s(), dlopen lib path: %s", __FUNCTION__, mSmartPa.attribute.spkLibPath);
    mLibHandle = dlopen(mSmartPa.attribute.spkLibPath, RTLD_NOW);

    if (!mLibHandle) {
        ALOGW("%s(), dlopen failed, dlerror = %s", __FUNCTION__, dlerror());
    } else {
        mtk_smartpa_init = (int (*)(struct SmartPa *))dlsym(mLibHandle, "mtk_smartpa_init");
        if (!mtk_smartpa_init) {
            ALOGW("%s(), dlsym failed, dlerror = %s", __FUNCTION__, dlerror());
        }
    }

    // lib init
    if (mtk_smartpa_init) {
        ret = mtk_smartpa_init(&mSmartPa);
        if (ret) {
            ALOGE("%s(), mtk_smartpa_init failed, ret = %d", __FUNCTION__, ret);
            ASSERT(ret != 0);
            return ret;
        }
    }

#ifndef MTK_APLL_DEFAULT_OFF
    if (mSmartPa.attribute.isApllNeeded) {
        if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_always_hd_Switch"), "On")) {
            ALOGE("Error: Audio_always_hd_Switch invalid value");
        }
    }
#endif

    // reset
    speakerOff();
    dspOnBoardSpeakerOff();

    // callback init
    if (mSmartPa.ops.init) {
        if (getI2sSetStage() & SPK_I2S_AUDIOSERVER_INIT) {
            if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_i2s0_hd_Switch"), "On")) {
                ALOGE("Error: Audio_i2s0_hd_Switch invalid value");
            }

            if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_i2s0_SideGen_Switch"), "On44100")) {
                ALOGE("Error: Audio_i2s0_SideGen_Switch invalid value");
            }
        }

        mSmartPa.ops.init(&mSmartPa);

        if (getI2sSetStage() & SPK_I2S_AUDIOSERVER_INIT) {
            if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_i2s0_SideGen_Switch"), "Off")) {
                ALOGE("Error: Audio_i2s0_SideGen_Switch invalid value");
            }

            if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_i2s0_hd_Switch"), "Off")) {
                ALOGE("Error: Audio_i2s0_hd_Switch invalid value");
            }
        }

    }

    return ret;
}

int AudioSmartPaController::deinit() {
    if (mSmartPa.ops.deinit) {
        mSmartPa.ops.deinit();
    }

    return 0;
}

bool AudioSmartPaController::isSmartPAUsed() {
    if (mSmartPa.attribute.isSmartPAUsed)
        return true;
    else
        return false;
}

int AudioSmartPaController::initSpkAmpType() {
    struct mixer_ctl *ctl = mixer_get_ctl_by_name(mMixer, "speaker_amp_type_query");

    if (ctl == NULL) {
        return -1;
    }

    return mixer_ctl_get_value(ctl, 0);
}

int AudioSmartPaController::initSmartPaAttribute() {
    struct SmartPaAttribute *attr = &mSmartPa.attribute;
    int ampType = initSpkAmpType();
    AppOps *appOps = appOpsGetInstance();
    if (appOps == NULL) {
        ALOGE("%s(), Error: AppOps == NULL", __FUNCTION__);
        return -ENOENT;
    }

    AppHandle *appHandle = appOps->appHandleGetInstance();
    const char *spkType = appOps->appHandleGetFeatureOptionValue(appHandle, "MTK_AUDIO_SPEAKER_PATH");
    const char audioTypeName[] = "SmartPa";

#if !defined(MTK_SMARTPA_DUMMY_LIB)
    // update amp type
    switch (ampType) {
    case SPK_RICHTEK_RT5509:
        spkType = "smartpa_richtek_rt5509";
        break;
    default:
        break;
    }
#endif

    if (ampType < 0 && strstr(spkType, "smartpa")) {
        attr->isSmartPAUsed = true;
    } else {
        attr->isSmartPAUsed = ampType > 0 ? true : false;
    }

    ALOGD("%s(), spkType = %s, amp_type = %d, isSmartPAUsed = %d\n",
          __FUNCTION__, spkType, ampType, attr->isSmartPAUsed);

    if (!attr->isSmartPAUsed) {
        return 0;
    }

    // extract parameters from xml
    AudioType *audioType;
    audioType = appOps->appHandleGetAudioTypeByName(appHandle, audioTypeName);
    if (!audioType) {
        ALOGW("error: get audioType fail, audioTypeName = %s", audioTypeName);
        ASSERT(false);
        return -ENOENT;
    }

    // Read lock
    appOps->audioTypeReadLock(audioType, __FUNCTION__);

    ParamUnit *paramUnit;
    std::string paramName(spkType);
    paramName = "Speaker type," + paramName;
    paramUnit = appOps->audioTypeGetParamUnit(audioType, paramName.c_str());
    if (!paramUnit) {
        ALOGW("error: get paramUnit fail, spkType = %s", paramName.c_str());
        appOps->audioTypeUnlock(audioType);
        ASSERT(false);
        return -ENOENT;
    }

    Param *param;
    param = appOps->paramUnitGetParamByName(paramUnit, "have_dsp");
    ASSERT(param);
    attr->dspType = *(int *)param->data;
    ALOGD("\tattr->dspType = %d", attr->dspType);

    param = appOps->paramUnitGetParamByName(paramUnit, "chip_delay_us");
    ASSERT(param);
    attr->chipDelayUs = *(unsigned int *)param->data;
    ALOGD("\tattr->chipDelayUs = %d", attr->chipDelayUs);

    // load lib path
    if (In64bitsProcess()) {
        param = appOps->paramUnitGetParamByName(paramUnit, "spk_lib64_path");
    } else {
        param = appOps->paramUnitGetParamByName(paramUnit, "spk_lib_path");
    }
    ASSERT(param);
    ASSERT(sizeof(attr->spkLibPath) / sizeof(char) > strlen((char *)param->data));
    memcpy(attr->spkLibPath, param->data, strlen((char *)param->data));
    ALOGD("\tattr->spkLibPath = %s", attr->spkLibPath);

    // get supported sample rate list, max rate, min rate
    param = appOps->paramUnitGetParamByName(paramUnit, "supported_rate_list");
    ASSERT(param);
    ALOGD("\tsupported_rate_list param->arraySize = %zu", param->arraySize);

    if (param->arraySize * sizeof(attr->supportedRateList[0]) < sizeof(attr->supportedRateList)) {
        memcpy(attr->supportedRateList, param->data, param->arraySize * sizeof(unsigned int));
    } else {
        ALOGE("%s(), support rate list too much", __FUNCTION__);
    }

    //get if is alsa codec
    param = appOps->paramUnitGetParamByName(paramUnit, "is_alsa_codec");
    ASSERT(param);
    attr->isAlsaCodec = *(int *)param->data;
    ALOGD("\tattr->is_alsa_codec = %d", attr->isAlsaCodec);

    //get codec control name, for not dsp supported SmartPA
    param = appOps->paramUnitGetParamByName(paramUnit, "codec_ctl_name");
    ASSERT(param);
    ASSERT(sizeof(attr->codecCtlName) / sizeof(char) > strlen((char *)param->data));
    memcpy(attr->codecCtlName, param->data, strlen((char *)param->data));
    ALOGD("\tattr->codec_ctl_name = %s", attr->codecCtlName);

    //get is_apll_needed
    param = appOps->paramUnitGetParamByName(paramUnit, "is_apll_needed");
    ASSERT(param);
    attr->isApllNeeded = *(int *)param->data;
    ALOGD("\tattr->is_apll_needed = %d", attr->isApllNeeded);

    //get i2s_set_stage
    param = appOps->paramUnitGetParamByName(paramUnit, "i2s_set_stage");
    ASSERT(param);
    attr->i2sSetStage = *(unsigned int *)param->data;
    ALOGD("\tattr->i2sSetStage = %d", attr->i2sSetStage);

    // Unlock
    appOps->audioTypeUnlock(audioType);

    ALOGD("%s(), supported rate:", __FUNCTION__);
    attr->supportedRateMax = 0;
    attr->supportedRateMin = UINT_MAX;
    for (size_t i = 0; i * sizeof(attr->supportedRateList[0]) < sizeof(attr->supportedRateList); i++) {
        if (attr->supportedRateList[i] == 0) {
            break;
        }

        if (attr->supportedRateList[i] > attr->supportedRateMax) {
            attr->supportedRateMax = attr->supportedRateList[i];
        }

        if (attr->supportedRateList[i] < attr->supportedRateMin) {
            attr->supportedRateMin = attr->supportedRateList[i];
        }

        ALOGD("\t[%zu] = %u Hz", i, attr->supportedRateList[i]);
    }

    return 0;
}

int AudioSmartPaController::initSmartPaRuntime() {
    mSmartPa.runtime.sampleRate = 44100;
    mSmartPa.runtime.mode = PLAYBACK_MODE_NORMAL;
    mSmartPa.runtime.device = PLAYBACK_DEVICE_SPEAKER;

    return 0;
}

int AudioSmartPaController::speakerOn(unsigned int sampleRate, unsigned int device) {
    ALOGD("%s(), sampleRate = %d, device = %d", __FUNCTION__, sampleRate, device);
    int ret = 0;

    // set runtime
    mSmartPa.runtime.sampleRate = sampleRate;

    // mixer ctrl
    if (getI2sSetStage() & SPK_I2S_BEFORE_SPK_ON) {
        ret = dspOnBoardSpeakerOn(sampleRate);
    }

    if (strlen(mSmartPa.attribute.codecCtlName)) {
        ret = mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, mSmartPa.attribute.codecCtlName), "On");
        if (ret) {
            ALOGE("Error: %s invalid value, ret = %d", mSmartPa.attribute.codecCtlName, ret);
        }
    }

    // speakerOn callback
    if (mSmartPa.ops.speakerOn) {
        setSmartPaRuntime(device);
        mSmartPa.ops.speakerOn(&mSmartPa.runtime);
    }

    return ret;
}

int AudioSmartPaController::speakerOff() {
    ALOGD("%s()", __FUNCTION__);

    int ret = 0;

    // speakerOff callback
    if (mSmartPa.ops.speakerOff) {
        mSmartPa.ops.speakerOff();
    }

    // mixer ctrl
    if (getI2sSetStage() & SPK_I2S_BEFORE_SPK_ON) {
        ret = dspOnBoardSpeakerOff();
    }

    if (strlen(mSmartPa.attribute.codecCtlName)) {
        ret = mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, mSmartPa.attribute.codecCtlName), "Off");
        if (ret) {
            ALOGE("Error: %s invalid value, ret = %d", mSmartPa.attribute.codecCtlName, ret);
        }
    }

    return ret;
}

unsigned int AudioSmartPaController::getSmartPaDelayUs() {
    return mSmartPa.attribute.chipDelayUs;
}

unsigned int AudioSmartPaController::getMaxSupportedRate() {
    return mSmartPa.attribute.supportedRateMax;
}

unsigned int AudioSmartPaController::getMinSupportedRate() {
    return mSmartPa.attribute.supportedRateMin;
}

bool AudioSmartPaController::isRateSupported(unsigned int rate) {
    struct SmartPaAttribute *attr = &mSmartPa.attribute;

    for (size_t i = 0; i * sizeof(attr->supportedRateList[0]) < sizeof(attr->supportedRateList); i++) {
        if (rate == attr->supportedRateList[i]) {
            return true;
        }
    }
    return false;
}

bool AudioSmartPaController::isAlsaCodec() {
    if (mSmartPa.attribute.isAlsaCodec) {
        return true;
    } else {
        return false;
    }
}

bool AudioSmartPaController::isApSideSpkProtect() {
    if (mSmartPa.attribute.dspType == SPK_ONBOARD_DSP) {
        return false;
    } else {
        return true;
    }
}

unsigned int AudioSmartPaController::getSpkProtectType() {
    return mSmartPa.attribute.dspType;
}

unsigned int AudioSmartPaController::getI2sSetStage() {
    return mSmartPa.attribute.i2sSetStage;
}

int AudioSmartPaController::dspOnBoardSpeakerOn(unsigned int sampleRate) {
    int ret = 0;
    modem_index_t modem_index = SpeechDriverFactory::GetInstance()->GetActiveModemIndex();
    ALOGD("+%s(), SampleRate = %d, MD_type = %d, Audio_i2s0_hd_Switch = %d\n",
          __FUNCTION__, sampleRate, modem_index, mSmartPa.attribute.isApllNeeded);

    if (mSmartPa.attribute.isApllNeeded) {
        if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_i2s0_hd_Switch"), "On")) {
            ALOGE("Error: Audio_i2s0_hd_Switch invalid value");
        }
    }

    /* Config echo reference */
    if ((AudioALSAStreamManager::getInstance()->isModeInPhoneCall() && !isApSideSpkProtect()) ||
        LoopbackManager::GetInstance()->CheckIsModemLoopback(LoopbackManager::GetInstance()->GetLoopbackType()) ||
        AudioALSASpeechPhoneCallController::getInstance()->isAudioTaste()) {
        ALOGD("Enable speaker echo reference path for MD");
        if (modem_index == MODEM_1) {
            if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_ExtCodec_EchoRef_Switch"), "MD1")) {
                ALOGE("Error: Audio_ExtCodec_EchoRef_Switch MD1 invalid value");
            }
        } else {
            if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_ExtCodec_EchoRef_Switch"), "MD3")) {
                ALOGE("Error: Audio_ExtCodec_EchoRef_Switch MD3 invalid value");
            }
        }
    }

    /* Enable SmartPa i2s */
    switch (sampleRate) {
    case 8000:
        ret = mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_i2s0_SideGen_Switch"), "On8000");
        break;
    case 16000:
        ret = mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_i2s0_SideGen_Switch"), "On16000");
        break;
    case 32000:
        ret = mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_i2s0_SideGen_Switch"), "On32000");
        break;
    case 44100:
        ret = mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_i2s0_SideGen_Switch"), "On44100");
        break;
    case 48000:
        ret = mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_i2s0_SideGen_Switch"), "On48000");
        break;
    case 96000:
        ret = mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_i2s0_SideGen_Switch"), "On96000");
        break;
    case 192000:
        ret = mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_i2s0_SideGen_Switch"), "On192000");
        break;
    }

    if (ret > 0) {
        ALOGE("%s(), ERROR: Audio_i2s0_SideGen_Switch, ret = %d, samplerate = %d\n", __FUNCTION__, ret, sampleRate);
    }

    return 0;
}

int AudioSmartPaController::dspOnBoardSpeakerOff() {
    ALOGD("+%s()", __FUNCTION__);

    if (mixer_ctl_get_value(mixer_get_ctl_by_name(mMixer, "Audio_i2s0_SideGen_Switch"), 0) > 0) {
        if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_i2s0_SideGen_Switch"), "Off")) {
            ALOGE("Error: Audio_i2s0_SideGen_Switch invalid value");
        }
    }

    if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_ExtCodec_EchoRef_Switch"), "Off")) {
        ALOGE("Error: Audio_ExtCodec_EchoRef_Switch invalid value");
    }

    if (mSmartPa.attribute.isApllNeeded) {
        ALOGV("+%s(), Audio_i2s0_hd_Switch off", __FUNCTION__);
        if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_i2s0_hd_Switch"), "Off")) {
            ALOGE("Error: Audio_i2s0_hd_Switch invalid value");
        }
    }

    return 0;
}

void AudioSmartPaController::setSmartPaRuntime(unsigned int device) {
    struct SmartPaRuntime *runtime = &mSmartPa.runtime;

    /* Get information of playback mode */
    if (AudioALSAStreamManager::getInstance()->isModeInPhoneCall()) {
        runtime->mode = PLAYBACK_MODE_IN_CALL;
    } else if (AudioALSAStreamManager::getInstance()->isModeInVoipCall()) {
        runtime->mode = PLAYBACK_MODE_VOIP;
    } else {
        runtime->mode = PLAYBACK_MODE_NORMAL;
    }

    /* Get information of output device */
    runtime->device = transformDeviceIndex(device);

    ALOGD("+%s(), device = %d, mode = %d", __FUNCTION__, runtime->device, runtime->mode);
}

int AudioSmartPaController::transformDeviceIndex(const unsigned int device) {
    unsigned int ret;

    if (device & AUDIO_DEVICE_OUT_SPEAKER) {
        ret = PLAYBACK_DEVICE_SPEAKER;
    } else if (device == AUDIO_DEVICE_OUT_EARPIECE) {
        ret = PLAYBACK_DEVICE_RECEIVER;
    } else {
        ALOGE("%s(), no such device supported.", __FUNCTION__);
        ret = PLAYBACK_DEVICE_NONE;
        ASSERT(false);
    }

    return ret;
}

}
