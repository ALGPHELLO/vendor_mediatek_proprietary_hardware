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

#include "RmcSuppServRequestBaseHandler.h"
#include "RfxIntsData.h"
#include "RfxVoidData.h"
#include "RfxStringsData.h"
#include "RfxStringData.h"
#include "RfxCallForwardInfoData.h"
#include "RfxCallForwardInfosData.h"
#include "RfxCallForwardInfoExData.h"
#include "RfxCallForwardInfosExData.h"
#include "RfxMessageId.h"
#include "SSUtil.h"
#include "sysenv_utils.h"
#include "RfxRilUtils.h"
#include "rfx_properties.h"
#include <telephony/mtk_ril.h>
#include "telephony/librilutilsmtk.h"

#include <string.h>

#define TAG "RmcSSBaseHandler"



RmcSuppServRequestBaseHandler::RmcSuppServRequestBaseHandler(int slot_id,
        int channel_id):RfxBaseHandler(slot_id, channel_id){
    // do nothing
}

RmcSuppServRequestBaseHandler::~RmcSuppServRequestBaseHandler() {
    // do nothing
}

void RmcSuppServRequestBaseHandler::requestClirOperation(const sp<RfxMclMessage>& msg) {
    sp<RfxAtResponse> p_response;
    int *n = (int *) (msg->getData()->getData());
    int err;
    char* cmd = NULL;
    // char* line = NULL;
    RfxAtLine *line;
    RIL_Errno ret = RIL_E_INTERNAL_ERR;
    int responses[2]={0};

    if (msg->getData()->getDataLength() != 0) {
        asprintf(&cmd, "AT+CLIR=%d", n[0]);

        // p_response = atSendCommand(cmd);
        p_response = atSendCommandMultiline(cmd, "+CLIR:");
        free(cmd);
    } else {

        /**
         * Get CLIR: +CLIR?
         * This action will trigger CLIR interrogation. Need to check FDN so use proprietary command
         */

        /**
         * AT+ECUSD=<m>,<n>,<str>
         * <m>: 1 for SS, 2 for USSD
         * <n>: 1 for execute SS or USSD, 2 for cancel USSD session
         * <str>: string type parameter, the SS or USSD string
         */
        p_response = atSendCommandSingleline("AT+ECUSD=1,1,\"*#31#\"", "+CLIR:");
    }

    err = p_response->getError();

    if (err < 0 || p_response == NULL) {
        logE(TAG, "requestClirOperation Fail");
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
        case CME_832_TERMINAL_BASE_SOLUTION:
            if (msg->getData()->getDataLength() != 0) {
                // set CLIR
                goto error;
            } else {
                // get CLIR
                ret = RIL_E_832_TERMINAL_BASE_SOLUTION;
                goto error;
            }
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
         * <n> parameter sets the adjustment for outgoing calls
         * 0   presentation indicator is used according to the subscription of the CLIR service
         * 1   CLIR invocation
         * 2   CLIR suppression
         */
        responses[0] = line->atTokNextint(&err);
        if (err < 0) {
            goto error;
        }

        /**
         * <m> parameter shows the subscriber CLIR service status in the network
         * 0   CLIR not provisioned
         * 1   CLIR provisioned in permanent mode
         * 2   unknown (e.g. no network, etc.)
         * 3   CLIR temporary mode presentation restricted
         * 4   CLIR temporary mode presentation allowed
         */
        responses[1] = line->atTokNextint(&err);
        if (err < 0) {
            goto error;
        }
    }

    /* return success here */
    ret = RIL_E_SUCCESS;

error:
    /* For SET CLIR responseVoid will ignore the responses */
    sp<RfxMclMessage> response = RfxMclMessage::obtainResponse(msg->getId(), ret,
            RfxIntsData(responses, sizeof(responses) / sizeof(int)), msg, false);

    // response to TeleCore
    responseToTelCore(response);
}

