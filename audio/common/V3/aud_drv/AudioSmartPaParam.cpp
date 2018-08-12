#include "AudioSmartPaParam.h"
#include "AudioParamParser.h"
#include "AudioUtility.h"
#include "AudioALSAStreamManager.h"
#include <string>
#include <dlfcn.h>
#include "AudioMessengerIPI.h"

#include <arsi_api.h>
#include <audio_task.h>
#include <arsi_library_entry_points.h>

#include <aurisys_utility.h>
#include "audio_spkprotect_msg_id.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "AudioSmartPaParam"
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define DMA_BUFFER_SIZE (6144)
#define AUDIO_SMARTPA_KEY_LEN (512)
static char svalue[DMA_BUFFER_SIZE];


namespace android {

/* should matching with enum smartpa_vendor*/
static const char *aurisys_param_vendor[] = {
    ",MAXIM",
    ",NXP",
    ",RICHTEK",
    ",CIRRUS",
    ",MTK",
};

enum {
    SMARTPA_VENDOR_MAXIM,
    SMARTPA_VENDOR_NXP,
    SMARTPA_VENDOR_RICHTEK,
    SMARTPA_VENDOR_CIRRUS,
    SMARTPA_VENDOR_MTK,
    SMARTPA_VENDOR_NUM,
};

static const char *aurisys_param_suffix[] = {
    ",PARAM_FILE",
    ",APPLY_PARAM",
    ",ADDR_VALUE",
    ",KEY_VALUE",
    ",ENABLE_DUMP",
    ",ENABLE_LOG",
};

enum {
    AURISYS_SET_PARAM_FILE,
    AURISYS_SET_APPLY_PARAM,
    AURISYS_SET_ADDR_VALUE,
    AURISYS_SET_KEY_VALUE,
    AURISYS_SET_ENABLE_DUMP,
    AURISYS_SET_ENABLE_DSP_LOG,
    AURISYS_SET_ENABLE_HAL_LOG,
    AURISYS_GET_OFFSET = 0x10,
    AURISYS_GET_ADDR_VALUE = AURISYS_GET_OFFSET + AURISYS_SET_ADDR_VALUE,
    AURISYS_GET_KEY_VALUE,
    AURISYS_PARAM_TOTAL_NUM,
};

static const char *aurisys_param_layer[] = {
    ",DSP",
    ",HAL",
};

enum {
    AURISYS_PARAM_LAYER_DSP,
    AURISYS_PARAM_LAYER_HAL,
    AURISYS_PARAM_LAYER_NUM,
};


/* string should comapre with prefix + vendor + suffix*/
static const char aurisys_set_param_dsp_prefix[] = "AURISYS_SET_PARAM,DSP,SMARTPA,";
static const char aurisys_set_param_hal_prefix[] = "AURISYS_SET_PARAM,HAL,SMARTPA,";
static const char aurisys_get_param_hal_prefix[] = "AURISYS_GET_PARAM,DSP,SMARTPA,";


static const char PROPERTY_KEY_SMARTPA_PARAM_PATH[PROPERTY_KEY_MAX]    = "persist.af.aurisys.smartpa_path";
static const char PROPERTY_KEY_PRODUCT_NAME[PROPERTY_KEY_MAX]  = "ro.mediatek.platform";
static const char PROPERTY_KEY_PLATFROM_NAME[PROPERTY_KEY_MAX] = "ro.product.model";
static const char PROPERTY_KEY_PCM_DUMP_ON[PROPERTY_KEY_MAX]   = "persist.af.aurisys.pcm_dump_on";


/* lib path reserve for flexibility*/
#if defined(__LP64__)
static char aurisys_vendor_lib_path[SMARTPA_VENDOR_NUM][256] = {
    "/vendor/lib64/libaudiosmartpadummy.so",
    "/vendor/lib64/libaudiosmartpamaxim.so",
    "/vendor/lib64/libaudiosmartpanxp.so",
    "/vendor/lib64/libaudiosmartparichtek.so",
    "/vendor/lib64/libaudiosmartpacirrus.so",
};
#else
static char aurisys_vendor_lib_path[SMARTPA_VENDOR_NUM][256] = {
    "/vendor/lib/libaudiosmartpadummy.so",
    "/vendor/lib/libaudiosmartpamaxim.so",
    "/vendor/lib/libaudiosmartpanxp.so",
    "/vendor/lib/libaudiosmartparichtek.so",
    "/vendor/lib/libaudiosmartpacirrus.so",
};

#endif

static char DEFAULT_SMARTPA_SYSTEM_PARAM_PATH[] = "/system/vendor/etc/audio_param_smartpa/SmartPaVendor1_AudioParam.dat";
static char SMARTPA_DUMP_DSP_PCM_DATA_PATH[] = "/sdcard/mtklog/aurisys_dump/";


typedef void (*dynamic_link_arsi_assign_lib_fp_t)(AurisysLibInterface *lib);
static dynamic_link_arsi_assign_lib_fp_t dynamic_link_arsi_assign_lib_fp = NULL;
static AurisysLibInterface gSmartpaLibAPI; // could implemented by any 3rd party
static bool mEnableLibLogHAL = true;


static void smartPaPrint(const char *message, ...) {
    if (mEnableLibLogHAL) {
        static char printf_msg[DMA_BUFFER_SIZE];

        va_list args;
        va_start(args, message);

        vsnprintf(printf_msg, sizeof(printf_msg), message, args);
        ALOGD("%s", printf_msg);

        va_end(args);
    }
}

/*
 * singleton
 */
AudioSmartPaParam *AudioSmartPaParam::mAudioSmartPaParam = NULL;

AudioSmartPaParam *AudioSmartPaParam::getInstance() {
    static AudioLock mGetInstanceLock;
    AL_UNLOCK(mGetInstanceLock);

    if (mAudioSmartPaParam == NULL) {
        mAudioSmartPaParam = new AudioSmartPaParam();
    }

    ASSERT(mAudioSmartPaParam != NULL);
    return mAudioSmartPaParam;
}

/*
 * constructor / destructor
 */
AudioSmartPaParam::AudioSmartPaParam() {
    ALOGD("%s constructor", __FUNCTION__);
    mSmartpaMode = 0;
    getDefalutParamFilePath();
    getDefaultProductName();
    char property_value[PROPERTY_VALUE_MAX];

    pIPI = AudioMessengerIPI::getInstance();
    memset((void *)mSmartParamFilePath, 0, SMARTPA_PARAM_LENGTH);
    memset((void *)mPhoneProductName, 0, SMARTPA_PARAM_LENGTH);
    memset((void *)mSvalue, 0, SMARTPA_PARAM_LENGTH);

    mSmartpaMode = 0;
    pIPI = NULL;
    mParamBuf = NULL;
    mAurisysLib = NULL;
    mEnableLibLogHAL = false;
    mEnableDump = false;

    mArsiTaskConfig = (arsi_task_config_t *)malloc(sizeof(arsi_task_config_t));
    ASSERT(mArsiTaskConfig != NULL);
    memset(mArsiTaskConfig, 0, sizeof(arsi_task_config_t));
    initArsiTaskConfig();

    mArsiLibConfig = (arsi_lib_config_t *)malloc(sizeof(arsi_lib_config_t));
    ASSERT(mArsiLibConfig != NULL);
    memset(mArsiLibConfig, 0, sizeof(arsi_lib_config_t));

    initlibconfig();

    mParamBuf = (data_buf_t *)malloc(sizeof(data_buf_t));
    ASSERT(mParamBuf != NULL);
    memset(mParamBuf, 0, sizeof(data_buf_t));

    mParamBuf->memory_size = DMA_BUFFER_SIZE;
    mParamBuf->data_size = 0;
    mParamBuf->p_buffer = malloc(mParamBuf->memory_size);
    ASSERT(mParamBuf->p_buffer != NULL);

    char *dlopen_lib_path = NULL;

    // TODO: move to aurisys lib manager
    if (mAurisysLib == NULL) {
        memset(&gSmartpaLibAPI, 0, sizeof(AurisysLibInterface));

        /* check and get dlopen_lib_path */
        for (unsigned int i = 0 ; i < ARRAY_SIZE(aurisys_vendor_lib_path); i++) {
            if (access(aurisys_vendor_lib_path[i], R_OK) == 0) {
                dlopen_lib_path = aurisys_vendor_lib_path[i];
                break;
            }
        }

        if (dlopen_lib_path == NULL) {
            ALOGW("%s dlopen dlopen_lib_path is not set!!", __FUNCTION__);
        }

        ALOGD("%s dlopen", __FUNCTION__);

        mAurisysLib = dlopen(dlopen_lib_path, RTLD_NOW);
        if (mAurisysLib == NULL) {
            ALOGE("dlopen(%s) fail!!", dlopen_lib_path);
        } else {
            dynamic_link_arsi_assign_lib_fp =
                (dynamic_link_arsi_assign_lib_fp_t)dlsym(mAurisysLib, "dynamic_link_arsi_assign_lib_fp");
            if (dynamic_link_arsi_assign_lib_fp == NULL) {
                ALOGE("%s dynamic_link_arsi_assign_lib_fp err", __FUNCTION__);
            }
        }

        if (dynamic_link_arsi_assign_lib_fp != NULL) {
            dynamic_link_arsi_assign_lib_fp(&gSmartpaLibAPI);

            ALOGD("%s dynamic_link_arsi_assign_lib_fp", __FUNCTION__);

            string_buf_t lib_version;
            lib_version.memory_size = 128;
            lib_version.string_size = 0;
            lib_version.p_string = (char *)malloc(lib_version.memory_size);
            memset(lib_version.p_string, 0, sizeof(lib_version.p_string));
            if (gSmartpaLibAPI.arsi_get_lib_version != NULL) {
                gSmartpaLibAPI.arsi_get_lib_version(&lib_version);
                ALOGD("lib_version: \"%s\"", lib_version.p_string);
            } else {
                ALOGW("unknown lib_version");
            }

            free(lib_version.p_string);
        }

        /* get param_file_path */
        char *param_file_path = NULL;
        if (access(DEFAULT_SMARTPA_SYSTEM_PARAM_PATH, R_OK) == 0) {
            param_file_path = DEFAULT_SMARTPA_SYSTEM_PARAM_PATH;
        } else {
            ALOGE("%s(), param_file_path not found!!", __FUNCTION__);
        }

        mEnableLibLogHAL = false;
        mEnableDump = false;

        property_get(PROPERTY_KEY_SMARTPA_PARAM_PATH, mSmartParamFilePath, "");
        ALOGV("%s(), mSmartParamFilePath = %s", __FUNCTION__, mSmartParamFilePath);

        property_get(PROPERTY_KEY_PLATFROM_NAME, mPhoneProductName, "");
        ALOGV("%s(), mPhoneProductName = %s", __FUNCTION__, mPhoneProductName);

        /* get pcm dump state */
        property_get(PROPERTY_KEY_PCM_DUMP_ON, property_value, "0");  //"0": default off
        mEnableDump = (atoi(property_value) == 0) ? false : true;
        ALOGD("mEnableDump = %d", mEnableDump);

        setParamFilePath(DEFAULT_SMARTPA_SYSTEM_PARAM_PATH);
        SetSmartpaParam(mSmartpaMode);
    }

}

AudioSmartPaParam::~AudioSmartPaParam() {
    ALOGD("%s destructor", __FUNCTION__);

    if (mParamBuf->p_buffer != NULL) {
        free(mParamBuf->p_buffer);
    }

    if (mAurisysLib != NULL) {
        dlclose(mAurisysLib);
        mAurisysLib = NULL;
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

void AudioSmartPaParam::initlibconfig() {
    /* alloc buffer in SCP */
    mArsiLibConfig->p_ul_buf_in = NULL;
    mArsiLibConfig->p_ul_buf_out = NULL;
    mArsiLibConfig->p_ul_ref_bufs = NULL;

    mArsiLibConfig->p_dl_buf_in = NULL;
    mArsiLibConfig->p_dl_buf_out = NULL;
    mArsiLibConfig->p_dl_ref_bufs = NULL;

    /* lib */
    mArsiLibConfig->sample_rate = pDlsamplerate;
    mArsiLibConfig->audio_format = AUDIO_FORMAT_PCM_32_BIT;
    mArsiLibConfig->frame_size_ms = 0;
    mArsiLibConfig->b_interleave = 0;
}


void AudioSmartPaParam::initArsiTaskConfig() {
    /* output device */
    mArsiTaskConfig->output_device_info.devices = AUDIO_DEVICE_OUT_SPEAKER;
    mArsiTaskConfig->output_device_info.audio_format = AUDIO_FORMAT_PCM_32_BIT;
    mArsiTaskConfig->output_device_info.sample_rate = pDlsamplerate;
    mArsiTaskConfig->output_device_info.channel_mask = AUDIO_CHANNEL_IN_STEREO;
    mArsiTaskConfig->output_device_info.num_channels = 2;
    mArsiTaskConfig->output_device_info.hw_info_mask = 0;

    /* task scene */
    mArsiTaskConfig->task_scene = TASK_SCENE_SPEAKER_PROTECTION;

    /* audio mode */
    mArsiTaskConfig->audio_mode = AUDIO_MODE_NORMAL;

    /* max device capability for allocating memory */
    mArsiTaskConfig->max_output_device_sample_rate = pDlsamplerate;
    mArsiTaskConfig->max_output_device_num_channels = 2;

}

int AudioSmartPaParam::SetArsiTaskConfig(arsi_task_config_t ArsiTaskConfig) {
    memcpy((void *)&mArsiTaskConfig, (void *)&ArsiTaskConfig, sizeof(arsi_task_config_t));
    return 0;
}

int AudioSmartPaParam::Setlibconfig(arsi_lib_config_t mArsiLibConfig) {
    memcpy((void *)&mArsiLibConfig, (void *)&mArsiLibConfig, sizeof(arsi_lib_config_t));
    return 0;
}

/* get param from lib and set to scp*/
int AudioSmartPaParam::SetSmartpaParam(int mode) {
    struct ipi_msg_t ipi_msg;
    int retval = NO_ERROR;

    int ret;
    uint32_t param_buf_size = 0;

    mSmartpaMode = mode;

    ALOGD("%s mSmartpaMode = %d", __FUNCTION__, mSmartpaMode);

    string_buf_t platform_info;
    platform_info.memory_size = strlen(mPhoneProductName) + 1;
    platform_info.string_size = strlen(mPhoneProductName);
    platform_info.p_string = mPhoneProductName;
    ALOGD("platform_info.p_string = %s", platform_info.p_string);

    string_buf_t file_path;
    file_path.memory_size = strlen(mSmartParamFilePath) + 1;
    file_path.string_size = strlen(mSmartParamFilePath);
    file_path.p_string = mSmartParamFilePath;
    ALOGD("file_path.p_string = %s", file_path.p_string);


    if (gSmartpaLibAPI.arsi_query_param_buf_size == NULL) {
        ALOGE("%s arsi_query_param_buf_size == NULL", __FUNCTION__);
        return -1;
    }

    ret = gSmartpaLibAPI.arsi_query_param_buf_size(mArsiTaskConfig,
                                                   mArsiLibConfig,
                                                   &platform_info,
                                                   &file_path,
                                                   mSmartpaMode,
                                                   &param_buf_size,
                                                   smartPaPrint);
    ALOGD("param_buf_size = %u", param_buf_size);

    if (gSmartpaLibAPI.arsi_parsing_param_file == NULL) {
        ALOGE("%s arsi_parsing_param_file == NULL", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    memset(mParamBuf->p_buffer, 0, mParamBuf->memory_size);
    ret = gSmartpaLibAPI.arsi_parsing_param_file(mArsiTaskConfig,
                                                 mArsiLibConfig,
                                                 &platform_info,
                                                 &file_path,
                                                 mSmartpaMode,
                                                 mParamBuf,
                                                 smartPaPrint);
    ALOGD("%s mParamBuf->data_size = %u p_buffer = %s", __FUNCTION__, mParamBuf->data_size, (char *)mParamBuf->p_buffer);
    if (ret != NO_ERROR) {
        ALOGW("smartpa_arsi_parsing_param_file fail");
        return UNKNOWN_ERROR;
    }

    ALOGD("%s pIPI->sendIpiMsg ", __FUNCTION__);

    retval = pIPI->sendIpiMsg(&ipi_msg,
                              TASK_SCENE_SPEAKER_PROTECTION, AUDIO_IPI_LAYER_HAL_TO_SCP,
                              AUDIO_IPI_DMA, AUDIO_IPI_MSG_NEED_ACK,
                              SPK_IPI_MSG_A2D_PARAM, mParamBuf->data_size, DMA_BUFFER_SIZE,
                              (char *)mParamBuf->p_buffer);

    return 0;
}

/* set param path to lib*/
int AudioSmartPaParam::setParamFilePath(const char *str) {

    if (str == NULL) {
        strncpy(mSmartParamFilePath, DEFAULT_SMARTPA_SYSTEM_PARAM_PATH, strlen(DEFAULT_SMARTPA_SYSTEM_PARAM_PATH));
    } else {
        strncpy(mSmartParamFilePath, str, strlen(str));
        ALOGD("gSmartParamFilePath = %s", mSmartParamFilePath);
    }

    property_set(PROPERTY_KEY_SMARTPA_PARAM_PATH, mSmartParamFilePath);

    unsigned int param_buf_size = 0;
    int ret = 0;

    string_buf_t platform_info;
    platform_info.memory_size = strlen(mPhoneProductName) + 1;
    platform_info.string_size = strlen(mPhoneProductName);
    platform_info.p_string = mPhoneProductName;
    ALOGD("platform_info.p_string = %s", platform_info.p_string);

    string_buf_t file_path;
    file_path.memory_size = strlen(mSmartParamFilePath) + 1;
    file_path.string_size = strlen(mSmartParamFilePath);
    file_path.p_string = mSmartParamFilePath;
    ALOGD("file_path.p_string = %s", file_path.p_string);


    if (gSmartpaLibAPI.arsi_query_param_buf_size == NULL) {
        ALOGE("%s arsi_query_param_buf_size == NULL", __FUNCTION__);
        return -1;
    }

    ret = gSmartpaLibAPI.arsi_query_param_buf_size(mArsiTaskConfig,
                                                   mArsiLibConfig,
                                                   &platform_info,
                                                   &file_path,
                                                   mSmartpaMode,
                                                   &param_buf_size,
                                                   smartPaPrint);
    ALOGD("param_buf_size = %u", param_buf_size);

    if (gSmartpaLibAPI.arsi_parsing_param_file == NULL) {
        ALOGE("%s arsi_parsing_param_file == NULL", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    memset(mParamBuf->p_buffer, 0, mParamBuf->memory_size);
    ret = gSmartpaLibAPI.arsi_parsing_param_file(mArsiTaskConfig,
                                                 mArsiLibConfig,
                                                 &platform_info,
                                                 &file_path,
                                                 mSmartpaMode,
                                                 mParamBuf,
                                                 smartPaPrint);
    ALOGD("mParamBuf->data_size = %u p_buffer = %s", mParamBuf->data_size, (char *)mParamBuf->p_buffer);
    if (ret != NO_ERROR) {
        ALOGW("smartpa_arsi_parsing_param_file fail");
        return UNKNOWN_ERROR;
    }

    return 0;
}

int AudioSmartPaParam::setProductName(const char *str) {
    if (str == NULL) {
        property_get("ro.product.model", mPhoneProductName, "0");
    } else {
        ALOGD("mPhoneProductName = %s", mPhoneProductName);
    }
    return 0;
}

char *AudioSmartPaParam::getParamFilePath(void) {
    return mSmartParamFilePath;
}

char *AudioSmartPaParam::getProductName(void) {
    return mPhoneProductName;
}

int AudioSmartPaParam::getDefalutParamFilePath(void) {
    memset(mSmartParamFilePath, 0, paramlength);
    strncpy(mSmartParamFilePath, DEFAULT_SMARTPA_SYSTEM_PARAM_PATH, strlen(DEFAULT_SMARTPA_SYSTEM_PARAM_PATH));
    return 0;
}

int AudioSmartPaParam::getDefaultProductName(void) {
    memset(mPhoneProductName, 0, paramlength);
    property_get("ro.product.model", mPhoneProductName, "0");
    return 0;
}


int AudioSmartPaParam::getsetParameterPrefixlength(int paramindex, int vendorindex) {
    int parameterslength;
    ALOGD("%s strlen(aurisys_set_param_dsp_prefix = %zu %s", __FUNCTION__ , strlen(aurisys_set_param_dsp_prefix), aurisys_set_param_dsp_prefix);
    ALOGD("%s strlen(aurisys_param_vendor[vendorindex]) = %zu %s", __FUNCTION__ , strlen(aurisys_param_vendor[vendorindex]), aurisys_param_vendor[vendorindex]);
    ALOGD("%s strlen(aurisys_param_suffix[paramindex]) = %zu %s", __FUNCTION__ , strlen(aurisys_param_suffix[paramindex]), aurisys_param_suffix[paramindex]);
    if ((paramindex >= 0 && paramindex < AURISYS_PARAM_TOTAL_NUM) &&
        (vendorindex >= 0 && vendorindex < SMARTPA_VENDOR_NUM))
        parameterslength = strlen(aurisys_set_param_dsp_prefix) +
                           strlen(aurisys_param_vendor[vendorindex]) + strlen(aurisys_param_suffix[paramindex]);
    else {
        parameterslength = 0;
    }
    ALOGD("%s parameterslength = %d", __FUNCTION__ , parameterslength);
    return parameterslength;
}


int AudioSmartPaParam::getgetParameterPrefixlength(int paramindex, int vendorindex) {
    int parameterslength;
    ALOGD("%s strlen(aurisys_get_param_hal_prefix = %zu %s", __FUNCTION__ , strlen(aurisys_get_param_hal_prefix), aurisys_get_param_hal_prefix);
    ALOGD("%s strlen(aurisys_param_vendor[vendorindex]) = %zu %s", __FUNCTION__ , strlen(aurisys_param_vendor[vendorindex]), aurisys_param_vendor[vendorindex]);
    ALOGD("%s strlen(aurisys_param_suffix[paramindex]) = %zu %s", __FUNCTION__ , strlen(aurisys_param_suffix[paramindex]), aurisys_param_suffix[paramindex]);
    if ((paramindex >= 0 && paramindex < AURISYS_PARAM_TOTAL_NUM) &&
        (vendorindex >= 0 && vendorindex < SMARTPA_VENDOR_NUM))
        parameterslength = strlen(aurisys_get_param_hal_prefix) +
                           strlen(aurisys_param_vendor[vendorindex]) + strlen(aurisys_param_suffix[paramindex]);
    else {
        parameterslength = 0;
    }
    ALOGD("%s parameterslength = %d", __FUNCTION__ , parameterslength);
    return parameterslength;
}

bool AudioSmartPaParam::checkParameter(int &paramindex, int &vendorinedx, int &direction, const char *keyValue) {

    int ret = 0;
    /* init for index*/
    paramindex = 0;
    vendorinedx = 0;
    char *pch;
    char keyValuePair[AUDIO_SMARTPA_KEY_LEN];
    if (keyValue == NULL) {
        ALOGD("%s = NULL", __FUNCTION__);
    } else {
        memcpy(keyValuePair, keyValue , AUDIO_SMARTPA_KEY_LEN);
    }
    ALOGD("%s  = %s", __FUNCTION__, keyValuePair);

    if (strncmp(aurisys_set_param_dsp_prefix, keyValuePair, strlen(aurisys_set_param_dsp_prefix)) == 0) {
        ret = true;

    } else if (strncmp(aurisys_set_param_hal_prefix, keyValuePair, strlen(aurisys_set_param_hal_prefix)) == 0) {
        ret = true;
    } else if (strncmp(aurisys_get_param_hal_prefix, keyValuePair, strlen(aurisys_get_param_hal_prefix)) == 0) {
        ret = true;
        direction = AURISYS_GET_OFFSET;
    } else {
        ret =  false;
    }

    if (ret == true) {
        ALOGD("1 %s  = %s", __FUNCTION__, keyValuePair);
        for (unsigned int i = 0; i < ARRAY_SIZE(aurisys_param_suffix) ; i++) {
            pch = strstr(keyValuePair, aurisys_param_suffix[i]);
            if (pch != NULL) {
                paramindex = i;
                ALOGD("%s aurisys_param_suffix pch = %s paramindex = %d", __FUNCTION__, pch, paramindex);
                break;
            }
        }
        ALOGD("2 %s  = %s", __FUNCTION__, keyValuePair);
        for (unsigned int i = 0; i <  ARRAY_SIZE(aurisys_param_vendor) ; i++) {
            pch = strstr(keyValuePair, (const char *)aurisys_param_vendor[i]);
            if (pch != NULL) {
                vendorinedx = i;
                ALOGD("%s aurisys_param_vendor pch = %s vendorinedx = %d", __FUNCTION__, pch, vendorinedx);
                break;
            }
        }
    }

    return ret;
}

int AudioSmartPaParam::SetParameter(const char *keyValuePair) {
    int ret = 0;

    ALOGD("%s keyValuePair = %s strlen = %zu", __FUNCTION__, keyValuePair, strlen(keyValuePair));
    const int max_parse_len = DMA_BUFFER_SIZE;
    char parse_str[DMA_BUFFER_SIZE];
    memset(parse_str, '\0', DMA_BUFFER_SIZE);
    int paramindex , vendorindex , direction;
    struct ipi_msg_t ipi_msg;
    int enable_dump = 0;
    int enable_log = 0;
    uint32_t aurisys_addr = 0;
    uint32_t aurisys_value = 0;

    /* check if keyValuePair valid*/
    if (checkParameter(paramindex, vendorindex, direction, keyValuePair) == true) {
        strncpy(parse_str, keyValuePair + getsetParameterPrefixlength(paramindex, vendorindex),
                strlen(keyValuePair) - getsetParameterPrefixlength(paramindex, vendorindex));
        ALOGD("%s parse_str = %s strlen = %zu paramindex = %d vendorindex = %d",
              __FUNCTION__, parse_str, strlen(parse_str), paramindex, vendorindex);
        switch (paramindex) {
        case AURISYS_SET_PARAM_FILE: {
            setParamFilePath(parse_str);
            SetSmartpaParam(mSmartpaMode);
            break;
        }
        case AURISYS_SET_APPLY_PARAM: {
            mSmartpaMode = atoi(parse_str);
            SetSmartpaParam(mSmartpaMode);
            break;
        }
        case AURISYS_SET_ADDR_VALUE: {

            sscanf(parse_str, "%x,%x", &aurisys_addr, &aurisys_value);
            ALOGD("addr = 0x%x, value = 0x%x", aurisys_addr, aurisys_value);
            ret = pIPI->sendIpiMsg(&ipi_msg,
                                   TASK_SCENE_SPEAKER_PROTECTION, AUDIO_IPI_LAYER_HAL_TO_SCP,
                                   AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_NEED_ACK,
                                   SPK_IPI_MSG_A2D_SET_ADDR_VALUE, aurisys_addr, aurisys_value, NULL);
            break;
        }
        case AURISYS_SET_KEY_VALUE: {
            ALOGD("key_value = %s", parse_str);
            ret = pIPI->sendIpiMsg(&ipi_msg,
                                   TASK_SCENE_SPEAKER_PROTECTION, AUDIO_IPI_LAYER_HAL_TO_SCP,
                                   AUDIO_IPI_DMA, AUDIO_IPI_MSG_NEED_ACK,
                                   SPK_IPI_MSG_A2D_SET_KEY_VALUE, strlen(parse_str) + 1, DMA_BUFFER_SIZE,
                                   parse_str);
            if (ret != NO_ERROR) {
                ALOGW("SPK_IPI_MSG_A2D_SET_KEY_VALUE fail");
            } else {
                ALOGD("return %d", ipi_msg.param1);
            }
            break;
        }
        case AURISYS_SET_ENABLE_DUMP: {
            sscanf(parse_str, "%d", &enable_dump);
            pIPI->configDumpPcmEnable(enable_dump);
            ret = pIPI->sendIpiMsg(&ipi_msg,
                                   TASK_SCENE_SPEAKER_PROTECTION, AUDIO_IPI_LAYER_HAL_TO_SCP,
                                   AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_BYPASS_ACK,
                                   SPK_IPI_MSG_A2D_PCM_DUMP_ON, enable_dump, 0,
                                   NULL);
            break;
        }
        case AURISYS_SET_ENABLE_DSP_LOG: {
            sscanf(parse_str, "%d", &enable_log);
            ALOGV("enh mode = %d", enable_log);
            ret = pIPI->sendIpiMsg(&ipi_msg,
                                   TASK_SCENE_SPEAKER_PROTECTION, AUDIO_IPI_LAYER_HAL_TO_SCP,
                                   AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_BYPASS_ACK,
                                   SPK_IPI_MSG_A2D_LIB_LOG_ON, enable_log, 0,
                                   NULL);

            break;
        }
        case AURISYS_SET_ENABLE_HAL_LOG: {
            break;
        }
        default:
            break;
        }
    }

    return ret;

}

char *AudioSmartPaParam::GetParameter(const char *key) {
    char *Retval = NULL;
    status_t retval = NO_ERROR;
    const int max_parse_len = DMA_BUFFER_SIZE;
    char parse_str[DMA_BUFFER_SIZE];
    memset(parse_str, '\0', DMA_BUFFER_SIZE);
    int paramindex , vendorindex, direction;
    struct ipi_msg_t ipi_msg;
    uint32_t aurisys_addr = 0;
    uint32_t aurisys_value = 0;

    ALOGD("%s keyValuePair = %s strlen = %zu", __FUNCTION__, key, strlen(key));

    /* check if keyValuePair valid*/
    if (checkParameter(paramindex, vendorindex, direction, key) == true) {
        strncpy(parse_str, key + getgetParameterPrefixlength(paramindex, vendorindex),
                strlen(key) - getgetParameterPrefixlength(paramindex, vendorindex));
        ALOGD("%s parse_str = %s strlen = %zu paramindex = %d vendorindex = %d direction = %d",
              __FUNCTION__, parse_str, strlen(parse_str), paramindex, vendorindex, direction);

        switch ((paramindex + direction)) {
        case AURISYS_GET_ADDR_VALUE: {
            uint32_t aurisys_addr = 0;
            ALOGD("AURISYS_GET_KEY_VALUE key = %s", parse_str);
            sscanf(parse_str, "%x", &aurisys_addr);
            retval = pIPI->sendIpiMsg(&ipi_msg,
                                      TASK_SCENE_SPEAKER_PROTECTION, AUDIO_IPI_LAYER_HAL_TO_SCP,
                                      AUDIO_IPI_PAYLOAD, AUDIO_IPI_MSG_NEED_ACK,
                                      SPK_IPI_MSG_A2D_GET_ADDR_VALUE, aurisys_addr, 0,
                                      NULL);

            if (retval != NO_ERROR) {
                ALOGW("IPI_MSG_A2D_GET_ADDR_VALUE fail");
            } else {
                ALOGD("param1 0x%x, param2 0x%x", ipi_msg.param1, ipi_msg.param2);
            }

            // can be modiied if
            if (ipi_msg.param1 != 0) {
                snprintf(svalue, DMA_BUFFER_SIZE - 1, "0x%x", ipi_msg.param2); // -1: for '\0'
            } else {
                strncpy(svalue, "GET_FAIL", DMA_BUFFER_SIZE - 1); // -1: for '\0'
            }
            ALOGD("svalue = %s", svalue);
            return svalue;
        }
        case AURISYS_GET_KEY_VALUE: {
            ALOGD("AURISYS_GET_KEY_VALUE key = %s", parse_str);

            struct ipi_msg_t ipi_msg;
            retval = pIPI->sendIpiMsg(&ipi_msg,
                                      TASK_SCENE_SPEAKER_PROTECTION, AUDIO_IPI_LAYER_HAL_TO_SCP,
                                      AUDIO_IPI_DMA, AUDIO_IPI_MSG_NEED_ACK,
                                      SPK_IPI_MSG_A2D_GET_KEY_VALUE, strlen(parse_str) + 1, max_parse_len,
                                      parse_str);

            if (retval != NO_ERROR) {
                ALOGW("IPI_MSG_A2D_GET_KEY_VALUE fail");
            } else {
                ALOGD("param1 0x%x, param2 0x%x", ipi_msg.param1, ipi_msg.param2);
            }

            ALOGD("key_value = %s", parse_str);

            char *p_eq = strstr(parse_str, "=");
            if (p_eq != NULL) {
                ALOGD("p_eq = %s", p_eq);
            }

            if (ipi_msg.param1 == 1 &&
                p_eq != NULL && p_eq < parse_str + max_parse_len - 1) {
                strncpy(svalue, strstr(parse_str, "=") + 1, max_parse_len - 1); // -1: for '\0'
            } else {
                strncpy(svalue, "GET_FAIL", max_parse_len - 1); // -1: for '\0'
            }
            ALOGD("svalue = %s", svalue);
            return svalue;
        }
        default:
            break;
        }
    }

    return Retval;
}

}
