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
#include "RmcSimBaseHandler.h"
#include "RmcCommSimDefs.h"
#include <cutils/properties.h>
#include "rfx_properties.h"
#include <telephony/mtk_ril.h>


/*****************************************************************************
 * Class RfxController
 *****************************************************************************/

/* OPERATOR*/
#define OPERATOR_OP09 "OP09"
#define SPEC_OP09_A "SEGDEFAULT"

RmcSimBaseHandler::RmcSimBaseHandler(int slot_id, int channel_id) :
        RfxBaseHandler(slot_id, channel_id) {
    setTag(String8("RmcSimBaseHandler"));
}

RmcSimBaseHandler::~RmcSimBaseHandler() {
}

RmcSimBaseHandler::SIM_HANDLE_RESULT RmcSimBaseHandler::needHandle(
        const sp<RfxMclMessage>& msg) {
    RFX_UNUSED(msg);
    return RESULT_IGNORE;
}

void RmcSimBaseHandler::handleRequest(const sp<RfxMclMessage>& msg) {
    RFX_UNUSED(msg);
}

void RmcSimBaseHandler::handleUrc(const sp<RfxMclMessage>& msg, RfxAtLine *urc) {
    RFX_UNUSED(msg);
    RFX_UNUSED(urc);
}

const int* RmcSimBaseHandler::queryTable(int channel_id, int *record_num) {
    RFX_UNUSED(channel_id);
    RFX_UNUSED(record_num);
    return NULL;
}

const char** RmcSimBaseHandler::queryUrcTable(int *record_num) {
    RFX_UNUSED(record_num);
    return NULL;
}

void RmcSimBaseHandler::setTag(String8 s) {
    mTag = s;
}

