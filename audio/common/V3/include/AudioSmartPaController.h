#ifndef ANDROID_AUDIO_SMART_PA_CONTROLLER_H
#define ANDROID_AUDIO_SMART_PA_CONTROLLER_H

#ifdef __cplusplus
#include <AudioLock.h>
#include "AudioType.h"
#include "AudioALSADriverUtility.h"
#endif

struct SmartPaRuntime {
    unsigned int sampleRate;
    int mode;
    int device;
};

struct SmartPaAttribute {
    bool isSmartPAUsed;
    unsigned int dspType;
    unsigned int chipDelayUs;

    char spkLibPath[128];

    unsigned int supportedRateList[32];
    unsigned int supportedRateMax;
    unsigned int supportedRateMin;

    char codecCtlName[128];
    int isAlsaCodec;
    int isApllNeeded;
    unsigned int i2sSetStage;
};

struct SmartPa;
struct SmartPaOps {
    int (*init)(struct SmartPa *smartPa);
    int (*speakerOn)(struct SmartPaRuntime *runtime);
    int (*speakerOff)();
    int (*deinit)();
};

struct SmartPa {
    struct SmartPaOps ops;
    struct SmartPaRuntime runtime;
    struct SmartPaAttribute attribute;
};

enum spk_enhancement_type {
    SPK_AP_DSP = 0, /* AP Sw Enhancement*/
    SPK_ONBOARD_DSP = 1, /* SPK on board Enhancement*/
    SPK_APSCP_DSP = 2, /* SPK AP SCP Enhancement*/
};

enum spk_amp_type {
    SPK_NOT_SMARTPA = 0,
    SPK_RICHTEK_RT5509,
    SPK_AMP_TYPE_NUM
};

enum spk_i2s_set_stage {
    SPK_I2S_NO_NEED = 0,
    SPK_I2S_AUDIOSERVER_INIT = 0x1,
    SPK_I2S_BEFORE_PCM_OPEN = 0x2,
    SPK_I2S_BEFORE_SPK_ON = 0x4,
};

#ifdef __cplusplus
namespace android {
class AudioSmartPaController {
    AudioSmartPaController();
    ~AudioSmartPaController();

    int init();
    int deinit();

    int initSpkAmpType();
    int initSmartPaAttribute();
    int initSmartPaRuntime();


    static AudioSmartPaController *mAudioSmartPaController;
    struct SmartPa mSmartPa;

    struct mixer *mMixer;

    void *mLibHandle;
    int (*mtk_smartpa_init)(struct SmartPa *smartPa);
    void setSmartPaRuntime(unsigned int device);
    int transformDeviceIndex(const unsigned int device);

public:
    static AudioSmartPaController *getInstance();

    int speakerOn(unsigned int sampleRate, unsigned int device);
    int speakerOff();

    int dspOnBoardSpeakerOn(unsigned int sampleRate);
    int dspOnBoardSpeakerOff();

    unsigned int getSmartPaDelayUs();

    unsigned int getMaxSupportedRate();
    unsigned int getMinSupportedRate();
    bool isRateSupported(unsigned int rate);

    bool isAlsaCodec();
    bool isApSideSpkProtect();
    unsigned int getSpkProtectType();
    unsigned int getI2sSetStage();
    bool isSmartPAUsed();
};

}
#endif
#endif
