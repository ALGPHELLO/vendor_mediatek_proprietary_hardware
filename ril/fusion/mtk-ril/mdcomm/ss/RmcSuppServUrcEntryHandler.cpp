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

#include "RmcSuppServUrcEntryHandler.h"
#include "GsmUtil.h"
#include "SSUtil.h"
#include "RfxStringsData.h"
#include "RfxVoidData.h"
#include "RfxIntsData.h"
#include "RfxVoidData.h"
#include "RfxSuppServNotificationData.h"

static const char * GsmCbsDcsStringp[MAX_DCS_SUPPORT] = {"GSM7","8BIT","UCS2"};

RFX_REGISTER_DATA_TO_URC_ID(RfxStringsData, RFX_MSG_UNSOL_ON_USSD);
RFX_REGISTER_DATA_TO_URC_ID(RfxIntsData, RFX_MSG_UNSOL_CALL_FORWARDING);
RFX_REGISTER_DATA_TO_URC_ID(RfxStringsData, RFX_MSG_UNSOL_ON_USSI);
RFX_REGISTER_DATA_TO_URC_ID(RfxStringsData, RFX_MSG_UNSOL_ON_XUI);

RFX_REGISTER_DATA_TO_EVENT_ID(RfxVoidData, RFX_MSG_EVENT_URC_CRING_NOTIFY);
RFX_REGISTER_DATA_TO_EVENT_ID(RfxIntsData, RFX_MSG_EVENT_URC_ECPI133_NOTIFY);

RmcSuppServUrcEntryHandler::RmcSuppServUrcEntryHandler(int slot_id, int channel_id) :
        RfxBaseHandler(slot_id, channel_id) {
    const char* urc[] = {
        "+CUSD",
        "+ECFU",
        "+ECMCCSS",
        "+EIMSEMSIND",
        "+EIUSD",
        "+EIMSXUI"
    };

    static const int event[] = {
        RFX_MSG_EVENT_URC_CRING_NOTIFY,
        RFX_MSG_EVENT_URC_ECPI133_NOTIFY
    };

    registerToHandleURC(urc, sizeof(urc)/sizeof(char *));
    registerToHandleEvent(event, sizeof(event)/sizeof(int));

    memset(&tmpSvcNotify, 0, sizeof(RIL_SuppSvcNotification));
}

RFX_IMPLEMENT_HANDLER_CLASS(RmcSuppServUrcEntryHandler, RIL_CMD_PROXY_URC);

RmcSuppServUrcEntryHandler::~RmcSuppServUrcEntryHandler() {
}

void RmcSuppServUrcEntryHandler::onHandleUrc(const sp<RfxMclMessage>& msg) {
    char *urc = msg->getRawUrc()->getLine();
    logD(TAG, "[onHandleUrc]%s", urc);

    if(strstr(urc, "+CUSD") != NULL) {
        handleOnUssd(msg);
    } else if (strstr(urc, "+ECFU") != NULL) {
        handleOnCfuNotify(msg);
    } else if (strstr(urc, "+ECMCCSS") != NULL) {
        handleOnLteSuppSvcNotification(msg);
    } else if (strstr(urc, "+EIMSEMSIND") != NULL) {
        handleOnLteSuppSvcNotification(msg);
    } else if (strstr(urc, "+EIUSD") != NULL) {
        handleOnUssi(msg);
    } else if (strstr(urc, "+EIMSXUI") != NULL) {
        handleOnXui(msg);
    }
}

void RmcSuppServUrcEntryHandler::onHandleEvent(const sp<RfxMclMessage>& msg) {
    int id = msg->getId();
    switch(id) {
        case RFX_MSG_EVENT_URC_CRING_NOTIFY:
            handleCRINGReveiced();
            break;
        case RFX_MSG_EVENT_URC_ECPI133_NOTIFY:
            handleECPI133Received(msg);
            break;

        default:
            logE(TAG, "should not be here");
            break;
    }
}

