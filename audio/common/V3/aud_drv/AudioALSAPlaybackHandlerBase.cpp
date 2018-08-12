#include "AudioALSAPlaybackHandlerBase.h"

#include "AudioALSADataProcessor.h"

#include "AudioALSADriverUtility.h"
#include "AudioALSAHardwareResourceManager.h"
#include "AudioUtility.h"

#include "AudioMTKFilter.h"

#if (defined(MTK_AUDIO_HIERARCHICAL_PARAM_SUPPORT) && (MTK_AUDIO_TUNING_TOOL_V2_PHASE >= 2))
#include "AudioParamParser.h"
#endif

#include <SpeechEnhancementController.h>

#ifdef MTK_AURISYS_FRAMEWORK_SUPPORT
#include <utils/String8.h>

#include <audio_memory_control.h>
#include <audio_lock.h>
#include <audio_ringbuf.h>


#include <audio_task.h>
#include <aurisys_scenario.h>

#include <arsi_type.h>

#include <audio_pool_buf_handler.h>

#include <aurisys_utility.h>
#include <aurisys_controller.h>
#include <aurisys_lib_manager.h>
#endif
#include "AudioSmartPaController.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "AudioALSAPlaybackHandlerBase"

namespace android {

static const uint32_t kMaxPcmDriverBufferSize = 0x20000;  // 128k
static const uint32_t kBliSrcOutputBufferSize = 0x10000;  // 64k
static const uint32_t kPcmDriverBufferSize    = 0x20000;  // 128k

static const uint32_t kAurisysBufSizeDlIn     = 0x10000;  // 64k
static const uint32_t kAurisysBufSizeDlOut    = 0x40000;  // 256k

static const uint32_t kAurisysBufSizeUlIn     = 0x10000;  // 64k
static const uint32_t kAurisysBufSizeUlOut    = 0x10000;  // 64k


uint32_t AudioALSAPlaybackHandlerBase::mDumpFileNum = 0;

#ifdef MTK_AURISYS_FRAMEWORK_SUPPORT
static AudioLock mAurisysLibManagerLock;
#endif


AudioALSAPlaybackHandlerBase::AudioALSAPlaybackHandlerBase(const stream_attribute_t *stream_attribute_source) :
    mPlaybackHandlerType(PLAYBACK_HANDLER_BASE),
    mHardwareResourceManager(AudioALSAHardwareResourceManager::getInstance()),
    mStreamAttributeSource(stream_attribute_source),
    mPcm(NULL),
    mComprStream(NULL),
    mStreamCbk(NULL),
    mCbkCookie(NULL),
    mAudioFilterManagerHandler(NULL),
    mPostProcessingOutputBuffer(NULL),
    mPostProcessingOutputBufferSize(0),
    mFirstDataWrite(true),
    mDcRemove(NULL),
    mDcRemoveWorkBuffer(NULL),
    mDcRemoveBufferSize(0),
    mBliSrc(NULL),
    mBliSrcOutputBuffer(NULL),
    mBitConverter(NULL),
    mBitConverterOutputBuffer(NULL),
    mdataPendingOutputBuffer(NULL),
    mdataPendingTempBuffer(NULL),
    mdataPendingOutputBufferSize(0),
    mdataPendingRemindBufferSize(0),
    mDataAlignedSize(64),
    mDataPendingForceUse(false),
    mNLEMnger(NULL),
    mPCMDumpFile(NULL),
    mMixer(AudioALSADriverUtility::getInstance()->getMixer()),
    mBytesWriteKernel(0),
    mTimeStampValid(true),
    mHalQueuedFrame(0),
#ifdef MTK_AURISYS_FRAMEWORK_SUPPORT
    mAurisysLibManager(NULL),
    mAurisysScenario(AURISYS_SCENARIO_INVALID),
    mAudioPoolBufUlIn(NULL),
    mAudioPoolBufUlOut(NULL),
    mAudioPoolBufDlIn(NULL),
    mAudioPoolBufDlOut(NULL),
    mTransferredBufferSize(0xFFFFFFFF),
    mLinearOut(NULL),
#endif
    mIsNeedUpdateLib(false),
    mDataProcessor(AudioALSADataProcessor::getInstance()),
    mIdentity(0xFFFFFFFF),
    mPcmflag(0),
    audio_pcm_write_wrapper_fp(NULL) {
    ALOGV("%s()", __FUNCTION__);

    memset(&mConfig, 0, sizeof(mConfig));
    memset(&mStreamAttributeTarget, 0, sizeof(mStreamAttributeTarget));
    memset(&mComprConfig, 0, sizeof(mComprConfig));
}


AudioALSAPlaybackHandlerBase::~AudioALSAPlaybackHandlerBase() {
    ALOGV("%s()", __FUNCTION__);
}


status_t AudioALSAPlaybackHandlerBase::ListPcmDriver(const unsigned int card, const unsigned int device) {
    struct pcm_params *params;
    unsigned int min, max ;
    params = pcm_params_get(card, device, PCM_OUT);
    if (params == NULL) {
        ALOGD("Device does not exist.\n");
    }
    min = pcm_params_get_min(params, PCM_PARAM_RATE);
    max = pcm_params_get_max(params, PCM_PARAM_RATE);
    ALOGD("        Rate:\tmin=%uHz\tmax=%uHz\n", min, max);
    min = pcm_params_get_min(params, PCM_PARAM_CHANNELS);
    max = pcm_params_get_max(params, PCM_PARAM_CHANNELS);
    ALOGD("    Channels:\tmin=%u\t\tmax=%u\n", min, max);
    min = pcm_params_get_min(params, PCM_PARAM_SAMPLE_BITS);
    max = pcm_params_get_max(params, PCM_PARAM_SAMPLE_BITS);
    ALOGD(" Sample bits:\tmin=%u\t\tmax=%u\n", min, max);
    min = pcm_params_get_min(params, PCM_PARAM_PERIOD_SIZE);
    max = pcm_params_get_max(params, PCM_PARAM_PERIOD_SIZE);
    ALOGD(" Period size:\tmin=%u\t\tmax=%u\n", min, max);
    min = pcm_params_get_min(params, PCM_PARAM_PERIODS);
    max = pcm_params_get_max(params, PCM_PARAM_PERIODS);
    ALOGD("Period count:\tmin=%u\t\tmax=%u\n", min, max);
    max = pcm_params_get_max(params, PCM_PARAM_BUFFER_SIZE);
    ALOGD("PCM_PARAM_BUFFER_SIZE :\t max=%u\t\n", max);
    max = pcm_params_get_max(params, PCM_PARAM_BUFFER_BYTES);
    ALOGD("PCM_PARAM_BUFFER_BYTES :\t max=%u\t\n", max);
    pcm_params_free(params);

    return NO_ERROR;
}
status_t AudioALSAPlaybackHandlerBase::openPcmDriverWithFlag(const unsigned int device, unsigned int flag) {
    ALOGD("+%s(), pcm device = %d flag = 0x%x", __FUNCTION__, device, flag);

    ASSERT(mPcm == NULL);

    mPcmflag = flag;
    mPcm = pcm_open(AudioALSADeviceParser::getInstance()->GetCardIndex(),
                    device, flag, &mConfig);
    if (mPcm == NULL) {
        ALOGE("%s(), mPcm == NULL!!", __FUNCTION__);
    } else if (pcm_is_ready(mPcm) == false) {
        ALOGE("%s(), pcm_is_ready(%p) == false due to %s, close pcm.", __FUNCTION__, mPcm, pcm_get_error(mPcm));
        pcm_close(mPcm);
        mPcm = NULL;
    } else if (pcm_prepare(mPcm) != 0) {
        ALOGE("%s(), pcm_prepare(%p) == false due to %s, close pcm.", __FUNCTION__, mPcm, pcm_get_error(mPcm));
        pcm_close(mPcm);
        mPcm = NULL;
    }

    if (mPcmflag & PCM_MMAP) {
        audio_pcm_write_wrapper_fp = pcm_mmap_write;
    } else {
        audio_pcm_write_wrapper_fp = pcm_write;
    }

    ALOGD("-%s(), mPcm = %p", __FUNCTION__, mPcm);
    ASSERT(mPcm != NULL);
    return NO_ERROR;

}

status_t AudioALSAPlaybackHandlerBase::pcmWrite(struct pcm *pcm, const void *data, unsigned int count) {
    return audio_pcm_write_wrapper_fp(pcm, data, count);
}

status_t AudioALSAPlaybackHandlerBase::openPcmDriver(const unsigned int device) {
    return openPcmDriverWithFlag(device, PCM_OUT | PCM_MONOTONIC);
}

status_t AudioALSAPlaybackHandlerBase::closePcmDriver() {
    ALOGD("+%s(), mPcm = %p", __FUNCTION__, mPcm);

    if (mPcm != NULL) {
        pcm_stop(mPcm);
        pcm_close(mPcm);
        mPcm = NULL;
    }

    ALOGD("-%s(), mPcm = %p", __FUNCTION__, mPcm);
    return NO_ERROR;
}

status_t AudioALSAPlaybackHandlerBase::openComprDriver(const unsigned int device) {
    ALOGD("+%s(), compr device = %d", __FUNCTION__, device);
    ASSERT(mComprStream == NULL);
    mComprStream = compress_open(AudioALSADeviceParser::getInstance()->GetCardIndex(),
                                 device, COMPRESS_IN, &mComprConfig);
    if (mComprStream == NULL) {
        ALOGE("%s(), mComprStream == NULL!!", __FUNCTION__);
        return INVALID_OPERATION;
    } else if (is_compress_ready(mComprStream) == false) {
        ALOGE("%s(), compress device open fail:%s", __FUNCTION__, compress_get_error(mComprStream));
        compress_close(mComprStream);
        mComprStream = NULL;
        return INVALID_OPERATION;
    }
    ALOGD("-%s(), mComprStream = %p", __FUNCTION__, mComprStream);
    return NO_ERROR;
}

status_t AudioALSAPlaybackHandlerBase::closeComprDriver() {
    ALOGD("+%s(), mComprStream = %p", __FUNCTION__, mComprStream);

    if (mComprStream != NULL) {
        //close compress driver
        compress_stop(mComprStream);
        compress_close(mComprStream);
        mComprStream = NULL;
    }

    ALOGD("-%s(), mComprStream = %p", __FUNCTION__, mComprStream);
    return NO_ERROR;

}

status_t AudioALSAPlaybackHandlerBase::setComprCallback(stream_callback_t StreamCbk, void *CbkCookie) {
    mStreamCbk = StreamCbk;
    mCbkCookie = CbkCookie;
    return NO_ERROR;
}

int AudioALSAPlaybackHandlerBase::updateAudioMode(audio_mode_t mode) {
    ALOGV("%s(), mode %d", __FUNCTION__, mode);
#ifdef MTK_AURISYS_FRAMEWORK_SUPPORT
    if (mAurisysLibManager && get_aurisys_on()) {
        if (mAurisysScenario == AURISYS_SCENARIO_PLAYBACK_NORMAL &&
            IsVoIPEnable()) {
            // change from normal to voip need delay
            mIsNeedUpdateLib = true;
        } else if (mAurisysScenario != AURISYS_SCENARIO_PLAYBACK_LOW_LATENCY) {
            mIsNeedUpdateLib = false;
            DestroyAurisysLibManager();
            CreateAurisysLibManager();
        }
    }
#endif
    return 0;
}

int AudioALSAPlaybackHandlerBase::preWriteOperation(const void *buffer, size_t bytes) {
    ALOGV("%s(), buffer %p, bytes %zu, mIsNeedUpdateLib %d", __FUNCTION__, buffer, bytes, mIsNeedUpdateLib);
#ifdef MTK_AURISYS_FRAMEWORK_SUPPORT
    // reopen aurisys when one hal buffer is in low level, below -60dB
    if (!mIsNeedUpdateLib) {
        return 0;
    }

    bool canUpdateLib = true;

    size_t tmp_bytes = bytes;

    if (mStreamAttributeSource->audio_format == AUDIO_FORMAT_PCM_16_BIT) {
        int16_t *sample = (int16_t *)buffer;
        int16_t threshold = 32;  // below -60dB, ((2^16) >> 10) / 2 = 32

        while (tmp_bytes > 0) {
            if (((*sample) > threshold) || ((*sample) < (-1 * threshold))) {
                canUpdateLib = false;
                break;
            }
            tmp_bytes -= audio_bytes_per_sample(mStreamAttributeSource->audio_format);
            sample++;
        }
    } else {    // assume AUDIO_FORMAT_PCM_32_BIT
        int32_t *sample = (int32_t *)buffer;
        int32_t threshold = 2097152;   // below -60dB, ((2^32) >> 10) / 2 = 2097152

        while (tmp_bytes > 0) {
            if (((*sample) > threshold) || ((*sample) < (-1 * threshold))) {
                canUpdateLib = false;
                break;
            }
            tmp_bytes -= audio_bytes_per_sample(mStreamAttributeSource->audio_format);
            sample++;
        }
    }

    if (canUpdateLib) {
        mIsNeedUpdateLib = false;
        DestroyAurisysLibManager();
        CreateAurisysLibManager();
    }

#endif
    return 0;
}

status_t AudioALSAPlaybackHandlerBase::doStereoToMonoConversionIfNeed(void *buffer, size_t bytes) {
#ifndef ENABLE_STEREO_SPEAKER
    if (mStreamAttributeSource->output_devices & AUDIO_DEVICE_OUT_SPEAKER) {
        if (mStreamAttributeSource->audio_format == AUDIO_FORMAT_PCM_32_BIT) {
            int32_t *Sample = (int32_t *)buffer;
            while (bytes > 0) {
                int32_t averageValue = ((*Sample) >> 1) + ((*(Sample + 1)) >> 1);
                *Sample++ = averageValue;
                *Sample++ = averageValue;
                bytes -= 8;
            }
        } else if (mStreamAttributeSource->audio_format == AUDIO_FORMAT_PCM_16_BIT) {
            int16_t *Sample = (int16_t *)buffer;
            while (bytes > 0) {
                int16_t averageValue = ((*Sample) >> 1) + ((*(Sample + 1)) >> 1);
                *Sample++ = averageValue;
                *Sample++ = averageValue;
                bytes -= 4;
            }
        }
    }
#endif
    return NO_ERROR;
}

status_t AudioALSAPlaybackHandlerBase::setScreenState(bool mode __unused,
                                                      size_t buffer_size __unused,
                                                      size_t reduceInterruptSize __unused,
                                                      bool bforce __unused) {
    return NO_ERROR;
}

status_t AudioALSAPlaybackHandlerBase::getHardwareBufferInfo(time_info_struct_t *HWBuffer_Time_Info) {
#if defined(CONFIG_MT_ENG_BUILD)
    ALOGV("+%s()", __FUNCTION__);
#endif

    if (mComprStream == NULL) {
        ASSERT(mPcm != NULL);
    } else {
        ALOGD("%s(), no pcm handler, return directly", __FUNCTION__);
        return NO_ERROR;
    }

    if (mTimeStampValid == false) {
        ALOGV("getHardwareBufferInfo: It doesn't start to fetch data on PCM buffer");
        return UNKNOWN_ERROR;
    }

    ASSERT(mPcm != NULL);
    int ret = pcm_get_htimestamp(mPcm, &HWBuffer_Time_Info->frameInfo_get, &HWBuffer_Time_Info->timestamp_get);
    if (ret != 0) {
        ALOGE("-%s(), pcm_get_htimestamp fail, ret = %d, pcm_get_error = %s", __FUNCTION__, ret, pcm_get_error(mPcm));
        return UNKNOWN_ERROR;
    } else {
        // kernel total buffer size to frame
        HWBuffer_Time_Info->buffer_per_time = pcm_bytes_to_frames(mPcm, mStreamAttributeTarget.buffer_size);

        HWBuffer_Time_Info->halQueuedFrame = mHalQueuedFrame;
    }
    ALOGV("-%s, frameInfo_get = %u, mStreamAttributeTarget.buffer_size = %d, buffer_per_time = %u, halQueuedFrame = %d",
          __FUNCTION__, HWBuffer_Time_Info->frameInfo_get, mStreamAttributeTarget.buffer_size, HWBuffer_Time_Info->buffer_per_time, HWBuffer_Time_Info->halQueuedFrame);
    return NO_ERROR;
}
status_t AudioALSAPlaybackHandlerBase::get_timeStamp(unsigned long *frames, unsigned int *samplerate) {
    if (mComprStream == NULL) {
        ALOGE("%s(), mComprStream NULL", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    if (compress_get_tstamp(mComprStream, frames, samplerate) == 0) {
        ALOGV("%s(), frames:%lu, samplerate:%u", __FUNCTION__, *frames, *samplerate);
        return NO_ERROR;
    } else {
        ALOGE("%s get_tstamp fail %s\n", __FUNCTION__, compress_get_error(mComprStream));
        return UNKNOWN_ERROR;
    }
    return NO_ERROR;
}

status_t AudioALSAPlaybackHandlerBase::updateHardwareBufferInfo(size_t sourceWrittenBytes, uint32_t targetWrittenBytes) {
    if (mPcm == NULL) {
        ALOGE("%s(), mPcm == NULL, return", __FUNCTION__);
        return INVALID_OPERATION;
    }

    // calculated hal queued buffer
    size_t sourceSizePerFrame = getSizePerFrame(mStreamAttributeSource->audio_format,
                                                mStreamAttributeSource->num_channels);
    size_t targetSizePerFrame = getSizePerFrame(mStreamAttributeTarget.audio_format,
                                                mStreamAttributeTarget.num_channels);

    size_t inBytesInHandler = ((uint64_t)sourceWrittenBytes * mStreamAttributeTarget.sample_rate * targetSizePerFrame) /
                              (sourceSizePerFrame * mStreamAttributeSource->sample_rate);

    if (inBytesInHandler >= targetWrittenBytes) {
        mHalQueuedFrame += pcm_bytes_to_frames(mPcm, inBytesInHandler - targetWrittenBytes);
    } else {
        mHalQueuedFrame -= pcm_bytes_to_frames(mPcm, targetWrittenBytes - inBytesInHandler);
    }

    return NO_ERROR;
}

playback_handler_t AudioALSAPlaybackHandlerBase::getPlaybackHandlerType() {
    return mPlaybackHandlerType;
}


status_t AudioALSAPlaybackHandlerBase::setFilterMng(AudioMTKFilterManager *pFilterMng __unused) {
    ALOGW("%s(), do nothing", __FUNCTION__);
    return INVALID_OPERATION;
}


status_t AudioALSAPlaybackHandlerBase::initPostProcessing() {
    // init post processing
    mPostProcessingOutputBufferSize = kMaxPcmDriverBufferSize;
    mPostProcessingOutputBuffer = new char[mPostProcessingOutputBufferSize];
    ASSERT(mPostProcessingOutputBuffer != NULL);

    return NO_ERROR;
}


status_t AudioALSAPlaybackHandlerBase::deinitPostProcessing() {
    // deinit post processing
    if (mPostProcessingOutputBuffer) {
        delete []mPostProcessingOutputBuffer;
        mPostProcessingOutputBuffer = NULL;
        mPostProcessingOutputBufferSize = 0;
    }

    if (mAudioFilterManagerHandler) {
        mAudioFilterManagerHandler->stop();
        mAudioFilterManagerHandler = NULL;
    }

    return NO_ERROR;
}


status_t AudioALSAPlaybackHandlerBase::doPostProcessing(void *pInBuffer, uint32_t inBytes, void **ppOutBuffer, uint32_t *pOutBytes) {
    // bypass downlink filter while DMNR tuning // TO DO Verification, HoChi
    if (mAudioFilterManagerHandler && mStreamAttributeSource->BesRecord_Info.besrecord_dmnr_tuningEnable == false && mStreamAttributeSource->bBypassPostProcessDL == false)
    {
        if (inBytes > mPostProcessingOutputBufferSize) {
            ALOGW("%s(), inBytes %d > mPostProcessingOutputBufferSize %d", __FUNCTION__, inBytes, mPostProcessingOutputBufferSize);
            ASSERT(0);
            *ppOutBuffer = pInBuffer;
            *pOutBytes = inBytes;
        } else {
            mAudioFilterManagerHandler->start(mFirstDataWrite); // TODO(Harvey, Hochi), why start everytime in write() ??
            uint32_t outputSize = mAudioFilterManagerHandler->process(pInBuffer, inBytes, mPostProcessingOutputBuffer, mPostProcessingOutputBufferSize);
            if (outputSize == 0) {
                *ppOutBuffer = pInBuffer;
                *pOutBytes = inBytes;
            } else {
                *ppOutBuffer = mPostProcessingOutputBuffer;
                *pOutBytes = outputSize;
            }
        }
    } else { // bypass
        *ppOutBuffer = pInBuffer;
        *pOutBytes = inBytes;
    }

    ASSERT(*ppOutBuffer != NULL && *pOutBytes != 0);
    return NO_ERROR;
}

int32 AudioALSAPlaybackHandlerBase::initDcRemoval() {
    DCR_BITDEPTH depth = DCREMOVE_BIT24;
    mDcRemove = newMtkDcRemove();
    ASSERT(mDcRemove != NULL);
    if (mStreamAttributeSource->audio_format == AUDIO_FORMAT_PCM_16_BIT) {
        depth = DCREMOVE_BIT16;
    }
    mDcRemove->init(mStreamAttributeSource->num_channels, mStreamAttributeSource->sample_rate,
                    DCR_MODE_3, depth);
    mDcRemoveBufferSize = kMaxPcmDriverBufferSize;
    mDcRemoveWorkBuffer = new char[mDcRemoveBufferSize];
    memset(mDcRemoveWorkBuffer, 0, mDcRemoveBufferSize);
    ASSERT(mDcRemoveWorkBuffer != NULL);
    return NO_ERROR;

}

int32 AudioALSAPlaybackHandlerBase::deinitDcRemoval() {
    if (mDcRemove) {
        mDcRemove->close();
        deleteMtkDcRemove(mDcRemove);
        mDcRemove = NULL;
    }
    if (mDcRemoveWorkBuffer) {
        delete [] mDcRemoveWorkBuffer;
        mDcRemoveWorkBuffer = NULL;
    }
    return NO_ERROR;
}

int32 AudioALSAPlaybackHandlerBase::doDcRemoval(void *pInBuffer, uint32_t inBytes, void **ppOutBuffer, uint32_t *pOutBytes) {
    uint32_t num_process_data = mDcRemoveBufferSize;

    if (mDcRemove == NULL) {
        *ppOutBuffer = pInBuffer;
        *pOutBytes = inBytes;
    } else if (inBytes > mDcRemoveBufferSize) {
        ALOGW("%s(), inBytes %d > mDcRemoveBufferSize %d", __FUNCTION__, inBytes, mDcRemoveBufferSize);
        ASSERT(0);
        *ppOutBuffer = pInBuffer;
        *pOutBytes = inBytes;
    } else {
        mDcRemove->process(pInBuffer, &inBytes, mDcRemoveWorkBuffer, &num_process_data);
        *ppOutBuffer = mDcRemoveWorkBuffer;
        *pOutBytes = num_process_data;
    }
    ALOGV("%s(), inBytes: %d, pOutBytes: %d ppOutBuffer = %p", __FUNCTION__, inBytes, *pOutBytes, *ppOutBuffer);
    ASSERT(*ppOutBuffer != NULL && *pOutBytes != 0);
    return NO_ERROR;
}


status_t AudioALSAPlaybackHandlerBase::initBliSrc() {
    // init BLI SRC if need
    if (mStreamAttributeSource->sample_rate  != mStreamAttributeTarget.sample_rate  ||
        mStreamAttributeSource->num_channels != mStreamAttributeTarget.num_channels) {
        ALOGD("%s(), sample_rate: %d => %d, num_channels: %d => %d, mStreamAttributeSource->audio_format: 0x%x", __FUNCTION__,
              mStreamAttributeSource->sample_rate,  mStreamAttributeTarget.sample_rate,
              mStreamAttributeSource->num_channels, mStreamAttributeTarget.num_channels,
              mStreamAttributeSource->audio_format);

        SRC_PCM_FORMAT src_pcm_format = SRC_IN_Q1P15_OUT_Q1P15;
        if (mStreamAttributeSource->audio_format == AUDIO_FORMAT_PCM_32_BIT) {
            src_pcm_format = SRC_IN_Q1P31_OUT_Q1P31;
        } else if (mStreamAttributeSource->audio_format == AUDIO_FORMAT_PCM_16_BIT) {
            src_pcm_format = SRC_IN_Q1P15_OUT_Q1P15;
        } else {
            ALOGE("%s(), not support mStreamAttributeSource->audio_format(0x%x) SRC!!", __FUNCTION__, mStreamAttributeSource->audio_format);
        }

        mBliSrc = newMtkAudioSrc(
                      mStreamAttributeSource->sample_rate, mStreamAttributeSource->num_channels,
                      mStreamAttributeTarget.sample_rate,  mStreamAttributeTarget.num_channels,
                      src_pcm_format);
        ASSERT(mBliSrc != NULL);
        mBliSrc->open();

        mBliSrcOutputBuffer = new char[kBliSrcOutputBufferSize];
        ASSERT(mBliSrcOutputBuffer != NULL);
    }

    return NO_ERROR;
}


status_t AudioALSAPlaybackHandlerBase::deinitBliSrc() {
    // deinit BLI SRC if need
    if (mBliSrc != NULL) {
        mBliSrc->close();
        deleteMtkAudioSrc(mBliSrc);
        mBliSrc = NULL;
    }

    if (mBliSrcOutputBuffer != NULL) {
        delete[] mBliSrcOutputBuffer;
        mBliSrcOutputBuffer = NULL;
    }

    return NO_ERROR;
}


status_t AudioALSAPlaybackHandlerBase::doBliSrc(void *pInBuffer, uint32_t inBytes, void **ppOutBuffer, uint32_t *pOutBytes) {
    if (mBliSrc == NULL) { // No need SRC
        *ppOutBuffer = pInBuffer;
        *pOutBytes = inBytes;
    } else {
        char *p_read = (char *)pInBuffer;
        uint32_t num_raw_data_left = inBytes;
        uint32_t num_converted_data = kBliSrcOutputBufferSize; // max convert num_free_space

        uint32_t consumed = num_raw_data_left;
        mBliSrc->process((int16_t *)p_read, &num_raw_data_left,
                         (int16_t *)mBliSrcOutputBuffer, &num_converted_data);
        consumed -= num_raw_data_left;
        p_read += consumed;

        ALOGV("%s(), num_raw_data_left = %u, num_converted_data = %u",
              __FUNCTION__, num_raw_data_left, num_converted_data);

        if (num_raw_data_left > 0) {
            ALOGW("%s(), num_raw_data_left(%u) > 0", __FUNCTION__, num_raw_data_left);
            ASSERT(num_raw_data_left == 0);
        }

        *ppOutBuffer = mBliSrcOutputBuffer;
        *pOutBytes = num_converted_data;
    }

    ASSERT(*ppOutBuffer != NULL && *pOutBytes != 0);
    return NO_ERROR;
}


pcm_format AudioALSAPlaybackHandlerBase::transferAudioFormatToPcmFormat(const audio_format_t audio_format) const {
    pcm_format retval = PCM_FORMAT_S16_LE;

    switch (audio_format) {
    case AUDIO_FORMAT_PCM_8_BIT: {
        retval = PCM_FORMAT_S8;
        break;
    }
    case AUDIO_FORMAT_PCM_16_BIT: {
        retval = PCM_FORMAT_S16_LE;
        break;
    }
    case AUDIO_FORMAT_PCM_8_24_BIT: {
        retval = PCM_FORMAT_S32_LE; //PCM_FORMAT_S24_LE; // TODO(Harvey, Chipeng): distinguish Q9P23 from Q1P31
        break;
    }
    case AUDIO_FORMAT_PCM_32_BIT: {
        retval = PCM_FORMAT_S32_LE;
        break;
    }
    default: {
        ALOGE("No such audio format(0x%x)!! Use AUDIO_FORMAT_PCM_16_BIT(0x%x) instead", audio_format, PCM_FORMAT_S16_LE);
        retval = PCM_FORMAT_S16_LE;
        break;
    }
    }

    ALOGD("%s(), audio_format(0x%x) => pcm_format(0x%x)", __FUNCTION__, audio_format, retval);
    return retval;
}


status_t AudioALSAPlaybackHandlerBase::initBitConverter() {
    // init bit converter if need
    if (mStreamAttributeSource->audio_format != mStreamAttributeTarget.audio_format) {
        BCV_PCM_FORMAT bcv_pcm_format;
        bool isInputValid = true;
        if ((mStreamAttributeSource->audio_format == AUDIO_FORMAT_PCM_32_BIT) || (mStreamAttributeSource->audio_format == AUDIO_FORMAT_PCM_8_24_BIT)) {
            if (mStreamAttributeTarget.audio_format == AUDIO_FORMAT_PCM_16_BIT) {
                bcv_pcm_format = BCV_IN_Q1P31_OUT_Q1P15;
            } else if (mStreamAttributeTarget.audio_format == AUDIO_FORMAT_PCM_8_24_BIT) {
                bcv_pcm_format = BCV_IN_Q1P31_OUT_Q9P23;
            } else {
                isInputValid = false;
            }
        } else if (mStreamAttributeSource->audio_format == AUDIO_FORMAT_PCM_16_BIT) {
            if (mStreamAttributeTarget.audio_format == AUDIO_FORMAT_PCM_32_BIT) {
                bcv_pcm_format = BCV_IN_Q1P15_OUT_Q1P31;
            } else if (mStreamAttributeTarget.audio_format == AUDIO_FORMAT_PCM_8_24_BIT) {
                bcv_pcm_format = BCV_IN_Q1P15_OUT_Q9P23;
            } else {
                isInputValid = false;
            }
        } else if (mStreamAttributeSource->audio_format == AUDIO_FORMAT_MP3) { //doug for tunneling
            if (mStreamAttributeTarget.audio_format == AUDIO_FORMAT_PCM_16_BIT) {
                return NO_ERROR;
            } else if (mStreamAttributeTarget.audio_format == AUDIO_FORMAT_PCM_8_24_BIT) {
                bcv_pcm_format = BCV_IN_Q1P15_OUT_Q9P23;
            } else {
                isInputValid = false;
            }
        }

        if (!isInputValid) {
            ASSERT(0);
            ALOGD("%s(), invalid, audio_format: 0x%x => 0x%x",
                  __FUNCTION__, mStreamAttributeSource->audio_format, mStreamAttributeTarget.audio_format);
            return INVALID_OPERATION;
        }

        ALOGD("%s(), audio_format: 0x%x => 0x%x, bcv_pcm_format = 0x%x",
              __FUNCTION__, mStreamAttributeSource->audio_format, mStreamAttributeTarget.audio_format, bcv_pcm_format);

        if (mStreamAttributeSource->num_channels > 2) {
            mBitConverter = newMtkAudioBitConverter(
                                mStreamAttributeSource->sample_rate,
                                2,
                                bcv_pcm_format);
        } else {
            mBitConverter = newMtkAudioBitConverter(
                                mStreamAttributeSource->sample_rate,
                                mStreamAttributeSource->num_channels,
                                bcv_pcm_format);
        }

        ASSERT(mBitConverter != NULL);
        mBitConverter->open();
        mBitConverter->resetBuffer();

        mBitConverterOutputBuffer = new char[kMaxPcmDriverBufferSize];
        ASSERT(mBitConverterOutputBuffer != NULL);
        ASSERT(mBitConverterOutputBuffer != NULL);
    }

    ALOGV("%s(), mBitConverter = %p, mBitConverterOutputBuffer = %p", __FUNCTION__, mBitConverter, mBitConverterOutputBuffer);
    return NO_ERROR;
}


status_t AudioALSAPlaybackHandlerBase::deinitBitConverter() {
    // deinit bit converter if need
    if (mBitConverter != NULL) {
        mBitConverter->close();
        deleteMtkAudioBitConverter(mBitConverter);
        mBitConverter = NULL;
    }

    if (mBitConverterOutputBuffer != NULL) {
        delete[] mBitConverterOutputBuffer;
        mBitConverterOutputBuffer = NULL;
    }

    return NO_ERROR;
}


status_t AudioALSAPlaybackHandlerBase::doBitConversion(void *pInBuffer, uint32_t inBytes, void **ppOutBuffer, uint32_t *pOutBytes) {
    if (mBitConverter != NULL) {
        *pOutBytes = kPcmDriverBufferSize;
        mBitConverter->process(pInBuffer, &inBytes, (void *)mBitConverterOutputBuffer, pOutBytes);
        *ppOutBuffer = mBitConverterOutputBuffer;
    } else {
        *ppOutBuffer = pInBuffer;
        *pOutBytes = inBytes;
    }

    ASSERT(*ppOutBuffer != NULL && *pOutBytes != 0);
    return NO_ERROR;
}

// we assue that buufer should write as 64 bytes align , so only src handler is create,
// will cause output buffer is not 64 bytes align
status_t AudioALSAPlaybackHandlerBase::initDataPending() {
    ALOGV("mBliSrc = %p", mBliSrc);
    if (mBliSrc != NULL || mDataPendingForceUse) {
        mdataPendingOutputBufferSize = (1024 * 128) + mDataAlignedSize; // here nned to cover max write buffer size
        mdataPendingOutputBuffer = new char[mdataPendingOutputBufferSize];
        mdataPendingTempBuffer  = new char[mDataAlignedSize];
        ASSERT(mdataPendingOutputBuffer != NULL);
    }
    return NO_ERROR;
}

status_t AudioALSAPlaybackHandlerBase::DeinitDataPending() {
    ALOGD("DeinitDataPending");
    if (mdataPendingOutputBuffer != NULL) {
        delete[] mdataPendingOutputBuffer;
        mdataPendingOutputBuffer = NULL;
    }
    if (mdataPendingTempBuffer != NULL) {
        delete[] mdataPendingTempBuffer ;
        mdataPendingTempBuffer = NULL;
    }
    mdataPendingOutputBufferSize = 0;
    mdataPendingRemindBufferSize = 0;
    return NO_ERROR;
}

// we assue that buufer should write as 64 bytes align , so only src handler is create,
// will cause output buffer is not 64 bytes align
status_t AudioALSAPlaybackHandlerBase::dodataPending(void *pInBuffer, uint32_t inBytes, void **ppOutBuffer, uint32_t *pOutBytes) {
    char *DataPointer = (char *)mdataPendingOutputBuffer;
    char *DatainputPointer = (char *)pInBuffer;
    uint32 TotalBufferSize  = inBytes + mdataPendingRemindBufferSize;
    uint32 tempRemind = TotalBufferSize % mDataAlignedSize;
    uint32 TotalOutputSize = TotalBufferSize - tempRemind;
    uint32 TotalOutputCount = TotalOutputSize;
    if (mBliSrc != NULL || mDataPendingForceUse) { // do data pending
        //ALOGD("inBytes = %d mdataPendingRemindBufferSize = %d TotalOutputSize = %d",inBytes,mdataPendingRemindBufferSize,TotalOutputSize);

        if (TotalOutputSize != 0) {
            if (mdataPendingRemindBufferSize != 0) { // deal previous remaind buffer
                memcpy((void *)DataPointer, (void *)mdataPendingTempBuffer, mdataPendingRemindBufferSize);
                DataPointer += mdataPendingRemindBufferSize;
                TotalOutputCount -= mdataPendingRemindBufferSize;
            }

            //deal with input buffer
            memcpy((void *)DataPointer, pInBuffer, TotalOutputCount);
            DataPointer += TotalOutputCount;
            DatainputPointer += TotalOutputCount;
            TotalOutputCount = 0;

            //ALOGD("tempRemind = %d pOutBytes = %d",tempRemind,*pOutBytes);

            // deal with remind buffer
            memcpy((void *)mdataPendingTempBuffer, (void *)DatainputPointer, tempRemind);
            mdataPendingRemindBufferSize = tempRemind;
        } else {
            // deal with remind buffer
            memcpy((void *)(mdataPendingTempBuffer + mdataPendingRemindBufferSize), (void *)DatainputPointer, inBytes);
            mdataPendingRemindBufferSize += inBytes;
        }

        // update pointer and data count
        *ppOutBuffer = mdataPendingOutputBuffer;
        *pOutBytes = TotalOutputSize;
    } else {
        *ppOutBuffer = pInBuffer;
        *pOutBytes = inBytes;
    }

    ASSERT(*ppOutBuffer != NULL);
    if (!mDataPendingForceUse) {
        ASSERT(*pOutBytes != 0);
    }
    return NO_ERROR;
}




void AudioALSAPlaybackHandlerBase::OpenPCMDump(const char *class_name) {
    ALOGV("%s()", __FUNCTION__);
    char mDumpFileName[128];
    sprintf(mDumpFileName, "%s.%d.%s.pid%d.tid%d.pcm", streamout, mDumpFileNum, class_name, getpid(), gettid());

    mPCMDumpFile = NULL;
    mPCMDumpFile = AudioOpendumpPCMFile(mDumpFileName, streamout_propty);

    if (mPCMDumpFile != NULL) {
        ALOGD("%s DumpFileName = %s", __FUNCTION__, mDumpFileName);

        mDumpFileNum++;
        mDumpFileNum %= MAX_DUMP_NUM;
    }
}

void AudioALSAPlaybackHandlerBase::ClosePCMDump() {
    ALOGV("%s()", __FUNCTION__);
    if (mPCMDumpFile) {
        AudioCloseDumpPCMFile(mPCMDumpFile);
        ALOGD("%s(), close it", __FUNCTION__);
    }
}

void  AudioALSAPlaybackHandlerBase::WritePcmDumpData(const void *buffer, ssize_t bytes) {
    if (mPCMDumpFile) {
        //ALOGD("%s()", __FUNCTION__);
        AudioDumpPCMData((void *)buffer, bytes, mPCMDumpFile);
    }
}

#if defined(MTK_HYBRID_NLE_SUPPORT)
status_t AudioALSAPlaybackHandlerBase::initNLEProcessing() {
    status_t dRet;

    if (mNLEMnger != NULL) {
        return ALREADY_EXISTS;
    }

    mNLEMnger = AudioALSAHyBridNLEManager::getInstance();

    if (mNLEMnger == NULL) {
        ALOGE("[Err] NLE %s New Fail Line#%d", __FUNCTION__, __LINE__);
        return NO_MEMORY;
    }

    dRet = mNLEMnger->initPlayBackHandler(mPlaybackHandlerType, &mStreamAttributeTarget, this);

    if (dRet != NO_ERROR) {
        mNLEMnger = NULL;
        ALOGV("Unsupport the Handler NLE %s init Fail Line#%d", __FUNCTION__, __LINE__);
    }
    return dRet;
}


status_t AudioALSAPlaybackHandlerBase::deinitNLEProcessing() {
    status_t dRet;

    if (mNLEMnger != NULL) {
        dRet = mNLEMnger->deinitPlayBackHandler(mPlaybackHandlerType);
        if (dRet != NO_ERROR) {
            ALOGW("[Warn] NLE %s deinit Fail Line#%d", __FUNCTION__, __LINE__);
        }
        mNLEMnger = NULL;
        return dRet;
    } else {
        ALOGV("Unsupport the Handler NLE %s ObjNull Fail Line#%d", __FUNCTION__, __LINE__);
        return NO_INIT;
    }
}


status_t AudioALSAPlaybackHandlerBase::doNLEProcessing(void *pInBuffer, size_t inBytes) {
    size_t dWriteByte = 0;
    status_t dRet;

    if (mNLEMnger != NULL) {
        dRet = mNLEMnger->process(mPlaybackHandlerType, pInBuffer, inBytes);
        if (dRet != NO_ERROR) {
            ALOGV("[Warn] NLE %s Line#%d dRet %d", __FUNCTION__, __LINE__, dRet);
        }
        return dRet;
    } else {
        ALOGV("[Warn] NLE %s ObjNull Fail Line#%d", __FUNCTION__, __LINE__);
        return NO_INIT;
    }
}
#else
status_t AudioALSAPlaybackHandlerBase::initNLEProcessing() {
    return INVALID_OPERATION;
}

status_t AudioALSAPlaybackHandlerBase::deinitNLEProcessing() {
    return INVALID_OPERATION;
}

status_t AudioALSAPlaybackHandlerBase::doNLEProcessing(void *pInBuffer __unused, size_t inBytes __unused) {
    return INVALID_OPERATION;
}
#endif



/*
 * =============================================================================
 *                     Aurisys Framework 2.0
 * =============================================================================
 */

#ifdef MTK_AURISYS_FRAMEWORK_SUPPORT
void AudioALSAPlaybackHandlerBase::CreateAurisysLibManager() {
    uint32_t sample_rate = 0 ;
    const char* libKey = NULL;

    /* scenario & sample rate */
    if (IsVoIPEnable()) {
        mAurisysScenario = AURISYS_SCENARIO_VOIP_WITHOUT_AEC; /* only DL */
        sample_rate = 16000;
        libKey = "MTKSE";
    } else if (mStreamAttributeSource->mAudioOutputFlags & AUDIO_OUTPUT_FLAG_FAST) {
        mAurisysScenario = AURISYS_SCENARIO_PLAYBACK_LOW_LATENCY;
        sample_rate = mStreamAttributeSource->sample_rate;
        libKey = "MTKBESSOUND";
    } else {
        mAurisysScenario = AURISYS_SCENARIO_PLAYBACK_NORMAL;
        sample_rate = mStreamAttributeSource->sample_rate;
        libKey = "MTKBESSOUND";
    }

    ALOGD("%s, voip: %d, mAurisysScenario: %u", __FUNCTION__, IsVoIPEnable(), mAurisysScenario);

    aurisys_user_prefer_configs_t prefer_configs;
    prefer_configs.audio_format = mStreamAttributeSource->audio_format;
    prefer_configs.sample_rate = mStreamAttributeSource->sample_rate;
    prefer_configs.frame_size_ms = 20; /* TODO */
    prefer_configs.num_channels_ul = 2;
    prefer_configs.num_channels_dl = mStreamAttributeSource->num_channels;

    AL_AUTOLOCK(mAurisysLibManagerLock);

    mAurisysLibManager = create_aurisys_lib_manager(mAurisysScenario, &prefer_configs);


    InitArsiTaskConfig(mAurisysLibManager);
    InitBufferConfig(mAurisysLibManager);

    aurisys_create_arsi_handlers(mAurisysLibManager); /* should init task/buf configs first */
    aurisys_pool_buf_formatter_init(mAurisysLibManager); /* should init task/buf configs first */

#if (defined(MTK_AUDIO_HIERARCHICAL_PARAM_SUPPORT) && (MTK_AUDIO_TUNING_TOOL_V2_PHASE >= 2))
    // Set custom scene information to all DL lib
    AppOps *appOps = appOpsGetInstance();
    if (appOps && appOps->appHandleIsFeatureOptionEnabled(appOps->appHandleGetInstance(), "VIR_SCENE_CUSTOMIZATION_SUPPORT"))
    {
        /* Apply parameter for scene */
        const char* scenarioStr = get_string_by_enum_aurisys_scenario(mAurisysScenario);
        String8 key_value_pair = String8::format("HAL,%s,%s,KEY_VALUE,SetAudioCustomScene,%s=SET",
            scenarioStr,
            libKey,
            mStreamAttributeSource->mCustScene);
        aurisys_set_parameter(key_value_pair.string());

        key_value_pair = String8::format("HAL,%s,%s,APPLY_PARAM,0=SET", scenarioStr, libKey);
        aurisys_set_parameter(key_value_pair.string());
    }
#endif

    //aurisys_set_dl_digital_gain(mAurisysLibManager, 0, 0);
}


/* TODO: move to aurisys framework?? add a new struct to keep hal arributes */
void AudioALSAPlaybackHandlerBase::InitArsiTaskConfig(struct aurisys_lib_manager_t *manager) {
    arsi_task_config_t *arsi_task_config = get_arsi_task_config(manager);

    /* input device */ /* TODO: get voip ul attribute */
    arsi_task_config->input_device_info.devices = mStreamAttributeSource->input_device; /* TODO */
    arsi_task_config->input_device_info.audio_format = mStreamAttributeSource->audio_format;
    arsi_task_config->input_device_info.sample_rate = mStreamAttributeSource->sample_rate; /* TODO */
    arsi_task_config->input_device_info.channel_mask = mStreamAttributeSource->audio_channel_mask;
    arsi_task_config->input_device_info.num_channels = mStreamAttributeSource->num_channels;
    arsi_task_config->input_device_info.hw_info_mask = 0; /* TODO */

    /* output device */
    audio_devices_t output_devices = mStreamAttributeSource->output_devices;
    if (isBtSpkDevice(output_devices)) {
        // use SPK setting for BTSCO + SPK
        output_devices = (audio_devices_t)(output_devices & (~AUDIO_DEVICE_OUT_ALL_SCO));
    }

    arsi_task_config->output_device_info.devices = mStreamAttributeSource->output_devices;
    arsi_task_config->output_device_info.audio_format = mStreamAttributeSource->audio_format;
    arsi_task_config->output_device_info.sample_rate = mStreamAttributeSource->sample_rate;
    arsi_task_config->output_device_info.channel_mask = mStreamAttributeSource->audio_channel_mask;
    arsi_task_config->output_device_info.num_channels = mStreamAttributeSource->num_channels;
    if (AudioSmartPaController::getInstance()->isSmartPAUsed()) {
        arsi_task_config->output_device_info.hw_info_mask = OUTPUT_DEVICE_HW_INFO_SMARTPA_SPEAKER; /* SMARTPA */
    } else {
        arsi_task_config->output_device_info.hw_info_mask = 0;
    }
    /* task scene */
    arsi_task_config->task_scene = get_task_scene_of_manager(manager);

    /* audio mode */
    arsi_task_config->audio_mode = mStreamAttributeSource->audio_mode;

    /* max device capability for allocating memory */
    arsi_task_config->max_input_device_sample_rate  = 48000; /* TODO */
    arsi_task_config->max_output_device_sample_rate = 48000; /* TODO */

    arsi_task_config->max_input_device_num_channels  = 2; /* TODO */
    arsi_task_config->max_output_device_num_channels = 2; /* TODO */

    /* flag & source */
    arsi_task_config->output_flags = mStreamAttributeSource->mAudioOutputFlags;
    arsi_task_config->input_source = mStreamAttributeSource->input_source;
    arsi_task_config->input_flags  = 0;

    /* Enhancement feature */
    if (arsi_task_config->output_device_info.devices == AUDIO_DEVICE_OUT_EARPIECE &&
        SpeechEnhancementController::GetInstance()->GetHACOn()) {
        arsi_task_config->enhancement_feature_mask |= ENHANCEMENT_FEATURE_EARPIECE_HAC;
    }

    if ((arsi_task_config->input_device_info.devices & AUDIO_DEVICE_IN_ALL_SCO)
        && (arsi_task_config->output_device_info.devices & AUDIO_DEVICE_OUT_ALL_SCO)
        && SpeechEnhancementController::GetInstance()->GetBtHeadsetNrecOn()) {
        arsi_task_config->enhancement_feature_mask |= ENHANCEMENT_FEATURE_BT_NREC;
    }


    dump_task_config(arsi_task_config);
}


void AudioALSAPlaybackHandlerBase::InitBufferConfig(struct aurisys_lib_manager_t *manager) {
    /* DL in */
    mAudioPoolBufDlIn = create_audio_pool_buf(manager, DATA_BUF_DOWNLINK_IN, kAurisysBufSizeDlIn);

    mAudioPoolBufDlIn->buf->b_interleave = 1; /* LRLRLRLR*/
    mAudioPoolBufDlIn->buf->frame_size_ms = 0;
    mAudioPoolBufDlIn->buf->num_channels = mStreamAttributeSource->num_channels;
    mAudioPoolBufDlIn->buf->sample_rate_buffer = mStreamAttributeSource->sample_rate;
    mAudioPoolBufDlIn->buf->sample_rate_content = mStreamAttributeSource->sample_rate;
    mAudioPoolBufDlIn->buf->audio_format = mStreamAttributeSource->audio_format;


    /* DL out */
    mAudioPoolBufDlOut = create_audio_pool_buf(manager, DATA_BUF_DOWNLINK_OUT, kAurisysBufSizeDlOut);
    if (IsVoIPEnable()) {
        mTransferredBufferSize = GetTransferredBufferSize(mStreamAttributeSource->buffer_size,
                                                          mStreamAttributeSource,
                                                          &mStreamAttributeTarget);
        ALOGD("insert %u bytes for VoIP SRC", mTransferredBufferSize);
        audio_ringbuf_write_zero(&mAudioPoolBufDlOut->ringbuf, mTransferredBufferSize);
    } else {
        mTransferredBufferSize = 0xFFFFFFFF;
    }

    mAudioPoolBufDlOut->buf->b_interleave = 1; /* LRLRLRLR*/
    mAudioPoolBufDlOut->buf->frame_size_ms = 0;
    mAudioPoolBufDlOut->buf->num_channels = mStreamAttributeTarget.num_channels;
    mAudioPoolBufDlOut->buf->sample_rate_buffer = mStreamAttributeTarget.sample_rate;
    mAudioPoolBufDlOut->buf->sample_rate_content = mStreamAttributeTarget.sample_rate;
    mAudioPoolBufDlOut->buf->audio_format = mStreamAttributeTarget.audio_format;


    if (IsVoIPEnable()) {
        mAudioPoolBufUlIn = create_audio_pool_buf(manager, DATA_BUF_UPLINK_IN, kAurisysBufSizeUlIn);

        mAudioPoolBufUlIn->buf->b_interleave = 1; /* LRLRLRLR*/
        mAudioPoolBufUlIn->buf->frame_size_ms = 0;
        mAudioPoolBufUlIn->buf->num_channels = mStreamAttributeSource->num_channels;
        mAudioPoolBufUlIn->buf->sample_rate_buffer = mStreamAttributeSource->sample_rate;
        mAudioPoolBufUlIn->buf->sample_rate_content = mStreamAttributeSource->sample_rate;
        mAudioPoolBufUlIn->buf->audio_format = mStreamAttributeSource->audio_format;

        mAudioPoolBufUlOut = create_audio_pool_buf(manager, DATA_BUF_UPLINK_OUT, kAurisysBufSizeUlOut);

        mAudioPoolBufUlOut->buf->b_interleave = 1; /* LRLRLRLR*/
        mAudioPoolBufUlOut->buf->frame_size_ms = 0;
        mAudioPoolBufUlOut->buf->num_channels = mStreamAttributeTarget.num_channels;
        mAudioPoolBufUlOut->buf->sample_rate_buffer = mStreamAttributeTarget.sample_rate;
        mAudioPoolBufUlOut->buf->sample_rate_content = mStreamAttributeTarget.sample_rate;
        mAudioPoolBufUlOut->buf->audio_format = mStreamAttributeTarget.audio_format;
    }

    AUDIO_ALLOC_CHAR_BUFFER(mLinearOut, kAurisysBufSizeDlOut);
}


void AudioALSAPlaybackHandlerBase::DestroyAurisysLibManager() {
    ALOGD("%s()", __FUNCTION__);

    AL_AUTOLOCK(mAurisysLibManagerLock);

    aurisys_destroy_lib_handlers(mAurisysLibManager);
    aurisys_pool_buf_formatter_deinit(mAurisysLibManager);
    destroy_aurisys_lib_manager(mAurisysLibManager);

    /* NOTE: auto destroy audio_pool_buf when destroy_aurisys_lib_manager() */
    mAudioPoolBufUlIn = NULL;
    mAudioPoolBufUlOut = NULL;
    mAudioPoolBufDlIn = NULL;
    mAudioPoolBufDlOut = NULL;

    mAurisysLibManager = NULL;

    mIsNeedUpdateLib = false;

    AUDIO_FREE_POINTER(mLinearOut);
}


uint32_t AudioALSAPlaybackHandlerBase::GetTransferredBufferSize(uint32_t sourceBytes,
                                                                const stream_attribute_t *source,
                                                                const stream_attribute_t *target) {

    uint32_t bytesPerSampleSource = (uint32_t)audio_bytes_per_sample(source->audio_format);
    uint32_t bytesPerSampleTarget = (uint32_t)audio_bytes_per_sample(target->audio_format);

    uint32_t bytesPerSecondSource = source->sample_rate * source->num_channels * bytesPerSampleSource;
    uint32_t bytesPerSecondTarget = target->sample_rate * target->num_channels * bytesPerSampleTarget;

    uint32_t unitTargetBytes = bytesPerSampleTarget * target->num_channels;
    uint32_t targetBytes = 0;

    if (bytesPerSecondSource == 0 || bytesPerSecondTarget == 0 || unitTargetBytes == 0) {
        ALOGW("%s(), audio_format: 0x%x, 0x%x error!!", __FUNCTION__,
              source->audio_format, target->audio_format);
        return sourceBytes;
    }

    targetBytes = (uint32_t)((double)sourceBytes *
                              ((double)bytesPerSecondTarget /
                               (double)bytesPerSecondSource));
    if ((targetBytes % unitTargetBytes) != 0) {
        targetBytes = ((targetBytes / unitTargetBytes) + 1) * unitTargetBytes; // cell()
    }

    return targetBytes;
}

#endif /* end if MTK_AURISYS_FRAMEWORK_SUPPORT */


} // end of namespace android
