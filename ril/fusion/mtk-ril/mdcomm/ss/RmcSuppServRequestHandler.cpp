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

#include "RmcSuppServRequestHandler.h"
#include "RfxMessageId.h"
#include "GsmUtil.h"
#include "SSUtil.h"
#include "sysenv_utils.h"
#include <telephony/mtk_ril.h>

#include <string.h>
#include <dlfcn.h>

RFX_REGISTER_DATA_TO_REQUEST_ID(RfxStringData,  RfxVoidData, RFX_MSG_REQUEST_SEND_USSD);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData,    RfxVoidData, RFX_MSG_REQUEST_CANCEL_USSD);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData,    RfxVoidData, RFX_MSG_REQUEST_SET_CLIR);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxCallForwardInfoData, RfxVoidData, RFX_MSG_REQUEST_SET_CALL_FORWARD);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxCallForwardInfoExData, RfxVoidData, RFX_MSG_REQUEST_SET_CALL_FORWARD_IN_TIME_SLOT);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData,    RfxVoidData, RFX_MSG_REQUEST_SET_CALL_WAITING);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxStringsData, RfxVoidData, RFX_MSG_REQUEST_CHANGE_BARRING_PASSWORD);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData,    RfxIntsData, RFX_MSG_REQUEST_QUERY_CLIP);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData,    RfxVoidData, RFX_MSG_REQUEST_SET_CLIP);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData,    RfxVoidData, RFX_MSG_REQUEST_SET_SUPP_SVC_NOTIFICATION);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData,    RfxIntsData, RFX_MSG_REQUEST_GET_COLP);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData,    RfxVoidData, RFX_MSG_REQUEST_SET_COLP);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData,    RfxIntsData, RFX_MSG_REQUEST_GET_COLR);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData,    RfxVoidData, RFX_MSG_REQUEST_SET_COLR);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData,    RfxIntsData, RFX_MSG_REQUEST_SEND_CNAP);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxStringsData, RfxVoidData, RFX_MSG_REQUEST_SEND_USSI);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData,    RfxVoidData, RFX_MSG_REQUEST_CANCEL_USSI);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxStringsData, RfxStringsData, RFX_MSG_REQUEST_RUN_GBA);

static const int requests[] = {
    RFX_MSG_REQUEST_SEND_USSD,
    RFX_MSG_REQUEST_CANCEL_USSD,
    RFX_MSG_REQUEST_SET_CLIR,
    RFX_MSG_REQUEST_SET_CALL_FORWARD,
    RFX_MSG_REQUEST_SET_CALL_FORWARD_IN_TIME_SLOT,
    RFX_MSG_REQUEST_SET_CALL_WAITING,
    RFX_MSG_REQUEST_CHANGE_BARRING_PASSWORD,
    RFX_MSG_REQUEST_QUERY_CLIP,
    RFX_MSG_REQUEST_SET_CLIP,
    RFX_MSG_REQUEST_SET_SUPP_SVC_NOTIFICATION,
    RFX_MSG_REQUEST_GET_COLP,
    RFX_MSG_REQUEST_SET_COLP,
    RFX_MSG_REQUEST_GET_COLR,
    RFX_MSG_REQUEST_SET_COLR,
    RFX_MSG_REQUEST_SEND_CNAP,
    RFX_MSG_REQUEST_SEND_USSI,
    RFX_MSG_REQUEST_CANCEL_USSI,
    RFX_MSG_REQUEST_RUN_GBA
};

// register handler to channel
RFX_IMPLEMENT_HANDLER_CLASS(RmcSuppServRequestHandler, RIL_CMD_PROXY_6);

RmcSuppServRequestHandler::RmcSuppServRequestHandler(int slot_id,
        int channel_id):RmcSuppServRequestBaseHandler(slot_id, channel_id){
    registerToHandleRequest(requests, sizeof(requests)/sizeof(int));

    if (RfxRilUtils::getRilRunMode() != RIL_RUN_MODE_MOCK) {
        if (slot_id == 0) {
            // Compatible with IMS repo utinterface binary
            if (startUtInterface("libutinterface_md.so") == NULL) {
                startUtInterface("libutinterface.so");
            }
        }
    }

    /*  +CSSU unsolicited supp service notifications */
    atSendCommand("AT+CSSN=1,1");

    /*  connected line identification on */
    atSendCommand("AT+COLP=1");

    /*  USSD unsolicited */
    atSendCommand("AT+CUSD=1");

    /*  Enable USSI URC */
    atSendCommand("AT+EIUSD=2,4,1,\"\",\"\",0");

    atSendCommand("AT+CLIP=1");

    atSendCommand("AT+CNAP=1");

    /* always not query for CFU status by modem itself after first camp-on network */
    atSendCommand("AT+ESSP=1");

    /* HEX character set */
    atSendCommand("AT+CSCS=\"UCS2\"");
}

