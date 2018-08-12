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
#include "RfxBaseHandler.h"
#include "RfxSimIoRspData.h"
#include "RfxSimIoData.h"
#include "RfxSimAuthData.h"
#include "RfxSimGenAuthData.h"
#include "RfxSimOpenChannelData.h"
#include "RfxSimApduData.h"
#include "RfxVoidData.h"
#include "RfxStringData.h"
#include "RfxStringsData.h"
#include "RfxIntsData.h"
#include "RfxSimStatusData.h"
#include "RfxRawData.h"
#include "RmcSimBaseHandler.h"
#include "RmcCommSimRequestHandler.h"
#include <telephony/mtk_ril.h>
#include "rfx_properties.h"
#include "RfxChannel.h"
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <cstring>
#include <pthread.h>
#include "RfxMessageId.h"
#include <cutils/properties.h>
#include "RfxRilUtils.h"

#ifdef __cplusplus
extern "C"
{
#endif
#include <hardware_legacy/power.h>
#include "base64.h"
#include "usim_fcp_parser.h"
#include <hardware/ril/librilutils/proto/sap-api.pb.h>
#include "pb_decode.h"
#include "pb_encode.h"
#ifdef __cplusplus
}
#endif

// External SIM [Start]
#include "RfxVsimEventData.h"
#include "RfxVsimOpEventData.h"
#include "RmcCommSimUrcHandler.h"
// External SIM [End]


using ::android::String8;
using namespace std;

static const int ch1ReqList[] = {
    RFX_MSG_REQUEST_GET_SIM_STATUS,
    RFX_MSG_REQUEST_ENTER_SIM_PIN,
    RFX_MSG_REQUEST_ENTER_SIM_PUK,
    RFX_MSG_REQUEST_ENTER_SIM_PIN2,
    RFX_MSG_REQUEST_ENTER_SIM_PUK2,
    RFX_MSG_REQUEST_CHANGE_SIM_PIN,
    RFX_MSG_REQUEST_CHANGE_SIM_PIN2,
    RFX_MSG_REQUEST_SIM_ENTER_NETWORK_DEPERSONALIZATION,
    RFX_MSG_REQUEST_SIM_IO,
    RFX_MSG_REQUEST_ISIM_AUTHENTICATION,
    RFX_MSG_REQUEST_GENERAL_SIM_AUTH,
    RFX_MSG_REQUEST_SIM_OPEN_CHANNEL,
    RFX_MSG_REQUEST_SET_SIM_CARD_POWER,
    RFX_MSG_REQUEST_SIM_CLOSE_CHANNEL,
    RFX_MSG_REQUEST_SIM_TRANSMIT_APDU_BASIC,
    RFX_MSG_REQUEST_SIM_TRANSMIT_APDU_CHANNEL,
    RFX_MSG_REQUEST_SIM_GET_ATR,
    RFX_MSG_REQUEST_LOCAL_SIM_SAP_RESET,
    RFX_MSG_REQUEST_SIM_SAP_CONNECT,
    RFX_MSG_REQUEST_SIM_SAP_DISCONNECT,
    RFX_MSG_REQUEST_SIM_SAP_APDU,
    RFX_MSG_REQUEST_SIM_SAP_TRANSFER_ATR,
    RFX_MSG_REQUEST_SIM_SAP_POWER,
    RFX_MSG_REQUEST_SIM_SAP_RESET_SIM,
    RFX_MSG_REQUEST_SIM_SAP_SET_TRANSFER_PROTOCOL,
    RFX_MSG_REQUEST_SIM_SAP_ERROR_RESP,
    RFX_MSG_REQUEST_QUERY_FACILITY_LOCK,
    RFX_MSG_REQUEST_SET_FACILITY_LOCK
};

static const int ch3ReqList[] = {
    RFX_MSG_REQUEST_SIM_AUTHENTICATION
};

static const int ch1EventList[] = {
    RFX_MSG_EVENT_SIM_QUERY_ICCID,
    RFX_MSG_EVENT_SIM_DETECT_SIM,
    RFX_MSG_EVENT_SIM_SIM_AUTHENTICATION,
    RFX_MSG_EVENT_SIM_USIM_AUTHENTICATION,
};

int RmcCommSimRequestHandler::mOldTestSimValue[MAX_SIM_COUNT] = {-1, -1, -1, -1};

RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxSimStatusData, RFX_MSG_REQUEST_GET_SIM_STATUS);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxSimIoData, RfxSimIoRspData, RFX_MSG_REQUEST_SIM_IO);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxStringData, RfxStringData, RFX_MSG_REQUEST_ISIM_AUTHENTICATION);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxSimAuthData, RfxSimIoRspData, RFX_MSG_REQUEST_SIM_AUTHENTICATION);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxSimGenAuthData, RfxSimIoRspData, RFX_MSG_REQUEST_GENERAL_SIM_AUTH);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxSimOpenChannelData, RfxIntsData, RFX_MSG_REQUEST_SIM_OPEN_CHANNEL);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData, RFX_MSG_REQUEST_SIM_CLOSE_CHANNEL);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData, RFX_MSG_REQUEST_SET_SIM_CARD_POWER);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxSimApduData, RfxSimIoRspData,
        RFX_MSG_REQUEST_SIM_TRANSMIT_APDU_BASIC);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxSimApduData, RfxSimIoRspData,
        RFX_MSG_REQUEST_SIM_TRANSMIT_APDU_CHANNEL);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxStringData, RFX_MSG_REQUEST_SIM_GET_ATR);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxStringsData, RfxIntsData, RFX_MSG_REQUEST_ENTER_SIM_PIN);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxStringsData, RfxIntsData, RFX_MSG_REQUEST_ENTER_SIM_PUK);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxStringsData, RfxIntsData, RFX_MSG_REQUEST_ENTER_SIM_PIN2);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxStringsData, RfxIntsData, RFX_MSG_REQUEST_ENTER_SIM_PUK2);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxStringsData, RfxIntsData, RFX_MSG_REQUEST_CHANGE_SIM_PIN);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxStringsData, RfxIntsData, RFX_MSG_REQUEST_CHANGE_SIM_PIN2);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxStringsData, RfxIntsData,
        RFX_MSG_REQUEST_SIM_ENTER_NETWORK_DEPERSONALIZATION);
RFX_REGISTER_DATA_TO_URC_ID(RfxVoidData, RFX_MSG_URC_RESPONSE_SIM_STATUS_CHANGED);
RFX_REGISTER_DATA_TO_URC_ID(RfxVoidData, RFX_MSG_URC_SIM_COMMON_SLOT_NO_CHANGED);
RFX_REGISTER_DATA_TO_URC_ID(RfxVoidData, RFX_MSG_URC_TRAY_PLUG_IN);
RFX_REGISTER_DATA_TO_URC_ID(RfxVoidData, RFX_MSG_URC_SIM_PLUG_OUT);
RFX_REGISTER_DATA_TO_URC_ID(RfxVoidData, RFX_MSG_URC_SIM_PLUG_IN);
RFX_REGISTER_DATA_TO_URC_ID(RfxVoidData, RFX_MSG_URC_SIM_IMSI_REFRESH_DONE);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxRawData, RfxRawData, \
        RFX_MSG_REQUEST_SIM_SAP_CONNECT);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxRawData, RfxRawData, \
        RFX_MSG_REQUEST_SIM_SAP_DISCONNECT);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxRawData, RfxRawData, \
        RFX_MSG_REQUEST_SIM_SAP_APDU);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxRawData, RfxRawData, \
        RFX_MSG_REQUEST_SIM_SAP_TRANSFER_ATR);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxRawData, RfxRawData, \
        RFX_MSG_REQUEST_SIM_SAP_POWER);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxRawData, RfxRawData, \
        RFX_MSG_REQUEST_SIM_SAP_RESET_SIM);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxRawData, RfxRawData, \
        RFX_MSG_REQUEST_SIM_SAP_SET_TRANSFER_PROTOCOL);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxRawData, \
        RFX_MSG_REQUEST_SIM_SAP_ERROR_RESP);

RFX_REGISTER_DATA_TO_EVENT_ID(RfxVoidData, RFX_MSG_EVENT_SIM_QUERY_ICCID);
RFX_REGISTER_DATA_TO_EVENT_ID(RfxStringData, RFX_MSG_EVENT_SIM_DETECT_SIM);
RFX_REGISTER_DATA_TO_EVENT_ID(RfxStringData, RFX_MSG_EVENT_SIM_SIM_AUTHENTICATION);
RFX_REGISTER_DATA_TO_EVENT_ID(RfxStringsData, RFX_MSG_EVENT_SIM_USIM_AUTHENTICATION);

/*****************************************************************************
 * Class RfxController
 *****************************************************************************/
RmcCommSimRequestHandler::RmcCommSimRequestHandler(int slot_id, int channel_id) :
        RmcSimBaseHandler(slot_id, channel_id) {
    setTag(String8("RmcCommSimReq"));
}

RmcCommSimRequestHandler::~RmcCommSimRequestHandler() {
    pthread_mutex_unlock(&simStatusMutex);
    pthread_mutex_destroy(&simStatusMutex);
}

RmcSimBaseHandler::SIM_HANDLE_RESULT RmcCommSimRequestHandler::needHandle(
        const sp<RfxMclMessage>& msg) {
    int request = msg->getId();
    switch(request) {
        case RFX_MSG_REQUEST_GET_SIM_STATUS:
        case RFX_MSG_REQUEST_SIM_IO:
        case RFX_MSG_REQUEST_ISIM_AUTHENTICATION:
        case RFX_MSG_REQUEST_SIM_AUTHENTICATION:
        case RFX_MSG_REQUEST_GENERAL_SIM_AUTH:
        case RFX_MSG_EVENT_SIM_QUERY_ICCID:
        case RFX_MSG_REQUEST_SIM_OPEN_CHANNEL:
        case RFX_MSG_REQUEST_SIM_CLOSE_CHANNEL:
        case RFX_MSG_REQUEST_SIM_TRANSMIT_APDU_BASIC:
        case RFX_MSG_REQUEST_SIM_TRANSMIT_APDU_CHANNEL:
        case RFX_MSG_REQUEST_SIM_GET_ATR:
        case RFX_MSG_REQUEST_ENTER_SIM_PIN:
        case RFX_MSG_REQUEST_ENTER_SIM_PUK:
        case RFX_MSG_REQUEST_ENTER_SIM_PIN2:
        case RFX_MSG_REQUEST_ENTER_SIM_PUK2:
        case RFX_MSG_REQUEST_CHANGE_SIM_PIN:
        case RFX_MSG_REQUEST_CHANGE_SIM_PIN2:
        case RFX_MSG_REQUEST_SIM_ENTER_NETWORK_DEPERSONALIZATION:
        case RFX_MSG_EVENT_SIM_DETECT_SIM:
        case RFX_MSG_EVENT_SIM_SIM_AUTHENTICATION:
        case RFX_MSG_EVENT_SIM_USIM_AUTHENTICATION:
        case RFX_MSG_REQUEST_SIM_SAP_CONNECT:
        case RFX_MSG_REQUEST_SIM_SAP_DISCONNECT:
        case RFX_MSG_REQUEST_SIM_SAP_APDU:
        case RFX_MSG_REQUEST_SIM_SAP_TRANSFER_ATR:
        case RFX_MSG_REQUEST_SIM_SAP_POWER:
        case RFX_MSG_REQUEST_SIM_SAP_RESET_SIM:
        case RFX_MSG_REQUEST_SIM_SAP_SET_TRANSFER_PROTOCOL:
        case RFX_MSG_REQUEST_SET_SIM_CARD_POWER:
            return RmcSimBaseHandler::RESULT_NEED;
        case RFX_MSG_REQUEST_QUERY_FACILITY_LOCK:
        case RFX_MSG_REQUEST_SET_FACILITY_LOCK:
            {
                sp<RfxMclMessage> response;
                char** pStrings = (char**)(msg->getData()->getData());
                char *facility = pStrings[0];

                if (facility == NULL || strlen(facility) <= 0) {
                    logE(mTag, "needHandle => QUERY_FACILITY_LOCK, facility empty");
                        response = RfxMclMessage::obtainResponse(msg->getId(),
                                RIL_E_INVALID_ARGUMENTS, RfxIntsData(), msg, false);
                        responseToTelCore(response);
                    return RmcSimBaseHandler::RESULT_NEED;
                }
            }
            break;
        default:
            logD(mTag, "Not support the request %d!", request);
            break;
    }
    return RmcSimBaseHandler::RESULT_IGNORE;
}

void RmcCommSimRequestHandler::handleRequest(const sp<RfxMclMessage>& msg) {
    int request = msg->getId();
    switch(request) {
        case RFX_MSG_REQUEST_GET_SIM_STATUS:
            handleGetSimStatus(msg);
            break;
        case RFX_MSG_REQUEST_SIM_IO:
            handleSimIo(msg);
            break;
        case RFX_MSG_REQUEST_ISIM_AUTHENTICATION:
            handleIsimAuthentication(msg);
            break;
        case RFX_MSG_REQUEST_SIM_AUTHENTICATION:
            handleSimAuthentication(msg);
            break;
        case RFX_MSG_REQUEST_GENERAL_SIM_AUTH:
            handleGeneralSimAuthentication(msg);
            break;
        case RFX_MSG_REQUEST_SIM_OPEN_CHANNEL:
            handleSimOpenChannel(msg);
            break;
        case RFX_MSG_REQUEST_ENTER_SIM_PIN:
            handleSecurityOperation(msg,ENTER_PIN1);
            break;
        case RFX_MSG_REQUEST_ENTER_SIM_PUK:
            handleSecurityOperation(msg,ENTER_PUK1);
            break;
        case RFX_MSG_REQUEST_ENTER_SIM_PIN2:
            handleSecurityOperation(msg,ENTER_PIN2);
            break;
        case RFX_MSG_REQUEST_ENTER_SIM_PUK2:
            handleSecurityOperation(msg,ENTER_PUK2);
            break;
        case RFX_MSG_REQUEST_CHANGE_SIM_PIN:
            handleSecurityOperation(msg,CHANGE_PIN1);
            break;
        case RFX_MSG_REQUEST_CHANGE_SIM_PIN2:
            handleSecurityOperation(msg,CHANGE_PIN2);
            break;
        case RFX_MSG_REQUEST_SIM_ENTER_NETWORK_DEPERSONALIZATION:
            handleEnterNetworkDepersonalization(msg);
            break;
        case RFX_MSG_REQUEST_SIM_CLOSE_CHANNEL:
            handleSimCloseChannel(msg);
            break;
        case RFX_MSG_REQUEST_SIM_TRANSMIT_APDU_BASIC:
            handleSimTransmitBasic(msg);
            break;
        case RFX_MSG_REQUEST_SIM_TRANSMIT_APDU_CHANNEL:
            handleSimTransmitChannel(msg);
            break;
        case RFX_MSG_REQUEST_SIM_GET_ATR:
            handleSimGetAtr(msg);
            break;
        case RFX_MSG_REQUEST_LOCAL_SIM_SAP_RESET:
            handleLocalBtSapReset(msg);
            break;
        case RFX_MSG_REQUEST_SIM_SAP_CONNECT:
            handleBtSapConnect(msg);
            break;
        case RFX_MSG_REQUEST_SIM_SAP_DISCONNECT:
            handleBtSapDisconnect(msg);
            break;
        case RFX_MSG_REQUEST_SIM_SAP_APDU:
            handleBtSapTransferApdu(msg);
            break;
        case RFX_MSG_REQUEST_SIM_SAP_TRANSFER_ATR:
            handleBtSapTransferAtr(msg);
            break;
        case RFX_MSG_REQUEST_SIM_SAP_POWER:
            handleBtSapPower(msg);
            break;
        case RFX_MSG_REQUEST_SIM_SAP_RESET_SIM:
            handleBtSapResetSim(msg);
            break;
        case RFX_MSG_REQUEST_SIM_SAP_SET_TRANSFER_PROTOCOL:
            handleBtSapSetTransferProtocol(msg);
            break;
        case RFX_MSG_REQUEST_SET_SIM_CARD_POWER:
            handleSetSimCardPower(msg);
            break;
        default:
            logD(mTag, "Not support the request %d!", request);
            break;
    }
}

void RmcCommSimRequestHandler::handleEvent(const sp<RfxMclMessage>& msg) {
    int event = msg->getId();
    switch(event) {
        case RFX_MSG_EVENT_SIM_QUERY_ICCID:
            handleQueryIccid(msg);
            break;
        case RFX_MSG_EVENT_SIM_DETECT_SIM:
            handleDetectSim(msg);
            break;
        case RFX_MSG_EVENT_SIM_SIM_AUTHENTICATION:
            handleLocalRequestSimAuthentication(msg);
            break;
        case RFX_MSG_EVENT_SIM_USIM_AUTHENTICATION:
            handleLocalRequestUsimAuthentication(msg);
            break;
        default:
            logD(mTag, "Not support the event %d!", event);
            break;
    }
}

const int* RmcCommSimRequestHandler::queryTable(int channel_id, int *record_num) {

    if (channel_id == RIL_CMD_PROXY_1) {
        *record_num = sizeof(ch1ReqList)/sizeof(int);
        return ch1ReqList;
    } else if (channel_id == RIL_CMD_PROXY_3) {
        *record_num = sizeof(ch3ReqList)/sizeof(int);
        return ch3ReqList;
    } else {
        // Impossible case!
        logE(mTag, "channel %d miss query table method!", channel_id);
    }

    return NULL;
}

const int* RmcCommSimRequestHandler::queryEventTable(int channel_id, int *record_num) {
    if (channel_id == RIL_CMD_PROXY_1) {
        *record_num = sizeof(ch1EventList)/sizeof(int);
        return ch1EventList;
    }

    return NULL;
}

void RmcCommSimRequestHandler::sleepMsec(long long msec) {
    struct timespec ts;
    int err;

    ts.tv_sec = (msec / 1000);
    ts.tv_nsec = (msec % 1000) * 1000 * 1000;

    do {
        err = nanosleep(&ts, &ts);
    } while (err < 0 && errno == EINTR);
}

