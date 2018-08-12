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

#include "RmcOemRequestHandler.h"
#include "RmcSuppServRequestHandler.h"
#include "RfxIntsData.h"
#include "RfxVoidData.h"
#include "RfxStringsData.h"
#include "RfxStringData.h"
#include "RfxRawData.h"
#include "RfxActivityData.h"
#include <string.h>
#include "RfxVersionManager.h"
#include "ratconfig.h"
#include "RfxRilUtils.h"

#define RFX_LOG_TAG "RmcOemRequestHandler"

#define PROPERTY_GSM_GCF_TEST_MODE  "gsm.gcf.testmode"
#define PROPERTY_SERIAL_NUMBER "gsm.serial"

// register handler to channel
RFX_IMPLEMENT_HANDLER_CLASS(RmcOemRequestHandler, RIL_CMD_PROXY_3);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxStringsData,
        RFX_MSG_REQUEST_DEVICE_IDENTITY);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxRawData, RfxRawData,
        RFX_MSG_REQUEST_OEM_HOOK_RAW);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxStringsData, RfxStringsData,
        RFX_MSG_REQUEST_OEM_HOOK_STRINGS);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxActivityData,
        RFX_MSG_REQUEST_GET_ACTIVITY_INFO);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxStringData,
        RFX_MSG_REQUEST_BASEBAND_VERSION);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxStringData, RfxStringData,
        RFX_MSG_REQUEST_QUERY_MODEM_THERMAL);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData,
        RFX_MSG_REQUEST_SET_TRM);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxStringData,
        RFX_MSG_REQUEST_GET_IMEI);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxStringData,
        RFX_MSG_REQUEST_GET_IMEISV);

RmcOemRequestHandler::RmcOemRequestHandler(int slot_id, int channel_id) :
        RfxBaseHandler(slot_id, channel_id) {
    const int request[] = {
        RFX_MSG_REQUEST_DEVICE_IDENTITY,
        RFX_MSG_REQUEST_OEM_HOOK_RAW,
        RFX_MSG_REQUEST_OEM_HOOK_STRINGS,
        RFX_MSG_REQUEST_GET_ACTIVITY_INFO,
        RFX_MSG_REQUEST_BASEBAND_VERSION,
        RFX_MSG_REQUEST_QUERY_MODEM_THERMAL,
        RFX_MSG_REQUEST_SET_TRM,
        RFX_MSG_REQUEST_GET_IMEI,
        RFX_MSG_REQUEST_GET_IMEISV
    };

    registerToHandleRequest(request, sizeof(request)/sizeof(int));

    if (RFX_SLOT_ID_0 == slot_id) {
        requestMdVersion();
    }
    /*  Enable getting CFU info +ECFU and speech info +ESPEECH*/
    int einfo_value;
    einfo_value = 50;//default value.

    /*  Enable getting CFU info +ECFU and speech info +ESPEECH and modem warning +EWARNING(0x100) */
    char modemWarningProperty[RFX_PROPERTY_VALUE_MAX];
    rfx_property_get("persist.radio.modem.warning", modemWarningProperty, "0");
    if (strcmp(modemWarningProperty, "1") == 0) {
        /* Enable "+EWARNING" */
       einfo_value |= 512;
    }
    /* Enable response message of call ctrl by sim. */
    einfo_value |= 1024;
    atSendCommand(String8::format("AT+EINFO=%d", einfo_value));

    /* M: Start - abnormal event logging for logger */
    einfo_value |= 8;
    /* Enable smart logging no service notification +ENWINFO */
    atSendCommand(String8::format("AT+EINFO=%d,401,0", einfo_value));
    /* M: End - abnormal event logging for logger */
    requestGetImei();
    requestGetImeisv();
    int mainSlotId = RfxRilUtils::getMajorSim() - 1;
    if (mainSlotId == slot_id) {
        requestGetGcfMode();
        bootupGetBasebandProject();

        // This is used for wifi-only version load
        // Since RIL is not connected to RILD in wifi-only version
        // we query it and stored into a system property
        // note: since this patch has no impact to nomal load, do this in normal initial procedure
        requestSN();
    }
}

RmcOemRequestHandler::~RmcOemRequestHandler() {
}

