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
#include "RtcCommSimController.h"
#include <telephony/mtk_ril.h>
#include "RfxVoidData.h"

using ::android::String8;

RFX_IMPLEMENT_CLASS("RtcCommSimController", RtcCommSimController, RfxController);



/*****************************************************************************
 * Class RtcCommSimController
 *****************************************************************************/
RtcCommSimController::RtcCommSimController() {
        setTag(String8("RtcCommSimCtrl"));
}


RtcCommSimController::~RtcCommSimController() {
}

void RtcCommSimController::onInit() {
    // Required: invoke super class implementation
    RfxController::onInit();

    // register request & URC id list for radio state check
    const int request_id_list[] = {
        RFX_MSG_REQUEST_GET_SIM_STATUS,
        RFX_MSG_REQUEST_ENTER_SIM_PIN,
        RFX_MSG_REQUEST_ENTER_SIM_PUK,
        RFX_MSG_REQUEST_ENTER_SIM_PIN2,
        RFX_MSG_REQUEST_ENTER_SIM_PUK2,
        RFX_MSG_REQUEST_CHANGE_SIM_PIN,
        RFX_MSG_REQUEST_CHANGE_SIM_PIN2,
        RFX_MSG_REQUEST_SIM_ENTER_NETWORK_DEPERSONALIZATION,
        RFX_MSG_REQUEST_SIM_IO,
        RFX_MSG_REQUEST_SIM_AUTHENTICATION,
        RFX_MSG_REQUEST_ISIM_AUTHENTICATION,
        RFX_MSG_REQUEST_GENERAL_SIM_AUTH,
        RFX_MSG_REQUEST_SIM_OPEN_CHANNEL,
        RFX_MSG_REQUEST_SIM_CLOSE_CHANNEL,
        RFX_MSG_REQUEST_SIM_TRANSMIT_APDU_BASIC,
        RFX_MSG_REQUEST_SIM_TRANSMIT_APDU_CHANNEL,
        RFX_MSG_REQUEST_SIM_GET_ATR,
        RFX_MSG_REQUEST_SIM_SAP_CONNECT,
        RFX_MSG_REQUEST_SIM_SAP_DISCONNECT,
        RFX_MSG_REQUEST_SIM_SAP_APDU,
        RFX_MSG_REQUEST_SIM_SAP_TRANSFER_ATR,
        RFX_MSG_REQUEST_SIM_SAP_POWER,
        RFX_MSG_REQUEST_SIM_SAP_RESET_SIM,
        RFX_MSG_REQUEST_SIM_SAP_SET_TRANSFER_PROTOCOL,
        RFX_MSG_REQUEST_SIM_SAP_ERROR_RESP,
        RFX_MSG_REQUEST_GET_IMSI,
        RFX_MSG_REQUEST_QUERY_FACILITY_LOCK,
        RFX_MSG_REQUEST_SET_FACILITY_LOCK,
        RFX_MSG_REQUEST_SIM_SET_SIM_NETWORK_LOCK,
        RFX_MSG_REQUEST_SIM_QUERY_SIM_NETWORK_LOCK,
        RFX_MSG_REQUEST_SIM_VSIM_NOTIFICATION,
        RFX_MSG_REQUEST_SIM_VSIM_OPERATION,
        RFX_MSG_REQUEST_CDMA_SUBSCRIPTION,
        RFX_MSG_REQUEST_CDMA_GET_SUBSCRIPTION_SOURCE,
        RFX_MSG_REQUEST_SET_SIM_CARD_POWER
    };

    // NOTE. one id can only be registered by one controller
    registerToHandleRequest(request_id_list, sizeof(request_id_list)/sizeof(const int));

    // register callbacks to get required information
    getStatusManager()->registerStatusChanged(RFX_STATUS_KEY_RADIO_STATE,
            RfxStatusChangeCallback(this, &RtcCommSimController::onRadioStateChanged));

    // register callbacks to mode switch status
    getNonSlotScopeStatusManager()->registerStatusChanged(RFX_STATUS_KEY_MODESWITCH_FINISHED,
            RfxStatusChangeCallback(this, &RtcCommSimController::onModeSwitchFinished));
}

