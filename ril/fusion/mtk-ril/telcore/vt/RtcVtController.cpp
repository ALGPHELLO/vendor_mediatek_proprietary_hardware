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

#include "RtcVtController.h"

#define RFX_LOG_TAG "VT RIL RTC"

/*****************************************************************************
 * Class RtcDataAllowController
 * this is a none slot controller to manage DATA_ALLOW_REQUEST.
 *****************************************************************************/

RFX_IMPLEMENT_CLASS("RtcVtController", RtcVtController, RfxController);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxVtCallStatusData, RFX_MSG_REQUEST_GET_INFO);
RFX_REGISTER_DATA_TO_EVENT_ID(RfxVtCallStatusData, RFX_MSG_EVENT_CALL_STATUS_UPDATE);
RFX_REGISTER_DATA_TO_EVENT_ID(RfxVtSendMsgData, RFX_MSG_EVENT_VT_SEND_MSG);
RFX_REGISTER_DATA_TO_URC_ID(RfxVtCallStatusData, RFX_MSG_URC_CALL_STATUS);

RtcVtController::RtcVtController() {
}

RtcVtController::~RtcVtController() {
}

void RtcVtController::onInit() {
    RfxController::onInit();  // Required: invoke super class implementation

    logD(RFX_LOG_TAG, "[RTC VT REQ HDLR] onInit");

    // VT dont receive SMG from RIL JAVA
    /*
    const int request_id_list[] = {
            RFX_MSG_REQUEST_GET_INFO,
    };

    // register request
    // NOTE. one id can only be registered by one controller
    for (int i = 0; i < RFX_SLOT_COUNT; i++) {
        registerToHandleRequest(i, request_id_list, sizeof(request_id_list) / sizeof(int));
    }
    */
    /*
    getStatusManager()->registerStatusChanged(
        RFX_STATUS_KEY_NWS_MODE,
        RfxStatusChangeCallback(this, &RtcVtController::onCallStatusChanged));
    */
}

void RtcVtController::onDeinit() {
    logD(RFX_LOG_TAG, "[RTC VT REQ HDLR] onDeinit");
    RfxController::onDeinit();
}

bool RtcVtController::onHandleRequest(const sp<RfxMessage>& message) {

    logD(RFX_LOG_TAG, "[Handle REQ] token = %d, request = %s", message->getPToken(), RFX_ID_TO_STR(message->getId()));

    switch (message->getId()) {
    case RFX_MSG_REQUEST_GET_INFO:
        handleGetInfoRequest(message);
        break;
    default:
        logD(RFX_LOG_TAG, "[Handle REQ] unknown request, ignore!");
        break;
    }
    return true;
}

bool RtcVtController::onHandleResponse(const sp<RfxMessage>& message) {

    logD(RFX_LOG_TAG, "[Handle RSP] token = %d, response = %s", message->getPToken(), RFX_ID_TO_STR(message->getId()));

    switch (message->getId()) {
    case RFX_MSG_REQUEST_GET_INFO:
        handleGetInfoResponse(message);
        break;
    default:
        logD(RFX_LOG_TAG, "[Handle RSP] unknown response, ignore!");
        break;
    }
    return true;
}

bool RtcVtController::onPreviewMessage(const sp<RfxMessage>& message) {
    int requestToken = message->getPToken();
    int requestId = message->getId();

    if (message->getType() == REQUEST) {

        // VT should not receive request, so suspend it
        logD(RFX_LOG_TAG, "[on PRE-MSG] put %s into pending list", RFX_ID_TO_STR(message->getId()));
        return false;
    } else {
        logD(RFX_LOG_TAG, "[on PRE-MSG] execute %s", RFX_ID_TO_STR(message->getId()));
        return true;
    }
}

bool RtcVtController::onCheckIfResumeMessage(const sp<RfxMessage>& message) {
    int requestToken = message->getPToken();
    int requestId = message->getId();

    // false mean FWK can send the message now
    return false;
}

void RtcVtController::handleGetInfoRequest(const sp<RfxMessage>& request) {

    const RIL_VT_CALL_STATUS_UPDATE *pRspData = (const RIL_VT_CALL_STATUS_UPDATE *)request->getData()->getData();
    int call_id = pRspData->call_id;
    int call_state = pRspData->call_state;

    logD(RFX_LOG_TAG, "[GET  INFO ] token = %d, requestId = %d, phone = %d, call id = %d",
            request->getPToken(),
            request->getId(),
            request->getSlotId(),
            call_id);
    /*
    sp<RfxMessage> message = RfxMessage::obtainRequest(request->getSlotId(), request->getId(), request, true);
    requestToMcl(message);
    */
}

void RtcVtController::handleGetInfoResponse(const sp<RfxMessage>& response) {
    logD(RFX_LOG_TAG, "[GET  INFO ] response->getError() = %d, getSlot() = %d", response->getError(), response->getSlotId());

    if (response->getError() > 0) {
        logD(RFX_LOG_TAG, "[GET  INFO ] error");
        return;
    }

    /*
    responseToRilj(response);
    */

    logD(RFX_LOG_TAG, "[GET  INFO ] RFX_MSG_REQUEST_GET_INFO");
}

void RtcVtController::onCallStatusChanged(RfxStatusKeyEnum key, RfxVariant old_value, RfxVariant new_value) {

    RFX_UNUSED(key);
    RFX_UNUSED(old_value);
    RFX_UNUSED(new_value);

    /*
    if (RFX_STATUS_KEY_NWS_MODE == key) {

        int old_state = new_value.asInt();
        int new_state = old_value.asInt();
    }
    */
    /*
    sp<RfxMessage> message = RfxMessage::obtainRequest(request->getSlotId(), request->getId(), request, true);
    requestToMcl(message);
    */
}