void RmcOemRequestHandler::onHandleRequest(const sp<RfxMclMessage>& msg) {
    int id = msg->getId();
    logD(RFX_LOG_TAG, "onHandleRequest: %s(%d)", idToString(id), id);
    switch (id) {
        case RFX_MSG_REQUEST_DEVICE_IDENTITY:
            requestDeviceIdentity(msg);
            break;
        case RFX_MSG_REQUEST_OEM_HOOK_RAW:
            requestOemHookRaw(msg);
            break;
        case RFX_MSG_REQUEST_OEM_HOOK_STRINGS:
            requestOemHookStrings(msg);
            break;
        case RFX_MSG_REQUEST_GET_IMEI:
            requestGetImei(msg);
            break;
        case RFX_MSG_REQUEST_GET_IMEISV:
            requestGetImeisv(msg);
            break;
        case RFX_MSG_REQUEST_GET_ACTIVITY_INFO:
            requestGetActivityInfo(msg);
            break;
        case RFX_MSG_REQUEST_BASEBAND_VERSION:
            requestBasebandVersion(msg);
            break;
        case RFX_MSG_REQUEST_QUERY_MODEM_THERMAL:
            requestQueryThermal(msg);
            break;
        case RFX_MSG_REQUEST_SET_TRM:
            requestSetTrm(msg);
            break;
        default:
            logE(RFX_LOG_TAG, "Should not be here");
            break;
    }
}

void RmcOemRequestHandler::onHandleTimer() {
    // do something
}

void RmcOemRequestHandler::onHandleEvent(const sp<RfxMclMessage>& msg) {
    int id = msg->getId();
    logD(RFX_LOG_TAG, "onHandleRequest: %s(%d)", idToString(id), id);
    switch (id) {
        default:
            logE(RFX_LOG_TAG, "should not be here");
            break;
    }
}

void RmcOemRequestHandler::requestDeviceIdentity(const sp<RfxMclMessage>& msg) {
    sp<RfxAtResponse> p_response;
    sp<RfxAtResponse> p_responseGSN;
    sp<RfxAtResponse> p_responseMEID;
    sp<RfxAtResponse> p_responseUIMID;
    sp<RfxMclMessage> response;
    int err = 0;
    RfxAtLine* line = NULL;
    char *tmp = NULL;
    RIL_IDENTITY identity;
    memset(&identity, 0, sizeof(identity));

    // Query IMEI
    p_response = atSendCommandNumeric("AT+CGSN");

    if (p_response == NULL
            || p_response->getError() != 0
            || p_response->getSuccess() == 0
            || p_response->getIntermediates() == NULL) {
        goto error;
    }

    identity.imei = p_response->getIntermediates()->getLine();

    // Query ImeiSv
    if (mImeiSv.isEmpty()) {
        requestGetImeisv();
    }
    if (!mImeiSv.isEmpty()) {
        identity.imeisv = (char *)mImeiSv.string();
    } else {
        goto error;
    }

    if (RfxRilUtils::isC2kSupport() && RatConfig_isC2kSupported()) {
        // Query ESN
        p_responseGSN = atSendCommandMultiline("AT+GSN", "+GSN:");

        if (p_responseGSN == NULL
                || p_responseGSN->getError() != 0
                || p_responseGSN->getSuccess() == 0
                || p_responseGSN->getIntermediates() == NULL) {
            goto error;
        }

        line = p_responseGSN->getIntermediates();

        line->atTokStart(&err);
        if (err < 0) goto error;

        tmp = line->atTokNextstr(&err);
        if (err < 0) goto error;

        if (strstr(tmp, "0x") != NULL) {
            identity.esnHex = tmp + 2;
            //logD(RFX_LOG_TAG, "identity.esnHex = %s", identity.esnHex);
        } else {
            identity.esnDec = tmp;
            //logD(RFX_LOG_TAG, "identity.esnDec = %s", identity.esnDec);
        }

        line = p_responseGSN->getIntermediates()->getNext();
        if (line != NULL) {
            line->atTokStart(&err);
            if (err < 0) goto error;

            tmp = line->atTokNextstr(&err);
            if (err < 0) goto error;

            if (strstr(tmp, "0x") != NULL) {
                identity.esnHex = tmp + 2;
                //logD(RFX_LOG_TAG, "identity.esnHex = %s", identity.esnHex);
            } else {
                identity.esnDec = tmp;
                //logD(RFX_LOG_TAG, "identity.esnDec = %s", identity.esnDec);
            }
        }

        //  Query MEID
        p_responseMEID = atSendCommandMultiline("AT^MEID", "^MEID:");

        if (p_responseMEID == NULL
                || p_responseMEID->getError() != 0
                || p_responseMEID->getSuccess() == 0
                || p_responseMEID->getIntermediates() == NULL) {
            goto error;
        }

        line = p_responseMEID->getIntermediates();

        line->atTokStart(&err);
        if (err < 0) goto error;

        tmp = line->atTokNextstr(&err);
        if (err < 0) goto error;

        if (strstr(tmp, "0x") != NULL) {
            identity.meidHex = tmp + 2;
            //logD(RFX_LOG_TAG, "identity.meidHex = %s", identity.meidHex);
        } else {
            identity.meidDec = tmp;
            //logD(RFX_LOG_TAG, "identity.meidDec = %s", identity.meidDec);
        }

        line = p_responseMEID->getIntermediates()->getNext();
        if (line != NULL) {
            line->atTokStart(&err);
            if (err < 0) goto error;

            tmp = line->atTokNextstr(&err);
            if (err < 0) goto error;

            if (strstr(tmp, "0x") != NULL) {
                identity.meidHex = tmp + 2;
                //logD(RFX_LOG_TAG, "identity.meidHex = %s", identity.meidHex);
            } else {
                identity.meidDec = tmp;
                //logD(RFX_LOG_TAG, "identity.meidDec = %s", identity.meidDec);
            }
        }

        if (identity.meidHex != NULL) {
            for (size_t i = 0; i < strlen(identity.meidHex); i++) {
                if (identity.meidHex[i] >= 'a' && identity.meidHex[i] <= 'z') {
                    identity.meidHex[i] -= 32;
                }
            }
        }

        //  Query UIMID
        p_responseUIMID = atSendCommandSingleline("AT+CCID?", "+CCID:");

        if (p_responseUIMID == NULL
                || p_responseUIMID->getError() != 0
                || p_responseUIMID->getSuccess() == 0
                || p_responseUIMID->getIntermediates() == NULL) {
        // goto error;
        identity.uimid = (char*)"0x0";
        } else {
            line = p_responseUIMID->getIntermediates();

            line->atTokStart(&err);
            if (err < 0) goto error;

            identity.uimid = line->atTokNextstr(&err);
            if (err < 0) goto error;
        }
    }
    response = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_SUCCESS,
            RfxStringsData(&identity, 4 * sizeof(char*)), msg, false);
    responseToTelCore(response);
    return;

