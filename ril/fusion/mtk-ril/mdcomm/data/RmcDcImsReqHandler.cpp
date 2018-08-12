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
#include "RmcDcImsReqHandler.h"
#include "RfxImsBearerNotifyData.h"
#include "RfxIntsData.h"
#include "RfxVoidData.h"
#include "RmcDcUtility.h"
#include "RfxMessageId.h"

#define RFX_LOG_TAG "RmcDcImsReqHandler"

RFX_REGISTER_DATA_TO_URC_ID(RfxImsBearerNotifyData, RFX_MSG_URC_IMS_BEARER_ACTIVATION);
RFX_REGISTER_DATA_TO_URC_ID(RfxImsBearerNotifyData, RFX_MSG_URC_IMS_BEARER_DEACTIVATION);
RFX_REGISTER_DATA_TO_URC_ID(RfxIntsData, RFX_MSG_URC_IMS_BEARER_INIT);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData, RFX_MSG_REQUEST_IMS_BEARER_ACTIVATION_DONE);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData, RFX_MSG_REQUEST_IMS_BEARER_DEACTIVATION_DONE);
RFX_REGISTER_DATA_TO_EVENT_ID(RfxVoidData, RFX_MSG_EVENT_IMS_REQUEST_HANDLER_REGISTER_DONE);

/*****************************************************************************
 * Class RmcDcImsReqHandler
 *****************************************************************************/
RmcDcImsReqHandler::RmcDcImsReqHandler(int slot_id, int channel_id, RmcDcPdnManager* pdnManager)
: RmcDcCommonReqHandler(slot_id, channel_id, pdnManager) {
    const int eventList[] = {
        RFX_MSG_EVENT_URC_HANDLER_REGISTER_DONE
    };
    registerToHandleEvent(eventList, sizeof(eventList) / sizeof(int));
    // notify RmcDcUrcHandler about ImsReqHandler registration done.
    sendEvent(RFX_MSG_EVENT_IMS_REQUEST_HANDLER_REGISTER_DONE, RfxVoidData(),
            RIL_CMD_PROXY_URC, m_slot_id);
    RFX_LOG_D(RFX_LOG_TAG, "RmcDcImsReqHandler Ctor");
}

RmcDcImsReqHandler::~RmcDcImsReqHandler() {
}

void RmcDcImsReqHandler::requestSetupDataCall(const sp<RfxMclMessage>& msg) {
    RmcDcCommonReqHandler::requestSetupDataCall(msg);
}

void RmcDcImsReqHandler::onRegisterUrcDone() {
    sp<RfxAtResponse> p_response;
    String8 cmd("");
    if (RmcDcUtility::isImsSupport()) {
        cmd.append(String8::format("AT+EIMSPDN= \"onoff\", 1"));
    } else {
        cmd.append(String8::format("AT+EIMSPDN= \"onoff\", 0"));
    }
    RFX_LOG_D(RFX_LOG_TAG, "[%d][onRegisterUrcDone], send %s", m_slot_id, cmd.string());
    p_response = atSendCommand(cmd);
    if (p_response->isAtResponseFail()) {
        RFX_LOG_E(RFX_LOG_TAG, "%s returns ERROR", cmd.string());
    }
}