bool RmcCommSimRequestHandler::getSimAppInfo(int app_id, RIL_AppStatus *p_app_info,
        UICC_Status uStatus) {
    bool result = false;
    int err, intPara = -1;
    char *strPara = NULL;
    RfxAtLine *line = NULL;
    sp<RfxAtResponse> p_response = NULL;
    RmcSimPinPukCount *retry = NULL;

    if (p_app_info != NULL) {
        memset(p_app_info, 0, sizeof(RIL_AppStatus));
    } else {
        return result;
    }

    // Query SIM app info
    String8 cmd = String8::format("AT+ESIMAPP=%d,0", app_id);
    p_response = atSendCommandSingleline(cmd.string(), "+ESIMAPP:");

    if (p_response != NULL && p_response->getSuccess() > 0) {
        // Success
        line = p_response->getIntermediates();

        line->atTokStart(&err);

        // application id
        intPara = line->atTokNextint(&err);

        // channel id
        intPara = line->atTokNextint(&err);

        // AID
        strPara = line->atTokNextstr(&err);
        asprintf(&p_app_info->aid_ptr, "%s", strPara);

        // App label
        strPara = NULL;
        strPara = line->atTokNextstr(&err);
        asprintf(&p_app_info->app_label_ptr, "%s", strPara);

        // Pin1 enable
        intPara = -1;
        intPara = line->atTokNextint(&err);
        if (intPara == 0) {
            p_app_info->pin1 = RIL_PINSTATE_DISABLED;
        }

        // Pin2 enable
        intPara = -1;
        intPara = line->atTokNextint(&err);
        if (intPara == 0) {
            p_app_info->pin2 = RIL_PINSTATE_DISABLED;
        }

        retry = (RmcSimPinPukCount*)calloc(1, sizeof(RmcSimPinPukCount));
        // PIN1 retry count
        retry->pin1 = line->atTokNextint(&err);
        // PIN2 retry count
        retry->pin2 = line->atTokNextint(&err);
        // PUK1 retry count
        retry->puk1 = line->atTokNextint(&err);
        // PUK2 retry count
        retry->puk2 = line->atTokNextint(&err);
        // Update the system properties of PIN and PUK
        setPinPukRetryCountProp(retry);

        // Perso state and correct pin1/pin2 enable state
        switch (uStatus) {
            case UICC_READY:
                p_app_info->app_state = RIL_APPSTATE_READY;
                p_app_info->perso_substate = RIL_PERSOSUBSTATE_READY;
                if (p_app_info->pin1 != RIL_PINSTATE_DISABLED) {
                    p_app_info->pin1 = RIL_PINSTATE_ENABLED_VERIFIED;
                }
                if (p_app_info->pin2 != RIL_PINSTATE_DISABLED) {
                    p_app_info->pin2 = RIL_PINSTATE_ENABLED_NOT_VERIFIED;
                }
                break;
            case UICC_PIN:
                p_app_info->app_state = RIL_APPSTATE_PIN;
                p_app_info->perso_substate = RIL_PERSOSUBSTATE_READY;
                break;
            case UICC_PUK:
                p_app_info->app_state = RIL_APPSTATE_PUK;
                p_app_info->perso_substate = RIL_PERSOSUBSTATE_READY;
                break;
            case UICC_NETWORK_PERSONALIZATION:
                p_app_info->app_state = RIL_APPSTATE_SUBSCRIPTION_PERSO;
                p_app_info->perso_substate = RIL_PERSOSUBSTATE_SIM_NETWORK;
                break;
            case UICC_NP:
                p_app_info->app_state = RIL_APPSTATE_SUBSCRIPTION_PERSO;
                p_app_info->perso_substate = RIL_PERSOSUBSTATE_SIM_NETWORK;
                break;
            case UICC_NSP:
                p_app_info->app_state = RIL_APPSTATE_SUBSCRIPTION_PERSO;
                p_app_info->perso_substate = RIL_PERSOSUBSTATE_SIM_NETWORK_SUBSET;
                break;
            case UICC_SP:
                p_app_info->app_state = RIL_APPSTATE_SUBSCRIPTION_PERSO;
                p_app_info->perso_substate = RIL_PERSOSUBSTATE_SIM_SERVICE_PROVIDER;
                break;
            case UICC_CP:
                p_app_info->app_state = RIL_APPSTATE_SUBSCRIPTION_PERSO;
                p_app_info->perso_substate = RIL_PERSOSUBSTATE_SIM_CORPORATE;
                break;
            case UICC_SIMP:
                p_app_info->app_state = RIL_APPSTATE_SUBSCRIPTION_PERSO;
                p_app_info->perso_substate = RIL_PERSOSUBSTATE_SIM_SIM;
                break;
            case UICC_PERM_BLOCKED:
                p_app_info->app_state = RIL_APPSTATE_PIN;
                p_app_info->perso_substate = RIL_PERSOSUBSTATE_UNKNOWN;
                p_app_info->pin1 = RIL_PINSTATE_ENABLED_PERM_BLOCKED;
                p_app_info->pin2 = RIL_PINSTATE_UNKNOWN;
                break;
            default:
                p_app_info->perso_substate = RIL_PERSOSUBSTATE_READY;
                break;
        }

        // App type
        switch (app_id) {
            case UICC_APP_SIM:
                p_app_info->app_type = RIL_APPTYPE_SIM;
                break;
            case UICC_APP_USIM:
                p_app_info->app_type = RIL_APPTYPE_USIM;
                break;
            case UICC_APP_CSIM:
                p_app_info->app_type = RIL_APPTYPE_CSIM;
                break;
            case UICC_APP_RUIM:
                p_app_info->app_type = RIL_APPTYPE_RUIM;
                break;
            case UICC_APP_ISIM:
                p_app_info->app_type = RIL_APPTYPE_ISIM;
                break;
        }
        String8 logStr;
        logStr = String8::format("app_id %d, app_type %d, app_state %d, AID %s, label %s, ",
                app_id, p_app_info->app_type, p_app_info->app_state, p_app_info->aid_ptr,
                p_app_info->app_label_ptr);
        logStr += String8::format("pin1 %d, pin2 %d, (%d, %d, %d, %d)", p_app_info->pin1,
                p_app_info->pin2, retry->pin1, retry->pin2, retry->puk1, retry->puk2);
        logD(mTag, logStr.string());
        result = true;
        free(retry);
    } else {
        logE(mTag, "Fail to get app info!");
    }

    p_response = NULL;

    return result;
}

void RmcCommSimRequestHandler::handleGetSimStatus(const sp<RfxMclMessage>& msg) {

    RIL_Errno ril_error = RIL_E_SIM_ERR;
    RIL_CardStatus_v6 *p_card_status = NULL;
    int cardType = getMclStatusManager()->getIntValue(RFX_STATUS_KEY_CARD_TYPE);
    int cdma3GNT = getMclStatusManager()->getIntValue(RFX_STATUS_KEY_CDMA3G_SWITCH_CARD);
    // MTK-START: AOSP SIM PLUG IN/OUT
    int esim_cause = getMclStatusManager()->getIntValue(RFX_STATUS_KEY_SIM_ESIMS_CAUSE);
    // MTK-END
    sp<RfxMclMessage> response;
    char tempstr[RFX_PROPERTY_VALUE_MAX] = { 0 };
    String8 prop("ril.iccid.sim");
    int ret = 0;
    bool cdma3GDM = getMclStatusManager()->getBoolValue(RFX_STATUS_KEY_CDMA3G_DUALMODE_CARD);

    memset(tempstr, 0, sizeof(tempstr));
    prop.append(String8::format("%d", (m_slot_id+1)));

    //check timestampe between the request msg and the capability switch begin
    int64_t tSimSwitch = getNonSlotMclStatusManager()->getInt64Value(
            RFX_STATUS_KEY_SIM_SWITCH_RADIO_UNAVAIL_TIME);
    if (msg->getTimeStamp() <= tSimSwitch) {
        //check timestampe between the request msg and the capability switch begin
        ril_error = RIL_E_INVALID_STATE;
    } else if (cdma3GDM && (((cardType == RFX_CARD_TYPE_SIM) &&
            ((cdma3GNT == AP_TRIGGER_SWITCH_RUIM) || (cdma3GNT == GMSS_TRIGGER_SWITCH_RUIM))) ||
            ((cardType == RFX_CARD_TYPE_RUIM) &&
            ((cdma3GNT == AP_TRIGGER_SWITCH_SIM) || (cdma3GNT == GMSS_TRIGGER_SWITCH_SIM))))) {
        // check CDMA3G consistency between next card type and current card type
        ril_error = RIL_E_INVALID_STATE;
    } else if (cardType == -1) {
        // The UICC card is not ready.
        p_card_status = (RIL_CardStatus_v6*)calloc(1, sizeof(RIL_CardStatus_v6));
        p_card_status->card_state = RIL_CARDSTATE_ERROR;
        p_card_status->universal_pin_state = RIL_PINSTATE_UNKNOWN;
        p_card_status->gsm_umts_subscription_app_index = -1;
        p_card_status->cdma_subscription_app_index = -1;
        p_card_status->ims_subscription_app_index = -1;
        p_card_status->num_applications = 0;
        ril_error = RIL_E_SUCCESS;
    } else {
        bool result = false;
        int err, count = 0;
        RfxAtLine *line = NULL;
        char *cpinResult = NULL;
        sp<RfxAtResponse> p_response = NULL;

        // Get SIM status
        do {
            p_response = atSendCommandSingleline("AT+CPIN?", "+CPIN:");
            err = p_response->getError();

            if (err != 0) {
                //if (err == AT_ERROR_INVALID_THREAD) {
                //    ret = UICC_BUSY;
                //} else {
                    ret = UICC_NOT_READY;
                //}
            } else if (p_response->getSuccess() == 0) {
                switch (p_response->atGetCmeError()) {
                    case CME_SIM_BUSY:
                        ret = UICC_BUSY;
                        break;
                    case CME_SIM_NOT_INSERTED:
                        // MTK-START: AOSP SIM PLUG IN/OUT
                        if (ESIMS_CAUSE_SIM_NO_INIT == esim_cause) {
                            ret = UICC_NO_INIT;
                            break;
                        }
                        // MTK-END
                    case CME_SIM_FAILURE:
                        ret = UICC_ABSENT;
                        break;
                    case CME_SIM_WRONG: {
                        RmcSimPinPukCount *retry = getPinPukRetryCount();
                        if (retry != NULL && retry->pin1 == 0 && retry->puk1 == 0)
                        {
                            ret = UICC_PERM_BLOCKED; // PERM_DISABLED
                        } else {
                            ret = UICC_NOT_READY;
                        }
                        if (retry != NULL) {
                            free(retry);
                        }
                        break;
                    }
                    default:
                        ret = UICC_NOT_READY;
                        break;
                }
            } else {
                // Success
                int state = RFX_SIM_STATE_NOT_READY;

                line = p_response->getIntermediates();
                line->atTokStart(&err);
                if (err < 0) {
                    ret = UICC_NOT_READY;
                    break;
                }

                cpinResult = line->atTokNextstr(&err);
                if (err < 0) {
                    ret = UICC_NOT_READY;
                    break;
                }

                if (ret != UICC_NOT_READY) {
                    String8 cpinStr(cpinResult);
                    if (cpinStr == String8::format("SIM PIN")) {
                        ret = UICC_PIN;
                        state = RFX_SIM_STATE_LOCKED;
                    } else if (cpinStr == String8::format("SIM PUK")) {
                        ret = UICC_PUK;
                        state = RFX_SIM_STATE_LOCKED;
                    } else if (cpinStr == String8::format("PH-NET PIN") ||
                            cpinStr == String8::format("PH-NET PUK")) {
                        ret = UICC_NP;
                        state = RFX_SIM_STATE_LOCKED;
                    } else if (cpinStr == String8::format("PH-NETSUB PIN") ||
                            cpinStr == String8::format("PH-NETSUB PUK")) {
                        ret = UICC_NSP;
                        state = RFX_SIM_STATE_LOCKED;
                    } else if (cpinStr == String8::format("PH-SP PIN") ||
                            cpinStr == String8::format("PH-SP PUK")) {
                        ret = UICC_SP;
                        state = RFX_SIM_STATE_LOCKED;
                    } else if (cpinStr == String8::format("PH-CORP PIN") ||
                            cpinStr == String8::format("PH-CORP PUK")) {
                        ret = UICC_CP;
                        state = RFX_SIM_STATE_LOCKED;
                    } else if (cpinStr == String8::format("PH-FSIM PIN") ||
                            cpinStr == String8::format("PH-FSIM PUK")) {
                        ret = UICC_SIMP;
                        state = RFX_SIM_STATE_LOCKED;
                    } else if (cpinStr != String8::format("READY"))  {
                        /* we're treating unsupported lock types as "sim absent" */
                        ret = UICC_ABSENT;
                        state = RFX_SIM_STATE_ABSENT;
                    } else {
                        ret = UICC_READY;
                        state = RFX_SIM_STATE_READY;
                    }
                }

                getMclStatusManager()->setIntValue(RFX_STATUS_KEY_SIM_STATE, state);

                p_response = NULL;
            }

            if (ret == UICC_BUSY) {
                count++;     //to avoid block; if the busy time is too long; need to check modem.
                if(count == 30 || RfxRilUtils::isVsimEnabledBySlot(m_slot_id)) {
                    ret = UICC_NOT_READY;
                    getMclStatusManager()->setIntValue(
                            RFX_STATUS_KEY_ECPIN_STATE, RFX_WAIT_FOR_ECPIN);
                } else {
                    sleepMsec(200);
                }
            } else if (ret == UICC_READY) {
                // Query test SIM from modem
                p_response = atSendCommandSingleline("AT+ETESTSIM?", "+ETESTSIM:");
                err = p_response->getError();

                if (err < 0 || p_response->getSuccess() == 0) {
                    logE(mTag, "detect test SIM failed");
                } else {
                    int para;
                    String8 testSimProp("gsm.sim.ril.testsim");

                    testSimProp.append(
                            (m_slot_id == 0) ? "" : String8::format(".%d", (m_slot_id + 1)));

                    line = p_response->getIntermediates();

                    line->atTokStart(&err);
                    para = line->atTokNextint(&err);

                    String8 testSimStr(String8::format("%d", para));
                    if (mOldTestSimValue[m_slot_id] != para) {
                        mOldTestSimValue[m_slot_id] = para;
                        rfx_property_set(testSimProp, testSimStr.string());
                    }
                }

                p_response = NULL;
            }
        } while (ret == UICC_BUSY);

        p_card_status = (RIL_CardStatus_v6*)calloc(1, sizeof(RIL_CardStatus_v6));
        if (ret == UICC_NOT_READY) {
            // Card state error
            p_card_status->card_state = RIL_CARDSTATE_ERROR;
            p_card_status->universal_pin_state = RIL_PINSTATE_UNKNOWN;
            p_card_status->gsm_umts_subscription_app_index = -1;
            p_card_status->cdma_subscription_app_index = -1;
            p_card_status->ims_subscription_app_index = -1;
            p_card_status->num_applications = 0;
        } else if (ret == UICC_ABSENT) {
            // UICC absent
            p_card_status->card_state = RIL_CARDSTATE_ABSENT;
            p_card_status->universal_pin_state = RIL_PINSTATE_UNKNOWN;
            p_card_status->gsm_umts_subscription_app_index = -1;
            p_card_status->cdma_subscription_app_index = -1;
            p_card_status->ims_subscription_app_index = -1;
            p_card_status->num_applications = 0;
            getMclStatusManager()->setIntValue(RFX_STATUS_KEY_SIM_STATE, RFX_SIM_STATE_ABSENT);
            rfx_property_get("persist.radio.c_capability_slot", tempstr, "0");
            if (((cardType & RFX_CARD_TYPE_RUIM) || (cardType & RFX_CARD_TYPE_CSIM)) &&
                    !(cardType & RFX_CARD_TYPE_USIM) && !(cardType & RFX_CARD_TYPE_SIM) &&
                    (m_slot_id != atoi(tempstr) - 1)) {
                // For RUIM single mode card, iccid is not set in rild. If C capability is not in
                // card for C+C mode, there is no chance to update iccid in rild. Here is to update
                // iccid to N/A in such situation.
                logI(mTag, "CDMA slot: %d, %s : N/A", atoi(tempstr) - 1, prop.string());
                rfx_property_set(prop.string(), "N/A");
            }
        // MTK-START: AOSP SIM PLUG IN/OUT
        } else if (ret == UICC_NO_INIT) {
            p_card_status->card_state = RIL_CARDSTATE_PRESENT;
            p_card_status->universal_pin_state = RIL_PINSTATE_UNKNOWN;
            p_card_status->gsm_umts_subscription_app_index = -1;
            p_card_status->cdma_subscription_app_index = -1;
            p_card_status->ims_subscription_app_index = -1;
            p_card_status->num_applications = 0;
        // MTK-END
        } else {
            // Normal case
            p_card_status->card_state = RIL_CARDSTATE_PRESENT;
            p_card_status->universal_pin_state = RIL_PINSTATE_UNKNOWN;
            p_card_status->gsm_umts_subscription_app_index = -1;
            p_card_status->cdma_subscription_app_index = -1;
            p_card_status->ims_subscription_app_index = -1;
            p_card_status->num_applications = 0;

            if (cardType & RFX_CARD_TYPE_USIM) {
                // Query USIM app info
                result = getSimAppInfo(UICC_APP_USIM,
                        &p_card_status->applications[p_card_status->num_applications],
                        (UICC_Status)ret);
            } else if (cardType & RFX_CARD_TYPE_SIM) {
                // Query SIM app info
                result = getSimAppInfo(UICC_APP_SIM,
                        &p_card_status->applications[p_card_status->num_applications],
                        (UICC_Status)ret);
            }

            if (result) {
                p_card_status->gsm_umts_subscription_app_index = p_card_status->num_applications;
                p_card_status->num_applications++;
            }

            // Query ISIM app info
            result = false;
            if (cardType & RFX_CARD_TYPE_ISIM) {
                result = getSimAppInfo(UICC_APP_ISIM,
                        &p_card_status->applications[p_card_status->num_applications],
                        (UICC_Status)ret);
            }

            if (result) {
                p_card_status->ims_subscription_app_index = p_card_status->num_applications;
                p_card_status->num_applications++;
            }

            result = false;
            if (cardType & RFX_CARD_TYPE_CSIM) {
                // Query CSIM app info
                result = getSimAppInfo(UICC_APP_CSIM,
                        &p_card_status->applications[p_card_status->num_applications],
                        (UICC_Status)ret);
            } else if (cardType & RFX_CARD_TYPE_RUIM) {
                // Query RUIM app info
                result = getSimAppInfo(UICC_APP_RUIM,
                        &p_card_status->applications[p_card_status->num_applications],
                        (UICC_Status)ret);
            }

            if (result) {
                p_card_status->cdma_subscription_app_index = p_card_status->num_applications;
                p_card_status->num_applications++;
            }

            // Correct universal pin state
            p_card_status->universal_pin_state = p_card_status->applications[0].pin1;
        }
        ril_error = RIL_E_SUCCESS;
    }

    // External SIM [Start]
    // 1. Card type is -1 mean modem haven't complete SIM initialzation. That is, we should treat
    //    as not ready state. When receiving card type, we will re-trigger SIM_STATE_CHANGE let
    //    framework has opportunity to get correct sim state.
    // 2. VSIM need more time to initialize, we keep SIM status as NOT_READY event if get BUSY
    //    over than 2 seconds.
    if (cardType == -1 || (ret == UICC_NOT_READY && RfxRilUtils::isVsimEnabledBySlot(m_slot_id))) {
        ril_error = RIL_E_SIM_ERR;
        logE(mTag, "SIM status is not ready under vsim enabled.");
    }
    // External SIM [End]

    // Send TC layer the response
    if (ril_error == RIL_E_SUCCESS) {
        response = RfxMclMessage::obtainResponse(msg->getId(), ril_error,
                RfxSimStatusData((void*)p_card_status, sizeof(RIL_CardStatus_v6)), msg, false);
    } else {
        response = RfxMclMessage::obtainResponse(msg->getId(), ril_error,
                RfxSimStatusData(NULL, 0), msg, false);
    }
    responseToTelCore(response);

    // Release memory
    if (p_card_status != NULL) {
        for (int i = 0; i < p_card_status->num_applications; i++) {
            if (p_card_status->applications[i].app_label_ptr != NULL) {
                free(p_card_status->applications[i].app_label_ptr);
            }
            if (p_card_status->applications[i].aid_ptr != NULL) {
                free(p_card_status->applications[i].aid_ptr);
            }
        }
        free(p_card_status);
    }
}

bool RmcCommSimRequestHandler::serviceActivationStatus(int fileId, int appTypeId) {
    int index = -1, err = 0, support = 0;
    bool result = true;
    String8 cmd("");
    RfxAtLine *line = NULL;
    sp<RfxAtResponse> p_response = NULL;

    switch(fileId) {
        case 28486: // 0x6F46 EF_SPN
            // SPN
            if (appTypeId == UICC_APP_USIM) {
                index = 19;
            } else if (appTypeId == UICC_APP_SIM) {
                index = 17;
            }
            break;
        case 28617: // 0x6FC9 EF_MBI
            // MBI. Check Mailbox Dialling Numbers
            if (appTypeId == UICC_APP_USIM) {
                index = 47;
            } else if (appTypeId == UICC_APP_SIM) {
                index = 53;
            }
            break;
        case 28618: // 0x6FCA EF_MWIS
            // MWIS
            if (appTypeId == UICC_APP_USIM) {
                index = 48;
            } else if (appTypeId == UICC_APP_SIM) {
                index = 54;
            }
            break;
    }

    if (index >= 0) {
        // Use AT+ESSTQ=<app>,<service> to query service table
        // 0:  Service not supported
        // 1:  Service supported
        // 2:  Service allocated but not activated
        cmd.append(String8::format("AT+ESSTQ=%d,%d", appTypeId, index));
        p_response = atSendCommandSingleline(cmd, "+ESSTQ:");
        cmd.clear();

        err = p_response->getError();
        // The same as AOSP. 0 - Available & Disabled, 1-Available & Enabled, 2-Unavailable.
        if (err < 0 || p_response->getSuccess() == 0) {
            logE(mTag, "Fail to query service table");
        } else {
            line = p_response->getIntermediates();

            line->atTokStart(&err);
            support = line->atTokNextint(&err);
            if (err >= 0) {
                if (support != 1) {
                    result = false;
                }
            }
            logD(mTag, "serviceActivationStatus result: %d", result);
        }
        p_response = NULL;
    }

    return result;
}