RmcSuppServRequestHandler::~RmcSuppServRequestHandler() {
}

void RmcSuppServRequestHandler::onHandleRequest(const sp<RfxMclMessage>& msg) {
    logD(TAG, "onHandleRequest: %d", msg->getId());
    int request = msg->getId();
    switch(request) {
        case RFX_MSG_REQUEST_SEND_USSD:
            requestSendUSSD(msg);
            break;

        case RFX_MSG_REQUEST_CANCEL_USSD:
            requestCancelUssd(msg);
            break;

        case RFX_MSG_REQUEST_SET_CLIR:
            requestSetClir(msg);
            break;

        case RFX_MSG_REQUEST_SET_CALL_FORWARD:
            requestSetCallForward(msg);
            break;

        case RFX_MSG_REQUEST_SET_CALL_FORWARD_IN_TIME_SLOT:
            requestSetCallForwardInTimeSlot(msg);
            break;

        case RFX_MSG_REQUEST_SET_CALL_WAITING:
            requestSetCallWaiting(msg);
            break;

        case RFX_MSG_REQUEST_CHANGE_BARRING_PASSWORD:
            requestChangeBarringPassword(msg);
            break;

        case RFX_MSG_REQUEST_QUERY_CLIP:
            requestQueryClip(msg);
            break;

        case RFX_MSG_REQUEST_SET_CLIP:
            requestSetClip(msg);
            break;

        case RFX_MSG_REQUEST_GET_COLP:
            requestGetColp(msg);
            break;

        case RFX_MSG_REQUEST_SET_COLP:
            requestSetColp(msg);
            break;

        case RFX_MSG_REQUEST_GET_COLR:
            requestGetColr(msg);
            break;

        case RFX_MSG_REQUEST_SET_COLR:
            requestSetColr(msg);
            break;

        case RFX_MSG_REQUEST_SEND_CNAP:
            ///M: For query CNAP
            requestSendCNAP(msg);
            break;

        case RFX_MSG_REQUEST_SEND_USSI:
            requestSendUSSI(msg);
            break;

        case RFX_MSG_REQUEST_CANCEL_USSI:
            requestCancelUssi(msg);
            break;

        case RFX_MSG_REQUEST_RUN_GBA:
            requestRunGBA(msg);
            break;

        case RFX_MSG_REQUEST_SET_SUPP_SVC_NOTIFICATION:
            requestSetSuppSvcNotification(msg);
            break;

        default:
            logE(TAG, "Should not be here");
            break;
    }
}

void RmcSuppServRequestHandler::onHandleTimer() {
    // do something
}

void RmcSuppServRequestHandler::requestSetClir(const sp<RfxMclMessage>& msg) {
    requestClirOperation(msg);
}

void RmcSuppServRequestHandler::requestSetCallForward(const sp<RfxMclMessage>& msg) {
    requestCallForwardOperation(msg, CCFC_E_SET);
}

void RmcSuppServRequestHandler::requestSetCallForwardInTimeSlot(const sp<RfxMclMessage>& msg) {
    requestCallForwardExOperation(msg, CCFC_E_SET);
}

void RmcSuppServRequestHandler::requestSetCallWaiting(const sp<RfxMclMessage>& msg) {
    requestCallWaitingOperation(msg);
}