void RmcSuppServUrcEntryHandler::handleOnUssd(const sp<RfxMclMessage>& msg) {
    logD(TAG, "onUssd: %s", msg->getRawUrc()->getLine());

    char* p_data[3];
    char* p_utf8Data = NULL;
    char* p_ucs2Data = NULL;
    char* p_str = NULL;
    RfxAtLine* line = msg->getRawUrc();
    int err;
    const char* dcs;
    int iDCS;
    int length = 0;
    char *dcsString = NULL;

    // Coverity for declaim in the begining.
    int      numStrings;
    bytes_t  utf8String = NULL;
    bytes_t  hexData = NULL;
    int      len = 0, maxLen = 0, i = 0;
    size_t   responselen = 0;
    char**   p_cur = NULL;
    char*    finalData[2];

    /* Initialize finalData */
    for(int i = 0; i < 2; i++) {
        finalData[i] = NULL;
    }

    sp<RfxMclMessage> urc;   // Declare here ?_?

    /**
     * USSD response from the network, or network initiated operation
     * +CUSD: <m>[,<str>,<dcs>] to the TE.
     */
    /*
     * <m>:
     * 0   no further user action required (network initiated USSD Notify, or no further information needed after mobile initiated operation)
     * 1   further user action required (network initiated USSD Request, or further information needed after mobile initiated operation)
     * 2   USSD terminated by network
     * 3   other local client has responded
     * 4   operation not supported
     * 5   network time out
     */

    line->atTokStart(&err);

    if (err < 0) {
        goto error;
    }

    p_data[0] = line->atTokNextstr(&err);

    if (err < 0) {
        goto error;
    }

    length++;

    /* Check if there is <str> */
    if (line->atTokHasmore()) {
        /* Get <str> */
        p_str = line->atTokNextstr(&err);
        if (err< 0) {
            goto error;
        }

        length++;

        /* Get <dcs> */
        iDCS = line-> atTokNextint(&err);
        if (err < 0) {
            logE(TAG, "No <dcs> information");
            goto error;
        }

        length++;
        logD(TAG, "onUssd:length = %d", length);
        logD(TAG, "onUssd: line = %s", line);
        logD(TAG, "onUssd: p_data[0] = %s", p_data[0]);
        logD(TAG, "onUssd: p_str = %s", p_str);
        logD(TAG, "onUssd: dcs = %d", iDCS);

        /* Refer to GSM 23.038, section 5 CBS Data Coding Scheme
         * The message starts with a two GSM 7-bits default alphabet character.
         * Need to ignore these two bytes.
         */
        GsmCbsDcsE dcsType = checkCbsDcs(iDCS);
        if (iDCS == 0x11) {
           logD(TAG, "Ignore the first two bytes for DCS_UCS2");
           p_str+=4;
        }

        p_data[1] = p_str;
        logD(TAG, "onUssd: p_data[1] (p_str) = %s", p_data[1]);
        // Default new SS service class feature is supported.
        if (dcsType == DCS_8BIT) {     // GsmCbsDcsE: DCS_8BIT = 1
            p_data[2] = (char *) GsmCbsDcsStringp[dcsType];
            logD(TAG, "onUssd: p_data[2] (dcsString) = %s", p_data[2]);
        } else {
            /* DCS is set as "UCS2" by AT+CSCS in ril_callbacks.c */
            dcsString = strdup("UCS2");
            p_data[2] = dcsString;
            logD(TAG, "onUssd: p_data[2] (dcsString) = %s", p_data[2]);
        }
    }

    // Decode response data by using ril_gsm_util.
    responselen = length * sizeof(char *);
    p_cur = (char **) p_data;
    // Only need m and str, dcs is only for str reference and no need to pass it.

    numStrings = responselen / sizeof(char *);

    if (numStrings > 1) {
        hexData = (bytes_t) calloc(strlen(p_cur[1]), sizeof(char));
        if(hexData == NULL) {
            logE(TAG, "onUssd:hexData malloc fail");
            goto error;
        }

        len = gsm_hex_to_bytes((cbytes_t) p_cur[1], strlen(p_cur[1]), hexData);
        logD(TAG, "onUsdd add value to hexData = %s", hexData);
        logD(TAG, "onUsdd len = %d", len);

        dcs = p_cur[2];
        logD(TAG, "onUsdd dcs = p_cur[2] = %s", dcs);

        maxLen = (!strcmp(dcs,"UCS2")) ? len / 2 : len;
        if ((maxLen < 0) || (maxLen > MAX_RIL_USSD_STRING_LENGTH)) {
            free(hexData);
            hexData = NULL;
            logE(TAG, "onUssd:max length is invalid, len = %d", maxLen);
            goto error;
        } else {
            logD(TAG, "Ussd string length:%d/n", maxLen);
        }
    }

    /**
     * According the USSD response format: "+CUSD: <m>[,<str>,<dcs>]",
     * the number of stings only could be 1 or 3. And we don't need to return dcs back
     * to framework if number of strings equal to 3.
     */
    finalData[0] = p_cur[0];
    logD(TAG, "onUssd, each string of finalData[0]= %s", finalData[0]);
    if (numStrings == 3) {
        utf8String = (bytes_t) calloc(2*len+1, sizeof(char));
        if (utf8String == NULL) {
            free(hexData);   // Coverity
            hexData = NULL;  // Coverity
            logE(TAG, "onUssd:utf8String malloc fail");
            goto error;
        }

        /* The USS strings need to be transform to utf8 */
        if (!strcmp(dcs, "GSM7")) {
            logD(TAG, "Ussd GSM7");
            utf8_from_unpackedgsm7((cbytes_t) hexData, 0, len, utf8String);
            logD(TAG, "onUssd for loop, utf8String= %s", utf8String);
        } else if (!strcmp(dcs, "UCS2")) {
            logD(TAG, "Ussd UCS2");

            // Some character can't display when receive the ussd notification
            zero4_to_space(hexData, len);
            ucs2_to_utf8((cbytes_t) hexData, len/2, utf8String);
        }  else {
            logD(TAG, "Ussd not GSM7 or UCS2");
            utf8_from_gsm8((cbytes_t) hexData, len, utf8String);
        }
        finalData[1] = (char *) utf8String;
        // Network might response empty str
        logD(TAG, "onUssd, each string of finalData[1]= %s", finalData[1]);
    }

    if (isSimulateUSSI()) {
        logD(TAG, "Simulate USSI by CS");
        char *ussiData[3] = {(char *)"1", finalData[0],
                (numStrings == 3) ? finalData[1] : (char *)""};
        urc = RfxMclMessage::obtainUrc(RFX_MSG_UNSOL_ON_USSI,
                m_slot_id, RfxStringsData(ussiData, 3));
    } else {
        urc = RfxMclMessage::obtainUrc(RFX_MSG_UNSOL_ON_USSD,
                m_slot_id, RfxStringsData(finalData, 2));
    }

    responseToTelCore(urc);
    logD(TAG, "Send RIL_UNSOL_ON_USSD");

    if (dcsString != NULL) {
        free(dcsString);
    }
    free(hexData);
    free(utf8String);
    return;

error:
    logE(TAG, "Parse RIL_UNSOL_ON_USSD fail: %s/n", msg->getRawUrc()->getLine());
}

