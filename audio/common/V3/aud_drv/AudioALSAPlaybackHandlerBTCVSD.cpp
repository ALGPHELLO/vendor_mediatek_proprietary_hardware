#include "AudioALSAPlaybackHandlerBTCVSD.h"

#include <inttypes.h>
#include <time.h>

#include "AudioUtility.h"

#include "WCNChipController.h"
#include "AudioALSACaptureDataProviderEchoRefBTCVSD.h"
#include "AudioALSACaptureDataProviderBTCVSD.h"
#include "AudioALSAStreamManager.h"
#include "AudioBTCVSDControl.h"
#include "AudioALSADriverUtility.h"

#if (defined(MTK_AUDIO_HIERARCHICAL_PARAM_SUPPORT) && (MTK_AUDIO_TUNING_TOOL_V2_PHASE >= 2))
#include "AudioParamParser.h"
#endif


#ifdef MTK_AURISYS_FRAMEWORK_SUPPORT
#include <audio_ringbuf.h>
#include <audio_pool_buf_handler.h>

#include <aurisys_controller.h>
#include <aurisys_lib_manager.h>
#endif



#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "AudioALSAPlaybackHandlerBTCVSD"

//#define DEBUG_TIMESTAMP

#ifdef DEBUG_TIMESTAMP
#define SHOW_TIMESTAMP(format, args...) ALOGD(format, ##args)
#else
#define SHOW_TIMESTAMP(format, args...)
#endif

#if defined(PLAYBACK_USE_24BITS_ONLY)
#define BUFFER_SIZE_PER_ACCESSS     (8192)
#else
#define BUFFER_SIZE_PER_ACCESSS     (4096)
#endif

#define calc_time_diff(x,y) ((x.tv_sec - y.tv_sec )+ (double)( x.tv_nsec - y.tv_nsec ) / (double)1000000000)

