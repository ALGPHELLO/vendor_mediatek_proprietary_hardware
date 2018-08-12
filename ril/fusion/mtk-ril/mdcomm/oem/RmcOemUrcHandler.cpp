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

#include "RmcOemUrcHandler.h"
#include "RfxIntsData.h"

#define RFX_LOG_TAG "RmcOemUrcHandler"

// register handler to channel
RFX_IMPLEMENT_HANDLER_CLASS(RmcOemUrcHandler, RIL_CMD_PROXY_URC);

RFX_REGISTER_DATA_TO_URC_ID(RfxIntsData, RFX_MSG_UNSOL_TX_POWER);

RmcOemUrcHandler::RmcOemUrcHandler(int slot_id, int channel_id) :
        RfxBaseHandler(slot_id, channel_id) {
    const char* urc[] = {
        (char *) "+EWARNING",
        (char *) "+ETXPWR",
    };

    registerToHandleURC(urc, sizeof(urc)/sizeof(char*));
}

RmcOemUrcHandler::~RmcOemUrcHandler() {
}

void RmcOemUrcHandler::onHandleUrc(const sp<RfxMclMessage>& msg) {
    int err = 0;
    RfxAtLine *line = msg->getRawUrc();
    char *data = line->getLine();
    if (strStartsWith(data, "+EWARNING")) {
        handleEWarning(line);
    } else if (strStartsWith(data, "+ETXPWR")) {
        handleTXPower(line);
    } else {
        RFX_LOG_E(RFX_LOG_TAG, "we can not handle this raw urc?! %s", data);
    }
}

void RmcOemUrcHandler::handleEWarning(RfxAtLine *line) {
    int err = 0;
    line->atTokStart(&err);

    if (line->atTokHasmore()) {
       RFX_LOG_D(RFX_LOG_TAG, "handleEWarning, get warning message");
       char warningMessage[255] = {0};
       char *s = line->getLine();
       strncpy(warningMessage, s, 254);

       char modemVersion[256];
       rfx_property_get("gsm.version.baseband", modemVersion, "");
       RFX_LOG_D(RFX_LOG_TAG, "handleEWarning, modem version = %s", modemVersion);

       RFX_LOG_D(RFX_LOG_TAG, "handleEWarning, warningMessage = %s\n", warningMessage);
       RfxRilUtils::handleAee(warningMessage, modemVersion);
    }
}

/**
* Modem will send +ETXPWR after receiving AT+ERFTX
*/
void RmcOemUrcHandler::handleTXPower(RfxAtLine *line) {
    RFX_LOG_D(RFX_LOG_TAG, "handleTXPower: %s", line->getLine());

    int err = 0;
    // response[0]: act (rat);
    // resposne[1]: txPower;
    int response[2] = {0, 0};
    sp<RfxMclMessage> urc;

    line->atTokStart(&err);
    if(err < 0) goto error;

    response[0] = line->atTokNextint(&err);
    if(err < 0) goto error;

    response[1] = line->atTokNextint(&err);
    if(err < 0) goto error;

    RFX_LOG_D(RFX_LOG_TAG, "handleTXPower: %d, %d", response[0], response[1]);
    urc = RfxMclMessage::obtainUrc(RFX_MSG_UNSOL_TX_POWER, m_slot_id, RfxIntsData(response, 2));
    responseToTelCore(urc);
    return;
error:
    RFX_LOG_E(RFX_LOG_TAG, "There is something wrong with the onNotifyTXPower URC");
}