void RmcCommSimRequestHandler::handleSimIo(const sp<RfxMclMessage>& msg) {
    int err = 0, intPara = 0;
    RIL_SIM_IO_v6 *pData = (RIL_SIM_IO_v6*)(msg->getData()->getData());
    String8 cmd("");
    String8 path("");
    String8 aid((pData->aidPtr != NULL)? pData->aidPtr : "");
    String8 data((pData->data != NULL)? pData->data : "");
    String8 simResponse("");
    int appTypeId = queryAppTypeId(aid), channelId = 0;
    int remain = pData->p3;
    RIL_Errno result = RIL_E_SIM_ERR;
    RfxAtLine *line = NULL;
    char *tmpStr = NULL;
    sp<RfxAtResponse> p_response = NULL;
    sp<RfxMclMessage> response;
    RIL_SIM_IO_Response sr;
    int64_t tSimSwitch = 0;

    memset(&sr, 0, sizeof(RIL_SIM_IO_Response));

    if (!serviceActivationStatus(pData->fileid, appTypeId)) {
        result = RIL_E_REQUEST_NOT_SUPPORTED;
        goto error;
    }

    //check timestampe between the request msg and the capability switch begin
    tSimSwitch = getNonSlotMclStatusManager()->getInt64Value(
            RFX_STATUS_KEY_SIM_SWITCH_RADIO_UNAVAIL_TIME);
    if (msg->getTimeStamp() <= tSimSwitch) {
        //check timestampe between the request msg and the capability switch begin
        result = RIL_E_INVALID_STATE;
        goto error;
    }

    if (pData->pin2 != NULL && strlen(pData->pin2) > 0) {
        logD(mTag, "handleSimIo pin2 %s", pData->pin2);
        cmd.append(String8::format("AT+EPIN2=\"%s\"", pData->pin2));
        p_response = atSendCommand(cmd.string());
        err = p_response->getError();

        if (p_response != NULL) {
            if (p_response->getSuccess() == 0) {
                switch (p_response->atGetCmeError()) {
                    case CME_SIM_PIN2_REQUIRED:
                        result = RIL_E_SIM_PIN2;
                        break;
                    case CME_SIM_PUK2_REQUIRED:
                        result = RIL_E_SIM_PUK2;
                        break;
                    default:
                        result = RIL_E_SIM_ERR;
                        break;
                }
            } else {
                // success case
                result = RIL_E_SUCCESS;
            }
        }

        p_response = NULL;
        cmd.clear();

        if (result != RIL_E_SUCCESS) {
            logE(mTag, "Fail to check PIN2");
            goto error;
        }
    }

    // SIM access
    // 1. Remove 3F00
    if (strstr(pData->path, "3F00") || strstr(pData->path, "3f00")) {
        if (strlen(pData->path) > 4) {
            path.append(String8::format("%s", pData->path + 4));
        }
    } else {
        path.append(String8::format("%s", pData->path));
    }

    // 2. Query channel id
    cmd.append(String8::format("AT+ESIMAPP=%d,0", appTypeId));
    p_response = atSendCommandSingleline(cmd.string(), "+ESIMAPP:");

    if (p_response != NULL && p_response->getSuccess() > 0) {
        // Success
        line = p_response->getIntermediates();
        line->atTokStart(&err);

        // application id
        intPara = line->atTokNextint(&err);

        // channel id
        channelId = line->atTokNextint(&err);
        if (channelId == 255) {
            logE(mTag, "channel id is 255!");
            result = RIL_E_SIM_BUSY;
            goto error;
        }

        p_response = NULL;
        cmd.clear();
        line = NULL;
    } else {
        logE(mTag, "Fail to get app type %d channel id!", appTypeId);
        result = RIL_E_SIM_ERR;
        goto error;
    }

    logD(mTag, "SIM_IO(%d, %d, %d, %d, %d, %d, %s)", channelId, pData->command, pData->fileid,
                pData->p1, pData->p2, pData->p3, pData->path);

    while (remain > 0) {
        // 3. Send AT+ECRLA
        result = RIL_E_SIM_ERR;
        cmd.clear();
        cmd.append(String8::format("AT+ECRLA=%d,%d,%d,%d,%d,%d,%d", appTypeId, channelId,
                pData->command, pData->fileid, pData->p1, pData->p2,
                ((pData->command != 192 /* GET_RESPONSE */)? pData->p3 : 0)));

        if (!data.isEmpty()) {
            // Command type WRITE
            // Append <data>
            const char *pHead = data.string();

            tmpStr = (char*)calloc(1, sizeof(char)*512);
            strncpy(tmpStr, pHead + 2*(pData->p3 - remain), 2*255);

            cmd.append(String8::format(",\"%s\"", tmpStr));
            remain = 2*remain - strlen(tmpStr);

            free(tmpStr);
            tmpStr = NULL;
        } else if (!path.isEmpty()) {
            // append comma only if there is path
            cmd.append(",");
        }

        if (!path.isEmpty()) {
            // Append <path>
            cmd.append(String8::format(",\"%s\"", path.string()));
        }

        if (cmd.length() > 510) {
            logE(mTag, "command(%s) length %d is too long to send!", cmd.string(), cmd.length());
            goto error;
        }

        p_response = atSendCommandSingleline(cmd.string(), "+ECRLA:");

        if (p_response != NULL && p_response->getSuccess() == 1) {
            // Success
            line = p_response->getIntermediates();

            line->atTokStart(&err);
            if (err < 0) {
                goto error;
            }

            sr.sw1 = line->atTokNextint(&err);
            if (err < 0) {
                goto error;
            }

            sr.sw2 = line->atTokNextint(&err);
            if (err < 0) {
                goto error;
            }

            if (line->atTokHasmore()) {
                char *pRes = NULL;
                pRes = line->atTokNextstr(&err);
                if (err < 0) {
                    goto error;
                }
                simResponse.append(String8::format("%s", pRes));

                if (pData->command == 192) {
                    // In the case of GET_RESPONSE, we use 0 instead of pData->p3 so we set reamin as 0
                    // directly.
                    remain = 0;
                } else if (pData->data == NULL) {
                    // Case of command type READ
                    remain = 2*remain - strlen(pRes);
                }

                if (remain == 0) {
                    // no more SIM acess
                    asprintf(&sr.simResponse, "%s", simResponse.string());
                }
                // Run FCP parser for USIM and CSIM when the command is GET_RESPONSE
                if ((appTypeId == UICC_APP_USIM || appTypeId == UICC_APP_CSIM ||
                        appTypeId == UICC_APP_ISIM) && pData->command == 192 /* GET_RESPONSE */) {
                    makeSimRspFromUsimFcp((unsigned char**)&sr.simResponse);
                }

            } else if (pData->data == NULL) {
                // The command type is READ, but can't read data
                remain = 0;
            }
            result = RIL_E_SUCCESS;
        } else {
            // Fail to do SIM_IO
            remain = 0;
        }
    }

    if (RfxRilUtils::isEngLoad()) {
        logI(mTag, "SIM_IO(result %d, sw1 %d, sw2 %d, response %s)", result, sr.sw1, sr.sw2,
                sr.simResponse);
    }

    response = RfxMclMessage::obtainResponse(msg->getId(), result,
            RfxSimIoRspData((void*)&sr, sizeof(sr)), msg, false);

    responseToTelCore(response);
    p_response = NULL;

    if (sr.simResponse != NULL) {
        free(sr.simResponse);
    }
    return;
error:
    response = RfxMclMessage::obtainResponse(msg->getId(), result,
            RfxSimIoRspData((void*)&sr, sizeof(sr)), msg, false);

    responseToTelCore(response);
    p_response = NULL;

    if (sr.simResponse != NULL) {
        free(sr.simResponse);
    }

}

void RmcCommSimRequestHandler::handleIsimAuthentication(const sp<RfxMclMessage>& msg) {
    char *pData = (char*)(msg->getData()->getData());
    sp<RfxAtResponse> p_response = NULL;
    int err = 0, intPara;
    int channelId;
    String8 cmd("");
    RfxAtLine *line = NULL;
    char *auth_rsp = NULL;
    RIL_SIM_IO_Response sr;
    RIL_Errno result = RIL_E_SIM_ERR;
    int currSimInsertedState = getMclStatusManager()->getIntValue(RFX_STATUS_KEY_SIM_STATE);

    do {
        if (RFX_SIM_STATE_ABSENT == currSimInsertedState) {
            logE(mTag, "handleIsimAuthentication, sim absent");
            result = RIL_E_SIM_ABSENT;
            break;
        }
        if (pData == NULL || strlen(pData) <= 0) {
            logE(mTag, "handleIsimAuthentication, empty parameter");
            result = RIL_E_INVALID_ARGUMENTS;
            break;
        }
        // 1. Query channel id
        // TODO: Set ISIM app type id
        cmd.append(String8::format("AT+ESIMAPP=1,0"));

        p_response = atSendCommandSingleline(cmd.string(), "+ESIMAPP:");

        if (p_response != NULL && p_response->getSuccess() == 1) {
            // Success
            line = p_response->getIntermediates();
            line->atTokStart(&err);

            // application id
            intPara = line->atTokNextint(&err);

            // channel id
            channelId = line->atTokNextint(&err);
            if (channelId == 255) {
                logE(mTag, "channel id is 255!");
                result = RIL_E_SIM_BUSY;
                break;
            }

            p_response = NULL;
            cmd.clear();
            line = NULL;
        } else {
            logE(mTag, "Fail to query ISIM channel");
            break;
        }

        // 2. ISIM authentication
        cmd.append(String8::format("AT+ESIMAUTH=%d,0,\"%s\"", channelId, pData));
        p_response = atSendCommandSingleline(cmd.string(), "+ESIMAUTH:");

        if (p_response != NULL && p_response->getSuccess() == 1) {
            // Success
            line = p_response->getIntermediates();
            line->atTokStart(&err);
            if(err < 0) {
                break;
            }

            sr.sw1 = line->atTokNextint(&err);
            if(err < 0) {
                break;
            }

            sr.sw2 = line->atTokNextint(&err);
            if(err < 0) {
                break;
            }

            if(line->atTokHasmore()){
                sr.simResponse = line->atTokNextstr(&err);
                if(err < 0) {
                    break;
                }

                auth_rsp = (char*)calloc(1, sizeof(char)*MAX_AUTH_RSP);
                strcpy(auth_rsp, sr.simResponse);
                logI(mTag, "requestIsimAuthentication %s, %02x, %02x", auth_rsp, sr.sw1, sr.sw2);
            } else {
                logI(mTag, "No response data");
            }
            result = RIL_E_SUCCESS;
        } else {
            logE(mTag, "Fail to do ISIM authentication!");
            break;
        }

        p_response = NULL;
    } while(0);

    sp<RfxMclMessage> response = NULL;

    if (RIL_E_SUCCESS == result) {
        response = RfxMclMessage::obtainResponse(msg->getId(), result,
                RfxStringData((void*)auth_rsp, strlen(auth_rsp)), msg, false);
    } else {
        response = RfxMclMessage::obtainResponse(msg->getId(), result,
                RfxStringData(), msg, false);
    }
    responseToTelCore(response);
    if (NULL != auth_rsp) {
        free(auth_rsp);
    }
    p_response = NULL;
}

void RmcCommSimRequestHandler::handleGeneralSimAuthentication(const sp<RfxMclMessage>& msg) {
    sp<RfxAtResponse> p_response = NULL;
    RIL_SIM_IO_Response sim_auth_response;
    RIL_SimAuthStructure *pSimAuthData = NULL;
    int err;
    String8 cmd("");
    RfxAtLine *line = NULL;
    RIL_Errno result = RIL_E_GENERIC_FAILURE;

    logD(mTag, "handleGeneralSimAuthentication");

    do {
        pSimAuthData = (RIL_SimAuthStructure*)(msg->getData()->getData());
        logI(mTag, "pSimAuthData %d, %d", pSimAuthData->mode,pSimAuthData->sessionId);
        if(pSimAuthData->mode == 0) {
            // mode 0 is represent AKA authentication mode
            if (pSimAuthData->sessionId == 0) {
                // Session id is equal to 0, for backward compability, we use old AT command
                if (pSimAuthData->param2 != NULL && strlen(pSimAuthData->param2) > 0) {
                    // There is param2 means that caller except to use USIM AUTH
                    cmd.append(String8::format("AT+EAUTH=\"%s\",\"%s\"",
                            pSimAuthData->param1, pSimAuthData->param2));
                } else {
                    // There is no param2 means that caller except to use SIM AUTH
                    cmd.append(String8::format("AT+EAUTH=\"%s\"", pSimAuthData->param1));
                }
            } else {
                // Session id is not equal to 0, means we can use new AT command
                cmd.append(String8::format("AT+ESIMAUTH=%d,%d,\"%s%s\"",
                        pSimAuthData->sessionId, pSimAuthData->mode,
                        pSimAuthData->param1, pSimAuthData->param2));
            }
        } else if (pSimAuthData->mode == 1) {
            // mode 1 is represent GBA authentication mode
            cmd.append(String8::format("AT+ESIMAUTH=%d,%d,\"%02x%s%s\"",
                    pSimAuthData->sessionId, pSimAuthData->mode, pSimAuthData->tag,
                    pSimAuthData->param1, pSimAuthData->param2));
        }

        if (pSimAuthData->mode == 0 && pSimAuthData->sessionId == 0) {
            p_response = atSendCommandSingleline(cmd.string(), "+EAUTH:");
        } else {
            p_response = atSendCommandSingleline(cmd.string(), "+ESIMAUTH:");
        }
        memset(&sim_auth_response, 0, sizeof(RIL_SIM_IO_Response));
        if (p_response != NULL) {
            err = p_response->getError();
            if (err < 0 || p_response->getSuccess() == 0) {
                logE(mTag, "requestGeneralSimAuth Fail", p_response->atGetCmeError());
                break;
            } else {
                line = p_response->getIntermediates();
                line->atTokStart(&err);
                if (err < 0) {
                    break;
                }
                sim_auth_response.sw1 = line->atTokNextint(&err);
                if (err < 0) {
                    break;
                }
                sim_auth_response.sw2 = line->atTokNextint(&err);
                if (err < 0) {
                    break;
                }

                if(line->atTokHasmore()){
                    sim_auth_response.simResponse = line->atTokNextstr(&err);
                    if(err < 0) {
                        break;
                    }
                    logI(mTag, "requestGeneralSimAuth response len = %d %02x, %02x",
                            strlen(sim_auth_response.simResponse),
                            sim_auth_response.sw1, sim_auth_response.sw2);
                } else {
                    logI(mTag, "No response data");
                }
            }
        } else {
            break;
        }
        result = RIL_E_SUCCESS;
    } while(0);
    p_response = NULL;
    cmd.clear();
    line = NULL;

    sp<RfxMclMessage> response = NULL;

    if (result == RIL_E_SUCCESS) {
        response = RfxMclMessage::obtainResponse(msg->getId(), result,
                RfxSimIoRspData((void*)&sim_auth_response, sizeof(RIL_SIM_IO_Response)), msg, false);
    } else {
        response = RfxMclMessage::obtainResponse(msg->getId(), result,
                RfxSimIoRspData(NULL, 0), msg, false);
    }
    responseToTelCore(response);
}
void RmcCommSimRequestHandler::handleSimAuthentication(const sp<RfxMclMessage>& msg) {
    sp<RfxAtResponse> p_response = NULL;
    RIL_SIM_IO_Response *sim_auth_response = NULL;
    RIL_SimAuthentication *pSimAuthData = NULL;
    int err;
    bool closeChannelLater = false;
    int appTypeId = -1, channelId = -1, len = 0, intPara = 0;
    String8 cmd("");
    RfxAtLine *line = NULL;
    unsigned char *out_put = NULL;
    char *payload = NULL, *pByteArray = NULL;
    char tmp[256] = {0};
    RIL_Errno result = RIL_E_SIM_ERR;

    do {
        pSimAuthData = (RIL_SimAuthentication*)(msg->getData()->getData());

        // 1. Query app type id
        appTypeId = queryAppTypeId(String8::format("%s", pSimAuthData->aid));

        // 2. Get channel id
        if (appTypeId == -1) {
            if (pSimAuthData->aid != NULL) {
                // Open channel for specific AID
                cmd.append(String8::format("AT+CCHO=\"%s\"", pSimAuthData->aid));
                p_response = atSendCommandNumeric(cmd.string());
                cmd.clear();
                if (p_response->getSuccess() > 0) {
                    channelId = atoi(p_response->getIntermediates()->getLine());
                    if (channelId <= 0 || channelId > 19) {
                        logE(mTag, "Invalid channel id");
                        break;
                    }
                    closeChannelLater = true;
                } else {
                    logE(mTag, "Fail to open channel for AID %s", pSimAuthData->aid);
                    if (p_response != NULL) {
                        switch (p_response->atGetCmeError()) {
                            case CME_MEMORY_FULL:
                                result = RIL_E_MISSING_RESOURCE;
                                break;
                            case CME_NOT_FOUND:
                                result = RIL_E_NO_SUCH_ELEMENT;
                                break;
                            default:
                                break;
                        }
                    }
                    break;
                }

                p_response = NULL;
            } else {
                logE(mTag, "Parameter error!");
                result = RIL_E_INVALID_ARGUMENTS;
                break;
            }
        } else {
            // Query channel id according to app type id
            cmd.append(String8::format("AT+ESIMAPP=%d,0", appTypeId));

            p_response = atSendCommandSingleline(cmd.string(), "+ESIMAPP:");
            cmd.clear();

            if (p_response != NULL && p_response->getSuccess() > 0) {
                // Success
                line = p_response->getIntermediates();
                line->atTokStart(&err);

                // application id
                intPara = line->atTokNextint(&err);

                // channel id
                channelId = line->atTokNextint(&err);
                if (channelId == 255) {
                    logE(mTag, "channel id is 255!");
                    result = RIL_E_SIM_BUSY;
                    break;
                }

                line = NULL;
            } else {
                logE(mTag, "Fail to query app %d channel", appTypeId);
                break;
            }

            p_response = NULL;
        }
        if (NULL == pSimAuthData->authData) {
            logE(mTag, "authData is null!");
            result = RIL_E_INVALID_ARGUMENTS;
            break;
        }
        // 3. Decode data
        out_put = base64_decode_to_str((unsigned char *)pSimAuthData->authData,
                strlen(pSimAuthData->authData));
        logI(mTag, "[requestSimAuthentication] decode output %s", out_put);

        // 4. Send authentication APDU
        // AT+CGLA="channel id","len","CLA+INS+P1+P2+Lc+Data+Le"
        // CLA: if channel id 0~3, then CLA=0X; otherwise, if channel id 4~19, then CLA=4X
        // INS: 88 for authentication command
        // P1:00
        // P2 is decided by user's parameter, will be 8X according authentication context type
        // Lc:length of authentication context
        // Data: authentication context decided by user's parameter
        // Le: max length of data expected in response. use 00 represent unknown.
        if (appTypeId == UICC_APP_USIM) {
            // TODO: The case of USIM
            cmd.append(String8::format("AT+CGLA=%d,%d,\"%02x%02x%02x%02x%02x%s00\"",
                    channelId, (unsigned int)(12 + strlen((char*)out_put)),
                    ((channelId <= 3) ? channelId : (4 * 16 + channelId - 4)), 0x88, 0,
                    pSimAuthData->authContext, (unsigned int)(strlen((char*)out_put)/2), out_put));
        } else {
            cmd.append(String8::format("AT+CGLA=%d,%d,\"%02x%02x%02x%02x%s0C\"",
                    channelId, (unsigned int)(10 + strlen((char*)out_put)), 0xA0, 0x88, 0x00,
                    0x00, out_put));
        }

        p_response = atSendCommandSingleline(cmd.string(), "+CGLA:");
        cmd.clear();
        out_put = NULL;
        if (p_response != NULL && p_response->getSuccess() > 0) {
            sim_auth_response = (RIL_SIM_IO_Response*)calloc(1, sizeof(RIL_SIM_IO_Response));

            line = p_response->getIntermediates();
            line->atTokStart(&err);
            if (err < 0) {
                break;
            }

            len = line->atTokNextint(&err);
            if (err < 0) {
                break;
            }
            payload = line->atTokNextstr(&err);
            if (err < 0) {
                break;
            }
            sscanf(&(payload[len - 4]), "%02x%02x", &(sim_auth_response->sw1),
                    &(sim_auth_response->sw2));

            if (len > 255) {
                len = 255;
            }

            if (len > 4) {
                len = (((len - 4) > 255)? 255 : (len - 4));
                memset(tmp, 0, sizeof(char)*256);
                strncpy(tmp, payload, len);

                logI(mTag, "SIM authentication rsp %s(%d)", tmp, sizeof(tmp));
                out_put = base64_encode_to_str((unsigned char*)tmp, strlen(tmp));
            }

            sim_auth_response->simResponse = (char*)out_put;

            logI(mTag, "SIM authentication payload = %s %02x, %02x",
                    sim_auth_response->simResponse, sim_auth_response->sw1, sim_auth_response->sw2);

            result = RIL_E_SUCCESS;
        } else {
            logE(mTag, "Fail to do SIM authentication");
            break;
        }
    } while(0);

    p_response = NULL;

    sp<RfxMclMessage> response = RfxMclMessage::obtainResponse(msg->getId(), result,
            RfxSimIoRspData((void*)sim_auth_response,
            ((sim_auth_response != NULL)? sizeof(RIL_SIM_IO_Response) : 0)), msg, false);

    responseToTelCore(response);

    if (closeChannelLater) {
        // Step3. Close channel to avoid open too much unuseless channels
        cmd.append(String8::format("AT+CCHC=%d", channelId));
        p_response = atSendCommand(cmd);
        cmd.clear();

        err = p_response->getError();
        if (err < 0 || p_response->getSuccess() == 0) {
            logE(mTag, "[requestSimAuthentication] close channel fail");
        }

        p_response = NULL;
    }

    if (out_put != NULL) {
        free(out_put);
    }

    if (sim_auth_response != NULL) {
        free(sim_auth_response);
    }

}

