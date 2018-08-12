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
#include "MJCScaler.h"
#include "MtkOmxVdecEx.h"
#include <utils/AndroidThreads.h>

#include <cutils/properties.h>

#include <media/stagefright/foundation/ADebug.h>
#include <utils/Trace.h>

#undef LOG_TAG
#define LOG_TAG "MJCScaler"

#undef ATRACE_TAG
#define ATRACE_TAG ATRACE_TAG_VIDEO

#define MTK_LOG_ENABLE 1
#include <cutils/log.h>

#define MJC_LOGV ALOGV

#define MJCSCALER_DEBUG
#ifdef MJCSCALER_DEBUG
#define MJC_LOGD(fmt, arg...) \
if (this->mMJCScalertagLog) \
{  \
    ALOGD(fmt, ##arg); \
}
#else
#define MJC_LOGD
#endif

#define MJCSCALER_INFO
#ifdef MJCSCALER_INFO
#define MJC_LOGI(fmt, arg...) \
if (this->mMJCScalertagLog) \
{  \
    ALOGI(fmt, ##arg); \
}
#else
#define MJC_LOGI
#endif

#define MJC_LOGE ALOGE

#define LOCK_TIMEOUT_MS 5000
#define LOCK_TIMEOUT_S 5

#undef WAIT_T
#define WAIT_T(X) \
    if (0 != WAIT_TIMEOUT(X,LOCK_TIMEOUT_S)){ \
        MJC_LOGI("[%d][%d] IQ: %d, OQ: %d, SQ: %d", \
                 pMJCScaler->mState, \
                 pMJCScaler->mMode, \
                 pMJCScaler->mInputBufQ.size(), \
                 pMJCScaler->mOutputBufQ.size(), \
                 pMJCScaler->mScaledBufQ.size()); \
        MJC_LOGI("## [ERROR] %s() line: %d WAIT mMJCScalerSem timeout...", __FUNCTION__,__LINE__); \
        WAIT(X);\
    }

static int64_t getTickCountMs() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)(tv.tv_sec * 1000LL + tv.tv_usec / 1000);
}

/*****************************************************************************
 * FUNCTION
 *    MtkMJCScalerThread
 * DESCRIPTION

 * PARAMETERS
 *    param1 : [IN]  void* pData
 *                Pointer of thread data.  (MJC object handle and omx component handle)
 * RETURNS
 *    Always return NULL.
 ****************************************************************************/
