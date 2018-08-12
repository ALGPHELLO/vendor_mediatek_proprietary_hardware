/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 *
 * MediaTek Inc. (C) 2017. All rights reserved.
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
#include "RtcCdmaSmsController.h"
#include "RfxCdmaSmsMessageId.h"
#include "sms/RmcCdmaMoSms.h"

/*****************************************************************************
 * Class RtcCdmaSmsController
 *****************************************************************************/
RFX_IMPLEMENT_CLASS("RtcCdmaSmsController", RtcCdmaSmsController, RfxController);
RtcCdmaSmsController::RtcCdmaSmsController() {
}


RtcCdmaSmsController::~RtcCdmaSmsController() {
}


void RtcCdmaSmsController::onInit() {
    // Required: invoke super class implementation
    RfxController::onInit();

    const int request[] = {
        RFX_MSG_REQUEST_IMS_SEND_CDMA_SMS,
        RFX_MSG_REQUEST_CDMA_SEND_SMS,
        RFX_MSG_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION,
        RFX_MSG_REQUEST_CDMA_DELETE_SMS_ON_RUIM,
        RFX_MSG_REQUEST_CDMA_WRITE_SMS_TO_RUIM,
        RFX_MSG_REQUEST_GET_SMS_RUIM_MEM_STATUS,
        RFX_MSG_REQUEST_CDMA_GET_BROADCAST_SMS_CONFIG,
        RFX_MSG_REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG,
        RFX_MSG_REQUEST_GET_SMSC_ADDRESS,
        RFX_MSG_REQUEST_SET_SMSC_ADDRESS
    };

    registerToHandleRequest(request, sizeof(request)/sizeof(const int));
}


bool RtcCdmaSmsController::onHandleRequest(const sp<RfxMessage>& message) {
    handleRequest(message);
    return true;
}

void RtcCdmaSmsController::handleMoSmsRequests(const sp<RfxMessage>& message) {
    if (message->getId() == RFX_MSG_REQUEST_IMS_SEND_SMS) {
        RIL_IMS_SMS_Message *pIms = (RIL_IMS_SMS_Message*)message->getData()->getData();
        sp<RfxMessage> req;
        req = RfxMessage::obtainRequest(RFX_MSG_REQUEST_IMS_SEND_CDMA_SMS,
                RmcCdmaMoSmsMessage(pIms->message.cdmaMessage), message, false);
        requestToMcl(req);
    } else {
        RFX_ASSERT(message->getId() == RFX_MSG_REQUEST_CDMA_SEND_SMS);
        requestToMcl(message);
    }
    getStatusManager()->setIntValue(
        RFX_STATUS_KEY_CDMA_MO_SMS_STATE, CDMA_MO_SMS_SENDING);
}


void RtcCdmaSmsController::handleSmscAdressResponses(const sp<RfxMessage>& message) {
    int type = getStatusManager()->getIntValue(RFX_STATUS_KEY_CDMA_CARD_TYPE);
    if (message->getError() != RIL_E_SUCCESS) {
        if (type == CT_3G_UIM_CARD || type == UIM_CARD ||
                type == CT_UIM_SIM_CARD || type == UIM_SIM_CARD) {
            sp<RfxMessage> newMsg =
                RfxMessage::obtainResponse(RIL_E_REQUEST_NOT_SUPPORTED, message);
            RfxController::onHandleResponse(newMsg);
            return;
        }
    }
    RfxController::onHandleResponse(message);
}


void RtcCdmaSmsController::handleRequest(const sp<RfxMessage>& message) {
    int msgId = message->getId();
    switch (msgId) {
        case RFX_MSG_REQUEST_IMS_SEND_SMS:
        case RFX_MSG_REQUEST_CDMA_SEND_SMS:
            handleMoSmsRequests(message);
            break;

        case RFX_MSG_REQUEST_GET_SMSC_ADDRESS:
        case RFX_MSG_REQUEST_SET_SMSC_ADDRESS:
        case RFX_MSG_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION:
        case RFX_MSG_REQUEST_CDMA_DELETE_SMS_ON_RUIM:
        case RFX_MSG_REQUEST_CDMA_WRITE_SMS_TO_RUIM:
        case RFX_MSG_REQUEST_GET_SMS_RUIM_MEM_STATUS:
        case RFX_MSG_REQUEST_CDMA_GET_BROADCAST_SMS_CONFIG:
        case RFX_MSG_REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG:
            requestToMcl(message);
            break;

        default:
            RFX_ASSERT(0);
            break;
    }
}

void RtcCdmaSmsController::handleMoSmsResponses(const sp<RfxMessage>& msg) {
    int msg_id = msg->getId();
    switch (msg_id) {
        case RFX_MSG_REQUEST_IMS_SEND_CDMA_SMS:
            {
                sp<RfxMessage> rsp;

                rsp = RfxMessage::obtainResponse(RFX_MSG_REQUEST_IMS_SEND_SMS, msg);
                responseToRilj(rsp);
            }
            break;
        case RFX_MSG_REQUEST_CDMA_SEND_SMS:
            RfxController::onHandleResponse(msg);
            break;
        default:
            RFX_ASSERT(0);
            break;
    }
    getStatusManager()->setIntValue(
        RFX_STATUS_KEY_CDMA_MO_SMS_STATE, CDMA_MO_SMS_SENT);
}

