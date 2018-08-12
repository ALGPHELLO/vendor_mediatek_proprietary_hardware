#include "PictureQuality.h"

#define LOG_TAG "PQ"
#define MTK_LOG_ENABLE 1

#include <utils/Log.h>
#include <android/hidl/allocator/1.0/IAllocator.h>
#include <hidlmemory/mapping.h>

#include <utils/String16.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <cutils/properties.h>
#include <dlfcn.h>
#include <math.h>

#include "cust_color.h"
#include "cust_ccorr.h"
#include "cust_dre.h"
#include <PQCommon.h>

#include <BluLight_Defender.h>
#ifdef CHAMELEON_DISPLAY_SUPPORT
#include <Chameleon_Display.h>
#endif
#include <PQTransition.h>

/* PQ ashmem proxy */
#include <PQAshmemProxy.h>

#include "PQLightSensor.h"
#include "PQServiceTimer.h"

#ifdef FACTORY_GAMMA_SUPPORT
#include "libnvram.h"
#include "Custom_NvRam_LID.h"
#include "CFG_PQ_File.h"
#endif

#define LUMINANCE_MAX 1023

#define DISP_DEV_NODE_PATH "/dev/mtk_disp_mgr"

// Property of blue light filter strength
#define MTK_BLUELIGHT_STRENGTH_PROPERTY_NAME "persist.sys.bluelight.strength"
#define MTK_BLUELIGHT_DEFAULT_PROPERTY_NAME "persist.sys.bluelight.default"
#ifdef CHAMELEON_DISPLAY_SUPPORT
// Property of chameleon strength
#define MTK_CHAMELEON_STRENGTH_PROPERTY_NAME "persist.sys.chameleon.strength"
#define MTK_CHAMELEON_DEFAULT_PROPERTY_NAME "persist.sys.chameleon.default"
#endif

