/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein is
 * confidential and proprietary to MediaTek Inc. and/or its licensors. Without
 * the prior written permission of MediaTek inc. and/or its licensors, any
 * reproduction, modification, use or disclosure of MediaTek Software, and
 * information contained herein, in whole or in part, shall be strictly
 * prohibited.
 *
 * MediaTek Inc. (C) 2010. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER
 * ON AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL
 * WARRANTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR
 * NONINFRINGEMENT. NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH
 * RESPECT TO THE SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY,
 * INCORPORATED IN, OR SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES
 * TO LOOK ONLY TO SUCH THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO.
 * RECEIVER EXPRESSLY ACKNOWLEDGES THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO
 * OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES CONTAINED IN MEDIATEK
 * SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE
 * RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S
 * ENTIRE AND CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE
 * RELEASED HEREUNDER WILL BE, AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE
 * MEDIATEK SOFTWARE AT ISSUE, OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE
 * CHARGE PAID BY RECEIVER TO MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek
 * Software") have been modified by MediaTek Inc. All revisions are subject to
 * any receiver's applicable license agreements with MediaTek Inc.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *   MJCFramework.cpp
 *
 * Project:
 * --------
 *   MT65xx
 *
 * Description:
 * ------------
 *   MTK MJC Framework
 *
 * Author:
 * -------
 *   Cloud Chou (mtk03000)
 *
 ****************************************************************************/

#include "stdlib.h"
#include "stdio.h"
#include "string.h"

#include <cutils/log.h>
#include <signal.h>
#include "osal_utils.h"

#include "MJCFramework.h"

#include <cutils/properties.h>

#include "MtkOmxVdecEx.h"
#include "MtkOmxMVAMgr.h"
#include <utils/AndroidThreads.h>

#include <media/stagefright/foundation/ADebug.h>
#include <utils/Trace.h>

#include <dlfcn.h>
#include "PerfServiceNative.h"

#include "refresh_rate_control.h"

#include "vdec_drv_if_public.h"

// perfservice +
#define PERF_SERVICE_LIB_FULL_NAME      "/vendor/lib/libperfservicenative.so"
int (*perfServiceNative_userRegScn)(void) = NULL;
void (*perfServiceNative_userUnregScn)(int) = NULL;
int (*perfServiceNative_userGetCapability)(int) = NULL;
void (*perfuserRegScnConfig)(int, int, int, int, int, int) = NULL;
void (*perfUserScnEnable)(int) = NULL;
void (*perfUserScnDisable)(int) = NULL;
void (*perfNotifyDispType)(int) = NULL;


typedef int (*userregscn)(void);
typedef void (*userunregscn)(int);
typedef void (*regscnconfig)(int handle, int cmd, int param_1, int param_2, int param_3, int param_4);
typedef int (*userGetCapability)(int);
typedef void (*userscnena)(int);
typedef void (*userscndisa)(int);

// perfservice -

#undef LOG_TAG
#define LOG_TAG "MJCFramework"

#define ATRACE_TAG ATRACE_TAG_VIDEO

#define MTK_LOG_ENABLE 1
#include <cutils/log.h>

#define MJC_LOGV ALOGV

#define MJC_FW_DEBUG
#ifdef MJC_FW_DEBUG
#define MJC_LOGD(fmt, arg...)    \
if (this->mMJCFrameworktagLog) \
{  \
    ALOGD(fmt, ##arg); \
}
#else
#define MJC_LOGD
#endif

#define MJC_FW_INFO
#ifdef MJC_FW_INFO
#define MJC_LOGI(fmt, arg...)    \
if (this->mMJCFrameworktagLog) \
{  \
    ALOGI(fmt, ##arg); \
}
#else
#define MJC_LOGI
#endif

#define MJC_LOGE ALOGE

#define SUPPORT_CLEARMOTION_PATH0 "/storage/sdcard0/SUPPORT_CLEARMOTION"
#define SUPPORT_CLEARMOTION_PATH1 "/storage/sdcard1/SUPPORT_CLEARMOTION"
#define TUNING_DATA_NUM sizeof(short)*2
#define MAX_LINE_LEN 256
#define MJC_CFG_STR_LEN 30
#define DELIMITERS " \t\r\n"

#undef WAIT_T
#define WAIT_T(X) \
    if (0 != WAIT_TIMEOUT(X,LOCK_TIMEOUT_S)){ \
        MJC_LOGI("[%d][%d] RQ: %d, OQ: %d, MJC Q: %d, Vdo Q: %d", pMJC->mState, pMJC->mMode, pMJC->mRefBufQ.size(), pMJC->mOutputBufQ.size(), pVdec->mMJCPPBufCount, pVdec->mMJCVdoBufCount); \
        MJC_LOGI("## [ERROR] %s() line: %d WAIT mMJCFrameworkSem timeout...", __FUNCTION__,__LINE__); \
        WAIT(X);\
    }


typedef struct _mjc_cfg_type {
    unsigned char name[MJC_CFG_STR_LEN];
    unsigned char value[MJC_CFG_STR_LEN];
} mjc_cfg_type;


static int64_t getTickCountMs() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)(tv.tv_sec * 1000LL + tv.tv_usec / 1000);
}

#define CHECK_MJC_INSTANCE(pInstance) \
    if (NULL == pInstance) \
    {\
        ALOGD("%s, 0x%08X", __FUNCTION__, (unsigned int)pInstance);\
        return MJC_ErrorBadParameter;\
    }

#define LOAD_MJC_DRV_FUNC(lib, func_type, func_name, func_ptr)\
    if (NULL == func_ptr)\
    {\
        func_ptr = (func_type) dlsym(lib, func_name);\
        if (NULL == func_ptr)\
        {\
            ALOGD("[MJC_DRV] Can't load %s", func_name);\
            goto LOAD_DRV_FUNC_FAIL;\
        }\
    }

/*****************************************************************************
 * FUNCTION
 *    MtkMJCThread
 * DESCRIPTION
 *    MJC main thread
 *    1. Collect drv init data (get input frame, estimage frame rate, ...)
 *    2. Call drv init
 *    3. handle data flow (Normal, Bypass, Flush mode)
 *    4. Prepare input and output buffer in Normal mode
 *    5. Call drv process in Normal mode
 *    6. Handle MJC reference frame in Normal mode
 *    7. Generate interploated frame timestamp
 *    8. Send real and interploated frame to display in Normal mode
 * PARAMETERS
 *    param1 : [IN]  void* pData
 *                Pointer of thread data.  (MJC object handle and omx component handle)
 * RETURNS
 *    Always return NULL.
 ****************************************************************************/
void *MtkMJCThread(void *pData) {
    MJC_ThreadParam *pParam = (MJC_ThreadParam *)pData;
    MtkOmxVdec *pVdec = (MtkOmxVdec *)(pParam->pUser);
    MJC *pMJC = (MJC *)(pParam->pMJC);

    ALOGD("MtkMJCThread created pVdec=0x%08X, pMJC=0x%08X, tid=%d", (unsigned int)pVdec, (unsigned int)pMJC, gettid());
    prctl(PR_SET_NAME, (unsigned long)"MtkMJCThread", 0, 0, 0);
    pMJC->mThreadTid = gettid();

    androidSetThreadPriority(0, ANDROID_PRIORITY_URGENT_DISPLAY + 1);

    int64_t in_time_1, out_time_1;

    while (1) {
        if (true == pMJC->mTerminated) {
            ALOGD("[%d][%d] exit MtkMJCThread! check mIsComponentAlive: %d", pMJC->mState, pMJC->mMode, pVdec->mIsComponentAlive);
            break;
        }

        if (false == pMJC->PrepareBuffer_CheckFrameRate(pData)) {
            return NULL;
        }

        if (MJC_STATE_INIT == pMJC->mState) {
            // Check init param
            pMJC->CheckInitCondition(pData);
            ALOGD("[%d][%d] -CheckInitCondition", pMJC->mState, pMJC->mMode);
            // Init dump YUV settings
            pMJC->InitDumpYUV(pData);
            // Init MJC driver
            pMJC->InitDriver(pData);
            ALOGD("[%d][%d] -InitDriver", pMJC->mState, pMJC->mMode);
            // From INIT-->READY
            pMJC->mState = MJC_STATE_READY;

        } // (MJC_STATE_INIT == pMJC->mState)


        if (pMJC->mState != MJC_STATE_FLUSH) {
            //ALOGD("[%d][%d] RQ: %d, OQ: %d, MJC Q: %d, Vdo Q: %d", pMJC->mState, pMJC->mMode, pMJC->mRefBufQ.size(), pMJC->mOutputBufQ.size(), pVdec->mMJCPPBufCount, pVdec->mMJCVdoBufCount);
            //ALOGD("[%d][%d] (i:%d,o:%d)", pMJC->mState, pMJC->mMode,  pMJC->mRefBufQ.size(),  pMJC->mOutputBufQ.size());
            in_time_1 = getTickCountMs();
            WAIT(pMJC->mMJCFrameworkSem);

            out_time_1 = getTickCountMs() - in_time_1;
            if (pMJC->mMJCLog) {
                if (out_time_1 > 30) {
                    ALOGD("[%d][%d] MJC(%lld)", pMJC->mState, pMJC->mMode, out_time_1);
                }
            }
        }

        switch (pMJC->mMode) {
        case MJC_MODE_BYPASS: {
            pMJC->BypassModeRoutine(pData);
        }
        break;

        case MJC_MODE_NORMAL: {
            pMJC->NormalModeRoutine(pData);
        }
        break;

        default: {
            pMJC->DefaultRoutine(pData);
        }
        break;
        }
        //SLEEP_MS(100);

    }


    // Disable scenario
    pMJC->DisablePerfService();


    ALOGD("MtkMJCThread terminated, %d, pVdec=0x%08X", __LINE__, (unsigned int)pVdec);
    return NULL;

}

#define DFRTOLERANCE (1000000/120)

MJC_INT_BUF_INFO *MJC::MJCGetBufInfoFromOMXHdr(void *pVdec, OMX_BUFFERHEADERTYPE *pBufHdr) {
    MJC_INT_BUF_INFO *mMJCBufInfo = NULL;
    /*
    unsigned int u4VA = (unsigned int)mCallbacks.pfnGetBufVA((MtkOmxVdec *)pVdec, pBufHdr);
    unsigned int u4MVA = (unsigned int)mCallbacks.pfnGetBufPA((MtkOmxVdec *)pVdec, pBufHdr);
    unsigned int u4GraphicBufHandle = (unsigned int)mCallbacks.pfnGetBufGraphicHndl((MtkOmxVdec *)pVdec, pBufHdr);
    unsigned int u4BufferHandle = (unsigned int)mCallbacks.pfnGetBuffferHndl((MtkOmxVdec *)pVdec, pBufHdr);
    */
    MJC_INT_BUF_INFO queryBufInfo = mCallbacks.pfnGetBufInfo((MtkOmxVdec *)pVdec, pBufHdr);

    VAL_UINT32_T u4I = 0, u4SrchCnt = 0;
    bool bFound = false;

    LOCK(mMJCBufInfoQLock);

    if (mMJCBufInfoNum == MAX_TOTAL_BUFFER_CNT) {
        MJC_LOGE("[MJCGetBufInfoFromOMXHdr] mMJCBufInfoQ is full!");
        CHECK(0);
    }

    //
    for (; u4I < MAX_TOTAL_BUFFER_CNT; u4I++) {
        if (u4SrchCnt == mMJCBufInfoNum) {
            break;
        }

        if (mMJCBufInfoQ[u4I].u4BufferHandle != 0xffffffff) {
            u4SrchCnt++;
            if (mMJCBufInfoQ[u4I].u4BufferHandle == queryBufInfo.u4BufferHandle) {
                // never happen?
                // VA exists update buffer header
                MJC_LOGD("[%d][%d] [MJCGetBufInfoFromOMXHdr] BufferHandle [0x%08X] exists! BHdr: (0x%08X), VA: 0x%08X, ts = %lld, len = %d, idx = %d", mState, mMode,
                         queryBufInfo.u4BufferHandle ,(unsigned int)pBufHdr, (unsigned int)queryBufInfo.u4VA, pBufHdr->nTimeStamp, pBufHdr->nFilledLen, u4I);
                mMJCBufInfo = &mMJCBufInfoQ[u4I];
                mMJCBufInfo->pBufHdr = pBufHdr;
                bFound = true;
                break;
            }
        }
    }

    if (bFound == false) {
        for (u4I = 0; u4I < MAX_TOTAL_BUFFER_CNT; u4I++) {
            if (mMJCBufInfoQ[u4I].u4BufferHandle == 0xffffffff) {
                if (mMJCLog) {
                    MJC_LOGD("[%d][%d] [MJCGetBufInfoFromOMXHdr] BufferHandle [0x%08X] does not exist! Insert internal buf! BHdr: (0x%08X), VA: 0x%08X, ts = %lld, len = %d, idx = %d", mState, mMode,
                             queryBufInfo.u4BufferHandle, (unsigned int)pBufHdr, (unsigned int)queryBufInfo.u4VA, pBufHdr->nTimeStamp, pBufHdr->nFilledLen, u4I);
                }
                mMJCBufInfo = &mMJCBufInfoQ[u4I];
                mMJCBufInfo->pBufHdr = pBufHdr;
                mMJCBufInfo->nTimeStamp = pBufHdr->nTimeStamp;
                mMJCBufInfo->nFilledLen = pBufHdr->nFilledLen;
                mMJCBufInfo->nFlags = pBufHdr->nFlags;
                mMJCBufInfo->u4MVA = queryBufInfo.u4MVA;
                mMJCBufInfo->u4VA = queryBufInfo.u4VA;
                mMJCBufInfo->u4BufferHandle = queryBufInfo.u4BufferHandle;
                mMJCBufInfo->u4GraphicBufHandle = queryBufInfo.u4GraphicBufHandle;
                mMJCBufInfo->u4Idx = u4I;
                mMJCBufInfo->mIsFilledBufferDone = false;
                mMJCBufInfoNum++;
                break;
            }
        }
    }

    //MJC_LOGD("@@+ mMJCBufInfoNum(%d)", mMJCBufInfoNum);
    UNLOCK(mMJCBufInfoQLock);

    return mMJCBufInfo;
}

void MJC::MJCSetBufRef(void *pVdec, MJC_INT_BUF_INFO *pMJCBufInfo) {
    if (mMJCLog) {
        MJC_LOGD("[%d][%d] MJCSetBufRef BHdr: (0x%08X) BufferHandle: (0x%08X) VA: (0x%08X), ts = %lld, Idx = %d",
                 mState, mMode, (unsigned int)pMJCBufInfo->pBufHdr, (unsigned int)pMJCBufInfo->u4BufferHandle, (unsigned int)pMJCBufInfo->u4VA, pMJCBufInfo->nTimeStamp, pMJCBufInfo->u4Idx);
    }

    mRefBufQ.add(pMJCBufInfo);
    mRefBufQTS.add(pMJCBufInfo->nTimeStamp);
    mCallbacks.pfnSetRef(pVdec, pMJCBufInfo);
}

void MJC::MJCClearBufRef(void *pVdec, MJC_INT_BUF_INFO *pMJCBufInfo) {
    MJC_INT_BUF_INFO pBufInfo;

    LOCK(mMJCBufInfoQLock);

    if (mMJCLog) {
        MJC_LOGD("[%d][%d] MJCClearBufRef BHdr: (0x%08X) BufferHandle: (0x%08X), VA: (0x%08X), ts = %lld, Idx = %d",
                 mState, mMode, (unsigned int)pMJCBufInfo->pBufHdr, (unsigned int)pMJCBufInfo->u4BufferHandle, (unsigned int)pMJCBufInfo->u4VA,pMJCBufInfo->nTimeStamp, pMJCBufInfo->u4Idx);
    }

    // To avoid race condition. Internal Q must be cleared before DecRef
    pBufInfo.pBufHdr = pMJCBufInfo->pBufHdr;
    pBufInfo.u4VA = pMJCBufInfo->u4VA;
    pBufInfo.u4MVA = pMJCBufInfo->u4MVA;
    pBufInfo.u4BufferHandle = pMJCBufInfo->u4BufferHandle;
    pBufInfo.nFilledLen = pMJCBufInfo->nFilledLen;
    pBufInfo.nTimeStamp = pMJCBufInfo->nTimeStamp;
    pBufInfo.nFlags = pMJCBufInfo->nFlags;

    pMJCBufInfo->pBufHdr = NULL;
    pMJCBufInfo->u4VA = 0xffffffff;
    pMJCBufInfo->u4MVA = 0xffffffff;
    pMJCBufInfo->u4BufferHandle = 0xffffffff;
    pMJCBufInfo->nFilledLen = 0;
    pMJCBufInfo->nTimeStamp = 0;
    pMJCBufInfo->nFlags = 0;
    mMJCBufInfoNum--;
    //MJC_LOGD("@@- mMJCBufInfoNum(%d)", mMJCBufInfoNum);

    UNLOCK(mMJCBufInfoQLock);

    mCallbacks.pfnClearRef(pVdec, &pBufInfo);
    mRefBufQ.removeItemsAt(0);
    mRefBufQTS.removeItemsAt(0);


}
void MJC::EnablePerfService() {

    char value[PROPERTY_VALUE_MAX];
    VAL_INT32_T minfreq = 0;

    void *perf_handle = NULL;
    void *perf_func = NULL;

    if (-1 != mPerfRegHandle) {
        MJC_LOGD("Perf Service has already been enabled.");
        return;
    }

    // open lib
    perf_handle = dlopen(PERF_SERVICE_LIB_FULL_NAME, RTLD_NOW);
    if (perf_handle == NULL) {
        MJC_LOGE("@@ [ERROR] perf dlopen failed, %s @ enable", dlerror());
        mPerfRegHandle = -1; // fail
        return;
    }

    // Register scenario
    perf_func = dlsym(perf_handle, "PerfServiceNative_userRegScn");
    if (NULL != perf_func) {
        perfServiceNative_userRegScn = reinterpret_cast<userregscn>(perf_func);
        mPerfRegHandle = perfServiceNative_userRegScn();
        MJC_LOGI("PerfRegHandle = %d", mPerfRegHandle);
        perf_func = NULL;
    } else {
        MJC_LOGI("can't get userRegScn");
        goto PERF_SERVICE_EXIT;
    }

    unsigned int PERF_FIX_CORE_NUM;
    unsigned int PERF_MIN_FREQ;

    property_get("mtk.mjc.minfreq", value, "0");
    minfreq = (VAL_INT32_T) atoi(value);

    if (cfgParam.u4OutFrmRate == 1200) {
        if (mu4ChipName == VAL_CHIP_NAME_MT6795) {
            PERF_FIX_CORE_NUM = 4;
            PERF_MIN_FREQ = 1183000;
        } else if (mu4ChipName == VAL_CHIP_NAME_MT6797) {
            PERF_FIX_CORE_NUM = 4;
            PERF_MIN_FREQ = 897000;
        } else {
            PERF_FIX_CORE_NUM = 4;
            PERF_MIN_FREQ = 1183000;
        }
    } else {
        if (mu4ChipName == VAL_CHIP_NAME_MT6795) {
            PERF_FIX_CORE_NUM = 2;
            PERF_MIN_FREQ = 806000;
        } else if (mu4ChipName == VAL_CHIP_NAME_MT6797) {
            PERF_FIX_CORE_NUM = 2;
            PERF_MIN_FREQ = 624000;
        } else {
            PERF_FIX_CORE_NUM = 4;
            PERF_MIN_FREQ = 806000;
        }
    }

    if (0 != minfreq) {
        PERF_MIN_FREQ = minfreq;
    }

    // Set requirement
    perf_func = dlsym(perf_handle, "PerfServiceNative_userRegScnConfig");
    if (NULL != perf_func) {
        perfuserRegScnConfig = reinterpret_cast<regscnconfig>(perf_func);
        perfuserRegScnConfig(mPerfRegHandle, CMD_SET_CPU_CORE_MIN, PERF_FIX_CORE_NUM, 0, 0, 0);
        perfuserRegScnConfig(mPerfRegHandle, CMD_SET_CPU_FREQ_MIN, PERF_MIN_FREQ, 0, 0, 0);
        perf_func = NULL;
    } else {
        MJC_LOGI("can't get regscnconfig");
        goto PERF_SERVICE_EXIT;
    }

    // Enable scenario
    perf_func = dlsym(perf_handle, "PerfServiceNative_userEnable");
    if (NULL != perf_func) {
        perfUserScnEnable = reinterpret_cast<userscnena>(perf_func);
        perfUserScnEnable(mPerfRegHandle);
        perf_func = NULL;
        MJC_LOGD("EnablePerfService. Core: %d, Freq: %d", PERF_FIX_CORE_NUM, PERF_MIN_FREQ);
    } else {
        MJC_LOGI("can't get userEnable");
        goto PERF_SERVICE_EXIT;
    }

    dlclose(perf_handle);
    return; // success

PERF_SERVICE_EXIT:
    mPerfRegHandle = -1; // fail
    dlclose(perf_handle);

}

