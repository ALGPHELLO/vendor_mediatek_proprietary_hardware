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
#include "utils/Timers.h"
#include "rfx_properties.h"
#include "RtcCapabilitySwitchController.h"
#include "RfxRadioCapabilityData.h"
#include "RfxMessageId.h"
#include "RfxRilUtils.h"
#include "RtcCapabilityGetController.h"
#include "RtcCapabilitySwitchUtil.h"

#define RFX_LOG_TAG "RtcCapa"
/*****************************************************************************
 * Class RtcCapabilitySwitchController
 *****************************************************************************/

RFX_IMPLEMENT_CLASS("RtcCapabilitySwitchController", RtcCapabilitySwitchController, RfxController);

RFX_REGISTER_DATA_TO_REQUEST_ID(RfxRadioCapabilityData, RfxRadioCapabilityData, RFX_MSG_REQUEST_SET_RADIO_CAPABILITY);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxRadioCapabilityData, RFX_MSG_REQUEST_GET_RADIO_CAPABILITY);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxIntsData, RFX_MSG_REQUEST_CAPABILITY_SWITCH_SET_MAJOR_SIM);

RtcCapabilitySwitchController::RtcCapabilitySwitchController() :
    m_request_count(0), m_max_capability(0), m_new_main_slot(0) {
    logI(RFX_LOG_TAG, "constructor entered");
}

RtcCapabilitySwitchController::~RtcCapabilitySwitchController() {
}

void RtcCapabilitySwitchController::onInit() {
    RfxController::onInit();

    const int request_id_list[] = {
        RFX_MSG_REQUEST_SET_RADIO_CAPABILITY,
        RFX_MSG_REQUEST_GET_RADIO_CAPABILITY,
        RFX_MSG_REQUEST_CAPABILITY_SWITCH_SET_MAJOR_SIM
    };

    logD(RFX_LOG_TAG, "onInit");
    // register request & URC id list
    // NOTE. one id can only be registered by one controller
    for (int i = 0; i < RfxRilUtils::getSimCount(); i++) {
        registerToHandleRequest(i, request_id_list, sizeof(request_id_list) / sizeof(const int));
    }
    getNonSlotScopeStatusManager()->setIntValue(RFX_STATUS_KEY_CAPABILITY_SWITCH_STATE,
                                CAPABILITY_SWITCH_STATE_IDLE);
}

bool RtcCapabilitySwitchController::isReadyForMessage(const sp<RfxMessage>& message, bool log) {
    int msg_id = message->getId();
    RIL_RadioCapability *capability = NULL;

    if (msg_id == RFX_MSG_REQUEST_SET_RADIO_CAPABILITY) {
        for (int i = RFX_SLOT_ID_0; i < RfxRilUtils::getSimCount(); i++) {
            RadioPowerLock radioLock = (RadioPowerLock) getStatusManager(i)
                    ->getIntValue(RFX_STATUS_KEY_RADIO_LOCK, RADIO_LOCK_IDLE);
            if (radioLock != RADIO_LOCK_IDLE && radioLock != RADIO_LOCK_BY_SIM_SWITCH) {
                if (log) {
                    logI(RFX_LOG_TAG, "Not ready for msg. radioLock: %d", radioLock);
                }
                return false;
            }
            bool ecc_mode = getStatusManager(i)->getBoolValue(RFX_STATUS_KEY_EMERGENCY_MODE,
                                                              false);
            if (ecc_mode) {
                if (log) {
                    logI(RFX_LOG_TAG, "Not ready for msg. in ecc_mode");
                }
                return false;
            }
        }
        capability = (RIL_RadioCapability *)message->getData()->getData();
        int modem_off_state = getNonSlotScopeStatusManager()->getIntValue(
                RFX_STATUS_KEY_MODEM_OFF_STATE, MODEM_OFF_IN_IDLE);
        if (capability->phase == RC_PHASE_START &&
                modem_off_state != MODEM_OFF_IN_IDLE &&
                modem_off_state != MODEM_OFF_BY_SIM_SWITCH) {
            if (log) {
                logI(RFX_LOG_TAG, "Not ready for msg. MODEM_OFF_STATE=%d", modem_off_state);
            }
            return false;
        } else if (capability->phase == RC_PHASE_APPLY &&
                getNonSlotScopeStatusManager()->getIntValue(
                RFX_STATUS_KEY_CAPABILITY_SWITCH_WAIT_MODULE, 0) != 0) {
            if (log) {
                logI(RFX_LOG_TAG, "Not ready for msg. wait module");
            }
            return false;
        }
    }
    return true;
}

bool RtcCapabilitySwitchController::onPreviewMessage(const sp<RfxMessage>& message) {
    return isReadyForMessage(message, true);
}