#define PQ_LOGD(fmt, arg...) ALOGD(fmt, ##arg)
#define PQ_LOGE(fmt, arg...) ALOGE(fmt, ##arg)
#define PQ_LOGI(fmt, arg...) \
    do { \
       ALOGI_IF (__android_log_is_loggable(ANDROID_LOG_VERBOSE, "PQ", ANDROID_LOG_DEBUG), "[PQ]" fmt, ##arg); \
    }while(0)

#define max( a, b )            (((a) > (b)) ? (a) : (b))

#define PQ_TIMER_LIMIT (16)
#define PQ_MIN_FORCE_TRANSITION_STEP (1)
#define PQ_MAX_FORCE_TRANSITION_STEP (0x7FFFFFFF)

#define PQ_MAX_SERIAL_NUM (0xFFFF)

#define MIN_COLOR_WIN_SIZE (0x0)
#define MAX_COLOR_WIN_SIZE (0xFFFF)

#define ARR_LEN_4BYTE(arr) (sizeof(arr) / 4)
#define UNUSED(expr) do { (void)(expr); } while (0)

/* Structure */
struct ColorRegistersTuning
{
    unsigned int GLOBAL_SAT  ;
    unsigned int CONTRAST    ;
    unsigned int BRIGHTNESS  ;
    unsigned int PARTIAL_Y    [CLR_PARTIAL_Y_SIZE];
    unsigned int PURP_TONE_S  [CLR_PQ_PARTIALS_CONTROL][CLR_PURP_TONE_SIZE];
    unsigned int SKIN_TONE_S  [CLR_PQ_PARTIALS_CONTROL][CLR_SKIN_TONE_SIZE];
    unsigned int GRASS_TONE_S [CLR_PQ_PARTIALS_CONTROL][CLR_GRASS_TONE_SIZE];
    unsigned int SKY_TONE_S   [CLR_PQ_PARTIALS_CONTROL][CLR_SKY_TONE_SIZE];
    unsigned int PURP_TONE_H  [CLR_PURP_TONE_SIZE];
    unsigned int SKIN_TONE_H  [CLR_SKIN_TONE_SIZE];
    unsigned int GRASS_TONE_H [CLR_GRASS_TONE_SIZE];
    unsigned int SKY_TONE_H   [CLR_SKY_TONE_SIZE];
    unsigned int CCORR_COEF   [3][3];
};

template <typename _T>
static char *printIntArray(char *ori_buffer, int bsize, const _T *arr, int length)
{
    char *buffer = ori_buffer;
    int n;

    n = 4;
    while (length > 0 && bsize > 1) {
        while (length < n)
            n >>= 1;
        if (n == 0) break; // length == 0

        // We print n items at one time
        int pr_len;
        if (n == 4) {
            pr_len = snprintf(buffer, bsize, " %4d %4d %4d %4d",
                (int)(arr[0]), (int)(arr[1]), (int)(arr[2]), (int)(arr[3]));
        } else if (n == 2) {
            pr_len = snprintf(buffer, bsize, " %4d %4d", (int)(arr[0]), (int)(arr[1]));
        } else {
            pr_len = snprintf(buffer, bsize, " %4d", (int)(arr[0]));
        }

        buffer += pr_len;
        bsize -= pr_len;
        arr += n;
        length -= n;
    }

    ori_buffer[bsize - 1] = '\0';

    return ori_buffer;
}

static inline int valueOf(const char ch)
{
    if ('0' <= ch && ch <= '9')
        return (ch - '0');
    else if ('a' <= ch && ch <= 'z')
        return (ch - 'a') + 10;
    else if ('A' <= ch && ch <= 'Z')
        return (ch - 'A') + 10;

    return 0;
}

static int asInt(const char *str)
{
    int val = 0;
    bool negative = false;
    int base = 10;

    const char *char_p = str;

    if (*char_p == '-') {
        negative = true;
        char_p++;
    } else if (*char_p == '+') {
        negative = false;
        char_p++;
    } else
        negative = false;

    if (*char_p == '0' && *(char_p + 1) == 'x') {
        base = 16;
        char_p += 2;
    }

    for ( ; *char_p != '\0'; char_p++) {
        val = val * base + valueOf(*char_p);
    }

    return (negative ? -val : val);
}


template <typename _D, typename _S>
int convertCopy(_D *dest, size_t destSize, const _S *src, size_t srcSize)
{
    size_t destLen = destSize / sizeof(_D);
    size_t srcLen = srcSize / sizeof(_S);

    if (srcLen < destLen)
        destLen = srcLen;

    for (size_t i = 0; i < destLen; i++) {
        dest[i] = static_cast<_D>(src[i]);
    }

    return destLen;
}

namespace vendor {
namespace mediatek {
namespace hardware {
namespace pq {
namespace V2_0 {
namespace implementation {

using ::android::hidl::allocator::V1_0::IAllocator;

PictureQuality::PictureQuality()
{
    PQ_LOGD("[PQ_SERVICE] PQService constructor start");

    m_drvID = open(DISP_DEV_NODE_PATH, O_RDONLY, 0);

    if (m_drvID < 0)
    {
        PQ_LOGE("[PQ_SERVICE] open device fail!!");
    }

    // set all feature default off
    memset(m_bFeatureSwitch, 0, sizeof(uint32_t) * PQ_FEATURE_MAX);

    // update it by each feature
#ifndef DISP_COLOR_OFF
    m_bFeatureSwitch[DISPLAY_COLOR] = 1;
#endif
#ifdef MDP_COLOR_ENABLE
    m_bFeatureSwitch[CONTENT_COLOR] = 1;
#endif
    m_bFeatureSwitch[DYNAMIC_SHARPNESS] = 1;

    m_pic_statndard = NULL;
    m_pic_vivid = NULL;
    m_pic_userdef = NULL;

    mPQInput = new PQInput;
    mPQOutput = new PQOutput;
    blueLight = new BluLightDefender;
    mBLInputMode = PQ_TUNING_NORMAL;
    mBLInput = NULL;
    mBLOutputMode = PQ_TUNING_NORMAL;
    mBLOutput = NULL;
#ifdef CHAMELEON_DISPLAY_SUPPORT
    mChameleonInputMode = PQ_TUNING_NORMAL;
    mChameleonInput = NULL;
    mChameleonOutputMode = PQ_TUNING_NORMAL;
    mChameleonOutput = NULL;
    mTargetBacklight = LUMINANCE_MAX; // android backlight setting
    mDriverBacklight = LUMINANCE_MAX;
    mChameleonBacklightOut = LUMINANCE_MAX; // chameleon backlight
    mAllowSensorDebounce = true;
    chameleonDisplayProcess = new TChameleonDisplayProcess;
    mLightSensor = new PQLightSensor;
    mLightSensor->setListener(onALIChanged, this);
    mSensorInputxyY = false;
    mInput_x = 0.0;
    mInput_y = 0.0;
    mInput_Y = 0.0;
#endif
    mPQTransition = new TPQTransitionProcess;
    mTransitionInputMode = PQ_TUNING_NORMAL;
    mTransitionInput = NULL;
    mTransitionOutputMode = PQ_TUNING_NORMAL;
    mTransitionOutput = NULL;
    mForceTransitionStep = PQ_DEFAULT_TRANSITION_ON_STEP;

    mCcorrDebug = false;
    mdp_win_param.split_en = 0;
    mdp_win_param.start_x = 0;
    mdp_win_param.start_y = 0;
    mdp_win_param.end_x = 0xffff;
    mdp_win_param.end_y = 0xffff;
    memset(&m_pqparam, 0, sizeof(DISP_PQ_PARAM));

    char value[PROPERTY_VALUE_MAX];
    property_get(PQ_PIC_MODE_PROPERTY_STR, value, PQ_PIC_MODE_DEFAULT);
    m_PQMode = atoi(value);

    m_PQScenario = SCENARIO_PICTURE;
    mBlueLightDebugFlag = 0;

    m_AshmemProxy = new PQAshmemProxy;

    m_EventEnabled = false;

    initDefaultPQParam();

    m_GlobalPQSwitch = 0;
    m_GlobalPQStrength = 0;
    m_GlobalPQStrengthRange = 0;
    m_GlobalPQSupport = 0;
    m_GlobalPQStableStatus = 0;
    bzero(&(m_GlobalPQindex.dcindex), sizeof(DISPLAY_DC_T));

    if (m_PQMode == PQ_PIC_MODE_USER_DEF) {
        getGammaIndex_impl(&m_gamma_id);
    } else {
        m_gamma_id = GAMMA_INDEX_DEFAULT;
    }

#ifdef MTK_GLOBAL_PQ_SUPPORT
    initGlobalPQ();
#endif
    m_NvGammaStatus = false;

    mDebugLevel = 0x0;

    memset(&mDebug, 0, sizeof(mDebug));

    memset(&m_pqparam_client, 0, sizeof(m_pqparam_client));

    m_PQParameterSN = 0;

    clearEvents();

    m_is_ashmem_init = false;

    PQ_LOGD("[PQ_SERVICE] PQService constructor end");
#ifndef BASIC_PACKAGE
    runThreadLoop();
#endif
}

PictureQuality::~PictureQuality()
{
    close(m_drvID);

    delete mPQInput;
    delete mPQOutput;
    delete blueLight;
    delete mPQTransition;

    if (mBLInput != NULL)
        delete static_cast<ColorRegistersTuning*>(mBLInput);
    if (mBLOutput != NULL)
        delete static_cast<ColorRegistersTuning*>(mBLOutput);
    if (mTransitionInput != NULL)
        delete static_cast<TPQTransitionInput*>(mTransitionInput);
    if (mTransitionOutput != NULL)
        delete static_cast<TPQTransitionOutput*>(mTransitionOutput);

    delete m_pic_statndard;
    delete m_pic_vivid;
    delete m_pic_userdef;

    delete m_AshmemProxy;

#ifdef CHAMELEON_DISPLAY_SUPPORT
    delete chameleonDisplayProcess;
    delete mLightSensor;

    if (mChameleonInput != NULL)
        delete static_cast<TChameleonDisplayInput*>(mChameleonInput);
    if (mChameleonOutput != NULL)
        delete static_cast<TChameleonDisplayOutput*>(mChameleonOutput);
#endif
}

// Methods from ::vendor::mediatek::hardware::pq::V2_0::IPictureQuality follow.
Return<Result> PictureQuality::setColorRegion(int32_t split_en, int32_t start_x, int32_t start_y, int32_t end_x, int32_t end_y) {
#ifndef DISP_COLOR_OFF
    Mutex::Autolock _l(mLock);

    if (m_drvID < 0)
    {
        PQ_LOGE("[PQ_SERVICE] open device fail!!");
        return Result::NOT_SUPPORTED;
    }

    if (start_x < MIN_COLOR_WIN_SIZE || start_x > MAX_COLOR_WIN_SIZE)
    {
        PQ_LOGE("[PQ_SERVICE] invalid parameter of start_x:[%d]!!", start_x);
        return Result::INVALID_ARGUMENTS;
    }

    if (end_x < MIN_COLOR_WIN_SIZE || end_x > MAX_COLOR_WIN_SIZE || end_x < start_x)
    {
        PQ_LOGE("[PQ_SERVICE] invalid parameter of end_x:[%d], with start_x:[%d]!!", end_x, start_x);
        return Result::INVALID_ARGUMENTS;
    }

    if (start_y < MIN_COLOR_WIN_SIZE || start_y > MAX_COLOR_WIN_SIZE)
    {
        PQ_LOGE("[PQ_SERVICE] invalid parameter of start_y:[%d]!!", start_y);
        return Result::INVALID_ARGUMENTS;
    }

    if (end_y < MIN_COLOR_WIN_SIZE || end_y > MAX_COLOR_WIN_SIZE || end_y < start_y)
    {
        PQ_LOGE("[PQ_SERVICE] invalid parameter of end_y:[%d], with start_y:[%d]!!", end_y, start_y);
        return Result::INVALID_ARGUMENTS;
    }

    mdp_win_param.split_en = split_en;
    mdp_win_param.start_x = start_x;
    mdp_win_param.start_y = start_y;
    mdp_win_param.end_x = end_x;
    mdp_win_param.end_y = end_y;
    ioctl(m_drvID, DISP_IOCTL_PQ_SET_WINDOW, &mdp_win_param);
#else
    UNUSED(split_en);
    UNUSED(start_x);
    UNUSED(start_y);
    UNUSED(end_x);
    UNUSED(end_y);
#endif

    return Result::OK;
}

Return<void> PictureQuality::getColorRegion(getColorRegion_cb _hidl_cb) {
    Mutex::Autolock _l(mLock);
    //for MDP split demo window, MDP should update this per frame???
    _hidl_cb(Result::OK, mdp_win_param);
    return Void();
}

Return<Result> PictureQuality::setPQMode(int32_t mode, int32_t step) {
    char value[PROPERTY_VALUE_MAX];
    Mutex::Autolock _l(mLock);
    DISP_PQ_PARAM *p_pqparam = &m_pqparam_table[0];
    m_PQMode = mode;

    snprintf(value, PROPERTY_VALUE_MAX, "%d\n", mode);
    property_set(PQ_PIC_MODE_PROPERTY_STR, value);
    PQ_LOGD("[PQ_SERVICE] property set... picture mode[%d]", mode);
#ifndef DISP_COLOR_OFF
    if (m_drvID < 0)
    {
        PQ_LOGE("[PQ_SERVICE] open device fail!!");
        return Result::NOT_SUPPORTED;
    }

    if (mode == PQ_PIC_MODE_STANDARD || mode == PQ_PIC_MODE_VIVID)
    {
        if (mode == PQ_PIC_MODE_STANDARD) {
            p_pqparam = m_pic_statndard->getPQParam(0);
        } else if (mode == PQ_PIC_MODE_VIVID) {
            p_pqparam = m_pic_vivid->getPQParam(0);
        }

        m_gamma_id = GAMMA_INDEX_DEFAULT;
        PQ_LOGD("[PQ_SERVICE] setGammaIdx[%d]", m_gamma_id);
        _setGammaIndex(m_gamma_id);

        PQ_LOGD("[PQ_SERVICE] setPQMode shp[%d],gsat[%d], cont[%d], bri[%d] ", p_pqparam->u4SHPGain, p_pqparam->u4SatGain, p_pqparam->u4Contrast, p_pqparam->u4Brightness);
        PQ_LOGD("[PQ_SERVICE] setPQMode hue0[%d], hue1[%d], hue2[%d], hue3[%d] ", p_pqparam->u4HueAdj[0], p_pqparam->u4HueAdj[1], p_pqparam->u4HueAdj[2], p_pqparam->u4HueAdj[3]);
        PQ_LOGD("[PQ_SERVICE] setPQMode sat0[%d], sat1[%d], sat2[%d], sat3[%d] ", p_pqparam->u4SatAdj[0], p_pqparam->u4SatAdj[1], p_pqparam->u4SatAdj[2], p_pqparam->u4SatAdj[3]);

        memcpy(&m_pqparam, p_pqparam, sizeof(DISP_PQ_PARAM));
    }
    else if (mode == PQ_PIC_MODE_USER_DEF)
    {
        p_pqparam = m_pic_userdef->getPQParam(0);

        property_get(GAMMA_INDEX_PROPERTY_NAME, value, PQ_GAMMA_INDEX_DEFAULT);
        m_gamma_id = atoi(value);
        PQ_LOGD("[PQ_SERVICE] setGammaIdx[%d]", m_gamma_id);
        _setGammaIndex(m_gamma_id);

        PQ_LOGD("[PQ_SERVICE] setPQMode shp[%d], gsat[%d], cont[%d], bri[%d] ", m_pqparam.u4SHPGain, m_pqparam.u4SatGain, m_pqparam.u4Contrast, m_pqparam.u4Brightness);
        PQ_LOGD("[PQ_SERVICE] setPQMode hue0[%d], hue1[%d], hue2[%d], hue3[%d] ", m_pqparam.u4HueAdj[0], m_pqparam.u4HueAdj[1], m_pqparam.u4HueAdj[2], m_pqparam.u4HueAdj[3]);
        PQ_LOGD("[PQ_SERVICE] setPQMode sat0[%d], sat1[%d], sat2[%d], sat3[%d] ", m_pqparam.u4SatAdj[0], m_pqparam.u4SatAdj[1], m_pqparam.u4SatAdj[2], m_pqparam.u4SatAdj[3]);

        getUserModePQParam();

        calcPQStrength(&m_pqparam, p_pqparam, m_pqparam_mapping.image); //default scenario = image
    }
    else
    {
        m_gamma_id = GAMMA_INDEX_DEFAULT;
        PQ_LOGD("[PQ_SERVICE] setGammaIdx[%d]", m_gamma_id);
        _setGammaIndex(m_gamma_id);

        PQ_LOGE("[PQ_SERVICE] unknown picture mode!!");

        memcpy(&m_pqparam, p_pqparam, sizeof(DISP_PQ_PARAM));
    }
#endif

    setEvent(eEvtPQChange);
    setForceTransitionStep(step);
    refreshDisplay();

    // Notify MDP PQ engines to update latest setting
    if (m_PQParameterSN >= PQ_MAX_SERIAL_NUM)
    {
        m_PQParameterSN = 0;
    }
    m_PQParameterSN++;

    m_AshmemProxy->setTuningField(PQ_PARAM_SN, m_PQParameterSN);

    return Result::OK;
}

Return<Result> PictureQuality::setTDSHPFlag(int32_t TDSHPFlag) {
    Mutex::Autolock _l(mLock);
    int32_t read_flag = 0;

    if (m_AshmemProxy->setTuningField(ASHMEM_TUNING_FLAG, TDSHPFlag) < 0)
    {
        PQ_LOGE("[PQ_SERVICE] setTDSHPFlag : m_AshmemProxy->setTuningField() failed\n");
        return Result::INVALID_STATE;
    }

    if (m_AshmemProxy->getTuningField(ASHMEM_TUNING_FLAG, &read_flag) == 0)
    {
        PQ_LOGD("[PQ_CMD] setTuningFlag[%x]\n", read_flag);
    }

    return Result::OK;
}

Return<void> PictureQuality::getTDSHPFlag(getTDSHPFlag_cb _hidl_cb) {
    Mutex::Autolock _l(mLock);
    Result retval = Result::NOT_SUPPORTED;
    int32_t flag_value = 0;

    if (m_AshmemProxy->getTuningField(ASHMEM_TUNING_FLAG, &flag_value) < 0)
    {
        PQ_LOGE("[PQ_SERVICE] getTDSHPFlag : m_AshmemProxy->getTuningField() failed\n");
        flag_value = 0;
        retval = Result::INVALID_STATE;
    } else {
        retval = Result::OK;
    }

    PQ_LOGD("[PQ_SERVICE] getTuningFlag[%x]", flag_value);

    _hidl_cb(retval, flag_value);
    return Void();
}

Return<void> PictureQuality::getMappedColorIndex(int32_t scenario, int32_t mode, getMappedColorIndex_cb _hidl_cb) {
    Mutex::Autolock _l(mLock);
    DISP_PQ_PARAM *p_pqparam = &m_pqparam_table[0];
    dispPQIndexParams *index = &m_pqparam_client;

    m_PQScenario = scenario;
    int scenario_index = PQPictureMode::getScenarioIndex(m_PQScenario);
    PQ_LOGI("[PQ_SERVICE] getMappedColorIndex : m_PQScenario = %d, scenario_index = %d, m_PQMode = %d\n", m_PQScenario, scenario_index, m_PQMode);

    if (isStandardPictureMode(m_PQMode, scenario_index))
    {
        p_pqparam = m_pic_statndard->getPQParam(scenario_index);
        memcpy(index, p_pqparam, sizeof(DISP_PQ_PARAM));
    }
    else if (isVividPictureMode(m_PQMode, scenario_index))
    {
        p_pqparam = m_pic_vivid->getPQParam(scenario_index);
        memcpy(index, p_pqparam, sizeof(DISP_PQ_PARAM));
    }
    else if (isUserDefinedPictureMode(m_PQMode, scenario_index))
    {
        p_pqparam = m_pic_userdef->getPQUserDefParam();
        calcPQStrength(index, p_pqparam, getPQStrengthRatio(m_PQScenario));
    }
    else
    {
        memcpy(index, p_pqparam, sizeof(DISP_PQ_PARAM));
        PQ_LOGD("[PQ_SERVICE] PQService : getMappedColorIndex, invalid mode or scenario\n");
    }
    UNUSED(mode);

    _hidl_cb(Result::OK, m_pqparam_client);
    return Void();
}

Return<void> PictureQuality::getMappedTDSHPIndex(int32_t scenario, int32_t mode, getMappedTDSHPIndex_cb _hidl_cb) {
    Mutex::Autolock _l(mLock);
    DISP_PQ_PARAM *p_pqparam = &m_pqparam_table[0];
    dispPQIndexParams *index = &m_pqparam_client;

    m_PQScenario = scenario;
    int scenario_index = PQPictureMode::getScenarioIndex(m_PQScenario);
    PQ_LOGI("[PQ_SERVICE] getMappedTDSHPIndex : m_PQScenario = %d, scenario_index = %d, m_PQMode = %d\n", m_PQScenario, scenario_index, m_PQMode);

    if (isStandardPictureMode(m_PQMode, scenario_index))
    {
        p_pqparam = m_pic_statndard->getPQParam(scenario_index);
        memcpy(index, p_pqparam, sizeof(DISP_PQ_PARAM));
    }
    else if (isVividPictureMode(m_PQMode, scenario_index))
    {
        p_pqparam = m_pic_vivid->getPQParam(scenario_index);
        memcpy(index, p_pqparam, sizeof(DISP_PQ_PARAM));
    }
    else if (isUserDefinedPictureMode(m_PQMode, scenario_index))
    {
        p_pqparam = m_pic_userdef->getPQUserDefParam();
        calcPQStrength(index, p_pqparam, getPQStrengthRatio(m_PQScenario));
    }
    else
    {
        memcpy(index, p_pqparam, sizeof(DISP_PQ_PARAM));
        PQ_LOGD("[PQ_SERVICE] PQService : getMappedTDSHPIndex, invalid mode or scenario\n");
    }
    UNUSED(mode);

    _hidl_cb(Result::OK, m_pqparam_client);
    return Void();
}

Return<void> PictureQuality::getColorIndex(int32_t scenario, int32_t mode, getColorIndex_cb _hidl_cb) {
    Mutex::Autolock _l(mLock);
    DISP_PQ_PARAM *p_pqparam = &m_pqparam_table[0];
    dispPQIndexParams *index = &m_pqparam_client;

    m_PQScenario = scenario;
    int scenario_index = PQPictureMode::getScenarioIndex(m_PQScenario);
    PQ_LOGD("[PQ_SERVICE] getColorIndex : m_PQScenario = %d, scenario_index = %d, m_PQMode = %d\n", m_PQScenario, scenario_index, m_PQMode);

    if (isStandardPictureMode(m_PQMode, scenario_index))
    {
        p_pqparam = m_pic_statndard->getPQParam(scenario_index);
        memcpy(index, p_pqparam, sizeof(DISP_PQ_PARAM));
    }
    else if (isVividPictureMode(m_PQMode, scenario_index))
    {
        p_pqparam = m_pic_vivid->getPQParam(scenario_index);
        memcpy(index, p_pqparam, sizeof(DISP_PQ_PARAM));
    }
    else if (isUserDefinedPictureMode(m_PQMode, scenario_index))
    {
        p_pqparam = m_pic_userdef->getPQUserDefParam();
        memcpy(index, p_pqparam, sizeof(DISP_PQ_PARAM));
    }
    else
    {
        memcpy(index, p_pqparam, sizeof(DISP_PQ_PARAM));
        PQ_LOGD("[PQ_SERVICE] PQService : getColorIndex, invalid mode or scenario\n");
    }
    UNUSED(mode);

    _hidl_cb(Result::OK, m_pqparam_client);
    return Void();
}

Return<void> PictureQuality::getTDSHPIndex(int32_t scenario, int32_t mode, getTDSHPIndex_cb _hidl_cb) {
    Mutex::Autolock _l(mLock);
    DISP_PQ_PARAM *p_pqparam = &m_pqparam_table[0];
    dispPQIndexParams *index = &m_pqparam_client;

    m_PQScenario = scenario;
    int scenario_index = PQPictureMode::getScenarioIndex(m_PQScenario);
    PQ_LOGD("[PQ_SERVICE] getTDSHPIndex : m_PQScenario = %d, scenario_index = %d, m_PQMode = %d\n", m_PQScenario, scenario_index, m_PQMode);

    if (isStandardPictureMode(m_PQMode, scenario_index))
    {
        p_pqparam = m_pic_statndard->getPQParam(scenario_index);
        memcpy(index, p_pqparam, sizeof(DISP_PQ_PARAM));
    }
    else if (isVividPictureMode(m_PQMode, scenario_index))
    {
        p_pqparam = m_pic_vivid->getPQParam(scenario_index);
        memcpy(index, p_pqparam, sizeof(DISP_PQ_PARAM));
    }
    else if (isUserDefinedPictureMode(m_PQMode, scenario_index))
    {
        p_pqparam = m_pic_userdef->getPQUserDefParam();
        memcpy(index, p_pqparam, sizeof(DISP_PQ_PARAM));
    }
    else
    {
        memcpy(index, p_pqparam, sizeof(DISP_PQ_PARAM));
        PQ_LOGD("[PQ_SERVICE] PQService : getTDSHPIndex, invalid mode or scenario\n");
    }
    UNUSED(mode);

    _hidl_cb(Result::OK, m_pqparam_client);
    return Void();
}

Return<Result> PictureQuality::setPQIndex(int32_t level, int32_t scenario, int32_t tuning_mode, int32_t index, int32_t step) {
    char value[PROPERTY_VALUE_MAX];
    Mutex::Autolock _l(mLock);
    DISP_PQ_PARAM *pqparam_image_ptr = &m_pqparam_table[0];
    DISP_PQ_PARAM *pqparam_video_ptr = &m_pqparam_table[1];

    if (m_drvID < 0)
    {
        PQ_LOGE("[PQ_SERVICE] open device fail!!");
        return Result::NOT_SUPPORTED;
    }

    if (scenario >= 0 && scenario < PQ_SCENARIO_COUNT)
    {
        m_PQScenario = scenario;
    }
    else
    {
        PQ_LOGE("[PQ_SERVICE] setPQIndex: scenario[%d] out of range", scenario);
        return Result::NOT_SUPPORTED ;
    }

    if (m_PQMode == PQ_PIC_MODE_STANDARD)
    {
        pqparam_image_ptr = m_pic_statndard->getPQParamImage();
        pqparam_video_ptr = m_pic_statndard->getPQParamVideo();
    }
    else if (m_PQMode == PQ_PIC_MODE_VIVID)
    {
        pqparam_image_ptr = m_pic_vivid->getPQParamImage();
        pqparam_video_ptr = m_pic_vivid->getPQParamVideo();
    }
    else if (m_PQMode == PQ_PIC_MODE_USER_DEF)
    {
        pqparam_image_ptr = m_pic_userdef->getPQUserDefParam();
        pqparam_video_ptr = m_pic_userdef->getPQUserDefParam();
    }
    else
    {
        PQ_LOGE("[PQ_SERVICE] PQService : Unknown m_PQMode\n");
    }

    setPQParamlevel(pqparam_image_ptr, index, level);

    if (m_PQMode == PQ_PIC_MODE_STANDARD || m_PQMode == PQ_PIC_MODE_VIVID)
    {
        memcpy(&m_pqparam, pqparam_image_ptr, sizeof(DISP_PQ_PARAM));
    }
    else if (m_PQMode == PQ_PIC_MODE_USER_DEF)
    {
        calcPQStrength(&m_pqparam, pqparam_image_ptr, getPQStrengthRatio(m_PQScenario));
    }

    // if in Gallery PQ tuning mode, sync video param with image param
    if(tuning_mode == TDSHP_FLAG_TUNING)
    {
        *pqparam_video_ptr = *pqparam_image_ptr;
    }

    setEvent(eEvtPQChange);
    setForceTransitionStep(step);
    refreshDisplay();

    // Notify MDP PQ engines to update latest setting
    if (m_PQParameterSN >= PQ_MAX_SERIAL_NUM)
    {
        m_PQParameterSN = 0;
    }
    m_PQParameterSN++;

    m_AshmemProxy->setTuningField(PQ_PARAM_SN, m_PQParameterSN);

    return Result::OK;
}

Return<Result> PictureQuality::setDISPScenario(int32_t scenario, int32_t step) {
    Mutex::Autolock _l(mLock);

    if (scenario >= 0 && scenario < PQ_SCENARIO_COUNT)
        m_PQScenario = scenario;

#ifndef DISP_COLOR_OFF
    DISP_PQ_PARAM *p_pqparam = &m_pqparam_table[0];
    int percentage = 0;
    int scenario_index = PQPictureMode::getScenarioIndex(m_PQScenario);

    if (m_drvID < 0)
    {
        return Result::NOT_SUPPORTED;
    }

    PQ_LOGD("[PQ_SERVICE] PQService : m_PQScenario = %d, m_PQMode = %d\n",m_PQScenario,m_PQMode);

    if (isStandardPictureMode(m_PQMode, scenario_index))
    {
        p_pqparam = m_pic_statndard->getPQParam(scenario_index);
        memcpy(&m_pqparam, p_pqparam, sizeof(DISP_PQ_PARAM));
    }
    else if (isVividPictureMode(m_PQMode, scenario_index))
    {
        p_pqparam = m_pic_vivid->getPQParam(scenario_index);
        memcpy(&m_pqparam, p_pqparam, sizeof(DISP_PQ_PARAM));
    }
    else if (isUserDefinedPictureMode(m_PQMode, scenario_index))
    {
        p_pqparam = m_pic_userdef->getPQParam(scenario_index);
        calcPQStrength(&m_pqparam, p_pqparam, getPQStrengthRatio(m_PQScenario));
    }
    else
    {
        memcpy(&m_pqparam, p_pqparam, sizeof(DISP_PQ_PARAM));
        PQ_LOGD("[PQ_SERVICE] PQService : getMappedTDSHPIndex, invalid mode or scenario\n");
    }

    setEvent(eEvtPQChange);
    setForceTransitionStep(step);
    refreshDisplay();
#endif

    return Result::OK;
}

Return<Result> PictureQuality::setFeatureSwitch(PQFeatureID id, uint32_t value) {
    Mutex::Autolock _l(mLock);
    Result retval = Result::NOT_SUPPORTED;

    PQ_LOGD("[PQ_SERVICE] setFeatureSwitch(), feature[%d], value[%d]", id, value);

    switch (id) {
    case PQFeatureID::DISPLAY_COLOR:
        if (enableDisplayColor(value) == NO_ERROR) {
            retval = Result::OK;
        }
        break;
    case PQFeatureID::CONTENT_COLOR:
        if (enableContentColor(value) == NO_ERROR) {
            retval = Result::OK;
        }
        break;
    case PQFeatureID::CONTENT_COLOR_VIDEO:
        if (enableContentColorVideo(value) == NO_ERROR) {
            retval = Result::OK;
        }
        break;
    case PQFeatureID::SHARPNESS:
        if (enableSharpness(value) == NO_ERROR) {
            retval = Result::OK;
        }
        break;
    case PQFeatureID::DYNAMIC_CONTRAST:
        if (enableDynamicContrast(value) == NO_ERROR) {
            retval = Result::OK;
        }
        break;
    case PQFeatureID::DYNAMIC_SHARPNESS:
        if (enableDynamicSharpness(value) == NO_ERROR) {
            retval = Result::OK;
        }
        break;
    case PQFeatureID::DISPLAY_GAMMA:
        if (enableDisplayGamma(value) == NO_ERROR) {
            retval = Result::OK;
        }
        break;
    case PQFeatureID::DISPLAY_OVER_DRIVE:
        if (enableDisplayOverDrive(value) == NO_ERROR) {
            retval = Result::OK;
        }
        break;
    case PQFeatureID::ISO_ADAPTIVE_SHARPNESS:
        if (enableISOAdaptiveSharpness(value) == NO_ERROR) {
            retval = Result::OK;
        }
        break;
    case PQFeatureID::ULTRA_RESOLUTION:
        if (enableUltraResolution(value) == NO_ERROR) {
            retval = Result::OK;
        }
        break;
    case PQFeatureID::VIDEO_HDR:
        if (enableVideoHDR(value) == NO_ERROR) {
            retval = Result::OK;
        }
        break;
    default:
        PQ_LOGE("[PQ_SERVICE] setFeatureSwitch(), feature[%d] is not implemented!!", id);
        retval = Result::INVALID_ARGUMENTS;
        break;
    }

    return retval;
}

Return<void> PictureQuality::getFeatureSwitch(PQFeatureID id, getFeatureSwitch_cb _hidl_cb) {
    Mutex::Autolock _l(mLock);
    Result retval = Result::NOT_SUPPORTED;
    uint32_t value = 0;
    uint32_t featureID = static_cast<uint32_t>(id);

    if (id < PQFeatureID::PQ_FEATURE_MAX)
    {
        value = m_bFeatureSwitch[featureID];
        PQ_LOGD("[PQ_SERVICE] getFeatureSwitch(), feature[%d], value[%d]", id, value);

        retval = Result::OK;
    }
    else
    {
        value = 0;
        PQ_LOGE("[PQ_SERVICE] getFeatureSwitch(), unsupported feature[%d]", id);

        retval = Result::INVALID_ARGUMENTS;
    }

    _hidl_cb(retval, value);
    return Void();
}

Return<Result> PictureQuality::enableBlueLight(bool enable, int32_t step) {
    Result retval = Result::NOT_SUPPORTED;
#ifdef BLULIGHT_DEFENDER_SUPPORT
    Mutex::Autolock _l(mLock);

    PQ_LOGD("[PQ_SERVICE] PQService : enableBlueLight(%d)", (enable ? 1 : 0));

    blueLight->setEnabled(enable);

    setForceTransitionStep(step);
    refreshDisplay();

    retval = Result::OK;
#else
    UNUSED(step);
    UNUSED(enable);
#endif

    return retval;
}

Return<void> PictureQuality::getBlueLightEnabled(getBlueLightEnabled_cb _hidl_cb) {
    Result retval = Result::NOT_SUPPORTED;
    bool isEnabled = false;
#ifdef BLULIGHT_DEFENDER_SUPPORT
    Mutex::Autolock _l(mLock);

    isEnabled = blueLight->isEnabled();
    retval = Result::OK;
#endif

    _hidl_cb(retval, isEnabled);
    return Void();
}

Return<Result> PictureQuality::setBlueLightStrength(int32_t strength, int32_t step) {
    Result retval = Result::NOT_SUPPORTED;
#ifdef BLULIGHT_DEFENDER_SUPPORT
    char value[PROPERTY_VALUE_MAX];
    Mutex::Autolock _l(mLock);

    PQ_LOGD("[PQ_SERVICE] PQService : setBlueLightStrength(%d)", strength);

    snprintf(value, PROPERTY_VALUE_MAX, "%d", strength);
    property_set(MTK_BLUELIGHT_STRENGTH_PROPERTY_NAME, value);

    if (strength > 255)
        strength = 255;
    else if (strength < 0)
        strength = 0;

    blueLight->setStrength(strength);

    setForceTransitionStep(step);
    refreshDisplay();

    retval = Result::OK;
#else
    UNUSED(strength);
    UNUSED(step);
#endif

    return retval;
}

Return<void> PictureQuality::getBlueLightStrength(getBlueLightStrength_cb _hidl_cb) {
    Result retval = Result::NOT_SUPPORTED;
    int32_t strength = 0;
#ifdef BLULIGHT_DEFENDER_SUPPORT
    char value[PROPERTY_VALUE_MAX];
    Mutex::Autolock _l(mLock);

    property_get(MTK_BLUELIGHT_STRENGTH_PROPERTY_NAME, value, "128");
    strength = atoi(value);

    retval = Result::OK;
#endif

    _hidl_cb(retval, strength);
    return Void();
}

Return<Result> PictureQuality::enableChameleon(bool enable, int32_t step) {
    Result retval = Result::NOT_SUPPORTED;
#ifdef CHAMELEON_DISPLAY_SUPPORT
    Mutex::Autolock _l(mLock);

    PQ_LOGD("[PQ_SERVICE] PQService : enableChameleon(%d)", (enable ? 1 : 0));

    chameleonDisplayProcess->setEnabled(enable);

    mLightSensor->setEnabled(enable);

    setEvent(eEvtChameleon);
    setForceTransitionStep(step);
    refreshDisplay();

    retval = Result::OK;
#else
    UNUSED(step);
    UNUSED(enable);
#endif

    return retval;
}

Return<void> PictureQuality::getChameleonEnabled(getChameleonEnabled_cb _hidl_cb) {
    Result retval = Result::NOT_SUPPORTED;
    bool isEnabled = false;
#ifdef CHAMELEON_DISPLAY_SUPPORT
    Mutex::Autolock _l(mLock);

    isEnabled = chameleonDisplayProcess->isEnabled();
    retval = Result::OK;
#endif

    _hidl_cb(retval, isEnabled);
    return Void();
}

Return<Result> PictureQuality::setChameleonStrength(int32_t strength, int32_t step) {
    Result retval = Result::NOT_SUPPORTED;
#ifdef CHAMELEON_DISPLAY_SUPPORT
    char value[PROPERTY_VALUE_MAX];
    Mutex::Autolock _l(mLock);

    PQ_LOGD("[PQ_SERVICE] PQService : setChameleonStrength(%d)", strength);

    snprintf(value, PROPERTY_VALUE_MAX, "%d", strength);
    property_set(MTK_CHAMELEON_STRENGTH_PROPERTY_NAME, value);

    chameleonDisplayProcess->setStrength(strength);

    setEvent(eEvtChameleon);
    setForceTransitionStep(step);
    refreshDisplay();

    retval = Result::OK;
#else
    UNUSED(strength);
    UNUSED(step);
#endif

    return retval;
}

Return<void> PictureQuality::getChameleonStrength(getChameleonStrength_cb _hidl_cb) {
    Result retval = Result::NOT_SUPPORTED;
    int32_t strength = 0;
#ifdef CHAMELEON_DISPLAY_SUPPORT
    char value[PROPERTY_VALUE_MAX];
    Mutex::Autolock _l(mLock);

    if (property_get(MTK_CHAMELEON_STRENGTH_PROPERTY_NAME, value, NULL) > 0) {
        strength = atoi(value);
    } else {
        strength = chameleonDisplayProcess->getStrength();
    }

    retval = Result::OK;
#endif

    _hidl_cb(retval, strength);
    return Void();
}

Return<Result> PictureQuality::setTuningField(int32_t pqModule, int32_t field, int32_t value) {
    Result retval = Result::NOT_SUPPORTED;
    status_t status;

    status = setTuningField_impl(pqModule, field, value);
    if (status == android::NO_ERROR) {
        retval = Result::OK;
    }

    return retval;
}

Return<void> PictureQuality::getTuningField(int32_t pqModule, int32_t field, getTuningField_cb _hidl_cb) {
    Result retval = Result::NOT_SUPPORTED;
    status_t status;
    int32_t value = 0;

    status = getTuningField_impl(pqModule, field, &value);
    if (status == android::NO_ERROR) {
        retval = Result::OK;
    }

    _hidl_cb(retval, value);
    return Void();
}

Return<void> PictureQuality::getAshmem(getAshmem_cb _hidl_cb) {
    Result retval = Result::NOT_SUPPORTED;

    if (m_is_ashmem_init == true) {
        retval = Result::OK;
    }

    _hidl_cb(retval, m_hidl_memory);
    return Void();
}

Return<Result> PictureQuality::setAmbientLightCT(double input_x, double input_y, double input_Y) {
#ifdef MTK_CHAMELEON_DISPLAY_SUPPORT
    Mutex::Autolock _l(mLock);

    PQ_LOGD("[PQ_SERVICE] PQService : setAmbientLightCT(x:%f, y:%f, Y:%f)", input_x, input_y, input_Y);

    AmbientLightCTChange(input_x, input_y, input_Y);
    return Result::OK;
#else
    UNUSED(input_x);
    UNUSED(input_y);
    UNUSED(input_Y);
    return Result::NOT_SUPPORTED;
#endif
}

Return<Result> PictureQuality::setAmbientLightRGBW(int32_t input_R, int32_t input_G, int32_t input_B, int32_t input_W) {
#ifdef MTK_CHAMELEON_DISPLAY_SUPPORT
    Mutex::Autolock _l(mLock);

    int sensorChanged = forceSensorDebounce(input_R, input_G, input_B, input_W);

    PQ_LOGD("[PQ_SERVICE] PQService : setAmbientLightRGBW(R:%d, G:%d, B:%d, W:%d) (ret: %d)",
        input_R, input_G, input_B, input_W, sensorChanged);
    return Result::OK;
#else
    UNUSED(input_R);
    UNUSED(input_G);
    UNUSED(input_B);
    UNUSED(input_W);
    return Result::NOT_SUPPORTED;
#endif
}

Return<Result> PictureQuality::setGammaIndex(int32_t index, int32_t step) {
    Mutex::Autolock _l(mLock);
    Result retval = Result::NOT_SUPPORTED;

    PQ_LOGD("[PQ_SERVICE] PQService : setGammaIndex(%d)", index);

    if (0 <= index && index < GAMMA_INDEX_MAX) {
        char value[PROPERTY_VALUE_MAX];
        snprintf(value, PROPERTY_VALUE_MAX, "%d", index);
        property_set(GAMMA_INDEX_PROPERTY_NAME, value);

        m_gamma_id = index;
        _setGammaIndex(m_gamma_id);

        retval= Result::OK;
    }

    UNUSED(step);

    return retval;
}

Return<void> PictureQuality::getGammaIndex(getGammaIndex_cb _hidl_cb) {
    Result retval = Result::NOT_SUPPORTED;
    status_t status;
    int32_t index = 0;

    status = getGammaIndex_impl(&index);
    if (status == android::NO_ERROR) {
        retval = Result::OK;
    }

    _hidl_cb(retval, index);
    return Void();
}

Return<Result> PictureQuality::setExternalPanelNits(uint32_t externalPanelNits) {
    Mutex::Autolock _l(mLock);

    if (m_AshmemProxy->setTuningField(EXTERNAL_PANEL_NITS, externalPanelNits) < 0)
    {
        PQ_LOGE("[PQ_SERVICE] setExternalPanelNits : m_AshmemProxy->setTuningField() failed\n");
        return Result::INVALID_ARGUMENTS;
    }

    PQ_LOGD("[PQService] setExternalPanelNits[%d]", externalPanelNits);

    return Result::OK;
}

Return<void> PictureQuality::getExternalPanelNits(getExternalPanelNits_cb _hidl_cb) {
    Mutex::Autolock _l(mLock);
    Result retval = Result::NOT_SUPPORTED;
    int32_t getValue = 0;
    uint32_t externalPanelNits = 0;

    if (m_AshmemProxy->getTuningField(EXTERNAL_PANEL_NITS, &getValue) < 0)
    {
        PQ_LOGE("[PQ_SERVICE] getExternalPanelNits : m_AshmemProxy->getTuningField() failed\n");
        retval = Result::INVALID_STATE;
    } else {
        externalPanelNits = (getValue > 0) ? getValue : 0;
        PQ_LOGD("[PQService] getExternalPanelNits[%d]", externalPanelNits);
        retval = Result::OK;
    }

    _hidl_cb(retval, externalPanelNits);
    return Void();
}

Return<Result> PictureQuality::setColorTransform(const hidl_array<float, 4, 4>& matrix, int32_t hint, int32_t step) {
    bool color_transform_support = true;

    for (size_t i = 0 ; i < 3; i++) {
        if (matrix[3][i] != 0 || matrix[i][3] != 0) {
            color_transform_support = false;
        }
    }

#ifdef CCORR_OFF
    color_transform_support = false;
#endif

    UNUSED(step);
    UNUSED(hint);

    if (color_transform_support == true) {
        return Result::OK;
    } else {
        PQ_LOGD("[PQ_SERVICE] return Result::INVALID_ARGUMENTS");
        return Result::INVALID_ARGUMENTS;
    }
}

Return<void> PictureQuality::execIoctl(const dispPQIoctlParams& arg, execIoctl_cb _hidl_cb) {
    Result retval = Result::NOT_SUPPORTED;
    dispPQIoctlParams retParam;
    memset(&retParam, 0, sizeof(retParam));

#ifdef PQ_DEBUG
    Mutex::Autolock _l(mLock);

    int32_t ioctlRet = -1;

    DISP_READ_REG rParams;
    rParams.reg = arg.reg;
    rParams.val = arg.val;
    rParams.mask = arg.mask;

    DISP_WRITE_REG wParams;
    wParams.reg = arg.reg;
    wParams.val = arg.val;
    wParams.mask = arg.mask;

    PQ_LOGD("[PQService] arg.request[%d], arg.reg[%x], arg.val[%x], arg.mask[%x]",
                arg.request, arg.reg, arg.val, arg.mask);

    switch(arg.request)
    {
        case ioctlRequest::IOCTL_WRITE_REG:
            ioctlRet = ioctl(m_drvID, DISP_IOCTL_WRITE_REG, &wParams);
            break;
        case ioctlRequest::IOCTL_READ_REG:
            ioctlRet = ioctl(m_drvID, DISP_IOCTL_READ_REG, &rParams);
            break;
        case ioctlRequest::IOCTL_MUTEX_CONTROL:
            ioctlRet = ioctl(m_drvID, DISP_IOCTL_MUTEX_CONTROL, &(arg.val));
            break;
        case ioctlRequest::IOCTL_WRITE_SW_REG:
            ioctlRet = ioctl(m_drvID, DISP_IOCTL_WRITE_SW_REG, &wParams);
            break;
        case ioctlRequest::IOCTL_READ_SW_REG:
            ioctlRet = ioctl(m_drvID, DISP_IOCTL_READ_SW_REG, &rParams);
            break;
        default:
            PQ_LOGD("[PQService] ioctl Request not supported[%d]", arg.request);
            break;
    }

    if (ioctlRet >= 0)
    {
        retval = Result::OK;
        retParam.reg = rParams.reg;
        retParam.val = rParams.val;
        retParam.mask = rParams.mask;
        PQ_LOGD("[PQService] rParams.reg[%x], rParams.val[%x], rParams.mask[%x]",
                rParams.reg, rParams.val, rParams.mask);
    }
    else
    {
        PQ_LOGE("[PQService] execIoctl failed[%d]", arg.request);
    }
#else
    PQ_LOGE("[PQService] execIoctl is not supported on user load");
#endif

    _hidl_cb(retval, retParam);
    return Void();
}

Return<Result> PictureQuality::setRGBGain(uint32_t r_gain, uint32_t g_gain, uint32_t b_gain, int32_t step) {
    m_r_gain = r_gain;
    m_g_gain = g_gain;
    m_b_gain = b_gain;

    PQ_LOGD("[PQ_SERVICE] PQService : setRGBGain(R:%4d,G=%4d,B=%4d)",
        m_r_gain, m_g_gain, m_b_gain);

    setEvent(eEvtPQChange);
    setForceTransitionStep(step);
    refreshDisplay();
    return Result::OK;
}

Return<void> PictureQuality::debug(const ::android::hardware::hidl_handle& handle, const ::android::hardware::hidl_vec<::android::hardware::hidl_string>& options)  {
    //  validate handle
    if (handle == nullptr) {
        PQ_LOGE("bad handle:%p", handle.getNativeHandle());
        return Void();
    }
    else if (handle->numFds != 1) {
        PQ_LOGE("bad handle:%p numFds:%d", handle.getNativeHandle(), handle->numFds);
        return Void();
    }
    else if (handle->data[0] < 0) {
        PQ_LOGE("bad handle:%p numFds:%d fd:%d < 0", handle.getNativeHandle(), handle->numFds, handle->data[0]);
        return Void();
    }

    const int fd = handle->data[0];
    static const size_t SIZE = 4096;
    char *buffer;
    std::string result;

    buffer = new char[SIZE];
    buffer[0] = '\0';

    PQ_LOGD("[PQ_SERVICE] PQ interface debug function");

    // Try to get the main lock, but don't insist if we can't
    // (this would indicate PQ Service is stuck, but we want to be able to
    // print something in dumpsys).
    int retry = 3;
    while (mLock.tryLock() < 0 && --retry >= 0) {
        usleep(500 * 1000);
    }
    const bool locked(retry >= 0);
    if (!locked) {
        snprintf(buffer, SIZE,
            "PQService appears to be unresponsive, "
            "dumping anyways (no locks held)\n");
        result.append(buffer);
    } else {
        size_t numArgs = options.size();
        for (size_t argIndex = 0; argIndex < numArgs; ) {
            if (strncmp(options[argIndex].c_str(), "--ccorrdebug", options[argIndex].size()) == 0) {
                mCcorrDebug = true;
                snprintf(buffer, SIZE, "CCORR Debug On");
            } else if (strncmp(options[argIndex].c_str(), "--ccorrnormal", options[argIndex].size()) == 0) {
                mCcorrDebug = false;
                snprintf(buffer, SIZE, "CCORR Debug Off");
            } else if (strncmp(options[argIndex].c_str(), "--ccorrcoef", options[argIndex].size()) == 0) {
                int idx = 0;
                uint32_t coef[3][3];
                for (int y = 0; y < 3; y += 1) {
                    for (int x = 0; x < 3; x += 1) {
                        coef[y][x] = 0;
                    }
                }
                for (idx = 0; idx < 9; idx += 1) {
                    if (argIndex + 1 < numArgs) {
                        argIndex++;
                        coef[idx/3][idx%3] = asInt(options[argIndex].c_str());
                    } else {
                        break;
                    }
                }
                configCCorrCoef(m_drvID, coef);
                snprintf(buffer, SIZE, "%5d %5d %5d\n%5d %5d %5d\n%5d %5d %5d\n",
                    coef[0][0], coef[0][1], coef[0][2],
                    coef[1][0], coef[1][1], coef[1][2],
                    coef[2][0], coef[2][1], coef[2][2]);
            } else if (strncmp(options[argIndex].c_str(), "--gammaidx", options[argIndex].size()) == 0) {
                if (argIndex + 1 < numArgs) {
                    int gammaidx = 0;
                    argIndex++;
                    gammaidx = asInt(options[argIndex].c_str());
                    _setGammaIndex(gammaidx);
                    snprintf(buffer, SIZE, "set gamma index(%2d)", gammaidx);
                } else {
                    snprintf(buffer, SIZE, "format wrong");
                }
            } else if (strncmp(options[argIndex].c_str(), "--od", options[argIndex].size()) == 0) {
                if (argIndex + 1 < numArgs) {
                    int en = asInt(options[argIndex + 1].c_str());
                    snprintf(buffer, SIZE, "OD enabled = %d\n", en);
                    enableDisplayOverDrive(en);
                }
            } else if (strncmp(options[argIndex].c_str(), "--debug_level", options[argIndex].size()) == 0) {
                if (argIndex + 1 < numArgs) {
                    argIndex++;
                    mDebugLevel = asInt(options[argIndex].c_str());
                    snprintf(buffer, SIZE, "Debug level = 0x%x\n", mDebugLevel);
                }
            } else if (strncmp(options[argIndex].c_str(), "--nvgamma", options[argIndex].size()) == 0) {
#ifdef FACTORY_GAMMA_SUPPORT
                if (argIndex + 1 < numArgs) {
                    argIndex++;
                    if (strncmp(options[argIndex].c_str(), "set", options[argIndex].size()) == 0) {
                        loadNvGammaTable();
                        gamma_entry_t *entry = &(m_NvGamma[0][GAMMA_INDEX_DEFAULT]);
                        configGamma(0, entry);
                        snprintf(buffer, SIZE, "set NV Gamma");
                    } else if (strncmp(options[argIndex].c_str(), "status", options[argIndex].size()) == 0) {
                        snprintf(buffer, SIZE, "get NV Gamma status: %d", m_NvGammaStatus);
                    }
                }
#else
                snprintf(buffer, SIZE, "Factory Gamma is not supported\n");
#endif
            } else if (strncmp(options[argIndex].c_str(), "--custgamma", options[argIndex].size()) == 0) {
                gamma_entry_t *entry = &(m_CustGamma[0][GAMMA_INDEX_DEFAULT]);
                configGamma(0, entry);
                snprintf(buffer, SIZE, "Cust Gamma");
            } else if (strncmp(options[argIndex].c_str(), "--bl", options[argIndex].size()) == 0) {
#ifdef BLULIGHT_DEFENDER_SUPPORT
                if (argIndex + 1 < numArgs) {
                    bool enabled = blueLight->isEnabled();
                    argIndex++;
                    if (strncmp(options[argIndex].c_str(), "on", options[argIndex].size()) == 0 || strncmp(options[argIndex].c_str(), "1", options[argIndex].size()) == 0)
                        enabled = true;
                    else
                        enabled = false;
                    blueLight->setEnabled(enabled);
                    snprintf(buffer, SIZE, "Blue light: %s\n", (enabled ? "on" : "off"));
                    result.append(buffer);
                    snprintf(buffer, SIZE, "Strength: %d\n", blueLight->getStrength());
                    refreshDisplay();
                } else {
                    snprintf(buffer, SIZE, "on/off?\n");
                }
#else
                snprintf(buffer, SIZE, "BluLight Defender is not supported\n");
#endif
            } else if (strncmp(options[argIndex].c_str(), "--bl_debug", options[argIndex].size()) == 0) {
#ifdef BLULIGHT_DEFENDER_SUPPORT
                if (argIndex + 1 < numArgs) {
                    argIndex++;
                    mBlueLightDebugFlag = asInt(options[argIndex].c_str());
                    blueLight->setDebugFlag(mBlueLightDebugFlag);
                    snprintf(buffer, SIZE, "Debug level = 0x%x\n", mBlueLightDebugFlag);
                }
#else
                snprintf(buffer, SIZE, "BluLight Defender is not supported\n");
#endif
            } else if (strncmp(options[argIndex].c_str(), "--pqparam_debug_shp", options[argIndex].size()) == 0) {
                setDebuggingPqparam(PQDEBUG_SHP_VALUE, asInt(options[argIndex + 1].c_str()), asInt(options[argIndex + 2].c_str()), asInt(options[argIndex + 3].c_str()));
                snprintf(buffer, SIZE, "set mode: %d, scenario: %d, shp level = %d\n", asInt(options[argIndex + 1].c_str()), asInt(options[argIndex + 2].c_str()), asInt(options[argIndex + 3].c_str()));
                argIndex = argIndex + 3;
            } else if (strncmp(options[argIndex].c_str(), "--pqparam_debug_on", options[argIndex].size()) == 0) {
                setDebuggingPqparam(PQDEBUG_ON,0,0,0);
                snprintf(buffer, SIZE, "pqparam_debug On\n");
            } else if (strncmp(options[argIndex].c_str(), "--pqparam_debug_off", options[argIndex].size()) == 0) {
                setDebuggingPqparam(PQDEBUG_OFF,0,0,0);
                snprintf(buffer, SIZE, "pqparam_debug Off\n");
            } else if (strncmp(options[argIndex].c_str(), "--Write", options[argIndex].size()) == 0) { // add for dvt
#ifdef PQ_DEBUG
                argIndex++;
                DISP_WRITE_REG params;
                params.reg = asInt(options[argIndex++].c_str());
                params.val = asInt(options[argIndex].c_str());
                params.mask = 0xFFFFFFFF;
                ioctl(m_drvID, DISP_IOCTL_WRITE_REG, &params);
                snprintf(buffer, SIZE, "W: 0x%08x = %08x", params.reg, params.val);
#else
                snprintf(buffer, SIZE, "HW Write cmd not supported in user load");
#endif
            } else if (strncmp(options[argIndex].c_str(), "--Read", options[argIndex].size()) == 0) { // add for dvt
#ifdef PQ_DEBUG
                argIndex++;
                DISP_READ_REG params;
                params.reg = asInt(options[argIndex].c_str());
                params.val = 0;
                params.mask = 0xFFFFFFFF;
                ioctl(m_drvID, DISP_IOCTL_READ_REG, &params);

                snprintf(buffer, SIZE, "R: 0x%08x = %08x", params.reg, params.val);
#else
                snprintf(buffer, SIZE, "HW Read cmd not supported in user load");
#endif
            } else if (strncmp(options[argIndex].c_str(), "--disp_color", options[argIndex].size()) == 0) {
                uint32_t enabled;
                argIndex++;
                if (strncmp(options[argIndex].c_str(), "on", options[argIndex].size()) == 0 || strncmp(options[argIndex].c_str(), "1", options[argIndex].size()) == 0)
                    enabled = 1;
                else
                    enabled = 0;
                enableDisplayColor(enabled);
                snprintf(buffer, SIZE, "DISP_COLOR: %s\n", (enabled ? "on" : "bypass"));
            } else if (strncmp(options[argIndex].c_str(), "--query_ur", options[argIndex].size()) == 0) {
#ifdef SUPPORT_UR
                snprintf(buffer, SIZE, "UR is supported: yes\n");
#else
                snprintf(buffer, SIZE, "UR is supported: no\n");
#endif
            } else if (strncmp(options[argIndex].c_str(), "--pqparam_debug_property", options[argIndex].size()) == 0) {
                int32_t feature_switch;
                map<string, ASHMEM_ENUM>::iterator iter;

                if (argIndex + 1 < numArgs)
                {
                    argIndex++;

                    iter = m_mapPQProperty.find(options[argIndex].c_str());
                    snprintf(buffer, SIZE, "Property name = %s, ", options[argIndex].c_str());
                    result.append(buffer);

                    if (iter != m_mapPQProperty.end())
                    {
                        feature_switch = asInt(options[++argIndex].c_str());
                        snprintf(buffer, SIZE, "value =  %d\n", feature_switch);
                        m_AshmemProxy->setTuningField(iter->second, feature_switch);

                        property_set(iter->first.c_str(), options[argIndex].c_str());
                    }
                    else
                    {
                        snprintf(buffer, SIZE, "Set pqparam_debug_property failed\n");
                    }
                }
                else
                {
                    for (iter = m_mapPQProperty.begin(); iter != m_mapPQProperty.end(); ++iter)
                    {
                        result.append(buffer);
                        m_AshmemProxy->getTuningField(iter->second, &feature_switch);
                        snprintf(buffer, SIZE, "Property name = %s, value = %d\n", iter->first.c_str(), feature_switch);
                    }
                }
            } else if (strncmp(options[argIndex].c_str(), "--chameleon", options[argIndex].size()) == 0) {
#ifdef CHAMELEON_DISPLAY_SUPPORT
                if (argIndex + 1 < numArgs) {
                    bool enabled = chameleonDisplayProcess->isEnabled();
                    argIndex++;
                    if (strncmp(options[argIndex].c_str(), "on", options[argIndex].size()) == 0 || strncmp(options[argIndex].c_str(), "1", options[argIndex].size()) == 0)
                        enabled = true;
                    else
                        enabled = false;

                    chameleonDisplayProcess->setEnabled(enabled);
                    mLightSensor->setEnabled(enabled);

                    setEvent(eEvtChameleon);
                    if (mSensorInputxyY == true || mDebug.sensorDebugMode == 1) {
                        setEvent(eEvtALI);
                    }

                    snprintf(buffer, SIZE, "Chameleon: %s\n", (enabled ? "on" : "off"));
                    refreshDisplay();
                } else {
                    snprintf(buffer, SIZE, "on/off?\n");
                }
#else
                snprintf(buffer, SIZE, "Chameleon is not supported\n");
#endif
            } else if (strncmp(options[argIndex].c_str(), "--transition", options[argIndex].size()) == 0) {
#ifdef TRANSITION_SUPPORT
                if (argIndex + 1 < numArgs) {
                    bool enabled = mPQTransition->isEnabled();
                    argIndex++;
                    if (strncmp(options[argIndex].c_str(), "on", options[argIndex].size()) == 0 || strncmp(options[argIndex].c_str(), "1", options[argIndex].size()) == 0)
                        enabled = true;
                    else
                        enabled = false;
                    mPQTransition->setEnabled(enabled);
                    snprintf(buffer, SIZE, "Transition: %s\n", (enabled ? "on" : "off"));
                    refreshDisplay();
                } else {
                    snprintf(buffer, SIZE, "on/off?\n");
                }
#else
                snprintf(buffer, SIZE, "Transition is not supported\n");
#endif
            } else if (strncmp(options[argIndex].c_str(), "--wALI", options[argIndex].size()) == 0) {
                if (argIndex + 4 < numArgs) {
                    int32_t aliR = 0;
                    int32_t aliG = 0;
                    int32_t aliB = 0;
                    int32_t aliW = 0;

                    argIndex++;
                    aliR = asInt(options[argIndex].c_str());
                    argIndex++;
                    aliG = asInt(options[argIndex].c_str());
                    argIndex++;
                    aliB = asInt(options[argIndex].c_str());
                    argIndex++;
                    aliW = asInt(options[argIndex].c_str());

                    int sensorChanged = forceSensorDebounce(aliR, aliG, aliB, aliW);

                    snprintf(buffer, SIZE, "Overwrite ALI(ret=%d) R=%4d G=%4d B=%4d W=%4d\n",
                        sensorChanged, aliR, aliG, aliB, aliW);
                } else {
                    snprintf(buffer, SIZE, "R G B W?\n");
                }
            } else if (strncmp(options[argIndex].c_str(), "--wAmbientLightCT", options[argIndex].size()) == 0) {
#ifdef CHAMELEON_DISPLAY_SUPPORT
                if (argIndex + 3 < numArgs) {
                    double input_x, input_y, input_Y;

                    argIndex++;
                    input_x = (double)asInt(options[argIndex].c_str()) / 10000.0f;
                    argIndex++;
                    input_y = (double)asInt(options[argIndex].c_str()) / 10000.0f;
                    argIndex++;
                    input_Y = (double)asInt(options[argIndex].c_str()) / 10000.0f;

                    AmbientLightCTChange(input_x, input_y, input_Y);

                    snprintf(buffer, SIZE, "Overwrite Ambient Light Color Temperature x=%f y=%f Y=%f\n",
                        input_x, input_y, input_Y);
                } else {
                    snprintf(buffer, SIZE, "x y Y?\n");
                }
#else
                snprintf(buffer, SIZE, "Chameleon is not supported\n");
#endif
            } else if (strncmp(options[argIndex].c_str(), "--setTuning", options[argIndex].size()) == 0) {
                int module;
                int field;
                int value;
                argIndex++;
                module = asInt(options[argIndex].c_str());
                argIndex++;
                field = asInt(options[argIndex].c_str());
                argIndex++;
                value = asInt(options[argIndex].c_str());
                setTuningField(module, field, value);
                snprintf(buffer, SIZE, "setTuning %8x %8x %8x\n", module, field, value);
            } else if (strncmp(options[argIndex].c_str(), "--getTuning", options[argIndex].size()) == 0) {
                int module;
                int field;
                int value;
                argIndex++;
                module = asInt(options[argIndex].c_str());
                argIndex++;
                field = asInt(options[argIndex].c_str());
                getTuningField_impl(module, field, &value);
                snprintf(buffer, SIZE, "getTuning %8x %8x %8x\n", module, field, value);
            }
#ifdef CHAMELEON_DISPLAY_SUPPORT
             else if (strncmp(options[argIndex].c_str(), "--dumpfield", options[argIndex].size()) == 0) {
                int size;
                int *fieldPtr;
                fieldPtr = reinterpret_cast<int*>(mChameleonInput);
                if (mChameleonInput != NULL) {
                    PQ_LOGD("dumpfield>--------------input--------------");
                    size = sizeof(TChameleonDisplayInput);
                    for (int i=0; i < size; i += 4)
                        PQ_LOGD("dumpfield %4d %08x\n", i/4, *(fieldPtr+i/4));
                }
                fieldPtr = reinterpret_cast<int*>(mChameleonOutput);
                if (mChameleonOutput != NULL) {
                    PQ_LOGD("dumpfield>-------------output--------------");
                    size = sizeof(TChameleonDisplayOutput);
                    for (int i=0; i < size; i += 4)
                        PQ_LOGD("dumpfield %4d %08x\n", i/4, *(fieldPtr+i/4));
                }
            }
#endif
             else if (strncmp(options[argIndex].c_str(), "--setExternalPanelNits", options[argIndex].size()) == 0) {
                argIndex++;
                m_AshmemProxy->setTuningField(EXTERNAL_PANEL_NITS, asInt(options[argIndex].c_str()));
                snprintf(buffer, SIZE, "setExternalPanelNits[%d]", asInt(options[argIndex].c_str()));
            } else if (strncmp(options[argIndex].c_str(), "--getExternalPanelNits", options[argIndex].size()) == 0) {
                uint32_t externalPanelNits;
                m_AshmemProxy->getTuningField(EXTERNAL_PANEL_NITS, reinterpret_cast<int32_t*>(&externalPanelNits));
                snprintf(buffer, SIZE, "getExternalPanelNits[%d]", externalPanelNits);
            }
            argIndex += 1;
        }
        result.append(buffer);
    }

    if (locked) {
       mLock.unlock();
    }

    write(fd, result.c_str(), result.size());
    dprintf(fd, "\n");

    delete [] buffer;

    return Void();
}

void PictureQuality::setEvent(unsigned int event) {
    mEventFlags |= event;
#ifdef CHAMELEON_DISPLAY_SUPPORT
    // forece chameleon generate debounce als then calculate
    if (isEventSet(eEvtPQChange) || isEventSet(eEvtChameleon) || isEventSet(eEvtBacklightChange)) {
        mAllowSensorDebounce = true;
        chameleonDisplayProcess->setForceChange(1);
    }
#endif
}

void PictureQuality::setForceTransitionStep(int step) {
#ifdef TRANSITION_SUPPORT
    if (step != PQ_DEFAULT_TRANSITION_ON_STEP) {
        if (step >= PQ_MAX_FORCE_TRANSITION_STEP) {
            step = PQ_MAX_FORCE_TRANSITION_STEP;
        } else if (step < PQ_MIN_FORCE_TRANSITION_STEP) {
            step = PQ_MIN_FORCE_TRANSITION_STEP;
        }

        // Always use faster step
        if (mForceTransitionStep == PQ_DEFAULT_TRANSITION_ON_STEP || step < mForceTransitionStep) {
            mForceTransitionStep = step;
            PQ_LOGD("[PQ_SERVICE] Set transition step to %d\n", mForceTransitionStep);
        }
    }
#else
    UNUSED(step);
#endif
}

void PictureQuality::ClearForceTransitionStep(void) {
#ifdef TRANSITION_SUPPORT
    mForceTransitionStep = PQ_DEFAULT_TRANSITION_ON_STEP;
#endif
}

void PictureQuality::calcPQStrength(void *pqparam_dst, DISP_PQ_PARAM *pqparam_src, int percentage)
{
    dispPQIndexParams *index = static_cast<dispPQIndexParams*>(pqparam_dst);

    memcpy(index, pqparam_src, sizeof(DISP_PQ_PARAM));

    index->u4SatGain = pqparam_src->u4SatGain * percentage / 100;
    index->u4SatAdj[1] = pqparam_src->u4SatAdj[1] * percentage / 100;
    index->u4SatAdj[2] = pqparam_src->u4SatAdj[2] * percentage / 100;
    index->u4SatAdj[3] = pqparam_src->u4SatAdj[3] * percentage / 100;
    index->u4Contrast = pqparam_src->u4Contrast * percentage / 100;
    index->u4Brightness = pqparam_src->u4Brightness * percentage / 100;
    index->u4SHPGain = pqparam_src->u4SHPGain * percentage / 100;
}

void PictureQuality::getUserModePQParam()
{
    char value[PROPERTY_VALUE_MAX];
    int i;

    property_get(PQ_TDSHP_PROPERTY_STR, value, PQ_TDSHP_INDEX_DEFAULT);
    i = atoi(value);
    PQ_LOGD("[PQ_SERVICE] property get... tdshp[%d]", i);
    m_pic_userdef->getPQUserDefParam()->u4SHPGain = i;

    property_get(PQ_GSAT_PROPERTY_STR, value, PQ_GSAT_INDEX_DEFAULT);
    i = atoi(value);
    PQ_LOGD("[PQ_SERVICE] property get... gsat[%d]", i);
    m_pic_userdef->getPQUserDefParam()->u4SatGain = i;

    property_get(PQ_CONTRAST_PROPERTY_STR, value, PQ_CONTRAST_INDEX_DEFAULT);
    i = atoi(value);
    PQ_LOGD("[PQ_SERVICE] property get... contrast[%d]", i);
    m_pic_userdef->getPQUserDefParam()->u4Contrast = i;

    property_get(PQ_PIC_BRIGHT_PROPERTY_STR, value, PQ_PIC_BRIGHT_INDEX_DEFAULT);
    i = atoi(value);
    PQ_LOGD("[PQ_SERVICE] property get... pic bright[%d]", i);
    m_pic_userdef->getPQUserDefParam()->u4Brightness = i;
}

int32_t PictureQuality::getPQStrengthRatio(int scenario)
{
    if (scenario ==  SCENARIO_PICTURE)
    {
        return m_pqparam_mapping.image;
    }
    else if (scenario == SCENARIO_VIDEO || scenario == SCENARIO_VIDEO_CODEC)
    {
        return m_pqparam_mapping.video;
    }
    else if (scenario == SCENARIO_ISP_PREVIEW || scenario == SCENARIO_ISP_CAPTURE)
    {
        return m_pqparam_mapping.camera;
    }
    else
    {
        PQ_LOGD("[PQ_SERVICE] invalid scenario %d\n",scenario);
        return m_pqparam_mapping.image;
    }
}

void PictureQuality::initPQProperty(void)
{
    char value[PROPERTY_VALUE_MAX];

    m_mapPQProperty[PQ_DBG_SHP_EN_STR] = SHP_ENABLE;
    property_get(PQ_DBG_SHP_EN_STR, value, PQ_DBG_SHP_EN_DEFAULT);
    property_set(PQ_DBG_SHP_EN_STR, value);
    m_AshmemProxy->setTuningField(SHP_ENABLE, atoi(value));
    PQ_LOGD("[PQ_SERVICE] PQ_DBG_SHP_EN_STR[%d]\n", atoi(value));

    m_mapPQProperty[PQ_DBG_DSHP_EN_STR] = DSHP_ENABLE;
    property_get(PQ_DBG_DSHP_EN_STR, value, PQ_DBG_DSHP_EN_DEFAULT);
    property_set(PQ_DBG_DSHP_EN_STR, value);
    m_AshmemProxy->setTuningField(DSHP_ENABLE, atoi(value));
    PQ_LOGD("[PQ_SERVICE] PQ_DBG_DSHP_EN_STR[%d]\n", atoi(value));

    m_mapPQProperty[PQ_MDP_COLOR_EN_STR] = VIDEO_CONTENT_COLOR_ENABLE;
    property_get(PQ_MDP_COLOR_EN_STR, value, PQ_MDP_COLOR_EN_DEFAULT);
    property_set(PQ_MDP_COLOR_EN_STR, value);
    m_AshmemProxy->setTuningField(VIDEO_CONTENT_COLOR_ENABLE, atoi(value));
    PQ_LOGD("[PQ_SERVICE] PQ_MDP_COLOR_EN_STR[%d]\n", atoi(value));

    m_mapPQProperty[PQ_ADL_PROPERTY_STR] = DC_ENABLE;
    property_get(PQ_ADL_PROPERTY_STR, value, PQ_ADL_INDEX_DEFAULT);
    property_set(PQ_ADL_PROPERTY_STR, value);
    m_AshmemProxy->setTuningField(DC_ENABLE, atoi(value));
    PQ_LOGD("[PQ_SERVICE] PQ_ADL_PROPERTY_STR[%d]\n", atoi(value));

    m_mapPQProperty[PQ_ISO_SHP_EN_STR] = ISO_SHP_ENABLE;
    property_get(PQ_ISO_SHP_EN_STR, value, PQ_ISO_SHP_EN_DEFAULT);
    property_set(PQ_ISO_SHP_EN_STR, value);
    m_AshmemProxy->setTuningField(ISO_SHP_ENABLE, atoi(value));
    PQ_LOGD("[PQ_SERVICE] PQ_ISO_SHP_EN_STR[%d]\n", atoi(value));

    m_mapPQProperty[PQ_ULTRARES_EN_STR] = UR_ENABLE;
    property_get(PQ_ULTRARES_EN_STR, value, PQ_ULTRARES_EN_DEFAULT);
    property_set(PQ_ULTRARES_EN_STR, value);
    m_AshmemProxy->setTuningField(UR_ENABLE, atoi(value));
    PQ_LOGD("[PQ_SERVICE] PQ_ULTRARES_EN_STR[%d]\n", atoi(value));

    m_mapPQProperty[PQ_MDP_COLOR_DBG_EN_STR] = CONTENT_COLOR_ENABLE;
    property_get(PQ_MDP_COLOR_DBG_EN_STR, value, PQ_MDP_COLOR_DBG_EN_DEFAULT);
    property_set(PQ_MDP_COLOR_DBG_EN_STR, value);
    m_AshmemProxy->setTuningField(CONTENT_COLOR_ENABLE, atoi(value));
    PQ_LOGD("[PQ_SERVICE] PQ_MDP_COLOR_DBG_EN_STR[%d]\n", atoi(value));

    m_mapPQProperty[PQ_LOG_EN_STR] = PQ_LOG_ENABLE;
    property_get(PQ_LOG_EN_STR, value, PQ_LOG_EN_DEFAULT);
    property_set(PQ_LOG_EN_STR, value);
    m_AshmemProxy->setTuningField(PQ_LOG_ENABLE, atoi(value));
    PQ_LOGD("[PQ_SERVICE] PQ_LOG_ENABLE[%d]\n", atoi(value));

    m_mapPQProperty[PQ_HDR_VIDEO_EN_STR] = HDR_VIDEO_ENABLE;
    property_get(PQ_HDR_VIDEO_EN_STR, value, PQ_HDR_VIDEO_EN_DEFAULT);
    property_set(PQ_HDR_VIDEO_EN_STR, value);
    m_AshmemProxy->setTuningField(HDR_VIDEO_ENABLE, atoi(value));
    PQ_LOGD("[PQ_SERVICE] HDR_VIDEO_ENABLE[%d]\n", atoi(value));

    m_mapPQProperty[PQ_DBG_ADL_DEBUG_STR] = DC_DEBUG_FLAG;
    property_get(PQ_DBG_ADL_DEBUG_STR, value, PQ_DBG_ADL_DEBUG_DEFAULT);
    property_set(PQ_DBG_ADL_DEBUG_STR, value);
    m_AshmemProxy->setTuningField(DC_DEBUG_FLAG, atoi(value));
    PQ_LOGD("[PQ_SERVICE] DC_DEBUG_FLAG[%d]\n", atoi(value));

    m_mapPQProperty[PQ_DBG_HDR_DEBUG_STR] = HDR_DEBUG_FLAG;
    property_get(PQ_DBG_HDR_DEBUG_STR, value, PQ_DBG_HDR_DEBUG_DEFAULT);
    property_set(PQ_DBG_HDR_DEBUG_STR, value);
    m_AshmemProxy->setTuningField(HDR_DEBUG_FLAG, atoi(value));
    PQ_LOGD("[PQ_SERVICE] HDR_DEBUG_FLAG[%d]\n", atoi(value));

    m_AshmemProxy->setTuningField(EXTERNAL_PANEL_NITS, EXTERNAL_PANEL_NITS_DEFAULT);
    PQ_LOGD("[PQ_SERVICE] PANEL_NITS[%d]\n", EXTERNAL_PANEL_NITS_DEFAULT);

    m_mapPQProperty[PQ_MDP_CCORR_EN_STR] = MDP_CCORR_DEBUG_FLAG;
    property_get(PQ_MDP_CCORR_EN_STR, value, PQ_MDP_CCORR_EN_DEFAULT);
    property_set(PQ_MDP_CCORR_EN_STR, value);
    m_AshmemProxy->setTuningField(MDP_CCORR_DEBUG_FLAG, atoi(value));
    PQ_LOGD("[PQ_SERVICE] MDP_CCORR_DEBUG_FLAG[%d]\n", atoi(value));

    m_mapPQProperty[PQ_MDP_DRE_EN_STR] = MDP_DRE_ENABLE;
    property_get(PQ_MDP_DRE_EN_STR, value, PQ_MDP_DRE_EN_DEFAULT);
    property_set(PQ_MDP_DRE_EN_STR, value);
    m_AshmemProxy->setTuningField(MDP_DRE_ENABLE, atoi(value));
    PQ_LOGD("[PQ_SERVICE] MDP_DRE_ENABLE[%d]\n", atoi(value));

    m_mapPQProperty[PQ_DBG_MDP_DRE_DEBUG_STR] = MDP_DRE_DEBUG_FLAG;
    property_get(PQ_DBG_MDP_DRE_DEBUG_STR, value, PQ_DBG_MDP_DRE_DEBUG_DEFAULT);
    property_set(PQ_DBG_MDP_DRE_DEBUG_STR, value);
    m_AshmemProxy->setTuningField(MDP_DRE_DEBUG_FLAG, atoi(value));
    PQ_LOGD("[PQ_SERVICE] MDP_DRE_DEBUG_FLAG[%d]\n", atoi(value));

    m_mapPQProperty[PQ_DBG_ADAPTIVE_CALTM_DEBUG_STR] = ADAPTIVE_CALTM_DEBUG_FLAG;
    property_get(PQ_DBG_ADAPTIVE_CALTM_DEBUG_STR, value, PQ_DBG_ADAPTIVE_CALTM_DEBUG_DEFAULT);
    property_set(PQ_DBG_ADAPTIVE_CALTM_DEBUG_STR, value);
    m_AshmemProxy->setTuningField(ADAPTIVE_CALTM_DEBUG_FLAG, atoi(value));
    PQ_LOGD("[PQ_SERVICE] ADAPTIVE_CALTM_DEBUG_FLAG[%d]\n", atoi(value));

    m_mapPQProperty[PQ_DBG_MDP_DREDRIVER_DEBUG_STR] = MDP_DREDRIVER_DEBUG_FLAG;
    property_get(PQ_DBG_MDP_DREDRIVER_DEBUG_STR, value, PQ_DBG_MDP_DREDRIVER_DEBUG_DEFAULT);
    property_set(PQ_DBG_MDP_DREDRIVER_DEBUG_STR, value);
    m_AshmemProxy->setTuningField(MDP_DREDRIVER_DEBUG_FLAG, atoi(value));
    PQ_LOGD("[PQ_SERVICE] MDP_DREDRIVER_DEBUG_FLAG[%d]\n", atoi(value));

    m_mapPQProperty[PQ_DBG_MDP_DRE_DEMO_WIN_X_STR] = MDP_DRE_DEMOWIN_X_FLAG;
    property_get(PQ_DBG_MDP_DRE_DEMO_WIN_X_STR, value, PQ_DBG_MDP_DRE_DEMO_WIN_X_DEFAULT);
    property_set(PQ_DBG_MDP_DRE_DEMO_WIN_X_STR, value);
    m_AshmemProxy->setTuningField(MDP_DRE_DEMOWIN_X_FLAG, atoi(value));
    PQ_LOGD("[PQ_SERVICE] MDP_DRE_DEMOWIN_X_FLAG[%d]\n", atoi(value));

    m_mapPQProperty[PQ_DBG_MDP_DREDRIVER_BLK_STR] = MDP_DREDRIVER_BLK_FLAG;
    property_get(PQ_DBG_MDP_DREDRIVER_BLK_STR, value, PQ_DBG_MDP_DREDRIVER_BLK_DEFAULT);
    property_set(PQ_DBG_MDP_DREDRIVER_BLK_STR, value);
    m_AshmemProxy->setTuningField(MDP_DREDRIVER_BLK_FLAG, atoi(value));
    PQ_LOGD("[PQ_SERVICE] MDP_DREDRIVER_BLK_FLAG[%d]\n", atoi(value));

    m_mapPQProperty[PQ_DBG_MDP_DRE_ISP_TUNING_STR] = MDP_DRE_ISPTUNING_FLAG;
    property_get(PQ_DBG_MDP_DRE_ISP_TUNING_STR, value, PQ_DBG_MDP_DRE_ISP_TUNING_DEFAULT);
    property_set(PQ_DBG_MDP_DRE_ISP_TUNING_STR, value);
    m_AshmemProxy->setTuningField(MDP_DRE_ISPTUNING_FLAG, atoi(value));
    PQ_LOGD("[PQ_SERVICE] MDP_DRE_ISPTUNING_FLAG[%d]\n", atoi(value));

    m_mapPQProperty[PQ_DBG_MDP_CZ_ISP_TUNING_STR] = MDP_CZ_ISPTUNING_FLAG;
    property_get(PQ_DBG_MDP_CZ_ISP_TUNING_STR, value, PQ_DBG_MDP_CZ_ISP_TUNING_DEFAULT);
    property_set(PQ_DBG_MDP_CZ_ISP_TUNING_STR, value);
    m_AshmemProxy->setTuningField(MDP_CZ_ISPTUNING_FLAG, atoi(value));
    PQ_LOGD("[PQ_SERVICE] MDP_CZ_ISPTUNING_FLAG[%d]\n", atoi(value));
}

bool PictureQuality::isStandardPictureMode(int32_t mode, int32_t scenario_index)
{
    if (mode == PQ_PIC_MODE_STANDARD && scenario_index < PQ_SCENARIO_COUNT)
        return true;
    else
        return false;
}

bool PictureQuality::isVividPictureMode(int32_t mode, int32_t scenario_index)
{
    if (mode == PQ_PIC_MODE_VIVID && scenario_index < PQ_SCENARIO_COUNT)
        return true;
    else
        return false;
}

bool PictureQuality::isUserDefinedPictureMode(int32_t mode, int32_t scenario_index)
{
    if (mode == PQ_PIC_MODE_USER_DEF && scenario_index < PQ_SCENARIO_COUNT)
        return true;
    else
        return false;
}

void PictureQuality::setPQParamlevel(DISP_PQ_PARAM *pqparam_image_ptr, int32_t index, int32_t level)
{
    switch (index) {
        case SET_PQ_SHP_GAIN:
            {
                if (level >= 0 && level < THSHP_TUNING_INDEX)
                {
                    pqparam_image_ptr->u4SHPGain = level;
                    PQ_LOGD("[PQ_SERVICE] setPQIndex SET_PQ_SHP_GAIN...[%d]\n", level);
                }
                else
                {
                    PQ_LOGE("[PQ_SERVICE] setPQIndex SET_PQ_SHP_GAIN out of range...[%d]\n", level);
                }
            }
            break;
        case SET_PQ_SAT_GAIN:
            {
                if (level >= 0 && level < GLOBAL_SAT_SIZE)
                {
                    pqparam_image_ptr->u4SatGain = level;
                    PQ_LOGD("[PQ_SERVICE] setPQIndex SET_PQ_SAT_GAIN...[%d]\n", level);
                }
                else
                {
                    PQ_LOGE("[PQ_SERVICE] setPQIndex SET_PQ_SAT_GAIN out of range...[%d]\n", level);
                }
            }
            break;
        case SET_PQ_LUMA_ADJ:
            {
                if (level >= 0 && level < PARTIAL_Y_INDEX)
                {
                    pqparam_image_ptr->u4PartialY= level;
                    PQ_LOGD("[PQ_SERVICE] setPQIndex SET_PQ_LUMA_ADJ...[%d]\n", level);
                }
                else
                {
                    PQ_LOGE("[PQ_SERVICE] setPQIndex SET_PQ_LUMA_ADJ out of range...[%d]\n", level);
                }
            }
            break;
        case  SET_PQ_HUE_ADJ_SKIN:
            {
                if (level >= 0 && level < COLOR_TUNING_INDEX)
                {
                    pqparam_image_ptr->u4HueAdj[1]= level;
                    PQ_LOGD("[PQ_SERVICE] setPQIndex SET_PQ_HUE_ADJ_SKIN...[%d]\n", level);
                }
                else
                {
                    PQ_LOGE("[PQ_SERVICE] setPQIndex SET_PQ_HUE_ADJ_SKIN out of range...[%d]\n", level);
                }
            }
            break;
        case  SET_PQ_HUE_ADJ_GRASS:
            {
                if (level >= 0 && level < COLOR_TUNING_INDEX)
                {
                    pqparam_image_ptr->u4HueAdj[2]= level;
                    PQ_LOGD("[PQ_SERVICE] setPQIndex SET_PQ_HUE_ADJ_GRASS...[%d]\n", level);
                }
                else
                {
                    PQ_LOGE("[PQ_SERVICE] setPQIndex SET_PQ_HUE_ADJ_GRASS out of range...[%d]\n", level);
                }
            }
            break;
        case  SET_PQ_HUE_ADJ_SKY:
            {
                if (level >= 0 && level < COLOR_TUNING_INDEX)
                {
                    pqparam_image_ptr->u4HueAdj[3]= level;
                    PQ_LOGD("[PQ_SERVICE] setPQIndex SET_PQ_HUE_ADJ_SKY...[%d]\n", level);
                }
                else
                {
                    PQ_LOGE("[PQ_SERVICE] setPQIndex SET_PQ_HUE_ADJ_SKY out of range...[%d]\n", level);
                }
            }
            break;
        case SET_PQ_SAT_ADJ_SKIN:
            {
                if (level >= 0 && level < COLOR_TUNING_INDEX)
                {
                    pqparam_image_ptr->u4SatAdj[1]= level;
                    PQ_LOGD("[PQ_SERVICE] setPQIndex SET_PQ_SAT_ADJ_SKIN...[%d]\n", level);
                }
                else
                {
                    PQ_LOGE("[PQ_SERVICE] setPQIndex SET_PQ_SAT_ADJ_SKIN out of range...[%d]\n", level);
                }
            }
            break;
        case SET_PQ_SAT_ADJ_GRASS:
            {
                if (level >= 0 && level < COLOR_TUNING_INDEX)
                {
                    pqparam_image_ptr->u4SatAdj[2]= level;
                    PQ_LOGD("[PQ_SERVICE] setPQIndex SET_PQ_SAT_ADJ_GRASS...[%d]\n", level);
                }
                else
                {
                    PQ_LOGE("[PQ_SERVICE] setPQIndex SET_PQ_SAT_ADJ_GRASS out of range...[%d]\n", level);
                }
            }
            break;
        case SET_PQ_SAT_ADJ_SKY:
            {
                if (level >= 0 && level < COLOR_TUNING_INDEX)
                {
                    pqparam_image_ptr->u4SatAdj[3]= level;
                    PQ_LOGD("[PQ_SERVICE] setPQIndex SET_PQ_SAT_ADJ_SKY...[%d]\n", level);
                }
                else
                {
                    PQ_LOGE("[PQ_SERVICE] setPQIndex SET_PQ_SAT_ADJ_SKY out of range...[%d]\n", level);
                }
            }
            break;
        case SET_PQ_CONTRAST:
            {
                if (level >= 0 && level < CONTRAST_SIZE)
                {
                    pqparam_image_ptr->u4Contrast= level;
                    PQ_LOGD("[PQ_SERVICE] setPQIndex SET_PQ_CONTRAST...[%d]\n", level);
                }
                else
                {
                    PQ_LOGE("[PQ_SERVICE] setPQIndex SET_PQ_CONTRAST out of range...[%d]\n", level);
                }
            }
            break;
        case SET_PQ_BRIGHTNESS:
            {
                if (level >= 0 && level < BRIGHTNESS_SIZE)
                {
                    pqparam_image_ptr->u4Brightness= level;
                    PQ_LOGD("[PQ_SERVICE] setPQIndex SET_PQ_BRIGHTNESS...[%d]\n", level);
                }
                else
                {
                    PQ_LOGE("[PQ_SERVICE] setPQIndex SET_PQ_BRIGHTNESS out of range...[%d]\n", level);
                }
            }
            break;
        default:
            PQ_LOGD("[PQ_SERVICE] setPQIndex default case...\n");
    }
}

void PictureQuality::configCcorrUnitGain(uint32_t ccorrCoef[3][3])
{
    ccorrCoef[0][0] = ccorrCoef[0][0] * m_r_gain /1024;
    ccorrCoef[1][1] = ccorrCoef[1][1] * m_g_gain /1024;
    ccorrCoef[2][2] = ccorrCoef[2][2] * m_b_gain /1024;
}

void PictureQuality::initColorShift()
{
    m_r_gain = 1024;
    m_g_gain = 1024;
    m_b_gain = 1024;
}

bool PictureQuality::getCCorrCoefByIndex(int32_t coefTableIdx, uint32_t coef[3][3])
{
    unsigned int coef_sum = 0;

    if (coefTableIdx >= CCORR_COEF_CNT || coefTableIdx < 0)
    {
        PQ_LOGE("ccorr table index [%d] is out of bound, set it to zero", coefTableIdx);
        coefTableIdx = 0;
    }

    PQ_LOGD("ccorr table index=%d", coefTableIdx);

    if (mCcorrDebug == true) { /* scenario debug mode */
        for (int y = 0; y < 3; y += 1) {
            for (int x = 0; x < 3; x += 1) {
                coef[y][x] = 0;
            }
        }
        int index = m_PQMode;
        if (m_PQScenario ==  SCENARIO_PICTURE)
            index += 0;
        else if (m_PQScenario ==  SCENARIO_VIDEO || m_PQScenario == SCENARIO_VIDEO_CODEC)
            index += 1;
        else
            index += 2;
        index = index % 3;
        coef[index][index] = 1024;
        coef_sum = 1024;
    } else { /* normal mode */
        for (int y = 0; y < 3; y += 1) {
            for (int x = 0; x < 3; x += 1) {
                coef[y][x] = m_pqindex.CCORR_COEF[coefTableIdx][y][x];
                coef_sum += coef[y][x];
            }
        }
    }

    if (coef_sum == 0) { /* zero coef protect */
        coef[0][0] = 1024;
        coef[1][1] = 1024;
        coef[2][2] = 1024;
        PQ_LOGD("ccorr coef all zero, prot on");
    }

    return true;
}

bool PictureQuality::copyCCorrCoef(uint32_t dstCoef[3][3], const uint32_t srcCoef[3][3])
{
    for (int y=0; y<3; y++)
        for (int x=0; x<3; x++)
            dstCoef[y][x] = srcCoef[y][x];
    return true;
}

bool PictureQuality::configCCorrCoef(int drvID, const uint32_t coef[3][3])
{
    DISP_CCORR_COEF_T ccorr;

    ccorr.hw_id = DISP_CCORR0;
    for (int y = 0; y < 3; y++) {
        for (int x = 0; x < 3; x++) {
            ccorr.coef[y][x] = coef[y][x];
        }
    }

    int ret = -1;
#if !defined(CCORR_OFF) && !defined(BASIC_PACKAGE)
    ret = ioctl(drvID, DISP_IOCTL_SET_CCORR, &ccorr);
    if (ret == -1)
        PQ_LOGE("ccorr ioctl fail");

    dumpCCORRRegisters("CONFIG ccorr_coef:", ccorr.coef);
#else
    UNUSED(drvID);
#endif
    return (ret == 0);
}

status_t PictureQuality::configCCorrCoefByIndex(int32_t coefTableIdx, int32_t drvID)
{
#ifndef CCORR_OFF
    uint32_t coef[3][3];

    if (getCCorrCoefByIndex(coefTableIdx, coef))
        configCCorrCoef(drvID, coef);
#else
    UNUSED(coefTableIdx);
    UNUSED(drvID);
#endif

    return NO_ERROR;
}

status_t PictureQuality::enableDisplayColor(uint32_t value)
{
#ifndef DISP_COLOR_OFF
    int bypass;
    PQ_LOGD("[PQ_SERVICE] enableDisplayColor(), enable[%d]", value);

    if (m_drvID < 0)
    {
        PQ_LOGE("[PQ_SERVICE] open device fail!!");
        return UNKNOWN_ERROR ;

    }

    //  set bypass COLOR to disp driver.
    if (value > 0)
    {
        bypass = 0;
        ioctl(m_drvID, DISP_IOCTL_PQ_SET_BYPASS_COLOR, &bypass);
    }
    else
    {
        bypass = 1;
        ioctl(m_drvID, DISP_IOCTL_PQ_SET_BYPASS_COLOR, &bypass);
    }
    PQ_LOGD("[PQService] enableDisplayColor[%d]", value);

    m_bFeatureSwitch[DISPLAY_COLOR] = value;
#else
    UNUSED(value);
#endif

    return NO_ERROR;
}

status_t PictureQuality::enableContentColorVideo(uint32_t value)
{
#ifdef MDP_COLOR_ENABLE
    char pvalue[PROPERTY_VALUE_MAX];
    int ret;

    if (m_AshmemProxy->setTuningField(VIDEO_CONTENT_COLOR_ENABLE, value) < 0)
    {
        PQ_LOGE("[PQ_SERVICE] enableContentColorVideo : m_AshmemProxy->setTuningField() failed\n");
        return UNKNOWN_ERROR;
    }

    snprintf(pvalue, PROPERTY_VALUE_MAX, "%d\n", value);
    ret = property_set(PQ_MDP_COLOR_EN_STR  , pvalue);
    PQ_LOGD("[PQService] enableContentColorVideo[%d]", value);

    m_bFeatureSwitch[CONTENT_COLOR_VIDEO] = value;
#else
    UNUSED(value);
#endif
    return NO_ERROR;
}

status_t PictureQuality::enableContentColor(uint32_t value)
{
#ifdef MDP_COLOR_ENABLE
    char pvalue[PROPERTY_VALUE_MAX];
    int ret;

    snprintf(pvalue, PROPERTY_VALUE_MAX, "%d\n", value);
    ret = property_set(PQ_MDP_COLOR_DBG_EN_STR  , pvalue);
    PQ_LOGD("[PQService] enableContentColor[%d]", value);

    m_bFeatureSwitch[CONTENT_COLOR] = value;
#else
    UNUSED(value);
#endif

    return NO_ERROR;
}

status_t PictureQuality::enableSharpness(uint32_t value)
{
    char pvalue[PROPERTY_VALUE_MAX];
    int ret;

    if (m_AshmemProxy->setTuningField(SHP_ENABLE, value) < 0)
    {
        PQ_LOGE("[PQ_SERVICE] enableSharpness : m_AshmemProxy->setTuningField() failed\n");
        return UNKNOWN_ERROR;
    }

    snprintf(pvalue, PROPERTY_VALUE_MAX, "%d\n", value);
    ret = property_set(PQ_DBG_SHP_EN_STR, pvalue);
    PQ_LOGD("[PQService] enableSharpness[%d]", value);

    m_bFeatureSwitch[SHARPNESS] = value;

    return NO_ERROR;
}

status_t PictureQuality::enableDynamicContrast(uint32_t value)
{
    char pvalue[PROPERTY_VALUE_MAX];
    int ret;

    if (m_AshmemProxy->setTuningField(DC_ENABLE, value) < 0)
    {
        PQ_LOGE("[PQ_SERVICE] enableDynamicContrast : m_AshmemProxy->setTuningField() failed\n");
        return UNKNOWN_ERROR;
    }

    snprintf(pvalue, PROPERTY_VALUE_MAX, "%d\n", value);
    ret = property_set(PQ_ADL_PROPERTY_STR, pvalue);
    PQ_LOGD("[PQService] enableDynamicContrast[%d]", value);

    m_bFeatureSwitch[DYNAMIC_CONTRAST] = value;

    return NO_ERROR;
}

status_t PictureQuality::enableDynamicSharpness(uint32_t value)
{
    char pvalue[PROPERTY_VALUE_MAX];
    int ret;

    if (m_AshmemProxy->setTuningField(DSHP_ENABLE, value) < 0)
    {
        PQ_LOGE("[PQ_SERVICE] enableDynamicSharpness : m_AshmemProxy->setTuningField() failed\n");
        return UNKNOWN_ERROR;
    }

    snprintf(pvalue, PROPERTY_VALUE_MAX, "%d\n", value);
    ret = property_set(PQ_DBG_DSHP_EN_STR, pvalue);
    PQ_LOGD("[PQService] enableDynamicSharpness[%d]", value);

    m_bFeatureSwitch[DYNAMIC_SHARPNESS] = value;

    return NO_ERROR;
}

status_t PictureQuality::enableDisplayGamma(uint32_t value)
{
    if (value > 0)
    {
        char pvalue[PROPERTY_VALUE_MAX];
        char dvalue[PROPERTY_VALUE_MAX];

        snprintf(dvalue, PROPERTY_VALUE_MAX, "%d\n", GAMMA_INDEX_DEFAULT);
        property_get(GAMMA_INDEX_PROPERTY_NAME, pvalue, dvalue);
        m_gamma_id = atoi(pvalue);
        _setGammaIndex(m_gamma_id);
    }
    else
    {
        m_gamma_id = GAMMA_INDEX_DEFAULT;
        _setGammaIndex(m_gamma_id);
    }
    PQ_LOGD("[PQService] enableDisplayGamma[%d]", value);

    m_bFeatureSwitch[DISPLAY_GAMMA] = value;

    return NO_ERROR;
}

status_t PictureQuality::enableDisplayOverDrive(uint32_t value)
{
#ifndef BASIC_PACKAGE
    DISP_OD_CMD cmd;

    if (m_drvID < 0)
    {
        PQ_LOGE("[PQService] enableDisplayOverDrive(), open device fail!!");
        return UNKNOWN_ERROR;
    }

    memset(&cmd, 0, sizeof(cmd));
    cmd.size = sizeof(cmd);
    cmd.type = 6;

    if (value > 0)
    {
        cmd.param0 = 1;
    }
    else
    {
        cmd.param0 = 0;
    }

    int ret = ioctl(m_drvID, DISP_IOCTL_OD_CTL, &cmd);

    PQ_LOGD("[PQService] enableDisplayOverDrive[%d] ret=%d", value, ret);
#endif
    m_bFeatureSwitch[DISPLAY_OVER_DRIVE] = value;

    return NO_ERROR;
}

status_t PictureQuality::enableISOAdaptiveSharpness(uint32_t value)
{
    char pvalue[PROPERTY_VALUE_MAX];
    int ret;

    if (m_AshmemProxy->setTuningField(ISO_SHP_ENABLE, value) < 0)
    {
        PQ_LOGE("[PQ_SERVICE] enableISOAdaptiveSharpness : m_AshmemProxy->setTuningField() failed\n");
        return UNKNOWN_ERROR;
    }

    snprintf(pvalue, PROPERTY_VALUE_MAX, "%d\n", value);
    ret = property_set(PQ_ISO_SHP_EN_STR, pvalue);
    PQ_LOGD("[PQService] enableISOAdaptiveSharpness[%d]", value);

    m_bFeatureSwitch[ISO_ADAPTIVE_SHARPNESS] = value;

    return NO_ERROR;
}

status_t PictureQuality::enableUltraResolution(uint32_t value)
{
    char pvalue[PROPERTY_VALUE_MAX];
    int ret;

    if (m_AshmemProxy->setTuningField(UR_ENABLE, value) < 0)
    {
        PQ_LOGE("[PQ_SERVICE] enableUltraResolution : m_AshmemProxy->setTuningField() failed\n");
        return UNKNOWN_ERROR;
    }

    snprintf(pvalue, PROPERTY_VALUE_MAX, "%d\n", value);
    ret = property_set(PQ_ULTRARES_EN_STR, pvalue);
    PQ_LOGD("[PQService] enableUltraResolution[%d]", value);

    m_bFeatureSwitch[ULTRA_RESOLUTION] = value;

    return NO_ERROR;
}

status_t PictureQuality::enableVideoHDR(uint32_t value)
{
    char pvalue[PROPERTY_VALUE_MAX];
    int ret;

    if (m_AshmemProxy->setTuningField(HDR_VIDEO_ENABLE, value) < 0)
    {
        PQ_LOGE("[PQ_SERVICE] enableVideoHDR : m_AshmemProxy->setTuningField() failed\n");
        return UNKNOWN_ERROR;
    }

    snprintf(pvalue, PROPERTY_VALUE_MAX, "%d\n", value);
    ret = property_set(PQ_HDR_VIDEO_EN_STR, pvalue);
    PQ_LOGD("[PQService] enableVideoHDR[%d]", value);

    m_bFeatureSwitch[VIDEO_HDR] = value;

    return NO_ERROR;
}

void PictureQuality::initBlueLight()
{
#ifdef BLULIGHT_DEFENDER_SUPPORT
    BluLightInitParam initParam;
    initParam.reserved = 0;
    blueLight->onInitCommon(initParam);
    blueLight->onInitPlatform(initParam);

    blueLight->setEnabled(false);

    char value[PROPERTY_VALUE_MAX];
    sprintf(value,"%d",blueLight->getStrength());
    property_set(MTK_BLUELIGHT_DEFAULT_PROPERTY_NAME, value);

    char property[PROPERTY_VALUE_MAX];
    if (property_get(MTK_BLUELIGHT_STRENGTH_PROPERTY_NAME, property, NULL) > 0) {
        int strength = (int)strtoul(property, NULL, 0);
        PQ_LOGD("[PQ_SERVICE] Blue-light init strength: %d", strength);
        blueLight->setStrength(strength);
    }
#endif
}

void PictureQuality::initChameleon()
{
#ifdef CHAMELEON_DISPLAY_SUPPORT
    chameleonDisplayProcess->setEnabled(false);
    mLightSensor->setEnabled(false);

    char value[PROPERTY_VALUE_MAX];
    sprintf(value,"%d",chameleonDisplayProcess->getStrength());
    property_set(MTK_CHAMELEON_DEFAULT_PROPERTY_NAME, value);

    char property[PROPERTY_VALUE_MAX];
    if (property_get(MTK_CHAMELEON_STRENGTH_PROPERTY_NAME, property, NULL) > 0) {
        int strength = (int)strtoul(property, NULL, 0);
        PQ_LOGD("[PQ_SERVICE] Chameleon init strength: %d", strength);
        chameleonDisplayProcess->setStrength(strength);
    }
#endif
}

void PictureQuality::AmbientLightCTChange(double input_x, double input_y, double input_Y)
{
#ifdef MTK_CHAMELEON_DISPLAY_SUPPORT
    mDebug.sensorDebugMode = 0;

    mSensorInputxyY = true;
    mInput_x = input_x;
    mInput_y = input_y;
    mInput_Y = input_Y;

    mAllowSensorDebounce = false;
    setEvent(eEvtALI);
    refreshDisplay();
#else
    UNUSED(input_x);
    UNUSED(input_y);
    UNUSED(input_Y);
#endif
}

int PictureQuality::forceSensorDebounce(int32_t aliR, int32_t aliG, int32_t aliB, int32_t aliW)
{
    int sensorChanged = 0;
#ifdef CHAMELEON_DISPLAY_SUPPORT
    const int iteration = 20;

    do {
        if (aliR <= 0 || aliG <= 0 || aliB <= 0 || aliW <= 0) {
            mDebug.sensorDebugMode = 0;
            break;
        }

        chameleonDisplayProcess->setForceChange(0);

        mDebug.sensorDebugMode = 1;

        if (mDebug.sensorInput.aliR == aliR && mDebug.sensorInput.aliG == aliG &&
            mDebug.sensorInput.aliB == aliB && mDebug.sensorInput.aliW == aliW) {
            sensorChanged = ENUM_SensorValueChanged;
            break;
        }

        mDebug.sensorInput.aliR = aliR;
        mDebug.sensorInput.aliG = aliG;
        mDebug.sensorInput.aliB = aliB;
        mDebug.sensorInput.aliW = aliW;

        for (int i = 0; i < iteration; i++) {
            sensorChanged = chameleonDisplayProcess->onSensorDebounce(aliR, aliG, aliB, aliW);
            if (sensorChanged == ENUM_SensorValueChanged) {
                break;
            }
            usleep(16 * 1000);
        }
    } while (0);

    if (sensorChanged == ENUM_SensorValueChanged) {
        mPQInput->sensorInput.aliR = mDebug.sensorInput.aliR;
        mPQInput->sensorInput.aliG = mDebug.sensorInput.aliG;
        mPQInput->sensorInput.aliB = mDebug.sensorInput.aliB;
        mPQInput->sensorInput.aliW = mDebug.sensorInput.aliW;

        mAllowSensorDebounce = false;
        setEvent(eEvtALI);
        refreshDisplay();
    }
#else
    UNUSED(aliR);
    UNUSED(aliG);
    UNUSED(aliB);
    UNUSED(aliW);
#endif
    return sensorChanged;
}

// Compose ColorRegisters by current scenario setting
bool PictureQuality::composeColorRegisters(void *_colorReg, const DISP_PQ_PARAM *pqparam, const unsigned int ccorr_coef[3][3], int copyFlag)
{
    bool result = false;
    TPQTransitionInput *TransitionReg;
    ColorRegisters *colorReg;
    const DISPLAY_PQ_T *pqTable = &m_pqindex;

    if (copyFlag == eRegColor) {
        colorReg = static_cast<ColorRegisters*>(_colorReg);
    } else if (copyFlag == eRegTransitionIn) {
        TransitionReg = static_cast<TPQTransitionInput*>(_colorReg);
        colorReg = &TransitionReg->InputColor.ColorReg;
    } else {
        return result;
    }

    colorReg->GLOBAL_SAT = pqTable->GLOBAL_SAT[pqparam->u4SatGain];
    colorReg->CONTRAST = pqTable->CONTRAST[pqparam->u4Contrast];
    colorReg->BRIGHTNESS = pqTable->BRIGHTNESS[pqparam->u4Brightness];
    /* copy ccorr from previous stage intput instead of from cust file */
    copyCCorrCoef(colorReg->CCORR_COEF, ccorr_coef);
    //getCCorrCoefByIndex(pqparam->u4Ccorr, colorReg->CCORR_COEF);
    if (sizeof(colorReg->PARTIAL_Y) == sizeof(pqTable->PARTIAL_Y[0]) &&
        sizeof(colorReg->PURP_TONE_S) == sizeof(pqTable->PURP_TONE_S[0]) &&
        sizeof(colorReg->SKIN_TONE_S) == sizeof(pqTable->SKIN_TONE_S[0]) &&
        sizeof(colorReg->GRASS_TONE_S) == sizeof(pqTable->GRASS_TONE_S[0]) &&
        sizeof(colorReg->SKY_TONE_S) == sizeof(pqTable->SKY_TONE_S[0]) &&
        sizeof(colorReg->PURP_TONE_H) == sizeof(pqTable->PURP_TONE_H[0]) &&
        sizeof(colorReg->SKIN_TONE_H) == sizeof(pqTable->SKIN_TONE_H[0]) &&
        sizeof(colorReg->GRASS_TONE_H) == sizeof(pqTable->GRASS_TONE_H[0]) &&
        sizeof(colorReg->SKY_TONE_H) == sizeof(pqTable->SKY_TONE_H[0]))
    {
        memcpy(colorReg->PARTIAL_Y, pqTable->PARTIAL_Y[pqparam->u4PartialY], sizeof(colorReg->PARTIAL_Y));
        memcpy(colorReg->PURP_TONE_S, pqTable->PURP_TONE_S[pqparam->u4SatAdj[PURP_TONE]], sizeof(colorReg->PURP_TONE_S));
        memcpy(colorReg->SKIN_TONE_S, pqTable->SKIN_TONE_S[pqparam->u4SatAdj[SKIN_TONE]], sizeof(colorReg->SKIN_TONE_S));
        memcpy(colorReg->GRASS_TONE_S, pqTable->GRASS_TONE_S[pqparam->u4SatAdj[GRASS_TONE]], sizeof(colorReg->GRASS_TONE_S));
        memcpy(colorReg->SKY_TONE_S, pqTable->SKY_TONE_S[pqparam->u4SatAdj[SKY_TONE]], sizeof(colorReg->SKY_TONE_S));
        memcpy(colorReg->PURP_TONE_H, pqTable->PURP_TONE_H[pqparam->u4HueAdj[PURP_TONE]], sizeof(colorReg->PURP_TONE_H));
        memcpy(colorReg->SKIN_TONE_H, pqTable->SKIN_TONE_H[pqparam->u4HueAdj[SKIN_TONE]], sizeof(colorReg->SKIN_TONE_H));
        memcpy(colorReg->GRASS_TONE_H, pqTable->GRASS_TONE_H[pqparam->u4HueAdj[GRASS_TONE]], sizeof(colorReg->GRASS_TONE_H));
        memcpy(colorReg->SKY_TONE_H, pqTable->SKY_TONE_H[pqparam->u4HueAdj[SKY_TONE]], sizeof(colorReg->SKY_TONE_H));

        result = true;
    } else {
        PQ_LOGE("composeColorRegisters: Parameter size does not match (%d, %d) (%d, %d) (%d, %d)",
            (int)sizeof(colorReg->PARTIAL_Y), (int)sizeof(pqTable->PARTIAL_Y[0]),
            (int)sizeof(colorReg->PURP_TONE_S), (int)sizeof(pqTable->PURP_TONE_S[0]),
            (int)sizeof(colorReg->SKIN_TONE_S), (int)sizeof(pqTable->SKIN_TONE_S[0]));
        PQ_LOGE("composeColorRegisters: (%d, %d) (%d, %d) (%d, %d) (%d, %d) (%d, %d) (%d, %d)",
            (int)sizeof(colorReg->GRASS_TONE_S), (int)sizeof(pqTable->GRASS_TONE_S[0]),
            (int)sizeof(colorReg->SKY_TONE_S), (int)sizeof(pqTable->SKY_TONE_S[0]),
            (int)sizeof(colorReg->PURP_TONE_H), (int)sizeof(pqTable->PURP_TONE_H[0]),
            (int)sizeof(colorReg->SKIN_TONE_H), (int)sizeof(pqTable->SKIN_TONE_H[0]),
            (int)sizeof(colorReg->GRASS_TONE_H), (int)sizeof(pqTable->GRASS_TONE_H[0]),
            (int)sizeof(colorReg->SKY_TONE_H), (int)sizeof(pqTable->SKY_TONE_H[0]));
    }

    return result;
}

// Translate ColorRegisters to TPQTransitionInput
bool PictureQuality::translateColorRegisters(void *TransitionReg, void *algoReg)
{
    bool result = false;
    TPQTransitionInput *TransitionIn = static_cast<TPQTransitionInput*>(TransitionReg);
    ColorRegisters *TransitionColor = &TransitionIn->InputColor.ColorReg;
    ColorRegisters *colorReg = static_cast<ColorRegisters*>(algoReg);

    TransitionColor->GLOBAL_SAT = colorReg->GLOBAL_SAT;
    TransitionColor->CONTRAST = colorReg->CONTRAST;
    TransitionColor->BRIGHTNESS = colorReg->BRIGHTNESS;
    /* copy ccorr from previous stage intput instead of from cust file */
    copyCCorrCoef(TransitionColor->CCORR_COEF, colorReg->CCORR_COEF);
    if (sizeof(TransitionColor->PARTIAL_Y) == sizeof(colorReg->PARTIAL_Y) &&
        sizeof(TransitionColor->PURP_TONE_S) == sizeof(colorReg->PURP_TONE_S) &&
        sizeof(TransitionColor->SKIN_TONE_S) == sizeof(colorReg->SKIN_TONE_S) &&
        sizeof(TransitionColor->GRASS_TONE_S) == sizeof(colorReg->GRASS_TONE_S) &&
        sizeof(TransitionColor->SKY_TONE_S) == sizeof(colorReg->SKY_TONE_S) &&
        sizeof(TransitionColor->PURP_TONE_H) == sizeof(colorReg->PURP_TONE_H) &&
        sizeof(TransitionColor->SKIN_TONE_H) == sizeof(colorReg->SKIN_TONE_H) &&
        sizeof(TransitionColor->GRASS_TONE_H) == sizeof(colorReg->GRASS_TONE_H) &&
        sizeof(TransitionColor->SKY_TONE_H) == sizeof(colorReg->SKY_TONE_H))
    {
        memcpy(TransitionColor->PARTIAL_Y, colorReg->PARTIAL_Y, sizeof(TransitionColor->PARTIAL_Y));
        memcpy(TransitionColor->PURP_TONE_S, colorReg->PURP_TONE_S, sizeof(TransitionColor->PURP_TONE_S));
        memcpy(TransitionColor->SKIN_TONE_S, colorReg->SKIN_TONE_S, sizeof(TransitionColor->SKIN_TONE_S));
        memcpy(TransitionColor->GRASS_TONE_S, colorReg->GRASS_TONE_S, sizeof(TransitionColor->GRASS_TONE_S));
        memcpy(TransitionColor->SKY_TONE_S, colorReg->SKY_TONE_S, sizeof(TransitionColor->SKY_TONE_S));
        memcpy(TransitionColor->PURP_TONE_H, colorReg->PURP_TONE_H, sizeof(TransitionColor->PURP_TONE_H));
        memcpy(TransitionColor->SKIN_TONE_H, colorReg->SKIN_TONE_H, sizeof(TransitionColor->SKIN_TONE_H));
        memcpy(TransitionColor->GRASS_TONE_H, colorReg->GRASS_TONE_H, sizeof(TransitionColor->GRASS_TONE_H));
        memcpy(TransitionColor->SKY_TONE_H, colorReg->SKY_TONE_H, sizeof(TransitionColor->SKY_TONE_H));

        result = true;
    } else {
        PQ_LOGE("translateColorRegisters: Parameter size does not match");
    }

    return result;
}

// Translate TColorRegisters to DISPLAY_COLOR_REG_T
bool PictureQuality::translateDrvRegisters(DISPLAY_COLOR_REG_T *drvReg, void *algoReg, int copyFlag)
{
    bool result = false;
    TPQTransitionInput *transitionInReg;
    TPQTransitionOutput *transitionOutReg;
    ColorRegisters *colorReg;

    if (copyFlag == eRegTransitionIn) {
        transitionInReg = static_cast<TPQTransitionInput*>(algoReg);
        colorReg = &transitionInReg->InputColor.ColorReg;
    } else if (copyFlag == eRegTransitionOut) {
        transitionOutReg = static_cast<TPQTransitionOutput*>(algoReg);
        colorReg = &transitionOutReg->OutputColor.ColorReg;
    } else {
        return result;
    }

    drvReg->GLOBAL_SAT = colorReg->GLOBAL_SAT;
    drvReg->CONTRAST = colorReg->CONTRAST;
    drvReg->BRIGHTNESS = colorReg->BRIGHTNESS;
    if (sizeof(drvReg->PARTIAL_Y) == sizeof(colorReg->PARTIAL_Y) &&
        sizeof(drvReg->PURP_TONE_S) == sizeof(colorReg->PURP_TONE_S) &&
        sizeof(drvReg->SKIN_TONE_S) == sizeof(colorReg->SKIN_TONE_S) &&
        sizeof(drvReg->GRASS_TONE_S) == sizeof(colorReg->GRASS_TONE_S) &&
        sizeof(drvReg->SKY_TONE_S) == sizeof(colorReg->SKY_TONE_S) &&
        sizeof(drvReg->PURP_TONE_H) == sizeof(colorReg->PURP_TONE_H) &&
        sizeof(drvReg->SKIN_TONE_H) == sizeof(colorReg->SKIN_TONE_H) &&
        sizeof(drvReg->GRASS_TONE_H) == sizeof(colorReg->GRASS_TONE_H) &&
        sizeof(drvReg->SKY_TONE_H) == sizeof(colorReg->SKY_TONE_H))
    {
        memcpy(drvReg->PARTIAL_Y, colorReg->PARTIAL_Y, sizeof(drvReg->PARTIAL_Y));
        memcpy(drvReg->PURP_TONE_S, colorReg->PURP_TONE_S, sizeof(drvReg->PURP_TONE_S));
        memcpy(drvReg->SKIN_TONE_S, colorReg->SKIN_TONE_S, sizeof(drvReg->SKIN_TONE_S));
        memcpy(drvReg->GRASS_TONE_S, colorReg->GRASS_TONE_S, sizeof(drvReg->GRASS_TONE_S));
        memcpy(drvReg->SKY_TONE_S, colorReg->SKY_TONE_S, sizeof(drvReg->SKY_TONE_S));
        memcpy(drvReg->PURP_TONE_H, colorReg->PURP_TONE_H, sizeof(drvReg->PURP_TONE_H));
        memcpy(drvReg->SKIN_TONE_H, colorReg->SKIN_TONE_H, sizeof(drvReg->SKIN_TONE_H));
        memcpy(drvReg->GRASS_TONE_H, colorReg->GRASS_TONE_H, sizeof(drvReg->GRASS_TONE_H));
        memcpy(drvReg->SKY_TONE_H, colorReg->SKY_TONE_H, sizeof(drvReg->SKY_TONE_H));

        result = true;
    } else {
        PQ_LOGE("translateDrvRegisters: Parameter size does not match");
    }

    return result;
}

// Compose configure ColorRegisters by current scenario setting
bool PictureQuality::composeConfigColorRegisters(DISPLAY_COLOR_REG_T *drvReg, const DISP_PQ_PARAM *pqparam)
{
#ifdef COLOR_3_0
    bool result = false;
    const DISPLAY_PQ_T *pqTable = &m_pqindex;

    if (sizeof(drvReg->COLOR_3D) == sizeof(pqTable->COLOR_3D[0]))
    {
        memcpy(drvReg->COLOR_3D, pqTable->COLOR_3D[pqparam->u4ColorLUT], sizeof(drvReg->COLOR_3D));

        result = true;
    } else {
        PQ_LOGE("composeConfigColorRegisters: Parameter size does not match (%d, %d)",
            (int)sizeof(drvReg->COLOR_3D), (int)sizeof(pqTable->COLOR_3D[0]));
    }

    return result;
#else
    UNUSED(drvReg);
    UNUSED(pqparam);

    return true;
#endif
}

// Convert _algoReg -> _tuningReg
void PictureQuality::translateToColorTuning(void *_algoReg, void *_tuningReg)
{
    if (_algoReg == NULL || _tuningReg == NULL)
        return;

    ColorRegisters *algoReg = static_cast<ColorRegisters*>(_algoReg);
    ColorRegistersTuning *tuningReg = static_cast<ColorRegistersTuning*>(_tuningReg);

#define CONVERT_COPY(tuningReg, algoReg, field, type) \
    convertCopy(&(tuningReg->field type), sizeof(tuningReg->field), &(algoReg->field type), sizeof(algoReg->field))

    tuningReg->GLOBAL_SAT = algoReg->GLOBAL_SAT;
    tuningReg->CONTRAST = algoReg->CONTRAST;
    tuningReg->BRIGHTNESS = algoReg->BRIGHTNESS;
    CONVERT_COPY(tuningReg, algoReg, PARTIAL_Y, [0]);
    CONVERT_COPY(tuningReg, algoReg, PURP_TONE_S, [0][0]);
    CONVERT_COPY(tuningReg, algoReg, SKIN_TONE_S, [0][0]);
    CONVERT_COPY(tuningReg, algoReg, GRASS_TONE_S, [0][0]);
    CONVERT_COPY(tuningReg, algoReg, SKY_TONE_S, [0][0]);
    CONVERT_COPY(tuningReg, algoReg, PURP_TONE_H, [0]);
    CONVERT_COPY(tuningReg, algoReg, SKIN_TONE_H, [0]);
    CONVERT_COPY(tuningReg, algoReg, GRASS_TONE_H, [0]);
    CONVERT_COPY(tuningReg, algoReg, SKY_TONE_H, [0]);
    CONVERT_COPY(tuningReg, algoReg, CCORR_COEF, [0][0]);

#undef CONVERT_COPY
}

// Convert _algoReg <- _tuningReg
void PictureQuality::translateFromColorTuning(void *_algoReg, void *_tuningReg)
{
    if (_algoReg == NULL || _tuningReg == NULL)
        return;

    ColorRegisters *algoReg = static_cast<ColorRegisters*>(_algoReg);
    ColorRegistersTuning *tuningReg = static_cast<ColorRegistersTuning*>(_tuningReg);

#define CONVERT_COPY(algoReg, tuningReg, field, type) \
    convertCopy(&(algoReg->field type), sizeof(algoReg->field), &(tuningReg->field type), sizeof(tuningReg->field))

    algoReg->GLOBAL_SAT = tuningReg->GLOBAL_SAT;
    algoReg->CONTRAST = tuningReg->CONTRAST;
    algoReg->BRIGHTNESS = tuningReg->BRIGHTNESS;
    CONVERT_COPY(algoReg, tuningReg, PARTIAL_Y, [0]);
    CONVERT_COPY(algoReg, tuningReg, PURP_TONE_S, [0][0]);
    CONVERT_COPY(algoReg, tuningReg, SKIN_TONE_S, [0][0]);
    CONVERT_COPY(algoReg, tuningReg, GRASS_TONE_S, [0][0]);
    CONVERT_COPY(algoReg, tuningReg, SKY_TONE_S, [0][0]);
    CONVERT_COPY(algoReg, tuningReg, PURP_TONE_H, [0]);
    CONVERT_COPY(algoReg, tuningReg, SKIN_TONE_H, [0]);
    CONVERT_COPY(algoReg, tuningReg, GRASS_TONE_H, [0]);
    CONVERT_COPY(algoReg, tuningReg, SKY_TONE_H, [0]);
    CONVERT_COPY(algoReg, tuningReg, CCORR_COEF, [0][0]);

#undef CONVERT_COPY
}

// Compose Chameleon info by current scenario setting and sensor value
bool PictureQuality::composeChameleonRegisters(void *_chameleonInReg, unsigned int ccorr_coef[3][3],
    SensorInput sensorInput, int backlight)
{
    bool result = false;
#ifdef CHAMELEON_DISPLAY_SUPPORT
    TChameleonDisplayInput *chameleonInReg = static_cast<TChameleonDisplayInput*>(_chameleonInReg);
    chameleonInReg->SensorValue.R = sensorInput.aliR;
    chameleonInReg->SensorValue.G = sensorInput.aliG;
    chameleonInReg->SensorValue.B = sensorInput.aliB;
    chameleonInReg->SensorValue.W = sensorInput.aliW;
    copyCCorrCoef(chameleonInReg->OriginalCCORR.Coef ,ccorr_coef);
    chameleonInReg->CurrentBacklightSettings = backlight;
#else
    UNUSED(_chameleonInReg);
    UNUSED(ccorr_coef);
    UNUSED(sensorInput);
    UNUSED(backlight);
#endif
    return result;
}

bool PictureQuality::copyTuningField(void *_dstReg, void *_srcReg, int size)
{
    if (_dstReg == NULL || _srcReg == NULL)
        return false;
    memcpy(_dstReg, _srcReg, size);
    return true;
}

bool PictureQuality::compareDrvColorRegisters(DISPLAY_COLOR_REG_T *drvReg, void *algoReg)
{
    bool result = false;
    TPQTransitionOutput *transitionOutReg = static_cast<TPQTransitionOutput*>(algoReg);
    ColorRegisters *colorReg = &transitionOutReg->OutputColor.ColorReg;;

    do {
        if (drvReg->GLOBAL_SAT != colorReg->GLOBAL_SAT) {
            result = true;
            break;
        }

        if (drvReg->CONTRAST != colorReg->CONTRAST) {
            result = true;
            break;
        }

        if (drvReg->BRIGHTNESS != colorReg->BRIGHTNESS) {
            result = true;
            break;
        }

        if (memcmp(drvReg->PARTIAL_Y, colorReg->PARTIAL_Y, sizeof(drvReg->PARTIAL_Y)) != 0) {
            result = true;
            break;
        }

        if (memcmp(drvReg->PURP_TONE_S, colorReg->PURP_TONE_S, sizeof(drvReg->PURP_TONE_S)) != 0) {
            result = true;
            break;
        }

        if (memcmp(drvReg->SKIN_TONE_S, colorReg->SKIN_TONE_S, sizeof(drvReg->SKIN_TONE_S)) != 0) {
            result = true;
            break;
        }

        if (memcmp(drvReg->GRASS_TONE_S, colorReg->GRASS_TONE_S, sizeof(drvReg->GRASS_TONE_S)) != 0) {
            result = true;
            break;
        }

        if (memcmp(drvReg->SKY_TONE_S, colorReg->SKY_TONE_S, sizeof(drvReg->SKY_TONE_S)) != 0) {
            result = true;
            break;
        }

        if (memcmp(drvReg->PURP_TONE_H, colorReg->PURP_TONE_H, sizeof(drvReg->PURP_TONE_H)) != 0) {
            result = true;
            break;
        }

        if (memcmp(drvReg->SKIN_TONE_H, colorReg->SKIN_TONE_H, sizeof(drvReg->SKIN_TONE_H)) != 0) {
            result = true;
            break;
        }

        if (memcmp(drvReg->GRASS_TONE_H, colorReg->GRASS_TONE_H, sizeof(drvReg->GRASS_TONE_H)) != 0) {
            result = true;
            break;
        }

        if (memcmp(drvReg->SKY_TONE_H, colorReg->SKY_TONE_H, sizeof(drvReg->SKY_TONE_H)) != 0) {
            result = true;
            break;
        }
    } while (0);

    return result;
}

bool PictureQuality::compareDrvCCorrCoef(uint32_t dstCoef[3][3], uint32_t srcCoef[3][3])
{
    for (int y=0; y<3; y++) {
        for (int x=0; x<3; x++) {
            if (dstCoef[y][x] != srcCoef[y][x]) {
                return true;
            }
        }
    }

    return false;
}

bool PictureQuality::blulightDefender(const DISP_PQ_PARAM *pqparam, void *TransitionReg, const unsigned int ccorr_coef[3][3])
{
    bool is_completed = true;

#ifdef BLULIGHT_DEFENDER_SUPPORT
    if (blueLight->isEnabled()) {
        ColorRegisters *inReg = new ColorRegisters;
        ColorRegisters *outReg = new ColorRegisters;

        if (composeColorRegisters(inReg, pqparam, ccorr_coef, eRegColor)) {
            if (mBLInputMode == PQ_TUNING_READING) {
                // Save composed registers for tuning tool reading
                translateToColorTuning(inReg, mBLInput);
            } else if (mBLInputMode == PQ_TUNING_OVERWRITTEN) {
                // Always apply tool-given input
                translateFromColorTuning(inReg, mBLInput);
            }

            dumpColorRegisters("Blue-light input:", inReg);
            blueLight->onCalculate(*inReg, outReg);
            dumpColorRegisters("Blue-light output:", outReg);

            if (mBLOutputMode == PQ_TUNING_READING) {
                // Save composed registers for tuning tool reading
                translateToColorTuning(outReg, mBLOutput);
            } else if (mBLOutputMode == PQ_TUNING_OVERWRITTEN) {
                // Always apply tool-given input
                translateFromColorTuning(outReg, mBLOutput);
            }

            if (translateColorRegisters(TransitionReg, outReg) == false) {
                is_completed = false;
            }
        }

        delete inReg;
        delete outReg;
    } else {
        if (composeColorRegisters(TransitionReg, pqparam, ccorr_coef, eRegTransitionIn) == false) {
            is_completed = false;
        }
    }
#else

#if defined(TRANSITION_SUPPORT) || defined(CHAMELEON_DISPLAY_SUPPORT)
    if (composeColorRegisters(TransitionReg, pqparam, ccorr_coef, eRegTransitionIn) == false) {
        is_completed = false;
    }
#else
    UNUSED(TransitionReg);
    UNUSED(pqparam);
    UNUSED(ccorr_coef);
#endif                //TRANSITION_SUPPORT
#endif                //BLULIGHT_DEFENDER_SUPPORT

    return is_completed;
}

bool PictureQuality::chameleonDisplay(int *backlightOut, uint32_t ccorrCoefOut[3][3],
    int backlightIn, uint32_t ccorrCoefIn[3][3], SensorInput sensorIn)
{
    int chameleonActiveTrigger = 0;
#ifdef CHAMELEON_DISPLAY_SUPPORT
    if (chameleonDisplayProcess->isEnabled()) {
        if (isEventSet(eEvtALI)) {
            TChameleonDisplayInput *chameleonInput = new TChameleonDisplayInput;
            TChameleonDisplayOutput *chameleonOutput = new TChameleonDisplayOutput;

            composeChameleonRegisters(chameleonInput,
                ccorrCoefIn,
                sensorIn,
                backlightIn);

            if (mChameleonInputMode == PQ_TUNING_READING) {
                // Save composed registers for tuning tool reading
                copyTuningField(mChameleonInput, chameleonInput, sizeof(TChameleonDisplayInput));
            } else if (mChameleonInputMode == PQ_TUNING_OVERWRITTEN) {
                // Always apply tool-given input
                copyTuningField(chameleonInput, mChameleonInput, sizeof(TChameleonDisplayInput));
            }

            dumpChameleonInRegisters("Chameleon input:", chameleonInput);
            chameleonActiveTrigger = chameleonDisplayProcess->onCalculate(*chameleonInput, chameleonOutput);
/*
            if (mSensorInputxyY == false) {
                chameleonActiveTrigger = chameleonDisplayProcess->onCalculate(*chameleonInput, chameleonOutput);
            }
            else{
                chameleonActiveTrigger = chameleonDisplayProcess->onCalculateDouble(*chameleonInput, mInput_x, mInput_y, mInput_Y, chameleonOutput);
            }
*/
            dumpChameleonOutRegisters("Chameleon output:", chameleonOutput);

            if (mChameleonOutputMode == PQ_TUNING_READING) {
                // Save composed registers for tuning tool reading
                copyTuningField(mChameleonOutput, chameleonOutput, sizeof(TChameleonDisplayOutput));
            } else if (mChameleonOutputMode == PQ_TUNING_OVERWRITTEN) {
                // Always apply tool-given input
                copyTuningField(chameleonOutput, mChameleonOutput, sizeof(TChameleonDisplayOutput));
            }

            if (chameleonActiveTrigger == 1) {
                copyCCorrCoef(mChameleonCcorrCoefOut, chameleonOutput->TargetCCORR.Coef);

                if (chameleonDisplayProcess->isBacklightControlEnabled()) {
                    *backlightOut = chameleonOutput->TargetBacklight;
                    mChameleonBacklightOut = chameleonOutput->TargetBacklight;
                } else {
                    *backlightOut = backlightIn;
                    mChameleonBacklightOut = backlightIn;
                }
            }

            mAllowSensorDebounce = true;

            delete chameleonInput;
            delete chameleonOutput;
        } else { // frame trigger but chameleon not calculate
            *backlightOut = mChameleonBacklightOut;
        }
    } else {
        /* chameleon disable : use scenario ccorr as chalmeleon output ccorr */
        PQ_LOGD("Chameleon is disable\n");
        copyCCorrCoef(mChameleonCcorrCoefOut, ccorrCoefIn);
        *backlightOut = backlightIn;
    }

#else // Chameleon not support
    copyCCorrCoef(mChameleonCcorrCoefOut, ccorrCoefIn);
    *backlightOut = backlightIn;
    UNUSED(sensorIn);
#endif
    copyCCorrCoef(ccorrCoefOut, mChameleonCcorrCoefOut);

    return (chameleonActiveTrigger == 1);
}

bool PictureQuality::calculatePQParamWithFilter(const PQInput &input, PQOutput *output)
{
    int ret = -1;
    unsigned int chameleonCcorrCoefOri[3][3];
    unsigned int bluelightCcorrCoefOri[3][3];
    int chameleonBacklight;
    int chameleonActiveTrigger = 0;

    TPQTransitionInput *transInReg = new TPQTransitionInput;

    DISP_PQ_PARAM *pqparam = input.pqparam;
    DISPLAY_COLOR_REG_T *drvReg = &output->colorRegTarget;

    int configFlag = eConfigNone;
    bool do_next = false;

    PQTimeValue       begin;
    PQTimeValue       end;
    uint64_t          diff = 0;

    //PQ_LOGD("[PQ_SERVICE] calculatePQParamWithFilter, gsat[%d], cont[%d], bri[%d] shp[%d]", pqparam->u4SatGain, pqparam->u4Contrast, pqparam->u4Brightness,pqparam->u4SHPGain);
    //PQ_LOGD("[PQ_SERVICE] calculatePQParamWithFilter, hue0[%d], hue1[%d], hue2[%d], hue3[%d]", pqparam->u4HueAdj[0], pqparam->u4HueAdj[1], pqparam->u4HueAdj[2], pqparam->u4HueAdj[3]);
    //PQ_LOGD("[PQ_SERVICE] calculatePQParamWithFilter, sat0[%d], sat1[%d], sat2[%d], sat3[%d]", pqparam->u4SatAdj[0], pqparam->u4SatAdj[1], pqparam->u4SatAdj[2], pqparam->u4SatAdj[3]);

    getCCorrCoefByIndex(m_pqparam.u4Ccorr, chameleonCcorrCoefOri);
    configCcorrUnitGain(chameleonCcorrCoefOri);
    PQSERVICE_TIMER_GET_CURRENT_TIME(begin);
    chameleonDisplay(&chameleonBacklight, bluelightCcorrCoefOri, input.oriBacklight, chameleonCcorrCoefOri, input.sensorInput);
    PQSERVICE_TIMER_GET_CURRENT_TIME(end);
    PQSERVICE_TIMER_GET_DURATION_IN_MS(begin, end, diff);
    if (diff >= PQ_TIMER_LIMIT) {
        PQ_LOGD("[PQ_SERVICE] chameleonDisplay takes %lu ms\n", (unsigned long)diff);
    }

    transInReg->InputColor.Backlight = chameleonBacklight;
    PQSERVICE_TIMER_GET_CURRENT_TIME(begin);
    do_next = blulightDefender(pqparam, transInReg, bluelightCcorrCoefOri);
    PQSERVICE_TIMER_GET_CURRENT_TIME(end);
    PQSERVICE_TIMER_GET_DURATION_IN_MS(begin, end, diff);
    if (diff >= PQ_TIMER_LIMIT) {
        PQ_LOGD("[PQ_SERVICE] blulightDefender takes %lu ms\n", (unsigned long)diff);
    }

#ifdef TRANSITION_SUPPORT
    if (do_next == true) {
        if (mPQTransition->isEnabled() == true) {
            TPQTransitionOutput *transoutReg = new TPQTransitionOutput;
            int brightToDarkStep;
            int darkToBrightStep;

            if (mTransitionInputMode == PQ_TUNING_READING) {
                // Save composed registers for tuning tool reading
                copyTuningField(mTransitionInput, transInReg, sizeof(TPQTransitionInput));
            } else if (mTransitionInputMode == PQ_TUNING_OVERWRITTEN) {
                // Always apply tool-given input
                copyTuningField(transInReg, mTransitionInput, sizeof(TPQTransitionInput));
            }

            if (mForceTransitionStep != PQ_DEFAULT_TRANSITION_ON_STEP) {
                brightToDarkStep = mPQTransition->GetBrightToDarkStep();
                darkToBrightStep = mPQTransition->GetDarkToBrightStep();
                mPQTransition->SetBrightToDarkStep(mForceTransitionStep);
                mPQTransition->SetDarkToBrightStep(mForceTransitionStep);
            }

            PQSERVICE_TIMER_GET_CURRENT_TIME(begin);
            dumpTransitionRegisters("Transition input:", transInReg, eRegTransitionIn);
            mPQTransition->onCalculate(*transInReg, transoutReg);
            dumpTransitionRegisters("Transition output:", transoutReg, eRegTransitionOut);
            PQSERVICE_TIMER_GET_CURRENT_TIME(end);
            PQSERVICE_TIMER_GET_DURATION_IN_MS(begin, end, diff);
            if (diff >= PQ_TIMER_LIMIT) {
                PQ_LOGD("[PQ_SERVICE] Transition algo takes %lu ms\n", (unsigned long)diff);
            }

            if (mForceTransitionStep != PQ_DEFAULT_TRANSITION_ON_STEP) {
                mPQTransition->SetBrightToDarkStep(brightToDarkStep);
                mPQTransition->SetDarkToBrightStep(darkToBrightStep);
            }

            if (mTransitionOutputMode == PQ_TUNING_READING) {
                // Save composed registers for tuning tool reading
                copyTuningField(mTransitionOutput, transoutReg, sizeof(TPQTransitionOutput));
            } else if (mBLOutputMode == PQ_TUNING_OVERWRITTEN) {
                // Always apply tool-given input
                copyTuningField(transoutReg, mTransitionOutput, sizeof(TPQTransitionOutput));
            }

            PQSERVICE_TIMER_GET_CURRENT_TIME(begin);
            if (compareDrvColorRegisters(drvReg, transoutReg)) {
                configFlag |= eConfigColor;
            }

            if (compareDrvCCorrCoef(output->ccorrCoef, transoutReg->OutputColor.ColorReg.CCORR_COEF)) {
                configFlag |= eConfigCCORR;
            }

            if (translateDrvRegisters(drvReg, transoutReg, eRegTransitionOut)) {
                copyCCorrCoef(output->ccorrCoef, transoutReg->OutputColor.ColorReg.CCORR_COEF);
            } else {
                configFlag = eConfigNone;
                do_next = false;
            }
            PQSERVICE_TIMER_GET_CURRENT_TIME(end);
            PQSERVICE_TIMER_GET_DURATION_IN_MS(begin, end, diff);
            if (diff >= PQ_TIMER_LIMIT) {
                PQ_LOGD("[PQ_SERVICE] Transition compare and copy %lu ms\n", (unsigned long)diff);
            }

            if (mPQTransition->isFinished() == 1) {
                ClearForceTransitionStep();
                configFlag |= eConfigLast;
            }

            output->targetBacklight = transoutReg->OutputColor.Backlight;

            delete transoutReg;
        } else {
            if (translateDrvRegisters(drvReg, transInReg, eRegTransitionIn)) {
                copyCCorrCoef(output->ccorrCoef, transInReg->InputColor.ColorReg.CCORR_COEF);
                configFlag = eConfigColor | eConfigCCORR | eConfigLast;
            } else {
                do_next = false;
            }

            output->targetBacklight = transInReg->InputColor.Backlight;
        }
    }

    if (configFlag & eConfigLast) {
        enablePQEvent(false);
    } else {
        enablePQEvent(true);
    }
#else

#ifdef BLULIGHT_DEFENDER_SUPPORT
    if (translateDrvRegisters(drvReg, transInReg, eRegTransitionIn)) {
        copyCCorrCoef(output->ccorrCoef, transInReg->InputColor.ColorReg.CCORR_COEF);
        configFlag = eConfigColor | eConfigCCORR | eConfigLast;
    } else {
        do_next = false;
    }
#endif                //BLULIGHT_DEFENDER_SUPPORT

#endif                //TRANSITION_SUPPORT

    delete transInReg;

    if (do_next == true) {
        do_next = composeConfigColorRegisters(drvReg, pqparam);
    }

    output->configFlag = configFlag;

    clearEvents();

    return do_next;
}

bool PictureQuality::setPQParamWithFilter(int drvID, const PQOutput &output)
{
    int ret = 0;
    DISP_PQ_PARAM *pqparam = output.pqparam;
    PQ_LOGD("[PQ_SERVICE] setPQParamWithFilter configFlag: %d\n", output.configFlag);
#if defined(BLULIGHT_DEFENDER_SUPPORT) || defined(TRANSITION_SUPPORT)
#ifndef DISP_COLOR_OFF
    if (output.configFlag & eConfigColor) {
        ret = ioctl(drvID, DISP_IOCTL_SET_COLOR_REG, &output.colorRegTarget);
        dumpDrvColorRegisters("CONFIG color:", &output.colorRegTarget);
    }
#endif
    if (output.configFlag & eConfigCCORR) {
        configCCorrCoef(drvID, output.ccorrCoef);
    }

    if (ret != 0) {
        PQ_LOGE("[PQ_SERVICE] setPQParamWithFilter: DISP_IOCTL_SET_COLOR_REG: ret = %d", ret);
    }
#else                // Legacy Chip
#ifndef DISP_COLOR_OFF
    ret = ioctl(drvID, DISP_IOCTL_SET_PQPARAM, pqparam);
    PQ_LOGD("[PQ_SERVICE] setPQParamWithFilter: DISP_IOCTL_SET_PQPARAM: ret = %d", ret);
#endif
    configCCorrCoefByIndex(pqparam->u4Ccorr, drvID);
#endif                //BLULIGHT_DEFENDER_SUPPORT || TRANSITION_SUPPORT

    return (ret == 0);
}

void PictureQuality::refreshDisplay(void)
{
    // Please use lock before this function
#ifdef BASIC_PACKAGE
    clearEvents();
#else

#ifdef TRANSITION_SUPPORT
    enablePQEvent(true);
#else
    mPQInput->pqparam = &m_pqparam;
    mPQOutput->pqparam = &m_pqparam;
    if (calculatePQParamWithFilter(*mPQInput, mPQOutput)) {
        setPQParamWithFilter(m_drvID, *mPQOutput);
    }
#endif

#endif
}

status_t PictureQuality::enablePQEvent(bool enable)
{
    int ret = NO_ERROR;
#ifdef TRANSITION_SUPPORT
    int enableValue;

    enableValue = enable ? 1 : 0;
    if (enable != m_EventEnabled)
        PQ_LOGD("[PQ_SERVICE]enablePQEvent: %d -> %d", (int)m_EventEnabled, enableValue);

    if ((ret = ioctl(m_drvID, DISP_IOCTL_CCORR_EVENTCTL, &enableValue)) != 0) {
        PQ_LOGE("DISP_IOCTL_CCORR_EVENTCTL error: %d", ret);
    } else {
        m_EventEnabled = enable;
}
#else
    UNUSED(enable);
#endif
    return ret;
}

void PictureQuality::dumpColorRegisters(const char *prompt, void *_colorReg)
{
    ColorRegisters *colorReg = static_cast<ColorRegisters*>(_colorReg);

    if (mBlueLightDebugFlag & 0x1 || mDebugLevel & eDebugTransition) {
        static const int buf_size = 512;
        char *buffer = new char[buf_size];

        PQ_LOGD("[PQ_SERVICE] %s", prompt);
        PQ_LOGD("[PQ_SERVICE] ColorRegisters: Sat:%d, Con:%d, Bri: %d",
            colorReg->GLOBAL_SAT, colorReg->CONTRAST, colorReg->BRIGHTNESS);

        #define ARR_LEN(arr) (sizeof(arr) / sizeof(arr[0]))

        PQ_LOGD("[PQ_SERVICE] PARTIAL_Y:%s",
            printIntArray(buffer, buf_size, &colorReg->PARTIAL_Y[0], ARR_LEN(colorReg->PARTIAL_Y)));

        for (int i = 0; i < CLR_PQ_PARTIALS_CONTROL; i++) {
            PQ_LOGD("[PQ_SERVICE] PURP_TONE_S[%d]:%s", i,
                printIntArray(buffer, buf_size, &colorReg->PURP_TONE_S[i][0], ARR_LEN(colorReg->PURP_TONE_S[0])));
            PQ_LOGD("[PQ_SERVICE] SKIN_TONE_S[%d]:%s", i,
                printIntArray(buffer, buf_size, &colorReg->SKIN_TONE_S[i][0], ARR_LEN(colorReg->SKIN_TONE_S[0])));
            PQ_LOGD("[PQ_SERVICE] GRASS_TONE_S[%d]:%s", i,
                printIntArray(buffer, buf_size, &colorReg->GRASS_TONE_S[i][0], ARR_LEN(colorReg->GRASS_TONE_S[0])));
            PQ_LOGD("[PQ_SERVICE] SKY_TONE_S[%d]:%s", i,
                printIntArray(buffer, buf_size, &colorReg->SKY_TONE_S[i][0], ARR_LEN(colorReg->SKY_TONE_S[0])));
        }

        PQ_LOGD("[PQ_SERVICE] PURP_TONE_H:%s",
            printIntArray(buffer, buf_size, &colorReg->PURP_TONE_H[0], ARR_LEN(colorReg->PURP_TONE_H)));
        PQ_LOGD("[PQ_SERVICE] SKIN_TONE_H:%s",
            printIntArray(buffer, buf_size, &colorReg->SKIN_TONE_H[0], ARR_LEN(colorReg->SKIN_TONE_H)));
        PQ_LOGD("[PQ_SERVICE] GRASS_TONE_H:%s",
            printIntArray(buffer, buf_size, &colorReg->GRASS_TONE_H[0], ARR_LEN(colorReg->GRASS_TONE_H)));
        PQ_LOGD("[PQ_SERVICE] SKY_TONE_H:%s",
            printIntArray(buffer, buf_size, &colorReg->SKY_TONE_H[0], ARR_LEN(colorReg->SKY_TONE_H)));
        PQ_LOGD("[PQ_SERVICE] CCORR_COEF:%s",
            printIntArray(buffer, buf_size, reinterpret_cast<unsigned int*>(colorReg->CCORR_COEF), 9));

        #undef ARR_LEN

        delete [] buffer;
    }
}

void PictureQuality::dumpDrvColorRegisters(const char *prompt, const DISPLAY_COLOR_REG_T *colorReg)
{
    if (mDebugLevel & eDebugColor) {
        static const int buf_size = 512;
        char *buffer = new char[buf_size];

        PQ_LOGD("[PQ_SERVICE] %s", prompt);
        PQ_LOGD("[PQ_SERVICE] ColorRegisters: Sat:%d, Con:%d, Bri: %d",
            colorReg->GLOBAL_SAT, colorReg->CONTRAST, colorReg->BRIGHTNESS);

        #define ARR_LEN(arr) (sizeof(arr) / sizeof(arr[0]))

        PQ_LOGD("[PQ_SERVICE] PARTIAL_Y:%s",
            printIntArray(buffer, buf_size, &colorReg->PARTIAL_Y[0], ARR_LEN(colorReg->PARTIAL_Y)));

        for (int i = 0; i < CLR_PQ_PARTIALS_CONTROL; i++) {
            PQ_LOGD("[PQ_SERVICE] PURP_TONE_S[%d]:%s", i,
                printIntArray(buffer, buf_size, &colorReg->PURP_TONE_S[i][0], ARR_LEN(colorReg->PURP_TONE_S[0])));
            PQ_LOGD("[PQ_SERVICE] SKIN_TONE_S[%d]:%s", i,
                printIntArray(buffer, buf_size, &colorReg->SKIN_TONE_S[i][0], ARR_LEN(colorReg->SKIN_TONE_S[0])));
            PQ_LOGD("[PQ_SERVICE] GRASS_TONE_S[%d]:%s", i,
                printIntArray(buffer, buf_size, &colorReg->GRASS_TONE_S[i][0], ARR_LEN(colorReg->GRASS_TONE_S[0])));
            PQ_LOGD("[PQ_SERVICE] SKY_TONE_S[%d]:%s", i,
                printIntArray(buffer, buf_size, &colorReg->SKY_TONE_S[i][0], ARR_LEN(colorReg->SKY_TONE_S[0])));
        }

        PQ_LOGD("[PQ_SERVICE] PURP_TONE_H:%s",
            printIntArray(buffer, buf_size, &colorReg->PURP_TONE_H[0], ARR_LEN(colorReg->PURP_TONE_H)));
        PQ_LOGD("[PQ_SERVICE] SKIN_TONE_H:%s",
            printIntArray(buffer, buf_size, &colorReg->SKIN_TONE_H[0], ARR_LEN(colorReg->SKIN_TONE_H)));
        PQ_LOGD("[PQ_SERVICE] GRASS_TONE_H:%s",
            printIntArray(buffer, buf_size, &colorReg->GRASS_TONE_H[0], ARR_LEN(colorReg->GRASS_TONE_H)));
        PQ_LOGD("[PQ_SERVICE] SKY_TONE_H:%s",
            printIntArray(buffer, buf_size, &colorReg->SKY_TONE_H[0], ARR_LEN(colorReg->SKY_TONE_H)));

        #undef ARR_LEN

        delete [] buffer;
    }
}

void PictureQuality::dumpCCORRRegisters(const char *prompt, uint32_t ccorrCoefIn[3][3])
{
    int i;

    if (mDebugLevel & eDebugCCORR) {
        PQ_LOGD("[PQ_SERVICE] %s", prompt);
        for (i=0; i<3; i++)
            PQ_LOGD("[PQ_SERVICE] ccorr_coef: %d %4d %4d %4d\n", i,
                ccorrCoefIn[i][0], ccorrCoefIn[i][1], ccorrCoefIn[i][2]);
    }
}

void PictureQuality::dumpChameleonInRegisters(const char *prompt, void *_chameleonReg)
{
#ifdef CHAMELEON_DISPLAY_SUPPORT
    int i;
    TChameleonDisplayInput *chameleonReg = static_cast<TChameleonDisplayInput*>(_chameleonReg);

    if (mDebugLevel & eDebugChameleon) {
        PQ_LOGD("[PQ_SERVICE] %s", prompt);
        PQ_LOGD("[PQ_SERVICE] backlight: %d", chameleonReg->CurrentBacklightSettings);
        PQ_LOGD("[PQ_SERVICE] sensor value: R=%d G=%d B=%d W=%d",
            chameleonReg->SensorValue.R,chameleonReg->SensorValue.G,
            chameleonReg->SensorValue.B,chameleonReg->SensorValue.W);
        for (i=0; i<3; i++)
            PQ_LOGD("[PQ_SERVICE] ccorr_coef: %d %4d %4d %4d\n", i,
                chameleonReg->OriginalCCORR.Coef[i][0], chameleonReg->OriginalCCORR.Coef[i][1],
                chameleonReg->OriginalCCORR.Coef[i][2]);
    }
#else
    UNUSED(prompt);
    UNUSED(_chameleonReg);
#endif
}

void PictureQuality::dumpChameleonOutRegisters(const char *prompt, void *_chameleonReg)
{
#ifdef CHAMELEON_DISPLAY_SUPPORT
    int i;
    TChameleonDisplayOutput *chameleonReg = static_cast<TChameleonDisplayOutput*>(_chameleonReg);

    if (mDebugLevel & eDebugChameleon) {
        PQ_LOGD("[PQ_SERVICE] %s", prompt);
        PQ_LOGD("[PQ_SERVICE] Target backlight: %d", chameleonReg->TargetBacklight);
        for (i=0; i<3; i++)
            PQ_LOGD("[PQ_SERVICE] ccorr_coef: %d %4d %4d %4d\n", i,
                chameleonReg->TargetCCORR.Coef[i][0], chameleonReg->TargetCCORR.Coef[i][1],
                chameleonReg->TargetCCORR.Coef[i][2]);
    }
#else
    UNUSED(prompt);
    UNUSED(_chameleonReg);
#endif
}

void PictureQuality::dumpTransitionRegisters(const char *prompt, void *_transitionReg, int dumpFlag)
{
    TPQTransitionInput *transitionInReg;
    TPQTransitionOutput *transitionOutReg;
    ColorRegisters *colorReg;
    int backlight;
    int checkCode;

    if (mDebugLevel & eDebugTransition) {
        if (dumpFlag == eRegTransitionIn) {
            transitionInReg = static_cast<TPQTransitionInput*>(_transitionReg);
            colorReg = &transitionInReg->InputColor.ColorReg;
            backlight = transitionInReg->InputColor.Backlight;
            checkCode = transitionInReg->InputColor.CheckCode;
        } else if (dumpFlag == eRegTransitionOut) {
            transitionOutReg = static_cast<TPQTransitionOutput*>(_transitionReg);
            colorReg = &transitionOutReg->OutputColor.ColorReg;
            backlight = transitionOutReg->OutputColor.Backlight;
            checkCode = transitionOutReg->OutputColor.CheckCode;
        } else {
            return;
        }

        dumpColorRegisters(prompt, colorReg);
        PQ_LOGD("[PQ_SERVICE] Transition: bl:%d", backlight);
    }
}

status_t PictureQuality::getGammaIndex_impl(int32_t *index)
{
    Mutex::Autolock _l(mLock);
    char value[PROPERTY_VALUE_MAX];
    int32_t p_index = 0;

    property_get(GAMMA_INDEX_PROPERTY_NAME, value, "-1");
    p_index = atoi(value);
    if (p_index < 0)
        p_index = GAMMA_INDEX_DEFAULT;
    *index = p_index;

    return NO_ERROR;
}

status_t PictureQuality::setTuningField_impl(int32_t module, int32_t field, int32_t value)
{
    bool isNoError = true;

    PQ_LOGD("[PQ_SERVICE] PQService : write module 0x%x, field 0x%x = 0x%x\n", module, field, value);

    switch (module) {
    case MOD_DISPLAY:
        if (field == 0 && value == 0) {
            Mutex::Autolock _l(mLock);
            refreshDisplay();
            return NO_ERROR;
        }
        break;
#ifdef CHAMELEON_DISPLAY_SUPPORT
    case MOD_CHAMELEON_ALGO:
        if (field == 0xffff)
        {
            return NO_ERROR;
        }
        else if (field == 0xfffe && value == 0x0)
        {
            Mutex::Autolock _l(mLock);
            setEvent(eEvtChameleon);
            refreshDisplay();
            return NO_ERROR;
        }
        else
        {
            PQ_LOGD("[PQ_SERVICE] Chameleon : set field 0x%x = 0x%x\n", field, value);
            if (chameleonDisplayProcess->setTuningField(field, static_cast<unsigned int>(value)))
                return NO_ERROR;
        }
        break;
    case MOD_CHAMELEON_INPUT:
        if (field == 0xffff) { // Change mode
            Mutex::Autolock _l(mLock);

            if (mChameleonInput == NULL) {
                mChameleonInput = new TChameleonDisplayInput;
            }

            if (PQ_TUNING_NORMAL <= value && value < PQ_TUNING_END) {
                mChameleonInputMode = static_cast<PQTuningMode>(value);
                if (mChameleonInputMode == PQ_TUNING_READING) {
                    setEvent(eEvtChameleon);
                    refreshDisplay();
                }
            } else {
                return BAD_INDEX;
            }

            PQ_LOGD("[PQ_SERVICE] Chameleon: input change tuning mode to %d\n", mChameleonInputMode);

            return NO_ERROR;
        } else if (field == 0xfffe && value == 0x0) {
            Mutex::Autolock _l(mLock);
            setEvent(eEvtChameleon);
            refreshDisplay();
            return NO_ERROR;
        } else {
            Mutex::Autolock _l(mLock);

            if (mChameleonInputMode == PQ_TUNING_OVERWRITTEN && mChameleonInput != NULL) {
                if (0 <= field && size_t(field) < sizeof(TChameleonDisplayInput) && (field & 0x3) == 0) {
                    char *inputPtr = reinterpret_cast<char*>(mChameleonInput);
                    *reinterpret_cast<int*>(inputPtr + field) = value;
                    PQ_LOGD("[PQ_SEVICE] Chameleon: input overwrite [0x%x] = %d\n", field, value);

                    return NO_ERROR;
                }
            } else {
                PQ_LOGE("[PQ_SERVICE] Chameleon: Not overwritten mode: %d\n", mChameleonInputMode);
            }
        }
        break;
    case MOD_CHAMELEON_OUTPUT:
        if (field == 0xffff) { // Change mode
            Mutex::Autolock _l(mLock);

            if (mChameleonOutput == NULL) {
                mChameleonOutput = new TChameleonDisplayOutput;
            }

            if (PQ_TUNING_NORMAL <= value && value < PQ_TUNING_END) {
                mChameleonOutputMode = static_cast<PQTuningMode>(value);
                if (mChameleonOutputMode == PQ_TUNING_READING) {
                    setEvent(eEvtChameleon);
                    refreshDisplay();
                }
            } else {
                return BAD_INDEX;
            }

            PQ_LOGD("[PQ_SERVICE] Chameleon: output change tuning mode to %d\n", mChameleonOutputMode);

            return NO_ERROR;
        } else if (field == 0xfffe && value == 0x0) {
            Mutex::Autolock _l(mLock);
            setEvent(eEvtChameleon);
            refreshDisplay();
            return NO_ERROR;
        } else {
            Mutex::Autolock _l(mLock);

            if (mChameleonOutputMode == PQ_TUNING_OVERWRITTEN && mChameleonOutput != NULL) {
                if (0 <= field && size_t(field) < sizeof(TChameleonDisplayOutput) && (field & 0x3) == 0) {
                    char *inputPtr = reinterpret_cast<char*>(mChameleonOutput);
                    *reinterpret_cast<int*>(inputPtr + field) = value;
                    PQ_LOGD("[PQ_SERVICE] Chameleon: output overwrite [0x%x] = %d\n", field, value);

                    return NO_ERROR;
                }
            } else {
                PQ_LOGE("[PQ_SERVICE] Chameleon: Not overwritten mode: %d\n", mChameleonOutputMode);
            }
        }
        break;
#endif
    case MOD_BLUE_LIGHT_ALGO:
        if (field == 0xffff)
        {
            return NO_ERROR;
        }
        else if (field == 0xfffe && value == 0x0)
        {
            Mutex::Autolock _l(mLock);
            refreshDisplay();
            return NO_ERROR;
        }
        else
        {
            if (blueLight->setTuningField(field, static_cast<unsigned int>(value)))
                return NO_ERROR;
        }
        break;
    case MOD_BLUE_LIGHT_INPUT:
        if (field == 0xffff) { // Change mode
            Mutex::Autolock _l(mLock);

            if (mBLInput == NULL) {
                mBLInput = new ColorRegistersTuning;
            }

            if (PQ_TUNING_NORMAL <= value && value < PQ_TUNING_END) {
                mBLInputMode = static_cast<PQTuningMode>(value);
                if (mBLInputMode == PQ_TUNING_READING)
                    refreshDisplay();
            } else {
                return BAD_INDEX;
            }

            PQ_LOGD("[PQ_SERVICE] Blue-light input change tuning mode to %d\n", mBLInputMode);

            return NO_ERROR;
        } else if (field == 0xfffe && value == 0x0){
            Mutex::Autolock _l(mLock);
            refreshDisplay();
            return NO_ERROR;
        } else {
            Mutex::Autolock _l(mLock);

            if (mBLInputMode == PQ_TUNING_OVERWRITTEN && mBLInput != NULL) {
                if (0 <= field && size_t(field) < sizeof(ColorRegistersTuning) && (field & 0x3) == 0) {
                    char *inputPtr = reinterpret_cast<char*>(mBLInput);
                    *reinterpret_cast<int*>(inputPtr + field) = value;
                    PQ_LOGD("[PQ_SERVICE] Blue-light input overwrite [0x%x] = %d\n", field, value);

                    return NO_ERROR;
                }
            } else {
                PQ_LOGE("[PQ_SERVICE] Blue-light: Not overwritten mode: %d\n", mBLInputMode);
            }
        }
        break;
    case MOD_BLUE_LIGHT_OUTPUT:
        if (field == 0xffff) { // Change mode
            Mutex::Autolock _l(mLock);

            if (mBLOutput == NULL) {
                mBLOutput = new ColorRegistersTuning;
            }

            if (PQ_TUNING_NORMAL <= value && value < PQ_TUNING_END) {
                mBLOutputMode = static_cast<PQTuningMode>(value);
                if (mBLOutputMode == PQ_TUNING_READING)
                    refreshDisplay();
            } else {
                return BAD_INDEX;
            }

            PQ_LOGD("[PQ_SERVICE] Blue-light output change tuning mode to %d\n", mBLOutputMode);

            return NO_ERROR;
        } else if (field == 0xfffe && value == 0x0) {
            Mutex::Autolock _l(mLock);
            refreshDisplay();
            return NO_ERROR;
        } else {
            Mutex::Autolock _l(mLock);

            if (mBLOutputMode == PQ_TUNING_OVERWRITTEN && mBLOutput != NULL) {
                if (0 <= field && size_t(field) < sizeof(ColorRegistersTuning) && (field & 0x3) == 0) {
                    char *inputPtr = reinterpret_cast<char*>(mBLOutput);
                    *reinterpret_cast<int*>(inputPtr + field) = value;
                    PQ_LOGD("[PQ_SERVICE] Blue-light output overwrite [0x%x] = %d\n", field, value);

                    return NO_ERROR;
                }
            } else {
                PQ_LOGE("[PQ_SERVICE] Blue-light: Not overwritten mode: %d\n", mBLOutputMode);
            }
        }
        break;
    case MOD_TRANSITION_ALGO:
        if (field == 0xffff)
        {
            return NO_ERROR;
        }
        else if (field == 0xfffe && value == 0x0)
        {
            Mutex::Autolock _l(mLock);
            refreshDisplay();
            return NO_ERROR;
        }
        else
        {
            PQ_LOGD("[PQ_SERVICE] Transition : set field 0x%x = 0x%x\n", field, value);
            if (mPQTransition->setTuningField(field, static_cast<unsigned int>(value)))
                return NO_ERROR;
        }
        break;
    case MOD_TRANSITION_INPUT:
        if (field == 0xffff) { // Change mode
            Mutex::Autolock _l(mLock);

            if (mTransitionInput == NULL) {
                mTransitionInput = new TPQTransitionInput;
            }

            if (PQ_TUNING_NORMAL <= value && value < PQ_TUNING_END) {
                mTransitionInputMode = static_cast<PQTuningMode>(value);
                if (mTransitionInputMode == PQ_TUNING_READING)
                    refreshDisplay();
            } else {
                return BAD_INDEX;
            }

            PQ_LOGD("[PQ_SERVICE] Transition: input change tuning mode to %d\n", mTransitionInputMode);

            return NO_ERROR;
        } else if (field == 0xfffe && value == 0x0) {
            Mutex::Autolock _l(mLock);
            refreshDisplay();
            return NO_ERROR;
        } else {
            Mutex::Autolock _l(mLock);

            if (mTransitionInputMode == PQ_TUNING_OVERWRITTEN && mTransitionInput != NULL) {
                if (0 <= field && size_t(field) < sizeof(TPQTransitionInput) && (field & 0x3) == 0) {
                    char *inputPtr = reinterpret_cast<char*>(mTransitionInput);
                    *reinterpret_cast<int*>(inputPtr + field) = value;
                    PQ_LOGD("[PQ_SEVICE] Transition: input overwrite [0x%x] = %d\n", field, value);

                    return NO_ERROR;
                }
            } else {
                PQ_LOGE("[PQ_SERVICE] Transition: Not overwritten mode: %d\n", mTransitionInputMode);
            }
        }
        break;
    case MOD_TRANSITION_OUTPUT:
        if (field == 0xffff) { // Change mode
            Mutex::Autolock _l(mLock);

            if (mTransitionOutput == NULL) {
                mTransitionOutput = new TPQTransitionOutput;
            }

            if (PQ_TUNING_NORMAL <= value && value < PQ_TUNING_END) {
                mTransitionOutputMode = static_cast<PQTuningMode>(value);
                if (mTransitionOutputMode == PQ_TUNING_READING)
                    refreshDisplay();
            } else {
                return BAD_INDEX;
            }

            PQ_LOGD("[PQ_SERVICE] Transition: output change tuning mode to %d\n", mTransitionOutputMode);

            return NO_ERROR;
        } else if (field == 0xfffe && value == 0x0) {
            Mutex::Autolock _l(mLock);
            refreshDisplay();
            return NO_ERROR;
        } else {
            Mutex::Autolock _l(mLock);

            if (mTransitionOutputMode == PQ_TUNING_OVERWRITTEN && mTransitionOutput != NULL) {
                if (0 <= field && size_t(field) < sizeof(TPQTransitionOutput) && (field & 0x3) == 0) {
                    char *outputPtr = reinterpret_cast<char*>(mTransitionOutput);
                    *reinterpret_cast<int*>(outputPtr + field) = value;
                    PQ_LOGD("[PQ_SERVICE] Transition: output overwrite [0x%x] = %d\n", field, value);

                    return NO_ERROR;
                }
            } else {
                PQ_LOGE("[PQ_SERVICE] Transition: Not overwritten mode: %d\n", mTransitionOutputMode);
            }
        }
        break;
    case MOD_DS_SWREG:
        isNoError = m_AshmemProxy->setPQValueToAshmem(PROXY_DS_SWREG, field, value, m_PQScenario);
        if (isNoError == true)
        {
            return NO_ERROR;
        }
        break;
    case MOD_DS_INPUT:
        isNoError = m_AshmemProxy->setPQValueToAshmem(PROXY_DS_INPUT, field, value, m_PQScenario);
        if (isNoError == true)
        {
            return NO_ERROR;
        }
        break;
    case MOD_DS_OUTPUT:
        isNoError = m_AshmemProxy->setPQValueToAshmem(PROXY_DS_OUTPUT, field, value, m_PQScenario);
        if (isNoError == true)
        {
            return NO_ERROR;
        }
        break;
    case MOD_DC_SWREG:
        isNoError = m_AshmemProxy->setPQValueToAshmem(PROXY_DC_SWREG, field, value, m_PQScenario);
        if (isNoError == true)
        {
            return NO_ERROR;
        }
        break;
    case MOD_DC_INPUT:
        isNoError = m_AshmemProxy->setPQValueToAshmem(PROXY_DC_INPUT, field, value, m_PQScenario);
        if (isNoError == true)
        {
            return NO_ERROR;
        }
        break;
    case MOD_DC_OUTPUT:
        isNoError = m_AshmemProxy->setPQValueToAshmem(PROXY_DC_OUTPUT, field, value, m_PQScenario);
        if (isNoError == true)
        {
            return NO_ERROR;
        }
        break;
    case MOD_RSZ_SWREG:
        isNoError = m_AshmemProxy->setPQValueToAshmem(PROXY_RSZ_SWREG, field, value, m_PQScenario);
        if (isNoError == true)
        {
            return NO_ERROR;
        }
        break;
    case MOD_RSZ_INPUT:
        isNoError = m_AshmemProxy->setPQValueToAshmem(PROXY_RSZ_INPUT, field, value, m_PQScenario);
        if (isNoError == true)
        {
            return NO_ERROR;
        }
        break;
    case MOD_RSZ_OUTPUT:
        isNoError = m_AshmemProxy->setPQValueToAshmem(PROXY_RSZ_OUTPUT, field, value, m_PQScenario);
        if (isNoError == true)
        {
            return NO_ERROR;
        }
        break;
    case MOD_COLOR_SWREG:
        isNoError = m_AshmemProxy->setPQValueToAshmem(PROXY_COLOR_SWREG, field, value, m_PQScenario);
        if (isNoError == true)
        {
            return NO_ERROR;
        }
        break;
    case MOD_COLOR_INPUT:
        isNoError = m_AshmemProxy->setPQValueToAshmem(PROXY_COLOR_INPUT, field, value, m_PQScenario);
        if (isNoError == true)
        {
            return NO_ERROR;
        }
        break;
    case MOD_COLOR_OUTPUT:
        isNoError = m_AshmemProxy->setPQValueToAshmem(PROXY_COLOR_OUTPUT, field, value, m_PQScenario);
        if (isNoError == true)
        {
            return NO_ERROR;
        }
        break;
    case MOD_HDR_SWREG:
        isNoError = m_AshmemProxy->setPQValueToAshmem(PROXY_HDR_SWREG, field, value, m_PQScenario);
        if (isNoError == true)
        {
            return NO_ERROR;
        }
        break;
    case MOD_HDR_INPUT:
        isNoError = m_AshmemProxy->setPQValueToAshmem(PROXY_HDR_INPUT, field, value, m_PQScenario);
        if (isNoError == true)
        {
            return NO_ERROR;
        }
        break;
    case MOD_HDR_OUTPUT:
        isNoError = m_AshmemProxy->setPQValueToAshmem(PROXY_HDR_OUTPUT, field, value, m_PQScenario);
        if (isNoError == true)
        {
            return NO_ERROR;
        }
        break;
    case MOD_CCORR_SWREG:
        isNoError = m_AshmemProxy->setPQValueToAshmem(PROXY_CCORR_SWREG, field, value, m_PQScenario);
        if (isNoError == true)
        {
            return NO_ERROR;
        }
        break;
    case MOD_CCORR_INPUT:
        isNoError = m_AshmemProxy->setPQValueToAshmem(PROXY_CCORR_INPUT, field, value, m_PQScenario);
        if (isNoError == true)
        {
            return NO_ERROR;
        }
        break;
    case MOD_CCORR_OUTPUT:
        isNoError = m_AshmemProxy->setPQValueToAshmem(PROXY_CCORR_OUTPUT, field, value, m_PQScenario);
        if (isNoError == true)
        {
            return NO_ERROR;
        }
        break;
    case MOD_DRE_SWREG:
        isNoError = m_AshmemProxy->setPQValueToAshmem(PROXY_DRE_SWREG, field, value, m_PQScenario);
        if (isNoError == true)
        {
            return NO_ERROR;
        }
        break;
    case MOD_DRE_INPUT:
        isNoError = m_AshmemProxy->setPQValueToAshmem(PROXY_DRE_INPUT, field, value, m_PQScenario);
        if (isNoError == true)
        {
            return NO_ERROR;
        }
        break;
    case MOD_DRE_OUTPUT:
        isNoError = m_AshmemProxy->setPQValueToAshmem(PROXY_DRE_OUTPUT, field, value, m_PQScenario);
        if (isNoError == true)
        {
            return NO_ERROR;
        }
        break;
    case MOD_TDSHP_REG:
        isNoError = m_AshmemProxy->setPQValueToAshmem(PROXY_TDSHP_REG, field, value, m_PQScenario);
        if (isNoError == true)
        {
            return NO_ERROR;
        }
        break;
    case MOD_ULTRARESOLUTION:
        isNoError = m_AshmemProxy->setPQValueToAshmem(PROXY_ULTRARESOLUTION, field, value, m_PQScenario);
        if (isNoError == true)
        {
            return NO_ERROR;
        }
        break;
    case MOD_DYNAMIC_CONTRAST:
        isNoError = m_AshmemProxy->setPQValueToAshmem(PROXY_DYNAMIC_CONTRAST, field, value, m_PQScenario);
        if (isNoError == true)
        {
            return NO_ERROR;
        }
        break;
    }
    return BAD_INDEX;
}

status_t PictureQuality::getTuningField_impl(int32_t module, int32_t field, int32_t *value)
{
    unsigned int uvalue;
    bool isNoError = true;

    switch (module) {
    case MOD_DISPLAY:
        break;
#ifdef CHAMELEON_DISPLAY_SUPPORT
    case MOD_CHAMELEON_ALGO:
        if (field == 0xffff)
        {
            return NO_ERROR;

        } else if (field == 0xfffe) {
            *value = 1;

        } else {
            if (chameleonDisplayProcess->getTuningField(field, &uvalue))
            {
                *value = static_cast<int32_t>(uvalue);
                PQ_LOGD("[PQ_SERVICE] Chameleon : get field 0x%x = 0x%x\n", field, *value);
                return NO_ERROR;
            }
        }
        break;
    case MOD_CHAMELEON_INPUT:
        if (field == 0xffff) { // mode
            Mutex::Autolock _l(mLock);
            *value = mChameleonInputMode;
            return NO_ERROR;
        } else if (field == 0xfffe) {
            *value = 1;
        } else {
            Mutex::Autolock _l(mLock);

            if (mChameleonInput != NULL) {
                if (0 <= field && size_t(field) < sizeof(TChameleonDisplayInput) && (field & 0x3) == 0) {
                    char *inputPtr = reinterpret_cast<char*>(mChameleonInput);
                    *value = *reinterpret_cast<int*>(inputPtr + field);

                    return NO_ERROR;
                }
            } else {
                PQ_LOGE("[PQ_SERVICE] Chameleon input: Not reading mode: %d\n", mChameleonInputMode);
            }
        }
        break;
    case MOD_CHAMELEON_OUTPUT:
        if (field == 0xffff) { // mode
            Mutex::Autolock _l(mLock);
            *value = mChameleonOutputMode;
            return NO_ERROR;
        } else if (field == 0xfffe) {
            *value = 1;
        } else {
            Mutex::Autolock _l(mLock);

            if (mChameleonOutput != NULL) {
                if (0 <= field && size_t(field) < sizeof(TChameleonDisplayOutput) && (field & 0x3) == 0) {
                    char *inputPtr = reinterpret_cast<char*>(mChameleonOutput);
                    *value = *reinterpret_cast<int*>(inputPtr + field);

                    return NO_ERROR;
                }
            } else {
                PQ_LOGE("[PQ_SERVICE] Chameleon output: Not reading mode: %d\n", mChameleonOutputMode);
            }
        }
        break;
#endif
    case MOD_BLUE_LIGHT_ALGO:
        if (field == 0xffff)
        {
            return NO_ERROR;
        }
        else
        {
            if (blueLight->getTuningField(field, &uvalue))
            {
                *value = static_cast<int32_t>(uvalue);
                return NO_ERROR;
            }
        }
        break;
    case MOD_BLUE_LIGHT_INPUT:
        if (field == 0xffff) { // mode
            Mutex::Autolock _l(mLock);
            *value = mBLInputMode;
            return NO_ERROR;
        } else {
            Mutex::Autolock _l(mLock);

            if (mBLInput != NULL) {
                if (0 <= field && size_t(field) < sizeof(ColorRegistersTuning) && (field & 0x3) == 0) {
                    char *inputPtr = reinterpret_cast<char*>(mBLInput);
                    *value = *reinterpret_cast<int*>(inputPtr + field);

                    return NO_ERROR;
                }
            } else {
                PQ_LOGE("[PQ_SERVICE] Blue-light input: Not reading mode: %d\n", mBLInputMode);
            }
        }
        break;
    case MOD_BLUE_LIGHT_OUTPUT:
        if (field == 0xffff) { // mode
            Mutex::Autolock _l(mLock);
            *value = mBLOutputMode;
            return NO_ERROR;
        } else {
            Mutex::Autolock _l(mLock);

            if (mBLOutput != NULL) {
                if (0 <= field && size_t(field) < sizeof(ColorRegistersTuning) && (field & 0x3) == 0) {
                    char *inputPtr = reinterpret_cast<char*>(mBLOutput);
                    *value = *reinterpret_cast<int*>(inputPtr + field);

                    return NO_ERROR;
                }
            } else {
                PQ_LOGE("[PQ_SERVICE] Blue-light output: Not reading mode: %d\n", mBLOutputMode);
            }
        }
        break;
    case MOD_TRANSITION_ALGO:
        if (field == 0xffff)
        {
            return NO_ERROR;
        }
        else
        {
            if (mPQTransition->getTuningField(field, &uvalue))
            {
                *value = static_cast<int32_t>(uvalue);
                return NO_ERROR;
            }
        }
        break;
    case MOD_TRANSITION_INPUT:
        if (field == 0xffff) { // mode
            Mutex::Autolock _l(mLock);
            *value = mTransitionInputMode;
            return NO_ERROR;
        } else {
            Mutex::Autolock _l(mLock);

            if (mTransitionInput != NULL) {
                if (0 <= field && size_t(field) < sizeof(TPQTransitionInput) && (field & 0x3) == 0) {
                    char *inputPtr = reinterpret_cast<char*>(mTransitionInput);
                    *value = *reinterpret_cast<int*>(inputPtr + field);

                    return NO_ERROR;
                }
            } else {
                PQ_LOGE("[PQ_SERVICE] Transition input: Not reading mode: %d\n", mTransitionInputMode);
            }
        }
        break;
    case MOD_TRANSITION_OUTPUT:
        if (field == 0xffff) { // mode
            Mutex::Autolock _l(mLock);
            *value = mTransitionOutputMode;
            return NO_ERROR;
        } else {
            Mutex::Autolock _l(mLock);

            if (mTransitionOutput != NULL) {
                if (0 <= field && size_t(field) < sizeof(TPQTransitionOutput) && (field & 0x3) == 0) {
                    char *outputPtr = reinterpret_cast<char*>(mTransitionOutput);
                    *value = *reinterpret_cast<int*>(outputPtr + field);

                    return NO_ERROR;
                }
            } else {
                PQ_LOGE("[PQ_SERVICE] Transition output: Not reading mode: %d\n", mTransitionOutputMode);
            }
        }
        break;
    case MOD_DS_SWREG:
        isNoError = m_AshmemProxy->getPQValueFromAshmem(PROXY_DS_SWREG, field, value);
        if (isNoError == true)
        {
            PQ_LOGD("[PQ_SERVICE] PQService : read module 0x%x, field 0x%x = 0x%x\n", module, field, *value);
            return NO_ERROR;
        }
        break;
    case MOD_DS_INPUT:
        isNoError = m_AshmemProxy->getPQValueFromAshmem(PROXY_DS_INPUT, field, value);
        if (isNoError == true)
        {
            PQ_LOGD("[PQ_SERVICE] PQService : read module 0x%x, field 0x%x = 0x%x\n", module, field, *value);
            return NO_ERROR;
        }
        break;
    case MOD_DS_OUTPUT:
        isNoError = m_AshmemProxy->getPQValueFromAshmem(PROXY_DS_OUTPUT, field, value);
        if (isNoError == true)
        {
            PQ_LOGD("[PQ_SERVICE] PQService : read module 0x%x, field 0x%x = 0x%x\n", module, field, *value);
            return NO_ERROR;
        }
        break;
    case MOD_DC_SWREG:
        isNoError = m_AshmemProxy->getPQValueFromAshmem(PROXY_DC_SWREG, field, value);
        if (isNoError == true)
        {
            PQ_LOGD("[PQ_SERVICE] PQService : read module 0x%x, field 0x%x = 0x%x\n", module, field, *value);
            return NO_ERROR;
        }
        break;
    case MOD_DC_INPUT:
        isNoError = m_AshmemProxy->getPQValueFromAshmem(PROXY_DC_INPUT, field, value);
        if (isNoError == true)
        {
            PQ_LOGD("[PQ_SERVICE] PQService : read module 0x%x, field 0x%x = 0x%x\n", module, field, *value);
            return NO_ERROR;
        }
        break;
    case MOD_DC_OUTPUT:
        isNoError = m_AshmemProxy->getPQValueFromAshmem(PROXY_DC_OUTPUT, field, value);
        if (isNoError == true)
        {
            PQ_LOGD("[PQ_SERVICE] PQService : read module 0x%x, field 0x%x = 0x%x\n", module, field, *value);
            return NO_ERROR;
        }
        break;
    case MOD_RSZ_SWREG:
        isNoError = m_AshmemProxy->getPQValueFromAshmem(PROXY_RSZ_SWREG, field, value);
        if (isNoError == true)
        {
            PQ_LOGD("[PQ_SERVICE] PQService : read module 0x%x, field 0x%x = 0x%x\n", module, field, *value);
            return NO_ERROR;
        }
        break;
    case MOD_RSZ_INPUT:
        isNoError = m_AshmemProxy->getPQValueFromAshmem(PROXY_RSZ_INPUT, field, value);
        if (isNoError == true)
        {
            PQ_LOGD("[PQ_SERVICE] PQService : read module 0x%x, field 0x%x = 0x%x\n", module, field, *value);
            return NO_ERROR;
        }
        break;
    case MOD_RSZ_OUTPUT:
        isNoError = m_AshmemProxy->getPQValueFromAshmem(PROXY_RSZ_OUTPUT, field, value);
        if (isNoError == true)
        {
            PQ_LOGD("[PQ_SERVICE] PQService : read module 0x%x, field 0x%x = 0x%x\n", module, field, *value);
            return NO_ERROR;
        }
        break;
    case MOD_COLOR_SWREG:
        isNoError = m_AshmemProxy->getPQValueFromAshmem(PROXY_COLOR_SWREG, field, value);
        if (isNoError == true)
        {
            PQ_LOGD("[PQ_SERVICE] PQService : read module 0x%x, field 0x%x = 0x%x\n", module, field, *value);
            return NO_ERROR;
        }
        break;
    case MOD_COLOR_INPUT:
        isNoError = m_AshmemProxy->getPQValueFromAshmem(PROXY_COLOR_INPUT, field, value);
        if (isNoError == true)
        {
            PQ_LOGD("[PQ_SERVICE] PQService : read module 0x%x, field 0x%x = 0x%x\n", module, field, *value);
            return NO_ERROR;
        }
        break;
    case MOD_COLOR_OUTPUT:
        isNoError = m_AshmemProxy->getPQValueFromAshmem(PROXY_COLOR_OUTPUT, field, value);
        if (isNoError == true)
        {
            PQ_LOGD("[PQ_SERVICE] PQService : read module 0x%x, field 0x%x = 0x%x\n", module, field, *value);
            return NO_ERROR;
        }
        break;
    case MOD_HDR_SWREG:
        isNoError = m_AshmemProxy->getPQValueFromAshmem(PROXY_HDR_SWREG, field, value);
        if (isNoError == true)
        {
            PQ_LOGD("[PQ_SERVICE] PQService : read module 0x%x, field 0x%x = 0x%x\n", module, field, *value);
            return NO_ERROR;
        }
        break;
    case MOD_HDR_INPUT:
        isNoError = m_AshmemProxy->getPQValueFromAshmem(PROXY_HDR_INPUT, field, value);
        if (isNoError == true)
        {
            PQ_LOGD("[PQ_SERVICE] PQService : read module 0x%x, field 0x%x = 0x%x\n", module, field, *value);
            return NO_ERROR;
        }
        break;
    case MOD_HDR_OUTPUT:
        isNoError = m_AshmemProxy->getPQValueFromAshmem(PROXY_HDR_OUTPUT, field, value);
        if (isNoError == true)
        {
            PQ_LOGD("[PQ_SERVICE] PQService : read module 0x%x, field 0x%x = 0x%x\n", module, field, *value);
            return NO_ERROR;
        }
        break;
    case MOD_CCORR_SWREG:
        isNoError = m_AshmemProxy->getPQValueFromAshmem(PROXY_CCORR_SWREG, field, value);
        if (isNoError == true)
        {
            PQ_LOGD("[PQ_SERVICE] PQService : read module 0x%x, field 0x%x = 0x%x\n", module, field, *value);
            return NO_ERROR;
        }
        break;
    case MOD_CCORR_INPUT:
        isNoError = m_AshmemProxy->getPQValueFromAshmem(PROXY_CCORR_INPUT, field, value);
        if (isNoError == true)
        {
            PQ_LOGD("[PQ_SERVICE] PQService : read module 0x%x, field 0x%x = 0x%x\n", module, field, *value);
            return NO_ERROR;
        }
        break;
    case MOD_CCORR_OUTPUT:
        isNoError = m_AshmemProxy->getPQValueFromAshmem(PROXY_CCORR_OUTPUT, field, value);
        if (isNoError == true)
        {
            PQ_LOGD("[PQ_SERVICE] PQService : read module 0x%x, field 0x%x = 0x%x\n", module, field, *value);
            return NO_ERROR;
        }
        break;
    case MOD_DRE_SWREG:
        isNoError = m_AshmemProxy->getPQValueFromAshmem(PROXY_DRE_SWREG, field, value);
        if (isNoError == true)
        {
            PQ_LOGD("[PQ_SERVICE] PQService : read module 0x%x, field 0x%x = 0x%x\n", module, field, *value);
            return NO_ERROR;
        }
        break;
    case MOD_DRE_INPUT:
        isNoError = m_AshmemProxy->getPQValueFromAshmem(PROXY_DRE_INPUT, field, value);
        if (isNoError == true)
        {
            PQ_LOGD("[PQ_SERVICE] PQService : read module 0x%x, field 0x%x = 0x%x\n", module, field, *value);
            return NO_ERROR;
        }
        break;
    case MOD_DRE_OUTPUT:
        isNoError = m_AshmemProxy->getPQValueFromAshmem(PROXY_DRE_OUTPUT, field, value);
        if (isNoError == true)
        {
            PQ_LOGD("[PQ_SERVICE] PQService : read module 0x%x, field 0x%x = 0x%x\n", module, field, *value);
            return NO_ERROR;
        }
        break;
    case MOD_TDSHP_REG:
        isNoError = m_AshmemProxy->getPQValueFromAshmem(PROXY_TDSHP_REG, field, value);
        if (isNoError == true)
        {
            PQ_LOGD("[PQ_SERVICE] PQService : read module 0x%x, field 0x%x = 0x%x\n", module, field, *value);
            return NO_ERROR;
        }
        break;
    case MOD_ULTRARESOLUTION:
        isNoError = m_AshmemProxy->getPQValueFromAshmem(PROXY_ULTRARESOLUTION, field, value);
        if (isNoError == true)
        {
            PQ_LOGD("[PQ_SERVICE] PQService : read module 0x%x, field 0x%x = 0x%x\n", module, field, *value);
            return NO_ERROR;
        }
        break;
    case MOD_DYNAMIC_CONTRAST:
        isNoError = m_AshmemProxy->getPQValueFromAshmem(PROXY_DYNAMIC_CONTRAST, field, value);
        if (isNoError == true)
        {
            PQ_LOGD("[PQ_SERVICE] PQService : read module 0x%x, field 0x%x = 0x%x\n", module, field, *value);
            return NO_ERROR;
        }
        break;
    }

    *value = 0;
    return BAD_INDEX;
}


void PictureQuality::runThreadLoop()
{
    if (getTid() == -1) {
        run("PQServiceHAL", PRIORITY_DISPLAY);
        PQ_LOGD("[PQ_SERVICE] PQService : runThreadLoop");
    }
}

status_t PictureQuality::readyToRun()
{
    PQ_LOGD("[PQ_SERVICE] PQService is ready to run.");
    return NO_ERROR;
}

bool PictureQuality::initAshmem(const unsigned int ashmemSize) {
    sp<IMemory> mapped_memory;
    unsigned int *ashmem_base = NULL;

    do {
        if (m_is_ashmem_init == true) {
            break;
        }

        sp<IAllocator> ashmem = IAllocator::getService("ashmem");
        if (ashmem == nullptr) {
            PQ_LOGD("[PQ_SERVICE] failed to get IAllocator HW service");
            break;
        }

        Return<void> result = ashmem->allocate(ashmemSize,
            [&](bool success, const hidl_memory& memory) {
            if (success == true) {
                m_hidl_memory = memory;
                mapped_memory = mapMemory(m_hidl_memory);

                m_AshmemProxy->initMapMemory(mapped_memory);
                m_is_ashmem_init = true;
                PQ_LOGD("[PQ_SERVICE] Get HIDL memory success.");
            }
        });
    } while (0);

    return m_is_ashmem_init;
}

void PictureQuality::initDefaultPQParam()
{
    /*cust PQ default setting*/
    m_pqparam_mapping =
    {
        .image = 80,
        .video = 100,
        .camera = 20,
    } ;

    DISP_PQ_PARAM pqparam_table_temp[PQ_PARAM_TABLE_SIZE] =
    {
        //std_image
        {
            .u4SHPGain = 2,
            .u4SatGain = 4,
            .u4PartialY = 0,
            .u4HueAdj = {9,9,12,12},
            .u4SatAdj = {0,6,10,10},
            .u4Contrast = 4,
            .u4Brightness = 4,
            .u4Ccorr = 0,
#ifdef COLOR_3_0
            .u4ColorLUT = 0
#endif
        },

        //std_video
        {
            .u4SHPGain = 3,
            .u4SatGain = 4,
            .u4PartialY = 0,
            .u4HueAdj = {9,9,12,12},
            .u4SatAdj = {0,6,12,12},
            .u4Contrast = 4,
            .u4Brightness = 4,
            .u4Ccorr = 0,
#ifdef COLOR_3_0
            .u4ColorLUT = 0
#endif
        },

        //std_camera
        {
            .u4SHPGain = 2,
            .u4SatGain = 4,
            .u4PartialY = 0,
            .u4HueAdj = {9,9,12,12},
            .u4SatAdj = {0,6,10,10},
            .u4Contrast = 4,
            .u4Brightness = 4,
            .u4Ccorr = 0,
#ifdef COLOR_3_0
            .u4ColorLUT = 0
#endif
        },

        //viv_image
        {
            .u4SHPGain = 2,
            .u4SatGain = 9,
            .u4PartialY = 0,
            .u4HueAdj = {9,9,12,12},
            .u4SatAdj = {16,16,16,16},
            .u4Contrast = 4,
            .u4Brightness = 4,
            .u4Ccorr = 0,
#ifdef COLOR_3_0
            .u4ColorLUT = 0
#endif
        },

        //viv_video
        {
            .u4SHPGain = 3,
            .u4SatGain = 9,
            .u4PartialY = 0,
            .u4HueAdj = {9,9,12,12},
            .u4SatAdj = {16,16,18,18},
            .u4Contrast = 4,
            .u4Brightness = 4,
            .u4Ccorr = 0,
#ifdef COLOR_3_0
            .u4ColorLUT = 0
#endif
        },

        //viv_camera
        {
            .u4SHPGain = 2,
            .u4SatGain = 4,
            .u4PartialY = 0,
            .u4HueAdj = {9,9,12,12},
            .u4SatAdj = {0,6,10,10},
            .u4Contrast = 4,
            .u4Brightness = 4,
            .u4Ccorr = 0,
#ifdef COLOR_3_0
            .u4ColorLUT = 0
#endif
        },

        //pqparam_usr
        {
            .u4SHPGain = 2,
            .u4SatGain = 9,
            .u4PartialY = 0,
            .u4HueAdj = {9,9,12,12},
            .u4SatAdj = {16,16,16,16},
            .u4Contrast = 4,
            .u4Brightness = 4,
            .u4Ccorr = 0,
#ifdef COLOR_3_0
            .u4ColorLUT = 0
#endif
        }
    };
    memcpy(&m_pqparam_table, &pqparam_table_temp, sizeof(DISP_PQ_PARAM)*PQ_PARAM_TABLE_SIZE);

    m_pic_statndard = new PicModeStandard(&m_pqparam_table[0]);
    m_pic_vivid = new PicModeVivid(&m_pqparam_table[0]);
    m_pic_userdef = new PicModeUserDef(&m_pqparam_table[0]);

    g_PQ_DS_Param = {
    .param =
        {1, -4, 1024, -4, 1024,
         1, 400, 200, 1600, 800,
         128, 8, 4, 12, 16,
         8, 24, -8, -4, -12,
         0, 0, 0,
         8, 4, 12, 16, 8, 24, -8, -4, -12,
         8, 4, 12, 16, 8, 24, -8, -4, -12,
         8, 4, 12, 8, 4, 12,
         1,
         4096, 2048, 1024, 34, 35, 51, 50,
         -1, -2, -4, 0, -4, 0, -1, -2, -1, -2}
    };

    g_PQ_DC_Param = {
    .param =
      {
       1, 1, 0, 0, 0, 0, 0, 0, 0, 0x0A,
       0x30, 0x40, 0x06, 0x12, 40, 0x40, 0x80, 0x40, 0x40, 1,
       0x80, 0x60, 0x80, 0x10, 0x34, 0x40, 0x40, 1, 0x80, 0xa,
       0x19, 0x00, 0x20, 0, 0, 1, 2, 1, 80, 1,
       0, 0, 0, 0, 0, 0, 0, 0, 0,
       0, 0, 0, 0
      }
    };

    g_tdshp_reg = {
    .param =
        {0x10, 0x20, 0x10, 0x4, 0x2, 0x20, 0x3,
         0x2, 0x4, 0x10, 0x3, 0x2, 0x4, 0x10, 0x3,
         0x10, 0x10, 0x10, 0x8, 0x4,
         0}
    };


    rszparam = {
    .param =
        {0, 10, 10, 1, 1, 1, 1, 1, 1,
         1, 24,
         1024, 2048, 4096, 3, 8, 0, 31,
         1024, 1536, 2048, 0, -7, 0, 15,
         4, 1024, 2048, 4096, 0, -2, 0, 255,
         8, 1024, 2048, 4096, 0, -7, 0, 31}

    };
}

void PictureQuality::onALIChanged(void* obj, int32_t aliR, int32_t aliG, int32_t aliB, int32_t aliW)
{
#ifdef CHAMELEON_DISPLAY_SUPPORT
    PictureQuality *service = static_cast<PictureQuality*>(obj);

    //PQ_LOGD("onALIChanged R=%d G=%d B=%d W=%d",aliR,aliG,aliB,aliW);

    Mutex::Autolock _l(service->mLock);

    // overwrite RGBW ambient light value if tuning mode on
    if (service->mChameleonInputMode == PQ_TUNING_OVERWRITTEN && service->mChameleonInput != NULL) {
        TChameleonDisplayInput *chameleonInput = static_cast<TChameleonDisplayInput*>(service->mChameleonInput);
        aliR = chameleonInput->SensorValue.R;
        aliG = chameleonInput->SensorValue.G;
        aliB = chameleonInput->SensorValue.B;
        aliW = chameleonInput->SensorValue.W;
    } else if (service->mDebug.sensorDebugMode == 1) {
        aliR = service->mDebug.sensorInput.aliR;
        aliG = service->mDebug.sensorInput.aliG;
        aliB = service->mDebug.sensorInput.aliB;
        aliW = service->mDebug.sensorInput.aliW;
    }

    if (service->mAllowSensorDebounce == true && service->mSensorInputxyY == false) {
        if (service->chameleonDisplayProcess->onSensorDebounce(aliR, aliG, aliB, aliW) == ENUM_SensorValueChanged) {
            service->mPQInput->sensorInput.aliR = aliR;
            service->mPQInput->sensorInput.aliG = aliG;
            service->mPQInput->sensorInput.aliB = aliB;
            service->mPQInput->sensorInput.aliW = aliW;
            PQ_LOGD("onALIChanged R=%d G=%d B=%d W=%d",
                service->mPQInput->sensorInput.aliR, service->mPQInput->sensorInput.aliG,
                service->mPQInput->sensorInput.aliB, service->mPQInput->sensorInput.aliW);
            service->mAllowSensorDebounce = false;
            service->setEvent(eEvtALI);
            service->enablePQEvent(true);
        }
    } else {
        PQ_LOGD("SensorDebounce not allowed");
    }
#else
    UNUSED(obj);
    UNUSED(aliR);
    UNUSED(aliG);
    UNUSED(aliB);
    UNUSED(aliW);
#endif
}

void PictureQuality::onBacklightChanged(int32_t level_1024)
{
#ifdef CHAMELEON_DISPLAY_SUPPORT
    if (mTargetBacklight != level_1024) {
        PQ_LOGD("[PQ_SERVICE] onBacklightChanged %4d-->%4d", mTargetBacklight, level_1024);
        PQ_LOGD("[PQ_SERVICE] chameleon is enable(%d)", chameleonDisplayProcess->isEnabled());

        if (mTargetBacklight == 0 && level_1024 != 0) {
            if(chameleonDisplayProcess->isEnabled()) {
                mLightSensor->setEnabled(true);
            }
        } else if (mTargetBacklight != 0 && level_1024 == 0) {
            if(chameleonDisplayProcess->isEnabled()) {
                mLightSensor->setEnabled(false);
            }
        }

        mTargetBacklight = level_1024;
        setEvent(eEvtBacklightChange);
    }
#else
    UNUSED(level_1024);
#endif
}

bool PictureQuality::threadLoop()
{
    char value[PROPERTY_VALUE_MAX];
    int i;
    int32_t  status;
    int percentage = m_pqparam_mapping.image;  //default scenario = image
    DISP_PQ_PARAM *p_pqparam = NULL;

    if (initAshmem(ASHMEM_SIZE) == true)
    {
        initPQProperty();
    } else {
        return false;
    }

    /* open the needed object */
    bool configDispColor = false;
    CustParameters &cust = CustParameters::getPQCust();
    if (!cust.isGood())
    {
        PQ_LOGD("[PQ_SERVICE] can't open libpq_cust.so, bypass init config\n");
    }
    /* find the address of function and data objects */
    else
    {

        configDispColor = loadPqparamMappingCoefficient() & loadPqparamTable()
                       & loadPqindexTable();
        loadRSZTable();
        loadDSTable();
        loadDCTable();
        loadHDRTable();
        loadTDSHPTable();
        loadCOLORTable();
        loadGammaEntryTable();
        loadCDparamTable();
        initBlueLight();
        initChameleon();
        initColorShift();
        loadTRSparamTable();
    }

    if (configDispColor)
    {
#ifndef DISP_COLOR_OFF

        PQ_LOGD("[PQ_SERVICE] DISP PQ init start...");

        if (m_drvID < 0)
        {
            PQ_LOGE("PQ device open failed!!");
        }

        // pq index
        ioctl(m_drvID, DISP_IOCTL_SET_PQINDEX, &m_pqindex);

        p_pqparam = &m_pqparam_table[0];
        if (m_PQMode == PQ_PIC_MODE_STANDARD || m_PQMode == PQ_PIC_MODE_VIVID)
        {
            if (m_PQMode == PQ_PIC_MODE_STANDARD) {
                p_pqparam = m_pic_statndard->getPQParam(0);
            } else if (m_PQMode == PQ_PIC_MODE_VIVID) {
                p_pqparam = m_pic_vivid->getPQParam(0);
            }
            memcpy(&m_pqparam, p_pqparam, sizeof(DISP_PQ_PARAM));
            //should be move out of #ifndef DISP_COLOR_OFF
            property_get(PQ_TDSHP_PROPERTY_STR, value, PQ_TDSHP_STANDARD_DEFAULT);
            property_set(PQ_TDSHP_PROPERTY_STR, value);
        }
        else if (m_PQMode == PQ_PIC_MODE_USER_DEF)
        {
            getUserModePQParam();

            p_pqparam = m_pic_userdef->getPQParam(0);
            calcPQStrength(&m_pqparam, p_pqparam, percentage);

            PQ_LOGD("[PQ_SERVICE] --Init_PQ_Userdef, gsat[%d], cont[%d], bri[%d] ", m_pqparam.u4SatGain, m_pqparam.u4Contrast, m_pqparam.u4Brightness);
            PQ_LOGD("[PQ_SERVICE] --Init_PQ_Userdef, hue0[%d], hue1[%d], hue2[%d], hue3[%d] ", m_pqparam.u4HueAdj[0], m_pqparam.u4HueAdj[1], m_pqparam.u4HueAdj[2], m_pqparam.u4HueAdj[3]);
            PQ_LOGD("[PQ_SERVICE] --Init_PQ_Userdef, sat0[%d], sat1[%d], sat2[%d], sat3[%d] ", m_pqparam.u4SatAdj[0], m_pqparam.u4SatAdj[1], m_pqparam.u4SatAdj[2], m_pqparam.u4SatAdj[3]);
        }
        else
        {
            memcpy(&m_pqparam, p_pqparam, sizeof(m_pqparam));
            PQ_LOGE("[PQ][main pq] main, property get... unknown pic_mode[%d]", m_PQMode);
        }
#ifndef TRANSITION_SUPPORT
        mPQInput->pqparam = &m_pqparam;
        mPQOutput->pqparam = &m_pqparam;
        if (calculatePQParamWithFilter(*mPQInput, mPQOutput)) {
            setPQParamWithFilter(m_drvID, *mPQOutput);
        }
#endif
        //status = ioctl(m_drvID, DISP_IOCTL_SET_TDSHPINDEX, &m_tdshpindex);
        //PQ_LOGD("[PQ_SERVICE] DISP_IOCTL_SET_TDSHPINDEX %d...",status);

        PQ_LOGD("[PQ_SERVICE] DISP PQ init end...");

#else // DISP_COLOR_OFF

        // We need a default m_pqparam
        p_pqparam = &m_pqparam_table[0];
        memcpy(&m_pqparam, p_pqparam, sizeof(DISP_PQ_PARAM));
#ifndef TRANSITION_SUPPORT
        mPQInput->pqparam = &m_pqparam;
        mPQOutput->pqparam = &m_pqparam;
        if (calculatePQParamWithFilter(*mPQInput, mPQOutput)) {
            setPQParamWithFilter(m_drvID, *mPQOutput);
        }
#endif // TRANSITION_SUPPORT

#endif // DISP_COLOR_OFF
        PQ_LOGD("[PQ_SERVICE] threadLoop config User_Def PQ... end");
    }  // end of if (configDispColor)

    // set chameleon
    getCCorrCoefByIndex(m_pqparam.u4Ccorr, mChameleonCcorrCoefOut);
    setEvent(eEvtPQChange);

#ifdef TRANSITION_SUPPORT
    int ret = 0;
    int failCount = 0;
    int config_HW = true;
    memset(mPQOutput, 0, sizeof(PQOutput));
#endif

    while(1)
    {
#ifdef TRANSITION_SUPPORT
        {
            Mutex::Autolock _l(mLock);

            mPQInput->oriBacklight = 1024;
            mPQInput->pqparam = &m_pqparam;
            mPQOutput->pqparam = &m_pqparam;

            // blue light and chameleon algorithm calculate
            config_HW = calculatePQParamWithFilter(*mPQInput, mPQOutput);
        }

        if (config_HW == true) // config setting to hardware
            setPQParamWithFilter(m_drvID, *mPQOutput);

        if ((ret = ioctl(m_drvID, DISP_IOCTL_CCORR_GET_IRQ, &mDriverBacklight)) != 0) {
            failCount++;
            PQ_LOGE("DISP_IOCTL_CCORR_GET_IRQ error: %d, fail# = %d", ret, failCount);

            if (failCount < 5)
                usleep(50 * 1000 *(1 << failCount));
            else // failCount >= 5
                sleep(1 << (failCount - 4));
        } else {
            onBacklightChanged(mDriverBacklight);
        }
#else
        sleep(10);
#endif
    }

    return true;
}

int PictureQuality::_getLcmIndexOfGamma()
{
    static int lcmIdx = -1;

    if (lcmIdx == -1) {
        int ret = ioctl(m_drvID, DISP_IOCTL_GET_LCMINDEX, &lcmIdx);
        if (ret == 0) {
            if (lcmIdx < 0 || GAMMA_LCM_MAX <= lcmIdx) {
                PQ_LOGE("Invalid LCM index %d, GAMMA_LCM_MAX = %d", lcmIdx, GAMMA_LCM_MAX);
                lcmIdx = 0;
            }
        } else {
            PQ_LOGE("ioctl(DISP_IOCTL_GET_LCMINDEX) return %d", ret);
            lcmIdx = 0;
        }
    }

    PQ_LOGD("LCM index: %d/%d", lcmIdx, GAMMA_LCM_MAX);

    return lcmIdx;
}

void PictureQuality::_setGammaIndex(int index)
{
#ifndef BASIC_PACKAGE
    if (index < 0 || GAMMA_INDEX_MAX <= index)
        index = GAMMA_INDEX_DEFAULT;

    DISP_GAMMA_LUT_T *driver_gamma = new DISP_GAMMA_LUT_T;

    int lcm_id = _getLcmIndexOfGamma();
    const gamma_entry_t *entry;

#ifdef FACTORY_GAMMA_SUPPORT
    if (m_NvGammaStatus == true)
        entry = &(m_NvGamma[lcm_id][index]);
    else
        entry = &(m_CustGamma[lcm_id][index]);
#else
    entry = &(m_CustGamma[lcm_id][index]);
#endif
    unsigned short R_entry, G_entry, B_entry;
#ifdef GAMMA_LIGHT
    unsigned short R_ref, G_ref, B_ref;
#endif
    driver_gamma->hw_id = DISP_GAMMA0;

#ifdef GAMMA_LIGHT
    for (int i = 0; i < DISP_GAMMA_LUT_SIZE; i+=2) {
        R_entry = (*entry)[0][i];
        G_entry = (*entry)[1][i];
        B_entry = (*entry)[2][i];
        driver_gamma->lut[i] = GAMMA_ENTRY(R_entry, G_entry, B_entry);
    }
    for (int i = 1; i < DISP_GAMMA_LUT_SIZE; i+=2) {
        R_entry = (*entry)[0][i];
        G_entry = (*entry)[1][i];
        B_entry = (*entry)[2][i];

        R_ref = (*entry)[0][i-1];
        G_ref = (*entry)[1][i-1];
        B_ref = (*entry)[2][i-1];
        driver_gamma->lut[i] = GAMMA_ENTRY(abs(R_entry - R_ref),
                                          abs(G_entry - G_ref),
                                          abs(B_entry - B_ref));
    }
#else
    for (int i = 0; i < DISP_GAMMA_LUT_SIZE; i++) {
        R_entry = (*entry)[0][i];
        G_entry = (*entry)[1][i];
        B_entry = (*entry)[2][i];
        driver_gamma->lut[i] = GAMMA_ENTRY(R_entry, G_entry, B_entry);
    }
#endif

    ioctl(m_drvID, DISP_IOCTL_SET_GAMMALUT, driver_gamma);
    delete driver_gamma;
#else           // not define BASIC_PACKAGE
    UNUSED(index);
#endif          // end of BASIC_PACKAGE
}

void PictureQuality::configGamma(int picMode, gamma_entry_t *entry)
{
#if (GAMMA_LCM_MAX > 0) && (GAMMA_INDEX_MAX > 0)
    int lcmIndex = 0;
    int gammaIndex = 0;
#endif

#if GAMMA_LCM_MAX > 1
    lcmIndex = _getLcmIndexOfGamma();
#endif

#if GAMMA_INDEX_MAX > 1
    // get gamma index from runtime property configuration
    char property[PROPERTY_VALUE_MAX];

    gammaIndex = GAMMA_INDEX_DEFAULT;
    if (picMode == PQ_PIC_MODE_USER_DEF &&
            property_get(GAMMA_INDEX_PROPERTY_NAME, property, NULL) > 0 &&
            strlen(property) > 0)
    {
        gammaIndex = atoi(property);
    }

    if (gammaIndex < 0 || GAMMA_INDEX_MAX <= gammaIndex)
        gammaIndex = GAMMA_INDEX_DEFAULT;

    PQ_LOGD("Gamma index: %d/%d", gammaIndex, GAMMA_INDEX_MAX);
#endif

#if (GAMMA_LCM_MAX > 0) && (GAMMA_INDEX_MAX > 0)
    DISP_GAMMA_LUT_T *driverGamma = new DISP_GAMMA_LUT_T;
    unsigned short R_entry, G_entry, B_entry;
#ifdef GAMMA_LIGHT
    unsigned short R_ref, G_ref, B_ref;
#endif
    driverGamma->hw_id = DISP_GAMMA0;
#ifdef GAMMA_LIGHT
    for (int i = 0; i < DISP_GAMMA_LUT_SIZE; i+=2) {
        R_entry = (*entry)[0][i];
        G_entry = (*entry)[1][i];
        B_entry = (*entry)[2][i];
        driverGamma->lut[i] = GAMMA_ENTRY(R_entry, G_entry, B_entry);
    }
    for (int i = 1; i < DISP_GAMMA_LUT_SIZE; i+=2) {
        R_entry = (*entry)[0][i];
        G_entry = (*entry)[1][i];
        B_entry = (*entry)[2][i];

        R_ref = (*entry)[0][i-1];
        G_ref = (*entry)[1][i-1];
        B_ref = (*entry)[2][i-1];
        driverGamma->lut[i] = GAMMA_ENTRY(abs(R_entry - R_ref),
                                          abs(G_entry - G_ref),
                                          abs(B_entry - B_ref));
    }
#else
    for (int i = 0; i < DISP_GAMMA_LUT_SIZE; i++) {
        R_entry = (*entry)[0][i];
        G_entry = (*entry)[1][i];
        B_entry = (*entry)[2][i];
        driverGamma->lut[i] = GAMMA_ENTRY(R_entry, G_entry, B_entry);
    }
#endif

    ioctl(m_drvID, DISP_IOCTL_SET_GAMMALUT, driverGamma);

    delete driverGamma;
#endif
}

bool PictureQuality::loadPqparamTable()
{
    CustParameters &cust = CustParameters::getPQCust();
    DISP_PQ_PARAM *pq_param_ptr;

    /* find the address of function and data objects */
    pq_param_ptr = (DISP_PQ_PARAM *)cust.getSymbol("pqparam_table");
    if (!pq_param_ptr) {
        PQ_LOGD("[PQ_SERVICE] pqparam_table is not found in libpq_cust.so\n");
        return false;
    }
    else
    {
        memcpy(&m_pqparam_table[0], pq_param_ptr, sizeof(DISP_PQ_PARAM)*PQ_PARAM_TABLE_SIZE);
        return true;
    }
}

bool PictureQuality::loadPqparamMappingCoefficient()
{
    CustParameters &cust = CustParameters::getPQCust();
    DISP_PQ_MAPPING_PARAM *pq_mapping_ptr;

    pq_mapping_ptr = (DISP_PQ_MAPPING_PARAM *)cust.getSymbol("pqparam_mapping");
    if (!pq_mapping_ptr) {
        PQ_LOGD("[PQ_SERVICE] pqparam_mapping is not found in libpq_cust.so\n");
        return false;
    }
    else
    {
        memcpy(&m_pqparam_mapping, pq_mapping_ptr, sizeof(DISP_PQ_MAPPING_PARAM));
        return true;
    }
}

bool PictureQuality::loadPqindexTable()
{
    CustParameters &cust = CustParameters::getPQCust();
    DISPLAY_PQ_T  *pq_table_ptr;

#ifdef MDP_COLOR_ENABLE
    pq_table_ptr = (DISPLAY_PQ_T *)cust.getSymbol("secondary_pqindex");
#else
    pq_table_ptr = (DISPLAY_PQ_T *)cust.getSymbol("primary_pqindex");
#endif

    if (!pq_table_ptr) {
        PQ_LOGD("[PQ_SERVICE] pqindex is not found in libpq_cust.so\n");
        return false;
    }
    else
    {
        memcpy(&m_pqindex, pq_table_ptr, sizeof(DISPLAY_PQ_T));
        return true;
    }
}

bool PictureQuality::loadRSZTable()
{
    CPQRszFW *pRszFW = new CPQRszFW;
    RszInitParam initParam;
    pRszFW->onInitPlatform(initParam, NULL);
    int32_t offset = 0;
    int32_t size = 0;
    int32_t isNoError = 0;
    /* save register value from cust file to ashmem */
    for (int index = 0; index < PROXY_RSZ_CUST_MAX; index++)
    {
        offset += size;
        if (index == PROXY_RSZ_CUST_SWREG)
        {
            size = ARR_LEN_4BYTE(pRszFW->m_rszReg);
            isNoError = m_AshmemProxy->setTuningArray(PROXY_RSZ_CUST, offset, &pRszFW->m_rszReg, size);

        }
        else if (index == PROXY_RSZ_CUST_HWREG)
        {
            size = ARR_LEN_4BYTE(pRszFW->m_rszRegHW);
            isNoError = m_AshmemProxy->setTuningArray(PROXY_RSZ_CUST, offset, &pRszFW->m_rszRegHW, size);
        }
        else if (index == PROXY_RSZ_CUST_CZSWREG)
        {
            size = ARR_LEN_4BYTE(pRszFW->RszEntrySWReg);
            isNoError = m_AshmemProxy->setTuningArray(PROXY_RSZ_CUST, offset, &pRszFW->RszEntrySWReg, size);
        }
        else if (index == PROXY_RSZ_CUST_CZHWREG)
        {
            size = ARR_LEN_4BYTE(pRszFW->RszEntryHWReg);
            isNoError = m_AshmemProxy->setTuningArray(PROXY_RSZ_CUST, offset, &pRszFW->RszEntryHWReg, size);
        }
        else if (index == PROXY_RSZ_CUST_CZLEVEL)
        {
            size = ARR_LEN_4BYTE(pRszFW->RszLevel);
            isNoError = m_AshmemProxy->setTuningArray(PROXY_RSZ_CUST, offset, &pRszFW->RszLevel, size);
        }

        if (isNoError < 0)
        {
            break;
        }
    }

    PQ_LOGD("[PQ_SERVICE] loadRSZTable:%d\n", isNoError);
    delete pRszFW;
    return (isNoError < 0) ? false : true;
}

bool PictureQuality::loadDSTable()
{
    CPQDSFW *pDSFW = new CPQDSFW;
    pDSFW->onInitPlatform();
    int32_t offset = 0;
    int32_t size = 0;
    int32_t isNoError = 0;
    /* save register value from cust file to ashmem */
    for (int index = 0; index < PROXY_DS_CUST_MAX; index++)
    {
        offset += size;
        if (index == PROXY_DS_CUST_REG)
        {
            size = ARR_LEN_4BYTE(DSReg);
            isNoError = m_AshmemProxy->setTuningArray(PROXY_DS_CUST, offset, pDSFW->pDSReg, size);
        }
        else if (index == PROXY_DS_CUST_CZSWREG)
        {
            size = ARR_LEN_4BYTE(pDSFW->iDSRegEntry);
            isNoError = m_AshmemProxy->setTuningArray(PROXY_DS_CUST, offset, &pDSFW->iDSRegEntry, size);
        }
        else if (index == PROXY_DS_CUST_CZHWREG)
        {
            size = ARR_LEN_4BYTE(pDSFW->iDSHWRegEntry);
            isNoError = m_AshmemProxy->setTuningArray(PROXY_DS_CUST, offset, &pDSFW->iDSHWRegEntry, size);
        }
        else if (index == PROXY_DS_CUST_CZLEVEL)
        {
            size = ARR_LEN_4BYTE(pDSFW->iTdshpLevel);
            isNoError = m_AshmemProxy->setTuningArray(PROXY_DS_CUST, offset, &pDSFW->iTdshpLevel, size);
        }

        if (isNoError < 0)
        {
            break;
        }
    }
    PQ_LOGD("[PQ_SERVICE] loadDSTable:%d\n", isNoError);
    delete pDSFW;
    return (isNoError < 0) ? false : true;
}

bool PictureQuality::loadDCTable()
{
    CPQDCFW *pADLFW = new CPQDCFW;
    ADLInitParam initParam;
    ADLInitReg initReg;
    pADLFW->onInitPlatform(initParam, &initReg);
    int32_t offset = 0;
    int32_t size = 0;
    int32_t isNoError = 0;
    /* save register value from cust file to ashmem */
    for (int index = 0; index < PROXY_DC_CUST_MAX; index++)
    {
        offset += size;
        if (index == PROXY_DC_CUST_ADLREG)
        {
            size = ARR_LEN_4BYTE(ADLReg);
            isNoError = m_AshmemProxy->setTuningArray(PROXY_DC_CUST, offset, pADLFW->pADLReg, size);
        }
        else if (index == PROXY_DC_CUST_HDRREG)
        {
            size = ARR_LEN_4BYTE(ADLReg);
            isNoError = m_AshmemProxy->setTuningArray(PROXY_DC_CUST, offset, pADLFW->pHDRModeReg, size);
        }

        if (isNoError < 0)
        {
            break;
        }
    }
    PQ_LOGD("[PQ_SERVICE] loadDCTable:%d\n", isNoError);
    delete pADLFW;
    return (isNoError < 0) ? false : true;
}

bool PictureQuality::loadHDRTable()
{
#ifdef SUPPORT_HDR
    CPQHDRFW *pHDRFW = new CPQHDRFW;
    pHDRFW->onInitPlatform();
    int32_t offset = 0;
    int32_t size = 0;
    int32_t isNoError = 0;
    /* save register value from cust file to ashmem */
    offset += size;
    size = ARR_LEN_4BYTE(HDRFWReg);
    isNoError = m_AshmemProxy->setTuningArray(PROXY_HDR_CUST, offset, pHDRFW->pHDRFWReg, size);
    PQ_LOGD("[PQ_SERVICE] loadHDRTable:%d\n", isNoError);
    delete pHDRFW;
    return (isNoError < 0) ? false : true;
#else
    PQ_LOGD("[PQ_SERVICE] loadHDRTable: not support HDR\n");
    return false;
#endif
}

bool PictureQuality::loadTDSHPTable()
{
    int32_t offset = 0;
    int32_t size = 0;
    int32_t isNoError = 0;
    DISPLAY_TDSHP_T  *tdshp_table_ptr;
    CustParameters &cust = CustParameters::getPQCust();

    if (!cust.isGood()) {
        PQ_LOGE("[PQ_SERVICE] can't open libpq_cust.so\n");
        return false;
    }
    /* find the address of function and data objects */
    tdshp_table_ptr = (DISPLAY_TDSHP_T *)cust.getSymbol("tdshpindex");
    if (!tdshp_table_ptr) {
        PQ_LOGE("[PQ_SERVICE] tdshpindex is not found in libpq_cust.so\n");
        return false;
    }
    PQ_LOGD("[PQ_SERVICE] load tdshpindex...\n");

    /* save register value from cust file to ashmem */
    size = sizeof(DISPLAY_TDSHP_T) / sizeof(unsigned int);
    isNoError = m_AshmemProxy->setTuningArray(PROXY_TDSHP_CUST, offset, tdshp_table_ptr, size);
    PQ_LOGD("[PQ_SERVICE] loadTDSHPTable:%d\n", isNoError);

    return (isNoError < 0) ? false : true;
}

bool PictureQuality::loadCOLORTable()
{
    int32_t offset = 0;
    int32_t size = 0;
    int32_t isNoError = 0;
    DISPLAY_PQ_T *mdp_color_ptr;
    CustParameters &cust = CustParameters::getPQCust();

    if (!cust.isGood()) {
        PQ_LOGE("[PQ_SERVICE] can't open libpq_cust.so\n");
        return false;
    }
    /* find the address of function and data objects */
    mdp_color_ptr = (DISPLAY_PQ_T *)cust.getSymbol("primary_pqindex");
    if (!mdp_color_ptr) {
        PQ_LOGE("[PQ_SERVICE] primary_pqindex is not found in libpq_cust.so\n");
        return false;
    }
    PQ_LOGD("[PQ_SERVICE] load primary_pqindex...\n");

    /* save register value from cust file to ashmem */
    size = sizeof(DISPLAY_PQ_T) / sizeof(unsigned int);
    isNoError = m_AshmemProxy->setTuningArray(PROXY_COLOR_CUST, offset, mdp_color_ptr, size);
    PQ_LOGD("[PQ_SERVICE] loadCOLORTable:%d\n", isNoError);

    return (isNoError < 0) ? false : true;
}

void PictureQuality::loadGammaEntryTable()
{
    // load gamma from cust lib if load factory gamma fail or factory gamma not support
    CustParameters &cust = CustParameters::getPQCust();
    gamma_entry_t* ptr = (gamma_entry_t*)cust.getSymbol("cust_gamma");
    gamma_entry_t *entry = NULL;

    if (!ptr) {
        PQ_LOGD("[PQ_SERVICE] cust_gamma is not found in libpq_cust.so\n");
    }
    else
    {
        memcpy(m_CustGamma, ptr, sizeof(gamma_entry_t) * GAMMA_LCM_MAX * GAMMA_INDEX_MAX);
        memcpy(m_NvGamma, ptr, sizeof(gamma_entry_t) * GAMMA_LCM_MAX * GAMMA_INDEX_MAX);
    }
#ifdef FACTORY_GAMMA_SUPPORT
    if (loadNvGammaTable() == NO_ERROR) {
        gamma_entry_t *entry = &(m_NvGamma[0][GAMMA_INDEX_DEFAULT]);
        configGamma(m_PQMode, entry);
        return;
    }
#endif
    if (ptr) {
        gamma_entry_t *entry = &(m_CustGamma[0][GAMMA_INDEX_DEFAULT]);
        configGamma(m_PQMode, entry);
    }
}

#define MAX_RETRY_COUNT 10000
status_t PictureQuality::loadMetaNvGammaTable(gamma_entry_t *entry)
{
#ifdef FACTORY_GAMMA_SUPPORT
    F_ID gamma_nvram_fd = {0};
    int rec_size = 0;
    int rec_num = 0;
    FILE* fCfg = NULL;
    unsigned int r, g, b, rgbCombine, checksum;
    PQ_CUSTOM_LUT gamma_nvram;
    int read_nvram_ready_retry = 0;
    bool read_nvram_ready = false;
    char nvram_init_val[PROPERTY_VALUE_MAX];
    char property[PROPERTY_VALUE_MAX];
    int factoryGammaDebugFlag = 1;

    if (property_get("persist.sys.gamma.debug", property, NULL) > 0 &&
        strlen(property) > 0) {
        factoryGammaDebugFlag = atoi(property);

        if (factoryGammaDebugFlag == 1) {
            PQ_LOGD("[GAMMA] factory gamma flag=%d: apply factory gamma", factoryGammaDebugFlag);
        } else {
            PQ_LOGD("[GAMMA] bypass factory gamma");
            return UNKNOWN_ERROR;
        }
    } else {
        PQ_LOGD("[GAMMA] factory gamma enable");
    }

    while (read_nvram_ready_retry < MAX_RETRY_COUNT) {
        read_nvram_ready_retry++;
        property_get("service.nvram_init", nvram_init_val, NULL);

        if (strcmp(nvram_init_val, "Ready") == 0) {
            read_nvram_ready = true;
            break;
        } else {
            PQ_LOGD("[GAMMA]wait nvram initial (%4d)\n", read_nvram_ready_retry);
            usleep(500*1000);
        }
    }

    PQ_LOGD("[GAMMA]Get nvram restore ready retry cc=%d flag=%d\n",
        read_nvram_ready_retry, read_nvram_ready);

    for (int i = 0; i < 20; i++) {
        gamma_nvram_fd = NVM_GetFileDesc(AP_CFG_RDCL_FILE_PQ_LID, &rec_size, &rec_num, ISWRITE);
        PQ_LOGD("[GAMMA]FD %d rec_size %d rec_num %d\n", gamma_nvram_fd.iFileDesc, rec_size, rec_num);

        if (gamma_nvram_fd.iFileDesc == -1) {
            NVM_CloseFileDesc(gamma_nvram_fd);

            PQ_LOGD("[GAMMA]try get file (%4d)\n", i);
            usleep(1000);
        } else {
            break;
        }
    }

    if (rec_num != 1) {
        PQ_LOGE("[GAMMA]Unexpected record num %d", rec_num);
        NVM_CloseFileDesc(gamma_nvram_fd);
        return UNKNOWN_ERROR;
    }

    if (rec_size != sizeof(PQ_CUSTOM_LUT)) {
        PQ_LOGE("[GAMMA]Unexpected record size %d ap_nvram_btradio_struct %d",
            rec_size, sizeof(PQ_CUSTOM_LUT));
        NVM_CloseFileDesc(gamma_nvram_fd);
        return UNKNOWN_ERROR;
    }

    if (read(gamma_nvram_fd.iFileDesc, &gamma_nvram, rec_num*rec_size) < 0){
        PQ_LOGE("[GAMMA]Read NVRAM fails");
        NVM_CloseFileDesc(gamma_nvram_fd);
        return UNKNOWN_ERROR;
    } else {
        for (int i = 0; i < 257; i++) {
            PQ_LOGD("[GAMMA] lutidx=%3d NVGamma value=0x%08x\n", i, gamma_nvram.gamma_lut[i]);
        }
        PQ_LOGD("[GAMMA] checksum(Nv)=0x%08x", gamma_nvram.gamma_checksum);
    }

    checksum = 0;
    for (int i = 0; i < 257; i++) {
        rgbCombine = gamma_nvram.gamma_lut[i];
        r = (rgbCombine >> 20) & 0x3ff;
        g = (rgbCombine >> 10) & 0x3ff;
        b = rgbCombine & 0x3ff;

        checksum += r;
        checksum &= 0xffff;
        checksum += g;
        checksum &= 0xffff;
        checksum += b;
        checksum &= 0xffff;

        if (i < 256) {
            (*entry)[0][i*2] = r;
            (*entry)[1][i*2] = g;
            (*entry)[2][i*2] = b;

            if (i > 0) {// interpolation
                for (int colorIdx = 0; colorIdx < 3;  colorIdx += 1)
                    (*entry)[colorIdx][i*2-1] = ((*entry)[colorIdx][i*2] + (*entry)[colorIdx][i*2-2]) / 2;
            }
        } else if (i == 256) {
            (*entry)[0][DISP_GAMMA_LUT_SIZE-1] = r;
            (*entry)[1][DISP_GAMMA_LUT_SIZE-1] = g;
            (*entry)[2][DISP_GAMMA_LUT_SIZE-1] = b;
        }
        PQ_LOGD("[GAMMA] lutidx=%3d NVGamma256 RGB=%8d,%8d,%8d", i, r, g, b);
    }
    PQ_LOGD("[GAMMA] checksum(Tb)=0x%08x", checksum);

    // compare checksum
    if ((checksum != gamma_nvram.gamma_checksum) || (checksum == 0)) {
        PQ_LOGE("[GAMMA] checksum(Tb)(0x%08x) != checksum(Nv)(0x%08x)",
            checksum, gamma_nvram.gamma_checksum);
        return UNKNOWN_ERROR;
    }
#endif
    UNUSED(entry);
    return NO_ERROR;
}

status_t PictureQuality::loadNormalNvGammaTable(gamma_entry_t *entry)
{
    UNUSED(entry);
    return NO_ERROR;
}

status_t PictureQuality::loadNvGammaTable(void)
{
    // get R, G, B from Gamma Entry
    gamma_entry_t *entry = &(m_NvGamma[0][GAMMA_INDEX_DEFAULT]);

    if (loadMetaNvGammaTable(entry) == NO_ERROR)
        m_NvGammaStatus = true;
    else
        return UNKNOWN_ERROR;

    for (int i = 0; i < DISP_GAMMA_LUT_SIZE; i+=16) {
        PQ_LOGD("[GAMMA] lutidx=%3d NVGamma512 RGB=%8d,%8d,%8d",
            i, (*entry)[0][i], (*entry)[1][i], (*entry)[2][i]);
    }

    int last_idx = DISP_GAMMA_LUT_SIZE - 1;
    PQ_LOGD("[GAMMA] lutidx=%3d NVGamma512 RGB=%8d,%8d,%8d",
            last_idx, (*entry)[0][last_idx], (*entry)[1][last_idx], (*entry)[2][last_idx]);

    return NO_ERROR;
}

bool PictureQuality::loadCDparamTable()
{
#ifdef CHAMELEON_DISPLAY_SUPPORT
    CustParameters &cust = CustParameters::getPQCust();
    int *cd_ff_param_ptr;
    int *cd_debounce_param_ptr;
    int *cd_cf_param_ptr;
    int *cd_panel_param_ptr;
    bool returnValue = false;

    /* find the address of function and data objects */
    cd_ff_param_ptr = (int *)cust.getSymbol("chameleon_display_ff");
    returnValue |= chameleonDisplayProcess->AssignChameleonDisplayRegSensorFactoryFactor(cd_ff_param_ptr);

    cd_cf_param_ptr = (int *)cust.getSymbol("chameleon_display_cf");
    returnValue |= chameleonDisplayProcess->AssignChameleonDisplayRegSensorCalibrationFactor(cd_cf_param_ptr);

    cd_debounce_param_ptr = (int *)cust.getSymbol("chameleon_display_debounce");
    returnValue |= chameleonDisplayProcess->AssignChameleonDisplayRegSensorDebounce(cd_debounce_param_ptr);

    cd_panel_param_ptr = (int *)cust.getSymbol("chameleon_display_panel");
    returnValue |= chameleonDisplayProcess->AssignChameleonDisplayRegPanel(cd_panel_param_ptr);

    return returnValue;
#else
    return false;
#endif
}

bool PictureQuality::loadTRSparamTable()
{
#ifdef TRANSITION_SUPPORT
    CustParameters &cust = CustParameters::getPQCust();
    int *pqtrs_bright2dark_param_ptr;
    int *pqtrs_dark2bright_param_ptr;
    bool returnValue = false;

    /* find the address of function and data objects */
    pqtrs_bright2dark_param_ptr = (int *)cust.getSymbol("pqtransition_bright2dark");
    returnValue |= mPQTransition->AssignPQTransitionRegBrightToDarkTransitionSettings(pqtrs_bright2dark_param_ptr);

    pqtrs_dark2bright_param_ptr = (int *)cust.getSymbol("pqtransition_dark2bright");
    returnValue |= mPQTransition->AssignPQTransitionRegDarkToBrightTransitionSettings(pqtrs_dark2bright_param_ptr);

    return returnValue;
#else
    return false;
#endif
}

void PictureQuality::setDebuggingPqparam(PQDebugFlag flag, uint32_t mode, uint32_t scenario, uint32_t value)
{
    switch(flag){
        case PQDEBUG_SHP_VALUE:
            if(scenario < PQ_SCENARIO_COUNT && mode < PQ_PREDEFINED_MODE_COUNT){
                m_pqparam_table[mode * PQ_SCENARIO_COUNT + scenario].u4SHPGain = value;
            }
            else if(mode == PQ_PIC_MODE_USER_DEF){
                m_pqparam_table[PQ_PREDEFINED_MODE_COUNT * PQ_SCENARIO_COUNT].u4SHPGain = value;
            }
            break;
        case PQDEBUG_ON:
            m_pqparam_table[PQ_PIC_MODE_STANDARD + PQPictureMode::getScenarioIndex(SCENARIO_PICTURE)].u4SHPGain = 2;
            m_pqparam_table[PQ_PIC_MODE_STANDARD + PQPictureMode::getScenarioIndex(SCENARIO_PICTURE)].u4Brightness = 4;
            m_pqparam_table[PQ_PIC_MODE_STANDARD + PQPictureMode::getScenarioIndex(SCENARIO_VIDEO)].u4SHPGain = 1;
            m_pqparam_table[PQ_PIC_MODE_STANDARD + PQPictureMode::getScenarioIndex(SCENARIO_VIDEO)].u4Brightness = 6;
            m_pqparam_table[PQ_PIC_MODE_STANDARD + PQPictureMode::getScenarioIndex(SCENARIO_ISP_PREVIEW)].u4SHPGain = 0;
            m_pqparam_table[PQ_PIC_MODE_STANDARD + PQPictureMode::getScenarioIndex(SCENARIO_ISP_PREVIEW)].u4Brightness= 8;
            break;
        case PQDEBUG_OFF:
            loadPqparamTable();
            break;
        default:
            PQ_LOGE("[PQ_SERVICE] setDebuggingPqparam(), PQDebugFlag[%d] is not defined!!", flag);
            break;
    }
}

void PictureQuality::initBasicGamma(void)
{
#ifdef BASIC_PACKAGE
    const gamma_entry_t basic_gamma =
    {
        {    0,    2,    4,    6,    8,   10,   12,   14,   16,   18,   20,   22,   24,   26,   28,   30,   32,   34,   36,   38,   40,   42,   44,   46,   48,   50,   52,   54,   56,   58,   60,   62,
            64,   66,   68,   70,   72,   74,   76,   78,   80,   82,   84,   86,   88,   90,   92,   94,   96,   98,  100,  102,  104,  106,  108,  110,  112,  114,  116,  118,  120,  122,  124,  126,
           128,  130,  132,  134,  136,  138,  140,  142,  144,  146,  148,  150,  152,  154,  156,  158,  160,  162,  164,  166,  168,  170,  172,  174,  176,  178,  180,  182,  184,  186,  188,  190,
           192,  194,  196,  198,  200,  202,  204,  206,  208,  210,  212,  214,  216,  218,  220,  222,  224,  226,  228,  230,  232,  234,  236,  238,  240,  242,  244,  246,  248,  250,  252,  254,
           256,  258,  260,  262,  264,  266,  268,  270,  272,  274,  276,  278,  280,  282,  284,  286,  288,  290,  292,  294,  296,  298,  300,  302,  304,  306,  308,  310,  312,  314,  316,  318,
           320,  322,  324,  326,  328,  330,  332,  334,  336,  338,  340,  342,  344,  346,  348,  350,  352,  354,  356,  358,  360,  362,  364,  366,  368,  370,  372,  374,  376,  378,  380,  382,
           384,  386,  388,  390,  392,  394,  396,  398,  400,  402,  404,  406,  408,  410,  412,  414,  416,  418,  420,  422,  424,  426,  428,  430,  432,  434,  436,  438,  440,  442,  444,  446,
           448,  450,  452,  454,  456,  458,  460,  462,  464,  466,  468,  470,  472,  474,  476,  478,  480,  482,  484,  486,  488,  490,  492,  494,  496,  498,  500,  502,  504,  506,  508,  510,
           512,  514,  516,  518,  520,  522,  524,  526,  528,  530,  532,  534,  536,  538,  540,  542,  544,  546,  548,  550,  552,  554,  556,  558,  560,  562,  564,  566,  568,  570,  572,  574,
           576,  578,  580,  582,  584,  586,  588,  590,  592,  594,  596,  598,  600,  602,  604,  606,  608,  610,  612,  614,  616,  618,  620,  622,  624,  626,  628,  630,  632,  634,  636,  638,
           640,  642,  644,  646,  648,  650,  652,  654,  656,  658,  660,  662,  664,  666,  668,  670,  672,  674,  676,  678,  680,  682,  684,  686,  688,  690,  692,  694,  696,  698,  700,  702,
           704,  706,  708,  710,  712,  714,  716,  718,  720,  722,  724,  726,  728,  730,  732,  734,  736,  738,  740,  742,  744,  746,  748,  750,  752,  754,  756,  758,  760,  762,  764,  766,
           768,  770,  772,  774,  776,  778,  780,  782,  784,  786,  788,  790,  792,  794,  796,  798,  800,  802,  804,  806,  808,  810,  812,  814,  816,  818,  820,  822,  824,  826,  828,  830,
           832,  834,  836,  838,  840,  842,  844,  846,  848,  850,  852,  854,  856,  858,  860,  862,  864,  866,  868,  870,  872,  874,  876,  878,  880,  882,  884,  886,  888,  890,  892,  894,
           896,  898,  900,  902,  904,  906,  908,  910,  912,  914,  916,  918,  920,  922,  924,  926,  928,  930,  932,  934,  936,  938,  940,  942,  944,  946,  948,  950,  952,  954,  956,  958,
           960,  962,  964,  966,  968,  970,  972,  974,  976,  978,  980,  982,  984,  986,  988,  990,  992,  994,  996,  998, 1000, 1002, 1004, 1006, 1008, 1010, 1012, 1014, 1016, 1018, 1020, 1022 },
        {    0,    2,    4,    6,    8,   10,   12,   14,   16,   18,   20,   22,   24,   26,   28,   30,   32,   34,   36,   38,   40,   42,   44,   46,   48,   50,   52,   54,   56,   58,   60,   62,
            64,   66,   68,   70,   72,   74,   76,   78,   80,   82,   84,   86,   88,   90,   92,   94,   96,   98,  100,  102,  104,  106,  108,  110,  112,  114,  116,  118,  120,  122,  124,  126,
           128,  130,  132,  134,  136,  138,  140,  142,  144,  146,  148,  150,  152,  154,  156,  158,  160,  162,  164,  166,  168,  170,  172,  174,  176,  178,  180,  182,  184,  186,  188,  190,
           192,  194,  196,  198,  200,  202,  204,  206,  208,  210,  212,  214,  216,  218,  220,  222,  224,  226,  228,  230,  232,  234,  236,  238,  240,  242,  244,  246,  248,  250,  252,  254,
           256,  258,  260,  262,  264,  266,  268,  270,  272,  274,  276,  278,  280,  282,  284,  286,  288,  290,  292,  294,  296,  298,  300,  302,  304,  306,  308,  310,  312,  314,  316,  318,
           320,  322,  324,  326,  328,  330,  332,  334,  336,  338,  340,  342,  344,  346,  348,  350,  352,  354,  356,  358,  360,  362,  364,  366,  368,  370,  372,  374,  376,  378,  380,  382,
           384,  386,  388,  390,  392,  394,  396,  398,  400,  402,  404,  406,  408,  410,  412,  414,  416,  418,  420,  422,  424,  426,  428,  430,  432,  434,  436,  438,  440,  442,  444,  446,
           448,  450,  452,  454,  456,  458,  460,  462,  464,  466,  468,  470,  472,  474,  476,  478,  480,  482,  484,  486,  488,  490,  492,  494,  496,  498,  500,  502,  504,  506,  508,  510,
           512,  514,  516,  518,  520,  522,  524,  526,  528,  530,  532,  534,  536,  538,  540,  542,  544,  546,  548,  550,  552,  554,  556,  558,  560,  562,  564,  566,  568,  570,  572,  574,
           576,  578,  580,  582,  584,  586,  588,  590,  592,  594,  596,  598,  600,  602,  604,  606,  608,  610,  612,  614,  616,  618,  620,  622,  624,  626,  628,  630,  632,  634,  636,  638,
           640,  642,  644,  646,  648,  650,  652,  654,  656,  658,  660,  662,  664,  666,  668,  670,  672,  674,  676,  678,  680,  682,  684,  686,  688,  690,  692,  694,  696,  698,  700,  702,
           704,  706,  708,  710,  712,  714,  716,  718,  720,  722,  724,  726,  728,  730,  732,  734,  736,  738,  740,  742,  744,  746,  748,  750,  752,  754,  756,  758,  760,  762,  764,  766,
           768,  770,  772,  774,  776,  778,  780,  782,  784,  786,  788,  790,  792,  794,  796,  798,  800,  802,  804,  806,  808,  810,  812,  814,  816,  818,  820,  822,  824,  826,  828,  830,
           832,  834,  836,  838,  840,  842,  844,  846,  848,  850,  852,  854,  856,  858,  860,  862,  864,  866,  868,  870,  872,  874,  876,  878,  880,  882,  884,  886,  888,  890,  892,  894,
           896,  898,  900,  902,  904,  906,  908,  910,  912,  914,  916,  918,  920,  922,  924,  926,  928,  930,  932,  934,  936,  938,  940,  942,  944,  946,  948,  950,  952,  954,  956,  958,
           960,  962,  964,  966,  968,  970,  972,  974,  976,  978,  980,  982,  984,  986,  988,  990,  992,  994,  996,  998, 1000, 1002, 1004, 1006, 1008, 1010, 1012, 1014, 1016, 1018, 1020, 1022 },
        {    0,    2,    4,    6,    8,   10,   12,   14,   16,   18,   20,   22,   24,   26,   28,   30,   32,   34,   36,   38,   40,   42,   44,   46,   48,   50,   52,   54,   56,   58,   60,   62,
            64,   66,   68,   70,   72,   74,   76,   78,   80,   82,   84,   86,   88,   90,   92,   94,   96,   98,  100,  102,  104,  106,  108,  110,  112,  114,  116,  118,  120,  122,  124,  126,
           128,  130,  132,  134,  136,  138,  140,  142,  144,  146,  148,  150,  152,  154,  156,  158,  160,  162,  164,  166,  168,  170,  172,  174,  176,  178,  180,  182,  184,  186,  188,  190,
           192,  194,  196,  198,  200,  202,  204,  206,  208,  210,  212,  214,  216,  218,  220,  222,  224,  226,  228,  230,  232,  234,  236,  238,  240,  242,  244,  246,  248,  250,  252,  254,
           256,  258,  260,  262,  264,  266,  268,  270,  272,  274,  276,  278,  280,  282,  284,  286,  288,  290,  292,  294,  296,  298,  300,  302,  304,  306,  308,  310,  312,  314,  316,  318,
           320,  322,  324,  326,  328,  330,  332,  334,  336,  338,  340,  342,  344,  346,  348,  350,  352,  354,  356,  358,  360,  362,  364,  366,  368,  370,  372,  374,  376,  378,  380,  382,
           384,  386,  388,  390,  392,  394,  396,  398,  400,  402,  404,  406,  408,  410,  412,  414,  416,  418,  420,  422,  424,  426,  428,  430,  432,  434,  436,  438,  440,  442,  444,  446,
           448,  450,  452,  454,  456,  458,  460,  462,  464,  466,  468,  470,  472,  474,  476,  478,  480,  482,  484,  486,  488,  490,  492,  494,  496,  498,  500,  502,  504,  506,  508,  510,
           512,  514,  516,  518,  520,  522,  524,  526,  528,  530,  532,  534,  536,  538,  540,  542,  544,  546,  548,  550,  552,  554,  556,  558,  560,  562,  564,  566,  568,  570,  572,  574,
           576,  578,  580,  582,  584,  586,  588,  590,  592,  594,  596,  598,  600,  602,  604,  606,  608,  610,  612,  614,  616,  618,  620,  622,  624,  626,  628,  630,  632,  634,  636,  638,
           640,  642,  644,  646,  648,  650,  652,  654,  656,  658,  660,  662,  664,  666,  668,  670,  672,  674,  676,  678,  680,  682,  684,  686,  688,  690,  692,  694,  696,  698,  700,  702,
           704,  706,  708,  710,  712,  714,  716,  718,  720,  722,  724,  726,  728,  730,  732,  734,  736,  738,  740,  742,  744,  746,  748,  750,  752,  754,  756,  758,  760,  762,  764,  766,
           768,  770,  772,  774,  776,  778,  780,  782,  784,  786,  788,  790,  792,  794,  796,  798,  800,  802,  804,  806,  808,  810,  812,  814,  816,  818,  820,  822,  824,  826,  828,  830,
           832,  834,  836,  838,  840,  842,  844,  846,  848,  850,  852,  854,  856,  858,  860,  862,  864,  866,  868,  870,  872,  874,  876,  878,  880,  882,  884,  886,  888,  890,  892,  894,
           896,  898,  900,  902,  904,  906,  908,  910,  912,  914,  916,  918,  920,  922,  924,  926,  928,  930,  932,  934,  936,  938,  940,  942,  944,  946,  948,  950,  952,  954,  956,  958,
           960,  962,  964,  966,  968,  970,  972,  974,  976,  978,  980,  982,  984,  986,  988,  990,  992,  994,  996,  998, 1000, 1002, 1004, 1006, 1008, 1010, 1012, 1014, 1016, 1018, 1020, 1022 },
    };

    for (int i = 0; i < GAMMA_LCM_MAX; i++) {
        for (int j = 0; j < GAMMA_INDEX_MAX; j++) {
            memcpy(&m_CustGamma[i][j], &basic_gamma, sizeof(gamma_entry_t));
        }
    }

    configGamma(m_PQMode, &(m_CustGamma[0][GAMMA_INDEX_DEFAULT]));
#endif
}

int loadGlobalPQDCIndex(GLOBAL_PQ_INDEX_T *globalPQindex)
{
    CustParameters &cust = CustParameters::getPQCust();
    DISPLAY_DC_T *dcindex;

    /* find the address of function and data objects */
    dcindex = (DISPLAY_DC_T *)cust.getSymbol("dcindex");
    if (!dcindex) {
        PQ_LOGD("[PQ_SERVICE] dcindex is not found in libpq_cust.so\n");
        return false;
    }
    memcpy(&(globalPQindex->dcindex), dcindex, sizeof(DISPLAY_DC_T));

    return true;
}

void PictureQuality::initGlobalPQ(void)
{
    char value[PROPERTY_VALUE_MAX];

    property_get(GLOBAL_PQ_SUPPORT_STR, value, GLOBAL_PQ_SUPPORT_DEFAULT);
    m_GlobalPQSupport = atoi(value);

    if (m_GlobalPQSupport == 0)
    {
        PQ_LOGD("Global PQ Not Support!");
        return;
    }

    PQ_LOGD("init Global PQ!");

    loadGlobalPQDCIndex(&m_GlobalPQindex);

    property_get(GLOBAL_PQ_ENABLE_STR, value, GLOBAL_PQ_ENABLE_DEFAULT);
    m_GlobalPQSwitch = atoi(value);

    property_get(GLOBAL_PQ_STRENGTH_STR, value, GLOBAL_PQ_STRENGTH_DEFAULT);
    m_GlobalPQStrength = atoi(value);

    m_GlobalPQStrengthRange =
        (GLOBAL_PQ_VIDEO_SHARPNESS_STRENGTH_RANGE << 0 ) |
        (GLOBAL_PQ_VIDEO_DC_STRENGTH_RANGE << 8 ) |
        (GLOBAL_PQ_UI_SHARPNESS_STRENGTH_RANGE << 16 ) |
        (GLOBAL_PQ_UI_DC_STRENGTH_RANGE << 24 );

}

Return<Result> PictureQuality::setGlobalPQSwitch(int32_t globalPQSwitch)
{
    Result retval = Result::NOT_SUPPORTED;
    Mutex::Autolock _l(mLock);

    if (m_GlobalPQSupport == 0)
    {
        PQ_LOGD("Global PQ Not Support!");
        return retval;
    }

    PQ_LOGD("[PQ_SERVICE] PQService : setGlobalPQSwitch(%d)", globalPQSwitch);

    char value[PROPERTY_VALUE_MAX];
    snprintf(value, PROPERTY_VALUE_MAX, "%d", globalPQSwitch);
    property_set(GLOBAL_PQ_ENABLE_STR, value);

    m_GlobalPQSwitch = globalPQSwitch;

    retval = Result::OK;

    return retval;
}

Return<void> PictureQuality::getGlobalPQSwitch(getGlobalPQSwitch_cb _hidl_cb)
{
    Result retval = Result::NOT_SUPPORTED;
    int32_t globalPQSwitch = 0;

    Mutex::Autolock _l(mLock);

    globalPQSwitch = m_GlobalPQSwitch;
    PQ_LOGD("[PQ_SERVICE] PQService : getGlobalPQSwitch(%d)", globalPQSwitch);

    retval = Result::OK;

    _hidl_cb(retval, globalPQSwitch);
    return Void();
}

Return<Result> PictureQuality::setGlobalPQStrength(int32_t globalPQStrength)
{
    Result retval = Result::NOT_SUPPORTED;
    Mutex::Autolock _l(mLock);

    PQ_LOGD("[PQ_SERVICE] PQService : setGlobalPQStrength(%d)", globalPQStrength);

    char value[PROPERTY_VALUE_MAX];

    snprintf(value, PROPERTY_VALUE_MAX, "%d", globalPQStrength);
    property_set(GLOBAL_PQ_STRENGTH_STR, value);

    m_GlobalPQStrength = globalPQStrength;

    retval = Result::OK;

    return retval;
}

Return<void> PictureQuality::getGlobalPQStrength(getGlobalPQStrength_cb _hidl_cb)
{
    Result retval = Result::NOT_SUPPORTED;
    int32_t globalPQStrength = 0;

    Mutex::Autolock _l(mLock);

    globalPQStrength = m_GlobalPQStrength;
    PQ_LOGD("[PQ_SERVICE] PQService : getGlobalPQStrength(%d)", globalPQStrength);

    retval = Result::OK;

    _hidl_cb(retval, globalPQStrength);
    return Void();
}

Return<void> PictureQuality::getGlobalPQStrengthRange(getGlobalPQStrengthRange_cb _hidl_cb)
{
    Result retval = Result::NOT_SUPPORTED;
    int32_t globalPQStrengthRange = 0;

    Mutex::Autolock _l(mLock);

    globalPQStrengthRange = m_GlobalPQStrengthRange;
    PQ_LOGD("[PQ_SERVICE] PQService : getGlobalPQStrength(%d)", globalPQStrengthRange);

    retval = Result::OK;

    _hidl_cb(retval, globalPQStrengthRange);
    return Void();
}

Return<void> PictureQuality::getGlobalPQIndex(getGlobalPQIndex_cb _hidl_cb)
{
    globalPQIndex_t globalPQindex;

    Mutex::Autolock _l(mLock);

    memcpy(m_GlobalPQindex.dcindex.entry[GLOBAL_PQ_DC_INDEX_MAX - 1], &(g_PQ_DC_Param.param[0]), sizeof(g_PQ_DC_Param));
    memcpy(&globalPQindex, &m_GlobalPQindex, sizeof(m_GlobalPQindex));

    _hidl_cb(Result::OK, globalPQindex);
    return Void();
}

Return<Result> PictureQuality::setGlobalPQStableStatus(int32_t globalPQStableStatus)
{
    Result retval = Result::NOT_SUPPORTED;
    Mutex::Autolock _l(mLock);

    PQ_LOGD("[PQ_SERVICE] PQService : setGlobalPQStableStatus(%d)", globalPQStableStatus);
    m_GlobalPQStableStatus = globalPQStableStatus;
    retval = Result::OK;

    return retval;
}

Return<void> PictureQuality::getGlobalPQStableStatus(getGlobalPQSwitch_cb _hidl_cb)
{
    Result retval = Result::NOT_SUPPORTED;
    int32_t globalPQStableStatus = 0;
    Mutex::Autolock _l(mLock);

    globalPQStableStatus = m_GlobalPQStableStatus;
    PQ_LOGD("[PQ_SERVICE] PQService : getGlobalPQStableStatus(%d)", globalPQStableStatus);
    retval = Result::OK;
    _hidl_cb(retval, globalPQStableStatus);

    return Void();
}

// Methods from ::android::hidl::base::V1_0::IBase follow.

IPictureQuality* HIDL_FETCH_IPictureQuality(const char* /* name */) {
    return new PictureQuality();
}

}  // namespace implementation
}  // namespace V2_0
}  // namespace pq
}  // namespace hardware
}  // namespace mediatek
}  // namespace vendor