void RtcCommSimController::onRadioStateChanged(RfxStatusKeyEnum key,
        RfxVariant old_value, RfxVariant value) {
    int oldState = -1, newState = -1;

    RFX_UNUSED(key);
    oldState = old_value.asInt();
    newState = value.asInt();

    logD(mTag, "onRadioStateChanged (%d, %d) (slot %d)", oldState, newState, getSlotId());

    if (newState == RADIO_STATE_UNAVAILABLE) {
        // Modem SIM task is not ready because radio is not available
        getStatusManager()->setBoolValue(RFX_STATUS_KEY_MODEM_SIM_TASK_READY, false, true);
        getStatusManager()->setIntValue(RFX_STATUS_KEY_CDMA3G_SWITCH_CARD, -1);
    }
}

void RtcCommSimController::onModeSwitchFinished(RfxStatusKeyEnum key,
        RfxVariant old_value, RfxVariant value) {

    RFX_UNUSED(key);

    logD(mTag, "onModeSwitchFinished (%d, %d) (slot %d)", old_value.asInt(), value.asInt(),
            getSlotId());

    sp<RfxMessage> message = RfxMessage::obtainUrc(getSlotId(),
            RFX_MSG_URC_RESPONSE_SIM_STATUS_CHANGED, RfxVoidData());
    responseToRilj(message);
}

bool RtcCommSimController::onCheckIfRejectMessage(
        const sp<RfxMessage>& message, bool isModemPowerOff, int radioState) {
    int msgId = message->getId();

    if (!isModemPowerOff && (radioState == (int)RADIO_STATE_OFF) &&
            (msgId == RFX_MSG_REQUEST_GET_SIM_STATUS ||
             msgId == RFX_MSG_REQUEST_ENTER_SIM_PIN ||
             msgId == RFX_MSG_REQUEST_ENTER_SIM_PUK ||
             msgId == RFX_MSG_REQUEST_ENTER_SIM_PIN2 ||
             msgId == RFX_MSG_REQUEST_ENTER_SIM_PUK2 ||
             msgId == RFX_MSG_REQUEST_CHANGE_SIM_PIN ||
             msgId == RFX_MSG_REQUEST_CHANGE_SIM_PIN2 ||
             msgId == RFX_MSG_REQUEST_SIM_ENTER_NETWORK_DEPERSONALIZATION ||
             msgId == RFX_MSG_REQUEST_SIM_IO ||
             msgId == RFX_MSG_REQUEST_ISIM_AUTHENTICATION ||
             msgId == RFX_MSG_REQUEST_GENERAL_SIM_AUTH ||
             msgId == RFX_MSG_REQUEST_SIM_AUTHENTICATION ||
             msgId == RFX_MSG_REQUEST_SIM_OPEN_CHANNEL ||
             msgId == RFX_MSG_REQUEST_SIM_CLOSE_CHANNEL ||
             msgId == RFX_MSG_REQUEST_SIM_TRANSMIT_APDU_BASIC ||
             msgId == RFX_MSG_REQUEST_SIM_TRANSMIT_APDU_CHANNEL ||
             msgId == RFX_MSG_REQUEST_SIM_GET_ATR ||
             msgId == RFX_MSG_REQUEST_SIM_SAP_CONNECT ||
             msgId == RFX_MSG_REQUEST_SIM_SAP_DISCONNECT ||
             msgId == RFX_MSG_REQUEST_SIM_SAP_APDU ||
             msgId == RFX_MSG_REQUEST_SIM_SAP_TRANSFER_ATR ||
             msgId == RFX_MSG_REQUEST_SIM_SAP_POWER ||
             msgId == RFX_MSG_REQUEST_SIM_SAP_RESET_SIM ||
             msgId == RFX_MSG_REQUEST_SIM_SAP_SET_TRANSFER_PROTOCOL ||
             msgId == RFX_MSG_REQUEST_SIM_SAP_ERROR_RESP ||
             msgId == RFX_MSG_REQUEST_GET_IMSI ||
             msgId == RFX_MSG_REQUEST_QUERY_FACILITY_LOCK ||
             msgId == RFX_MSG_REQUEST_SET_FACILITY_LOCK ||
             msgId == RFX_MSG_REQUEST_SIM_SET_SIM_NETWORK_LOCK ||
             msgId == RFX_MSG_REQUEST_SIM_QUERY_SIM_NETWORK_LOCK ||
             msgId == RFX_MSG_REQUEST_CDMA_SUBSCRIPTION ||
             msgId == RFX_MSG_REQUEST_CDMA_GET_SUBSCRIPTION_SOURCE ||
             msgId == RFX_MSG_REQUEST_SET_SIM_CARD_POWER)) {
        return false;
    } else if (msgId == RFX_MSG_REQUEST_SIM_VSIM_NOTIFICATION ||
            msgId == RFX_MSG_REQUEST_SIM_VSIM_OPERATION) {
        return false;
    }

    return RfxController::onCheckIfRejectMessage(message, isModemPowerOff, radioState);
}

