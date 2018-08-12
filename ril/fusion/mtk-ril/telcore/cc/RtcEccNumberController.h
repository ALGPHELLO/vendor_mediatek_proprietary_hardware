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

#ifndef __RTC_ECC_NUMBER_CONTROLLER_H__
#define __RTC_ECC_NUMBER_CONTROLLER_H__

/*****************************************************************************
 * Include
 *****************************************************************************/
#include "RfxController.h"
#include "RfxTimer.h"
#include "RfxVoidData.h"
#include "RfxStringData.h"
#include "RfxStringsData.h"
#include "RfxIntsData.h"
#include "RfxMessageId.h"
#include "RfxAtLine.h"
#include "rfx_properties.h"

/*****************************************************************************
 * Class RfxController
 *****************************************************************************/
class RtcEccNumberController : public RfxController {
    // Required: declare this class
    RFX_DECLARE_CLASS(RtcEccNumberController);

public:
    RtcEccNumberController();
    virtual ~RtcEccNumberController();

// Override
protected:
    virtual bool onHandleUrc(const sp<RfxMessage>& message);
    virtual void onInit();

public:

private:
    void onCardTypeChanged(RfxStatusKeyEnum key,
            RfxVariant oldValue, RfxVariant newValue);
    void onPlmnChanged(RfxStatusKeyEnum key,
            RfxVariant old_value, RfxVariant value);
    void onSimRecovery(RfxStatusKeyEnum key,
            RfxVariant old_value, RfxVariant value);
    void handleGsmSimEcc(const sp<RfxMessage>& message);
    void handleC2kSimEcc(const sp<RfxMessage>& message);
    void parseSimEcc(RfxAtLine *line, bool isGsm);
    bool isCdmaCard(int cardType);
    inline bool isBspPackage() {
        // return false for BSP+
        return false;
    }

    inline bool isOp12Package() {
        char optr[RFX_PROPERTY_VALUE_MAX] = {0};
        rfx_property_get("ro.mtk_md_sbp_custom_value", optr, "0");
        return (strcmp(optr, "12") == 0);
    }

private:
    String8 mDefaultEccNumber;
    String8 mDefaultEccNumberNoSim;
    String8 mGsmEcc;
    String8 mC2kEcc;
    RfxAtLine *mCachedGsmUrc;
    RfxAtLine *mCachedC2kUrc;
};

#endif /* __RTC_ECC_NUMBER_CONTROLLER_H__ */