error:
    logE(RFX_LOG_TAG, "requestDeviceIdentity error");
    response = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_EMPTY_RECORD,
            RfxStringsData(), msg, false);
    responseToTelCore(response);
}

void RmcOemRequestHandler::requestOemHookRaw(const sp<RfxMclMessage>& msg) {
    sp<RfxAtResponse> pResponse;
    RfxAtLine *pAtLine = NULL, *pCur = NULL;
    char* data = (char *) msg->getData()->getData();
    int datalen = msg->getData()->getDataLength();
    char* line;
    int i;
    int strLength = 0;
    int size = -1;
    sp<RfxMclMessage> responseMsg;
    char* buffer = (char *) alloca(datalen+1);
    if (buffer == NULL) {
        logE(RFX_LOG_TAG, "OOM");
        goto error;
    }
    strncpy(buffer, data, datalen);
    buffer[datalen] = '\0';
    logD(RFX_LOG_TAG, "data = %s, length = %d", buffer, datalen);
    pResponse = atSendCommandRaw(buffer);

    if (pResponse->getError() < 0) {
        logE(RFX_LOG_TAG, "OEM_HOOK_RAW fail");
        goto error;
    }
    logD(RFX_LOG_TAG, "success = %d, finalResponse", pResponse->getSuccess(),
            pResponse->getFinalResponse()->getLine());

    strLength += 2; //for the pre tag of the first string in response.

    for (pCur = pResponse->getIntermediates(); pCur != NULL;
        pCur = pCur->getNext()) {
        logD(RFX_LOG_TAG, "pResponse->getIntermediates() = <%s>", pCur->getLine());
        strLength += (strlen(pCur->getLine()) + 2); //M:To append \r\n
    }
    strLength += (strlen(pResponse->getFinalResponse()->getLine()) + 2);
    logD(RFX_LOG_TAG, "strLength = %d", strLength);

    size = strLength * sizeof(char) + 1;
    line = (char *) alloca(size);
    if (line == NULL) {
        logE(RFX_LOG_TAG, "OOM");
        goto error;
    }
    memset(line, 0, size);
    strncpy(line, "\r\n", 2);

    for (i = 0, pCur = pResponse->getIntermediates(); pCur != NULL; pCur = pCur->getNext(), i++) {
       strncat(line, pCur->getLine(), strlen(pCur->getLine()));
       strncat(line, "\r\n", 2);
       logD(RFX_LOG_TAG, "line[%d] = <%s>", i, line);
    }
    strncat(line, pResponse->getFinalResponse()->getLine(),
            strlen(pResponse->getFinalResponse()->getLine()));
    strncat(line, "\r\n", 2);
    logD(RFX_LOG_TAG, "line = <%s>", line);
    responseMsg = RfxMclMessage::obtainResponse(RIL_E_SUCCESS,
            RfxRawData(line, strlen(line)), msg);
    responseToTelCore(responseMsg);
    return;

error:
    line = (char *) alloca(10);
    if (line == NULL) {
        logE(RFX_LOG_TAG, "OOM");
        responseMsg = RfxMclMessage::obtainResponse(RIL_E_GENERIC_FAILURE,
                RfxRawData(), msg);
    } else {
        memset(line, 0, 10);
        strncpy(line, "\r\nERROR\r\n", 9);
        logD(RFX_LOG_TAG, "line = <%s>", line);
        responseMsg = RfxMclMessage::obtainResponse(RIL_E_GENERIC_FAILURE,
                RfxRawData(line, strlen(line)), msg);
    }
    responseToTelCore(responseMsg);

    return;
}