bool RtcCommSimController::onHandleRequest(const sp<RfxMessage>& msg) {
    int msg_id = msg->getId();
    switch (msg_id) {
        case RFX_MSG_REQUEST_GET_SIM_STATUS :
        case RFX_MSG_REQUEST_ENTER_SIM_PIN :
        case RFX_MSG_REQUEST_ENTER_SIM_PUK :
        case RFX_MSG_REQUEST_ENTER_SIM_PIN2 :
        case RFX_MSG_REQUEST_ENTER_SIM_PUK2 :
        case RFX_MSG_REQUEST_CHANGE_SIM_PIN :
        case RFX_MSG_REQUEST_CHANGE_SIM_PIN2 :
        case RFX_MSG_REQUEST_SIM_ENTER_NETWORK_DEPERSONALIZATION :
        case RFX_MSG_REQUEST_SIM_IO :
        case RFX_MSG_REQUEST_ISIM_AUTHENTICATION :
        case RFX_MSG_REQUEST_GENERAL_SIM_AUTH :
        case RFX_MSG_REQUEST_SIM_AUTHENTICATION :
        case RFX_MSG_REQUEST_SIM_OPEN_CHANNEL :
        case RFX_MSG_REQUEST_SIM_CLOSE_CHANNEL :
        case RFX_MSG_REQUEST_SIM_TRANSMIT_APDU_BASIC :
        case RFX_MSG_REQUEST_SIM_TRANSMIT_APDU_CHANNEL :
        case RFX_MSG_REQUEST_SIM_GET_ATR :
        case RFX_MSG_REQUEST_SIM_SAP_CONNECT :
        case RFX_MSG_REQUEST_SIM_SAP_DISCONNECT :
        case RFX_MSG_REQUEST_SIM_SAP_APDU :
        case RFX_MSG_REQUEST_SIM_SAP_TRANSFER_ATR :
        case RFX_MSG_REQUEST_SIM_SAP_POWER :
        case RFX_MSG_REQUEST_SIM_SAP_RESET_SIM :
        case RFX_MSG_REQUEST_SIM_SAP_SET_TRANSFER_PROTOCOL :
        case RFX_MSG_REQUEST_SIM_SAP_ERROR_RESP :
        case RFX_MSG_REQUEST_GET_IMSI :
        case RFX_MSG_REQUEST_QUERY_FACILITY_LOCK :
        case RFX_MSG_REQUEST_SET_FACILITY_LOCK :
        case RFX_MSG_REQUEST_SIM_SET_SIM_NETWORK_LOCK :
        case RFX_MSG_REQUEST_SIM_QUERY_SIM_NETWORK_LOCK:
        case RFX_MSG_REQUEST_SIM_VSIM_NOTIFICATION:
        case RFX_MSG_REQUEST_SIM_VSIM_OPERATION:
        case RFX_MSG_REQUEST_CDMA_SUBSCRIPTION:
        case RFX_MSG_REQUEST_CDMA_GET_SUBSCRIPTION_SOURCE: 
        case RFX_MSG_REQUEST_SET_SIM_CARD_POWER: {
                // Send RMC directly
                requestToMcl(msg);
            }
            break;
        default:
            logD(mTag, "Not Support the req %s", idToString(msg_id));
            break;
    }

    return true;
}

