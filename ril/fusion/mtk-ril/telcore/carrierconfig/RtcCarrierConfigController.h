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

#ifndef __RFX_CARRIER_CONFIG_CONTROLLER_H__
#define __RFX_CARRIER_CONFIG_CONTROLLER_H__

#define PROP_NAME_OPERATOR_ID_SIM1 "persist.radio.sim.opid"
#define PROP_NAME_OPERATOR_ID_SIM2 "persist.radio.sim.opid_1"
#define PROP_NAME_OPERATOR_ID_SIM3 "persist.radio.sim.opid_2"
#define PROP_NAME_OPERATOR_ID_SIM4 "persist.radio.sim.opid_3"
#define MAX_SIM_COUNT 4
#define NOT_MATCHED_OP_ID    -999


/*****************************************************************************
 * Include
 *****************************************************************************/
#include "RfxController.h"

/*****************************************************************************
 * Class RfxController
 *****************************************************************************/

/* {mccMnc-start, mccMnc-end, OP-ID} */
typedef struct OperatorTableStruct {
    int mccMnc_range_start;
    int mccMnc_range_end;
    int opId;
} OperatorTable;

class RtcCarrierConfigController : public RfxController {
    // Required: declare this class
    RFX_DECLARE_CLASS(RtcCarrierConfigController);

public:
    RtcCarrierConfigController();
    virtual ~RtcCarrierConfigController();
    bool responseToRilj(const sp<RfxMessage>& message);

// Override
protected:
    virtual void onInit();
    virtual void onDeinit();
    virtual bool onHandleRequest(const sp<RfxMessage>& message);
    virtual bool onHandleUrc(const sp<RfxMessage>& message);
    virtual bool onHandleResponse(const sp<RfxMessage>& message);

private:
    void onUiccMccMncChanged(RfxStatusKeyEnum key,
                RfxVariant oldValue, RfxVariant value);
    void onCardTypeChanged(RfxStatusKeyEnum key,
                RfxVariant oldValue, RfxVariant value);
    void updateOpIdProperty(int mccmnc);

    int mCurrentOperatorId[MAX_SIM_COUNT];
};

#endif /* __RFX_CARRIER_CONFIG_CONTROLLER_H__ */
