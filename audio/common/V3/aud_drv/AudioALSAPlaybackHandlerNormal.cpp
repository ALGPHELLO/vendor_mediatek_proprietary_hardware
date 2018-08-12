#include "AudioALSAPlaybackHandlerNormal.h"

#include "AudioALSAHardwareResourceManager.h"
//#include "AudioALSAVolumeController.h"
//#include "AudioVolumeInterface.h"
#include "AudioVolumeFactory.h"
#include "AudioALSASampleRateController.h"

#include "AudioMTKFilter.h"
#include "AudioVUnlockDL.h"
#include "AudioALSADeviceParser.h"
#include "AudioALSADriverUtility.h"
#if defined(MTK_SPEAKER_MONITOR_SUPPORT)
#include "AudioALSASpeakerMonitor.h"
#endif
#include "AudioSmartPaController.h"

#undef MTK_HDMI_SUPPORT

#if defined(MTK_HDMI_SUPPORT)
#include "AudioExtDisp.h"
#endif

#include "AudioALSAStreamManager.h"

#ifdef MTK_AURISYS_FRAMEWORK_SUPPORT
#include <audio_ringbuf.h>
#include <audio_pool_buf_handler.h>

#include <aurisys_controller.h>
#include <aurisys_lib_manager.h>
#endif

#ifdef MTK_LATENCY_DETECT_PULSE
#include "AudioDetectPulse.h"
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "AudioALSAPlaybackHandlerNormal"

//awinic add 
#ifdef awinic_mec
#include<dlfcn.h>
#include<string.h>
#include<cutils/properties.h>
#endif
//awinic add 


// define this to enable mmap playback.
//#define PLAYBACK_MMAP

#ifdef DEBUG_LATENCY
// Latency Detect
//#define DEBUG_LATENCY
#define THRESHOLD_FRAMEWORK   0.010
#define THRESHOLD_HAL         0.010
#define THRESHOLD_KERNEL      0.010
#endif

#ifndef KERNEL_BUFFER_SIZE_DL1_DATA2_NORMAL
#define KERNEL_BUFFER_SIZE_DL1_DATA2_NORMAL         KERNEL_BUFFER_SIZE_DL1_NORMAL
#endif

#ifndef KERNEL_BUFFER_SIZE_DL1_DATA2_HIFI_96K
#define KERNEL_BUFFER_SIZE_DL1_DATA2_HIFI_96K       KERNEL_BUFFER_SIZE_DL1_HIFI_96K
#endif

#ifndef KERNEL_BUFFER_SIZE_DL1_DATA2_HIFI_192K
#define KERNEL_BUFFER_SIZE_DL1_DATA2_HIFI_192K      KERNEL_BUFFER_SIZE_DL1_HIFI_192K
#endif


#define calc_time_diff(x,y) ((x.tv_sec - y.tv_sec )+ (double)( x.tv_nsec - y.tv_nsec ) / (double)1000000000)
static   const char PROPERTY_KEY_EXTDAC[PROPERTY_KEY_MAX]  = "af.resouce.extdac_support";

static const uint32_t kPcmDriverBufferSize = 0x20000; // 128k

