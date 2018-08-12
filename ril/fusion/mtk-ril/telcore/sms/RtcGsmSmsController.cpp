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
#include "RtcGsmSmsController.h"
#include <telephony/mtk_ril.h>
#include "RfxMessageId.h"
#include "RfxStringsData.h"
#include "RfxSmsRspData.h"


#include "rfx_properties.h"

using ::android::String8;

RFX_IMPLEMENT_CLASS("RtcGsmSmsController", RtcGsmSmsController, RfxController);

// Register dispatch and response class
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxStringsData, RfxSmsRspData, \
        RFX_MSG_REQUEST_IMS_SEND_GSM_SMS);



/*****************************************************************************
 * Class RtcGsmSmsController
 *****************************************************************************/
RtcGsmSmsController::RtcGsmSmsController() {
    setTag(String8("RtcGsmSmsCtrl"));
}

RtcGsmSmsController::~RtcGsmSmsController() {
}

void RtcGsmSmsController::onInit() {
    // Required: invoke super class implementation
    RfxController::onInit();

    const int request_id_list[] = {
        RFX_MSG_REQUEST_IMS_SEND_GSM_SMS,
        RFX_MSG_REQUEST_SEND_SMS,
        RFX_MSG_REQUEST_SEND_SMS_EXPECT_MORE,
        RFX_MSG_REQUEST_WRITE_SMS_TO_SIM,
        RFX_MSG_REQUEST_DELETE_SMS_ON_SIM,
        RFX_MSG_REQUEST_REPORT_SMS_MEMORY_STATUS,
        RFX_MSG_REQUEST_GET_SMS_SIM_MEM_STATUS,
        RFX_MSG_REQUEST_GSM_GET_BROADCAST_SMS_CONFIG,
        RFX_MSG_REQUEST_GSM_SET_BROADCAST_SMS_CONFIG,
        RFX_MSG_REQUEST_GSM_GET_BROADCAST_LANGUAGE,
        RFX_MSG_REQUEST_GSM_SET_BROADCAST_LANGUAGE,
        RFX_MSG_REQUEST_GSM_SMS_BROADCAST_ACTIVATION,
        RFX_MSG_REQUEST_GET_GSM_SMS_BROADCAST_ACTIVATION,
        RFX_MSG_REQUEST_SMS_ACKNOWLEDGE,
    };

    // register request & URC id list
    // NOTE. one id can only be registered by one controller
    registerToHandleRequest(request_id_list, sizeof(request_id_list)/sizeof(const int));
}

bool RtcGsmSmsController::onCheckIfRejectMessage(
        const sp<RfxMessage>& message, bool isModemPowerOff, int radioState) {
    int msgId = message->getId();
    char prop_value[RFX_PROPERTY_VALUE_MAX] = {0};
    rfx_property_get("persist.mtk_wfc_support", prop_value, "0");
    int isWfcSupport = atoi(prop_value);

    if (!isModemPowerOff && (radioState == (int)RADIO_STATE_OFF) &&
            (msgId == RFX_MSG_REQUEST_SEND_SMS ||
            msgId == RFX_MSG_REQUEST_SEND_SMS_EXPECT_MORE) &&
            (isWfcSupport != 0)) {
        logD(mTag, "onCheckIfRejectMessage, isModemPowerOff %d, isWfcSupport %d",
                (isModemPowerOff == false) ? 0 : 1, isWfcSupport);
        return false;
    } else if (!isModemPowerOff && (radioState == (int)RADIO_STATE_OFF) &&
            (msgId == RFX_MSG_REQUEST_WRITE_SMS_TO_SIM ||
            msgId == RFX_MSG_REQUEST_DELETE_SMS_ON_SIM ||
            msgId == RFX_MSG_REQUEST_REPORT_SMS_MEMORY_STATUS ||
            msgId == RFX_MSG_REQUEST_GET_SMS_SIM_MEM_STATUS ||
            msgId == RFX_MSG_REQUEST_GSM_GET_BROADCAST_SMS_CONFIG ||
            msgId == RFX_MSG_REQUEST_GSM_SET_BROADCAST_SMS_CONFIG ||
            msgId == RFX_MSG_REQUEST_GSM_GET_BROADCAST_LANGUAGE ||
            msgId == RFX_MSG_REQUEST_GSM_SET_BROADCAST_LANGUAGE ||
            msgId == RFX_MSG_REQUEST_GSM_SMS_BROADCAST_ACTIVATION ||
            msgId == RFX_MSG_REQUEST_GET_GSM_SMS_BROADCAST_ACTIVATION ||
            msgId == RFX_MSG_REQUEST_SMS_ACKNOWLEDGE)) {
        logD(mTag, "onCheckIfRejectMessage, isModemPowerOff %d, radioState %d",
                (isModemPowerOff == false) ? 0 : 1, radioState);
        return false;
    }

    return RfxController::onCheckIfRejectMessage(message, isModemPowerOff, radioState);
}

