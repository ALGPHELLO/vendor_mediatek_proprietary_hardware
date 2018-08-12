#include "AudioALSAPlaybackHandlerVoice.h"

#include "SpeechDriverFactory.h"
#include "SpeechBGSPlayer.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "AudioALSAPlaybackHandlerVoice"

#define calc_time_diff(x,y) ((x.tv_sec - y.tv_sec )+ (double)( x.tv_nsec - y.tv_nsec ) / (double)1000000000)

namespace android {

AudioALSAPlaybackHandlerVoice::AudioALSAPlaybackHandlerVoice(const stream_attribute_t *stream_attribute_source) :
    AudioALSAPlaybackHandlerBase(stream_attribute_source),
    mBGSPlayer(BGSPlayer::GetInstance()) {
    mSpeechDriver = NULL;
    mPlaybackHandlerType = PLAYBACK_HANDLER_VOICE;
    mBGSPlayBuffer = NULL;

    memset((void *)&mNewtime, 0, sizeof(mNewtime));
    memset((void *)&mOldtime, 0, sizeof(mOldtime));
    mlatency = 0;

    if (mStreamAttributeSource->mAudioOutputFlags & (AUDIO_OUTPUT_FLAG_FAST | AUDIO_OUTPUT_FLAG_PRIMARY)) {
        const uint8_t size_per_channel = audio_bytes_per_sample(mStreamAttributeSource->audio_format);
        const uint8_t size_per_frame = mStreamAttributeSource->num_channels * size_per_channel;
        mlatency = (mStreamAttributeSource->buffer_size + 0.0) / (mStreamAttributeSource->sample_rate
                                                                  * size_per_frame);
        if (mStreamAttributeSource->mAudioOutputFlags & AUDIO_OUTPUT_FLAG_FAST) {
            mlatency -= 0.001;  // avoid MD can't pull data on time.
        } else {
            mlatency -= 0.004;
        }

        //ALOGD("%s, mlatency %1.6lf, buffer_size %d, sample_rate %d, size_per_frame %d",
        //      __FUNCTION__, mlatency, mStreamAttributeSource->buffer_size,
        //      mStreamAttributeSource->sample_rate, size_per_frame);
    }
}


AudioALSAPlaybackHandlerVoice::~AudioALSAPlaybackHandlerVoice() {
}

status_t AudioALSAPlaybackHandlerVoice::open() {
    // debug pcm dump
    OpenPCMDump(LOG_TAG);


    // HW attribute config // TODO(Harvey): query this
    mStreamAttributeTarget.audio_format = AUDIO_FORMAT_PCM_16_BIT;
    mStreamAttributeTarget.audio_channel_mask = mStreamAttributeSource->audio_channel_mask; // same as source stream
    mStreamAttributeTarget.num_channels = popcount(mStreamAttributeTarget.audio_channel_mask);
    mStreamAttributeTarget.sample_rate = BGS_TARGET_SAMPLE_RATE; // same as source stream
    mStreamAttributeTarget.u8BGSDlGain = mStreamAttributeSource->u8BGSDlGain;
    mStreamAttributeTarget.u8BGSUlGain = mStreamAttributeSource->u8BGSUlGain;
    mStreamAttributeTarget.buffer_size = BGS_PLAY_BUFFER_LEN;

    ALOGD("%s(), audio_mode: %d, u8BGSUlGain: %d, u8BGSDlGain: %d"
          ", audio_format: %d => %d, sample_rate: %u => %u, flag: 0x%x",
          __FUNCTION__, mStreamAttributeSource->audio_mode,
          mStreamAttributeSource->u8BGSUlGain,
          mStreamAttributeSource->u8BGSDlGain,
          mStreamAttributeSource->audio_format,
          mStreamAttributeTarget.audio_format,
          mStreamAttributeSource->sample_rate,
          mStreamAttributeTarget.sample_rate,
          mStreamAttributeSource->mAudioOutputFlags);


    // bit conversion
    initBitConverter();


    // open background sound
    AL_LOCK(mBGSPlayer->mBGSMutex);

    if (mStreamAttributeTarget.num_channels > 2) {
        mBGSPlayBuffer = mBGSPlayer->CreateBGSPlayBuffer(
                             mStreamAttributeSource->sample_rate,
                             2,
                             mStreamAttributeTarget.audio_format);

    } else {
        mBGSPlayBuffer = mBGSPlayer->CreateBGSPlayBuffer(
                             mStreamAttributeSource->sample_rate,
                             mStreamAttributeTarget.num_channels,
                             mStreamAttributeTarget.audio_format);
    }
    mSpeechDriver = SpeechDriverFactory::GetInstance()->GetSpeechDriver();
    mBGSPlayer->Open(mSpeechDriver, mStreamAttributeTarget.u8BGSUlGain, mStreamAttributeTarget.u8BGSDlGain);

    AL_UNLOCK(mBGSPlayer->mBGSMutex);

    return NO_ERROR;
}


status_t AudioALSAPlaybackHandlerVoice::close() {
    ALOGD("%s(), flag: 0x%x", __FUNCTION__, mStreamAttributeSource->mAudioOutputFlags);

    // close background sound
    AL_LOCK(mBGSPlayer->mBGSMutex);

    mBGSPlayer->Close();

    mBGSPlayer->DestroyBGSPlayBuffer(mBGSPlayBuffer);

    AL_UNLOCK(mBGSPlayer->mBGSMutex);


    // bit conversion
    deinitBitConverter();


    // debug pcm dump
    ClosePCMDump();

    return NO_ERROR;
}


status_t AudioALSAPlaybackHandlerVoice::routing(const audio_devices_t output_devices __unused) {
    return INVALID_OPERATION;
}

uint32_t AudioALSAPlaybackHandlerVoice::ChooseTargetSampleRate(uint32_t SampleRate) {
    ALOGD("ChooseTargetSampleRate SampleRate = %d ", SampleRate);
    uint32_t TargetSampleRate = 44100;
    if ((SampleRate % 8000) == 0) { // 8K base
        TargetSampleRate = 48000;
    }
    return TargetSampleRate;
}
int AudioALSAPlaybackHandlerVoice::pause() {
    return -ENODATA;
}

int AudioALSAPlaybackHandlerVoice::resume() {
    return -ENODATA;
}

int AudioALSAPlaybackHandlerVoice::flush() {
    return 0;
}

status_t AudioALSAPlaybackHandlerVoice::setVolume(uint32_t vol __unused) {
    return INVALID_OPERATION;
}

int AudioALSAPlaybackHandlerVoice::drain(audio_drain_type_t type __unused) {
    return 0;
}

ssize_t AudioALSAPlaybackHandlerVoice::write(const void *buffer, size_t bytes) {
    ALOGV("%s()", __FUNCTION__);

    if (mSpeechDriver == NULL) {
        ALOGW("%s(), mSpeechDriver == NULL!!", __FUNCTION__);
        return bytes;
    }

    if (mSpeechDriver->CheckModemIsReady() == false) {
        uint32_t sleep_ms = getBufferLatencyMs(mStreamAttributeSource, bytes);
        if (sleep_ms != 0) {
            ALOGW("%s(), modem not ready, sleep %u ms", __FUNCTION__, sleep_ms);
            usleep(sleep_ms * 1000);
        }
        return bytes;
    }

    void *newbuffer[96 * 1024] = {0};
    unsigned char *aaa;
    unsigned char *bbb;
    size_t i = 0;
    size_t j = 0;
    int retval = 0;
    // const -> to non const
    void *pBuffer = const_cast<void *>(buffer);
    ASSERT(pBuffer != NULL);

    aaa = (unsigned char *)newbuffer;
    bbb = (unsigned char *)buffer;


    if (mStreamAttributeSource->audio_format == AUDIO_FORMAT_PCM_16_BIT) {
        if (mStreamAttributeTarget.num_channels == 8) {
            for (i = 0 ; j < bytes; i += 4) {
                memcpy(aaa + i, bbb + j, 4);
                j += 16;
            }
            bytes = (bytes >> 2);
        } else if (mStreamAttributeTarget.num_channels == 6) {
            for (i = 0 ; j < bytes; i += 4) {
                memcpy(aaa + i, bbb + j, 4);
                j += 12;
            }
            bytes = (bytes / 3);
        } else {
            memcpy(aaa, bbb, bytes);
        }
    } else {
        if (mStreamAttributeTarget.num_channels == 8) {
            for (i = 0 ; j < bytes; i += 8) {
                memcpy(aaa + i, bbb + j, 8);
                j += 32;
            }
            bytes = (bytes >> 2);
        } else if (mStreamAttributeTarget.num_channels == 6) {
            for (i = 0 ; j < bytes; i += 8) {
                memcpy(aaa + i, bbb + j, 8);
                j += 24;
            }
            bytes = (bytes / 3);
        } else {
            memcpy(aaa, bbb, bytes);
        }

    }

    // bit conversion
    void *pBufferAfterBitConvertion = NULL;
    uint32_t bytesAfterBitConvertion = 0;
    doBitConversion(newbuffer, bytes, &pBufferAfterBitConvertion, &bytesAfterBitConvertion);


    // write data to background sound
    WritePcmDumpData(pBufferAfterBitConvertion, bytesAfterBitConvertion);
    uint32_t u4WrittenBytes = BGSPlayer::GetInstance()->Write(mBGSPlayBuffer, pBufferAfterBitConvertion, bytesAfterBitConvertion);
    if (u4WrittenBytes != bytesAfterBitConvertion) { // TODO: 16/32
        ALOGE("%s(), BGSPlayer::GetInstance()->Write() error, u4WrittenBytes(%u) != bytesAfterBitConvertion(%u)", __FUNCTION__, u4WrittenBytes, bytesAfterBitConvertion);
    }

    if (mStreamAttributeSource->mAudioOutputFlags & (AUDIO_OUTPUT_FLAG_FAST | AUDIO_OUTPUT_FLAG_PRIMARY)) {
        double latencyTime;
        double ns = 0;

        clock_gettime(CLOCK_MONOTONIC, &mNewtime);
        latencyTime = calc_time_diff(mNewtime, mOldtime);
        if (latencyTime < mlatency) {
            ns = mlatency - latencyTime;
            usleep(ns * 1000000);
        }
        clock_gettime(CLOCK_MONOTONIC, &mOldtime);

        //ALOGD("latency %1.3lf, ns %1.3lf", latencyTime, ns);
    }

    return bytes;
}


} // end of namespace android