void RmcCommSimRequestHandler::handleQueryIccid(const sp < RfxMclMessage > & msg) {
    sp<RfxAtResponse> p_response = NULL;
    int err;
    RfxAtLine *line;
    char *iccId = NULL;
    RIL_Errno ret = RIL_E_SIM_ERR;
    String8 prop("ril.iccid.sim");

    RFX_UNUSED(msg);
    prop.append(String8::format("%d", (m_slot_id+1)));


    do {
        p_response = atSendCommandSingleline("AT+ICCID?", "+ICCID:");
        err = p_response->getError();

        if (err < 0) {
            logE(mTag, "Fail to query iccid");
            break;
        }

        if (p_response->getSuccess() > 0) {
            ret = RIL_E_SUCCESS;
        } else {
            if (p_response->atGetCmeError() == CME_SIM_BUSY) {
                ret = RIL_E_SIM_BUSY;
                sleepMsec(200);
            } else if (p_response->atGetCmeError() == CME_SIM_NOT_INSERTED) {
                ret = RIL_E_SIM_ABSENT;
            }
        }

        if (ret != RIL_E_SUCCESS && ret != RIL_E_SIM_BUSY && ret != RIL_E_SIM_ABSENT) {
            logE(mTag, "Query iccid, but got error!");
            break;
        }

        if (ret == RIL_E_SUCCESS) {
            line = p_response->getIntermediates();
            line->atTokStart(&err);
            if (err < 0) break;

            iccId = line->atTokNextstr(&err);
            if (err < 0) break;

            // TODO: To print iccid using special log
            rfx_property_set(prop.string(), iccId);
        } else if (ret == RIL_E_SIM_ABSENT) {
            logD(mTag, "Query iccid, but SIM card is absent");
            rfx_property_set(prop.string(), "N/A");
        }
    } while(ret == RIL_E_SIM_BUSY);

    sp<RfxMclMessage> unsol = RfxMclMessage::obtainUrc(RFX_MSG_URC_RESPONSE_SIM_STATUS_CHANGED,
            m_slot_id, RfxVoidData());
    responseToTelCore(unsol);

    //RLOGD("queryIccId before release_wake_lock");
    release_wake_lock("sim_hot_plug");

    // Generate CDMA card type.
    handleCdmaCardType(iccId);
}

int RmcCommSimRequestHandler::openChannel(const sp < RfxMclMessage > & msg,
        RIL_SIM_IO_Response *sr, int *len) {
    sp<RfxAtResponse> p_response;
    String8 cmd("");
    int err, ril_error = RIL_E_SIM_ERR;
    RfxAtLine *line = NULL;
    char *rData = NULL;
    int cmeError = CME_SUCCESS;

    RFX_UNUSED(msg);
    // AT+CGLA=<sessionid>,<length>,<command>
    // <sessionid> = 0
    // <command> = 00 70 00 00 01
    cmd.append(String8::format("AT+CGLA=%d,%d,\"%02x%02x%02x%02x%02x\"",
            0, 10, 0x00, 0x70, 0x00, 0x00, 0x01));
    p_response = atSendCommandSingleline(cmd, "+CGLA:");
    err = p_response->getError();
    if (err < 0 || p_response->getSuccess() == 0) {
        logE(mTag, "Fail to open channel");
        if (p_response->getFinalResponse() != NULL) {
            if (!strcmp(p_response->getFinalResponse()->getLine(), "+CME ERROR: MEMORY FULL")) {
                ril_error = RIL_E_MISSING_RESOURCE;
            } else if (!strcmp(p_response->getFinalResponse()->getLine(), "+CME ERROR: NOT FOUND")) {
                ril_error = RIL_E_NO_SUCH_ELEMENT;
            } else {
                cmeError = p_response->atGetCmeError();
                logE(mTag, "openChannel: cme_ERROR: %d", cmeError);
                switch(cmeError) {
                    // GSMA NFC: IO Error if SIM has no response.
                    case CME_UNKNOWN:
                        ril_error = RIL_E_OEM_ERROR_1;
                        break;
                    default:
                        break;
                }
            }
        }
        goto error;
    }

    line = p_response->getIntermediates();
    line->atTokStart(&err);
    if (err < 0) goto error;
    (*len) = line->atTokNextint(&err);
    if (err < 0) goto error;
    rData = line->atTokNextstr(&err);
    if (err < 0) goto error;

    sscanf(&(rData[(*len) - 4]), "%02x%02x", &(sr->sw1), &(sr->sw2));
    rData[(*len) - 4] = '\0';
    // Remember to free it!
    asprintf(&sr->simResponse, "%s", rData);

    p_response = NULL;

    if (sr->sw1 != 0x90 && sr->sw1 != 0x91) { // wrong sw1, sw2
        if ((sr->sw1 == 0x6A && sr->sw2 == 0x82) ||
                (sr->sw1 == 0x69 && sr->sw2 == 0x85) ||
                (sr->sw1 == 0x69 && sr->sw2 == 0x99)) {
            ril_error = RIL_E_NO_SUCH_ELEMENT;
        } else if ((sr->sw1 == 0x6A && sr->sw2 == 0x84) ||
                   (sr->sw1 == 0x6A && sr->sw2 == 0x81) ||
                   (sr->sw1 == 0x68 && sr->sw2 == 0x81)) {
            logE(mTag, "Not enough memory space in the file");
            ril_error = RIL_E_MISSING_RESOURCE;
        }
    } else {
        ril_error = RIL_E_SUCCESS;
    }

    return ril_error;

error:
    sr->sw1 = 0x6f;
    sr->sw2 = 0x00;
    sr->simResponse = NULL;

    return ril_error;
}

int RmcCommSimRequestHandler::checkRetryFCI(int sw1, int sw2) {
    int retry = 0;
    if (sw1 != 0x61 && sw1 != 0x91 && sw1 != 0x9F) {
        retry = 1;
    }

    if ((sw1 == 0x63 && sw2 == 0x10) || (sw1 == 0x92 && sw2 == 0x40)
            || (sw1 == 0x90 && sw2 == 0x00)) {
        retry = 0;
    }
    logD(mTag, "checkRetryFCI, sw1 %d sw2 %d retry %d", sw1, sw2, retry);
    return retry;
}

int RmcCommSimRequestHandler::selectAid(RIL_SIM_IO_Response *sr, int *len, int channel,
        int p2, char* pAid) {
    sp<RfxAtResponse> p_response;
    String8 cmd("");
    int err, ril_error = RIL_E_SIM_ERR;
    RfxAtLine *line = NULL;
    char *rData = NULL;
    // Parameters
    char *aid = pAid;
    int length = (10 + strlen(aid));
    int cla = ((channel > 3)? 0x40|(channel-4) : channel);
    int ins = 0xA4;
    int p1 = 0x04;
    int mP2 = p2;
    int p3 = (strlen(aid)/2);
    bool retry = false;
    int cmeError = CME_SUCCESS;

redo:
    // Reset pointer
    *len = 0;
    rData = NULL;

    // Reset the response in the sr
    if (sr->simResponse != NULL) {
        free(sr->simResponse);
        sr->simResponse = NULL;
    }

    if (aid != NULL) {
        if(ins == 0xA4 && mP2==0x04) {
            cmd.append(String8::format("AT+CGLA=%d,%d,\"%02x%02x%02x%02x%02x%s00\"",
                    channel, length+2, cla, ins, p1, mP2, p3, aid));
        } else {
            cmd.append(String8::format("AT+CGLA=%d,%d,\"%02x%02x%02x%02x%02x%s\"",
                    channel, length, cla, ins, p1, mP2, p3, aid));
        }
    } else {
        if(ins == 0xA4 && mP2==0x04) {
            cmd.append(String8::format("AT+CGLA=%d,%d,\"%02x%02x%02x%02x%02x00\"",
                    channel, length+2, cla, ins, p1, mP2, p3));
        } else {
            cmd.append(String8::format("AT+CGLA=%d,%d,\"%02x%02x%02x%02x%02x\"",
                    channel, length, cla, ins, p1, mP2, p3));
        }
    }
    logD(mTag, "selectAid, cmd: %s", cmd.string());

    p_response = atSendCommandSingleline(cmd, "+CGLA:");
    err = p_response->getError();

    if (err < 0 || p_response->getSuccess() == 0) {
        logE(mTag, "Fail to open channel");
        if (p_response->getFinalResponse() != NULL) {
            if (!strcmp(p_response->getFinalResponse()->getLine(), "+CME ERROR: MEMORY FULL")) {
                ril_error = RIL_E_MISSING_RESOURCE;
            } else if (!strcmp(p_response->getFinalResponse()->getLine(), "+CME ERROR: NOT FOUND")) {
                ril_error = RIL_E_NO_SUCH_ELEMENT;
            } else {
                cmeError = p_response->atGetCmeError();
                logE(mTag, "openChannel: cme_ERROR: %d", cmeError);
                switch(cmeError) {
                    // GSMA NFC: IO Error if SIM has no response.
                    case CME_UNKNOWN:
                        ril_error = RIL_E_OEM_ERROR_1;
                        break;
                    default:
                        break;
                }
            }
        }
        goto error;
    }

    line = p_response->getIntermediates();
    line->atTokStart(&err);
    if (err < 0) goto error;
    (*len) = line->atTokNextint(&err);
    if (err < 0) goto error;
    rData = line->atTokNextstr(&err);
    if (err < 0) goto error;

    sscanf(&(rData[(*len) - 4]), "%02x%02x", &(sr->sw1), &(sr->sw2));
    rData[(*len) - 4] = '\0';
    // Remember to free it!
    asprintf(&sr->simResponse, "%s", rData);

    p_response = NULL;
    cmd.clear();

    if(sr->sw1 == 0x61) { // GET RESPONSE setting p3 = sr.sw2
        // GET RESPONSE for selecting AID if T=0
        // AT+CGLA=<sessionid>,<length>,<command>
        // <length> = 10
        // <command> = 0X C0 00 00 sr.sw2
        // X = channel
        length = 10;
        ins = 0xC0;
        p1 = 0x00;
        mP2 = 0x00;
        p3 = sr->sw2;
        aid = NULL;
        goto redo;
    } else if(sr->sw1 == 0x6C) { // resend previous command header setting p3 = sr.sw2
        p3 = sr->sw2;
        goto redo;
    } else if (sr->sw1 != 0x90 && sr->sw1 != 0x91) { // wrong sw1, sw2
        if ((sr->sw1 == 0x6A && sr->sw2 == 0x82) ||
                (sr->sw1 == 0x69 && sr->sw2 == 0x85) ||
                (sr->sw1 == 0x69 && sr->sw2 == 0x99)) {
            ril_error = RIL_E_NO_SUCH_ELEMENT;
        } else if ((sr->sw1 == 0x6A && sr->sw2 == 0x84) ||
                   (sr->sw1 == 0x6A && sr->sw2 == 0x81) ||
                   (sr->sw1 == 0x68 && sr->sw2 == 0x81)) {
            logE(mTag, "Not enough memory space in the file");
            ril_error = RIL_E_MISSING_RESOURCE;
        }
        if (ins == 0xA4 && aid != NULL && (mP2 == 0x04 || mP2 == 0x00) && !retry) {
            // Retry only for "Select AID"
            retry = true;
            if (checkRetryFCI(sr->sw1, sr->sw2) == 1) {
                mP2 = ((mP2 == 0x00)? 0x04 : 0x00);
                goto redo;
            }
        }
        // GSMA NFC test: We should treat warning code 0x62 or 0x63 as
        // success.
        if (sr->sw1 == 0x62 || sr->sw1 == 0x63) {
            logI(mTag, "0x62 || 0x63 warning code.");
            ril_error = RIL_E_SUCCESS;
        } else {
            return ril_error;
        }
    } else {
        ril_error = RIL_E_SUCCESS;
    }

    return ril_error;
error:
    sr->sw1 = 0x6f;
    sr->sw2 = 0x00;
    sr->simResponse = NULL;

    return ril_error;
}

// Hexa To Int
unsigned int RmcCommSimRequestHandler::htoi(char* szHex) {
    unsigned int hex = 0;
    int nibble;
    while (*szHex) {
        hex <<= 4;

        if (*szHex >= '0' && *szHex <= '9') {
            nibble = *szHex - '0';
        } else if (*szHex >= 'a' && *szHex <= 'f') {
            nibble = *szHex - 'a' + 10;
        } else if (*szHex >= 'A' && *szHex <= 'F') {
            nibble = *szHex - 'A' + 10;
        } else {
            nibble = 0;
        }

        hex |= nibble;

        szHex++;
    }

    return hex;
}

void RmcCommSimRequestHandler::handleSimOpenChannel(const sp < RfxMclMessage > & msg) {
    sp<RfxAtResponse> p_response;
    sp<RfxMclMessage> response;
    RIL_SIM_IO_Response sr;
    int ril_error = RIL_E_GENERIC_FAILURE, err = 0;
    int channel = 0;
    int len, i = 0;
    int *finalRsp = NULL;
    char *pAid = NULL;
    String8 cmd("");
    int cmeError = CME_SUCCESS;
    RIL_OpenChannelParams *openChannelParams = NULL;
    int p2 = 0x00;

    openChannelParams = (RIL_OpenChannelParams *)(msg->getData()->getData());
    pAid = openChannelParams->aidPtr;
    p2 = openChannelParams->p2;
    if (NULL != openChannelParams->aidPtr) {
        logD(mTag, "handleSimOpenChannel - pAid = %s, p2 = %d", pAid, p2);
    } else {
        logD(mTag, "handleSimOpenChannel - pAid null, p2 = %d", p2);
    }

    // Open channel
    ril_error = openChannel(msg, &sr, &len);

    // if sw1 == sw2 == 0xff, it means that open logical channel fail
    if(ril_error == RIL_E_SUCCESS) {
        // Support Extended Channel - 4 to 19 (0x40 ~ 0x4F)
        if (sr.simResponse != NULL) {
            channel = htoi(sr.simResponse);
        }
        logD(mTag, "handleSimOpenChannel - channel = %x", channel);
        if(channel < 1 || channel > 19) {
            sr.sw1 = 0xff;
            sr.sw2 = 0xff;
            ril_error = RIL_E_SIM_ERR;
            goto error;
        }
    } else if (ril_error == RIL_E_NO_SUCH_ELEMENT || ril_error == RIL_E_MISSING_RESOURCE
            || ril_error == RIL_E_OEM_ERROR_1) {
        goto error;
    } else {
        sr.sw1 = 0xff;
        sr.sw2 = 0xff;
        goto error;
    }

    // Select AID
    if (pAid != NULL) {
        if (sr.simResponse != NULL) {
            free(sr.simResponse);
        }
        memset(&sr, 0, sizeof(RIL_SIM_IO_Response));
        ril_error = selectAid(&sr, &len, channel, p2, pAid);
    } else {
        logI(mTag, "handleSimOpenChannel, empty AID");
        ril_error = RIL_E_SUCCESS;
    }

    if (ril_error != RIL_E_SUCCESS) {
        logE(mTag, "handleSimOpenChannel: fail to select AID, and then close logical channel %d",
                channel);
        goto error_select_aid;
    }

    // Open channel successfully
    if (sr.simResponse != NULL && strlen(sr.simResponse) > 0) {
        finalRsp = (int*)calloc(1, sizeof(int) * ((len / 2) + 3));
        if (NULL != finalRsp) {
            for(i = 0; i < (len / 2 - 2); i++) {
                sscanf(&(sr.simResponse[i * 2]), "%02x", &finalRsp[i + 1]);
            }
        } else {
            logE(mTag, "handleSimOpenChannel, calloc fail.");
            finalRsp = (int*)calloc(1, sizeof(int) * 3);
        }
    } else {
        logI(mTag, "handleSimOpenChannel, simResponse is null.");
        finalRsp = (int*)calloc(1, sizeof(int) * 3);
    }
    finalRsp[0] = channel;
    finalRsp[i + 1] = sr.sw1;
    finalRsp[i + 2] = sr.sw2;
    logI(mTag, "handleSimOpenChannel, success to open channel %d sw1 %x, sw2 %x, sr.simresponse %s",
        finalRsp[0], finalRsp[i + 1], finalRsp[i + 2], finalRsp);

    response = RfxMclMessage::obtainResponse(msg->getId(), (RIL_Errno)ril_error,
            RfxIntsData((void*)finalRsp, (i + 3)*sizeof(int)), msg, false);

    responseToTelCore(response);

    if (sr.simResponse != NULL) {
        free(sr.simResponse);
        sr.simResponse = NULL;
    }
    if (finalRsp != NULL) {
        free(finalRsp);
    }
    return;

error_select_aid:
    // Close logical Channel if selecting AID is failed
    // AT+CGLA=<sessionid>,<length>,<command>
    // <command> = 00 70 80 0X
    // X = channel
    logI(mTag, "handleSimOpenChannel: close logical channel %d due to fail to select AID",
            channel);
    cmd.append(String8::format("AT+CGLA=0,%d,\"%02x%02x%02x%02x\"",
                    8, 0x00, 0x70, 0x80, channel));
    p_response = atSendCommandSingleline(cmd, "+CGLA:");
    err = p_response->getError();
    if (err < 0 || p_response->getSuccess() == 0) {
        logE(mTag, "handleSimOpenChannel: Close logical Channel failed");
        err = RIL_E_GENERIC_FAILURE;
        cmeError = p_response->atGetCmeError();
        logE(mTag, "err select aid: cme_ERROR: %d", cmeError);
        switch(cmeError) {
            // GSMA NFC: IO Error if SIM has no response.
            case CME_UNKNOWN:
                ril_error = RIL_E_OEM_ERROR_1;
                break;
            default:
                break;
        }
    } else if (p_response->getSuccess() > 0) {
        channel = 0;
    }
    goto error;

error:
    logE(mTag, "handleSimOpenChannel, error %d, sw1 %x, sw2 %x, sr.simresponse %s",
        ril_error, sr.sw1, sr.sw2, sr.simResponse);
    if (sr.simResponse != NULL && strlen(sr.simResponse) > 0) {
        finalRsp = (int*)calloc(1, sizeof(int) * ((len / 2) + 3));
        if (NULL != finalRsp) {
            for(i = 0; i < (len / 2 - 2); i++) {
                sscanf(&(sr.simResponse[i * 2]), "%02x", &finalRsp[i + 1]);
            }
        } else {
            logE(mTag, "handleSimOpenChannel, calloc fail.");
            finalRsp = (int*)calloc(1, sizeof(int) * 3);
        }
    } else {
        finalRsp = (int*)calloc(1, sizeof(int) * 3);
    }
    finalRsp[0] = channel;
    finalRsp[i + 1] = sr.sw1;
    finalRsp[i + 2] = sr.sw2;

    response = RfxMclMessage::obtainResponse(msg->getId(), (RIL_Errno)ril_error,
            RfxIntsData((void*)finalRsp, (i + 3)*sizeof(int)), msg, false);

    responseToTelCore(response);
    if (sr.simResponse != NULL) {
        free(sr.simResponse);
        sr.simResponse = NULL;
    }
    if (finalRsp) {
        free(finalRsp);
    }
}
void RmcCommSimRequestHandler::handleSimCloseChannel(const sp < RfxMclMessage > & msg) {
    sp<RfxAtResponse> p_response;
    sp<RfxMclMessage> response;
    int err;
    String8 cmd("");
    RfxAtLine *line = NULL;
    int* channel = (int*)(msg->getData()->getData());
    int cmeError = CME_UNKNOWN;

    if (channel == NULL || *channel == 0) {
        logE(mTag, "close channel 0 is not allowed.");
        err = RIL_E_INVALID_ARGUMENTS;
        goto error;
    }
    cmd.append(String8::format("AT+CCHC=%d", (*channel)));
    p_response = atSendCommand(cmd);
    err = p_response->getError();
    if (err < 0 || p_response->getSuccess() == 0) {
        err = RIL_E_GENERIC_FAILURE;
        cmeError = p_response->atGetCmeError();
        logE(mTag, "close channel: cme_ERROR: %d", cmeError);
        switch(cmeError) {
            // GSMA NFC: IO Error if SIM has no response.
            case CME_UNKNOWN:
                err = RIL_E_OEM_ERROR_1;
                break;
            default:
                break;
        }
        goto error;
    }

    response = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_SUCCESS,
            RfxVoidData(), msg, false);

    responseToTelCore(response);
    return;
error:
    response = RfxMclMessage::obtainResponse(msg->getId(), (RIL_Errno)err,
            RfxVoidData(), msg, false);

    responseToTelCore(response);
}