void RmcOemRequestHandler::requestOemHookStrings(const sp<RfxMclMessage>& msg) {
    int i;
    const char ** cur;
    sp<RfxAtResponse> pResponse;
    RfxAtLine *pCur = NULL;
    char** line;
    char **data = (char **) msg->getData()->getData();
    int datalen = msg->getData()->getDataLength();
    int strLength = datalen / sizeof(char *);
    RIL_Errno ret = RIL_E_GENERIC_FAILURE;
    sp<RfxMclMessage> responseMsg;

    logD(RFX_LOG_TAG, "got OEM_HOOK_STRINGS: 0x%8p %lu", data, (long)datalen);

    for (i = strLength, cur = (const char **)data ;
         i > 0 ; cur++, i --) {
            logD(RFX_LOG_TAG, "> '%s'", *cur);
    }


    if (strLength != 2) {
        /* Non proietary. Loopback! */
        responseMsg = RfxMclMessage::obtainResponse(RIL_E_SUCCESS, RfxStringsData((void *)data,
                datalen), msg);
        responseToTelCore(responseMsg);
        return;
    }

    /* For AT command access */
    cur = (const char **)data;

    logD(RFX_LOG_TAG, "OEM_HOOK_STRINGS : receive %s", cur[0]);
    if (NULL != cur[1] && strlen(cur[1]) != 0) {
        /*
        * Response of these two command would not contain prefix. For example,
        * AT+CGSN
        * 490154203237518
        * OK
        * So, RILD should use atSendCommandNumeric to stroe intermediate instead of atSendCommandMultiline
        */
        if ((strncmp(cur[1],"+CIMI",5) == 0) ||(strncmp(cur[1],"+CGSN",5) == 0)) {
            pResponse = atSendCommandNumeric(cur[0]);
        } else {
            pResponse = atSendCommandMultiline(cur[0], cur[1]);
        }
    } else {
        pResponse = atSendCommand(cur[0]);
    }

    if (pResponse->isAtResponseFail()) {
            logE(RFX_LOG_TAG, "OEM_HOOK_STRINGS fail");
            goto error;
    }

    switch (pResponse->atGetCmeError()) {
        case CME_SUCCESS:
            ret = RIL_E_SUCCESS;
            break;
        case CME_INCORRECT_PASSWORD:
            ret = RIL_E_PASSWORD_INCORRECT;
            break;
        case CME_SIM_PIN_REQUIRED:
        case CME_SIM_PUK_REQUIRED:
            ret = RIL_E_PASSWORD_INCORRECT;
            break;
        case CME_SIM_PIN2_REQUIRED:
            ret = RIL_E_SIM_PIN2;
            break;
        case CME_SIM_PUK2_REQUIRED:
            ret = RIL_E_SIM_PUK2;
            break;
        default:
            ret = RIL_E_GENERIC_FAILURE;
            break;
    }

    if (ret != RIL_E_SUCCESS) {
        goto error;
    }

    /* Set the ESSP value stored in NVRAM which configures if
     * to query CFU status by modem itself after first
     * camp-on network */
    if (strncmp(cur[0], "AT+ESSP", 7) == 0) {
        if (cur[1] == NULL) {
            logD(RFX_LOG_TAG, "updateCfuQueryType");
            RmcSuppServRequestHandler::updateCfuQueryType(cur[0]);
        }
    }

    /* Count response length */
    strLength = 0;

    for (pCur = pResponse->getIntermediates(); pCur != NULL;
        pCur = pCur->getNext())
        strLength++;

    if (strLength == 0) {
        responseMsg = RfxMclMessage::obtainResponse(RIL_E_SUCCESS, RfxVoidData(), msg);
    } else {
        logV(RFX_LOG_TAG, "%d of %s received!",strLength, cur[1]);

        line = (char **) alloca(strLength * sizeof(char *));
        if (line == NULL) {
            logE(RFX_LOG_TAG, "OOM");
            goto error;
        }
        for (i = 0, pCur = pResponse->getIntermediates(); pCur != NULL;
                pCur = pCur->getNext(), i++) {
            line[i] = pCur->getLine();
        }
        responseMsg = RfxMclMessage::obtainResponse(RIL_E_SUCCESS,
                RfxStringsData(line, strLength), msg);
    }
    responseToTelCore(responseMsg);
    return;

error:
    responseMsg = RfxMclMessage::obtainResponse(ret, RfxVoidData(), msg);
    responseToTelCore(responseMsg);
}

