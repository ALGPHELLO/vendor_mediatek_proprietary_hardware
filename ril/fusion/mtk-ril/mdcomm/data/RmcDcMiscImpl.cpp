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
#include "RmcDcMiscImpl.h"
#include "RmcDataDefs.h"
#include "RmcCapabilitySwitchUtil.h"

#define RFX_LOG_TAG "RmcDcMiscImpl"

//[New R8 modem FD]
#define PROPERTY_FD_SCREEN_ON_TIMER     "persist.radio.fd.counter"
#define PROPERTY_FD_SCREEN_ON_R8_TIMER  "persist.radio.fd.r8.counter"
#define PROPERTY_FD_SCREEN_OFF_TIMER    "persist.radio.fd.off.counter"
#define PROPERTY_FD_SCREEN_OFF_R8_TIMER "persist.radio.fd.off.r8.counter"
#define PROPERTY_FD_ON_ONLY_R8_NETWORK  "persist.radio.fd.on.only.r8"
#define PROPERTY_RIL_FD_MODE       "ril.fd.mode"
#define PROPERTY_MTK_FD_SUPPORT    "ro.mtk_fd_support"

/* FD related timer: units: 0.1 sec */
#define DEFAULT_FD_SCREEN_ON_TIMER "150"
#define DEFAULT_FD_SCREEN_ON_R8_TIMER "150"
#define DEFAULT_FD_SCREEN_OFF_TIMER "50"
#define DEFAULT_FD_SCREEN_OFF_R8_TIMER "50"
#define DEFAULT_FD_ON_ONLY_R8_NETWORK "0"

/*****************************************************************************
 * Class RmcDcMiscImpl
 * Send AT Command through this class. Created by RmcDcMiscHandler.
 *****************************************************************************/

RmcDcMiscImpl::RmcDcMiscImpl(RfxBaseHandler* handler, int slotId) :
        mHandler(handler), mSlotId(slotId) {
}

RmcDcMiscImpl::~RmcDcMiscImpl() {
}

/**
 * Method for initialization module.
 */
void RmcDcMiscImpl::init() {
    if (isFastDormancySupport() == 1) {
        initializeFastDormancy();
    }
    syncEpdgConfigToMd();
}

void RmcDcMiscImpl::syncEpdgConfigToMd() {
    char propertyValue[255] = {0};

    rfx_property_get("persist.mtk_epdg_support", propertyValue, "0");
    mHandler->atSendCommand(String8::format("AT+EEPDG=%d", (int) (atoi(propertyValue))));
}

void RmcDcMiscImpl::requestStartLce(const sp<RfxMclMessage>& msg) {
    RfxAtLine *pLine = NULL;
    sp<RfxAtResponse> p_response;
    sp<RfxMclMessage> responseMsg;
    int *pInt = (int *)msg->getData()->getData();
    int desiredInterval = pInt[0]; // The desired reporting interval in ms.
    int lceMode = pInt[1] + 1; // LCE mode : start with PUSH mode = 1; start with PULL mode = 2.
    char lceStatus = 0xFF; // LCE status : not supported = 0xFF; stopped = 0; active = 1.
    unsigned int actualInterval = 0; //The actual reporting interval in ms.
    int err = 0;

    /* Initialize LCE status response */
    RIL_LceStatusInfo* response = (RIL_LceStatusInfo*)calloc(1, sizeof(RIL_LceStatusInfo));
    RFX_ASSERT(response != NULL);

    /* Use AT+ELCE=<lce_mode>[,<desired_interval>] to start LCE service */
    p_response = atSendCommandSingleline(String8::format("AT+ELCE=%d,%u",
            lceMode, desiredInterval), "+ELCE:");

    if (p_response == NULL) {
        RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] fail to get p_response!",
                mSlotId, __FUNCTION__);
        goto error;
    }

    if (p_response->isAtResponseFail()) {
        RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] AT+ELCE=%d,%u responce ERROR",
                mSlotId, __FUNCTION__, lceMode, desiredInterval);
        goto done;
    }

    /* pLine => +ELCE:<lce_status>,<actual_interval> */
    pLine = p_response->getIntermediates();

    if (pLine == NULL) {
        RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] fail to get intermediate results!",
                mSlotId, __FUNCTION__);
        goto error;
    }

    pLine->atTokStart(&err);
    if (err < 0) {
        RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] ERROR occurs when token start", mSlotId, __FUNCTION__);
        goto error;
    }

    /* Get 1st parameter: lceStatus */
    lceStatus = (char)pLine->atTokNextint(&err);
    if (err < 0) {
        RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] ERROR occurs when parsing lce status",
                mSlotId, __FUNCTION__);
        goto error;
    }

    /* Get 2nd parameter: actualInterval */
    if (pLine->atTokHasmore()) {
        actualInterval = pLine->atTokNextint(&err);
        if (err < 0) {
            RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] ERROR occurs when parsing actual interval",
                    mSlotId, __FUNCTION__);
            goto error;
        }
    }