void RmcDcImsReqHandler::onImsBearerNotify(const sp<RfxMclMessage>& msg) {
    //+EIMSPDN: <cmd>, <aid>, <state>, <type>
    //<cmd> :
    //       "notify" -> MD notify AP to construct IMS PDN
    static int ACTION_IMS_BEARER_DEACTIVATION = 0;
    static int ACTION_IMS_BEARER_ACTIVATION = 1;
    int action = -1;

    char *urc = (char*)msg->getData()->getData();
    int rid = m_slot_id;
    int err = 0;
    char *cmdFormat = NULL;
    RIL_IMS_BearerNotification* notification = (RIL_IMS_BearerNotification*)calloc(1, sizeof(RIL_IMS_BearerNotification));
    RfxAtLine *pLine = new RfxAtLine(urc, NULL);
    RFX_LOG_I(RFX_LOG_TAG, "[%d][%s] urc=%s", rid, __FUNCTION__, urc);
    pLine->atTokStart(&err);
    if (err < 0) goto error;

    cmdFormat = pLine->atTokNextstr(&err);
    if (err < 0) {
        RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] ERROR occurs when parsing cmd",
                rid, __FUNCTION__);
        goto error;
    }
    if (strncmp("notify", cmdFormat, strlen("notify")) == 0) {
        int aid = -1;
        int state = -1;
        char *type = NULL;

        aid = pLine->atTokNextint(&err);
        if (err < 0) {
            RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] ERROR occurs when parsing aid",
                    rid, __FUNCTION__);
            goto error;
        }

        action = pLine->atTokNextint(&err);
        if (err < 0) {
            RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] ERROR occurs when parsing state",
                    rid, __FUNCTION__);
            goto error;
        }

        type = pLine->atTokNextstr(&err);
        if (err < 0) {
            RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] ERROR occurs when parsing type",
                    rid, __FUNCTION__);
            goto error;
        }

        notification->phone = m_slot_id;
        notification->aid = aid;
        notification->type = type;
        RFX_LOG_D(RFX_LOG_TAG, "[%d][%s] IMS notification phone=%d, aid=%d, type=%s, action = %d",
                    rid, __FUNCTION__, notification->phone, notification->aid, notification->type, action);

        if (action == ACTION_IMS_BEARER_ACTIVATION) {
            notifyImsBearerActivationRequest(notification);
        } else if (action == ACTION_IMS_BEARER_DEACTIVATION) {
            sp<RfxMclMessage> urc_to_tel_core = RfxMclMessage::obtainUrc(RFX_MSG_URC_IMS_BEARER_DEACTIVATION,
                    m_slot_id, RfxImsBearerNotifyData((void*)notification, sizeof(RIL_IMS_BearerNotification)));
            responseToTelCore(urc_to_tel_core);
        }
    } else if(strncmp("init", cmdFormat, strlen("init")) == 0) {

        RFX_LOG_D(RFX_LOG_TAG, "[%d][%s] IMS notification phone=%d bearer initial...",
                rid, __FUNCTION__, m_slot_id);
        sp<RfxMclMessage> urc_to_tel_core = RfxMclMessage::obtainUrc(
                RFX_MSG_URC_IMS_BEARER_INIT, m_slot_id,
                RfxIntsData((void*)&m_slot_id, sizeof(int)));
        responseToTelCore(urc_to_tel_core);
    }

error:
    AT_LINE_FREE(pLine);
    free(notification);

    return;
}

void RmcDcImsReqHandler::notifyImsBearerActivationRequest(RIL_IMS_BearerNotification* notification) {
    RFX_LOG_D(RFX_LOG_TAG, "%s", __FUNCTION__);
    sp<RfxMclMessage> urc_to_tel_core = RfxMclMessage::obtainUrc(RFX_MSG_URC_IMS_BEARER_ACTIVATION,
            m_slot_id, RfxImsBearerNotifyData((void*)notification, sizeof(RIL_IMS_BearerNotification)));
    responseToTelCore(urc_to_tel_core);
}

void RmcDcImsReqHandler::requestImsBearerActivationDone(const sp<RfxMclMessage>& msg) {
    const int *pReqData = (const int*)msg->getData()->getData();
    int aid = pReqData[0];
    int err = pReqData[1];
    int rid = m_slot_id;
    sp<RfxAtResponse> p_response;
    sp<RfxMclMessage> responseMsg;

    String8 cmd = String8::format("AT+EIMSPDN= \"confirm\", %d, %d", aid, err);
    RFX_LOG_D(RFX_LOG_TAG, "[%d][%s] send %s", rid, __FUNCTION__, cmd.string());
    p_response = atSendCommand(cmd);
    if (p_response->isAtResponseFail()) {
        RFX_LOG_E(RFX_LOG_TAG, "[%d][%s], %s returns ERROR: %d", rid, __FUNCTION__,
            cmd.string(), p_response->getError());
    }
    responseMsg = RfxMclMessage::obtainResponse(RIL_E_SUCCESS, RfxVoidData(), msg);
    responseToTelCore(responseMsg);
}

