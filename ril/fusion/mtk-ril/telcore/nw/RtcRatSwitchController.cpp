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
#include "RtcRatSwitchController.h"
#include <cutils/properties.h>

/*****************************************************************************
 * Class RfxController
 *****************************************************************************/

#define RAT_CTRL_TAG "RtcRatSwCtrl"

RFX_IMPLEMENT_CLASS("RtcRatSwitchController", RtcRatSwitchController, RfxController);

RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData, RFX_MSG_REQUEST_SET_PREFERRED_NETWORK_TYPE);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxIntsData, RFX_MSG_REQUEST_GET_PREFERRED_NETWORK_TYPE);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxIntsData, RFX_MSG_REQUEST_VOICE_RADIO_TECH);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxIntsData, RFX_MSG_REQUEST_GET_GMSS_RAT_MODE);

RFX_REGISTER_DATA_TO_URC_ID(RfxIntsData, RFX_MSG_URC_VOICE_RADIO_TECH_CHANGED);

#ifndef RIL_LOCAL_REQUEST_OEM_HOOK_ATCI_INTERNAL
#define RIL_LOCAL_REQUEST_OEM_HOOK_ATCI_INTERNAL 1
#endif
bool RtcRatSwitchController::sIsInSwitching = false;

RtcRatSwitchController::RtcRatSwitchController() :
    mDefaultNetworkType(-1),
    mCurPreferedNetWorkType(-1),
    mPhoneMode(RADIO_TECH_UNKNOWN),
    mNwsMode(NWS_MODE_CSFB) {
}

RtcRatSwitchController::~RtcRatSwitchController() {
}

void RtcRatSwitchController::onInit() {
    // Required: invoke super class implementation
    RfxController::onInit();
    RFX_LOG_V(RAT_CTRL_TAG, "[onInit]");
    const int request_id_list[] = {
        RFX_MSG_REQUEST_SET_PREFERRED_NETWORK_TYPE,
        RFX_MSG_REQUEST_GET_PREFERRED_NETWORK_TYPE,
        RFX_MSG_REQUEST_VOICE_RADIO_TECH,
        RFX_MSG_REQUEST_GET_GMSS_RAT_MODE
    };

    const int atci_request_id_list[] = {
        RIL_LOCAL_REQUEST_OEM_HOOK_ATCI_INTERNAL
    };

    const int urc_id_list[] = {
        RFX_MSG_URC_GMSS_RAT_CHANGED
    };

    // register request & URC id list
    // NOTE. one id can only be registered by one controller
    registerToHandleRequest(request_id_list, sizeof(request_id_list)/sizeof(const int), DEFAULT);
    registerToHandleRequest(atci_request_id_list,
            sizeof(atci_request_id_list)/sizeof(const int), HIGHEST);
    registerToHandleUrc(urc_id_list, sizeof(urc_id_list)/sizeof(const int));

    getStatusManager()->setIntValue(RFX_STATUS_KEY_PREFERRED_NW_TYPE, mCurPreferedNetWorkType);
    getStatusManager()->setBoolValue(RFX_STATUS_KEY_IS_RAT_MODE_SWITCHING, false);

    for (int slotId = RFX_SLOT_ID_0; slotId < RFX_SLOT_COUNT; slotId++) {
        getStatusManager(slotId)->registerStatusChangedEx(RFX_STATUS_KEY_AP_VOICE_CALL_COUNT,
                RfxStatusChangeCallbackEx(this, &RtcRatSwitchController::onApVoiceCallCountChanged));
    }
}

bool RtcRatSwitchController::onHandleRequest(const sp<RfxMessage>& message) {
    int msg_id = message->getId();
    // logD(RAT_CTRL_TAG, "[onHandleRequest] %s", RFX_ID_TO_STR(msg_id));

    switch (msg_id) {
        case RFX_MSG_REQUEST_SET_PREFERRED_NETWORK_TYPE:
            setPreferredNetworkType(message);
            break;
        case RFX_MSG_REQUEST_GET_PREFERRED_NETWORK_TYPE:
            getPreferredNetworkType(message);
            break;
        case RFX_MSG_REQUEST_VOICE_RADIO_TECH:
            requestVoiceRadioTech(message);
            break;
        default:
            break;
    }
    return true;
}

bool RtcRatSwitchController::onHandleUrc(const sp<RfxMessage>& message) {
    int msg_id = message->getId();
    logD(RAT_CTRL_TAG, "[onHandleUrc] %s", RFX_ID_TO_STR(msg_id));
    switch (msg_id) {
        case RFX_MSG_URC_GMSS_RAT_CHANGED:
            handleGmssRatChanged(message);
            return true;
        default:
            logW(RAT_CTRL_TAG, "[onHandleUrc] default case");
            break;
    }
    return true;
}

bool RtcRatSwitchController::onHandleResponse(const sp<RfxMessage>& response) {
    int msg_id = response->getId();

    switch (msg_id) {
        case RFX_MSG_REQUEST_SET_PREFERRED_NETWORK_TYPE:
            responseSetPreferredNetworkType(response);
            return true;
        case RFX_MSG_REQUEST_GET_PREFERRED_NETWORK_TYPE:
            responseGetPreferredNetworkType(response);
            return true;
        case RFX_MSG_REQUEST_VOICE_RADIO_TECH:
            responseGetVoiceRadioTech(response);
            return true;
        case RFX_MSG_REQUEST_GET_GMSS_RAT_MODE:
            responseGetGmssRatMode(response);
            return true;
        default:
            logW(RAT_CTRL_TAG, "[onHandleResponse] %s", RFX_ID_TO_STR(msg_id));
            break;
    }
    return false;
}

bool RtcRatSwitchController::onHandleAtciRequest(const sp<RfxMessage>& message) {
    int msg_id = message->getId();
    int dataLength;
    const char *data;
    char *pString = (char *)message->getData();

    switch (msg_id) {
        case RIL_LOCAL_REQUEST_OEM_HOOK_ATCI_INTERNAL:
            data = &pString[0];
            dataLength = strlen(data);

            logD(RAT_CTRL_TAG, "Inject AT command %s (length:%d)", data, dataLength);
            if (dataLength > 0 && strncmp(data, "AT+ERAT=", 8) == 0) {
                int rat = -1;
                int pref_rat = -1;
                int targetSlotId = 0;
                char simNo[RFX_PROPERTY_VALUE_MAX] = {0};
                rfx_property_get("persist.service.atci.sim", simNo, "0");
                logD(RAT_CTRL_TAG, "[onHandleAtciRequest] simNo: %c", simNo[0]);

                if (simNo[0] == '0') {
                    targetSlotId = 0;
                } else if (simNo[0] == '1') {
                    targetSlotId = 1;
                } else {
                    logD(RAT_CTRL_TAG, "Not support slot: %d", simNo[0]);
                    break;
                }
                if (targetSlotId == m_slot_id) {
                    sscanf(data, "AT+ERAT=%d", &rat);
                    switch (rat) {
                        case 0:
                            mCurPreferedNetWorkType = PREF_NET_TYPE_GSM_ONLY;
                            break;
                        case 1:
                            mCurPreferedNetWorkType = PREF_NET_TYPE_WCDMA;
                            break;
                        case 2:
                            mCurPreferedNetWorkType = PREF_NET_TYPE_GSM_WCDMA;
                            break;
                        case 3:
                            mCurPreferedNetWorkType = PREF_NET_TYPE_LTE_ONLY;
                            break;
                        case 4:
                            mCurPreferedNetWorkType = PREF_NET_TYPE_LTE_GSM;
                            break;
                        case 5:
                            mCurPreferedNetWorkType = PREF_NET_TYPE_LTE_WCDMA;
                            break;
                        case 6:
                            mCurPreferedNetWorkType = PREF_NET_TYPE_LTE_GSM_WCDMA;
                            break;
                        case 7:
                            mCurPreferedNetWorkType = PREF_NET_TYPE_CDMA_EVDO_AUTO;
                            break;
                        case 8:
                            mCurPreferedNetWorkType = PREF_NET_TYPE_CDMA_EVDO_GSM;
                            break;
                        case 10:
                            mCurPreferedNetWorkType = PREF_NET_TYPE_GSM_WCDMA_CDMA_EVDO_AUTO;
                            break;
                        case 11:
                            mCurPreferedNetWorkType = PREF_NET_TYPE_LTE_CDMA_EVDO;
                            break;
                        case 12:
                            mCurPreferedNetWorkType = PREF_NET_TYPE_LTE_CDMA_EVDO_GSM;
                            break;
                        case 14:
                            mCurPreferedNetWorkType = PREF_NET_TYPE_LTE_CMDA_EVDO_GSM_WCDMA;
                            break;
                        default:
                            break;
                    }
                    getStatusManager()->setIntValue(RFX_STATUS_KEY_PREFERRED_NW_TYPE,
                            mCurPreferedNetWorkType);
                }
                logD(RAT_CTRL_TAG, "[onHandleAtciRequest] mCurPreferedNetWorkType=%d",
                        mCurPreferedNetWorkType);
            } else {
                logW(RAT_CTRL_TAG, "[onHandleAtciRequest] length=0");
            }
            break;

        default:
            break;
    }
    return false;
}