void RmcSuppServRequestHandler::requestChangeBarringPassword(const sp<RfxMclMessage>& msg) {
    const char** strings = (const char**) (msg->getData()->getData());
    sp<RfxAtResponse> p_response;
    int err;
    char* cmd = NULL;
    RIL_Errno ret = RIL_E_INTERNAL_ERR;

    /**
     * "data" is const char **
     *
     * ((const char **)data)[0] = facility string code from TS 27.007 7.4 (eg "AO" for BAOC)
     * ((const char **)data)[1] = old password
     * ((const char **)data)[2] = new password
     * ((const char **)data)[3] = new password confirmed
     */
    if (msg->getData()->getDataLength() == 3 * sizeof(char*)) {
        if (strings[0] == NULL || strlen(strings[0]) == 0 ||
            strings[1] == NULL || strlen(strings[1]) == 0 ||
            strings[2] == NULL || strlen(strings[2]) == 0) {
            RLOGE("ChangeBarringPassword: Null parameters.");
            ret = RIL_E_INVALID_ARGUMENTS;
            goto error;
        }
        asprintf(&cmd, "AT+ECUSD=1,1,\"**03*%s*%s*%s*%s#\"", callBarFacToServiceCodeStrings(strings[0]), strings[1], strings[2], strings[2]);
    } else if (msg->getData()->getDataLength() == 4 * sizeof(char*)) {
        if (strings[0] == NULL || strlen(strings[0]) == 0 ||
            strings[1] == NULL || strlen(strings[1]) == 0 ||
            strings[2] == NULL || strlen(strings[2]) == 0 ||
            strings[3] == NULL || strlen(strings[3]) == 0) {
            RLOGE("ChangeBarringPassword: Null parameters.");
            ret = RIL_E_INVALID_ARGUMENTS;
            goto error;
        }
        asprintf(&cmd, "AT+ECUSD=1,1,\"**03*%s*%s*%s*%s#\"", callBarFacToServiceCodeStrings(strings[0]), strings[1], strings[2], strings[3]);
    } else {
        goto error;
    }

    p_response = atSendCommand(cmd);

    free(cmd);

    err = p_response->getError();
    if (err < 0 || p_response == NULL) {
        logE(TAG, "requestChangeBarringPassword Fail");
        goto error;
    }

    switch (p_response->atGetCmeError()) {
        case CME_SUCCESS:
            ret = RIL_E_SUCCESS;
            break;
        case CME_INCORRECT_PASSWORD:
            ret = RIL_E_PASSWORD_INCORRECT;
            break;
        case CME_CALL_BARRED:
        case CME_OPR_DTR_BARRING:
            ret = RIL_E_CALL_BARRED;
            break;
        case CME_PHB_FDN_BLOCKED:
            ret = RIL_E_FDN_CHECK_FAILURE;
            break;
        default:
            break;
    }

error:
    sp<RfxMclMessage> response = RfxMclMessage::obtainResponse(msg->getId(), ret,
            RfxVoidData(), msg, false);

    // response to TeleCore
    responseToTelCore(response);
}