void RmcDcImsReqHandler::requestImsBearerDeactivationDone(const sp<RfxMclMessage>& msg) {
    const int *pReqData = (const int*)msg->getData()->getData();
    int aid = pReqData[0];
    int err = pReqData[1];
    int rid = m_slot_id;
    sp<RfxAtResponse> p_response;
    sp<RfxMclMessage> responseMsg;

    String8 cmd = String8::format("AT+EIMSPDN= \"confirm\", %d, %d", aid, err);
    RFX_LOG_D(RFX_LOG_TAG, "[%d][%s] send %s", rid, __FUNCTION__, cmd.string());
    p_response = atSendCommand(cmd);
    if (p_response->isAtResponseFail()) {
        RFX_LOG_E(RFX_LOG_TAG, "[%d][%s], %s returns ERROR: %d", rid, __FUNCTION__,
            cmd.string(), p_response->getError());
    }
    responseMsg = RfxMclMessage::obtainResponse(RIL_E_SUCCESS, RfxVoidData(), msg);
    responseToTelCore(responseMsg);
}

void RmcDcImsReqHandler::onPcscfAddressDiscovery(const sp<RfxMclMessage>& msg) {
    //+EIMSPDIS:  <transaction_id>,<em_ind>,<method>, <nw_if_name[]>
    //AT+EIMSPCSCF= <transaction_id>,<method>, <protocol_type>, <port_num>, <addr>
    //AT+EIMSPDIS= <transaction_id>, <method>, <is_success>
    char *urc = (char*)msg->getData()->getData();
    int rid = m_slot_id;
    int err = 0;
    int tranid = -1;
    int em_ind = -1;
    int method = -1;
    char *interfaceId = NULL;
    sp<RfxAtResponse> p_response;
    String8 pcscfCmd;

    RfxAtLine *pLine = new RfxAtLine(urc, NULL);
    RFX_LOG_I(RFX_LOG_TAG, "[%d][%s] urc=%s", rid, __FUNCTION__, urc);

    pLine->atTokStart(&err);
    if (err < 0) goto error;

    tranid = pLine->atTokNextint(&err);
    if (err < 0) {
        RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] ERROR occurs when parsing tranid",
                rid, __FUNCTION__);
        goto error;
    }

    em_ind = pLine->atTokNextint(&err);
    if (err < 0) {
        RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] ERROR occurs when parsing em_ind",
                rid, __FUNCTION__);
        goto error;
    }

    method = pLine->atTokNextint(&err);
    if (err < 0) {
        RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] ERROR occurs when parsing method",
                rid, __FUNCTION__);
        goto error;
    }

    interfaceId = pLine->atTokNextstr(&err);
    if (err < 0) {
        RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] ERROR occurs when parsing interfaceId",
                rid, __FUNCTION__);
        goto error;
    }

    RFX_LOG_D(RFX_LOG_TAG, "[%d][%s] pcscf discovery tranid=%d, em_ind=%d, method = %d,ifaceId=%s",
            rid, __FUNCTION__, tranid, em_ind, method, interfaceId);

    //Current not support pcscf discovery, so always response fail to MD.
    pcscfCmd = String8::format("AT+EIMSPDIS= %d, %d, 0", tranid, method);
    RFX_LOG_D(RFX_LOG_TAG, "[%d][%s], rid, __FUNCTION__, send %s", rid, __FUNCTION__, pcscfCmd.string());
    p_response = atSendCommand(pcscfCmd);
    if (p_response->isAtResponseFail()) {
        RFX_LOG_E(RFX_LOG_TAG, "[%d][%s], %s returns ERROR", rid, __FUNCTION__, pcscfCmd.string());
    }

error:
    AT_LINE_FREE(pLine);
    return;
}

void RmcDcImsReqHandler::onHandleEvent(const sp<RfxMclMessage>& msg) {
    switch (msg->getId()) {
        case RFX_MSG_EVENT_URC_HANDLER_REGISTER_DONE:
            onRegisterUrcDone();
            break;
        default:
            RFX_LOG_W(RFX_LOG_TAG, "[%d][%s]: Unknown event, ignore!", m_slot_id, __FUNCTION__);
            break;
    }
}