void MJC::DisablePerfService() {
    void *perf_handle = NULL;
    void *perf_func = NULL;

    if (-1 == mPerfRegHandle) {
        //MJC_LOGD("Perf Service has NOT been enabled");
        return;
    }

    // open lib
    perf_handle = dlopen(PERF_SERVICE_LIB_FULL_NAME, RTLD_NOW);
    if (perf_handle == NULL) {
        MJC_LOGE("@@ [ERROR] perf dlopen failed, %s @ disable", dlerror());
        return;
    }


    perf_func = dlsym(perf_handle, "PerfServiceNative_userDisable");
    if (NULL != perf_func) {
        perfUserScnDisable = reinterpret_cast<userscndisa>(perf_func);
        perfUserScnDisable(mPerfRegHandle);
        perf_func = NULL;
        MJC_LOGD("DisablePerfService");
    } else {
        MJC_LOGI("can't get userDisable");
    }

    // Un-Register scenario
    perf_func = dlsym(perf_handle, "PerfServiceNative_userUnregScn");
    if (NULL != perf_func) {
        perfServiceNative_userUnregScn = reinterpret_cast<userunregscn>(perf_func);
        perfServiceNative_userUnregScn(mPerfRegHandle);
        MJC_LOGI("PerfRegHandle = %d", mPerfRegHandle);
        perf_func = NULL;
    } else {
        MJC_LOGI("can't get userUnRegscn");
    }


    mPerfRegHandle = -1;
    dlclose(perf_handle);
}

/*****************************************************************************
 * FUNCTION
 *    MJC::AtomicChangeMode
 * DESCRIPTION
 *    1. Modify the mode in a atomic operation
 * PARAMETERS
 *    None
 * RETURNS
 *    Type: unsigned int. 0 mean fail and others mean success.
 ****************************************************************************/
bool MJC::AtomicChangeMode(MJC_MODE_OPERATION eOP, MJC_MODE eNewMode) {
    bool bRet = true;
    if (mMJCLog) {
        MJC_LOGI("AtomicChangeMode, OP(%d), NewMode(%d), OriMode(%d)", eOP, eNewMode, mMode);
    }

    if (eNewMode == MJC_MODE_BYPASS) {
        DisablePerfService();
        if (mbSupport120Hz == true && mbNotifyRRC == true) {
            ((RefreshRateControl *)pRRC)->setScenario(RRC_TYPE_VIDEO_120HZ, false);
            mbNotifyRRC = false;
            MJC_LOGI("set RRC_TYPE_VIDEO_120HZ false");
        }
    } else if (eNewMode == MJC_MODE_NORMAL) {
        if (mbSupport120Hz == true && cfgParam.u4OutFrmRate == 1200 && mbNotifyRRC == false) {
            ((RefreshRateControl *)pRRC)->setScenario(RRC_TYPE_VIDEO_120HZ, true);
            mbNotifyRRC = true;
            MJC_LOGI("set RRC_TYPE_VIDEO_120HZ true");
        }
    }

    LOCK(mModeLock);

    MJC_MODE OriMode = mMode;

    switch (eOP) {
    case MJC_MODE_SET: {
        mMode = eNewMode;
    }
    break;

    case MJC_MODE_ADD: {
        mMode |= eNewMode;
    }
    break;

    default: {
        MJC_LOGD("Invalid mode operation");
        bRet = false;
    }
    break;
    }

    MJC_LOGI("[%d][%d] Atomic Change Mode %d TO %d => %d", gettid(), mThreadTid, OriMode, eNewMode, mMode);

    UNLOCK(mModeLock);

    return bRet;
}

bool MJC::CheckSupportSlowmotionSpeed(unsigned int u4ToBeCheckSpeed) {
    bool bRet = false;

    switch (mu4ChipName) {
    case VAL_CHIP_NAME_MT6797:
        if (320 == u4ToBeCheckSpeed || 160 == u4ToBeCheckSpeed) {
            MJC_LOGD("[6797] Support 32X/16X Slowmotion");
            bRet = true;
        } else {
            MJC_LOGE("[6797] Incorrect speed:%d", u4ToBeCheckSpeed);
        }
        break;

    case VAL_CHIP_NAME_MT6795:
        if (160 == u4ToBeCheckSpeed) {
            MJC_LOGD("[6795] Support 16X Slowmotion");
            bRet = true;
        } else {
            MJC_LOGE("[6795] Incorrect speed:%d", u4ToBeCheckSpeed);
        }
        break;

    default:
        MJC_LOGV("[%d] Need to check chip speed:%d", mu4ChipName, u4ToBeCheckSpeed);
        break;
    }

    if (false == bRet) {
        MJC_LOGD("[%d] Not Support Speed:%d", mu4ChipName, u4ToBeCheckSpeed);
    }

    return bRet;
}

//#ifdef MTK_16X_SLOWMOTION_VIDEO_SUPPORT
/*****************************************************************************
 * FUNCTION
 *    MJC::NeedToForceTrigger
 * DESCRIPTION
 *    1. According the source framerate and the Slowmotion Speed to
 *         decide whether we should force to trigger MJC for interpolation or not.
 * PARAMETERS
 *    None
 * RETURNS
 *    Type: unsigned int.
 *          0: No need for interpolation
 *          others: Suggested interpolcation ratio
 ****************************************************************************/
unsigned int MJC::ForceTriggerRatio(unsigned int u4InFrameRate) {
    if (false == mbSlowMotionMode) {
        return 0;
    }

    unsigned int u4InterpolationRatio = 0;
    unsigned int u4SectionMargin = 500000;
    unsigned int u4ForceTriggerSpeed = 40;

    OMX_S64 i64NewStartTime = 0;

    MJC_INT_BUF_INFO *pMJCBufInfo;
    pMJCBufInfo = mRefBufQ[0];  // Get the original frame

    if (meSlowMotionSection.StartTime >= 0 && meSlowMotionSection.EndTime >= 0 && mu4SlowmotionSpeed > u4ForceTriggerSpeed) { // Todo: should modify 'mu4SlowmotionSpeed > u4ForceTriggerSpeed'
        //OMX_BUFFERHEADERTYPE *pBuffHdr;
        //pBuffHdr = mRefBufQ[0];  // Get the original frame

        // To relief transition judder, we would add some margin.
        if ((meSlowMotionSection.StartTime - u4SectionMargin) > 0) {
            i64NewStartTime = meSlowMotionSection.StartTime - u4SectionMargin;
        }

        if (pMJCBufInfo->nTimeStamp >= (i64NewStartTime) && pMJCBufInfo->nTimeStamp <= (meSlowMotionSection.EndTime + u4SectionMargin)) {
            //MJC_LOGD("SM Section: ts(%lld), start(%lld), end(%lld), Speed(%d)", pBuffHdr->nTimeStamp, meSlowMotionSection.StartTime, meSlowMotionSection.EndTime, mu4SlowmotionSpeed);
            if (true == CheckSupportSlowmotionSpeed(mu4SlowmotionSpeed)) { // Check support speed according to the chip name
                mCheckDFR = true;
                u4InterpolationRatio = 4; // 120-->480. This should use a better calculation.
            } else {
                MJC_LOGD("Not Support Speed: %d", mu4SlowmotionSpeed);
            }
        } else {
            MJC_LOGD("Not in Section. CurrTS(%lld), startTS(%lld), EndTS(%lld)", pMJCBufInfo->nTimeStamp, meSlowMotionSection.StartTime, meSlowMotionSection.EndTime);
        }
    }

    MJC_LOGD("ForceTriggerRatio : ratio(%d), InFR(%d), ts(%lld), start(%lld), end(%lld), i64NewStartTime(%lld), Speed(%d)", u4InterpolationRatio, u4InFrameRate, pMJCBufInfo->nTimeStamp, meSlowMotionSection.StartTime, meSlowMotionSection.EndTime, i64NewStartTime, mu4SlowmotionSpeed);
    //MJC_LOGD("ratio[%d], InFR[%d], FRThre[%d]", u4InterpolationRatio, u4InFrameRate, u4FrameRateThreshold);
    return u4InterpolationRatio;
}
//#endif

/*****************************************************************************
 * FUNCTION
 *    MJC::FrameRateDetect
 * DESCRIPTION
 *    1. According the mRefBufQ to estimate frame rate
 * PARAMETERS
 *    None
 * RETURNS
 *    Type: unsigned int. 0 mean fail and others mean success.
 ****************************************************************************/
unsigned int MJC::FrameRateDetect(bool *fgDFR, unsigned char *pBufDepth, unsigned char *pLowFRHint) {
    MJC_INT_BUF_INFO *pMJCBufInfo;

    unsigned int u4Count =  mRefBufQ.size();
    unsigned int n4Framerate = 0;
    unsigned int u4AvgFrmDuration = 0;
    unsigned int u4MaxDiffDuration = 0;

    if (u4Count <= 1) {
        return n4Framerate;
    }

    if (mRefBufQTS[1] <= mRefBufQTS[0]) {
        MJC_LOGD("ts 0 = %lld > ts 1 = %lld", mRefBufQTS[0], mRefBufQTS[1]);
        return n4Framerate;
    }
    if (mRefBufQTS[2] <= mRefBufQTS[1]) {
        MJC_LOGD("ts 1 = %lld > ts 2 = %lld", mRefBufQTS[0], mRefBufQTS[1]);
        return n4Framerate;
    }

    // use first 2 frames to get a framerate
    // We will use the consecutinve frames to get a dymamic framerate later
    //
    mInFrmDuration = mRefBufQTS[1] - mRefBufQTS[0];

    n4Framerate = 10000000 / mInFrmDuration;
    n4Framerate = (n4Framerate + 5) / 10 * 10;

    if (u4Count < mDetectNum) {
        return n4Framerate;
    }

    u4Count = mDetectNum;

    for (int index = u4Count - 1; index > 0; index--) {
        if (mRefBufQTS[index] == 0) {
            u4Count--;
            MJC_LOGD("[%d] idx %d, ts = 0, cnt = %d", mState, index, u4Count);
        } else {
            break;
        }
    }

    if (u4Count <= 1) {
        return n4Framerate;
    }

    // (Last frame - first frame) / Total frame count ==> Get average frame duration
    u4AvgFrmDuration = (mRefBufQTS[u4Count - 1] - mRefBufQTS[0]) / (u4Count - 1);

    // Go through all ref frames and check each frame durations.
    for (unsigned int index = 0; index < u4Count; index++) {
        pMJCBufInfo = mRefBufQ[index];
        //MJC_LOGD("[%d] RBQ idx %d, 0x%08X, ts = %lld", mState, index, pBuffHdr, pBuffHdr->nTimeStamp);
        if (index >= 1) {
            unsigned int u4FrmDuration = mRefBufQTS[index] - mRefBufQTS[index - 1];
            unsigned int u4diff = 0;
            if (u4FrmDuration > u4AvgFrmDuration) {
                u4diff = u4FrmDuration - u4AvgFrmDuration;
            } else {
                u4diff = u4AvgFrmDuration - u4FrmDuration;
            }
            if (u4diff > u4MaxDiffDuration) {
                u4MaxDiffDuration = u4diff;
            }
        }
    }


    if (mMJCLog) {
        switch (u4Count) {
        case 4:
            MJC_LOGD("[%d] AvgFD = %d, MaxDD = %d, RBQ[0](%lld), RBQ[1](%lld), RBQ[2](%lld), RBQ[3](%lld)",
                     mState,
                     u4AvgFrmDuration,
                     u4MaxDiffDuration,
                     mRefBufQTS[0],
                     mRefBufQTS[1],
                     mRefBufQTS[2],
                     mRefBufQTS[3]);
            break;

        case 3:
            MJC_LOGD("[%d] AvgFD = %d, MaxDD = %d, RBQ[0](%lld), RBQ[1](%lld), RBQ[2](%lld)",
                     mState,
                     u4AvgFrmDuration,
                     u4MaxDiffDuration,
                     mRefBufQTS[0],
                     mRefBufQTS[1],
                     mRefBufQTS[2]);
            break;

        case 2:
            MJC_LOGD("[%d] AvgFD = %d, MaxDD = %d, RBQ[0](%lld), RBQ[1](%lld)",
                     mState,
                     u4AvgFrmDuration,
                     u4MaxDiffDuration,
                     mRefBufQTS[0],
                     mRefBufQTS[1]);
            break;

        default:
            break;
        }
    }

    // If the max frame duration diverges from the average frame duration too much, it should be a dynamic frame rate
    if (u4MaxDiffDuration > DFRTOLERANCE) {
        *fgDFR = true;
    } else {
        *fgDFR = false;
        n4Framerate = 10000000 / u4AvgFrmDuration;
    }

    n4Framerate = (n4Framerate + 5) / 10 * 10;

    if (pBufDepth != NULL) {
        *pBufDepth = mRefBufQ.size();
        for (unsigned int index = 0; index < u4Count; index++) {
            if (index >= 1) {
                unsigned int u4FrmDuration = mRefBufQTS[index] - mRefBufQTS[index - 1];

                if (u4FrmDuration > 100000) { //10fps
                    if (pLowFRHint != NULL) {
                        *pLowFRHint = index;
                    }
                    break;
                }
            }

        }
        MJC_LOGV("[%d] LFR: BufDepth = %d, LFRHint = %d", mState, *pBufDepth, *pLowFRHint);
    }

    return n4Framerate;
}

void MJC::ShowDemoLine(unsigned long pBuffer, int u4Width, int u4Height, MJC_VIDEO_FORMAT eFormat, MJC_DEMO_TYPE eType) {
    int widthblock = u4Width / 16;
    int heightblock = u4Height / 32;
    switch (eFormat) {
    case MJC_FORMAT_BLK:
        if (MJC_DEMO_HORIZONTAL == eType) {
            for (int i = 0; i < widthblock; i++) {
                memset((void *)(pBuffer + widthblock * (heightblock / 2) * 16 * 32 + 16 * 32 * i + 16 * ((u4Height % 16 == 0) ? 16 : 29)), 255, 16);
            }
        } else if (MJC_DEMO_VERTICAL == eType) {
            for (int j = 0; j < heightblock; j++)
                for (int i = 0; i < 32; i++) {
                    memset((void *)(pBuffer + widthblock * 16 * 32 * j + (widthblock / 2) * 16 * 32 + 16 * i), 255, 1);
                }
        }
        break;
    case MJC_FORMAT_LINE:
        if (MJC_DEMO_HORIZONTAL == eType) {
            memset((void *)(pBuffer + u4Width * (u4Height / 2)), 255, u4Width);
        } else if (MJC_DEMO_VERTICAL == eType) {
            for (int i = 0; i < u4Height; i++) {
                memset((void *)(pBuffer + u4Width * i + (u4Width / 2)), 255, 1);
            }
        }
        break;
    default:
        break;
    }
}

void MJC::PDFAndSetMJCBufferFlag(void *pdata, MJC_INT_BUF_INFO *pMJCBufInfo) {
    // Only update video size when MJC scaler is NOT working.
    OMX_BOOL bPreScale, bMJCEnabled, isMJCRefBuf;
    VAL_UINT32_T u4I = 0;
    OMX_BUFFERHEADERTYPE *pBufHdr ;
    GetParameter(MJC_PARAM_PRESCALE, &bPreScale);
    GetParameter(MJC_PARAM_MODE, &bMJCEnabled);

    LOCK(mMJCBufInfoQLock);

    pBufHdr = pMJCBufInfo->pBufHdr;

    if (OMX_TRUE == bPreScale) {
        pMJCBufInfo->pBufHdr->nFlags |= OMX_BUFFERFLAG_NOT_UPDATE_VIDEO_SIZE;
    }

    if (OMX_TRUE == bMJCEnabled) {
        pMJCBufInfo->pBufHdr->nFlags |= OMX_BUFFERFLAG_CLEARMOTION_ENABLED;
    }

    // Remove MJC BufInfo from array if no longer referenced
    isMJCRefBuf = OMX_FALSE;
    for (; u4I < mRefBufQ.size() ; u4I++) {
        if (pMJCBufInfo->u4BufferHandle == mRefBufQ[u4I]->u4BufferHandle) {
            //MJC_LOGV("[%d][%d] Keep Ref in Queue after PutDispFrame (0x%08X) 0x%08X, ts = %lld, len = %d, idx = %d", mState, mMode, pMJCBufInfo->pBufHdr, pMJCBufInfo->u4VA, pMJCBufInfo->nTimeStamp, pMJCBufInfo->nFilledLen,pMJCBufInfo->u4Idx);
            isMJCRefBuf = OMX_TRUE;
            break;
        }
    }

    if (isMJCRefBuf == OMX_FALSE) {
        if (mMJCLog) {
            MJC_LOGD("[%d][%d] internal buffer freed (0x%08X) IonBufHandle: 0x%08X, VA: 0x%08X, ts = %lld, len = %d, idx = %d", mState, mMode,
                     (unsigned int)pMJCBufInfo->pBufHdr, (unsigned int)pMJCBufInfo->u4BufferHandle, (unsigned int)pMJCBufInfo->u4VA, pMJCBufInfo->nTimeStamp,
                     pMJCBufInfo->nFilledLen, pMJCBufInfo->u4Idx);
        }

        pMJCBufInfo->pBufHdr = NULL;
        pMJCBufInfo->u4VA = 0xffffffff;
        pMJCBufInfo->u4MVA = 0xffffffff;
        pMJCBufInfo->u4BufferHandle = 0xffffffff;
        pMJCBufInfo->nFilledLen = 0;
        pMJCBufInfo->nTimeStamp = 0;
        pMJCBufInfo->nFlags = 0;
        pMJCBufInfo->u4Idx = MAX_TOTAL_BUFFER_CNT;
        mMJCBufInfoNum--;
        //MJC_LOGD("@@- mMJCBufInfoNum(%d)", mMJCBufInfoNum);
    }

    pMJCBufInfo->mIsFilledBufferDone = true;
    UNLOCK(mMJCBufInfoQLock);

    mCallbacks.pfnPutDispFrame((MtkOmxVdec *)pdata, pBufHdr); // Just retrun the frame back to VDEC, wait the next round

}

void MJC::PrepareOutputBuffer(void *pData, MJC_BUFFER_STATUS *pBufferStatus) {
    MtkOmxVdec *pVdec = (MtkOmxVdec *)pData;

    OMX_BUFFERHEADERTYPE *pOutBuffHdr;
    MJC_INT_BUF_INFO *pOutMJCBufInfo;
    bool bNNOB = true;

    // + Prepare output buffer
    while (bNNOB) {
        pOutBuffHdr = mCallbacks.pfnGetOutputFrame(pVdec);

        if (NULL == pOutBuffHdr) {
            //MJC_LOGI("[%d] NOB", pMJC->mState);
            if (bNNOB) {
                *pBufferStatus |= MJC_NO_NEW_OUTPUT_BUFFER;
            }
            break;
        } else {
            pOutMJCBufInfo = MJCGetBufInfoFromOMXHdr(pVdec, pOutBuffHdr);

            // TODO: need modify
            // This may happen when MJC gets an output frame from VDEC which is still used by MJC for reference (the origianl frame(Scaled Frame))
            for (unsigned int i = 0; i < mRefBufQ.size(); i++) {
                if (pOutMJCBufInfo->u4BufferHandle == mRefBufQ[i]->u4BufferHandle) {
                    MJC_LOGD("[%d][%d] Keep Ref 0x%08X, 0x%08X", mState, mMode, (unsigned int)pOutMJCBufInfo->pBufHdr, (unsigned int)pOutMJCBufInfo->u4BufferHandle);
                    pOutMJCBufInfo->pBufHdr->nFilledLen = 0;
                    pOutMJCBufInfo->pBufHdr->nTimeStamp = 0;
                    PDFAndSetMJCBufferFlag(pVdec, pOutMJCBufInfo); // Just retrun the frame back to VDEC, wait the next round
                    break;
                }
            }

            if (NULL == pOutMJCBufInfo->pBufHdr) {
                break;
            }

            if (pOutMJCBufInfo->nFlags & OMX_BUFFERFLAG_SCALER_FRAME) {
                mScaler.FillThisBuffer(pOutMJCBufInfo);
            } else {
                mOutputBufQ.add(pOutMJCBufInfo);
            }

            bNNOB = false;
        }
    }
}
void MJC::PrepareInputBuffer(void *pData, MJC_BUFFER_STATUS *pBufferStatus) {
    MtkOmxVdec *pVdec = (MtkOmxVdec *)pData;

    OMX_BUFFERHEADERTYPE *pInBuffHdr;
    MJC_INT_BUF_INFO *pInMJCBufInfo;
    bool bNNIB = true;

    // + Prepare Input buffer
    int InputCount = mInputNumRequire;
    if (true == mCheckDFR) {
        InputCount = (mInputNumRequire > mDetectNum) ? mInputNumRequire : mDetectNum;
        //MJC_LOGD("[%d] CheckDFR. ReqInputCnt %d (%d, %d)", mState, InputCount, mInputNumRequire, mDetectNum);
    }

    while (mEOSReceived == false) {
        pInBuffHdr = mCallbacks.pfnGetInputFrame(pVdec);

        if (pInBuffHdr == NULL) {
            //MJC_LOGI("[%d] NIB", pMJC->mState);
            if (bNNIB) {
                *pBufferStatus |= MJC_NO_NEW_INPUT_BUFFER;
            }
            break;
        } else {
            pInMJCBufInfo = MJCGetBufInfoFromOMXHdr(pVdec, pInBuffHdr);

            if (pInMJCBufInfo->nFlags & OMX_BUFFERFLAG_EOS) {
                MJC_LOGD("[%d][%d] EOS, 0x%08X, ts = %lld", mState, mMode, (unsigned int)pInMJCBufInfo->pBufHdr, pInMJCBufInfo->nTimeStamp);
                mScaler.EmptyThisBuffer(pInMJCBufInfo);
                break;
            } else if (0 == pInMJCBufInfo->nFilledLen) {
                MJC_LOGD("[%d] PDF, 0x%08X, ts = %lld, len = %d", mState, (unsigned int)pInMJCBufInfo->pBufHdr, pInMJCBufInfo->nTimeStamp, pInMJCBufInfo->nFilledLen);
                PDFAndSetMJCBufferFlag(pVdec, pInMJCBufInfo);
                SIGNAL(mScaler.mMJCScalerSem);
            } else {
                mScaler.EmptyThisBuffer(pInMJCBufInfo);
            }
            bNNIB = false;
        }
    }
}
void MJC::PrepareRefBuffer(void *pData, MJC_BUFFER_STATUS *pBufferStatus) {
    MtkOmxVdec *pVdec = (MtkOmxVdec *)pData;

    bool bNNRB = true;

    unsigned int InputCount = mInputNumRequire;
    if (true == mCheckDFR) {
        InputCount = (mInputNumRequire > mDetectNum) ? mInputNumRequire : mDetectNum;
        //MJC_LOGD("[%d] CheckDFR. ReqInputCnt %d (%d, %d)", mState, InputCount, mInputNumRequire, mDetectNum);
    }

    while (1) {
        MJC_INT_BUF_INFO *pMJCRefBufInfo = NULL;
        pMJCRefBufInfo = mScaler.GetScaledBuffer();
        if (NULL != pMJCRefBufInfo) {
            if (pMJCRefBufInfo->nFlags & OMX_BUFFERFLAG_EOS) {
                mEOSReceived = true;
            }
            MJCSetBufRef(pVdec, pMJCRefBufInfo);

            bNNRB = false;
        } else {
            if (mMJCDrainInputBuffer) {
                LOCK(mScaler.mScalerInputDrainedLock);
                if (mScaler.mScalerInputDrained) {
                    // Clear RefQ when drain mode. Scaler should processed all input buffer by now
                    if (mRefBufQ.size() <= InputCount) {
                        //check filledlen != 0 to avoid PDF original buffer that had been shifted before.
                        while (mRefBufQ.size() != 0 && mRefBufQ[0]->nFilledLen != 0) {
                            PDFAndSetMJCBufferFlag(pVdec, mRefBufQ[0]);
                            MJCClearBufRef(pVdec, mRefBufQ[0]);
                        }
                        MJC_LOGI("Signal pVdec->mMJCVdoBufQDrainedSem.");
                        mMJCDrainInputBuffer = false;
                        SIGNAL(pVdec->mMJCVdoBufQDrainedSem);
                    }
                }
                UNLOCK(mScaler.mScalerInputDrainedLock);
            }
            //MJC_LOGI("[%d] NRB", pMJC->mState);
            if (bNNRB) {
                *pBufferStatus |= MJC_NO_NEW_REF_BUFFER;
            }
            break;
        }
    }
}