void *MtkMJCScalerThread(void *pData) {
    MJCScaler_ThreadParam *pParam = (MJCScaler_ThreadParam *)pData;
    MJC *pMJC = (MJC *)(pParam->pUser);
    MJCScaler *pMJCScaler = (MJCScaler *)(pParam->pMJCScaler);

    ALOGD("MtkMJCScalerThread created pMJC=0x%08X, pMJCScaler=0x%08X, tid=%d", (unsigned int)pMJC, (unsigned int)pMJCScaler, gettid());
    prctl(PR_SET_NAME, (unsigned long)"MtkMJCScalerThread", 0, 0, 0);

    androidSetThreadPriority(0, ANDROID_PRIORITY_URGENT_DISPLAY + 1);

    while (1) {
        //OMX_BUFFERHEADERTYPE* pInBufHdr = NULL;
        //OMX_BUFFERHEADERTYPE* pOutBufHdr = NULL;
        if (true == pMJC->mTerminated) {
            ALOGD("[%d][%d] check mTerminated: %d", pMJCScaler->mState, pMJCScaler->mMode, pMJC->mTerminated);
            break;
        }

        if (MJCScaler_STATE_INIT == pMJCScaler->mState) {
            pMJCScaler->mState = MJCScaler_STATE_READY;
        }

        if (pMJCScaler->mState != MJCScaler_STATE_FLUSH) {
            /*ALOGD("[%d][%d] IQ: %d, OQ: %d, SQ: %d",
             pMJCScaler->mState,
             pMJCScaler->mMode,
             pMJCScaler->mInputBufQ.size(),
             pMJCScaler->mOutputBufQ.size(),
             pMJCScaler->mScaledBufQ.size());*/

            int64_t in_time_1 = getTickCountMs();
            int64_t out_time_1 = 0;
            WAIT(pMJCScaler->mMJCScalerSem);

            out_time_1 = getTickCountMs() - in_time_1;
            if (pMJCScaler->mMJCLog) {
                if (out_time_1 > 30) {
                    ALOGD("[%d][%d] Scaler(%lld)", pMJCScaler->mState, pMJCScaler->mMode, out_time_1);
                }
            }
        }

        switch (pMJCScaler->mMode) {
        case MJCScaler_MODE_BYPASS:
            // BYPASS MODE
            pMJCScaler->BypassModeRoutine(pData);
            break;

        case MJCScaler_MODE_NORMAL:
            // NORMAL MODE
            pMJCScaler->NormalModeRoutine(pData);
            break;
        default:
            // FLUSH MODE
            pMJCScaler->DefaultRoutine(pData);
            break;
        }

    }
    ALOGD("MtkMJCScalerThread terminated, %d, pMJC=0x%08X", __LINE__, (unsigned int)pMJC);
    return NULL;

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
MJCScaler::MJCScaler() {
    mState = MJCScaler_STATE_INIT;
    mMode = MJCScaler_MODE_NORMAL;
    mFormat = MJCScaler_FORMAT_NONE;

    memset((void *)&mAlignment, 0, sizeof(MJCScaler_VIDEORESOLUTION));

    INIT_SEMAPHORE(mMJCScalerSem);
    INIT_SEMAPHORE(mMJCScalerFlushDoneSem);

    INIT_MUTEX(mInputBufQLock);
    INIT_MUTEX(mOutputBufQLock);
    INIT_MUTEX(mScaledBufQLock);

    INIT_MUTEX(mModeLock);
    INIT_MUTEX(mScalerInputDrainedLock);

    pStream = new DpBlitStream();

    mScalerDrainInputBuffer = false;
    mScalerInputDrained = false;
    mIsHDRVideo = false;
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
MJCScaler::~MJCScaler() {
    DESTROY_SEMAPHORE(mMJCScalerSem);
    DESTROY_SEMAPHORE(mMJCScalerFlushDoneSem);

    DESTROY_MUTEX(mInputBufQLock);
    DESTROY_MUTEX(mOutputBufQLock);
    DESTROY_MUTEX(mScaledBufQLock);

    DESTROY_MUTEX(mModeLock);
    DESTROY_MUTEX(mScalerInputDrainedLock);

    delete pStream;
}

/*****************************************************************************
 * FUNCTION
 *    MJC::Init
 * DESCRIPTION
 *    1. Create MtkMJCThread
 * PARAMETERS
 *    param1 : [IN]  MJC_USERHANDLETYPE hUser
 *                Pointer of user (omx component) handle.
 * RETURNS
 *    Type: MJC_ERRORTYPE. MJC_ErrorNone mean success, MJC_ErrorInsufficientResources mean thread create fail and others mean fail.
 ****************************************************************************/
MJCScaler_ERRORTYPE MJCScaler::Create(MJC_IN MJC_USERHANDLETYPE hUser) {
    MJCScaler_ERRORTYPE err = MJCScaler_ErrorNone;

    mhFramework = hUser;
    mThreadParam.pMJCScaler = this;
    mThreadParam.pUser = mhFramework;
    MJC_LOGI("[0x%08X] Drv Create pMJCScaler=0x%08X, pMJCFramework=0x%08X", (unsigned int)this, (unsigned int)mThreadParam.pMJCScaler, (unsigned int)mThreadParam.pUser);


    char BuildType[PROPERTY_VALUE_MAX];
    property_get("ro.build.type", BuildType, "eng");
    if (!strcmp(BuildType,"user")){
        mMJCScalertagLog = false;
    } else {
        mMJCScalertagLog = true;
    }

    char mMJCValue[PROPERTY_VALUE_MAX];
    property_get("mtk.omxvdec.mjc.log", mMJCValue, "0");
    mMJCLog = (bool) atoi(mMJCValue);
    if(mMJCLog)
    {
        mMJCScalertagLog = true;
    }

    ALOGD("mMJCFrameworktagLog %d, mMJCLog %d", mMJCScalertagLog, mMJCLog);

    // create MJC thread
    int ret = pthread_create(&mMJCScalerThread, NULL, &MtkMJCScalerThread, (void *)&mThreadParam);
    if (ret) {
        MJC_LOGE("[ERR] MtkMJCScalerThread creation failure");
        err = MJCScaler_ErrorInsufficientResources;
        goto EXIT;
    }

    InitDumpYUV(&mThreadParam);

EXIT:
    return err;

}

/*****************************************************************************
 * FUNCTION
 *    MJC::Init
 * DESCRIPTION
 *    1. Call drv create
 * PARAMETERS
 *
 * RETURNS
 *    Type: MJC_ERRORTYPE. MJC_ErrorNone mean success, MJC_ErrorDrvCreateFail mean drv fail (enter bypass mode automatically) and others mean fail.
 ****************************************************************************/
MJCScaler_ERRORTYPE MJCScaler::Init() {
    MJCScaler_ERRORTYPE err = MJCScaler_ErrorNone;
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
MJCScaler_ERRORTYPE MJCScaler::Deinit() {
    MJCScaler_ERRORTYPE err = MJCScaler_ErrorNone;

    if (!pthread_equal(pthread_self(), mMJCScalerThread)) {
        // wait for mMJCThread terminate
        pthread_join(mMJCScalerThread, NULL);
    }

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
MJCScaler_ERRORTYPE MJCScaler::SetParameter(MJC_IN MJCScaler_PARAMTYPE nParamIndex, MJC_IN MJCScaler_PTR pCompParam) {
    MJCScaler_ERRORTYPE err = MJCScaler_ErrorNone;
    if (NULL == pCompParam) {
        err = MJCScaler_ErrorBadParameter;
        goto EXIT;
    }

    switch (nParamIndex) {
    case MJCScaler_PARAM_MODE: {
        MJCScaler_MODE mOriMode = mMode;
        MJCScaler_MODE *pParams = (MJCScaler_MODE *)pCompParam;

        LOCK(mModeLock);

        if (mMode & MJCScaler_MODE_FLUSH) {
            if (MJCScaler_MODE_BYPASS == *pParams) {
                mMode |= *pParams;
            } else if (MJCScaler_MODE_NORMAL == *pParams) {
                mMode = MJCScaler_MODE_FLUSH;
            }
        } else if (MJCScaler_MODE_FLUSH == *pParams) {
            mMode |= *pParams;
        } else {
            mMode = *pParams;
        }

        MJC_LOGI("SP Mode %d TO %d => %d", mOriMode, *pParams, mMode);

        UNLOCK(mModeLock);

        break;
    }
    case MJCScaler_PARAM_FORMAT: {
        MJCScaler_VIDEO_FORMAT *pParams = (MJCScaler_VIDEO_FORMAT *)pCompParam;
        mFormat = *pParams;
        CHECK(MJCScaler_FORMAT_NONE != mFormat);
        break;
    }
    case MJCScaler_PARAM_FRAME_RESOLUTION: {
        MJCScaler_VIDEORESOLUTION *pParams = (MJCScaler_VIDEORESOLUTION *)pCompParam;
        mFrame.u4Width = pParams->u4Width;
        mFrame.u4Height = pParams->u4Height;
        MJC_LOGI("SP FRM_RES %d, %d", mFrame.u4Width, mFrame.u4Height);

        break;
    }
    case MJCScaler_PARAM_SCALE_RESOLUTION: {
        MJCScaler_VIDEORESOLUTION *pParams = (MJCScaler_VIDEORESOLUTION *)pCompParam;
        mScale.u4Width = pParams->u4Width;
        mScale.u4Height = pParams->u4Height;
        MJC_LOGI("SP SCALE_RES %d, %d", mScale.u4Width, mScale.u4Height);

        break;
    }
    case MJCScaler_PARAM_ALIGH_SIZE: {
        MJCScaler_VIDEORESOLUTION *pParams = (MJCScaler_VIDEORESOLUTION *)pCompParam;
        mAlignment.u4Width = pParams->u4Width;
        mAlignment.u4Height = pParams->u4Height;
        MJC_LOGI("SP ALIGH_SIZE %d, %d", mAlignment.u4Width, mAlignment.u4Height);
        break;
    }
    case MJCScaler_PARAM_DRAIN_VIDEO_BUFFER: {
        // lock
        LOCK(mScalerInputDrainedLock);
        mScalerInputDrained = false;
        mScalerDrainInputBuffer = *(bool *)pCompParam;
        MJC_LOGI("[%d] mScalerDrainInputBuffer:%d, mScalerInputDrained:%d", mMode, mScalerDrainInputBuffer, mScalerInputDrained);
        UNLOCK(mScalerInputDrainedLock);

        if (mScalerDrainInputBuffer == true) {
            SIGNAL(mMJCScalerSem);
            MJC_LOGI("[%d] SIGNAL mMJCScalerSem", mMode);
        }

        break;
    }
    case MJCScaler_PARAM_IS_HDRVIDEO: {
        MJCScaler_HDRVideoInfo *pParams = (MJCScaler_HDRVideoInfo *)pCompParam;
        mIsHDRVideo = pParams->isHDRVideo;
        MJC_LOGI("[%d] mIsHDRVideo:%d", mMode, mIsHDRVideo);
        break;
    }
    case MJCScaler_PARAM_SET_COLOR_DESC: {
        mpColorDesc = (VDEC_DRV_COLORDESC_T *)pCompParam;

        if (mIsHDRVideo && mpColorDesc) {
            MJC_LOGI("[Scaler info] HDR video !! Get COLOR_DESC param: mColorDesc u4ColorPrimaries %d u4TransformCharacter %d u4MatrixCoeffs %d",
                mpColorDesc->u4ColorPrimaries, mpColorDesc->u4TransformCharacter, mpColorDesc->u4MatrixCoeffs);
            MJC_LOGI("[Scaler info] HDR video !! Get COLOR_DESC param: u4DisplayPrimariesX %d %d %d u4DisplayPrimariesY %d %d %d",
                mpColorDesc->u4DisplayPrimariesX[0], mpColorDesc->u4DisplayPrimariesX[1], mpColorDesc->u4DisplayPrimariesX[2], mpColorDesc->u4DisplayPrimariesY[0], mpColorDesc->u4DisplayPrimariesY[1], mpColorDesc->u4DisplayPrimariesY[2]);
            MJC_LOGI("[Scaler info] HDR video !! Get COLOR_DESC param: u4WhitePointX %d u4WhitePointY %d u4MaxDisplayMasteringLuminance %d u4MinDisplayMasteringLuminance %d",
                mpColorDesc->u4WhitePointX, mpColorDesc->u4WhitePointY, mpColorDesc->u4MaxDisplayMasteringLuminance, mpColorDesc->u4MinDisplayMasteringLuminance);
        }
        break;
    }
    case MJCScaler_PARAM_CROP_INFO: {
        memcpy((void *)&mCropInfo, (void *)pCompParam, sizeof(MJCScaler_VIDEOCROP));
        break;
    }

    default:
        MJC_LOGE("SP Err param: %d", nParamIndex);
        CHECK(0);
        break;
    }

EXIT:
    return err;
}

void MJCScaler::InitDumpYUV(void *pData) {
    MJCScaler_ThreadParam *pParam = (MJCScaler_ThreadParam *)pData;
    MJCScaler *pMJCScaler = (MJCScaler *)(pParam->pMJCScaler);

    char value[PROPERTY_VALUE_MAX];
    pMJCScaler->u4DumpYUV = 0;
    pMJCScaler->u4DumpCount = 0;

    property_get("mjc.scaler.dump", value, "0");
    pMJCScaler->u4DumpYUV = atoi(value);
    property_get("mjc.scaler.dump.start", value, "0");
    pMJCScaler->u4DumpStartTime = atoi(value);

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
MJCScaler_ERRORTYPE MJCScaler::GetParameter(MJC_IN MJCScaler_PARAMTYPE nParamIndex, MJC_INOUT MJCScaler_PTR pCompParam) {
    MJCScaler_ERRORTYPE err = MJCScaler_ErrorNone;
    if (NULL == pCompParam) {
        err = MJCScaler_ErrorBadParameter;
        goto EXIT;
    }

    switch (nParamIndex) {
    case MJCScaler_PARAM_MODE: {
        MJCScaler_MODE *pParams = (MJCScaler_MODE *)pCompParam;
        *pParams = mMode;
        break;
    }

    default:
        MJC_LOGE("GP Err param: %d", nParamIndex);
        CHECK(0);
        break;
    }

EXIT:
    return err;
}

/*****************************************************************************
 * FUNCTION
 *    MJCScaler::EmptyThisBuffer
 * DESCRIPTION
 *    1. According to the parameter type to fill the value.
 * PARAMETERS
 *    param1 : [IN]  MJC_PARAMTYPE nParamIndex
 *                Parameter ID.
 * RETURNS
 *    None.
 ****************************************************************************/
void MJCScaler::EmptyThisBuffer(MJC_INT_BUF_INFO *pMJCBufInfo) {
    LOCK(mInputBufQLock);
    mInputBufQ.add(pMJCBufInfo);
    UNLOCK(mInputBufQLock);
    if (mMJCLog) {
        MJC_LOGI("[%d][%d] ETB 0x%08X (0x%08X), cnt: %d", mState, mMode, (unsigned int)pMJCBufInfo->pBufHdr, (unsigned int)pMJCBufInfo->u4VA, mInputBufQ.size());
    }
    SIGNAL(mMJCScalerSem);

}

/*****************************************************************************
 * FUNCTION
 *    MJCScaler::FillThisBuffer
 * DESCRIPTION
 *    1. According to the parameter type to fill the value.
 * PARAMETERS
 *    param1 : [IN]  MJC_PARAMTYPE nParamIndex
 *                Parameter ID.
 * RETURNS
 *    None.
 ****************************************************************************/
void MJCScaler::FillThisBuffer(MJC_INT_BUF_INFO *pMJCBufInfo) {
    LOCK(mOutputBufQLock);
    mOutputBufQ.add(pMJCBufInfo);
    UNLOCK(mOutputBufQLock);
    if (mMJCLog) {
        MJC_LOGI("[%d][%d] FTB 0x%08X(0x%08X), cnt: %d", mState, mMode, (unsigned int)pMJCBufInfo->pBufHdr, (unsigned int)pMJCBufInfo->u4VA, mOutputBufQ.size());
    }
    SIGNAL(mMJCScalerSem);
}

/*****************************************************************************
 * FUNCTION
 *    MJCScaler::GetScaledBuffer
 * DESCRIPTION
 *    1. According to the parameter type to fill the value.
 * PARAMETERS
 *    param1 : [IN]  MJC_PARAMTYPE nParamIndex
 *                Parameter ID.
 * RETURNS
 *    None.
 ****************************************************************************/
MJC_INT_BUF_INFO *MJCScaler::GetScaledBuffer() {
    MJC_INT_BUF_INFO *pMJCBufInfo = NULL;

    LOCK(mScaledBufQLock);
    if (mScaledBufQ.size() > 0) {
        pMJCBufInfo = mScaledBufQ[0];
        mScaledBufQ.removeItemsAt(0);
        if (mMJCLog) {
            MJC_LOGI("[%d][%d] GSB 0x%08X, ts = %lld cnt: %d", mState, mMode, (unsigned int)pMJCBufInfo->pBufHdr, pMJCBufInfo->nTimeStamp , mScaledBufQ.size());
        }
    }
    UNLOCK(mScaledBufQLock);

    return pMJCBufInfo;
}

void MJCScaler::ConfigAndTriggerMDP(void *pData, MJC_INT_BUF_INFO *pScalerInBufInfo, MJC_INT_BUF_INFO *pScalerOutBufInfo) {
    MJCScaler_ThreadParam *pParam = (MJCScaler_ThreadParam *)pData;
    MJC *pMJC = (MJC *)(pParam->pUser);
    MJCScaler *pMJCScaler = (MJCScaler *)(pParam->pMJCScaler);

    void           *pBuffer[3];
    void           *pBufferMVA[3];
    uint32_t       size[3];
    DP_STATUS_ENUM ret;
    unsigned int pSource = pScalerInBufInfo->u4MVA;
    unsigned int pDest = pScalerOutBufInfo->u4MVA;


    DP_PROFILE_ENUM u4InputColorProfile;
    DP_PROFILE_ENUM u4OutputColorProfile;
    bool isFullColorRange; // false for limited color range

    //Update Color profile
    pScalerOutBufInfo->pBufHdr->bVideoRangeExist = pScalerInBufInfo->pBufHdr->bVideoRangeExist;
    pScalerOutBufInfo->pBufHdr->u4VideoRange= pScalerInBufInfo->pBufHdr->u4VideoRange;
    pScalerOutBufInfo->pBufHdr->bColourPrimariesExist = pScalerInBufInfo->pBufHdr->bColourPrimariesExist;

    if(mScale.u4Width * mScale.u4Height < 1280 * 720)
    {
        pScalerOutBufInfo->pBufHdr->eColourPrimaries = OMX_COLOR_PRIMARIES_BT601;
    }
    else if (mScale.u4Width * mScale.u4Height < 3840 * 2176)
    {
        pScalerOutBufInfo->pBufHdr->eColourPrimaries = OMX_COLOR_PRIMARIES_BT709;
    }
    else
    {
        pScalerOutBufInfo->pBufHdr->eColourPrimaries = OMX_COLOR_PRIMARIES_BT2020;
    }

    isFullColorRange = (pScalerInBufInfo->pBufHdr->bVideoRangeExist && 1 == pScalerInBufInfo->pBufHdr->u4VideoRange);

    // input color profile
    switch(pScalerInBufInfo->pBufHdr->eColourPrimaries)
    {
        case OMX_COLOR_PRIMARIES_BT601:
            u4InputColorProfile = isFullColorRange ? DP_PROFILE_FULL_BT601 : DP_PROFILE_BT601;
            break;
        case OMX_COLOR_PRIMARIES_BT709:
            u4InputColorProfile = isFullColorRange ? DP_PROFILE_FULL_BT709 : DP_PROFILE_BT709;
            break;
        case OMX_COLOR_PRIMARIES_BT2020:
            u4InputColorProfile = isFullColorRange ? DP_PROFILE_FULL_BT2020 : DP_PROFILE_BT2020;
            break;
        default:
            u4InputColorProfile = isFullColorRange ? DP_PROFILE_FULL_BT601 : DP_PROFILE_BT601;
            break;
    }

    //output color profile
    switch(pScalerOutBufInfo->pBufHdr->eColourPrimaries)
    {
        case OMX_COLOR_PRIMARIES_BT601:
            u4OutputColorProfile = isFullColorRange ? DP_PROFILE_FULL_BT601 : DP_PROFILE_BT601;
            break;
        case OMX_COLOR_PRIMARIES_BT709:
            u4OutputColorProfile = isFullColorRange ? DP_PROFILE_FULL_BT709 : DP_PROFILE_BT709;
            break;
        case OMX_COLOR_PRIMARIES_BT2020:
            u4OutputColorProfile = isFullColorRange ? DP_PROFILE_FULL_BT2020 : DP_PROFILE_BT2020;
            break;
        default:
            u4OutputColorProfile = isFullColorRange ? DP_PROFILE_FULL_BT601 : DP_PROFILE_BT601;
            break;
    }

    if (MJCScaler_FORMAT_BLK == pMJCScaler->mFormat || pMJCScaler->mFormat == MJCScaler_FORMAT_BLK_10BIT_H || pMJCScaler->mFormat == MJCScaler_FORMAT_BLK_10BIT_V) {
        // Block Format

        unsigned int align_w = (pMJCScaler->mAlignment.u4Width == 0) ? ALIGN_BLK_W : pMJCScaler->mAlignment.u4Width;
        unsigned int align_h = (pMJCScaler->mAlignment.u4Height == 0) ? ALIGN_BLK_H : pMJCScaler->mAlignment.u4Height;

        // Setup buffer address
        if (pMJCScaler->mFormat == MJCScaler_FORMAT_BLK_10BIT_H || pMJCScaler->mFormat == MJCScaler_FORMAT_BLK_10BIT_V) {
            pBuffer[0] = (void *)pScalerInBufInfo->u4VA;
            pBuffer[1] = (void *)(pScalerInBufInfo->u4VA + ALIGN((ALIGN(pMJCScaler->mFrame.u4Width, align_w) * ALIGN(pMJCScaler->mFrame.u4Height, align_h)) * 5 / 4, 512));
            pBuffer[2] = NULL;

            pBufferMVA[0] = (void *)pSource;
            pBufferMVA[1] = (void *)(pSource + ALIGN((ALIGN(pMJCScaler->mFrame.u4Width, align_w) * ALIGN(pMJCScaler->mFrame.u4Height, align_h)) * 5 / 4, 512));
            pBufferMVA[2] = NULL;

            // Setup buffer size
            size[0] = ALIGN(pMJCScaler->mFrame.u4Width, align_w) * ALIGN(pMJCScaler->mFrame.u4Height, align_h) * 5 / 4 ;
            size[1] = size[0] >> 1;
            size[2] = 0;
        } else {
            pBuffer[0] = (void *)pScalerInBufInfo->u4VA;
            pBuffer[1] = (void *)(pScalerInBufInfo->u4VA + (ALIGN(pMJCScaler->mFrame.u4Width, align_w) * ALIGN(pMJCScaler->mFrame.u4Height, align_h)));
            pBuffer[2] = NULL;

            pBufferMVA[0] = (void *)pSource;
            pBufferMVA[1] = (void *)(pSource + (ALIGN(pMJCScaler->mFrame.u4Width, align_w) * ALIGN(pMJCScaler->mFrame.u4Height, align_h)));
            pBufferMVA[2] = NULL;

            // Setup buffer size
            size[0] = ALIGN(pMJCScaler->mFrame.u4Width, align_w) * ALIGN(pMJCScaler->mFrame.u4Height, align_h) ;
            size[1] = (ALIGN(pMJCScaler->mFrame.u4Width, align_w) * ALIGN(pMJCScaler->mFrame.u4Height, align_h)) >> 1;
            size[2] = 0;
        }
        ret = pStream->setSrcBuffer(pBuffer,     // pVABaseList
                                    pBufferMVA,  // pMVABaseList
                                    size,        // pSizeList
                                    2);          // planeNumber
        ret = pStream->setRotate(0);
        DpRect src_roi;
        src_roi.x = 0;
        src_roi.y = 0;
        src_roi.w = mCropInfo.mCropWidth; //pMJCScaler->mFrame.u4Width;
        src_roi.h = mCropInfo.mCropHeight; //pMJCScaler->mFrame.u4Height;

        DP_COLOR_ENUM u4MDPColorFormat;
        unsigned char u1YBlockHeightInByte;
        unsigned char u1UVBlockHeightInByte;
        if (pMJCScaler->mFormat == MJCScaler_FORMAT_BLK_10BIT_H) {
            u4MDPColorFormat = DP_COLOR_420_BLKP_10_H;
            u1YBlockHeightInByte = ALIGN_BLK_H * 5 / 4;
            u1UVBlockHeightInByte = u1YBlockHeightInByte / 2;
        } else if (pMJCScaler->mFormat == MJCScaler_FORMAT_BLK_10BIT_V) {
            u4MDPColorFormat = DP_COLOR_420_BLKP_10_V;
            u1YBlockHeightInByte = ALIGN_BLK_H * 5 / 4;
            u1UVBlockHeightInByte = u1YBlockHeightInByte / 2;
        } else {
            u4MDPColorFormat = DP_COLOR_420_BLKP;
            u1YBlockHeightInByte = ALIGN_BLK_H;
            u1UVBlockHeightInByte = u1YBlockHeightInByte / 2;
        }

        ret = pStream->setSrcConfig(ALIGN(pMJCScaler->mFrame.u4Width, align_w),         // width
                                    ALIGN(pMJCScaler->mFrame.u4Height, align_h),        // height
                                    ALIGN(pMJCScaler->mFrame.u4Width, align_w) * u1YBlockHeightInByte,    // YPitch
                                    ALIGN(pMJCScaler->mFrame.u4Width, align_w) * u1UVBlockHeightInByte,    // UVPitch
                                    u4MDPColorFormat,  // format
                                    u4InputColorProfile,   // profile
                                    eInterlace_None,    // field
                                    &src_roi,           // pROI
                                    DP_SECURE_NONE,     // secure
                                    false);             // doFlush
    } else if (MJCScaler_FORMAT_LINE == pMJCScaler->mFormat) {
        // Line --> YV12

        unsigned int align_w = (pMJCScaler->mAlignment.u4Width == 0) ? ALIGN_MTKYV12_W : pMJCScaler->mAlignment.u4Width;
        unsigned int align_h = (pMJCScaler->mAlignment.u4Height == 0) ? ALIGN_MTKYV12_H : pMJCScaler->mAlignment.u4Height;
        unsigned int stride = ALIGN(pMJCScaler->mFrame.u4Width, align_w);
        unsigned int y_size = stride * ALIGN(pMJCScaler->mFrame.u4Height, align_h);
        unsigned int c_stride = ALIGN(stride / 2, 16);
        unsigned int c_size = c_stride * ALIGN(pMJCScaler->mFrame.u4Height, align_h) / 2;
        unsigned int cr_offset = y_size;
        unsigned int cb_offset = y_size + c_size;

        // Setup buffer address
        pBuffer[0] = (void *)pScalerInBufInfo->u4VA;
        pBuffer[1] = (void *)(pScalerInBufInfo->u4VA + cr_offset);
        pBuffer[2] = (void *)(pScalerInBufInfo->u4VA + cb_offset);

        pBufferMVA[0] = (void *)pSource;
        pBufferMVA[1] = (void *)(pSource + cr_offset);
        pBufferMVA[2] = (void *)(pSource + cb_offset);

        // Setup buffer size
        size[0] = y_size;
        size[1] = c_size;
        size[2] = c_size;
        MJC_LOGV("size [%d][%d][%d]", size[0], size[1], size[2]);

        ret = pStream->setSrcBuffer(pBuffer,    // pVABaseList
                                    pBufferMVA, // pMVABaseList
                                    size,       // pSizeList
                                    3);         // planeNumber
        ret = pStream->setRotate(0);

        DpRect src_roi;
        src_roi.x = 0;
        src_roi.y = 0;
        src_roi.w = mCropInfo.mCropWidth; //pMJCScaler->mFrame.u4Width;
        src_roi.h = mCropInfo.mCropHeight; //pMJCScaler->mFrame.u4Height;

        ret = pStream->setSrcConfig(ALIGN(pMJCScaler->mFrame.u4Width, align_w),  // width
                                    ALIGN(pMJCScaler->mFrame.u4Height, align_h), // height
                                    stride,             // YPitch
                                    c_stride,           // UVPitch
                                    DP_COLOR_YV12,      // format
                                    u4InputColorProfile,// profile
                                    eInterlace_None,    // field
                                    &src_roi,           // pROI
                                    DP_SECURE_NONE,     // secure
                                    false);             // doFlush
    } else {
        CHECK(0);
    }

    if (mIsHDRVideo == OMX_TRUE)
    {
        DpPqParam dppq_param;
        dppq_param.scenario = MEDIA_VIDEO_CODEC;
        dppq_param.u.video.id = (unsigned int)this;
        dppq_param.u.video.timeStamp = pScalerInBufInfo->nTimeStamp;
        dppq_param.u.video.grallocExtraHandle = (buffer_handle_t)pScalerInBufInfo->u4GraphicBufHandle;

        if (sizeof(dppq_param.u.video.HDRinfo) != sizeof(VDEC_DRV_COLORDESC_T)) {
            MJC_LOGE("[ERROR] HDR color desc info size not sync!!dppq_param.u.video.HDRinfo(%d) != mColorDesc(%d)", sizeof(dppq_param.u.video.HDRinfo), sizeof(VDEC_DRV_COLORDESC_T));
        } else {
            memcpy(&dppq_param.u.video.HDRinfo, mpColorDesc, sizeof(VDEC_DRV_COLORDESC_T));
        }

        /*MJC_LOGI("[MJC][HDRConvert] pScalerInBufInfo->u4GraphicBufHandle 0x%x,LINE:%d", pScalerInBufInfo->u4GraphicBufHandle, __LINE__);
        MJC_LOGI("[MDP info] HDR video !! Get COLOR_DESC param: mColorDesc u4ColorPrimaries %d u4TransformCharacter %d u4MatrixCoeffs %d",
            dppq_param.u.video.HDRinfo.u4ColorPrimaries, dppq_param.u.video.HDRinfo.u4TransformCharacter, dppq_param.u.video.HDRinfo.u4MatrixCoeffs);
        MJC_LOGI("[MDP info] HDR video !! Get COLOR_DESC param: u4DisplayPrimariesX %d %d %d u4DisplayPrimariesY %d %d %d",
            dppq_param.u.video.HDRinfo.u4DisplayPrimariesX[0], dppq_param.u.video.HDRinfo.u4DisplayPrimariesX[1], dppq_param.u.video.HDRinfo.u4DisplayPrimariesX[2], dppq_param.u.video.HDRinfo.u4DisplayPrimariesY[0], dppq_param.u.video.HDRinfo.u4DisplayPrimariesY[1], dppq_param.u.video.HDRinfo.u4DisplayPrimariesY[2]);
        MJC_LOGI("[MDP info] HDR video !! Get COLOR_DESC param: u4WhitePointX %d u4WhitePointY %d u4MaxDisplayMasteringLuminance %d u4MinDisplayMasteringLuminance %d",
            dppq_param.u.video.HDRinfo.u4WhitePointX, dppq_param.u.video.HDRinfo.u4WhitePointY, dppq_param.u.video.HDRinfo.u4MaxDisplayMasteringLuminance, dppq_param.u.video.HDRinfo.u4MinDisplayMasteringLuminance);
            */

        pStream->setPQParameter(dppq_param);
    }

    // Output configuration --> Support YV12 ONLY

    unsigned int stride = ALIGN(pMJCScaler->mScale.u4Width, ALIGN_YV12_W);
    unsigned int y_size = stride * ALIGN(pMJCScaler->mScale.u4Height, ALIGN_YV12_H);
    unsigned int c_stride = ALIGN(stride / 2, 16);
    unsigned int c_size = c_stride * ALIGN(pMJCScaler->mScale.u4Height, ALIGN_YV12_H) / 2;
    unsigned int cr_offset = y_size;
    unsigned int cb_offset = y_size + c_size;

    // Setup buffer address
    pBuffer[0] = (void *)pScalerOutBufInfo->u4VA;
    pBuffer[1] = (void *)(pScalerOutBufInfo->u4VA + cr_offset);
    pBuffer[2] = (void *)(pScalerOutBufInfo->u4VA + cb_offset);

    pBufferMVA[0] = (void *)pDest;
    pBufferMVA[1] = (void *)(pDest + cr_offset);
    pBufferMVA[2] = (void *)(pDest + cb_offset);

    // Setup buffer size
    size[0] = y_size;
    size[1] = c_size;
    size[2] = c_size;
    ret = pStream->setDstBuffer(pBuffer,    // pVABaseList
                                pBufferMVA, // pMVABaseList
                                size,       // pSizeList
                                3);         // planeNumber

    ret = pStream->setDstConfig(pMJCScaler->mScale.u4Width,     // width
                                pMJCScaler->mScale.u4Height,    // height
                                stride,                         // YPitch
                                c_stride,                       // UVPitch
                                DP_COLOR_YV12,                  // format
                                u4OutputColorProfile);          // profile

    MJC_LOGI("[%d][%d] Trigger MDP, source: (%d,%d), crop: (%d, %d), dest: (%d, %d), pInBufHdr: 0x%08X, pSource: 0x%08X, pOutBufHdr: 0x%08X, pDest: 0x%08X",
             pMJCScaler->mState,
             pMJCScaler->mMode,
             pMJCScaler->mFrame.u4Width,
             pMJCScaler->mFrame.u4Height,
             mCropInfo.mCropWidth,
             mCropInfo.mCropHeight,
             pMJCScaler->mScale.u4Width,
             pMJCScaler->mScale.u4Height,
             (unsigned int)pScalerInBufInfo->pBufHdr,
             pSource,
             (unsigned int)pScalerOutBufInfo->pBufHdr,
             pDest);

    int64_t in_time_1 = getTickCountMs();

    ATRACE_NAME("MDP Scale");
    ret = pStream->invalidate();

    if (mMJCLog) {
        MJC_LOGI("[%d][%d] MDP done %d, (%lld) ms", pMJCScaler->mState, pMJCScaler->mMode, ret, getTickCountMs() - in_time_1);
    }
    CHECK(DP_STATUS_RETURN_SUCCESS == ret);

}


void MJCScaler::DefaultRoutine(void *pData) {
    MJCScaler_ThreadParam *pParam = (MJCScaler_ThreadParam *)pData;
    MJC *pMJC = (MJC *)(pParam->pUser);
    MJCScaler *pMJCScaler = (MJCScaler *)(pParam->pMJCScaler);

    MJC_INT_BUF_INFO *pScalerInBufInfo  = NULL;
    MJC_INT_BUF_INFO *pScalerOutBufInfo = NULL;

    //
    // This routine should only be used for flush
    //

    pMJCScaler->mState = MJCScaler_STATE_FLUSH;
    MJC_LOGI("[%d][%d] IQ: %d, OQ: %d, SQ: %d", pMJCScaler->mState, pMJCScaler->mMode, pMJCScaler->mInputBufQ.size(), pMJCScaler->mOutputBufQ.size(), pMJCScaler->mScaledBufQ.size());

    // Move Scaled  buffer to DispQ
    LOCK(pMJCScaler->mScaledBufQLock);

    while (pMJCScaler->mScaledBufQ.size() > 0) {
        MJC_INT_BUF_INFO *pMJCBufInfo = pMJCScaler->mScaledBufQ[0];
        pMJCScaler->mScaledBufQ.removeItemsAt(0);

        if (pMJCBufInfo != NULL) {
            pMJCBufInfo->nFilledLen = pMJCBufInfo->pBufHdr->nFilledLen = 0;
            pMJCBufInfo->nTimeStamp = pMJCBufInfo->pBufHdr->nTimeStamp = 0;
            MJC_LOGD("[%d][%d] Flush SB, 0x%08X", pMJCScaler->mState, pMJCScaler->mMode, (unsigned int)pMJCBufInfo->pBufHdr);
            pMJC->PDFAndSetMJCBufferFlag(pMJC->hComponent, pMJCBufInfo);
        } else {
            break;
        }
    }
    UNLOCK(pMJCScaler->mScaledBufQLock);

    // Move intput  buffer to DispQ
    LOCK(pMJCScaler->mInputBufQLock);

    while (pMJCScaler->mInputBufQ.size() > 0) {
        pScalerInBufInfo = pMJCScaler->mInputBufQ[0];
        pMJCScaler->mInputBufQ.removeItemsAt(0);

        if (pScalerInBufInfo != NULL) {
            pScalerInBufInfo->nFilledLen = pScalerInBufInfo->pBufHdr->nFilledLen = 0;
            pScalerInBufInfo->nTimeStamp = pScalerInBufInfo->pBufHdr->nTimeStamp = 0;
            MJC_LOGD("[%d][%d] Flush IB, 0x%08X", pMJCScaler->mState, pMJCScaler->mMode, (unsigned int)pScalerInBufInfo);
            pMJC->PDFAndSetMJCBufferFlag(pMJC->hComponent, pScalerInBufInfo);
        } else {
            break;
        }
    }
    UNLOCK(pMJCScaler->mInputBufQLock);


    // Move output buffer to DispQ
    LOCK(pMJCScaler->mOutputBufQLock);

    while (pMJCScaler->mOutputBufQ.size() > 0) {
        pScalerOutBufInfo = pMJCScaler->mOutputBufQ[0];
        pMJCScaler->mOutputBufQ.removeItemsAt(0);

        if (pScalerOutBufInfo != NULL) {
            pScalerOutBufInfo->pBufHdr->nFilledLen = 0;
            pScalerOutBufInfo->pBufHdr->nTimeStamp = 0;
            MJC_LOGD("[%d][%d] Flush OB, 0x%08X", pMJCScaler->mState, pMJCScaler->mMode, (unsigned int)pScalerOutBufInfo);
            pMJC->PDFAndSetMJCBufferFlag(pMJC->hComponent, pScalerOutBufInfo); // We must call PutDispFrame() if the frame buffer comes from Decoder
        } else {
            break;
        }
    }
    UNLOCK(pMJCScaler->mOutputBufQLock);
    //MJC_LOGI("[%d][%d] send mMJCScalerFlushDoneSem", pMJCScaler->mState, pMJCScaler->mMode);

    LOCK(pMJCScaler->mModeLock);
    pMJCScaler->mMode = (~MJCScaler_MODE_FLUSH) & pMJCScaler->mMode;
    UNLOCK(pMJCScaler->mModeLock);
    pMJCScaler->mState = MJCScaler_STATE_READY;

    MJC_LOGI("[%d][%d] signal ScalerFlushSem, I:%d, O:%d", pMJCScaler->mState, pMJCScaler->mMode,  pMJCScaler->mInputBufQ.size(), pMJCScaler->mOutputBufQ.size());
    CHECK(0 == pMJCScaler->mOutputBufQ.size());
    CHECK(0 == pMJCScaler->mInputBufQ.size());

    SIGNAL(pMJCScaler->mMJCScalerFlushDoneSem);

}



void MJCScaler::NormalModeRoutine(void *pData) {
    MJCScaler_ThreadParam *pParam = (MJCScaler_ThreadParam *)pData;
    MJC *pMJC = (MJC *)(pParam->pUser);
    MJCScaler *pMJCScaler = (MJCScaler *)(pParam->pMJCScaler);
    MtkOmxVdec *pVdec = (MtkOmxVdec *)(pMJC->hComponent);

    MJC_INT_BUF_INFO *pScalerInBufInfo  = NULL;
    MJC_INT_BUF_INFO *pScalerOutBufInfo = NULL;

    if (mMJCLog) {
        MJC_LOGI("[%d][%d] IQ: %d, OQ: %d, SQ: %d",
                 pMJCScaler->mState,
                 pMJCScaler->mMode,
                 pMJCScaler->mInputBufQ.size(),
                 pMJCScaler->mOutputBufQ.size(),
                 pMJCScaler->mScaledBufQ.size());
    }


    // Prepare Input buffer
    LOCK(pMJCScaler->mInputBufQLock);
    if (pMJCScaler->mInputBufQ.size() > 0) {
        pScalerInBufInfo = pMJCScaler->mInputBufQ[0];
    } else {
        UNLOCK(pMJCScaler->mInputBufQLock);
        //MJC_LOGI("[%d] NIB", pMJC->mState);
        goto CHECK_INPUT_DRAINED;
    }
    UNLOCK(pMJCScaler->mInputBufQLock);

    // Handle EOS input
    // TODO: EOS with decoded data will not be used
    if (pScalerInBufInfo->nFlags & OMX_BUFFERFLAG_EOS && pScalerInBufInfo->nFilledLen == 0) {
        MJC_LOGD("[%d][%d] EOS, 0x%08X, ts = %lld", pMJCScaler->mState, pMJCScaler->mMode, (unsigned int)pScalerInBufInfo->pBufHdr, pScalerInBufInfo->nTimeStamp);

        LOCK(pMJCScaler->mInputBufQLock);
        pMJCScaler->mInputBufQ.removeItemsAt(0);
        UNLOCK(pMJCScaler->mInputBufQLock);

        LOCK(pMJCScaler->mScaledBufQLock);
        pMJCScaler->mScaledBufQ.add(pScalerInBufInfo);
        UNLOCK(pMJCScaler->mScaledBufQLock);

        CHECK(pMJCScaler->mInputBufQ.size() == 0);

        SIGNAL(pMJC->mMJCFrameworkSem);

        goto CHECK_INPUT_DRAINED;
    }

    // Prepare Output buffer
    LOCK(pMJCScaler->mOutputBufQLock);
    if (pMJCScaler->mOutputBufQ.size() > 0) {
        pScalerOutBufInfo = pMJCScaler->mOutputBufQ[0];
    } else {
        UNLOCK(pMJCScaler->mOutputBufQLock);
        //MJC_LOGI("[%d] NOB", pMJC->mState);
        goto CHECK_INPUT_DRAINED;
    }
    UNLOCK(pMJCScaler->mOutputBufQLock);

    // Trigger MDP do Scale
    CHECK(pScalerInBufInfo != NULL);
    CHECK(pScalerOutBufInfo != NULL);

    ConfigAndTriggerMDP(pData, pScalerInBufInfo, pScalerOutBufInfo);

    // Send  Scaled frame to MJC
    // TODO: need modify
    LOCK(pMJCScaler->mOutputBufQLock);
    pMJCScaler->mOutputBufQ.removeItemsAt(0);
    UNLOCK(pMJCScaler->mOutputBufQLock);

    LOCK(pMJCScaler->mInputBufQLock);
    pMJCScaler->mInputBufQ.removeItemsAt(0);
    UNLOCK(pMJCScaler->mInputBufQLock);

    if (pScalerInBufInfo->nFlags & OMX_BUFFERFLAG_EOS) {
        pScalerOutBufInfo->nFlags |= OMX_BUFFERFLAG_EOS;
    }

    CHECK(pScalerOutBufInfo->pBufHdr != NULL);

    {
        // TODO: 10Bit size
        pScalerOutBufInfo->nTimeStamp = pScalerOutBufInfo->pBufHdr->nTimeStamp = pScalerInBufInfo->nTimeStamp;
        pScalerOutBufInfo->nFilledLen = pScalerOutBufInfo->pBufHdr->nFilledLen = pScalerInBufInfo->nFilledLen;

        if (pMJCScaler->u4DumpYUV > 0 && (pScalerInBufInfo->nTimeStamp / 1000000 >= pMJCScaler->u4DumpStartTime)) {
            char buf[255];
            sprintf(buf, "/sdcard/scaler_out_%d_%d.yuv", pMJCScaler->mScale.u4Width, pMJCScaler->mScale.u4Height);
            FILE *fp = fopen(buf, "ab");
            if (fp) {
                if (pMJCScaler->u4DumpCount < pMJCScaler->u4DumpYUV) {
                    fwrite((void *)pScalerOutBufInfo->u4VA, 1, pMJCScaler->mScale.u4Width * pMJCScaler->mScale.u4Height * 3 / 2, fp);
                    pMJCScaler->u4DumpCount++;
                }

                MJC_LOGI("u4DumpCount(%d), u4DumpYUV(%d)", pMJCScaler->u4DumpCount, pMJCScaler->u4DumpYUV);
                fclose(fp);
            }

            MJC_LOGE("Fail to open %s", buf);
        }

        LOCK(pMJCScaler->mScaledBufQLock);
        pMJCScaler->mScaledBufQ.add(pScalerOutBufInfo);
        UNLOCK(pMJCScaler->mScaledBufQLock);
    }
    SIGNAL(pMJC->mMJCFrameworkSem);

    //Return processed  frame
    pScalerInBufInfo->nFilledLen = pScalerInBufInfo->pBufHdr->nFilledLen = 0;
    pScalerInBufInfo->nTimeStamp = pScalerInBufInfo->pBufHdr->nTimeStamp = 0;

    pMJC->PDFAndSetMJCBufferFlag(pMJC->hComponent, pScalerInBufInfo);

CHECK_INPUT_DRAINED:

    // Make sure SetParameter goes first.
    LOCK(mScalerInputDrainedLock);
    if (pMJCScaler->mScalerDrainInputBuffer) {
        LOCK(pVdec->mMJCVdoBufQLock);
        bool isVdoBufQEmpty = pVdec->mMJCVdoBufCount == 0 ? true : false ;
        UNLOCK(pVdec->mMJCVdoBufQLock);

        // Check if VdoQ and scaler inputQ are empty
        MJC_LOGD("[%d][%d] Vdo: %d, InputQ: %d, ScaledQ: %d", pMJCScaler->mState , pMJCScaler->mMode , pVdec->mMJCVdoBufCount, pMJCScaler->mInputBufQ.size(), pMJCScaler->mScaledBufQ.size());
        if (isVdoBufQEmpty && pMJCScaler->mInputBufQ.size() == 0) {
            MJC_LOGI("[%d][%d] Scaler Drained mScalerInputDrained: %d", pMJCScaler->mState, pMJCScaler->mMode, pMJCScaler->mScalerInputDrained);
            pMJCScaler->mScalerInputDrained = true;
            pMJCScaler->mScalerDrainInputBuffer = false;
            SIGNAL(pMJC->mMJCFrameworkSem);
        }
    }
    UNLOCK(mScalerInputDrainedLock);
}



void MJCScaler::BypassModeRoutine(void *pData) {
    MJCScaler_ThreadParam *pParam = (MJCScaler_ThreadParam *)pData;
    MJC *pMJC = (MJC *)(pParam->pUser);
    MJCScaler *pMJCScaler = (MJCScaler *)(pParam->pMJCScaler);
    MtkOmxVdec *pVdec = (MtkOmxVdec *)(pMJC->hComponent);

    MJC_INT_BUF_INFO *pScalerInBufInfo  = NULL;
    MJC_INT_BUF_INFO *pScalerOutBufInfo = NULL;


    // BYPASS MODE
    if (mMJCLog) {
        MJC_LOGI("[%d][%d] IQ: %d, OQ: %d, SQ: %d",
                 pMJCScaler->mState,
                 pMJCScaler->mMode,
                 pMJCScaler->mInputBufQ.size(),
                 pMJCScaler->mOutputBufQ.size(),
                 pMJCScaler->mScaledBufQ.size());
    }

    // Move output buffer to DispQ
    LOCK(pMJCScaler->mOutputBufQLock);

    while (pMJCScaler->mOutputBufQ.size() > 0) {
        pScalerOutBufInfo = pMJCScaler->mOutputBufQ[0];
        pMJCScaler->mOutputBufQ.removeItemsAt(0);

        if (pScalerOutBufInfo != NULL) {
            pScalerOutBufInfo->nFilledLen = pScalerOutBufInfo->pBufHdr->nFilledLen = 0;
            pScalerOutBufInfo->nTimeStamp = pScalerOutBufInfo->pBufHdr->nTimeStamp = 0;
            MJC_LOGD("[%d][%d] Return OB (%d), 0x%08X (0x%08X)", pMJCScaler->mState, pMJCScaler->mMode, pMJCScaler->mOutputBufQ.size(), (unsigned int)pScalerOutBufInfo->pBufHdr, (unsigned int)pScalerOutBufInfo->u4VA);
            pMJC->PDFAndSetMJCBufferFlag(pMJC->hComponent, pScalerOutBufInfo);
        } else {
            break;
        }
    }
    UNLOCK(pMJCScaler->mOutputBufQLock);


    // Check Input buffer
    LOCK(pMJCScaler->mInputBufQLock);
    if (pMJCScaler->mInputBufQ.size() > 0) {
        pScalerInBufInfo = pMJCScaler->mInputBufQ[0];
        pMJCScaler->mInputBufQ.removeItemsAt(0);
    } else {
        UNLOCK(pMJCScaler->mInputBufQLock);
        //MJC_LOGI("[%d] NIB", pMJC->mState);
        goto CHECK_INPUT_DRAINED;
    }
    UNLOCK(pMJCScaler->mInputBufQLock);

    // Send frame to MJC
    CHECK(pScalerInBufInfo != NULL);

    {
        LOCK(pMJCScaler->mScaledBufQLock);
        pMJCScaler->mScaledBufQ.add(pScalerInBufInfo);
        UNLOCK(pMJCScaler->mScaledBufQLock);
    }
    SIGNAL(pMJC->mMJCFrameworkSem);

CHECK_INPUT_DRAINED:

    // Make sure SetParameter goes first.
    LOCK(mScalerInputDrainedLock);
    if (pMJCScaler->mScalerDrainInputBuffer) {
        LOCK(pVdec->mMJCVdoBufQLock);
        bool isVdoBufQEmpty = pVdec->mMJCVdoBufCount == 0 ? true : false ;
        UNLOCK(pVdec->mMJCVdoBufQLock);

        // Check if VdoQ and scaler inputQ are empty
        MJC_LOGD("[%d][%d] Vdo: %d, InputQ: %d, ScaledQ: %d", pMJCScaler->mState , pMJCScaler->mMode , pVdec->mMJCVdoBufCount, pMJCScaler->mInputBufQ.size(), pMJCScaler->mScaledBufQ.size());
        if (isVdoBufQEmpty && pMJCScaler->mInputBufQ.size() == 0) {
            MJC_LOGI("[%d][%d] Scaler Drained mScalerInputDrained: %d", pMJCScaler->mState, pMJCScaler->mMode, pMJCScaler->mScalerInputDrained);
            pMJCScaler->mScalerInputDrained = true;
            pMJCScaler->mScalerDrainInputBuffer = false;
            SIGNAL(pMJC->mMJCFrameworkSem);
        }
    }
    UNLOCK(mScalerInputDrainedLock);

}