done:
    RFX_LOG_I(RFX_LOG_TAG, "[%d][%s] lceStatus=%d, actualInterval=%u",
            mSlotId, __FUNCTION__, lceStatus, actualInterval);
    response->lce_status = lceStatus;
    response->actual_interval_ms = actualInterval;
    responseMsg = RfxMclMessage::obtainResponse(RIL_E_SUCCESS,
            RfxLceStatusResponseData(response, sizeof(RIL_LceStatusInfo)), msg);
    responseToTelCore(responseMsg);
    FREEIF(response);
    return;

error:
    responseMsg = RfxMclMessage::obtainResponse(RIL_E_GENERIC_FAILURE, RfxVoidData(), msg);
    responseToTelCore(responseMsg);
    FREEIF(response);
}

void RmcDcMiscImpl::requestStopLce(const sp<RfxMclMessage>& msg) {
    RFX_UNUSED(msg);
    RfxAtLine *pLine = NULL;
    sp<RfxAtResponse> p_response;
    sp<RfxMclMessage> responseMsg;
    char lceStatus = 0xFF; // LCE status : not supported = 0xFF; stopped = 0; active = 1.
    unsigned int actualInterval = 0; //The actual reporting interval in ms.
    int err = 0;

    /* Initialize LCE status response */
    RIL_LceStatusInfo* response = (RIL_LceStatusInfo*)calloc(1, sizeof(RIL_LceStatusInfo));
    RFX_ASSERT(response != NULL);

    /* Use AT+ELCE=0 to stop LCE service */
    p_response = atSendCommandSingleline(String8::format("AT+ELCE=0"), "+ELCE:");

    if (p_response == NULL) {
        RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] fail to get p_response!",
                mSlotId, __FUNCTION__);
        goto error;
    }

    if (p_response->isAtResponseFail()) {
        RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] AT+ELCE=0 responce ERROR", mSlotId, __FUNCTION__);
        goto done;
    }

    /* pLine => +ELCE:<lce_status>,<actual_interval> */
    pLine = p_response->getIntermediates();

    if (pLine == NULL) {
        RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] fail to get intermediate results!",
                mSlotId, __FUNCTION__);
        goto error;
    }

    pLine->atTokStart(&err);
    if (err < 0) {
        RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] ERROR occurs when token start", mSlotId, __FUNCTION__);
        goto error;
    }

    /* Get 1st parameter: lceStatus */
    lceStatus = (char)pLine->atTokNextint(&err);
    if (err < 0) {
        RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] ERROR occurs when parsing lce status",
                mSlotId, __FUNCTION__);
        goto error;
    }

    /* Get 2nd parameter: actualInterval */
    if (pLine->atTokHasmore()) {
        actualInterval = pLine->atTokNextint(&err);
        if (err < 0) {
            RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] ERROR occurs when parsing actual interval",
                    mSlotId, __FUNCTION__);
            goto error;
        }
    }

done:
    RFX_LOG_I(RFX_LOG_TAG, "[%d][%s] lceStatus=%d, actualInterval=%u",
            mSlotId, __FUNCTION__, lceStatus, actualInterval);
    response->lce_status = lceStatus;
    response->actual_interval_ms = actualInterval;
    responseMsg = RfxMclMessage::obtainResponse(RIL_E_SUCCESS,
            RfxLceStatusResponseData(response, sizeof(RIL_LceStatusInfo)), msg);
    responseToTelCore(responseMsg);
    FREEIF(response);
    return;

error:
    responseMsg = RfxMclMessage::obtainResponse(RIL_E_GENERIC_FAILURE, RfxVoidData(), msg);
    responseToTelCore(responseMsg);
    FREEIF(response);
}

