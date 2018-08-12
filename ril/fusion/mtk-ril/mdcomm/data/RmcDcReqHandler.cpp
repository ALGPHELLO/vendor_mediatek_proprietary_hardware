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
#include <regex>
#include <string>
#include "RfxVoidData.h"
#include "RfxMessageId.h"
#include "RmcDcReqHandler.h"
#include "NetAgentService.h"
#include "RfxVoidData.h"
#include "RfxIntsData.h"
#include "RmcDcUtility.h"

#define RFX_LOG_TAG "RmcDcReqHandler"

/*****************************************************************************
 * Class RmcDcReqHandler
 *****************************************************************************/
RFX_IMPLEMENT_OP_PARENT_HANDLER_CLASS(RmcDcReqHandler, RIL_CMD_PROXY_5);

RmcDcReqHandler::RmcDcReqHandler(int slot_id, int channel_id) :
    RfxBaseHandler(slot_id, channel_id),
    m_pRmcDcDefaultReqHdlr(NULL),
    m_pRmcDcImsReqHdlr(NULL),
    m_pRmcDcOnDemandReqHdlr(NULL),
    m_pPdnManager(NULL) {
    // Init NetAgent service.
    bool bSuccess = false;
    do {
        bSuccess = NetAgentService::createNetAgentService();
        if(!bSuccess) {
            RFX_LOG_E(RFX_LOG_TAG, "Fail to create NetAgent service!");
            sleep(10);
            /* never returns */
        }
    } while (!bSuccess);

    // Create Pdn manager first and pass to request handler as parameter.
    RFX_HANDLER_CREATE(m_pPdnManager, RmcDcPdnManager, (m_slot_id, m_channel_id));
    RFX_HANDLER_CREATE(m_pRmcDcDefaultReqHdlr, RmcDcDefaultReqHandler, (m_slot_id, m_channel_id, m_pPdnManager));
    RFX_HANDLER_CREATE(m_pRmcDcOnDemandReqHdlr, RmcDcOnDemandReqHandler, (m_slot_id, m_channel_id, m_pPdnManager));

    const int requestList[] = {
        RFX_MSG_REQUEST_SETUP_DATA_CALL,
        RFX_MSG_REQUEST_DEACTIVATE_DATA_CALL,
        RFX_MSG_REQUEST_DATA_CALL_LIST,
        RFX_MSG_REQUEST_LAST_DATA_CALL_FAIL_CAUSE,
        RFX_MSG_REQUEST_SET_DATA_PROFILE,
        RFX_MSG_REQUEST_SET_INITIAL_ATTACH_APN,
        RFX_MSG_REQUEST_SYNC_DATA_SETTINGS_TO_MD,
        RFX_MSG_REQUEST_RESET_MD_DATA_RETRY_COUNT,
        RFX_MSG_REQUEST_SET_LTE_ACCESS_STRATUM_REPORT,
        RFX_MSG_REQUEST_SET_LTE_UPLINK_DATA_TRANSFER,
        RFX_MSG_REQUEST_IMS_BEARER_ACTIVATION_DONE,
        RFX_MSG_REQUEST_IMS_BEARER_DEACTIVATION_DONE,
        RFX_MSG_REQUEST_CLEAR_ALL_PDN_INFO,
        RFX_MSG_REQUEST_REUSE_IMS_DATA_CALL,
        RFX_MSG_REQUEST_RESEND_SYNC_DATA_SETTINGS_TO_MD
    };
    const int eventList[] = {
        RFX_MSG_EVENT_DATA_NW_PDN_ACT,
        RFX_MSG_EVENT_DATA_NW_PDN_DEACT,
        RFX_MSG_EVENT_DATA_NW_MODIFY,
        RFX_MSG_EVENT_DATA_NW_REACT,
        RFX_MSG_EVENT_DATA_ME_PDN_ACT,
        RFX_MSG_EVENT_DATA_ME_PDN_DEACT,
        RFX_MSG_EVENT_DATA_PDN_CHANGE,
        RFX_MSG_EVENT_DATA_UT_TEST,
        RFX_MSG_EVENT_LTE_ACCESS_STRATUM_STATE_CHANGE,
        RFX_MSG_EVENT_DATA_IMS_PDN_NOTIFY,
        RFX_MSG_EVENT_DATA_PCSCF_ADDRESS_DISCOVERY,
        RFX_MSG_EVENT_CREATE_DC_IMS_HANDLER,
        RFX_MSG_EVENT_QUERY_PCO_WITH_URC,
        RFX_MSG_EVENT_QUERY_PCO_WITH_AID,
    };
    registerToHandleRequest(requestList, sizeof(requestList) / sizeof(int));
    registerToHandleEvent(eventList, sizeof(eventList) / sizeof(int));

    sendEvent(RFX_MSG_EVENT_CREATE_DC_IMS_HANDLER, RfxVoidData(),
            RIL_CMD_PROXY_5, m_slot_id);
}