void RmcCommSimRequestHandler::handleSimTransmitBasic(const sp < RfxMclMessage > & msg) {
    sp<RfxAtResponse> p_response;
    sp<RfxMclMessage> response;
    RIL_SIM_IO_Response sr;
    int err;
    RIL_Errno ril_error = RIL_E_SIM_ERR;
    String8 cmd("");
    RIL_SIM_APDU *p_args;
    RfxAtLine *line = NULL;
    int len;

    memset(&sr, 0, sizeof(sr));

    p_args = (RIL_SIM_APDU *)(msg->getData()->getData());

    if ((p_args->data == NULL) || (strlen(p_args->data) == 0)) {
        logD(mTag, "handleSimTransmitBasic p3 = %d", p_args->p3);

        if (p_args->p3 < 0) {
            cmd.append(String8::format("AT+CGLA=0,%d,\"%02x%02x%02x%02x\"",
                    8, p_args->cla, p_args->instruction,
                    p_args->p1, p_args->p2));
        } else {
            cmd.append(String8::format("AT+CGLA=0,%d,\"%02x%02x%02x%02x%02x\"",
                    10, p_args->cla, p_args->instruction,
                    p_args->p1, p_args->p2, p_args->p3));
        }
    } else {
        cmd.append(String8::format("AT+CGLA=0,%d,\"%02x%02x%02x%02x%02x%s\"",
                (unsigned int)(10 + strlen(p_args->data)), p_args->cla, p_args->instruction,
                p_args->p1, p_args->p2, p_args->p3,
                p_args->data));
    }
    p_response = atSendCommandSingleline(cmd, "+CGLA:");
    err = p_response->getError();

    if (err < 0 || p_response->getSuccess() == 0) {
        ril_error = RIL_E_GENERIC_FAILURE;
        goto error;
    }

    line = p_response->getIntermediates();
    line->atTokStart(&err);
    if (err < 0) goto error;

    len = line->atTokNextint(&err);
    if (err < 0) goto error;

    sr.simResponse = line->atTokNextstr(&err);
    if (err < 0) goto error;

    sscanf(&(sr.simResponse[len - 4]), "%02x%02x", &(sr.sw1), &(sr.sw2));
    sr.simResponse[len - 4] = '\0';
    logD(mTag, "handleSimTransmitBasic len = %d %02x, %02x", len, sr.sw1, sr.sw2);

    response = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_SUCCESS,
            RfxSimIoRspData((void*)&sr, sizeof(sr)), msg, false);

    responseToTelCore(response);
    return;

error:
    response = RfxMclMessage::obtainResponse(msg->getId(), ril_error,
            RfxSimIoRspData(NULL, 0), msg, false);

    responseToTelCore(response);
}

void RmcCommSimRequestHandler::handleDetectSim(const sp < RfxMclMessage > & msg) {
    char* urc = (char *) msg->getData()->getData();
    sp<RfxAtResponse> p_response = NULL;
    int err;
    RfxAtLine *line;
    char *iccId;
    RIL_Errno ret = RIL_E_SIM_ERR;
    int cardPresent = 0;
    int cmeError;
    char *cpinResult;

    logD("detectSim: entering, urc:%s", urc);

    do {
        // TODO: wait for radio manager ready
        //if (getRadioState(rid) == RADIO_STATE_UNAVAILABLE) {
        //    cardPresent = 1;
        //    break;
        //}

        p_response = atSendCommandSingleline("AT+CPIN?", "+CPIN:");
        err = p_response->getError();
        if (err != 0) {
            logE(mTag, "detectSim: err: %d", err);
            //if (err == AT_ERROR_INVALID_THREAD) {
            //    cardPresent = 1;
            //} else {
                cardPresent = 0;
            //}
            break;
        }
        if (p_response->getSuccess() == 0) {
            cmeError = p_response->atGetCmeError();
            logE(mTag, "detectSim: cme_ERROR: %d", cmeError);
            switch (cmeError) {
                case CME_SIM_BUSY: // try to do again until more than 20 times
                cardPresent = 14; // SIM busy, framework needs to query again
                    break;
                case CME_SIM_FAILURE:
                case CME_SIM_NOT_INSERTED:
                    cardPresent = 0;
                    break;
                case CME_SIM_WRONG:
                    cardPresent = 1;
                    break;
                default:
                    cardPresent = 1; // NOT_READY
                    break;
            }
        } else {
            logD(mTag, "detectSim: success");
            line = p_response->getIntermediates();
            line->atTokStart(&err);
            if (err < 0) {
                logD(mTag, "detectSim: atTokStart: err: %d", err);
                cardPresent = 1;
                break;
            }
            cpinResult = line->atTokNextstr(&err);
            if (err < 0) {
                logD(mTag, "detectSim: atTokNextstr: err: %d", err);
                cardPresent = 1;
                break;
            }
            if (strcmp (cpinResult, "SIM PIN") && strcmp (cpinResult, "SIM PUK") && strcmp(cpinResult, "PH-NET PIN") && strcmp (cpinResult, "PH-NET PUK") &&
                    strcmp (cpinResult, "PH-NETSUB PIN") && strcmp (cpinResult, "PH-NETSUB PUK") && strcmp (cpinResult, "PH-SP PIN") && strcmp (cpinResult, "PH-SP PUK") &&
                    strcmp (cpinResult, "PH-CORP PIN") && strcmp (cpinResult, "PH-CORP PUK") && strcmp (cpinResult, "PH-FSIM PIN") && strcmp (cpinResult, "PH-FSIM PUK") &&
                    strcmp (cpinResult, "READY") ) {
                logD(mTag, "detectSim: out of strcmp: %s", cpinResult);
                cardPresent = 0;
            } else {
                cardPresent = 1;
            }
            break;
        }

    } while(0);
    //RLOGD("detectSim before release_wake_lock");
    release_wake_lock("sim_hot_plug");
    //RLOGD("detectSim after release_wake_lock");
    int cardType = getMclStatusManager()->getIntValue(RFX_STATUS_KEY_CARD_TYPE);

    switch (cardPresent) {
        case 1:
            /* Card inserted */
            logD(mTag, "Ready to check EUSIM (%d) in the case of event plug in", cardType);
            if (cardType & RFX_CARD_TYPE_SIM || cardType & RFX_CARD_TYPE_USIM) {
                logD(mTag, "To notify RFX_MSG_URC_SIM_PLUG_IN");
                if (strStartsWith(urc, "+ESIMS: 1,9")) {
                    //RIL_UNSOL_RESPONSE(RIL_UNSOL_VIRTUAL_SIM_ON,
                    //        &sim_inserted_status, sizeof(int), rid);
                } else if (strStartsWith(urc, "+ESIMS: 1,12")) {
                    //notifyBtSapStatusInd(
                    //        RIL_SIM_SAP_STATUS_IND_Status_RIL_SIM_STATUS_CARD_INSERTED, rid);
                } else if (strStartsWith(urc, "+ESIMS: 1,14")) {
                    //notifyBtSapStatusInd(
                    //        RIL_SIM_SAP_STATUS_IND_Status_RIL_SIM_STATUS_RECOVERED, rid);
                }

                // External SIM [Start]
                if ((isCommontSlotSupport() == true) &&
                        (RmcCommSimUrcHandler::getVsimPlugInOutEvent(m_slot_id) == VSIM_TRIGGER_PLUG_IN)) {
                    logD(mTag, "vsim trigger plug in on common slot project.");
                    RmcCommSimUrcHandler::setVsimPlugInOutEvent(m_slot_id, VSIM_TRIGGER_RESET);
                } else if ((isCommontSlotSupport() == true) && RfxRilUtils::isVsimEnabled()) {
                    logD(mTag, "Ingore plug in event during vsim enabled on common slot project.");
                } else {
                // External SIM [End]
                    sp<RfxMclMessage> unsol = RfxMclMessage::obtainUrc(RFX_MSG_URC_SIM_PLUG_IN,
                            m_slot_id, RfxVoidData());
                    responseToTelCore(unsol);
                }
            } else if (cardType == -1 || cardType == 0) {
                // Wait for +EUSIM
                sendEvent(RFX_MSG_EVENT_SIM_DETECT_SIM, RfxStringData(urc), RIL_CMD_PROXY_1, m_slot_id);
                acquire_wake_lock(PARTIAL_WAKE_LOCK, "sim_hot_plug");
            }
            break;
        case 0:
        {
            sp<RfxMclMessage> unsol = NULL;
            String8 iccId(PROPERTY_ICCID_PREIFX);
            logD(mTag, "Event plug in was happened, but there is no SIM inserted actually.(%d)", cardType);
            if (cardType != 0) {
                logE(mTag, "It should not happen here. cardtype != 0.");
            }
            if (isCommontSlotSupport()) {
                logI(mTag, "notify RFX_MSG_URC_SIM_COMMON_SLOT_NO_CHANGED");
                iccId.append(String8::format("%d", (m_slot_id + 1)));
                rfx_property_set(iccId.string(), "N/A");

                // External SIM [Start]
                if ((RmcCommSimUrcHandler::getVsimPlugInOutEvent(m_slot_id) == VSIM_TRIGGER_PLUG_IN)) {
                    logD(mTag, "vsim trigger no change on common slot project.");
                    RmcCommSimUrcHandler::setVsimPlugInOutEvent(m_slot_id, VSIM_TRIGGER_RESET);
                } else if (RfxRilUtils::isVsimEnabled()) {
                    logD(mTag, "Ingore no change event during vsim enabled on common slot project.");
                } else {
                // External SIM [End]
                    unsol = RfxMclMessage::obtainUrc(RFX_MSG_URC_SIM_COMMON_SLOT_NO_CHANGED,
                            m_slot_id, RfxVoidData());
                    responseToTelCore(unsol);
                }
            }
        }
            break;
        case 14:
            logD(mTag, "detect sim and sim busy so try again");
            sendEvent(RFX_MSG_EVENT_SIM_DETECT_SIM, RfxStringData(urc), RIL_CMD_PROXY_1, m_slot_id,
                -1, -1, 200000000);
            acquire_wake_lock(PARTIAL_WAKE_LOCK, "sim_hot_plug");
            break;
        default:
            logE(mTag, "Event plug in was happened, but CPIN? response");
            break;
    }
    return;
}

void RmcCommSimRequestHandler::handleLocalRequestUsimAuthentication(const sp < RfxMclMessage > & msg) {
    RfxBaseData *rbd = msg->getData();
    const char**  strings = (const char**)rbd->getData();
    sp<RfxAtResponse> p_response = NULL;
    sp<RfxMclMessage> response;
    int err;
    RfxAtLine *line;
    int cmeError;
    char* auth_rsp;
    String8 cmd("");
    int sw1, sw2;

    logD(mTag, "handleLocalRequestUsimAuthentication str %s %s ", strings[0], strings[1]);
    cmd.append(String8::format("AT+EAUTH=\"%s\",\"%s\"", strings[0], strings[1]));
    p_response = atSendCommandSingleline(cmd, "+EAUTH:");
    err = p_response->getError();
    if (err < 0 || p_response->getSuccess() == 0) {
        logE(mTag, "p_response = %d /n", p_response->atGetCmeError());
        goto error;
    }

    line = p_response->getIntermediates();
    line->atTokStart(&err);
    if (err < 0) {
        goto error;
    }
    sw1 = line->atTokNextint(&err);
    if (err < 0) {
        goto error;
    }
    sw2 = line->atTokNextint(&err);
    if (err < 0) {
        goto error;
    }

    if(sw1 == 0x90 && sw2 == 0x00 && line->atTokHasmore()){
        auth_rsp = line->atTokNextstr(&err);
        if(err < 0) {
            goto error;
        }
        logD(mTag, "response str %s length is %d ", auth_rsp, strlen(auth_rsp));
    } else {
        goto error;
    }
    response = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_SUCCESS,
            RfxStringData((void*)auth_rsp, strlen(auth_rsp)), msg, false);

    responseToTelCore(response);
    return;

error:
    logE(mTag,"localRequestSimAuthentication Fail");
    response = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_GENERIC_FAILURE,
            RfxStringData(NULL, 0), msg, false);

    responseToTelCore(response);
}
void RmcCommSimRequestHandler::handleLocalRequestSimAuthentication(const sp < RfxMclMessage > & msg) {
    char* param = (char *) msg->getData()->getData();
    sp<RfxAtResponse> p_response = NULL;
    sp<RfxMclMessage> response;
    int err;
    RfxAtLine *line;
    int cmeError;
    char* auth_rsp;
    String8 cmd("");
    int sw1, sw2;

    cmd.append(String8::format("AT+EAUTH=\"%s\"",param));

    p_response = atSendCommandSingleline(cmd, "+EAUTH:");
    err = p_response->getError();

    if (err < 0 || p_response->getSuccess() == 0) {
        logE(mTag, "p_response = %d /n", p_response->atGetCmeError());
        goto error;
    }

    line = p_response->getIntermediates();
    line->atTokStart(&err);
    if (err < 0) {
        goto error;
    }
    sw1 = line->atTokNextint(&err);
    if (err < 0) {
        goto error;
    }
    sw2 = line->atTokNextint(&err);
    if (err < 0) {
        goto error;
    }

    if(sw1 == 0x90 && sw2 == 0x00 && line->atTokHasmore()){
        auth_rsp = line->atTokNextstr(&err);
        if(err < 0) {
            goto error;
        }
        logD(mTag, "response str %s length is %d ", auth_rsp, strlen(auth_rsp));
    } else {
        goto error;
    }
    response = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_SUCCESS,
            RfxStringData((void*)auth_rsp, strlen(auth_rsp)), msg, false);

    responseToTelCore(response);
    return;

error:
    logE(mTag,"localRequestSimAuthentication Fail");
    response = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_GENERIC_FAILURE,
            RfxStringData(NULL, 0), msg, false);

    responseToTelCore(response);
}
void RmcCommSimRequestHandler::handleEnterNetworkDepersonalization(const sp<RfxMclMessage>& msg) {
    RfxBaseData *rbd = msg->getData();
    const char**  strings = (const char**)rbd->getData();
    sp<RfxAtResponse> p_response = NULL;
    int err = -1;
    String8 cmd("");
    UICC_Status sim_status = getSimStatus();
    RIL_Errno result = RIL_E_GENERIC_FAILURE;
    int currSimInsertedState = getMclStatusManager()->getIntValue(RFX_STATUS_KEY_SIM_STATE);

    do {
        if (RFX_SIM_STATE_ABSENT == currSimInsertedState) {
            logE(mTag, "NetworkDepersonalization, sim absent");
            result = RIL_E_SIM_ABSENT;
            break;
        }

        if (UICC_NP == sim_status ||
            UICC_NSP == sim_status ||
            UICC_SP == sim_status ||
            UICC_CP == sim_status ||
            UICC_SIMP == sim_status) {
            cmd.append(String8::format("AT+CPIN=\"%s\"", strings[0]));
            p_response = atSendCommand(cmd.string());
            err = p_response->getError();

            cmd.clear();
            if (p_response->getSuccess() == 0) {
                switch (p_response->atGetCmeError()) {
                    case CME_NETWORK_PERSONALIZATION_PUK_REQUIRED:
                    case CME_INCORRECT_PASSWORD:
                        result = RIL_E_PASSWORD_INCORRECT;
                        break;
                    case CME_NETWORK_SUBSET_PERSONALIZATION_PUK_REQUIRED:
                        if (UICC_NP == sim_status) {
                            result = RIL_E_SUCCESS;
                        } else if (UICC_NSP == sim_status) {
                            result = RIL_E_PASSWORD_INCORRECT;
                        }
                        break;
                    case CME_SERVICE_PROVIDER_PERSONALIZATION_PUK_REQUIRED:
                        if (UICC_NP == sim_status || UICC_NSP == sim_status) {
                            result = RIL_E_SUCCESS;
                        } else if (UICC_SP == sim_status) {
                            result = RIL_E_PASSWORD_INCORRECT;
                        }
                        break;
                    case CME_CORPORATE_PERSONALIZATION_PUK_REQUIRED:
                        if (UICC_NP == sim_status ||
                            UICC_NSP == sim_status ||
                            UICC_SP == sim_status) {
                            result = RIL_E_SUCCESS;
                        } else if (UICC_CP == sim_status) {
                            result = RIL_E_PASSWORD_INCORRECT;
                        }
                        break;
                    case CME_PH_FSIM_PUK_REQUIRED:
                        if (UICC_SIMP == sim_status) {
                            result = RIL_E_PASSWORD_INCORRECT;
                        } else {
                            result = RIL_E_SUCCESS;
                        }
                        break;
                    case CME_OPERATION_NOT_ALLOWED_ERR:
                    case CME_OPERATION_NOT_SUPPORTED:
                    case CME_UNKNOWN:
                        logE(mTag, "Unknow error");
                        break;
                    default:
                        logE(mTag, "Not wrong password or not allowed.");
                        result = RIL_E_SUCCESS;
                        break;
                }
            } else {
                result = RIL_E_SUCCESS;
                //setRadioState(RADIO_STATE_SIM_READY,rid);
                sendSimStatusChanged();
            }
        }
        p_response = NULL;
        if (result == RIL_E_SUCCESS) {
            sendSimStatusChanged();
            /// M: ALPS02832079. Dynamic-SBP feature. @{
            //logD(mTag, "SIM NP unlocked! Setup Dynamic-SBP for RIL:%d", rid);
            // TODO: setup DSBP
            //setupDynamicSBP(rid);
            /// @}
            //  setRadioState(RADIO_STATE_SIM_READY,rid);
        }
    } while(0);
    sp<RfxMclMessage> response = RfxMclMessage::obtainResponse(msg->getId(), result,
            RfxIntsData(), msg, false);

    responseToTelCore(response);
}

void RmcCommSimRequestHandler::handleSimTransmitChannel(const sp < RfxMclMessage > & msg) {
    sp<RfxAtResponse> p_response;
    sp<RfxMclMessage> response;
    RIL_SIM_IO_Response sr;
    int err;
    RIL_Errno ril_error = RIL_E_SIM_ERR;
    String8 cmd("");
    RIL_SIM_APDU *p_args = (RIL_SIM_APDU *)(msg->getData()->getData());
    RfxAtLine *line = NULL;
    int len;

    memset(&sr, 0, sizeof(sr));
    // Set class byte according TS 102.221
    p_args->cla = setChannelToClassByte(p_args->cla, p_args->sessionid);

    logD(mTag, "requestSIM_TransmitChannel enter!");
    if ((p_args->data == NULL) || (strlen(p_args->data) == 0)) {
       logD(mTag, "requestSIM_TransmitChannel p3 = %d", p_args->p3);
       if (p_args->p3 < 0) {
           cmd.append(String8::format("AT+CGLA=%d,%d,\"%02x%02x%02x%02x\"",
                    p_args->sessionid, 8, p_args->cla, p_args->instruction,
                    p_args->p1, p_args->p2));
       } else {
           cmd.append(String8::format("AT+CGLA=%d,%d,\"%02x%02x%02x%02x%02x\"",
                    p_args->sessionid, 10, p_args->cla, p_args->instruction,
                    p_args->p1, p_args->p2, p_args->p3));
       }
    } else {
        cmd.append(String8::format("AT+CGLA=%d,%d,\"%02x%02x%02x%02x%02x%s\"",
                p_args->sessionid,
                (unsigned int)(10 + strlen(p_args->data)), p_args->cla, p_args->instruction,
                p_args->p1, p_args->p2, p_args->p3,
                p_args->data));
    }

    p_response = atSendCommandSingleline(cmd, "+CGLA:");
    err = p_response->getError();
    if (err < 0 || p_response->getSuccess() == 0) {
        ril_error = RIL_E_GENERIC_FAILURE;
        goto error;
    }

    line = p_response->getIntermediates();

    line->atTokStart(&err);
    if (err < 0) goto error;

    len = line->atTokNextint(&err);
    if (err < 0) goto error;

    sr.simResponse = line->atTokNextstr(&err);
    if (err < 0) goto error;

    sscanf(&(sr.simResponse[len - 4]), "%02x%02x", &(sr.sw1), &(sr.sw2));
    sr.simResponse[len - 4] = '\0';
    logD(mTag, "requestSIM_TransmitChannel len = %d %02x, %02x", len, sr.sw1, sr.sw2);

    response = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_SUCCESS,
            RfxSimIoRspData((void*)&sr, sizeof(sr)), msg, false);

    responseToTelCore(response);
    return;

error:
    response = RfxMclMessage::obtainResponse(msg->getId(), ril_error,
            RfxSimIoRspData(NULL, 0), msg, false);

    responseToTelCore(response);

}

/**
 * Returns a copy of the given CLA byte where the channel number bits are
 * set as specified by the given channel number See GlobalPlatform Card
 * Specification 2.2.0.7: 11.1.4 Class Byte Coding.
 *
 * @param cla the CLA byte. Won't be modified
 * @param channelNumber within [0..3] (for first interindustry class byte
 *            coding) or [4..19] (for further interindustry class byte
 *            coding)
 * @return the CLA byte with set channel number bits. The seventh bit
 *         indicating the used coding (first/further interindustry class
 *         byte coding) might be modified
 */
 int RmcCommSimRequestHandler::setChannelToClassByte(int cla, int channelNumber) {
    if (channelNumber < 4) {
        // b7 = 0 indicates the first interindustry class byte coding
        cla = ((cla & 0xBC) | channelNumber);

    } else if (channelNumber < 20) {
        // b7 = 1 indicates the further interindustry class byte coding
        bool isSM = (cla & 0x0C) != 0;
        cla = ((cla & 0xB0) | 0x40 | (channelNumber - 4));
        if (isSM) {
            cla |= 0x20;
        }
    }
    return cla;
}