bool RtcCapabilitySwitchController::onCheckIfResumeMessage(const sp<RfxMessage>& message) {
    return isReadyForMessage(message, false);
}

void RtcCapabilitySwitchController::calculateNewMainSlot(int capability, int slot) {
    int diff = (m_max_capability ^ capability);
    logI(RFX_LOG_TAG, "calculateNewMainSlot, m_max_capability=%d, m_new_main_slot=%d, capability=%d, slot=%d",
         m_max_capability, m_new_main_slot, capability, slot);
    if (diff & RAF_GPRS) {  // RAF_GPRS is used to mark main capability
        if (capability & RAF_GPRS) {
            m_max_capability = capability;
            m_new_main_slot = slot;
        }
    } else if (diff & RAF_LTE) {
        if (capability & RAF_LTE) {
            m_max_capability = capability;
            m_new_main_slot = slot;
        }
    } else if (diff & RAF_TD_SCDMA) {
        if (capability & RAF_TD_SCDMA) {
            m_max_capability = capability;
            m_new_main_slot = slot;
        }
    } else if (diff & RAF_UMTS) {
        if (capability & RAF_UMTS) {
            m_max_capability = capability;
            m_new_main_slot = slot;
        }
    }
}

bool RtcCapabilitySwitchController::onHandleRequest(const sp<RfxMessage> &message) {
    int msg_id = message->getId();
    RIL_RadioCapability *capability = NULL;
    char tempstr[RFX_PROPERTY_VALUE_MAX] = { 0 };
    //logD(RFX_LOG_TAG, "onHandleRequest, handle: %s", idToString(msg_id));
    switch (msg_id) {
        case RFX_MSG_REQUEST_SET_RADIO_CAPABILITY:
            capability = (RIL_RadioCapability *)message->getData()->getData();
            logI(RFX_LOG_TAG, "RadioCapability version=%d, session=%d, phase=%d, rat=%d, logicMD=%d, status=%d",
                 capability->version, capability->session, capability->phase, capability->rat,
                 capability->logicalModemUuid, capability->status);
            switch (capability->phase) {
                case RC_PHASE_START: {
                    if (message->getSlotId() != 0) {
                        responseToRilj(RfxMessage::obtainResponse(RIL_E_SUCCESS, message, true));
                        return true;
                    }
                    int modem_off_state = getNonSlotScopeStatusManager()->getIntValue(
                                              RFX_STATUS_KEY_MODEM_OFF_STATE, MODEM_OFF_IN_IDLE);
                    if (modem_off_state == MODEM_OFF_IN_IDLE) {
                        getNonSlotScopeStatusManager()->setIntValue(RFX_STATUS_KEY_MODEM_OFF_STATE,
                                MODEM_OFF_BY_SIM_SWITCH);
                        for (int i = RFX_SLOT_ID_0; i < RfxRilUtils::getSimCount(); i++) {
                            getStatusManager(i)->setIntValue(RFX_STATUS_KEY_RADIO_LOCK,
                                                             RADIO_LOCK_BY_SIM_SWITCH);
                        }
                        getNonSlotScopeStatusManager()->setIntValue(RFX_STATUS_KEY_CAPABILITY_SWITCH_STATE,
                                CAPABILITY_SWITCH_STATE_START);
                    } else if (modem_off_state == MODEM_OFF_BY_SIM_SWITCH) {
                        // sim switch get the key
                    } else {
                        // do not get the key,return fail to RilJ
                        sp<RfxMessage> un_set_capability_request =
                            RfxMessage::obtainResponse(RIL_E_GENERIC_FAILURE, message, false);
                        responseToRilj(un_set_capability_request);
                        return true;
                    }
                    if (RtcCapabilitySwitchUtil::isDssNoResetSupport() == false) {
                        for (int i = 0; i < RfxRilUtils::getSimCount(); i++) {
                            getStatusManager(i)->setIntValue(RFX_STATUS_KEY_RADIO_STATE,
                                RADIO_STATE_UNAVAILABLE, false, false);
                        }
                        getNonSlotScopeStatusManager()->setInt64Value(
                            RFX_STATUS_KEY_SIM_SWITCH_RADIO_UNAVAIL_TIME,
                            systemTime(SYSTEM_TIME_MONOTONIC), false, false);
                    }
                    m_request_count = 0;
                    m_max_capability = 0;
                    m_new_main_slot = 0;
                    rfx_property_set("ril.rc.session.id1", String8::format("%d", capability->session).string());
                    memset(m_modem_capability, 0, sizeof(m_modem_capability));
                    sp<RfxMessage> set_capability_request =
                        RfxMessage::obtainResponse(RIL_E_SUCCESS, message, true);
                    responseToRilj(set_capability_request);
                    return true;
                }
                case RC_PHASE_APPLY: {
                    m_request_count++;
                    m_modem_capability[message->getSlotId()] = capability->rat;
                    calculateNewMainSlot(capability->rat, message->getSlotId());
                    sp<RfxMessage> set_capability_request =
                        RfxMessage::obtainResponse(RIL_E_SUCCESS, message, true);
                    responseToRilj(set_capability_request);
                    if (m_request_count == RfxRilUtils::getSimCount()) {
                        m_request_count = 0;
                        memset(tempstr, 0, sizeof(tempstr));
                        rfx_property_get("persist.radio.simswitch", tempstr, "1");
                        int current_main_slot = atoi(tempstr) - 1;
                        int msg_data[1];
                        msg_data[0] = m_new_main_slot;
                        sp<RfxMessage> msg = RfxMessage::obtainRequest(current_main_slot, RFX_MSG_REQUEST_CAPABILITY_SWITCH_SET_MAJOR_SIM,
                                             RfxIntsData(msg_data, 1));
                        requestToMcl(msg);
                    }
                    return true;
                }
                case RC_PHASE_FINISH: {
                    sp<RfxMessage> set_capability_request =
                        RfxMessage::obtainResponse(RIL_E_SUCCESS, message, true);
                    responseToRilj(set_capability_request);
                    return true;
                }
                default:
                    sp<RfxMessage> set_capability_request =
                        RfxMessage::obtainResponse(RIL_E_INVALID_ARGUMENTS, message, true);
                    responseToRilj(set_capability_request);
                    return true;
            }
            break;
        case RFX_MSG_REQUEST_GET_RADIO_CAPABILITY:
            requestToMcl(message);
            break;
        default:
            break;
    }
    return true;
}