void RtcRatSwitchController::setPreferredNetworkType(const sp<RfxMessage>& message) {
    if (isChipTestMode()) {
        logD(RAT_CTRL_TAG, "ChipTest! setPrefNwType not executed!");
        sp<RfxMessage> resToRilj = RfxMessage::obtainResponse(RIL_E_GENERIC_FAILURE, message, false);
        responseToRilj(resToRilj);
    } else {
        // there is no sim, so ignore it
        int cardType = getStatusManager(m_slot_id)->getIntValue(
            RFX_STATUS_KEY_CARD_TYPE, 0);
        if (cardType == CARD_TYPE_NONE) {
            logD(RAT_CTRL_TAG, "setPreferredNetworkType: return directly because no sim.");
            sp<RfxMessage> resToRilj = RfxMessage::obtainResponse(
                    RIL_E_INTERNAL_ERR, message, false);
            responseToRilj(resToRilj);
            return;
        }

        int nwType = ((int *)message->getData()->getData())[0];
        // No matter switch success or fail, always record the rilj prefer network type in system property.
        // This is consistent with the UI behavior.
        // logD(RAT_CTRL_TAG, "[setPrefNwType] from RILJ, rilj_nw_type set to %d", nwType);
        setPreferredNetWorkTypeToSysProp(m_slot_id, nwType);

        switchNwRat(nwType, RAT_SWITCH_NORMAL, NULL, message);
    }
}

void RtcRatSwitchController::setPreferredNetworkType(RatSwitchInfo ratSwtichInfo) {
    if (isChipTestMode()) {
         logD(RAT_CTRL_TAG, "ChipTest! setPreferredNetworkType not executed!");
         mRatSwitchSignal.emit(m_slot_id, RIL_E_SUCCESS);
         return;
    }
    RFX_LOG_V(RAT_CTRL_TAG, "[setPreferredNetworkType] ratSwtichInfo.card_type :%d, "
                    "ratSwtichInfo.card_state: %d, ratSwtichInfo.rat_mode: %d, ratSwtichInfo.isCt3GDualMode: %s, ratSwtichInfo.ct3gStatus : %d. ",
            ratSwtichInfo.card_type, ratSwtichInfo.card_state,
            ratSwtichInfo.rat_mode,
            ratSwtichInfo.isCt3GDualMode ? "true" : "false",
            ratSwtichInfo.ct3gStatus);
    //Only card not plug in/out and not occur sim switch, the card state will be card_state_no_changed
    int defaultNetworkType = calculateDefaultNetworkType(ratSwtichInfo);
    if (ratSwtichInfo.card_state == CARD_STATE_NO_CHANGED && defaultNetworkType == mDefaultNetworkType) {
        logD(RAT_CTRL_TAG, "Card state no changed! setPreferredNetworkType not executed!");
        mRatSwitchSignal.emit(m_slot_id, RIL_E_SUCCESS);
        return;
    }

    /* In no SIM case.
     * 1. Switch RAT if ECC retry.
     * 2. Swithc RAT if card not hot plug case
         a. First time boot up
         b. RIL proxy restart
     * 3. Skip RAT switch if C capability does not change
     */
    if (ratSwtichInfo.card_type == 0
            && !(ratSwtichInfo.rat_mode != RAT_MODE_INVALID  // not ECC retry
            || ratSwtichInfo.card_state == CARD_STATE_NOT_HOT_PLUG)) {  // not hot plug case.
        int slotCapability = getStatusManager(m_slot_id)->getIntValue(
                RFX_STATUS_KEY_SLOT_CAPABILITY, 0);
        int PreviousRaf = getSlotCapability(m_slot_id);
        // logD(RAT_CTRL_TAG, "slotCapability:%d PreviousRaf:%d ", slotCapability, PreviousRaf);
        if (getSlotCapability(m_slot_id) != -1
                && !(isRafContainsCdma(PreviousRaf) ^ isRafContainsCdma(slotCapability))) {
            // case 3. do not change RAT.
            logD(RAT_CTRL_TAG, "Skip switch if C2K no change, slotCapability:%d PreviousRaf:%d ",
                    slotCapability, PreviousRaf);
            setSlotCapability(m_slot_id, getStatusManager(m_slot_id)->getIntValue(
                    RFX_STATUS_KEY_SLOT_CAPABILITY, 0));
            mRatSwitchSignal.emit(m_slot_id, RIL_E_SUCCESS);
            return;
        }
    }

    // case 1, 2. change RAT.
    setSlotCapability(m_slot_id, getStatusManager(m_slot_id)->getIntValue(
            RFX_STATUS_KEY_SLOT_CAPABILITY, 0));
    mRatSettings.prefNwTypeDefault = defaultNetworkType;
    int defaultRaf = RtcCapabilitySwitchUtil::getRafFromNetworkType(defaultNetworkType);
    int rafFromRiljNetworkType = RtcCapabilitySwitchUtil::getRafFromNetworkType(
            getPreferredNetWorkTypeFromSysProp(m_slot_id));
    int targetRaf = defaultRaf;

    if (isNewSimCard(m_slot_id)) {
        setPreferredNetWorkTypeToSysProp(m_slot_id, -1);
    } else {
        targetRaf = (defaultRaf & rafFromRiljNetworkType);
        if (targetRaf == 0) {
            logD(RAT_CTRL_TAG, "[setPreferredNetworkType] Raf filter result is 0, "
                    "so use defaultRaf as targetRaf");
            targetRaf = defaultRaf;
        }
    }

    if (ratSwtichInfo.isCt3GDualMode
            && ratSwtichInfo.card_state == CARD_STATE_CARD_TYPE_CHANGED
            && isRafContainsCdma(targetRaf)) {
        if (ratSwtichInfo.ct3gStatus == GMSS_TRIGGER_SWITCH_SIM) {
            mNwsMode = NWS_MODE_CSFB;
        } else if (ratSwtichInfo.ct3gStatus == GMSS_TRIGGER_SWITCH_RUIM) {
            mNwsMode = NWS_MODE_CDMALTE;
        } else {
            mNwsMode = NWS_MODE_CDMALTE;
        }
    } else if (isRafContainsCdma(targetRaf)) {
        mNwsMode = NWS_MODE_CDMALTE;
    } else {
        mNwsMode = NWS_MODE_CSFB;
    }

    int targetNetworkType = RtcCapabilitySwitchUtil::getNetworkTypeFromRaf(targetRaf);
    logD(RAT_CTRL_TAG, "[setPreferredNetworkType] rafFromRiljNetworkType: %d, targetRaf: %d, targetNetworkType: %d, "
             "mNwsMode: %d. ",
            rafFromRiljNetworkType, targetRaf, targetNetworkType, mNwsMode);
    switchNwRat(targetNetworkType, RAT_SWITCH_INIT, NULL, NULL);
}