void RmcSuppServRequestHandler::requestSendUSSD(const sp<RfxMclMessage>& msg) {
    const char* p_ussdRequest = convertToUCS2((char*) msg->getData()->getData());
    sp<RfxAtResponse> p_response;
    int err;
    char* cmd = NULL;
    RIL_Errno ret = RIL_E_INTERNAL_ERR;
    int strLen = 0;
    char* pTmpStr = NULL;

    if (p_ussdRequest == NULL) {
        logE(TAG, "requestSendUSSD:p_ussdRequest malloc fail");
        goto error;
    }

    /**
     * AT+ECUSD=<m>,<n>,<str>,<dcs>
     * <m>: 1 for SS, 2 for USSD
     * <n>: 1 for execute SS or USSD, 2 for cancel USSD session
     * <str>: string type parameter, the SS or USSD string
     */

    /**
     * 01xx    General Data Coding indication
     *
     * Bits 5..0 indicate the following:
     *   Bit 5, if set to 0, indicates the text is uncompressed
     *   Bit 5, if set to 1, indicates the text is compressed using the compression algorithm defined in 3GPP TS 23.042 [13]
     *
     *   Bit 4, if set to 0, indicates that bits 1 to 0 are reserved and have no message class meaning
     *   Bit 4, if set to 1, indicates that bits 1 to 0 have a message class meaning:
     *
     *     Bit 1   Bit 0       Message Class:
     *       0       0           Class 0
     *       0       1           Class 1 Default meaning: ME-specific.
     *       1       0           Class 2 (U)SIM specific message.
     *       1       1           Class 3 Default meaning: TE-specific (see 3GPP TS 27.005 [8])
     *
     *   Bits 3 and 2 indicate the character set being used, as follows:
     *
     *     Bit 3   Bit 2       Character set:
     *       0       0           GSM 7 bit default alphabet
     *       0       1           8 bit data
     *       1       0           UCS2 (16 bit) [10]
     *       1       1           Reserved
     */
    //BEGIN mtk08470 [20130109][ALPS00436983]
    // USSD string cannot more than MAX_RIL_USSD_NUMBER_LENGTH digits
    // We convert input char to unicode hex string and store it to p_ussdRequest.
    // For example, convert input "1" to "3100"; So len of p_ussdRequest is 4 times of input
    strLen = strlen(p_ussdRequest)/4;
    if (strLen > MAX_RIL_USSD_NUMBER_LENGTH) {
        logW(TAG, "USSD stringlen = %d, max = %d", strLen, MAX_RIL_USSD_NUMBER_LENGTH);
        strLen = MAX_RIL_USSD_NUMBER_LENGTH;
    }
    pTmpStr = (char*) calloc(1, (4*strLen+1));
    if(pTmpStr == NULL) {
        logE(TAG, "Malloc fail");
        free((char *)p_ussdRequest);
        goto error;
    }
    memcpy(pTmpStr, p_ussdRequest, 4*strLen);
    //END mtk08470 [20130109][ALPS00436983]
    asprintf(&cmd, "AT+ECUSD=2,1,\"%s\",72", pTmpStr); /* <dcs> = 0x48 */

    p_response = atSendCommand(cmd);

    free(cmd);
    free(pTmpStr);
    free((char *)p_ussdRequest);

    err = p_response->getError();
    if (err < 0 || p_response == NULL) {
        logE(TAG, "requestSendUSSD Fail");
        goto error;
    }

    switch (p_response->atGetCmeError()) {
        case CME_SUCCESS:
            ret = RIL_E_SUCCESS;
            break;
        case CME_CALL_BARRED:
        case CME_OPR_DTR_BARRING:
            ret = RIL_E_CALL_BARRED;
            break;
        case CME_PHB_FDN_BLOCKED:
            ret = RIL_E_FDN_CHECK_FAILURE;
            break;
        default:
            atSendCommand("AT+ECUSD=2,2");
            break;
    }

error:
    sp<RfxMclMessage> response = RfxMclMessage::obtainResponse(msg->getId(), ret,
            RfxVoidData(), msg, false);

    // response to TeleCore
    responseToTelCore(response);
}

void RmcSuppServRequestHandler::requestCancelUssd(const sp<RfxMclMessage>& msg) {
    sp<RfxAtResponse> p_response;
    int err;
    RIL_Errno ret = RIL_E_INTERNAL_ERR;

    /**
     * AT+ECUSD=<m>,<n>,<str>
     * <m>: 1 for SS, 2 for USSD
     * <n>: 1 for execute SS or USSD, 2 for cancel USSD session
     * <str>: string type parameter, the SS or USSD string
     */

    p_response = atSendCommand("AT+ECUSD=2,2");
    err = p_response->getError();

    if (err < 0 || p_response->getSuccess() == 0) {
        logD(TAG, "Cancel USSD failed.");
    } else {
        ret = RIL_E_SUCCESS;
    }

    sp<RfxMclMessage> response = RfxMclMessage::obtainResponse(msg->getId(), ret,
            RfxVoidData(), msg, false);

    // response to TeleCore
    responseToTelCore(response);
}


void RmcSuppServRequestHandler::requestGetColp(const sp<RfxMclMessage>& msg) {
    requestColpOperation(msg);
}

void RmcSuppServRequestHandler::requestSetColp(const sp<RfxMclMessage>& msg) {
    requestColpOperation(msg);
}

void RmcSuppServRequestHandler::requestGetColr(const sp<RfxMclMessage>& msg) {
    requestColrOperation(msg);
}

void RmcSuppServRequestHandler::requestSetColr(const sp<RfxMclMessage>& msg) {
    requestColrOperation(msg);
}

/**
 * This command refers to the supplementary service CNAP (Calling Name Presentation)
 * according to 3GPP TS 22.096 that enables a called subscriber to get a calling name
 * indication (CNI) of the calling party when receiving a mobile terminated call.
 *
 * Set command enables or disables the presentation of the CNI at the TE.
 * It has no effect on the execution of the supplementary service CNAP in the network.
 * When <n>=1, the presentation of the calling name indication at the TE is enabled and
 * CNI is provided the unsolicited result code. Read command gives the status of<n>,
 * and also triggers an interrogation of the provision status of the CNAP service
 * according 3GPP TS 22.096 (given in <m>). Test command returns values supported
 * as a compound value.
 */