namespace android {
/*==============================================================================
 *                     Implementation
 *============================================================================*/

static bool mBTMode_Open;

AudioALSAPlaybackHandlerBTCVSD::AudioALSAPlaybackHandlerBTCVSD(const stream_attribute_t *stream_attribute_source) :
    AudioALSAPlaybackHandlerBase(stream_attribute_source),
    mTotalEchoRefBufSize(0),
    mDataProviderEchoRefBTCVSD(AudioALSACaptureDataProviderEchoRefBTCVSD::getInstance()),
    mWCNChipController(WCNChipController::GetInstance()),
    mAudioBTCVSDControl(AudioBTCVSDControl::getInstance()),
    mMixer(AudioALSADriverUtility::getInstance()->getMixer())
#ifndef MTK_SUPPORT_BTCVSD_ALSA
    , mFd2(mAudioBTCVSDControl->getFd())
#else
    , mFd2(-1)
#endif
{
    ALOGD("%s()", __FUNCTION__);
    mPlaybackHandlerType = PLAYBACK_HANDLER_BT_CVSD;

    /* Init EchoRef Resource */
    memset(&mEchoRefStartTime, 0, sizeof(mEchoRefStartTime));

    /* Init EchoRef Resource */
    memset(&mStreamAttributeTargetEchoRef, 0, sizeof(mStreamAttributeTargetEchoRef));

    /* Init timestamp*/
    memset(&mNewtime, 0, sizeof(mNewtime));
    memset(&mOldtime, 0, sizeof(mOldtime));
}


AudioALSAPlaybackHandlerBTCVSD::~AudioALSAPlaybackHandlerBTCVSD() {
    ALOGD("%s()", __FUNCTION__);
}


status_t AudioALSAPlaybackHandlerBTCVSD::open() {
    ALOGD("+%s(), mDevice = 0x%x, sample_rate = %d, num_channels = %d, buffer_size = %d, audio_format = %d",
          __FUNCTION__, mStreamAttributeSource->output_devices,
          mStreamAttributeSource->sample_rate, mStreamAttributeSource->num_channels,
          mStreamAttributeSource->buffer_size, mStreamAttributeSource->audio_format);

    // debug pcm dump
    OpenPCMDump(LOG_TAG);

    // HW attribute config // TODO(Harvey): query this
    mStreamAttributeTarget.audio_format = AUDIO_FORMAT_PCM_16_BIT;
    mStreamAttributeTarget.audio_channel_mask = AUDIO_CHANNEL_IN_STEREO;
    mStreamAttributeTarget.num_channels = popcount(mStreamAttributeTarget.audio_channel_mask);
    mStreamAttributeTarget.sample_rate = mWCNChipController->GetBTCurrentSamplingRateNumber();

    // Setup echoref stream attribute
    mStreamAttributeTargetEchoRef.audio_format = mStreamAttributeTarget.audio_format;
    mStreamAttributeTargetEchoRef.audio_channel_mask = mStreamAttributeTarget.audio_channel_mask;
    mStreamAttributeTargetEchoRef.num_channels = mStreamAttributeTarget.num_channels;
#ifdef MTK_AURISYS_FRAMEWORK_SUPPORT
    mStreamAttributeTargetEchoRef.sample_rate = mStreamAttributeTarget.sample_rate;
#else
    mStreamAttributeTargetEchoRef.sample_rate = mStreamAttributeSource->sample_rate;    // No src applied, using source's sample rate
#endif

#ifdef MTK_SUPPORT_BTCVSD_ALSA
#if 0
    int pcmindex = AudioALSADeviceParser::getInstance()->GetPcmIndexByString(keypcmBTCVSDPlayback);
    int cardindex = AudioALSADeviceParser::getInstance()->GetCardIndexByString(keypcmBTCVSDPlayback);

    struct pcm_params *params;
    params = pcm_params_get(cardindex, pcmindex,  PCM_OUT);
    if (params == NULL) {
        ALOGD("Device does not exist.\n");
    }

    // HW pcm config
    mConfig.channels = mStreamAttributeTarget.num_channels;
    mConfig.rate = mStreamAttributeTarget.sample_rate;
    mConfig.period_count = 2;
    mConfig.period_size = 1024;//(mStreamAttributeTarget.buffer_size / (mConfig.channels * mConfig.period_count)) / ((mStreamAttributeTarget.audio_format == AUDIO_FORMAT_PCM_16_BIT) ? 2 : 4);
    mConfig.format = PCM_FORMAT_S16_LE;//transferAudioFormatToPcmFormat(mStreamAttributeTarget.audio_format);
    mConfig.start_threshold = 0;
    mConfig.stop_threshold = 0;
    mConfig.silence_threshold = 0;
    ALOGD("%s(), mConfig: channels = %d, rate = %d, period_size = %d, period_count = %d, format = %d, pcmindex=%d",
          __FUNCTION__, mConfig.channels, mConfig.rate, mConfig.period_size, mConfig.period_count, mConfig.format, pcmindex);

    // open pcm driver
    openPcmDriver(pcmindex);
    //openPcmDriver(26);
#else
    //////copy from Capture
    memset(&mConfig, 0, sizeof(mConfig));
    mConfig.channels = mStreamAttributeTarget.num_channels;
    mConfig.rate = mStreamAttributeTarget.sample_rate;
    mConfig.period_size = 1020; // must be multiple of SCO_TX_ENCODE_SIZE
    mConfig.period_count = 2;
    mConfig.format = PCM_FORMAT_S16_LE;
    mConfig.start_threshold = 0;
    mConfig.stop_threshold = 0;
    mConfig.silence_threshold = 0;

    ASSERT(mPcm == NULL);
    int pcmIdx = AudioALSADeviceParser::getInstance()->GetPcmIndexByString(keypcmBTCVSDPlayback);
    int cardIdx = AudioALSADeviceParser::getInstance()->GetCardIndexByString(keypcmBTCVSDPlayback);
    mPcm = pcm_open(cardIdx, pcmIdx, PCM_OUT, &mConfig);
    ASSERT(mPcm != NULL && pcm_is_ready(mPcm) == true);
    pcm_start(mPcm);

    /////////////////////
#endif
#endif
    mBTMode_Open = mAudioBTCVSDControl->BT_SCO_isWideBand();
    if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "btcvsd_band"), mBTMode_Open ? "WB" : "NB")) {
        ALOGE("Error: btcvsd_band invalid value");
    }

    if (mStreamAttributeSource->buffer_size < BUFFER_SIZE_PER_ACCESSS) {
        ALOGD("%s(), data align to %d", __FUNCTION__, BUFFER_SIZE_PER_ACCESSS);
        mDataAlignedSize = BUFFER_SIZE_PER_ACCESSS;
        mDataPendingForceUse = true;
    }
    initDataPending();

    // init DC Removal
    initDcRemoval();