void RtcRatSwitchController::setPreferredNetworkType(const int prefNwType,
        const sp<RfxAction>& action) {
    if (prefNwType == -1) {
        logD(RAT_CTRL_TAG, "[setRestrictedNetworkMode] leaving restricted mode");
        mPendingRestrictedRatSwitchRecord.prefNwType = -1;
        if (isChipTestMode()) {
            // In chip test, original mode is stored by read atci injected ERAT
            logD(RAT_CTRL_TAG, "[setRestrictedNetworkMode] before execute"
                    " nwType:%d", mCurPreferedNetWorkType);
            switchNwRat(mCurPreferedNetWorkType, RAT_SWITCH_RESTRICT, action, NULL);
        } else {
            // Change caller to restrict to avoid request can't work before exit ECBM or emergency mode.
            if (mPendingInitRatSwitchRecord.prefNwType != -1) {
                logD(RAT_CTRL_TAG, "[setRestrictedNetworkMode] Init pending record in queue");
                switchNwRat(mPendingInitRatSwitchRecord.prefNwType,
                        RAT_SWITCH_INIT, action, NULL);
            } else if (mPendingNormalRatSwitchRecord.prefNwType != -1) {
                // Filter nw type avoid use nw type to switch without capability filter.
                int targetPrefNwType = filterPrefNwTypeFromRilj(
                        mPendingNormalRatSwitchRecord.prefNwType);
                logD(RAT_CTRL_TAG, "[setRestrictedNetworkMode] Norm pending record in queue");
                switchNwRat(targetPrefNwType,
                        RAT_SWITCH_NORMAL, action, mPendingNormalRatSwitchRecord.message);
            } else {
                logD(RAT_CTRL_TAG, "[setRestrictedNetworkMode] No pending record in queue");
                action->act();
                processPendingRatSwitchRecord();
            }
        }
    } else {
        logD(RAT_CTRL_TAG, "[setRestrictedNetworkMode] entering restricted mode: %d",
                prefNwType);
        if (mPendingInitRatSwitchRecord.prefNwType == -1 &&
                mPendingNormalRatSwitchRecord.prefNwType == -1) {
            // save current state to pending queue if no init rat switch in queue.
            queueRatSwitchRecord(mCurPreferedNetWorkType, RAT_SWITCH_NORMAL, NULL, NULL);
        }
        switchNwRat(prefNwType, RAT_SWITCH_RESTRICT, action, NULL);
    }
}

void RtcRatSwitchController::switchNwRat(int prefNwType, const RatSwitchCaller ratSwitchCaller,
        const sp<RfxAction>& action, const sp<RfxMessage>& message) {

    RFX_LOG_V(RAT_CTRL_TAG, "[switchNwRat] CurPreferedNwType: %d, prefNwType: %d,"
            " ratSwitchCaller: %s, sIsInSwitching: %s",
            mCurPreferedNetWorkType, prefNwType, switchCallerToString(ratSwitchCaller),
            sIsInSwitching ? "true" : "false");

    if (sIsInSwitching) {
        queueRatSwitchRecord(prefNwType, ratSwitchCaller, action, message);
    } else if ((ratSwitchCaller == RAT_SWITCH_NORMAL || ratSwitchCaller == RAT_SWITCH_INIT) &&
                isAPInCall() == true) {

        /* In call case.
         * 1. Init case:   return operation not allowed to mode controller.
         * 2. Normal case: return success to fwk and quene switch rat until AP call count is 0.
         */
        if (ratSwitchCaller == RAT_SWITCH_INIT) {
            mRatSwitchSignal.emit(m_slot_id, RIL_E_OPERATION_NOT_ALLOWED);
        } else {
            if (action != NULL) {
                action->act();
            }
            if (message != NULL) {
                sp<RfxMessage> resToRilj = RfxMessage::obtainResponse(RIL_E_SUCCESS, message);
                responseToRilj(resToRilj);
            }
            queueRatSwitchRecord(prefNwType, ratSwitchCaller, action, NULL);
        }
        logD(RAT_CTRL_TAG, "[switchNwRat] in call, do not set rat!");
    } else {
        sp<RfxMessage> resToRilj;
        int targetPrefNwType = prefNwType;

        if (ratSwitchCaller == RAT_SWITCH_NORMAL) {
            targetPrefNwType = filterPrefNwTypeFromRilj(prefNwType);

            if (targetPrefNwType == -1) {
                logD(RAT_CTRL_TAG, "[switchNwRat] from RILJ, invalid nwType:%d", prefNwType);
                if (message != NULL) {
                    sp<RfxMessage> resToRilj = RfxMessage::obtainResponse(RIL_E_GENERIC_FAILURE,
                            message);
                    responseToRilj(resToRilj);
                }
                processPendingRatSwitchRecord();
                return;
            }
        }

        if (targetPrefNwType == mCurPreferedNetWorkType && ratSwitchCaller != RAT_SWITCH_INIT) {
            // logD(RAT_CTRL_TAG, "[switchNwRat] Already in desired mode, switch not executed");
            if (action != NULL) {
                action->act();
            }
            if (message != NULL) {
                resToRilj = RfxMessage::obtainResponse(RIL_E_SUCCESS, message);
                responseToRilj(resToRilj);
            }
            processPendingRatSwitchRecord();
            return;
        }
        logD(RAT_CTRL_TAG, "[switchNwRat] ratSwitchCaller: %s, mCurPreferedNetWorkType: %d, "
                "mDefaultNetworkType: %d, prefNwTypeFromRilj: %d, "
                "targetPrefNwType: %d, GsmOnlySim: %d",
                switchCallerToString(ratSwitchCaller),
                mCurPreferedNetWorkType, mDefaultNetworkType,
                prefNwType, targetPrefNwType, isGsmOnlySim());

        sIsInSwitching = true;
        getStatusManager()->setBoolValue(RFX_STATUS_KEY_IS_RAT_MODE_SWITCHING, true);
        if (ratSwitchCaller == RAT_SWITCH_NORMAL) {
            mRatSettings.prefNwTypeFromRilj = prefNwType;
        }
        mRatSettings.prefNwType = targetPrefNwType;
        mRatSettings.ratSwitchCaller = ratSwitchCaller;
        mRatSettings.action = action;
        mRatSettings.message = message;

        int targetRaf = RtcCapabilitySwitchUtil::getRafFromNetworkType(targetPrefNwType);
        if (mRatSettings.ratSwitchCaller == RAT_SWITCH_INIT
                && isRafContainsCdma(targetRaf) && isRafContainsGsm(targetRaf)) {
            getGmssRatMode();
        } else {
            configRatMode();
        }
    }
}