// Protect
void RmcSuppServUrcEntryHandler::handleOnCfuNotify(const sp<RfxMclMessage>& msg) {
    RfxAtLine* line = msg->getRawUrc();
    int err;
    int response[2] = {0}; /* status, line */

    sp<RfxMclMessage> urc;

    line->atTokStart(&err);
    if (err < 0) {
        goto error;
    }

    /* Get Status */
    response[0] = line-> atTokNextint(&err);
    if (err < 0) {
        goto error;
    }

    /* Get Line info */
    response[1] = line-> atTokNextint(&err);
    if (err < 0) {
        goto error;
    }

    urc = RfxMclMessage::obtainUrc(RFX_MSG_UNSOL_CALL_FORWARDING,
            m_slot_id, RfxIntsData(response, 2));
    responseToTelCore(urc);
    logD(TAG, "Send RIL_UNSOL_CALL_FORWARDING");

    return;

error:
    logE("Parse RIL_UNSOL_CALL_FORWARDING fail: %s/n", msg->getRawUrc()->getLine());
}

// Protect
void RmcSuppServUrcEntryHandler::handleOnLteSuppSvcNotification(const sp<RfxMclMessage>& msg) {
    RIL_SuppSvcNotification svcNotify;
    RfxAtLine* line = msg->getRawUrc();
    int err;
    char *s = line->getLine();
    int callId, service;
    char *rawString;
    char *tmpString = NULL;
    memset(&svcNotify, 0, sizeof(RIL_SuppSvcNotification));
    svcNotify.notificationType = 0;  /* MO: 0, MT: 1 */

    if ((strStartsWith(s, "+EIMSEMSIND:"))) {
       /* +EIMSEMSIND: <call_id>[,"<pau>"] */
        line->atTokStart(&err);
        if (err < 0) {
            logE("Parse +EIMSEMSIND fail(start token): %s\n", s);
            return;
        }

        /* call_id */
        svcNotify.index = line-> atTokNextint(&err);
        if (err < 0) {
            logD("Parse +EIMSEMSIND fail(call_id): %s\n", s);
        }

        /* pau */
        svcNotify.number = line-> atTokNextstr(&err);
        if (err < 0) {
            logD("Parse +EIMSEMSIND fail(call_id): %s\n", s);
        }

        /* Refer to MO_CODE_CALL_IS_EMERGENCY in SuppServiceNotification.java */
        svcNotify.code = 10;
    } else if ((strStartsWith(s, "+ECMCCSS:"))) {
       /*
        * +ECMCCSS=<call_id><service><raw_string>
        * call_id: The id(index) of current call which the URC is related to.
        * service: URC type -> 13  - Call Forwarding
        *                      256 - Outgoing call barring
        *                      259 - Call Waiting
        */


        /* +ECMCCSS */
        line->atTokStart(&err);
        if (err < 0) {
            logE("Parse +ECMCCSS fail(start token): %s\n", s);
            return;
        }

        /* call_id */
        callId = line-> atTokNextint(&err);
        if (err < 0) {
            logE("Parse +ECMCCSS fail(call_id): %s\n", s);
            return;
        }

        /* service */
        service = line-> atTokNextint(&err);
        if (err < 0) {
            logE("Parse +ECMCCSS fail(service): %s\n", s);
            return;
        }

        /* Raw string */
       rawString = line->getLine() + 1;
       /*if (at_tok_nextstr(&line, &rawString) < 0) {
            LOGE("Parse +ECMCCSS fail(Raw string): %s\n", s);
            return;
       }*/

        logD(TAG, "[onLteSuppSvcNotification]call_id = %d, service = %d, Raw string = %s\n", callId, service, rawString);

        switch (service) {
            case 13:        /* Call Forwarding */
                /* Refer to MO_CODE_CALL_FORWARDED_TO in SuppServiceNotification.java */
                svcNotify.code = 9;
                /* Retrieve forwarded-to number or SIP URI */
                tmpString = (char*)calloc(1, 256 * sizeof(char));
                if (tmpString == NULL) {
                   LOGE("onLteSuppSvcNotification: tmpString malloc fail");
                   return;
                }
                retrieveCallForwardedToNumber(rawString, tmpString);
                svcNotify.number = tmpString;
                break;

            case 256:       /* Outgoing call barring */
                /* Reuse <code1> in +CSSI URC */
                svcNotify.code = 5;
                break;

            case 259:       /* Call waiting */
                svcNotify.code = 3;     /* Reuse <code1> in +CSSI URC */
                /* Sometime rawString is empty, so do not need to check its content
                if (NULL == strstr(rawString, "call-waiting")) {
                return;
                }
                */
                break;
            case 257:   /* Call Forwarded*/
                handleECMCCSS257Received();
                svcNotify.notificationType = 1;
                svcNotify.code = 0;
                svcNotify.index = callId;
                break;
        }

        /* Add this check for sensitive log */
        if (checkUserLoad()) {
            logD(TAG, "[onLteSuppSvcNotification] svcNotify.notificationType = %d,\
                    svcNotify.code = %d, svcNotify.number = ********",
                    svcNotify.notificationType, svcNotify.code);
        } else {
            logD(TAG, "[onLteSuppSvcNotification] svcNotify.notificationType = %d,\
                    svcNotify.code = %d, svcNotify.number = %s",
                    svcNotify.notificationType, svcNotify.code, svcNotify.number);
        }

        if (service == 257) {
            /* CRING has arrived already, send SSN to RILJ */
            if (isCRINGReceived) {
                logD(TAG, "ECMCCSS and CRING have been received! Send SSN!");
                sp<RfxMclMessage> urc = RfxMclMessage::obtainUrc(
                        RFX_MSG_UNSOL_SUPP_SVC_NOTIFICATION, m_slot_id,
                        RfxSuppServNotificationData(&svcNotify, sizeof(RIL_SuppSvcNotification)));
                responseToTelCore(urc);
                resetFlagAndSvcNotify();
            } else {
                /* CRING has not arrived yet, keep the object in RILD */
                logD(TAG, "CRING not received, keep svcNotify until CRING comes");
                memcpy(&tmpSvcNotify, &svcNotify, sizeof(RIL_SuppSvcNotification));
            }

            return;
        }
    }

    sp<RfxMclMessage> urc = RfxMclMessage::obtainUrc(RFX_MSG_UNSOL_SUPP_SVC_NOTIFICATION,
            m_slot_id, RfxSuppServNotificationData(&svcNotify, sizeof(RIL_SuppSvcNotification)));
    responseToTelCore(urc);
    logD(TAG, "Send RIL_UNSOL_SUPP_SVC_NOTIFICATION");

    if (tmpString != NULL) {
       free(tmpString);
    }
}