void RmcSuppServRequestBaseHandler::requestCallForwardOperation(const sp<RfxMclMessage>& msg, CallForwardOperationE op) {
    RIL_CallForwardInfo* p_args = (RIL_CallForwardInfo*) (msg->getData()->getData());
    sp<RfxAtResponse> p_response;
    int err;
    char* cmd = NULL;
    char* precmd = NULL;
    RfxAtLine* p_cur = NULL;
    RIL_Errno ret = RIL_E_INTERNAL_ERR;
    RIL_CallForwardInfo ** pp_CfInfoResponses = NULL;
    RIL_CallForwardInfo * p_CfInfoResponse = NULL;
    HasSIFlagE eSiStatus = HAS_NONE;
    char* pStrTmp = NULL;
    int resLength = 0;
    int dnlen = 0;
    int serviceClass = 0;

    /**
     * AT+ECUSD=<m>,<n>,<str>
     * <m>: 1 for SS, 2 for USSD
     * <n>: 1 for execute SS or USSD, 2 for cancel USSD session
     * <str>: string type parameter, the SS or USSD string
     */

    /**
     *                      SC  SIA  SIB  SIC
     * CFU                  21  DN   BS   -
     * CF Busy              67  DN   BS   -
     * CF No Reply          61  DN   BS   T
     * CF Not Reachable     62  DN   BS   -
     * All CF               002 DN   BS   T
     * All conditional CF   004 DN   BS   T
     */

    /**
     * 3GPP 24.082 and 3GPP 24.030
     * Registration         **SC*DN*BS(*T)#
     * Erasure              ##SC**BS#
     * Activation           *SC**BS#
     * Deactivation         #SC**BS#
     * Interrogation        *#SC**BS#
     */

     if ((CCFC_E_QUERY == op) && (p_args->reason >= CF_ALL)) {
        logE(TAG, "CF_ALL & CF_ALLCOND cannot be used in QUERY");
        goto error;
    }

    /* For Query Call Forwarding in O version, RILJ doesn't assign cf.status. The default value of
     * cf.status is 0, which means Deactivation in SsStatusE. We need chaneg it to Interrogation.
     */
    if (CCFC_E_QUERY == op) {
        LOGW("Call Forwarding: change DEACTIVATE to INTERROGATE");
        p_args->status = SS_INTERROGATE;
    }

    if ((p_args->number != NULL) && (p_args->status == SS_ACTIVATE)) {
        logW(TAG, "Call Forwarding: change ACTIVATE to REGISTER");
        p_args->status = SS_REGISTER;
    }

    /* Check Op Code and MMI Service Code */
    asprintf(&cmd,"AT+ECUSD=1,1,\"%s%s",
             ssStatusToOpCodeString((SsStatusE) p_args->status),
             callForwardReasonToServiceCodeString((CallForwardReasonE) p_args->reason));

    precmd = cmd;

    /* Check SIA: Dial number. Only Registration need to pack DN and others are ignored. */
    if ((p_args->number != NULL)
        && ((p_args->status == SS_REGISTER) || (p_args->status == SS_ACTIVATE))) {

        eSiStatus = (HasSIFlagE) (eSiStatus | HAS_SIA);
        dnlen = strlen((const char *) p_args->number);
        //BEGIN mtk08470 [20130109][ALPS00436983]
        // number string cannot more than MAX_RIL_USSD_NUMBER_LENGTH digits
        if (dnlen > MAX_RIL_USSD_NUMBER_LENGTH) {
            logE(TAG, "cur number len = %d, max = %d", dnlen, MAX_RIL_USSD_NUMBER_LENGTH);
            free(precmd);
            goto error;
        }
        //END mtk08470 [20130109][ALPS00436983]
        if ((p_args->toa == TYPE_ADDRESS_INTERNATIONAL) && (strncmp((const char *)p_args->number, "+", 1))) {
            asprintf(&cmd, "%s*+%s", precmd, p_args->number);
            dnlen++;
        } else {
            asprintf(&cmd, "%s*%s", precmd, p_args->number);
        }

        /* Add this check for sensitive log */
        if (checkUserLoad()) {
            logD(TAG, "toa:%d, number: ********, len:%d", p_args->toa, dnlen);
        } else {
            logD(TAG, "toa:%d, number:%s, len:%d", p_args->toa, p_args->number, dnlen);
        }

        free(precmd);
        precmd = cmd;
    } else {
        if ((p_args->number == NULL) && (p_args->status == SS_REGISTER)) {
            logE(TAG, "Call Forwarding Error: Address cannot be NULL in registration!");
            free(cmd);
            goto error;
        }
    }

    /* Check SIB */
    if (p_args->serviceClass != 0) {
        if (eSiStatus == HAS_SIA) {
            asprintf(&cmd, "%s*%s", precmd, InfoClassToMmiBSCodeString((AtInfoClassE) p_args->serviceClass));
        } else {
            asprintf(&cmd, "%s**%s", precmd, InfoClassToMmiBSCodeString((AtInfoClassE) p_args->serviceClass));
        }

        eSiStatus = (HasSIFlagE) (eSiStatus | HAS_SIB);
        serviceClass = p_args->serviceClass;
        logD(TAG, "Reserve serviceClass. serviceClass = %d", serviceClass);
        logD(TAG, "BS code from serviceClass = %s", InfoClassToMmiBSCodeString((AtInfoClassE) serviceClass));

        free(precmd);
        precmd = cmd;
    }

    /* Check SIC: No reply timer */
    /* shall we check CF_ALL and CF_ALLCOND ? In ril.h time is for CF_NORPLY only. */
    if (((p_args->reason == CF_NORPLY) || (p_args->reason == CF_ALL) || (p_args->reason == CF_ALLCOND))
        && (p_args->status == SS_REGISTER || p_args->status == SS_ACTIVATE) && (p_args->timeSeconds!=0)) {

        if (eSiStatus == HAS_NONE) {
            asprintf(&cmd, "%s***%d", precmd, p_args->timeSeconds);
        } else if (eSiStatus == HAS_SIA) {
            asprintf(&cmd, "%s**%d", precmd, p_args->timeSeconds);
        } else {
            asprintf(&cmd, "%s*%d", precmd, p_args->timeSeconds);
        }

        free(precmd);
        precmd = cmd;
    }

    /* Check END */
    asprintf(&cmd, "%s#\"", precmd);

    free(precmd);

    if (CCFC_E_QUERY == op) {

        /**
         * RIL_REQUEST_QUERY_CALL_FORWARD_STATUS
         *
         * "data" is const RIL_CallForwardInfo *
         *
         * "response" is const RIL_CallForwardInfo **
         * "response" points to an array of RIL_CallForwardInfo *'s, one for
         * each distinct registered phone number.
         *
         * For example, if data is forwarded to +18005551212 and voice is forwarded
         * to +18005559999, then two separate RIL_CallForwardInfo's should be returned
         *
         * If, however, both data and voice are forwarded to +18005551212, then
         * a single RIL_CallForwardInfo can be returned with the service class
         * set to "data + voice = 3")
         *
         * Valid errors:
         *  SUCCESS
         *  RADIO_NOT_AVAILABLE
         *  GENERIC_FAILURE
         */

         p_response = atSendCommandMultiline(cmd, "+CCFC:");

    } else {

        /* add DN length */
        if (dnlen != 0) {
            precmd = cmd;
            asprintf(&cmd, "%s,,%d", precmd, dnlen);
            free(precmd);
        }

        /**
         * RIL_REQUEST_SET_CALL_FORWARD
         *
         * Configure call forward rule
         *
         * "data" is const RIL_CallForwardInfo *
         * "response" is NULL
         *
         * Valid errors:
         *  SUCCESS
         *  RADIO_NOT_AVAILABLE
         *  GENERIC_FAILURE
         */

        p_response = atSendCommandMultiline(cmd, "+CCFC:");
    }

    free(cmd);

    err = p_response->getError();
    if (err < 0 ||  p_response == NULL) {
        logE(TAG, "requestCallForwardOperation Fail");
        goto error;
    }

    switch (p_response->atGetCmeError()) {
        case CME_SUCCESS:
            ret = RIL_E_SUCCESS;
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
        case CME_NETWORK_TIMEOUT:
            ret = RIL_E_UT_UNKNOWN_HOST;
            goto error;
            break;
        case CME_403_FORBIDDEN:
            ret = RIL_E_UT_XCAP_403_FORBIDDEN;
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
        default:
            goto error;
    }

    if (CCFC_E_QUERY == op) {
        for (p_cur = p_response->getIntermediates(); p_cur != NULL; p_cur = p_cur->getNext()) {
            resLength++;
        }

        logD(TAG, "%d of +CCFC: received!", resLength);

        pp_CfInfoResponses = (RIL_CallForwardInfo **) alloca(resLength * sizeof(RIL_CallForwardInfo *));
        memset(pp_CfInfoResponses, 0, resLength * sizeof(RIL_CallForwardInfo *));

        resLength = 0; /* reset resLength for decoding */

        for (p_cur = p_response->getIntermediates(); p_cur != NULL; p_cur = p_cur->getNext()) {
            char *line  = NULL;
            int  bsCode = 0;

            line = p_cur->getLine();

            if (line == NULL) {
                logE(TAG, "CCFC: NULL line");
                break;
            }

            if (p_CfInfoResponse == NULL) {
                p_CfInfoResponse = (RIL_CallForwardInfo *) alloca(sizeof(RIL_CallForwardInfo));
                memset(p_CfInfoResponse, 0, sizeof(RIL_CallForwardInfo));
                p_CfInfoResponse->reason = p_args->reason;
            }

            ((RIL_CallForwardInfo   **)pp_CfInfoResponses)[resLength] = p_CfInfoResponse;

            /**
             * For Query CCFC only
             * +CCFC: <status>,<class1>[,<number>,<type>
             * [,<subaddr>,<satype>[,<time>]]]
             */


            p_cur->atTokStart(&err);
            if (err < 0) {
                logE(TAG, "+CCFC: fail");
                continue;
            }

            p_CfInfoResponse->status = p_cur->atTokNextint(&err);
            if (err < 0) {
                logE(TAG, "+CCFC: status fail!");
                //continue;
            }

            bsCode = p_cur->atTokNextint(&err);
            if (err < 0) {
                logE(TAG, "+CCFC: bsCode fail!");
                //continue;
            }

            if (serviceClass != 0 && p_CfInfoResponse->status == 0 && bsCode == 0) {
                p_CfInfoResponse->serviceClass = serviceClass;
            } else {
                p_CfInfoResponse->serviceClass = bsCode;
            }

            if (p_cur->atTokHasmore()) {
                p_CfInfoResponse->number = p_cur-> atTokNextstr(&err);
                if (err < 0) {
                    logE(TAG, "+CCFC: number fail!");
                }

                p_CfInfoResponse->toa = p_cur-> atTokNextint(&err);
                if (err < 0) {
                    logE(TAG, "+CCFC: toa fail!");
                }
            }

            if (p_cur->atTokHasmore()) {
                /* skip subaddr */
                pStrTmp = p_cur-> atTokNextstr(&err);
                if(err < 0) {
                    logE(TAG, "+CCFC: sub fail!");
                }

                /* skip satype */
                p_CfInfoResponse->timeSeconds = p_cur-> atTokNextint(&err);
                if(err < 0) {
                    logE(TAG, "+CCFC: sa type fail!");
                }

                p_CfInfoResponse->timeSeconds = p_cur-> atTokNextint(&err);
                if(err < 0) {
                    logE(TAG, "+CCFC: time fail!");
                }
            }

#ifdef MTK_VT3G324M_SUPPORT
           if (p_CfInfoResponse->serviceClass == CLASS_DATA_SYNC) {
              p_CfInfoResponse->serviceClass = CLASS_MTK_VIDEO;
           }
#endif
            /* Add this check for sensitive log */
            if (checkUserLoad()) {
                logD(TAG, "CfInfoResponse status:%d class:%d num:******** toa:%d time:%d",
                    p_CfInfoResponse->status,
                    p_CfInfoResponse->serviceClass,
                    p_CfInfoResponse->toa,
                    p_CfInfoResponse->timeSeconds);
            } else {
                logD(TAG, "CfInfoResponse status:%d class:%d num:%s toa:%d time:%d",
                    p_CfInfoResponse->status,
                    p_CfInfoResponse->serviceClass,
                    p_CfInfoResponse->number,
                    p_CfInfoResponse->toa,
                    p_CfInfoResponse->timeSeconds);
            }

            p_CfInfoResponse = NULL;
            resLength++;
        }

        logD(TAG, "%d of +CCFC: decoded!", resLength);
    }

error:
    // RIL_onRequestComplete(t, ret, pp_CfInfoResponses, resLength*sizeof(RIL_CallForwardInfo *));
    // at_response_free(p_response);

    sp<RfxMclMessage> response = RfxMclMessage::obtainResponse(msg->getId(), ret,
            RfxCallForwardInfosData(pp_CfInfoResponses, resLength * sizeof(RIL_CallForwardInfo *)), msg, false);

    // response to TeleCore
    responseToTelCore(response);
}

