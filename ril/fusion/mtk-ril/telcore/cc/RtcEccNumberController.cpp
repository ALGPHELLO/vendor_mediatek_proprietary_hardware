/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 *
 * MediaTek Inc. (C) 2010. All rights reserved.
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
#include "RtcEccNumberController.h"
#include "RfxAtLine.h"
#include <string.h>

#define RFX_LOG_TAG "RtcEccNumberController"

#define MAX_PROP_CHARS   50
#define MCC_CHAR_LEN     3
#define ESIMS_CAUSE_RECOVERY 14

static const char PROPERTY_GSM_SIM_ECC[MAX_SIM_COUNT][MAX_PROP_CHARS] = {
    "ril.ecclist",
    "ril.ecclist1",
    "ril.ecclist2",
    "ril.ecclist3",
};

static const char PROPERTY_C2K_SIM_ECC[MAX_SIM_COUNT][MAX_PROP_CHARS] = {
    "ril.cdma.ecclist",
    "ril.cdma.ecclist1",
    "ril.cdma.ecclist2",
    "ril.cdma.ecclist3",
};

static const char PROPERTY_NW_ECC_LIST[MAX_SIM_COUNT][MAX_PROP_CHARS] = {
    "ril.ecc.service.category.list",
    "ril.ecc.service.category.list.1",
    "ril.ecc.service.category.list.2",
    "ril.ecc.service.category.list.3",
};

static const char PROPERTY_NW_ECC_MCC[MAX_SIM_COUNT][MAX_PROP_CHARS] = {
    "ril.ecc.service.category.mcc",
    "ril.ecc.service.category.mcc.1",
    "ril.ecc.service.category.mcc.2",
    "ril.ecc.service.category.mcc.3",
};

/*****************************************************************************
 * Class RtcEccNumberController
 *****************************************************************************/

RFX_IMPLEMENT_CLASS("RtcEccNumberController", RtcEccNumberController, RfxController);


RtcEccNumberController::RtcEccNumberController() :
    mCachedGsmUrc(NULL),
    mCachedC2kUrc(NULL) {
    if (isBspPackage()) {
        if (isOp12Package()) {
            mDefaultEccNumber = String8("112,911,*911,#911");
            mDefaultEccNumberNoSim = String8("112,911,*911,#911,000,08,110,999,118,119");
        } else {
            mDefaultEccNumber = String8("112,911");
            mDefaultEccNumberNoSim = String8("112,911,000,08,110,999,118,119");
        }
    } else {
        // default service category set to -1 to not
        // conflict with customized ecc because SIM
        // ECC priority high then customized ECC
        mDefaultEccNumber = String8("112,-1;911,-1");
    }

    // Init once for new modem generation.
    if (m_slot_id == 0) {
        // For backward compatible old solution: read SIM ECC from Java framework
        rfx_property_set("ril.ef.ecc.support", "1");
    }
}

RtcEccNumberController::~RtcEccNumberController() {
    if (mCachedC2kUrc != NULL) {
        delete(mCachedC2kUrc);
    }
    if (mCachedGsmUrc != NULL) {
        delete(mCachedGsmUrc);
    }
}

void RtcEccNumberController::onInit() {
    // Required: invoke super class implementation
    RfxController::onInit();

    logD(RFX_LOG_TAG, "[%s]", __FUNCTION__);

    //Set the default ECC number
    if (isBspPackage()) {
        rfx_property_set(PROPERTY_GSM_SIM_ECC[m_slot_id], mDefaultEccNumberNoSim.string());
    } else {
        rfx_property_set(PROPERTY_GSM_SIM_ECC[m_slot_id], "");
        rfx_property_set(PROPERTY_C2K_SIM_ECC[m_slot_id], "");
    }

    const int urc_id_list[] = {
        RFX_MSG_URC_CC_GSM_SIM_ECC,
        RFX_MSG_URC_CC_C2K_SIM_ECC
    };

    // register request & URC id list
    // NOTE. one id can only be registered by one controller
    registerToHandleUrc(urc_id_list, sizeof(urc_id_list)/sizeof(const int));

    // register callbacks to get card type change event
    getStatusManager()->registerStatusChanged(RFX_STATUS_KEY_CARD_TYPE,
        RfxStatusChangeCallback(this, &RtcEccNumberController::onCardTypeChanged));

    // register callbacks to get PLMN(MCC,MNC) change event
    getStatusManager()->registerStatusChanged(RFX_STATUS_KEY_OPERATOR,
        RfxStatusChangeCallback(this, &RtcEccNumberController::onPlmnChanged));

    // register callbacks to get sim recovery event
    getStatusManager()->registerStatusChanged(RFX_STATUS_KEY_SIM_ESIMS_CAUSE,
        RfxStatusChangeCallback(this, &RtcEccNumberController::onSimRecovery));

}

