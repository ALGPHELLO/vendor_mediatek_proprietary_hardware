#include "AudioALSACaptureDataProviderBase.h"

#include <inttypes.h>
#include <utils/threads.h>

#include "AudioType.h"
#include <AudioLock.h>

#include <audio_lock.h>

#include "IAudioALSACaptureDataClient.h"
#include "AudioALSAHardwareResourceManager.h"


#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "AudioALSACaptureDataProviderBase"

namespace android {
int AudioALSACaptureDataProviderBase::mDumpFileNum = 0;
status_t AudioALSACaptureDataProviderBase::mPcmStatus = NO_ERROR;
AudioALSACaptureDataProviderBase::AudioALSACaptureDataProviderBase() :
    mHardwareResourceManager(AudioALSAHardwareResourceManager::getInstance()),
    mCaptureFramesReaded(0),
    mEnable(false),
    mOpenIndex(0),
    mPcm(NULL),
    mStart(true),
    mReadThreadReady(true),
    mCaptureDataProviderType(CAPTURE_PROVIDER_BASE),
    mPcmflag(0),
    audio_pcm_read_wrapper_fp(NULL) {
    ALOGD("%s(), %p", __FUNCTION__, this);

    mCaptureDataClientVector.clear();

    memset((void *)&mPcmReadBuf, 0, sizeof(mPcmReadBuf));

    memset((void *)&mConfig, 0, sizeof(mConfig));

    memset((void *)&mStreamAttributeSource, 0, sizeof(mStreamAttributeSource));

    memset((void *)&mCaptureTimeStamp, 0, sizeof(timespec));

    mPCMDumpFile = NULL;
    mPCMDumpFile4ch = NULL;

    mlatency = UPLINK_NORMAL_LATENCY_MS;

    audio_pcm_read_wrapper_fp = pcm_read;

    mPcmStatus = NO_ERROR;
}

AudioALSACaptureDataProviderBase::~AudioALSACaptureDataProviderBase() {
    ALOGD("%s(), %p", __FUNCTION__, this);
}


status_t AudioALSACaptureDataProviderBase::openPcmDriver(const unsigned int device) {
    return openPcmDriverWithFlag(device, PCM_IN);
}

status_t AudioALSACaptureDataProviderBase::openPcmDriverWithFlag(const unsigned int device, unsigned int flag) {
    ALOGD("+%s(), pcm device = %d", __FUNCTION__, device);

    ASSERT(mPcm == NULL);
    mPcmflag = flag;
    mPcm = pcm_open(AudioALSADeviceParser::getInstance()->GetCardIndex(), device, flag, &mConfig);
    if (mPcm == NULL) {
        ALOGE("%s(), mPcm == NULL!!", __FUNCTION__);
    } else if (pcm_is_ready(mPcm) == false) {
        ALOGE("%s(), pcm_is_ready(%p) == false due to %s, close pcm.", __FUNCTION__, mPcm, pcm_get_error(mPcm));
        pcm_close(mPcm);
        mPcm = NULL;
    } else {
        pcm_start(mPcm);
    }

    if (flag & PCM_MMAP) {
        audio_pcm_read_wrapper_fp = pcm_mmap_read;
    } else {
        audio_pcm_read_wrapper_fp = pcm_read;
    }

    ALOGD("-%s(), mPcm = %p", __FUNCTION__, mPcm);
    ASSERT(mPcm != NULL);
    return NO_ERROR;
}

status_t AudioALSACaptureDataProviderBase::closePcmDriver() {
    ALOGD("+%s(), mPcm = %p", __FUNCTION__, mPcm);

    if (mPcm != NULL) {
        pcm_stop(mPcm);
        pcm_close(mPcm);
        mPcm = NULL;
    }

    ALOGD("-%s(), mPcm = %p", __FUNCTION__, mPcm);
    return NO_ERROR;
}

void AudioALSACaptureDataProviderBase::enablePmicInputDevice(bool enable) {
    if (mCaptureDataProviderType == CAPTURE_PROVIDER_NORMAL)
    {
        ALOGV("%s(+), enable=%d", __FUNCTION__, enable);
        if (enable == true)
        {
            mHardwareResourceManager->startInputDevice(mStreamAttributeSource.input_device);
        } else {
            mHardwareResourceManager->stopInputDevice(mStreamAttributeSource.input_device);
        }
    }
}

void AudioALSACaptureDataProviderBase::attach(IAudioALSACaptureDataClient *pCaptureDataClient) {
    uint32_t size = 0;

    AL_LOCK(mEnableLock);

    // add client
    AL_LOCK(mClientLock);
    ALOGD("%s(), %p, mCaptureDataClientVector.size()=%u, Identity=%p", __FUNCTION__, this,
          (uint32_t)mCaptureDataClientVector.size(),
          pCaptureDataClient->getIdentity());
    mCaptureDataClientVector.add(pCaptureDataClient->getIdentity(), pCaptureDataClient);
    size = (uint32_t)mCaptureDataClientVector.size();
    AL_UNLOCK(mClientLock);

    // open pcm interface when 1st attach
    if (size == 1) {
        mOpenIndex++;
        open();
    } else {
        enablePmicInputDevice(true);
    }

    AL_UNLOCK(mEnableLock);
    ALOGV("%s(-), size=%u", __FUNCTION__, size);
}


void AudioALSACaptureDataProviderBase::detach(IAudioALSACaptureDataClient *pCaptureDataClient) {
    uint32_t size = 0;

    AL_LOCK(mEnableLock);

    // remove client
    AL_LOCK(mClientLock);
    ALOGD("%s(), %p, mCaptureDataClientVector.size()=%u, Identity=%p", __FUNCTION__, this,
          (uint32_t)mCaptureDataClientVector.size(),
          pCaptureDataClient->getIdentity());
    mCaptureDataClientVector.removeItem(pCaptureDataClient->getIdentity());
    size = (uint32_t)mCaptureDataClientVector.size();
    AL_UNLOCK(mClientLock);


    enablePmicInputDevice(false);

    // close pcm interface when there is no client attached
    if (size == 0) {
        close();
    }

    AL_UNLOCK(mEnableLock);
    ALOGV("%s(-), size=%u", __FUNCTION__, size);
}


void AudioALSACaptureDataProviderBase::provideCaptureDataToAllClients(const uint32_t open_index) {
    ALOGV("+%s()", __FUNCTION__);

    if (open_index != mOpenIndex) {
        ALOGD("%s(), open_index(%d) != mOpenIndex(%d), return", __FUNCTION__, open_index, mOpenIndex);
        return;
    }

    IAudioALSACaptureDataClient *pCaptureDataClient = NULL;

    WritePcmDumpData();

    AL_LOCK(mClientLock);
    for (size_t i = 0; i < mCaptureDataClientVector.size(); i++) {
        pCaptureDataClient = mCaptureDataClientVector[i];
        pCaptureDataClient->copyCaptureDataToClient(mPcmReadBuf);
    }
    AL_UNLOCK(mClientLock);

    ALOGV("-%s()", __FUNCTION__);
}


bool AudioALSACaptureDataProviderBase::isNeedSyncPcmStart() {
    bool retval = false;

    AL_LOCK(mClientLock);
    retval = (mCaptureDataClientVector.size() == 0)
             ? false
             : mCaptureDataClientVector[0]->isNeedSyncPcmStart();
    AL_UNLOCK(mClientLock);

    return retval;
}


void AudioALSACaptureDataProviderBase::signalPcmStart() { /* for client */
    AL_LOCK(mStartLock);

    if (mStart == true || mPcm == NULL || isNeedSyncPcmStart() == false) {
        AL_UNLOCK(mStartLock);
        return;
    }

    AL_SIGNAL(mStartLock);
    AL_UNLOCK(mStartLock);
}

int AudioALSACaptureDataProviderBase::pcmRead(struct pcm *mpcm, void *data, unsigned int count) {
    return audio_pcm_read_wrapper_fp(mpcm, data, count);
}

void AudioALSACaptureDataProviderBase::waitPcmStart() { /* for read thread */
    int wait_result = 0;

    AL_LOCK(mStartLock);

    mReadThreadReady = true;

    if (mStart == true || mPcm == NULL) {
        AL_UNLOCK(mStartLock);
        return;
    }

    if (isNeedSyncPcmStart() == true) {
        wait_result = AL_WAIT_MS(mStartLock, 100);
        if (wait_result != 0) {
            ALOGW("%s() wait fail", __FUNCTION__);
        }
    }

    ALOGD("pcm_start");
    pcm_start(mPcm);
    mStart = true;
    AL_UNLOCK(mStartLock);
}


bool AudioALSACaptureDataProviderBase::HasLowLatencyCapture(void) {
    bool bRet = false;
    IAudioALSACaptureDataClient *pCaptureDataClient = NULL;

    AL_LOCK(mClientLock);
    for (size_t i = 0; i < mCaptureDataClientVector.size(); i++) {
        pCaptureDataClient = mCaptureDataClientVector[i];
        if (pCaptureDataClient->IsLowLatencyCapture()) {
            bRet = true;
            break;
        }
    }
    AL_UNLOCK(mClientLock);

    ALOGV("%s(), bRet=%d", __FUNCTION__, bRet);
    return bRet;
}

void AudioALSACaptureDataProviderBase::setThreadPriority(void) {
#ifdef UPLINK_LOW_LATENCY
    if (HasLowLatencyCapture()) {
        audio_sched_setschedule(0, SCHED_RR, sched_get_priority_min(SCHED_RR));
    } else
#endif
    {
        audio_sched_setschedule(0, SCHED_NORMAL, sched_get_priority_max(SCHED_NORMAL));
    }
}

void AudioALSACaptureDataProviderBase::OpenPCMDump(const char *class_name) {
    ALOGV("%s(), mCaptureDataProviderType=%d", __FUNCTION__, mCaptureDataProviderType);
    char mDumpFileName[128];

    snprintf(mDumpFileName, sizeof(mDumpFileName) - 1, "%s%d.%s.pcm", streamin, mDumpFileNum, class_name);

    mPCMDumpFile = NULL;
    mPCMDumpFile = AudioOpendumpPCMFile(mDumpFileName, streamin_propty);


    char mDumpFileName4ch[128];
    if (mConfig.channels == 4) {
        snprintf(mDumpFileName4ch, sizeof(mDumpFileName4ch) - 1, "%s%d.%s_4ch.pcm", streamin, mDumpFileNum, class_name);
        mPCMDumpFile4ch = AudioOpendumpPCMFile(mDumpFileName4ch, streamin_propty);
        if (mPCMDumpFile4ch != NULL) {
            ALOGD("%s mDumpFileName4ch = %s", __FUNCTION__, mDumpFileName4ch);
        }
    }

    if (mPCMDumpFile != NULL) {
        ALOGD("%s DumpFileName = %s", __FUNCTION__, mDumpFileName);

        mDumpFileNum++;
        mDumpFileNum %= MAX_DUMP_NUM;
    }
}

void AudioALSACaptureDataProviderBase::ClosePCMDump() {
    if (mPCMDumpFile) {
        AudioCloseDumpPCMFile(mPCMDumpFile);
        ALOGD("%s(), mCaptureDataProviderType=%d", __FUNCTION__, mCaptureDataProviderType);
        mPCMDumpFile = NULL;
    }

    if (mPCMDumpFile4ch) {
        AudioCloseDumpPCMFile(mPCMDumpFile4ch);
        mPCMDumpFile4ch = NULL;
    }
}

void  AudioALSACaptureDataProviderBase::WritePcmDumpData(void) {
    if (mPCMDumpFile) {
        //ALOGD("%s()", __FUNCTION__);
        AudioDumpPCMData((void *)mPcmReadBuf.pBufBase, mPcmReadBuf.bufLen - 1, mPCMDumpFile);
    }
}

//echoref+++
void AudioALSACaptureDataProviderBase::provideEchoRefCaptureDataToAllClients(const uint32_t open_index) {
    ALOGV("+%s()", __FUNCTION__);

    if (open_index != mOpenIndex) {
        ALOGD("%s(), open_index(%d) != mOpenIndex(%d), return", __FUNCTION__, open_index, mOpenIndex);
        return;
    }

    IAudioALSACaptureDataClient *pCaptureDataClient = NULL;

    WritePcmDumpData();

    AL_LOCK(mClientLock);
    for (size_t i = 0; i < mCaptureDataClientVector.size(); i++) {
        pCaptureDataClient = mCaptureDataClientVector[i];
        pCaptureDataClient->copyEchoRefCaptureDataToClient(mPcmReadBuf);
    }
    AL_UNLOCK(mClientLock);


    ALOGV("-%s()", __FUNCTION__);
}
//echoref---


status_t AudioALSACaptureDataProviderBase::GetCaptureTimeStamp(time_info_struct_t *Time_Info, unsigned int read_size) {
    status_t retval = NO_ERROR;

    ALOGV("%s()", __FUNCTION__);
    ASSERT(mPcm != NULL);

    long ret_ns;
    size_t avail;
    Time_Info->timestamp_get.tv_sec  = 0;
    Time_Info->timestamp_get.tv_nsec = 0;
    Time_Info->frameInfo_get = 0;
    Time_Info->buffer_per_time = 0;
    Time_Info->kernelbuffer_ns = 0;

    //ALOGD("%s(), Going to check pcm_get_htimestamp", __FUNCTION__);
    int ret = pcm_get_htimestamp(mPcm, &Time_Info->frameInfo_get, &Time_Info->timestamp_get);
    if (ret == 0) {
        Time_Info->buffer_per_time = pcm_bytes_to_frames(mPcm, read_size);
        Time_Info->kernelbuffer_ns = 1000000000 / mStreamAttributeSource.sample_rate * (Time_Info->buffer_per_time + Time_Info->frameInfo_get);
        Time_Info->total_frames_readed += Time_Info->buffer_per_time;
        ALOGV("%s(), pcm_get_htimestamp sec= %ld, nsec=%ld, frameInfo_get = %u, buffer_per_time=%u, ret_ns = %lu, read_size = %u\n",
              __FUNCTION__, Time_Info->timestamp_get.tv_sec, Time_Info->timestamp_get.tv_nsec, Time_Info->frameInfo_get,
              Time_Info->buffer_per_time, Time_Info->kernelbuffer_ns, read_size);

        // Write time stamp to cache to avoid getCapturePosition performance issue
        AL_LOCK(mTimeStampLock);
        mCaptureFramesReaded = Time_Info->total_frames_readed;
        mCaptureTimeStamp = Time_Info->timestamp_get;
        AL_UNLOCK(mTimeStampLock);
#if 0
        if ((TimeStamp->tv_nsec - ret_ns) >= 0) {
            TimeStamp->tv_nsec -= ret_ns;
        } else {
            TimeStamp->tv_sec -= 1;
            TimeStamp->tv_nsec = 1000000000 + TimeStamp->tv_nsec - ret_ns;
        }

        ALOGD("%s calculate pcm_get_htimestamp sec= %ld, nsec=%ld, avail = %d, ret_ns = %ld\n",
              __FUNCTION__, TimeStamp->tv_sec, TimeStamp->tv_nsec, avail, ret_ns);
#endif
    } else {
        ALOGE("%s pcm_get_htimestamp fail, ret: %d, pcm_get_error: %s, time: %lld.%.9ld, frameInfo_get = %u",
              __FUNCTION__, ret, pcm_get_error(mPcm),
              (long long)Time_Info->timestamp_get.tv_sec,
              Time_Info->timestamp_get.tv_nsec,
              Time_Info->frameInfo_get);
        retval = UNKNOWN_ERROR;
    }
    return retval;
}


void AudioALSACaptureDataProviderBase::configStreamAttribute(const stream_attribute_t *attribute) {
    AL_LOCK(mEnableLock);

    ALOGD("%s(), audio_mode: %d => %d, input_device: 0x%x => 0x%x, flag: 0x%x => 0x%x, input_source: %d->%d",
          __FUNCTION__,
          mStreamAttributeSource.audio_mode, attribute->audio_mode,
          mStreamAttributeSource.input_device, attribute->input_device,
          mStreamAttributeSource.mAudioInputFlags, attribute->mAudioInputFlags,
          mStreamAttributeSource.input_source, attribute->input_source);

    if (mEnable == false) {
        mStreamAttributeSource.audio_mode = attribute->audio_mode;
        mStreamAttributeSource.input_device = attribute->input_device;
        mStreamAttributeSource.mAudioInputFlags = attribute->mAudioInputFlags;
        mStreamAttributeSource.input_source = attribute->input_source;
    } else {
        ALOGW("%s(), already enabled!! bypass config", __FUNCTION__);
    }

    AL_UNLOCK(mEnableLock);
}


int AudioALSACaptureDataProviderBase::getCapturePosition(int64_t *frames, int64_t *time) {
    AL_LOCK(mTimeStampLock);
    *frames = mCaptureFramesReaded;
    *time = mCaptureTimeStamp.tv_sec * 1000000000LL + mCaptureTimeStamp.tv_nsec;
    ALOGV("%s(), return frames = %" PRIu64 ", time = %" PRIu64 "", __FUNCTION__, *frames, *time);
    AL_UNLOCK(mTimeStampLock);

    return 0;
}

status_t AudioALSACaptureDataProviderBase::getPcmStatus() {
    ALOGD("%s()", __FUNCTION__);
    return mPcmStatus;
}

} // end of namespace android

