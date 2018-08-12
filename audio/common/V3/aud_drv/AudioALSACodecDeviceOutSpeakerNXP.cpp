#include "AudioALSACodecDeviceOutSpeakerNXP.h"
#include "AudioALSAStreamManager.h"

#include <AudioLock.h>
#include "audio_custom_exp.h"

#include <mtk_tfa98xx_interface.h>

#define APLL_ON //Low jitter Mode Set

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "AudioALSACodecDeviceOutSpeakerNXP"

namespace android {

AudioALSACodecDeviceOutSpeakerNXP *AudioALSACodecDeviceOutSpeakerNXP::mAudioALSACodecDeviceOutSpeakerNXP = NULL;
AudioALSACodecDeviceOutSpeakerNXP *AudioALSACodecDeviceOutSpeakerNXP::getInstance() {
    static AudioLock mGetInstanceLock;
    AL_AUTOLOCK(mGetInstanceLock);

    if (mAudioALSACodecDeviceOutSpeakerNXP == NULL) {
        mAudioALSACodecDeviceOutSpeakerNXP = new AudioALSACodecDeviceOutSpeakerNXP();
    }
    ASSERT(mAudioALSACodecDeviceOutSpeakerNXP != NULL);
    return mAudioALSACodecDeviceOutSpeakerNXP;
}


AudioALSACodecDeviceOutSpeakerNXP::AudioALSACodecDeviceOutSpeakerNXP() {
    ALOGD("%s()", __FUNCTION__);
    // use default samplerate to load setting.
#ifndef MTK_APLL_DEFAULT_OFF
    if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_always_hd_Switch"), "On")) {
        ALOGE("Error: Audio_always_hd_Switch invalid value");
    }
#endif

    open();
    close();
}


AudioALSACodecDeviceOutSpeakerNXP::~AudioALSACodecDeviceOutSpeakerNXP() {
    ALOGD("%s()", __FUNCTION__);

    MTK_Tfa98xx_Deinit();
}


status_t AudioALSACodecDeviceOutSpeakerNXP::open() {
#ifndef EXTCODEC_ECHO_REFERENCE_SUPPORT
    MTK_Tfa98xx_SetBypassDspIncall(1);
#endif

    return open(44100);
}

status_t AudioALSACodecDeviceOutSpeakerNXP::open(const uint32_t SampleRate) {
    ALOGD("+%s(), mClientCount = %d, SampleRate = %d", __FUNCTION__, mClientCount, SampleRate);
    modem_index_t modem_index = SpeechDriverFactory::GetInstance()->GetActiveModemIndex();
    int ret;

    if (mClientCount == 0) {
#ifdef APLL_ON
        ALOGD("+%s(), Audio_i2s0_hd_Switch on", __FUNCTION__);
        if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_i2s0_hd_Switch"), "On")) {
            ALOGE("Error: Audio_i2s0_hd_Switch invalid value");
        }
#endif
        /* Config echo reference */
        if (AudioALSAStreamManager::getInstance()->isModeInPhoneCall()) {
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
        switch (SampleRate) {
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

        MTK_Tfa98xx_SetSampleRate(SampleRate);
        MTK_Tfa98xx_SpeakerOn();
#ifdef EXTCODEC_ECHO_REFERENCE_SUPPORT
        //Echo Reference configure will be set on all sample rate
        MTK_Tfa98xx_EchoReferenceConfigure(1);
#endif

#if defined(MTK_SPEAKER_MONITOR_SUPPORT)
        //AudioALSASpeakerMonitor::getInstance()->Activate();
        //AudioALSASpeakerMonitor::getInstance()->EnableSpeakerMonitorThread(true);
#endif
    }

    mClientCount++;

    ALOGD("-%s(), mClientCount = %d", __FUNCTION__, mClientCount);
    return NO_ERROR;
}


status_t AudioALSACodecDeviceOutSpeakerNXP::close() {
    ALOGD("+%s(), mClientCount = %d", __FUNCTION__, mClientCount);

    mClientCount--;

    if (mClientCount == 0) {
        MTK_Tfa98xx_SpeakerOff();
        if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_i2s0_SideGen_Switch"), "Off")) {
            ALOGE("Error: Audio_i2s0_SideGen_Switch invalid value");
        }
#ifdef EXTCODEC_ECHO_REFERENCE_SUPPORT
        ALOGD("%s(), Audio_ExtCodec_EchoRef_Switch off", __FUNCTION__);
        if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_ExtCodec_EchoRef_Switch"), "Off")) {
            ALOGE("Error: Audio_ExtCodec_EchoRef_Switch invalid value");
        }
#endif
#ifdef APLL_ON
        ALOGD("+%s(), Audio_i2s0_hd_Switch off", __FUNCTION__);
        if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_i2s0_hd_Switch"), "Off")) {
            ALOGE("Error: Audio_i2s0_hd_Switch invalid value");
        }
#endif

#if defined(MTK_SPEAKER_MONITOR_SUPPORT)
        //AudioALSASpeakerMonitor::getInstance()->EnableSpeakerMonitorThread(false);
        //AudioALSASpeakerMonitor::getInstance()->Deactivate();
#endif
    }

    ALOGD("-%s(), mClientCount = %d", __FUNCTION__, mClientCount);
    return NO_ERROR;
}


} // end of namespace android
