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

#include "RmcImsControlUrcHandler.h"
#include <telephony/mtk_ril.h>

// register handler to channel
RFX_IMPLEMENT_HANDLER_CLASS(RmcImsControlUrcHandler, RIL_CMD_PROXY_URC);

// register data
RFX_REGISTER_DATA_TO_URC_ID(RfxIntsData, RFX_MSG_UNSOL_IMS_DISABLE_START);
RFX_REGISTER_DATA_TO_URC_ID(RfxIntsData, RFX_MSG_UNSOL_IMS_ENABLE_START);
RFX_REGISTER_DATA_TO_URC_ID(RfxIntsData, RFX_MSG_UNSOL_IMS_DISABLE_DONE);
RFX_REGISTER_DATA_TO_URC_ID(RfxIntsData, RFX_MSG_UNSOL_IMS_ENABLE_DONE);
RFX_REGISTER_DATA_TO_URC_ID(RfxIntsData, RFX_MSG_UNSOL_IMS_REGISTRATION_INFO);
RFX_REGISTER_DATA_TO_URC_ID(RfxVoidData, RFX_MSG_UNSOL_RESPONSE_IMS_NETWORK_STATE_CHANGED);
RFX_REGISTER_DATA_TO_URC_ID(RfxIntsData, RFX_MSG_UNSOL_IMS_DEREG_DONE);
RFX_REGISTER_DATA_TO_URC_ID(RfxStringsData, RFX_MSG_UNSOL_SIP_CALL_PROGRESS_INDICATOR);
RFX_REGISTER_DATA_TO_URC_ID(RfxIntsData, RFX_MSG_UNSOL_IMS_SUPPORT_ECC);
RFX_REGISTER_DATA_TO_URC_ID(RfxStringsData, RFX_MSG_UNSOL_IMS_RTP_INFO);
RFX_REGISTER_DATA_TO_URC_ID(RfxIntsData, RFX_MSG_UNSOL_IMS_MULTIIMS_COUNT);

RmcImsControlUrcHandler::RmcImsControlUrcHandler(int slot_id, int channel_id) :
        RfxBaseHandler(slot_id, channel_id) {
    logD(RFX_LOG_TAG, "RmcImsControlUrcHandler constructor");

    int m_slot_id = slot_id;
    int m_channel_id = channel_id;
    const char* urc[] = {
        "+EIMS:",
        "+EIMCFLAG:",
        "+CIREGU:",
        "+EIMSDEREG:",
        "+ESIPCPI",
        "+EIMSESS",
        "+EIMSRTPRPT"
    };

    registerToHandleURC(urc, sizeof(urc)/sizeof(char *));
}

RmcImsControlUrcHandler::~RmcImsControlUrcHandler() {
}

void RmcImsControlUrcHandler::onHandleUrc(const sp<RfxMclMessage>& msg) {
  logD(RFX_LOG_TAG, "onHandleUrc: %s", msg->getRawUrc()->getLine());
  if (strStartsWith(msg->getRawUrc()->getLine(), "+EIMS: 0")) {
        handleImsDisabling(msg);
    } else if (strStartsWith(msg->getRawUrc()->getLine(), "+EIMS: 1")) {
        handleImsEnabling(msg);
    } else if (strStartsWith(msg->getRawUrc()->getLine(), "+EIMCFLAG: 0")) {
        handleImsDisabled(msg);
    } else if (strStartsWith(msg->getRawUrc()->getLine(), "+EIMCFLAG: 1")) {
        handleImsEnabled(msg);
    } else if (strStartsWith(msg->getRawUrc()->getLine(), "+CIREGU")) {
        handleImsRegistrationInfo(msg);
    } else if (strStartsWith(msg->getRawUrc()->getLine(), "+EIMSDEREG")) {
        handleImsDereg(msg);
    } else if (strStartsWith(msg->getRawUrc()->getLine(), "+ESIPCPI")) {
        handleSipMsgIndication(msg);
    } else if (strStartsWith(msg->getRawUrc()->getLine(), "+EIMSESS")) {
        handleImsEccSupportInfo(msg);
    } else if (strStartsWith(msg->getRawUrc()->getLine(), "+EIMSRTPRPT")) {
        handleImsRtpInfo(msg);
    }
}

