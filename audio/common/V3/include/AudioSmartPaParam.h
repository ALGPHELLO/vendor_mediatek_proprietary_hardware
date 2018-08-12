#ifndef ANDROID_AUDIO_SMART_PA_PARAM_H
#define ANDROID_AUDIO_SMART_PA_PARAM_H


#include "AudioLock.h"
#include "AudioType.h"
#include "AudioALSADriverUtility.h"
#include <arsi_type.h>
#include "AudioMessengerIPI.h"
#define SMARTPA_PARAM_LENGTH (256)

namespace android {

class AudioSmartPaParam {

public:
    ~AudioSmartPaParam();

    int             SetParameter(const char *keyValuePair);
    char            *GetParameter(const char *key);


    /**
     * get instance's pointer
     */
    static AudioSmartPaParam *getInstance(void);

    int SetArsiTaskConfig(arsi_task_config_t ArsiTaskConfig);
    int Setlibconfig(arsi_lib_config_t mArsiLibConfig);
    int SetSmartpaParam(int mode);

protected:

    AudioSmartPaParam();

private:
    static AudioSmartPaParam *mAudioSmartPaParam;

    bool checkParameter(int &paramindex, int &vendorinedx, int &direction, const char *keyValuePair);
    int getsetParameterPrefixlength(int paramindex, int vendorindex);
    int getgetParameterPrefixlength(int paramindex, int vendorindex);
    int setParamFilePath(const char *str);
    int setProductName(const char *str);
    char *getParamFilePath(void);
    char *getProductName(void);
    int getDefalutParamFilePath(void);
    int getDefaultProductName(void);


    void initArsiTaskConfig(void);
    void initlibconfig();

    static const int paramlength = SMARTPA_PARAM_LENGTH;
    static const int pDlsamplerate = 48000;
    char mSmartParamFilePath[paramlength];
    char mPhoneProductName[paramlength];

    /* mdoe to set parameters*/
    int mSmartpaMode;

    /* IPI message */
    AudioMessengerIPI *pIPI;

    arsi_task_config_t *mArsiTaskConfig;
    arsi_lib_config_t *mArsiLibConfig;

    data_buf_t      *mParamBuf;

    void *mAurisysLib;

    bool mEnableLibLogHAL;
    bool mEnableDump;
    char mSvalue[SMARTPA_PARAM_LENGTH];

};

}
#endif
