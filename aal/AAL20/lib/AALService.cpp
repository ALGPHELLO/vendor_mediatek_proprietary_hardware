#define LOG_TAG "AALService"

#define MTK_LOG_ENABLE 1
#include <dlfcn.h>
#include <fcntl.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <cutils/log.h>
#include <cutils/properties.h>
#include <utils/SortedVector.h>
#include <binder/PermissionCache.h>
#include <cutils/memory.h>
#include <fstream>
#include <iostream>

#include <linux/disp_session.h>
#include <ddp_drv.h>
#include <ddp_aal.h>
#include <AAL20/AALService.h>

#include <AAL20/AALCust.h>
#include <AAL20/AALDef.h>
#include <AAL20/AALImplInterface.h>
#include "AALLightSensor.h"


namespace android {

#define DISP_DEV_NODE_PATH "/dev/mtk_disp_mgr"
#define LED_RESERVEBIT_SHIFT 16
#define LED_RESERVEBIT_PATTERN 1
#define AAL_PROPERTY_INIT_FUNCTION "persist.sys.aal.function"
#define AAL_PROPERTY_DEBUG "persist.aal.debug_level"
#define AAL_PROPERTY_UNIT_TEST "aal.ut"

#define AAL_DRE30_MODE_STR                  "ro.mtk_dre30_support"

#define LUMINANCE_MAX           1023
#define LOGITEMNUMBER           5
#define LOGITEMSIZE             128
#define LOGBUFFERSIZE           LOGITEMSIZE*LOGITEMNUMBER

#define AAL_LOGD(fmt, arg...) ALOGD(fmt, ##arg)
#define AAL_LOGE(fmt, arg...) ALOGE(fmt, ##arg)

enum DREMode {
    eDRE2 = 0,
    eDRE30,
    eDREALL,
};

#define AAL_MIN(a,b) (((a) < (b)) ? (a) : (b))
#define AAL_CLIP(val, min, max) ((val>=max) ? max : ((val<=min) ? min : val))

static Mutex msgLock;
static char mLogBuffer[LOGBUFFERSIZE];
static int mLogIndex;

#ifndef min
#define min( a, b )            (((a) < (b)) ? (a) : (b))
#endif

#define AAL_GET_ID_FROM_CMD(CMD) (CMD >> 16)
#define AAL_GET_FLAG_FROM_CMD(CMD) (CMD & 0xFFFF)
#define UNUSED(expr) do { (void)(expr); } while (0)


template <class T>
static inline void swap(T& a, T& b)
{
    T t = a;
    a = b;
    b = t;
}


static inline const char *getScrStateStr(int scrState)
{
    switch (scrState) {
    case eScrStateOff: return "Off";
    case eScrStateDoze: return "Doze";
    case eScrStateDim: return "Dim";
    case eScrStateOn: return "On";
    }

    return "UNKNOWN";
}


static char *printIntArray(char *ori_buffer, int bsize, int *arr,
    int arr_size, int arr_offset, int length)
{
    char *buffer = ori_buffer;
    int pr_len;
    int printUpperBound = arr_offset + length;

    if (printUpperBound > arr_size)
        printUpperBound = arr_size;

    for (int i = arr_offset; i < printUpperBound; i += 1) {
        pr_len = snprintf(buffer, bsize, " %4d", arr[i]);
        buffer += pr_len;
        bsize -= pr_len;
    }

    return ori_buffer;
}


static inline int valueOf(const char16_t ch)
{
    if (L'0' <= ch && ch <= L'9')
        return (ch - L'0');
    else if (L'a' <= ch && ch <= L'z')
        return (ch - L'a') + 10;
    else if (L'A' <= ch && ch <= L'Z')
        return (ch - L'A') + 10;

    return 0;
}


static int asInt(const String16& str)
{
    int val = 0;
    bool negative = false;
    int base = 10;

    const char16_t *char_p = str.string();

    if (*char_p == L'-') {
        negative = true;
        char_p++;
    } else if (*char_p == '+') {
        negative = false;
        char_p++;
    } else
        negative = false;

    if (*char_p == L'0' && *(char_p + 1) == L'x') {
        base = 16;
        char_p += 2;
    }

    for ( ; *char_p != L'\0'; char_p++) {
        val = val * base + valueOf(*char_p);
    }

    return (negative ? -val : val);
}


static bool getScreenResolution(int dev, int *screenWidth, int *screenHeight)
{
    bool result = false;
    int dre30_option_setting = 0;
    char property[PROPERTY_VALUE_MAX];

    if (dev <= 0) {
        return result;
    }

#ifdef HAS_DRE30
    if (property_get(AAL_DRE30_MODE_STR, property, NULL) > 0) {
        dre30_option_setting = (int)strtoul(property, NULL, 0);
    }

    if (dre30_option_setting == 1) {
        DISP_AAL_DISPLAY_SIZE display_size;
        int ret;

        ret = ioctl(dev, DISP_IOCTL_AAL_GET_SIZE, &display_size);
        if (ret >= 0) {
            *screenWidth = display_size.width;
            *screenHeight = display_size.height;
            result = true;
        }
    }
    else
#endif
    {
        disp_session_config config;
        int ret;

        // create session
        memset((void*)&config, 0, sizeof(disp_session_config));
        config.type = DISP_SESSION_PRIMARY;
        config.device_id = 0;
        ret = ioctl(dev, DISP_IOCTL_CREATE_SESSION, &config);
        if (ret < 0) {
            AAL_LOGE("display CREATE_SESSION failed, ret = %d", ret);
            return false;
        }

        disp_session_info session_info;
        session_info.session_id = config.session_id;
        ret = ioctl(dev, DISP_IOCTL_GET_SESSION_INFO, &session_info);
        if (ret >= 0) {
            *screenWidth = session_info.displayWidth;
            *screenHeight = session_info.displayHeight;
            result = true;
        } else
            AAL_LOGE("display GET_SESSION_INFO failed, ret = %d", ret);

        config.type = DISP_SESSION_PRIMARY;
        config.device_id = 0;
        ret = ioctl(dev, DISP_IOCTL_DESTROY_SESSION, &config);
    }

    return result;
}

static millisec_t getTimeOfMillisecond()
{
    struct timeval time;
    gettimeofday(&time, NULL);

    return static_cast<millisec_t>(time.tv_sec) * 1000L +
        static_cast<millisec_t>(time.tv_usec / 1000L);
}

static void attachTimeTag(char *tStrbuf)
{
    time_t rawtime;
    struct tm * timeinfo;
    struct timeval time;
    gettimeofday(&time, NULL);

    rawtime = time.tv_sec;
    timeinfo = localtime(&rawtime);

    strftime(tStrbuf,80,"%m-%d %I:%M:%S.",timeinfo);
    sprintf(tStrbuf+strlen(tStrbuf), "%03lu", (unsigned long)(time.tv_usec / 1000L));
}

static void printLogBuffer(bool need_lock)
{
    if (need_lock)
        Mutex::Autolock _l(msgLock);

    if (mLogIndex > 0) {
        AAL_LOGD("%s", mLogBuffer);
        mLogIndex = 0;
        strncpy(mLogBuffer, "", LOGBUFFERSIZE - 1);
    }
}

static void addLogToBuffer(char *message)
{
    Mutex::Autolock _l(msgLock);

    char itemStrBuffer[LOGITEMSIZE] = "";

    int lenTimeStr = 0;
    int lenMsgStr = 0;
    int lenLog = 0;

    attachTimeTag(itemStrBuffer);
    lenTimeStr = strlen(itemStrBuffer);

    snprintf(itemStrBuffer+lenTimeStr, LOGITEMSIZE-lenTimeStr, " %s",
            message);

    lenMsgStr = strlen(itemStrBuffer);
    lenLog = strlen(mLogBuffer);

    // protect for avoid out of log buffer size
    if ((lenMsgStr + lenLog + 32) > LOGBUFFERSIZE) {
        AAL_LOGE("log buffer almost full, skip attach latest message");

    // add message to log buffer
    } else {
        snprintf(mLogBuffer+lenLog, LOGITEMSIZE, "%s, ",
            itemStrBuffer);
        mLogIndex += 1;

        if (mLogIndex >= LOGITEMNUMBER) {// log buffer is full
            printLogBuffer(false);
        }
    }
}

// We allow customer inject actions during AAL main loop
// This is useful if customer needs wait for other events besides
// the histogram retrival. E.g. SurfaceFlinger.
class CustFunctions {
private:
    typedef int (*CustInitFunc)(AALService *, void **custDataP);
    typedef int (*CustFunc)(AALService *, void *custData);
    typedef status_t (*CustInvokeFunc)(AALService *, void *custData, int32_t cmd, int64_t arg);
    typedef int (*DimmingVirtual10ToPhysical12)(AALService *, void *custData, int virtualValue10);

    AALService *mService;
    CustInitFunc mCustInit;
    CustFunc mPreWait, mPostWait;
    CustFunc mCustDeinit;
    CustInvokeFunc mCustInvoke;
    DimmingVirtual10ToPhysical12 mVirtual10ToPhysical12;
    void *mCustData;

public:
    CustFunctions(AALService *service) :
            mService(service),
            mCustInit(NULL), mPreWait(NULL), mPostWait(NULL), mCustDeinit(NULL),
            mCustInvoke(NULL), mVirtual10ToPhysical12(NULL), mCustData(NULL)
    {
    }

