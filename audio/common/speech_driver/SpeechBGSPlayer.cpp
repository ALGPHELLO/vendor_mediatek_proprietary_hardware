#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "BGSPlayBuffer"
#include "SpeechBGSPlayer.h"
#include <sys/time.h>
#include <utils/threads.h>
#include <audio_utils/primitives.h>
#include "SpeechDriverInterface.h"

#ifndef bgs_msleep
#define bgs_msleep(ms) usleep((ms)*1000)
#endif
#define BGS_RETRY_TIMES 5
//Maximum Latency between two modem data request: 200ms
//AP sould fill data to buffer in 60ms while receiving request

namespace android {
/*==============================================================================
 *                     Property keys
 *============================================================================*/
const char PROPERTY_KEY_BGS_DUMP_ON[PROPERTY_KEY_MAX] = "persist.af.bgs_dump_on";
const char PROPERTY_KEY_BGS_BLISRC_DUMP_ON[PROPERTY_KEY_MAX] = "persist.af.bgs_blisrc_dump_on";

/*==============================================================================
 *                     Constant
 *============================================================================*/
#define BGS_CHANNEL_NUM         (1)
static const char kFileNameBGS[] = "/sdcard/mtklog/audio_dump/BGS";
static const char kFileNameBGSBlisrc[] = "/sdcard/mtklog/audio_dump/BGS_before_Blisrc";
static const uint32_t kMaxSizeOfFileName = 128;
static const uint32_t kSizeOfPrefixFileNameBGS = sizeof(kFileNameBGS) - 1;
static const uint32_t kSizeOfPrefixFileNameBGSBlisrc = sizeof(kFileNameBGSBlisrc) - 1;

#ifdef BGS_USE_SINE_WAVE
static const uint16_t table_1k_tone_16000_hz[] = {
    0x0000, 0x30FC, 0x5A82, 0x7641,
    0x7FFF, 0x7641, 0x5A82, 0x30FB,
    0x0001, 0xCF05, 0xA57E, 0x89C0,
    0x8001, 0x89BF, 0xA57E, 0xCF05
};
static const uint32_t kSizeSinewaveTable = sizeof(table_1k_tone_16000_hz);
#endif

/*==============================================================================
 *                     Implementation
 *============================================================================*/
BGSPlayBuffer::BGSPlayBuffer() :
    mExitRequest(false) {

    mFormat = AUDIO_FORMAT_DEFAULT;
    mRingBuf.pBufBase = NULL;
    mRingBuf.bufLen   = 0;
    mRingBuf.pRead = NULL;
    mRingBuf.pWrite   = NULL;
    mRingBuf.pBufEnd = NULL;
    mBliSrc = NULL;
    mIsBGSBlisrcDumpEnable = false;
    mBliOutputLinearBuffer = NULL;
    pDumpFile = NULL;
}

bool BGSPlayBuffer::IsBGSBlisrcDumpEnable() {
    // BGS Dump before Blisrc system property
    char property_value[PROPERTY_VALUE_MAX];
    property_get(PROPERTY_KEY_BGS_BLISRC_DUMP_ON, property_value, "0"); //"0": default off
    if (property_value[0] == '1') {
        return true;
    } else {
        return false;
    }
}

status_t BGSPlayBuffer::InitBGSPlayBuffer(BGSPlayer *playPointer,
                                          uint32_t sampleRate,
                                          uint32_t chNum,
                                          int32_t format) {
    ALOGV("InitBGSPlayBuffer sampleRate=%d ,ch=%d, format=%d", sampleRate, chNum, format);
    (void)playPointer;
    // keep the format
    ASSERT(format == AUDIO_FORMAT_PCM_16_BIT);
    mFormat = format;

    // set internal ring buffer
    mRingBuf.pBufBase = new char[BGS_PLAY_BUFFER_LEN];
    mRingBuf.bufLen   = BGS_PLAY_BUFFER_LEN;
    mRingBuf.pRead    = mRingBuf.pBufBase;
    mRingBuf.pWrite   = mRingBuf.pBufBase;

    ALOGV("%s(), pBufBase: %p, pRead: 0x%x, pWrite: 0x%x, bufLen:%u", __FUNCTION__,
          mRingBuf.pBufBase, (int)(mRingBuf.pRead - mRingBuf.pBufBase), (int)(mRingBuf.pWrite - mRingBuf.pBufBase), mRingBuf.bufLen);

    mIsBGSBlisrcDumpEnable = IsBGSBlisrcDumpEnable();
    if (mIsBGSBlisrcDumpEnable) {
        char fileNameBGSBlisrc[kMaxSizeOfFileName];
        memset((void *)fileNameBGSBlisrc, 0, kMaxSizeOfFileName);

        time_t rawtime;
        time(&rawtime);
        struct tm *timeinfo = localtime(&rawtime);
        audio_strncpy(fileNameBGSBlisrc, kFileNameBGSBlisrc, kMaxSizeOfFileName);
        strftime(fileNameBGSBlisrc + kSizeOfPrefixFileNameBGSBlisrc, kMaxSizeOfFileName - kSizeOfPrefixFileNameBGSBlisrc - 1, "_%Y_%m_%d_%H%M%S.pcm", timeinfo);
        if (pDumpFile == NULL) {
            AudiocheckAndCreateDirectory(fileNameBGSBlisrc);
            pDumpFile = fopen(fileNameBGSBlisrc, "wb");
        }
        if (pDumpFile == NULL) {
            ALOGW("%s(), Fail to open %s", __FUNCTION__, fileNameBGSBlisrc);
        } else {
            ALOGD("%s(), open %s", __FUNCTION__, fileNameBGSBlisrc);
        }
    }
    // set blisrc
    mBliSrc = newMtkAudioSrc(sampleRate, chNum, BGS_TARGET_SAMPLE_RATE, BGS_CHANNEL_NUM, SRC_IN_Q1P15_OUT_Q1P15);
    mBliSrc->open();

    ASSERT(mBliSrc != NULL);

    // set blisrc converted buffer
    mBliOutputLinearBuffer = new char[BGS_PLAY_BUFFER_LEN];
    ALOGV("%s(), mBliOutputLinearBuffer = %p, size = %u", __FUNCTION__, mBliOutputLinearBuffer, BGS_PLAY_BUFFER_LEN);

    return NO_ERROR;
}

BGSPlayBuffer::~BGSPlayBuffer() {
    mExitRequest = true;

    AL_LOCK(mBGSPlayBufferRuningMutex);
    AL_LOCK(mBGSPlayBufferMutex);

    // delete blisrc handler buffer
    if (mBliSrc) {
        mBliSrc->close();
        deleteMtkAudioSrc(mBliSrc);
        mBliSrc = NULL;
    }

    // delete blisrc converted buffer
    delete[] mBliOutputLinearBuffer;

    // delete internal ring buffer
    delete[] mRingBuf.pBufBase;

    if (pDumpFile != NULL) {
        fclose(pDumpFile);
        pDumpFile = NULL;
    }

    AL_SIGNAL(mBGSPlayBufferMutex);
    AL_UNLOCK(mBGSPlayBufferMutex);
    AL_UNLOCK(mBGSPlayBufferRuningMutex);
}

uint32_t BGSPlayBuffer::Write(char *buf, uint32_t num) {
    // lock
    AL_LOCK(mBGSPlayBufferRuningMutex);
    AL_LOCK(mBGSPlayBufferMutex);

    ALOGV("%s(), num = %u", __FUNCTION__, num);

    if (mIsBGSBlisrcDumpEnable) {
        if (pDumpFile != NULL) {
            fwrite(buf, sizeof(char), num, pDumpFile);
        }
    }
    uint32_t leftCount = num;
    uint16_t dataCountInBuf = 0;
    uint32_t tryCount = 0;
    while (tryCount < BGS_RETRY_TIMES && !mExitRequest) { // max mLatency = 200, max sleep (20 * 10) ms here
        // BLISRC: output buffer: buf => local buffer: mRingBuf
        if (leftCount > 0) {
            // get free space in ring buffer
            uint32_t outCount = RingBuf_getFreeSpace(&mRingBuf);

            // do conversion
            ASSERT(mBliSrc != NULL);
            uint32_t consumed = leftCount;
            mBliSrc->process((int16_t *)buf, &leftCount, (int16_t *)mBliOutputLinearBuffer, &outCount);
            consumed -= leftCount;

            buf += consumed;
            ALOGV("%s(), buf consumed = %u, leftCount = %u, outCount = %u",
                  __FUNCTION__, consumed, leftCount, outCount);

            // copy converted data to ring buffer //TODO(Harvey): try to reduce additional one memcpy here
            RingBuf_copyFromLinear(&mRingBuf, mBliOutputLinearBuffer, outCount);
            ALOGV("%s(), pRead: 0x%x, pWrite: 0x%x, dataCount: %u", __FUNCTION__,
                  (int)(mRingBuf.pRead - mRingBuf.pBufBase), (int)(mRingBuf.pWrite - mRingBuf.pBufBase), RingBuf_getDataCount(&mRingBuf));
        }

        // leave while loop
        if (leftCount <= 0) {
            break;
        }

        // wait modem side to retrieve data
        int retval = AL_WAIT_MS(mBGSPlayBufferMutex, 40);
        if (!mExitRequest) {
            dataCountInBuf = RingBuf_getDataCount(&mRingBuf);
        }
        if (retval != 0) {
            ALOGV("%s(), tryCount = %u, leftCount = %u, dataCountInBuf = %u",
                  __FUNCTION__, tryCount, leftCount, dataCountInBuf);
            tryCount++;
        }

    }

    // leave warning message if need
    if (leftCount != 0) {
        dataCountInBuf = RingBuf_getDataCount(&mRingBuf);
        ALOGW("%s(), still leftCount = %u, dataCountInBuf = %u", __FUNCTION__, leftCount, dataCountInBuf);
    }

    // unlock
    AL_UNLOCK(mBGSPlayBufferMutex);
    AL_UNLOCK(mBGSPlayBufferRuningMutex);

    return num - leftCount;
}

//*****************************************************************************************
//--------------------------for LAD Player------------------------------------------
//*****************************************************************************************
#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "BGSPlayer"

BGSPlayer *BGSPlayer::mBGSPlayer = NULL;
BGSPlayer *BGSPlayer::GetInstance() {
    static Mutex mGetInstanceLock;
    Mutex::Autolock _l(mGetInstanceLock);

    if (mBGSPlayer == NULL) {
        mBGSPlayer = new BGSPlayer();
    }
    ASSERT(mBGSPlayer != NULL);
    return mBGSPlayer;
}

BGSPlayer::BGSPlayer() {
    // initial all table entry to zero, means non of them are occupied
    mCount = 0;
    mBufBaseTemp = new char[BGS_PLAY_BUFFER_LEN];
    mSpeechDriver = NULL;
    mIsBGSDumpEnable = false;
    pDumpFile = NULL;
}

BGSPlayer::~BGSPlayer() {
    AL_AUTOLOCK(mBGSPlayBufferVectorLock);

    size_t count = mBGSPlayBufferVector.size();
    for (size_t i = 0 ; i < count ; i++) {
        BGSPlayBuffer *pBGSPlayBuffer = mBGSPlayBufferVector.itemAt(i);
        delete pBGSPlayBuffer;
    }
    mBGSPlayBufferVector.clear();

    delete[] mBufBaseTemp;

    if (pDumpFile != NULL) {
        fclose(pDumpFile);
        pDumpFile = NULL;
    }
}

BGSPlayBuffer *BGSPlayer::CreateBGSPlayBuffer(uint32_t sampleRate, uint32_t chNum, int32_t format) {
    ALOGV("CreateBGSPlayBuffer sampleRate=%u ,ch=%u, format=%d", sampleRate, chNum, format);

    // protection
    ASSERT(format == AUDIO_FORMAT_PCM_16_BIT);

    AL_AUTOLOCK(mBGSPlayBufferVectorLock);

    // create BGSPlayBuffer
    BGSPlayBuffer *pBGSPlayBuffer = new BGSPlayBuffer();
    pBGSPlayBuffer->InitBGSPlayBuffer(this, sampleRate, chNum, format);

    mBGSPlayBufferVector.add(pBGSPlayBuffer);
    return pBGSPlayBuffer;
}

bool BGSPlayer::IsBGSDumpEnable() {
    // BGS Dump system property
    char property_value[PROPERTY_VALUE_MAX];
    property_get(PROPERTY_KEY_BGS_DUMP_ON, property_value, "0"); //"0": default off
    if (property_value[0] == '1') {
        return true;
    } else {
        return false;
    }
}

uint32_t BGSPlayer::Write(BGSPlayBuffer *pBGSPlayBuffer, void *buf, uint32_t num) {
    ASSERT(pBGSPlayBuffer != NULL);
    return pBGSPlayBuffer->Write((char *)buf, num);
}

void BGSPlayer::DestroyBGSPlayBuffer(BGSPlayBuffer *pBGSPlayBuffer) {
    ASSERT(pBGSPlayBuffer != NULL);

    AL_AUTOLOCK(mBGSPlayBufferVectorLock);

    mBGSPlayBufferVector.remove(pBGSPlayBuffer);
    delete pBGSPlayBuffer;
}

bool BGSPlayer::Open(SpeechDriverInterface *pSpeechDriver, uint8_t uplink_gain, uint8_t downlink_gain) {
    if (NULL != mSpeechDriver && mSpeechDriver != pSpeechDriver) {
        ALOGE("BGSPlayer can't support differ SpeechDriver");
        return false;
    }

    mCount++;
    if (1 != mCount) {
        ALOGD("%s(), already open. mCount %d", __FUNCTION__, mCount);
        return true;
    }

    ALOGD("%s(), first open, mCount %d", __FUNCTION__, mCount);

    AL_AUTOLOCK(mBGSPlayBufferVectorLock);

    // get Speech Driver (to open/close BGS)
    mSpeechDriver = pSpeechDriver;
    mIsBGSDumpEnable = IsBGSDumpEnable();

    if (mIsBGSDumpEnable) {
        char fileNameBGS[kMaxSizeOfFileName];
        memset((void *)fileNameBGS, 0, kMaxSizeOfFileName);

        time_t rawtime;
        time(&rawtime);
        struct tm *timeinfo = localtime(&rawtime);
        audio_strncpy(fileNameBGS, kFileNameBGS, kMaxSizeOfFileName);
        strftime(fileNameBGS + kSizeOfPrefixFileNameBGS, kMaxSizeOfFileName - kSizeOfPrefixFileNameBGS - 1, "_%Y_%m_%d_%H%M%S.pcm", timeinfo);
        if (pDumpFile == NULL) {
            AudiocheckAndCreateDirectory(fileNameBGS);
            pDumpFile = fopen(fileNameBGS, "wb");
        }
        if (pDumpFile == NULL) {
            ALOGW("%s(), Fail to open %s", __FUNCTION__, fileNameBGS);
        } else {
            ALOGD("%s(), open %s", __FUNCTION__, fileNameBGS);
        }
    }
    //no stream is inside player, should return false
    ASSERT(mBGSPlayBufferVector.size() != 0);

    //turn on background sound
    mSpeechDriver->BGSoundOn();

    //recover the UL gain
    //backup Background Sound UL and DL gain
    //bcs we set them to zero when normal recording
    //we need to set it back when phone call recording
    mSpeechDriver->BGSoundConfig(uplink_gain, downlink_gain);

    return true;
}

uint32_t BGSPlayer::PutData(BGSPlayBuffer *pBGSPlayBuffer, char *target_ptr, uint16_t num_data_request) {
    uint16_t write_count = 0;

    if (pBGSPlayBuffer == NULL) {
        ALOGW("%s(), pBGSPlayBuffer == NULL, return 0.", __FUNCTION__);
        return 0;
    }

    AL_LOCK(pBGSPlayBuffer->mBGSPlayBufferMutex);

    // check data count in pBGSPlayBuffer
    uint16_t dataCountInBuf = RingBuf_getDataCount(&pBGSPlayBuffer->mRingBuf);
    if (dataCountInBuf == 0) { // no data in buffer, just return 0
        ALOGV("%s(), dataCountInBuf == 0, return 0.", __FUNCTION__);
        AL_UNLOCK(pBGSPlayBuffer->mBGSPlayBufferMutex);
        return 0;
    }

    write_count = (dataCountInBuf >= num_data_request) ? num_data_request : dataCountInBuf;

    // copy to share buffer
    RingBuf_copyToLinear(target_ptr, &pBGSPlayBuffer->mRingBuf, write_count);
    ALOGV("%s(), pRead: 0x%x, pWrite: 0x%x, dataCount:%u", __FUNCTION__,
          (int)(pBGSPlayBuffer->mRingBuf.pRead - pBGSPlayBuffer->mRingBuf.pBufBase),
          (int)(pBGSPlayBuffer->mRingBuf.pWrite - pBGSPlayBuffer->mRingBuf.pBufBase),
          RingBuf_getDataCount(&pBGSPlayBuffer->mRingBuf));

    AL_SIGNAL(pBGSPlayBuffer->mBGSPlayBufferMutex);
    AL_UNLOCK(pBGSPlayBuffer->mBGSPlayBufferMutex);

    return write_count;
}

uint16_t BGSPlayer::PutDataToSpeaker(char *target_ptr, uint16_t num_data_request) {
    uint16_t write_count = 0;

#ifndef BGS_USE_SINE_WAVE
    AL_AUTOLOCK(mBGSPlayBufferVectorLock);

    size_t count = mBGSPlayBufferVector.size();

    if (count == 0) {
        ALOGW("%s(), mBGSPlayBufferVector == NULL, return 0.", __FUNCTION__);
        return 0;
    }

    uint16_t dataCountInBuf, dataCountInBufMin = 65535;
    for (size_t i = 0 ; i < count ; i++) {
        BGSPlayBuffer *pBGSPlayBuffer = mBGSPlayBufferVector.itemAt(i);

        AL_LOCK(pBGSPlayBuffer->mBGSPlayBufferMutex);
        dataCountInBuf = RingBuf_getDataCount(&pBGSPlayBuffer->mRingBuf);
        AL_UNLOCK(pBGSPlayBuffer->mBGSPlayBufferMutex);

        if (dataCountInBuf < dataCountInBufMin) {
            dataCountInBufMin = dataCountInBuf;
        }
    }

    // check data count in pBGSPlayBuffer
    if (dataCountInBufMin == 0) { // no data in buffer, just return 0
        ALOGW("%s(), dataCountInBuf == 0!! num_data_request %d",
              __FUNCTION__, num_data_request);
        return 0;
    }

    write_count = (dataCountInBufMin >= num_data_request) ? num_data_request : dataCountInBufMin;
    //ALOGD("write_count %d %d %d", write_count, dataCountInBufMin, num_data_request);

    memset(target_ptr, 0, num_data_request);
    for (size_t i = 0 ; i < count ; i++) {
        BGSPlayBuffer *pBGSPlayBuffer = mBGSPlayBufferVector.itemAt(i);
        if (count == 1) {
            PutData(pBGSPlayBuffer, target_ptr, write_count);
            continue;
        }
        PutData(pBGSPlayBuffer, mBufBaseTemp, write_count);

        // mixer
        int16_t *in = (int16_t *)mBufBaseTemp;
        int16_t *out = (int16_t *)target_ptr;
        int16_t frameCnt = write_count / BGS_CHANNEL_NUM / audio_bytes_per_sample(AUDIO_FORMAT_PCM_16_BIT);
        for (int16_t j = 0; j < frameCnt; j++) {
            out[j] = clamp16((int32_t)out[j] + (int32_t)in[j]);
        }
    }
#else
    static uint32_t i4Count = 0;
    uint32_t current_count = 0, remain_count = 0;
    char *tmp_ptr = NULL;

    remain_count = write_count = num_data_request;
    tmp_ptr = target_ptr;

    if (remain_count > (kSizeSinewaveTable - i4Count)) {
        memcpy(tmp_ptr, table_1k_tone_16000_hz + (i4Count >> 1), kSizeSinewaveTable - i4Count);
        tmp_ptr += (kSizeSinewaveTable - i4Count);
        remain_count -= (kSizeSinewaveTable - i4Count);
        i4Count = 0;
    }
    while (remain_count > kSizeSinewaveTable) {
        memcpy(tmp_ptr, table_1k_tone_16000_hz, kSizeSinewaveTable);
        tmp_ptr += kSizeSinewaveTable;
        remain_count -= kSizeSinewaveTable;
    }
    if (remain_count > 0) {
        memcpy(tmp_ptr, table_1k_tone_16000_hz, remain_count);
        i4Count = remain_count;
    }
#endif

    if (mIsBGSDumpEnable) {
        if (pDumpFile != NULL) {
            fwrite(target_ptr, sizeof(char), write_count, pDumpFile);
        }
    }
    return write_count;
}

bool BGSPlayer::Close() {

    mCount--;
    if (0 != mCount) {
        ALOGD("%s, has other user, return. mCount %d", __FUNCTION__, mCount);
        return true;
    }

    ALOGD("%s(), mCount %d, stop", __FUNCTION__, mCount);

    // tell modem side to close BGS
    mSpeechDriver->BGSoundOff();
    mSpeechDriver = NULL;

    if (pDumpFile != NULL) {
        fclose(pDumpFile);
        pDumpFile = NULL;
    }
    return true;
}

}; // namespace android