#ifdef MTK_AURISYS_FRAMEWORK_SUPPORT
    if (get_aurisys_on()) {
        // open mAudioBTCVSDControl
        mAudioBTCVSDControl->BTCVSD_Init(mFd2, mStreamAttributeTarget.sample_rate, mStreamAttributeTarget.num_channels);

        CreateAurisysLibManager();
    } else
#endif
    {
        // open mAudioBTCVSDControl
        mAudioBTCVSDControl->BTCVSD_Init(mFd2, mStreamAttributeSource->sample_rate, mStreamAttributeSource->num_channels);

        // bit conversion
        initBitConverter();
    }

    /* Reset software timestamp information */
    mTotalEchoRefBufSize = 0;
    memset((void *)&mEchoRefStartTime, 0, sizeof(mEchoRefStartTime));

    ALOGD("-%s(), mStreamAttributeTarget, ch=%d, sr=%d, mStreamAttributeTargetEchoRef, ch=%d, sr=%d",
        __FUNCTION__,
        mStreamAttributeTarget.num_channels,
        mStreamAttributeTarget.sample_rate,
        mStreamAttributeTargetEchoRef.num_channels,
        mStreamAttributeTargetEchoRef.sample_rate);

    return NO_ERROR;
}


status_t AudioALSAPlaybackHandlerBTCVSD::close() {
    ALOGD("+%s()", __FUNCTION__);

#ifndef MTK_SUPPORT_BTCVSD_ALSA // clean bt buffer before closing, alsa version will clean in kernel close
    const uint32_t mute_buf_len = BUFFER_SIZE_PER_ACCESSS;
    char mute_buf[mute_buf_len];
    memset(mute_buf, 0, mute_buf_len);

    this->write(mute_buf, mute_buf_len);
    this->write(mute_buf, mute_buf_len);
    this->write(mute_buf, mute_buf_len);
    this->write(mute_buf, mute_buf_len);
#endif

#ifdef MTK_SUPPORT_BTCVSD_ALSA
    // close pcm driver
    closePcmDriver();
#endif

    // close mAudioBTCVSDControl
    mAudioBTCVSDControl->BTCVSD_StandbyProcess(mFd2);

#ifdef MTK_AURISYS_FRAMEWORK_SUPPORT
    if (get_aurisys_on()) {
        DestroyAurisysLibManager();
    } else
#endif
    {
        // bit conversion
        deinitBitConverter();
    }
    //DC removal
    deinitDcRemoval();

    DeinitDataPending();

    // debug pcm dump
    ClosePCMDump();


    ALOGD("-%s()", __FUNCTION__);
    return NO_ERROR;
}


status_t AudioALSAPlaybackHandlerBTCVSD::routing(const audio_devices_t output_devices __unused) {
    return INVALID_OPERATION;
}

int AudioALSAPlaybackHandlerBTCVSD::pause() {
    return -ENODATA;
}

int AudioALSAPlaybackHandlerBTCVSD::resume() {
    return -ENODATA;
}

