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
#include "RtcDataController.h"
#include "RtcDataUtils.h"

#define RFX_LOG_TAG "RtcDC"

/*****************************************************************************
 * Class RtcDataController
 *****************************************************************************/
RFX_IMPLEMENT_CLASS("RtcDataController", RtcDataController, RfxController);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxStringsData, RfxDataCallResponseData, RFX_MSG_REQUEST_SETUP_DATA_CALL);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxStringsData, RfxVoidData, RFX_MSG_REQUEST_DEACTIVATE_DATA_CALL);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxIntsData, RFX_MSG_REQUEST_LAST_DATA_CALL_FAIL_CAUSE);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxDataCallResponseData, RFX_MSG_REQUEST_DATA_CALL_LIST);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxSetDataProfileData, RfxVoidData, RFX_MSG_REQUEST_SET_DATA_PROFILE);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData, RFX_MSG_REQUEST_SYNC_DATA_SETTINGS_TO_MD);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxStringData, RfxVoidData, RFX_MSG_REQUEST_RESET_MD_DATA_RETRY_COUNT);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIaApnData, RfxVoidData, RFX_MSG_REQUEST_SET_INITIAL_ATTACH_APN);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxLceStatusResponseData, RFX_MSG_REQUEST_START_LCE);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxLceStatusResponseData, RFX_MSG_REQUEST_STOP_LCE);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxLceDataResponseData, RFX_MSG_REQUEST_PULL_LCEDATA);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData, RFX_MSG_REQUEST_SET_LTE_ACCESS_STRATUM_REPORT);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData, RFX_MSG_REQUEST_SET_LTE_UPLINK_DATA_TRANSFER);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxFdModeData, RfxVoidData, RFX_MSG_REQUEST_SET_FD_MODE);
RFX_REGISTER_DATA_TO_URC_ID(RfxDataCallResponseData, RFX_MSG_URC_DATA_CALL_LIST_CHANGED);
RFX_REGISTER_DATA_TO_URC_ID(RfxLceDataResponseData, RFX_MSG_URC_LCEDATA_RECV);
RFX_REGISTER_DATA_TO_URC_ID(RfxIntsData, RFX_MSG_URC_LTE_ACCESS_STRATUM_STATE_CHANGE);
RFX_REGISTER_DATA_TO_URC_ID(RfxVoidData, RFX_MSG_URC_MD_DATA_RETRY_COUNT_RESET);

RtcDataController::RtcDataController() :
    isUnderCapabilitySwitch(false) {
}

RtcDataController::~RtcDataController() {
}

void RtcDataController::onInit() {
    RfxController::onInit();  // Required: invoke super class implementation
    RFX_LOG_D(RFX_LOG_TAG, "[%d][%s] enter", m_slot_id, __FUNCTION__);

    int modemOffState = getNonSlotScopeStatusManager()->getIntValue(
        RFX_STATUS_KEY_MODEM_OFF_STATE, MODEM_OFF_IN_IDLE);
    isUnderCapabilitySwitch = (modemOffState == MODEM_OFF_BY_SIM_SWITCH) ? true : false;

    const int request_id_list[] = {
        RFX_MSG_REQUEST_SYNC_DATA_SETTINGS_TO_MD,
        RFX_MSG_REQUEST_RESET_MD_DATA_RETRY_COUNT,
        RFX_MSG_REQUEST_START_LCE,
        RFX_MSG_REQUEST_STOP_LCE,
        RFX_MSG_REQUEST_PULL_LCEDATA,
        RFX_MSG_REQUEST_SETUP_DATA_CALL,
        RFX_MSG_REQUEST_DEACTIVATE_DATA_CALL,
        RFX_MSG_REQUEST_DATA_CALL_LIST,
        RFX_MSG_REQUEST_LAST_DATA_CALL_FAIL_CAUSE,
        RFX_MSG_REQUEST_SET_DATA_PROFILE,
        RFX_MSG_REQUEST_SET_INITIAL_ATTACH_APN,
        RFX_MSG_REQUEST_SET_LTE_ACCESS_STRATUM_REPORT,
        RFX_MSG_REQUEST_SET_LTE_UPLINK_DATA_TRANSFER,
        RFX_MSG_REQUEST_SET_FD_MODE,
    };

    registerToHandleRequest(request_id_list,
            sizeof(request_id_list) / sizeof(const int));

    registerForStatusChange();
}

void RtcDataController::registerForStatusChange() {
    RFX_LOG_D(RFX_LOG_TAG, "[%d][%s] enter", m_slot_id, __FUNCTION__);
    getStatusManager()->registerStatusChanged(RFX_STATUS_KEY_WORLD_MODE_STATE,
        RfxStatusChangeCallback(this, &RtcDataController::onWorldModeStateChanged));

    getNonSlotScopeStatusManager()->registerStatusChanged(RFX_STATUS_KEY_MODEM_OFF_STATE,
        RfxStatusChangeCallback(this, &RtcDataController::onModemOffStateChanged));
}