/**
 * Retrieve call forwarded-to number or sip uri from History-Info(rawString).
 *
 * @param rawString Contains History-Info of SIP response
 * @param number    To store the last forwarded-to number or sip uri
 *
 */
void RmcSuppServUrcEntryHandler::retrieveCallForwardedToNumber(char *rawString, char *number) {
   char *begin = strchr(rawString, ':');
   char *end, *end1;
   char tmpStr[256];
   int len;

   /* For example:
      History-Info: <sip:UserB@ims.example.com>;index=1,<sip:UserC@ims.example.com?Reason=SIP;cause=302;text="CDIV">;index=1.1,
      <sip:UserD@ims.example.com?Reason=SIP;cause=302;text="CDIV">;index=1.2
   */

   /* Find ':' of History-Info */
   if (begin == NULL) {
      return;
   }

   /* Find each number or sip uri */
   end = begin;
   while ((begin != NULL) && (end != NULL)) {
      end = strchr(begin, '>');
      begin = strchr(begin, '<');
      if ((begin != NULL) && (end != NULL)) {
         len = end - begin - 1;
         if ((len > 0) && (len < 256)) {
            strncpy(tmpStr, begin+1, len);
            tmpStr[len] = '\0';
            strncpy(number, tmpStr, strlen(tmpStr));
            /* Extract number of SIP uri only */
            if (strStartsWith(tmpStr, "sip:") || strStartsWith(tmpStr, "tel:")) {
               strncpy(number, tmpStr+4, strlen(tmpStr+4));
               end1 = strchr(number, '?');
               if (end1 == NULL) {
                   end1 = strchr(number, ';');
               }
               if (end1 != NULL) {
                  number[end1 - number] = '\0';
               }
            }
            if (checkUserLoad()) {  // Add this check for sensitive log
                logD(TAG, "[retrieveCallForwardedToNumber] number = ********");
            } else {
                logD(TAG, "[retrieveCallForwardedToNumber] number = %s", number);
            }
         }
         begin = end + 1;
      }
   }
}