RmcDcReqHandler::~RmcDcReqHandler() {
}

void RmcDcReqHandler::onHandleRequest(const sp<RfxMclMessage>& msg) {
    switch (msg->getId()) {
        case RFX_MSG_REQUEST_SETUP_DATA_CALL:
            handleSetupDataCallRequest(msg);
            break;
        case RFX_MSG_REQUEST_DEACTIVATE_DATA_CALL:
            handleDeactivateDataCallRequest(msg);
            break;
        case RFX_MSG_REQUEST_LAST_DATA_CALL_FAIL_CAUSE:
            handleLastFailCauseRequest(msg);
            break;
        case RFX_MSG_REQUEST_DATA_CALL_LIST:
            handleDataCallListRequest(msg);
            break;
        case RFX_MSG_REQUEST_SET_DATA_PROFILE:
            handleSyncApnTableRequest(msg);
            break;
        case RFX_MSG_REQUEST_SET_INITIAL_ATTACH_APN:
            handleSetInitialAttachApnRequest(msg);
            break;
        case RFX_MSG_REQUEST_SYNC_DATA_SETTINGS_TO_MD:
            handleSyncDataSettingsToMdRequest(msg);
            break;
        case RFX_MSG_REQUEST_RESET_MD_DATA_RETRY_COUNT:
            handleResetMdDataRetryCount(msg);
            break;
        case RFX_MSG_REQUEST_SET_LTE_ACCESS_STRATUM_REPORT:
            handleSetLteAccessStratumReportRequest(msg);
            break;
        case RFX_MSG_REQUEST_SET_LTE_UPLINK_DATA_TRANSFER:
            handleSetLteUplinkDataTransferRequest(msg);
            break;
        case RFX_MSG_REQUEST_CLEAR_ALL_PDN_INFO:
            handleClearAllPdnInfoRequest(msg);
            break;
        /// M: Ims Data Framework {@
        case RFX_MSG_REQUEST_IMS_BEARER_ACTIVATION_DONE:
            handleImsBearerActivationDone(msg);
            break;
        case RFX_MSG_REQUEST_IMS_BEARER_DEACTIVATION_DONE:
            handleImsBearerDeactivationDone(msg);
        /// @}
            break;
        case RFX_MSG_REQUEST_REUSE_IMS_DATA_CALL:
            RFX_LOG_D(RFX_LOG_TAG, "RFX_MSG_REQUEST_REUSE_IMS_DATA_CALL");
            handleSetupDataCallRequest(msg);
            break;
        case RFX_MSG_REQUEST_RESEND_SYNC_DATA_SETTINGS_TO_MD:
            handleResendSyncDataSettingsToMd(msg);
            break;
        default:
            RFX_LOG_D(RFX_LOG_TAG, "unknown request, ignore!");
            break;
    }
}

void RmcDcReqHandler::onHandleEvent(const sp<RfxMclMessage>& msg) {
    switch (msg->getId()) {
        case RFX_MSG_EVENT_DATA_NW_PDN_ACT:
            handleNwPdnAct(msg);
            break;
        case RFX_MSG_EVENT_DATA_NW_PDN_DEACT:
            handleNwPdnDeact(msg);
            break;
        case RFX_MSG_EVENT_DATA_NW_MODIFY:
            handleNwModify(msg);
            break;
        case RFX_MSG_EVENT_DATA_NW_REACT:
            handleNwReact(msg);
            break;
        case RFX_MSG_EVENT_DATA_ME_PDN_ACT:
            handleMePdnAct(msg);
            break;
        case RFX_MSG_EVENT_DATA_ME_PDN_DEACT:
            handleMePdnDeact(msg);
            break;
        case RFX_MSG_EVENT_DATA_PDN_CHANGE:
            handlePdnChange(msg);
            break;
        case RFX_MSG_EVENT_DATA_UT_TEST:
            handleUtTest(msg);
            break;
        case RFX_MSG_EVENT_LTE_ACCESS_STRATUM_STATE_CHANGE:
            handleLteAccessStratumStateChange(msg);
            break;
        /// M: Ims Data Framework {@
        case RFX_MSG_EVENT_DATA_IMS_PDN_NOTIFY:
            handleImsBearerNotify(msg);
            break;
        case RFX_MSG_EVENT_DATA_PCSCF_ADDRESS_DISCOVERY:
            handlePcscfAddressDiscovery(msg);
            break;
        case RFX_MSG_EVENT_CREATE_DC_IMS_HANDLER:
            createDcImsHandler();
            break;
        /// @}
        case RFX_MSG_EVENT_QUERY_PCO_WITH_URC:
        case RFX_MSG_EVENT_QUERY_PCO_WITH_AID:
            handleQueryPco(msg);
            break;
        default:
            RFX_LOG_E(RFX_LOG_TAG, "should not be here");
            break;
    }
}