int AudioALSAPlaybackHandlerBTCVSD::flush() {
    return 0;
}

status_t AudioALSAPlaybackHandlerBTCVSD::setVolume(uint32_t vol __unused) {
    return INVALID_OPERATION;
}


int AudioALSAPlaybackHandlerBTCVSD::drain(audio_drain_type_t type __unused) {
    return 0;
}


ssize_t AudioALSAPlaybackHandlerBTCVSD::write(const void *buffer, size_t bytes) {
#ifdef MTK_SUPPORT_BTCVSD_ALSA
    if (mPcm == NULL) {
        ALOGE("%s(), mPcm == NULL, return", __FUNCTION__);
        return bytes;
    }
#endif
    ALOGV("%s(), buffer = %p, bytes = %zu", __FUNCTION__, buffer, bytes);

    if (mPCMDumpFile) {
        clock_gettime(CLOCK_REALTIME, &mNewtime);
        latencyTime[0] = calc_time_diff(mNewtime, mOldtime);
        mOldtime = mNewtime;
    }

    // const -> to non const
    void *pBuffer = const_cast<void *>(buffer);
    ASSERT(pBuffer != NULL);

    void *pBufferAfterDataPending = NULL;
    uint32_t bytesAfterDataPending = 0;
    if (mDataPendingForceUse) {
        dodataPending(pBuffer, bytes, &pBufferAfterDataPending, &bytesAfterDataPending);

        if (bytesAfterDataPending < mDataAlignedSize) {
            ALOGV("%s(), bytesAfterDataPending %u, return", __FUNCTION__, bytesAfterDataPending);
            return bytes;
        }
    } else {
        pBufferAfterDataPending = pBuffer;
        bytesAfterDataPending = bytes;
    }

    void *pBufferAfterDcRemoval = NULL;
    uint32_t bytesAfterDcRemoval = 0;
    // DC removal before DRC
    doDcRemoval(pBufferAfterDataPending, bytesAfterDataPending, &pBufferAfterDcRemoval, &bytesAfterDcRemoval);

    // bit conversion
    void *pBufferAfterBitConvertion = NULL;
    uint32_t bytesAfterBitConvertion = 0;

    uint32_t buf_sample_rate = 0;
    uint32_t buf_num_channels = 0;

#ifdef MTK_AURISYS_FRAMEWORK_SUPPORT
    if (get_aurisys_on()) {
        buf_sample_rate = mStreamAttributeTarget.sample_rate;
        buf_num_channels = mStreamAttributeTarget.num_channels;

        audio_ringbuf_copy_from_linear(&mAudioPoolBufDlIn->ringbuf, (char *)pBufferAfterDcRemoval, bytesAfterDcRemoval);

        // post processing + SRC + Bit conversion
        aurisys_process_dl_only(mAurisysLibManager, mAudioPoolBufDlIn, mAudioPoolBufDlOut);

        uint32_t data_size = audio_ringbuf_count(&mAudioPoolBufDlOut->ringbuf);
        audio_ringbuf_copy_to_linear(mLinearOut, &mAudioPoolBufDlOut->ringbuf, data_size);
        //ALOGD("aurisys process data_size: %u", data_size);

        // wrap to original playback handler
        pBufferAfterBitConvertion = (void *)mLinearOut;
        bytesAfterBitConvertion = data_size;
    } else
#endif
    {
        buf_sample_rate = mStreamAttributeSource->sample_rate;
        buf_num_channels = mStreamAttributeSource->num_channels;

        doBitConversion(pBufferAfterDcRemoval, bytesAfterDcRemoval, &pBufferAfterBitConvertion, &bytesAfterBitConvertion);
    }

    // EchoRef
    uint32_t echoRefDataSize = bytesAfterBitConvertion;
    const char *pEchoRefBuffer = (const char *)pBufferAfterBitConvertion;

    // write data to bt cvsd driver
    uint8_t *outbuffer, *inbuf, *workbuf;
    uint32_t insize, outsize, workbufsize, total_outsize, src_fs_s;

    inbuf = (uint8_t *)pBufferAfterBitConvertion;

    if (mPCMDumpFile) {
        clock_gettime(CLOCK_REALTIME, &mNewtime);
        latencyTime[1] = calc_time_diff(mNewtime, mOldtime);
        mOldtime = mNewtime;
    }

    do {
        outbuffer = mAudioBTCVSDControl->BT_SCO_TX_GetCVSDOutBuf();
        outsize = SCO_TX_ENCODE_SIZE;
        insize = bytesAfterBitConvertion;
        workbuf = mAudioBTCVSDControl->BT_SCO_TX_GetCVSDWorkBuf();
        workbufsize = SCO_TX_PCM64K_BUF_SIZE;
        total_outsize = 0;
        do {
            if (mBTMode_Open != mAudioBTCVSDControl->BT_SCO_isWideBand()) {
                ALOGD("BTSCO change mode after TX_Begin!!!");
                mAudioBTCVSDControl->BT_SCO_TX_End(mFd2);
                mAudioBTCVSDControl->BT_SCO_TX_Begin(mFd2, buf_sample_rate, buf_num_channels);
                mBTMode_Open = mAudioBTCVSDControl->BT_SCO_isWideBand();
                if (mixer_ctl_set_enum_by_string(mixer_get_ctl_by_name(mMixer, "btcvsd_band"), mBTMode_Open ? "WB" : "NB")) {
                    ALOGE("Error: btcvsd_band invalid value");
                }
                return bytes;
            }

            if (mAudioBTCVSDControl->BT_SCO_isWideBand()) {
                mAudioBTCVSDControl->btsco_process_TX_MSBC(inbuf, &insize, outbuffer, &outsize, workbuf); // return insize is consumed size
                ALOGV("WriteDataToBTSCOHW, do mSBC encode outsize=%d, consumed size=%d, bytesAfterBitConvertion=%d", outsize, insize, bytesAfterBitConvertion);
            } else {
                mAudioBTCVSDControl->btsco_process_TX_CVSD(inbuf, &insize, outbuffer, &outsize, workbuf, workbufsize); // return insize is consumed size
                ALOGV("WriteDataToBTSCOHW, do CVSD encode outsize=%d, consumed size=%d, bytesAfterBitConvertion=%d", outsize, insize, bytesAfterBitConvertion);
            }
            outbuffer += outsize;
            inbuf += insize;

            ASSERT(bytesAfterBitConvertion >= insize);  // bytesAfterBitConvertion - insize >= 0
            bytesAfterBitConvertion -= insize;

            insize = bytesAfterBitConvertion;
            total_outsize += outsize;
        } while (total_outsize < BTSCO_CVSD_TX_OUTBUF_SIZE && outsize != 0);

        ALOGV("WriteDataToBTSCOHW write to kernel(+) total_outsize = %d", total_outsize);
        if (total_outsize > 0) {
            WritePcmDumpData((void *)(mAudioBTCVSDControl->BT_SCO_TX_GetCVSDOutBuf()), total_outsize);
#ifdef MTK_SUPPORT_BTCVSD_ALSA

            // check if timeout during write bt data
            bool skip_write = false;

            struct mixer_ctl *ctl = mixer_get_ctl_by_name(mMixer, "btcvsd_tx_timeout");
            int index = mixer_ctl_get_value(ctl, 0);
            bool tx_timeout = index != 0;
            ALOGV("%s(), btcvsd_tx_timeout, tx_timeout %d, index %d", __FUNCTION__, tx_timeout, index);
            if (tx_timeout) {
                // write timeout
                struct timespec timeStamp;
                unsigned int avail;
                if (pcm_get_htimestamp(mPcm, &avail, &timeStamp) != 0) {
                    ALOGV("%s(), pcm_get_htimestamp fail %s\n", __FUNCTION__, pcm_get_error(mPcm));
                } else {
                    if ((total_outsize / (mConfig.channels * pcm_format_to_bits(mConfig.format) / 8)) > avail) {
                        skip_write = true;
                        ALOGW("%s(), tx_timeout %d, total_out_size %u, avail %u, skip write", __FUNCTION__, tx_timeout, total_outsize, avail);
                        return 0;
                    }
                }
            }

            if (!skip_write) {
                int retval = pcm_write(mPcm, mAudioBTCVSDControl->BT_SCO_TX_GetCVSDOutBuf(), total_outsize);
                if (retval != 0) {
                    ALOGE("%s(), pcm_write() error, retval = %d", __FUNCTION__, retval);
                }
            }
#else
            ssize_t WrittenBytes = ::write(mFd2, mAudioBTCVSDControl->BT_SCO_TX_GetCVSDOutBuf(), total_outsize);
#endif
            updateStartTimeStamp();

        }

        ALOGV("WriteDataToBTSCOHW write to kernel(-) remaining bytes = %d", bytesAfterBitConvertion);
    } while (bytesAfterBitConvertion > 0);

    if (mPCMDumpFile) {
        clock_gettime(CLOCK_REALTIME, &mNewtime);
        latencyTime[2] = calc_time_diff(mNewtime, mOldtime);
        mOldtime = mNewtime;
    }

    /* Write echo ref data to data provider if needed */
    writeEchoRefDataToDataProvider(mDataProviderEchoRefBTCVSD, pEchoRefBuffer, echoRefDataSize);

    if (mPCMDumpFile) {
        clock_gettime(CLOCK_REALTIME, &mNewtime);
        latencyTime[3] = calc_time_diff(mNewtime, mOldtime);
        mOldtime = mNewtime;

        if (latencyTime[3] > 0.022) {
            ALOGD("latency_in_s,%1.3lf,%1.3lf,%1.3lf,%1.3lf, interrupt,%1.3lf", latencyTime[0], latencyTime[1], latencyTime[2], latencyTime[3], mStreamAttributeTarget.mInterrupt);
        }
    }

    return bytes;
}