void MJC::SendToDisplay(void *pData, void *pProcResult, void *pFrame) {
    MtkOmxVdec *pVdec = (MtkOmxVdec *)pData;
    MJC_DRV_NEXT_INFO_T *rProcResult = (MJC_DRV_NEXT_INFO_T *)pProcResult;
    MJC_DRV_FRAME_T *rFrame = (MJC_DRV_FRAME_T *)pFrame;

    char value[PROPERTY_VALUE_MAX];
    int u4Debug = 0, i;

    property_get("mtk.mjc.debug", value, "0");
    u4Debug = atoi(value);

    if (rProcResult->u1OutputOrgFrame == 1) {
        MJC_INT_BUF_INFO *pMJCBufInfo;
        OMX_TICKS pBuffHdrTS;

        if (rProcResult->u1UpdateRefTimeStamp) {
            eOutputShiftedOrg = MJC_NO_REF_SHIFTED;
            pMJCBufInfo = mRefBufQ[0];  // Get the original frame
            pBuffHdrTS = mRefBufQTS[0];  // Get the original frame timestamp
        } else if (mRefBufQ[0]->u4MVA == rProcResult->u4OutputAddress[0]) {
            eOutputShiftedOrg = MJC_REF_SHIFTED_FORWARD;
            //MJC_LOGD("Original Frame is shifted forward");
            pMJCBufInfo = mRefBufQ[0];  // Get the original frame
            pBuffHdrTS = mRefBufQTS[0];  // Get the original frame timestamp
        } else if (mRefBufQ[1]->u4MVA == rProcResult->u4OutputAddress[0]) {
            eOutputShiftedOrg = MJC_REF_SHIFTED_BACKWARD;
            //MJC_LOGD("Original Frame is shifted backward");
            pMJCBufInfo = mRefBufQ[1];  // Get the original frame
            pBuffHdrTS = mRefBufQTS[1];  // Get the original frame timestamp
        } else {
            CHECK(0);
        }

        CHECK(pMJCBufInfo->u4MVA == rProcResult->u4OutputAddress[0]);

        // TODO: 10-bit size
        pMJCBufInfo->pBufHdr->nFilledLen = mBuffer.u4Height * mBuffer.u4Width * 1.5;
        pMJCBufInfo->nFilledLen = pMJCBufInfo->pBufHdr->nFilledLen;

        // Update output Color profile from MJC input each time
        mOutputColorProfile.bColourPrimariesExist = pMJCBufInfo->pBufHdr->bColourPrimariesExist;
        mOutputColorProfile.bVideoRangeExist = pMJCBufInfo->pBufHdr->bVideoRangeExist;
        mOutputColorProfile.u4VideoRange= pMJCBufInfo->pBufHdr->u4VideoRange;
        mOutputColorProfile.eColourPrimaries = pMJCBufInfo->pBufHdr->eColourPrimaries;

        DumpDispFrame(true, pMJCBufInfo->u4VA, pMJCBufInfo->pBufHdr->nTimeStamp, pMJCBufInfo->pBufHdr->nFilledLen);

        if (rProcResult->u1UpdateRefTimeStamp) {
            if (u4Debug != 1) {
                mFrmCount = 0; // Used to calculate timestamp. The original frame should start from 0
            }
            mRefTimestamp = pBuffHdrTS;
            if (mMJCLog) {
                MJC_LOGD("[%d][%d] Update mRefTimestamp: %lld, mLastTimestamp: %lld", mState, mMode, mRefTimestamp, mLastTimestamp);
            }
            if (mLastTimestamp <= mRefTimestamp) {
                MJC_LOGD("[%d][%d] mRefTimestamp: %lld, mLastTimestamp: %lld", mState, mMode, mRefTimestamp, mLastTimestamp);
            }
            CHECK(mLastTimestamp <= mRefTimestamp);
        } else if (eOutputShiftedOrg != MJC_NO_REF_SHIFTED) {
            mFrmCount++;
            pMJCBufInfo->pBufHdr->nTimeStamp = mRefTimestamp + mOutFrmDuration * mFrmCount;
        }

        MJC_LOGD("[%d][%d] PDF %d, 0x%08X, ts = %lld, len = %d, addr1 = 0x%08X, addr2 = 0x%08X", mState, mMode, rProcResult->u1OutputOrgFrame + eOutputShiftedOrg,
                 (unsigned int)pMJCBufInfo->pBufHdr, pMJCBufInfo->pBufHdr->nTimeStamp, pMJCBufInfo->pBufHdr->nFilledLen, (unsigned int)pMJCBufInfo->u4VA, rProcResult->u4OutputAddress[0]);

        mLastTimestamp = pMJCBufInfo->pBufHdr->nTimeStamp;

        if (u4Debug != 1) {
            PDFAndSetMJCBufferFlag(pVdec, pMJCBufInfo);
        }

        pMJCBufInfo->nFilledLen =  0; // for skip no show check

    } else if (rProcResult->u1IntpOutputNum != 0) { // Output non-original frames (MJC intepolated frames)
        if (mbNeedToEnablePerfService == true) {
            EnablePerfService();
        }

        for (i = 0; i < rProcResult->u1IntpOutputNum; i++) {
            MJC_INT_BUF_INFO *pMJCOutBufInfo;
            pMJCOutBufInfo = mOutputBufQ[0];
            if (mApkSetDemoMode == 0) {
                if (cfgParam.u4DemoMode == 1) {
                    ShowDemoLine(pMJCOutBufInfo->u4VA, mDrvConfig.u4Width, mDrvConfig.u4Height, MJC_FORMAT_BLK, MJC_DEMO_VERTICAL);
                } else if (cfgParam.u4DemoMode == 2 || u4Debug == 2) {
                    ShowDemoLine(pMJCOutBufInfo->u4VA, mDrvConfig.u4Width, mDrvConfig.u4Height, MJC_FORMAT_BLK, MJC_DEMO_HORIZONTAL);
                }
            }
            // TODO: need check nFilledLen
            if (rProcResult->u1NonProcessedFrame != 1) {
                pMJCOutBufInfo->pBufHdr->nFilledLen = mBuffer.u4Height * mBuffer.u4Width * 1.5;
            } else {
                pMJCOutBufInfo->pBufHdr->nFilledLen = 0;
            }

            mFrmCount++;
            pMJCOutBufInfo->pBufHdr->nTimeStamp = mRefTimestamp + mOutFrmDuration * mFrmCount;

            // Timestamp reversed!!!
            if (pMJCOutBufInfo->pBufHdr->nTimeStamp <= mLastTimestamp) {
                MJC_LOGD("(disorder) timestamp = %lld, mLastTimestamp = %lld, mRefTimestamp = %lld, mOutFrmDuration = %d, mFrmCount = %d",
                         pMJCOutBufInfo->pBufHdr->nTimeStamp, mLastTimestamp, mRefTimestamp,  mOutFrmDuration, mFrmCount);
                //CHECK(pBuffHdr->nTimeStamp > pMJC->mLastTimestamp);
                // Just return the buffer and ask not to display it.
                pMJCOutBufInfo->pBufHdr->nTimeStamp = 0;
                pMJCOutBufInfo->pBufHdr->nFilledLen = 0;
            } else {
                mLastTimestamp = pMJCOutBufInfo->pBufHdr->nTimeStamp; // Update the last ts only when timestamp behaved normally
            }

            //Apply Color profile to Output buffer header
            pMJCOutBufInfo->pBufHdr->bVideoRangeExist = mOutputColorProfile.bVideoRangeExist;
            pMJCOutBufInfo->pBufHdr->u4VideoRange= mOutputColorProfile.u4VideoRange;
            pMJCOutBufInfo->pBufHdr->bColourPrimariesExist = mOutputColorProfile.bColourPrimariesExist;
            pMJCOutBufInfo->pBufHdr->eColourPrimaries = mOutputColorProfile.eColourPrimaries;

            //-----------------
            MJC_LOGD("[%d][%d] PDF %d, 0x%08X, ts = %lld, len = %d, addr1 = 0x%08X, addr2 = 0x%08X",
                     mState, mMode, rProcResult->u1OutputOrgFrame, (unsigned int)pMJCOutBufInfo->pBufHdr,
                     pMJCOutBufInfo->pBufHdr->nTimeStamp, pMJCOutBufInfo->pBufHdr->nFilledLen, (unsigned int)pMJCOutBufInfo->u4VA, rProcResult->u4OutputAddress[i]);
            CHECK(rFrame->u4OutputAddress[i] == rProcResult->u4OutputAddress[i]);

            DumpDispFrame(false, pMJCOutBufInfo->u4VA, pMJCOutBufInfo->pBufHdr->nTimeStamp, pMJCOutBufInfo->pBufHdr->nFilledLen);

            PDFAndSetMJCBufferFlag(pVdec, pMJCOutBufInfo);

            mOutputBufQ.removeItemsAt(0);
        }
    } else if (rProcResult->u1IntpOutputNum == 0 && rProcResult->u1OutputOrgFrame == 0) {
        MJC_INT_BUF_INFO *pMJCOutBufInfo;
        pMJCOutBufInfo = mOutputBufQ[0];

        DisablePerfService();

        if (rProcResult->u1UpdateRefTimeStamp) {
            mFrmCount = 0;
            mRefTimestamp = mRefBufQTS[0];
            // Update timestamp for dummy buffer
            pMJCOutBufInfo->pBufHdr->nTimeStamp = mRefTimestamp;
            if (mMJCLog) {
                MJC_LOGD("[%d][%d] Original frame has been shifted. Update mRefTimestamp: %lld", mState, mMode, mRefTimestamp);
            }
            SIGNAL(mMJCFrameworkSem); // Output buffer is not returned. Add semaphore for next trigger.
            CHECK(mLastTimestamp <= mRefTimestamp);
        } else {
            mFrmCount++;
            pMJCOutBufInfo->pBufHdr->nTimeStamp = mRefTimestamp + mOutFrmDuration * mFrmCount;

            if (mbUseDummyBuffer) {
                pMJCOutBufInfo->pBufHdr->nFlags |= OMX_BUFFERFLAG_MJC_DUMMY_OUTPUT_BUFFER;
                mLastTimestamp = pMJCOutBufInfo->pBufHdr->nTimeStamp;
                pMJCOutBufInfo->pBufHdr->nFilledLen = mBuffer.u4Height * mBuffer.u4Width * 1.5;
                MJC_LOGD("[%d][%d] PDF dummy(%d), 0x%08X, ts = %lld, len = %d", mState, mMode, mbUseDummyBuffer, (unsigned int)pMJCOutBufInfo->pBufHdr, pMJCOutBufInfo->pBufHdr->nTimeStamp, pMJCOutBufInfo->pBufHdr->nFilledLen);
                PDFAndSetMJCBufferFlag(pVdec, pMJCOutBufInfo);
                mOutputBufQ.removeItemsAt(0);
            }
        }
    }
}
void MJC::SyncDumpFrmNum(void *pFrame) {
    MJC_DRV_FRAME_T *rFrame = (MJC_DRV_FRAME_T *)pFrame;

    if (u4DumpYUV > 0) {
        MJC_INT_BUF_INFO *pMJCBufInfo;
        OMX_TICKS pBuffHdrTS;

        pMJCBufInfo = mRefBufQ[0];  // Get the original frame
        pBuffHdrTS = mRefBufQTS[0];  // Get the original frame timestamp


        if (!mbStartDump) {
            if (pBuffHdrTS / 1000000 >= u4DumpStartTime) {
                mbStartDump = true;
            }
        }

        if (pBuffHdrTS / 1000000 < u4DumpStartTime) {
            MJC_LOGD("DumpYUV ts reversed. ts(%lld)", pBuffHdrTS / 1000000);
        }

        if (u4DumpCount < u4DumpYUV && mbStartDump) {
            rFrame->u4SyncFrameNum = u4DumpCount;
        } else {
            rFrame->u4SyncFrameNum = 0xFFFFFFFF;
        }
    } else {
        rFrame->u4SyncFrameNum = 0xFFFFFFFF;
    }
}
void MJC::DumpDispFrame(bool bOriginalFrame, unsigned long u4VA, OMX_TICKS nTimeStamp, OMX_U32 nFilledLen) {
    //-----------------
    //4 Dump Disp Frame
    //-----------------
    if (u4DumpYUV > 0 && ((u4DumpType == 1 && bOriginalFrame) || (u4DumpType == 0 && !bOriginalFrame) || u4DumpType == 2) && (nTimeStamp / 1000000 >= u4DumpStartTime)) {
        char buf[255];
        sprintf(buf, "/sdcard/mjc_out_%d_%d.yuv", mBuffer.u4Width, mBuffer.u4Height);
        FILE *fp = fopen(buf, "ab");
        if (fp) {
            if (u4DumpCount < u4DumpYUV) {
                fwrite((void *)u4VA, 1, nFilledLen, fp);
                u4DumpCount++;
            }
            fclose(fp);
        }
    }
}

void MJC::SendRefToDispInSeeking(void *pData) {
    MtkOmxVdec *pVdec = (MtkOmxVdec *)pData;

    MJC_INT_BUF_INFO *pMJCBufInfo;
    OMX_TICKS pBuffHdrTS;
    if (true == mEOSReceived) {
        //  Return all Reference frame
        while (mRefBufQ.size() > 0) {
            pMJCBufInfo = mRefBufQ[0];
            pBuffHdrTS = mRefBufQTS[0];

            if (pMJCBufInfo->nFlags & OMX_BUFFERFLAG_EOS) {
                MJC_LOGD("[%d][%d][SEEK] PDF Ref Q (EOS), 0x%08X, ts = %lld, FilledLen = %d", mState, mMode, (unsigned int)pMJCBufInfo->pBufHdr, pBuffHdrTS, pMJCBufInfo->pBufHdr->nFilledLen);
                PDFAndSetMJCBufferFlag(pVdec, pMJCBufInfo);
                MJCClearBufRef(pVdec, pMJCBufInfo);
                CHECK(mRefBufQ.size() == 0 && mRefBufQTS.size() == 0);
                break;
            }

            if (pMJCBufInfo->nFilledLen != 0) {
                MJC_LOGD("[%d][%d][SEEK] PDF Ref Q, 0x%08X, ts = %lld", mState, mMode, (unsigned int)pMJCBufInfo->pBufHdr, pBuffHdrTS);
                PDFAndSetMJCBufferFlag(pVdec, pMJCBufInfo);
            }
            MJCClearBufRef(pVdec, pMJCBufInfo);
        }
    } else {
        while (mRefBufQ.size() > 0) {
            pMJCBufInfo = mRefBufQ[0];
            pBuffHdrTS = mRefBufQTS[0];
            if (pMJCBufInfo->nFilledLen != 0) {
                MJC_LOGD("[%d][%d][SEEK] PDF, 0x%08X, ts = %lld, len = %d, addr = 0x%08X", mState, mMode, (unsigned int)pMJCBufInfo->pBufHdr, pBuffHdrTS, pMJCBufInfo->nFilledLen, (unsigned int)pMJCBufInfo->u4VA);
                PDFAndSetMJCBufferFlag(pVdec, pMJCBufInfo);
            }
            MJCClearBufRef(pVdec, pMJCBufInfo);
        }
    }
}

/*****************************************************************************
 * FUNCTION
 *    MJC::MJC
 * DESCRIPTION
 *    MJC class constructor
 *    1. Init class member
 * PARAMETERS
 *    None
 * RETURNS
 *    None
 ****************************************************************************/
MJC::MJC() {
    mState = MJC_STATE_INIT;
    mMode = MJC_MODE_NORMAL;

    mFormat = MJC_FORMAT_NONE;
    mMJCInputFormat = MJC_FORMAT_NONE;
    m3DType = MJC_3DTYPE_OFF;
    mCodecType = MTK_VDEC_CODEC_ID_INVALID;

    mCheckDFR = false;
    mDFR = false;
    mDetectNum = 4; // must > 0

    mInFrmRate = 0;
    mOutFrmRate = 600;
    mInFrmDuration = 0;
    mOutFrmDuration = 0;

    mInputNumRequire = 0;
    mOutputNumRequire = 0;

    mFrmCount = 0;
    mRefTimestamp = 0;
    mLastTimestamp = 0;
    mRunTimeDisable = 0;

    mSeek = false;
    mEOSReceived = false;

    mDrv = 0;
    mDrvCreate = false;
    mDrvInit = false;

    //#ifdef MTK_16X_SLOWMOTION_VIDEO_SUPPORT
    mbSlowMotionMode = false;
    mu4SlowmotionSpeed = 10; // Speed*10
    meSlowMotionSection.StartTime = 0;
    meSlowMotionSection.EndTime   = 0;
    //#endif
    mDrvConfig.u4Height = 0;
    mDrvConfig.u4Width = 0;

    memset((void *)&mAlignment, 0, sizeof(MJC_VIDEORESOLUTION));
    memset((void *)&mCropInfo, 0, sizeof(MJC_VIDEOCROP));

    mTerminated = false;
    mUseScaler = false;
    mIsHDRVideo = false;
    INIT_SEMAPHORE(mMJCFrameworkSem);
    INIT_MUTEX(mModeLock);
    INIT_MUTEX(mMJCBufInfoQLock);

    mThreadTid = 0;

    mPerfRegHandle = -1;
    mbNeedToEnablePerfService = false;

    mbSupport120Hz = false;
    pRRC = NULL;
    mbNotifyRRC = false;

    mbUseDummyBuffer = true;

    eOutputShiftedOrg = MJC_NO_REF_SHIFTED;

    LOCK(mMJCBufInfoQLock);

    for (VAL_UINT32_T u4i = 0; u4i < MAX_TOTAL_BUFFER_CNT; u4i++) {
        mMJCBufInfoQ[u4i].pBufHdr = NULL;
        mMJCBufInfoQ[u4i].u4VA = 0xffffffff;
        mMJCBufInfoQ[u4i].u4MVA = 0xffffffff;
        mMJCBufInfoQ[u4i].u4BufferHandle = 0xffffffff;
        mMJCBufInfoQ[u4i].nFilledLen = 0;
        mMJCBufInfoQ[u4i].nTimeStamp = 0;
        mMJCBufInfoQ[u4i].nFlags = 0;
        mMJCBufInfoQ[u4i].u4Idx = 0;
        mMJCBufInfoQ[u4i].mIsFilledBufferDone = false;
    }

    UNLOCK(mMJCBufInfoQLock);

    mMJCBufInfoNum = 0;

    char BuildType[PROPERTY_VALUE_MAX];
    property_get("ro.build.type", BuildType, "eng");
    if (!strcmp(BuildType,"user")){
        mMJCFrameworktagLog = false;
    } else {
        mMJCFrameworktagLog = true;
    }

    char mMJCValue[PROPERTY_VALUE_MAX];
    property_get("mtk.omxvdec.mjc.log", mMJCValue, "0");
    mMJCLog = (bool) atoi(mMJCValue);
    if(mMJCLog)
    {
        mMJCFrameworktagLog = true;
    }

    ALOGD("mMJCFrameworktagLog %d, mMJCLog %d", mMJCFrameworktagLog, mMJCLog);

    mpMTK_MJC_Lib       = NULL;
    mfn_eMjcDrvCreate   = NULL;
    mfn_eMjcDrvRelease  = NULL;
    mfn_eMjcDrvInit     = NULL;
    mfn_eMjcDrvDeInit   = NULL;
    mfn_eMjcDrvProcess  = NULL;
    mfn_eMjcDrvReset    = NULL;
    mfn_eMjcDrvGetParam = NULL;
    mfn_eMjcDrvSetParam = NULL;

    mMJCDrainInputBuffer = false;

    mOutputColorProfile.bColourPrimariesExist = OMX_FALSE;
    mOutputColorProfile.bVideoRangeExist = OMX_FALSE;
    mOutputColorProfile.u4VideoRange= 0;
    mOutputColorProfile.eColourPrimaries = OMX_COLOR_PRIMARIES_BT709; // MJC output is larger than 720P but no greater than 1080P
}

