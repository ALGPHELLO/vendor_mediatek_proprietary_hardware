#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/stat.h>
#include <utils/Log.h>
#if defined(NXP_TFA9890_SUPPORT)
#include "tfa9890/tfa9890_interface.h"
#elif defined(NXP_TFA9887_SUPPORT)
#include "tfa9887/interface/tfa9887_interface.h"
#endif

#ifdef WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "mtk_tfa98xx_interface.h"

int mtk_tfa98xx_init(struct SmartPa *smartPa);
int mtk_tfa98xx_deinit(void);
int mtk_tfa98xx_speaker_on(struct SmartPaRuntime *runtime);
int mtk_tfa98xx_speaker_off(void);

// smart pa init
int mtk_smartpa_init(struct SmartPa *smartPa)
{
    ALOGD("%s() begin", __func__);
    struct SmartPaOps *ops = &smartPa->ops;

    ops->init = mtk_tfa98xx_init;
    ops->deinit = mtk_tfa98xx_deinit;
    ops->speakerOn = mtk_tfa98xx_speaker_on;
    ops->speakerOff = mtk_tfa98xx_speaker_off;

    return 0;
}

// smart pa operation callbacks
int mtk_tfa98xx_init(struct SmartPa *smartPa)
{
    // initialize the lib first, to cut down the open time later
    // e.g. download lib, param, etc.

    mtk_tfa98xx_speaker_on(&smartPa->runtime);
    mtk_tfa98xx_speaker_off();

    return 0;
}

int mtk_tfa98xx_deinit(void)
{
    return MTK_Tfa98xx_Deinit();
}

int mtk_tfa98xx_speaker_on(struct SmartPaRuntime *runtime)
{
    MTK_Tfa98xx_SetSampleRate(runtime->sampleRate);
    MTK_Tfa98xx_SpeakerOn();

    return 0;
}

int mtk_tfa98xx_speaker_off(void)
{
    MTK_Tfa98xx_SpeakerOff();
    return 0;
}

// 3rd party wrapper
int MTK_Tfa98xx_Check_TfaOpen(void);

int MTK_Tfa98xx_Check_TfaOpen(void)
{
#if defined(NXP_TFA9890_SUPPORT)
   return tfa9890_check_tfaopen();
#elif defined(NXP_TFA9887_SUPPORT)
   return tfa9887_check_tfaopen();
#endif

}

int MTK_Tfa98xx_Init(void)
{
    int res;
    ALOGD("Tfa98xx: +%s",__func__);
#if defined(NXP_TFA9890_SUPPORT)
    res = tfa9890_init();
#elif defined(NXP_TFA9887_SUPPORT)
    res = tfa9887_init();
#endif
    ALOGD("Tfa98xx: -%s, res= %d",__func__,res);
    return res;
}

int MTK_Tfa98xx_Deinit(void)
{
    int res = 0;
    ALOGD("Tfa98xx: +%s",__func__);
    if(MTK_Tfa98xx_Check_TfaOpen())
    {
#if defined(NXP_TFA9890_SUPPORT)
        res = tfa9890_deinit();
#elif defined(NXP_TFA9887_SUPPORT)
        res = tfa9887_deinit();
#endif
    }
    return res;
}

void MTK_Tfa98xx_SpeakerOn(void)
{
    ALOGD("Tfa98xx: +%s, nxp_init_flag= %d",__func__,MTK_Tfa98xx_Check_TfaOpen());
    if(!MTK_Tfa98xx_Check_TfaOpen())                   //already done in tfa9890_SpeakerOn()
        MTK_Tfa98xx_Init();

#if defined(NXP_TFA9890_SUPPORT)
    tfa9890_SpeakerOn();
#elif defined(NXP_TFA9887_SUPPORT)
    tfa9887_SpeakerOn();
#endif
    ALOGD("Tfa98xx: -%s, nxp_init_flag= %d",__func__,MTK_Tfa98xx_Check_TfaOpen());
}

void MTK_Tfa98xx_SpeakerOff(void)
{
    ALOGD("Tfa98xx: +%s",__func__);

#if defined(NXP_TFA9890_SUPPORT)
    tfa9890_SpeakerOff();
    usleep(10*1000);
#elif defined(NXP_TFA9887_SUPPORT)
    tfa9887_SpeakerOff();
    usleep(10*1000);
#endif
}

void MTK_Tfa98xx_SetSampleRate(int samplerate)
{
    ALOGD("Tfa98xx: +%s, samplerate=%d",__func__,samplerate);

#if defined(NXP_TFA9890_SUPPORT)
    tfa9890_setSamplerate(samplerate);
#elif defined(NXP_TFA9887_SUPPORT)
    tfa9887_setSamplerate(samplerate);
#endif
}

void MTK_Tfa98xx_SetBypassDspIncall(int bypass)
{
    ALOGD("Tfa98xx: +%s, bypass= %d",__func__,bypass);
#if defined(NXP_TFA9890_SUPPORT)
    tfa9890_set_bypass_dsp_incall(bypass);
#elif defined(NXP_TFA9887_SUPPORT)
    tfa9887_set_bypass_dsp_incall(bypass);
#endif
}

void MTK_Tfa98xx_EchoReferenceConfigure(int config)
{
    ALOGD("Tfa98xx: +%s, nxp_init_flag= %d, config= %d",__func__,MTK_Tfa98xx_Check_TfaOpen(), config);
    if(MTK_Tfa98xx_Check_TfaOpen())
    {
#if defined(NXP_TFA9890_SUPPORT)
       tfa9890_EchoReferenceConfigure(config);
#elif defined(NXP_TFA9887_SUPPORT)
       tfa9887_EchoReferenceConfigure(config);
#endif
    }
}

void MTK_Tfa98xx_Reset()
{
    ALOGD("MTK_Tfa98xx_Reset");
#if defined(NXP_TFA9890_SUPPORT)
    tfa9890_deinit();
    tfa9890_reset();
#elif defined(NXP_TFA9887_SUPPORT)
    tfa9887_deinit();
    tfa9887_reset();
#endif
}