void RtcRatSwitchController::configRatMode() {

    sp<RfxMessage> reqToRild = RfxMessage::obtainRequest(m_slot_id,
            RFX_MSG_REQUEST_SET_PREFERRED_NETWORK_TYPE, RfxIntsData(&mRatSettings.prefNwType, 1));
    requestToMcl(reqToRild);
}

void RtcRatSwitchController::queueRatSwitchRecord(int prefNwType,
        const RatSwitchCaller ratSwitchCaller,
        const sp<RfxAction>& action, const sp<RfxMessage>& message) {
    /* Pending if in switching. */
    RFX_LOG_V(RAT_CTRL_TAG, "queueRatSwitchRecord(), ratSwitchCaller:%d prefNwType:%d",
            ratSwitchCaller, prefNwType);
    if (ratSwitchCaller == RAT_SWITCH_RESTRICT) {
        mPendingRestrictedRatSwitchRecord.prefNwType = prefNwType;
        mPendingRestrictedRatSwitchRecord.ratSwitchCaller = ratSwitchCaller;
        mPendingRestrictedRatSwitchRecord.action = action;
        mPendingRestrictedRatSwitchRecord.message = message;
    } else if (ratSwitchCaller == RAT_SWITCH_INIT) {
        mPendingInitRatSwitchRecord.prefNwType = prefNwType;
        mPendingInitRatSwitchRecord.ratSwitchCaller = ratSwitchCaller;
        mPendingInitRatSwitchRecord.action = action;
        mPendingInitRatSwitchRecord.message = message;
    } else {
        if (mPendingNormalRatSwitchRecord.prefNwType != -1
                && mPendingNormalRatSwitchRecord.message != NULL) {
            RFX_LOG_V(RAT_CTRL_TAG, "queueRatSwitchRecord(), set prefer network type is pending, "
                    "will be ignored, send response.");
            sp<RfxMessage> resToRilj = RfxMessage::obtainResponse(
                    RIL_E_GENERIC_FAILURE, mPendingNormalRatSwitchRecord.message);
            responseToRilj(resToRilj);
        }
        mPendingNormalRatSwitchRecord.prefNwType = prefNwType;
        mPendingNormalRatSwitchRecord.ratSwitchCaller = ratSwitchCaller;
        mPendingNormalRatSwitchRecord.action = action;
        mPendingNormalRatSwitchRecord.message = message;
    }
}

void RtcRatSwitchController::processPendingRatSwitchRecord() {
    // param for INIT, NOR switch caller use, clean pending nw type after request is triggered
    int prefNwType = -1;
    if (mPendingRestrictedRatSwitchRecord.prefNwType != -1) {
        logD(RAT_CTRL_TAG, "[processPendingRestrictedRatSwitchRecord] "
                "prefNwType: %d, ratSwitchCaller: %s",
                mPendingRestrictedRatSwitchRecord.prefNwType,
                switchCallerToString(mPendingRestrictedRatSwitchRecord.ratSwitchCaller));
        prefNwType = mPendingRestrictedRatSwitchRecord.prefNwType;
        mPendingRestrictedRatSwitchRecord.prefNwType = -1;
        switchNwRat(prefNwType,
                mPendingRestrictedRatSwitchRecord.ratSwitchCaller,
                mPendingRestrictedRatSwitchRecord.action,
                mPendingRestrictedRatSwitchRecord.message);
    } else if (mPendingInitRatSwitchRecord.prefNwType != -1) {
        logD(RAT_CTRL_TAG, "[processPendingInitRatSwitchRecord] "
                "prefNwType: %d, ratSwitchCaller: %s",
                mPendingInitRatSwitchRecord.prefNwType,
                switchCallerToString(mPendingInitRatSwitchRecord.ratSwitchCaller));
        prefNwType = mPendingInitRatSwitchRecord.prefNwType;
        mPendingInitRatSwitchRecord.prefNwType = -1;
        switchNwRat(prefNwType,
                mPendingInitRatSwitchRecord.ratSwitchCaller,
                mPendingInitRatSwitchRecord.action,
                mPendingInitRatSwitchRecord.message);
    } else if (mPendingNormalRatSwitchRecord.prefNwType != -1) {
        /* logD(RAT_CTRL_TAG, "[processPendingNormalRatSwitchRecord] "
                "prefNwType: %d, ratSwitchCaller: %s",
                mPendingNormalRatSwitchRecord.prefNwType,
                switchCallerToString(mPendingNormalRatSwitchRecord.ratSwitchCaller)); */
        prefNwType = mPendingNormalRatSwitchRecord.prefNwType;
        mPendingNormalRatSwitchRecord.prefNwType = -1;
        switchNwRat(prefNwType,
                mPendingNormalRatSwitchRecord.ratSwitchCaller,
                mPendingNormalRatSwitchRecord.action,
                mPendingNormalRatSwitchRecord.message);
    } else {
        RtcRatSwitchController *another = (RtcRatSwitchController *) findController(
                m_slot_id == 0 ? 1 : 0, RFX_OBJ_CLASS_INFO(RtcRatSwitchController));
        if (another != NULL && another->hasPendingRecord()) {
            // logD(RAT_CTRL_TAG, "[processPendingRatSwitchRecord] another SIM has pending record");
            another->processPendingRatSwitchRecord();
        } else {
            // logD(RAT_CTRL_TAG, "[processPendingRatSwitchRecord] no pending record");
        }
    }
}

int RtcRatSwitchController::calculateDefaultNetworkType(RatSwitchInfo ratSwtichInfo) {
    int slotCapability = getStatusManager(m_slot_id)->getIntValue(
            RFX_STATUS_KEY_SLOT_CAPABILITY, 0);
    int defaultRaf = slotCapability;

    //For gsm only card(not 3g dual mode card), the default network type can only be pure GSM
    if (isGsmOnlySimFromMode(ratSwtichInfo) == true) {
        defaultRaf &= ~(RAF_CDMA_GROUP | RAF_EVDO_GROUP);
    }

    //For cdma only card, the default network type can only be pure CDMA
    if (isRafContainsCdma(defaultRaf)) {
        if ((((ratSwtichInfo.card_type & CARD_TYPE_RUIM) > 0) || ((ratSwtichInfo.card_type & CARD_TYPE_CSIM) > 0))
                    && (!((ratSwtichInfo.card_type & CARD_TYPE_USIM) > 0))
                    && (!ratSwtichInfo.isCt3GDualMode)) {
            if ((defaultRaf & RAF_EVDO_GROUP) > 0) {
                defaultRaf = RAF_CDMA_GROUP | RAF_EVDO_GROUP;
            } else {
                defaultRaf = RAF_CDMA_GROUP;
            }
        }
    }
    //For ECC suggest rat mode.
    if (ratSwtichInfo.rat_mode != RAT_MODE_INVALID) {
        defaultRaf = slotCapability;
    }
    int defaultNetworkType = RtcCapabilitySwitchUtil::getNetworkTypeFromRaf(
            defaultRaf);

    logD(RAT_CTRL_TAG, "[calculateDefaultNetworkType] card_type: %d, "
            "card_state: %d, rat_mode: %d, isCt3GDualMode: %s, ct3gStatus: %d. "
            " slotCap: %d, slotNwtype: %d, defaultRaf: %d, defNwType: %d.",
            ratSwtichInfo.card_type, ratSwtichInfo.card_state,
            ratSwtichInfo.rat_mode,
            ratSwtichInfo.isCt3GDualMode ? "true" : "false",
            ratSwtichInfo.ct3gStatus,
            slotCapability,
            RtcCapabilitySwitchUtil::getNetworkTypeFromRaf(slotCapability),
            defaultRaf, defaultNetworkType);
    return defaultNetworkType;
}