/*****************************************************************************
 * FUNCTION
 *    MJC::~MJC
 * DESCRIPTION
 *    MJC class deconstructor
 *    1. Call drv release
 * PARAMETERS
 *    None
 * RETURNS
 *    None
 ****************************************************************************/
MJC::~MJC() {

    if (0 != mDrv && true == mDrvCreate) {
        MJC_DRV_RESULT_T ret;

        MJC_LOGI("[0x%08X] Drv Release, 0x%08X", (unsigned int)this, mDrv);
        ret = mfn_eMjcDrvRelease(mDrv);

        if (MJC_DRV_RESULT_OK != ret) {
            MJC_LOGE("[0x%08X] Drv Release fail, 0x%08X", (unsigned int)this, mDrv);
        }
        CHECK(MJC_DRV_RESULT_OK == ret);

        UnLoadMJCDrvFunc();

        mDrvCreate = false;

        DisablePerfService();
    }

    DESTROY_SEMAPHORE(mMJCFrameworkSem);
    DESTROY_MUTEX(mModeLock);
    DESTROY_MUTEX(mMJCBufInfoQLock);

}

/*****************************************************************************
 * FUNCTION
 *    MJCCreateInstance
 * DESCRIPTION
 *    1. Create MJC Instance
 * PARAMETERS
 *    param1 : [IN]  unsigned int *pu4MJCInstance
 *                Pointer to create MJC Instance
 * RETURNS
 *    Type: MJC_ERRORTYPE. MJC_ErrorNone means success and MJC_ErrorInsufficientResources means fail.
 ****************************************************************************/

MJC_ERRORTYPE MJCCreateInstance(MJC **ppMJCInstance) {
    MJC *pAllocatedMJCInstance = NULL;

    // 1. allocate Instance
    pAllocatedMJCInstance = new MJC;

    if (NULL == pAllocatedMJCInstance) {
        return MJC_ErrorInsufficientResources;
    }

    *ppMJCInstance = pAllocatedMJCInstance;

    ALOGD("MJCCreateInstance(0x%08X)", (unsigned int)*ppMJCInstance);
    return MJC_ErrorNone;
}

/*****************************************************************************
 * FUNCTION
 *    MJCDestroyInstance
 * DESCRIPTION
 *    1. DestroyInstance MJC Instance
 * PARAMETERS
 *    param1 : [IN]  unsigned int *pu4MJCInstance
 *                MJC Instance to be destroyed
 * RETURNS
 *    Type: MJC_ERRORTYPE. MJC_ErrorNone means success and MJC_ErrorInsufficientResources means fail.
 ****************************************************************************/
MJC_ERRORTYPE MJCDestroyInstance(MJC *pMJCInstance) {
    CHECK_MJC_INSTANCE(pMJCInstance);

    ALOGD("MJCDestroyInstance, 0x%08X", (unsigned int)pMJCInstance);

    delete((MJC *)pMJCInstance);

    return MJC_ErrorNone;
}

/*****************************************************************************
 * FUNCTION
 *    MJCDestroyInstance
 * DESCRIPTION
 *    1. DestroyInstance MJC Instance
 * PARAMETERS
 *    param1 : [IN]  unsigned int *pu4MJCInstance
 *                MJC Instance to be destroyed
 * RETURNS
 *    Type: MJC_ERRORTYPE. MJC_ErrorNone means success and MJC_ErrorInsufficientResources means fail.
 ****************************************************************************/
MJC_ERRORTYPE MJCCreate(MJC *pMJCInstance, MJC_USERHANDLETYPE hUser) {
    CHECK_MJC_INSTANCE(pMJCInstance);

    pMJCInstance->Create(hUser);

    return MJC_ErrorNone;
}

/*****************************************************************************
 * FUNCTION
 *    MJCDestroyInstance
 * DESCRIPTION
 *    1. DestroyInstance MJC Instance
 * PARAMETERS
 *    param1 : [IN]  unsigned int *pu4MJCInstance
 *                MJC Instance to be destroyed
 * RETURNS
 *    Type: MJC_ERRORTYPE. MJC_ErrorNone means success and MJC_ErrorInsufficientResources means fail.
 ****************************************************************************/
MJC_ERRORTYPE MJCInit(MJC *pMJCInstance, bool fgUsed) {
    CHECK_MJC_INSTANCE(pMJCInstance);

    pMJCInstance->Init(fgUsed);

    return MJC_ErrorNone;
}

/*****************************************************************************
 * FUNCTION
 *    MJCDestroyInstance
 * DESCRIPTION
 *    1. DestroyInstance MJC Instance
 * PARAMETERS
 *    param1 : [IN]  unsigned int *pu4MJCInstance
 *                MJC Instance to be destroyed
 * RETURNS
 *    Type: MJC_ERRORTYPE. MJC_ErrorNone means success and MJC_ErrorInsufficientResources means fail.
 ****************************************************************************/
MJC_ERRORTYPE MJCDeInit(MJC *pMJCInstance) {
    CHECK_MJC_INSTANCE(pMJCInstance);

    pMJCInstance->Deinit();

    return MJC_ErrorNone;
}

/*****************************************************************************
 * FUNCTION
 *    MJCDestroyInstance
 * DESCRIPTION
 *    1. DestroyInstance MJC Instance
 * PARAMETERS
 *    param1 : [IN]  unsigned int *pu4MJCInstance
 *                MJC Instance to be destroyed
 * RETURNS
 *    Type: MJC_ERRORTYPE. MJC_ErrorNone means success and MJC_ErrorInsufficientResources means fail.
 ****************************************************************************/
MJC_ERRORTYPE MJCSetParameter(MJC *pMJCInstance, MJC_PARAMTYPE nParamIndex, MJC_PTR pCompParam) {
    CHECK_MJC_INSTANCE(pMJCInstance);

    pMJCInstance->SetParameter(nParamIndex, pCompParam);

    return MJC_ErrorNone;
}

/*****************************************************************************
 * FUNCTION
 *    MJCDestroyInstance
 * DESCRIPTION
 *    1. DestroyInstance MJC Instance
 * PARAMETERS
 *    param1 : [IN]  unsigned int *pu4MJCInstance
 *                MJC Instance to be destroyed
 * RETURNS
 *    Type: MJC_ERRORTYPE. MJC_ErrorNone means success and MJC_ErrorInsufficientResources means fail.
 ****************************************************************************/
MJC_ERRORTYPE MJCGetParameter(MJC *pMJCInstance, MJC_PARAMTYPE nParamIndex, MJC_PTR pCompParam) {
    CHECK_MJC_INSTANCE(pMJCInstance);

    pMJCInstance->GetParameter(nParamIndex, pCompParam);

    return MJC_ErrorNone;
}


/*****************************************************************************
 * FUNCTION
 *    MJC::Create
 * DESCRIPTION
 *    1. Create MtkMJCThread
 * PARAMETERS
 *    param1 : [IN]  MJC_USERHANDLETYPE hUser
 *                Pointer of user (omx component) handle.
 * RETURNS
 *    Type: MJC_ERRORTYPE. MJC_ErrorNone mean success, MJC_ErrorInsufficientResources mean thread create fail and others mean fail.
 ****************************************************************************/
MJC_ERRORTYPE MJC::Create(MJC_IN MJC_USERHANDLETYPE hUser) {
    MJC_ERRORTYPE err = MJC_ErrorNone;
    int ret;

    hComponent = hUser;
    mThreadParam.pMJC = this;
    mThreadParam.pUser = hComponent;
    MJC_LOGI("[0x%08X] Drv Create pMJC=0x%08X, pVdec=0x%08X", (unsigned int)this, (unsigned int)mThreadParam.pMJC, (unsigned int)mThreadParam.pUser);

    ret = mScaler.Create(this);
    if (MJCScaler_ErrorNone != ret) {
        MJC_LOGE("[ERR] MJCScaler Create fail");
        err = MJC_ErrorInsufficientResources;
        goto EXIT;
    }

    // create MJC thread
    ret = pthread_create(&mMJCThread, NULL, &MtkMJCThread, (void *)&mThreadParam);
    if (ret) {
        MJC_LOGE("[ERR] MtkMJCThread creation failure");
        err = MJC_ErrorInsufficientResources;
        goto EXIT;
    }
    if (false == LoadMJCDrvFunc()) {
        MJC_LOGE("Failed to load MJC Drv Func");
        err = MJC_ErrorLoadDriverLib;
        goto EXIT;
    }
EXIT:
    return err;

}


bool MJC::SetAndCheckProp(const char *szPropName, const char *szValue) {
    unsigned int u4CheckNum = 0;

    if (szPropName == NULL || szValue == NULL) {
        return false;
    }

    property_set(szPropName, szValue);

    do {
        char value[PROPERTY_VALUE_MAX];
        property_get(szPropName, value, "0");
        if (!strcmp(szValue, value)) {
            MJC_LOGI("Property: %s set %s OK", szPropName, szValue);
            return true;
        } else {
            u4CheckNum++;
            if (u4CheckNum > 100) {
                MJC_LOGD("Property latency is too long, %s", szPropName);
                break;
            }
            SLEEP_MS(1);
        }
    } while (1);

    return false;

}

void MJC::CheckPowerSavingSettings(unsigned int u4ToBeCheckedChip) {
    if (true == mbSlowMotionMode) {
        if (u4ToBeCheckedChip == VAL_CHIP_NAME_MT6797) {
            // Backup the power saving settings
            char value[PROPERTY_VALUE_MAX];
            property_get("mtk.mjc.dummy.buffer", value, "0");
            mePowerSavingSetting.i4DummyBuffer = atoi(value);

            property_get("mjc.lib.lowpower.off", value, "0");
            mePowerSavingSetting.i4LowPower = atoi(value);

            property_get("mjc.lib.cafrc.off", value, "0");
            mePowerSavingSetting.i4Cafrc = atoi(value);

            MJC_LOGD("Ori Dummy(%d), LowPower(%d), Cafrc(%d)", mePowerSavingSetting.i4DummyBuffer, mePowerSavingSetting.i4LowPower, mePowerSavingSetting.i4Cafrc);

            // disable power saving mechanism
            SetAndCheckProp("mtk.mjc.dummy.buffer", "0");
            SetAndCheckProp("mjc.lib.lowpower.off", "1");
            SetAndCheckProp("mjc.lib.cafrc.off", "1");
        }
    }
}

void MJC::RestorePowerSavingSettings(unsigned int u4ToBeCheckedChip) {
    if (true == mbSlowMotionMode) {
        if (u4ToBeCheckedChip == VAL_CHIP_NAME_MT6797) {
            // restore power saving mechanism
            SetAndCheckProp("mtk.mjc.dummy.buffer", (mePowerSavingSetting.i4DummyBuffer > 0) ? "1" : "0");
            SetAndCheckProp("mjc.lib.lowpower.off", (mePowerSavingSetting.i4LowPower > 0) ? "1" : "0");
            SetAndCheckProp("mjc.lib.cafrc.off", (mePowerSavingSetting.i4Cafrc > 0) ? "1" : "0");
        }
    }
}

void MJC::UnLoadMJCDrvFunc() {
    //MJC_LOGD("%s", __FUNCTION__);

    if (NULL != mpMTK_MJC_Lib) {
        dlclose(mpMTK_MJC_Lib);

        mfn_eMjcDrvCreate   = NULL;
        mfn_eMjcDrvRelease  = NULL;
        mfn_eMjcDrvInit     = NULL;
        mfn_eMjcDrvProcess  = NULL;
        mfn_eMjcDrvReset    = NULL;
        mfn_eMjcDrvGetParam = NULL;
        mfn_eMjcDrvSetParam = NULL;

        mpMTK_MJC_Lib       = NULL;
    }
}

bool MJC::LoadMJCDrvFunc() {
    char value[PROPERTY_VALUE_MAX];
    int FakeEngine = 0;

    property_get("mtk.mjc.fake.engine.support", value, "0");
    FakeEngine = atoi(value);

    if (NULL != mpMTK_MJC_Lib) {
        MJC_LOGD("Driver has been loaded.");
        return true;
    }

    if (FakeEngine == 0) {
        mpMTK_MJC_Lib = dlopen(MTK_MJC_DRIVER_LIB_NAME, RTLD_NOW);
    } else {
        mpMTK_MJC_Lib = dlopen(MTK_MJC_FAKE_ENGINE_DRIVER_LIB_NAME, RTLD_NOW);
    }

    if (NULL == mpMTK_MJC_Lib) {
        MJC_LOGD("Can't load MJC driver, %s", dlerror());
        goto LOAD_DRV_FUNC_FAIL;
    }

    LOAD_MJC_DRV_FUNC(mpMTK_MJC_Lib, eMjcDrvCreate_ptr, MTK_MJC_DRVIER_CREATE_NAME, mfn_eMjcDrvCreate);
    LOAD_MJC_DRV_FUNC(mpMTK_MJC_Lib, eMjcDrvRelease_ptr, MTK_MJC_DRVIER_RELEASE_NAME, mfn_eMjcDrvRelease);
    LOAD_MJC_DRV_FUNC(mpMTK_MJC_Lib, eMjcDrvInit_ptr, MTK_MJC_DRVIER_INIT_NAME, mfn_eMjcDrvInit);
    LOAD_MJC_DRV_FUNC(mpMTK_MJC_Lib, eMjcDrvDeInit_ptr, MTK_MJC_DRVIER_DEINIT_NAME, mfn_eMjcDrvDeInit);
    LOAD_MJC_DRV_FUNC(mpMTK_MJC_Lib, eMjcDrvProcess_ptr, MTK_MJC_DRVIER_PROCESS_NAME, mfn_eMjcDrvProcess);
    LOAD_MJC_DRV_FUNC(mpMTK_MJC_Lib, eMjcDrvReset_ptr, MTK_MJC_DRVIER_RESET_NAME, mfn_eMjcDrvReset);
    LOAD_MJC_DRV_FUNC(mpMTK_MJC_Lib, eMjcDrvGetParam_ptr, MTK_MJC_DRVIER_GETPARAM_NAME, mfn_eMjcDrvGetParam);
    LOAD_MJC_DRV_FUNC(mpMTK_MJC_Lib, eMjcDrvSetParam_ptr, MTK_MJC_DRVIER_SETPARAM_NAME, mfn_eMjcDrvSetParam);

    return true;

LOAD_DRV_FUNC_FAIL:

    return false;

}

/*****************************************************************************
 * FUNCTION
 *    MJC::Init
 * DESCRIPTION
 *    1. Call drv create
 * PARAMETERS
 *    param1 : [IN]  bool fgUsed
 *                Drv create flag.
 * RETURNS
 *    Type: MJC_ERRORTYPE. MJC_ErrorNone mean success, MJC_ErrorDrvCreateFail mean drv fail (enter bypass mode automatically) and others mean fail.
 ****************************************************************************/
MJC_ERRORTYPE MJC::Init(bool fgUsed) {
    MJC_ERRORTYPE err = MJC_ErrorNone;
    mUseScaler = fgUsed; //default as Init flag

    char value[PROPERTY_VALUE_MAX];
    int bypass = 0, u4Enable = 0, MJC120hz = 0, Dummy = 0;

    MJCScaler_MODE eScalerMode;
    MtkOmxVdec *pVdec = (MtkOmxVdec *)hComponent;

    MJC_LOGD("MJC.Init(%d), 16xMode(%d)", fgUsed, mbSlowMotionMode);

    property_get("mtk.mjc.scaler.bypass", value, "0");
    bypass = atoi(value);

    property_get("persist.sys.display.clearMotion", value, "0");
    MJC_LOGD("persist.sys.display.clearMotion: %s", value);
    u4Enable = atoi(value);

    property_get("ro.mtk_display_120hz_support", value, "0");
    MJC120hz = atoi(value);

    property_get("mtk.mjc.dummy.buffer", value, "0");
    Dummy = atoi(value);

    if (MJC120hz == 1) {
        // Support MJC 120
        mbSupport120Hz = true;
        pRRC = new RefreshRateControl();
    }

    mbUseDummyBuffer = (Dummy == 0) ? false : true;

    if (1 == bypass || mUseScaler == false || u4Enable == 0) {
        mUseScaler = false;
        eScalerMode = MJCScaler_MODE_BYPASS;
        mScaler.SetParameter(MJCScaler_PARAM_MODE, &eScalerMode);
        //pVdec->mMJCScalerByPassFlag = OMX_TRUE;
        mCallbacks.pfnEventHandler(pVdec, MJC_EVENT_SCALERBYPASS, &eScalerMode);
    } else {
        eScalerMode = MJCScaler_MODE_NORMAL;
        mScaler.SetParameter(MJCScaler_PARAM_MODE, &eScalerMode);
        //pVdec->mMJCScalerByPassFlag = OMX_FALSE;
        mCallbacks.pfnEventHandler(pVdec, MJC_EVENT_SCALERBYPASS, &eScalerMode);
    }

    if ((true == fgUsed && u4Enable == 1) || true == mbSlowMotionMode) {
        MJC_LOGD("Succeed to dlopen MJC Drv. fgUsed(%d), mbSlowMotionMode(%d)", fgUsed, mbSlowMotionMode);

        if (MJC_DRV_RESULT_OK != mfn_eMjcDrvCreate(&mDrv)) {
            MJC_LOGD("[0x%08X] Check Drv Create, 0x%08X", (unsigned int)this, mDrv);
            mDrv = 0;
            err = MJC_ErrorDrvCreateFail;
            AtomicChangeMode(MJC_MODE_SET, MJC_MODE_BYPASS);
        } else {
            MJC_LOGI("[0x%08X] pVdec=0x%08X, 0x%08X", (unsigned int)this, (unsigned int)mThreadParam.pUser, mDrv);
            mDrvCreate = true;

            MJC_DRV_PARAM_DRV_INFO_T rDrvInfo;
            memset((void *)&rDrvInfo, 0, sizeof(MJC_DRV_PARAM_DRV_INFO_T));

            if (MJC_DRV_RESULT_OK != mfn_eMjcDrvGetParam(mDrv, MJC_DRV_TYPE_DRV_INFO, &rDrvInfo)) {
                AtomicChangeMode(MJC_MODE_SET, MJC_MODE_BYPASS);
            }

            if (rDrvInfo.u1DetectNum > 1) {
                mDetectNum = rDrvInfo.u1DetectNum;
            } else {
                CHECK(0);
            }
        }
    }

INIT_EXIT:

    MJC_LOGE("MJC init return:%d, mDrv(0x%08x)", err, mDrv);
    return err;
}

/*****************************************************************************
 * FUNCTION
 *    MJC::Deinit
 * DESCRIPTION
 *    1. Call drv deinit if drv init
 * PARAMETERS
 *    None
 * RETURNS
 *    Type: MJC_ERRORTYPE. MJC_ErrorNone mean success and others mean fail.
 ****************************************************************************/
MJC_ERRORTYPE MJC::Deinit() {
    MJC_ERRORTYPE err = MJC_ErrorNone;

    mTerminated = true;
    SIGNAL(mMJCFrameworkSem); // wake up myself to exit main thread
    SIGNAL(mScaler.mMJCScalerSem);
    mScaler.Deinit();

    if (pRRC != 0) {
        if (true == mbNotifyRRC) {
            ((RefreshRateControl *)pRRC)->setScenario(RRC_TYPE_VIDEO_120HZ, false);
            mbNotifyRRC = false;
            MJC_LOGI("set RRC_TYPE_VIDEO_120HZ false");
        }
        delete(RefreshRateControl *)pRRC;
    }

    if (!pthread_equal(pthread_self(), mMJCThread)) {
        // wait for mMJCThread terminate
        pthread_join(mMJCThread, NULL);
    }

    if (0 != mDrv && true == mDrvInit) {
        MJC_DRV_RESULT_T ret;
        ret = mfn_eMjcDrvDeInit(mDrv);
        MJC_LOGI("[0x%08X] Drv Deinit, 0x%08X", (unsigned int)this, mDrv);
        CHECK(MJC_DRV_RESULT_OK == ret);
        DisablePerfService();
        mDrvInit = false;
    }

    // Check should we restore the power saving settings
    RestorePowerSavingSettings(mu4ChipName);

    return err;
}


