/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 *
 * MediaTek Inc. (C) 2016. All rights reserved.
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

/*****************************************************************************
 * Include
 *****************************************************************************/
#include "RtcImsSmsController.h"
#include <telephony/mtk_ril.h>
#include "RfxMessageId.h"
#include "RfxImsSmsData.h"
#include "RfxSmsRspData.h"
#include "rfx_properties.h"

using ::android::String8;

RFX_IMPLEMENT_CLASS("RtcImsSmsController", RtcImsSmsController, RfxController);


// Register solicited message
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxImsSmsData, RfxSmsRspData, \
        RFX_MSG_REQUEST_IMS_SEND_SMS);


/*****************************************************************************
 * Class RtcImsSmsController
 *****************************************************************************/
RtcImsSmsController::RtcImsSmsController() {
        setTag(String8("RtcImsSmsCtrl"));
}


RtcImsSmsController::~RtcImsSmsController() {
}

void RtcImsSmsController::onInit() {
    // Required: invoke super class implementation
    RfxController::onInit();

    RFX_OBJ_CREATE(mGsmCtrl, RtcGsmSmsController, this);
    RFX_OBJ_CREATE(mCdmaCtrl, RtcCdmaSmsController, this);

    const int request_id_list[] = {
        RFX_MSG_REQUEST_IMS_SEND_SMS
    };

    // register request & URC id list
    // NOTE. one id can only be registered by one controller
    registerToHandleRequest(request_id_list, sizeof(request_id_list)/sizeof(const int));
}

bool RtcImsSmsController::onCheckIfRejectMessage(
        const sp<RfxMessage>& message, bool isModemPowerOff, int radioState) {
    int msgId = message->getId();
    char prop_value[RFX_PROPERTY_VALUE_MAX] = {0};
    rfx_property_get("persist.mtk_wfc_support", prop_value, "0");
    int isWfcSupport = atoi(prop_value);

    if (!isModemPowerOff && (radioState == (int)RADIO_STATE_OFF) &&
            msgId == RFX_MSG_REQUEST_IMS_SEND_SMS && (isWfcSupport != 0)) {
        logD(mTag, "onCheckIfRejectMessage, isModemPowerOff %d, isWfcSupport %d",
                (isModemPowerOff == false) ? 0 : 1, isWfcSupport);
        return false;
    }

    return RfxController::onCheckIfRejectMessage(message, isModemPowerOff, radioState);
}

bool RtcImsSmsController::onHandleRequest(const sp<RfxMessage>& msg) {
    int msg_id = msg->getId();
    switch (msg_id) {
        case RFX_MSG_REQUEST_IMS_SEND_SMS:
            {
                RIL_IMS_SMS_Message *pIms = (RIL_IMS_SMS_Message*)msg->getData()->getData();
                if (pIms->tech == RADIO_TECH_3GPP) {
                    mGsmCtrl->handleRequest(msg);
                } else {
                    mCdmaCtrl->handleRequest(msg);
                }
            }
            break;
        default:
            logD(mTag, "Not Support the req %d", msg_id);
            break;
    }
    return true;
}

bool RtcImsSmsController::onPreviewMessage(const sp<RfxMessage>& msg) {
    int msg_id = msg->getId();
    switch (msg_id) {
        case RFX_MSG_REQUEST_IMS_SEND_SMS:
            {
                RIL_IMS_SMS_Message *pIms = (RIL_IMS_SMS_Message*)msg->getData()->getData();
                if (pIms->tech == RADIO_TECH_3GPP2) {
                    return mCdmaCtrl->previewMessage(msg);
                }
            }
            break;
        default:
            logD(mTag, "Not Support the req %d", msg_id);
            break;
    }
    return true;

}

bool RtcImsSmsController::onCheckIfResumeMessage(const sp<RfxMessage>& msg) {
    int msg_id = msg->getId();
    switch (msg_id) {
        case RFX_MSG_REQUEST_IMS_SEND_SMS:
            {
                RIL_IMS_SMS_Message *pIms = (RIL_IMS_SMS_Message*)msg->getData()->getData();
                if (pIms->tech == RADIO_TECH_3GPP2) {
                    return mCdmaCtrl->checkIfResumeMessage(msg);
                }
            }
            break;
        default:
            logD(mTag, "Not Support the req %d", msg_id);
            break;
    }
    return false;
}