UICC_Status RmcSimBaseHandler::getSimStatus() {
    int err, count = 0;
    UICC_Status ret = UICC_NOT_READY;
    RfxAtLine *line = NULL;
    char *cpinResult = NULL;
    sp<RfxAtResponse> p_response = NULL;
    int pivot = 0;
    int inserted = 0;
    int currSimInsertedState = getMclStatusManager()->getIntValue(RFX_STATUS_KEY_SIM_STATE);
    int currRadioState = getMclStatusManager()->getIntValue(RFX_STATUS_KEY_RADIO_STATE);

    pthread_mutex_lock(&simStatusMutex);
    logD(mTag, "getSIMStatus: currRadioState %d, currSimInsertedState %d",
            currRadioState,currSimInsertedState);
    // Get SIM status
    do {
        // JB MR1, it will request sim status after receiver iccStatusChangedRegistrants,
        // but MD is off in the mean time, so it will get the exception result of CPIN.
        // For this special case, handle it specially.
        // check md off and sim inserted status, then return the result directly instead of request CPIN.
        // not insert: return SIM_ABSENT, insert: return SIM_NOT_READY or USIM_NOT_READY
        // TODO: wait s_md_off support
        /*
        if (s_md_off) {
            int slot = (1 << getMappingSIMByCurrentMode(rid));
            logI(mTag, "getSIMStatus s_md_off: %d slot: %d", s_md_off, slot);
            if (isDualTalkMode()) {
                pivot = 1 << m_slot_id;
                inserted = pivot & currSimInsertedState;
            } else {
                inserted = slot & currSimInsertedState;
            }
            if (!inserted) {
                ret = UICC_ABSENT;
                break;
            } else {
                ret = UICC_NOT_READY;
                break;
            }
        }
        */
        if (currRadioState == RADIO_STATE_UNAVAILABLE) {
             ret = UICC_NOT_READY;
             break;
        }
        p_response = atSendCommandSingleline("AT+CPIN?", "+CPIN:");
        err = p_response->getError();
        if (err != 0) {
            //if (err == AT_ERROR_INVALID_THREAD) {
            //  ret = UICC_BUSY;
            //} else {
                ret = UICC_NOT_READY;
            //}
        } else if (p_response->getSuccess() == 0) {
            switch (p_response->atGetCmeError()) {
                case CME_SIM_BUSY:
                    ret = UICC_BUSY;
                    break;
                case CME_SIM_NOT_INSERTED:
                case CME_SIM_FAILURE:
                    ret = UICC_ABSENT;
                    break;
                case CME_SIM_WRONG: {
                    RmcSimPinPukCount *retry = getPinPukRetryCount();
                    if (retry != NULL && retry->pin1 == 0 && retry->puk1 == 0) {
                        ret = UICC_PERM_BLOCKED; // PERM_DISABLED
                    } else if (retry->pin1 == -1 && retry->puk1 == -1 &&
                        retry->pin2 == -1 && retry->puk2 == -1) {
                        ret = UICC_ABSENT;
                    }else {
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
            logD(mTag, "getSIMStatus: cpinResult %s", cpinResult);

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
    } while (0);
    pthread_mutex_unlock(&simStatusMutex);
    logD(mTag, "getSIMStatus: ret %d", ret);
    return ret;
}

int RmcSimBaseHandler::queryAppTypeId(String8 aid) {
    int appTypeId = UICC_APP_SIM; // Default is SIM

    if (aid.isEmpty()) {
        // SIM or RUIM
        int cardType = getMclStatusManager()->getIntValue(RFX_STATUS_KEY_CARD_TYPE, -1);

        if (cardType & RFX_CARD_TYPE_SIM) {
            appTypeId = UICC_APP_SIM;
        } else if (cardType & RFX_CARD_TYPE_RUIM) {
            appTypeId = UICC_APP_RUIM;
        } else if (cardType & RFX_CARD_TYPE_CSIM) {
            appTypeId = UICC_APP_CSIM;
        } else {
            logD(mTag, "Could not get app id because card type is not ready!");
        }
    } else if (strncmp(aid.string(), "A0000000871002", 14) == 0) {
        // USIM
        appTypeId = UICC_APP_USIM; // USIM
    } else if (strncmp(aid.string(), "A0000000871004", 14) == 0) {
        // ISIM
        appTypeId = UICC_APP_ISIM;
    } else if (strncmp(aid.string(), "A0000003431002", 14) == 0) {
        // CSIM
        appTypeId = UICC_APP_CSIM;
    } else {
        logD(mTag, "Not support the aid %s", aid.string());
        appTypeId = -1;
    }

    return appTypeId;
}

bool RmcSimBaseHandler::bIsTc1()
{
    static int siTc1 = -1;

    if (siTc1 < 0)
    {
        char cTc1[RFX_PROPERTY_VALUE_MAX] = { 0 };

        rfx_property_get("ro.mtk_tc1_feature", cTc1, "0");
        siTc1 = atoi(cTc1);
    }

    return ((siTc1 > 0) ? true : false);
}

bool RmcSimBaseHandler::isSimInserted() {
    char iccid[RFX_PROPERTY_VALUE_MAX] = {0};
    String8 prop(PROPERTY_ICCID_PREIFX);

    prop.append(String8::format("%d", (m_slot_id + 1)));
    rfx_property_get(prop.string(), iccid, "");

    if ((strlen(iccid) > 0) && (strcmp(iccid, "N/A") != 0)){
        return true;
    }
    return false;
}

bool RmcSimBaseHandler::isCommontSlotSupport() {
    char property_value[RFX_PROPERTY_VALUE_MAX] = { 0 };
    rfx_property_get(PROPERTY_COMMON_SLOT_SUPPORT, property_value, "0");
    return atoi(property_value) == 1 ? true:false;
}

RmcSimPinPukCount* RmcSimBaseHandler::getPinPukRetryCount() {
    sp<RfxAtResponse> p_response = NULL;
    int err;
    int ret;
    RfxAtLine*line;
    RmcSimPinPukCount *retry = (RmcSimPinPukCount*)calloc(1, sizeof(RmcSimPinPukCount));

    retry->pin1 = -1;
    retry->pin2 = -1;
    retry->puk1 = -1;
    retry->puk2 = -1;

    p_response = atSendCommandSingleline("AT+EPINC", "+EPINC:");

    if (p_response != NULL && p_response->getSuccess() > 0) {
        // Success
        do {
            line = p_response->getIntermediates();

            line->atTokStart(&err);

            if (err < 0) {
                logE(mTag, "get token error");
                break;
            }

            retry->pin1 = line->atTokNextint(&err);
            if (err < 0) {
                logE(mTag, "get pin1 fail");
                break;
            }

            retry->pin2 = line->atTokNextint(&err);
            if (err < 0) {
                logE(mTag, "get pin2 fail");
                break;
            }

            retry->puk1 = line->atTokNextint(&err);
            if (err < 0) {
                logE(mTag, "get puk1 fail");
                break;
            }

            retry->puk2 = line->atTokNextint(&err);
            if (err < 0) {
                logE(mTag, "get puk2 fail");
                break;
            }

            setPinPukRetryCountProp(retry);
        } while(0);

    } else {
        logE(mTag, "Fail to get PIN and PUK retry count!");
    }

    p_response = NULL;
    logD(mTag, "pin1:%d, pin2:%d, puk1:%d, puk2:%d",
            retry->pin1,retry->pin2,retry->puk1,retry->puk2);

    return retry;
}

void RmcSimBaseHandler::setPinPukRetryCountProp(RmcSimPinPukCount *retry) {
    String8 pin1("gsm.sim.retry.pin1");
    String8 pin2("gsm.sim.retry.pin2");
    String8 puk1("gsm.sim.retry.puk1");
    String8 puk2("gsm.sim.retry.puk2");

    pin1.append((m_slot_id == 0)? "" : String8::format(".%d", (m_slot_id+1)));
    pin2.append((m_slot_id == 0)? "" : String8::format(".%d", (m_slot_id+1)));
    puk1.append((m_slot_id == 0)? "" : String8::format(".%d", (m_slot_id+1)));
    puk2.append((m_slot_id == 0)? "" : String8::format(".%d", (m_slot_id+1)));

    rfx_property_set(pin1.string(), String8::format("%d", retry->pin1).string());
    rfx_property_set(pin2.string(), String8::format("%d", retry->pin2).string());
    rfx_property_set(puk1.string(), String8::format("%d", retry->puk1).string());
    rfx_property_set(puk2.string(), String8::format("%d", retry->puk2).string());
}

bool RmcSimBaseHandler::isOP09AProject() {
    char optr_value[RFX_PROPERTY_VALUE_MAX] = { 0 };
    char seg_value[RFX_PROPERTY_VALUE_MAX] = { 0 };

    rfx_property_get("persist.operator.optr", optr_value, "0");
    rfx_property_get("persist.operator.seg", seg_value, "0");

    if ((strncmp(optr_value, OPERATOR_OP09, strlen(OPERATOR_OP09)) == 0) &&
            (strncmp(seg_value, SPEC_OP09_A, strlen(SPEC_OP09_A)) == 0)) {
        return true;
    }

    return false;
}