void RmcSuppServRequestBaseHandler::requestCallForwardExOperation(
        const sp<RfxMclMessage>& msg, CallForwardOperationE op) {
    RIL_CallForwardInfoEx* p_args = (RIL_CallForwardInfoEx*) (msg->getData()->getData());
    sp<RfxAtResponse> p_response;
    int err;
    char* cmd = NULL;
    char* precmd = NULL;
    RfxAtLine* p_cur = NULL;
    RIL_Errno ret = RIL_E_GENERIC_FAILURE;
    RIL_CallForwardInfoEx ** pp_CfInfoResponses = NULL;
    RIL_CallForwardInfoEx * p_CfInfoResponse = NULL;
    HasSIFlagE eSiStatus = HAS_NONE;
    char* pStrTmp = NULL;
    int resLength = 0;
    int dnlen = 0;
    int serviceClass = 0;

    logD(TAG, "Enter CallForwardEx");

    /**
     * AT+ECUSD=<m>,<n>,<str>
     * <m>: 1 for SS, 2 for USSD
     * <n>: 1 for execute SS or USSD, 2 for cancel USSD session
     * <str>: string type parameter, the SS or USSD string
     */

    /**
     *                      SC  SIA  SIB  SIC
     * CFU                  21  DN   BS   -
     * CF Busy              67  DN   BS   -
     * CF No Reply          61  DN   BS   T
     * CF Not Reachable     62  DN   BS   -
     * All CF               002 DN   BS   T
     * All conditional CF   004 DN   BS   T
     */

    /**
     * 3GPP 24.082 and 3GPP 24.030
     * Registration         **SC*DN*BS(*T)#
     * Erasure              ##SC**BS#
     * Activation           *SC**BS#
     * Deactivation         #SC**BS#
     * Interrogation        *#SC**BS#
     */

     if ((CCFC_E_QUERY == op) && (p_args->reason >= CF_ALL)) {
        logE(TAG, "CF_ALL & CF_ALLCOND cannot be used in QUERY");
        goto error;
    }

    /* For Query Call Forwarding in O version, RILJ doesn't assign cf.status. The default value of
     * cf.status is 0, which means Deactivation in SsStatusE. We need chaneg it to Interrogation.
     */
     if (CCFC_E_QUERY == op) {
        LOGW("Call Forwarding: change DEACTIVATE to INTERROGATE");
        p_args->status = SS_INTERROGATE;
    }

    if ((p_args->number != NULL) && (p_args->status == SS_ACTIVATE)) {
        logW(TAG, "Call Forwarding: change ACTIVATE to REGISTER");
        p_args->status = SS_REGISTER;
    }

    /* Check Op Code and MMI Service Code */
    asprintf(&cmd,"AT+ECUSD=1,1,\"%s%s",
             ssStatusToOpCodeString((SsStatusE) p_args->status),
             callForwardReasonToServiceCodeString((CallForwardReasonE) p_args->reason));

    precmd = cmd;


    /* Check SIA: Dial number. Only Registration need to pack DN and others are ignored. */
    if ((p_args->number != NULL)
        && ((p_args->status == SS_REGISTER) || (p_args->status == SS_ACTIVATE))) {

        eSiStatus = (HasSIFlagE) (eSiStatus | HAS_SIA);
        dnlen = strlen((const char *) p_args->number);
        //BEGIN mtk08470 [20130109][ALPS00436983]
        // number string cannot more than MAX_RIL_USSD_NUMBER_LENGTH digits
        if (dnlen > MAX_RIL_USSD_NUMBER_LENGTH) {
            logE(TAG, "cur number len = %d, max = %d", dnlen, MAX_RIL_USSD_NUMBER_LENGTH);
            free(precmd);
            goto error;
        }
        //END mtk08470 [20130109][ALPS00436983]
        if ((p_args->toa == TYPE_ADDRESS_INTERNATIONAL) && (strncmp((const char *)p_args->number, "+", 1))) {
            asprintf(&cmd, "%s*+%s", precmd, p_args->number);
            dnlen++;
        } else {
            asprintf(&cmd, "%s*%s", precmd, p_args->number);
        }

        /* Add this check for sensitive log */
        if (checkUserLoad()) {
            logD(TAG, "toa:%d, number:********, len:%d", p_args->toa, dnlen);
        } else {
            logD(TAG, "toa:%d, number:%s, len:%d", p_args->toa, p_args->number, dnlen);
        }

        free(precmd);
        precmd = cmd;
    } else {
        if ((p_args->number == NULL) && (p_args->status == SS_REGISTER)) {
            logE(TAG, "Call Forwarding Error: Address cannot be NULL in registration!");
            free(cmd);
            goto error;
        }
    }

    /* Check SIB */
    if (p_args->serviceClass != 0) {
        if (eSiStatus == HAS_SIA) {
            asprintf(&cmd, "%s*%s", precmd, InfoClassToMmiBSCodeString((AtInfoClassE) p_args->serviceClass));
        } else {
            asprintf(&cmd, "%s**%s", precmd, InfoClassToMmiBSCodeString((AtInfoClassE) p_args->serviceClass));
        }

        eSiStatus = (HasSIFlagE) (eSiStatus | HAS_SIB);
        serviceClass = p_args->serviceClass;
        logD(TAG, "Reserve serviceClass. serviceClass = %d", serviceClass);
        logD(TAG, "BS code from serviceClass = %s", InfoClassToMmiBSCodeString((AtInfoClassE) serviceClass));

        free(precmd);
        precmd = cmd;
    }

    /* Check SIC: No reply timer */
    /* shall we check CF_ALL and CF_ALLCOND ? In ril.h time is for CF_NORPLY only. */
    if (((p_args->reason == CF_NORPLY) || (p_args->reason == CF_ALL) || (p_args->reason == CF_ALLCOND))
        && (p_args->status == SS_REGISTER || p_args->status == SS_ACTIVATE) && (p_args->timeSeconds!=0)) {

        if (eSiStatus == HAS_NONE) {
            asprintf(&cmd, "%s***%d", precmd, p_args->timeSeconds);
        } else if (eSiStatus == HAS_SIA) {
            asprintf(&cmd, "%s**%d", precmd, p_args->timeSeconds);
        } else {
            asprintf(&cmd, "%s*%d", precmd, p_args->timeSeconds);
        }

        free(precmd);
        precmd = cmd;
    }

    /* Check timeSlot */
    if (CCFC_E_QUERY == op) {
        // query time slot
        if (eSiStatus == HAS_NONE) {
            asprintf(&cmd, "%s*****", precmd);
        } else if (eSiStatus == HAS_SIA) {
            asprintf(&cmd, "%s****", precmd);
        } else {
            asprintf(&cmd, "%s***", precmd);
        }

        free(precmd);
        precmd = cmd;

    } else {
        // set
        if (p_args->timeSlotBegin != 0 && p_args->timeSlotEnd != 0) {

            if (eSiStatus == HAS_NONE) {
                asprintf(&cmd, "%s****%s*%s", precmd, p_args->timeSlotBegin, p_args->timeSlotEnd);
            } else if (eSiStatus == HAS_SIA) {
                asprintf(&cmd, "%s***%s*%s", precmd, p_args->timeSlotBegin, p_args->timeSlotEnd);
            } else {
                asprintf(&cmd, "%s**%s*%s", precmd, p_args->timeSlotBegin, p_args->timeSlotEnd);
            }

            free(precmd);
            precmd = cmd;
        }
    }

    /* Check END */
    asprintf(&cmd, "%s#\"", precmd);

    free(precmd);

    if (CCFC_E_QUERY == op) {

        /**
         * RIL_REQUEST_QUERY_CALL_FORWARD_STATUS
         *
         * "data" is const RIL_CallForwardInfoEx *
         *
         * "response" is const RIL_CallForwardInfoEx **
         * "response" points to an array of RIL_CallForwardInfoEx *'s, one for
         * each distinct registered phone number.
         *
         * For example, if data is forwarded to +18005551212 and voice is forwarded
         * to +18005559999, then two separate RIL_CallForwardInfoEx's should be returned
         *
         * If, however, both data and voice are forwarded to +18005551212, then
         * a single RIL_CallForwardInfoEx can be returned with the service class
         * set to "data + voice = 3")
         *
         * Valid errors:
         *  SUCCESS
         *  RADIO_NOT_AVAILABLE
         *  GENERIC_FAILURE
         */

         p_response = atSendCommandMultiline(cmd, "+CCFC:");

    } else {

        /* add DN length */
        if (dnlen != 0) {
            precmd = cmd;
            asprintf(&cmd, "%s,,%d", precmd, dnlen);
            free(precmd);
        }

        /**
         * RIL_REQUEST_SET_CALL_FORWARD
         *
         * Configure call forward rule
         *
         * "data" is const RIL_CallForwardInfoEx *
         * "response" is NULL
         *
         * Valid errors:
         *  SUCCESS
         *  RADIO_NOT_AVAILABLE
         *  GENERIC_FAILURE
         */

         p_response = atSendCommand(cmd);
    }

    free(cmd);

    err = p_response->getError();
    if (err < 0 ||  p_response == NULL) {
        logE(TAG, "requestCallForwardOperationEx Fail");
        goto error;
    }

    logD(TAG, "CallForwardEx err = %d", p_response->atGetCmeError());
    switch (p_response->atGetCmeError()) {
        case CME_SUCCESS:
            ret = RIL_E_SUCCESS;
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
        case CME_NETWORK_TIMEOUT:
            ret = RIL_E_UT_UNKNOWN_HOST;
            goto error;
            break;
        case CME_403_FORBIDDEN:
            ret = RIL_E_UT_XCAP_403_FORBIDDEN;
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
        default:
            goto error;
    }

    if (CCFC_E_QUERY == op) {
        for (p_cur = p_response->getIntermediates(); p_cur != NULL; p_cur = p_cur->getNext()) {
            resLength++;
        }

        logD(TAG, "%d of +CCFC: received!", resLength);

        pp_CfInfoResponses = (RIL_CallForwardInfoEx **) alloca(resLength * sizeof(RIL_CallForwardInfoEx *));
        memset(pp_CfInfoResponses, 0, resLength * sizeof(RIL_CallForwardInfoEx *));

        resLength = 0; /* reset resLength for decoding */

        for (p_cur = p_response->getIntermediates(); p_cur != NULL; p_cur = p_cur->getNext()) {
            char *line  = NULL;
            int  bsCode = 0;

            line = p_cur->getLine();

            if (line == NULL) {
                logE(TAG, "CCFC: NULL line");
                break;
            }

            if (p_CfInfoResponse == NULL) {
                p_CfInfoResponse = (RIL_CallForwardInfoEx *) alloca(sizeof(RIL_CallForwardInfoEx));
                memset(p_CfInfoResponse, 0, sizeof(RIL_CallForwardInfoEx));
                p_CfInfoResponse->reason = p_args->reason;
            }

            ((RIL_CallForwardInfoEx   **)pp_CfInfoResponses)[resLength] = p_CfInfoResponse;

            /**
             * For Query CCFC only
             * +CCFC: <status>,<class1>[,<number>,<type>
             * [,<subaddr>,<satype>[,<time>]]]
             */


            p_cur->atTokStart(&err);
            if (err < 0) {
                logE(TAG, "+CCFC: fail");
                continue;
            }

            p_CfInfoResponse->status = p_cur->atTokNextint(&err);
            if (err < 0) {
                logE(TAG, "+CCFC: status fail!");
                //continue;
            }

            bsCode = p_cur->atTokNextint(&err);
            if (err < 0) {
                logE(TAG, "+CCFC: bsCode fail!");
                //continue;
            }

            if (serviceClass != 0 && p_CfInfoResponse->status == 0 && bsCode == 0) {
                p_CfInfoResponse->serviceClass = serviceClass;
            } else {
                p_CfInfoResponse->serviceClass = bsCode;
            }

            if (p_cur->atTokHasmore()) {
                p_CfInfoResponse->number = p_cur-> atTokNextstr(&err);
                if (err < 0) {
                    logE(TAG, "+CCFC: number fail!");
                }

                p_CfInfoResponse->toa = p_cur-> atTokNextint(&err);
                if (err < 0) {
                    logE(TAG, "+CCFC: toa fail!");
                }
            }

            if (p_cur->atTokHasmore()) {
                /* skip subaddr */
                pStrTmp = p_cur-> atTokNextstr(&err);
                if(err < 0) {
                    logE(TAG, "+CCFC: sub fail!");
                }

                /* skip satype */
                p_CfInfoResponse->timeSeconds = p_cur-> atTokNextint(&err);
                if(err < 0) {
                    logE(TAG, "+CCFC: sa type fail!");
                }

                p_CfInfoResponse->timeSeconds = p_cur-> atTokNextint(&err);
                if(err < 0) {
                    logE(TAG, "+CCFC: time fail!");
                }
            }

            if (p_cur->atTokHasmore()) {
                p_CfInfoResponse->timeSlotBegin = p_cur-> atTokNextstr(&err);
                if (err < 0) {
                    logE(TAG, "+CCFC: timeSlotBegin fail!");
                }

                p_CfInfoResponse->timeSlotEnd = p_cur-> atTokNextstr(&err);
                if (err < 0) {
                    logE(TAG, "+CCFC: timeSlotEnd fail!");
                }
            }

#ifdef MTK_VT3G324M_SUPPORT
           if (p_CfInfoResponse->serviceClass == CLASS_DATA_SYNC) {
              p_CfInfoResponse->serviceClass = CLASS_MTK_VIDEO;
           }
#endif
            /* Add this check for sensitive log */
            if (checkUserLoad()) {
                logD(TAG, "CfInfoResponse status:%d class:%d num:******** toa:%d time:%d begin:%s end:%s",
                    p_CfInfoResponse->status,
                    p_CfInfoResponse->serviceClass,
                    p_CfInfoResponse->toa,
                    p_CfInfoResponse->timeSeconds,
                    p_CfInfoResponse->timeSlotBegin,
                    p_CfInfoResponse->timeSlotEnd);
            } else {
                logD(TAG, "CfInfoResponse status:%d class:%d num:%s toa:%d time:%d begin:%s end:%s",
                    p_CfInfoResponse->status,
                    p_CfInfoResponse->serviceClass,
                    p_CfInfoResponse->number,
                    p_CfInfoResponse->toa,
                    p_CfInfoResponse->timeSeconds,
                    p_CfInfoResponse->timeSlotBegin,
                    p_CfInfoResponse->timeSlotEnd);
            }

            p_CfInfoResponse = NULL;
            resLength++;
        }

        logD(TAG, "%d of +CCFC: decoded!", resLength);
    }

error:
    // RIL_onRequestComplete(t, ret, pp_CfInfoResponses, resLength*sizeof(RIL_CallForwardInfoEx *));
    // at_response_free(p_response);

    sp<RfxMclMessage> response = RfxMclMessage::obtainResponse(msg->getId(), ret,
            RfxCallForwardInfosExData(pp_CfInfoResponses, resLength * sizeof(RIL_CallForwardInfoEx *)), msg, false);

    // response to TeleCore
    responseToTelCore(response);
}