bool RtcCommSimController::onHandleResponse(const sp<RfxMessage>& msg) {
    int msg_id = msg->getId();
    switch (msg_id) {
        case RFX_MSG_REQUEST_GET_SIM_STATUS :
        case RFX_MSG_REQUEST_ENTER_SIM_PIN :
        case RFX_MSG_REQUEST_ENTER_SIM_PUK :
        case RFX_MSG_REQUEST_ENTER_SIM_PIN2 :
        case RFX_MSG_REQUEST_ENTER_SIM_PUK2 :
        case RFX_MSG_REQUEST_CHANGE_SIM_PIN :
        case RFX_MSG_REQUEST_CHANGE_SIM_PIN2 :
        case RFX_MSG_REQUEST_SIM_ENTER_NETWORK_DEPERSONALIZATION :
        case RFX_MSG_REQUEST_SIM_IO :
        case RFX_MSG_REQUEST_ISIM_AUTHENTICATION :
        case RFX_MSG_REQUEST_GENERAL_SIM_AUTH :
        case RFX_MSG_REQUEST_SIM_AUTHENTICATION :
        case RFX_MSG_REQUEST_SIM_OPEN_CHANNEL :
        case RFX_MSG_REQUEST_SIM_CLOSE_CHANNEL :
        case RFX_MSG_REQUEST_SIM_TRANSMIT_APDU_BASIC :
        case RFX_MSG_REQUEST_SIM_TRANSMIT_APDU_CHANNEL :
        case RFX_MSG_REQUEST_SIM_GET_ATR :
        case RFX_MSG_REQUEST_SIM_SAP_CONNECT :
        case RFX_MSG_REQUEST_SIM_SAP_DISCONNECT :
        case RFX_MSG_REQUEST_SIM_SAP_APDU :
        case RFX_MSG_REQUEST_SIM_SAP_TRANSFER_ATR :
        case RFX_MSG_REQUEST_SIM_SAP_POWER :
        case RFX_MSG_REQUEST_SIM_SAP_RESET_SIM :
        case RFX_MSG_REQUEST_SIM_SAP_SET_TRANSFER_PROTOCOL :
        case RFX_MSG_REQUEST_SIM_SAP_ERROR_RESP :
        case RFX_MSG_REQUEST_GET_IMSI :
        case RFX_MSG_REQUEST_QUERY_FACILITY_LOCK :
        case RFX_MSG_REQUEST_SET_FACILITY_LOCK :
        case RFX_MSG_REQUEST_SIM_SET_SIM_NETWORK_LOCK :
        case RFX_MSG_REQUEST_SIM_QUERY_SIM_NETWORK_LOCK:
        case RFX_MSG_REQUEST_SIM_VSIM_NOTIFICATION:
        case RFX_MSG_REQUEST_SIM_VSIM_OPERATION:
        case RFX_MSG_REQUEST_CDMA_SUBSCRIPTION:
        case RFX_MSG_REQUEST_CDMA_GET_SUBSCRIPTION_SOURCE: 
        case RFX_MSG_REQUEST_SET_SIM_CARD_POWER: {
                // Send RILJ directly
                responseToRilj(msg);
            }
            break;
        default:
            logD(mTag, "Not Support the req %s", idToString(msg_id));
            break;
    }

    return true;
}