void RmcCommSimRequestHandler::handleSimGetAtr(const sp < RfxMclMessage > & msg) {
    sp<RfxAtResponse> p_response;
    sp<RfxMclMessage> response;
    int err;
    RIL_Errno ril_error = RIL_E_SIM_ERR;
    char *tmp;
    char *atr;
    RfxAtLine *line;

    p_response = atSendCommandSingleline("AT+ESIMINFO=0", "+ESIMINFO:");
    err = p_response->getError();

    if (err < 0 || p_response->getSuccess() == 0) {
        logE(mTag, "handleSimGetAtr Fail");
        goto error;
    }

    // +ESIMINFO:<mode>, <data> => if<mode>=0,<data>=ATR Hex String
    line = p_response->getIntermediates();
    line->atTokStart(&err);
    if (err < 0) goto error;

    tmp = line->atTokNextstr(&err);
    if(err < 0) goto error;

    atr = line->atTokNextstr(&err);
    if (err < 0) goto error;

    logD(mTag, "handleSimGetAtr strlen of response is %d", strlen(atr));

    response = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_SUCCESS,
                RfxStringData((void*)atr, strlen(atr)), msg, false);

    responseToTelCore(response);
    return;

error:
    logE(mTag, "handleSimGetAtr, fail to parse respnse");
    response = RfxMclMessage::obtainResponse(msg->getId(), ril_error,
                RfxStringData(), msg, false);

    responseToTelCore(response);
}

void RmcCommSimRequestHandler::handleSecurityOperation(const sp<RfxMclMessage>& msg, UICC_Security_Operation op) {
    RfxBaseData *rbd = msg->getData();
    const char**  strings = (const char**)rbd->getData();
    size_t dataLen = rbd->getDataLength();
    String8 cmd("");
    //String8 aid("");
    int err = 0;
    sp<RfxAtResponse> p_response = NULL;
    int opRetryCount = -1;
    RIL_Errno result = RIL_E_GENERIC_FAILURE;
    RfxIntsData pRspData;
    RfxAtLine *line = NULL;
    int isSimPinEnabled = 0;
    bool bIgnoreCmd = false;
    int i = 0;
    UICC_Status sim_status;
    //int appTypeId = -1;

    do {
        switch (op) {
            case ENTER_PIN1:
            case ENTER_PUK1:
                if (dataLen == 2 * sizeof(char*) ) {
                    cmd.append(String8::format("AT+CPIN=\"%s\"", strings[0]));
                } else if (dataLen == 3 * sizeof(char*)) {
                    cmd.append(String8::format("AT+EPIN1=\"%s\",\"%s\"", strings[0], strings[1]));
                } else {
                    err = -1;
                    break;
                }
                p_response = atSendCommand(cmd.string());
                err = p_response->getError();
                break;
            case ENTER_PIN2:
            case ENTER_PUK2:
                if (dataLen == 2 * sizeof(char*) ) {
                    cmd.append(String8::format("AT+EPIN2=\"%s\"", strings[0]));
                } else if (dataLen == 3 * sizeof(char*) ) {
                    cmd.append(String8::format("AT+EPIN2=\"%s\",\"%s\"", strings[0], strings[1]));
                } else {
                    err = -1;
                    break;
                }
                p_response = atSendCommand(cmd.string());
                err = p_response->getError();
                break;
            case CHANGE_PIN1:
                /* Check if SIM pin has enabled */
                cmd.append(String8::format("AT+CLCK=\"SC\",2"));
                p_response = atSendCommandSingleline(cmd, "+CLCK:");
                //if (dataLen == 3 * sizeof(char*) && strings[2] != NULL) {
                //    aid.append(String8::format("%s", strings[2]));
                //}
                // Get app type id
                //appTypeId = queryAppTypeId(aid);
                //logD(mTag, "appTypeId=%d", appTypeId);
                //cmd.append(String8::format("AT+ECLCK=%d,\"SC\",2", appTypeId));
                //p_response = atSendCommandSingleline(cmd, "+ECLCK:");
                err = p_response->getError();

                if (err < 0) {
                    logE(mTag, "query SIM PIN lock:%d", err);
                    break;
                }

                if (p_response->getSuccess() == 0) {
                    err = -1;
                    break;
                }
                line = p_response->getIntermediates();
                line->atTokStart(&err);

                if (err < 0) {
                    logE(mTag, "query SIM PIN lock, get token error");
                    break;
                }
                isSimPinEnabled = line->atTokNextint(&err);

                if (err < 0) {
                    logE(mTag, "query SIM PIN lock, get result fail");
                    break;
                }

                logE(mTag, "query SIM PIN lock, isSimPinEnabled: %d", isSimPinEnabled);

                if (isSimPinEnabled == 0) {
                    result = RIL_E_REQUEST_NOT_SUPPORTED;
                    err = -1;
                    break;
                }
            case CHANGE_PIN2:
                /*  UI shall handle CHANGE PIN only operated on PIN which is enabled and nonblocking state. */
                if (dataLen == 3 * sizeof(char*)) {
                    if (CHANGE_PIN1 == op) {
                        sim_status = getSimStatus();
                        if (sim_status == UICC_PIN) {
                            op = ENTER_PIN1;
                            /* Solve CR - ALPS00260076 PIN can not be changed by using SS string when PIN is dismissed
                                          Modem does not support multiple AT commands, so input PIN1 first */
                            cmd.clear();
                            cmd.append(String8::format("AT+CPIN=\"%s\"", strings[0]));
                            bIgnoreCmd = true;
                            p_response = atSendCommand(cmd.string());
                            if (p_response->getSuccess() > 0) {
                               bIgnoreCmd = false;
                               cmd.clear();
                               cmd.append(String8::format("AT+CPWD=\"SC\",\"%s\",\"%s\"", strings[0], strings[1]));
                               /* Wait for SIM ready before changing PIN password
                                              Used the same retry scenario with getCardStatus */
                               for (i = 0; i < 30; i++) {
                                   logI(mTag, "Wait for SIM ready, try count:%d", i+1);
                                   sleepMsec(200);
                                   sim_status = getSimStatus();
                                   if (sim_status == UICC_READY) {
                                      logI(mTag, "SIM ready");
                                      break;
                                   }
                               }/* End of for */
                            }
                        } else {
                            cmd.clear();
                            cmd.append(String8::format("AT+CPWD=\"SC\",\"%s\",\"%s\"", strings[0], strings[1]));
                        }
                    } else {
                        cmd.clear();
                        cmd.append(String8::format("AT+CPWD=\"P2\",\"%s\",\"%s\"", strings[0], strings[1]));
                    }
                } else {
                    err = -1;
                    break;
                }
                if (!bIgnoreCmd) {
                    // [mtk02772] p_response should be free before use it, or it will be leakage
                    p_response = atSendCommand(cmd.string());
                    err = p_response->getError();
                }
                break;
            default:
                err = -1;
                break;
        }
        cmd.clear();
        if (err != 0) {
            /* AT runtime error */
            break;
        }

        RmcSimPinPukCount *retryCount = getPinPukRetryCount();

        if (retryCount != NULL) {
            if ((ENTER_PIN1 == op) || (CHANGE_PIN1 == op)) {
                opRetryCount = retryCount->pin1;
            } else if ((ENTER_PIN2 == op) || (CHANGE_PIN2 == op)) {
                opRetryCount = retryCount->pin2;
            } else if (ENTER_PUK1 == op) {
                opRetryCount = retryCount->puk1;
            } else if (ENTER_PUK2 == op) {
                opRetryCount = retryCount->puk2;
            }
            if (retryCount != NULL) {
                free(retryCount);
            }
        }
        logD(mTag, "handleSecurityOperation: op=%d, RetryCount=%d", op, opRetryCount);

        if (p_response->getSuccess() == 0) {
            switch (p_response->atGetCmeError()) {
                case CME_INCORRECT_PASSWORD:
                    result = RIL_E_PASSWORD_INCORRECT;
                    break;
                case CME_SIM_PIN_REQUIRED:
                case CME_SIM_PUK_REQUIRED:
                    result = RIL_E_PASSWORD_INCORRECT;
                    break;
                case CME_SIM_PIN2_REQUIRED:
                    result = RIL_E_SIM_PIN2;
                    break;
                case CME_SIM_PUK2_REQUIRED:
                    result = RIL_E_SIM_PUK2;
                    break;
                default:
                    result = RIL_E_GENERIC_FAILURE;
                    break;
            }
        } else {
            /* Only for FTA mode */
            char property_value[RFX_PROPERTY_VALUE_MAX] = { 0 };
            rfx_property_get(PROPERTY_GSM_GCF_TEST_MODE, property_value, "0");

            if (atoi(property_value) == 2) {
                // 1. Since modem need to init after unlock SIM PIN, we try to query SIM state to check
                //  if modem has complted initialization.
                int count = 0;
                do {
                    sim_status = getSimStatus();
                    if (UICC_BUSY == sim_status) {
                        sleepMsec(200);
                        count++; //to avoid block; if the busy time is too long; need to check modem.
                        if(count == 30) {
                            break;
                        }
                    }
                } while (UICC_BUSY == sim_status);
                String8 aid("");
                if (dataLen == 3 * sizeof(char*)) {
                    aid.append(String8::format("%s", strings[1]));
                }
                int appTypeId = queryAppTypeId(aid), channelId = 0, intPara = 0;
                cmd.clear();
                cmd.append(String8::format("AT+ESIMAPP=%d,0", appTypeId));

                p_response = atSendCommandSingleline(cmd.string(), "+ESIMAPP:");

                if (p_response != NULL && p_response->getSuccess() > 0) {
                    // Success
                    line = p_response->getIntermediates();
                    line->atTokStart(&err);
                    // application id
                    intPara = line->atTokNextint(&err);
                    // channel id
                    channelId = line->atTokNextint(&err);
                    if (channelId == 255) {
                        logE(mTag, "channel id is 255!");
                        result = RIL_E_SIM_BUSY;
                        break;
                    }
                    cmd.clear();
                    line = NULL;
                } else {
                    logE(mTag, "Fail to query app %d channel", appTypeId);
                    result = RIL_E_GENERIC_FAILURE;
                    break;
                }

                // 2. Check if there is techinal problem in SIM card to support lab test PTCRB 27.16
                //     Read any file to ensure there is no technical problem.
                sleepMsec(200);
                sp<RfxAtResponse> p_responseIccid = NULL;
                cmd.clear();
                cmd.append(String8::format("AT+ECRLA=%d,%d,%d,%d,%d,%d,%d", appTypeId, channelId,
                        0xb2, 0x2fe2, 1, 4, 10));
                p_responseIccid = atSendCommandSingleline(cmd.string(), "+ECRLA:");
                cmd.clear();
                if (p_responseIccid->getError() < 0 || p_responseIccid->getSuccess() == 0) {
                    logE(mTag, "Fail to read EF_ICCID");
                    if (p_responseIccid->atGetCmeError() == CME_SIM_TECHNICAL_PROBLEM) {
                        logE(mTag, "Fail to read EF_ICCID, got CME_SIM_TECHNICAL_PROBLEM.");
                        result = RIL_E_GENERIC_FAILURE;
                        break;
                    }
                } else {
                    logD(mTag, "success to read EF_ICCID");
                    result = RIL_E_SUCCESS;
                }
            } else {
                result = RIL_E_SUCCESS;
            }
        }
        /* release resource for getPINretryCount usage */

        if ((result == RIL_E_SUCCESS)&&((ENTER_PIN1 == op) || (ENTER_PUK1 == op))) {
            if (op == ENTER_PIN1) {
                logD(mTag, "ENTER_PIN1 Success!");
            } else {
                logD(mTag, "ENTER_PUK1 Success!");
            }

            sendSimStatusChanged();
            /// M: ALPS02301695. Dynamic-SBP feature. @{
            //logD(mTag, "PIN unlocked! Setup Dynamic-SBP for RIL:%d", rid);
            // TODO: setup DSBP
            //setupDynamicSBP(rid);
            /// @}
        }
    } while(0);
    cmd.clear();

    sp<RfxMclMessage> response = RfxMclMessage::obtainResponse(msg->getId(), result,
            RfxIntsData((void*)&opRetryCount, sizeof(opRetryCount)), msg, false);

    responseToTelCore(response);
}

void RmcCommSimRequestHandler::sendSimStatusChanged() {
    sp<RfxMclMessage> unsol = RfxMclMessage::obtainUrc(RFX_MSG_URC_RESPONSE_SIM_STATUS_CHANGED,
            m_slot_id, RfxVoidData());
    responseToTelCore(unsol);

}

void RmcCommSimRequestHandler::setSimInsertedStatus(int slotId, int isInserted) {
    int pivot = 1;
    int pivotSim = slotId;
    int simInsertedStatus = getMclStatusManager()->getIntValue(RFX_STATUS_KEY_SIM_INSERT_STATE);

    if(isInserted == 1) {
        pivotSim = pivot << slotId;
        simInsertedStatus = simInsertedStatus | pivotSim;
    } else {
        pivotSim = pivot << slotId;
        pivotSim = ~pivotSim;
        simInsertedStatus = (simInsertedStatus & pivotSim );
    }
    getMclStatusManager()->setIntValue(RFX_STATUS_KEY_SIM_INSERT_STATE, simInsertedStatus);
}

void RmcCommSimRequestHandler::makeSimRspFromUsimFcp(unsigned char ** simResponse) {
    int format_wrong = 0;
    unsigned char * fcpByte = NULL;
    unsigned short  fcpLen = 0;
    usim_file_descriptor_struct fDescriptor = {0,0,0,0};
    usim_file_size_struct fSize  = {0};
    unsigned char simRspByte[GET_RESPONSE_EF_SIZE_BYTES] = {0};

    fcpLen = hexStringToByteArray(*simResponse, &fcpByte);

    if (FALSE == usim_fcp_query_tag(fcpByte, fcpLen, FCP_FILE_DES_T, &fDescriptor)) {
        logE(mTag, "USIM FD Parse fail:%s", *simResponse);
        format_wrong = 1;
        goto done;
    }

    if ((!IS_DF_ADF(fDescriptor.fd)) && (FALSE == usim_fcp_query_tag(fcpByte, fcpLen, FCP_FILE_SIZE_T,&fSize))) {
        logE(mTag, "USIM File Size fail:%s", *simResponse);
        format_wrong = 1;
        goto done;
    }

    if (IS_DF_ADF(fDescriptor.fd)) {
        simRspByte[RESPONSE_DATA_FILE_TYPE] = TYPE_DF;
        goto done;
    } else {
        simRspByte[RESPONSE_DATA_FILE_TYPE] = TYPE_EF;
    }

    simRspByte[RESPONSE_DATA_FILE_SIZE_1] = (fSize.file_size & 0xFF00) >> 8;
    simRspByte[RESPONSE_DATA_FILE_SIZE_2] = fSize.file_size & 0xFF;

    if (IS_LINEAR_FIXED_EF(fDescriptor.fd)) {
        simRspByte[RESPONSE_DATA_STRUCTURE] = EF_TYPE_LINEAR_FIXED;
        simRspByte[RESPONSE_DATA_RECORD_LENGTH] = fDescriptor.rec_len;
    } else if (IS_TRANSPARENT_EF(fDescriptor.fd)) {
        simRspByte[RESPONSE_DATA_STRUCTURE] = EF_TYPE_TRANSPARENT;

    } else if (IS_CYCLIC_EF(fDescriptor.fd)) {
        simRspByte[RESPONSE_DATA_STRUCTURE] = EF_TYPE_CYCLIC;
        simRspByte[RESPONSE_DATA_RECORD_LENGTH] = fDescriptor.rec_len;
    }


done:
    free(*simResponse);
    free(fcpByte);
    if (format_wrong != 1) {
        *simResponse = byteArrayToHexString(simRspByte, GET_RESPONSE_EF_SIZE_BYTES);
        if (RfxRilUtils::isEngLoad()) {
            logD(mTag, "simRsp done:%s", *simResponse);
        }
    } else {
        *simResponse = NULL;
        logE(mTag, "simRsp done, but simRsp is null because command format may be wrong");
    }

}

// BTSAP - Start
void RmcCommSimRequestHandler::decodeBtSapPayload(int msgId, void *src,
        size_t srclen, void *dst) {
    pb_istream_t stream;
    const pb_field_t *fields = NULL;

    logD(mTag, "[BTSAP] decodeBtSapPayload start (%s)", idToString(msgId));
    if (dst == NULL || src == NULL) {
        logE(mTag, "[BTSAP] decodeBtSapPayload, dst or src is NULL!!");
        return;
    }

    switch (msgId) {
        case RFX_MSG_REQUEST_SIM_SAP_CONNECT:
            fields = RIL_SIM_SAP_CONNECT_REQ_fields;
            break;
        case RFX_MSG_REQUEST_SIM_SAP_DISCONNECT:
            fields = RIL_SIM_SAP_DISCONNECT_REQ_fields;
            break;
        case RFX_MSG_REQUEST_SIM_SAP_APDU:
            fields = RIL_SIM_SAP_APDU_REQ_fields;
            break;
        case RFX_MSG_REQUEST_SIM_SAP_TRANSFER_ATR:
            fields = RIL_SIM_SAP_TRANSFER_ATR_REQ_fields;
            break;
        case RFX_MSG_REQUEST_SIM_SAP_POWER:
            fields = RIL_SIM_SAP_POWER_REQ_fields;
            break;
        case RFX_MSG_REQUEST_SIM_SAP_RESET_SIM:
            fields = RIL_SIM_SAP_RESET_SIM_REQ_fields;
            break;
        case RFX_MSG_REQUEST_SIM_SAP_SET_TRANSFER_PROTOCOL:
            fields = RIL_SIM_SAP_SET_TRANSFER_PROTOCOL_REQ_fields;
            break;
        default:
            logE(mTag, "[BTSAP] decodeBtSapPayload, MsgId is mistake!");
            return;
    }

    stream = pb_istream_from_buffer((uint8_t *)src, srclen);
    if (!pb_decode(&stream, fields, dst) ) {
        logE(mTag, "[BTSAP] decodeBtSapPayload, Error decoding protobuf buffer : %s",
                PB_GET_ERROR(&stream));
    } else {
        logD(mTag, "[BTSAP] decodeBtSapPayload, Success!");
    }
}

void RmcCommSimRequestHandler::resetBtSapContext() {
    logD(mTag, "[BTSAP] resetBtSapAtr");
    getMclStatusManager()->setIntValue(RFX_STATUS_KEY_BTSAP_STATUS, BT_SAP_INIT);
    getMclStatusManager()->setIntValue(RFX_STATUS_KEY_BTSAP_CURRENT_PROTOCOL, 0);
    getMclStatusManager()->setIntValue(RFX_STATUS_KEY_BTSAP_SUPPORT_PROTOCOL, 0);
    getMclStatusManager()->setString8Value(RFX_STATUS_KEY_BTSAP_ATR, String8(""));
}

int RmcCommSimRequestHandler::toByte(char c) {
    if (c >= '0' && c <= '9') return (c - '0');
    if (c >= 'A' && c <= 'F') return (c - 'A' + 10);
    if (c >= 'a' && c <= 'f') return (c - 'a' + 10);

    logE(mTag, "toByte Error: %c",c);
    return 0;
}

int RmcCommSimRequestHandler::hexStringToByteArrayEx(unsigned char* hexString,
        int hexStringLen, unsigned char ** byte) {
    int length = hexStringLen/2;
    unsigned char* buffer = (unsigned char*)malloc((length + 1)*sizeof(char));
    int i = 0;

    memset(buffer, 0, ((length + 1)*sizeof(char)));
    for (i = 0 ; i < hexStringLen ; i += 2)
    {
        buffer[i / 2] = (unsigned char)((toByte(hexString[i]) << 4) | toByte(hexString[i+1]));
    }

    *byte = buffer;

    return (hexStringLen/2);
}

char* RmcCommSimRequestHandler::convertBtSapIntToHexString(uint8_t *data, size_t datalen) {
    char* output = NULL, *pOut = NULL;
    unsigned int i = 0;

    if (data == NULL || datalen <= 0) {
        return output;
    }

    output = (char*)calloc(1, (sizeof(char)*datalen+1)*2);
    pOut = output;

    for (i = 0; i < datalen; i++) {
        pOut = &output[2*i];
        sprintf(pOut, "%02x", data[i]);
    }

    logD(mTag, "[BTSAP] convert string (%d, %s)", datalen, output);
    return output;
}