/*****************************************************************************
 * FUNCTION
 *    MJC::SetParameter
 * DESCRIPTION
 *    1. According to the parameter type to set the value.
 * PARAMETERS
 *    param1 : [IN]  MJC_PARAMTYPE nParamIndex
 *                Parameter ID.
 *    param2 : [OUT]  MJC_PTR pCompParam
 *                Pointer of parameter value
 * RETURNS
 *    Type: MJC_ERRORTYPE. MJC_ErrorNone mean success and others mean fail.
 ****************************************************************************/
MJC_ERRORTYPE MJC::SetParameter(MJC_IN MJC_PARAMTYPE nParamIndex, MJC_IN MJC_PTR pCompParam) {
    MJC_ERRORTYPE err = MJC_ErrorNone;
    if (NULL == pCompParam) {
        err = MJC_ErrorBadParameter;
        goto EXIT;
    }

    switch (nParamIndex) {
    case MJC_PARAM_MODE: {
        MJC_MODE OriMode = mMode;
        MJC_MODE *pParams = (MJC_MODE *)pCompParam;

        if (*pParams == MJC_MODE_NORMAL) {
            if ((mDrvInit == false && mState != MJC_STATE_INIT)) {
                MJC_LOGD("[%d] SP Mode %d, ignore 4 DrvInit %d", mMode, *pParams, mDrvInit);
                break;
            }
        }

        //MJC_LOGD("SP Mode %d", *pParams);

        CHECK(mMode == MJC_MODE_NORMAL || mMode == MJC_MODE_BYPASS || mMode == MJC_MODE_FLUSH);
        CHECK(MJC_STATE_FLUSH != mState);
        if (MJC_MODE_FLUSH == *pParams) {
            AtomicChangeMode(MJC_MODE_ADD, *pParams);
        } else {
            AtomicChangeMode(MJC_MODE_SET, *pParams);
        }

        MJC_LOGI("[%d][%d] SP Mode %d TO %d => %d", gettid(), mThreadTid, OriMode, *pParams, mMode);
        break;
    }
    case MJC_PARAM_FRAME_RESOLUTION: {
        MJC_VIDEORESOLUTION *pParams = (MJC_VIDEORESOLUTION *)pCompParam;

        MJC_DRV_PARAM_HW_INFO_T rAlignment;
        MJC_DRV_RESULT_T ret;

        memset((void *)&rAlignment, 0 , sizeof(MJC_DRV_PARAM_HW_INFO_T));
        if (0 != mDrv) {
            ret = mfn_eMjcDrvGetParam(mDrv, MJC_DRV_TYPE_HW_INFO, &rAlignment);
            CHECK(MJC_DRV_RESULT_OK == ret);
        } else {
            rAlignment.u4WidthPitch = 1;
            rAlignment.u4HeightPitch = 1;
        }

        mFrame.u4Width = pParams->u4Width;
        mFrame.u4Height = pParams->u4Height;

        MJCScaler_VIDEORESOLUTION rOri;

        rOri.u4Width = mFrame.u4Width;
        rOri.u4Height = mFrame.u4Height;
        mScaler.SetParameter(MJCScaler_PARAM_FRAME_RESOLUTION, &rOri);

        MJCScaler_MODE eScalerMode;
        MtkOmxVdec *pVdec = (MtkOmxVdec *)hComponent;

        if (!mIsHDRVideo) {
            if (mFrame.u4Width <  MJC_SCALE_WIDTH * 0.9 && mFrame.u4Height < MJC_SCALE_HEIGHT * 0.9
                && mFrame.u4Width >= MJC_SCALE_MIN_WIDTH   && mFrame.u4Height >= MJC_SCALE_MIN_HEIGHT) {
                if (true ==  mUseScaler) {
                    eScalerMode = MJCScaler_MODE_NORMAL;
                    mScaler.SetParameter(MJCScaler_PARAM_MODE, &eScalerMode);
                    //pVdec->mMJCScalerByPassFlag = OMX_FALSE;
                    mCallbacks.pfnEventHandler(pVdec, MJC_EVENT_SCALERBYPASS, &eScalerMode);
                }
                else {
                    eScalerMode = MJCScaler_MODE_BYPASS;
                    mScaler.SetParameter(MJCScaler_PARAM_MODE, &eScalerMode);
                    //pVdec->mMJCScalerByPassFlag = OMX_TRUE;
                    mCallbacks.pfnEventHandler(pVdec, MJC_EVENT_SCALERBYPASS, &eScalerMode);
                }
            } else {
                eScalerMode = MJCScaler_MODE_BYPASS;
                mScaler.SetParameter(MJCScaler_PARAM_MODE, &eScalerMode);
                //pVdec->mMJCScalerByPassFlag = OMX_TRUE;
                mCallbacks.pfnEventHandler(pVdec, MJC_EVENT_SCALERBYPASS, &eScalerMode);
            }
        }

        MJC_LOGD("SP FRM_RES %d, %d", mFrame.u4Width, mFrame.u4Height);

        break;
    }
    case MJC_PARAM_BUFFER_RESOLTUION: {
        MJC_VIDEORESOLUTION *pParams = (MJC_VIDEORESOLUTION *)pCompParam;
        mBuffer.u4Width = pParams->u4Width;
        mBuffer.u4Height = pParams->u4Height;

        MJCScaler_VIDEORESOLUTION rScaled;

        rScaled.u4Width = mBuffer.u4Width;
        rScaled.u4Height = mBuffer.u4Height;
        mScaler.SetParameter(MJCScaler_PARAM_SCALE_RESOLUTION, &rScaled);

        MJC_LOGD("SP BUF_RES %d, %d", mBuffer.u4Width, mBuffer.u4Height);

        break;
    }
    case MJC_PARAM_ALIGH_SIZE: {
        MJC_VIDEORESOLUTION *pParams = (MJC_VIDEORESOLUTION *)pCompParam;
        mAlignment.u4Width = pParams->u4Width;
        mAlignment.u4Height = pParams->u4Height;

        MJC_LOGD("SP VDO_Align (%d, %d)", mAlignment.u4Width, mAlignment.u4Height);

        MJCScaler_VIDEORESOLUTION alignment;
        alignment.u4Height = mAlignment.u4Height;
        alignment.u4Width = mAlignment.u4Width;
        mScaler.SetParameter(MJCScaler_PARAM_ALIGH_SIZE, &alignment);

        break;
    }

    case MJC_PARAM_CROP_INFO: {
        memcpy((void *)&mCropInfo, (void *)pCompParam, sizeof(MJC_VIDEOCROP));
        MJC_LOGD("SP CropInfo: (%d, %d) (%d, %d)", mCropInfo.mCropLeft, mCropInfo.mCropTop, mCropInfo.mCropWidth, mCropInfo.mCropHeight);

        if (0 != mCropInfo.mCropLeft || 0 != mCropInfo.mCropTop /* ||
            (0 != mCropInfo.mCropWidth && mFrame.u4Width !=  mCropInfo.mCropWidth) ||
            (0 != mCropInfo.mCropHeight && mFrame.u4Height !=  mCropInfo.mCropHeight) */) {
            AtomicChangeMode(MJC_MODE_ADD, MJC_MODE_BYPASS);
            MJC_LOGD("[CropVideo] init change to mode: %d", mMode);

            mUseScaler = false;
            MtkOmxVdec *pVdec = (MtkOmxVdec *)hComponent;
            MJCScaler_MODE eScalerMode = MJCScaler_MODE_BYPASS;
            mScaler.SetParameter(MJCScaler_PARAM_MODE, &eScalerMode);
            //pVdec->mMJCScalerByPassFlag = OMX_TRUE;
            mCallbacks.pfnEventHandler(pVdec, MJC_EVENT_SCALERBYPASS, &eScalerMode);
        } else {
            mScaler.SetParameter(MJCScaler_PARAM_CROP_INFO, &mCropInfo);
        }

        break;
    }

    case MJC_PARAM_FORMAT: {
        MJC_VIDEO_FORMAT *pParams = (MJC_VIDEO_FORMAT *)pCompParam;
        mFormat = *pParams;
        MJC_LOGD("SP FMT: %d", mFormat);
        CHECK(MJC_FORMAT_NONE != mFormat);

        MJCScaler_VIDEO_FORMAT eScalerFormat;
        if (MJC_FORMAT_BLK == mFormat) {
            eScalerFormat = MJCScaler_FORMAT_BLK;
        } else if (MJC_FORMAT_LINE == mFormat) {
            eScalerFormat = MJCScaler_FORMAT_LINE;
        } else if (MJC_FORMAT_BLK_10BIT_H == mFormat) {
            eScalerFormat = MJCScaler_FORMAT_BLK_10BIT_H;
        } else if (MJC_FORMAT_BLK_10BIT_V == mFormat) {
            eScalerFormat = MJCScaler_FORMAT_BLK_10BIT_V;
        }
        mScaler.SetParameter(MJCScaler_PARAM_FORMAT, &eScalerFormat);

        break;
    }
    case MJC_PARAM_CALLBACKS: {
        MJC_CALLBACKTYPE *pParams = (MJC_CALLBACKTYPE *)pCompParam;
        mCallbacks = *pParams;

        break;
    }
    case MJC_PARAM_3DTYPE: {
        MJC_3DTYPE *pParams = (MJC_3DTYPE *)pCompParam;
        MJC_LOGD("SP 3DTYPE: %d", *pParams);

        if (*pParams < MJC_3DTYPE_MAX) {
            m3DType = *pParams;
        } else {
            CHECK(0);
        }

        break;
    }
    case MJC_PARAM_CODECTYPE: {
        MTK_VDEC_CODEC_ID *pParams = (MTK_VDEC_CODEC_ID *)pCompParam;
        mCodecType = *pParams;
        MJC_LOGD("SP CODECTYPE: %d", mCodecType);

        break;
    }
    case MJC_PARAM_SEEK: {
        bool *pParams = (bool *)pCompParam;
        mSeek = *pParams;
        mCheckDFR = true;
        MJC_LOGD("SP SEEK: %d", mSeek);
        break;
    }
    //#ifdef MTK_DEINTERLACE_SUPPORT
    case MJC_PARAM_RUNTIME_DISABLE: {
        bool *pParams = (bool *)pCompParam;
        MJCScaler_MODE eScalerMode;
        MtkOmxVdec *pVdec = (MtkOmxVdec *)hComponent;

        mRunTimeDisable = *pParams;
        mUseScaler = false;
        eScalerMode = MJCScaler_MODE_BYPASS;
        mScaler.SetParameter(MJCScaler_PARAM_MODE, &eScalerMode);
        //pVdec->mMJCScalerByPassFlag = OMX_TRUE;
        mCallbacks.pfnEventHandler(pVdec, MJC_EVENT_SCALERBYPASS, &eScalerMode);
        mMode |= MJC_MODE_BYPASS;
        // Since global setting is off, we should turn off MJC driver to save power.
        {
            if (0 != mDrv && true == mDrvCreate) {
                MJC_DRV_RESULT_T ret;

                MJC_LOGI("[0x%08X] Drv Release for setting off, 0x%08X", (unsigned int)this, mDrv);
                ret = mfn_eMjcDrvRelease(mDrv);

                if (MJC_DRV_RESULT_OK != ret) {
                    MJC_LOGE("[0x%08X] Drv Release fail, 0x%08X", (unsigned int)this, mDrv);
                }
                CHECK(MJC_DRV_RESULT_OK == ret);
                mDrvCreate = false;
                mDrv = 0;
            }
        }
        MJC_LOGD("SetParameter MJC_PARAM_RUNTIME_DISABLE: %d", mRunTimeDisable);
        break;
    }
    //#endif
    //#ifdef MTK_16X_SLOWMOTION_VIDEO_SUPPORT
    case MJC_PARAM_16XSLOWMOTION_MODE: {
        bool *pParams = (bool *)pCompParam;
        mbSlowMotionMode = *pParams;
        MJC_LOGD("SP 16xSMMode: %d", mbSlowMotionMode);

        break;
    }
    case MJC_PARAM_SLOWMOTION_SPEED: {
        unsigned int *pParams = (unsigned int *)pCompParam;
        mu4SlowmotionSpeed = (*pParams) * 10;
        MJC_LOGD("SP SMSpeed: %d", mu4SlowmotionSpeed);

        break;
    }
    case MJC_PARAM_SLOWMOTION_SECTION: {
        OMX_MTK_SLOWMOTION_SECTION *pParams = (OMX_MTK_SLOWMOTION_SECTION *)pCompParam;
        meSlowMotionSection.StartTime = pParams->StartTime;
        meSlowMotionSection.EndTime   = pParams->EndTime;

        MJC_LOGD("SP SMSection: %lld~%lld", meSlowMotionSection.StartTime, meSlowMotionSection.EndTime);

        break;
    }
    //#endif

    case MJC_PARAM_CHIP_NAME: {
        mu4ChipName = *((VAL_UINT32_T *)pCompParam);

        if (mu4ChipName == VAL_CHIP_NAME_MT6795 || mu4ChipName == VAL_CHIP_NAME_MT6797) {
            mbNeedToEnablePerfService = true;
        }
        MJC_LOGD("Chip Name: %d, EnablePerfService:%d", mu4ChipName, mbNeedToEnablePerfService);

        // Check should we adjust the power settings
        CheckPowerSavingSettings(mu4ChipName);

        break;
    }
    case MJC_PARAM_DRAIN_VIDEO_BUFFER: {
        MtkOmxVdec *pVdec = (MtkOmxVdec *)hComponent;
        // Init Drain mode
        INIT_SEMAPHORE(pVdec->mMJCVdoBufQDrainedSem);

        //Set MJCScaler to drain mode
        mScaler.SetParameter(MJCScaler_PARAM_DRAIN_VIDEO_BUFFER, pCompParam);
        //Set MJCFramework to drain mode
        mMJCDrainInputBuffer = *((bool *)pCompParam);
        MJC_LOGI("[%d] mMJCDrainInputBuffer:%d", mMode , mMJCDrainInputBuffer);

        SIGNAL(mMJCFrameworkSem);
        break;
    }
    case MJC_PARAM_IS_HDRVIDEO: {
        MJC_HDRVideoInfo *pParams = (MJC_HDRVideoInfo *)pCompParam;
        MJCScaler_MODE eScalerMode;
        MtkOmxVdec *pVdec = (MtkOmxVdec *)hComponent;

        mIsHDRVideo = pParams->isHDRVideo;
        if (mIsHDRVideo && (pParams->u4Width * pParams->u4Height <= 1920 * 1088)) {
            if (true ==  mUseScaler) {
                eScalerMode = MJCScaler_MODE_NORMAL;
                mScaler.SetParameter(MJCScaler_PARAM_MODE, &eScalerMode);
                //pVdec->mMJCScalerByPassFlag = OMX_FALSE;
                mCallbacks.pfnEventHandler(pVdec, MJC_EVENT_SCALERBYPASS, &eScalerMode);
            }
        }

        MJCScaler_HDRVideoInfo HDRinfoParams;
        HDRinfoParams.isHDRVideo = mIsHDRVideo;
        mScaler.SetParameter(MJCScaler_PARAM_IS_HDRVIDEO, &HDRinfoParams);
        break;
    }
    case MJC_PARAM_SET_COLOR_DESC: {
        VDEC_DRV_COLORDESC_T *pColorDesc = (VDEC_DRV_COLORDESC_T *)pCompParam;

        if (mIsHDRVideo) {
            mScaler.SetParameter(MJCScaler_PARAM_SET_COLOR_DESC, pColorDesc);
        }
        break;
    }
    case MJC_PARAM_SET_DEMO_MODE: {
        unsigned int *pParams = (unsigned int *)pCompParam;
        mApkSetDemoMode = (*pParams);

        MJC_LOGI("[%d] Set MJC Demo Mode: %d", mMode , mApkSetDemoMode);
        break;
    }

    default:
        MJC_LOGE("SP Err param %d", nParamIndex);
        CHECK(0);
        break;
    }

EXIT:
    return err;
}

/*****************************************************************************
 * FUNCTION
 *    MJC::GetParameter
 * DESCRIPTION
 *    1. According to the parameter type to fill the value.
 * PARAMETERS
 *    param1 : [IN]  MJC_PARAMTYPE nParamIndex
 *                Parameter ID.
 *    param2 : [OUT]  MJC_PTR pCompParam
 *                Pointer of parameter value
 * RETURNS
 *    Type: MJC_ERRORTYPE. MJC_ErrorNone mean success and others mean fail.
 ****************************************************************************/
MJC_ERRORTYPE MJC::GetParameter(MJC_IN MJC_PARAMTYPE nParamIndex, MJC_INOUT MJC_PTR pCompParam) {
    MJC_ERRORTYPE err = MJC_ErrorNone;
    if (NULL == pCompParam) {
        err = MJC_ErrorBadParameter;
        goto EXIT;
    }

    switch (nParamIndex) {
    case MJC_PARAM_ALIGN_RESOLTUION: {
        MJC_VIDEORESOLUTION *pParams = (MJC_VIDEORESOLUTION *)pCompParam;

        MJC_DRV_PARAM_HW_INFO_T rAlignment;
        MJC_DRV_RESULT_T ret;

        memset((void *)&rAlignment, 0 , sizeof(MJC_DRV_PARAM_HW_INFO_T));
        if (0 != mDrv) {
            ret = mfn_eMjcDrvGetParam(mDrv, MJC_DRV_TYPE_HW_INFO, &rAlignment);
            CHECK(MJC_DRV_RESULT_OK == ret);
        } else {
            rAlignment.u4WidthPitch = 1;
            rAlignment.u4HeightPitch = 1;
        }

        pParams->u4Width = ALIGN(mFrame.u4Width, rAlignment.u4WidthPitch);
        pParams->u4Height = ALIGN(mFrame.u4Height, rAlignment.u4HeightPitch);

        MJCScaler_MODE eScalerMode;

        mScaler.GetParameter(MJCScaler_PARAM_MODE, &eScalerMode);

        if ((MJCScaler_MODE_BYPASS & eScalerMode) != 1) { // As long as we're not in BYPASS mode, we should set the correct size after scale-up
            if (mFrame.u4Width < MJC_SCALE_WIDTH && mFrame.u4Height < MJC_SCALE_HEIGHT) {
                unsigned int u4RatioW = MJC_SCALE_WIDTH * 100 / mFrame.u4Width;
                unsigned int u4RatioH = MJC_SCALE_HEIGHT * 100 / mFrame.u4Height;
                if (u4RatioW > u4RatioH) {
                    MJC_LOGD("RatioH: %d, %d", ALIGN(mFrame.u4Width * u4RatioH / 100, ALIGN_BLK_W) , MJC_SCALE_HEIGHT);
                    pParams->u4Width = ALIGN(mFrame.u4Width * u4RatioH / 100, ALIGN_BLK_W);
                    pParams->u4Height = MJC_SCALE_HEIGHT;
                } else {
                    MJC_LOGD("RatioW: %d, %d", MJC_SCALE_WIDTH , ALIGN(mFrame.u4Height * u4RatioW / 100, ALIGN_BLK_H));
                    pParams->u4Width = MJC_SCALE_WIDTH;
                    pParams->u4Height = ALIGN(mFrame.u4Height * u4RatioW / 100, ALIGN_BLK_H);
                }
            }
        }

        break;
    }

    case MJC_PARAM_PRESCALE: {
        OMX_BOOL *pParams = (OMX_BOOL *)pCompParam;
        MJCScaler_MODE eScalerMode;
        mScaler.GetParameter(MJCScaler_PARAM_MODE, &eScalerMode);
        if ((MJCScaler_MODE_BYPASS & eScalerMode) != 1) {
            *pParams = OMX_TRUE;
            //MJC_LOGD("pre-scaled(%d)", eScalerMode);
        } else {
            *pParams = OMX_FALSE;
            //MJC_LOGD("NOT pre-scaled(%d)", eScalerMode);
        }

        break;
    }

    case MJC_PARAM_MODE: {
        OMX_BOOL *pParams = (OMX_BOOL *)pCompParam;
        if ((MJC_MODE_BYPASS & mMode) != 0) {
            *pParams = OMX_FALSE; // MJC disabled
        } else {
            *pParams = OMX_TRUE; // MJC enabled
        }

        break;

    }
    case MJC_PARAM_DRIVER_REGISTER: {
        OMX_BOOL *pParams = (OMX_BOOL *)pCompParam;
        MJC_DRV_PARAM_DRV_INFO_T rDrvInfo;
        memset((void *)&rDrvInfo, 0, sizeof(MJC_DRV_PARAM_DRV_INFO_T));

        // Driver handle 0 since driver hasn't been created

        if (MJC_DRV_RESULT_OK == mfn_eMjcDrvGetParam(0, MJC_DRV_TYPE_DRV_REGISTER_INFO, &rDrvInfo)) {
            if (rDrvInfo.u1SucessfullyRegisterDrv) {
                *pParams = OMX_TRUE; // Instance successfully registerS MJC Driver for using
            } else {
                *pParams = OMX_FALSE; // MJC driver has been registered by other instance
                MJC_LOGE("MJC_PARAM_DRIVER_REGISTER MJC Not support multiple instance");
            }
        }
        break;
    }
    case MJC_PARAM_DRIVER_UNREGISTER: {
        OMX_BOOL *pParams = (OMX_BOOL *)pCompParam;
        MJC_DRV_PARAM_DRV_INFO_T rDrvInfo;
        memset((void *)&rDrvInfo, 0, sizeof(MJC_DRV_PARAM_DRV_INFO_T));

        // Driver handle 0 since driver hasn't been created

        if (MJC_DRV_RESULT_OK == mfn_eMjcDrvGetParam(0, MJC_DRV_TYPE_DRV_UNREGISTER_INFO, &rDrvInfo)) {
            *pParams = OMX_TRUE; // Successfully unregistered
        }
        else {
            *pParams = OMX_FALSE; // Instance unregisters fail
            MJC_LOGE("MJC_PARAM_DRIVER_UNREGISTER Instance unregisters fail");
        }

        break;
    }

    default:
        MJC_LOGE("GP Error param %d", nParamIndex);
        CHECK(0);
        break;
    }

EXIT:
    return err;
}