static bytes_t hexString2Byte(const char* hexString) {
    size_t count = 0;
    char* pos = (char*)hexString;
    size_t byteLength = strlen(hexString)/2;
    bytes_t byteArray = (bytes_t) calloc(byteLength + 1, sizeof(byte_t));
    for(count = 0; count < byteLength; count++) {
        sscanf(pos, "%2hhx", &byteArray[count]);
        pos += 2;
    }
    return byteArray;
}

void RmcSuppServUrcEntryHandler::handleOnUssi(const sp<RfxMclMessage>& msg) {

    logD(TAG, "OnUssi: %s", msg->getRawUrc()->getLine());

    char* p_data[8];
    bytes_t ussdString = NULL;
    RfxAtLine* line = msg->getRawUrc();
    sp<RfxMclMessage> urc;   // Declare here ?_?
    int length_of_urc_from_md = 7;
    int length_of_urc_to_ap = 8;  // add urc + socket information to AP
    int err;
    int i = 0;

    /*
     * USSI response from the network, or network initiated operation
     +EIUSD: <class>,<status>,<str>,<lang>,<error_code>,<alertingpattern>,<sip_cause>
     <m>:
     1   USSD notify
     2   SS notify
     3   MD execute result
     <n>:  if m=1
     0   no further user action required
     1   further user action required
     2   USSD terminated by network
     3   other local client has responded
     4   operation not supported
     5    network time out
     <n>:if m=3, value is return value of MD
     0   execute success
     1   common error
     2   IMS unregistered
     3   IMS busy
     4   NW error response
     5   session not exist
     6   NW not support(404)
     <str>: USSD/SS string
     <lang>: USSD language
     < error_code > USSD error code in xml
     < alertingpattern > alerting pattern of NW initiated INVITE
     <sip_cause> sip error code
     */

    line->atTokStart(&err);
    if (err < 0) {
        logD(TAG, "onUssi: at_tok_start, error");
        goto error;
    }

    for (i = 0 ; i < length_of_urc_from_md ; i++) {
        p_data[i] = line->atTokNextstr(&err);
        if (err < 0) {
            logD(TAG, "onUssi: at_tok_nextstr: error, p_data[%d] = %s", i, p_data[i]);
            goto error;
        }
        logD(TAG, "onUssi: p_data[%d] = %s", i, p_data[i]);
    }

    asprintf(&p_data[7], "%d", m_slot_id);

    ussdString = hexString2Byte(p_data[2]);
    p_data[2] = (char*)ussdString;

    urc = RfxMclMessage::obtainUrc(RFX_MSG_UNSOL_ON_USSI,
            m_slot_id, RfxStringsData(p_data, length_of_urc_to_ap));
    responseToTelCore(urc);

    free(p_data[7]);

    if (ussdString != NULL) {
        free(ussdString);
    }

    logD(TAG, "Send RIL_UNSOL_ON_USSI");

    return;

error:
    logE(TAG, "Parse RIL_UNSOL_ON_USSI fail: %s/n", msg->getRawUrc()->getLine());
}