void RmcSuppServRequestHandler::requestSendCNAP(const sp<RfxMclMessage>& msg) {
    sp<RfxAtResponse> p_response;
    int err;
    RfxAtLine *line;
    RIL_Errno ret = RIL_E_GENERIC_FAILURE;
    int responses[2]={0};

    /**
     * AT+ECUSD=<m>,<n>,<str>
     * <m>: 1 for SS, 2 for USSD
     * <n>: 1 for execute SS or USSD, 2 for cancel USSD session
     * <str>: string type parameter, the SS or USSD string
     */
    p_response = atSendCommandSingleline("AT+ECUSD=1,1,\"*#300#\"", "+CNAP:");

    err = p_response->getError();
    if (err < 0 || p_response == NULL) {
       logE(TAG, "requestSendCNAP Fail");
       goto error;
    }

    switch (p_response->atGetCmeError()) {
       case CME_SUCCESS:
          break;
       case CME_CALL_BARRED:
       case CME_OPR_DTR_BARRING:
          ret = RIL_E_CALL_BARRED;
          break;
       case CME_PHB_FDN_BLOCKED:
          ret = RIL_E_FDN_CHECK_FAILURE;
          break;
       default:
          goto error;
    }

    if (p_response->getIntermediates() != NULL) {
        line = p_response->getIntermediates();
        line->atTokStart(&err);
        if (err < 0) {
            goto error;
        }

        /**
         * <n> integer type (parameter sets/shows the result code presentation status to the TE)
         * 0   disable
         * 1   enable
         */
        responses[0] = line->atTokNextint(&err);
        if (err < 0) {
            goto error;
        }

        /**
         * <m> integer type (parameter shows the subscriber CNAP service status in the network)
         * 0   CNAP not provisioned
         * 1   CNAP provisioned
         * 2   unknown (e.g. no network, etc.)
         */
        responses[1] = line->atTokNextint(&err);
        if (err < 0) {
            goto error;
        }
    }

    /* return success here */
    ret = RIL_E_SUCCESS;

error:
    sp<RfxMclMessage> response = RfxMclMessage::obtainResponse(msg->getId(), ret,
            RfxIntsData(responses, sizeof(responses)/sizeof(int)), msg, false);

    // response to TeleCore
    responseToTelCore(response);
}

void RmcSuppServRequestHandler::requestQueryClip(const sp<RfxMclMessage>& msg) {
    sp<RfxAtResponse> p_response;
    int err;
    RfxAtLine *line;
    RIL_Errno ret = RIL_E_INTERNAL_ERR;
    int responses[2]={0};

    /**
     * AT+ECUSD=<m>,<n>,<str>
     * <m>: 1 for SS, 2 for USSD
     * <n>: 1 for execute SS or USSD, 2 for cancel USSD session
     * <str>: string type parameter, the SS or USSD string
     */
    p_response = atSendCommandSingleline("AT+ECUSD=1,1,\"*#30#\"", "+CLIP:");

    err = p_response->getError();
    if (err < 0 || p_response == NULL) {
        logE(TAG, "requestQueryClip Fail");
        goto error;
    }


    switch (p_response->atGetCmeError()) {
        case CME_SUCCESS:
            break;
        case CME_CALL_BARRED:
        case CME_OPR_DTR_BARRING:
            ret = RIL_E_CALL_BARRED;
            goto error;
            break;
        case CME_PHB_FDN_BLOCKED:
            ret = RIL_E_FDN_CHECK_FAILURE;
            goto error;
            break;
        case CME_403_FORBIDDEN:
            ret = RIL_E_UT_XCAP_403_FORBIDDEN;
            goto error;
            break;
        case CME_404_NOT_FOUND:
            ret = RIL_E_404_NOT_FOUND;
            goto error;
            break;
        case CME_409_CONFLICT:
            ret = RIL_E_409_CONFLICT;
            goto error;
            break;
        case CME_412_PRECONDITION_FAILED:
            ret = RIL_E_412_PRECONDITION_FAILED;
            goto error;
            break;
        case CME_NETWORK_TIMEOUT:
            ret = RIL_E_UT_UNKNOWN_HOST;
            goto error;
            break;
        default:
            goto error;
    }

    if ( p_response->getIntermediates() != NULL ) {
        line = p_response->getIntermediates();
        line->atTokStart(&err);
        if (err < 0) {
            goto error;
        }

        /**
         * <n> (parameter sets/shows the result code presentation status in the MT/TA):
         * 0   disable
         * 1   enable
         */
        responses[0] = line->atTokNextint(&err);
        if (err < 0) {
            goto error;
        }

        /**
         * <m> (parameter shows the subscriber CLIP service status in the network):
         * 0   CLIP not provisioned
         * 1   CLIP provisioned
         * 2   unknown (e.g. no network, etc.)
         */
        responses[1] = line->atTokNextint(&err);
        if (err < 0) {
            goto error;
        }
    }

    /* return success here */
    ret = RIL_E_SUCCESS;

error:
    sp<RfxMclMessage> response = RfxMclMessage::obtainResponse(msg->getId(), ret,
            RfxIntsData(&responses[1], sizeof(responses[1])/sizeof(int)), msg, false);

    // response to TeleCore
    responseToTelCore(response);
}