void RtcEccNumberController::onCardTypeChanged(RfxStatusKeyEnum key,
    RfxVariant oldValue, RfxVariant newValue) {
    RFX_UNUSED(key);
    if (oldValue.asInt() != newValue.asInt()) {
        logV(RFX_LOG_TAG, "[%s] oldValue %d, newValue %d", __FUNCTION__,
            oldValue.asInt(), newValue.asInt());
        if (newValue.asInt() == 0) {
            /*  For No SIM inserted, the behavior is different on TK and BSP because
                TK support hotplug and customized ECC list.
                [TK]:  Reset to "" otherwise it will got wrong value in
                     isEmergencyNumberExt variable bSIMInserted. (ALPS02749228)
                [BSP]: Reset to mDefaultEccNumber because BSP don't support custom ECC
                     So we use this property instead (ALPS02572162)
            */
            logD(RFX_LOG_TAG,"[%s], reset SIM/NW ECC property due to No SIM", __FUNCTION__);
            if (isBspPackage()) {
                rfx_property_set(PROPERTY_GSM_SIM_ECC[m_slot_id],
                    mDefaultEccNumberNoSim.string());
                mGsmEcc = String8("");
                mC2kEcc = String8("");
            } else {
                rfx_property_set(PROPERTY_GSM_SIM_ECC[m_slot_id], "");
                rfx_property_set(PROPERTY_C2K_SIM_ECC[m_slot_id], "");
            }

            // Clear network ECC when SIM removed according to spec.
            rfx_property_set(PROPERTY_NW_ECC_LIST[m_slot_id], "");
        } else if (!isCdmaCard(newValue.asInt())) {
            // no CSIM or RUIM application, clear CDMA ecc property
            if (isBspPackage()) {
                logV(RFX_LOG_TAG,"[%s], Remove C2K property due to No C2K SIM", __FUNCTION__);
                String8 temEcc = mGsmEcc + mDefaultEccNumber;
                rfx_property_set(PROPERTY_GSM_SIM_ECC[m_slot_id], temEcc.string());
            } else {
                logV(RFX_LOG_TAG,"[%s], reset C2K property due to No C2K SIM", __FUNCTION__);
                rfx_property_set(PROPERTY_C2K_SIM_ECC[m_slot_id], "");
            }
        }
    }
}

void RtcEccNumberController::onPlmnChanged(RfxStatusKeyEnum key,
    RfxVariant oldValue, RfxVariant newValue) {
    RFX_UNUSED(key);
    logV(RFX_LOG_TAG, "[%s] oldValue %s, newValue %s", __FUNCTION__,
        (const char *)(oldValue.asString8()), (const char *)(newValue.asString8()));

    if (newValue.asString8().length() < MCC_CHAR_LEN) {
        logE(RFX_LOG_TAG, "[%s] MCC length error !", __FUNCTION__);
        return;
    }

    char currentMccmnc[RFX_PROPERTY_VALUE_MAX] = {0};
    /* Check if the latest MCC/MNC is different from the value stored in system property,
       and if they are different then clear emergency number and service category */
    rfx_property_get(PROPERTY_NW_ECC_MCC[m_slot_id], currentMccmnc, "0");
    char mcc[MCC_CHAR_LEN + 1] = {0};
    strncpy(mcc, (const char *)newValue.asString8(), MCC_CHAR_LEN);
    if (strcmp(currentMccmnc, mcc)) {
        rfx_property_set(PROPERTY_NW_ECC_LIST[m_slot_id], "");
    }
}

void RtcEccNumberController::onSimRecovery(RfxStatusKeyEnum key,
    RfxVariant oldValue, RfxVariant newValue) {
    RFX_UNUSED(key);
    RFX_UNUSED(oldValue);

    if (newValue.asInt() == ESIMS_CAUSE_RECOVERY) {
        logD(RFX_LOG_TAG, "[%s] parse from cached URC", __FUNCTION__);

        // Need parse from cached ECC URC when SIM recovery because when
        // sim lost it will clear ECC in card type change event
        parseSimEcc(mCachedGsmUrc, true);
        parseSimEcc(mCachedC2kUrc, false);
    }
}

bool RtcEccNumberController::onHandleUrc(const sp<RfxMessage>& message) {
    int msgId = message->getId();

    switch (msgId) {
        case RFX_MSG_URC_CC_GSM_SIM_ECC:
            handleGsmSimEcc(message);
            break;
        case RFX_MSG_URC_CC_C2K_SIM_ECC:
            handleC2kSimEcc(message);
            break;
        default:
            break;
    }

    return true;
}