bool MJC::PrepareBuffer_CheckFrameRate(void *pData) {
    MJC_ThreadParam *pParam = (MJC_ThreadParam *)pData;
    MtkOmxVdec *pVdec = (MtkOmxVdec *)(pParam->pUser);
    MJC *pMJC = (MJC *)(pParam->pMJC);

    int64_t in_time_1, out_time_1;

    if (MJC_STATE_INIT == pMJC->mState && MJC_MODE_FLUSH != pMJC->mMode) {
        MJC_LOGI("[%d] MJCSem +", pMJC->mState);
        in_time_1 = getTickCountMs();

        WAIT(pMJC->mMJCFrameworkSem);

        out_time_1 = getTickCountMs() - in_time_1;

        if (out_time_1 > 3) {
            MJC_LOGI("[%d] MJCSem - %lld ms", pMJC->mState, out_time_1);
        }

        OMX_BUFFERHEADERTYPE *pBuffHdr;
        MJC_INT_BUF_INFO *pMJCBufInfo;

        while (pMJC->mRefBufQ.size() < pMJC->mDetectNum) {
            if (true == pMJC->mTerminated) {
                MJC_LOGI("MtkMJCThread terminated, %d, pVdec=0x%08X, mIsComponentAlive(%d)", __LINE__, (unsigned int)pVdec, pVdec->mIsComponentAlive);
                return false;
            }

            if ((MJC_MODE_FLUSH & pMJC->mMode) != 0) {
                pMJC->mState = MJC_STATE_FLUSH;
                break;
            }

            //4 Send MJCScaler output frame to MJCScaler
            while (1) {
                OMX_BUFFERHEADERTYPE *pOutBuffHdr = NULL;
                MJC_INT_BUF_INFO *pMJCOutBufInfo;
                pOutBuffHdr = pMJC->mCallbacks.pfnGetOutputFrame(pVdec);
                if (NULL == pOutBuffHdr) {
                    MJC_LOGI("[%d] NNOB", pMJC->mState);
                    break;
                } else {
                    pMJCOutBufInfo = MJCGetBufInfoFromOMXHdr(pVdec, pOutBuffHdr);
                    // May happen with the original frame.
                    for (unsigned int i = 0; i < pMJC->mRefBufQ.size(); i++) {
                        if (pMJCOutBufInfo == pMJC->mRefBufQ[i]) {
                            MJC_LOGD("[%d][%d] Keep Ref 0x%08X", pMJC->mState, pMJC->mMode, (unsigned int)pMJCOutBufInfo->pBufHdr);
                            pMJCOutBufInfo->nFilledLen = pMJCOutBufInfo->pBufHdr->nFilledLen = 0;
                            pMJCOutBufInfo->nFilledLen = pMJCOutBufInfo->pBufHdr->nTimeStamp = 0;
                            PDFAndSetMJCBufferFlag(pVdec, pMJCOutBufInfo);
                            break;
                        }
                    }
                    if (NULL == pMJCOutBufInfo->pBufHdr) {
                        break;
                    }

                    if (pMJCOutBufInfo->nFlags & OMX_BUFFERFLAG_SCALER_FRAME) {
                        pMJC->mScaler.FillThisBuffer(pMJCOutBufInfo);
                    } else {
                        pMJC->mOutputBufQ.add(pMJCOutBufInfo);
                    }
                }
            }

            pBuffHdr = pMJC->mCallbacks.pfnGetInputFrame(pVdec);
            if (pBuffHdr != NULL) {
                pMJCBufInfo = MJCGetBufInfoFromOMXHdr(pVdec, pBuffHdr);
                MJC_LOGD("[%d] GIF, 0x%08X, ts = %lld, len = %d", pMJC->mState, (unsigned int)pMJCBufInfo->pBufHdr, pMJCBufInfo->nTimeStamp, pMJCBufInfo->nFilledLen);

                if (pMJCBufInfo->nFlags & OMX_BUFFERFLAG_EOS) {
                    pMJC->mScaler.EmptyThisBuffer(pMJCBufInfo); // All buffers should go through Decoder->Scaler->Framework
                    pMJC->AtomicChangeMode(MJC_MODE_ADD, MJC_MODE_BYPASS);
                    MJC_LOGD("EOS in start(0x%08X), BYPASS", (unsigned int)pMJCBufInfo->pBufHdr);
                    SIGNAL(pMJC->mMJCFrameworkSem);
                } else {
                    if (0 == pMJCBufInfo->nFilledLen) {
                        MJC_LOGD("[%d][%d] PDF, 0x%08X, ts = %lld, len = %d", pMJC->mState, pMJC->mMode, (unsigned int)pMJCBufInfo->pBufHdr, pMJCBufInfo->nTimeStamp, pMJCBufInfo->nFilledLen);
                        PDFAndSetMJCBufferFlag(pVdec, pMJCBufInfo);
                        SIGNAL(pMJC->mScaler.mMJCScalerSem);
                    } else {
                        //4 Send MJCScaler input frame to MJCScaler
                        pMJC->mScaler.EmptyThisBuffer(pMJCBufInfo);
                    }
                }
            } else {
                MJC_LOGI("[%d][%d] WIF in start", pMJC->mState, pMJC->mMode);
                SLEEP_MS(10);
            }

            //4 Get Ref frame from  MJCScaler
            while (1) {
                MJC_INT_BUF_INFO *pMJCRefBufInfo = NULL;
                pMJCRefBufInfo = mScaler.GetScaledBuffer();
                if (NULL != pMJCRefBufInfo) {
                    if (pMJCRefBufInfo->nFlags & OMX_BUFFERFLAG_EOS) {
                        mEOSReceived = true;
                    }
                    MJCSetBufRef(pVdec, pMJCRefBufInfo);
                } else {
                    if (mMJCDrainInputBuffer) {
                        LOCK(mScaler.mScalerInputDrainedLock);
                        if (mScaler.mScalerInputDrained) {
                            // Clear RefQ when drain mode. Scaler should processed all input buffer by now
                            if (mRefBufQ.size() <= 4) {
                                //check filledlen != 0 to avoid PDF original buffer that had been shifted before.
                                while (mRefBufQ.size() != 0 && mRefBufQ[0]->nFilledLen != 0) {
                                    PDFAndSetMJCBufferFlag(pVdec, mRefBufQ[0]);
                                    MJCClearBufRef(pVdec, mRefBufQ[0]);
                                }
                                MJC_LOGI("Signal pVdec->mMJCVdoBufQDrainedSem.");
                                mMJCDrainInputBuffer = false;
                                SIGNAL(pVdec->mMJCVdoBufQDrainedSem);
                            }
                        }
                        UNLOCK(mScaler.mScalerInputDrainedLock);
                    }
                    break;
                }
            }

            if ((MJC_MODE_BYPASS & pMJC->mMode) != 0) {
                MJC_LOGD("BYPASS! Escape FR check");
                break;
            }
        } // (pMJC->mRefBufQ.size() < pMJC->mDetectNum)

        //4  FrameRateDetect
        pMJC->mInFrmRate = pMJC->FrameRateDetect(&pMJC->mDFR, NULL, NULL);
        MJC_LOGD("[%d] FRD: %d, DFR: %d", pMJC->mState, pMJC->mInFrmRate, pMJC->mDFR);
    } // (MJC_STATE_INIT == pMJC->mState && MJC_MODE_FLUSH != pMJC->mMode)


    return true;
}


void MJC::CheckInitCondition(void *pData) {
    MJC_ThreadParam *pParam = (MJC_ThreadParam *)pData;
    MtkOmxVdec *pVdec = (MtkOmxVdec *)(pParam->pUser);
    MJC *pMJC = (MJC *)(pParam->pMJC);
    MJC_CfgParam *pcfgParam = &(pMJC->cfgParam);

    char value[PROPERTY_VALUE_MAX];
    int u4Enable = 0;
    int u4WFD = 0;

    property_get("persist.sys.display.clearMotion", value, "0");

    MJC_LOGD("persist.sys.display.clearMotion: %s", value);

    u4Enable = atoi(value);

    property_get("sys.display.clearMotion.dimmed", value, "0");

    MJC_LOGD("sys.display.clearMotion.dimmed: %s", value);

    u4WFD = atoi(value);

    memset((void *)(pcfgParam), 0, sizeof(MJC_CfgParam));
    ParseMjcConfig(pcfgParam);

    MJC_LOGD("ParseMjcConfig NRM %d WIDH %d HEIGHT %d MinFPS %d MaxFPS %d",
             pcfgParam->u4NrmFbLvlIdx, pcfgParam->u4MaxWidth, pcfgParam->u4MaxHeight, pcfgParam->u4MinFps, pcfgParam->u4MaxFps);


    // If Demo mode is not set by Settings.apk. Use property to decide demo mode instead.
    if (mApkSetDemoMode == 0)
    {
        SetApkParameter(pcfgParam);
    }
    else
    {
        pcfgParam->u4DemoMode = mApkSetDemoMode;
    }

    if (1 != u4Enable && false == mbSlowMotionMode) {
        AtomicChangeMode(MJC_MODE_ADD, MJC_MODE_BYPASS);
        MJC_LOGD("[%d][%d] global setting disable", pMJC->mState, pMJC->mMode);
    }


    if (1 == u4WFD) {
        pMJC->AtomicChangeMode(MJC_MODE_ADD, MJC_MODE_BYPASS);
        MJC_LOGD("[%d][%d] WFD enable", pMJC->mState, pMJC->mMode);
    }

    if (0 == pMJC->mInFrmRate || MJC_FORMAT_NONE == pMJC->mFormat || pcfgParam->u4MinFps * 10 > pMJC->mInFrmRate) {
        pMJC->AtomicChangeMode(MJC_MODE_ADD, MJC_MODE_BYPASS);
        MJC_LOGE("[%d][%d] BP 4 invaid drv param, IFR: %d, FMT: %d", pMJC->mState, pMJC->mMode, pMJC->mInFrmRate, pMJC->mFormat);
    }

    return;
}

void MJC::InitDriver(void *pData) {
    MJC_ThreadParam *pParam = (MJC_ThreadParam *)pData;
    MtkOmxVdec *pVdec = (MtkOmxVdec *)(pParam->pUser);
    MJC *pMJC = (MJC *)(pParam->pMJC);
    MJC_CfgParam *pcfgParam = &(pMJC->cfgParam);
    MJCScaler_MODE eScalerMode;

    if (0 != pMJC->mDrv) { //alwyas init drv
        // Call drv init
        MJC_DRV_CONFIG_T rConfig;
        MJC_DRV_INIT_INFO_T rInitResult;
        memset((void *)&rConfig, 0, sizeof(MJC_DRV_CONFIG_T));
        memset((void *)&rInitResult, 0, sizeof(MJC_DRV_INIT_INFO_T));

        rConfig.u1DFR = pMJC->mDFR;
        rConfig.u4Width = pMJC->mFrame.u4Width;
        rConfig.u4Height = pMJC->mFrame.u4Height;

        if (false == mbSlowMotionMode || mInFrmRate < MJC_FR(pcfgParam->u4MaxFps)) { // If it's slowmotion scenario, the video should be much higher than MaxFps
            rConfig.u2InFrmRate = (unsigned short)pMJC->mInFrmRate;
            rConfig.u2OutFrmRate = (unsigned short)pcfgParam->u4OutFrmRate; // mOutFrmRate = 600; @ MJC constructor
        } else {
            // Never enter bypass mode when we're in SlowMotion mode
            rConfig.u2InFrmRate = (unsigned short)150;
            rConfig.u2OutFrmRate = (unsigned short)600;
        }

        pMJC->mScaler.GetParameter(MJCScaler_PARAM_MODE, &eScalerMode);
        if (MJCScaler_MODE_NORMAL == eScalerMode) {
            rConfig.u4Width = pMJC->mBuffer.u4Width;
            rConfig.u4Height = pMJC->mBuffer.u4Height;
            rConfig.u1InPicWidthAlign = ALIGN_YV12_W;
            rConfig.u1InPicHeightAlign = ALIGN_YV12_H;

            rConfig.u1OutPicWidthAlign = ALIGN_BLK_W;
            rConfig.u1OutPicHeightAlign = ALIGN_BLK_H;
        } else {
            if (pMJC->mAlignment.u4Height != 0 && pMJC->mAlignment.u4Width != 0) {
                rConfig.u1InPicWidthAlign = pMJC->mAlignment.u4Width;
                rConfig.u1InPicHeightAlign = pMJC->mAlignment.u4Height;

                rConfig.u1OutPicWidthAlign = ALIGN_BLK_W;
                rConfig.u1OutPicHeightAlign = ALIGN_BLK_H;
            } else {
                if (MJC_FORMAT_BLK == pMJC->mFormat) {
                    rConfig.u1InPicWidthAlign = ALIGN_BLK_W;
                    rConfig.u1InPicHeightAlign = ALIGN_BLK_H;
                } else {
                    rConfig.u1InPicWidthAlign = ALIGN_MTKYV12_W;
                    rConfig.u1InPicHeightAlign = ALIGN_MTKYV12_H;
                }

                rConfig.u1OutPicWidthAlign = ALIGN_BLK_W;
                rConfig.u1OutPicHeightAlign = ALIGN_BLK_H;
            }
        }

        rConfig.u1NrmFbTabIdx = pcfgParam->u4NrmFbLvlIdx; // Customization item
        rConfig.u1BdrFbTabIdx = pcfgParam->u4BdrFbLvlIdx; // Customization item
        rConfig.u2MaxFps = MJC_FR(pcfgParam->u4MaxFps);   // Customization item


        // Ink +
        char Inkvalue[PROPERTY_VALUE_MAX];

        property_get("mtk.mjc.inkmodu", Inkvalue, "0");
        rConfig.u1InkModu = (unsigned char)(atoi(Inkvalue));

        property_get("mtk.mjc.inkinfo", Inkvalue, "0");
        rConfig.u1InkInfo = (unsigned char)(atoi(Inkvalue));

        // Ink -


        if (pcfgParam->u4DemoMode == 0) { //demo off
            rConfig.u1DemoMode = E_MJC_DEMO_OFF;
        } else if (pcfgParam->u4DemoMode == 1) { //demo vertical
            rConfig.u1DemoMode = E_MJC_DEMO_COL_LEFT_INTP;
        } else if (pcfgParam->u4DemoMode == 2) { //demo horizontal
            rConfig.u1DemoMode = E_MJC_DEMO_ROW_TOP_INTP;
        } else { //demo off
            rConfig.u1DemoMode = E_MJC_DEMO_OFF;
        }

        switch (pMJC->m3DType) {
        case MJC_3DTYPE_OFF:
            rConfig.e3DModeIn = rConfig.e3DModeOut = MJC_3D_OFF;
            break;

        case MJC_3DTYPE_FRAME_SEQ:
            rConfig.e3DModeIn = rConfig.e3DModeOut = MJC_3D_FRAME_SEQ;
            break;

        case MJC_3DTYPE_SIDE_BY_SIDE:
            rConfig.e3DModeIn = rConfig.e3DModeOut = MJC_3D_SIDE_BY_SIDE;
            break;

        case MJC_3DTYPE_TOP_BOTTOM:
            rConfig.e3DModeIn = rConfig.e3DModeOut = MJC_3D_TOP_BOTTOM;
            break;
        default:
            break;
        }

        // TODO: temp bypass 3D
        if (MJC_3DTYPE_OFF != pMJC->m3DType) {
            pMJC->AtomicChangeMode(MJC_MODE_ADD, MJC_MODE_BYPASS);
        }

        switch (pMJC->mFormat) {
        case MJC_FORMAT_BLK_10BIT_H:
            rConfig.u1Input10Bit = MJC_10BIT_HORIZONTAL;
            break;
        case MJC_FORMAT_BLK_10BIT_V:
            rConfig.u1Input10Bit = MJC_10BIT_VERTICAL;
            break;
        default:
            rConfig.u1Input10Bit = MJC_10BIT_OFF;
            break;
        }

        //------------------
        // TODO:  temp init value
        rConfig.u1Effect = 0xff;
        //------------------

        if (MJC_FORMAT_LINE == pMJC->mFormat) {
            //if(MTK_VDEC_CODEC_ID_HEVC == pMJC->mCodecType || MTK_VDEC_CODEC_ID_VPX == pMJC->mCodecType || MTK_VDEC_CODEC_ID_VC1 == pMJC->mCodecType)
            {
                if (rConfig.u4Width * rConfig.u4Height > MTK_MJC_SWCODEC_SUPPORT_RESOLUTION) {
                    pMJC->AtomicChangeMode(MJC_MODE_ADD, MJC_MODE_BYPASS);
                    MJC_LOGD("res > SWCODEC support, BP: %d", pMJC->mMode);
                }
            }
        }

        if (pcfgParam->u4MaxWidth != 0 && pcfgParam->u4MaxHeight != 0) {
            if (pMJC->mFrame.u4Width > pcfgParam->u4MaxWidth ||  pMJC->mFrame.u4Height > pcfgParam->u4MaxHeight) {
                pMJC->AtomicChangeMode(MJC_MODE_ADD, MJC_MODE_BYPASS);
                MJC_LOGD("res > user cfg (%d, %d), BP: %d", pcfgParam->u4MaxWidth, pcfgParam->u4MaxHeight, pMJC->mMode);
            }
        }
        if (pcfgParam->u4MinWidth != 0 && pcfgParam->u4MinHeight != 0) {
            if (pMJC->mFrame.u4Width < pcfgParam->u4MinWidth ||  pMJC->mFrame.u4Height < pcfgParam->u4MinHeight) {
                pMJC->AtomicChangeMode(MJC_MODE_ADD, MJC_MODE_BYPASS);
                MJC_LOGD("res < user cfg (%d, %d), BP: %d", pcfgParam->u4MinWidth, pcfgParam->u4MinHeight, pMJC->mMode);
            }
        }

        if (MJC_FORMAT_BLK == pMJC->mFormat) {
            rConfig.eInPicFormat = MJC_DRV_BLK_FORMAT;
            pMJC->mMJCInputFormat = MJC_FORMAT_BLK;
        } else if (MJC_FORMAT_LINE == pMJC->mFormat) {
            rConfig.eInPicFormat = MJC_DRV_LINE_FORMAT_YV12;
            pMJC->mMJCInputFormat = MJC_FORMAT_LINE;
        }

        pMJC->mScaler.GetParameter(MJCScaler_PARAM_MODE, &eScalerMode);
        if (MJCScaler_MODE_NORMAL == eScalerMode) {
            rConfig.eInPicFormat = MJC_DRV_LINE_FORMAT_YV12;
            pMJC->mMJCInputFormat = MJC_FORMAT_LINE;
        }

        MJC_LOGD("[%d][%d] drv init, (W: %d, H: %d),(InPicWA: %d, InPicHA: %d),(OutPicWA: %d, OutPicHA: %d),MaxFps: %d,InPicFormat :%d",
                 pMJC->mState,
                 pMJC->mMode,
                 rConfig.u4Width,
                 rConfig.u4Height,
                 rConfig.u1InPicWidthAlign,
                 rConfig.u1InPicHeightAlign,
                 rConfig.u1OutPicWidthAlign,
                 rConfig.u1OutPicHeightAlign,
                 rConfig.u2MaxFps,
                 rConfig.eInPicFormat);


        if (MJC_DRV_RESULT_OK == mfn_eMjcDrvInit(pMJC->mDrv, &rConfig, &rInitResult)) {
            pMJC->mInputNumRequire = rInitResult.u1NextInputNum;
            pMJC->mOutputNumRequire = rInitResult.u1NextOutputNum;
            pMJC->mFRRatio = rInitResult.u1FRRatio;
            CHECK(0 != pMJC->mFRRatio);
            pMJC->mOutFrmDuration = (pMJC->mInFrmDuration * 10) / pMJC->mFRRatio;
            pMJC->mLastTimestamp = 0;
            MJC_LOGD("[%d][%d] Drv Init success, InputNum: %d,OutputNum: %d,FRRatio: %d,OutFD: %d",
                     pMJC->mState,
                     pMJC->mMode,
                     pMJC->mInputNumRequire,
                     pMJC->mOutputNumRequire,
                     pMJC->mFRRatio,
                     pMJC->mOutFrmDuration);
            pMJC->mDrvInit = true;

            pMJC->mDrvConfig.u4Width = rConfig.u4Width;
            pMJC->mDrvConfig.u4Height = rConfig.u4Height;

            // Enable when succeed to init driver. Disable when entering bypass mode or main thread exits.
            if (pMJC->mbNeedToEnablePerfService == true) {
                pMJC->EnablePerfService();
            }

            if (mbSupport120Hz == true) {
                if (pMJC->mMode == MJC_MODE_NORMAL) {
                    if (cfgParam.u4OutFrmRate == 1200 && mbNotifyRRC == false) {
                        ((RefreshRateControl *)pRRC)->setScenario(RRC_TYPE_VIDEO_120HZ, true);
                        mbNotifyRRC = true;
                        MJC_LOGI("set RRC_TYPE_VIDEO_120HZ true");
                    }
                }
            }
        } else {
            pMJC->AtomicChangeMode(MJC_MODE_ADD, MJC_MODE_BYPASS);
            MJC_LOGE("[%d][%d] Drv Init fail, BP", pMJC->mState, pMJC->mMode);
            pMJC->mDrvInit = false;
        }

    } else {
        pMJC->AtomicChangeMode(MJC_MODE_ADD, MJC_MODE_BYPASS);
        MJC_LOGE("[%d][%d] Drv is NULL. BP", pMJC->mState, pMJC->mMode);
    }
}

