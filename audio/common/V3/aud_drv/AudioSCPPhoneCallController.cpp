#include "AudioSCPPhoneCallController.h"
#include <audio_utils/format.h>
#include <tinyalsa/asoundlib.h>
#include <math.h>
#include <sys/resource.h>

#include "SpeechDriverFactory.h"
#include "AudioALSADriverUtility.h"
#include "AudioALSADeviceParser.h"
#include "AudioVolumeFactory.h"
#include "AudioALSAHardwareResourceManager.h"
#include "AudioUtility.h"
#include "SpeechDriverFactory.h"
#include "AudioSmartPaController.h"

#ifdef HAVE_AEE_FEATURE
#include <aee.h>
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "AudioSCPPhoneCallController"
#define calc_time_diff(x,y) ((x.tv_sec - y.tv_sec )+ (double)( x.tv_nsec - y.tv_nsec ) / (double)1000000000)
#define SCP_SPH_LATENCY_MS (10) // in ms
#define SCP_SPH_Interrupt_MS (5) // in ms


namespace android {

AudioSCPPhoneCallController *AudioSCPPhoneCallController::mSCPPhoneCallController = NULL;
struct mixer *AudioSCPPhoneCallController::mMixer = NULL;

AudioSCPPhoneCallController *AudioSCPPhoneCallController::getInstance() {
    static AudioLock mGetInstanceLock;
    AL_AUTOLOCK(mGetInstanceLock);

    if (!mSCPPhoneCallController) {
        mSCPPhoneCallController = new AudioSCPPhoneCallController();
    }

    return mSCPPhoneCallController;
}

AudioSCPPhoneCallController::AudioSCPPhoneCallController() {
    mEnable = 0;
    mSpeechRate = 0;
    memset((void *)&mConfig, 0, sizeof(pcm_config));
    mPcmIn = NULL;
    memset(&mLpbkNewTime, 0, sizeof(struct timespec));
    memset(&mLpbkOldTime, 0, sizeof(struct timespec));
    memset(&mLpbkStartTime, 0, sizeof(struct timespec));

    if (mMixer == NULL) {
        mMixer = AudioALSADriverUtility::getInstance()->getMixer();
        ASSERT(mMixer != NULL);
    }

}

AudioSCPPhoneCallController::~AudioSCPPhoneCallController() {

}

int AudioSCPPhoneCallController::enable(unsigned int speechRate) {
    modem_index_t md = SpeechDriverFactory::GetInstance()->GetActiveModemIndex();
    speechRate = 48000;

    ALOGD("+%s(), mEnable %d, md %d, rate %u ", __FUNCTION__, mEnable, md, speechRate);

    AL_AUTOLOCK(mLock);

    if (mEnable) {
        ALOGW("%s(), already enabled, mEnable %d", __FUNCTION__, mEnable);
        return INVALID_OPERATION;
    }

    // set enable flag
    mEnable = true;
    mSpeechRate = speechRate;
    int PcmInIdx = 0;
    int PcmOutIdx = 0;
    int CardIndex = 0;

    // set debug info
    char value[PROPERTY_VALUE_MAX];

    // set modem
    if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_Scp_Voice_MD_Select"), md == MODEM_1 ? "md1" : "md2")) {
        ALOGE("Error: SCP_voice_Modem_Select invalid value");
    }

    // set interrupt interval
    unsigned int Irq_period = (speechRate * SCP_SPH_Interrupt_MS) / 1000;
    int retval = mixer_ctl_set_value(mixer_get_ctl_by_name(mMixer, "Scp_Voice_Irq_Cnt"), 0, Irq_period);
    if (retval != 0) {
        ALOGE("%s(), retval = %d", __FUNCTION__, retval);
        ASSERT(retval == 0);
    }

    PcmInIdx = AudioALSADeviceParser::getInstance()->GetPcmIndexByString(keypcmScpVoicePlayback);
    PcmOutIdx = AudioALSADeviceParser::getInstance()->GetPcmIndexByString(keypcmScpVoicePlayback);
    CardIndex = AudioALSADeviceParser::getInstance()->GetCardIndexByString(keypcmScpVoicePlayback);

    memset(&mConfig, 0, sizeof(mConfig));
    mConfig.channels = 2;
    mConfig.rate = speechRate;
    mConfig.period_size = (SCP_SPH_LATENCY_MS  * speechRate) / 1000;
    mConfig.period_count = 2;
    mConfig.format = PCM_FORMAT_S32_LE;
    mConfig.start_threshold = 0;
    mConfig.stop_threshold = 0;
    mConfig.silence_threshold = 0;

    ASSERT(mPcmIn == NULL);

    mPcmIn = pcm_open(CardIndex, PcmOutIdx, PCM_IN, &mConfig);
    pcm_start(mPcmIn);

    /* debug information*/
    if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "Audio_Scp_Voice_MD_Select"), md == MODEM_1 ? "md1" : "md2")) {
        ALOGE("Error: SCP_voice_Modem_Select invalid value");
    }

    ALOGD("-%s()", __FUNCTION__);
    return 0;
}

int AudioSCPPhoneCallController::disable() {
    ALOGD("+%s(), mEnable %d", __FUNCTION__, mEnable);

    AL_AUTOLOCK(mLock);

    if (!mEnable) {
        ALOGW("%s(), already disabled, mEnable %d", __FUNCTION__, mEnable);
        return INVALID_OPERATION;
    }

    if (mPcmIn != NULL) {
        pcm_stop(mPcmIn);
        pcm_close(mPcmIn);
        mPcmIn = NULL;
    }

    mEnable = false;

    ALOGD("-%s()", __FUNCTION__);
    return 0;
}

bool AudioSCPPhoneCallController::deviceSupport(const audio_devices_t output_devices) {
    if (output_devices == AUDIO_DEVICE_OUT_SPEAKER) {
        return true;
    }
    return false;
}

bool AudioSCPPhoneCallController::isSupportPhonecall(const audio_devices_t output_devices) {
    if (AudioSmartPaController::getInstance()->isSmartPAUsed()) {
        if (deviceSupport(output_devices) &&
            AudioSmartPaController::getInstance()->getSpkProtectType() == SPK_APSCP_DSP) {
            return true;
        }
    }
    return false;
}

bool AudioSCPPhoneCallController::isEnable() {
    ALOGD("%s isEnable = %d", __FUNCTION__, mEnable);
    return mEnable;
}

unsigned int AudioSCPPhoneCallController::getSpeechRate() {
    return mSpeechRate;
}

unsigned int AudioSCPPhoneCallController::getPeriodByte(const struct pcm_config *config) {
    return config->period_size * config->channels * (pcm_format_to_bits(config->format) / 8);
}

void AudioSCPPhoneCallController::setSCPDebugInfo(bool enable, int dbgType) {
    int previousDebugEnable = mixer_ctl_get_value(mixer_get_ctl_by_name(mMixer, "ScpSpk_Voice_Debug"), 0);
    int debugEnable = 0;

    if (enable) {
        debugEnable = dbgType | previousDebugEnable;
    } else {
        debugEnable = (~dbgType) & previousDebugEnable;
    }


    ALOGD("%s(), enable %d, dbgType 0x%x, previousDebugEnable 0x%x, debugEnable 0x%x",
          __FUNCTION__, enable, dbgType, previousDebugEnable, debugEnable);

    mixer_ctl_set_value(mixer_get_ctl_by_name(mMixer, "ScpSpk_Voice_Debug"), 0, debugEnable);
}

}