void RmcOemRequestHandler::requestGetImei() {
    sp<RfxMclMessage> responseMsg;

    sp<RfxAtResponse> pResponse = atSendCommandNumeric("AT+CGSN");
    if (!pResponse->isAtResponseFail()) {
        mImei = String8(pResponse->getIntermediates()->getLine());
        //logD(RFX_LOG_TAG, "imei: %s", mImei.string());
    } else {
        logE(RFX_LOG_TAG, "requestGetImei send at command Fail");
    }
}

void RmcOemRequestHandler::requestGetImei(const sp<RfxMclMessage>& msg) {
    sp<RfxMclMessage> responseMsg;

    if (mImei.isEmpty()) {
        sp<RfxAtResponse> pResponse = atSendCommandNumeric("AT+CGSN");
        if (!pResponse->isAtResponseFail()) {
            mImei = String8(pResponse->getIntermediates()->getLine());
            //logD(RFX_LOG_TAG, "imei: %s", mImei.string());
            responseMsg = RfxMclMessage::obtainResponse(RIL_E_SUCCESS,
                    RfxStringData((void *)mImei.string(), strlen(mImei.string())), msg);
        } else {
            responseMsg = RfxMclMessage::obtainResponse(RIL_E_GENERIC_FAILURE,
                    RfxVoidData(), msg);
            logE(RFX_LOG_TAG, "requestGetImei send at command Fail");
        }

    } else {
        responseMsg = RfxMclMessage::obtainResponse(RIL_E_SUCCESS,
                RfxStringData((void *)mImei.string(), strlen(mImei.string())), msg);
    }
    responseToTelCore(responseMsg);
}

void RmcOemRequestHandler::requestGetImeisv() {
    int err = 0;
    sp<RfxAtResponse> pResponse = atSendCommandSingleline("AT+EGMR=0,9", "+EGMR:");

    if (!pResponse->isAtResponseFail()) {
        char* sv = NULL;
        RfxAtLine* line = pResponse->getIntermediates();
        line->atTokStart(&err);
        if(err >= 0) {
            sv = line->atTokNextstr(&err);
            if(err >= 0) {
                mImeiSv = String8(sv);
                //logD(RFX_LOG_TAG, "imeisv: %s", mImeiSv.string());
            } else {
                logE(RFX_LOG_TAG, "requestGetImeisv atTokNextstr fail");
            }
        } else {
            logE(RFX_LOG_TAG, "requestGetImeisv atTokStart fail");
        }
    } else {
        logE(RFX_LOG_TAG, "requestGetImeisv send AT command fail");
    }
}

void RmcOemRequestHandler::requestGetImeisv(const sp<RfxMclMessage>& msg) {
    int err = 0;
    sp<RfxMclMessage> responseMsg;

    if (mImeiSv.isEmpty()) {
        sp<RfxAtResponse> pResponse = atSendCommandSingleline("AT+EGMR=0,9", "+EGMR:");
        if (!pResponse->isAtResponseFail()) {
            char* sv = NULL;
            RfxAtLine* line = pResponse->getIntermediates();
            line->atTokStart(&err);
            if(err >= 0) {
                sv = line->atTokNextstr(&err);
                if(err >= 0) {
                    mImeiSv = String8(sv);
                    //logD(RFX_LOG_TAG, "imeisv: %s", mImeiSv.string());
                    responseMsg = RfxMclMessage::obtainResponse(RIL_E_SUCCESS,
                            RfxStringData((void *)mImeiSv.string(), strlen(mImeiSv.string())), msg);
                } else {
                    logE(RFX_LOG_TAG, "requestGetImeisv atTokNextstr fail");
                    responseMsg = RfxMclMessage::obtainResponse(RIL_E_GENERIC_FAILURE,
                        RfxVoidData(), msg);
                }
            } else {
                logE(RFX_LOG_TAG, "requestGetImeisv atTokStart fail");
                responseMsg = RfxMclMessage::obtainResponse(RIL_E_GENERIC_FAILURE,
                        RfxVoidData(), msg);
            }
        } else {
            logE(RFX_LOG_TAG, "requestGetImeisv send AT command fail");
            responseMsg = RfxMclMessage::obtainResponse(RIL_E_GENERIC_FAILURE,
                    RfxVoidData(), msg);
        }
    } else {
        responseMsg = RfxMclMessage::obtainResponse(RIL_E_SUCCESS,
                RfxStringData((void *)mImeiSv.string(), strlen(mImeiSv.string())), msg);
    }
    responseToTelCore(responseMsg);
}

