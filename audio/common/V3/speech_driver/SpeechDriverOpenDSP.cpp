#include "SpeechDriverOpenDSP.h"

#include <utils/threads.h>
#include <cutils/properties.h>

#include <dlfcn.h>


#include <cutils/properties.h>

#include <arsi_api.h>
#include <audio_task.h>
#include <arsi_library_entry_points.h>

#include <aurisys_utility.h>

#include <AudioLock.h>
#include "AudioUtility.h"

#include "AudioALSAHardwareResourceManager.h"
#include "AudioALSAStreamManager.h"

#include "SpeechDriverLAD.h"
#include <SpeechDriverNormal.h>
#include "SpeechDriverDummy.h"

#include "AudioMessengerIPI.h"

#include "audio_speech_msg_id.h"

#define LOG_TAG "SpeechDriverOpenDSP"


namespace android {

/**
 * lib related
 */
static const char PROPERTY_KEY_SCP_CALL_STATE[NUM_MODEM][PROPERTY_KEY_MAX] = {
    "persist.af.scp.call_state.md1",
    "persist.af.scp.call_state.md2",
    "persist.af.scp.call_state.md3"
};

static const char PROPERTY_KEY_PARAM_PATH[PROPERTY_KEY_MAX] = "persist.af.aurisys.param_path";
static const char PROPERTY_KEY_PRODUCT_NAME[PROPERTY_KEY_MAX] = "ro.product.model";
static const char PROPERTY_KEY_PCM_DUMP_ON[PROPERTY_KEY_MAX] = "persist.af.aurisys.pcm_dump_on";

#if defined(__LP64__)
#define AURISYS_PHONE_VENDOR_LIB_PATH "/vendor/lib64/libfvaudio.so"
#define AURISYS_PHONE_SYSTEM_LIB_PATH "/system/lib64/libfvaudio.so"
#else
#define AURISYS_PHONE_VENDOR_LIB_PATH "/vendor/lib/libfvaudio.so"
#define AURISYS_PHONE_SYSTEM_LIB_PATH "/system/lib/libfvaudio.so"
#endif

#define DEFAULT_VENDOR_PARAM_PATH     "/vendor/etc/aurisys_param/FV-SAM-MTK2.dat"
#define DEFAULT_SYSTEM_PARAM_PATH     "/system/etc/aurisys_param/FV-SAM-MTK2.dat"

#define DEFAULT_PRODUCT_NAME   "vendor=mediatek,model=k97v1_64_op02_lwg_ss_dsp_mp3,device=k97v1_64" // TODO

#define DUMP_DSP_PCM_DATA_PATH "/sdcard/mtklog/aurisys_dump/"

#define AUDIO_CHANNEL_IN_3MIC (AUDIO_CHANNEL_IN_LEFT | AUDIO_CHANNEL_IN_RIGHT | AUDIO_CHANNEL_IN_BACK)



typedef void (*dynamic_link_arsi_assign_lib_fp_t)(AurisysLibInterface *lib);
static dynamic_link_arsi_assign_lib_fp_t dynamic_link_arsi_assign_lib_fp = NULL;
static AurisysLibInterface gLibAPI; // could implemented by any 3rd party


static void *gAurisysLib = NULL;
static int   gEnhMode;

static char  gEnhParamFilePath[256];
static char  gPhoneProductName[256];

static int   gRetvalSetParameter;


static bool gEnableLibLogHAL = false;
static bool gEnableDump;
static char svalue[256];


/*==============================================================================
 *                     Singleton Pattern
 *============================================================================*/

SpeechDriverOpenDSP *SpeechDriverOpenDSP::mSpeechDriverMD1 = NULL;
SpeechDriverOpenDSP *SpeechDriverOpenDSP::mSpeechDriverMD2 = NULL;
SpeechDriverOpenDSP *SpeechDriverOpenDSP::mSpeechDriverMDExt = NULL;

SpeechDriverInterface *SpeechDriverOpenDSP::GetInstance(
    modem_index_t modem_index) {
    static AudioLock mGetInstanceLock;
    AL_AUTOLOCK(mGetInstanceLock);

    SpeechDriverOpenDSP *pSpeechDriver = NULL;
    ALOGD("%s(), modem_index = %d", __FUNCTION__, modem_index);

    switch (modem_index) {
    case MODEM_1:
        if (mSpeechDriverMD1 == NULL) {
            mSpeechDriverMD1 = new SpeechDriverOpenDSP(modem_index);
        }
        pSpeechDriver = mSpeechDriverMD1;
        break;
    case MODEM_2:
        if (mSpeechDriverMD2 == NULL) {
            mSpeechDriverMD2 = new SpeechDriverOpenDSP(modem_index);
        }
        pSpeechDriver = mSpeechDriverMD2;
        break;
    case MODEM_EXTERNAL:
        if (mSpeechDriverMDExt == NULL) {
            mSpeechDriverMDExt = new SpeechDriverOpenDSP(modem_index);
        }
        pSpeechDriver = mSpeechDriverMDExt;
        break;
    default:
        ALOGE("%s: no such modem_index %d", __FUNCTION__, modem_index);
        break;
    }

    ASSERT(pSpeechDriver != NULL);
    return pSpeechDriver;
}


/*==============================================================================
 *                     Constructor / Destructor / Init / Deinit
 *============================================================================*/

SpeechDriverOpenDSP::SpeechDriverOpenDSP(modem_index_t modem_index) :
    pSpeechDriverInternal(NULL),
    pIPI(NULL) {
    char property_value[PROPERTY_VALUE_MAX];

    ALOGD("%s(), modem_index = %d", __FUNCTION__, modem_index);

    mModemIndex = modem_index;

    // control internal modem & FD216
#if defined(MTK_COMBO_MODEM_SUPPORT)
    pSpeechDriverInternal = SpeechDriverNormal::GetInstance(modem_index);
#else
    pSpeechDriverInternal = SpeechDriverLAD::GetInstance(modem_index);
#endif

    pIPI = AudioMessengerIPI::getInstance();


#ifndef SPH_SR32K /* non-SWB */
    mModemDspSampleRate = 16000;
#elif defined(SPH_SR48K) /* SWB */
    mModemDspSampleRate = (mModemIndex == MODEM_1) ? 48000 : 16000;
#else /* SWB */
    mModemDspSampleRate = (mModemIndex == MODEM_1) ? 32000 : 16000;
#endif
    mModemPcmSampleRate = mModemDspSampleRate;


    if (AudioALSAHardwareResourceManager::getInstance()->getNumPhoneMicSupport() == 3) {
        if (mModemIndex == MODEM_1) { /* only modem 1 support 3-mic */
            mModemDspNumChannels = 3;
            mModemDspChannelMask = AUDIO_CHANNEL_IN_3MIC;
        } else {
            mModemDspNumChannels = 2;
            mModemDspChannelMask = AUDIO_CHANNEL_IN_STEREO;
        }
    } else {
        ASSERT(AudioALSAHardwareResourceManager::getInstance()->getNumPhoneMicSupport() == 2);
        mModemDspNumChannels = 2;
        mModemDspChannelMask = AUDIO_CHANNEL_IN_STEREO;
    }

    ALOGD("mModemDspSampleRate = %u, mModemDspNumChannels = %d, mModemDspChannelMask = 0x%x",
          mModemDspSampleRate, mModemDspNumChannels, mModemDspChannelMask);

    mArsiTaskConfig = (arsi_task_config_t *)malloc(sizeof(arsi_task_config_t));
    ASSERT(mArsiTaskConfig != NULL);
    memset(mArsiTaskConfig, 0, sizeof(arsi_task_config_t));

    InitArsiTaskConfig();

    mArsiLibConfig = (arsi_lib_config_t *)malloc(sizeof(arsi_lib_config_t));
    ASSERT(mArsiLibConfig != NULL);
    memset(mArsiLibConfig, 0, sizeof(arsi_lib_config_t));

    init_lib_config();

    mParamBuf = (data_buf_t *)malloc(sizeof(data_buf_t));
    ASSERT(mParamBuf != NULL);
    memset(mParamBuf, 0, sizeof(data_buf_t));

    mParamBuf->memory_size = 8192;
    mParamBuf->data_size = 0;
    mParamBuf->p_buffer = malloc(mParamBuf->memory_size);
    ASSERT(mParamBuf->p_buffer != NULL);




    char *dlopen_lib_path = NULL;

    // TODO: move to aurisys lib manager
    if (gAurisysLib == NULL) {
        memset(&gLibAPI, 0, sizeof(AurisysLibInterface));

        /* get dlopen_lib_path */
        if (access(AURISYS_PHONE_VENDOR_LIB_PATH, R_OK) == 0) {
            dlopen_lib_path = AURISYS_PHONE_VENDOR_LIB_PATH;
        } else if (access(AURISYS_PHONE_SYSTEM_LIB_PATH, R_OK) == 0) {
            dlopen_lib_path = AURISYS_PHONE_SYSTEM_LIB_PATH;
        } else {
            ALOGE("%s(), dlopen_lib_path not found!!", __FUNCTION__);
            ASSERT(dlopen_lib_path != NULL);
        }

        gAurisysLib = dlopen(dlopen_lib_path, RTLD_NOW);
        if (gAurisysLib == NULL) {
            ALOGE("dlopen(%s) fail!!", dlopen_lib_path);
            ASSERT(gAurisysLib != NULL);
        } else {
            dynamic_link_arsi_assign_lib_fp =
                (dynamic_link_arsi_assign_lib_fp_t)dlsym(gAurisysLib, "dynamic_link_arsi_assign_lib_fp");
            if (dynamic_link_arsi_assign_lib_fp == NULL) {
                ALOGE("%s dynamic_link_arsi_assign_lib_fp err", __FUNCTION__);
                ASSERT(dynamic_link_arsi_assign_lib_fp != NULL);
            }
        }

        if (dynamic_link_arsi_assign_lib_fp != NULL) {
            dynamic_link_arsi_assign_lib_fp(&gLibAPI);

            string_buf_t lib_version;
            lib_version.memory_size = 128;
            lib_version.string_size = 0;
            lib_version.p_string = (char *)malloc(lib_version.memory_size);
            memset(lib_version.p_string, 0, sizeof(lib_version.p_string));
            if (gLibAPI.arsi_get_lib_version != NULL) {
                gLibAPI.arsi_get_lib_version(&lib_version);
                ALOGD("lib_version: \"%s\"", lib_version.p_string);
            } else {
                ALOGW("unknown lib_version");
            }

            free(lib_version.p_string);
        }

        gEnhMode = 0;
        gRetvalSetParameter = -1;


        /* get param_file_path */
        char *param_file_path = NULL;
        if (access(DEFAULT_VENDOR_PARAM_PATH, R_OK) == 0) {
            param_file_path = DEFAULT_VENDOR_PARAM_PATH;
        } else if (access(DEFAULT_SYSTEM_PARAM_PATH, R_OK) == 0) {
            param_file_path = DEFAULT_SYSTEM_PARAM_PATH;
        } else {
            ALOGE("%s(), param_file_path not found!!", __FUNCTION__);
            ASSERT(param_file_path != NULL);
        }

        property_get(PROPERTY_KEY_PARAM_PATH, gEnhParamFilePath, param_file_path);
        ALOGV("%s(), gEnhParamFilePath = %s", __FUNCTION__, gEnhParamFilePath);


        //property_get(PROPERTY_KEY_PRODUCT_NAME, gPhoneProductName, DEFAULT_PRODUCT_NAME);
        //ALOGV("%s(), gPhoneProductName = %s", __FUNCTION__, gPhoneProductName);
        audio_strncpy(gPhoneProductName, DEFAULT_PRODUCT_NAME, 256);

        /* get pcm dump state */
        property_get(PROPERTY_KEY_PCM_DUMP_ON, property_value, "0");  //"0": default off
        gEnableDump = (atoi(property_value) == 0) ? false : true;
        ALOGD("gEnableDump = %d", gEnableDump);
    }

    /* check function pointer is valid */
    ASSERT(gLibAPI.arsi_query_param_buf_size != NULL);
    ASSERT(gLibAPI.arsi_parsing_param_file != NULL);


    /* get scp function state */
    property_get(PROPERTY_KEY_SCP_CALL_STATE[mModemIndex], property_value, "0");  //"0": default off
    mScpSideSpeechStatus = atoi(property_value);

    /* recovery scp state for mediaserver die */
    RecoverModemSideStatusToInitState();
}


SpeechDriverOpenDSP::~SpeechDriverOpenDSP() {
    ALOGD("%s()", __FUNCTION__);

    pSpeechDriverInternal = NULL;
    if (mParamBuf->p_buffer != NULL) {
        free(mParamBuf->p_buffer);
    }

    if (gAurisysLib != NULL) {
        dlclose(gAurisysLib);
        gAurisysLib = NULL;
    }

    if (mArsiTaskConfig != NULL) {
        free(mArsiTaskConfig);
        mArsiTaskConfig = NULL;
    }

    if (mArsiLibConfig != NULL) {
        free(mArsiLibConfig);
        mArsiLibConfig = NULL;
    }

    if (mParamBuf != NULL) {
        free(mParamBuf);
        mParamBuf = NULL;
    }
}


/*==============================================================================
 *                     Lib Related
 *============================================================================*/

static void myprint(const char *message, ...) {
    if (gEnableLibLogHAL) {
        static char printf_msg[256];

        va_list args;
        va_start(args, message);

        vsnprintf(printf_msg, sizeof(printf_msg), message, args);
        ALOGD("%s", printf_msg);

        va_end(args);
    }
}


int SpeechDriverOpenDSP::GetRetvalSetParameter() {
    int retval = gRetvalSetParameter;
    gRetvalSetParameter = -1;
    return retval;
}


status_t SpeechDriverOpenDSP::SetParameter(const char *keyValuePair) {
    ipi_msg_t ipi_msg;
    status_t retval = NO_ERROR;

    char aurisys_param_file[] = "AURISYS_SET_PARAM,DSP,PHONE_CALL,FVSAM,PARAM_FILE";
    char aurisys_apply_param[] = "AURISYS_SET_PARAM,DSP,PHONE_CALL,FVSAM,APPLY_PARAM";
    char aurisys_addr_value[] = "AURISYS_SET_PARAM,DSP,PHONE_CALL,FVSAM,ADDR_VALUE";
    char aurisys_key_value[] = "AURISYS_SET_PARAM,DSP,PHONE_CALL,FVSAM,KEY_VALUE";
    char aurisys_key_dump[] = "AURISYS_SET_PARAM,DSP,PHONE_CALL,FVSAM,ENABLE_DUMP";
    char aurisys_key_log_dsp[] = "AURISYS_SET_PARAM,DSP,PHONE_CALL,FVSAM,ENABLE_LOG";
    char aurisys_key_log_hal[] = "AURISYS_SET_PARAM,HAL,PHONE_CALL,FVSAM,ENABLE_LOG";

    const int max_parse_len = 256;
    char parse_str[max_parse_len];

    if (strncmp(aurisys_param_file, keyValuePair, strlen(aurisys_param_file)) == 0) {
        strncpy(parse_str, keyValuePair + strlen(aurisys_param_file) + 1, max_parse_len - 1); // -1: for '\0'
        parse_str[strstr(parse_str, "=") - parse_str] = '\0';

        ALOGV("param_file = %s", parse_str);

        SetParamFilePath(parse_str);
        gRetvalSetParameter = 1;

        return NO_ERROR;
    }

    if (strncmp(aurisys_apply_param, keyValuePair, strlen(aurisys_apply_param)) == 0) {
        strncpy(parse_str, keyValuePair + strlen(aurisys_apply_param) + 1, max_parse_len - 1); // -1: for '\0'
        parse_str[strstr(parse_str, "=") - parse_str] = '\0';

        int enh_mode = 0;
        sscanf(parse_str, "%d", &enh_mode);
        ALOGV("enh mode = %d", enh_mode);

        status_t lib_ret = SetArsiSpeechParam(enh_mode);
        gRetvalSetParameter = (lib_ret == NO_ERROR) ? 1 : 0;

        return NO_ERROR;
    }

    if (strncmp(aurisys_addr_value, keyValuePair, strlen(aurisys_addr_value)) == 0) {
        strncpy(parse_str, keyValuePair + strlen(aurisys_addr_value) + 1, max_parse_len - 1); // -1: for '\0'
        parse_str[strstr(parse_str, "=") - parse_str] = '\0';

        uint32_t aurisys_addr = 0;
        uint32_t aurisys_value = 0;
        sscanf(parse_str, "%x,%x", &aurisys_addr, &aurisys_value);
        ALOGD("addr = 0x%x, value = 0x%x", aurisys_addr, aurisys_value);

        retval = pIPI->sendIpiMsg(&ipi_msg,
                                  TASK_SCENE_PHONE_CALL, AUDIO_IPI_LAYER_HAL_TO_SCP,
                                  AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_NEED_ACK,
                                  IPI_MSG_A2D_SET_ADDR_VALUE, aurisys_addr, aurisys_value,
                                  NULL);
        if (retval != NO_ERROR) {
            ALOGW("IPI_MSG_A2D_SET_ADDR_VALUE fail");
            gRetvalSetParameter = 0;
        } else {
            ALOGD("return %d", ipi_msg.param1);
            gRetvalSetParameter = (ipi_msg.param1 == 1) ? 1 : 0;
        }
        return NO_ERROR;
    }

    if (strncmp(aurisys_key_value, keyValuePair, strlen(aurisys_key_value)) == 0) {
        strncpy(parse_str, keyValuePair + strlen(aurisys_key_value) + 1, max_parse_len - 1); // -1: for '\0'
        parse_str[strstr(parse_str, "=") - parse_str] = '\0';
        parse_str[strstr(parse_str, ",") - parse_str] = '=';

        ALOGD("key_value = %s", parse_str);

        retval = pIPI->sendIpiMsg(&ipi_msg,
                                  TASK_SCENE_PHONE_CALL, AUDIO_IPI_LAYER_HAL_TO_SCP,
                                  AUDIO_IPI_DMA, AUDIO_IPI_MSG_NEED_ACK,
                                  IPI_MSG_A2D_SET_KEY_VALUE, strlen(parse_str) + 1, strlen(parse_str) + 1,
                                  parse_str);

        if (retval != NO_ERROR) {
            ALOGW("IPI_MSG_A2D_SET_KEY_VALUE fail");
            gRetvalSetParameter = 0;
        } else {
            ALOGD("return %d", ipi_msg.param1);
            gRetvalSetParameter = (ipi_msg.param1 == 1) ? 1 : 0;
        }
        return NO_ERROR;
    }

    if (strncmp(aurisys_key_dump, keyValuePair, strlen(aurisys_key_dump)) == 0) {
        strncpy(parse_str, keyValuePair + strlen(aurisys_key_dump) + 1, max_parse_len - 1); // -1: for '\0'
        parse_str[strstr(parse_str, "=") - parse_str] = '\0';

        int enable_dump = 0;
        sscanf(parse_str, "%d", &enable_dump);
        gEnableDump = (enable_dump == 0) ? false : true;
        ALOGD("gEnableDump = %d", gEnableDump);
        property_set(PROPERTY_KEY_PCM_DUMP_ON, (gEnableDump == false) ? "0" : "1");

        pIPI->configDumpPcmEnable(gEnableDump);
        retval = pIPI->sendIpiMsg(&ipi_msg,
                                  TASK_SCENE_PHONE_CALL, AUDIO_IPI_LAYER_HAL_TO_SCP,
                                  AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_BYPASS_ACK,
                                  IPI_MSG_A2D_PCM_DUMP_ON, gEnableDump, 0,
                                  NULL);

        gRetvalSetParameter = 1;
        return NO_ERROR;
    }


    if (strncmp(aurisys_key_log_dsp, keyValuePair, strlen(aurisys_key_log_dsp)) == 0) {
        strncpy(parse_str, keyValuePair + strlen(aurisys_key_log_dsp) + 1, max_parse_len - 1); // -1: for '\0'
        parse_str[strstr(parse_str, "=") - parse_str] = '\0';

        int enable_log = 0;
        sscanf(parse_str, "%d", &enable_log);
        ALOGV("enh mode = %d", enable_log);

        retval = pIPI->sendIpiMsg(&ipi_msg,
                                  TASK_SCENE_PHONE_CALL, AUDIO_IPI_LAYER_HAL_TO_SCP,
                                  AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_BYPASS_ACK,
                                  IPI_MSG_A2D_LIB_LOG_ON, enable_log, 0,
                                  NULL);

        gRetvalSetParameter = 1;
        return NO_ERROR;
    }

    if (strncmp(aurisys_key_log_hal, keyValuePair, strlen(aurisys_key_log_hal)) == 0) {
        strncpy(parse_str, keyValuePair + strlen(aurisys_key_log_hal) + 1, max_parse_len - 1); // -1: for '\0'
        parse_str[strstr(parse_str, "=") - parse_str] = '\0';

        int enable_lib_log_hal = 0;
        sscanf(parse_str, "%d", &enable_lib_log_hal);
        gEnableLibLogHAL = (enable_lib_log_hal == 0) ? false : true;
        ALOGD("gEnableLibLogHAL = %d", gEnableLibLogHAL);

        gRetvalSetParameter = 1;
        return NO_ERROR;
    }

    return INVALID_OPERATION;
}


char *SpeechDriverOpenDSP::GetParameter(const char *key) {
    ipi_msg_t ipi_msg;
    status_t retval = NO_ERROR;

    char aurisys_param_file[] = "AURISYS_GET_PARAM,DSP,PHONE_CALL,FVSAM,PARAM_FILE";
    char aurisys_addr_value[] = "AURISYS_GET_PARAM,DSP,PHONE_CALL,FVSAM,ADDR_VALUE";
    char aurisys_key_value[] = "AURISYS_GET_PARAM,DSP,PHONE_CALL,FVSAM,KEY_VALUE";

    const int max_parse_len = 256;
    char parse_str[max_parse_len];

    if (strncmp(aurisys_param_file, key, strlen(aurisys_param_file)) == 0) {
        return GetParamFilePath();
    }

    if (strncmp(aurisys_addr_value, key, strlen(aurisys_addr_value)) == 0) {
        strncpy(parse_str, key + strlen(aurisys_addr_value) + 1, max_parse_len - 1); // -1: for '\0'

        uint32_t aurisys_addr = 0;
        sscanf(parse_str, "%x", &aurisys_addr);

        ALOGD("addr = 0x%x", aurisys_addr);
        retval = pIPI->sendIpiMsg(&ipi_msg,
                                  TASK_SCENE_PHONE_CALL, AUDIO_IPI_LAYER_HAL_TO_SCP,
                                  AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_NEED_ACK,
                                  IPI_MSG_A2D_GET_ADDR_VALUE, aurisys_addr, 0,
                                  NULL);

        if (retval != NO_ERROR) {
            ALOGW("IPI_MSG_A2D_GET_ADDR_VALUE fail");
        } else {
            ALOGD("param1 0x%x, param2 0x%x", ipi_msg.param1, ipi_msg.param2);
        }

        if (ipi_msg.param1 == 1) {
            snprintf(svalue, 256 - 1, "0x%x", ipi_msg.param2); // -1: for '\0'
        } else {
            strncpy(svalue, "GET_FAIL", 256 - 1); // -1: for '\0'
        }
        return svalue;
    }

    if (strncmp(aurisys_key_value, key, strlen(aurisys_key_value)) == 0) {
        strncpy(parse_str, key + strlen(aurisys_key_value) + 1, max_parse_len - 1); // -1: for '\0'
        ALOGD("key = %s", parse_str);

        ipi_msg_t ipi_msg;
        retval = pIPI->sendIpiMsg(&ipi_msg,
                                  TASK_SCENE_PHONE_CALL, AUDIO_IPI_LAYER_HAL_TO_SCP,
                                  AUDIO_IPI_DMA, AUDIO_IPI_MSG_NEED_ACK,
                                  IPI_MSG_A2D_GET_KEY_VALUE, strlen(parse_str) + 1, max_parse_len,
                                  parse_str);

        if (retval != NO_ERROR) {
            ALOGW("IPI_MSG_A2D_GET_KEY_VALUE fail");
        } else {
            ALOGD("param1 0x%x, param2 0x%x", ipi_msg.param1, ipi_msg.param2);
        }
        ALOGD("key_value = %s", parse_str);

        char *p_eq = strstr(parse_str, "=");
        if (ipi_msg.param1 == 1 &&
            p_eq != NULL && p_eq < parse_str + max_parse_len - 1) {
            strncpy(svalue, strstr(parse_str, "=") + 1, 256 - 1); // -1: for '\0'
        } else {
            strncpy(svalue, "GET_FAIL", 256 - 1); // -1: for '\0'
        }
        return svalue;
    }

    return "GET_FAIL";
}


status_t SpeechDriverOpenDSP::SetParamFilePath(const char *file_path) {
    ALOGD("file_path %s => %s", gEnhParamFilePath, file_path);
    strncpy(gEnhParamFilePath, file_path, 256 - 1); // -1: for '\0'

    property_set(PROPERTY_KEY_PARAM_PATH, file_path);
    return NO_ERROR;
}

char *SpeechDriverOpenDSP::GetParamFilePath() {
    ALOGD("file_path %s", gEnhParamFilePath);
    return gEnhParamFilePath;
}


void SpeechDriverOpenDSP::InitArsiTaskConfig() {
    /* input device */
    mArsiTaskConfig->input_device_info.devices = AUDIO_DEVICE_IN_BUILTIN_MIC;
    mArsiTaskConfig->input_device_info.audio_format = AUDIO_FORMAT_PCM_16_BIT;
    mArsiTaskConfig->input_device_info.sample_rate = mModemDspSampleRate;
    mArsiTaskConfig->input_device_info.channel_mask = mModemDspChannelMask;
    mArsiTaskConfig->input_device_info.num_channels = mModemDspNumChannels;
    mArsiTaskConfig->input_device_info.hw_info_mask = 0;

    /* output device */
    mArsiTaskConfig->output_device_info.devices = AUDIO_DEVICE_OUT_EARPIECE;
    mArsiTaskConfig->output_device_info.audio_format = AUDIO_FORMAT_PCM_16_BIT;
    mArsiTaskConfig->output_device_info.sample_rate = mModemDspSampleRate;
    mArsiTaskConfig->output_device_info.channel_mask = AUDIO_CHANNEL_OUT_MONO;
    mArsiTaskConfig->output_device_info.num_channels = 1;
    mArsiTaskConfig->output_device_info.hw_info_mask = 0;

    /* task scene */
    mArsiTaskConfig->task_scene = TASK_SCENE_PHONE_CALL;

    /* audio mode */
    mArsiTaskConfig->audio_mode = AUDIO_MODE_IN_CALL;

    /* max device capability for allocating memory */
    mArsiTaskConfig->max_input_device_sample_rate  = mModemDspSampleRate;
    mArsiTaskConfig->max_output_device_sample_rate = mModemDspSampleRate;

    mArsiTaskConfig->max_input_device_num_channels  = mModemDspNumChannels;
    mArsiTaskConfig->max_output_device_num_channels = 1; // phone call mono DL

    /* max device capability for allocating memory */
    mArsiTaskConfig->input_source = AUDIO_SOURCE_DEFAULT;
}


void SpeechDriverOpenDSP::init_lib_config() {
    /* alloc buffer in SCP */
    mArsiLibConfig->p_ul_buf_in = NULL;
    mArsiLibConfig->p_ul_buf_out = NULL;
    mArsiLibConfig->p_ul_ref_bufs = NULL;

    mArsiLibConfig->p_dl_buf_in = NULL;
    mArsiLibConfig->p_dl_buf_out = NULL;
    mArsiLibConfig->p_dl_ref_bufs = NULL;

    /* lib */
    mArsiLibConfig->sample_rate = mModemDspSampleRate;
    mArsiLibConfig->audio_format = AUDIO_FORMAT_PCM_16_BIT;
    mArsiLibConfig->frame_size_ms = 20;
    mArsiLibConfig->b_interleave = 0;
}


/*==============================================================================
 *                     Speech Control
 *============================================================================*/

static void putTimestampAndEnableToParam(uint32_t *param, const bool enable) {
    char time_h[3];
    char time_m[3];
    char time_s[3];

    uint8_t time_value_h;
    uint8_t time_value_m;
    uint8_t time_value_s;

    time_t rawtime;
    time(&rawtime);

    struct tm *timeinfo = localtime(&rawtime);
    strftime(time_h, 3, "%H", timeinfo);
    strftime(time_m, 3, "%M", timeinfo);
    strftime(time_s, 3, "%S", timeinfo);

    time_value_h = (uint8_t)atoi(time_h);
    time_value_m = (uint8_t)atoi(time_m);
    time_value_s = (uint8_t)atoi(time_s);

    /* param[24:31] => hour
     * param[16:23] => minute
     * param[8:15]  => second
     * param[0:7]   => enable */
    *param = (time_value_h << 24) |
             (time_value_m << 16) |
             (time_value_s << 8)  |
             (enable);
}


static void printTimeFromParam(const uint32_t param) {
    uint8_t time_value_h = (param & 0xFF000000) >> 24;
    uint8_t time_value_m = (param & 0x00FF0000) >> 16;
    uint8_t time_value_s = (param & 0x0000FF00) >> 8;

    ALOGD("HAL Time %02d:%02d:%02d", time_value_h, time_value_m, time_value_s);
}


void SpeechDriverOpenDSP::SetArsiTaskConfigByDevice(
    const audio_devices_t input_device,
    const audio_devices_t output_devices) {
    ipi_msg_t ipi_msg;
    status_t retval = NO_ERROR;


    /* set input device */
    switch (input_device) {
    case AUDIO_DEVICE_IN_BUILTIN_MIC: {
        mArsiTaskConfig->input_device_info.devices = input_device;
        mArsiTaskConfig->input_device_info.audio_format = AUDIO_FORMAT_PCM_16_BIT;
        mArsiTaskConfig->input_device_info.sample_rate = mModemDspSampleRate;
        mArsiTaskConfig->input_device_info.channel_mask = mModemDspChannelMask;
        mArsiTaskConfig->input_device_info.num_channels = mModemDspNumChannels;
        mArsiTaskConfig->input_device_info.hw_info_mask = 0;
        break;
    }
    case AUDIO_DEVICE_IN_BACK_MIC: {
        mArsiTaskConfig->input_device_info.devices = input_device;
        mArsiTaskConfig->input_device_info.audio_format = AUDIO_FORMAT_PCM_16_BIT;
        mArsiTaskConfig->input_device_info.sample_rate = mModemDspSampleRate;
        mArsiTaskConfig->input_device_info.channel_mask = mModemDspChannelMask;
        mArsiTaskConfig->input_device_info.num_channels = mModemDspNumChannels;
        mArsiTaskConfig->input_device_info.hw_info_mask = 0;
        break;
    }
    case AUDIO_DEVICE_IN_WIRED_HEADSET: {
        mArsiTaskConfig->input_device_info.devices = input_device;
        mArsiTaskConfig->input_device_info.audio_format = AUDIO_FORMAT_PCM_16_BIT;
        mArsiTaskConfig->input_device_info.sample_rate = mModemDspSampleRate;
        mArsiTaskConfig->input_device_info.channel_mask = AUDIO_CHANNEL_IN_MONO;
        mArsiTaskConfig->input_device_info.num_channels = 1;
        mArsiTaskConfig->input_device_info.hw_info_mask = 0;
        break;
    }
    case AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET: {
        mArsiTaskConfig->input_device_info.devices = input_device;
        mArsiTaskConfig->input_device_info.audio_format = AUDIO_FORMAT_PCM_16_BIT;
        mArsiTaskConfig->input_device_info.sample_rate = mModemPcmSampleRate; /* NB/WB only */
        mArsiTaskConfig->input_device_info.channel_mask = AUDIO_CHANNEL_IN_MONO;
        mArsiTaskConfig->input_device_info.num_channels = 1;
        mArsiTaskConfig->input_device_info.hw_info_mask = 0;
        break;
    }
    case AUDIO_DEVICE_IN_USB_DEVICE: {
        mArsiTaskConfig->input_device_info.devices = input_device;
        mArsiTaskConfig->input_device_info.audio_format = AUDIO_FORMAT_PCM_16_BIT;
        mArsiTaskConfig->input_device_info.sample_rate = mModemDspSampleRate;
        mArsiTaskConfig->input_device_info.channel_mask = AUDIO_CHANNEL_IN_MONO;
        mArsiTaskConfig->input_device_info.num_channels = 1;
        mArsiTaskConfig->input_device_info.hw_info_mask = 0;
        break;
    }
    default: {
        ALOGE("Not support input_device 0x%x", input_device);
        mArsiTaskConfig->input_device_info.devices = AUDIO_DEVICE_IN_BUILTIN_MIC;
        mArsiTaskConfig->input_device_info.audio_format = AUDIO_FORMAT_PCM_16_BIT;
        mArsiTaskConfig->input_device_info.sample_rate = mModemDspSampleRate;
        mArsiTaskConfig->input_device_info.channel_mask = mModemDspChannelMask;
        mArsiTaskConfig->input_device_info.num_channels = mModemDspNumChannels;
        mArsiTaskConfig->input_device_info.hw_info_mask = 0;
        break;
    }
    }


    /* output device */
    switch (output_devices) {
    case AUDIO_DEVICE_OUT_EARPIECE: {
        mArsiTaskConfig->output_device_info.devices = output_devices;
        mArsiTaskConfig->output_device_info.audio_format = AUDIO_FORMAT_PCM_16_BIT;
        mArsiTaskConfig->output_device_info.sample_rate = mModemDspSampleRate;
        mArsiTaskConfig->output_device_info.channel_mask = AUDIO_CHANNEL_OUT_MONO;
        mArsiTaskConfig->output_device_info.num_channels = 1;
        mArsiTaskConfig->output_device_info.hw_info_mask = 0;
        break;
    }
    case AUDIO_DEVICE_OUT_SPEAKER: {
        mArsiTaskConfig->output_device_info.devices = output_devices;
        mArsiTaskConfig->output_device_info.audio_format = AUDIO_FORMAT_PCM_16_BIT;
        mArsiTaskConfig->output_device_info.sample_rate = mModemDspSampleRate;
        mArsiTaskConfig->output_device_info.channel_mask = AUDIO_CHANNEL_OUT_MONO;
        mArsiTaskConfig->output_device_info.num_channels = 1;
        mArsiTaskConfig->output_device_info.hw_info_mask = 0;
        break;
    }
    case AUDIO_DEVICE_OUT_WIRED_HEADSET: {
        mArsiTaskConfig->output_device_info.devices = output_devices;
        mArsiTaskConfig->output_device_info.audio_format = AUDIO_FORMAT_PCM_16_BIT;
        mArsiTaskConfig->output_device_info.sample_rate = mModemDspSampleRate;
        mArsiTaskConfig->output_device_info.channel_mask = AUDIO_CHANNEL_OUT_MONO;
        mArsiTaskConfig->output_device_info.num_channels = 1;
        mArsiTaskConfig->output_device_info.hw_info_mask = 0;
        break;
    }
    case AUDIO_DEVICE_OUT_WIRED_HEADPHONE: {
        mArsiTaskConfig->output_device_info.devices = output_devices;
        mArsiTaskConfig->output_device_info.audio_format = AUDIO_FORMAT_PCM_16_BIT;
        mArsiTaskConfig->output_device_info.sample_rate = mModemDspSampleRate;
        mArsiTaskConfig->output_device_info.channel_mask = AUDIO_CHANNEL_OUT_MONO;
        mArsiTaskConfig->output_device_info.num_channels = 1;
        mArsiTaskConfig->output_device_info.hw_info_mask = 0;
        break;
    }
    case AUDIO_DEVICE_OUT_BLUETOOTH_SCO:
    case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET:
    case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT: {
        mArsiTaskConfig->output_device_info.devices = output_devices;
        mArsiTaskConfig->output_device_info.audio_format = AUDIO_FORMAT_PCM_16_BIT;
        mArsiTaskConfig->output_device_info.sample_rate = mModemPcmSampleRate; /* NB/WB only */
        mArsiTaskConfig->output_device_info.channel_mask = AUDIO_CHANNEL_OUT_MONO;
        mArsiTaskConfig->output_device_info.num_channels = 1;
        mArsiTaskConfig->output_device_info.hw_info_mask = 0;
        break;
    }
    case AUDIO_DEVICE_OUT_USB_DEVICE: {
        mArsiTaskConfig->output_device_info.devices = output_devices;
        mArsiTaskConfig->output_device_info.audio_format = AUDIO_FORMAT_PCM_16_BIT;
        mArsiTaskConfig->output_device_info.sample_rate = mModemDspSampleRate;
        mArsiTaskConfig->output_device_info.channel_mask = AUDIO_CHANNEL_OUT_MONO;
        mArsiTaskConfig->output_device_info.num_channels = 1;
        mArsiTaskConfig->output_device_info.hw_info_mask = 0;
        break;
    }
    default: {
        ALOGE("Not support output_devices 0x%x", output_devices);
        mArsiTaskConfig->output_device_info.devices = AUDIO_DEVICE_OUT_EARPIECE;
        mArsiTaskConfig->output_device_info.audio_format = AUDIO_FORMAT_PCM_16_BIT;
        mArsiTaskConfig->output_device_info.sample_rate = mModemDspSampleRate;
        mArsiTaskConfig->output_device_info.channel_mask = AUDIO_CHANNEL_OUT_MONO;
        mArsiTaskConfig->output_device_info.num_channels = 1;
        mArsiTaskConfig->output_device_info.hw_info_mask = 0;
        break;
    }
    }

    /* set task config to SCP */
    dump_task_config(mArsiTaskConfig);
    dump_lib_config(mArsiLibConfig);

    retval = pIPI->sendIpiMsg(&ipi_msg,
                              TASK_SCENE_PHONE_CALL, AUDIO_IPI_LAYER_HAL_TO_SCP,
                              AUDIO_IPI_DMA, AUDIO_IPI_MSG_NEED_ACK,
                              IPI_MSG_A2D_TASK_CFG, sizeof(arsi_task_config_t), 0,
                              (char *)mArsiTaskConfig);

    retval = pIPI->sendIpiMsg(&ipi_msg,
                              TASK_SCENE_PHONE_CALL, AUDIO_IPI_LAYER_HAL_TO_SCP,
                              AUDIO_IPI_DMA, AUDIO_IPI_MSG_NEED_ACK,
                              IPI_MSG_A2D_LIB_CFG, sizeof(arsi_lib_config_t), 0,
                              (char *)mArsiLibConfig);
}


#ifdef OPENDSP_DUMP_PARAM
static void dump_param(uint32_t data_size, void *param) {
    int i = 0;
    uint16_t *buf = (uint16_t *)param;
    ALOGD("param data_size: %d, param: 0x%x, buf: 0x%x\n",
          data_size,
          param,
          buf);

    for (i = 0; i < (data_size / sizeof(uint16_t)); i++) {
        ALOGD("%d: %x \n", i, *buf);
        buf++;
    }
}
#endif


status_t SpeechDriverOpenDSP::SetArsiSpeechParam(const int enh_mode) {
    ipi_msg_t ipi_msg;
    status_t retval = NO_ERROR;

    status_t ret;
    uint32_t param_buf_size = 0;


    gEnhMode = enh_mode;
    ALOGD("gEnhMode = %d", gEnhMode);


    string_buf_t platform_info;
    platform_info.memory_size = strlen(gPhoneProductName) + 1;
    platform_info.string_size = strlen(gPhoneProductName);
    platform_info.p_string = gPhoneProductName;
    ALOGD("platform_info.p_string = %s", platform_info.p_string);


    string_buf_t file_path;
    file_path.memory_size = strlen(gEnhParamFilePath) + 1;
    file_path.string_size = strlen(gEnhParamFilePath);
    file_path.p_string = gEnhParamFilePath;
    ALOGD("file_path.p_string = %s", file_path.p_string);

    if (gLibAPI.arsi_query_param_buf_size == NULL) {
        ALOGE("%s arsi_query_param_buf_size == NULL", __FUNCTION__);
        return UNKNOWN_ERROR;
    }
    ret = gLibAPI.arsi_query_param_buf_size(mArsiTaskConfig,
                                            mArsiLibConfig,
                                            &platform_info,
                                            &file_path,
                                            gEnhMode,
                                            &param_buf_size,
                                            myprint);
    ALOGD("param_buf_size = %u", param_buf_size);
    if (ret != NO_ERROR) {
        ALOGW("fvsoft_arsi_query_param_buf_size fail");
        return UNKNOWN_ERROR;
    }



    if (gLibAPI.arsi_parsing_param_file == NULL) {
        ALOGE("%s arsi_parsing_param_file == NULL", __FUNCTION__);
        return UNKNOWN_ERROR;
    }
    memset(mParamBuf->p_buffer, 0, mParamBuf->memory_size);
    ret = gLibAPI.arsi_parsing_param_file(mArsiTaskConfig,
                                          mArsiLibConfig,
                                          &platform_info,
                                          &file_path,
                                          gEnhMode,
                                          mParamBuf,
                                          myprint);
    ALOGD("mParamBuf->data_size = %u", mParamBuf->data_size);
    if (ret != NO_ERROR) {
        ALOGW("fvsoft_arsi_parsing_param_file fail");
        return UNKNOWN_ERROR;
    }


    /* set speech param to SCP */
#ifdef OPENDSP_DUMP_PARAM
    dump_param(mParamBuf->data_size, mParamBuf->p_buffer);
#endif
    retval = pIPI->sendIpiMsg(&ipi_msg,
                              TASK_SCENE_PHONE_CALL, AUDIO_IPI_LAYER_HAL_TO_SCP,
                              AUDIO_IPI_DMA, AUDIO_IPI_MSG_NEED_ACK,
                              IPI_MSG_A2D_SPH_PARAM, mParamBuf->data_size, 0,
                              (char *)mParamBuf->p_buffer);

    return NO_ERROR;
}


void SpeechDriverOpenDSP::createDumpFolder() {
    ipi_msg_t ipi_msg;
    status_t retval = NO_ERROR;

    if (gEnableDump == true) {
        int ret = AudiocheckAndCreateDirectory(DUMP_DSP_PCM_DATA_PATH);
        if (ret < 0) {
            ALOGE("AudiocheckAndCreateDirectory(%s) fail!", DUMP_DSP_PCM_DATA_PATH);
            gEnableDump = 0;
            pIPI->configDumpPcmEnable(false);
            retval = pIPI->sendIpiMsg(&ipi_msg,
                                      TASK_SCENE_PHONE_CALL, AUDIO_IPI_LAYER_HAL_TO_SCP,
                                      AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_BYPASS_ACK,
                                      IPI_MSG_A2D_PCM_DUMP_ON, false, 0,
                                      NULL);
        }
    }
}


status_t SpeechDriverOpenDSP::ScpSpeechOn() {
    ipi_msg_t ipi_msg;
    status_t retval = NO_ERROR;
    uint32_t param1 = 0;

    ALOGD("%s(+)", __FUNCTION__);

    putTimestampAndEnableToParam(&param1, true);
    printTimeFromParam(param1);

    createDumpFolder();

    pIPI->configDumpPcmEnable(gEnableDump);
    pIPI->registerScpFeature(true);
    pIPI->setSpmApMdSrcReq(true);

    retval = pIPI->sendIpiMsg(&ipi_msg,
                              TASK_SCENE_PHONE_CALL, AUDIO_IPI_LAYER_HAL_TO_SCP,
                              AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_BYPASS_ACK,
                              IPI_MSG_A2D_PCM_DUMP_ON, gEnableDump, 0,
                              NULL);

    retval = pIPI->sendIpiMsg(&ipi_msg,
                              TASK_SCENE_PHONE_CALL, AUDIO_IPI_LAYER_HAL_TO_SCP,
                              AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_NEED_ACK,
                              IPI_MSG_A2D_SPH_ON, param1, mModemIndex,
                              NULL);

    ALOGD("%s(-)", __FUNCTION__);
    return NO_ERROR;
}


status_t SpeechDriverOpenDSP::ScpSpeechOff() {
    ipi_msg_t ipi_msg;
    status_t retval = NO_ERROR;
    uint32_t param1 = 0;

    ALOGD("%s(+)", __FUNCTION__);

    putTimestampAndEnableToParam(&param1, false);
    printTimeFromParam(param1);

    static const char PROPERTY_KEY_MODEM_STATUS[NUM_MODEM][PROPERTY_KEY_MAX] = {
        "af.modem_1.status", "af.modem_2.status", "af.modem_ext.status"
    };

    char property_value[PROPERTY_VALUE_MAX];
    property_get(PROPERTY_KEY_MODEM_STATUS[mModemIndex], property_value, "0");

    for (int i = 0; i < 50; i++) {
        if (property_value[0] == '0') {
            break;
        } else {
            ALOGW("%s(), sleep 10ms, i = %d", __FUNCTION__, i);
            usleep(10000); // sleep 10 ms
        }
    }

    retval = pIPI->sendIpiMsg(&ipi_msg,
                              TASK_SCENE_PHONE_CALL, AUDIO_IPI_LAYER_HAL_TO_SCP,
                              AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_NEED_ACK,
                              IPI_MSG_A2D_SPH_ON, param1, mModemIndex,
                              NULL);

    pIPI->setSpmApMdSrcReq(false);
    pIPI->registerScpFeature(false);

    ALOGD("%s(-)", __FUNCTION__);
    return NO_ERROR;
}


status_t SpeechDriverOpenDSP::SetSpeechMode(const audio_devices_t input_device,
                                            const audio_devices_t output_devices) {
    ALOGD("%s(+)", __FUNCTION__);

    SetArsiTaskConfigByDevice(input_device, output_devices);
    SetArsiSpeechParam(gEnhMode);

    usleep(5000); ////wait to all data is played

    pSpeechDriverInternal->SetSpeechMode(input_device, output_devices);
    ALOGD("%s(-)", __FUNCTION__);

    return NO_ERROR;
}


status_t SpeechDriverOpenDSP::setMDVolumeIndex(int stream, int device,
                                               int index) {
    return pSpeechDriverInternal->setMDVolumeIndex(stream, device, index);
}


status_t SpeechDriverOpenDSP::SpeechOn() {
    ALOGD("%s(), mModemIndex = %d", __FUNCTION__, mModemIndex);

    CheckApSideModemStatusAllOffOrDie();
    SetApSideModemStatus(SPEECH_STATUS_MASK);
    SetScpSideSpeechStatus(SPEECH_STATUS_MASK);

    ScpSpeechOn();
    pSpeechDriverInternal->SpeechOn();

    return NO_ERROR;
}

status_t SpeechDriverOpenDSP::SpeechOff() {
    ALOGD("%s(), mModemIndex = %d", __FUNCTION__, mModemIndex);

    ResetScpSideSpeechStatus(SPEECH_STATUS_MASK);
    ResetApSideModemStatus(SPEECH_STATUS_MASK);
    CheckApSideModemStatusAllOffOrDie();

    // Clean gain value and mute status
    CleanGainValueAndMuteStatus();

    pSpeechDriverInternal->SpeechOff();
    ScpSpeechOff();

    return NO_ERROR;
}

status_t SpeechDriverOpenDSP::VideoTelephonyOn() {
    ALOGD("%s()", __FUNCTION__);
    CheckApSideModemStatusAllOffOrDie();
    SetApSideModemStatus(VT_STATUS_MASK);
    SetScpSideSpeechStatus(VT_STATUS_MASK);

    ScpSpeechOn();
    pSpeechDriverInternal->VideoTelephonyOn();
    return NO_ERROR;
}

status_t SpeechDriverOpenDSP::VideoTelephonyOff() {
    ALOGD("%s()", __FUNCTION__);
    ResetScpSideSpeechStatus(VT_STATUS_MASK);
    ResetApSideModemStatus(VT_STATUS_MASK);
    CheckApSideModemStatusAllOffOrDie();

    // Clean gain value and mute status
    CleanGainValueAndMuteStatus();

    pSpeechDriverInternal->VideoTelephonyOff();
    ScpSpeechOff();

    return NO_ERROR;
}

status_t SpeechDriverOpenDSP::SpeechRouterOn() {
    return INVALID_OPERATION;
}

status_t SpeechDriverOpenDSP::SpeechRouterOff() {
    return INVALID_OPERATION;
}


/*==============================================================================
 *                     Recording Control
 *============================================================================*/

status_t SpeechDriverOpenDSP::RecordOn() {
    return pSpeechDriverInternal->RecordOn();
}

status_t SpeechDriverOpenDSP::RecordOn(record_type_t type_record) {
    return pSpeechDriverInternal->RecordOn(type_record);
}

status_t SpeechDriverOpenDSP::RecordOff() {
    return pSpeechDriverInternal->RecordOff();
}

status_t SpeechDriverOpenDSP::RecordOff(record_type_t type_record) {
    return pSpeechDriverInternal->RecordOff(type_record);
}

status_t SpeechDriverOpenDSP::SetPcmRecordType(record_type_t type_record) {
    return pSpeechDriverInternal->SetPcmRecordType(type_record);
}

status_t SpeechDriverOpenDSP::VoiceMemoRecordOn() {
    ALOGD("%s()", __FUNCTION__);
    SetApSideModemStatus(VM_RECORD_STATUS_MASK);
    return pSpeechDriverInternal->VoiceMemoRecordOn();
}

status_t SpeechDriverOpenDSP::VoiceMemoRecordOff() {
    ALOGD("%s()", __FUNCTION__);
    ResetApSideModemStatus(VM_RECORD_STATUS_MASK);
    return pSpeechDriverInternal->VoiceMemoRecordOff();
}

uint16_t SpeechDriverOpenDSP::GetRecordSampleRate() const {
    return pSpeechDriverInternal->GetRecordSampleRate();
}

uint16_t SpeechDriverOpenDSP::GetRecordChannelNumber() const {
    return pSpeechDriverInternal->GetRecordChannelNumber();
}


/*==============================================================================
 *                     Background Sound
 *============================================================================*/

status_t SpeechDriverOpenDSP::BGSoundOn() {
    return pSpeechDriverInternal->BGSoundOn();
}

status_t SpeechDriverOpenDSP::BGSoundConfig(uint8_t ul_gain, uint8_t dl_gain) {
    return pSpeechDriverInternal->BGSoundConfig(ul_gain, dl_gain);
}

status_t SpeechDriverOpenDSP::BGSoundOff() {
    return pSpeechDriverInternal->BGSoundOff();
}

/*==============================================================================
*                     PCM 2 Way
*============================================================================*/

status_t SpeechDriverOpenDSP::PCM2WayOn(const bool wideband_on) {
    return pSpeechDriverInternal->PCM2WayOn(wideband_on);
}


status_t SpeechDriverOpenDSP::PCM2WayOff() {
    return pSpeechDriverInternal->PCM2WayOff();
}



/*==============================================================================
 *                     TTY-CTM Control
 *============================================================================*/
status_t SpeechDriverOpenDSP::TtyCtmOn(tty_mode_t ttyMode) {
    ipi_msg_t ipi_msg;
    status_t retval = NO_ERROR;

    retval = pIPI->sendIpiMsg(&ipi_msg,
                              TASK_SCENE_PHONE_CALL, AUDIO_IPI_LAYER_HAL_TO_SCP,
                              AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_NEED_ACK,
                              IPI_MSG_A2D_TTY_ON, true, ttyMode,
                              NULL);

    return pSpeechDriverInternal->TtyCtmOn(ttyMode);
}

status_t SpeechDriverOpenDSP::TtyCtmOff() {
    ipi_msg_t ipi_msg;
    status_t retval = NO_ERROR;

    retval = pIPI->sendIpiMsg(&ipi_msg,
                              TASK_SCENE_PHONE_CALL, AUDIO_IPI_LAYER_HAL_TO_SCP,
                              AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_NEED_ACK,
                              IPI_MSG_A2D_TTY_ON, false, AUD_TTY_OFF,
                              NULL);

    return pSpeechDriverInternal->TtyCtmOff();
}

status_t SpeechDriverOpenDSP::TtyCtmDebugOn(bool tty_debug_flag) {
    return pSpeechDriverInternal->TtyCtmDebugOn(tty_debug_flag);
}

/*==============================================================================
 *                     RTT
 *============================================================================*/
int SpeechDriverOpenDSP::RttConfig(int rttMode) {
    return pSpeechDriverInternal->RttConfig(rttMode);
}

/*==============================================================================
 *                     Modem Audio DVT and Debug
 *============================================================================*/

status_t SpeechDriverOpenDSP::SetModemLoopbackPoint(uint16_t loopback_point) {
    ALOGD("%s(), loopback_point = %d", __FUNCTION__, loopback_point);
    return pSpeechDriverInternal->SetModemLoopbackPoint(loopback_point);
}

/*==============================================================================
 *                     Speech Encryption
 *============================================================================*/

status_t SpeechDriverOpenDSP::SetEncryption(bool encryption_on) {
    ALOGD("%s(), encryption_on = %d", __FUNCTION__, encryption_on);
    return pSpeechDriverInternal->SetEncryption(encryption_on);
}

/*==============================================================================
 *                     Acoustic Loopback
 *============================================================================*/

status_t SpeechDriverOpenDSP::SetAcousticLoopback(bool loopback_on) {
    ALOGD("%s(), loopback_on = %d", __FUNCTION__, loopback_on);

    if (loopback_on == true) {
        CheckApSideModemStatusAllOffOrDie();
        SetApSideModemStatus(LOOPBACK_STATUS_MASK);
        SetScpSideSpeechStatus(LOOPBACK_STATUS_MASK);

        ScpSpeechOn();
        pSpeechDriverInternal->SetAcousticLoopback(loopback_on);
    } else {
        ResetScpSideSpeechStatus(LOOPBACK_STATUS_MASK);
        ResetApSideModemStatus(LOOPBACK_STATUS_MASK);
        CheckApSideModemStatusAllOffOrDie();

        // Clean gain value and mute status
        CleanGainValueAndMuteStatus();

        pSpeechDriverInternal->SetAcousticLoopback(loopback_on);
        ScpSpeechOff();
    }

    return NO_ERROR;
}

status_t SpeechDriverOpenDSP::SetAcousticLoopbackBtCodec(bool enable_codec) {
    return pSpeechDriverInternal->SetAcousticLoopbackBtCodec(enable_codec);
}

status_t SpeechDriverOpenDSP::SetAcousticLoopbackDelayFrames(int32_t delay_frames) {
    return pSpeechDriverInternal->SetAcousticLoopbackDelayFrames(delay_frames);
}

status_t SpeechDriverOpenDSP::setLpbkFlag(bool enableLpbk) {
    return pSpeechDriverInternal->setLpbkFlag(enableLpbk);
}

/*==============================================================================
 *                     Volume Control
 *============================================================================*/

status_t SpeechDriverOpenDSP::SetDownlinkGain(int16_t gain) {
    ipi_msg_t ipi_msg;
    status_t retval = NO_ERROR;

    ALOGD("%s(), gain = 0x%x, old mDownlinkGain = 0x%x",
          __FUNCTION__, gain, mDownlinkGain);
    if (gain == mDownlinkGain) { return NO_ERROR; }

    mDownlinkGain = gain;
    return pIPI->sendIpiMsg(&ipi_msg,
                            TASK_SCENE_PHONE_CALL, AUDIO_IPI_LAYER_HAL_TO_SCP,
                            AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_BYPASS_ACK,
                            IPI_MSG_A2D_DL_GAIN, gain, 0/*TODO*/,
                            NULL);
}

status_t SpeechDriverOpenDSP::SetEnh1DownlinkGain(int16_t gain) {
    return INVALID_OPERATION; // not support anymore
}

status_t SpeechDriverOpenDSP::SetUplinkGain(int16_t gain) {
    ipi_msg_t ipi_msg;
    status_t retval = NO_ERROR;

    ALOGD("%s(), gain = 0x%x, old mUplinkGain = 0x%x",
          __FUNCTION__, gain, mUplinkGain);
    if (gain == mUplinkGain) { return NO_ERROR; }

    mUplinkGain = gain;
    return pIPI->sendIpiMsg(&ipi_msg,
                            TASK_SCENE_PHONE_CALL, AUDIO_IPI_LAYER_HAL_TO_SCP,
                            AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_BYPASS_ACK,
                            IPI_MSG_A2D_UL_GAIN, gain, 0/*TODO*/,
                            NULL);
}

status_t SpeechDriverOpenDSP::SetDownlinkMute(bool mute_on) {
    ipi_msg_t ipi_msg;
    status_t retval = NO_ERROR;

    ALOGD("%s(), mute_on = %d, old mDownlinkMuteOn = %d",
          __FUNCTION__, mute_on, mDownlinkMuteOn);
    if (mute_on == mDownlinkMuteOn) { return NO_ERROR; }

    mDownlinkMuteOn = mute_on;

    // mute voice dl + bgs
    retval = pIPI->sendIpiMsg(&ipi_msg,
                              TASK_SCENE_PHONE_CALL, AUDIO_IPI_LAYER_HAL_TO_SCP,
                              AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_BYPASS_ACK,
                              IPI_MSG_A2D_DL_MUTE_ON, mute_on, 0,
                              NULL);
    return pSpeechDriverInternal->SetDownlinkMute(mute_on); // for bgs
}

status_t SpeechDriverOpenDSP::SetDownlinkMuteCodec(bool mute_on) {
    ALOGD("%s(), mute_on = %d, old mDownlinkMuteOn = %d", __FUNCTION__, mute_on, mDownlinkMuteOn);
    return pSpeechDriverInternal->SetDownlinkMuteCodec(mute_on);
}

status_t SpeechDriverOpenDSP::SetUplinkMute(bool mute_on) {
    ipi_msg_t ipi_msg;
    status_t retval = NO_ERROR;

    ALOGD("%s(), mute_on = %d, old mUplinkMuteOn = %d",
          __FUNCTION__, mute_on, mUplinkMuteOn);
    if (mute_on == mUplinkMuteOn) { return NO_ERROR; }

    mUplinkMuteOn = mute_on;

    // mute voice ul + bgs
    retval = pIPI->sendIpiMsg(&ipi_msg,
                              TASK_SCENE_PHONE_CALL, AUDIO_IPI_LAYER_HAL_TO_SCP,
                              AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_BYPASS_ACK,
                              IPI_MSG_A2D_UL_MUTE_ON, mute_on, 0,
                              NULL);
    return pSpeechDriverInternal->SetUplinkMute(mute_on); // for bgs
}

status_t SpeechDriverOpenDSP::SetUplinkSourceMute(bool mute_on) {
    ipi_msg_t ipi_msg;
    status_t retval = NO_ERROR;

    ALOGD("%s(), mute_on = %d, old mUplinkSourceMuteOn = %d",
          __FUNCTION__, mute_on, mUplinkSourceMuteOn);
    if (mute_on == mUplinkSourceMuteOn) { return NO_ERROR; }

    mUplinkSourceMuteOn = mute_on;
    return pIPI->sendIpiMsg(&ipi_msg,
                            TASK_SCENE_PHONE_CALL, AUDIO_IPI_LAYER_HAL_TO_SCP,
                            AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_BYPASS_ACK,
                            IPI_MSG_A2D_UL_MUTE_ON, mute_on, 0,
                            NULL);
}

status_t SpeechDriverOpenDSP::SetSidetoneGain(int16_t gain) {
    ALOGD("%s(), gain = 0x%x, old mSideToneGain = 0x%x",
          __FUNCTION__, gain, mSideToneGain);
    if (gain == mSideToneGain) { return NO_ERROR; }

    mSideToneGain = gain;
    return pSpeechDriverInternal->SetSidetoneGain(gain);
}


/*==============================================================================
 *                     Device related Config
 *============================================================================*/

status_t SpeechDriverOpenDSP::SetModemSideSamplingRate(uint16_t sample_rate) {
    ALOGD("%s(), %u => %u", __FUNCTION__, mModemPcmSampleRate, sample_rate);
    mModemPcmSampleRate = sample_rate;
    return pSpeechDriverInternal->SetModemSideSamplingRate(sample_rate);
}


/*==============================================================================
 *                     Speech Enhancement Control
 *============================================================================*/
status_t SpeechDriverOpenDSP::SetSpeechEnhancement(bool enhance_on) {
    ipi_msg_t ipi_msg;
    status_t retval = NO_ERROR;

    retval = pIPI->sendIpiMsg(&ipi_msg,
                              TASK_SCENE_PHONE_CALL, AUDIO_IPI_LAYER_HAL_TO_SCP,
                              AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_BYPASS_ACK,
                              IPI_MSG_A2D_UL_ENHANCE_ON, enhance_on, 0,
                              NULL);
    retval = pIPI->sendIpiMsg(&ipi_msg,
                              TASK_SCENE_PHONE_CALL, AUDIO_IPI_LAYER_HAL_TO_SCP,
                              AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_BYPASS_ACK,
                              IPI_MSG_A2D_DL_ENHANCE_ON, enhance_on, 0,
                              NULL);
    return NO_ERROR;
}

status_t SpeechDriverOpenDSP::SetSpeechEnhancementMask(const
                                                       sph_enh_mask_struct_t &mask) {
    return INVALID_OPERATION; // not support anymore
}

status_t SpeechDriverOpenDSP::SetBtHeadsetNrecOn(const bool bt_headset_nrec_on) {
    ipi_msg_t ipi_msg;
    status_t retval = NO_ERROR;

    return pIPI->sendIpiMsg(&ipi_msg,
                            TASK_SCENE_PHONE_CALL, AUDIO_IPI_LAYER_HAL_TO_SCP,
                            AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_BYPASS_ACK,
                            IPI_MSG_A2D_BT_NREC_ON, bt_headset_nrec_on, 0,
                            NULL);
}


/*==============================================================================
 *                     Speech Enhancement Parameters
 *============================================================================*/

status_t SpeechDriverOpenDSP::GetVibSpkParam(void *eVibSpkParam) {
    return pSpeechDriverInternal->GetVibSpkParam(eVibSpkParam);
}

status_t SpeechDriverOpenDSP::SetVibSpkParam(void *eVibSpkParam) {
    return pSpeechDriverInternal->SetVibSpkParam(eVibSpkParam);
}

status_t SpeechDriverOpenDSP::SetDynamicSpeechParameters(const int type,
                                                         const void *param_arg) {
    return pSpeechDriverInternal->SetDynamicSpeechParameters(type, param_arg);
}


/*==============================================================================
 *                     Recover State
 *============================================================================*/

bool SpeechDriverOpenDSP::GetScpSideSpeechStatus(const modem_status_mask_t modem_status_mask) {
    AL_AUTOLOCK(mScpSideSpeechStatusLock);
    return ((mScpSideSpeechStatus & modem_status_mask) > 0);
}


void SpeechDriverOpenDSP::SetScpSideSpeechStatus(const modem_status_mask_t modem_status_mask) {
    ALOGD("%s(), modem_status_mask = 0x%x, mScpSideSpeechStatus = 0x%x",
          __FUNCTION__, modem_status_mask, mScpSideSpeechStatus);

    AL_AUTOLOCK(mScpSideSpeechStatusLock);

    ASSERT(((mScpSideSpeechStatus & modem_status_mask) > 0) == false);
    mScpSideSpeechStatus |= modem_status_mask;

    char property_value[PROPERTY_VALUE_MAX];
    sprintf(property_value, "%u", mScpSideSpeechStatus);
    property_set(PROPERTY_KEY_SCP_CALL_STATE[mModemIndex], property_value);
}


void SpeechDriverOpenDSP::ResetScpSideSpeechStatus(const modem_status_mask_t modem_status_mask) {
    ALOGD("%s(), modem_status_mask = 0x%x, mScpSideSpeechStatus = 0x%x",
          __FUNCTION__, modem_status_mask, mScpSideSpeechStatus);

    AL_AUTOLOCK(mScpSideSpeechStatusLock);

    ASSERT(((mScpSideSpeechStatus & modem_status_mask) > 0) == true);
    mScpSideSpeechStatus &= (~modem_status_mask);

    char property_value[PROPERTY_VALUE_MAX];
    sprintf(property_value, "%u", mScpSideSpeechStatus);
    property_set(PROPERTY_KEY_SCP_CALL_STATE[mModemIndex], property_value);
}


void SpeechDriverOpenDSP::RecoverModemSideStatusToInitState() {
    // Phone Call / Loopback
    if (GetScpSideSpeechStatus(SPEECH_STATUS_MASK) == true) {
        ALOGD("%s(), modem_index = %d, speech_on = true", __FUNCTION__, mModemIndex);
        ResetScpSideSpeechStatus(SPEECH_STATUS_MASK);
        ScpSpeechOff();
    } else if (GetScpSideSpeechStatus(VT_STATUS_MASK) == true) {
        ALOGD("%s(), modem_index = %d, vt_on = true", __FUNCTION__, mModemIndex);
        ResetScpSideSpeechStatus(VT_STATUS_MASK);
        ScpSpeechOff();
    } else if (GetScpSideSpeechStatus(LOOPBACK_STATUS_MASK) == true) {
        ALOGD("%s(), modem_index = %d, loopback_on = true", __FUNCTION__, mModemIndex);
        ResetScpSideSpeechStatus(LOOPBACK_STATUS_MASK);
        ScpSpeechOff();
    }
}


/*==============================================================================
 *                     Check Modem Status
 *============================================================================*/
bool SpeechDriverOpenDSP::CheckModemIsReady() {
    // TODO: [OpenDSP] scp ready
    return pSpeechDriverInternal->CheckModemIsReady();
}



} // end of namespace android

