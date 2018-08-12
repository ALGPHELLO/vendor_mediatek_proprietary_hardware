#include "AudioALSAPlaybackHandlerSpeakerProtectionDsp.h"

#include "AudioALSAHardwareResourceManager.h"
#include "AudioVolumeFactory.h"
#include "AudioALSASampleRateController.h"

#include "AudioMTKFilter.h"
#include "AudioVUnlockDL.h"
#include "AudioALSADeviceParser.h"
#include "AudioALSADriverUtility.h"
#include "AudioMessengerIPI.h"
#include "AudioSmartPaParam.h"

#undef MTK_HDMI_SUPPORT

#if defined(MTK_HDMI_SUPPORT)
#include "AudioExtDisp.h"
#endif

#include "AudioSmartPaController.h"

#include "AudioALSAStreamManager.h"


#ifdef MTK_AURISYS_FRAMEWORK_SUPPORT
#include <audio_ringbuf.h>
#include <audio_pool_buf_handler.h>
#include <audio_task.h>
#include <aurisys_controller.h>
#include <aurisys_lib_manager.h>
#endif


#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "AudioALSAPlaybackHandlerSpeakerProtectionDsp"


// Latency Detect
//#define DEBUG_LATENCY
#ifdef DEBUG_LATENCY
#define THRESHOLD_FRAMEWORK   0.010
#define THRESHOLD_HAL         0.010
#define THRESHOLD_KERNEL      0.010
#endif


#define calc_time_diff(x,y) ((x.tv_sec - y.tv_sec )+ (double)( x.tv_nsec - y.tv_nsec ) / (double)1000000000)
static  const char PROPERTY_KEY_BYPASSPROTECTION[PROPERTY_KEY_MAX]  = "speakerprotection_bypass";