void RmcDcReqHandler::createDcImsHandler() {
    RFX_LOG_D(RFX_LOG_TAG, "createDcImsHandler");
    RFX_HANDLER_CREATE(m_pRmcDcImsReqHdlr, RmcDcImsReqHandler, (m_slot_id, m_channel_id, m_pPdnManager));
}

void RmcDcReqHandler::handleDataCallListRequest(const sp<RfxMclMessage>& msg) {
    RFX_ASSERT(m_pRmcDcDefaultReqHdlr != NULL);
    m_pRmcDcDefaultReqHdlr->requestOrSendDataCallList(msg);
}

void RmcDcReqHandler::handleLastFailCauseRequest(const sp<RfxMclMessage>& msg) {
    RFX_ASSERT(m_pRmcDcDefaultReqHdlr != NULL);
    m_pRmcDcDefaultReqHdlr->requestLastFailCause(msg);

}

void RmcDcReqHandler::handleSetupDataCallRequest(const sp<RfxMclMessage>& msg) {
    const char **pReqData = (const char **)msg->getData()->getData();
    int nProfileId = atoi(pReqData[1]);
    if (RIL_DATA_PROFILE_DEFAULT == nProfileId) {
        RFX_ASSERT(m_pRmcDcDefaultReqHdlr != NULL);
        m_pRmcDcDefaultReqHdlr->requestSetupDataCall(msg);
    } else {
        RFX_ASSERT(m_pRmcDcOnDemandReqHdlr != NULL);
        m_pRmcDcOnDemandReqHdlr->requestSetupDataCall(msg);
    }
}

void RmcDcReqHandler::handleDeactivateDataCallRequest(const sp<RfxMclMessage>& msg) {
    RFX_ASSERT(m_pRmcDcDefaultReqHdlr != NULL);
    m_pRmcDcDefaultReqHdlr->requestDeactivateDataCall(msg);
}

void RmcDcReqHandler::handleNwPdnAct(const sp<RfxMclMessage>& msg) {
    RFX_ASSERT(m_pRmcDcDefaultReqHdlr != NULL);
    m_pRmcDcDefaultReqHdlr->onNwPdnAct(msg);
}

void RmcDcReqHandler::handleNwPdnDeact(const sp<RfxMclMessage>& msg) {
    RFX_ASSERT(m_pRmcDcDefaultReqHdlr != NULL);
    m_pRmcDcDefaultReqHdlr->onNwPdnDeact(msg);
}

void RmcDcReqHandler::handleNwModify(const sp<RfxMclMessage>& msg) {
    RFX_ASSERT(m_pRmcDcDefaultReqHdlr != NULL);
    m_pRmcDcDefaultReqHdlr->onNwModify(msg);
}

void RmcDcReqHandler::handleNwReact(const sp<RfxMclMessage>& msg) {
    RFX_ASSERT(m_pRmcDcDefaultReqHdlr != NULL);
    m_pRmcDcDefaultReqHdlr->onNwReact(msg);
}

void RmcDcReqHandler::handleMePdnAct(const sp<RfxMclMessage>& msg) {
    RFX_ASSERT(m_pRmcDcDefaultReqHdlr != NULL);
    m_pRmcDcDefaultReqHdlr->onMePdnAct(msg);
}

void RmcDcReqHandler::handleMePdnDeact(const sp<RfxMclMessage>& msg) {
    RFX_ASSERT(m_pRmcDcDefaultReqHdlr != NULL);
    m_pRmcDcDefaultReqHdlr->onMePdnDeact(msg);
}

