#ifndef ANDROID_AUDIO_ALSA_PLAYBACK_HANDLER_NORMAL_H
#define ANDROID_AUDIO_ALSA_PLAYBACK_HANDLER_NORMAL_H

#include "AudioALSAPlaybackHandlerBase.h"
//awinic add
#define awinic_mec
#ifdef awinic_mec
typedef int (*AwAlgoInit)(void*);
typedef void (*AwAlgoDeinit)(void*);
typedef uint32_t (*AwAlgoGetSize)(void);
typedef int32_t (*AwAudioHandle)(void*,uint32_t,void*);
typedef void (*AwAlgoClear)(void*);
#define AWINIC_LIB_PATH "/system/vendor/lib/hw/awinic.audio.effect.so"
#endif
//awinic add


namespace android {



class AudioALSAPlaybackHandlerNormal : public AudioALSAPlaybackHandlerBase {
public:
    AudioALSAPlaybackHandlerNormal(const stream_attribute_t *stream_attribute_source);
    virtual ~AudioALSAPlaybackHandlerNormal();


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

    virtual status_t setFilterMng(AudioMTKFilterManager *pFilterMng);


    /**
     * low latency
     */
    virtual status_t setScreenState(bool mode, size_t buffer_size, size_t reduceInterruptSize, bool bforce = false);

private:
    struct timespec mNewtime, mOldtime;
    bool SetLowJitterMode(bool bEnable, uint32_t SampleRate);
    uint32_t ChooseTargetSampleRate(uint32_t SampleRate, audio_devices_t outputdevice);
    bool DeviceSupportHifi(audio_devices_t outputdevice);
    uint32_t GetLowJitterModeSampleRate(void);
    struct pcm_config mHpImpedanceConfig;
    struct pcm *mHpImpeDancePcm;
    double latencyTime[3];

    //#ifdef MTK_AUDIO_SW_DRE
    bool mForceMute;
    int mCurMuteBytes;
    int mStartMuteBytes;
    char *mAllZeroBlock;
    //#endif

    bool mSupportNLE;
	//awinic Add
#ifdef awinic_mec
    AwAlgoInit    Aw_Algo_Init;
    AwAlgoDeinit  Aw_Algo_Deinit;
    AwAudioHandle Aw_Algo_Handle;
    AwAlgoClear   Aw_Algo_Clear;
    AwAlgoGetSize Aw_Get_Size;
    bool Aw_Ready ;
    char *Aw_Cfg_buffer;
    char *audio_buffer;
#endif	
	//awinic add
	
};

} // end namespace android

#endif // end of ANDROID_AUDIO_ALSA_PLAYBACK_HANDLER_NORMAL_H
