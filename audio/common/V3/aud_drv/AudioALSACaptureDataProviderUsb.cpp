#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "AudioALSACaptureDataProviderUsb"

#include "AudioALSACaptureDataProviderUsb.h"
//#include "AudioType.h"

#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <sys/prctl.h>

#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <log/log.h>
#include <cutils/str_parms.h>
#include <cutils/properties.h>
#include "AudioALSADriverUtility.h"

#include <hardware/audio.h>
//#include <hardware/audio_alsaops.h>
#include <hardware/hardware.h>

#include <system/audio.h>

#include "IAudioALSACaptureDataClient.h"
#include "AudioALSAStreamManager.h"

extern "C" {
    //#include <tinyalsa/asoundlib.h>
#include "alsa_device_profile.h"
#include "alsa_device_proxy.h"
#include "alsa_logging.h"
#include <audio_utils/channels.h>
}


#define calc_time_diff(x,y) ((x.tv_sec - y.tv_sec )+ (double)( x.tv_nsec - y.tv_nsec ) / (double)1000000000)
#ifdef DEBUG_TIMESTAMP
#define SHOW_TIMESTAMP(format, args...) ALOGD(format, ##args)
#else
#define SHOW_TIMESTAMP(format, args...)
#endif

namespace android {


/*==============================================================================
 *                     Constant
 *============================================================================*/


static uint32_t kReadBufferSize = 0;
static alsa_device_proxy *usbProxy = NULL;
static bool usbVoipMode = false;
//static FILE *pDCCalFile = NULL;
static bool btempDebug = false;

/*==============================================================================
 *                     Implementation
 *============================================================================*/

AudioALSACaptureDataProviderUsb *AudioALSACaptureDataProviderUsb::mAudioALSACaptureDataProviderUsb = NULL;

AudioALSACaptureDataProviderUsb *AudioALSACaptureDataProviderUsb::getInstance() {
    static AudioLock mGetInstanceLock;
    AL_AUTOLOCK(mGetInstanceLock);

    if (mAudioALSACaptureDataProviderUsb == NULL) {
        mAudioALSACaptureDataProviderUsb = new AudioALSACaptureDataProviderUsb();
    }
    ASSERT(mAudioALSACaptureDataProviderUsb != NULL);
    return mAudioALSACaptureDataProviderUsb;
}

AudioALSACaptureDataProviderUsb::AudioALSACaptureDataProviderUsb():
    hReadThread(0) {
    ALOGD("%s()", __FUNCTION__);
    memset(&mNewtime, 0, sizeof(mNewtime));
    memset(&mOldtime, 0, sizeof(mOldtime));
    memset(timerec, 0, sizeof(timerec));
    memset((void *)&mCaptureStartTime, 0, sizeof(mCaptureStartTime));
    memset((void *)&mEstimatedBufferTimeStamp, 0, sizeof(mEstimatedBufferTimeStamp));
    mlatency = 16;
}

AudioALSACaptureDataProviderUsb::~AudioALSACaptureDataProviderUsb() {
    ALOGD("%s()", __FUNCTION__);
}

void AudioALSACaptureDataProviderUsb::initUsbInfo(stream_attribute_t stream_attribute_source_usb, alsa_device_proxy *proxy, size_t buffer_size, bool enable) {
    usbProxy = proxy;
    kReadBufferSize = (uint32_t)buffer_size;
    usbVoipMode = false;
    mStreamAttributeSource = stream_attribute_source_usb;
    mStreamAttributeSource.BesRecord_Info.besrecord_voip_enable = false;
    mStreamAttributeSource.mVoIPEnable = false;
    mStreamAttributeSource.audio_mode = AUDIO_MODE_NORMAL;
    mPcmStatus = NO_ERROR;
    bool audiomode = AudioALSAStreamManager::getInstance()->isModeInVoipCall();
    ALOGD("%s(), kReadBufferSize = %d, enable = %d, mStreamAttributeSource.input_source = %d ,audiomode = %d", __FUNCTION__, kReadBufferSize, enable, mStreamAttributeSource.input_source, audiomode);

    if ((mStreamAttributeSource.input_source == AUDIO_SOURCE_VOICE_COMMUNICATION) || (audiomode == true)) {
        usbVoipMode = true;
        if (enable == true) {
            mStreamAttributeSource.BesRecord_Info.besrecord_voip_enable = true;
            mStreamAttributeSource.mVoIPEnable = true;
            mStreamAttributeSource.audio_mode = AUDIO_MODE_IN_COMMUNICATION;
        } else {
            // LIB Parser error handling
            mStreamAttributeSource.input_source = AUDIO_SOURCE_MIC;
        }
    }
}

bool AudioALSACaptureDataProviderUsb::isNeedEchoRefData() {
    ALOGD("%s(), usbVoipMode = %d, mStreamAttributeSource.input_source = %d", __FUNCTION__, usbVoipMode, mStreamAttributeSource.input_source);
    if (usbVoipMode == true) {
        return true;
    }
    return false;
}

status_t AudioALSACaptureDataProviderUsb::open() {
    ALOGD("%s()", __FUNCTION__);

    ASSERT(mEnable == false);
    mCaptureDataProviderType = CAPTURE_PROVIDER_USB;
    /* Reset frames readed counter & mCaptureStartTime */
    mStreamAttributeSource.Time_Info.total_frames_readed = 0;
    memset((void *)&mCaptureStartTime, 0, sizeof(mCaptureStartTime));
    memset((void *)&mEstimatedBufferTimeStamp, 0, sizeof(mEstimatedBufferTimeStamp));
    mEnable = true;

    int ret_thread = pthread_create(&hReadThread, NULL, AudioALSACaptureDataProviderUsb::readThread, (void *)this);
    if (ret_thread != 0) {
        ALOGD("%s(),pthread_create fail", __FUNCTION__);
        mEnable = false;
        pthread_join(hReadThread, NULL);
        proxy_close(usbProxy);
        //ASSERT((ret == 0)&&(ret_thread == 0));
        mPcmStatus = BAD_VALUE;
        return mPcmStatus;
    } else {
        mPcmStatus = NO_ERROR;
    }
    mPcm = usbProxy->pcm;
    OpenPCMDump(LOG_TAG);
    return NO_ERROR;
}


status_t AudioALSACaptureDataProviderUsb::close() {
    ALOGD("%s(), kReadBufferSize = %d", __FUNCTION__, kReadBufferSize);
    if (mEnable == true) {
        mEnable = false;
        pthread_join(hReadThread, NULL);
        ALOGD("pthread_join hReadThread done");
        ClosePCMDump();
        mPcm = NULL;
    }
    return NO_ERROR;
}

void *AudioALSACaptureDataProviderUsb::readThread(void *arg) {
    ALOGD("+%s1(), kReadBufferSize = %d", __FUNCTION__, kReadBufferSize);
    int ret = 0;

    status_t retval = NO_ERROR;
    AudioALSACaptureDataProviderUsb *pDataProvider = static_cast<AudioALSACaptureDataProviderUsb *>(arg);

    uint32_t open_index = pDataProvider->mOpenIndex;

    char nameset[32];
    sprintf(nameset, "%s%d", __FUNCTION__, pDataProvider->mCaptureDataProviderType);
    prctl(PR_SET_NAME, (unsigned long)nameset, 0, 0, 0);
    pDataProvider->setThreadPriority();
    ALOGD("+%s2(), pid: %d, tid: %d, kReadBufferSize = %d, open_index=%d", __FUNCTION__, getpid(), gettid(), kReadBufferSize, open_index);


    // read raw data from alsa driver
    char linear_buffer[kReadBufferSize];
    uint32_t Read_Size = kReadBufferSize;
    while (pDataProvider->mEnable == true) {
        if (open_index != pDataProvider->mOpenIndex) {
            ALOGD("%s(), open_index(%d) != mOpenIndex(%d), return", __FUNCTION__, open_index, pDataProvider->mOpenIndex);
            break;
        }

        clock_gettime(CLOCK_REALTIME, &pDataProvider->mNewtime);
        pDataProvider->timerec[0] = calc_time_diff(pDataProvider->mNewtime, pDataProvider->mOldtime);
        pDataProvider->mOldtime = pDataProvider->mNewtime;

        int retval = proxy_read(usbProxy, linear_buffer, kReadBufferSize);
        if (retval != 0) {
            ALOGD("%s(), proxy_read fail", __FUNCTION__);
            pDataProvider->provideCaptureDataToAllClients(open_index);
            usleep(15000);
            mPcmStatus = BAD_VALUE;
            continue;
        }
        mPcmStatus = NO_ERROR;
        retval = pDataProvider->GetCaptureTimeStamp(&pDataProvider->mStreamAttributeSource.Time_Info, kReadBufferSize);
        clock_gettime(CLOCK_REALTIME, &pDataProvider->mNewtime);
        pDataProvider->timerec[1] = calc_time_diff(pDataProvider->mNewtime, pDataProvider->mOldtime);
        pDataProvider->mOldtime = pDataProvider->mNewtime;
        /* Update capture start time if needed */
        pDataProvider->updateStartTimeStamp(pDataProvider->mStreamAttributeSource.Time_Info.timestamp_get);

        //use ringbuf format to save buffer info
        pDataProvider->mPcmReadBuf.pBufBase = linear_buffer;
        pDataProvider->mPcmReadBuf.bufLen   = kReadBufferSize + 1; // +1: avoid pRead == pWrite
        pDataProvider->mPcmReadBuf.pRead    = linear_buffer;
        pDataProvider->mPcmReadBuf.pWrite   = linear_buffer + kReadBufferSize;

        /* update capture timestamp by start time */
        pDataProvider->updateCaptureTimeStampByStartTime(kReadBufferSize);

        pDataProvider->provideCaptureDataToAllClients(open_index);

        clock_gettime(CLOCK_REALTIME, &pDataProvider->mNewtime);
        pDataProvider->timerec[2] = calc_time_diff(pDataProvider->mNewtime, pDataProvider->mOldtime);
        pDataProvider->mOldtime = pDataProvider->mNewtime;
        if (pDataProvider->mPCMDumpFile || btempDebug) {
            ALOGD("%s, latency_in_us,%1.6lf,%1.6lf,%1.6lf", __FUNCTION__, pDataProvider->timerec[0], pDataProvider->timerec[1], pDataProvider->timerec[2]);
        }
    }

    ALOGD("-%s(), pid: %d, tid: %d", __FUNCTION__, getpid(), gettid());
    pthread_exit(NULL);
    return NULL;


}

status_t AudioALSACaptureDataProviderUsb::updateStartTimeStamp(struct timespec timeStamp) {
    if (mCaptureStartTime.tv_sec == 0 && mCaptureStartTime.tv_nsec == 0) {
        mCaptureStartTime = timeStamp;

        ALOGD("%s(), set start timestamp = %ld.%09ld",
              __FUNCTION__, mCaptureStartTime.tv_sec, mCaptureStartTime.tv_nsec);

        return NO_ERROR;
    }

    return INVALID_OPERATION;
}

status_t AudioALSACaptureDataProviderUsb::updateCaptureTimeStampByStartTime(uint32_t readBytes) {
    ALOGV("%s()", __FUNCTION__);

    if (mCaptureStartTime.tv_sec == 0 && mCaptureStartTime.tv_nsec == 0) {
        ALOGW("No valid mCaptureStartTime! Don't update timestamp info.");
        return BAD_VALUE;
    }

    /* Update timeInfo structure */
    uint32_t bytesPerSample = audio_bytes_per_sample(mStreamAttributeSource.audio_format);
    if (bytesPerSample == 0) {
        ALOGW("audio_format is invalid! (%d)", mStreamAttributeSource.audio_format);
        return BAD_VALUE;
    }
    uint32_t readFrames = readBytes / bytesPerSample / mStreamAttributeSource.num_channels;
    time_info_struct_t *timeInfo = &mStreamAttributeSource.Time_Info;

    timeInfo->frameInfo_get = 0;    // Already counted in mCaptureStartTime
    timeInfo->buffer_per_time = 0;  // Already counted in mCaptureStartTime
    timeInfo->kernelbuffer_ns = 0;
    calculateTimeStampByFrames(mCaptureStartTime, timeInfo->total_frames_readed, mStreamAttributeSource, &timeInfo->timestamp_get);

    /* Update total_frames_readed after timestamp calculation */
    timeInfo->total_frames_readed += readFrames;

    ALOGV("%s(), read size = %d, readFrames = %d (bytesPerSample = %d, ch = %d, new total_frames_readed = %d), timestamp = %ld.%09ld -> %ld.%09ld",
          __FUNCTION__,
          readBytes, readFrames, bytesPerSample, mStreamAttributeSource.num_channels, timeInfo->total_frames_readed,
          mCaptureStartTime.tv_sec, mCaptureStartTime.tv_nsec,
          timeInfo->timestamp_get.tv_sec, timeInfo->timestamp_get.tv_nsec);

    /* Write time stamp to cache to avoid getCapturePosition performance issue */
    AL_LOCK(mTimeStampLock);
    mCaptureFramesReaded = timeInfo->total_frames_readed;
    mCaptureTimeStamp = timeInfo->timestamp_get;
    AL_UNLOCK(mTimeStampLock);

    return NO_ERROR;
}


} // end of namespace android