void RmcDcMiscImpl::requestPullLceData(const sp<RfxMclMessage>& msg) {
    RFX_UNUSED(msg);
    RfxAtLine *pLine = NULL;
    sp<RfxAtResponse> p_response;
    sp<RfxMclMessage> responseMsg;
    int lceMode = 0; // LCE service mode : stop mode = 0; start with PUSH mode = 1; start with PULL mode = 2.
    int err = 0;

    /* Initialize LCE data response */
    RIL_LceDataInfo* response = (RIL_LceDataInfo*)calloc(1, sizeof(RIL_LceDataInfo));
    RFX_ASSERT(response != NULL);

    /* Use AT+ELCE? to pull LCE service for capacity information */
    p_response = atSendCommandSingleline(String8::format("AT+ELCE?"), "+ELCE:");

    if (p_response == NULL) {
        RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] fail to get p_response!",
                mSlotId, __FUNCTION__);
        goto error;
    }

    if (p_response->isAtResponseFail()) {
        RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] AT+ELCE? responce ERROR", mSlotId, __FUNCTION__);
        goto error;
    }

    /* pLine => +ELCE:<lce_mode>[,<last_hop_capacity_kbps>,<confidence_level>,<lce_suspended>] */
    pLine = p_response->getIntermediates();

    if (pLine == NULL) {
        RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] fail to get intermediate results!",
                mSlotId, __FUNCTION__);
        goto error;
    }

    pLine->atTokStart(&err);
    if (err < 0) {
        RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] ERROR occurs when token start", mSlotId, __FUNCTION__);
        goto error;
    }

    /* Get 1st parameter: lceMode(unused) */
    lceMode = pLine->atTokNextint(&err);
    if (err < 0) {
        RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] ERROR occurs when parsing lce mode",
                mSlotId, __FUNCTION__);
        goto error;
    }

    if (pLine->atTokHasmore()) {
        /* Get 2nd parameter: last_hop_capacity_kbps */
        response->last_hop_capacity_kbps = pLine->atTokNextint(&err);
        if (err < 0) {
            RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] ERROR occurs when parsing last hop capacity",
                    mSlotId, __FUNCTION__);
            goto error;
        }

        if (pLine->atTokHasmore()) {
            /* Get 3rd parameter: confidence_level */
            response->confidence_level = (unsigned char)pLine->atTokNextint(&err);
            if (err < 0) {
                RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] ERROR occurs when parsing confidence level",
                        mSlotId, __FUNCTION__);
                goto error;
            }

            if (pLine->atTokHasmore()) {
                /* Get 4th parameter: lce_suspended */
                response->lce_suspended = (unsigned char)pLine->atTokNextint(&err);
                if (err < 0) {
                    RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] ERROR occurs when parsing lce suspended",
                            mSlotId, __FUNCTION__);
                    goto error;
                }
            }
        }
    }

    RFX_LOG_I(RFX_LOG_TAG, "[%d][%s] last_hop_capacity_kbps=%u, confidence_level=%u, "
            "lce_suspended=%u", mSlotId, __FUNCTION__, response->last_hop_capacity_kbps,
            response->confidence_level, response->lce_suspended);
    responseMsg = RfxMclMessage::obtainResponse(RIL_E_SUCCESS,
            RfxLceDataResponseData(response, sizeof(RIL_LceDataInfo)), msg);
    responseToTelCore(responseMsg);
    FREEIF(response);
    return;

error:
    responseMsg = RfxMclMessage::obtainResponse(RIL_E_GENERIC_FAILURE, RfxVoidData(), msg);
    responseToTelCore(responseMsg);
    FREEIF(response);
}