bool RtcRatSwitchController::hasPendingRecord() {
    if (mPendingInitRatSwitchRecord.prefNwType != -1
            || mPendingNormalRatSwitchRecord.prefNwType != -1
            || mPendingRestrictedRatSwitchRecord.prefNwType != -1) {
        return true;
    }
    return false;
}

void RtcRatSwitchController::getPreferredNetworkType(const sp<RfxMessage>& message) {
    sp<RfxMessage> resToRilj;

    if (isAPInCall() == true &&
        mPendingNormalRatSwitchRecord.prefNwType != -1) {
        logD(RAT_CTRL_TAG, "[handleGetPreferredNwType] in call mode:%d",
                mPendingNormalRatSwitchRecord.prefNwType);
        resToRilj = RfxMessage::obtainResponse(m_slot_id, RFX_MSG_REQUEST_GET_PREFERRED_NETWORK_TYPE,
                RIL_E_SUCCESS, RfxIntsData(&mPendingNormalRatSwitchRecord.prefNwType, 1), message);

        responseToRilj(resToRilj);
        return;
    } else if (sIsInSwitching == true) {
        logD(RAT_CTRL_TAG, "[handleGetPreferredNwType] in RAT switching Desired:%d",
                mRatSettings.prefNwType);
        resToRilj = RfxMessage::obtainResponse(m_slot_id, RFX_MSG_REQUEST_GET_PREFERRED_NETWORK_TYPE,
                RIL_E_SUCCESS, RfxIntsData(&mRatSettings.prefNwType, 1), message);
        responseToRilj(resToRilj);
        return;
    } else {
        requestToMcl(message);
    }
}

void RtcRatSwitchController::requestVoiceRadioTech(const sp<RfxMessage>& message) {
    sp<RfxMessage> resToRilj;
    resToRilj = RfxMessage::obtainResponse(m_slot_id, RFX_MSG_REQUEST_VOICE_RADIO_TECH, RIL_E_SUCCESS,
            RfxIntsData(&mPhoneMode, 1), message);
    responseToRilj(resToRilj);
}

void RtcRatSwitchController::handleGmssRatChanged(const sp<RfxMessage>& message) {
    handleGmssRat(message);
    updatePhoneMode(GMSS_RAT);
}

void RtcRatSwitchController::updateState(int prefNwType, RatSwitchResult switchResult) {
    // logD(RAT_CTRL_TAG, "[updateState] prefNwType: %d, switchResut: %d", prefNwType, switchResult);
    if (switchResult == RAT_SWITCH_SUCC) {
        if (mRatSettings.ratSwitchCaller == RAT_SWITCH_INIT) {
            mDefaultNetworkType = mRatSettings.prefNwTypeDefault;
        }
        mCurPreferedNetWorkType = prefNwType;
        getStatusManager()->setIntValue(RFX_STATUS_KEY_PREFERRED_NW_TYPE,
                mCurPreferedNetWorkType);
    }
    getStatusManager()->setBoolValue(RFX_STATUS_KEY_IS_RAT_MODE_SWITCHING, false);
    sIsInSwitching = false;
}

void RtcRatSwitchController::responseSetPreferredNetworkType(const sp<RfxMessage>& response) {
    RIL_Errno error = response->getError();

    if (error == RIL_E_SUCCESS) {
        updateState(mRatSettings.prefNwType, RAT_SWITCH_SUCC);
        if(mRatSettings.ratSwitchCaller == RAT_SWITCH_INIT){
            mRatSwitchSignal.emit(m_slot_id, RIL_E_SUCCESS);
        }
        if (mRatSettings.action != NULL) {
            mRatSettings.action->act();
        }
        if (mRatSettings.message != NULL) {
            sp<RfxMessage> resToRilj = RfxMessage::obtainResponse(error, mRatSettings.message, false);
            responseToRilj(resToRilj);
        }
        updatePhoneMode(SWITCH_RAT);
        // logD(RAT_CTRL_TAG, "%s switch prefNwType: %d success!",
        //        switchCallerToString(mRatSettings.ratSwitchCaller), mRatSettings.prefNwType);
    } else {
        updateState(mRatSettings.prefNwType, RAT_SWITCH_FAIL);
        if (error == RIL_E_OPERATION_NOT_ALLOWED) {
            //  Queue FWK network type and return success when in call
            //  and no more switch rat from FWK.
            if(mRatSettings.ratSwitchCaller == RAT_SWITCH_NORMAL &&
                    mRatSettings.message == NULL &&
                    mPendingNormalRatSwitchRecord.prefNwType == -1) {
                queueRatSwitchRecord(mRatSettings.prefNwType, RAT_SWITCH_NORMAL, NULL, NULL);
                logD(RAT_CTRL_TAG, "Queue network type: %d in call", mRatSettings.prefNwType);
            }
        }

        if(mRatSettings.ratSwitchCaller == RAT_SWITCH_INIT) {
            mRatSwitchSignal.emit(m_slot_id, error);
        }
        if (mRatSettings.action != NULL) {
            mRatSettings.action->act();
        }
        if (mRatSettings.message != NULL) {
            sp<RfxMessage> resToRilj = RfxMessage::obtainResponse(error, mRatSettings.message, false);
            responseToRilj(resToRilj);
        }
        logD(RAT_CTRL_TAG, "%s switch prefNwType: %d fail!",
                switchCallerToString(mRatSettings.ratSwitchCaller), mRatSettings.prefNwType);
    }
    processPendingRatSwitchRecord();
}

void RtcRatSwitchController::responseGetPreferredNetworkType(const sp<RfxMessage>& response) {
    responseToRilj(response);
}

void RtcRatSwitchController::responseGetVoiceRadioTech(const sp<RfxMessage>& response) {
    responseToRilj(response);
}

