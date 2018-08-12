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

#include "RtcDataAllowController.h"
#include <telephony/librilutilsmtk.h>
#include "RfxRilUtils.h"

#define RTC_DAC_LOG_TAG "RTC_DAC"

/*****************************************************************************
 * Class RtcDataAllowController
 * this is a none slot controller to manage DATA_ALLOW_REQUEST.
 *****************************************************************************/

RFX_IMPLEMENT_CLASS("RtcDataAllowController", RtcDataAllowController, RfxController);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData, RFX_MSG_REQUEST_ALLOW_DATA);

RtcDataAllowController::RtcDataAllowController() :
    mDoingDataAllow(false),
    mReqDataAllow(false),
    mDisallowingPeer(0),
    mLastAllowTrueRequest(NULL) {
}

RtcDataAllowController::~RtcDataAllowController() {
}

void RtcDataAllowController::onInit() {
    RfxController::onInit();  // Required: invoke super class implementation
    logD(RTC_DAC_LOG_TAG, "onInit");
    mDoingDataAllow = false;
    mReqDataAllow = false;
    mDisallowingPeer = 0;
    mLastAllowTrueRequest = NULL;
    const int requestIdList[] = {
        RFX_MSG_REQUEST_ALLOW_DATA,  // 123
    };

    // register request
    // NOTE. one id can only be registered by one controller
    for (int i = 0; i < RfxRilUtils::getSimCount(); i++) {
        registerToHandleRequest(i, requestIdList, sizeof(requestIdList) / sizeof(int));
    }
}

void RtcDataAllowController::onDeinit() {
    logD(RTC_DAC_LOG_TAG, "onDeinit");
    mDoingDataAllow = false;
    mReqDataAllow = false;
    mDisallowingPeer = 0;
    mLastAllowTrueRequest = NULL;
    RfxController::onDeinit();
}

bool RtcDataAllowController::onHandleRequest(const sp<RfxMessage>& message) {
    logV(RTC_DAC_LOG_TAG, "[%d]Handle request %s",
            message->getPToken(), RFX_ID_TO_STR(message->getId()));

    switch (message->getId()) {
    case RFX_MSG_REQUEST_ALLOW_DATA:
        preprocessRequest(message);
        break;
    default:
        logD(RTC_DAC_LOG_TAG, "unknown request, ignore!");
        break;
    }
    return true;
}

bool RtcDataAllowController::onHandleResponse(const sp<RfxMessage>& message) {
    logV(RTC_DAC_LOG_TAG, "[%d]Handle response %s.",
            message->getPToken(), RFX_ID_TO_STR(message->getId()));

    switch (message->getId()) {
    case RFX_MSG_REQUEST_ALLOW_DATA:
        handleSetDataAllowResponse(message);
        break;
    default:
        logD(RTC_DAC_LOG_TAG, "unknown response, ignore!");
        break;
    }
    return true;
}

bool RtcDataAllowController::onPreviewMessage(const sp<RfxMessage>& message) {
    // This function will be called in the case of the registered request/response and urc.
    // For instance, register RIL_REQUEST_ALLOW_DATA will receive its request and response.
    // Therefore, it will be called twice in the way and we only care REQUEST in preview message,
    // but still need to return true in the case of type = RESPONSE.
    int requestToken = message->getPToken();
    int requestId = message->getId();

    // Only log REQUEST type.
    if (message->getType() == REQUEST
            && isNeedSuspendRequest(message)) {
        return false;
    } else {
        if (message->getType() == REQUEST && requestId == RFX_MSG_REQUEST_ALLOW_DATA) {
            logD(RTC_DAC_LOG_TAG, "[%d]onPreviewMessage: execute %s, type = [%d]",
                    requestToken,
                    RFX_ID_TO_STR(message->getId()),message->getType());
        }
        return true;
    }
}

bool RtcDataAllowController::isNeedSuspendRequest(const sp<RfxMessage>& message) {
    /*
     * white list for suspend request.
     */
    int requestToken = message->getPToken();
    int requestId = message->getId();
    if (requestId == RFX_MSG_REQUEST_ALLOW_DATA) {
        if (!mDoingDataAllow) {
            logD(RTC_DAC_LOG_TAG, "[%d]isNeedSuspendRequest: First RFX_MSG_REQUEST_ALLOW_DATA"
                    ", set flag on", requestToken);
            mDoingDataAllow = true;
            return false;
        } else {
            return true;
        }
    }
    return false;
}

bool RtcDataAllowController::onCheckIfResumeMessage(const sp<RfxMessage>& message) {
    int requestToken = message->getPToken();
    int requestId = message->getId();

    if (!mDoingDataAllow) {
        return true;
    } else {
        return false;
    }
}

