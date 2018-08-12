#ifndef ANDROID_AUDIO_MIXER_OUT_H
#define ANDROID_AUDIO_MIXER_OUT_H

#include <system/audio.h>
#include <utils/KeyedVector.h>
#include <pthread.h>

#include "AudioType.h"
#include "AudioUtility.h"
#include "AudioLock.h"

namespace android {

class MtkAudioSrcBase;
class AudioALSAPlaybackHandlerBase;
class AudioALSAStreamManager;

struct MixerOutClient {
    const void *id;
    stream_attribute_t attribute;
    RingBuf dataBuffer;

    bool suspend;

    AudioLock *waitFreeSpaceLock;

    // blisrc
    MtkAudioSrcBase *blisrc;
    char *blisrcOutBuffer;

    // bit convert
    audio_format_t dstFmt;
    audio_format_t srcFmt;
    char *bitConvertBuffer;
};

typedef KeyedVector<const void *, struct MixerOutClient *> MixerOutClientVector;

struct MixerOutInfo {
    const void *id;
    stream_attribute_t attribute;

    AudioLock *threadLock;
    AudioLock *waitSuspendLock;
    AudioLock *waitOutThreadLock;

    MixerOutClientVector *clients;
    bool clientAllSuspend;

    // bit convert
    audio_format_t dstFmt;
    audio_format_t srcFmt;
    char *bitConvertBuffer;

    unsigned int readBufferTimeUs;
};

class AudioMixerOut {
public:
    static AudioMixerOut *getInstance();
    ~AudioMixerOut();

    status_t attach(const void *id, const stream_attribute_t *attribute);
    void detach(const void *id);

    size_t write(const void *id, const void *buffer, size_t bytes);
    status_t setSuspend(const void *id, bool suspend);

    // TODO: need getPresentationPosition?

private:
    static AudioMixerOut *mAudioMixerOut;
    AudioMixerOut();

    status_t createOutThread();
    void destroyOutThread();

    void deleteClient(struct MixerOutClient *client);

    static void *outThread(void *arg);

    static int destroyPlaybackHandler(AudioALSAPlaybackHandlerBase *playbackHandler,
                                      AudioALSAStreamManager *streamManager);
    static bool clientAllSuspend(const MixerOutClientVector *clients);
    static int waitSignal(AudioLock *mWaitLock, unsigned int waitTimeUs, const char *dbgString);

    // dump
    static FILE *mixerOutDumpOpen(const char *name, const char *property);
    static void mixerOutDumpClose(FILE *file);
    static void mixerOutDumpWriteData(FILE *file, const void *buffer, size_t bytes);

    // blisrc
    static status_t initBliSrc(struct MixerOutClient *client, const struct MixerOutInfo *outInfo);
    static status_t deinitBliSrc(struct MixerOutClient *client);
    static status_t doBliSrc(struct MixerOutClient *client, void *pInBuffer, uint32_t inBytes,
                             void **ppOutBuffer, uint32_t *pOutBytes);

    // bit convert
    static unsigned int getBitConvertDstBufferSize(audio_format_t dstFmt,
                                                   audio_format_t srcFmt,
                                                   unsigned int srcBufSizeByte);
    static status_t initBitConverter(struct MixerOutClient *client, audio_format_t dstFmt);
    static status_t initBitConverter(struct MixerOutInfo *info, audio_format_t srcFmt);
    template<class T>
    static status_t initBitConverter(T *client, audio_format_t srcFmt, audio_format_t dstFmt);
    template<class T>
    static status_t deinitBitConverter(T *client);
    template<class T>
    static status_t doBitConversion(T *client,
                                    void *pInBuffer, uint32_t inBytes,
                                    void **ppOutBuffer, uint32_t *pOutBytes);
private:
    AudioLock mLock;
    AudioLock mWaitSuspendLock;
    AudioLock mWaitOutThreadLock;
    AudioLock mThreadLock;

    struct MixerOutInfo mOutInfo;

    pthread_t mOutThread;

    /**
     * client vector
     */
    MixerOutClientVector mClients;
    KeyedVector<const void *, AudioLock *> mClientsLock;

    int mDebugType;
};

}
#endif

