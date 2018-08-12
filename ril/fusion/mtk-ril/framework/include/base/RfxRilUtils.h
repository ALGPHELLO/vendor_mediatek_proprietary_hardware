/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 *
 * MediaTek Inc. (C) 2010. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
 * AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
 * NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
 * SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
 * SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
 * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
 * THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
 * SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
 * AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
 * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 * MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek Software")
 * have been modified by MediaTek Inc. All revisions are subject to any receiver's
 * applicable license agreements with MediaTek Inc.
 */

#ifndef __RFX_RIL_UTILS__
#define __RFX_RIL_UTILS__

#include <rfx_properties.h>
#include <stdlib.h>
#include <string.h>
#include <RfxLog.h>
#include <RfxDefs.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "RfxStatusDefs.h"
#include "RfxVariant.h"

#if !defined(PURE_AP_USE_EXTERNAL_MODEM)
#include "hardware/ccci_intf.h"
#endif

#define NUM_ELEMS(x) (sizeof(x)/sizeof(x[0]))
#define MIN(a,b) ((a)<(b) ? (a) : (b))
typedef void (*STATUSCALLBACK)(int slotId, const RfxStatusKeyEnum key,
            const RfxVariant &value);

#define OPERATOR_KDDI 129

class RfxRilUtils {

public:
    static int getSimCount();
    static int isEngLoad();
    static bool isChipTestMode();
    static int isUserLoad();
    static int isC2kSupport();
    static int isLteSupport();
    static int isImsSupport();
    static int isMultipleImsSupport();
    static int triggerCCCIIoctlEx(int request, int *param);
    static int triggerCCCIIoctl(int request);
    static RilRunMode getRilRunMode();
    static void setRilRunMode(RilRunMode mode);
    static void setStatusValueForGT(int slotId, const RfxStatusKeyEnum key, const RfxVariant &value);
    static void updateStatusToGT(int slotId, const RfxStatusKeyEnum key, const RfxVariant &value);
    static void setStatusCallbackForGT(STATUSCALLBACK cb);
    /// M: add for op09 volte setting @{
    static bool isOp09();
    static bool isCtVolteSupport();
    /// @}
    static int getMajorSim();
    static void printLog(int level, String8 tag, String8 log, int slot);
    static bool isInLogReductionList(int reqId);
    static int handleAee(const char *modem_warning, const char *modem_version);

    // External SIM [Start]
    static int isExternalSimSupport();
    static int isExternalSimOnlySlot(int slot);
    static int isNonDsdaRemoteSupport();
    static int isSwitchVsimWithHotSwap();
    static int isPersistExternalSimDisabled();
    static int isVsimEnabledBySlot(int slot);
    static bool isVsimEnabled();
    // External SIM [End]

    static bool isTplusWSupport();
    static int getKeep3GMode();
    static bool isWfcEnable(int slotId);
    static bool isDigitsSupport();
    static int getOperatorId(int simId);

private:
    static void readMultiSIMConfig();
    static int m_multiSIMConfig;
    static int m_isEngLoad;
    static int m_isChipTest;
    static int m_isInternalLoad;
    static int m_isUserLoad;
    static int mIsC2kSupport;
    static int mIsLteSupport;
    static int mIsImsSupport;
    static int mIsMultiIms;
    /// M: add for op09 volte setting @{
    static int mIsOp09;
    static int mIsCtVolteSupport;
    /// @}
    static RilRunMode m_rilRunMode;
    static STATUSCALLBACK s_statusCallback;
};

#endif