void MJC::InitDumpYUV(void *pData) {
    MJC_ThreadParam *pParam = (MJC_ThreadParam *)pData;
    MtkOmxVdec *pVdec = (MtkOmxVdec *)(pParam->pUser);
    MJC *pMJC = (MJC *)(pParam->pMJC);

    char value[PROPERTY_VALUE_MAX];
    pMJC->u4DumpYUV = 0;
    pMJC->u4DumpCount = 0;

    property_get("mtk.mjc.dump", value, "0");
    pMJC->u4DumpYUV = atoi(value);
    property_get("mtk.mjc.dump.start", value, "0");
    pMJC->u4DumpStartTime = atoi(value);
    property_get("mtk.mjc.dump.type", value, "0");
    pMJC->u4DumpType = atoi(value);

    mbStartDump = false;

}


void MJC::BypassModeRoutine(void *pData) {
    MJC_ThreadParam *pParam = (MJC_ThreadParam *)pData;
    MtkOmxVdec *pVdec = (MtkOmxVdec *)(pParam->pUser);
    MJC *pMJC = (MJC *)(pParam->pMJC);

    MJC_INT_BUF_INFO *pMJCBufInfo;
    OMX_BUFFERHEADERTYPE *pBuffHdr;
    OMX_TICKS pBufferHdrTS;
    // Return all Reference frame
    while (pMJC->mRefBufQ.size() > 0) {
        pMJCBufInfo = pMJC->mRefBufQ[0];
        pBufferHdrTS = pMJC->mRefBufQTS[0];

        if (pMJCBufInfo->pBufHdr != NULL && (pMJCBufInfo->nFilledLen != 0 || pMJCBufInfo->nFlags & OMX_BUFFERFLAG_EOS)) {
            //Return Ref Buffer that has not been PDF yet
            //Check pMJCBufInfo->nFilledLen != 0  for runtime switching to bypass mode
            //Check pMJCBufInfo->nFlags for for EOS in start
            MJC_LOGD("[%d][%d] PDF, 0x%08X, ts = %lld", pMJC->mState, pMJC->mMode, (unsigned int)pMJCBufInfo->pBufHdr, pBufferHdrTS);
            PDFAndSetMJCBufferFlag(pVdec, pMJCBufInfo);
        }
        MJCClearBufRef(pVdec, pMJCBufInfo);
    }

    // Send MJCScaler output frame to MJCScaler
    while (1) {
        pBuffHdr = pMJC->mCallbacks.pfnGetOutputFrame(pVdec);
        if (NULL == pBuffHdr) {
            if (mMJCLog) {
                MJC_LOGI("[%d] NNOB", pMJC->mState);
            }
            break;
        } else {
            pMJCBufInfo = MJCGetBufInfoFromOMXHdr(pVdec, pBuffHdr);
            if (pMJCBufInfo->nFlags & OMX_BUFFERFLAG_SCALER_FRAME) {
                pMJC->mScaler.FillThisBuffer(pMJCBufInfo);
            } else {
                pMJC->mOutputBufQ.add(pMJCBufInfo);
            }
        }
    }

    // Move VdoQ buffer to DispQ
    while (1) {
        pBuffHdr = pMJC->mCallbacks.pfnGetInputFrame(pVdec);

        if (pBuffHdr != NULL) {
            // 1. Move (pBuffHdr->nFilledLen>0) buffers and EOS buffer to Scaler
            // 2. Move (pBuffHdr->nFilledLen==0) buffers to OMXCODEC
            pMJCBufInfo = MJCGetBufInfoFromOMXHdr(pVdec, pBuffHdr);
            if (pMJCBufInfo->nFlags & OMX_BUFFERFLAG_EOS) {
                MJC_LOGD("[%d][%d] EOS, 0x%08X, ts = %lld, len = %d", pMJC->mState, pMJC->mMode, (unsigned int)pMJCBufInfo->pBufHdr, pMJCBufInfo->nTimeStamp, pMJCBufInfo->nFilledLen);
                pMJC->mScaler.EmptyThisBuffer(pMJCBufInfo);
                break;
            } else if (0 == pMJCBufInfo->nFilledLen) {
                MJC_LOGD("[%d][%d] PDF, 0x%08X, ts = %lld, len = %d", pMJC->mState, pMJC->mMode, (unsigned int)pMJCBufInfo->pBufHdr, pMJCBufInfo->nTimeStamp, pMJCBufInfo->nFilledLen);
                PDFAndSetMJCBufferFlag(pVdec, pMJCBufInfo);
                SIGNAL(pMJC->mScaler.mMJCScalerSem);
            } else {
                pMJC->mScaler.EmptyThisBuffer(pMJCBufInfo);
            }
        } else {
            break;
        }
    }

    // Get Ref frame from MJCScaler
    while (1) {
        MJC_INT_BUF_INFO *pMJCRefBufInfo;
        pMJCRefBufInfo = pMJC->mScaler.GetScaledBuffer();
        if (NULL != pMJCRefBufInfo) {
            MJC_LOGD("[%d][%d] PDF, 0x%08X, ts = %lld, len = %d", pMJC->mState, pMJC->mMode, (unsigned int)pMJCRefBufInfo->pBufHdr, pMJCRefBufInfo->nTimeStamp, pMJCRefBufInfo->nFilledLen);
            PDFAndSetMJCBufferFlag(pVdec, pMJCRefBufInfo);
        } else {
            if (mMJCDrainInputBuffer) {
                // Check if VdoBufQ and ScaledBufQ still have buffer
                LOCK(pMJC->mScaler.mScalerInputDrainedLock);
                bool isScalerDrained = pMJC->mScaler.mScalerInputDrained;

                LOCK(pVdec->mMJCVdoBufQLock);
                bool isVideoBufQEmpty = (pVdec->mMJCVdoBufCount == 0) ? true : false;

                if (isVideoBufQEmpty && isScalerDrained && (mScaler.mScaledBufQ.size() == 0)) {
                    MJC_LOGI("Signal pVdec->mMJCVdoBufQDrainedSem.");
                    mMJCDrainInputBuffer = false;
                    SIGNAL(pVdec->mMJCVdoBufQDrainedSem);
                }

                UNLOCK(pVdec->mMJCVdoBufQLock);
                UNLOCK(pMJC->mScaler.mScalerInputDrainedLock);
            }
            break;
        }

    }

}

void MJC::NormalModeRoutine(void *pData) {
    MJC_ThreadParam *pParam = (MJC_ThreadParam *)pData;
    MtkOmxVdec *pVdec = (MtkOmxVdec *)(pParam->pUser);
    MJC *pMJC = (MJC *)(pParam->pMJC);
    MJC_CfgParam *pcfgParam = &(pMJC->cfgParam);

    char value[PROPERTY_VALUE_MAX];
    int u4Debug = 0;
    int dimmed = 0;

    property_get("mtk.mjc.debug", value, "0");
    u4Debug = atoi(value);

    MJC_BUFFER_STATUS eBufferStatus = 0;

    //4  Prepare Output buffer
    ATRACE_INT("MJC buffer", pVdec->mMJCPPBufCount);
    ATRACE_INT("VDO buffer", pVdec->mMJCVdoBufCount);

    PrepareOutputBuffer(pVdec, &eBufferStatus);

    if (pMJC->mOutputBufQ.size() < pMJC->mOutputNumRequire) {
        //MJC_LOGD("[%d] insuff. OB %d < %d", pMJC->mState, pMJC->mOutputBufQ.size(), pMJC->mOutputNumRequire);
        return;
    }

    PrepareInputBuffer(pVdec, &eBufferStatus);

    PrepareRefBuffer(pVdec, &eBufferStatus);

    //MJC_LOGI("[%d] NNOB/NNRB/NNIB(0x%03x)", pMJC->mState, eBufferStatus);
    if (mMJCLog) {
        MJC_LOGI("[%d][%d] RQ: %d, OQ: %d, MJC Q: %d, Vdo Q: %d", pMJC->mState, pMJC->mMode, pMJC->mRefBufQ.size(), pMJC->mOutputBufQ.size(), pVdec->mMJCPPBufCount, pVdec->mMJCVdoBufCount);
    }

    if (pMJC->mRefBufQ.size() < pMJC->mInputNumRequire && pMJC->mEOSReceived == false) {
        //MJC_LOGD("[%d] insuff. Ref Buf %d < %d", pMJC->mState, pMJC->mRefBufQ.size(), pMJC->mInputNumRequire);
        return;
    }

    if (pMJC->mRefBufQ.size() == 0 && pMJC->mEOSReceived == true) {
        MJC_LOGD("[%d][%d] Already EOS!!", pMJC->mState, pMJC->mMode);
        return;
    }

    if (pMJC->mRefBufQ[0]->nFlags & OMX_BUFFERFLAG_EOS) {
        MJC_LOGD("[%d][%d] PDF, 0x%08X, ts = %lld, EOS!! Remove from ref Q", pMJC->mState, pMJC->mMode, (unsigned int)pMJC->mRefBufQ[0]->pBufHdr, pMJC->mRefBufQ[0]->nTimeStamp);
        pMJC->PDFAndSetMJCBufferFlag(pVdec, pMJC->mRefBufQ[0]);
        pMJC->MJCClearBufRef(pVdec, pMJC->mRefBufQ[0]);
        return;
    }

    unsigned int u4Forceratio = pMJC->ForceTriggerRatio(mInFrmRate);
    if (u4Forceratio > 0) {
        MJC_LOGD("u4Forceratio(%d), mInFrmRate(%d), start(%lld), end(%lld)", u4Forceratio, mInFrmRate, meSlowMotionSection.StartTime, meSlowMotionSection.EndTime);
    }
    bool bNormalPlayback = (false == mbSlowMotionMode);
    bool bInSlowMotionSection = (true == mbSlowMotionMode && u4Forceratio > 0);
    bool bNotSlowMotionVideo = (true == mbSlowMotionMode && u4Forceratio == 0 && mInFrmRate < MJC_FR(pcfgParam->u4MaxFps));

    //MJC_LOGD("mSeek(%d), bNormalPlayback(%d), bInSlowMotionSection(%d), bNotSlowMotionVideo(%d)", mSeek, bNormalPlayback, bInSlowMotionSection, bNotSlowMotionVideo);

    // + Call drv process
    if (false == pMJC->mSeek && (bNormalPlayback || bNotSlowMotionVideo || bInSlowMotionSection)) {
        unsigned int i;
        MJC_DRV_NEXT_INFO_T rProcResult;
        MJC_DRV_FRAME_T rFrame;
        pMJC->mState = MJC_STATE_RUNNING;
        memset((void *)&rFrame, 0, sizeof(MJC_DRV_FRAME_T));
        memset((void *)&rProcResult, 0, sizeof(MJC_DRV_NEXT_INFO_T));
        rFrame.u4InputNum = pMJC->mInputNumRequire;

        // Translate Reference Frame's VA to PA
        for (i = 0; i < rFrame.u4InputNum; i++) {
            if (pMJC->mRefBufQ[i]->nFlags & OMX_BUFFERFLAG_EOS) {
                break;
            } else {
                rFrame.u4InputAddress[i] = pMJC->mRefBufQ[i]->u4MVA;
                MJC_LOGV("[%d][%d] Set Ref frame , %d,0x%08X, 0x%08X", pMJC->mState, pMJC->mMode, i, (unsigned int)pMJC->mRefBufQ[i]->pBufHdr, (unsigned int)pMJC->mRefBufQ[i]->u4VA);
            }
        }

        if (i == 1 && pMJC->mRefBufQ[i]->nFilledLen == 0) {
            MJC_LOGD("Last Ref frame detected(0x%08X) (0x%08X) PDF and ts = %lld", (unsigned int)pMJC->mRefBufQ[0]->pBufHdr, (unsigned int)pMJC->mRefBufQ[0]->u4VA, pMJC->mRefBufQ[0]->nTimeStamp);
            if (pMJC->mRefBufQ[0]->mIsFilledBufferDone != true) {
                pMJC->PDFAndSetMJCBufferFlag(pVdec, mRefBufQ[0]);
            }
            MJCClearBufRef(pVdec, mRefBufQ[0]);
            SIGNAL(pMJC->mMJCFrameworkSem);
            return;
        } else if (i == 1 && pMJC->mRefBufQ[i]->nFilledLen != 0) {

            if (pMJC->mRefBufQ[0]->mIsFilledBufferDone == true) {
                MJC_LOGD("Ref[0] is shtited and Ref[1] is EOS with data. Clear Ref[0] (0x%08X) (0x%08X) ts = %lld",
                         (unsigned int)pMJC->mRefBufQ[0]->pBufHdr, (unsigned int)pMJC->mRefBufQ[0]->u4VA, pMJC->mRefBufQ[0]->nTimeStamp);
                MJCClearBufRef(pVdec, mRefBufQ[0]);
                //Clear Ref[0] here and disallow interpolation for the last pair
                SIGNAL(pMJC->mMJCFrameworkSem);
                return;
            }
            //if EOS frame containing real decoded data as ref if no original frame shifted in the end
            rFrame.u4InputAddress[i] = pMJC->mRefBufQ[i]->u4MVA;
            MJC_LOGD("[%d][%d] Set Ref frame (EOS), %d,0x%08X, 0x%08X", pMJC->mState, pMJC->mMode, i, (unsigned int) pMJC->mRefBufQ[i]->pBufHdr, (unsigned int)pMJC->mRefBufQ[i]->u4VA);
            i++;
        }

        // This may happen when we found EOS during assigning Reference Frame's MVA
        for (; i < rFrame.u4InputNum; i++) {
            MJC_LOGV("[%d][%d] Set Ref frame in EOS case, i = %d", pMJC->mState, pMJC->mMode, i);
            CHECK(0 != i);
            rFrame.u4InputAddress[i] = rFrame.u4InputAddress[i - 1]; // Remove the EOS frame
        }

        if (true == pMJC->mCheckDFR) {
            // FrameRateDetect
            // The retruned frame-rate from FrameRateDetec() would be calculated by the first 2 frames or average duration of all ref frames
            unsigned int u4FR = pMJC->FrameRateDetect(&pMJC->mDFR, &rFrame.u1LFRDepth, &rFrame.u1LFRPosition);
            if (pMJC->mInFrmRate != u4FR || true == pMJC->mDFR) {
                rFrame.u2UpdateInFrmRate = pMJC->mInFrmRate = u4FR;
                rFrame.u2UpdateOutFrmRate = pcfgParam->u4OutFrmRate;
                MJC_LOGD("[%d] FRD: %d, DFR: %d", pMJC->mState, pMJC->mInFrmRate, pMJC->mDFR);
            }
            rFrame.u1DFR = pMJC->mDFR;
        }

        rFrame.u4OutputNum = pMJC->mOutputNumRequire;

        //#ifdef MTK_16X_SLOWMOTION_VIDEO_SUPPORT
        if (u4Forceratio > 0) {
            // Assign fake framerate to trigger driver
            rFrame.u2UpdateInFrmRate = 150;
            rFrame.u2UpdateOutFrmRate = 600;

        }
        //#endif

        // Assing Output Frame's MVA
        for (i = 0; i < rFrame.u4OutputNum; i++) {
            CHECK(NULL != pMJC->mOutputBufQ[i]);
            rFrame.u4OutputAddress[i] = pMJC->mOutputBufQ[i]->u4MVA;
            MJC_LOGV("[%d] set OutputAddr: %d,0x%08X", pMJC->mState, i, rFrame.u4OutputAddress[i]);
        }

        int64_t exetime = 0;
        int64_t in_time_1;
        in_time_1 = getTickCountMs();


        // Call MJC Driver to work
        {
            ATRACE_NAME("MJC Process");

            SyncDumpFrmNum(&rFrame);

            MJC_DRV_RESULT_T res = mfn_eMjcDrvProcess(pMJC->mDrv, &rFrame, &rProcResult);

            if (MJC_DRV_RESULT_OK != res) {
                MJC_LOGE("[%d] Drv Process fail, NextInputNum: %d, NextOutputNum: %d ", pMJC->mState, rFrame.u4InputNum, rFrame.u4OutputNum);
                pMJC->AtomicChangeMode(MJC_MODE_ADD, MJC_MODE_BYPASS);
                SIGNAL(pMJC->mMJCFrameworkSem);
                return;
            } else {
                exetime = getTickCountMs() - in_time_1;

                if (true == pMJC->mCheckDFR && pMJC->mRefBufQ.size() > 1) {
                    if (pMJC->mRefBufQTS[1] > pMJC->mRefBufQTS[0]) {
                        pMJC->mInFrmDuration = pMJC->mRefBufQTS[1] - pMJC->mRefBufQTS[0];
                        pMJC->mFRRatio = rProcResult.u1FRRatio;
                        pMJC->mOutFrmDuration = (pMJC->mInFrmDuration * 10) / pMJC->mFRRatio;
                        if (mMJCLog) {
                            MJC_LOGI("[%d] ts 0: %lld, ts 1: %lld, FRRatio: %d, mOutFD: %d", pMJC->mState, pMJC->mRefBufQTS[0], pMJC->mRefBufQTS[1], pMJC->mFRRatio, pMJC->mOutFrmDuration);
                        }
                    }
                }

                pMJC->mInputNumRequire = rProcResult.u1NextInputNum;
                pMJC->mOutputNumRequire = rProcResult.u1NextOutputNum;
                pMJC->mCheckDFR = rProcResult.u1CheckDFR;
            }
        }
        MJC_LOGD("[%d] Drv Proc out: %d, NextInputNum: %d, NextOutputNum: %d, proc time: %lld ms, CheckDFR: %d", pMJC->mState, rProcResult.u1OutputOrgFrame, rProcResult.u1NextInputNum, rProcResult.u1NextOutputNum, exetime, pMJC->mCheckDFR);
        ATRACE_INT("MJC exetime (ms)", exetime);
        if ((exetime > 16 && pcfgParam->u4OutFrmRate == 600) || (exetime > 8 && pcfgParam->u4OutFrmRate == 1200)) {
            ATRACE_INT("MJC exetime (ms), overtime", exetime);
        } else {
            ATRACE_INT("MJC exetime (ms), overtime", 0);
        }

        SendToDisplay(pVdec, &rProcResult, &rFrame);

        if (rProcResult.u1UpdateFrame && u4Debug != 1) {
            MJC_INT_BUF_INFO *pMJCBufInfo;
            pMJCBufInfo = pMJC->mRefBufQ[0];
            if (pMJCBufInfo->nFilledLen != 0) {
                MJC_LOGD("[%d][%d] PutNOSHOWFrame %d, 0x%08X, ts = %lld, len = %d, addr = 0x%08X", pMJC->mState, pMJC->mMode, rProcResult.u1OutputOrgFrame, (unsigned int)pMJCBufInfo->pBufHdr, pMJCBufInfo->nTimeStamp, pMJCBufInfo->nFilledLen, (unsigned int)pMJCBufInfo->u4VA);

                // Need to display NOSHOWFrame when driver indicates fake engine
                if (rProcResult.u1NonProcessedFrame == 0) {
                    pMJCBufInfo->nFilledLen = pMJCBufInfo->pBufHdr->nFilledLen = 0;
                    pMJCBufInfo->nTimeStamp = pMJCBufInfo->pBufHdr->nTimeStamp = 0;
                }
                pMJC->PDFAndSetMJCBufferFlag(pVdec, pMJCBufInfo);
            }
            if (mMJCLog) {
                MJC_LOGD("[%d][%d] Update ref Frame 0x%08X, len = %d, ts = %lld", pMJC->mState, pMJC->mMode, (unsigned int)pMJCBufInfo->pBufHdr, pMJCBufInfo->nFilledLen, pMJCBufInfo->nTimeStamp);
            }
            MJCClearBufRef(pVdec, pMJCBufInfo);
        }

        pMJC->mState = MJC_STATE_READY;
    } // false == pMJC->mSeek
    else {
        // We're seeking
        SendRefToDispInSeeking(pVdec);
    } // true == pMJC->mSeek

    //MJC_LOGD("[%d][%d] MJC Q: %d, Vdo Q: %d", pMJC->mState, pMJC->mMode, pVdec->mMJCPPBufCount, pVdec->mMJCVdoBufCount);

    if ((pVdec->mMJCVdoBufCount > 0 && pVdec->mMJCPPBufCount > 0) ||
        (pMJC->mOutputBufQ.size() >= pMJC->mOutputNumRequire && pMJC->mRefBufQ.size() >= pMJC->mInputNumRequire) ||
        (pMJC->mEOSReceived == true)
       ) {
        SIGNAL(pMJC->mMJCFrameworkSem); // Trigger next round when the resources are ready.
    }


    //#ifdef MTK_16X_SLOWMOTION_VIDEO_SUPPORT
    int u4Enable = 0;

    property_get("persist.sys.display.clearMotion", value, "0");
    u4Enable = atoi(value);

    bool bNotSlowmotionVideo = (pMJC->mInFrmRate < MJC_FR(pcfgParam->u4MaxFps) && meSlowMotionSection.StartTime == 0 && meSlowMotionSection.EndTime == 0);
    //#endif

    property_get("sys.display.clearMotion.dimmed", value, "0");
    dimmed = atoi(value);

    bool bLowFR = (pMJC->mInFrmRate < pcfgParam->u4MinFps * 10);

    if (1 == dimmed || (0 == u4Enable && bNotSlowmotionVideo) || bLowFR) {
        pMJC->AtomicChangeMode(MJC_MODE_ADD, MJC_MODE_BYPASS);
        MJC_LOGD("[%d][%d] dimmed: %d, global setting: %d, frame rate: %d, BYPASS MODE", pMJC->mState, pMJC->mMode, dimmed, u4Enable, pMJC->mInFrmRate);
    }

}