void RmcOemRequestHandler::requestGetActivityInfo(const sp<RfxMclMessage>& msg) {
    sp<RfxMclMessage> responseMsg;
    int err;
    RIL_ActivityStatsInfo *activityStatsInfo; // RIL_NUM_TX_POWER_LEVELS 5
    int num_tx_levels = 0;
    int op_code = 0;

    sp<RfxAtResponse> pResponse = atSendCommandSingleline("AT+ERFTX=11", "+ERFTX:");

    if (pResponse == NULL
            || pResponse->getError() != 0
            || pResponse->getSuccess() == 0
            || pResponse->getIntermediates() == NULL) {
        RFX_LOG_E(RFX_LOG_TAG, "requestGetActivityInfo error");
        responseMsg = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_GENERIC_FAILURE,
                RfxVoidData(), msg);
        responseToTelCore(responseMsg);
        return;
    }
    activityStatsInfo = (RIL_ActivityStatsInfo*)calloc(1, sizeof(RIL_ActivityStatsInfo));
    if (activityStatsInfo == NULL) {
        RFX_LOG_E(RFX_LOG_TAG, "OOM");
        return;
    }
    RfxAtLine* line = pResponse->getIntermediates();
    line->atTokStart(&err);
    if (err < 0) goto error;
    op_code = line->atTokNextint(&err);
    if (err < 0 || op_code != 11) goto error;
    num_tx_levels = line->atTokNextint(&err);
    if (err < 0) goto error;
    if (num_tx_levels > RIL_NUM_TX_POWER_LEVELS) {
        RFX_LOG_D(RFX_LOG_TAG, "requestGetActivityInfo TX level invalid (%d)", num_tx_levels);
        goto error;
    }
    for (int i = 0; i < num_tx_levels; i++) {
        activityStatsInfo->tx_mode_time_ms[i] =  line->atTokNextint(&err);
        if (err < 0) goto error;
    }
    activityStatsInfo->rx_mode_time_ms = line->atTokNextint(&err);
    if (err < 0) goto error;
    activityStatsInfo->sleep_mode_time_ms = line->atTokNextint(&err);
    if (err < 0) goto error;
    activityStatsInfo->idle_mode_time_ms = line->atTokNextint(&err);
    if (err < 0) goto error;

    RFX_LOG_D(RFX_LOG_TAG, "requestGetActivityInfo Tx/Rx (%d, %d, %d, %d, %d, %d, %d, %d, %d)",
            num_tx_levels,
            activityStatsInfo->tx_mode_time_ms[0], activityStatsInfo->tx_mode_time_ms[1],
            activityStatsInfo->tx_mode_time_ms[2], activityStatsInfo->tx_mode_time_ms[3],
            activityStatsInfo->tx_mode_time_ms[4], activityStatsInfo->rx_mode_time_ms,
            activityStatsInfo->sleep_mode_time_ms, activityStatsInfo->idle_mode_time_ms);

    responseMsg = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_SUCCESS,
            RfxActivityData((void *)activityStatsInfo, sizeof(RIL_ActivityStatsInfo)), msg);
    responseToTelCore(responseMsg);
    free(activityStatsInfo);
    return;

error:
    RFX_LOG_E(RFX_LOG_TAG, "requestGetActivityInfo error");
    free(activityStatsInfo);
    responseMsg = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_GENERIC_FAILURE,
            RfxVoidData(), msg);
    responseToTelCore(responseMsg);
}

void RmcOemRequestHandler::requestBasebandVersion(const sp<RfxMclMessage>& msg) {
    sp<RfxMclMessage> responseMsg;
    int err, i, len;
    char *ver = NULL;
    char *tmp = NULL;
    sp<RfxAtResponse> pResponse = atSendCommandMultiline("AT+CGMR", "+CGMR:");

    if (pResponse == NULL
            || pResponse->getError() != 0
            || pResponse->getSuccess() == 0) {
        RFX_LOG_E(RFX_LOG_TAG, "requestBasebandVersion error");
        responseMsg = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_GENERIC_FAILURE,
                RfxVoidData(), msg);
        responseToTelCore(responseMsg);
        return;
    }

    if(pResponse->getIntermediates() != NULL) {
        RfxAtLine* line = pResponse->getIntermediates();
        line->atTokStart(&err);
        if(err < 0) goto error;
        ver = line->atTokNextstr(&err);
        if(err < 0) goto error;
    } else {
        RFX_LOG_E(RFX_LOG_TAG,
                "Retry AT+CGMR without expecting +CGMR prefix");
        pResponse = atSendCommandRaw("AT+CGMR");

        if (pResponse == NULL
                || pResponse->getError() != 0
                || pResponse->getSuccess() == 0
                || pResponse->getIntermediates() == NULL) {
            RFX_LOG_E(RFX_LOG_TAG, "requestBasebandVersion error");
            responseMsg = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_GENERIC_FAILURE,
                    RfxVoidData(), msg);
            responseToTelCore(responseMsg);
            return;
        }
        if(pResponse->getIntermediates() != 0) {
            tmp = pResponse->getIntermediates()->getLine();
            len = strlen(tmp);
            while( len > 0 && isspace(tmp[len-1]) )
                len --;
            tmp[len] = '\0';

            //remove the white space from the beginning
            while( (*tmp) != '\0' &&  isspace(*tmp) )
                tmp++;
            ver = tmp;
        }
    }
    //RFX_LOG_E(RFX_LOG_TAG, "ver: %s", ver);
    responseMsg = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_SUCCESS,
            RfxStringData((void *)ver, strlen(ver)), msg);
    responseToTelCore(responseMsg);
    return;