void RtcRatSwitchController::updatePhoneMode(PHONE_CHANGE_SOURCE source) {
    RFX_UNUSED(source);
    int tech = RADIO_TECH_UNKNOWN;

    switch (mCurPreferedNetWorkType) {
        case PREF_NET_TYPE_GSM_ONLY:
        case PREF_NET_TYPE_GSM_WCDMA:
        case PREF_NET_TYPE_GSM_WCDMA_AUTO:
        case PREF_NET_TYPE_LTE_GSM_WCDMA:
        case PREF_NET_TYPE_LTE_WCDMA:
        case PREF_NET_TYPE_LTE_GSM:
        case PREF_NET_TYPE_TD_SCDMA_GSM_LTE:
        case PREF_NET_TYPE_TD_SCDMA_GSM_WCDMA_LTE:
        case PREF_NET_TYPE_TD_SCDMA_GSM:
        case PREF_NET_TYPE_TD_SCDMA_GSM_WCDMA:
        case PREF_NET_TYPE_TD_SCDMA_LTE:
        case PREF_NET_TYPE_TD_SCDMA_WCDMA_LTE:
            tech = RADIO_TECH_GPRS;
            break;

        case PREF_NET_TYPE_WCDMA:
        case PREF_NET_TYPE_TD_SCDMA_ONLY:
        case PREF_NET_TYPE_TD_SCDMA_WCDMA:
            tech = RADIO_TECH_UMTS;
            break;

        //  LTE, don't change in C2K card.
        case PREF_NET_TYPE_LTE_ONLY:
            if (true == isGsmOnlySim()) {
                tech = RADIO_TECH_GPRS;
            }
            break;

        case PREF_NET_TYPE_CDMA_ONLY:
        case PREF_NET_TYPE_CDMA_EVDO_AUTO:
        case PREF_NET_TYPE_EVDO_ONLY:
        case PREF_NET_TYPE_LTE_CDMA_EVDO:
            tech = RADIO_TECH_1xRTT;
            break;

        case PREF_NET_TYPE_TD_SCDMA_LTE_CDMA_EVDO_GSM_WCDMA:
        case PREF_NET_TYPE_TD_SCDMA_GSM_WCDMA_CDMA_EVDO_AUTO:
        case PREF_NET_TYPE_GSM_WCDMA_CDMA_EVDO_AUTO:
        case PREF_NET_TYPE_LTE_CMDA_EVDO_GSM_WCDMA:
        case PREF_NET_TYPE_CDMA_GSM:
        case PREF_NET_TYPE_CDMA_EVDO_GSM:
        case PREF_NET_TYPE_LTE_CDMA_EVDO_GSM:
            tech = RADIO_TECH_1xRTT;
            if (mNwsMode == NWS_MODE_CSFB) {
                tech = RADIO_TECH_GPRS;
            }
            break;

        default:
            logW(RAT_CTRL_TAG, "[updatePhoneType] unknown Nw type: %d", mCurPreferedNetWorkType);
            break;
    }

    if (tech != RADIO_TECH_UNKNOWN && mPhoneMode != tech) {
        sp<RfxMessage> urcToRilj;
        mPhoneMode = tech;
        urcToRilj = RfxMessage::obtainUrc(m_slot_id, RFX_MSG_URC_VOICE_RADIO_TECH_CHANGED,
                RfxIntsData(&mPhoneMode, 1));
        responseToRilj(urcToRilj);
        logD(RAT_CTRL_TAG, "[updatePhoneMode] mPhoneMode: %d", mPhoneMode);
    }
}

bool RtcRatSwitchController::onCheckIfRejectMessage(const sp<RfxMessage>& message,
        bool isModemPowerOff,int radioState) {
    int id = message->getId();
    if (RFX_MSG_REQUEST_SET_PREFERRED_NETWORK_TYPE == id) {
        if (RADIO_STATE_UNAVAILABLE == radioState) {
            RtcWpController* wpController =
                    (RtcWpController *)findController(RFX_OBJ_CLASS_INFO(RtcWpController));
            if (wpController->isWorldModeSwitching()) {
                return false;
            } else {
                logD(RAT_CTRL_TAG, "onCheckIfRejectMessage:id=%d,isModemPowerOff=%d,rdioState=%d",
                        message->getId(), isModemPowerOff, radioState);
                return true;
            }
        }
        return false;
    }
    if (RFX_MSG_REQUEST_GET_PREFERRED_NETWORK_TYPE == id
            || RFX_MSG_REQUEST_VOICE_RADIO_TECH == id) {
        if (RADIO_STATE_UNAVAILABLE == radioState) {
            return true;
        }
        return false;
    }
    return true;
}

bool RtcRatSwitchController::onPreviewMessage(const sp<RfxMessage>& message) {
    if (sIsInSwitching == true &&
            message->getId() == RFX_MSG_REQUEST_VOICE_RADIO_TECH) {
        logD(RAT_CTRL_TAG, "onPreviewMessage, put %s into pending list",
                RFX_ID_TO_STR(message->getId()));
        return false;
    } else {
        return true;
    }
}

bool RtcRatSwitchController::onCheckIfResumeMessage(const sp<RfxMessage>& message) {
    if (sIsInSwitching == false) {
        logD(RAT_CTRL_TAG, "resume the request %s",
                RFX_ID_TO_STR(message->getId()));
        return true;
    } else {
        return false;
    }
}

void RtcRatSwitchController::onApVoiceCallCountChanged(int slotId, RfxStatusKeyEnum key,
        RfxVariant old_value, RfxVariant value) {
    int oldMode = old_value.asInt();
    int mode = value.asInt();

    if (mode == 0 && oldMode > 0) {
        if (isAPInCall() == false && hasPendingRecord()) {
            RFX_LOG_V(RAT_CTRL_TAG, "%s, slotId:%d, key:%d oldMode:%d, mode:%d",
                    __FUNCTION__, slotId, key, oldMode, mode);
            processPendingRatSwitchRecord();
        }
    }
}

int RtcRatSwitchController::isAPInCall() {
    int ret = false;

    for (int slotId = RFX_SLOT_ID_0; slotId < RFX_SLOT_COUNT; slotId++) {
        if (getStatusManager(slotId)->getIntValue(RFX_STATUS_KEY_AP_VOICE_CALL_COUNT, 0) > 0) {
            ret = true;
        }
    }
    return ret;
}

bool RtcRatSwitchController::isNoSimInserted() {
    for (int i = 0; i < RFX_SLOT_COUNT; i++) {
        int cardType = getStatusManager(i)->getIntValue(
                RFX_STATUS_KEY_CARD_TYPE, 0);
        logD(RAT_CTRL_TAG, "[isNoSimInserted] SIM%d cardType: %d", i, cardType);
        if (cardType > 0) {
            return false;
        }
    }
    logD(RAT_CTRL_TAG, "[isNoSimInserted] No sim inserted");
    return true;
}

bool RtcRatSwitchController::isGsmOnlySimFromMode(RatSwitchInfo ratSwtichInfo) {
    bool ret = false;
    if ((ratSwtichInfo.card_type == RFX_CARD_TYPE_SIM
            || ratSwtichInfo.card_type == RFX_CARD_TYPE_USIM
            || ratSwtichInfo.card_type == (RFX_CARD_TYPE_SIM | RFX_CARD_TYPE_ISIM)
            || ratSwtichInfo.card_type == (RFX_CARD_TYPE_USIM | RFX_CARD_TYPE_ISIM))
            && (!ratSwtichInfo.isCt3GDualMode)) {
        ret = true;
    }

    // logD(RAT_CTRL_TAG, "[isGsmOnlySimFromMode] GSM only: %s", ret ? "true" : "false");
    return ret;
}

bool RtcRatSwitchController::isGsmOnlySim() {
    bool ret = false;
    int nCardType = getStatusManager()->getIntValue(RFX_STATUS_KEY_CARD_TYPE);

    if ((nCardType == RFX_CARD_TYPE_SIM
            || nCardType == RFX_CARD_TYPE_USIM
            || nCardType == (RFX_CARD_TYPE_SIM | RFX_CARD_TYPE_ISIM)
            || nCardType == (RFX_CARD_TYPE_USIM | RFX_CARD_TYPE_ISIM))
            && (!isCdmaDualModeSimCard())) {
        ret = true;
    }

    // logD(RAT_CTRL_TAG, "[isGsmOnlySim] GSM only: %s", ret ? "true" : "false");
    return ret;
}

bool RtcRatSwitchController::isCdmaOnlySim() {
    bool ret = false;
    int nCardType = getStatusManager()->getIntValue(RFX_STATUS_KEY_CARD_TYPE);
    if (getStatusManager()->getBoolValue(RFX_STATUS_KEY_CDMA3G_DUALMODE_CARD)) {
        logD(RAT_CTRL_TAG, "[isCdmaOnlySim] is C2K 3G dual mode card");
        return false;
    }
    if (nCardType == RFX_CARD_TYPE_RUIM) {
        ret = true;
    }
    logD(RAT_CTRL_TAG, "[isCdmaOnlySim] CardType:%d", nCardType);
    return ret;
}