void RtcGsmSmsController::handleRequest(const sp<RfxMessage>& msg) {
    int msg_id = msg->getId();
    switch (msg_id) {
        case RFX_MSG_REQUEST_SEND_SMS:
        case RFX_MSG_REQUEST_SEND_SMS_EXPECT_MORE:
        case RFX_MSG_REQUEST_WRITE_SMS_TO_SIM:
        case RFX_MSG_REQUEST_DELETE_SMS_ON_SIM:
        case RFX_MSG_REQUEST_REPORT_SMS_MEMORY_STATUS:
        case RFX_MSG_REQUEST_GET_SMS_SIM_MEM_STATUS:
        case RFX_MSG_REQUEST_GSM_GET_BROADCAST_SMS_CONFIG:
        case RFX_MSG_REQUEST_GSM_SET_BROADCAST_SMS_CONFIG:
        case RFX_MSG_REQUEST_GSM_GET_BROADCAST_LANGUAGE:
        case RFX_MSG_REQUEST_GSM_SET_BROADCAST_LANGUAGE:
        case RFX_MSG_REQUEST_GSM_SMS_BROADCAST_ACTIVATION:
        case RFX_MSG_REQUEST_GET_GSM_SMS_BROADCAST_ACTIVATION:
        case RFX_MSG_REQUEST_SMS_ACKNOWLEDGE: {
                // Send RMC directly
                requestToMcl(msg);
            }
            break;
        case RFX_MSG_REQUEST_IMS_SEND_SMS: {
                RIL_IMS_SMS_Message *pIms = (RIL_IMS_SMS_Message*)msg->getData()->getData();
                char** pStrs = pIms->message.gsmMessage;
                int countStr = GSM_SMS_MESSAGE_STRS_COUNT;
                sp<RfxMessage> req;

                logD(mTag, "smsc %s, pdu %s", ((pStrs[0] != NULL)? pStrs[0] : "null"),
                        ((pStrs[1] != NULL)? pStrs[1] : "null"));
                req = RfxMessage::obtainRequest(RFX_MSG_REQUEST_IMS_SEND_GSM_SMS,
                        RfxStringsData(pStrs, countStr), msg, false);

                requestToMcl(req);
            }
            break;
    }
}

bool RtcGsmSmsController::onHandleResponse(const sp<RfxMessage>& msg) {
    int msg_id = msg->getId();
    switch (msg_id) {
        case RFX_MSG_REQUEST_SEND_SMS:
        case RFX_MSG_REQUEST_SEND_SMS_EXPECT_MORE:
        case RFX_MSG_REQUEST_WRITE_SMS_TO_SIM:
        case RFX_MSG_REQUEST_DELETE_SMS_ON_SIM:
        case RFX_MSG_REQUEST_REPORT_SMS_MEMORY_STATUS:
        case RFX_MSG_REQUEST_GET_SMS_SIM_MEM_STATUS:
        case RFX_MSG_REQUEST_GSM_GET_BROADCAST_SMS_CONFIG:
        case RFX_MSG_REQUEST_GSM_SET_BROADCAST_SMS_CONFIG:
        case RFX_MSG_REQUEST_GSM_GET_BROADCAST_LANGUAGE:
        case RFX_MSG_REQUEST_GSM_SET_BROADCAST_LANGUAGE:
        case RFX_MSG_REQUEST_GSM_SMS_BROADCAST_ACTIVATION:
        case RFX_MSG_REQUEST_GET_GSM_SMS_BROADCAST_ACTIVATION:
        case RFX_MSG_REQUEST_SMS_ACKNOWLEDGE: {
                // Send RILJ directly
                responseToRilj(msg);
            }
            break;
        case RFX_MSG_REQUEST_IMS_SEND_GSM_SMS:
            {
                sp<RfxMessage> rsp;

                rsp = RfxMessage::obtainResponse(RFX_MSG_REQUEST_IMS_SEND_SMS, msg);

                responseToRilj(rsp);
            }
            break;
        default:
            logD(mTag, "Not Support the req %d", msg_id);
            break;
    }

    return true;
}