void RtcDataAllowController::handleSetDataAllowRequest(const sp<RfxMessage>& request) {
    const int *pRspData = (const int *)request->getData()->getData();
    bool allowData = pRspData[0];
    mReqDataAllow = allowData;

    logD(RTC_DAC_LOG_TAG, "[%d]handleSetDataAllowRequest: requestId:%d, phone:%d, allow:%d",
            request->getPToken(), request->getId(), request->getSlotId(), allowData);

    sp<RfxMessage> message = RfxMessage::obtainRequest(request->getSlotId(),
            request->getId(), request, true);
    requestToMcl(message);
}

void RtcDataAllowController::handleSetDataAllowResponse(const sp<RfxMessage>& response) {
    logD(RTC_DAC_LOG_TAG,
            "[%d]handleSetDataAllowResponse: allowData = %d, response->getError()=%d, getSlot()=%d",
            response->getPToken(), mReqDataAllow, response->getError(), response->getSlotId());
    /*
     * Modem will return EDALLOW error (4117), in the case of command conflict.
     * The reason for this would be AP send EDALLOW=1 to both SIMs, therefore we need
     * to do error handling in this case
     */
    if (mReqDataAllow && (RIL_E_OEM_MULTI_ALLOW_ERR == response->getError())) {
        handleMultiAllowError(response->getSlotId());
        return;
    }

    if (checkDisallowingPeer()) {
        // Deact Peer Result couldn't pass to RILJ, it will re-attach directly
        logD(RTC_DAC_LOG_TAG, "handleSetDataAllowResponse checkDisallowingPeer");
        return;
    }
    responseToRilj(response);
    mDoingDataAllow = false;
}

bool RtcDataAllowController::preprocessRequest(const sp<RfxMessage>& request) {
    const int *pRspData = (const int *)request->getData()->getData();
    bool allowData = pRspData[0];
    mReqDataAllow = allowData;

    if (allowData) {
        // Copy the request
        // 1. if allow true,  apply for retry.
        mLastAllowTrueRequest = RfxMessage::obtainRequest(request->getSlotId(),
                request->getId(), request, true);
    }
    handleSetDataAllowRequest(request);

    return true;
}

/*
 * Create request to disallow peer phone.
 * The follow will be:
 *   handleMultiAllowError ->
 *   -> handleSetDataAllowResponse -> handleSetDataAllowRequest(mLastAllowTrueRequest)
 * The last step means that re-attach for original attach request.
 */
void RtcDataAllowController::handleMultiAllowError(int activePhoneId) {
    // Check with ims module if we could detach
    int i = 0;
    int allowMessage = INVAILD_ID;

    logD(RTC_DAC_LOG_TAG, "detachPeerPhone: activePhoneId = %d", activePhoneId);

    // Detach peer phone
    for (i = 0; i < RfxRilUtils::getSimCount(); i++) {
        if (i != activePhoneId) {
            // Create disallow request for common
            allowMessage = DISALLOW_DATA;
            sp<RfxMessage> msg = RfxMessage::obtainRequest(
                    i, RFX_MSG_REQUEST_ALLOW_DATA, RfxIntsData(&allowMessage, 1));
            msg->setSlotId(i);

            logD(RTC_DAC_LOG_TAG, "disallowPeerPhone: precheck PhoneId = %d", i);
            // Notify disallow precheck
            mDisallowingPeer++;
            handleSetDataAllowRequest(msg);
        }
    }
}

/*
  * Check if the process is detaching peer.
  * Return true for ignoring response to RILJ because the requests are created by RILProxy.
*/
bool RtcDataAllowController::checkDisallowingPeer() {
    if (mDisallowingPeer > 0) {
        mDisallowingPeer--;
        logD(RTC_DAC_LOG_TAG, "handleSetDataAllowResponse consume disallow peer,"
                " mDisallowingPeer %d", mDisallowingPeer);
        if (mDisallowingPeer == 0) {
            // resume the attach request
            handleSetDataAllowRequest(mLastAllowTrueRequest);
            logD(RTC_DAC_LOG_TAG, "handleSetDataAllowResponse re-attach");
        }
        return true;
    }
    return false;
}

bool RtcDataAllowController::onCheckIfRejectMessage(const sp<RfxMessage>& message,
        bool isModemPowerOff,int radioState) {
    // always execute request
    if((radioState == (int)RADIO_STATE_OFF || radioState == (int)RADIO_STATE_UNAVAILABLE) &&
            message->getId() == RFX_MSG_REQUEST_ALLOW_DATA) {
        return false;
    }
    return RfxController::onCheckIfRejectMessage(message, isModemPowerOff, radioState);
}