void MJC::DefaultRoutine(void *pData) {
    MJC_ThreadParam *pParam = (MJC_ThreadParam *)pData;
    MtkOmxVdec *pVdec = (MtkOmxVdec *)(pParam->pUser);
    MJC *pMJC = (MJC *)(pParam->pMJC);

    // FLUSH MODE
    {
        MJC_INT_BUF_INFO *pMJCBufInfo;
        OMX_BUFFERHEADERTYPE *pBuffHdr;
        OMX_TICKS pBuffHdrTS;

        pMJC->mState = MJC_STATE_FLUSH;

        // Ask Scaler to enter Flush mode
        MJCScaler_MODE eScalerMode = MJCScaler_MODE_FLUSH;
        pMJC->mScaler.SetParameter(MJCScaler_PARAM_MODE, &eScalerMode);
        SIGNAL(pMJC->mScaler.mMJCScalerSem);
        //MJC_LOGD("[%d][%d] send mMJCScalerSem", pMJC->mState, pMJC->mMode);
        WAIT(pMJC->mScaler.mMJCScalerFlushDoneSem);
        pMJC->mScaler.GetParameter(MJCScaler_PARAM_MODE, &eScalerMode);

        // Move MJC  buffer to DispQ
        while (1) {
            //default routine: buffers from PPQ bypass MJC internal queue
            pBuffHdr = pMJC->mCallbacks.pfnGetOutputFrame(pVdec);
            if (pBuffHdr != NULL) {
                MJC_LOGD("[%d][%d] PDF OB, 0x%08X, ts = %lld, len = %d", pMJC->mState, pMJC->mMode, (unsigned int)pBuffHdr, pBuffHdr->nTimeStamp, pBuffHdr->nFilledLen);
                pBuffHdr->nFilledLen = 0;
                pMJC->mCallbacks.pfnPutDispFrame(pVdec, pBuffHdr);
            } else {
                break;
            }
        }

        // Return all Reference frame
        while (pMJC->mRefBufQ.size() > 0) {
            pMJCBufInfo = pMJC->mRefBufQ[0];
            pBuffHdrTS = pMJC->mRefBufQTS[0];

            if (pMJCBufInfo->nFlags & OMX_BUFFERFLAG_EOS) {
                if (pMJCBufInfo->pBufHdr != NULL) {
                    MJC_LOGD("[%d][%d] PDF Ref Q (EOS), 0x%08X, ts = %lld, FilledLen = %d",
                             pMJC->mState, pMJC->mMode, (unsigned int)pMJCBufInfo->pBufHdr, pBuffHdrTS, pMJCBufInfo->nFilledLen);
                    PDFAndSetMJCBufferFlag(pVdec, pMJCBufInfo);
                }
                pMJC->MJCClearBufRef(pVdec, pMJCBufInfo);
                CHECK(pMJC->mRefBufQ.size() == 0 && pMJC->mRefBufQTS.size() == 0);
                break;
            }

            if (pMJCBufInfo->nFilledLen != 0 && pMJCBufInfo->pBufHdr != NULL) {
                MJC_LOGD("[%d][%d] PDF Ref Q, 0x%08X, ts = %lld", pMJC->mState, pMJC->mMode, (unsigned int)pMJCBufInfo->pBufHdr, pBuffHdrTS);
                PDFAndSetMJCBufferFlag(pVdec, pMJCBufInfo);
            }
            MJCClearBufRef(pVdec, pMJCBufInfo);
        }

        // Return all MJC  frame
        while (pMJC->mOutputBufQ.size() > 0) {
            pMJCBufInfo = pMJC->mOutputBufQ[0];
            MJC_LOGD("[%d][%d] PMF, 0x%08X, ts = %lld", pMJC->mState, pMJC->mMode, (unsigned int)pMJCBufInfo->pBufHdr, pMJCBufInfo->pBufHdr->nTimeStamp);
            pMJCBufInfo->pBufHdr->nFilledLen = 0;
            PDFAndSetMJCBufferFlag(pVdec, pMJCBufInfo);
            pMJC->mOutputBufQ.removeItemsAt(0);
        }

        // Move VdoQ buffer to DispQ
        while (1) {
            pBuffHdr = pMJC->mCallbacks.pfnGetInputFrame(pVdec);

            if (pBuffHdr != NULL) {
                MJC_LOGD("[%d][%d] PDF IB, 0x%08X, ts = %lld, len = %d", pMJC->mState, pMJC->mMode, (unsigned int)pBuffHdr, pBuffHdr->nTimeStamp, pBuffHdr->nFilledLen);
                //default routine: no need to set buffer flag
                pMJC->mCallbacks.pfnPutDispFrame(pVdec, pBuffHdr);
            } else {
                break;
            }
        }


        //4  Call drv reset
        if ((0 != pMJC->mDrv)  && pMJC->mDrvInit == true) {
            MJC_DRV_RESET_CONFIG_T prConfig;
            MJC_DRV_RESET_INFO_T rRestResult;
            memset((void *)&prConfig, 0, sizeof(MJC_DRV_RESET_CONFIG_T));
            memset((void *)&rRestResult, 0, sizeof(MJC_DRV_RESET_INFO_T));

            MJC_DRV_RESULT_T res = mfn_eMjcDrvReset(pMJC->mDrv, &prConfig,  &rRestResult);
            pMJC->mCheckDFR = true;
            pMJC->mLastTimestamp = 0;

            if (res == MJC_DRV_RESULT_OK) {
                CHECK(0 != rRestResult.u1FRRatio);
                pMJC->mFRRatio = rRestResult.u1FRRatio;
                pMJC->mInputNumRequire = rRestResult.u1NextInputNum;
                pMJC->mOutputNumRequire = rRestResult.u1NextOutputNum;
                pMJC->mOutFrmDuration = (pMJC->mInFrmDuration * 10) / pMJC->mFRRatio;
                MJC_LOGD("[%d][%d] Drv Reset success, InputNum: %d, OutputNum: %d, FRRatio: %d, OutFD: %d", pMJC->mState, pMJC->mMode, pMJC->mInputNumRequire, pMJC->mOutputNumRequire, pMJC->mFRRatio, pMJC->mOutFrmDuration);
            } else {
                pMJC->AtomicChangeMode(MJC_MODE_ADD, MJC_MODE_BYPASS);
            }
        }

        pMJC->AtomicChangeMode(MJC_MODE_SET, (~MJC_MODE_FLUSH) & pMJC->mMode);

        unsigned int u4Width;
        unsigned int u4Height;

        pMJC->mScaler.GetParameter(MJCScaler_PARAM_MODE, &eScalerMode);
        if (MJCScaler_MODE_NORMAL == eScalerMode) {
            u4Width = pMJC->mBuffer.u4Width;
            u4Height = pMJC->mBuffer.u4Height;
        } else {
            u4Width = pMJC->mFrame.u4Width;
            u4Height = pMJC->mFrame.u4Height;
        }

        MJC_LOGD("[%d][%d] ScalerMode = %d, W = %d, H = %d", pMJC->mState, pMJC->mMode, eScalerMode, u4Width, u4Height);

        if (pMJC->mDrvInit == true && u4Width == pMJC->mDrvConfig.u4Width && u4Height == pMJC->mDrvConfig.u4Height) {
            // TODO: check format change
            pMJC->mState = MJC_STATE_READY;
        } else {
            pMJC->mState = MJC_STATE_INIT;

            MJC_LOGD("[%d][%d] Change to INIT, %d, %d, %d, %d", pMJC->mState, pMJC->mMode, pMJC->mDrvConfig.u4Width, pMJC->mDrvConfig.u4Height, u4Width, u4Height);

            if (0 != pMJC->mDrv && pMJC->mDrvInit == true) {
                MJC_DRV_RESULT_T ret;
                MJC_LOGD("[%d][%d] DrvDeInit, %d, %d, %d, %d", pMJC->mState, pMJC->mMode, pMJC->mDrvConfig.u4Width, pMJC->mDrvConfig.u4Height, pMJC->mFrame.u4Width, pMJC->mFrame.u4Height);
                ret = mfn_eMjcDrvDeInit(pMJC->mDrv);
                CHECK(MJC_DRV_RESULT_OK == ret);
                pMJC->mDrvInit = false;
                DisablePerfService();
            }
        }

        MJC_LOGD("Check No More input frame");
        pBuffHdr = pMJC->mCallbacks.pfnGetInputFrame(pVdec);
        if (NULL != pBuffHdr) {
            MJC_LOGE("VdoQ not free! 0x%08X, ts = %lld, len = %d", (unsigned int)pBuffHdr, pBuffHdr->nTimeStamp, pBuffHdr->nFilledLen);
        }

        MJC_LOGD("Check No More OutputBuf and RefBuf");
        CHECK(NULL == pBuffHdr);
        CHECK(0 == pMJC->mOutputBufQ.size());
        CHECK(0 == pMJC->mRefBufQ.size() && 0 == pMJC->mRefBufQTS.size());

        MJC_LOGD("Check No More Internal Buffer. mMJCBufInfoNum(%d)", mMJCBufInfoNum);
        if (mMJCBufInfoNum > 0) {
            for (int u4I = 0; u4I < MAX_TOTAL_BUFFER_CNT; u4I++) {
                if (mMJCBufInfoQ[u4I].u4BufferHandle != 0xffffffff) {
                    MJC_LOGE("[%d][%d] mMJCBufInfoQ[%d].pBufHdr = 0x%X (0x%X) is not freed. len = %d, ts =%lld", pMJC->mState, pMJC->mMode,
                             u4I, (unsigned int)mMJCBufInfoQ[u4I].pBufHdr, mMJCBufInfoQ[u4I].u4BufferHandle, mMJCBufInfoQ[u4I].nFilledLen, mMJCBufInfoQ[u4I].nTimeStamp);
                }
            }
        }
        CHECK(0 == mMJCBufInfoNum);

        pMJC->mEOSReceived = false;

        //MJC_LOGD("[%d][%d] send mMJCFlushBufQDoneSem", pMJC->mState, pMJC->mMode);
        MJC_LOGD("[%d][%d] signal FlushDoneSem, outputQ:%d, refQ:%d", pMJC->mState, pMJC->mMode, pMJC->mOutputBufQ.size(), pMJC->mRefBufQ.size());
        SIGNAL(pVdec->mMJCFlushBufQDoneSem);

    }

}


/*****************************************************************************
 * FUNCTION
 *    SetApkParameter
 * DESCRIPTION
 *    1. According to the parameter type to fill the value.
 * PARAMETERS
 *    param1 : [IN]  MJC_CfgParam pParam
 *                MJC tuning apk parameters
 * RETURNS
 *    none
 ****************************************************************************/
void MJC::SetApkParameter(MJC_CfgParam *pParam) {
    unsigned short value[2];
    char uc_value[PROPERTY_VALUE_MAX];
    int num = 0;
    FILE *fp = NULL;

    /* Engineer Mode */
    unsigned int u4EngModeEnableCustomization = 0;
    char szEngModeEnableCustomization[PROPERTY_VALUE_MAX];
    unsigned int u4EngModeDemoType = 0;
    char szEngModeDemoType[PROPERTY_VALUE_MAX];

    // Check if we should fopen 'SUPPORT_CLEARMOTION'
    property_get("sys.display.mjc.customer", szEngModeEnableCustomization, "0");
    u4EngModeEnableCustomization = atoi(szEngModeEnableCustomization);

    // Check which demo mode is used
    property_get("sys.display.mjc.demo", szEngModeDemoType, "0");
    u4EngModeDemoType = atoi(szEngModeDemoType);


    if (1 == u4EngModeEnableCustomization) {
        pParam->u4DemoMode = 0;

        // Allow to enable demo mode by SUPPORT_CLEARMOTION only when user or userdebug load.
        /*
        unsigned int u4EnableDemo = 0;
        char szLoadType[PROPERTY_VALUE_MAX];
        property_get("ro.build.type", szLoadType, "0");
        MJC_LOGD("%s", szLoadType);
        if (strcmp(szLoadType, "user") == 0 || strcmp(szLoadType, "userdebug") == 0)
        {
            u4EnableDemo = 1;
        }

        if (1 == u4EnableDemo)
        */

        if (1) {
            MJC_LOGD("Checking SUPPORT_CLEARMOTION");

            fp = fopen(SUPPORT_CLEARMOTION_PATH0, "rb"); //check phone storage first
            if (!fp) {
                MJC_LOGD("sdcard0 SUPPORT_CLEARMOTION not exist");

                // check ext sdcard then.
                if (1) { // Todo: enable search SUPPORT_CLEARMOTION in ext sdcard
                    fp = fopen(SUPPORT_CLEARMOTION_PATH1, "rb");
                    if (!fp) {
                        MJC_LOGD("sdcard1 SUPPORT_CLEARMOTION not exist");
                    } else {
                        MJC_LOGI("[1] found SUPPORT_CLEARMOTION!");
                    }
                } else {
                    MJC_LOGD("disable find sdcard1");
                }
            } else {
                MJC_LOGI("[0] found SUPPORT_CLEARMOTION !");
            }
        }

    }

    if (fp) {
        num = fread(value, 1, TUNING_DATA_NUM, fp);

        if (num == TUNING_DATA_NUM) {
            value[0] = value[0] >> 8;
            value[1] = value[1] >> 8;

            pParam->u4NrmFbLvlIdx = value[0];
            if (value[1] <= 2) {
                pParam->u4DemoMode = value[1];
            }
        } else {
            MJC_LOGE("read SUPPORT_CLEARMOTION fail");
        }
        fclose(fp);

    } else {
        property_get("sys.display.clearMotion.demo", uc_value, "0");
        pParam->u4DemoMode = atoi(uc_value);
    }

    // Demo mode set by Engineer Mode will overwrite all others settings.
    if (0 != u4EngModeDemoType) {
        pParam->u4DemoMode = u4EngModeDemoType;
    }

    MJC_LOGD("Nrm bdr Demo %d %d %d", pParam->u4NrmFbLvlIdx, pParam->u4BdrFbLvlIdx, pParam->u4DemoMode);
}


/*****************************************************************************
 * FUNCTION
 *    SetFallBackIndex
 * DESCRIPTION
 *    1. According to the parameter to fill the value.
 *    2. This function must be invoked before SetApkParameter, otherwise tuning apk is useless
 * PARAMETERS
 *    param1 : [IN]  MJC_CfgParam pParam
 *                Tuned fallback parameters
 * RETURNS
 *    none
 ****************************************************************************/
void MJC::SetFallBackIndex(MJC_CfgParam *pParam) {
    int num = 0;
    unsigned char value[2];
    FILE *fp = fopen("/etc/mtk_mjc.cfg", "rb");

    pParam->u4NrmFbLvlIdx = 255;
    pParam->u4BdrFbLvlIdx = 255;

    if (fp) {
        num = fread(value, 1, 1, fp);
        if (num == 1) {
            pParam->u4NrmFbLvlIdx = value[0];
        } else {
            MJC_LOGE("read mtk_mjc.cfg fail");
        }
        fclose(fp);
    } else {

        MJC_LOGE("unable to open mjc cfg file.");
    }

}

bool MJC::ParseMjcConfig(MJC_CfgParam *pParam) {
    MJC_LOGD("+ParseMjcConfig");
    bool ret = false;
    FILE *fp = NULL;
    char str[MAX_LINE_LEN] = {0};
    unsigned int cur_idx = 0;
    unsigned int mjcCfgCounts = 0;
    mjc_cfg_type *mjcCfgs = NULL;

    //default vaule
    pParam->u4NrmFbLvlIdx = 255;
    pParam->u4BdrFbLvlIdx = 255;
    pParam->u4DemoMode = 0;
    pParam->u4MaxWidth = 1920;
    pParam->u4MaxHeight = 1088;
    pParam->u4MinWidth = 256;//1280;
    pParam->u4MinHeight = 120;//720;
    pParam->u4MinFps = 12;
    pParam->u4MaxFps = 35;//30;

    char value[PROPERTY_VALUE_MAX];
    int MJC120hz;

    property_get("ro.mtk_display_120hz_support", value, "0");
    MJC120hz = atoi(value);

    if (MJC120hz == 0) {
        pParam->u4OutFrmRate = 600;
    } else {
        pParam->u4OutFrmRate = 1200;
    }

    MJC_LOGI("%d fps mode by default value", (pParam->u4OutFrmRate / 10));


    if ((fp = fopen("/vendor/etc/mtk_clear_motion.cfg", "r")) == NULL) {
        MJC_LOGE("ParseMjcConfig failed. Can't open %s", "/etc/mtk_clear_motion.cfg");
        ret = false;
        goto EXIT;
    }

    mjcCfgCounts = 0;
    // calculate line count to know how many components we have

    while (fgets(str, MAX_LINE_LEN, fp) != NULL) {
        mjcCfgCounts++;;
    }

    if (mjcCfgCounts == 0) {
        MJC_LOGE("[ERROR] No core component available");
        ret = false;
        goto EXIT;
    }

    MJC_LOGE("ParseMtkCoreConfig: mjcCfgCounts = %d", mjcCfgCounts);

    fseek(fp, 0, SEEK_SET);

    // allocate memory for mjcCfgs
    mjcCfgs = (mjc_cfg_type *)malloc(sizeof(mjc_cfg_type) * mjcCfgCounts);
    memset(mjcCfgs, 0, sizeof(mjc_cfg_type)*mjcCfgCounts);

    if (NULL == mjcCfgs) {
        MJC_LOGE("[ERROR] out of memory to allocate mjcCfgs");
        ret = false;
        goto EXIT;
    }

    // parse OMX component name, role, and path
    while (fgets(str, MAX_LINE_LEN, fp) != NULL) {
        char *pch;
        pch = strtok(str, DELIMITERS);

        while (pch != NULL && sizeof(pch) < MJC_CFG_STR_LEN) {
            strlcpy((char *)mjcCfgs[cur_idx].name, pch, sizeof(mjcCfgs[cur_idx].name));
            pch = strtok(NULL, DELIMITERS);
            if (pch != NULL && sizeof(pch) < MJC_CFG_STR_LEN) {
                strlcpy((char *)mjcCfgs[cur_idx].value, pch, sizeof(mjcCfgs[cur_idx].value));
            }
            pch = strtok(NULL, DELIMITERS);
        }

        cur_idx++;
    }

    for (unsigned int i = 0; i < mjcCfgCounts; i++) {
        if (!strcmp("FLUENCY_LEVEL", (const char *)mjcCfgs[i].name)) {
            pParam->u4NrmFbLvlIdx = atoi((const char *)mjcCfgs[i].value);
        } else if (!strcmp("MIN_WIDTH", (const char *)mjcCfgs[i].name)) {
            pParam->u4MinWidth = atoi((const char *)mjcCfgs[i].value);
        } else if (!strcmp("MIN_HEIGHT", (const char *)mjcCfgs[i].name)) {
            pParam->u4MinHeight = atoi((const char *)mjcCfgs[i].value);
        } else if (!strcmp("MAX_WIDTH", (const char *)mjcCfgs[i].name)) {
            pParam->u4MaxWidth = atoi((const char *)mjcCfgs[i].value);
        } else if (!strcmp("MAX_HEIGHT", (const char *)mjcCfgs[i].name)) {
            pParam->u4MaxHeight = atoi((const char *)mjcCfgs[i].value);
        } else if (!strcmp("MIN_FRAMERATE", (const char *)mjcCfgs[i].name)) {
            pParam->u4MinFps = atoi((const char *)mjcCfgs[i].value);
        } else if (!strcmp("MAX_FRAMERATE", (const char *)mjcCfgs[i].name)) {
            pParam->u4MaxFps = atoi((const char *)mjcCfgs[i].value);
        } else if (!strcmp("OUTPUT_FRAMERATE", (const char *)mjcCfgs[i].name) && MJC120hz != 0) {
            pParam->u4OutFrmRate = atoi((const char *)mjcCfgs[i].value);
            MJC_LOGI("%d fps mode by cfg file", pParam->u4OutFrmRate);
            pParam->u4OutFrmRate *= 10;
        } else {
            MJC_LOGD("[ParseMtkCoreConfig] No such config: %s", mjcCfgs[i].name);
        }
    }

EXIT:

    if (MJC120hz != 0) {
        property_get("mtk.mjc.120mode", value, "1");

        if (0 == atoi(value)) {
            pParam->u4OutFrmRate = 600;
            MJC_LOGI("60 fps mode by mtk.mjc.120mode");
        }
    }
    ret = true;

    if (fp) {
        fclose(fp);
    }

    if (mjcCfgs) {
        free(mjcCfgs);
    }
    MJC_LOGD("-ParseMjcConfig");
    return ret;
}