void RmcDcReqHandler::handlePdnChange(const sp<RfxMclMessage>& msg) {
    RFX_ASSERT(m_pRmcDcDefaultReqHdlr != NULL);
    m_pRmcDcDefaultReqHdlr->onPdnChange(msg);
}
void RmcDcReqHandler::handleSyncApnTableRequest(const sp<RfxMclMessage>& msg) {
    RFX_ASSERT(m_pRmcDcDefaultReqHdlr != NULL);
    m_pRmcDcDefaultReqHdlr->requestSyncApnTable(msg);
}

void RmcDcReqHandler::handleSetInitialAttachApnRequest(const sp<RfxMclMessage>& msg) {
    int requestId = msg->getId();
    RIL_InitialAttachApn_v15 *iaData = (RIL_InitialAttachApn_v15 *)msg->getData()->getData();
    int err;
    sp<RfxAtResponse> pAtResponse;
    sp<RfxMclMessage> pMclResponse;
    ApnTableReq apnTableReq = {NULL, NULL, NULL, 0, NULL, 0};
    int cmdIndex = 0;
    char apn[MAX_APN_NAME_LENGTH] = {0};

    asprintf(&apnTableReq.apn, "%s", iaData->apn);
    asprintf(&apnTableReq.username, "%s", iaData->username);
    asprintf(&apnTableReq.password, "%s", iaData->password);
    apnTableReq.apnTypeId = APN_TYPE_INVALID; //for skip check apnType in getCmdIndexFromApnTable API
    asprintf(&apnTableReq.protocol, "%s", iaData->protocol);
    apnTableReq.authtype = iaData->authtype;
    cmdIndex = RmcDcCommonReqHandler::getCmdIndexFromApnTable(m_slot_id, &apnTableReq);

    RFX_LOG_D(RFX_LOG_TAG, "[%d][%s] cmdIndex = %d", m_slot_id, __FUNCTION__, cmdIndex);

    strncpy(apn, iaData->apn, MAX_APN_NAME_LENGTH-1);
    RmcDcCommonReqHandler::addEscapeSequence(apn);
    pAtResponse = atSendCommand(String8::format("AT+EIAAPN=\"%s\",%d", apn, cmdIndex));
    if(pAtResponse->isAtResponseFail()) {
        pMclResponse = RfxMclMessage::obtainResponse(msg->getId(),
                RIL_E_GENERIC_FAILURE, RfxVoidData(), msg, false);
    } else {
        pMclResponse = RfxMclMessage::obtainResponse(msg->getId(),
                RIL_E_SUCCESS, RfxVoidData(), msg, false);
    }

    FREEIF(apnTableReq.apn);
    FREEIF(apnTableReq.username);
    FREEIF(apnTableReq.password);
    FREEIF(apnTableReq.protocol);

    // response to TeleCore
    responseToTelCore(pMclResponse);
}

void RmcDcReqHandler::handleSyncDataSettingsToMdRequest(const sp<RfxMclMessage>& msg) {
    RFX_ASSERT(m_pRmcDcDefaultReqHdlr != NULL);
    m_pRmcDcDefaultReqHdlr->requestSyncDataSettingsToMd(msg);
}

void RmcDcReqHandler::handleResetMdDataRetryCount(const sp<RfxMclMessage>& msg) {
    RFX_ASSERT(m_pRmcDcDefaultReqHdlr != NULL);
    m_pRmcDcDefaultReqHdlr->requestResetMdDataRetryCount(msg);
}