void RmcDcMiscImpl::onLceStatusChanged(const sp<RfxMclMessage>& msg) {
    //+ELCE: <last_hop_capacity_kbps>,<confidence_level>,<lce_suspended>
    char *urc = (char*)msg->getData()->getData();
    int err = 0;
    RfxAtLine *pLine = NULL;
    sp<RfxMclMessage> urcMsg;

    /* Initialize LCE data response */
    RIL_LceDataInfo* response = (RIL_LceDataInfo*)calloc(1, sizeof(RIL_LceDataInfo));
    RFX_ASSERT(response != NULL);

    RFX_LOG_I(RFX_LOG_TAG, "[%d][%s] urc=%s", mSlotId, __FUNCTION__, urc);

    pLine = new RfxAtLine(urc, NULL);

    if (pLine == NULL) {
        RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] fail to new pLine!",
                mSlotId, __FUNCTION__);
        goto error;
    }

    pLine->atTokStart(&err);
    if (err < 0) {
        RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] ERROR occurs when token start", mSlotId, __FUNCTION__);
        goto error;
    }

    if (pLine->atTokHasmore()) {
        /* Get 1st parameter: last_hop_capacity_kbps */
        response->last_hop_capacity_kbps = pLine->atTokNextint(&err);
        if (err < 0) {
            RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] ERROR occurs when parsing last hop capacity",
                    mSlotId, __FUNCTION__);
            goto error;
        }

        if (pLine->atTokHasmore()) {
            /* Get 2nd parameter: confidence_level */
            response->confidence_level = (unsigned char)pLine->atTokNextint(&err);
            if (err < 0) {
                RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] ERROR occurs when parsing confidence level",
                        mSlotId, __FUNCTION__);
                goto error;
            }

            if (pLine->atTokHasmore()) {
                /* Get 3rd parameter: lce_suspended */
                response->lce_suspended = (unsigned char)pLine->atTokNextint(&err);
                if (err < 0) {
                    RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] ERROR occurs when parsing lce suspended",
                            mSlotId, __FUNCTION__);
                    goto error;
                }
            }
        }
    }

    urcMsg = RfxMclMessage::obtainUrc(RFX_MSG_URC_LCEDATA_RECV, mSlotId,
                    RfxLceDataResponseData(response, sizeof(RIL_LceDataInfo)));
    responseToTelCore(urcMsg);
    AT_LINE_FREE(pLine);
    FREEIF(response);
    return;

error:
    AT_LINE_FREE(pLine);
    FREEIF(response);
}

void RmcDcMiscImpl::setFdMode(const sp<RfxMclMessage>& msg) {
    int *pReqInt = (int *)msg->getData()->getData();
    int argsNum = pReqInt[0];
    sp<RfxAtResponse> responseFromModem;
    sp<RfxMclMessage> responseToTcl;
    int err;

    if (argsNum == 1) {
        // AT+EFD=1: Enable modem fast dormancy.
        // AT+EFD=0: Disable modem fast dormancy.
        responseFromModem = atSendCommand(String8::format("AT+EFD=%d", pReqInt[1]));
    } else if (argsNum == 2) {
        // Format: AT+EFD=3,screen_status(0:screen off, 1:screen on).
        responseFromModem = atSendCommand(String8::format("AT+EFD=%d,%d",
                pReqInt[1], pReqInt[2]));
    } else if (argsNum == 3) {
        // Format: AT+EFD=2,timer_id,timerValue (unit:0.1 sec)
        responseFromModem = atSendCommand(String8::format("AT+EFD=%d,%d,%d",
                pReqInt[1], pReqInt[2], pReqInt[3]));
    } else {
        RFX_LOG_E(RFX_LOG_TAG, "Weird, should never be here!");
    }

    if (responseFromModem != NULL && !responseFromModem->isAtResponseFail()) {
        responseToTcl = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_SUCCESS,
                RfxVoidData(), msg, true);
    } else {
        responseToTcl = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_MODEM_ERR,
                RfxVoidData(), msg, true);
    }

    responseToTelCore(responseToTcl);
}

