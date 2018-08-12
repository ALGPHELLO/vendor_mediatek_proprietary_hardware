#ifndef ANDROID_AUDIO_ALSA_PLAYBACK_HANDLER_VOICE_H
#define ANDROID_AUDIO_ALSA_PLAYBACK_HANDLER_VOICE_H

#include "AudioALSAPlaybackHandlerBase.h"

namespace android {

class BGSPlayer;
class BGSPlayBuffer;
class SpeechDriverInterface;


class AudioALSAPlaybackHandlerVoice : public AudioALSAPlaybackHandlerBase {
public:
    AudioALSAPlaybackHandlerVoice(const stream_attribute_t *stream_attribute_source);
    virtual ~AudioALSAPlaybackHandlerVoice();


    /**
     * open/close audio hardware
     */
    virtual status_t open();
    virtual status_t close();
    virtual int pause();
    virtual int resume();
    virtual int flush();
    virtual int drain(audio_drain_type_t type);

    virtual status_t routing(const audio_devices_t output_devices);
    virtual status_t setVolume(uint32_t vol);


    /**
     * write data to audio hardware
     */
    virtual ssize_t  write(const void *buffer, size_t bytes);

    uint32_t ChooseTargetSampleRate(uint32_t SampleRate);


    /**
     * get hardware buffer info (framecount)
     */
    virtual status_t getHardwareBufferInfo(time_info_struct_t *HWBuffer_Time_Info __unused) { return INVALID_OPERATION; }



private:
    SpeechDriverInterface *mSpeechDriver;
    BGSPlayer *mBGSPlayer;
    BGSPlayBuffer *mBGSPlayBuffer;

    struct timespec mNewtime, mOldtime;
    double mlatency;
};

} // end namespace android

#endif // end of ANDROID_AUDIO_ALSA_PLAYBACK_HANDLER_VOICE_H