void RmcSuppServRequestHandler::requestSetClip(const sp<RfxMclMessage>& msg) {
    int *n = (int *) (msg->getData()->getData());
    sp<RfxAtResponse> p_response = NULL;
    int err = 0; //Coverity, follow the err init value in at_send_command_full_nolock().
    char* cmd = NULL;
    RfxAtLine *line;
    RIL_Errno ret = RIL_E_GENERIC_FAILURE;
    int responses[2]={0};

    if (msg->getData()->getDataLength() != 0) {
        /**
         * Set CLIP: +CLIP=[<n>]
         * "data" is int *
         * ((int *)data)[0] is "n" parameter from TS 27.007 7.6
         *  <n> (Set command enables or disables the presentation of the CLI at the TE)
         */
        asprintf(&cmd, "AT+CLIP=%d", n[0]);

        // p_response = atSendCommand(cmd);
        p_response = atSendCommandMultiline(cmd, "+CLIP:");
        err = p_response->getError();

        free(cmd);
    }

    if (err < 0 || p_response == NULL) {
        logE(TAG, "requestSetClip Fail");
        goto error;
    }

    switch (p_response->atGetCmeError()) {
        case CME_SUCCESS:
            break;
        case CME_CALL_BARRED:
        case CME_OPR_DTR_BARRING:
            ret = RIL_E_CALL_BARRED;
            goto error;
            break;
        case CME_PHB_FDN_BLOCKED:
            ret = RIL_E_FDN_CHECK_FAILURE;
            goto error;
            break;
        case CME_403_FORBIDDEN:
            ret = RIL_E_UT_XCAP_403_FORBIDDEN;
            goto error;
            break;
        case CME_404_NOT_FOUND:
            ret = RIL_E_404_NOT_FOUND;
            goto error;
            break;
        case CME_409_CONFLICT: {
            ret = RIL_E_409_CONFLICT;
            char *errorMsg = parseErrorMessageFromXCAP(p_response);
            if (errorMsg != NULL) {
                setErrorMessageFromXcap(m_slot_id, 409, errorMsg);
            }
            goto error;
            break;
        }
        case CME_412_PRECONDITION_FAILED:
            ret = RIL_E_412_PRECONDITION_FAILED;
            goto error;
            break;
        case CME_NETWORK_TIMEOUT:
            ret = RIL_E_UT_UNKNOWN_HOST;
            goto error;
            break;
        default:
            goto error;
    }

    /* return success here */
    ret = RIL_E_SUCCESS;

error:
    /* For SET CLIP responseVoid will ignore the responses */
    sp<RfxMclMessage> response = RfxMclMessage::obtainResponse(msg->getId(), ret,
            RfxIntsData(responses, sizeof(responses) / sizeof(int)), msg, false);

    // response to TeleCore
    responseToTelCore(response);
}