void RmcDcReqHandler::handleUtTest(const sp<RfxMclMessage>& msg) {
    RfxAtLine *pLine = NULL;
    char *strParam = NULL;
    char *urc = (char*)msg->getData()->getData();
    int rid = m_slot_id;
    int aid = INVALID_AID;
    int err = 0;
    NetAgentService *pNetAgentService = NetAgentService::getInstance();
    int transIntfId = INVALID_TRANS_INTF_ID;

    RFX_LOG_D(RFX_LOG_TAG, "[%d][%s] receive %s", rid, __FUNCTION__, urc);

    if (m_pPdnManager == NULL) {
        RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] ERROR due to PdnManager is NULL",
                rid, __FUNCTION__);
        return;
    }

    pLine = new RfxAtLine(urc, NULL);
    pLine->atTokStart(&err);
    if (err < 0) {
        RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] ERROR occurs when token start",
                rid, __FUNCTION__);
        AT_LINE_FREE(pLine);
        return;
    }

    strParam = pLine->atTokNextstr(&err);
    if (err < 0){
        RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] ERROR occurs when parsing string param.",
                rid, __FUNCTION__);
        AT_LINE_FREE(pLine);
        return;
    }

    if (strParam != NULL && strlen(strParam) > 0) {
        if (strncmp(CLEAR_ALL_PDN_TABLE, strParam, strlen(CLEAR_ALL_PDN_TABLE)) == 0) {
            m_pPdnManager->clearAllPdnInfo();
        } else if (strncmp(CLEAR_PDN_TABLE, strParam, strlen(CLEAR_PDN_TABLE)) == 0) {
            strParam += strlen(CLEAR_PDN_TABLE);
            aid = RmcDcUtility::stringToBinaryBase(strParam, 10, &err);
            if (err < 0) {
                RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] ERROR occurs when converting string aid"
                        " to binary, err = %d", rid, __FUNCTION__, err);
                AT_LINE_FREE(pLine);
                return;
            }
            m_pPdnManager->clearPdnInfo(aid);
        } else if (strncmp(SET_TRANSACTION_INTERFACE_ID, strParam,
                strlen(SET_TRANSACTION_INTERFACE_ID)) == 0) {
            if (NULL == pNetAgentService) {
                RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] NetAgentService is NULL, return error",
                        rid, __FUNCTION__);
            } else {
                strParam += strlen(SET_TRANSACTION_INTERFACE_ID);
                transIntfId = RmcDcUtility::stringToBinaryBase(strParam, 10, &err);
                if (err < 0) {
                    RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] ERROR occurs when converting string transIntfId"
                            " to binary, err = %d", rid, __FUNCTION__, err);
                    AT_LINE_FREE(pLine);
                    return;
                }
                pNetAgentService->setTransactionInterfaceId(transIntfId);
            }
        } else if (strncmp(REMOVE_TRANSACTION_INTERFACE_ID, strParam,
                strlen(REMOVE_TRANSACTION_INTERFACE_ID)) == 0) {
            if (NULL == pNetAgentService) {
                RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] NetAgentService is NULL, return error",
                        rid, __FUNCTION__);
            } else {
                strParam += strlen(REMOVE_TRANSACTION_INTERFACE_ID);
                transIntfId = RmcDcUtility::stringToBinaryBase(strParam, 10, &err);
                if (err < 0) {
                    RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] ERROR occurs when converting string transIntfId"
                            " to binary, err = %d", rid, __FUNCTION__, err);
                    AT_LINE_FREE(pLine);
                    return;
                }
                pNetAgentService->removeTransactionInterfaceId(transIntfId);
            }
        } else if (strncmp(REMOVE_ALL_TRANSACTION_INTERFACE_ID, strParam,
                strlen(REMOVE_ALL_TRANSACTION_INTERFACE_ID)) == 0) {
            if (NULL == pNetAgentService) {
                RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] NetAgentService is NULL, return error",
                        rid, __FUNCTION__);
            } else {
                pNetAgentService->removeAllTransactionInterfaceId();
            }
        } else if (strncmp("AT", strParam, strlen("AT")) == 0) {
            atSendCommand("AT");
        } else if (strncmp("INIT AT CMDS", strParam, strlen("INIT AT CMDS")) == 0) {
            m_pPdnManager->initAtCmds();
        }
    }

    AT_LINE_FREE(pLine);
}

void RmcDcReqHandler::handleSetLteAccessStratumReportRequest(const sp<RfxMclMessage>& msg) {
    RFX_ASSERT(m_pRmcDcDefaultReqHdlr != NULL);
    m_pRmcDcDefaultReqHdlr->requestSetLteAccessStratumReport(msg);
}

void RmcDcReqHandler::handleLteAccessStratumStateChange(const sp<RfxMclMessage>& msg) {
    RFX_ASSERT(m_pRmcDcDefaultReqHdlr != NULL);
    m_pRmcDcDefaultReqHdlr->onLteAccessStratumStateChange(msg);
}

void RmcDcReqHandler::handleSetLteUplinkDataTransferRequest(const sp<RfxMclMessage>& msg) {
    RFX_ASSERT(m_pRmcDcDefaultReqHdlr != NULL);
    m_pRmcDcDefaultReqHdlr->requestSetLteUplinkDataTransfer(msg);
}

