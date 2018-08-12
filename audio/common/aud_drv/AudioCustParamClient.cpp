#include "AudioCustParamClient.h"


#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "AudioCustParamClient"

namespace android {

/*==============================================================================
 *                     Implementation
 *============================================================================*/

AudioCustParamClient *AudioCustParamClient::mAudioCustParamClient = NULL;
AudioCustParamClient *AudioCustParamClient::GetInstance() {
    static Mutex mGetInstanceLock;
    Mutex::Autolock _l(mGetInstanceLock);

    if (mAudioCustParamClient == NULL) {
        mAudioCustParamClient = new AudioCustParamClient();
    }
    ASSERT(mAudioCustParamClient != NULL);
    return mAudioCustParamClient;
}

AudioCustParamClient::AudioCustParamClient() {
    ALOGD("%s()", __FUNCTION__);
    acpOpsInited = 0;
    QueryFeatureSupportInfo = NULL;
    GetNBSpeechParamFromNVRam = NULL;
    SetNBSpeechParamToNVRam = NULL;
    GetDualMicSpeechParamFromNVRam = NULL;
    SetDualMicSpeechParamToNVRam = NULL;
    GetWBSpeechParamFromNVRam = NULL;
    SetWBSpeechParamToNVRam = NULL;
    GetMedParamFromNV = NULL;
    SetMedParamToNV = NULL;
    GetVolumeVer1ParamFromNV = NULL;
    SetVolumeVer1ParamToNV = NULL;
    GetAudioCustomParamFromNV = NULL;
    SetAudioCustomParamToNV = NULL;
    GetAudioGainTableParamFromNV = NULL;
    SetAudioGainTableParamToNV = NULL;
    GetHdRecordParamFromNV = NULL;
    SetHdRecordParamToNV = NULL;
    GetHdRecordSceneTableFromNV = NULL;
    SetHdRecordSceneTableToNV = NULL;
    GetVoiceRecogCustParamFromNV = NULL;
    SetVoiceRecogCustParamToNV = NULL;
    GetVOWCustParamFromNV = NULL;
    SetVOWCustParamToNV = NULL;
    GetAudEnhControlOptionParamFromNV = NULL;
    SetAudEnhControlOptionParamToNV = NULL;
    GetHiFiDACControlOptionParamFromNV = NULL;
    SetHiFiDACControlOptionParamToNV = NULL;
    GetDcCalibrationParamFromNV = NULL;
    SetDcCalibrationParamToNV = NULL;
    GetAudioVoIPParamFromNV = NULL;
    SetAudioVoIPParamToNV = NULL;
    GetAudioHFPParamFromNV = NULL;
    SetAudioHFPParamToNV = NULL;
    GetBesLoudnessControlOptionParamFromNV = NULL;
    SetBesLoudnessControlOptionParamToNV = NULL;
    GetMagiConSpeechParamFromNVRam = NULL;
    SetMagiConSpeechParamToNVRam = NULL;
    GetHACSpeechParamFromNVRam = NULL;
    SetHACSpeechParamToNVRam = NULL;
    GetSpeakerMonitorParamFromNVRam = NULL;
    SetSpeakerMonitorParamToNVRam = NULL;
    GetNBSpeechLpbkParamFromNVRam = NULL;
    SetNBSpeechLpbkParamToNVRam = NULL;
    GetAudioBTGainParamFromNV = NULL;
    SetAudioBTGainParamToNV = NULL;
    GetAudioFuncSwitchParamFromNV = NULL;
    SetAudioFuncSwitchParamToNV = NULL;

    mGetNumMicSupport = NULL;

    Init();

    memset(&mDeviceParam, 0, sizeof(struct AudioDeviceParam));
    initParam();
}

AudioCustParamClient::~AudioCustParamClient() {
    ALOGD("%s()", __FUNCTION__);
    Deinit();
}

void AudioCustParamClient::Init(void) {
    const char *error;
    const char *funName = NULL;
    ALOGD("%s(), acpOpsInited(%d)", __FUNCTION__, acpOpsInited);

    if (acpOpsInited == 0) {
        ALOGD("%s(), init AcpOps struct", __FUNCTION__);

        /* dlopen */
        handle = dlopen("libaudiocustparam_vendor.so", RTLD_LAZY);
        if (handle == NULL) {
            ALOGE("%s(), dlopen fail! (%s)\n", __FUNCTION__, dlerror());
        } else {
            dlerror();    /* Clear any existing error */

            /* dlsym */
            // QueryFeatureSupportInfo
            funName = "QueryFeatureSupportInfo";
            QueryFeatureSupportInfo = (uint32_t (*)(void)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }
            // NB speech parameters
            funName = "GetNBSpeechParamFromNVRam";
            GetNBSpeechParamFromNVRam = (int (*)(AUDIO_CUSTOM_PARAM_STRUCT * pSphParamNB)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            funName = "SetNBSpeechParamToNVRam";
            SetNBSpeechParamToNVRam = (int (*)(AUDIO_CUSTOM_PARAM_STRUCT * pSphParamNB)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            // Dual mic speech parameters
            funName = "GetDualMicSpeechParamFromNVRam";
            GetDualMicSpeechParamFromNVRam = (int (*)(AUDIO_CUSTOM_EXTRA_PARAM_STRUCT * pSphParamDualMic)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            funName = "SetDualMicSpeechParamToNVRam";
            SetDualMicSpeechParamToNVRam = (int (*)(AUDIO_CUSTOM_EXTRA_PARAM_STRUCT * pSphParamDualMic)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            // WB speech parameters
            funName = "GetWBSpeechParamFromNVRam";
            GetWBSpeechParamFromNVRam = (int (*)(AUDIO_CUSTOM_WB_PARAM_STRUCT * pSphParamWB)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            funName = "SetWBSpeechParamToNVRam";
            SetWBSpeechParamToNVRam = (int (*)(AUDIO_CUSTOM_WB_PARAM_STRUCT * pSphParamWB)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            // Med param parameter
            funName = "GetMedParamFromNV";
            GetMedParamFromNV = (int (*)(AUDIO_PARAM_MED_STRUCT * pPara)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            funName = "SetMedParamToNV";
            SetMedParamToNV = (int (*)(AUDIO_PARAM_MED_STRUCT * pPara)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            // VolumeVer1 parameter
            funName = "GetVolumeVer1ParamFromNV";
            GetVolumeVer1ParamFromNV = (int (*)(AUDIO_VER1_CUSTOM_VOLUME_STRUCT * pPara)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            funName = "SetVolumeVer1ParamToNV";
            SetVolumeVer1ParamToNV = (int (*)(AUDIO_VER1_CUSTOM_VOLUME_STRUCT * pPara)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            // Audio Custom Paramete
            funName = "GetAudioCustomParamFromNV";
            GetAudioCustomParamFromNV = (int (*)(AUDIO_VOLUME_CUSTOM_STRUCT * pPara)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            funName = "SetAudioCustomParamToNV";
            SetAudioCustomParamToNV = (int (*)(AUDIO_VOLUME_CUSTOM_STRUCT * pPara)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            // AudioGainTable Parameter
            funName = "GetAudioGainTableParamFromNV";
            GetAudioGainTableParamFromNV = (int (*)(AUDIO_GAIN_TABLE_STRUCT * pPara)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            funName = "SetAudioGainTableParamToNV";
            SetAudioGainTableParamToNV = (int (*)(AUDIO_GAIN_TABLE_STRUCT * pPara)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            // Audio HD record parameters
            funName = "GetHdRecordParamFromNV";
            GetHdRecordParamFromNV = (int (*)(AUDIO_HD_RECORD_PARAM_STRUCT * pPara)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            funName = "SetHdRecordParamToNV";
            SetHdRecordParamToNV = (int (*)(AUDIO_HD_RECORD_PARAM_STRUCT * pPara)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            //  Audio HD record scene table
            funName = "GetHdRecordSceneTableFromNV";
            GetHdRecordSceneTableFromNV = (int (*)(AUDIO_HD_RECORD_SCENE_TABLE_STRUCT * pPara)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            funName = "SetHdRecordSceneTableToNV";
            SetHdRecordSceneTableToNV = (int (*)(AUDIO_HD_RECORD_SCENE_TABLE_STRUCT * pPara)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            // Voice Revognition Customization Parameters
            funName = "GetVoiceRecogCustParamFromNV";
            GetVoiceRecogCustParamFromNV = (int (*)(VOICE_RECOGNITION_PARAM_STRUCT * pPara)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            funName = "SetVoiceRecogCustParamToNV";
            SetVoiceRecogCustParamToNV = (int (*)(VOICE_RECOGNITION_PARAM_STRUCT * pPara)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            // Voice Wakeup Customization Parameters
            funName = "GetVOWCustParamFromNV";
            GetVOWCustParamFromNV = (int (*)(int index)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            funName = "SetVOWCustParamToNV";
            SetVOWCustParamToNV = (int (*)(int index, int value)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            // Audio Enhancement Control Option Parameters
            funName = "GetAudEnhControlOptionParamFromNV";
            GetAudEnhControlOptionParamFromNV = (int (*)(AUDIO_AUDENH_CONTROL_OPTION_STRUCT * pPara)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            funName = "SetAudEnhControlOptionParamToNV";
            SetAudEnhControlOptionParamToNV = (int (*)(AUDIO_AUDENH_CONTROL_OPTION_STRUCT * pPara)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }


            // HiFi DAC Control Option Parameters
            funName = "GetHiFiDACControlOptionParamFromNV";
            GetHiFiDACControlOptionParamFromNV = (int (*)(AUDIO_AUDENH_CONTROL_OPTION_STRUCT * pPara)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            funName = "SetHiFiDACControlOptionParamToNV";
            SetHiFiDACControlOptionParamToNV = (int (*)(AUDIO_AUDENH_CONTROL_OPTION_STRUCT * pPara)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            // Audio Buffer DC Calibration data
            funName = "GetDcCalibrationParamFromNV";
            GetDcCalibrationParamFromNV = (int (*)(AUDIO_BUFFER_DC_CALIBRATION_STRUCT * pPara)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            funName = "SetDcCalibrationParamToNV";
            SetDcCalibrationParamToNV = (int (*)(AUDIO_BUFFER_DC_CALIBRATION_STRUCT * pPara)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            // Audio VoIP Parameters
            funName = "GetAudioVoIPParamFromNV";
            GetAudioVoIPParamFromNV = (int (*)(AUDIO_VOIP_PARAM_STRUCT * pPara)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            funName = "SetAudioVoIPParamToNV";
            SetAudioVoIPParamToNV = (int (*)(AUDIO_VOIP_PARAM_STRUCT * pPara)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            // Audio HFP Parameters
            funName = "GetAudioHFPParamFromNV";
            GetAudioHFPParamFromNV = (int (*)(AUDIO_HFP_PARAM_STRUCT * pPara)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            funName = "SetAudioHFPParamToNV";
            SetAudioHFPParamToNV = (int (*)(AUDIO_HFP_PARAM_STRUCT * pPara)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            // BesLoudness Control Option Parameters
            funName = "GetBesLoudnessControlOptionParamFromNV";
            GetBesLoudnessControlOptionParamFromNV = (int (*)(AUDIO_AUDENH_CONTROL_OPTION_STRUCT * pPara)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            funName = "SetBesLoudnessControlOptionParamToNV";
            SetBesLoudnessControlOptionParamToNV = (int (*)(AUDIO_AUDENH_CONTROL_OPTION_STRUCT * pParaB)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            // Magic Conference Call parameters
            funName = "GetMagiConSpeechParamFromNVRam";
            GetMagiConSpeechParamFromNVRam = (int (*)(AUDIO_CUSTOM_MAGI_CONFERENCE_STRUCT * pSphParamMagiCon)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            funName = "SetMagiConSpeechParamToNVRam";
            SetMagiConSpeechParamToNVRam = (int (*)(AUDIO_CUSTOM_MAGI_CONFERENCE_STRUCT * pSphParamMagiCon)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            // HAC parameters
            funName = "GetHACSpeechParamFromNVRam";
            GetHACSpeechParamFromNVRam = (int (*)(AUDIO_CUSTOM_HAC_PARAM_STRUCT * pSphParamHAC)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            funName = "SetHACSpeechParamToNVRam";
            SetHACSpeechParamToNVRam = (int (*)(AUDIO_CUSTOM_HAC_PARAM_STRUCT * pSphParamHAC)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            // Speaker Monitor parameters
            funName = "GetSpeakerMonitorParamFromNVRam";
            GetSpeakerMonitorParamFromNVRam = (int (*)(AUDIO_SPEAKER_MONITOR_PARAM_STRUCT * pParam)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            funName = "SetSpeakerMonitorParamToNVRam";
            SetSpeakerMonitorParamToNVRam = (int (*)(AUDIO_SPEAKER_MONITOR_PARAM_STRUCT * pParam)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            // Speech Loopback parameters
            funName = "GetNBSpeechLpbkParamFromNVRam";
            GetNBSpeechLpbkParamFromNVRam = (int (*)(AUDIO_CUSTOM_SPEECH_LPBK_PARAM_STRUCT * pSphParamNBLpbk)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            funName = "SetNBSpeechLpbkParamToNVRam";
            SetNBSpeechLpbkParamToNVRam = (int (*)(AUDIO_CUSTOM_SPEECH_LPBK_PARAM_STRUCT * pSphParamNBLpbk)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            // BT Gain parameter
            funName = "GetAudioBTGainParamFromNV";
            GetAudioBTGainParamFromNV = (int (*)(AUDIO_BT_GAIN_STRUCT * pParaBT)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            funName = "SetAudioBTGainParamToNV";
            SetAudioBTGainParamToNV = (int (*)(AUDIO_BT_GAIN_STRUCT * pParaBT)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            // Audio Function Switch parameter
            funName = "GetAudioFuncSwitchParamFromNV";
            GetAudioFuncSwitchParamFromNV = (int (*)(AUDIO_CUSTOM_AUDIO_FUNC_SWITCH_PARAM_STRUCT * pParaAudioFuncSwitch)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            funName = "SetAudioFuncSwitchParamToNV";
            SetAudioFuncSwitchParamToNV = (int (*)(AUDIO_CUSTOM_AUDIO_FUNC_SWITCH_PARAM_STRUCT * pParaAudioFuncSwitch)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            // mGetNumMicSupport
            funName = "getNumMicSupport";
            mGetNumMicSupport = (int (*)(void)) dlsym(handle, funName);
            error = dlerror();
            if (error != NULL) {
                ALOGE("%s(), dlsym %s fail. (%s)\n", __FUNCTION__, funName, error);
            }

            acpOpsInited = 1;
        }
    }
}

void AudioCustParamClient::Deinit() {
    ALOGD("%s(), acpOpsInited (%d)\n", __FUNCTION__, acpOpsInited);
    if (acpOpsInited != 0) {
        dlclose(handle);
        acpOpsInited = 0;
    }
}

void AudioCustParamClient::initParam() {
    if (mGetNumMicSupport) {
        mDeviceParam.numMicSupport = mGetNumMicSupport();
    } else {
        ALOGE("%s(), mGetNumMicSupport == NULL", __FUNCTION__);
        ASSERT(0);
        mDeviceParam.numMicSupport = 2;
    }
}

int AudioCustParamClient::getNumMicSupport() {
    return mDeviceParam.numMicSupport;
}

}