void RtcDataController::onWorldModeStateChanged(RfxStatusKeyEnum key,
    RfxVariant old_value, RfxVariant value) {
    RFX_UNUSED(key);
    int newValue = value.asInt();
    int oldValue = old_value.asInt();
    RFX_LOG_I(RFX_LOG_TAG, "[%d][%s] old = %d, new = %d",
            m_slot_id, __FUNCTION__, oldValue, newValue);
    if (newValue == WORLD_MODE_SWITCHING) {
        sp<RfxMessage> reqToRild = RfxMessage::obtainRequest(m_slot_id,
                RFX_MSG_REQUEST_CLEAR_ALL_PDN_INFO, RfxVoidData());
        requestToMcl(reqToRild);
    }
}

void RtcDataController::onModemOffStateChanged(RfxStatusKeyEnum key,
    RfxVariant old_value, RfxVariant value) {
    RFX_UNUSED(key);
    int newValue = value.asInt();
    int oldValue = old_value.asInt();
    if (newValue == MODEM_OFF_BY_SIM_SWITCH) {
        RFX_LOG_I(RFX_LOG_TAG, "[%d][%s] Enter Sim switch state", m_slot_id, __FUNCTION__);
        isUnderCapabilitySwitch = true;
    } else {
        if (isUnderCapabilitySwitch) {
            RFX_LOG_D(RFX_LOG_TAG, "[%d][%s] Leave Sim switch state", m_slot_id, __FUNCTION__);
            char no_reset_support[RFX_PROPERTY_VALUE_MAX] = { 0 };
            rfx_property_get("ril.simswitch.no_reset_support", no_reset_support, "0");
            if (strcmp(no_reset_support, "1")==0) {
                sp<RfxMessage> reqToRild = RfxMessage::obtainRequest(m_slot_id,
                    RFX_MSG_REQUEST_RESEND_SYNC_DATA_SETTINGS_TO_MD, RfxVoidData());
                reqToRild->setAddAtFront(true);
                requestToMcl(reqToRild);
            }
        }
        isUnderCapabilitySwitch = false;
    }
}

void RtcDataController::onDeinit() {
    RFX_LOG_D(RFX_LOG_TAG, "[%d][%s] enter", m_slot_id, __FUNCTION__);
    RfxController::onDeinit();
}

bool RtcDataController::onHandleRequest(const sp<RfxMessage>& message) {
    int msg_id = message->getId();
    RFX_LOG_D(RFX_LOG_TAG, "[%d][%s] requestId: %s",
            m_slot_id, __FUNCTION__, idToString(msg_id));

    switch (msg_id) {
        case RFX_MSG_REQUEST_SYNC_DATA_SETTINGS_TO_MD:
            handleSyncDataSettingsToMD(message);
            break;
        case RFX_MSG_REQUEST_RESET_MD_DATA_RETRY_COUNT:
        case RFX_MSG_REQUEST_START_LCE:
        case RFX_MSG_REQUEST_STOP_LCE:
        case RFX_MSG_REQUEST_PULL_LCEDATA:
        case RFX_MSG_REQUEST_SETUP_DATA_CALL:
        case RFX_MSG_REQUEST_DEACTIVATE_DATA_CALL:
        case RFX_MSG_REQUEST_DATA_CALL_LIST:
        case RFX_MSG_REQUEST_LAST_DATA_CALL_FAIL_CAUSE:
        case RFX_MSG_REQUEST_SET_DATA_PROFILE:
        case RFX_MSG_REQUEST_SET_INITIAL_ATTACH_APN:
        case RFX_MSG_REQUEST_SET_LTE_ACCESS_STRATUM_REPORT:
        case RFX_MSG_REQUEST_SET_LTE_UPLINK_DATA_TRANSFER:
        case RFX_MSG_REQUEST_SET_FD_MODE:
            requestToMcl(message);
            break;
        default:
            RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] unknown request, ignore!", m_slot_id, __FUNCTION__);
            break;
    }
    return true;
}

bool RtcDataController::onHandleResponse(const sp<RfxMessage>& message) {
    int msg_id = message->getId();
    RFX_LOG_D(RFX_LOG_TAG, "[%d][%s] responseId: %s",
            m_slot_id, __FUNCTION__ , idToString(msg_id));

    switch (msg_id) {
        case RFX_MSG_REQUEST_SYNC_DATA_SETTINGS_TO_MD:
        case RFX_MSG_REQUEST_RESET_MD_DATA_RETRY_COUNT:
        case RFX_MSG_REQUEST_START_LCE:
        case RFX_MSG_REQUEST_STOP_LCE:
        case RFX_MSG_REQUEST_PULL_LCEDATA:
        case RFX_MSG_REQUEST_SETUP_DATA_CALL:
        case RFX_MSG_REQUEST_DEACTIVATE_DATA_CALL:
        case RFX_MSG_REQUEST_DATA_CALL_LIST:
        case RFX_MSG_REQUEST_LAST_DATA_CALL_FAIL_CAUSE:
        case RFX_MSG_REQUEST_SET_DATA_PROFILE:
        case RFX_MSG_REQUEST_SET_INITIAL_ATTACH_APN:
        case RFX_MSG_REQUEST_SET_LTE_ACCESS_STRATUM_REPORT:
        case RFX_MSG_REQUEST_SET_LTE_UPLINK_DATA_TRANSFER:
        case RFX_MSG_REQUEST_SET_FD_MODE:
            responseToRilj(message);
            break;
        default:
            RFX_LOG_E(RFX_LOG_TAG, "[%d][%s] unknown response, ignore!", m_slot_id, __FUNCTION__);
            break;
    }
    return true;
}