bool RtcRatSwitchController::isCdmaDualModeSimCard() {
    if (getStatusManager()->getBoolValue(RFX_STATUS_KEY_CDMA3G_DUALMODE_CARD)) {
        logD(RAT_CTRL_TAG, "isCdmaDualModeSimCard, is CT3G dual mode card");
        return true;
    } else {
        int nCardType = getStatusManager()->getIntValue(RFX_STATUS_KEY_CARD_TYPE);
        bool ret = false;
        if (RFX_FLAG_HAS_ALL(nCardType, (RFX_CARD_TYPE_USIM | RFX_CARD_TYPE_CSIM)) ||
                RFX_FLAG_HAS_ALL(nCardType, (RFX_CARD_TYPE_USIM | RFX_CARD_TYPE_RUIM))) {
            logD(RAT_CTRL_TAG, "isCdmaDualModeSimCard, nCardType=0x%x, ret = %d", nCardType, ret);
            ret = true;
        }
        return ret;
    }
}

int RtcRatSwitchController::getMajorSlotId() {
    char tempstr[RFX_PROPERTY_VALUE_MAX];
    memset(tempstr, 0, sizeof(tempstr));
    rfx_property_get("persist.radio.simswitch", tempstr, "1");
    int majorSlotId = atoi(tempstr) - 1;
    // logD(RAT_CTRL_TAG, "[getMajorSlotId] %d", majorSlotId);
    return majorSlotId;
}

bool RtcRatSwitchController::isChipTestMode() {
    bool ret = false;
    char chipsetMode[RFX_PROPERTY_VALUE_MAX] = { 0 };
    rfx_property_get("persist.chiptest.enable", chipsetMode, "0");
    ret = (strcmp(chipsetMode, "1") == 0);
    // logD(RAT_CTRL_TAG, "[isChipTestMode] %d", ret);
    return ret;
}

void RtcRatSwitchController::setSlotCapability(int slotId, int val) {
    char PROPERTY_RILJ_NW_RAF[4][30] = {
        "persist.radio.raf1",
        "persist.radio.raf2",
        "persist.radio.raf3",
        "persist.radio.raf4",
    };
    setIntSysProp(PROPERTY_RILJ_NW_RAF[slotId], val);
}

int RtcRatSwitchController::getSlotCapability(int slotId) {
    char PROPERTY_RILJ_NW_RAF[4][30] = {
        "persist.radio.raf1",
        "persist.radio.raf2",
        "persist.radio.raf3",
        "persist.radio.raf4",
    };
    return getIntSysProp(PROPERTY_RILJ_NW_RAF[slotId], -1);
}

void RtcRatSwitchController::setPreferredNetWorkTypeToSysProp(int slotId, int val) {
    char PROPERTY_RILJ_NW_TYPE[4][30] = {
        "persist.radio.rilj_nw_type1",
        "persist.radio.rilj_nw_type2",
        "persist.radio.rilj_nw_type3",
        "persist.radio.rilj_nw_type4",
    };
    if (getPreferredNetWorkTypeFromSysProp(slotId) != val) {
        setIntSysProp(PROPERTY_RILJ_NW_TYPE[slotId], val);
    }
}

// only for major slot.
int RtcRatSwitchController::getPreferredNetWorkTypeFromSysProp(int slotId) {
    char PROPERTY_RILJ_NW_TYPE[4][30] = {
        "persist.radio.rilj_nw_type1",
        "persist.radio.rilj_nw_type2",
        "persist.radio.rilj_nw_type3",
        "persist.radio.rilj_nw_type4",
    };
    return getIntSysProp(PROPERTY_RILJ_NW_TYPE[slotId], 10);
}

void RtcRatSwitchController::setIntSysProp(char *propName, int val) {
    char stgBuf[RFX_PROPERTY_VALUE_MAX] = { 0 };
    sprintf(stgBuf, "%d", val);
    rfx_property_set(propName, stgBuf);
}

int RtcRatSwitchController::getIntSysProp(char *propName, int defaultVal) {
    int val = -1;
    char stgBuf[RFX_PROPERTY_VALUE_MAX] = { 0 };
    rfx_property_get(propName, stgBuf, "-1");
    val = strtol(stgBuf, NULL, 10);
    if (val == -1) {
        // logD(RAT_CTRL_TAG, "[getIntSysProp] %s not exist, return %d",
        //         propName, defaultVal);
        val = defaultVal;
    }
    return val;
}

int RtcRatSwitchController::isNewSimCard(int slotId) {
    int result = -1;
    char lastTimeIccid[RFX_PROPERTY_VALUE_MAX] = {0};
    char currentIccid[RFX_PROPERTY_VALUE_MAX] = {0};

    char PROPERTY_ICCID_SIM[4][25] = {
        "ril.iccid.sim1",
        "ril.iccid.sim2",
        "ril.iccid.sim3",
        "ril.iccid.sim4",
    };
    char PROPERTY_LAST_BOOT_ICCID_SIM[4][30] = {
        "persist.radio.last_iccid_sim1",
        "persist.radio.last_iccid_sim2",
        "persist.radio.last_iccid_sim3",
        "persist.radio.last_iccid_sim4",
    };

    rfx_property_get(PROPERTY_ICCID_SIM[slotId], currentIccid, "");
    if (strlen(currentIccid) == 0 || strcmp("N/A", currentIccid) == 0) {
        // logD(RAT_CTRL_TAG, "[isNewSimCard]:iccid not ready");
        result = 0;
    }
    rfx_property_get(PROPERTY_LAST_BOOT_ICCID_SIM[slotId], lastTimeIccid, "");
    if (strlen(lastTimeIccid) == 0 || strcmp("N/A", lastTimeIccid) == 0) {
        logD(RAT_CTRL_TAG, "[isNewSimCard]:first time boot-up");
        rfx_property_set(PROPERTY_LAST_BOOT_ICCID_SIM[slotId], currentIccid);
        result = 1;
    } else {
        if (strlen(currentIccid) == 0 || sIsInSwitching == true) {
            logD(RAT_CTRL_TAG, "[isNewSimCard]:Sim card is not ready or in switcing");
            result = 0;
        } else if (strcmp(lastTimeIccid, currentIccid) == 0) {
            // logD(RAT_CTRL_TAG, "[isNewSimCard]:Sim card is no change");
            result = 0;
        } else {
            logD(RAT_CTRL_TAG, "[isNewSimCard]:this is new Sim card");
            rfx_property_set(PROPERTY_LAST_BOOT_ICCID_SIM[slotId], currentIccid);
            result = 1;
        }
    }
    // logD(RAT_CTRL_TAG, "[isNewSimCard] return:%d", result);
    return result;
}

int RtcRatSwitchController::isRafContainsCdma(int raf) {
    int result = -1;
    if (((raf & RAF_EVDO_GROUP) > 0) || ((raf & RAF_CDMA_GROUP) > 0)) {
        result = 1;
    } else {
        result = 0;
    }
    return result;
}

int RtcRatSwitchController::isRafContainsGsm(int raf) {
    int result = -1;
    if (((raf & RAF_GSM_GROUP) > 0) || ((raf & RAF_HS_GROUP) > 0)
            || ((raf & RAF_WCDMA_GROUP) > 0) || ((raf & RAF_LTE) > 0)) {
        result = 1;
    } else {
        result = 0;
    }
    return result;
}