void RmcImsControlUrcHandler::handleImsDisabling(const sp<RfxMclMessage>& msg) {
    RFX_UNUSED(msg);
    int response = m_slot_id;
    sp<RfxMclMessage> urc;

    urc = RfxMclMessage::obtainUrc(RFX_MSG_UNSOL_IMS_DISABLE_START,
            m_slot_id, RfxIntsData(&response, 1));
    responseToTelCore(urc);
}

void RmcImsControlUrcHandler::handleImsEnabling(const sp<RfxMclMessage>& msg) {
    RFX_UNUSED(msg);
    int response = m_slot_id;
    sp<RfxMclMessage> urc;

    urc = RfxMclMessage::obtainUrc(RFX_MSG_UNSOL_IMS_ENABLE_START,
            m_slot_id, RfxIntsData(&response, 1));
    responseToTelCore(urc);
}

void RmcImsControlUrcHandler::handleImsDisabled(const sp<RfxMclMessage>& msg) {
    RFX_UNUSED(msg);
    int response = m_slot_id;
    sp<RfxMclMessage> urc;

    urc = RfxMclMessage::obtainUrc(RFX_MSG_UNSOL_IMS_DISABLE_DONE,
            m_slot_id, RfxIntsData(&response, 1));
    responseToTelCore(urc);
}

void RmcImsControlUrcHandler::handleImsEnabled(const sp<RfxMclMessage>& msg) {
    RFX_UNUSED(msg);
    int response = m_slot_id;
    sp<RfxMclMessage> urc;

    urc = RfxMclMessage::obtainUrc(RFX_MSG_UNSOL_IMS_ENABLE_DONE,
            m_slot_id, RfxIntsData(&response, 1));
    responseToTelCore(urc);
}

void RmcImsControlUrcHandler::handleImsRegistrationInfo(const sp<RfxMclMessage>& msg) {
    int err;
    int response[3] = {0};
    char* tokenStr = NULL;
    RfxAtLine* line = msg->getRawUrc();
    sp<RfxMclMessage> urc;
    sp<RfxMclMessage> urc2;
    int wfcState;
    const int WFC_STATE_ON = 1 << 4;

    // go to start position
    line->atTokStart(&err);
    if (err < 0) goto error;

    // get reg_info
    response[0] = line->atTokNextint(&err);
    if (err < 0) goto error;

    // get ext_info , value range is 1~FFFFFFFF
    tokenStr = line->atTokNextstr(&err); //hex string
    if (err < 0) {
        // report mode is 1 , no ext_info available
        // set invalid value 0 for upper layer to distinguish if ext_info is availble or not
        response[1] = 0;
    } else if (strlen(tokenStr) > 0) {
        response[1] = (int)strtol(tokenStr, NULL, 16);
    }

    // Fix DSDS bug : transfer rid socket information to IMS Service
    // to judgement for valid sim/phone id
    response[2] = m_slot_id;

    // 93MD IMS framework can NOT get ims RAN type, transfer EWFC state
    // to it. Indicate current RAN type IWLAN or not.
    wfcState = getMclStatusManager()->getIntValue(RFX_STATUS_KEY_WFC_STATE, -1);
    logD(RFX_LOG_TAG, "get WFC state : %d", wfcState);
    if (wfcState == 1) {
        // enable VoWifi bit in ext_info if wfc state was 1
        response[1] |= WFC_STATE_ON;
    }

    logD(RFX_LOG_TAG, "handleImsRegistrationInfo reg_info = %d, ext_info = %d, m_slot_id = %d",
            response[0], response[1], response[2]);

    // MTK defined UNSOL EVENT (with IMS data info attached)
    urc = RfxMclMessage::obtainUrc(RFX_MSG_UNSOL_IMS_REGISTRATION_INFO,
            m_slot_id, RfxIntsData(response, 3));
    responseToTelCore(urc);
    // for AOSP defined UNSOL EVENT (no data)
    urc2 = RfxMclMessage::obtainUrc(RFX_MSG_UNSOL_RESPONSE_IMS_NETWORK_STATE_CHANGED,
            m_slot_id, RfxVoidData());
    responseToTelCore(urc2);
    return;

error:
    logE(RFX_LOG_TAG, "There is something wrong with the +CIREGU");
}