status_t AudioALSAPlaybackHandlerBTCVSD::updateStartTimeStamp() {
    if (mEchoRefStartTime.tv_sec == 0 && mEchoRefStartTime.tv_nsec == 0) {
        TimeBufferInfo *pTimeInfoBuffer = NULL;

        /* Get tx timestamp */
#ifdef MTK_SUPPORT_BTCVSD_ALSA
        struct mixer_ctl *ctl;
        unsigned int num_values, i;
        int index = 0;
        TimeBufferInfo timeBufferInfo;

        /* get timestamp from driver*/
        ctl = mixer_get_ctl_by_name(mMixer, "btcvsd_tx_timestamp");
        int ret_val = mixer_ctl_get_array(ctl, &timeBufferInfo, sizeof(timeBufferInfo));
        if (ret_val < 0) {
            ALOGE("%s() mixer_ctl_get_array() failed (error %d)", __FUNCTION__, ret_val);
            pTimeInfoBuffer = NULL;
            return INVALID_OPERATION;
        } else {
            pTimeInfoBuffer = &timeBufferInfo;
        }
#else
        pTimeInfoBuffer = (TimeBufferInfo *)(mAudioBTCVSDControl->BT_SCO_TX_GetTimeBufferInfo());
#endif
        ASSERT(pTimeInfoBuffer != NULL);

        /* Convert TimeBufferInfo to timespec */
        unsigned long long timeStamp = pTimeInfoBuffer->timestampUS + pTimeInfoBuffer->dataCountEquiTime;
        mEchoRefStartTime.tv_sec = timeStamp / 1000000000;
        mEchoRefStartTime.tv_nsec = timeStamp % 1000000000;

        int delayMs = 0;
        const char *btDeviceName = AudioALSAStreamManager::getInstance()->GetBtHeadsetName();

#if (defined(MTK_AUDIO_HIERARCHICAL_PARAM_SUPPORT) && (MTK_AUDIO_TUNING_TOOL_V2_PHASE >= 2))
        /* Get the BT device delay parameter */
        AppOps *appOps = appOpsGetInstance();
        if (appOps == NULL) {
            ALOGE("%s(), Error: AppOps == NULL", __FUNCTION__);
            ASSERT(0);
            return INVALID_OPERATION;
        }

        AppHandle *pAppHandle = appOps->appHandleGetInstance();
        AudioType *audioType = appOps->appHandleGetAudioTypeByName(pAppHandle, "BtInfo");
        if (audioType) {
            String8 categoryPath("BT headset,");
            categoryPath += (btDeviceName ? btDeviceName : "");

            ParamUnit *paramUnit = appOps->audioTypeGetParamUnit(audioType, categoryPath.string());
            ASSERT(paramUnit);

            // isVoIP : true(VoIP) or false(3GVT)
            Param *param = appOps->paramUnitGetParamByName(paramUnit, "voip_ap_delay_ms");
            ASSERT(param);

            delayMs = *(int *)param->data;
        } else {
            ALOGW("%s(), No BtInfo audio type found!", __FUNCTION__);
        }
#endif

        struct timespec origStartTime = mEchoRefStartTime;
        adjustTimeStamp(&mEchoRefStartTime, delayMs);

        ALOGD("%s(), Set start timestamp (%ld.%09ld->%ld.%09ld), mTotalEchoRefBufSize = %d, BT headset = %s, delayMs = %d (audio_mode = %d), dataCountEquiTime=%" PRIu64 ", timestampUS=%" PRIu64 "",
              __FUNCTION__,
              origStartTime.tv_sec,
              origStartTime.tv_nsec,
              mEchoRefStartTime.tv_sec,
              mEchoRefStartTime.tv_nsec,
              mTotalEchoRefBufSize,
              btDeviceName,
              delayMs,
              mStreamAttributeSource->audio_mode,
              pTimeInfoBuffer->dataCountEquiTime,
              pTimeInfoBuffer->timestampUS);
    } else {
        ALOGV("%s(), start timestamp (%ld.%09ld), mTotalEchoRefBufSize = %d", __FUNCTION__, mEchoRefStartTime.tv_sec, mEchoRefStartTime.tv_nsec, mTotalEchoRefBufSize);
    }

    return NO_ERROR;
}

