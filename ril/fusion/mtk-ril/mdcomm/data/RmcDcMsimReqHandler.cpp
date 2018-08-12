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
#include "RfxVoidData.h"
#include "RfxIntsData.h"
#include "RmcDcMsimReqHandler.h"

#define RFX_LOG_TAG "RmcDcMsimReqHandler"

/*****************************************************************************
 * Class RmcDcMsimReqHandler
 *****************************************************************************/
RFX_IMPLEMENT_HANDLER_CLASS(RmcDcMsimReqHandler, RIL_CMD_PROXY_5);

RmcDcMsimReqHandler::RmcDcMsimReqHandler(int slot_id, int channel_id)
: RfxBaseHandler(slot_id, channel_id) {
    // The use of EDALLOW=3 to notify modem current project is single PS attach.
    char multiPs[PROPERTY_VALUE_MAX] = {0};
    property_get("ro.mtk_data_config", multiPs, "0");
    // Check if Single-PS or Multi-PS
    if (atoi(multiPs) != 1) {
        RFX_LOG_D(RFX_LOG_TAG,
                "Single PS project - multiPs= %s, send EDALLOW=3", multiPs);
        atSendCommand(String8::format("AT+EDALLOW=3"));
    }

    // TODO: Since AOSP doesn't send ALLOW_DATA on single SIM project from O1,
    // Remove it if AOSP send data allow in single PS project again.
    if (RfxRilUtils::getSimCount() == 1 && slot_id == 0) {
        RFX_LOG_D(RFX_LOG_TAG,
                "Single SIM project, send EDALLOW=1 at boot time");
        atSendCommand(String8::format("AT+EDALLOW=1"));
    }

    const int requestList[] = {
        RFX_MSG_REQUEST_ALLOW_DATA,
    };

    registerToHandleRequest(requestList, sizeof(requestList) / sizeof(int));
}

RmcDcMsimReqHandler::~RmcDcMsimReqHandler() {
}

void RmcDcMsimReqHandler::onHandleRequest(const sp<RfxMclMessage>& msg) {
    int request = msg->getId();
    switch (request) {
        case RFX_MSG_REQUEST_ALLOW_DATA:
            handleRequestAllowData(msg);
            break;
        default:
            RFX_LOG_D(RFX_LOG_TAG, "unknown request, ignore!");
            break;
    }
}

void RmcDcMsimReqHandler::handleRequestAllowData(const sp<RfxMclMessage>& msg) {
    const int *pRspData = (const int *)msg->getData()->getData();
    int dataAllowed = pRspData[0];
    setDataAllowed(dataAllowed, msg);
}

// The use of setDataAllowed() is to indicate that which SIM is allowed to use data.
// AP should make sure both two protocols are NOT set to ON at the same time in the
// case of default data switch flow.
// <Usage> AT+EDALLOW= <data_allowed_on_off>
//                   0 -> off
//                   1 -> on
void RmcDcMsimReqHandler::setDataAllowed(int allowed, const sp<RfxMclMessage>& msg) {
    sp<RfxAtResponse> p_response;
    sp<RfxMclMessage> response;
    sp<RfxMclMessage> urc;
    AT_CME_Error atCause = CME_SUCCESS;
    RIL_Errno cause = RIL_E_SUCCESS;

    RFX_LOG_D(RFX_LOG_TAG, "setDataAllowed: allowed= %d", allowed);

    p_response = atSendCommand(String8::format("AT+EDALLOW=%d", allowed));
    if (p_response->isAtResponseFail()) {
        // Return the error cause (e.g. 4117) for the upper to do error handling.
        atCause = p_response->atGetCmeError();
        RFX_LOG_E(RFX_LOG_TAG, "setDataAllowed: response error");
        goto error;
    }

    response = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_SUCCESS,
            RfxVoidData(), msg);
    responseToTelCore(response);

    return;
error:
    // 4117: multi PS allowed error
    if (atCause == CME_MULTI_ALLOW_ERR) {
        cause = RIL_E_OEM_MULTI_ALLOW_ERR;
    }
    RFX_LOG_E(RFX_LOG_TAG, "setDataAllowed: modem response ERROR cause= %d", cause);
    response = RfxMclMessage::obtainResponse(msg->getId(), (RIL_Errno) cause,
                RfxVoidData(), msg, true);
    responseToTelCore(response);
    return;
}