/*
 * [MD1 EF ECC URC format]
 * + ESMECC: <m>[,<number>,<service category>[,<number>,<service category>]]
 * <m>: number of ecc entry
 * <number>: ecc number
 * <service category>: service category
 * Ex.
 * URC string:+ESIMECC:3,115,4,334,5,110,1
 *
 * Note:If it has no EF ECC, RtcEccNumberController will receive "0"
*/
void RtcEccNumberController::handleGsmSimEcc(const sp<RfxMessage>& message){
    if (mCachedGsmUrc != NULL) {
        delete(mCachedGsmUrc);
    }
    mCachedGsmUrc = new RfxAtLine((const char* )(message->getData()->getData()), NULL);

    parseSimEcc(mCachedGsmUrc, true);
}

/*
 * [MD3 EF ECC URC format]
 * +CECC:<m>[,<number [,<number >]]
 * <m>: number of ecc entry
 * <number>: ecc number
 * Ex.
 * URC string:2,115,334
 *
 * Note:If it has no EF ECC, RtcEccNumberController will receive "0"
 *
*/
void RtcEccNumberController::handleC2kSimEcc(const sp<RfxMessage>& message){
    if (mCachedC2kUrc != NULL) {
        delete(mCachedC2kUrc);
    }
    mCachedC2kUrc = new RfxAtLine((const char* )(message->getData()->getData()), NULL);

    parseSimEcc(mCachedC2kUrc, false);
}

void RtcEccNumberController::parseSimEcc(RfxAtLine *line, bool isGsm) {
    String8 writeEcc = String8("");
    int err = 0;
    int count = 0;

    if (line == NULL) {
        logE(RFX_LOG_TAG, "[%s] error: line is NULL", __FUNCTION__);
        return;
    }

    logV(RFX_LOG_TAG, "[%s] line: %s", __FUNCTION__, line->getLine());

    line->atTokStart(&err);
    if (err < 0) goto error;

    // get ECC number count
    count = line->atTokNextint(&err);
    if (err < 0) goto error;

    if (count > 0) {
        for (int i = 0; i < count; i++) {
            if (isGsm) {
                char* ecc = line->atTokNextstr(&err);
                if (err < 0) goto error;
                char* eccCategory = line->atTokNextstr(&err);
                if (err < 0) goto error;
                if (isBspPackage()) {
                    // BSP don't support service category
                    writeEcc.appendFormat("%s,", ecc);
                } else {
                    writeEcc.appendFormat("%s,%s;", ecc, eccCategory);
                }
            } else {
                char* ecc = line->atTokNextstr(&err);
                if (err < 0) goto error;
                writeEcc.appendFormat("%s,", ecc);
            }
        }
    } else {
        logV(RFX_LOG_TAG, "[%s] There is no ECC number stored in SIM", __FUNCTION__);
    }

    if (isBspPackage()) {
        if (isGsm) {
            //Add the default ECC number
            mGsmEcc = writeEcc;
        } else {
            mC2kEcc = writeEcc;
        }
        writeEcc = mGsmEcc + mC2kEcc + mDefaultEccNumber;
        logD(RFX_LOG_TAG,"[%s] PROPERTY_GSM_SIM_ECC: %s, writeEcc: %s",
                __FUNCTION__,
                PROPERTY_GSM_SIM_ECC[m_slot_id],
                writeEcc.string());
        rfx_property_set(PROPERTY_GSM_SIM_ECC[m_slot_id], writeEcc.string());
    } else {
        if (isGsm) {
            //Add the default ECC number
            writeEcc += mDefaultEccNumber;

            logD(RFX_LOG_TAG,"[%s] PROPERTY_GSM_SIM_ECC: %s, writeEcc: %s",
                __FUNCTION__,
                PROPERTY_GSM_SIM_ECC[m_slot_id],
                writeEcc.string());
            rfx_property_set(PROPERTY_GSM_SIM_ECC[m_slot_id], writeEcc.string());
        } else {
            logD(RFX_LOG_TAG,"[%s] PROPERTY_C2K_SIM_ECC: %s, writeEcc: %s",
                __FUNCTION__,
                PROPERTY_C2K_SIM_ECC[m_slot_id],
                writeEcc.string());
            rfx_property_set(PROPERTY_C2K_SIM_ECC[m_slot_id], writeEcc.string());
        }
    }

    return;
error:
    logE(RFX_LOG_TAG, "[%s] parsing error!", __FUNCTION__);
}

bool RtcEccNumberController::isCdmaCard(int cardType) {
     if ((cardType & RFX_CARD_TYPE_RUIM) > 0 ||
         (cardType & RFX_CARD_TYPE_CSIM) > 0) {
         return true;
     }
     return false;
}