bool AudioALSAPlaybackHandlerBTCVSD::writeEchoRefDataToDataProvider(AudioALSACaptureDataProviderEchoRefBTCVSD *dataProvider, const char *echoRefData, uint32_t dataSize) {
    if (dataProvider->isEnable()) {
        /* Calculate buffer's time stamp */
        struct timespec newTimeStamp;
        calculateTimeStampByBytes(mEchoRefStartTime, mTotalEchoRefBufSize, mStreamAttributeTargetEchoRef, &newTimeStamp);

        SHOW_TIMESTAMP("%s(), mTotalEchoRefBufSize = %d, write size = %d, newTimeStamp = %ld.%09ld -> %ld.%09ld",
                       __FUNCTION__, mTotalEchoRefBufSize, dataSize, mEchoRefStartTime.tv_sec, mEchoRefStartTime.tv_nsec,
                       newTimeStamp.tv_sec, newTimeStamp.tv_nsec);

        // TODO(JH): Consider the close case, need to free EchoRef data from provider
        dataProvider->writeData(echoRefData, dataSize, &newTimeStamp);

        //WritePcmDumpData(echoRefData, dataSize);
    } else {
        SHOW_TIMESTAMP("%s(), data provider is not enabled, Do not write echo ref data to provider", __FUNCTION__);
    }
    mTotalEchoRefBufSize += dataSize;

    return true;
}

} // end of namespace android