    void link(CustParameters &cust) {
        mCustInit = cust.getFunction<CustInitFunc>("custInit");
        mPreWait = cust.getFunction<CustFunc>("preWait");
        mPostWait = cust.getFunction<CustFunc>("postWait");
        mCustDeinit = cust.getFunction<CustFunc>("custDeinit");
        mCustInvoke = cust.getFunction<CustInvokeFunc>("custInvoke");
        if (mCustInit != NULL || mPreWait != NULL || mPostWait != NULL || mCustDeinit != NULL) {
            AAL_LOGD("custInit = %lx, preWait = %lx, postWait = %lx, custDeinit = %lx",
                    (unsigned long)mCustInit, (unsigned long)mPreWait,
                    (unsigned long)mPostWait, (unsigned long)mCustDeinit);
        }
        mVirtual10ToPhysical12 = cust.getFunction<DimmingVirtual10ToPhysical12>("dimmingVirtual10ToPhysical12");
        AAL_LOGD("mVirtual10ToPhysical12 = %lx", (unsigned long)mVirtual10ToPhysical12);
        mCustData = NULL;
    }

    int init() {
        int ret = -1;
        if (mCustInit)
            ret = mCustInit(mService, &mCustData);
        return ret;
    }

    int deinit() {
        int ret = -1;
        if (mCustDeinit)
            ret = mCustDeinit(mService, mCustData);
        return ret;
    }

    int preWait(unsigned int debugLevel = 0) {
        int ret = -1;
        if (mPreWait) {
            if (debugLevel & eDebugTime) AAL_LOGD("preWait() ...");
            ret = mPreWait(mService, mCustData);
        }
        return ret;
    }

    int postWait(unsigned int debugLevel = 0) {
        int ret = -1;
        if (mPostWait) {
            if (debugLevel & eDebugTime) AAL_LOGD("postWait() ...");
            ret = mPostWait(mService, mCustData);
        }
        return ret;
    }

    status_t custInvoke(int32_t cmd, int64_t arg, unsigned int debugLevel = 0) {
        status_t ret = -1;
        if (mCustInvoke) {
            ret = mCustInvoke(mService, mCustData, cmd, arg);
        }
        return ret;
    }