namespace android {

AudioALSAPlaybackHandlerNormal::AudioALSAPlaybackHandlerNormal(const stream_attribute_t *stream_attribute_source) :
    AudioALSAPlaybackHandlerBase(stream_attribute_source),
    mHpImpeDancePcm(NULL),
    mForceMute(false),
    mCurMuteBytes(0),
    mStartMuteBytes(0),
    mAllZeroBlock(NULL) {
    ALOGD("%s()", __FUNCTION__);
    mPlaybackHandlerType = isIsolatedDeepBuffer(mStreamAttributeSource->mAudioOutputFlags) ?
                           PLAYBACK_HANDLER_DEEP_BUFFER : PLAYBACK_HANDLER_NORMAL;

    if (mStreamAttributeSource->mAudioOutputFlags & AUDIO_OUTPUT_FLAG_FAST) {
        mPlaybackHandlerType = PLAYBACK_HANDLER_FAST;
    }

    memset((void *)&mNewtime, 0, sizeof(mNewtime));
    memset((void *)&mOldtime, 0, sizeof(mOldtime));
    memset((void *)&mHpImpedanceConfig, 0, sizeof(mHpImpedanceConfig));

    if (!(platformIsolatedDeepBuffer()) ||
        (platformIsolatedDeepBuffer() &&
         mStreamAttributeSource->mAudioOutputFlags & AUDIO_OUTPUT_FLAG_DEEP_BUFFER)) {
        mSupportNLE = true;
    } else {
        mSupportNLE = false;
    }
//awinic add
#ifdef awinic_mec
    int ret;
    audio_buffer = (char*)calloc(64*1024,sizeof(char));
    void *awinic_lib= dlopen(AWINIC_LIB_PATH,RTLD_NOW);
    if(awinic_lib==NULL)
      ALOGE("%s:Awinic: dlopen lib failed\n",__func__);
    else
      ALOGD("%s:Awinic: dlopen success \n",__func__);

     Aw_Algo_Init= (AwAlgoInit) dlsym(awinic_lib,"AwinicAlgoInit");
     Aw_Algo_Deinit= (AwAlgoDeinit) dlsym(awinic_lib,"AwinicAlgoDeinit");
     Aw_Algo_Handle = (AwAudioHandle) dlsym(awinic_lib,"AwinicAudioHandle");
     Aw_Algo_Clear  = (AwAlgoClear) dlsym(awinic_lib,"AwinicAlgoClear");
     Aw_Get_Size    = (AwAlgoGetSize)dlsym(awinic_lib,"AwinicGetSize");
      if(Aw_Algo_Init == NULL || Aw_Algo_Deinit == NULL || \
         Aw_Algo_Handle == NULL || Aw_Algo_Clear == NULL|| Aw_Get_Size == NULL)
      {
          ALOGE("%s:Awinic Get function error \n",__func__);
          Aw_Ready = false;
          return;
      }
      Aw_Ready = true;
      ret = Aw_Get_Size();

      if(ret > 0)
        Aw_Cfg_buffer = (char*)calloc(ret,sizeof(char));
      else{
        Aw_Ready = false;
		ALOGE("%s:Awinic Get memory error \n",__func__);
        return;
      }
      if(Aw_Cfg_buffer == NULL)
      {
        Aw_Ready = false;
        return;
      }

      ret =Aw_Algo_Init(Aw_Cfg_buffer);
      if(ret < 0)
      {
            ALOGE("%s:Awinic Init Error \n",__func__);
            Aw_Ready = false;
            return;
      }

      Aw_Algo_Clear(Aw_Cfg_buffer);
#endif
//awinic add 
	
	
}


AudioALSAPlaybackHandlerNormal::~AudioALSAPlaybackHandlerNormal() {
//awinic add
#ifdef awinic_mec
   if(Aw_Ready == true)
        Aw_Algo_Deinit(Aw_Cfg_buffer);
    free(Aw_Cfg_buffer);
    Aw_Cfg_buffer = NULL;
    free(audio_buffer);
    audio_buffer = NULL;
#endif
//awinic add
}

uint32_t AudioALSAPlaybackHandlerNormal::GetLowJitterModeSampleRate() {
    return 48000;
}

bool AudioALSAPlaybackHandlerNormal::SetLowJitterMode(bool bEnable, uint32_t SampleRate) {
    enum mixer_ctl_type type;
    struct mixer_ctl *ctl;
    int retval = 0;

    // check need open low jitter mode
    if (SampleRate <= GetLowJitterModeSampleRate() && (AudioALSADriverUtility::getInstance()->GetPropertyValue(PROPERTY_KEY_EXTDAC)) == false) {
        ALOGD("%s(), bypass low jitter mode, bEnable = %d, device = 0x%x, SampleRate = %u",
              __FUNCTION__, bEnable, mStreamAttributeSource->output_devices, SampleRate);
        return false;
    } else {
        ALOGD("%s() bEanble = %d, SampleRate = %u, use %s",
              __FUNCTION__, bEnable, SampleRate,
              isIsolatedDeepBuffer(mStreamAttributeSource->mAudioOutputFlags)?
              "deep_buffer_dl_hd_Switch": "Audio_I2S0dl1_hd_Switch");
    }

    if (isIsolatedDeepBuffer(mStreamAttributeSource->mAudioOutputFlags)) {
        ctl = mixer_get_ctl_by_name(mMixer, "deep_buffer_dl_hd_Switch");
    } else {
        ctl = mixer_get_ctl_by_name(mMixer, "Audio_I2S0dl1_hd_Switch");
    }

    if (ctl == NULL) {
        ALOGE("mixer control not support");
        return false;
    }

    if (bEnable == true) {
        retval = mixer_ctl_set_enum_by_string(ctl, "On");
        ASSERT(retval == 0);
    } else {
        retval = mixer_ctl_set_enum_by_string(ctl, "Off");
        ASSERT(retval == 0);
    }
    return true;
}

bool AudioALSAPlaybackHandlerNormal::DeviceSupportHifi(audio_devices_t outputdevice) {
    // modify this to let output device support hifi audio
    if (outputdevice == AUDIO_DEVICE_OUT_WIRED_HEADSET || outputdevice == AUDIO_DEVICE_OUT_WIRED_HEADPHONE) {
        return true;
    } else if (outputdevice & AUDIO_DEVICE_OUT_SPEAKER) {
        if (AudioSmartPaController::getInstance()->isSmartPAUsed()) {
            return AudioSmartPaController::getInstance()->getMaxSupportedRate() > 48000;
        } else {
            return true;
        }
    }
    return false;
}


uint32_t AudioALSAPlaybackHandlerNormal::ChooseTargetSampleRate(uint32_t SampleRate, audio_devices_t outputdevice) {
    ALOGV("ChooseTargetSampleRate SampleRate = %d outputdevice = %d", SampleRate, outputdevice);
    uint32_t TargetSampleRate = 44100;
    if (SampleRate <=  192000 && SampleRate > 96000 && DeviceSupportHifi(outputdevice)) {
        TargetSampleRate = 192000;
    } else if (SampleRate <= 96000 && SampleRate > 48000 && DeviceSupportHifi(outputdevice)) {
        TargetSampleRate = 96000;
    } else if (SampleRate <= 48000 && SampleRate >= 32000) {
        TargetSampleRate = SampleRate;
    }
    return TargetSampleRate;
}
status_t SetMHLChipEnable(int enable __unused) {
#if 0
    ALOGD("+%s(), enable %d", __FUNCTION__, enable);
#if defined(MTK_HDMI_SUPPORT)
    // File descriptor
    int fd_audio = ::open(HDMI_DRV, O_RDWR);
    ALOGD("%s(), open(%s), fd_audio = %d", __FUNCTION__, HDMI_DRV, fd_audio);

    if (fd_audio >= 0) {
        ::ioctl(fd_audio, MTK_HDMI_AUDIO_ENABLE, enable);

        ALOGD("%s(), ioctl:MTK_HDMI_AUDIO_FORMAT =0x%x \n", __FUNCTION__, enable);

        ::close(fd_audio);
    }
    ALOGD("-%s(), fd_audio=%d", __FUNCTION__, fd_audio);
#endif
#endif

    return NO_ERROR;
}

status_t AudioALSAPlaybackHandlerNormal::open() {
    ALOGD("+%s(), flag %d, mDevice = 0x%x", __FUNCTION__, mStreamAttributeSource->mAudioOutputFlags,
          mStreamAttributeSource->output_devices);
    AL_LOCK_MS(AudioALSADriverUtility::getInstance()->getStreamSramDramLock(), 3000);
    int pcmindex = 0;
    int cardindex = 0;

    // debug pcm dump
    OpenPCMDump(LOG_TAG);
    // acquire pmic clk
    mHardwareResourceManager->EnableAudBufClk(true);

    String8 pcmPath = isIsolatedDeepBuffer(mStreamAttributeSource->mAudioOutputFlags) ?
                      keypcmDL1DATA2PLayback : keypcmI2S0Dl1Playback;
    pcmindex = AudioALSADeviceParser::getInstance()->GetPcmIndexByString(pcmPath);
    cardindex = AudioALSADeviceParser::getInstance()->GetCardIndexByString(pcmPath);

    //ListPcmDriver(cardindex, pcmindex);

    struct pcm_params *params;
    params = pcm_params_get(cardindex, pcmindex,  PCM_OUT);
    if (params == NULL) {
        ALOGD("Device does not exist.\n");
    }
    mStreamAttributeTarget.buffer_size = pcm_params_get_max(params, PCM_PARAM_BUFFER_BYTES);
    pcm_params_free(params);

    // HW attribute config // TODO(Harvey): query this
#ifdef PLAYBACK_USE_24BITS_ONLY
    mStreamAttributeTarget.audio_format = AUDIO_FORMAT_PCM_8_24_BIT;
#else
    mStreamAttributeTarget.audio_format = (mStreamAttributeSource->audio_format == AUDIO_FORMAT_PCM_32_BIT) ? AUDIO_FORMAT_PCM_8_24_BIT : AUDIO_FORMAT_PCM_16_BIT;
#endif

    mStreamAttributeTarget.audio_channel_mask = AUDIO_CHANNEL_IN_STEREO;
    mStreamAttributeTarget.num_channels = popcount(mStreamAttributeTarget.audio_channel_mask);

    mStreamAttributeTarget.sample_rate = ChooseTargetSampleRate(AudioALSASampleRateController::getInstance()->getPrimaryStreamOutSampleRate(),
                                                                mStreamAttributeSource->output_devices);
#ifdef HIFI_DEEP_BUFFER
    if (mStreamAttributeTarget.sample_rate <= 48000) {
        mStreamAttributeTarget.buffer_size = isIsolatedDeepBuffer(mStreamAttributeSource->mAudioOutputFlags) ?
                                             KERNEL_BUFFER_SIZE_DL1_DATA2_NORMAL :
                                             KERNEL_BUFFER_SIZE_DL1_NORMAL;
#if defined(MTK_HYBRID_NLE_SUPPORT)
#ifdef PLAYBACK_USE_24BITS_ONLY
#define KERNEL_BUFFER_SIZE_WITH_DRE  (40 * 1024) /* 40KB for 32bit hal */
#else
#define KERNEL_BUFFER_SIZE_WITH_DRE  (20 * 1024) /* 20KB for 16bit hal */
#endif
        if (mSupportNLE && (mStreamAttributeTarget.buffer_size < KERNEL_BUFFER_SIZE_WITH_DRE)) {
            mStreamAttributeTarget.buffer_size = KERNEL_BUFFER_SIZE_WITH_DRE;
        }
#endif
    } else if (mStreamAttributeTarget.sample_rate > 48000 && mStreamAttributeTarget.sample_rate <= 96000) {
        uint32_t hifi_buffer_size = isIsolatedDeepBuffer(mStreamAttributeSource->mAudioOutputFlags) ?
                                    KERNEL_BUFFER_SIZE_DL1_DATA2_HIFI_96K :
                                    KERNEL_BUFFER_SIZE_DL1_HIFI_96K;

        if (mStreamAttributeTarget.buffer_size >= hifi_buffer_size) {
            mStreamAttributeTarget.buffer_size = hifi_buffer_size;
        }
    } else {
        uint32_t hifi_buffer_size = isIsolatedDeepBuffer(mStreamAttributeSource->mAudioOutputFlags) ?
                                    KERNEL_BUFFER_SIZE_DL1_DATA2_HIFI_192K :
                                    KERNEL_BUFFER_SIZE_DL1_HIFI_192K;

        if (mStreamAttributeTarget.buffer_size >= hifi_buffer_size) {
            mStreamAttributeTarget.buffer_size = hifi_buffer_size;
        }
    }
#endif  /* end of #ifdef HIFI_DEEP_BUFFER */

    //Change hwbuffer size in Comminuication
    if (!(platformIsolatedDeepBuffer()) &&
        mStreamAttributeSource->audio_mode == AUDIO_MODE_IN_COMMUNICATION) {
        mStreamAttributeTarget.buffer_size = 2 * mStreamAttributeSource->buffer_size /
                                             ((mStreamAttributeSource->audio_format == AUDIO_FORMAT_PCM_16_BIT) ? 2 : 4) *
                                             ((mStreamAttributeTarget.audio_format == AUDIO_FORMAT_PCM_16_BIT) ? 2 : 4);
    }

    // HW pcm config
    memset(&mConfig, 0, sizeof(mConfig));
    mConfig.channels = mStreamAttributeTarget.num_channels;
    mConfig.rate = mStreamAttributeTarget.sample_rate;


    if (mStreamAttributeSource->mAudioOutputFlags & AUDIO_OUTPUT_FLAG_FAST) {
        // audio low latency param - playback - interrupt rate
        mConfig.period_count = 2;
        mConfig.period_size = (mStreamAttributeSource->buffer_size / mConfig.channels /
                               ((mStreamAttributeSource->audio_format == AUDIO_FORMAT_PCM_16_BIT) ? 2 : 4));
        mStreamAttributeTarget.buffer_size = mConfig.period_size * mConfig.period_count * mConfig.channels *
                                             ((mStreamAttributeTarget.audio_format == AUDIO_FORMAT_PCM_16_BIT) ? 2 : 4);

        // Soc_Aud_AFE_IO_Block_MEM_DL1 assign to use DRAM.
        AudioALSAHardwareResourceManager::getInstance()->AssignDRAM(0);
    } else {
        // Buffer size: 1536(period_size) * 2(ch) * 4(byte) * 2(period_count) = 24 kb
        mConfig.period_count = 2;
        mConfig.period_size = (mStreamAttributeTarget.buffer_size / (mConfig.channels * mConfig.period_count)) /
                              ((mStreamAttributeTarget.audio_format == AUDIO_FORMAT_PCM_16_BIT) ? 2 : 4);
    }

    mConfig.format = transferAudioFormatToPcmFormat(mStreamAttributeTarget.audio_format);

    mConfig.start_threshold = 0;
    mConfig.stop_threshold = 0;
    mConfig.silence_threshold = 0;
    mConfig.avail_min = mStreamAttributeSource->buffer_size / ((mStreamAttributeSource->audio_format == AUDIO_FORMAT_PCM_16_BIT) ? 2 : 4) / mStreamAttributeSource->num_channels;
    ALOGD("%s(), mConfig: channels = %d, rate = %d, period_size = %d, period_count = %d, format = %d avail_min = %d",
          __FUNCTION__, mConfig.channels, mConfig.rate, mConfig.period_size, mConfig.period_count, mConfig.format, mConfig.avail_min);

    // disable lowjitter mode
    SetLowJitterMode(true, mStreamAttributeTarget.sample_rate);

#if defined(PLAYBACK_MMAP) // must be after pcm open
    unsigned int flag = PCM_MMAP | PCM_OUT | PCM_MONOTONIC;
    openPcmDriverWithFlag(pcmindex, flag);
#else
    openPcmDriver(pcmindex);
#endif

    AL_UNLOCK(AudioALSADriverUtility::getInstance()->getStreamSramDramLock());



#if defined(MTK_AUDIO_SW_DRE) && defined(MTK_NEW_VOL_CONTROL)
    mStartMuteBytes = mConfig.period_size *
                      mConfig.period_count *
                      mConfig.channels *
                      (pcm_format_to_bits(mConfig.format) / 8);

    mAllZeroBlock = new char[kPcmDriverBufferSize];
    memset(mAllZeroBlock, 0, kPcmDriverBufferSize);
#endif

#ifdef MTK_AURISYS_FRAMEWORK_SUPPORT
    if (get_aurisys_on()) {
        CreateAurisysLibManager();
    } else
#endif
    {
        // post processing
        initPostProcessing();

        // SRC
        initBliSrc();

        // bit conversion
        initBitConverter();

        initDataPending();
    }

    // init DC Removal
    initDcRemoval();

#if defined(MTK_SPEAKER_MONITOR_SUPPORT)
    unsigned int fc, bw;
    int th;
    if (mAudioFilterManagerHandler) {
        AudioALSASpeakerMonitor::getInstance()->GetFilterParam(&fc, &bw, &th);
        ALOGD("%s(), fc %d bw %d, th %d", __FUNCTION__, fc, bw, th);
        mAudioFilterManagerHandler->setSpkFilterParam(fc, bw, th);
    }
#endif

#if defined(MTK_HYBRID_NLE_SUPPORT) // must be after pcm open
    mStreamAttributeTarget.output_devices = mStreamAttributeSource->output_devices;
    initNLEProcessing();
#endif

    // open codec driver
    mHardwareResourceManager->startOutputDevice(mStreamAttributeSource->output_devices, mStreamAttributeTarget.sample_rate);


    //============Voice UI&Unlock REFERECE=============
    AudioVUnlockDL *VUnlockhdl = AudioVUnlockDL::getInstance();
    if (VUnlockhdl != NULL) {
        VUnlockhdl->SetInputStandBy(false);
        VUnlockhdl-> GetSRCInputParameter(mStreamAttributeTarget.sample_rate, mStreamAttributeTarget.num_channels, mStreamAttributeTarget.audio_format);
        VUnlockhdl->GetFirstDLTime();
    }
    //===========================================

    mTimeStampValid = false;
    mBytesWriteKernel = 0;
    ALOGD("-%s()", __FUNCTION__);
    return NO_ERROR;
}


status_t AudioALSAPlaybackHandlerNormal::close() {
    ALOGD("+%s(), flag %d, mDevice = 0x%x", __FUNCTION__, mStreamAttributeSource->mAudioOutputFlags,
          mStreamAttributeSource->output_devices);


    //============Voice UI&Unlock REFERECE=============
    AudioVUnlockDL *VUnlockhdl = AudioVUnlockDL::getInstance();
    if (VUnlockhdl != NULL) {
        VUnlockhdl->SetInputStandBy(true);
    }
    //===========================================
#if defined(MTK_AUDIO_SW_DRE) && defined(MTK_NEW_VOL_CONTROL)
    delete [] mAllZeroBlock;
    if (mForceMute) {
        mForceMute = false;
        ALOGD("%s(), SWDRE swdre unmute", __FUNCTION__);
        AudioMTKGainController::getInstance()->requestMute(getIdentity(), false);
    }
#endif

#if defined(MTK_HYBRID_NLE_SUPPORT)
    // Must do this before close analog path
    deinitNLEProcessing();
#endif

    // close codec driver
    mHardwareResourceManager->stopOutputDevice();

    // close pcm driver
    AL_AUTOLOCK(*AudioALSADriverUtility::getInstance()->getStreamSramDramLock());
    closePcmDriver();

    // disable lowjitter mode
    SetLowJitterMode(false, mStreamAttributeTarget.sample_rate);

#ifdef MTK_AURISYS_FRAMEWORK_SUPPORT
    if (get_aurisys_on()) {
        DestroyAurisysLibManager();
    } else
#endif
    {
        // bit conversion
        deinitBitConverter();

        // SRC
        deinitBliSrc();

        // post processing
        deinitPostProcessing();

        DeinitDataPending();
    }

    //DC removal
    deinitDcRemoval();

    // debug pcm dump
    ClosePCMDump();
	
	//awinic add
	#ifdef awinic_mec
    if(Aw_Ready == true)
      Aw_Algo_Clear(Aw_Cfg_buffer);
    #endif
	//awinic add

    //release pmic clk
    mHardwareResourceManager->EnableAudBufClk(false);

    ALOGD("-%s()", __FUNCTION__);
    return NO_ERROR;
}


status_t AudioALSAPlaybackHandlerNormal::routing(const audio_devices_t output_devices) {
    mHardwareResourceManager->changeOutputDevice(output_devices);
    if (mAudioFilterManagerHandler) { mAudioFilterManagerHandler->setDevice(output_devices); }
    return NO_ERROR;
}
int AudioALSAPlaybackHandlerNormal::pause() {
    return -ENODATA;
}

int AudioALSAPlaybackHandlerNormal::resume() {
    return -ENODATA;
}

int AudioALSAPlaybackHandlerNormal::flush() {
    return 0;
}

status_t AudioALSAPlaybackHandlerNormal::setVolume(uint32_t vol __unused) {
    return INVALID_OPERATION;
}


int AudioALSAPlaybackHandlerNormal::drain(audio_drain_type_t type __unused) {
    return 0;
}


status_t AudioALSAPlaybackHandlerNormal::setScreenState(bool mode, size_t buffer_size, size_t reduceInterruptSize, bool bforce __unused) {
    // don't increase irq period when play hifi
    if (mode == 0 && mStreamAttributeSource->sample_rate > 48000) {
        return NO_ERROR;
    }

    if (0 == buffer_size) {
        buffer_size = mStreamAttributeSource->buffer_size;
    }

#if defined(MTK_POWERHAL_AUDIO_POWER)
    if (!(platformIsolatedDeepBuffer()) ||
        (platformIsolatedDeepBuffer() &&
         mStreamAttributeSource->mAudioOutputFlags & AUDIO_OUTPUT_FLAG_DEEP_BUFFER)) {
        if (mStreamAttributeSource->mPowerHalEnable) {
            if (mode) {
                power_hal_hint(POWERHAL_POWER_DL, false);
            } else {
                power_hal_hint(POWERHAL_POWER_DL, true);
            }
        }
    }
#endif

    int rate;
    if (mStreamAttributeSource->mAudioOutputFlags & AUDIO_OUTPUT_FLAG_FAST) {
        return NO_ERROR;
    } else {
        rate = mode ? (buffer_size / mStreamAttributeSource->num_channels) / ((mStreamAttributeSource->audio_format == AUDIO_FORMAT_PCM_16_BIT) ? 2 : 4) :
               ((mStreamAttributeTarget.buffer_size / mConfig.channels) / ((mStreamAttributeTarget.audio_format == AUDIO_FORMAT_PCM_16_BIT) ? 2 : 4)
                - reduceInterruptSize);
    }

    mStreamAttributeTarget.mInterrupt = (rate + 0.0) / mStreamAttributeTarget.sample_rate;

    ALOGD("%s, flag %d, rate %d %f, mode = %d , buffer_size = %zu, channel %d, format%d",
          __FUNCTION__, mStreamAttributeSource->mAudioOutputFlags, rate,
          mStreamAttributeTarget.mInterrupt, mode, buffer_size, mConfig.channels,
          mStreamAttributeTarget.audio_format);

    mHardwareResourceManager->setInterruptRate(mStreamAttributeSource->mAudioOutputFlags, rate);

    return NO_ERROR;
}

ssize_t AudioALSAPlaybackHandlerNormal::write(const void *buffer, size_t bytes) {
    ALOGV("%s(), buffer = %p, bytes = %zu", __FUNCTION__, buffer, bytes);

    if (mPcm == NULL) {
        ALOGE("%s(), mPcm == NULL, return", __FUNCTION__);
        return bytes;
    }
//awinic add 
#ifdef awinic_mec
    char prop[1024];
    bool flag = false;
    size_t outputSize = 0;
    property_get("af.awinic.support",prop,"");
    if(!strncmp("true",prop,sizeof("true"))){
          flag =true;
          //ALOGD("%s: enable Awinic \n",__func__);
    }else{
          flag =false;
          //ALOGD("%s:disable Awinic",__func__);
    }
#endif
//awinic add
    // const -> to non const
    void *pBuffer = const_cast<void *>(buffer);
    ASSERT(pBuffer != NULL);

#ifdef DEBUG_LATENCY
    clock_gettime(CLOCK_REALTIME, &mNewtime);
    latencyTime[0] = calc_time_diff(mNewtime, mOldtime);
    mOldtime = mNewtime;
#endif

#if defined(MTK_AUDIO_SW_DRE) && defined(MTK_NEW_VOL_CONTROL)
    if (mStreamAttributeSource->output_devices == AUDIO_DEVICE_OUT_WIRED_HEADSET ||
        mStreamAttributeSource->output_devices == AUDIO_DEVICE_OUT_WIRED_HEADPHONE) {
        bool isAllMute = false;

        /* check if contents is mute */
        if (!memcmp(mAllZeroBlock, buffer, bytes)) {
            isAllMute = true;
        } else {
            isAllMute = true;
            size_t tmp_bytes = bytes;
            int32_t *sample = (int32_t *)buffer;
            while (tmp_bytes > 0) {
                if ((*sample) >> 8 != 0 && ((*sample) & 0xffffff00) != 0xffffff00) {
                    isAllMute = false;
                    break;
                }
                tmp_bytes -= 4;
                sample++;
            }
        }

        /* calculate delay and apply mute */
        ALOGV("%s(), isAllMute = %d, mForceMute = %d, mCurMuteBytes = %d, mStartMuteBytes = %d",
              __FUNCTION__,
              isAllMute,
              mForceMute,
              mCurMuteBytes,
              mStartMuteBytes);

        if (isAllMute) {
            if (!mForceMute) { /* not mute yet */
                mCurMuteBytes += bytes;
                if (mCurMuteBytes >= mStartMuteBytes) {
                    mForceMute = true;
                    ALOGD("%s(), SWDRE swdre mute", __FUNCTION__);
                    AudioMTKGainController::getInstance()->requestMute(getIdentity(), true);
                }
            }
        } else {
            mCurMuteBytes = 0;

            if (mForceMute) {
                mForceMute = false;
                ALOGD("%s(), SWDRE swdre unmute", __FUNCTION__);
                AudioMTKGainController::getInstance()->requestMute(getIdentity(), false);
            }
        }
    }
#endif
    void *pBufferAfterDcRemoval = NULL;
    uint32_t bytesAfterDcRemoval = 0;
    // DC removal before DRC
    doDcRemoval(pBuffer, bytes, &pBufferAfterDcRemoval, &bytesAfterDcRemoval);

    // stereo to mono for speaker
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
        doPostProcessing(pBufferAfterDcRemoval, bytesAfterDcRemoval, &pBufferAfterPostProcessing, &bytesAfterPostProcessing);


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


    // pcm dump
    WritePcmDumpData(pBufferAfterPending, bytesAfterpending);

#ifdef DEBUG_LATENCY
    clock_gettime(CLOCK_REALTIME, &mNewtime);
    latencyTime[1] = calc_time_diff(mNewtime, mOldtime);
    mOldtime = mNewtime;
#endif

#ifdef MTK_LATENCY_DETECT_PULSE
    AudioDetectPulse::doDetectPulse(TAG_PLAYERBACK_HANDLER, PULSE_LEVEL, 0, (void *)pBufferAfterPending,
                                    bytesAfterpending, mStreamAttributeTarget.audio_format,
                                    mStreamAttributeTarget.num_channels, mStreamAttributeTarget.sample_rate);
#endif
//awinic add 
#ifdef awinic_mec
   if(true == flag){
     memcpy(audio_buffer,(char*)pBufferAfterPending,bytesAfterpending);
     if(Aw_Ready == true){
		 //ALOGD("%s: AwinicAudioHandle bytes %d\n",__func__,bytesAfterpending);
         Aw_Algo_Handle(audio_buffer,bytesAfterpending,Aw_Cfg_buffer);
	 }
   }
#endif
//awinic add 

    // write data to pcm driver
#ifdef awinic_mec
	//awinic add 
    int  retval=0;
    if(true == flag){	 
         retval= pcmWrite(mPcm, audio_buffer, bytesAfterpending);	 
    }else{
         retval= pcmWrite(mPcm, pBufferAfterPending, bytesAfterpending);
    }
#else
	int retval= pcmWrite(mPcm, pBufferAfterPending, bytesAfterpending);
#endif
//awinic add
    mBytesWriteKernel = mBytesWriteKernel + bytesAfterpending;
    if (mTimeStampValid == false) {
        if (mBytesWriteKernel >= (mStreamAttributeTarget.buffer_size >> 1)) {
            mTimeStampValid = true;
        }
    }

 
#if defined(MTK_HYBRID_NLE_SUPPORT)
    if (mSupportNLE) {
        doNLEProcessing(pBufferAfterPending, bytesAfterpending);
    }
#endif

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
    if (latencyTime[0] > THRESHOLD_FRAMEWORK || latencyTime[1] > THRESHOLD_HAL || latencyTime[2] > (mStreamAttributeTarget.mInterrupt - latencyTime[0] - latencyTime[1] + THRESHOLD_KERNEL)) {
        ALOGD("latency_in_s,%1.3lf,%1.3lf,%1.3lf, interrupt,%1.3lf,byte:%u", latencyTime[0], latencyTime[1], latencyTime[2], mStreamAttributeTarget.mInterrupt, bytesAfterpending);
    }
#endif

    return bytes;
}


status_t AudioALSAPlaybackHandlerNormal::setFilterMng(AudioMTKFilterManager *pFilterMng) {
    ALOGD("+%s() mAudioFilterManagerHandler [%p]", __FUNCTION__, pFilterMng);
    mAudioFilterManagerHandler = pFilterMng;
    return NO_ERROR;
}



} // end of namespace android