error:
    RFX_LOG_E(RFX_LOG_TAG, "requestBasebandVersion error");
    responseMsg = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_GENERIC_FAILURE,
            RfxVoidData(), msg);
    responseToTelCore(responseMsg);
}

void RmcOemRequestHandler::bootupGetBasebandProject() {
    sp<RfxMclMessage> responseMsg;
    int err;
    char *proj = NULL;
    char *flavor = NULL;
    char *outStr = NULL;
    RfxAtLine *line1 = NULL;
    RfxAtLine *line2 = NULL;
    sp<RfxAtResponse> pResponse = NULL;
    sp<RfxAtResponse> pResponse2 = NULL;
    pResponse = atSendCommandSingleline("AT+EGMR=0,4", "+EGMR:");
    if (pResponse == NULL
            || pResponse->getError() != 0
            || pResponse->getSuccess() == 0
            || pResponse->getIntermediates() == NULL) {
        goto error;
    }
    if(pResponse->getIntermediates() != 0) {
        line1 = pResponse->getIntermediates();
        line1->atTokStart(&err);
        if(err < 0) goto error;
        proj = line1->atTokNextstr(&err);
        if(err < 0) goto error;
    }
    pResponse2 = atSendCommandSingleline("AT+EGMR=0,13", "+EGMR:");
    if (pResponse2 == NULL
            || pResponse2->getError() != 0
            || pResponse2->getSuccess() == 0
            || pResponse2->getIntermediates() == NULL) {
        goto error;
    }
    if(pResponse2->getIntermediates() != 0) {
        line2 = pResponse2->getIntermediates();
        line2->atTokStart(&err);
        if(err < 0) goto error;
        flavor = line2->atTokNextstr(&err);
        if(err < 0) goto error;
    }
    RFX_LOG_E(RFX_LOG_TAG, "proj: %s, flavor: %s", proj, flavor);
    asprintf(&outStr, "%s(%s)",proj ,flavor);
    rfx_property_set("gsm.project.baseband", outStr);
    if (outStr != NULL) {
        free(outStr);
    }
    return;

error:
    RFX_LOG_E(RFX_LOG_TAG, "bootupGetBasebandProject error");
}

void RmcOemRequestHandler::requestQueryThermal(const sp<RfxMclMessage>& msg) {
    sp<RfxAtResponse> pResponse;
    sp<RfxMclMessage> resMsg;
    int err = 0;

    char *data = (char *) msg->getData()->getData();
    if (data != NULL) {
        data[strlen(data)-1] = 0;
    }
    RFX_LOG_I(RFX_LOG_TAG, "requestQueryThermal Enter: %s", data);
    /*
    * thermal service have two action.
    * 1. set threshold at bootup
    *     => thermal service sends command with four parameter. So, AT+ETHERMAL=x,x,x
    * 2. query template of modem
    *     => the second parameter is -1. So, RILD will send "AT+ETHERMAL" to modem
    */
    if(atoi(data) == -1){
        // Enhancement for thermal: Do not query temperature if all radio is off
        int index;
        for (index = 0; index < RFX_SLOT_COUNT; index++) {
            RIL_RadioState radioState = (RIL_RadioState) getMclStatusManager(index)->getIntValue(
                    RFX_STATUS_KEY_RADIO_STATE, 0);
            if (RADIO_STATE_ON == radioState) {
                break;
            }
        }
        if (RFX_SLOT_COUNT == index) {
            RFX_LOG_I(RFX_LOG_TAG, "requestQueryThermal: all radio is off, return error");
            resMsg = RfxMclMessage::obtainResponse(RIL_E_GENERIC_FAILURE,
                    RfxVoidData(), msg);
        } else {
            pResponse = atSendCommandSingleline((char *) "AT+ETHERMAL", (char *) "+ETHERMAL:");
            if (pResponse->getError() < 0 || pResponse->getSuccess() == 0) {
                RFX_LOG_I(RFX_LOG_TAG, "requestQueryThermal error");
                resMsg = RfxMclMessage::obtainResponse(RIL_E_GENERIC_FAILURE,
                        RfxVoidData(), msg);
            } else {
                RFX_LOG_I(RFX_LOG_TAG, "requestQueryThermal success");
                RfxAtLine *line = pResponse->getIntermediates();
                line->atTokStart(&err);
                if (err == 0){
                    resMsg = RfxMclMessage::obtainResponse(RIL_E_SUCCESS,
                            RfxStringData(line->getCurrentLine(), strlen(line->getCurrentLine())),
                            msg);
                } else {
                    RFX_LOG_I(RFX_LOG_TAG, "requestQueryThermal token start error");
                    resMsg = RfxMclMessage::obtainResponse(RIL_E_GENERIC_FAILURE,
                        RfxVoidData(), msg);
                }
            }
        }
    } else {
        pResponse = atSendCommandSingleline(String8::format("AT+ETHERMAL=%s", data).string(),
                (char*) "+ETHERMAL:");
        if (pResponse->getError() < 0 || pResponse->getSuccess() == 0) {
            RFX_LOG_I(RFX_LOG_TAG, "requestQueryThermal error");
            resMsg = RfxMclMessage::obtainResponse(RIL_E_GENERIC_FAILURE,
                    RfxVoidData(), msg);
        } else {
            RFX_LOG_I(RFX_LOG_TAG, "requestQueryThermal success");
            resMsg = RfxMclMessage::obtainResponse(RIL_E_SUCCESS,
                    RfxVoidData(), msg);
        }
    }
    responseToTelCore(resMsg);
}