void RtcCapabilitySwitchController::processSetMajorSimResponse(const sp<RfxMessage> &message) {
    char property_value[RFX_PROPERTY_VALUE_MAX] = { 0 };
    int session_id;

    rfx_property_get("ril.rc.session.id1", property_value, "-1");
    session_id = atoi(property_value);
    if (message->getError() != RIL_E_SUCCESS) {
        if (session_id != -1) {
            int msg_data[1];
            msg_data[0] = m_new_main_slot;
            //retry if session hasn't been terminated
            sp<RfxMessage> msg = RfxMessage::obtainRequest(message->getSlotId(), message->getId(),
                                                RfxIntsData(msg_data, 1));
            requestToMcl(msg);
        }
    } else {
        int modem_off_state = getNonSlotScopeStatusManager()->getIntValue(
                                  RFX_STATUS_KEY_MODEM_OFF_STATE, MODEM_OFF_IN_IDLE);
        if (modem_off_state == MODEM_OFF_BY_SIM_SWITCH) {
            for (int i = RFX_SLOT_ID_0; i < RfxRilUtils::getSimCount(); i++) {
                getStatusManager(i)->setIntValue(RFX_STATUS_KEY_RADIO_LOCK,
                                                 RADIO_LOCK_IDLE);
            }
            getNonSlotScopeStatusManager()->setIntValue(RFX_STATUS_KEY_MODEM_OFF_STATE,
                    MODEM_OFF_IN_IDLE);
            getNonSlotScopeStatusManager()->setIntValue(RFX_STATUS_KEY_CAPABILITY_SWITCH_STATE,
                    CAPABILITY_SWITCH_STATE_IDLE);
        }
    }
}

bool RtcCapabilitySwitchController::onHandleResponse(const sp<RfxMessage> &message) {
    int msg_id = message->getId();

    logI(RFX_LOG_TAG, "onHandleResponse:%s", idToString(msg_id));
    switch (msg_id) {
        case RFX_MSG_REQUEST_CAPABILITY_SWITCH_SET_MAJOR_SIM: {
            processSetMajorSimResponse(message);
            break;
        }
        case RFX_MSG_REQUEST_GET_RADIO_CAPABILITY: {
            responseToRilj(message);
            break;
        }
        default:
            break;
    }
    return true;
}

bool RtcCapabilitySwitchController::onCheckIfRejectMessage(const sp<RfxMessage>& message,
        bool isModemPowerOff, int radioState) {
    RFX_UNUSED(isModemPowerOff);
    RFX_UNUSED(radioState);
    int msg_id = message->getId();
    RIL_RadioCapability *capability = NULL;
    //logD(RFX_LOG_TAG, "onCheckIfRejectMessage, msg_id: %s", idToString(msg_id));
    if (msg_id == RFX_MSG_REQUEST_GET_RADIO_CAPABILITY ||
        msg_id == RFX_MSG_REQUEST_SET_RADIO_CAPABILITY ||
        msg_id == RFX_MSG_REQUEST_CAPABILITY_SWITCH_SET_MAJOR_SIM) {
        return false;
    } else {
        return true;
    }
}