PsRatFamily RtcRatSwitchController::getPsRatFamily(int radioTechnology) {
    if (radioTechnology == RADIO_TECH_GPRS
            || radioTechnology == RADIO_TECH_EDGE
            || radioTechnology == RADIO_TECH_UMTS
            || radioTechnology == RADIO_TECH_HSDPA
            || radioTechnology == RADIO_TECH_HSUPA
            || radioTechnology == RADIO_TECH_HSPA
            || radioTechnology == RADIO_TECH_LTE
            || radioTechnology == RADIO_TECH_HSPAP
            || radioTechnology == RADIO_TECH_GSM
            || radioTechnology == RADIO_TECH_TD_SCDMA
            || radioTechnology == RADIO_TECH_LTE_CA) {
        return PS_RAT_FAMILY_GSM;
    } else if (radioTechnology == RADIO_TECH_IS95A
            || radioTechnology == RADIO_TECH_IS95B
            || radioTechnology == RADIO_TECH_1xRTT
            || radioTechnology == RADIO_TECH_EVDO_0
            || radioTechnology == RADIO_TECH_EVDO_A
            || radioTechnology == RADIO_TECH_EVDO_B
            || radioTechnology == RADIO_TECH_EHRPD) {
        return PS_RAT_FAMILY_CDMA;
    } else if (radioTechnology == RADIO_TECH_IWLAN) {
        return PS_RAT_FAMILY_IWLAN;
    } else {
        return PS_RAT_FAMILY_UNKNOWN;
    }
}

const char *RtcRatSwitchController::switchCallerToString(int callerEnum) {
    switch (callerEnum) {
        case RAT_SWITCH_INIT:
            return "INIT";
        case RAT_SWITCH_NORMAL:
            return "NOR";
        case RAT_SWITCH_RESTRICT:
            return "RES";
        default:
            logW(RAT_CTRL_TAG, "<UNKNOWN> %d", callerEnum);
            break;
    }
    return "";
}

int RtcRatSwitchController::filterPrefNwTypeFromRilj(const int prefNwTypeFromRilj) {
    int targetPrefNwType = -1;
    if (mDefaultNetworkType != -1) {
        int slot_capability = RtcCapabilitySwitchUtil::getRafFromNetworkType(mDefaultNetworkType);
        int rafFromType = RtcCapabilitySwitchUtil::getRafFromNetworkType(prefNwTypeFromRilj);
        int filteredRaf = (slot_capability & rafFromType);

        if (isGsmOnlySim() == true) {
            rafFromType &= ~(RAF_CDMA_GROUP | RAF_EVDO_GROUP);
            targetPrefNwType = RtcCapabilitySwitchUtil::getNetworkTypeFromRaf(rafFromType);
        } else if (filteredRaf != 0) {
            targetPrefNwType = RtcCapabilitySwitchUtil::getNetworkTypeFromRaf(filteredRaf);
        }
    } else {
        if (isGsmOnlySim() == true) {
            int rafFromType = RtcCapabilitySwitchUtil::getRafFromNetworkType(prefNwTypeFromRilj);
            rafFromType &= ~(RAF_CDMA_GROUP | RAF_EVDO_GROUP);
            targetPrefNwType = RtcCapabilitySwitchUtil::getNetworkTypeFromRaf(rafFromType);
        } else {
            targetPrefNwType = prefNwTypeFromRilj;
        }
    }
    return targetPrefNwType;
}

void RtcRatSwitchController::responseGetGmssRatMode(const sp<RfxMessage>& response) {
    handleGmssRat(response);
    configRatMode();
}

void RtcRatSwitchController::getGmssRatMode() {
    sp<RfxMessage> reqToRild = RfxMessage::obtainRequest(m_slot_id,
            RFX_MSG_REQUEST_GET_GMSS_RAT_MODE, RfxVoidData());
    requestToMcl(reqToRild);
}

void RtcRatSwitchController::handleGmssRat(const sp<RfxMessage>& message) {
    if (message->getType() == RESPONSE && message->getError() != RIL_E_SUCCESS) {
        logD(RAT_CTRL_TAG, "[handleGmssRat] error: %d ", message->getError());
        return;
    }

    RfxIntsData *intsData = (RfxIntsData*)message->getData();
    int *data = (int*)intsData->getData();
    GmssInfo gmssInfo;
    gmssInfo.rat = data[0];
    gmssInfo.mcc = data[1];
    gmssInfo.status = data[2];
    gmssInfo.mspl = data[3];
    gmssInfo.ishome = data[4];

    if (gmssInfo.rat == GMSS_RAT_INVALID
            && gmssInfo.mcc == 0
            && gmssInfo.status == GMSS_STATUS_SELECT
            && gmssInfo.mspl == MSPL_RAT_NONE
            && gmssInfo.ishome == false) {
        // logD(RAT_CTRL_TAG, "[handleGmssRat] invalid value");
        return;
    }

    if (gmssInfo.status == GMSS_STATUS_ECC) {
        if (gmssInfo.rat == GMSS_RAT_3GPP2 ||
                gmssInfo.rat == GMSS_RAT_C2K1X ||
                gmssInfo.rat == GMSS_RAT_C2KHRPD) {
            mNwsMode = NWS_MODE_CDMALTE;
        } else if (gmssInfo.rat == GMSS_RAT_GSM ||
                gmssInfo.rat == GMSS_RAT_WCDMA) {
            mNwsMode = NWS_MODE_CSFB;
        } else {
            if (gmssInfo.ishome == false ||
                    isCdmaDualModeSimCard() == false) {
                mNwsMode = NWS_MODE_CSFB;
            } else {
                mNwsMode = NWS_MODE_CDMALTE;
            }
        }
    } else {
        if (RFX_FLAG_HAS(gmssInfo.mspl, MSPL_RAT_C2K) &&
                (RFX_FLAG_HAS(gmssInfo.mspl, MSPL_RAT_GSM)
                || RFX_FLAG_HAS(gmssInfo.mspl, MSPL_RAT_UMTS))) {
            if (gmssInfo.status == GMSS_STATUS_ATTACHED) {
                if (GMSS_RAT_3GPP2 == gmssInfo.rat
                        || GMSS_RAT_C2K1X == gmssInfo.rat
                        || GMSS_RAT_C2KHRPD == gmssInfo.rat) {
                    mNwsMode = NWS_MODE_CDMALTE;
                } else if (GMSS_RAT_GSM == gmssInfo.rat
                        || GMSS_RAT_WCDMA == gmssInfo.rat) {
                    mNwsMode = NWS_MODE_CSFB;
                } else {
                    // EUTRAN (LTE)
                    mNwsMode = gmssInfo.ishome ? NWS_MODE_CDMALTE : NWS_MODE_CSFB;
                }
            }
        } else if (RFX_FLAG_HAS(gmssInfo.mspl, MSPL_RAT_C2K)) {
            mNwsMode = NWS_MODE_CDMALTE;
        } else {
            mNwsMode = NWS_MODE_CSFB;
        }
    }
    logD(RAT_CTRL_TAG, "[handleGmssRat] rat:%d, mcc:%d, status:%d, mspl:%d, ishome:%d, NWS mode:%d",
            gmssInfo.rat, gmssInfo.mcc, gmssInfo.status, gmssInfo.mspl, gmssInfo.ishome, mNwsMode);
}