bool RtcDataController::onHandleUrc(const sp<RfxMessage>& message) {
    int msg_id = message->getId();
    RFX_LOG_D(RFX_LOG_TAG, "[%d][%s] urcId: %s", m_slot_id, __FUNCTION__, idToString(msg_id));
    return true;
}

void RtcDataController::handleSyncDataSettingsToMD(const sp<RfxMessage>& message) {
    // For sync the data settings.
    int *pReqData = (int *) message->getData()->getData();
    int reqDataNum = message->getData()->getDataLength() / sizeof(int);

    int defaultDataSelected = SKIP_DATA_SETTINGS; // default data Sim

    if (reqDataNum >= DEFAULT_DATA_SIM + 1) {  // For telephony framework backward comparable.
        defaultDataSelected = pReqData[DEFAULT_DATA_SIM];
    }

    if (defaultDataSelected != SKIP_DATA_SETTINGS) {
        getNonSlotScopeStatusManager()->setIntValue(RFX_STATUS_KEY_DEFAULT_DATA_SIM,
            defaultDataSelected);
    }

    requestToMcl(message);
}

bool RtcDataController::onCheckIfRejectMessage(const sp<RfxMessage>& message,
        bool isModemPowerOff, int radioState) {
    int msgId = message->getId();
    if((radioState == (int)RADIO_STATE_OFF) &&
            (msgId == RFX_MSG_REQUEST_START_LCE ||
             msgId == RFX_MSG_REQUEST_STOP_LCE ||
             msgId == RFX_MSG_REQUEST_PULL_LCEDATA ||
             msgId == RFX_MSG_REQUEST_SYNC_DATA_SETTINGS_TO_MD ||
             msgId == RFX_MSG_REQUEST_SET_DATA_PROFILE ||
             msgId == RFX_MSG_REQUEST_SET_INITIAL_ATTACH_APN ||
            (RtcDataUtils::isWfcSupport() &&
             msgId == RFX_MSG_REQUEST_SETUP_DATA_CALL) ||
            (RtcDataUtils::isWfcSupport() &&
             msgId == RFX_MSG_REQUEST_DEACTIVATE_DATA_CALL))) {
        return false;
    } else if ((radioState == (int)RADIO_STATE_UNAVAILABLE) &&
            (msgId == RFX_MSG_REQUEST_SYNC_DATA_SETTINGS_TO_MD ||
             msgId == RFX_MSG_REQUEST_SET_DATA_PROFILE ||
            (RtcDataUtils::isWfcSupport() &&
             msgId == RFX_MSG_REQUEST_SETUP_DATA_CALL) ||
            (RtcDataUtils::isWfcSupport() &&
             msgId == RFX_MSG_REQUEST_DEACTIVATE_DATA_CALL))) {
        return false;
    }
    return RfxController::onCheckIfRejectMessage(message, isModemPowerOff, radioState);
}

bool RtcDataController::onPreviewMessage(const sp<RfxMessage>& message) {
    if (canHandleRequest(message)) {
        // RFX_LOG_D(RFX_LOG_TAG, "onPreviewMessage: true");
        return true;
    }
    // RFX_LOG_D(RFX_LOG_TAG, "onPreviewMessage: false");
    return false;
}

bool RtcDataController::onCheckIfResumeMessage(const sp<RfxMessage>& message) {
    if (canHandleRequest(message)) {
        // RFX_LOG_D(RFX_LOG_TAG, "onCheckIfResumeMessage: true");
        return true;
    }
    // RFX_LOG_D(RFX_LOG_TAG, "onCheckIfResumeMessage: false");
    return false;
}

bool RtcDataController::canHandleRequest(const sp<RfxMessage>& message) {
    int msgId = message->getId();

    if (msgId == RFX_MSG_REQUEST_SYNC_DATA_SETTINGS_TO_MD ||
            msgId == RFX_MSG_REQUEST_SET_DATA_PROFILE) {
        //check sim switch
        if (isUnderCapabilitySwitch == true) {
            // RFX_LOG_D(RFX_LOG_TAG, "[%s] Is under sim switch, don't process DDS sync to MD.",
                // idToString(msgId));
            return false;
        }
    }
    // RFX_LOG_D(RFX_LOG_TAG, "canHandleRequest [%s] true.", idToString(msgId));
    return true;
}