void RmcSuppServRequestHandler::requestSendUSSI(const sp<RfxMclMessage>& msg) {

    const char** strings = (const char**) (msg->getData()->getData());
    sp<RfxAtResponse> p_response;
    int action = atoi(strings[0]);
    const char* ussi = strings[1];
    logD(TAG, "requestSendUSSI: action=%d, ussi=%s", action, ussi);

    const char* p_ussdRequest = convertToUCS2((char*)ussi);

    int err;
    RIL_Errno ret = RIL_E_GENERIC_FAILURE;
    int strLen = 0;
    char* pTmpStr = NULL;

    if (p_ussdRequest == NULL) {
        logE(TAG, "requestSendUSSI:p_ussdRequest malloc fail");
        goto error;
    }

    strLen = strlen(p_ussdRequest)/4;
    if (strLen > MAX_RIL_USSD_NUMBER_LENGTH) {
        logW(TAG, "USSI stringlen = %d, max = %d", strLen, MAX_RIL_USSD_NUMBER_LENGTH);
        strLen = MAX_RIL_USSD_NUMBER_LENGTH;
    }
    pTmpStr = (char*) calloc(1, (4*strLen+1));
    if(pTmpStr == NULL) {
        logE(TAG, "Malloc fail");
        free((char *)p_ussdRequest);
        goto error;
    }
    memcpy(pTmpStr, p_ussdRequest, 4*strLen);

    if (isSimulateUSSI()) {
        logD(TAG, "Simulate USSI by CS");
        p_response = atSendCommand(String8::format("AT+ECUSD=2,1,\"%s\",72", pTmpStr));
    } else {
        p_response = atSendCommand(String8::format("AT+EIUSD=2,1,%d,\"%s\",\"en\",0",
                action, ussi));
    }

    free(pTmpStr);
    free((char *)p_ussdRequest);

    err = p_response->getError();
    if (err < 0 || p_response == NULL) {
        logE(TAG, "requestSendUSSI Fail");
        goto error;
    }

    switch (p_response->atGetCmeError()) {
        case CME_SUCCESS:
            ret = RIL_E_SUCCESS;
            break;
        case CME_CALL_BARRED:
        case CME_OPR_DTR_BARRING:
            ret = RIL_E_CALL_BARRED;
            break;
        case CME_PHB_FDN_BLOCKED:
            ret = RIL_E_FDN_CHECK_FAILURE;
            break;
        default:
            break;
    }

error:
    sp<RfxMclMessage> response = RfxMclMessage::obtainResponse(msg->getId(), ret,
            RfxVoidData(), msg, false);

    // response to TeleCore
    responseToTelCore(response);
}

void RmcSuppServRequestHandler::requestCancelUssi(const sp<RfxMclMessage>& msg) {

    sp<RfxAtResponse> p_response;
    int err;
    RIL_Errno ret = RIL_E_GENERIC_FAILURE;

    if (isSimulateUSSI()) {
        p_response = atSendCommand("AT+ECUSD=2,2");
    } else {
        p_response = atSendCommand("AT+EIUSD=2,2,2,\"\",\"en\",0");
    }

    err = p_response->getError();

    if (err < 0 || p_response->getSuccess() == 0) {
        logD(TAG, "Cancel USSD failed.");
    } else {
        ret = RIL_E_SUCCESS;
    }

    sp<RfxMclMessage> response = RfxMclMessage::obtainResponse(msg->getId(), ret,
            RfxVoidData(), msg, false);

    // response to TeleCore
    responseToTelCore(response);
}

void RmcSuppServRequestHandler::updateCfuQueryType(const char *cmd) {
    char *value = NULL;
    const char *prop_name = "persist.radio.cfu.querytype";

    /**
    * <mode> integer type
    * 0  default mode, query when sim replaced
    * 1  always not query
    * 2  always query
    */
    asprintf(&value, "%s", (cmd+8));

    RFX_LOG_D(TAG, "updateCfuQueryType: set [persist.radio.cfu.querytype]: [%s]", value);
    rfx_property_set(prop_name, value);

    RFX_LOG_D(TAG, "updateCfuQueryType: write /proc/lk_env");
    sysenv_set(prop_name, value);

    free(value);
}