void RmcSuppServUrcEntryHandler::handleOnXui(const sp<RfxMclMessage>& msg) {

    logD(TAG, "OnXui: %s", msg->getRawUrc()->getLine());

    char* p_data[4];
    RfxAtLine* line = msg->getRawUrc();
    sp<RfxMclMessage> urc;   // Declare here ?_?
    int length_of_urc_from_md = 3;
    int length_of_urc_to_ap = 4;  // add urc + socket information to AP
    int err;
    int i = 0;

    /*
     * XUI information.
     +EIMSXUI=<account_id>, <broadcast_flag>,<xui_info>
     < account_id >: account number 0~7
     < broadcast_flag >: broadcast flag 0~1
     < xui_info >: Xui information
     */

    line->atTokStart(&err);
    if (err < 0) {
        goto error;
    }

    for (i = 0 ; i < length_of_urc_from_md ; i++) {
        p_data[i] = line->atTokNextstr(&err);
        if (err < 0) {
            goto error;
        }
    }

    for (i = 0 ; i < length_of_urc_from_md ; i++) {
        logD(TAG, "onXui: p_data[%d] = %s", i, p_data[i]);
    }

    asprintf(&p_data[3], "%d", m_slot_id);

    urc = RfxMclMessage::obtainUrc(RFX_MSG_UNSOL_ON_XUI,
            m_slot_id, RfxStringsData(p_data, length_of_urc_to_ap));
    responseToTelCore(urc);

    free(p_data[3]);

    logD(TAG, "Send RFX_MSG_UNSOL_ON_XUI");

    return;

error:
    logE(TAG, "Parse RFX_MSG_UNSOL_ON_XUI fail: %s/n", msg->getRawUrc()->getLine());
}

void RmcSuppServUrcEntryHandler::handleCRINGReveiced() {
    logD(TAG, "handleCRINGReveiced");
    isCRINGReceived = true;

    /* ECMCCSS has arrived already */
    if (isECMCCSS257Received) {
        logD(TAG, "Both ECMCCSS 257 & CRING are received, return tmpSvcNotify");
        sp<RfxMclMessage> urc = RfxMclMessage::obtainUrc(RFX_MSG_UNSOL_SUPP_SVC_NOTIFICATION,
            m_slot_id,
            RfxSuppServNotificationData(&tmpSvcNotify, sizeof(RIL_SuppSvcNotification)));
        responseToTelCore(urc);
        resetFlagAndSvcNotify();
    }
}

void RmcSuppServUrcEntryHandler::handleECPI133Received(const sp<RfxMclMessage>& msg) {
    int* callId = (int*)(msg->getData()->getData());
    logD(TAG, "handleECPI133Received, tmpSvcNotify.index = %d, callId = %d",
            tmpSvcNotify.index, (*callId));
    /* Only when the call id equals to ECMCCSS's, we reset the flag and SvcNotify.
     * If there is no ECMCCSS, the default index in tmpSvcNotify is zero */
    if (tmpSvcNotify.index == (*callId)) {
        resetFlagAndSvcNotify();
    }
}

void RmcSuppServUrcEntryHandler::handleECMCCSS257Received() {
    logD(TAG, "handleECMCCSS257Received");
    isECMCCSS257Received = true;
}

/* Reset SvcNotify object and flags about ECMCCSS and CRING
 * when SvcNotify is sent to RILJ or receive ECPI 133 */
void RmcSuppServUrcEntryHandler::resetFlagAndSvcNotify() {
    logD(TAG, "resetFlagAndSvcNotify");
    isECMCCSS257Received = false;
    isCRINGReceived = false;
    memset(&tmpSvcNotify, 0, sizeof(RIL_SuppSvcNotification));
}