void RmcImsControlUrcHandler::handleImsDereg(const sp<RfxMclMessage>& msg) {
    RFX_UNUSED(msg);
    int response = m_slot_id;
    sp<RfxMclMessage> urc;

    urc = RfxMclMessage::obtainUrc(RFX_MSG_UNSOL_IMS_DEREG_DONE,
            m_slot_id, RfxIntsData(&response, 1));
    responseToTelCore(urc);
}

void RmcImsControlUrcHandler::handleImsEccSupportInfo(const sp<RfxMclMessage>& msg) {
    int err, ratType, supportEmc;
    int response[2] = {0};
    char* tokenStr = NULL;
    RfxAtLine* line = msg->getRawUrc();
    sp<RfxMclMessage> urc;

    // go to start position
    line->atTokStart(&err);
    if (err < 0) goto error;

    // get rat type
    ratType = line->atTokNextint(&err);
    if (err < 0) goto error;

    // get emc support value
    supportEmc = line->atTokNextint(&err);
    if (err < 0) goto error;

    logD(RFX_LOG_TAG, "onImsEccUpdated: rat :%d, support_emc : %d", ratType, supportEmc);
    if ((ratType == 3 || ratType == 4) && supportEmc == 1) {
        response[0] = 1;
    } else {
        response[0] = 0;
    }
    response[1] = m_slot_id;

    urc = RfxMclMessage::obtainUrc(RFX_MSG_UNSOL_IMS_SUPPORT_ECC,
            m_slot_id, RfxIntsData(response, 2));
    responseToTelCore(urc);
    return;

error:
    logE(RFX_LOG_TAG, "There is something wrong with the +EIMSESS");
}

void RmcImsControlUrcHandler::handleSipMsgIndication(const sp<RfxMclMessage>& msg) {
    /*
    * +ESIPCPI: <call_id>,<dir>,<SIP_msg_type>,<method>,<response_code>[,<reason_text>]
    * <call_id>: 0 = incoming call; 1~32 = call id
    * <SIP_msg_type>: 0 = request; 1 = response
    * <method>: 1~32 and mapping to INVITE, PRACK, UPDATE, ACK, CANCEL, BYE, REFER, OK
    * <response_code>: 0-only used when SIP_msg_type is 0(request), else 100~600
    * [<reason_text>]: Optional, The text in the SIP response reason header.
    */
    const int maxLen = 6;
    int rfxMsg = RFX_MSG_UNSOL_SIP_CALL_PROGRESS_INDICATOR;
    bool appendPhoneId = true;
    notifyStringsDataToTcl(msg, rfxMsg, maxLen, appendPhoneId);
}

void RmcImsControlUrcHandler::onHandleTimer() {
    // do something
}

void RmcImsControlUrcHandler::onHandleEvent(const sp<RfxMclMessage>& msg) {
    RFX_UNUSED(msg);
    // handle event
}

void RmcImsControlUrcHandler::handleImsRtpInfo(const sp<RfxMclMessage>& msg) {
    /* +EIMSRTPRPT=<default_ebi>, <network_id>, <timer>, <send pkt lost>, <recv pkt lost> */
    const int maxLen = 5;
    int rfxMsg = RFX_MSG_UNSOL_IMS_RTP_INFO;
    bool appendPhoneId = false;
    notifyStringsDataToTcl(msg, rfxMsg, maxLen, appendPhoneId);
}