void RmcDcMiscImpl::initializeFastDormancy() {
    // Fast Dormancy is only available on 3G protocol set, so when maxRadio is 2G, disable it.
    int radioCapability = getIntValue(mSlotId, RFX_STATUS_KEY_SLOT_CAPABILITY, 0);
    int maxCapability = RmcCapabilitySwitchUtil::getMaxRadioGeneration(radioCapability);
    RFX_LOG_D(RFX_LOG_TAG, "[%d][%s]: maxCapability=%d", mSlotId, __FUNCTION__, maxCapability);

    if (maxCapability == RADIO_GENERATION_2G) {
        rfx_property_set(PROPERTY_RIL_FD_MODE, "0");
        return;
    }

    sp<RfxAtResponse> response;
    char propertyValue[RFX_PROPERTY_VALUE_MAX] = { 0 };

    // [Step#01] Query if the new FD mechanism is supported by modem or not.
    response = atSendCommandSingleline(String8::format("AT+EFD=?"), "+EFD:");

    if (response != NULL && !response->isAtResponseFail()) {
        // Set PROPERTY_RIL_FD_MODE. Framework can query this to know if AP side is necessary to
        // execute FD or not.
        int errcode = rfx_property_set(PROPERTY_RIL_FD_MODE, "1");
        memset(propertyValue, 0, sizeof(propertyValue));
        rfx_property_get(PROPERTY_RIL_FD_MODE, propertyValue, "0");

        // [Step#02] Set default FD related timers for mode:
        //           format => AT+EFD=2, timer_id, timerValue(unit:0.1 sec)
        // timerId=0: Screen Off + Legacy FD
        memset(propertyValue, 0, sizeof(propertyValue));
        rfx_property_get(PROPERTY_FD_SCREEN_OFF_TIMER, propertyValue, DEFAULT_FD_SCREEN_OFF_TIMER);
        atSendCommand(String8::format("AT+EFD=2,0,%d", (int)(atof(propertyValue))));

        // timerId=2: Screen Off + R8 FD
        memset(propertyValue, 0, sizeof(propertyValue));
        rfx_property_get(PROPERTY_FD_SCREEN_OFF_R8_TIMER, propertyValue,
                DEFAULT_FD_SCREEN_OFF_R8_TIMER);
        atSendCommand(String8::format("AT+EFD=2,2,%d", (int)(atof(propertyValue))));

        // timerId=1: Screen On + Legacy FD
        memset(propertyValue, 0, sizeof(propertyValue));
        rfx_property_get(PROPERTY_FD_SCREEN_ON_TIMER, propertyValue, DEFAULT_FD_SCREEN_ON_TIMER);
        atSendCommand(String8::format("AT+EFD=2,1,%d", (int)(atof(propertyValue))));

        // timerId=3: Screen On + R8 FD
        memset(propertyValue, 0, sizeof(propertyValue));
        rfx_property_get(PROPERTY_FD_SCREEN_ON_R8_TIMER, propertyValue,
                DEFAULT_FD_SCREEN_ON_R8_TIMER);
        atSendCommand(String8::format("AT+EFD=2,3,%d", (int)(atof(propertyValue))));

        // For special operator request.
        memset(propertyValue, 0, sizeof(propertyValue));
        rfx_property_get(PROPERTY_FD_ON_ONLY_R8_NETWORK, propertyValue,
                DEFAULT_FD_ON_ONLY_R8_NETWORK);
        RFX_LOG_D(RFX_LOG_TAG, "[%d][%s]: %s = %s", mSlotId, __FUNCTION__,
                PROPERTY_FD_ON_ONLY_R8_NETWORK, propertyValue);
        if (atoi(propertyValue) == 1) {
            atSendCommand(String8::format("AT+EPCT=0,4194304"));
        }

        // [Step#03] Enable FD Mechanism MD:
        //           after finishing to set FD related default timer to modem
        atSendCommand(String8::format("AT+EFD=1"));
    }
}

int RmcDcMiscImpl::isFastDormancySupport() {
    int isFdSupport = 0;
    char propertyValue[RFX_PROPERTY_VALUE_MAX] = { 0 };
    rfx_property_get(PROPERTY_MTK_FD_SUPPORT, propertyValue, "0");
    isFdSupport = atoi(propertyValue);
    return isFdSupport ? 1 : 0;
}

int RmcDcMiscImpl::getIntValue(int slotId, const RfxStatusKeyEnum key, int default_value) {
    RFX_ASSERT(getMclStatusManager(slotId) != NULL);
    return getMclStatusManager(slotId)->getIntValue(key, default_value);
}

RfxMclStatusManager* RmcDcMiscImpl::getMclStatusManager(int slotId) {
    RFX_ASSERT(mHandler != NULL);
    return mHandler->getMclStatusManager(slotId);
}

sp<RfxAtResponse> RmcDcMiscImpl::atSendCommand(const String8 cmd) {
    RFX_ASSERT(mHandler != NULL);
    return mHandler->atSendCommand(cmd.string());
}

sp<RfxAtResponse> RmcDcMiscImpl::atSendCommandSingleline(const String8 cmd, const char *rspPrefix) {
    RFX_ASSERT(mHandler != NULL);
    return mHandler->atSendCommandSingleline(cmd.string(), rspPrefix);
}

void RmcDcMiscImpl::responseToTelCore(const sp<RfxMclMessage> msg) {
    RFX_ASSERT(mHandler != NULL);
    mHandler->responseToTelCore(msg);
}