void RmcSuppServRequestHandler::requestRunGBA(const sp<RfxMclMessage>& msg) {
    const char** strings = (const char**) (msg->getData()->getData());
    sp<RfxAtResponse> p_response;
    int err;
    char* cmd = NULL;
    RIL_Errno ret = RIL_E_GENERIC_FAILURE;
    RfxAtLine *line;
    char *responses[4] = {NULL, NULL, NULL, NULL};

    logD(TAG, "strings[0]: %s", strings[0]);
    logD(TAG, "strings[1]: %s", strings[1]);
    logD(TAG, "strings[2]: %s", strings[2]);
    logD(TAG, "strings[3]: %s", strings[3]);
    logD(TAG, "length: %d", msg->getData()->getDataLength());

    /**
     * AT+EGBA=Nafqdn,nafSecureProtocolId,bforcerun,netid
     * <Nafqdn>: Nafqdn is a string to indicate GBA key
     * <nafSecureProtocolId>: is a string for GBA protocol
     * <bforcerun>: bforcerun is a string to indicate to force run GBA or using cache.
     *                      0: no need
     *                      1: force run
     * <netid>: is an string for network access
     */

    /* check gba parameters are good or not*/
    if (msg->getData()->getDataLength() != 4 * sizeof(char*)) {
        goto error;
    }

    asprintf(&cmd,"AT+EGBA=%s,%s,%s,%s",
             strings[0], strings[1], strings[2], strings[3]);

    p_response = atSendCommandMultiline(cmd, "+EGBA:");

    free(cmd);

    err = p_response->getError();
    if (err < 0 ||  p_response == NULL) {
        logE(TAG, "requestRunGBA Fail");
        goto error;
    }

    switch (p_response->atGetCmeError()) {
        case CME_SUCCESS:
            ret = RIL_E_SUCCESS;
            break;
        default:
            goto error;
    }

    if (p_response->getIntermediates() != NULL) {
        line = p_response->getIntermediates();
        line->atTokStart(&err);
        if (err < 0) {
            goto error;
        }

        /**
         * <key>GBA key
         */
        responses[0] = line->atTokNextstr(&err);
        if (err < 0) {
            goto error;
        }

        /**
         * <key_length> key length
         */
        responses[1] = line->atTokNextstr(&err);
        if (err < 0) {
            goto error;
        }

        /**
         * <btid> bitId

         */
        responses[2] = line->atTokNextstr(&err);
        if (err < 0) {
            goto error;
        }

        /**
         * <keylifetime> the life time of key
         */
        responses[3] = line->atTokNextstr(&err);
        if (err < 0) {
            goto error;
        }

        logD(TAG, "requestRunGBA: key=%s, key_length=%s, btid=%s, keylifetime=%s",
                responses[0], responses[1],responses[2],responses[3]);
    }

    /* return success here */
    ret = RIL_E_SUCCESS;

error:
    sp<RfxMclMessage> response = RfxMclMessage::obtainResponse(msg->getId(), ret,
            RfxStringsData((void*)responses, sizeof(responses)), msg, false);
    // response to TeleCore
    responseToTelCore(response);
}

void RmcSuppServRequestHandler::requestSetSuppSvcNotification(const sp<RfxMclMessage>& msg) {
    int *n = (int *) (msg->getData()->getData());
    sp<RfxAtResponse> p_response;
    int err;
    char* cmd = NULL;
    RIL_Errno ret = RIL_E_GENERIC_FAILURE;

    //asprintf(&cmd, "AT+ECUSD=1,1,\"%s\",72", p_ussdRequest); /* <dcs> = 0x48 */
    asprintf(&cmd, "AT+CSSN=%d,%d", n[0], n[0]);

    p_response = atSendCommand(cmd);

    free(cmd);

    err = p_response->getError();
    if (err < 0 || p_response == NULL) {
       logE(TAG, "requestSetSuppSvcNotification Fail");
       goto error;
    }

    switch (p_response->atGetCmeError()) {
       case CME_SUCCESS:
          ret = RIL_E_SUCCESS;
          break;
       default:
          break;
    }

error:
    sp<RfxMclMessage> response = RfxMclMessage::obtainResponse(msg->getId(), ret,
            RfxVoidData(), msg, false);

    // response to TeleCore
    responseToTelCore(response);
}

void* RmcSuppServRequestHandler::startUtInterface(const char* libutinterfacePath) {
    char* dllerror;
    void (*fnstartUtInterface)();
    void* hDll;
    logD(TAG, "startUtInterface(): %s", libutinterfacePath);

    hDll = dlopen(libutinterfacePath, RTLD_NOW);
    if(hDll) {
        fnstartUtInterface = (void (*)())dlsym(hDll, "startUtInterface");
        if ((dllerror = (char*)dlerror()) != NULL) {
            logE(TAG, "dlerror: %s", dllerror);
        }
        if (fnstartUtInterface == NULL) {
            logE(TAG, "fnstartUtInterface is NULL");
        } else {
            logD(TAG, "call fnstartUtInterface");
            (*fnstartUtInterface)();
        }
    } else {
        logE(TAG, "hDll is NULL");
    }
    return hDll;
}