bool RtcCdmaSmsController::onCheckCdmaSupported(const sp<RfxMessage>& msg) {
    if (RfxRilUtils::isC2kSupport() == 0) {
        int msgId = msg->getId();
        switch (msgId) {
            case RFX_MSG_REQUEST_CDMA_SEND_SMS: {
                getStatusManager()->setIntValue(
                        RFX_STATUS_KEY_CDMA_MO_SMS_STATE, CDMA_MO_SMS_SENT);
                /* falls through */
            }
            case RFX_MSG_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION:
            case RFX_MSG_REQUEST_CDMA_DELETE_SMS_ON_RUIM:
            case RFX_MSG_REQUEST_CDMA_WRITE_SMS_TO_RUIM:
            case RFX_MSG_REQUEST_GET_SMS_RUIM_MEM_STATUS:
            case RFX_MSG_REQUEST_CDMA_GET_BROADCAST_SMS_CONFIG:
            case RFX_MSG_REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG: {
                sp<RfxMessage> rsp =
                        RfxMessage::obtainResponse(RIL_E_REQUEST_NOT_SUPPORTED, msg);
                responseToRilj(rsp);
                return true;
            }

            default:
                break;
        }
    }
    return false;
}

bool RtcCdmaSmsController::onCheckSimStatus(const sp<RfxMessage>& msg) {
    int state = getStatusManager()->getIntValue(RFX_STATUS_KEY_SIM_STATE);
    if ((state != RFX_SIM_STATE_READY) && (msg->getError() != RIL_E_SUCCESS)) {
        int msgId = msg->getId();
        switch (msgId) {
            case RFX_MSG_REQUEST_CDMA_DELETE_SMS_ON_RUIM:
            case RFX_MSG_REQUEST_CDMA_WRITE_SMS_TO_RUIM:
            case RFX_MSG_REQUEST_GET_SMS_RUIM_MEM_STATUS: {
                sp<RfxMessage> rsp = RfxMessage::obtainResponse(RIL_E_SIM_ABSENT, msg);
                responseToRilj(rsp);
                return true;
            }

            default:
                break;
        }
    }
    return false;
}

bool RtcCdmaSmsController::onHandleResponse(const sp<RfxMessage>& msg) {
    if (onCheckCdmaSupported(msg) || onCheckSimStatus(msg)) {
        return true;
    }
    int msgId = msg->getId();
    switch (msgId) {
        case RFX_MSG_REQUEST_IMS_SEND_CDMA_SMS:
        case RFX_MSG_REQUEST_CDMA_SEND_SMS:
            handleMoSmsResponses(msg);
            break;

        case RFX_MSG_REQUEST_GET_SMSC_ADDRESS:
        case RFX_MSG_REQUEST_SET_SMSC_ADDRESS:
            handleSmscAdressResponses(msg);
            break;

        case RFX_MSG_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION:
        case RFX_MSG_REQUEST_CDMA_DELETE_SMS_ON_RUIM:
        case RFX_MSG_REQUEST_CDMA_WRITE_SMS_TO_RUIM:
        case RFX_MSG_REQUEST_GET_SMS_RUIM_MEM_STATUS:
        case RFX_MSG_REQUEST_CDMA_GET_BROADCAST_SMS_CONFIG:
        case RFX_MSG_REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG:
            return RfxController::onHandleResponse(msg);

        default:
            RFX_ASSERT(0);
            break;
    }
    return true;
}


bool RtcCdmaSmsController::onPreviewMessage(const sp<RfxMessage>& message) {

    switch (message->getId()) {
        case RFX_MSG_REQUEST_IMS_SEND_SMS:
        case RFX_MSG_REQUEST_CDMA_SEND_SMS: {
            int value = getStatusManager()->getIntValue(
                RFX_STATUS_KEY_CDMA_MO_SMS_STATE,CDMA_MO_SMS_SENT);
            if (value == CDMA_MO_SMS_SENDING && (message->getType() == REQUEST)) {
                return false;
            }
            break;
        }
    }
    return true;
}

bool RtcCdmaSmsController::onCheckIfResumeMessage(const sp<RfxMessage>& message) {
    switch (message->getId()) {
        case RFX_MSG_REQUEST_IMS_SEND_SMS:
        case RFX_MSG_REQUEST_CDMA_SEND_SMS: {
            int value = getStatusManager()->getIntValue(
                RFX_STATUS_KEY_CDMA_MO_SMS_STATE,CDMA_MO_SMS_SENT);
            if ( (value == CDMA_MO_SMS_SENT) && (message->getType() == REQUEST)) {
                return true;
            }
            break;
        }
    }
    return false;
}

bool RtcCdmaSmsController::onCheckIfRejectMessage(
        const sp<RfxMessage>& message, bool isModemPowerOff, int radioState) {
    int msgId = message->getId();
    if (!isModemPowerOff && (radioState == (int)RADIO_STATE_OFF) &&
            (msgId == RFX_MSG_REQUEST_GET_SMSC_ADDRESS ||
             msgId == RFX_MSG_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION ||
             msgId == RFX_MSG_REQUEST_CDMA_DELETE_SMS_ON_RUIM ||
             msgId == RFX_MSG_REQUEST_CDMA_WRITE_SMS_TO_RUIM ||
             msgId == RFX_MSG_REQUEST_GET_SMS_RUIM_MEM_STATUS ||
             msgId == RFX_MSG_REQUEST_CDMA_GET_BROADCAST_SMS_CONFIG ||
             msgId == RFX_MSG_REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG ||
             msgId == RFX_MSG_REQUEST_SET_SMSC_ADDRESS )) {
        return false;
    }
    return RfxController::onCheckIfRejectMessage(message, isModemPowerOff, radioState);
}

bool RtcCdmaSmsController::previewMessage(const sp<RfxMessage>& message) {
    return onPreviewMessage(message);
}

bool RtcCdmaSmsController::checkIfResumeMessage(const sp<RfxMessage>& message) {
    return onCheckIfResumeMessage(message);
}