namespace android {

AudioALSAPlaybackHandlerSpeakerProtectionDsp::AudioALSAPlaybackHandlerSpeakerProtectionDsp(const stream_attribute_t *stream_attribute_source) :
    AudioALSAPlaybackHandlerBase(stream_attribute_source) {
    ALOGD("%s()", __FUNCTION__);
    memset((void *)&mNewtime, 0, sizeof(mNewtime));
    memset((void *)&mOldtime, 0, sizeof(mOldtime));
    mPlaybackHandlerType = PLAYBACK_HANDLER_SPEAKERPROTECTION;
}

AudioALSAPlaybackHandlerSpeakerProtectionDsp::~AudioALSAPlaybackHandlerSpeakerProtectionDsp() {
    ALOGD("%s()", __FUNCTION__);
}

int AudioALSAPlaybackHandlerSpeakerProtectionDsp::setPcmDump(bool enable) {
    ALOGD("%s() enable = %d", __FUNCTION__, enable);

    char value[PROPERTY_VALUE_MAX];
    int ret;

    enum mixer_ctl_type type;
    struct mixer_ctl *ctl;
    int retval = 0;

    property_get(streamout_propty, value, "0");
    int flag = atoi(value);

    if (flag == 0) {
        ALOGD("%s() %s property not set no dump", __FUNCTION__, streamout_propty);
        return 0;
    }

    ctl = mixer_get_ctl_by_name(mMixer, "Audio_spk_pcm_dump");

    if (ctl == NULL) {
        ALOGE("Audio_spk_pcm_dump not support");
        return -1;
    }

    if (enable == true) {
        retval = mixer_ctl_set_enum_by_string(ctl, "On");
        ALOGD("%s(), On enable = %d", __FUNCTION__, enable);
        ASSERT(retval == 0);
    } else {
        retval = mixer_ctl_set_enum_by_string(ctl, "Off");
        ALOGD("%s(), Off enable = %d", __FUNCTION__, enable);
        ASSERT(retval == 0);
    }

    return 0;
}

uint32_t AudioALSAPlaybackHandlerSpeakerProtectionDsp::ChooseTargetSampleRate(uint32_t SampleRate, audio_devices_t outputdevice) {
    ALOGD("ChooseTargetSampleRate SampleRate in = %d outputdevice = %d", SampleRate, outputdevice);
    uint32_t TargetSampleRate = 48000;

    if (AudioSmartPaController::getInstance()->isSmartPAUsed()) {
        ALOGD("%s(): find out speaker support rate", __FUNCTION__);
        bool isRateSupport = AudioSmartPaController::getInstance()->isRateSupported(SampleRate);
        if (isRateSupport == true) {
            TargetSampleRate =  SampleRate;
        } else {
            TargetSampleRate = AudioSmartPaController::getInstance()->getMaxSupportedRate();
        }
    } else {
        if (SampleRate >= 48000) {
            TargetSampleRate = 48000;
        } else if (SampleRate < 48000 && SampleRate >= 44100) {
            TargetSampleRate = 44100;
        } else if (SampleRate == 16000) {
            TargetSampleRate = 16000;
        }
    }

    return TargetSampleRate;
}

status_t AudioALSAPlaybackHandlerSpeakerProtectionDsp::open() {
    ALOGD("+%s(), mDevice = 0x%x", __FUNCTION__, mStreamAttributeSource->output_devices);
    AL_AUTOLOCK(*AudioALSADriverUtility::getInstance()->getStreamSramDramLock());

    // debug pcm dump
    OpenPCMDump(LOG_TAG);
    // acquire pmic clk
    mHardwareResourceManager->EnableAudBufClk(true);
    int pcmindex = AudioALSADeviceParser::getInstance()->GetPcmIndexByString(keypcmDl1SpkPlayback);
    int cardindex = AudioALSADeviceParser::getInstance()->GetCardIndexByString(keypcmDl1SpkPlayback);

    ALOGD("%s(): pcmindex = %d", __FUNCTION__, pcmindex);
    //ListPcmDriver(cardindex, pcmindex);

    mStreamAttributeTarget.buffer_size =  AudioALSADeviceParser::getInstance()->GetPcmBufferSize(pcmindex, PCM_OUT);

#ifdef PLAYBACK_USE_24BITS_ONLY
    mStreamAttributeTarget.audio_format = AUDIO_FORMAT_PCM_8_24_BIT;
#else
    mStreamAttributeTarget.audio_format = (mStreamAttributeSource->audio_format == AUDIO_FORMAT_PCM_32_BIT) ? AUDIO_FORMAT_PCM_8_24_BIT : AUDIO_FORMAT_PCM_16_BIT;
#endif
    mStreamAttributeTarget.audio_channel_mask = AUDIO_CHANNEL_IN_STEREO;
    mStreamAttributeTarget.num_channels = popcount(mStreamAttributeTarget.audio_channel_mask);

    mStreamAttributeTarget.sample_rate = ChooseTargetSampleRate(AudioALSASampleRateController::getInstance()->getPrimaryStreamOutSampleRate(),
                                                                mStreamAttributeSource->output_devices);

    ALOGD("mStreamAttributeTarget.sample_rate = %d", mStreamAttributeTarget.sample_rate);

    // HW pcm config
    mConfig.channels = mStreamAttributeTarget.num_channels;
    mConfig.rate = mStreamAttributeTarget.sample_rate;

    if (mStreamAttributeTarget.sample_rate > 16000) {

        mConfig.period_count = 4;
        mConfig.period_size = (mStreamAttributeTarget.buffer_size / (mConfig.channels * mConfig.period_count)) / ((mStreamAttributeTarget.audio_format == AUDIO_FORMAT_PCM_16_BIT) ? 2 : 4);

        mConfig.format = transferAudioFormatToPcmFormat(mStreamAttributeTarget.audio_format);

        mConfig.start_threshold = 0;
        mConfig.stop_threshold = 0;
        mConfig.silence_threshold = 0;
        ALOGD("%s(), mConfig: channels = %d, rate = %d, period_size = %d, period_count = %d, format = %d",
              __FUNCTION__, mConfig.channels, mConfig.rate, mConfig.period_size, mConfig.period_count, mConfig.format);
    } else { // voice playback , need low latency.
        mConfig.period_count = 12;
        mConfig.period_size = 160;

        mConfig.format = transferAudioFormatToPcmFormat(mStreamAttributeTarget.audio_format);

        mConfig.start_threshold = mConfig.period_size * 3;
        mConfig.stop_threshold = 0;
        mConfig.silence_threshold = 0;
        ALOGD("%s(), mConfig: channels = %d, rate = %d, period_size = %d, period_count = %d, format = %d",
              __FUNCTION__, mConfig.channels, mConfig.rate, mConfig.period_size, mConfig.period_count, mConfig.format);
    }

#ifdef MTK_AURISYS_FRAMEWORK_SUPPORT
    if (get_aurisys_on()) {
        CreateAurisysLibManager();
    } else
#endif
    {
        // post processing
        initPostProcessing();

        // SRCS_ESTSAMPLERATE
        initBliSrc();

        // bit conversion
        initBitConverter();

        initDataPending();
    }
    /* smartpa param*/
    initSmartPaConfig();

    setPcmDump(true);
    // init DC Removal
    initDcRemoval();

    // open pcm driver
    openPcmDriver(pcmindex);

    // open codec driver
    if (mStreamAttributeSource->output_devices != AUDIO_DEVICE_OUT_SPEAKER_SAFE) {
        mHardwareResourceManager->startOutputDevice(mStreamAttributeSource->output_devices, mStreamAttributeTarget.sample_rate);
    }


    //============Voice UI&Unlock REFERECE=============
    AudioVUnlockDL *VUnlockhdl = AudioVUnlockDL::getInstance();
    if (VUnlockhdl != NULL) {
        VUnlockhdl->SetInputStandBy(false);
        VUnlockhdl->GetSRCInputParameter(mStreamAttributeTarget.sample_rate, mStreamAttributeTarget.num_channels, mStreamAttributeSource->audio_format);
        VUnlockhdl->GetFirstDLTime();
    }
    mTimeStampValid = false;
    mBytesWriteKernel = 0;


    ALOGD("-%s()", __FUNCTION__);
    return NO_ERROR;
}

int AudioALSAPlaybackHandlerSpeakerProtectionDsp::initSmartPaConfig() {
    ALOGD("%s", __FUNCTION__);
    AudioSmartPaParam *mAudioSmartPainstance = AudioSmartPaParam::getInstance();

    if (mAudioSmartPainstance == NULL) {
        return -1;
    }

    arsi_lib_config_t arsiLibConfig;
    arsiLibConfig.p_ul_buf_in = NULL;
    arsiLibConfig.p_ul_buf_out = NULL;
    arsiLibConfig.p_ul_ref_bufs = NULL;

    arsiLibConfig.p_dl_buf_in = NULL;
    arsiLibConfig.p_dl_buf_out = NULL;
    arsiLibConfig.p_dl_ref_bufs = NULL;

    /* lib */
    arsiLibConfig.sample_rate = mStreamAttributeTarget.sample_rate;
    arsiLibConfig.audio_format = AUDIO_FORMAT_PCM_32_BIT;
    arsiLibConfig.frame_size_ms = 0;

    /* output device */
    arsi_task_config_t ArsiTaskConfig;
    ArsiTaskConfig.output_device_info.devices = mStreamAttributeTarget.output_devices;
    ArsiTaskConfig.output_device_info.audio_format = AUDIO_FORMAT_PCM_32_BIT;
    ArsiTaskConfig.output_device_info.sample_rate = mStreamAttributeTarget.sample_rate;
    ArsiTaskConfig.output_device_info.channel_mask = AUDIO_CHANNEL_IN_STEREO;
    ArsiTaskConfig.output_device_info.num_channels = 2;
    ArsiTaskConfig.output_device_info.hw_info_mask = 0;

    /* task scene */
    ArsiTaskConfig.task_scene = TASK_SCENE_SPEAKER_PROTECTION;

    /* audio mode */
    ArsiTaskConfig.audio_mode = AudioALSAStreamManager::getInstance()->isModeInRingtone();

    /* max device capability for allocating memory */
    ArsiTaskConfig.max_output_device_sample_rate = mStreamAttributeTarget.sample_rate;
    ArsiTaskConfig.max_output_device_num_channels = 2;

    mAudioSmartPainstance->SetSmartpaParam(ArsiTaskConfig.audio_mode);

    return 0;
}
status_t AudioALSAPlaybackHandlerSpeakerProtectionDsp::close() {
    ALOGD("+%s()", __FUNCTION__);
    AL_AUTOLOCK(*AudioALSADriverUtility::getInstance()->getStreamSramDramLock());

    setPcmDump(false);
    //============Voice UI&Unlock REFERECE=============
    AudioVUnlockDL *VUnlockhdl = AudioVUnlockDL::getInstance();
    if (VUnlockhdl != NULL) {
        VUnlockhdl->SetInputStandBy(true);
    }
    //===========================================

    if (mStreamAttributeSource->output_devices != AUDIO_DEVICE_OUT_SPEAKER_SAFE) {
        mHardwareResourceManager->stopOutputDevice();
    }

    // close pcm driver
    closePcmDriver();

    //DC removal
    deinitDcRemoval();

#ifdef MTK_AURISYS_FRAMEWORK_SUPPORT
    if (get_aurisys_on()) {
        DestroyAurisysLibManager();
    } else
#endif
    {
        DeinitDataPending();

        // bit conversion
        deinitBitConverter();

        // SRC
        deinitBliSrc();

        // post processing
        deinitPostProcessing();
    }


    // debug pcm dump
    ClosePCMDump();

    //release pmic clk
    mHardwareResourceManager->EnableAudBufClk(false);

    ALOGD("-%s()", __FUNCTION__);
    return NO_ERROR;
}


status_t AudioALSAPlaybackHandlerSpeakerProtectionDsp::routing(const audio_devices_t output_devices) {
    mHardwareResourceManager->changeOutputDevice(output_devices);
    if (mAudioFilterManagerHandler) { mAudioFilterManagerHandler->setDevice(output_devices); }
    return NO_ERROR;
}

int AudioALSAPlaybackHandlerSpeakerProtectionDsp::pause() {
    return -ENODATA;
}

int AudioALSAPlaybackHandlerSpeakerProtectionDsp::resume() {
    return -ENODATA;
}

int AudioALSAPlaybackHandlerSpeakerProtectionDsp::flush() {
    return 0;
}

status_t AudioALSAPlaybackHandlerSpeakerProtectionDsp::setVolume(uint32_t vol __unused) {
    return INVALID_OPERATION;
}

int AudioALSAPlaybackHandlerSpeakerProtectionDsp::drain(audio_drain_type_t type __unused) {
    return 0;
}

status_t AudioALSAPlaybackHandlerSpeakerProtectionDsp::setScreenState(bool mode, size_t buffer_size, size_t reduceInterruptSize, bool bforce __unused) {
    // don't increase irq period when play hifi
    if (mode == 0 && mStreamAttributeSource->sample_rate > 48000) {
        return NO_ERROR;
    }

    if (0 == buffer_size) {
        buffer_size = mStreamAttributeSource->buffer_size;
    }

    int rate = mode ? (buffer_size / mStreamAttributeSource->num_channels) / ((mStreamAttributeSource->audio_format == AUDIO_FORMAT_PCM_16_BIT) ? 2 : 4) :
                      ((mStreamAttributeTarget.buffer_size - reduceInterruptSize) / mConfig.channels) / ((mStreamAttributeTarget.audio_format == AUDIO_FORMAT_PCM_16_BIT) ? 2 : 4);

    mStreamAttributeTarget.mInterrupt = (rate + 0.0) / mStreamAttributeTarget.sample_rate;

    ALOGD("%s, rate %d %f, mode = %d , buffer_size = %zu, channel %d, format%d", __FUNCTION__, rate, mStreamAttributeTarget.mInterrupt, mode, buffer_size, mConfig.channels, mStreamAttributeTarget.audio_format);

    mHardwareResourceManager->setInterruptRate(mStreamAttributeSource->mAudioOutputFlags, rate);
    return NO_ERROR;
}

// here using android define format
unsigned int  AudioALSAPlaybackHandlerSpeakerProtectionDsp::GetSampleSize(unsigned int Format) {
    unsigned returnsize = 2;
    if (Format == AUDIO_FORMAT_PCM_16_BIT) {
        returnsize = 2;
    } else if (Format == AUDIO_FORMAT_PCM_32_BIT || Format == AUDIO_FORMAT_PCM_8_24_BIT) {
        returnsize = 4;
    } else if (Format == AUDIO_FORMAT_PCM_8_BIT) {
        returnsize = 1;
    } else {
        ALOGD("%s Format == %d", __FUNCTION__, Format);
    }
    return returnsize;
}

// here using android define format
unsigned int  AudioALSAPlaybackHandlerSpeakerProtectionDsp::GetFrameSize(unsigned int channels, unsigned int Format) {
    unsigned returnsize = 2;
    if (Format == AUDIO_FORMAT_PCM_16_BIT) {
        returnsize = 2;
    } else if (Format == AUDIO_FORMAT_PCM_32_BIT || Format == AUDIO_FORMAT_PCM_8_24_BIT) {
        returnsize = 4;
    } else if (Format == AUDIO_FORMAT_PCM_8_BIT) {
        returnsize = 1;
    } else {
        ALOGD("%s Format = %d", __FUNCTION__, Format);
    }
    returnsize *= channels;
    return returnsize;;
}

ssize_t AudioALSAPlaybackHandlerSpeakerProtectionDsp::write(const void *buffer, size_t bytes) {
    //ALOGD("%s(), buffer = %p, bytes = %d", __FUNCTION__, buffer, bytes);

    if (mPcm == NULL) {
        ALOGE("%s(), mPcm == NULL, return", __FUNCTION__);
        return bytes;
    }

    // const -> to non const
    void *pBuffer = const_cast<void *>(buffer);
    ASSERT(pBuffer != NULL);

#ifdef DEBUG_LATENCY
    clock_gettime(CLOCK_REALTIME, &mNewtime);
    latencyTime[0] = calc_time_diff(mNewtime, mOldtime);
    mOldtime = mNewtime;
#endif

    void *pBufferAfterDcRemoval = NULL;
    uint32_t bytesAfterDcRemoval = 0;
    // DC removal before DRC
    doDcRemoval(pBuffer, bytes, &pBufferAfterDcRemoval, &bytesAfterDcRemoval);

    doStereoToMonoConversionIfNeed(pBufferAfterDcRemoval, bytesAfterDcRemoval);

    void *pBufferAfterPending = NULL;
    uint32_t bytesAfterpending = 0;

#ifdef MTK_AURISYS_FRAMEWORK_SUPPORT
    if (get_aurisys_on()) {
        audio_ringbuf_copy_from_linear(&mAudioPoolBufDlIn->ringbuf, (char *)pBufferAfterDcRemoval, bytesAfterDcRemoval);

        // post processing + SRC + Bit conversion
        aurisys_process_dl_only(mAurisysLibManager, mAudioPoolBufDlIn, mAudioPoolBufDlOut);

        // data pending: sram is device memory, need word size align 64 byte for 64 bit platform
        uint32_t data_size = audio_ringbuf_count(&mAudioPoolBufDlOut->ringbuf);
        if (data_size > mTransferredBufferSize) {
            data_size = mTransferredBufferSize;
        }
        data_size &= 0xFFFFFFC0;
        audio_ringbuf_copy_to_linear(mLinearOut, &mAudioPoolBufDlOut->ringbuf, data_size);
        //ALOGD("aurisys process data_size: %u", data_size);

        // wrap to original playback handler
        pBufferAfterPending = (void *)mLinearOut;
        bytesAfterpending = data_size;
    } else
#endif
    {
        // post processing (can handle both Q1P16 and Q1P31 by audio_format_t)
        void *pBufferAfterPostProcessing = NULL;
        uint32_t bytesAfterPostProcessing = 0;
        status_t ret = doPostProcessing(pBufferAfterDcRemoval, bytesAfterDcRemoval, &pBufferAfterPostProcessing, &bytesAfterPostProcessing);
        if (ret != NO_ERROR) {
            ALOGW("%s(), No processed data output, don't write data to PCM(input bytes = %zu)", __FUNCTION__, bytes);
            return bytes;
        }

        // SRC
        void *pBufferAfterBliSrc = NULL;
        uint32_t bytesAfterBliSrc = 0;
        doBliSrc(pBufferAfterPostProcessing, bytesAfterPostProcessing, &pBufferAfterBliSrc, &bytesAfterBliSrc);

        // bit conversion
        void *pBufferAfterBitConvertion = NULL;
        uint32_t bytesAfterBitConvertion = 0;
        doBitConversion(pBufferAfterBliSrc, bytesAfterBliSrc, &pBufferAfterBitConvertion, &bytesAfterBitConvertion);

        // data pending
        pBufferAfterPending = NULL;
        bytesAfterpending = 0;
        dodataPending(pBufferAfterBitConvertion, bytesAfterBitConvertion, &pBufferAfterPending, &bytesAfterpending);
    }


#ifdef DEBUG_LATENCY
    clock_gettime(CLOCK_REALTIME, &mNewtime);
    latencyTime[1] = calc_time_diff(mNewtime, mOldtime);
    mOldtime = mNewtime;
#endif

    // write data to pcm driver
    int retval = pcm_write(mPcm, pBufferAfterPending, bytesAfterpending);
    mBytesWriteKernel = mBytesWriteKernel + bytesAfterpending;
    if (mTimeStampValid == false) {
        if (mBytesWriteKernel >= (mStreamAttributeTarget.buffer_size >> 1)) {
            mTimeStampValid = true;
        }
    }

    updateHardwareBufferInfo(bytes, bytesAfterpending);

#ifdef DEBUG_LATENCY
    clock_gettime(CLOCK_REALTIME, &mNewtime);
    latencyTime[2] = calc_time_diff(mNewtime, mOldtime);
    mOldtime = mNewtime;
#endif

#if 1 // TODO(Harvey, Wendy), temporary disable Voice Unlock until 24bit ready
    //============Voice UI&Unlock REFERECE=============
    AudioVUnlockDL *VUnlockhdl = AudioVUnlockDL::getInstance();
    if (VUnlockhdl != NULL) {
        // get remain time
        //VUnlockhdl->SetDownlinkStartTime(ret_ms);
        VUnlockhdl->GetFirstDLTime();

        //VUnlockhdl->SetInputStandBy(false);
        if (mStreamAttributeSource->output_devices & AUDIO_DEVICE_OUT_WIRED_HEADSET ||
            mStreamAttributeSource->output_devices & AUDIO_DEVICE_OUT_WIRED_HEADPHONE) {
            memset((void *)pBufferAfterPending, 0, bytesAfterpending);
        }
        VUnlockhdl->WriteStreamOutToRing(pBufferAfterPending, bytesAfterpending);
    }
    //===========================================
#endif

    if (retval != 0) {
        ALOGE("%s(), pcm_write() error, retval = %d", __FUNCTION__, retval);
    }

#ifdef DEBUG_LATENCY
    ALOGD("%s ::write (-) latency_in_us,%1.6lf,%1.6lf,%1.6lf", __FUNCTION__, latencyTime[0], latencyTime[1], latencyTime[2]);
#endif

    return bytes;
}


status_t AudioALSAPlaybackHandlerSpeakerProtectionDsp::setFilterMng(AudioMTKFilterManager *pFilterMng) {
    ALOGD("+%s() mAudioFilterManagerHandler [0x%p]", __FUNCTION__, pFilterMng);
    mAudioFilterManagerHandler = pFilterMng;
    ALOGD("-%s()", __FUNCTION__);
    return NO_ERROR;
}

} // end of namespace android