void RmcCommSimRequestHandler::sendBtSapResponseComplete(const sp<RfxMclMessage>& msg,
        RIL_Errno ret, int msgId, void *data) {
    const pb_field_t *fields = NULL;
    size_t encoded_size = 0;
    size_t buffer_size = 0;
    pb_ostream_t ostream;
    bool success = false;
    ssize_t written_bytes;
    int i = 0;
    sp<RfxMclMessage> response = NULL;

    logD(mTag, "[BTSAP] sendBtSapResponseComplete, start (%s)", idToString(msgId));

    switch (msgId) {
        case RFX_MSG_REQUEST_SIM_SAP_CONNECT:
            fields = RIL_SIM_SAP_CONNECT_RSP_fields;
            break;
        case RFX_MSG_REQUEST_SIM_SAP_DISCONNECT:
            fields = RIL_SIM_SAP_DISCONNECT_RSP_fields;
            break;
        case RFX_MSG_REQUEST_SIM_SAP_APDU:
            fields = RIL_SIM_SAP_APDU_RSP_fields;
            break;
        case RFX_MSG_REQUEST_SIM_SAP_TRANSFER_ATR:
            fields = RIL_SIM_SAP_TRANSFER_ATR_RSP_fields;
            break;
        case RFX_MSG_REQUEST_SIM_SAP_POWER:
            fields = RIL_SIM_SAP_POWER_RSP_fields;
            break;
        case RFX_MSG_REQUEST_SIM_SAP_RESET_SIM:
            fields = RIL_SIM_SAP_RESET_SIM_RSP_fields;
            break;
        case RFX_MSG_REQUEST_SIM_SAP_SET_TRANSFER_PROTOCOL:
            fields = RIL_SIM_SAP_SET_TRANSFER_PROTOCOL_RSP_fields;
            break;
        case RFX_MSG_REQUEST_SIM_SAP_ERROR_RESP:
            fields = RIL_SIM_SAP_ERROR_RSP_fields;
            break;
        default:
            logE(mTag, "[BTSAP] sendBtSapResponseComplete, MsgId is mistake!");
            return;
    }

    if ((success = pb_get_encoded_size(&encoded_size, fields, data)) &&
            encoded_size <= INT32_MAX) {
        //buffer_size = encoded_size + sizeof(uint32_t);
        buffer_size = encoded_size;
        uint8_t buffer[buffer_size];
        //written_size = htonl((uint32_t) encoded_size);
        ostream = pb_ostream_from_buffer(buffer, buffer_size);
        //pb_write(&ostream, (uint8_t *)&written_size, sizeof(written_size));
        success = pb_encode(&ostream, fields, data);

        if(success) {
            logD(mTag, "[BTSAP] sendBtSapResponseComplete, Size: %d (0x%x)", encoded_size,
                    encoded_size);
            // Send response
            //RIL_SAP_onRequestComplete(t, ret, buffer, buffer_size);
            response = RfxMclMessage::obtainSapResponse(msgId, ret,
                    RfxRawData((void*)buffer, buffer_size), msg, false);
            responseToTelCore(response);
        } else {
            logE(mTag, "[BTSAP] sendBtSapResponseComplete, Encode failed!");
        }
    } else {
        logE(mTag, "Not sending response type %d: encoded_size: %u. encoded size result: %d",
        msgId, encoded_size, success);
    }
}

void RmcCommSimRequestHandler::handleLocalBtSapReset(const sp<RfxMclMessage>& msg) {
    RFX_UNUSED(msg);
    sendBtSapStatusInd(RIL_SIM_SAP_STATUS_IND_Status_RIL_SIM_STATUS_CARD_RESET);
}

void RmcCommSimRequestHandler::handleBtSapConnect(const sp<RfxMclMessage>& msg) {
    RIL_SIM_SAP_CONNECT_REQ *req = NULL;
    RIL_SIM_SAP_CONNECT_RSP rsp;
    sp<RfxAtResponse> p_response;
    int err, type = -1;
    int sysMaxSize = 32767;
    char *pAtr = NULL;
    RfxAtLine* line = NULL;
    RIL_Errno ret = RIL_E_GENERIC_FAILURE;
    RIL_SIM_SAP_STATUS_IND_Status unsolMsg = RIL_SIM_SAP_STATUS_IND_Status_RIL_SIM_STATUS_UNKNOWN_ERROR;
    void* data = msg->getData()->getData();
    int datalen = msg->getData()->getDataLength();

    logD(mTag, "[BTSAP] handleBtSapConnect start");
    req = (RIL_SIM_SAP_CONNECT_REQ*)calloc(1, sizeof(RIL_SIM_SAP_CONNECT_REQ));
    decodeBtSapPayload(RFX_MSG_REQUEST_SIM_SAP_CONNECT, data, datalen, req);

    if (req->max_message_size > sysMaxSize) {
        // Max message size from request > project setting
        rsp.response = RIL_SIM_SAP_CONNECT_RSP_Response_RIL_E_SAP_MSG_SIZE_TOO_LARGE;
        rsp.max_message_size = sysMaxSize;
        rsp.has_max_message_size = TRUE;
        sendBtSapResponseComplete(msg, ret, RFX_MSG_REQUEST_SIM_SAP_CONNECT, &rsp);
        free(req);
        return;
    }

    p_response = atSendCommandSingleline("AT+EBTSAP=0", "+EBTSAP:");
    err = p_response->getError();

    if (err < 0) {
        rsp.response = RIL_SIM_SAP_CONNECT_RSP_Response_RIL_E_SAP_CONNECT_FAILURE;
        goto format_error;
    }

    if (p_response->getSuccess() == 0) {
        logE(mTag, "[BTSAP] CME ERROR = %d", p_response->atGetCmeError());
        rsp.response = RIL_SIM_SAP_CONNECT_RSP_Response_RIL_E_SAP_CONNECT_FAILURE;
        switch (p_response->atGetCmeError()) {
            case CME_BT_SAP_UNDEFINED:
                ret = RIL_E_REQUEST_NOT_SUPPORTED;
                break;
            case CME_BT_SAP_NOT_ACCESSIBLE:
                ret = RIL_E_SIM_ERR;
                unsolMsg = RIL_SIM_SAP_STATUS_IND_Status_RIL_SIM_STATUS_CARD_NOT_ACCESSIBLE;
                break;
            case CME_BT_SAP_CARD_REMOVED:
            case CME_SIM_NOT_INSERTED:
                ret = RIL_E_SIM_ABSENT;
                unsolMsg = RIL_SIM_SAP_STATUS_IND_Status_RIL_SIM_STATUS_CARD_REMOVED;
                break;
            default:
                break;
        }
        goto format_error;
    } else {
        line = p_response->getIntermediates();
        rsp.response = RIL_SIM_SAP_CONNECT_RSP_Response_RIL_E_SAP_CONNECT_FAILURE;

        line->atTokStart(&err);
        if (err < 0) goto format_error;

        type = line->atTokNextint(&err);
        if (err < 0) goto format_error;

        getMclStatusManager()->setIntValue(RFX_STATUS_KEY_BTSAP_CURRENT_PROTOCOL, type);

        type = line->atTokNextint(&err);
        if (err < 0) goto  format_error;

        getMclStatusManager()->setIntValue(RFX_STATUS_KEY_BTSAP_SUPPORT_PROTOCOL, type);

        pAtr = line->atTokNextstr(&err);
        if (err < 0) goto  format_error;

        int atrLen = 0;
        if (pAtr != NULL) {
            atrLen = strlen(pAtr);
            getMclStatusManager()->setString8Value(RFX_STATUS_KEY_BTSAP_ATR,
                    String8::format("%s", pAtr));
        }

        logD(mTag, "[BTSAP] requestLocalSapConnect, cur_type: %d, supp_type: %d, size: %d",
                getMclStatusManager()->getIntValue(RFX_STATUS_KEY_BTSAP_CURRENT_PROTOCOL, -1),
                getMclStatusManager()->getIntValue(RFX_STATUS_KEY_BTSAP_SUPPORT_PROTOCOL, -1),
                atrLen);

        ret = RIL_E_SUCCESS;
        // For AOSP BT SAP UT case, to set the flag has_max_message_size as true.
        //rsp.has_max_message_size = false;
        rsp.has_max_message_size = true;
        rsp.max_message_size = 0;
        if (getMclStatusManager()->getIntValue(RFX_STATUS_KEY_VOICE_CALL_COUNT, 0) > 0) {
            // There is an ongoing call so do not notify STATUS_IND now
            logI(mTag, "[BTSAP] Connection Success but there is ongoing call");
            rsp.response = RIL_SIM_SAP_CONNECT_RSP_Response_RIL_E_SAP_CONNECT_OK_CALL_ONGOING;
        } else {
            logD(mTag, "[BTSAP] Connection Success");
            rsp.response = RIL_SIM_SAP_CONNECT_RSP_Response_RIL_E_SUCCESS;
            unsolMsg = RIL_SIM_SAP_STATUS_IND_Status_RIL_SIM_STATUS_CARD_RESET;
        }

        getMclStatusManager()->setIntValue(RFX_STATUS_KEY_BTSAP_STATUS, BT_SAP_CONNECTION_SETUP);
    }

    sendBtSapResponseComplete(msg, ret, RFX_MSG_REQUEST_SIM_SAP_CONNECT, &rsp);

    if (unsolMsg != RIL_SIM_SAP_STATUS_IND_Status_RIL_SIM_STATUS_UNKNOWN_ERROR) {
        sendBtSapStatusInd(unsolMsg);
    }
    if (req != NULL) {
        free(req);
    }
    logD(mTag, "[BTSAP] requestLocalSapConnect end");
    return;
format_error:
    logE(mTag, "[BTSAP] Connection Fail");
    rsp.has_max_message_size = false;
    rsp.max_message_size = 0;
    sendBtSapResponseComplete(msg, ret, RFX_MSG_REQUEST_SIM_SAP_CONNECT, &rsp);
    if (req != NULL) {
        free(req);
    }
}

void RmcCommSimRequestHandler::handleBtSapDisconnect(const sp<RfxMclMessage>& msg) {
    RIL_SIM_SAP_DISCONNECT_REQ *req = NULL;
    RIL_SIM_SAP_DISCONNECT_RSP rsp;
    sp<RfxAtResponse> p_response;
    int err;
    RfxAtLine *line = NULL;
    RIL_Errno ret = RIL_E_GENERIC_FAILURE;
    void *data = msg->getData()->getData();
    int datalen = msg->getData()->getDataLength();

    logD(mTag, "[BTSAP] requestLocalBtSapDisconnect start");
    req = (RIL_SIM_SAP_DISCONNECT_REQ*)calloc(1, sizeof(RIL_SIM_SAP_DISCONNECT_REQ));
    decodeBtSapPayload(RFX_MSG_REQUEST_SIM_SAP_DISCONNECT, data, datalen, req);

    getMclStatusManager()->setIntValue(RFX_STATUS_KEY_BTSAP_STATUS, BT_SAP_DISCONNECT);
    if (getMclStatusManager()->getIntValue(RFX_STATUS_KEY_CARD_TYPE, -1) > 0) {
        p_response = atSendCommand("AT+EBTSAP=1");
        err = p_response->getError();

        if (err < 0) {
            goto format_error;
        }

        if (p_response->getSuccess() == 0) {
            logE(mTag, "[BTSAP] CME ERROR = %d", p_response->atGetCmeError());
            switch (p_response->atGetCmeError()) {
                case CME_BT_SAP_UNDEFINED:
                    ret = RIL_E_REQUEST_NOT_SUPPORTED;
                    break;
                case CME_BT_SAP_NOT_ACCESSIBLE:
                    ret = RIL_E_SIM_ERR;
                    break;
                case CME_BT_SAP_CARD_REMOVED:
                case CME_SIM_NOT_INSERTED:
                    ret = RIL_E_SIM_ABSENT;
                    break;
                default:
                    break;
            }
            goto format_error;
        } else {
            resetBtSapContext();
            ret = RIL_E_SUCCESS;
            rsp.dummy_field = 1;
        }

        sendBtSapResponseComplete(msg, ret, RFX_MSG_REQUEST_SIM_SAP_DISCONNECT, &rsp);

        logD(mTag, "[BTSAP] requestLocalBtSapDisconnect end");
    } else {
        logD(mTag, "[BTSAP] requestBtSapDisconnect but card was removed");
        rsp.dummy_field = 1;
        getMclStatusManager()->setIntValue(RFX_STATUS_KEY_BTSAP_STATUS, BT_SAP_INIT);
        ret = RIL_E_SUCCESS;
        sendBtSapResponseComplete(msg, ret, RFX_MSG_REQUEST_SIM_SAP_DISCONNECT, &rsp);
    }
    if (req != NULL) {
        free(req);
    }
    return;
format_error:
    logE(mTag, "[BTSAP] Fail to disconnect");
    getMclStatusManager()->setIntValue(RFX_STATUS_KEY_BTSAP_STATUS, BT_SAP_INIT);
    rsp.dummy_field = 0;
    sendBtSapResponseComplete(msg, ret, RFX_MSG_REQUEST_SIM_SAP_DISCONNECT, &rsp);
    if (req != NULL) {
        free(req);
    }
}

void RmcCommSimRequestHandler::handleBtSapTransferApdu(const sp<RfxMclMessage>& msg) {
    RIL_SIM_SAP_APDU_REQ *req = NULL;
    RIL_SIM_SAP_APDU_RSP rsp;
    sp<RfxAtResponse> p_response;
    int err;
    String8 cmd("");
    RfxAtLine *line = NULL;
    char *pApduResponse = NULL;
    char *hexStr = NULL;
    pb_bytes_array_t *apduResponse = NULL;
    RIL_Errno ret = RIL_E_GENERIC_FAILURE;
    void *data = msg->getData()->getData();
    int datalen = msg->getData()->getDataLength();

    req = (RIL_SIM_SAP_APDU_REQ*)calloc(1, sizeof(RIL_SIM_SAP_APDU_REQ));
    decodeBtSapPayload(RFX_MSG_REQUEST_SIM_SAP_APDU, data, datalen, req);
    if (getMclStatusManager()->getIntValue(RFX_STATUS_KEY_CARD_TYPE, -1) <= 0) {
        logD(mTag, "[BTSAP] requestBtSapTransferApdu but card was removed");
        rsp.response = RIL_SIM_SAP_APDU_RSP_Response_RIL_E_SIM_ABSENT;
        ret = RIL_E_SIM_ABSENT;
        sendBtSapResponseComplete(msg, ret, RFX_MSG_REQUEST_SIM_SAP_APDU, &rsp);
    } else if (getMclStatusManager()->getIntValue(RFX_STATUS_KEY_BTSAP_STATUS, 0) ==
            BT_SAP_POWER_OFF) {
        logD(mTag, "[BTSAP] requestBtSapTransferApdu but card was already power off");
        rsp.response = RIL_SIM_SAP_APDU_RSP_Response_RIL_E_SIM_ALREADY_POWERED_OFF;
        sendBtSapResponseComplete(msg, ret, RFX_MSG_REQUEST_SIM_SAP_APDU, &rsp);
    } else {
        hexStr = convertBtSapIntToHexString(req->command->bytes, req->command->size);
        logD(mTag, "[BTSAP] requestLocalBtSapTransferApdu start, (%d,%d,%s)", req->type,
                req->command->size, hexStr);
        memset(&rsp, 0, sizeof(RIL_SIM_SAP_APDU_RSP));
        cmd.append(String8::format("AT+EBTSAP=5,0,\"%s\"", hexStr));
        p_response = atSendCommandSingleline(cmd, "+EBTSAP:");

        if (hexStr != NULL) {
            free(hexStr);
        }
        cmd.clear();

        err = p_response->getError();
        if (err < 0) {
            rsp.response = RIL_SIM_SAP_APDU_RSP_Response_RIL_E_GENERIC_FAILURE;
            goto format_error;
        }

        if (p_response->getSuccess() == 0) {
            logE(mTag, "[BTSAP] CME ERROR = %d", p_response->atGetCmeError());
            rsp.response = RIL_SIM_SAP_APDU_RSP_Response_RIL_E_GENERIC_FAILURE;
            switch (p_response->atGetCmeError()) {
                case CME_BT_SAP_UNDEFINED:
                    ret = RIL_E_REQUEST_NOT_SUPPORTED;
                    rsp.response = RIL_SIM_SAP_APDU_RSP_Response_RIL_E_GENERIC_FAILURE;
                    break;
                case CME_BT_SAP_NOT_ACCESSIBLE:
                    ret = RIL_E_SIM_ERR;
                    rsp.response = RIL_SIM_SAP_APDU_RSP_Response_RIL_E_SIM_NOT_READY;
                    break;
                case CME_BT_SAP_CARD_REMOVED:
                case CME_SIM_NOT_INSERTED:
                    ret = RIL_E_SIM_ABSENT;
                    rsp.response = RIL_SIM_SAP_APDU_RSP_Response_RIL_E_SIM_ABSENT;
                    break;
                default:
                    break;
            }
            goto format_error;
        } else {
            char *pRes = NULL;

            line = p_response->getIntermediates();

            line->atTokStart(&err);
            if (err < 0) goto format_error;

            pApduResponse = line->atTokNextstr(&err);
            if (err < 0) goto  format_error;

            int apduLen = strlen(pApduResponse);
            int reslen = hexStringToByteArrayEx((unsigned char *)pApduResponse, apduLen,
                    (unsigned char **)&pRes);
            apduResponse = (pb_bytes_array_t *)calloc(1,sizeof(pb_bytes_array_t) + reslen);
            memcpy(apduResponse->bytes, pRes, reslen);
            apduResponse->size = reslen;
            rsp.apduResponse = apduResponse;
            rsp.response = RIL_SIM_SAP_APDU_RSP_Response_RIL_E_SUCCESS;
            ret = RIL_E_SUCCESS;
            free(pRes);
        }

        sendBtSapResponseComplete(msg, ret, RFX_MSG_REQUEST_SIM_SAP_APDU, &rsp);

        if (apduResponse != NULL) {
            free(apduResponse);
        }
    }
    if (req != NULL) {
        free(req);
    }
    logD(mTag, "[BTSAP] requestLocalBtSapTransferApdu end");
    return;
format_error:
    logE(mTag, "[BTSAP] Fail to send APDU");

    sendBtSapResponseComplete(msg, ret, RFX_MSG_REQUEST_SIM_SAP_APDU, &rsp);

    if (req != NULL) {
        free(req);
    }
}

void RmcCommSimRequestHandler::handleBtSapTransferAtr(const sp<RfxMclMessage>& msg) {
    RIL_SIM_SAP_TRANSFER_ATR_REQ *req = NULL;
    RIL_SIM_SAP_TRANSFER_ATR_RSP rsp;
    RIL_Errno ret = RIL_E_GENERIC_FAILURE;
    int status = getMclStatusManager()->getIntValue(RFX_STATUS_KEY_BTSAP_STATUS, 0);
    pb_bytes_array_t *atrResponse = NULL;
    void *data = msg->getData()->getData();
    int datalen = msg->getData()->getDataLength();

    logD(mTag, "[BTSAP] requestBtSapTransferAtr start");
    req = (RIL_SIM_SAP_TRANSFER_ATR_REQ*)calloc(1, sizeof(RIL_SIM_SAP_TRANSFER_ATR_REQ));
    decodeBtSapPayload(RFX_MSG_REQUEST_SIM_SAP_TRANSFER_ATR, data, datalen, req);

    if (getMclStatusManager()->getIntValue(RFX_STATUS_KEY_CARD_TYPE, -1) <= 0) {
        logD(mTag, "[BTSAP] requestBtSapTransferAtr but card was removed");
        rsp.response = RIL_SIM_SAP_TRANSFER_ATR_RSP_Response_RIL_E_SIM_ABSENT;
        rsp.atr = NULL;
        ret = RIL_E_SIM_ABSENT;
    } else if (status == BT_SAP_POWER_OFF) {
        logD(mTag, "[BTSAP] requestBtSapTransferAtr but card was already power off");
        rsp.response = RIL_SIM_SAP_TRANSFER_ATR_RSP_Response_RIL_E_SIM_ALREADY_POWERED_OFF;
        rsp.atr = NULL;
    } else if (status == BT_SAP_POWER_ON ||
                status == BT_SAP_CONNECTION_SETUP ||
                status == BT_SAP_ONGOING_CONNECTION) {
        char *pRes = NULL;
        rsp.response = RIL_SIM_SAP_TRANSFER_ATR_RSP_Response_RIL_E_SUCCESS;
        String8 atr = getMclStatusManager()->getString8Value(RFX_STATUS_KEY_BTSAP_ATR);
        const char* atrStr = atr.string();
        int atrLen = strlen(atrStr);
        int reslen = hexStringToByteArrayEx((unsigned char *)atrStr, atrLen,
                (unsigned char **)&pRes);
        atrResponse = (pb_bytes_array_t *)calloc(1,sizeof(pb_bytes_array_t) + reslen);
        memcpy(atrResponse->bytes, pRes, reslen);
        atrResponse->size = reslen;
        rsp.atr = atrResponse;
        if (pRes != NULL) {
            free(pRes);
        }

        logD(mTag, "[BTSAP] requestBtSapTransferAtr, status: %d, size: %d", status, atrLen);

        if (status == BT_SAP_CONNECTION_SETUP) {
            getMclStatusManager()->setIntValue(RFX_STATUS_KEY_BTSAP_STATUS, BT_SAP_ONGOING_CONNECTION);
        }
    } else {
        rsp.response = RIL_SIM_SAP_TRANSFER_ATR_RSP_Response_RIL_E_GENERIC_FAILURE;
        rsp.atr = NULL;
    }
    sendBtSapResponseComplete(msg, ret, RFX_MSG_REQUEST_SIM_SAP_TRANSFER_ATR, &rsp);

    if (atrResponse != NULL) {
        free(atrResponse);
    }

    free(req);
    logD(mTag, "[BTSAP] requestBtSapTransferAtr end");
}