    int dimmingVirtual10ToPhysical12(int virtualValue10) {
        if (mVirtual10ToPhysical12) {
            int physicalValue12 = mVirtual10ToPhysical12(mService, mCustData, virtualValue10);
            if (virtualValue10 > 0 && physicalValue12 <= 0)
                physicalValue12 = 1;
            return physicalValue12;
        }
        return (virtualValue10 << 2) | (virtualValue10 >> 8);
    }
};


AALService::AALService()
{
    AAL_LOGD("AALService constructor");

    mAALInitReg = new AALInitReg;
    mAALInput = new AALInput;
    mAALOutput = new AALOutput;
    mAALInterface = new AALInterface;
    mCustFunc = new CustFunctions(this);
    mLightSensor = new AALLightSensor;

    mDispFd = open(DISP_DEV_NODE_PATH, O_RDONLY, 0);
    if (mDispFd == -1) {
        AAL_LOGE("Fail to open node: " DISP_DEV_NODE_PATH ", please check the permission.");
    }

    mScrWidth = 1080;
    mScrHeight = 1920;
    if (mDispFd > 0) {
        if (!getScreenResolution(mDispFd, &mScrWidth, &mScrHeight)) {
            AAL_LOGE("Fail to get LCM rsolution.");
        }
        AAL_LOGD("Screen size = (w %d, h %d)", mScrWidth, mScrHeight);
    }

    mPmsScrState = eScrStateOn;
    mPrevScrState = eScrStateOn;
    mCurrScrState = eScrStateOn;
    mTargetBacklight = LUMINANCE_MAX;
    mLongTermBacklight = LUMINANCE_MAX;
    mBacklight = LUMINANCE_MAX;
    mPrevFuncFlags = eFuncNone;
    mFuncFlags = eFuncNone;
    mPrevIsPartialUpdate = 0;
    mIsPartialUpdate = 0;
    mUserBrightness = 128;
    mALI = -1;
    mOutBacklight = -1;
    mOutCabcGain = 256;
    mBacklightInitFlag = false;

    setFunction(FUNC_NONE);

    clearEvents();

    mDebug.panelOn = false;
    mDebug.panelDisplayed = false;
    mDebug.alwaysEnable = false;
    mDebug.overwriteALI = -1;
    mDebug.overwriteBacklight = -1;
    mDebug.overwriteCABC = -1;
    mDebug.overwriteDRE = -1;
    mDebug.overwritePartial = -1;
    mDebug.overwriteRefreshLatency = 0;

    mDebugLevel = 0;
    char property[PROPERTY_VALUE_MAX];
    if (property_get(AAL_PROPERTY_DEBUG, property, NULL) > 0) {
        mDebugLevel = (unsigned int)strtoul(property, NULL, 0);
        AAL_LOGD("AAL debug level: 0x%x", mDebugLevel);
    }

    mUseExternalAli = false;
    mEventEnabled = false;
    mToEnableEvent = false;
    mLightSensor->setListener(onALIChanged, this);

    m_dre_mode = eDRE2;

    mSupportEssLevelCtlByKernel = 0;
    mSupportEssLevelRemapping = 0;
    mSupportDreEnableCtlByKernel = 0;
    mInitSmartBacklightStrength = 0;
    mDreEnCommandId = -1;
    mEssEnCommandId = -1;
    mEssLevelCommandId = -1;

    initAdapt();
}

AALService::~AALService()
{
    deinitAdapt();

    delete mAALInitReg;
    delete mAALInput;
    delete mAALOutput;
    delete mAALInterface;
    delete mCustFunc;
    delete mLightSensor;

    if (mDispFd != -1)
        close(mDispFd);
}

status_t AALService::setFunction(uint32_t funcFlags)
{
    Mutex::Autolock _l(mLock);

    AAL_LOGD("setFunction: 0x%x -> 0x%x", mFuncFlags, funcFlags);

    if (mFuncFlags != funcFlags) {
        if (!isEventSet(eEvtFunction)) {
            mPrevFuncFlags = mFuncFlags;
        }

        mFuncFlags = 0;
        if (funcFlags & FUNC_CABC) mFuncFlags |= eFuncCABC;
        if (funcFlags & FUNC_DRE)  mFuncFlags |= eFuncDRE;

        mLightSensor->setEnabled((mFuncFlags & eFuncDRE) != 0);

        setEvent(eEvtFunction);
        enableAALEvent(true);
    }

    return NO_ERROR;
}

void AALService::onPartialUpdateChange(int32_t partialUpdate)
{
    if (mIsPartialUpdate != partialUpdate) {
        AAL_LOGD("onPartialUpdateChange: 0x%x -> 0x%x", mIsPartialUpdate, partialUpdate);

        mPrevIsPartialUpdate = mIsPartialUpdate;
        mIsPartialUpdate = partialUpdate;

        setEvent(eEvtPartialUpdate);
        enableAALEvent(true);
    }
}

void AALService::onBacklightChanged(int32_t level_1024)
{
    if (level_1024 < 0) // Uninitialized value from kernel
        return;

    int newScrState = mCurrScrState;

    if (mBacklight != level_1024 || (mBacklightInitFlag == false)) {
        int backlight4095 = mCustFunc->dimmingVirtual10ToPhysical12(level_1024);

        AAL_LOGD("onBacklightChanged: %d/1023 -> %d/1023(phy:%d/4095)",
                mBacklight, level_1024, backlight4095);

        mBacklight = level_1024;
        mTargetBacklight = backlight4095;
        mLongTermBacklight = backlight4095;
        setEvent(eEvtTargetBacklight | eEvtLongTermBacklight);

        if (mBacklight == 0) {
            if (mCurrScrState != eScrStateOff)
                newScrState = eScrStateOff;
        } else {
            if (mPmsScrState != eScrStateOff) {
                newScrState = mPmsScrState;
            } else {
                newScrState = eScrStateOn;
            }
        }
        mBacklightInitFlag = true;
    }

    if (newScrState != mCurrScrState) {
        AAL_LOGD("onBacklightChanged: change screen state %d(%s) -> %d(%s)",
                mCurrScrState, getScrStateStr(mCurrScrState), newScrState, getScrStateStr(newScrState));

        if (mCurrScrState == eScrStateOff) { // from off -> on
            mLightSensor->setEnabled(mFuncFlags & eFuncDRE);
        } else if (newScrState == eScrStateOff) { // from on -> off
            mLightSensor->setEnabled(false);
        }

        if (!isEventSet(eEvtScrState)) {
            mPrevScrState = mCurrScrState;
            setEvent(eEvtScrState);
        }
        mCurrScrState = newScrState;
    }
}

void AALService::onESSLevelChanged(int32_t strengthIndex_cmd)
{
#ifdef AAL_SUPPORT_KERNEL_API
    int currEssStrength = 0;
    int prevEssStrength = 0;
    int currEnable = 0;
    int regValue[16];
    int ret = 0;

    int strengthIndex = AAL_GET_FLAG_FROM_CMD(strengthIndex_cmd);
    int id = AAL_GET_ID_FROM_CMD(strengthIndex_cmd);

    if ((mFuncFlags & eFuncCABC) == eFuncCABC)
        currEnable = 1;
    else
        currEnable = 0;

    if ((mSupportEssLevelCtlByKernel == 1) && (id != mEssLevelCommandId)) {

        int tableUpBound = sizeof(mESSLevelMappingTable)/sizeof(int);

        if (strengthIndex == ESS_LEVEL_BY_CUSTOM_LIB) {// reset to initial strength
            currEssStrength = mInitSmartBacklightStrength;
        } else {
            if (mSupportEssLevelRemapping == 1) {
                strengthIndex = AAL_CLIP(strengthIndex, 0, tableUpBound - 1);
                currEssStrength = mESSLevelMappingTable[strengthIndex];
            } else {
                strengthIndex = AAL_CLIP(strengthIndex, 0, 255);
                currEssStrength = strengthIndex;
            }
        }

        ret = mAALInterface->getTuningReg("SmartBacklightStrength", regValue);
        prevEssStrength = regValue[0];

        if (currEssStrength != prevEssStrength) {
            AAL_LOGD("onESSLevelChanged: srength=%4d->%4d(level=%4d)",
                prevEssStrength, currEssStrength, strengthIndex);
            mAALInterface->setSmartBacklightStrength(currEssStrength);
            setEvent(eEvtConfigChanged);
            enableAALEvent(true);
        }

        mEssLevelCommandId = id;
    }
#else
    UNUSED(strengthIndex_cmd);
#endif
}

void AALService::onESSEnableChanged(int32_t enable_cmd)
{
#ifdef AAL_SUPPORT_KERNEL_API
    unsigned int funcFlags = 0;
    int currEnable = 0;

    int enable = AAL_GET_FLAG_FROM_CMD(enable_cmd);
    int id = AAL_GET_ID_FROM_CMD(enable_cmd);

    if ((mFuncFlags & eFuncCABC) == eFuncCABC)
        currEnable = 1;
    else
        currEnable = 0;

    if ((enable != ESS_EN_BY_CUSTOM_LIB) && (id != mEssEnCommandId)) {
        if ((enable != currEnable)) {
            if (enable == 1) {
                funcFlags = mFuncFlags | eFuncCABC;
                mSupportEssLevelCtlByKernel = 1;
            } else {
                funcFlags = mFuncFlags & (unsigned int)(~eFuncCABC);
                mSupportEssLevelCtlByKernel = 0;
            }

            if (mFuncFlags != funcFlags) {
                if (!isEventSet(eEvtFunction)) {
                    mPrevFuncFlags = mFuncFlags;
                }
                mFuncFlags = 0;
                if (funcFlags & FUNC_CABC) mFuncFlags |= eFuncCABC;
                if (funcFlags & FUNC_DRE)  mFuncFlags |= eFuncDRE;

                AAL_LOGD("onESSEnableChanged: 0x%x->0x%x", mPrevFuncFlags, mFuncFlags);

                setEvent(eEvtFunction);
                enableAALEvent(true);
            }
        }
        mEssEnCommandId = id;
    }
#else
    UNUSED(enable_cmd);
#endif
}

void AALService::onDREEnableChanged(int32_t enable_cmd)
{
#ifdef AAL_SUPPORT_KERNEL_API
    unsigned int funcFlags = 0;
    int currEnable = 0;

    int enable = AAL_GET_FLAG_FROM_CMD(enable_cmd);
    int id = AAL_GET_ID_FROM_CMD(enable_cmd);

    if ((mFuncFlags & eFuncDRE) == eFuncDRE)
        currEnable = 1;
    else
        currEnable = 0;

    if ((mSupportDreEnableCtlByKernel == 1) && (enable != DRE_EN_BY_CUSTOM_LIB) && (id != mDreEnCommandId)) {
        if (enable != currEnable) {
            if (enable == 1)
                funcFlags = mFuncFlags | eFuncDRE;
            else
                funcFlags = mFuncFlags & (unsigned int)(~eFuncDRE);

            if (mFuncFlags != funcFlags) {
                if (!isEventSet(eEvtFunction)) {
                    mPrevFuncFlags = mFuncFlags;
                }

                mFuncFlags = 0;
                if (funcFlags & FUNC_CABC) mFuncFlags |= eFuncCABC;
                if (funcFlags & FUNC_DRE)  mFuncFlags |= eFuncDRE;

                mLightSensor->setEnabled((mFuncFlags & eFuncDRE) != 0);

                AAL_LOGD("onDREEnableChanged: 0x%x->0x%x", mPrevFuncFlags, mFuncFlags);

                setEvent(eEvtFunction);
                enableAALEvent(true);
            }
        }
        mDreEnCommandId = id;
    }
#else
    UNUSED(enable_cmd);
#endif
}

void AALService::onPanelTypeChanged(int32_t panel_type)
{
#ifdef AAL_SUPPORT_KERNEL_API
    if (panel_type == CONFIG_TO_LCD)
        mAALInterface->setESS_PanelType(0);
    else if (panel_type == CONFIG_TO_AMOLED)
        mAALInterface->setESS_PanelType(1);
#else
     UNUSED(panel_type);
#endif
}

status_t AALService::setLightSensorValue(int32_t value)
{
    Mutex::Autolock _l(mLock);

    AAL_LOGD("setLightSensorValue: %d -> %d", mALI, value);

    mUseExternalAli = true;

    // Disable light sensor first
    if (mLightSensor->isEnabled()) {
        mLightSensor->setEnabled(false); // disable light sensor
    }

    mALI = value;
    setEvent(eEvtALI);
    enableAALEvent(true);

    return NO_ERROR;
}


void AALService::onALIChanged(void* obj, int32_t ali)
{
    AALService *service = static_cast<AALService*>(obj);

    Mutex::Autolock _l(service->mLock);

    if (service->mUseExternalAli)
        return;

    if (service->mALI != ali) {
        AAL_LOGD("onALIChanged: %d -> %d", service->mALI, ali);
        service->mALI = ali;
        service->setEvent(eEvtALI);
        service->enableAALEvent(true);
    }

    if (service->mDebug.overwriteALI >= 0) {
        AAL_LOGD("onALIChanged() overwrite ALI to %d", service->mDebug.overwriteALI);
    }
}


status_t AALService::setScreenState(int32_t state, int32_t brightness_256)
{
    Mutex::Autolock _l(mLock);

    mPmsScrState = state;
    if (mCurrScrState != mPmsScrState) {
        AAL_LOGD("setScreenState: %d(%s), current = %d(%s)",
                mPmsScrState, getScrStateStr(mPmsScrState), mCurrScrState, getScrStateStr(mCurrScrState));

        // On/off is controlled by onBacklightChanged()
        if (mPmsScrState != eScrStateOff && mCurrScrState != eScrStateOff) {
            if (!isEventSet(eEvtScrState)) {
                mPrevScrState = mCurrScrState;
                setEvent(eEvtScrState);
            }
            mCurrScrState = mPmsScrState;
            enableAALEvent(true);
        }
    }

    return NO_ERROR;
}


status_t AALService::custInvoke(int32_t cmd, int64_t arg)
{
    return mCustFunc->custInvoke(cmd, arg);
}

status_t AALService::readField(uint32_t field, uint32_t *value)
{
    if (mAALInterface->isFieldLegal(field) == false)
    {
        *value = 0;
        return BAD_INDEX;
    }

    {
        Mutex::Autolock _l(mLock);

        setEvent(eEvtFieldRead);

        enableAALEvent(true);
    }

    usleep(50 * 1000);

    {
        Mutex::Autolock _l(mLock);

        mAALInterface->readField(field, value);
    }

    return NO_ERROR;
}

status_t AALService::writeField(uint32_t field, uint32_t value)
{
    if (mAALInterface->isFieldLegal(field) == false)
    {
        return BAD_INDEX;
    }

    Mutex::Autolock _l(mLock);

    mAALInterface->writeField(field, value);

    setEvent(eEvtFieldWritten);

    enableAALEvent(true);

    return NO_ERROR;
}


#define OUTPUT_BUFFER_MAX 8192
#define CURVE_BUFFER_MAX  (4096 + 1)

static const String16 sDump("android.permission.DUMP");
status_t AALService::dump(int fd, const Vector<String16>& args)
{
    static const size_t SIZE = 4096;
    char *buffer;
    String8 result;

    buffer = new char[SIZE];

    if (!PermissionCache::checkCallingPermission(sDump)) {
        snprintf(buffer, SIZE, "an't dump AAL Service from\n");
        result.append(buffer);
    } else {
        // Try to get the main lock, but don't insist if we can't
        // (this would indicate AALService is stuck, but we want to be able to
        // print something in dumpsys).
        int retry = 3;
        while (mLock.tryLock() < 0 && --retry >= 0) {
            usleep(500 * 1000);
        }
        const bool locked(retry >= 0);
        if (!locked) {
            snprintf(buffer, SIZE,
                    "AALService appears to be unresponsive, "
                    "dumping anyways (no locks held)\n");
            result.append(buffer);
        }

        // dump ESS/DRE version for quickly check
        struct AALInfo aalinfo;
        mAALInterface->getAALInfo(&aalinfo);
        char panel_type[16] = "UNKNOWN";

        if (aalinfo.lcm_type == LCD)
            strncpy(panel_type, "LCD", 15);
        else if (aalinfo.lcm_type == AMOLED)
            strncpy(panel_type, "AMOLED", 15);
        int funcAvailable = 0x0;
        if (aalinfo.ess_ready)
            funcAvailable = funcAvailable | eFuncCABC;
        if (aalinfo.dre_ready)
            funcAvailable = funcAvailable | eFuncDRE;

        snprintf(buffer, SIZE,
                "Service state:\n"
                "   Function                    : setting(0x%x) availible(0x%x)\n"
                "   Screen state                : %d\n"
                "   Backlight brightness        : %d/4095\n"
                "   Ambient-light               : %d lux\n"
                "   Enable AAL event            : %d\n"
                "   ESS Version                 : %1.1f(%s)\n"
                "   DRE Version                 : %1.1f\n",
                mAALInput->funcFlags, funcAvailable,
                mAALInput->currScrState,
                mAALInput->targetBacklight,
                mAALInput->currALI,
                (int)mEventEnabled,
                aalinfo.ess_version, panel_type,
                aalinfo.dre_version);
        result.append(buffer);

        size_t numArgs = args.size();
        for (size_t argIndex = 0; argIndex < numArgs; ) {
            if (args[argIndex] == String16("--histogram")) {
                snprintf(buffer, SIZE, "\nLuma Histogram:\n");
                result.append(buffer);
                for (int i = 0; i < 33; i++) {
                    snprintf(buffer, SIZE, "   [%2d] %10d\n", i, mAALInput->MaxHisBin[i]);
                    result.append(buffer);
                }

                if (m_dre_mode == eDRE30) {
                    int blk_x = 0, blk_y = 0;

                    if (++argIndex < numArgs) {
                        blk_y = AAL_CLIP(asInt(args[argIndex]), 0, DRE30_BLK_NUM_MAX-1);
                    }
                    if (++argIndex < numArgs) {
                        blk_x = AAL_CLIP(asInt(args[argIndex]), 0, DRE30_BLK_NUM_MAX-1);
                    }

                    snprintf(buffer, SIZE, "\nblock [y, x] = [%d, %d], Block Luma Histogram:\n", blk_y, blk_x);
                    result.append(buffer);
                    for (int i = 0; i < DRE30_LUMA_HIST_NUM_WEIGHT; i++) {
                        snprintf(buffer, SIZE, "   [%2d] %10d\n", i, mAALInput->DREMaxHisSet[blk_y][blk_x][i]);
                        result.append(buffer);
                    }
                }
            } else if (args[argIndex] == String16("--output")) {
                snprintf(buffer, SIZE,
                        "\n"
                        "Output backlight: %d/1023\n"
                        "Gain: %d\n"
                        "Output curve: (DRE / ESSlmt)\n",
                        mAALOutput->FinalBacklight,
                        mAALOutput->cabc_fltgain_force);
                result.append(buffer);

                for (int i = 0; i < 33; i++) {
                    int dreGain = 0;
                    if (i < DRE_POINT_NUM)
                        dreGain = mAALOutput->DREGainFltStatus[i];

                    snprintf(buffer, SIZE, "   [%2d]  %7d  %7d \n", i,
                        dreGain, mAALOutput->cabc_gainlmt[i]);

                    result.append(buffer);
                }
            }
            else if (args[argIndex] == String16("--debug")) {
                debugDump(eDebugInput | eDebugOutput | eDebugContent);
            } else if (args[argIndex] == String16("--aal_debug_on")) {
                mDebug.panelOn = true;
                snprintf(buffer, SIZE, "Debug panel: on\n");
                result.append(buffer);
            } else if (args[argIndex] == String16("--aal_debug_off")) {
                mDebug.panelOn = false;
                snprintf(buffer, SIZE, "Debug panel: off\n");
                result.append(buffer);
            } else if (args[argIndex] == String16("--wDRE")) {
                if (m_dre_mode == eDRE30 && argIndex + 1 < numArgs) {
                    int blk_x = 0, blk_y = 0;

                    argIndex++;
                    mDebug.overwriteDRE = AAL_MIN(asInt(args[argIndex]), 255);

                    if (++argIndex < numArgs) {
                        blk_y = AAL_CLIP(asInt(args[argIndex]), 0, DRE30_BLK_NUM_MAX-1);
                    }
                    if (++argIndex < numArgs) {
                        blk_x = AAL_CLIP(asInt(args[argIndex]), 0, DRE30_BLK_NUM_MAX-1);
                    }

                    mDebug.overwriteDREBlockX = blk_x;
                    mDebug.overwriteDREBlockY = blk_y;
                    snprintf(buffer, SIZE, "Overwrite block [y, x] = [%d, %d] DRE: %d\n",
                        mDebug.overwriteDREBlockY, mDebug.overwriteDREBlockX, mDebug.overwriteDRE);
                } else if (argIndex + 1 < numArgs) {
                    argIndex++;
                    mDebug.overwriteDRE = asInt(args[argIndex]);
                    snprintf(buffer, SIZE, "Overwrite DRE: %d\n", mDebug.overwriteDRE);
                } else {
                    snprintf(buffer, SIZE, "Overwrite DRE off\n");
                }
                result.append(buffer);
            } else if (args[argIndex] == String16("--partial_on")) {
                mDebug.overwritePartial = 1;
                snprintf(buffer, SIZE, "partial always on\n");
                result.append(buffer);
            } else if (args[argIndex] == String16("--partial_off")) {
                mDebug.overwritePartial = 0;
                snprintf(buffer, SIZE, "partial always off\n");
                result.append(buffer);
            } else if (args[argIndex] == String16("--partial_auto")) {
                mDebug.overwritePartial = -1;
                snprintf(buffer, SIZE, "partial by alg\n");
                result.append(buffer);
            } else if (args[argIndex] == String16("--wESS")) {
                if (argIndex + 1 < numArgs) {
                    argIndex++;
                    mDebug.overwriteCABC = asInt(args[argIndex]);
                    snprintf(buffer, SIZE, "Overwrite ESS: %d\n", mDebug.overwriteCABC);
                } else {
                    snprintf(buffer, SIZE, "Overwrite ESS off\n");
                }
                result.append(buffer);
            } else if (args[argIndex] == String16("--always_enable")) {
                mDebug.alwaysEnable = true;
                enableAALEvent(true);
                snprintf(buffer, SIZE, "Alwasys enable trigger\n");
                result.append(buffer);
            } else if (args[argIndex] == String16("--debug_level")) {
                if (argIndex + 1 < numArgs) {
                    argIndex++;
                    mDebugLevel = asInt(args[argIndex]);
                    mAALInterface->setDebugFlags(mDebugLevel);
                    snprintf(buffer, SIZE, "Debug level = 0x%x\n", mDebugLevel);
                    result.append(buffer);
                }
            } else if (args[argIndex] == String16("--function")) {
                snprintf(buffer, SIZE, "Bits: 0x%x = ESS, 0x%x = DRE\n", eFuncCABC, eFuncDRE);
                result.append(buffer);

                if (argIndex + 1 < numArgs) {
                    argIndex++;
                    if (!isEventSet(eEvtFunction)) {
                        mPrevFuncFlags = mFuncFlags;
                    }
                    mFuncFlags = asInt(args[argIndex]);
                    setEvent(eEvtFunction);
                    mLightSensor->setEnabled((mFuncFlags & eFuncDRE) != 0);
                    enableAALEvent(true);
                    if (mFuncFlags != 0) {
                        snprintf(buffer, SIZE, "Function =%s%s\n",
                            ((mFuncFlags & eFuncCABC) ? " ESS" : ""),
                            ((mFuncFlags & eFuncDRE) ? " DRE" : ""));
                        result.append(buffer);
                    } else {
                        snprintf(buffer, SIZE, "All functions off\n");
                        result.append(buffer);
                    }
                }
            } else if (args[argIndex] == String16("--wALI")) {
                if (argIndex + 1 < numArgs) {
                    argIndex++;
                    mDebug.overwriteALI = asInt(args[argIndex]);
                    setEvent(eEvtALI);
                    enableAALEvent(true);
                    snprintf(buffer, SIZE, "Overwrite ALI = %d\n", mDebug.overwriteALI);
                    result.append(buffer);
                }
            } else if (args[argIndex] == String16("--wBL")) {
                if (argIndex + 1 < numArgs) {
                    argIndex++;
                    mDebug.overwriteBacklight = asInt(args[argIndex]);
                    setEvent(eEvtTargetBacklight);
                    enableAALEvent(true);
                    snprintf(buffer, SIZE, "Overwrite backlight = %d\n", mDebug.overwriteBacklight);
                    result.append(buffer);
                }
            } else if (args[argIndex] == String16("--wPara")) {//adb cmd for AAL tuning parameter control
                if (argIndex + 2 < numArgs) {
                    int ParaIdx, ParaVal;
                    int Update = true;

                    argIndex++;
                    ParaIdx = asInt(args[argIndex]);
                    argIndex++;
                    ParaVal = asInt(args[argIndex]);

                    switch (ParaIdx) {
                        case 0:
                            mAALInterface->setSmartBacklightStrength(ParaVal);
                            snprintf(buffer, SIZE, "Set Smart BL Strength = %d\n", ParaVal);
                            break;

                        case 1:
                            mAALInterface->setSmartBacklightRange(ParaVal);
                            snprintf(buffer, SIZE, "Set Smart BL Range = %d\n", ParaVal);
                            break;

                        case 2:
                            mAALInterface->setReadabilityLevel(ParaVal);
                            snprintf(buffer, SIZE, "Set Sunlight Readability = %d\n", ParaVal);
                            break;

                        case 3:
                            mAALInterface->setReadabilityBLILevel(ParaVal);
                            snprintf(buffer, SIZE, "Set Low BL Readability = %d\n", ParaVal);
                            break;

                        default:
                            Update = false;
                            snprintf(buffer, SIZE, "Parameter Index Error = %d\n", ParaIdx);
                            break;
                    }

                    if (Update) {
                        setEvent(eEvtConfigChanged);
                        enableAALEvent(true);
                    }

                    result.append(buffer);
                }
            } else if (args[argIndex] == String16("--rPara")) {//adb cmd for read AAL settings
                ImplParameters implParam;

                mAALInterface->getParameters(&implParam);
                snprintf(buffer, SIZE,
                    "Current Setting:\n"
                    "Set Smart BL Strength = %d\n"
                    "Set Smart BL Range = %d\n"
                    "Set Sunlight Readability = %d\n"
                    "Set Low BL Readability = %d\n"
                    , implParam.smartBacklightStrength, implParam.smartBacklightRange
                    , implParam.readabilityLevel, implParam.readabilityBLILevel);

                result.append(buffer);
            } else if (args[argIndex] == String16("--refresh")) {
                if (argIndex + 1 < numArgs) {
                    argIndex++;
                    mDebug.overwriteRefreshLatency = asInt(args[argIndex]);
                    if (mDebug.overwriteRefreshLatency >= (int)eRefresh_33ms)
                        mDebug.overwriteRefreshLatency = (int)eRefresh_33ms;
                    else if (mDebug.overwriteRefreshLatency > 0)
                        mDebug.overwriteRefreshLatency = (int)eRefresh_17ms;
                    setEvent(eEvtTargetBacklight);
                    enableAALEvent(true);
                    if (mDebug.overwriteRefreshLatency > 0) {
                        snprintf(buffer, SIZE, "Overwrite refresh latency = %dms\n", mDebug.overwriteRefreshLatency);
                    } else {
                        snprintf(buffer, SIZE, "Overwrite refresh latency off\n");
                    }
                    result.append(buffer);
                }
            } else if (args[argIndex] == String16("--load_blpl")) {
                std::ifstream inputFile;
                char output[OUTPUT_BUFFER_MAX];
                int num, index, item_index = 0, remapping_type = 0;
                bool isNum = false;
                inputFile.open("/system/bin/aal_blpl.txt");
                if (inputFile.is_open()) {
                    snprintf(buffer, SIZE, "load file sucess\n");
                    while (!inputFile.eof()) {
                        inputFile >> output;
                        if (strcmp(output, "bl") == 0) {
                            remapping_type = BACKLIGHT_LEVEL;
                            item_index = 0;
                            AAL_LOGD("scan file label: %s", output);
                        } else if (strcmp(output, "pl") == 0) {
                            remapping_type = PIXEL_GAIN;
                            item_index = 0;
                            AAL_LOGD("scan file label: %s", output);
                        } else {
                            index = 0;
                            num = 0;
                            for (index = 0; index < OUTPUT_BUFFER_MAX; index++) {
                                if (output[index] >= '0' && output[index] <= '9') {
                                    num = num * 10 + (output[index] - '0');
                                    isNum = true;
                                } else {
                                    if (isNum == true) {
                                        mAALInterface->setUDCurve(remapping_type, item_index, num);
                                        item_index += 1;
                                        item_index = item_index > CURVE_BUFFER_MAX ? CURVE_BUFFER_MAX : item_index;
                                    }

                                    if (output[index] == '\n' || output[index] == '\0') {
                                        break;
                                    } else {
                                        num = 0;
                                        isNum = false;
                                    }
                                }
                            }
                        }
                    }
                } else {
                    snprintf(buffer, SIZE, "load file fail\n");
                }
                result.append(buffer);

            } else if (args[argIndex] == String16("--load_tuning_reg")) {
                std::ifstream inputFile;
                char output[OUTPUT_BUFFER_MAX];
                char reg_name[128];
                char log_buffer[512];
                char reg_value[6];
                int tuning_parameter[CURVE_BUFFER_MAX];
                int num, index, item_index = 0;
                bool isNum = false;
                int fieldNum = 0;
                inputFile.open("/system/bin/aal_tuning.txt");

                if (inputFile.is_open()) {
                    snprintf(buffer, SIZE, "load file sucess\n");
                    result.append(buffer);
                    while (!inputFile.eof()) {
                        inputFile.getline(output, OUTPUT_BUFFER_MAX);
                        if (strncmp(output, "label:", 6) == 0) {
                            if (sizeof(output) > 6) {
                                sscanf(output, "label:%s", reg_name);
                                item_index = 0;
                                fieldNum += 1;
                            }
                        } else if (strncmp(output, "end", 3) == 0) {
                            if (fieldNum > 0) {
                                int ret = mAALInterface->setTuningReg(reg_name, tuning_parameter);
                                snprintf(buffer, SIZE, "reg:%20s num#:%4d ret=%d\n", reg_name, item_index, ret);
                                result.append(buffer);
                                for (int i = 0; i < item_index; i += 16) {
                                    strncpy(log_buffer, "", sizeof(log_buffer) - 1);
                                    int last = min(item_index, i + 16);
                                    printIntArray(log_buffer, CURVE_BUFFER_MAX, tuning_parameter, item_index, i, last);
                                    snprintf(buffer, SIZE, "%s\n", log_buffer);
                                    result.append(buffer);
                                }
                            }
                        } else {
                            index = 0;
                            num = 0;
                            for (index = 0; index < OUTPUT_BUFFER_MAX; index++) {
                                if (output[index] >= '0' && output[index] <= '9') {
                                    num = num * 10 + (output[index] - '0');
                                    isNum = true;
                                } else {
                                    if (isNum == true) {
                                        tuning_parameter[item_index] = num;
                                        item_index += 1;
                                        item_index = item_index > CURVE_BUFFER_MAX ? CURVE_BUFFER_MAX : item_index;
                                    }

                                    if (output[index] == '\n' || output[index] == '\0') {
                                        break;
                                    } else {
                                        num = 0;
                                        isNum = false;
                                    }
                                }
                            }
                        }
                    }
                }else {
                    snprintf(buffer, SIZE, "load file fail\n");
                    result.append(buffer);
                }

            } else if (args[argIndex] == String16("--set_tuning_reg")) {
                if (argIndex + 1 < numArgs) {
                    argIndex++;
                    char reg_name[128];
                    char log_buffer[512];
                    int value[CURVE_BUFFER_MAX];
                    int item_index = 0;
                    strncpy(reg_name, String8(args[argIndex]), 127);

                    for (int i = 0; i < CURVE_BUFFER_MAX; i += 1) {
                        argIndex++;
                        if (argIndex >= numArgs)
                            break;
                        else {
                            value[item_index] = asInt(args[argIndex]);
                            item_index += 1;
                        }
                    }

                    int ret = mAALInterface->setTuningReg(reg_name, value);

                    if (ret == 0) {
                        snprintf(buffer, SIZE, "set tuning reg(%s) success\n", reg_name);
                        result.append(buffer);
                        snprintf(buffer, SIZE, "reg:%20s num#:%4d ret=%d\n", reg_name, item_index, ret);
                        result.append(buffer);
                        for (int i = 0; i < item_index; i += 16) {
                            strncpy(log_buffer, "", sizeof(log_buffer) - 1);
                            int last = min(item_index, i + 16);
                            printIntArray(log_buffer, CURVE_BUFFER_MAX, value, item_index, i, last);
                            snprintf(buffer, SIZE, "%s\n", log_buffer);
                            result.append(buffer);
                        }
                    } else {
                        snprintf(buffer, SIZE, "set tuning reg(%s) fail\n", reg_name);
                        result.append(buffer);
                    }
                }

            } else if (args[argIndex] == String16("--get_tuning_reg")) {
                if (argIndex + 1 < numArgs) {
                    argIndex++;
                    char reg_name[128];
                    char log_buffer[512];
                    int value[CURVE_BUFFER_MAX];
                    int item_index = 0;

                    strncpy(reg_name, String8(args[argIndex]), sizeof(reg_name) - 1);

                    for (int i = 0; i < CURVE_BUFFER_MAX; i += 1)
                        value[i] = -1;

                    int ret = mAALInterface->getTuningReg(reg_name, value);

                    for (int i = 0; i < CURVE_BUFFER_MAX; i += 1) {
                        if (value[i] == -1) {
                            item_index = i;
                            break;
                        }
                    }

                    if (ret == 0) {
                        snprintf(buffer, SIZE, "get tuning reg(%s) num(%d) success\n", reg_name, item_index);
                        result.append(buffer);
                        for (int i = 0; i < item_index; i += 16) {
                            strncpy(log_buffer, "", sizeof(log_buffer) - 1);
                            int last = min(item_index, i + 16);
                            printIntArray(log_buffer, CURVE_BUFFER_MAX, value, item_index, i, last);
                            snprintf(buffer, SIZE, "%s\n", log_buffer);
                            result.append(buffer);
                        }
                    } else {
                        snprintf(buffer, SIZE, "get tuning reg(%s) fail\n", reg_name);
                        result.append(buffer);
                    }
                }

            } else {
                snprintf(buffer, SIZE, "Unknown argument\n");
                result.append(buffer);
            }

            argIndex++;
        }

        if (locked) {
            mLock.unlock();
        }
    }

    write(fd, result.string(), result.size());

    delete [] buffer;

    return NO_ERROR;
}

status_t AALService::setSmartBacklightStrength(int32_t level)
{
    Mutex::Autolock _l(mLock);

    AAL_LOGD("setSmartBacklightStrength: %d", level);

    mAALInterface->setSmartBacklightStrength(level);
    setEvent(eEvtConfigChanged);
    enableAALEvent(true);

    return NO_ERROR;
}

status_t AALService::setSmartBacklightRange(int32_t level)
{
    Mutex::Autolock _l(mLock);

    AAL_LOGD("setSmartBacklightRange: %d", level);

    mAALInterface->setSmartBacklightRange(level);
    setEvent(eEvtConfigChanged);
    enableAALEvent(true);

    return NO_ERROR;
}

status_t AALService::setReadabilityLevel(int32_t level)
{
    Mutex::Autolock _l(mLock);

    AAL_LOGD("setReadabilityLevel: %d", level);

    mAALInterface->setReadabilityLevel(level);
    setEvent(eEvtConfigChanged);
    enableAALEvent(true);

    return NO_ERROR;
}

status_t AALService::setLowBLReadabilityLevel(int32_t level)
{
    Mutex::Autolock _l(mLock);

    AAL_LOGD("setLowBLReadabilityLevel: %d", level);

    mAALInterface->setReadabilityBLILevel(level);
    setEvent(eEvtConfigChanged);
    enableAALEvent(true);

    return NO_ERROR;
}

status_t AALService::getParameters(AALParameters *outParam)
{
    Mutex::Autolock _l(mLock);
    ImplParameters implParam;

    mAALInterface->getParameters(&implParam);

    outParam->readabilityLevel = implParam.readabilityLevel;
    outParam->lowBLReadabilityLevel = implParam.readabilityBLILevel;
    outParam->smartBacklightStrength = implParam.smartBacklightStrength;
    outParam->smartBacklightRange = implParam.smartBacklightRange;

    return NO_ERROR;
}


status_t AALService::debugDump(unsigned int debugLevel) // Already locked in dump()
{
    if (debugLevel == 0)
        return NO_ERROR;

    static const int BSIZE = 512;
    char *buffer = new char[BSIZE];
    int blk_x, blk_y, dre_blk_x_num, dre_blk_y_num;

    if (debugLevel & eDebugInput) {
        AAL_LOGD("=============== Input ===============");
        AAL_LOGD("Function flags = 0x%x, events = 0x%x", mAALInput->funcFlags, mAALInput->eventFlags);
        AAL_LOGD("ALI = %d, user brightness level = %d", mAALInput->currALI, mAALInput->userBrightnessLevel);
        AAL_LOGD("State = %d(%s); target BL = %d/4095(%d/1023), long term = %d/4095",
                mAALInput->currScrState, getScrStateStr(mAALInput->currScrState),
                mAALInput->targetBacklight, (mAALInput->targetBacklight >> 2), mAALInput->longTermBacklight);

        AAL_LOGD("His[ 0.. 9]:%s", printIntArray(buffer, BSIZE, &mAALInput->MaxHisBin[0], 33, 0, 10));
        AAL_LOGD("His[10..19]:%s", printIntArray(buffer, BSIZE, &mAALInput->MaxHisBin[0], 33, 10, 10));
        AAL_LOGD("His[20..29]:%s", printIntArray(buffer, BSIZE, &mAALInput->MaxHisBin[0], 33, 20, 10));
        AAL_LOGD("His[30..32]:%s", printIntArray(buffer, BSIZE, &mAALInput->MaxHisBin[0], 33, 30, 3));

        if (m_dre_mode == eDRE30) {
            dre_blk_y_num = mAALInput->dre_blk_y_num;
            dre_blk_x_num = mAALInput->dre_blk_x_num;
            AAL_LOGD("His block number [y, x] [%d, %d]", dre_blk_y_num, dre_blk_x_num);

            for (blk_y = 0; blk_y < dre_blk_y_num; blk_y++) {
                for (blk_x = 0; blk_x < dre_blk_x_num; blk_x++) {
                    AAL_LOGD("His block[%d, %d][ 0.. 8]:%s",
                        blk_y, blk_x, printIntArray(buffer, BSIZE, &mAALInput->DREMaxHisSet[blk_y][blk_x][0], DRE30_LUMA_HIST_NUM_WEIGHT, 0, 9));
                    AAL_LOGD("His block[%d, %d][ 9.. 16]:%s",
                        blk_y, blk_x, printIntArray(buffer, BSIZE, &mAALInput->DREMaxHisSet[blk_y][blk_x][0], DRE30_LUMA_HIST_NUM_WEIGHT, 9, 8));
                    AAL_LOGD("His block[%d, %d] rgb_max_sum = %d, large_diff_count_set = %d, max_diff_set = %d, chroma_hist = %d, flat_line_count_set = %d",
                        blk_y, blk_x, mAALInput->DRERGBMaxSum[blk_y][blk_x], mAALInput->DRELargeDiffCountSet[blk_y][blk_x],
                        mAALInput->DREMaxDiffSet[blk_y][blk_x], mAALInput->DREChromaHist[blk_y][blk_x],
                        mAALInput->DREFlatLineCountSet[blk_y][blk_x]);
                }
            }
        }
        AAL_LOGD("EnableAALEvent = %d", (int)mEventEnabled);
    }

    if (debugLevel & eDebugOutput) {
        AAL_LOGD("=============== Output ==============");

        AAL_LOGD("DRE[ 0.. 9]:%s", printIntArray(buffer, BSIZE, mAALOutput->DREGainFltStatus, DRE_POINT_NUM, 0, 10));
        AAL_LOGD("DRE[10..19]:%s", printIntArray(buffer, BSIZE, mAALOutput->DREGainFltStatus, DRE_POINT_NUM, 10, 10));
        AAL_LOGD("DRE[20..28]:%s", printIntArray(buffer, BSIZE, mAALOutput->DREGainFltStatus, DRE_POINT_NUM, 20, 9));

        if (m_dre_mode == eDRE30) {
            dre_blk_y_num = mAALInput->dre_blk_y_num;
            dre_blk_x_num = mAALInput->dre_blk_x_num;
            AAL_LOGD("DRE block number [y, x] [%d, %d]", dre_blk_y_num, dre_blk_x_num);

            for (blk_y = 0; blk_y < dre_blk_y_num; blk_y++) {
                for (blk_x = 0; blk_x < dre_blk_x_num; blk_x++) {
                    AAL_LOGD("DRE block[%d, %d][ 0.. 8]:%s",
                        blk_y, blk_x, printIntArray(buffer, BSIZE, &mAALOutput->DRECurveSet[blk_y][blk_x][0], DRE30_LUMA_CURVE_NUM, 0, 9));
                    AAL_LOGD("DRE block[%d, %d][ 9.. 16]:%s",
                        blk_y, blk_x, printIntArray(buffer, BSIZE, &mAALOutput->DRECurveSet[blk_y][blk_x][0], DRE30_LUMA_CURVE_NUM, 9, 8));
                }
            }
        }

        AAL_LOGD("ESSlmt[ 0.. 9]:%s", printIntArray(buffer, BSIZE, &mAALOutput->cabc_gainlmt[0], 33, 0, 10));
        AAL_LOGD("ESSlmt[10..19]:%s", printIntArray(buffer, BSIZE, &mAALOutput->cabc_gainlmt[0], 33, 10, 10));
        AAL_LOGD("ESSlmt[20..29]:%s", printIntArray(buffer, BSIZE, &mAALOutput->cabc_gainlmt[0], 33, 20, 10));
        AAL_LOGD("ESSlmt[30..32]:%s", printIntArray(buffer, BSIZE, &mAALOutput->cabc_gainlmt[0], 33, 30, 3));

        AAL_LOGD("Backlight = %d/1023, ESS force = %d, ActiveTrigger = %d, latency = %d",
            mAALOutput->FinalBacklight, mAALOutput->cabc_fltgain_force, mAALOutput->activeTrigger,
            mAALOutput->refreshLatency);
    }

    delete [] buffer;

    return NO_ERROR;
}


status_t AALService::enableAALEvent(bool enable)
{
    int ret = NO_ERROR;
    int enableValue;

    if (mDebug.alwaysEnable) {
        enable = true;
    }

        enableValue = enable ? 1 : 0;
    if (enable != mEventEnabled)
        if (mDebugLevel & eDebugBasic)
            AAL_LOGD("enableAALEvent: %d -> %d", (int)mEventEnabled, enableValue);
        /* force print log in log buffer when status of  event from 1 to 0 */
        if (enableValue == false)
            if ((mDebugLevel & eDebugBasic) == 0)
                printLogBuffer(true);

        if ((ret = ioctl(mDispFd, DISP_IOCTL_AAL_EVENTCTL, &enableValue)) != 0) {
            AAL_LOGE("DISP_IOCTL_AAL_EVENTCTL error: %d", ret);
        } else {
            mEventEnabled = enable;
        }

    if (enable) {
        mToEnableEvent = true;
    }

    return ret;
}


void AALService::onFirstRef()
{
    run("AALServiceMain", PRIORITY_DISPLAY);
}

status_t AALService::readyToRun()
{
    AAL_LOGD("AALService is ready to run.");
    return NO_ERROR;
}

bool AALService::initDriverRegs()
{
    if (mDispFd <= 0) {
        return false;
    }

    int ret = -1;
    int tryCount;

    DISP_AAL_INITREG *driver_init = new DISP_AAL_INITREG;

    driver_init->dre_map_bypass = mAALInitReg->dre_map_bypass;
    memcpy(driver_init->cabc_gainlmt, mAALInitReg->cabc_gainlmt, sizeof(driver_init->cabc_gainlmt));
#ifdef HAS_DRE30
    if (m_dre_mode == eDRE30) {
        /* DRE 3.0 Reg. */
        driver_init->dre_s_lower = mAALInitReg->dre_s_lower;
        driver_init->dre_s_upper = mAALInitReg->dre_s_upper;
        driver_init->dre_y_lower = mAALInitReg->dre_y_lower;
        driver_init->dre_y_upper = mAALInitReg->dre_y_upper;
        driver_init->dre_h_lower = mAALInitReg->dre_h_lower;
        driver_init->dre_h_upper = mAALInitReg->dre_h_upper;
        driver_init->dre_x_alpha_base = mAALInitReg->dre_x_alpha_base;
        driver_init->dre_x_alpha_shift_bit = mAALInitReg->dre_x_alpha_shift_bit;
        driver_init->dre_y_alpha_base = mAALInitReg->dre_y_alpha_base;
        driver_init->dre_y_alpha_shift_bit = mAALInitReg->dre_y_alpha_shift_bit;
        driver_init->act_win_x_end = mAALInitReg->act_win_x_end;
        driver_init->dre_blk_x_num = mAALInitReg->dre_blk_x_num;
        driver_init->dre_blk_y_num = mAALInitReg->dre_blk_y_num;
        driver_init->dre_blk_height = mAALInitReg->dre_blk_height;
        driver_init->dre_blk_width = mAALInitReg->dre_blk_width;
        driver_init->dre_blk_area = mAALInitReg->dre_blk_area;
        driver_init->dre_blk_area_min = mAALInitReg->dre_blk_area_min;
        driver_init->hist_bin_type = mAALInitReg->hist_bin_type;
        driver_init->dre_flat_length_slope = mAALInitReg->dre_flat_length_slope;
    }
#endif
    tryCount = 0;
    while (true) {
        ret = ioctl(mDispFd, DISP_IOCTL_AAL_INIT_REG, driver_init);
        tryCount++;

        if (ret != 0) {
            AAL_LOGE("DISP_IOCTL_AAL_INIT_REG error: %d", ret);
        } else {
            AAL_LOGD("Registers initialized");
            break;
        }

        if (tryCount >= 3)
            break;

        usleep(50 * 1000 * (1 << tryCount));
    }

    delete driver_init;

    return (ret == 0);
}

bool AALService::initDRE30SW(unsigned long dre30_hist_addr)
{
    if (mDispFd <= 0) {
        return false;
    }

    int ret = -1;
#ifdef HAS_DRE30
    int tryCount;

    DISP_DRE30_INIT *dre30_sw_init = new DISP_DRE30_INIT;

    dre30_sw_init->dre30_hist_addr = dre30_hist_addr;

    tryCount = 0;
    while (true) {
        ret = ioctl(mDispFd, DISP_IOCTL_AAL_INIT_DRE30, dre30_sw_init);
        tryCount++;

        if (ret != 0) {
            AAL_LOGE("DISP_IOCTL_AAL_INIT_DRE30 error: %d", ret);
        } else {
            AAL_LOGD("Registers initialized");
            break;
        }

        if (tryCount >= 3)
            break;

        usleep(50 * 1000 * (1 << tryCount));
    }

    delete dre30_sw_init;
#endif
    return (ret == 0);
}

bool AALService::threadLoop()
{
    int ret = 0;
    int regValue[16];
    // variable used for profile aal refresh latency
    millisec_t currentTime;
    millisec_t previousTime;
    millisec_t timeDelta;
    int profileFrmCount = 0;

    CustParameters cust("libaal_cust.so");

    int dre30_option_setting = 0;
    bool aalContinueRun = true;
    bool dre30Support = false;

    AAL_LOGD("AALFW initializing");

    if (cust.isGood()) {
        mCustFunc->link(cust);
    }
    mCustFunc->init();

    { // Init function
        mFuncFlags = (eFuncCABC | eFuncDRE);

        char property[PROPERTY_VALUE_MAX];
        if (property_get(AAL_DRE30_MODE_STR, property, NULL) > 0) {
            dre30_option_setting = (int)strtoul(property, NULL, 0);
        }

        if (cust.isGood()) {
            cust.loadVar("InitFunction", &mFuncFlags);
            cust.loadVar("SupportEssLevelCtlByKernel", &mSupportEssLevelCtlByKernel);
            cust.loadVar("SupportEssLevelRemapping", &mSupportEssLevelRemapping);
            cust.loadVar("SupportDreEnableCtlByKernel", &mSupportDreEnableCtlByKernel);
            cust.loadArray("ESSLevelMappingTable", mESSLevelMappingTable, 17, 0);
            AALLightSensor::loadCustParameters(cust);
        }

        if (property_get(AAL_PROPERTY_INIT_FUNCTION, property, NULL) > 0) {
            mFuncFlags = (unsigned int)strtoul(property, NULL, 0);
            AAL_LOGD("Init function from property: 0x%x", mFuncFlags);
        }

        mFuncFlags &= (eFuncCABC | eFuncDRE);
        mLightSensor->setEnabled((mFuncFlags & eFuncDRE) != 0);
    }

    mAALInterface->onInitFunction(dre30_option_setting);

    if (dre30Support == 1)
        m_dre_mode = eDRE30;

    mAALInterface->setDebugFlags(mDebugLevel);
    AALInitParam initParam;
    initParam.width = mScrWidth;
    initParam.height = mScrHeight;
    mAALInterface->onInitCommon(initParam, mAALInitReg);
#ifndef SUPPORT_CABC_HW
    mAALInterface->setESS_CurveType(0);
#endif
    // Load customizations
    if (cust.isGood()) {
        mAALInterface->onInitPlatform(initParam, cust, mAALInitReg);
        mCustFunc->link(cust);
    } else {
        AAL_LOGE("[WARNING] No libaal_cust.so");
    }

    ret = mAALInterface->getTuningReg("SmartBacklightStrength", regValue);
    mInitSmartBacklightStrength = regValue[0];
    AAL_LOGD("mInitSmartBacklightStrength = %d", mInitSmartBacklightStrength);

    if (!initDriverRegs()) {
        while (true) {
            if (mDispFd > 0) {
                AAL_LOGE("[ERROR] Kernel does not support AAL, please enable CONFIG_MTK_AAL_SUPPORT in kconfig.");
            } else {
                AAL_LOGE("[ERROR] Failed to open " DISP_DEV_NODE_PATH);
            }
            sleep(5);
        }
    }

    DISP_AAL_HIST *driver_hist = new DISP_AAL_HIST;
    DISP_AAL_PARAM *driver_param = new DISP_AAL_PARAM;
    AALOutput *prevOutput = new AALOutput;
    AALOutput *buffOutput = new AALOutput;
    memset(mAALOutput, 0, sizeof(AALOutput));
    memset(prevOutput, 0, sizeof(AALOutput));
    memset(buffOutput, 0, sizeof(AALOutput));

    setEvent(eEvtInit);

#ifdef HAS_DRE30
    DISP_DRE30_HIST *driver_dre30_hist = NULL;
    DISP_DRE30_PARAM *driver_dre30_param = NULL;

    if (m_dre_mode == eDRE30) {
        driver_dre30_hist = new DISP_DRE30_HIST;
        driver_hist->dre30_hist = (unsigned long)driver_dre30_hist;

        driver_dre30_param = new DISP_DRE30_PARAM;
        driver_param->dre30_gain = (unsigned long)driver_dre30_param;

        if (!initDRE30SW((unsigned long)driver_dre30_hist)) {
            while (true) {
                if (mDispFd > 0) {
                    AAL_LOGE("[ERROR] Kernel initialize DRE30 fail.");
                } else {
                    AAL_LOGE("[ERROR] Failed to open " DISP_DEV_NODE_PATH);
                }
                sleep(5);
            }
        }
    }
#endif

    int failCount = 0;
    mLogIndex = 0;
    char logbuffer[LOGITEMSIZE];

    currentTime = getTimeOfMillisecond();
    previousTime = getTimeOfMillisecond();

    while (aalContinueRun)
    {
        mCustFunc->preWait(mDebugLevel);

        // Wait for Vsync/COLOR signal and get histogram data
        if (mDebugLevel & eDebugTime) AAL_LOGD("DISP_IOCTL_AAL_GET_HIST waiting ...");

        // If somebody calls enablesAALEvent(true) before here,
        // the ioctl will return at next frame and mToEnableEvent will be false finally.
        mToEnableEvent = false;
        // If enablesAALEvent(true) occurs during here,
        // the ioctl will return at next frame and mToEnableEvent will be true finally,
        // results in another iteration for next-next frame. (maybe unnecessary)

        if ((ret = ioctl(mDispFd, DISP_IOCTL_AAL_GET_HIST, driver_hist)) != 0) {
            failCount++;
            AAL_LOGE("DISP_IOCTL_AAL_GET_HIST error: %d, fail# = %d", ret, failCount);

            if (failCount < 5)
                usleep(50 * 1000 *(1 << failCount));
            else // failCount >= 5
                sleep(1 << (failCount - 4));

            continue;
        }

        failCount = 0;

        mCustFunc->postWait(mDebugLevel);

        // If enablesAALEvent(true) occurs after here,
        // this iteration will not call enablesAALEvent(false) and another iteration
        // will be triggered successfully. That's what we want.

        if (mDebugLevel & eDebugTime) {
            currentTime = getTimeOfMillisecond();
            profileFrmCount += 1;
            timeDelta = currentTime - previousTime;
            if (timeDelta > 1000L) { // profile once about one second
                AAL_LOGD("AAL avg refresh latency=%u", timeDelta/profileFrmCount);
                profileFrmCount = 0;
                previousTime = currentTime;
            }
            AAL_LOGD("DISP_IOCTL_AAL_GET_HIST done");
        }

        // call AAL firmware to perfom calculation
        {
            Mutex::Autolock _l(mLock);

            // event flag must be locked
            onBacklightChanged(driver_hist->backlight);
#ifdef AAL_SUPPORT_KERNEL_API
            onESSEnableChanged(driver_hist->ess_enable);
            onESSLevelChanged(driver_hist->essStrengthIndex);
            onDREEnableChanged(driver_hist->dre_enable);
            onPanelTypeChanged(driver_hist->panel_type);
#endif
            onPartialUpdateChange(driver_hist->requestPartial);
            if (prevOutput != NULL && mAALOutput != NULL && buffOutput != NULL) {
                memcpy(buffOutput, mAALOutput, sizeof(AALOutput));
                memcpy(mAALOutput, prevOutput, sizeof(AALOutput));
                memcpy(prevOutput, buffOutput, sizeof(AALOutput));
            } else {
                AAL_LOGE("mAALOutput or prevOutput = NULL");
                aalContinueRun = false;
                break;
            }

            if (isEventSet(eEvtScrState)) {
                // For dirty check to force to write registers
                prevOutput->DREGainFltStatus[0] += 1;
            }

            mAALInput->eventFlags = mEventFlags;
            if (mDebug.overwriteALI < 0)
                mAALInput->currALI = mALI;
            else
                mAALInput->currALI = mDebug.overwriteALI;
            mAALInput->prevFuncFlags = mPrevFuncFlags;
            mAALInput->funcFlags = mFuncFlags;

            // set partial update
            mAALInput->prevIsPartialUpdate = (mPrevIsPartialUpdate == 0) ? false : true;
            mAALInput->isPartialUpdate = (mIsPartialUpdate == 0) ? false : true;

            mAALInput->prevScrState = (ScreenState)mPrevScrState;
            mAALInput->currScrState = (ScreenState)mCurrScrState;
            if (mDebug.overwriteBacklight < 0) {
                mAALInput->targetBacklight = mTargetBacklight;
                mAALInput->longTermBacklight = mLongTermBacklight;
            } else {
                mAALInput->targetBacklight = mDebug.overwriteBacklight;
                mAALInput->longTermBacklight = mDebug.overwriteBacklight;
            }
            mAALInput->userBrightnessLevel = mUserBrightness;
            mAALInput->ColorHist = driver_hist->colorHist;
            memcpy(&(mAALInput->MaxHisBin), driver_hist->maxHist, sizeof(mAALInput->MaxHisBin));
#ifdef HAS_DRE30
            if (m_dre_mode == eDRE30) {
                memcpy(&(mAALInput->DREMaxHisSet), driver_dre30_hist->dre_max_hist, sizeof(mAALInput->DREMaxHisSet));
                memcpy(&(mAALInput->DRERGBMaxSum), driver_dre30_hist->dre_rgb_max_sum, sizeof(mAALInput->DRERGBMaxSum));
                memcpy(&(mAALInput->DREChromaHist), driver_dre30_hist->dre_chroma_hist, sizeof(mAALInput->DREChromaHist));
                memcpy(&(mAALInput->DREFlatLineCountSet), driver_dre30_hist->dre_flat_line_count_set, sizeof(mAALInput->DREFlatLineCountSet));
                memcpy(&(mAALInput->DRELargeDiffCountSet), driver_dre30_hist->dre_large_diff_count_set, sizeof(mAALInput->DRELargeDiffCountSet));
                memcpy(&(mAALInput->DREMaxDiffSet), driver_dre30_hist->dre_max_diff_set, sizeof(mAALInput->DREMaxDiffSet));
                mAALInput->dre_blk_x_num = driver_dre30_hist->dre_blk_x_num;
                mAALInput->dre_blk_y_num = driver_dre30_hist->dre_blk_y_num;
            }
#endif

            if (mDebug.overwriteRefreshLatency > 0) {
                mAALOutput->refreshLatency = (AALRefreshLatency)mDebug.overwriteRefreshLatency;
            } else {
                mAALOutput->refreshLatency = eRefresh_33ms;
            }

            debugDump(mDebugLevel & eDebugInput);

            mAALInterface->onCalculate(*mAALInput, mAALOutput);

            debugDump(mDebugLevel & eDebugOutput);

            if (mAALInterface->isAALFwBypassed() != true)
                clearEvents();

            if (mAALOutput->activeTrigger) {
                setEvent(eEvtActiveTrigger);
                enableAALEvent(true);
            } else {
                if (!mToEnableEvent) {
                    /* Will be triggered by overlay dirty or new event */
                    enableAALEvent(false);
                }
            }
        }

        if (mBacklight <= 0) {
            // We don't turn on backlight until PMS turn it on
            mAALOutput->FinalBacklight = 0;
        }

        unitTest();

        // Ignore following fields in dirty check
        prevOutput->activeTrigger = mAALOutput->activeTrigger;
        prevOutput->refreshLatency = mAALOutput->refreshLatency;

        if (mAALOutput->activeTrigger || // force screen refresh
            (driver_hist->serviceFlags & AAL_SERVICE_FORCE_UPDATE) || // update request from driver
            memcmp(prevOutput, mAALOutput, sizeof(AALOutput)) != 0) // Write registers only if dirty
        {
            memcpy(driver_param->DREGainFltStatus, mAALOutput->DREGainFltStatus, sizeof(driver_param->DREGainFltStatus));
            driver_param->cabc_fltgain_force = mAALOutput->cabc_fltgain_force;
            memcpy(driver_param->cabc_gainlmt, mAALOutput->cabc_gainlmt, sizeof(driver_param->cabc_gainlmt));
            driver_param->FinalBacklight = mAALOutput->FinalBacklight;
#ifdef HAS_DRE30
            if (m_dre_mode == eDRE30) {
                memcpy(driver_dre30_param->dre3_gain, mAALOutput->DRECurveSet, sizeof(driver_dre30_param->dre3_gain));
            }
#endif
            if (mDebug.overwritePartial >= 0) // allow partial by debug flag
                driver_param->allowPartial = mDebug.overwritePartial;
            else // allow partial by algorithm
                driver_param->allowPartial = (mAALOutput->allowPartialUpdate == true) ? 1 : 0;

            if (mDebugLevel & eDebugPartial)
                AAL_LOGD("ALG allow partial=%d", mAALOutput->allowPartialUpdate);

            if (mAALOutput->refreshLatency == eRefresh_33ms)
                driver_param->refreshLatency = AAL_REFRESH_33MS;
            else
                driver_param->refreshLatency = AAL_REFRESH_17MS;

            if (mDebugLevel & eDebugTime) AAL_LOGD("DISP_IOCTL_AAL_SET_PARAM writting ...");

            if ((ret = ioctl(mDispFd, DISP_IOCTL_AAL_SET_PARAM, driver_param)) != 0) {
                AAL_LOGE("DISP_IOCTL_AAL_SET_PARAM error: %d", ret);
            }

            if (mDebugLevel & eDebugTime) AAL_LOGD("DISP_IOCTL_AAL_SET_PARAM done ...");

            // We also write the value to LED node so that application can
            // obtain latest value from the node
            if (mAALOutput->FinalBacklight != mOutBacklight || mAALOutput->cabc_fltgain_force != mOutCabcGain) {
                mOutBacklight = mAALOutput->FinalBacklight;
                mOutCabcGain = mAALOutput->cabc_fltgain_force;
                if (mDebugLevel & eDebugBasic)
                    AAL_LOGD("Output backlight = %d/1023, ESS gain = %d", mOutBacklight, mOutCabcGain);
                else {
                    snprintf(logbuffer, LOGITEMSIZE, "BL=%4d,ESS=%4d",
                        mOutBacklight, mOutCabcGain);
                    addLogToBuffer(logbuffer);
                }
            }
        }
    }

    mCustFunc->deinit();

    delete prevOutput;
    delete buffOutput;
#ifdef HAS_DRE30
    if (driver_dre30_param != NULL) {
        delete driver_dre30_param;
    }
    if (driver_dre30_hist != NULL) {
        delete driver_dre30_hist;
    }
#endif
    //if (DREImpl != NULL && destroyDRE30 != NULL) {
    //    destroyDRE30(DREImpl);
    //}
    delete driver_hist;
    delete driver_param;

    return true;
}


void AALService::unitTest()
{
#ifdef AAL_UT_FROM_PROPERTY
    char property[PROPERTY_VALUE_MAX];
    int value;

    if (property_get(AAL_PROPERTY_UNIT_TEST, property, NULL) > 0 &&
            strlen(property) > 0)
    {
        AAL_LOGD("AAL unit test: %s", property);

        switch (property[0]) {
        case 'f':
            if (property[1] != ' ') goto reset_property;
            value = atoi(&property[2]);
            setFunction((uint32_t)value);
            break;

        case 'd':
            if (property[1] != ' ') goto reset_property;
            value = atoi(&property[2]);
            debugDump((unsigned int)value);
            break;

        case 'D':
            if (property[1] != ' ') goto reset_property;
            value = atoi(&property[2]);
            mDebugLevel = value;
            break;
        }
reset_property:
        property_set(AAL_PROPERTY_UNIT_TEST, "");
    }
#endif

    if (mDebug.overwriteDRE >= 0) {
        if (m_dre_mode == eDRE30) {
            int blk_x = mDebug.overwriteDREBlockX, blk_y = mDebug.overwriteDREBlockY;

            AAL_LOGD("[UT] Overwrite block [y, x] = [%d, %d] DRE: %d\n",
                blk_y, blk_x, mDebug.overwriteDRE);
            for (int i = 0; i < DRE30_LUMA_CURVE_NUM; i++) {
                mAALOutput->DRECurveSet[blk_y][blk_x][i] = mDebug.overwriteDRE;
            }
        } else {
            AAL_LOGD("[UT] overwrite DRE: %d", mDebug.overwriteDRE);
            for (int i = 0; i < DRE_POINT_NUM; i++) {
                mAALOutput->DREGainFltStatus[i] = mDebug.overwriteDRE;
            }
        }
    }

    if (mDebug.overwriteCABC >= 0) {
        AAL_LOGD("[UT] overwrite ESS: %d", mDebug.overwriteCABC);
        mAALOutput->cabc_fltgain_force = mDebug.overwriteCABC;
    }
}

};
