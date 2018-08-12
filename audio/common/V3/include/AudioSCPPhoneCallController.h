#ifndef ANDROID_AUDIO_SCP_PHONE_CALL_CONTROLLER_H
#define ANDROID_AUDIO_SCP_PHONE_CALL_CONTROLLER_H

#include <system/audio.h>
#include <pthread.h>
#include <vector>
#include <string>

extern "C" {
#include <alsa_device_profile.h>
#include <alsa_device_proxy.h>
}
#include <AudioLock.h>

struct mixer;
struct pcm_config;

namespace android {

class AudioSCPPhoneCallController {
public:
    static AudioSCPPhoneCallController *getInstance();
    ~AudioSCPPhoneCallController();

    int enable(unsigned int speechRate);
    int disable();
    bool isEnable();
    bool deviceSupport(const audio_devices_t output_devices);
    bool isSupportPhonecall(const audio_devices_t output_devices);

    int setUSBOutConnectionState(audio_devices_t devices, bool connect, int card, int device);
    int setUSBInConnectionState(audio_devices_t devices, bool connect, int card, int device);
    unsigned int getSpeechRate();

private:
    static AudioSCPPhoneCallController *mSCPPhoneCallController;
    AudioSCPPhoneCallController();

    unsigned int getPeriodByte(const struct pcm_config *config);
    unsigned int getPcmAvail(struct pcm *pcm);

    // debug
    static void setSCPDebugInfo(bool enable, int dbgType);

private:
    AudioLock mLock;
    bool mEnable;
    unsigned int mSpeechRate;
    struct pcm_config mConfig;
    struct pcm *mPcmIn;
    static struct mixer *mMixer;
    struct timespec mLpbkNewTime, mLpbkOldTime, mLpbkStartTime;
};

}
#endif

