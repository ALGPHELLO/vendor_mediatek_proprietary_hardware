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

#include "RmcPhbURCHandler.h"
#include <telephony/mtk_ril.h>
#include "RfxMessageId.h"
#include "RmcWpRequestHandler.h"

#define TRUE  1
#define FALSE 0
#define RFX_LOG_TAG "RmcPhbUrc"

using ::android::String8;

RFX_IMPLEMENT_HANDLER_CLASS(RmcPhbURCHandler, RIL_CMD_PROXY_URC);
RFX_REGISTER_DATA_TO_URC_ID(RfxIntsData, RFX_MSG_URC_PHB_READY_NOTIFICATION);
RFX_REGISTER_DATA_TO_EVENT_ID(RfxVoidData, RFX_MSG_EVENT_PHB_READY_RESET);

RmcPhbURCHandler::RmcPhbURCHandler(int slot_id, int channel_id) :
        RfxBaseHandler(slot_id, channel_id) {
    const char* urc[] = {
        "+EIND: 2",
        "+EIND: 32"
    };
    const int event[] = {
        RFX_MSG_EVENT_PHB_READY_RESET,
    };
    registerToHandleURC(urc, sizeof(urc)/sizeof(char *));
    registerToHandleEvent(event, sizeof(event)/sizeof(int));
}

RmcPhbURCHandler::~RmcPhbURCHandler() {
}

void RmcPhbURCHandler::onHandleUrc(const sp<RfxMclMessage>& msg) {
    char *urc = (char*)msg->getRawUrc()->getLine();
    if (strStartsWith(urc, "+EIND: 2")) {
        onPhbStateChanged(TRUE);
    } else if (strStartsWith(urc, "+EIND: 32")) {
        onPhbStateChanged(FALSE);
    }
}

void RmcPhbURCHandler::onHandleEvent(const sp<RfxMclMessage>& msg) {
    int id = msg->getId();
    switch (id) {
        case RFX_MSG_EVENT_PHB_READY_RESET:  // Currently not used.
            resetPhbReady();
            break;
        default:
            logE(RFX_LOG_TAG, "RmcPhbURCHandler::onHandleEvent: unknown message id: %d", id);
            break;
    }
}

bool RmcPhbURCHandler::onCheckIfRejectMessage(const sp<RfxMclMessage>& msg,
        RIL_RadioState radioState) {
    bool reject = false;
    RFX_UNUSED(msg);
    // ALPS03608851 After leave flight mode and turn on modem,
    // +EIND: 2 may report before AT+EFUN = 0 done.
    // It will be rejected because the radio state is still unavailable.
    // We shouldn't reject PHB URC based on radio state.
    /*
    char *urc = (char*)msg->getRawUrc()->getLine();
    if ((RADIO_STATE_UNAVAILABLE == radioState) &&
            (1 == RmcWpRequestHandler::isWorldModeSwitching())) {
        if (strStartsWith(urc, "+EIND: 2")) {
            reject = false;
        } else if (strStartsWith(urc, "+EIND: 32")) {
            reject = false;
        } else {
            reject = true;
        }
    } else if (RADIO_STATE_UNAVAILABLE == radioState) {
        reject = true;
    }
    */
    logD(RFX_LOG_TAG, "onCheckIfRejectMessage: %d %d", radioState, reject);
    return reject;
}

void RmcPhbURCHandler::onPhbStateChanged(int isPhbReady) {
    // do something
    String8 prop("gsm.sim.ril.phbready");
    bool isModemResetStarted =
        getNonSlotMclStatusManager()->getBoolValue(RFX_STATUS_KEY_MODEM_POWER_OFF, false);
    int isSimInserted = getMclStatusManager()->getIntValue(RFX_STATUS_KEY_CARD_TYPE, FALSE);
    prop.append((m_slot_id == 0)? "" : String8::format(".%d", m_slot_id + 1));

    logI(RFX_LOG_TAG, "onPhbStateChanged slot=%d, isPhbReady=%d", m_slot_id, isPhbReady);

    if (RFX_SLOT_COUNT >= 2) {
        logI(RFX_LOG_TAG, "onPhbStateChanged isSimInserted=%d, isModemResetStarted=%d",
                isSimInserted, isModemResetStarted);

        if (isSimInserted == FALSE) {
            return;
        }

        if ((isPhbReady == TRUE) && (isModemResetStarted)) {
            return;
        }
    }

    if (isPhbReady == TRUE) {
        rfx_property_set(prop, "true");
    } else {
        sendEvent(RFX_MSG_EVENT_PHB_CURRENT_STORAGE_RESET, RfxVoidData(), RIL_CMD_PROXY_1, m_slot_id);
        rfx_property_set(prop, "false");
    }

    // response to TeleCore
    sp<RfxMclMessage> urc = RfxMclMessage::obtainUrc(RFX_MSG_URC_PHB_READY_NOTIFICATION,
            m_slot_id, RfxIntsData((void*)&isPhbReady, sizeof(int)));
    responseToTelCore(urc);
}

void RmcPhbURCHandler::resetPhbReady() {  // Currently not used.
    int isPhbReady = FALSE;

    sendEvent(RFX_MSG_EVENT_PHB_CURRENT_STORAGE_RESET, RfxVoidData(), RIL_CMD_PROXY_1, m_slot_id);
    String8 phbReady("gsm.sim.ril.phbready");

    phbReady.append((m_slot_id == 0)? "" : String8::format(".%d", m_slot_id + 1));
    rfx_property_set(phbReady.string(), "false");

    // response to TeleCore
    sp<RfxMclMessage> urc = RfxMclMessage::obtainUrc(RFX_MSG_URC_PHB_READY_NOTIFICATION,
            m_slot_id, RfxIntsData((void*)&isPhbReady, sizeof(int)));
    responseToTelCore(urc);
}