void RmcDcReqHandler::handleClearAllPdnInfoRequest(const sp<RfxMclMessage>& msg) {
    RFX_ASSERT(m_pRmcDcDefaultReqHdlr != NULL);
    m_pRmcDcDefaultReqHdlr->requestClearAllPdnInfo(msg);
}
/// M: Ims Data Framework {@
void RmcDcReqHandler::handleImsBearerNotify(const sp<RfxMclMessage>& msg) {
    m_pRmcDcImsReqHdlr->onImsBearerNotify(msg);
}

void RmcDcReqHandler::handleImsBearerActivationDone(const sp<RfxMclMessage>& msg) {
    m_pRmcDcImsReqHdlr->requestImsBearerActivationDone(msg);
}

void RmcDcReqHandler::handleImsBearerDeactivationDone(const sp<RfxMclMessage>& msg) {
    m_pRmcDcImsReqHdlr->requestImsBearerDeactivationDone(msg);
}

void RmcDcReqHandler::handlePcscfAddressDiscovery(const sp<RfxMclMessage>& msg) {
    m_pRmcDcImsReqHdlr->onPcscfAddressDiscovery(msg);
}
/// @}

void RmcDcReqHandler::handleResendSyncDataSettingsToMd(const sp<RfxMclMessage>& msg) {
    RFX_ASSERT(m_pRmcDcDefaultReqHdlr != NULL);
    m_pRmcDcDefaultReqHdlr->requestResendSyncDataSettingsToMd(msg);
}

void RmcDcReqHandler::handleQueryPco(const sp<RfxMclMessage>& msg) {
    int msgId = msg->getId();
    int idx = INVALID_AID;
    int ia = 0;
    std::string apn;
    std::string iptype;
    std::string urc;
    std::smatch m;
    std::string prefixR1 = "CGEV: ME MODIFY";
    std::string prefixR2 = "EIAREG: ME ATTACH";
    static std::regex r1(prefixR1 + " *(\\d+)");
    static std::regex r2(prefixR2 + " *\"([^\"]+)\", *(\\w+), *(\\d+)");
    static std::regex r2a(prefixR2 + " *\"([^\"]+)\\.mnc\\d+\\.mcc\\d+[^\"]+\", *(\\w+), *(\\d+)");

    if (msgId == RFX_MSG_EVENT_QUERY_PCO_WITH_AID) {
        idx = *((int*) msg->getData()->getData());
    } else if (msgId == RFX_MSG_EVENT_QUERY_PCO_WITH_URC) {
        urc = std::string((char*)msg->getData()->getData());

        RFX_LOG_D(RFX_LOG_TAG, "[%d][%s] urc %s", m_slot_id, __FUNCTION__, urc.c_str());
        if (urc.find(prefixR1) != std::string::npos) {
            std::regex_search(urc, m, r1);
            ia = 0;

            if (m.empty() || m.size() != 2) {
                for (std::size_t n = 0; n < m.size(); ++n) {
                    RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] match error [%s]",
                        m_slot_id, __FUNCTION__, m.str(n).c_str());
                }
                RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] ME MODIFY content error, return",
                    m_slot_id, __FUNCTION__);
                return;
            } else {
                idx = std::stoi(m.str(1), NULL, 10);
            }
        } else if (urc.find(prefixR2) != std::string::npos) {
            std::regex_search(urc, m, r2a);
            if (m.empty() || m.size() != 4) {
                std::regex_search(urc, m, r2);
            }
            ia = 1;

            if (m.empty() || m.size() != 4) {
                for (std::size_t n = 0; n < m.size(); ++n) {
                    RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] match error [%s]",
                        m_slot_id, __FUNCTION__, m.str(n).c_str());
                }
                RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] ME ATTACH content error, #matched %d, return",
                    m_slot_id, __FUNCTION__, (int) m.size());
                return;
            } else {
                apn = std::string(m.str(1));
                iptype = std::string(m.str(2));
                idx = std::stoi(m.str(3), NULL, 10);

                RFX_LOG_D(RFX_LOG_TAG, "[%d][%s] apn %s, iptype %s, apn index %d",
                    m_slot_id, __FUNCTION__, apn.c_str(), iptype.c_str(), idx);
            }
        } else {
            RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] urc format error, return",
                m_slot_id, __FUNCTION__);
            return;
        }
    } else {
        RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] msgid error, return",
            m_slot_id, __FUNCTION__);
        return;
    }

    m_pRmcDcDefaultReqHdlr->requestQueryPco(idx, ia, apn.c_str(), iptype.c_str());
}