void RmcSuppServRequestBaseHandler::requestCallWaitingOperation(const sp<RfxMclMessage>& msg) {
    sp<RfxAtResponse> p_response;
    int *p_int = (int *) (msg->getData()->getData());
    int err;
    char* cmd = NULL;
    RfxAtLine *p_cur;
    RIL_Errno ret = RIL_E_INTERNAL_ERR;
    int responses[2]={0};
    int resLength = 0;
    int sendBsCode = 0;
    int responseForAll = 0;

    char tbCWStatus[RFX_PROPERTY_VALUE_MAX] = { 0 };

    getMSimProperty(m_slot_id, (char *)PROPERTY_TERMINAL_BASED_CALL_WAITING_MODE, tbCWStatus);

    if (strlen(tbCWStatus) == 0) {
        strncpy(tbCWStatus, TERMINAL_BASED_CALL_WAITING_DISABLED,
                strlen(TERMINAL_BASED_CALL_WAITING_DISABLED));
    }

    if (msg->getData()->getDataLength() == sizeof(int)) {
        sendBsCode = p_int[0];
        logD(TAG, "sendBsCode = %d", sendBsCode);

        /*
           From call settings: sendBsCode = 512(CLASS_MTK_VIDEO)
           From MMI command: sendBsCode = 528(CLASS_MTK_VIDEO + CLASS_DATA_SYNC)
        */

        // Default new SS service class feature is supported.
        if ((sendBsCode == CLASS_MTK_VIDEO) ||
           (sendBsCode == (CLASS_MTK_VIDEO + CLASS_DATA_SYNC))) {
            sendBsCode = CLASS_DATA_SYNC;
        }

        asprintf(&cmd, "AT+ECUSD=1,1,\"*#43#\"");
        p_response = atSendCommandMultiline(cmd, "+CCWA:");
    } else if (msg->getData()->getDataLength() == 2 * sizeof(int)) {
        logD(TAG, "p_int[0] = %d, p_int[1] = %d", p_int[0], p_int[1]);
        if (p_int[1] != 0) {
            /* with InfoClass */
            asprintf(&cmd, "AT+ECUSD=1,1,\"%s43*%s#\"",
                     ssStatusToOpCodeString(SsStatusE(p_int[0])),
                     InfoClassToMmiBSCodeString(AtInfoClassE(p_int[1])));
        } else {
            /* User did not input InfoClass */
            asprintf(&cmd, "AT+ECUSD=1,1,\"%s43#\"",
                     ssStatusToOpCodeString(SsStatusE(p_int[0])));
        }

        // p_response = atSendCommand(cmd);
        p_response = atSendCommandMultiline(cmd, "+CCWA:");
    } else {
        goto error;
    }

    free(cmd);

    err = p_response->getError();
    if (err < 0 || p_response == NULL) {
        logE(TAG, "requestCallWaitingOperation Fail");
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
        case CME_NETWORK_TIMEOUT:
            ret = RIL_E_UT_UNKNOWN_HOST;
            goto error;
            break;
        case CME_403_FORBIDDEN:
            ret = RIL_E_UT_XCAP_403_FORBIDDEN;
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
        case CME_832_TERMINAL_BASE_SOLUTION:
            if (msg->getData()->getDataLength() == 2 * sizeof(int)) {
                // If CW terminal based set and MD return 832, the flag will be eabled @{
                setMSimProperty(m_slot_id,
                            (char *) PROPERTY_TERMINAL_BASED_CALL_WAITING_ENABLE, (char *) "1");

                if(p_int[0] == SS_ACTIVATE || p_int[0] == SS_REGISTER) {
                    logD(TAG, "832 terminal base call waitng enable");
                    setMSimProperty(m_slot_id,
                            (char *)PROPERTY_TERMINAL_BASED_CALL_WAITING_MODE,
                            (char *)TERMINAL_BASED_CALL_WAITING_ENABLED_ON);
                } else if(p_int[0] == SS_DEACTIVATE || p_int[0] == SS_ERASE) {
                    logD(TAG, "832 terminal base call waitng disable");
                    setMSimProperty(m_slot_id,
                            (char *)PROPERTY_TERMINAL_BASED_CALL_WAITING_MODE,
                            (char *)TERMINAL_BASED_CALL_WAITING_ENABLED_OFF);
                }
            }
            break;
        default:
            goto error;
    }

    if (p_response->getIntermediates() != NULL) {
        for (p_cur = p_response->getIntermediates(); p_cur != NULL; p_cur = p_cur->getNext()) {
            resLength++;
        }

        logD(TAG, "%d of +CCWA: received!", resLength);

        resLength = 0; /* reset resLength for decoding */

        // Default new SS service class feature is supported in
        responses[1] = 0;

        int tmpEnable = 0;   // Keep the enable status for temporary. If the service class from
                             // network responses is match from AP, than assign the status to the
                             // responses[0].

        bool find = false;   // Check the responses from network are with the service class which
                             // is AP needed.

        for (p_cur = p_response->getIntermediates(); p_cur != NULL; p_cur = p_cur->getNext()) {
            char *line  = NULL;
            int  bsCode = 0;

            line = p_cur->getLine();

            if (line == NULL) {
                logE(TAG, "CCWA: NULL line");
                break;
            }

            p_cur->atTokStart(&err);
            if (err < 0) {
                goto error;
            }

            /**
             * <status>
             * 0   not active
             * 1   active
            */
            tmpEnable = p_cur->atTokNextint(&err);
            if (err < 0) {
                goto error;
            }

            /**
             * <classx> is a sum of integers each representing a class of information (default 7):
             * 1   voice (telephony)
             * 2   data (refers to all bearer services; with <mode>=2 this may refer only
             *     to some bearer service if TA does not support values 16, 32, 64 and 128)
             * 4   fax (facsimile services)
             * 8   short message service
             * 16  data circuit sync
             * 32  data circuit async
             * 64  dedicated packet access
             * 128 dedicated PAD access
             */
            bsCode = p_cur->atTokNextint(&err);
            if (err < 0) {
                goto error;
            }

            /*
              * MD will send +CCWA:255,0 to add to indicate it's terminal base call waiting
              * solution. AP needs to reassign the call waiting status from system property
              * and send back to FWK.
              */
            if (tmpEnable == 255) {
                if (strcmp(tbCWStatus, TERMINAL_BASED_CALL_WAITING_ENABLED_OFF) == 0) {
                    logD(TAG, "CCWA 0xff is received and return terminal base cw disable");
                    responses[0] = 0;
                } else if (strcmp(tbCWStatus, TERMINAL_BASED_CALL_WAITING_ENABLED_ON) == 0) {
                    logD(TAG, "CCWA 0xff is received and return terminal base cw enable");
                    responses[0] = 1;
                } else {
                    responses[0] = 1;
                    logD(TAG, "reset terminal base call waiting mode to enable");
                    setMSimProperty(m_slot_id,
                            (char *)PROPERTY_TERMINAL_BASED_CALL_WAITING_MODE,
                            (char *)TERMINAL_BASED_CALL_WAITING_ENABLED_ON);
                }
                if (sendBsCode != 0) {
                    bsCode = sendBsCode;
                } else {
                    bsCode = 1;  // Default is only set VOICE service class.
                }
                responses[1] = sendBsCode;
                find = true;
            } else {
                /*
                 * tbCWStatus is TBCW mode enable but MD tell us the solution is NW solution.
                 * We need to reset the tbcw property to TBCW mode disabled.
                 */
                if (strcmp(tbCWStatus, TERMINAL_BASED_CALL_WAITING_DISABLED) != 0) {
                    logD(TAG, "Receive CCWA normal event and set TBCW mode to disable");
                    setMSimProperty(m_slot_id,
                            (char *)PROPERTY_TERMINAL_BASED_CALL_WAITING_MODE,
                            (char *)TERMINAL_BASED_CALL_WAITING_DISABLED);
                }
            }

            // The below flow is only for NW base solution.
            // Default new SS service class feature is supported.
            if (sendBsCode != 0) {
                if (sendBsCode == bsCode) {
                   /*
                    *Set response[1] to 1 to indicated that the call waiting is enabled
                    *(Refer to CallWaitingCheckBoxPreference.java).
                    */
                    if (tmpEnable != 255) {
                        responses[0] = tmpEnable;
                    }

                    responses[1] = bsCode;

                    //Check if call waiting is queried via MMI command
                    if (p_int[0] == CLASS_MTK_VIDEO + CLASS_DATA_SYNC) {
                        responses[1] = CLASS_MTK_VIDEO + CLASS_DATA_SYNC;
                    }

                    logD(TAG, "[Found] responses = %d, %d", responses[0], responses[1]);
                    find = true;
                    break;
                }
            } else {    /* For call wating query by MMI command */
                if (tmpEnable != 255) {
                    responses[0] = tmpEnable;
                }

                if (responses[0] == 1) {
                    responses[1] |=
                        (bsCode == CLASS_DATA_SYNC) ? CLASS_MTK_VIDEO + CLASS_DATA_SYNC : bsCode;
                }
                if (responseForAll == 0) {
                    responseForAll = responses[0];
                }
            }

            logD(TAG, "responses = %d, %d", responses[0], responses[1]);
            resLength++;
        }

        if (!find && sendBsCode != 0) {
            responses[1] = sendBsCode;
            logD(TAG, "[Not found] responses = %d, %d", responses[0], responses[1]);
        }

        logD(TAG, "%d of +CCWA: decoded!", resLength);

        /*
           For solving [ALPS00113964]Call waiting of VT hasn't response when turn on call waiting item, MTK04070, 2012.01.12
           sendBsCode = 0   --> Voice Call Waiting, refer to SERVICE_CLASS_NONE  in CommandInterface.java, GsmPhone.java
           sendBsCode = 512 --> Video Call Waiting, refer to SERVICE_CLASS_VIDEO in CommandInterface.java, GsmPhone.java

           Query Call Waiting: Network returned +CCWA: 1, 11 or/and +CCWA: 1, 24
           MmiBSCodeToInfoClassX method will convert 11 to 1(CLASS_VOICE), and convert 24 to
           16(CLASS_DATA_SYNC) + 512(CLASS_MTK_VIDEO)

           CallWaiting settings checked response[1] value, 0 as disabled and 1 as enabled.
        */

        // Default new SS service class feature is supported.
        /* For call wating query by MMI command */
        if (sendBsCode == 0) {
            responses[0] = responseForAll;
        }
    }

    ret = RIL_E_SUCCESS;

error:
    /* For SET CCWA responseVoid will ignore the responses */
    sp<RfxMclMessage> response = RfxMclMessage::obtainResponse(msg->getId(), ret,
            RfxIntsData(responses, sizeof(responses) / sizeof(int)), msg, false);

    // response to TeleCore
    responseToTelCore(response);
}