void RmcCommSimRequestHandler::handleBtSapPower(const sp<RfxMclMessage>& msg) {
    RIL_SIM_SAP_POWER_REQ *req = NULL;
    RIL_SIM_SAP_POWER_RSP rsp;
    RIL_Errno ret = RIL_E_GENERIC_FAILURE;
    sp<RfxAtResponse> p_response;
    int err;
    int type = 0;
    char *pAtr = NULL;
    String8 cmd("");
    RfxAtLine *line = NULL;
    int status = getMclStatusManager()->getIntValue(RFX_STATUS_KEY_BTSAP_STATUS);
    void *data = msg->getData()->getData();
    int datalen = msg->getData()->getDataLength();

    req = (RIL_SIM_SAP_POWER_REQ*)calloc(1, sizeof(RIL_SIM_SAP_POWER_REQ));
    decodeBtSapPayload(RFX_MSG_REQUEST_SIM_SAP_POWER, data, datalen, req);

    logD(mTag, "[BTSAP] requestBtSapPower start, (%d)", req->state);
    do {
        if (getMclStatusManager()->getIntValue(RFX_STATUS_KEY_CARD_TYPE) <= 0) {
            logD(mTag, "[BTSAP] requestBtSapPower but card was removed");
            rsp.response = RIL_SIM_SAP_POWER_RSP_Response_RIL_E_SIM_ABSENT;
            ret = RIL_E_SIM_ABSENT;
        } else if (req->state == TRUE && status == BT_SAP_POWER_ON) {
            logD(mTag, "[BTSAP] requestBtSapPower on but card was already power on");
            rsp.response = RIL_SIM_SAP_POWER_RSP_Response_RIL_E_SIM_ALREADY_POWERED_ON;
        } else if (req->state == FALSE && status == BT_SAP_POWER_OFF) {
            logD(mTag, "[BTSAP] requestBtSapPower off but card was already power off");
            rsp.response = RIL_SIM_SAP_POWER_RSP_Response_RIL_E_SIM_ALREADY_POWERED_OFF;
        } else {
            if (req->state == TRUE) {
                cmd.append(String8::format("AT+EBTSAP=2,%d", getMclStatusManager()->getIntValue(
                        RFX_STATUS_KEY_BTSAP_CURRENT_PROTOCOL)));
                p_response = atSendCommandSingleline(cmd, "+EBTSAP:");
            } else {
                p_response = atSendCommand("AT+EBTSAP=3");
            }

            err = p_response->getError();
            if (err < 0) {
                rsp.response = RIL_SIM_SAP_POWER_RSP_Response_RIL_E_GENERIC_FAILURE;
                goto format_error;
            }

            if (p_response->getSuccess() == 0) {
                logE(mTag, "[BTSAP] CME ERROR = %d", p_response->atGetCmeError());
                rsp.response = RIL_SIM_SAP_POWER_RSP_Response_RIL_E_GENERIC_FAILURE;
                switch (p_response->atGetCmeError()) {
                    case CME_BT_SAP_CARD_REMOVED:
                    case CME_SIM_NOT_INSERTED:
                        ret = RIL_E_SIM_ABSENT;
                        rsp.response = RIL_SIM_SAP_POWER_RSP_Response_RIL_E_SIM_ABSENT;
                        break;
                    default:
                        break;
                }
                goto format_error;
            } else {
                if (req->state == TRUE) {
                    line = p_response->getIntermediates();

                    line->atTokStart(&err);
                    if (err < 0) goto format_error;

                    type = line->atTokNextint(&err);
                    if (err < 0) goto  format_error;

                    getMclStatusManager()->setIntValue(RFX_STATUS_KEY_BTSAP_CURRENT_PROTOCOL, type);

                    pAtr = line->atTokNextstr(&err);
                    if (err < 0) goto  format_error;

                    int atrLen = 0;
                    if (pAtr != NULL) {
                        // Update ATR
                        atrLen = strlen(pAtr);
                        getMclStatusManager()->setString8Value(RFX_STATUS_KEY_BTSAP_ATR,
                                String8::format("%s", pAtr));
                    }
                    logD(mTag, "[BTSAP] requestLocalBtSapPower, cur_type: %d, size: %d",
                            getMclStatusManager()->getIntValue(
                            RFX_STATUS_KEY_BTSAP_CURRENT_PROTOCOL), atrLen);
                    getMclStatusManager()->setIntValue(RFX_STATUS_KEY_BTSAP_STATUS,
                            BT_SAP_POWER_ON);
                } else {
                    getMclStatusManager()->setIntValue(RFX_STATUS_KEY_BTSAP_STATUS,
                            BT_SAP_POWER_OFF);
                }
                logD(mTag, "[BTSAP] requestLocalBtSapPower, success! status: %d",
                        getMclStatusManager()->getIntValue(RFX_STATUS_KEY_BTSAP_STATUS));
                ret = RIL_E_SUCCESS;
                rsp.response = RIL_SIM_SAP_POWER_RSP_Response_RIL_E_SUCCESS;
            }
        }

        sendBtSapResponseComplete(msg, ret, RFX_MSG_REQUEST_SIM_SAP_POWER, &rsp);
    } while(0);

    free(req);

    logD(mTag, "[BTSAP] requestBtSapPower end");
    return;
format_error:
    logE(mTag, "[BTSAP] Fail to Set Power");
    sendBtSapResponseComplete(msg, ret, RFX_MSG_REQUEST_SIM_SAP_POWER, &rsp);
    if (req != NULL) {
        free(req);
    }

}

void RmcCommSimRequestHandler::handleBtSapResetSim(const sp<RfxMclMessage>& msg) {
    RIL_SIM_SAP_RESET_SIM_REQ *req = NULL;
    RIL_SIM_SAP_RESET_SIM_RSP rsp;
    RIL_Errno ret = RIL_E_GENERIC_FAILURE;
    int status = getMclStatusManager()->getIntValue(RFX_STATUS_KEY_BTSAP_STATUS);
    void *data = msg->getData()->getData();
    int datalen = msg->getData()->getDataLength();
    sp<RfxAtResponse> p_response;
    int err, type = 0;
    String8 cmd("");
    char *pAtr = NULL;
    RfxAtLine *line = NULL;
    pb_bytes_array_t *atrResponse = NULL;

    logD(mTag, "[BTSAP] requestBtSapResetSim start");
    req = (RIL_SIM_SAP_RESET_SIM_REQ*)calloc(1, sizeof(RIL_SIM_SAP_RESET_SIM_REQ));
    decodeBtSapPayload(RFX_MSG_REQUEST_SIM_SAP_RESET_SIM, data, datalen, req);

    do {
        if (getMclStatusManager()->getIntValue(RFX_STATUS_KEY_CARD_TYPE) <= 0) {
            logD(mTag, "[BTSAP] requestBtSapResetSim but card was removed");
            rsp.response = RIL_SIM_SAP_RESET_SIM_RSP_Response_RIL_E_SIM_ABSENT;
        } else if (status == BT_SAP_POWER_OFF) {
            logD(mTag, "[BTSAP] requestBtSapResetSim off but card was already power off");
            rsp.response = RIL_SIM_SAP_RESET_SIM_RSP_Response_RIL_E_SIM_ALREADY_POWERED_OFF;
        } else {
            memset(&rsp, 0, sizeof(RIL_SIM_SAP_RESET_SIM_RSP));
            cmd.append(String8::format("AT+EBTSAP=4,%d", getMclStatusManager()->getIntValue(
                    RFX_STATUS_KEY_BTSAP_CURRENT_PROTOCOL)));
            p_response = atSendCommandSingleline(cmd, "+EBTSAP:");
            err = p_response->getError();

            if (err < 0) {
                rsp.response = RIL_SIM_SAP_RESET_SIM_RSP_Response_RIL_E_GENERIC_FAILURE;
                goto format_error;
            }

            if (p_response->getSuccess() == 0) {
                logE(mTag, "[BTSAP] CME ERROR = %d", p_response->atGetCmeError());
                rsp.response = RIL_SIM_SAP_RESET_SIM_RSP_Response_RIL_E_GENERIC_FAILURE;
                switch (p_response->atGetCmeError()) {
                    case CME_BT_SAP_CARD_REMOVED:
                    case CME_SIM_NOT_INSERTED:
                        ret = RIL_E_SIM_ABSENT;
                        rsp.response = RIL_SIM_SAP_RESET_SIM_RSP_Response_RIL_E_SIM_ABSENT;
                        break;
                    default:
                        break;
                }
                goto format_error;
            } else {
                line = p_response->getIntermediates();

                line->atTokStart(&err);
                if (err < 0) goto format_error;

                type = line->atTokNextint(&err);
                if (err < 0) goto  format_error;

                getMclStatusManager()->setIntValue(RFX_STATUS_KEY_BTSAP_CURRENT_PROTOCOL, type);

                pAtr = line->atTokNextstr(&err);
                if (err < 0) goto  format_error;

                int atrLen = 0;

                if (pAtr != NULL) {
                    // Update ATR
                    atrLen = strlen(pAtr);
                    getMclStatusManager()->setString8Value(RFX_STATUS_KEY_BTSAP_ATR,
                            String8::format("%s", pAtr));
                }

                logD(mTag, "[BTSAP] requestLocalBtSapResetSim, cur_type: %d, size: %d",
                        getMclStatusManager()->getIntValue(RFX_STATUS_KEY_BTSAP_CURRENT_PROTOCOL),
                        atrLen);
                ret = RIL_E_SUCCESS;
                rsp.response = RIL_SIM_SAP_RESET_SIM_RSP_Response_RIL_E_SUCCESS;
            }
        }

        sendBtSapResponseComplete(msg, ret, RFX_MSG_REQUEST_SIM_SAP_RESET_SIM, &rsp);
    } while(0);

    free(req);

    logD(mTag, "[BTSAP] requestBtSapResetSim end");
    return;
format_error:
    logE(mTag, "[BTSAP] Fail to Reset SIM");

    sendBtSapResponseComplete(msg, ret, RFX_MSG_REQUEST_SIM_SAP_RESET_SIM, &rsp);
}

void RmcCommSimRequestHandler::handleBtSapSetTransferProtocol(const sp<RfxMclMessage>& msg) {
    RIL_SIM_SAP_ERROR_RSP rsp;
    RIL_Errno ret = RIL_E_REQUEST_NOT_SUPPORTED;
    sp<RfxMclMessage> response;

    logD(mTag, "[BTSAP] requestBtSapSetTransferProtocol start");
    rsp.dummy_field = 1;

    sendBtSapResponseComplete(msg, ret, RFX_MSG_REQUEST_SIM_SAP_SET_TRANSFER_PROTOCOL, &rsp);
}
// BTSAP - End

void RmcCommSimRequestHandler::handleCdmaCardType(const char *iccid) {
    int cardType = UNKOWN_CARD;
    String8 cdmaCardType("ril.cdma.card.type");
    cdmaCardType.append(String8::format(".%d", (m_slot_id + 1)));

    bool cdmalockedcard =  getMclStatusManager()->getBoolValue(RFX_STATUS_KEY_CDMA_LOCKED_CARD);

    // Card is locked if ESIMS:0,16 is reported.
    if (cdmalockedcard == true) {
        cardType = LOCKED_CARD;
    } else {
        // Card is absent if iccid is NULL
        if (iccid == NULL) {
            cardType = CARD_NOT_INSERTED;
        } else {
            if (isOp09Card(iccid)) {
                // OP09 card type
                if (getMclStatusManager()->getBoolValue(RFX_STATUS_KEY_CDMA3G_DUALMODE_CARD)
                        == 1) {
                    // OP09 3G dual mode card.
                    cardType = CT_UIM_SIM_CARD;
                } else {
                    int eusim = getMclStatusManager()->getIntValue(RFX_STATUS_KEY_CARD_TYPE);

                    if (eusim < 0) {
                        cardType = UNKOWN_CARD;
                        logE(mTag, "Do not get a valid CDMA card type: %d !", cardType);
                    } else if (eusim == 0) {
                        // Rarely happen, iccid exists but card type is empty.
                        cardType = CARD_NOT_INSERTED;
                    } else if (((eusim & RFX_CARD_TYPE_SIM) == 0) &&
                            ((eusim & RFX_CARD_TYPE_USIM) == 0) &&
                            ((eusim & RFX_CARD_TYPE_ISIM) == 0)) {
                        // OP09 3G single mode card.
                        cardType = CT_3G_UIM_CARD;
                    } else if (((eusim & RFX_CARD_TYPE_CSIM) != 0) &&
                            ((eusim & RFX_CARD_TYPE_USIM) != 0)) {
                        // Typical OP09 4G dual mode card.
                        cardType = CT_4G_UICC_CARD;
                    } else if (eusim == RFX_CARD_TYPE_SIM) {
                        // For OP09 A lib no C capability slot, CT 3G card type is reported as
                        // "SIM" card and "+ECT3G" value is false.
                        cardType = CT_UIM_SIM_CARD;
                    } else if (((eusim & RFX_CARD_TYPE_USIM) != 0) &&
                            (strStartsWith(iccid, "8985231"))) {
                        // OP09 CTEXCEL card.
                        cardType = CT_EXCEL_GG_CARD;
                    } else if ((eusim == RFX_CARD_TYPE_USIM) ||
                            (eusim == (RFX_CARD_TYPE_USIM | RFX_CARD_TYPE_ISIM))) {
                        if (isOP09AProject() && (m_slot_id == 1)) {
                            // For OP09 A lib no C capability slot, the card may have CSIM but not
                            // reported by Modem. To check full uicc card type by reading EFdir file.
                            int applist = getMclStatusManager()->getIntValue(
                                    RFX_STATUS_KEY_ESIMIND_APPLIST);

                            if (applist == (RFX_UICC_APPLIST_USIM | RFX_UICC_APPLIST_CSIM)) {
                                // OP09 card in non C slot for OP09 A lib.
                                cardType = CT_4G_UICC_CARD;
                            } else if (applist < 0) {
                                if (isApplicationIdExist("A0000003431002") == 1) {
                                    // OP09 card in non C slot for OP09 A lib.
                                    cardType = CT_4G_UICC_CARD;
                                } else {
                                    // USIM card if there is decode error or no CSIM aid exists.
                                    cardType = SIM_CARD;
                                }
                            } else {
                                // USIM card if there is no CSIM aid exist.
                                cardType = SIM_CARD;
                            }
                        } else {
                            // SIM card.
                            cardType = SIM_CARD;
                        }
                    } else {
                        cardType = UNKOWN_CARD;
                        logE(mTag, "Do not get a valid CDMA card type: %d !", cardType);
                    }
                }
            } else {
                // Non-OP09 card type.
                if (getMclStatusManager()->getBoolValue(RFX_STATUS_KEY_CDMA3G_DUALMODE_CARD)
                        == 1) {
                    // Non-OP09 CDMA 3G dual mode card.
                    cardType = UIM_SIM_CARD;
                } else {
                    int eusim = getMclStatusManager()->getIntValue(RFX_STATUS_KEY_CARD_TYPE);

                    if (eusim < 0) {
                        cardType = UNKOWN_CARD;
                        logE(mTag, "Do not get a valid CDMA card type: %d !", cardType);
                    } else if (eusim == 0) {
                        // Rarely happen, iccid exists but card type is empty.
                        cardType = CARD_NOT_INSERTED;
                    } else if (((eusim & RFX_CARD_TYPE_SIM) == 0) &&
                            ((eusim & RFX_CARD_TYPE_USIM) == 0) &&
                            ((eusim & RFX_CARD_TYPE_ISIM) == 0)) {
                        // Non-OP09 3G single mode card.
                        cardType = UIM_CARD;
                    } else if (((eusim & RFX_CARD_TYPE_CSIM) != 0) &&
                            ((eusim & RFX_CARD_TYPE_USIM) != 0)) {
                        // Typical non-OP09 4G dual mode card.
                        cardType = NOT_CT_UICC_CARD;
                    } else if (eusim == RFX_CARD_TYPE_SIM) {
                        // SIM card.
                        cardType = SIM_CARD;
                    } else if ((eusim == RFX_CARD_TYPE_USIM) ||
                            (eusim == (RFX_CARD_TYPE_USIM | RFX_CARD_TYPE_ISIM))) {
                        if (isOP09AProject() && (m_slot_id == 1)) {
                            // For OP09 A lib no C capability slot, the card may have CSIM but not
                            // reported by Modem. To check full uicc card type by reading EFdir file.
                            int applist = getMclStatusManager()->getIntValue(
                                    RFX_STATUS_KEY_ESIMIND_APPLIST);

                            if (applist == (RFX_UICC_APPLIST_USIM | RFX_UICC_APPLIST_CSIM)) {
                                // OP09 card in non C slot for OP09 A lib.
                                cardType = NOT_CT_UICC_CARD;
                            } else if (applist < 0) {
                                if (isApplicationIdExist("A0000003431002") == 1) {
                                    // OP09 card in non C slot for OP09 A lib.
                                    cardType = NOT_CT_UICC_CARD;
                                } else {
                                    // USIM card if there is decode error or no CSIM aid exists.
                                    cardType = SIM_CARD;
                                }
                            } else {
                                // USIM card if there is no CSIM aid exist.
                                cardType = SIM_CARD;
                            }
                        } else {
                            // SIM card.
                            cardType = SIM_CARD;
                        }
                    } else {
                        cardType = UNKOWN_CARD;
                        logE(mTag, "Do not get a valid CDMA card type: %d !", cardType);
                    }
                }
            }
        }
    }

    // Set cdma card type.
    rfx_property_set(cdmaCardType, String8::format("%d", cardType).string());
    getMclStatusManager()->setIntValue(RFX_STATUS_KEY_CDMA_CARD_TYPE, cardType);
}

bool RmcCommSimRequestHandler::isOp09Card(const char *iccid) {
    bool isOp09Card = false;

    // Compare with OP09 iccid prefix.
    if ((strStartsWith(iccid, "898603")) ||
            (strStartsWith(iccid, "898611")) ||
            (strStartsWith(iccid, "8985302")) ||
            (strStartsWith(iccid, "8985307")) ||
            (strStartsWith(iccid, "8985231"))) {
        isOp09Card = true;
    }
    return isOp09Card;
}

int RmcCommSimRequestHandler::isApplicationIdExist(const char *aid) {
    int isAidExist = -1;
    sp<RfxAtResponse> p_response;
    int err;
    char *atr;
    RfxAtLine *line;

    // AT+CUAD[=<option>]
    // <option>: integer type.
    // 0:   no parameters requested in addition to <response>.
    // 1    include <active_application>.
    // +CUAD: <response>[,<active_application>[,<AID>]]
    // <response>: string type in hexadecimal character format.The response is the content of the
    // EFDIR.
    // <active_application>: integer type.
    // 0    no SIM or USIM active.
    // 1    active application is SIM.
    // 2    active application is USIM, followed by <AID>.
    // <AID>: string type in hexadecimal character format. AID of active USIM.

    p_response = atSendCommandSingleline("AT+CUAD=0", "+CUAD:");

    if (p_response != NULL) {
        err = p_response->getError();
    } else {
        goto error;
    }

    if (err < 0 || p_response->getSuccess() == 0) {
        goto error;
    }

    line = p_response->getIntermediates();
    line->atTokStart(&err);
    if (err < 0) goto error;

    atr = line->atTokNextstr(&err);
    if (err < 0) goto error;

    if (strstr(atr, aid) != NULL) {
        isAidExist = 1;
    } else {
        isAidExist = 0;
    }

    logD(mTag, "isApplicationIdExist isAidExist: %d", isAidExist);
    return isAidExist;

error:
    logE(mTag, "isApplicationIdExist, fail to parse response!");

    return isAidExist;
}

void RmcCommSimRequestHandler::handleSetSimCardPower(const sp < RfxMclMessage > & msg) {
    sp<RfxAtResponse> p_response;
    sp<RfxMclMessage> response;
    int err = RIL_E_SUCCESS;
    String8 cmd("");
    RfxAtLine *line = NULL;
    int* mode = (int*)(msg->getData()->getData());
    int cmeError = CME_UNKNOWN;

    cmd.append(String8::format("AT+ESIMPOWER=%d", (*mode)));
    p_response = atSendCommand(cmd);
    err = p_response->getError();
    if (err < 0 || p_response->getSuccess() == 0) {
        logE(mTag, "requestSetSimCardPower AT+ESIMPOWER=1 Fail, e= %d", err);
        err = RIL_E_REQUEST_NOT_SUPPORTED;
    }

done:
    response = RfxMclMessage::obtainResponse(msg->getId(), (RIL_Errno)err,
            RfxVoidData(), msg, false);

    responseToTelCore(response);
}