void RmcOemRequestHandler::requestSetTrm(const sp<RfxMclMessage>& msg) {
    int err = 0;
    int* mode = (int*)(msg->getData()->getData());

    logD(RFX_LOG_TAG, "requestSetTrm: %d", *mode);

    switch (*mode) {
        case 1:
            rfx_property_set("ril.mux.report.case", "1");
            rfx_property_set("ril.muxreport", "1");
            break;
        case 2:
            rfx_property_set("ril.mux.report.case", "2");
            rfx_property_set("ril.muxreport", "1");
            break;
        default:
            break;
    }
    sp<RfxMclMessage> responseMsg = RfxMclMessage::obtainResponse(RIL_E_SUCCESS,
            RfxVoidData(), msg);
    responseToTelCore(responseMsg);
}

/*
    return value for EPCT:
    PS_CONF_TEST_NONE,
    PS_CONF_TEST_CTA,
    PS_CONF_TEST_FTA,
    PS_CONF_TEST_IOT,
    PS_CONF_TEST_OPERATOR,
    PS_CONF_TEST_FACTORY,
    PS_CONF_TEST_END
*/
void RmcOemRequestHandler::requestGetGcfMode() {
    int err = 0;
    int ret = 0;

    sp<RfxAtResponse> pResponse = atSendCommandSingleline(String8::format("AT+EPCT?"),
            (char *) "+EPCT:");

    if (pResponse->getError() < 0 || pResponse->getSuccess() == 0) {
        // assume radio is off
        RFX_LOG_D(RFX_LOG_TAG, "AT+EPCT return ERROR");
        return;
    }

    RfxAtLine *line = pResponse->getIntermediates();

    line->atTokStart(&err);
    if (err < 0) {
        RFX_LOG_D(RFX_LOG_TAG, "AT+EPCT return ERROR");
        return;
    }

    ret = line->atTokNextint(&err);
    if (err < 0){
        RFX_LOG_D(RFX_LOG_TAG, "AT+EPCT return ERROR");
        return;
    }

    rfx_property_set(PROPERTY_GSM_GCF_TEST_MODE, String8::format("%d", ret));
    getNonSlotMclStatusManager()->setIntValue(RFX_STATUS_KEY_GCF_TEST_MODE, ret);

    //RFX_LOG_D(RFX_LOG_TAG, "AT+EPCT return %d", ret);
}

void RmcOemRequestHandler::requestMdVersion() {
    sp<RfxAtResponse> pResponse = atSendCommandMultiline(String8::format("AT+EMDVER?"),
            (char *) "+EMDVER:");
    if (pResponse->getError() < 0 || pResponse->getSuccess() == 0) {
        RFX_LOG_E(RFX_LOG_TAG, "AT+EMDVER? fail");
    }

    // notify RfxVersionManager
    RfxVersionManager::getInstance()->initVersion(pResponse->getIntermediates());
}

void RmcOemRequestHandler::requestSN() {
    int err;
    char *sv;
    // type 5: Serial Number
    sp<RfxAtResponse> pResponse = atSendCommandSingleline(String8::format("AT+EGMR=0,5") ,
            (char *) "+EGMR:");

    if (pResponse->getError() < 0 || pResponse->getSuccess() == 0) {
        RFX_LOG_E(RFX_LOG_TAG, "requestSN fail");
        return;
    }

    RfxAtLine *line = pResponse->getIntermediates();

    line->atTokStart(&err);
    if(err < 0) {
        RFX_LOG_E(RFX_LOG_TAG, "requestSN fail");
        return;
    }

    sv = line->atTokNextstr(&err);
    if(err < 0) {
        RFX_LOG_E(RFX_LOG_TAG, "requestSN fail");
        return;
    }

    property_set(PROPERTY_SERIAL_NUMBER, sv);
    //RFX_LOG_D(RFX_LOG_TAG, "[RIL%d] Get serial number: %s", m_slot_id + 1, sv);
}