void RmcSuppServRequestBaseHandler::requestColpOperation(const sp<RfxMclMessage>& msg) {
    int* n = (int *) (msg->getData()->getData());
    sp<RfxAtResponse> p_response;
    int err;
    char* cmd = NULL;
    RfxAtLine *line;
    RIL_Errno ret = RIL_E_GENERIC_FAILURE;
    int responses[2]={0};

    if (msg->getData()->getDataLength() != 0) {
        /**
         * Set COLP: +COLP=[<n>]
         * "data" is int *
         * ((int *)data)[0] is "n" parameter from TS 27.007 7.8
         *  <n> (parameter sets the adjustment for outgoing calls)
         */
        asprintf(&cmd, "AT+COLP=%d", n[0]);
        // p_response = atSendCommand(cmd);
        p_response = atSendCommandMultiline(cmd, "+COLP:");
        free(cmd);
    } else {
        /**
         * AT+ECUSD=<m>,<n>,<str>
         * <m>: 1 for SS, 2 for USSD
         * <n>: 1 for execute SS or USSD, 2 for cancel USSD session
         * <str>: string type parameter, the SS or USSD string
         */
        p_response = atSendCommandSingleline("AT+ECUSD=1,1,\"*#76#\"", "+COLP:");
    }

    err = p_response->getError();
    if (err < 0 || p_response == NULL) {
        logE(TAG, "requestColpOperation Fail");
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

    /* For Get COLP only */
    if (p_response->getIntermediates() != NULL) {
        line = p_response->getIntermediates();
        line->atTokStart(&err);
        if (err < 0) {
            goto error;
        }

        /**
         * <n> parameter sets/shows the result code presentation status to the TE
         * 0   COLP disabled in MS
         * 1   COLP enabled in MS
         */
        responses[0] = line->atTokNextint(&err);
        if (err < 0) {
            goto error;
        }

        /**
         * <m> parameter shows the subscriber COLP service status in the network
         * 0   COLP not provisioned
         * 1   COLP provisioned in permanent mode
         */
        responses[1] = line->atTokNextint(&err);
        if (err < 0) {
            goto error;
        }
    }

    /* return success here */
    ret = RIL_E_SUCCESS;

error:
    /* For SET COLP responseVoid will ignore the responses */
    sp<RfxMclMessage> response = RfxMclMessage::obtainResponse(msg->getId(), ret,
            RfxIntsData(responses, sizeof(responses) / sizeof(int)), msg, false);

    // response to TeleCore
    responseToTelCore(response);
}

void RmcSuppServRequestBaseHandler::requestColrOperation(const sp<RfxMclMessage>& msg) {
    int* n = (int *) (msg->getData()->getData());
    sp<RfxAtResponse> p_response;
    int err;
    char* cmd = NULL;
    RfxAtLine *line;
    RIL_Errno ret = RIL_E_GENERIC_FAILURE;
    int responses[2] = {0};

    if (msg->getData()->getDataLength() != 0) {
        /**
         * Set COLR:
         */
        asprintf(&cmd, "AT+COLR=%d", n[0]);
        // p_response = atSendCommand(cmd);
        p_response = atSendCommandMultiline(cmd, "+COLR:");
        free(cmd);
    } else {
        /**
         * AT+ECUSD=<m>,<n>,<str>
         * <m>: 1 for SS, 2 for USSD
         * <n>: 1 for execute SS or USSD, 2 for cancel USSD session
         * <str>: string type parameter, the SS or USSD string
         */
        p_response = atSendCommandSingleline("AT+ECUSD=1,1,\"*#77#\"", "+COLR:");
    }

    err = p_response->getError();
    if (err < 0 || p_response == NULL) {
        logE(TAG, "requestColrOperation Fail");
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

    /* For Get COLR only */
    if (p_response->getIntermediates() != NULL) {
        line = p_response->getIntermediates();
        line->atTokStart(&err);
        if (err < 0) {
            goto error;
        }

        /**
         * <n> parameter sets the adjustment for outgoing calls
         * 0   COLR not provisioned
         * 1   COLR provisioned
         * 2   unknown
         */
        responses[0] = line->atTokNextint(&err);
        if (err < 0) {
            goto error;
        }
    }

    ret = RIL_E_SUCCESS;

error:
    sp<RfxMclMessage> response = RfxMclMessage::obtainResponse(msg->getId(), ret,
            RfxIntsData(&responses[0], sizeof(responses[0])/sizeof(int)), msg, false);

    // response to Telcore
    responseToTelCore(response);
}

void RmcSuppServRequestBaseHandler::requestCallBarring(const sp<RfxMclMessage>& msg, CallBarringOperationE op) {
    logE(TAG, "requestCallBarring: %d", op);
    sp<RfxAtResponse> p_response;
    int err = -1;
    char* cmd = NULL;
    RfxAtLine * p_cur;
    const char** strings = (const char**) (msg->getData()->getData());
    int response = -1;
    const char * p_serviceClass = NULL;
    RIL_Errno ret = RIL_E_GENERIC_FAILURE;
    int resLength = 0;
    int* p_res = NULL;
    int sendBsCode = 0;
    int allResponse = -1;

    // [ALPS00451149][MTK02772]
    // CLCK is query before MSG_ID_SIM_MMI_READY_IND
    // FD's flag is ready after receive this msg
    // solution: request again if modem response busy, max 2.5s
    int isSimBusy = 0;
    int count = 0;
    sp<RfxMclMessage> resp;

    // Check radio state first
    RIL_RadioState state = (RIL_RadioState) getMclStatusManager()->getIntValue(RFX_STATUS_KEY_RADIO_STATE);
    if (state == RADIO_STATE_UNAVAILABLE || state == RADIO_STATE_OFF) {
        if (!RfxRilUtils::isWfcEnable(msg->getSlotId())) {
            logD(TAG, "Modem is power off, just response to RILJ");
            ret = RIL_E_RADIO_NOT_AVAILABLE;
            goto error;
        }
    }

    do {
         //ALPS00839044: Modem needs more time for some special cards.
         //The detail of the time 2.5s is in the note of this CR.
         if( count == 13 ) {
             logE(TAG, "Set Facility Lock: CME_SIM_BUSY and time out.");
             goto error;
         }

        if ( msg->getData()->getDataLength() == 4*sizeof(char*) ) {
            /* Query Facility Lock */

            if ((0 == strcmp("AB",strings[0]))
                || (0 == strcmp("AG",strings[0]))
                || (0 == strcmp("AC",strings[0]))) {

                logE(TAG, "Call Barring Error: %s Cannot be used for Query!",strings[0]);
                goto error;
            }

            if ((NULL != strings[2]) && (0 != strcmp (strings[2],"0"))) {
                p_serviceClass = strings[2];
                sendBsCode = atoi(p_serviceClass);
            }

            // Default new SS service class feature is supported.
            if (sendBsCode == CLASS_MTK_VIDEO) {
                sendBsCode = CLASS_DATA_SYNC;
            }
            logD(TAG, "sendBsCode = %d", sendBsCode);

            /* PASSWD is given and CLASS is necessary. Because of NW related operation */
            /* asprintf(&cmd, "AT+CLCK=\"%s\",2,\"%s\",\"%s\"", strings[0], strings[1], strings[2]);
                asprintf(&cmd, "AT+ECUSD=1,1,\"*#%s**%s#\"", callBarFacToServiceCodeStrings(strings[0]),
                                                      InfoClassToMmiBSCodeString(atoi(p_serviceClass)));

            } else {*/
            /* BS_ALL NULL BSCodeString */
            // When query call barring setting, don't send BS code because some network cannot support BS code.
            asprintf(&cmd, "AT+ECUSD=1,1,\"*#%s#\"", callBarFacToServiceCodeStrings(strings[0]));
            //}

            p_response = atSendCommandMultiline(cmd, "+CLCK:");

        } else if ( msg->getData()->getDataLength() == 5*sizeof(char*) ) {
            if(NULL == strings[2]) {
                logE(TAG, "Set Facility Lock: Pwd cannot be null!");
                ret = RIL_E_PASSWORD_INCORRECT;
                goto error;
            }

            /* Set Facility Lock */
            if(strlen(strings[2]) != 4) {

                logE(TAG, "Set Facility Lock: Incorrect passwd length:%d",strlen(strings[2]));
                ret = RIL_E_PASSWORD_INCORRECT;
                goto error;

            }

            if ((NULL != strings[3]) && (0 != strcmp (strings[3],"0"))) {

                p_serviceClass = strings[3];

                /* Network operation. PASSWD is necessary */
                //asprintf(&cmd, "AT+CLCK=\"%s\",%s,\"%s\",\"%s\"", strings[0], strings[1], strings[2], strings[3]);
                if ( 0 == strcmp (strings[1],"0")) {
                    asprintf(&cmd, "AT+ECUSD=1,1,\"#%s*%s*%s#\"", callBarFacToServiceCodeStrings(strings[0]),
                                                              strings[2],
                                                              InfoClassToMmiBSCodeString(AtInfoClassE(atoi(p_serviceClass))));
                } else {
                    asprintf(&cmd, "AT+ECUSD=1,1,\"*%s*%s*%s#\"", callBarFacToServiceCodeStrings(strings[0]),
                                                              strings[2],
                                                              InfoClassToMmiBSCodeString(AtInfoClassE(atoi(p_serviceClass))));
                }
            } else {
                /* For BS_ALL BS==NULL */
                if ( 0 == strcmp (strings[1],"0")) {
                    asprintf(&cmd, "AT+ECUSD=1,1,\"#%s*%s#\"", callBarFacToServiceCodeStrings(strings[0]),
                                                          strings[2]);
                } else {
                    asprintf(&cmd, "AT+ECUSD=1,1,\"*%s*%s#\"", callBarFacToServiceCodeStrings(strings[0]),
                                                          strings[2]);
                }
            }
            // p_response = atSendCommand(cmd);
            p_response = atSendCommandMultiline(cmd, "+CLCK:");
        } else
            goto error;

        free(cmd);
        cmd = NULL;

        err = p_response->getError();
        if (err < 0 || p_response == NULL) {
            logE(TAG, "getFacilityLock Fail");
            goto error;
        }
        switch (p_response->atGetCmeError()) {
            case CME_SIM_BUSY:
                logD(TAG, "simFacilityLock: CME_SIM_BUSY");
                sleepMsec(200);
                count++;
                isSimBusy = 1;
                break;
            default:
                logD(TAG, "simFacilityLock: default");
                isSimBusy = 0;
                break;
        }
    } while (isSimBusy == 1);

    if (p_response->getSuccess() == 0) {
        switch (p_response->atGetCmeError()) {
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
            case CME_INCORRECT_PASSWORD:
                ret = RIL_E_PASSWORD_INCORRECT;
                goto error;
                break;
            case CME_CALL_BARRED:
            case CME_OPR_DTR_BARRING:
                ret = RIL_E_GENERIC_FAILURE;
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
    } else {
        ret = RIL_E_SUCCESS;
    }

    /* For Query command only */
    if ( p_response->getIntermediates() != NULL ) {
        for (p_cur = p_response->getIntermediates(); p_cur != NULL; p_cur = p_cur->getNext()) {
            resLength++;
        }
        logD(TAG, "%d of +CLCK: received!", resLength);
        resLength = 0; /* reset resLength for decoding */

        for (p_cur = p_response->getIntermediates(); p_cur != NULL; p_cur = p_cur->getNext()) {
            char *line  = NULL;
            int serviceClass;
            line = p_cur->getLine();

            if (line == NULL) {
                logE(TAG, "CLCK: NULL line");
                break;
            }

            p_cur->atTokStart(&err);   //m_pCurr will point to the position next to ":"
            if (err < 0) {
                logE(TAG, "CLCK: decode error 1!");
                goto error;
            }

            /**
             * <status>
             * 0    not active
             * 1    active
             */
            response = p_cur->atTokNextint(&err); /* 0 disable 1 enable */
            if (err < 0) {
                logE(TAG, "CLCK: decode error 2!");
                goto error;
            }

            /** <classx> is a sum of integers each representing a class of information (default 7):
             * 1   voice (telephony)
             * 2   data (refers to all bearer services)
             * 8   short message service
             * 16  data circuit sync
             * 32  data circuit async
             * 64  dedicated packet access
             * 128 dedicated PAD access
             */
            serviceClass = p_cur->atTokNextint(&err);
            if (err < 0) {
                logE(TAG, "CLCK: decode error 3!");
                goto error;
            }

            // Default new SS service class feature is supported.
            if (sendBsCode == serviceClass) {
                break;
            }

            /* For solving ALPS01415650.
             * When BsCode is not specified, return all responses for all service classes */
            logD(TAG, "sendBsCode = %d, response = %d, serviceClass = %d", sendBsCode, response, serviceClass);
            if ((sendBsCode == 0) && (response != 0)) {       //For voice/video service
                allResponse = (allResponse == -1) ? 0 : allResponse;
                if (serviceClass == CLASS_DATA_SYNC) {
                    allResponse |= CLASS_MTK_VIDEO;
                } else {
                    allResponse |= serviceClass;
                }
            }

            // Default new SS service class feature is supported.
            if (sendBsCode != 0) {
                response = 0;
            }
        }
    }


    // Default new SS service class feature is supported.
    // The specific call barring is activated(enabled), return service class value
    // (Refer to CallBarringBasePreference.java).
    if (sendBsCode != 0) {
        if (response != 0)  {
            response = atoi(p_serviceClass);
        }
    } else {
        if (allResponse != -1) {
            response = allResponse;
        }
    }

    ret = RIL_E_SUCCESS;

error:
    int msgid = (op == CB_E_QUERY) ? RFX_MSG_REQUEST_QUERY_FACILITY_LOCK : RFX_MSG_REQUEST_SET_FACILITY_LOCK;
    logD(TAG, "requestCallBarring response:%d, msg id: %d", response, msgid);
    /* For SET Facility Lock responseVoid will ignore the responses */
    resp = RfxMclMessage::obtainResponse(msgid, ret,
            RfxIntsData(&response, sizeof(response) / sizeof(int)), msg, false);

   // response to TeleCore
    responseToTelCore(resp);
}

void RmcSuppServRequestBaseHandler::sleepMsec(long long msec) {
    struct timespec ts;
    int err;

    ts.tv_sec = (msec / 1000);
    ts.tv_nsec = (msec % 1000) * 1000 * 1000;

    do
        err = nanosleep(&ts, &ts);
    while (err < 0 && errno == EINTR);
}

char* RmcSuppServRequestBaseHandler::parseErrorMessageFromXCAP(sp<RfxAtResponse> p_response) {
    int err;
    RfxAtLine* p_cur = p_response->getIntermediates();

    if (p_cur == NULL) {
        return NULL;
    }

    char *line = p_cur->getLine();

    if (line == NULL) {
        return NULL;
    }

    p_cur->atTokStart(&err);
    if (err < 0) {
        return NULL;
    }

    char* errorMsg = p_cur->atTokNextstr(&err);
    if (err < 0) {
        return NULL;
    }

    return errorMsg;
}


