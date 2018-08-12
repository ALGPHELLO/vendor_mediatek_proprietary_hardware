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
#include "RtcCallController.h"
#include "RfxRootController.h"

#include "RfxCallFailCauseData.h"
#include "RfxCallListData.h"
#include "RfxCdmaInfoRecData.h"
#include "RfxCdmaWaitingCallData.h"
#include "RfxDialData.h"
#include "RfxCrssNotificationData.h"
#include "RfxSuppServNotificationData.h"
#include "RfxIntsData.h"
#include "RfxRilUtils.h"
#include "RfxStringData.h"
#include "RfxStringsData.h"
#include "RfxTimer.h"
#include "RfxVoidData.h"
#include "rfx_properties.h"

/*****************************************************************************
 * Class RfxController
 *****************************************************************************/

#define RFX_LOG_TAG "RtcCC"
#define MIN_IMS_CALL_MODE 20

RFX_IMPLEMENT_CLASS("RtcCallController", RtcCallController, RfxController);

// register request to RfxData
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxCallListData, RFX_MSG_REQUEST_GET_CURRENT_CALLS);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxDialData, RfxVoidData, RFX_MSG_REQUEST_DIAL);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxVoidData, RFX_MSG_REQUEST_ANSWER);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxVoidData, RFX_MSG_REQUEST_UDUB);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxVoidData, RFX_MSG_REQUEST_EXPLICIT_CALL_TRANSFER);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxCallFailCauseData, RFX_MSG_REQUEST_LAST_CALL_FAIL_CAUSE);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxStringData, RfxVoidData, RFX_MSG_REQUEST_DTMF);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxStringData, RfxVoidData, RFX_MSG_REQUEST_DTMF_START);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxVoidData, RFX_MSG_REQUEST_DTMF_STOP);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData, RFX_MSG_REQUEST_SET_CALL_INDICATION);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData, RFX_MSG_REQUEST_SET_ECC_SERVICE_CATEGORY);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxDialData, RfxVoidData, RFX_MSG_REQUEST_EMERGENCY_DIAL);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxDialData, RfxVoidData, RFX_MSG_REQUEST_VT_DIAL);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData, RFX_MSG_REQUEST_VOICE_ACCEPT);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData, RFX_MSG_REQUEST_VIDEO_CALL_ACCEPT);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData, RFX_MSG_REQUEST_REPLACE_VT_CALL);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData, RFX_MSG_REQUEST_CURRENT_STATUS);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData, RFX_MSG_REQUEST_LOCAL_CURRENT_STATUS);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData, RFX_MSG_REQUEST_ECC_PREFERRED_RAT);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxVoidData, RFX_MSG_REQUEST_HANGUP_ALL);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData, RFX_MSG_REQUEST_HANGUP);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxVoidData, RFX_MSG_REQUEST_HANGUP_WAITING_OR_BACKGROUND);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxVoidData, RFX_MSG_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxVoidData, RFX_MSG_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxVoidData, RFX_MSG_REQUEST_CONFERENCE);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData, RFX_MSG_REQUEST_SEPARATE_CONNECTION);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData, RFX_MSG_REQUEST_SET_MUTE);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxIntsData, RFX_MSG_REQUEST_GET_MUTE);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData, RFX_MSG_REQUEST_SET_TTY_MODE);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxIntsData, RFX_MSG_REQUEST_QUERY_TTY_MODE);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData, RFX_MSG_REQUEST_FORCE_RELEASE_CALL);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxVoidData, RFX_MSG_REQUEST_EXIT_EMERGENCY_CALLBACK_MODE);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxDialData, RfxVoidData, RFX_MSG_REQUEST_IMS_EMERGENCY_DIAL);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxDialData, RfxVoidData, RFX_MSG_REQUEST_IMS_DIAL);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxDialData, RfxVoidData, RFX_MSG_REQUEST_IMS_VT_DIAL);

RFX_REGISTER_DATA_TO_URC_ID(RfxVoidData, RFX_MSG_UNSOL_RESPONSE_CALL_STATE_CHANGED);
RFX_REGISTER_DATA_TO_URC_ID(RfxSuppServNotificationData, RFX_MSG_UNSOL_SUPP_SVC_NOTIFICATION);
RFX_REGISTER_DATA_TO_URC_ID(RfxIntsData, RFX_MSG_UNSOL_RINGBACK_TONE);
RFX_REGISTER_DATA_TO_URC_ID(RfxVoidData, RFX_MSG_UNSOL_VT_RING_INFO);
RFX_REGISTER_DATA_TO_URC_ID(RfxVoidData, RFX_MSG_UNSOL_CALL_RING);
RFX_REGISTER_DATA_TO_URC_ID(RfxStringsData,  RFX_MSG_UNSOL_INCOMING_CALL_INDICATION);
RFX_REGISTER_DATA_TO_URC_ID(RfxStringsData, RFX_MSG_UNSOL_CIPHER_INDICATION);
RFX_REGISTER_DATA_TO_URC_ID(RfxIntsData, RFX_MSG_UNSOL_SPEECH_CODEC_INFO);
RFX_REGISTER_DATA_TO_URC_ID(RfxCrssNotificationData, RFX_MSG_UNSOL_CRSS_NOTIFICATION);
RFX_REGISTER_DATA_TO_URC_ID(RfxIntsData, RFX_MSG_UNSOL_VT_STATUS_INFO);
RFX_REGISTER_DATA_TO_EVENT_ID(RfxStringData, RFX_MSG_EVENT_CNAP_UPDATE);
RFX_REGISTER_DATA_TO_EVENT_ID(RfxIntsData, RFX_MSG_EVENT_CLEAR_CLCCNAME);
RFX_REGISTER_DATA_TO_EVENT_ID(RfxVoidData, RFX_MSG_EVENT_EXIT_EMERGENCY_CALLBACK_MODE);
// CDMA
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData, RFX_MSG_REQUEST_CDMA_SET_PREFERRED_VOICE_PRIVACY_MODE);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxIntsData, RFX_MSG_REQUEST_CDMA_QUERY_PREFERRED_VOICE_PRIVACY_MODE);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxStringData, RfxVoidData, RFX_MSG_REQUEST_CDMA_FLASH);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxStringsData, RfxVoidData, RFX_MSG_REQUEST_CDMA_BURST_DTMF);
RFX_REGISTER_DATA_TO_URC_ID(RfxCdmaInfoRecData, RFX_MSG_UNSOL_CDMA_INFO_REC);
RFX_REGISTER_DATA_TO_URC_ID(RfxVoidData, RFX_MSG_UNSOL_ENTER_EMERGENCY_CALLBACK_MODE);
RFX_REGISTER_DATA_TO_URC_ID(RfxVoidData, RFX_MSG_UNSOL_EXIT_EMERGENCY_CALLBACK_MODE);
RFX_REGISTER_DATA_TO_URC_ID(RfxVoidData, RFX_MSG_UNSOL_NO_EMERGENCY_CALLBACK_MODE);
RFX_REGISTER_DATA_TO_URC_ID(RfxVoidData, RFX_MSG_UNSOL_CDMA_CALL_ACCEPTED);
RFX_REGISTER_DATA_TO_URC_ID(RfxCdmaWaitingCallData, RFX_MSG_UNSOL_CDMA_CALL_WAITING);
// IMS
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxStringsData, RfxVoidData, RFX_MSG_REQUEST_CONFERENCE_DIAL);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxStringData, RfxVoidData, RFX_MSG_REQUEST_DIAL_WITH_SIP_URI);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxStringData, RfxVoidData, RFX_MSG_REQUEST_VT_DIAL_WITH_SIP_URI);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData, RFX_MSG_REQUEST_HOLD_CALL);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData, RFX_MSG_REQUEST_RESUME_CALL);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData, RFX_MSG_REQUEST_ASYNC_HOLD_CALL);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData, RFX_MSG_REQUEST_ASYNC_RESUME_CALL);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxStringsData, RfxVoidData, RFX_MSG_REQUEST_ADD_IMS_CONFERENCE_CALL_MEMBER);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxStringsData, RfxVoidData, RFX_MSG_REQUEST_REMOVE_IMS_CONFERENCE_CALL_MEMBER);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxStringsData, RfxVoidData, RFX_MSG_REQUEST_PULL_CALL);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxStringsData, RfxVoidData, RFX_MSG_REQUEST_IMS_ECT);

RFX_REGISTER_DATA_TO_URC_ID(RfxStringsData, RFX_MSG_UNSOL_CALL_INFO_INDICATION);
RFX_REGISTER_DATA_TO_URC_ID(RfxIntsData, RFX_MSG_UNSOL_ECONF_SRVCC_INDICATION);
RFX_REGISTER_DATA_TO_URC_ID(RfxStringsData, RFX_MSG_UNSOL_ECONF_RESULT_INDICATION);
RFX_REGISTER_DATA_TO_URC_ID(RfxStringsData, RFX_MSG_UNSOL_CALLMOD_CHANGE_INDICATOR);
RFX_REGISTER_DATA_TO_URC_ID(RfxStringsData, RFX_MSG_UNSOL_VIDEO_CAPABILITY_INDICATOR);
RFX_REGISTER_DATA_TO_URC_ID(RfxIntsData, RFX_MSG_UNSOL_SRVCC_STATE_NOTIFY);
RFX_REGISTER_DATA_TO_URC_ID(RfxStringsData, RFX_MSG_URC_IMS_EVENT_PACKAGE_INDICATION);
RFX_REGISTER_DATA_TO_URC_ID(RfxIntsData, RFX_MSG_UNSOL_ECT_INDICATION);
RFX_REGISTER_DATA_TO_URC_ID(RfxIntsData, RFX_MSG_UNSOL_IMS_ASYNC_CALL_CONTROL_RESULT);

RtcImsCall::RtcImsCall(int id, int state) : mCallId(id), mCallState(state) {

}

RtcCallController::RtcCallController() :
mRedialCtrl(NULL)
, mCallRat(CALL_RAT_NONE), mEccNumber(NULL), mEccNumberBuffer("")
, mPreciseCallStateList(NULL)
, mUseLocalCallFailCause(false), mDialLastError(0)
, mVtCallCount(0), mCsCallCount(0), mPendingCallControlMessage(NULL)
, mWaitForCurrentStatusResponse(false), mPendingSrvccCallCount(0){
}

RtcCallController::~RtcCallController() {
}

void RtcCallController::onInit() {
    // Required: invoke super class implementation
    RfxController::onInit();

    createRedialController();

    mPreciseCallStateList = new Vector<RfxPreciseCallState*>();

    const int request_id_list[] = {
        RFX_MSG_REQUEST_GET_CURRENT_CALLS,
        RFX_MSG_REQUEST_DIAL,
        RFX_MSG_REQUEST_HANGUP,
        RFX_MSG_REQUEST_HANGUP_WAITING_OR_BACKGROUND,
        RFX_MSG_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND,
        RFX_MSG_REQUEST_LAST_CALL_FAIL_CAUSE,
        RFX_MSG_REQUEST_HANGUP_ALL,
        RFX_MSG_REQUEST_VT_DIAL,
        RFX_MSG_REQUEST_CDMA_FLASH,
        RFX_MSG_REQUEST_EMERGENCY_DIAL,
        RFX_MSG_REQUEST_DIAL_WITH_SIP_URI,
        RFX_MSG_REQUEST_CONFERENCE_DIAL,
        RFX_MSG_REQUEST_VT_DIAL_WITH_SIP_URI,
        RFX_MSG_REQUEST_PULL_CALL,
        RFX_MSG_REQUEST_CURRENT_STATUS,
        RFX_MSG_REQUEST_IMS_DIAL,
        RFX_MSG_REQUEST_IMS_VT_DIAL,
        RFX_MSG_REQUEST_IMS_EMERGENCY_DIAL,
        RFX_MSG_REQUEST_HOLD_CALL,
        RFX_MSG_REQUEST_RESUME_CALL,
        RFX_MSG_REQUEST_ASYNC_HOLD_CALL,
        RFX_MSG_REQUEST_ASYNC_RESUME_CALL,
        RFX_MSG_REQUEST_ADD_IMS_CONFERENCE_CALL_MEMBER,
        RFX_MSG_REQUEST_REMOVE_IMS_CONFERENCE_CALL_MEMBER,
        RFX_MSG_REQUEST_ANSWER,
        RFX_MSG_REQUEST_SET_CALL_INDICATION,
        RFX_MSG_REQUEST_FORCE_RELEASE_CALL,
        RFX_MSG_REQUEST_VIDEO_CALL_ACCEPT,
        RFX_MSG_REQUEST_VOICE_ACCEPT,
        RFX_MSG_REQUEST_CONFERENCE,
        RFX_MSG_REQUEST_EXIT_EMERGENCY_CALLBACK_MODE,
        RFX_MSG_REQUEST_UDUB,
        RFX_MSG_REQUEST_QUERY_TTY_MODE,
        RFX_MSG_REQUEST_SET_TTY_MODE,
        RFX_MSG_REQUEST_DTMF,
        RFX_MSG_REQUEST_DTMF_START,
        RFX_MSG_REQUEST_DTMF_STOP,
    };

    const int urc_id_list[] = {
        /* Common URC */
        RFX_MSG_UNSOL_CALL_RING,
        RFX_MSG_UNSOL_CDMA_CALL_WAITING,
        /* MTK RIL URC */
        RFX_MSG_UNSOL_CALL_INFO_INDICATION,
        // GSM
        RFX_MSG_UNSOL_INCOMING_CALL_INDICATION,
        RFX_MSG_UNSOL_RESPONSE_CALL_STATE_CHANGED,
        // IMS
        RFX_MSG_UNSOL_ECONF_RESULT_INDICATION,
        RFX_MSG_UNSOL_CALLMOD_CHANGE_INDICATOR,
        RFX_MSG_UNSOL_VIDEO_CAPABILITY_INDICATOR,
        RFX_MSG_UNSOL_SIP_CALL_PROGRESS_INDICATOR,
        RFX_MSG_URC_IMS_EVENT_PACKAGE_INDICATION,
        RFX_MSG_UNSOL_SRVCC_STATE_NOTIFY,
        RFX_MSG_UNSOL_VT_RING_INFO,
        RFX_MSG_UNSOL_ECT_INDICATION,
        RFX_MSG_UNSOL_IMS_ASYNC_CALL_CONTROL_RESULT,
        // ECBM
        RFX_MSG_UNSOL_ENTER_EMERGENCY_CALLBACK_MODE,
        RFX_MSG_UNSOL_EXIT_EMERGENCY_CALLBACK_MODE,
        RFX_MSG_UNSOL_NO_EMERGENCY_CALLBACK_MODE,
    };

    // register request & URC id list
    // NOTE. one id can only be registered by one controller
    registerToHandleRequest(request_id_list, sizeof(request_id_list)/sizeof(const int));
    registerToHandleUrc(urc_id_list, sizeof(urc_id_list)/sizeof(const int));

    getStatusManager()->registerStatusChanged(RFX_STATUS_KEY_SERVICE_STATE,
        RfxStatusChangeCallback(this, &RtcCallController::onServiceStateChanged));

    // register callbacks to get card type change event
    getStatusManager()->registerStatusChanged(RFX_STATUS_KEY_CARD_TYPE,
        RfxStatusChangeCallback(this, &RtcCallController::onCardTypeChanged));
}

void RtcCallController::onDeinit() {
    logD(RFX_LOG_TAG, "onDeinit");
    RfxController::onDeinit();
    freePreciseCallStateList(mPreciseCallStateList);
    mPreciseCallStateList = NULL;
}

void RtcCallController::createRedialController() {
    RFX_OBJ_CREATE(mRedialCtrl, RtcRedialController, this);
}

bool RtcCallController::onHandleRequest(const sp<RfxMessage>& message) {
    int msg_id = message->getId();
    //logD(RFX_LOG_TAG, "onHandleRequest: %s", RFX_ID_TO_STR(msg_id));

    switch (msg_id) {
        case RFX_MSG_REQUEST_EMERGENCY_DIAL:
            if (mRedialCtrl != NULL) {
                mRedialCtrl->notifyRilRequest(message);
            }
            handleEmergencyDialRequest(message);
            // fall through
        case RFX_MSG_REQUEST_DIAL:
            if (rejectDualDialForDSDS()) {
                responseDialFailed(message);
                return true;
            }
            break;
        case RFX_MSG_REQUEST_VT_DIAL:
            /// M: For 3G VT only @{
            if (reject3gVtForMultipartyCall()) {
                sp<RfxMessage> responseMsg = RfxMessage::obtainResponse(RIL_E_GENERIC_FAILURE,
                        message, true);
                responseToRilj(responseMsg);
                return true;
            }
            /// @}
            if (rejectDualDialForDSDS()) {
                responseDialFailed(message);
                return true;
            }
            break;
        case RFX_MSG_REQUEST_IMS_EMERGENCY_DIAL:
            if (mRedialCtrl != NULL) {
                mRedialCtrl->notifyRilRequest(message);
            }
            handleEmergencyDialRequest(message);
            // fall through
        case RFX_MSG_REQUEST_IMS_DIAL:
        case RFX_MSG_REQUEST_IMS_VT_DIAL:
        case RFX_MSG_REQUEST_PULL_CALL:
        case RFX_MSG_REQUEST_CONFERENCE_DIAL:
        case RFX_MSG_REQUEST_DIAL_WITH_SIP_URI:
        case RFX_MSG_REQUEST_VT_DIAL_WITH_SIP_URI:
            if (rejectDualDialForDSDS()) {
                responseDialFailed(message);
                return true;
            }
            handleImsDialRequest(message->getSlotId());
            break;
        case RFX_MSG_REQUEST_LAST_CALL_FAIL_CAUSE :
            if (mUseLocalCallFailCause) {
                RIL_LastCallFailCauseInfo callFailCause;
                memset(&callFailCause, 0, sizeof(RIL_LastCallFailCauseInfo));
                callFailCause.cause_code = (RIL_LastCallFailCause) mDialLastError;
                callFailCause.vendor_cause = NULL;
                sp<RfxMessage> responseMsg = RfxMessage::obtainResponse(
                        m_slot_id, RFX_MSG_REQUEST_LAST_CALL_FAIL_CAUSE,
                        RIL_E_SUCCESS, RfxCallFailCauseData(&callFailCause, sizeof(callFailCause)),
                        message);
                responseToRilj(responseMsg);
                logD(RFX_LOG_TAG, "Use local call fail cause for slot%d", m_slot_id);
                mUseLocalCallFailCause = false;
                mDialLastError = 0;
                return true;
            }
            break;
        case RFX_MSG_REQUEST_HANGUP: {
            int hangupCallId = ((int *)message->getData()->getData())[0];

            if (mRedialCtrl != NULL) {
                mRedialCtrl->notifyRilRequest(message);
            }

            if (hasPendingHangupRequest(hangupCallId)) {
                sp<RfxMessage> responseMsg = RfxMessage::obtainResponse(RIL_E_SUCCESS, message,
                        true);
                responseToRilj(responseMsg);
                return true;
            }
            updateDisconnecting(mPreciseCallStateList, hangupCallId);
            break;
        }

        case RFX_MSG_REQUEST_HANGUP_ALL: {
            int callCount = getStatusManager(m_slot_id)->getIntValue(
                        RFX_STATUS_KEY_VOICE_CALL_COUNT, 0);
            if (callCount == 0) {
                logD(RFX_LOG_TAG, "No call, ignore hangup all: slot = %d", m_slot_id);
                return true;
            }
            if (mRedialCtrl != NULL) {
                mRedialCtrl->notifyRilRequest(message);
            }
            break;
        }
        case RFX_MSG_REQUEST_CURRENT_STATUS:
            if (mRedialCtrl != NULL && mRedialCtrl->notifyRilRequest(message)) {
                return true;
            }
            /* ALPS03346578: Emergency dial can be handled after receiving response of
                RFX_MSG_REQUEST_CURRENT_STATUS */
            mWaitForCurrentStatusResponse = true;
            break;

        case RFX_MSG_REQUEST_HANGUP_WAITING_OR_BACKGROUND:
            if (hasPendingHangupRequest(false/*isForegnd*/)) {
                sp<RfxMessage> responseMsg = RfxMessage::obtainResponse(RIL_E_SUCCESS, message,
                        true);
                responseToRilj(responseMsg);
                return true;
            }
            updateDisconnecting(mPreciseCallStateList, false/*isForegnd*/);
            break;
        case RFX_MSG_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND:
            if (hasPendingHangupRequest(true/*isForegnd*/)) {
                sp<RfxMessage> responseMsg = RfxMessage::obtainResponse(RIL_E_SUCCESS, message,
                        true);
                responseToRilj(responseMsg);
                return true;
            }
            updateDisconnecting(mPreciseCallStateList, true/*isForegnd*/);
            break;
        case RFX_MSG_REQUEST_CDMA_FLASH:
            handleCdmaFlashRequest(message);
            break;
        case RFX_MSG_REQUEST_HOLD_CALL:
        case RFX_MSG_REQUEST_RESUME_CALL:
            if (shouldDoAsyncImsCallControl()) {
                handleAsyncImsCallControlRequest(message);
                return true;
            }
        default:
            break;
    }
    updateIsImsCallExistToStatusManager(message->getSlotId());
    requestToMcl(message);
    return true;
}

bool RtcCallController::onHandleUrc(const sp<RfxMessage>& message) {
    int msg_id = message->getId();
    int slotId = message->getSlotId();
    logD(RFX_LOG_TAG, "onHandleUrc: %s", RFX_ID_TO_STR(msg_id));

    bool responseToRilJ = true;

    if (mRedialCtrl != NULL && mRedialCtrl->notifyRilUrc(message)) {
       return true;
    }

    switch (msg_id) {
        case RFX_MSG_UNSOL_CDMA_CALL_WAITING:
            handleCdmaCallWait();
            break;
        case RFX_MSG_UNSOL_INCOMING_CALL_INDICATION:
            if (!handleIncomingCall(slotId, (RfxStringsData*)message->getData())) {
                return true;
            }
            break;
        case RFX_MSG_UNSOL_CALL_INFO_INDICATION:
            if (hasImsCall(slotId)) {
                responseToRilJ = handleImsCallInfoUpdate(message);
            } else {
                responseToRilJ = handleCsCallInfoUpdate(message);
                //logD(RFX_LOG_TAG, "No ImsCall, report CALL_STATE_CHANGED");
                return true;
            }
            break;
        case RFX_MSG_UNSOL_SRVCC_STATE_NOTIFY:
            handleSrvcc(slotId, message);
            break;
        case RFX_MSG_UNSOL_CALL_RING:
        case RFX_MSG_UNSOL_VT_RING_INFO:
            if (hasImsCall(slotId)) {
                handleCallRing(slotId);
            }
            break;
        case RFX_MSG_UNSOL_RESPONSE_CALL_STATE_CHANGED:
            if (hasImsCall(slotId)) {
                //logD(RFX_LOG_TAG, "ImsCall exist, ignore the CALL_STATE_CHANGED");
                return true;
            }
            break;
        case RFX_MSG_UNSOL_CALLMOD_CHANGE_INDICATOR:
            if (hasImsCall(slotId)) {
                updatePendingMTCallMode(message);
            }
            break;
        case RFX_MSG_UNSOL_IMS_ASYNC_CALL_CONTROL_RESULT:
            handleAsyncCallControlResult(message);
            return true;
        default:
            break;
    }

    if (responseToRilJ) {
        responseToRilJAndUpdateIsImsCallExist(message);
    }
    return true;
}

bool RtcCallController::onHandleResponse(const sp<RfxMessage>& message) {
    int msg_id = message->getId();
    //logD(RFX_LOG_TAG, "onHandleResponse: %s", RFX_ID_TO_STR(msg_id));

    switch (msg_id) {
        case RFX_MSG_REQUEST_GET_CURRENT_CALLS:
            if (mRedialCtrl != NULL && mRedialCtrl->notifyRilResponse(message)) {
                return true;
            }

            handleGetCurrentCallsResponse(message);
            responseToRilJAndUpdateIsImsCallExist(message);
            return true;
        case RFX_MSG_REQUEST_IMS_EMERGENCY_DIAL:
        case RFX_MSG_REQUEST_EMERGENCY_DIAL:
            if (mRedialCtrl != NULL) {
                mRedialCtrl->notifyRilResponse(message);
            }
            // fall through
        case RFX_MSG_REQUEST_IMS_DIAL:
        case RFX_MSG_REQUEST_DIAL_WITH_SIP_URI:
        case RFX_MSG_REQUEST_CONFERENCE_DIAL:
        case RFX_MSG_REQUEST_VT_DIAL_WITH_SIP_URI:
        case RFX_MSG_REQUEST_IMS_VT_DIAL:
            if (message->getError() != RIL_E_SUCCESS) {
                imsCallEstablishFailed(message->getSlotId());
                responseToRilJAndUpdateIsImsCallExist(message);
                return true;
            }
            break;
        case RFX_MSG_REQUEST_CURRENT_STATUS:
            /* ALPS03346578: Emergency dial can be handled after receiving response of
                RFX_MSG_REQUEST_CURRENT_STATUS */
                mWaitForCurrentStatusResponse = false;
            break;

        case RFX_MSG_REQUEST_UDUB:
            {
                int callCount = getStatusManager(m_slot_id)->getIntValue(
                        RFX_STATUS_KEY_VOICE_CALL_COUNT, 0);
                int card_types = getStatusManager(m_slot_id)->getIntValue(
                        RFX_STATUS_KEY_CARD_TYPE, -1);
                if (callCount == 0 && card_types <= 0) {
                    RLOGI("handle UDUB response, update error code (callCount:%d, card_types %d)",
                            callCount, card_types);
                    /// M: CC: update response code to fix VTS
                    sp<RfxMessage> responseMsg = RfxMessage::obtainResponse(
                            RIL_E_INVALID_STATE, message, true);
                    responseToRilj(responseMsg);
                    return true;
                }
            }
            break;
        case RFX_MSG_REQUEST_ASYNC_HOLD_CALL:
        case RFX_MSG_REQUEST_ASYNC_RESUME_CALL:
            handleAsyncCallControlResponse(message);
            return true;
        default:
            break;
    }
    responseToRilj(message);
    return true;
}

bool RtcCallController::onPreviewMessage(const sp<RfxMessage>& message) {
    return canHandleEmergencyDialRequest(message);
}

void RtcCallController::handleAsyncCallControlResponse(const sp<RfxMessage>& message) {
    logD(RFX_LOG_TAG, "handleAsyncCallControlResponse %d", message->getError());
    if (mPendingCallControlMessage == NULL) {
        logD(RFX_LOG_TAG, "No pending unsynchronize call control exist");
        return;
    }
    if (message->getError() == RIL_E_ERROR_ASYNC_IMS_CALL_CONTROL_WAIT_RESULT) {
        logD(RFX_LOG_TAG, "MD accept unsynchronize call control");
        return;
    }
    sp<RfxMessage> responseMsg = RfxMessage::obtainResponse(
            message->getError(), mPendingCallControlMessage, true);
    responseToRilj(responseMsg);
    mPendingCallControlMessage = NULL;
}

void RtcCallController::handleAsyncImsCallControlRequest(const sp<RfxMessage>& message) {
    if (mPendingCallControlMessage != NULL) {
        logD(RFX_LOG_TAG, "Pending unsynchronize call control exist");
        sp<RfxMessage> responseMsg =
                RfxMessage::obtainResponse(RIL_E_GENERIC_FAILURE, message, true);
        responseToRilj(responseMsg);
        return;
    }
    mPendingCallControlMessage = message;

    void* params = message->getData()->getData();
    int msg_data[1];
    int callId = ((int*)params)[0];
    msg_data[0] = callId;

    int requestId = (message->getId() == RFX_MSG_REQUEST_HOLD_CALL)?
            RFX_MSG_REQUEST_ASYNC_HOLD_CALL : RFX_MSG_REQUEST_ASYNC_RESUME_CALL;
    sp<RfxMessage> request =
            RfxMessage::obtainRequest(getSlotId(), requestId, RfxIntsData(msg_data, 1));
    requestToMcl(request);
}

void RtcCallController::handleAsyncCallControlResult(const sp<RfxMessage>& message) {
    /* +ECCTRL: <call_id>, <cmd>, <result>, [<failed cause>]
     * call_id: ignore
     * cmd: 131 (hold), 132 (resume)
     * result: 0 (success), 1 (failed)
     * failed cause: optional failed cause
     */
    if (mPendingCallControlMessage == NULL) {
        logD(RFX_LOG_TAG, "No pending unsynchronize call control exist");
        return;
    }
    void* params = message->getData()->getData();
    int len = message->getData()->getDataLength();
    int result = ((int*)params)[2];
    RIL_Errno cause = RIL_E_SUCCESS;
    if (len == 4 && result == 1) {
        int failedCause = ((int*)params)[3];
        if (failedCause == CME_HOLD_FAILED_CAUSED_BY_TERMINATED) {
            cause = RIL_E_ERROR_IMS_HOLD_CALL_FAILED_CALL_TERMINATED;
        } else {
            cause = (RIL_Errno)failedCause;
        }
    }
    sp<RfxMessage> responseMsg =
            RfxMessage::obtainResponse(cause, mPendingCallControlMessage, true);
    responseToRilj(responseMsg);
    mPendingCallControlMessage = NULL;
}

bool RtcCallController::onCheckIfResumeMessage(const sp<RfxMessage>& message) {
    return canHandleEmergencyDialRequest(message);
}

/* ALPS03346578: Emergency dial can be handled after receiving response of
    RFX_MSG_REQUEST_CURRENT_STATUS */
bool RtcCallController::canHandleEmergencyDialRequest(const sp<RfxMessage>& message) {
    if (message->getType() == REQUEST) {
        int msg_id = message->getId();
        if (msg_id == RFX_MSG_REQUEST_EMERGENCY_DIAL ||
                msg_id == RFX_MSG_REQUEST_IMS_EMERGENCY_DIAL) {
            return (!mWaitForCurrentStatusResponse);
        }
    }
    return true;
}

bool RtcCallController::onCheckIfRejectMessage(
        const sp<RfxMessage>& message, bool isModemPowerOff, int radioState) {

    char prop_value[RFX_PROPERTY_VALUE_MAX] = {0};
    rfx_property_get("persist.mtk_wfc_support", prop_value, "0");
    int isWfcSupport = atoi(prop_value);
    int msgId = message->getId();

    // M: CC: FLIGHT MODE POWER OFF MD check
    // ro.mtk_flight_mode_power_off_md=1 : isModemPowerOff=true, radioState=RADIO_STATE_OFF
    // ro.mtk_flight_mode_power_off_md=0 : isModemPowerOff=false, radioState=RADIO_STATE_OFF
    if (isModemPowerOff && msgId == RFX_MSG_REQUEST_GET_CURRENT_CALLS) {
       return true;
    }

    // [ALPS03598523] block all request if SIM switch ongoing
    int modemOffState = getNonSlotScopeStatusManager()->getIntValue(
        RFX_STATUS_KEY_MODEM_OFF_STATE, MODEM_OFF_IN_IDLE);
    if (modemOffState == MODEM_OFF_BY_SIM_SWITCH) {
        logD(RFX_LOG_TAG, "block request due to SIM switch ongoing");
        if (msgId == RFX_MSG_REQUEST_CURRENT_STATUS ||
                msgId == RFX_MSG_REQUEST_ECC_PREFERRED_RAT) {
            return true;
        }

        // reset emergency Mode to false, before blocking EMERGENCY_DIAL
        if (msgId == RFX_MSG_REQUEST_EMERGENCY_DIAL ||
                msgId == RFX_MSG_REQUEST_IMS_EMERGENCY_DIAL) {
            mRedialCtrl->setEmergencyMode(false);
        }

        // Fake error cause as ERROR_UNSPECIFIED
        if (msgId == RFX_MSG_REQUEST_DIAL ||
                msgId == RFX_MSG_REQUEST_EMERGENCY_DIAL ||
                msgId == RFX_MSG_REQUEST_IMS_DIAL ||
                msgId == RFX_MSG_REQUEST_IMS_VT_DIAL ||
                msgId == RFX_MSG_REQUEST_IMS_EMERGENCY_DIAL) {
            mUseLocalCallFailCause = true;
            mDialLastError = CALL_FAIL_ERROR_UNSPECIFIED;
            return true;
        }
    }

    // The RIL Request should be bypass anyway
    if (isWfcSupport == 1 && !isModemPowerOff) {
        return false;
    }

    if (radioState == (int)RADIO_STATE_OFF) {
        // Need to unblock requests not handled by TCL here
        if (msgId == RFX_MSG_REQUEST_QUERY_TTY_MODE ||
                msgId == RFX_MSG_REQUEST_SET_TTY_MODE ||
                msgId == RFX_MSG_REQUEST_CURRENT_STATUS ||
                msgId == RFX_MSG_REQUEST_GET_CURRENT_CALLS ||
                msgId == RFX_MSG_REQUEST_EXIT_EMERGENCY_CALLBACK_MODE) {
            return false;
        }
        // reset emergency Mode to false, before blocking EMERGENCY_DIAL
        if (msgId == RFX_MSG_REQUEST_EMERGENCY_DIAL ||
                msgId == RFX_MSG_REQUEST_IMS_EMERGENCY_DIAL) {
            mRedialCtrl->setEmergencyMode(false);
            return true;
        }
    }

    return RfxController::onCheckIfRejectMessage(message, isModemPowerOff, radioState);
}

void RtcCallController::onServiceStateChanged(RfxStatusKeyEnum key,
        RfxVariant oldValue, RfxVariant newValue) {
    RFX_UNUSED(key);
    RfxNwServiceState oldSS = oldValue.asServiceState();
    RfxNwServiceState newSS = newValue.asServiceState();
    int voiceRadioTech = oldSS.getRilVoiceRadioTech();
    int voiceRegState = oldSS.getRilVoiceRegState();

    if (oldSS.getRilVoiceRadioTech() != newSS.getRilVoiceRadioTech()) {
        voiceRadioTech = newSS.getRilVoiceRadioTech();
        //logD(RFX_LOG_TAG, "(slot %d) Voice service state changed: radioTech=%d",
        //        m_slot_id, voiceRadioTech);
    }
    if (oldSS.getRilVoiceRegState() != newSS.getRilVoiceRegState()) {
        voiceRegState = newSS.getRilVoiceRegState();
        //logD(RFX_LOG_TAG, "(slot %d) Voice service state changed: regState=%d",
        //        m_slot_id, voiceRegState);
    }

    if (voiceRegState != RIL_REG_STATE_HOME && voiceRegState != RIL_REG_STATE_ROAMING) {
        mCallRat = CALL_RAT_NO_SERIVCE;
    } else {
        switch (voiceRadioTech) {
            case RADIO_TECH_GPRS:
            case RADIO_TECH_EDGE:
            case RADIO_TECH_GSM:
                mCallRat = CALL_RAT_GSM;
                break;
            case RADIO_TECH_UMTS:
            case RADIO_TECH_HSDPA:
            case RADIO_TECH_HSUPA:
            case RADIO_TECH_HSPA:
            case RADIO_TECH_HSPAP:
            case RADIO_TECH_TD_SCDMA:
                mCallRat = CALL_RAT_UMTS;
                break;
            case RADIO_TECH_IS95A:
            case RADIO_TECH_IS95B:
            case RADIO_TECH_1xRTT:
            case RADIO_TECH_EVDO_0:
            case RADIO_TECH_EVDO_A:
            case RADIO_TECH_EVDO_B:
            case RADIO_TECH_EHRPD:
                mCallRat = CALL_RAT_CDMA;
                break;
            case RADIO_TECH_LTE:
                mCallRat = CALL_RAT_LTE;
                break;
            case RADIO_TECH_IWLAN:
                mCallRat = CALL_RAT_WIFI;
                break;
            default:
                mCallRat = CALL_RAT_NONE;
                break;
            }
    }

    /*logD(RFX_LOG_TAG, "[onServiceStateChanged][slot: %d][callRat: %d] newSS: %s",
            m_slot_id, mCallRat, newSS.toString().string());*/
}

void RtcCallController::onCardTypeChanged(RfxStatusKeyEnum key,
    RfxVariant oldValue, RfxVariant newValue) {
    RFX_UNUSED(key);
    if (oldValue.asInt() != newValue.asInt()) {
        //logD(RFX_LOG_TAG, "[%s] oldValue: %d, newValue: %d", __FUNCTION__,
        //    oldValue.asInt(), newValue.asInt());
        if ((newValue.asInt() == 0) && isCallExistAndNoEccExist()) {
            // When SIM plugged out, hang up the mormal call directly.
            logD(RFX_LOG_TAG,"[%s], hang up normal call due to SIM plug out", __FUNCTION__);
            sp<RfxMessage> msg = RfxMessage::obtainRequest(
                    getSlotId(), RFX_MSG_REQUEST_HANGUP_ALL, RfxVoidData());
            requestToMcl(msg);
        }
    }
}

bool RtcCallController::isCallExistAndNoEccExist() {
    int size = mPreciseCallStateList->size();

    if (size <= 0) {
        return false;
    }

    for (int i = 0; i < size; i++) {
        if (mPreciseCallStateList->itemAt(i)->mCallType == CALL_TYPE_EMERGENCY) {
            return false;
        }
    }

    return true;
}

/* IMS Call Start */
bool RtcCallController::hasImsCall(int slotId) {
    if ((mImsCalls[slotId].size() == 0) && (mEstablishingCall[slotId] == NULL)) {
        return false;
    }
    return true;
}

bool RtcCallController::handleIncomingCall(int slotId, RfxStringsData* data) {
    /* +EAIC: <call_id>,<number>,<type>,<call_mode>,<seq_no>, [<redirect_num>] */
    char** params = (char**)data->getData();
    int len = data->getDataLength();
    int callId = atoi(params[0]);
    int callMode = atoi(params[3]);
    int seqNo = atoi(params[4]);

    if (rejectIncomingForDSDS(callId, seqNo)) {
        return false;
    }

    //cannot auto accept since Terminal-based call waiting is not implemented in RILD
    //autoAcceptIncoming(callId, seqNo);

    if (callMode >= MIN_IMS_CALL_MODE) {
        RtcImsCall* call = new RtcImsCall(callId, RtcImsCall::STATE_ESTABLISHED);
        addImsCall(slotId, call);
    }
    return true;
}

void RtcCallController::handleImsDialRequest(int slotId) {
    if (mEstablishingCall[slotId] != NULL) {
        logD(RFX_LOG_TAG, "An establishing ImsCall exist slot: %d", slotId);
        return;
    }
    RtcImsCall* call = new RtcImsCall(-1, RtcImsCall::STATE_ESTABLISHING);

    mEstablishingCall[slotId] = call;
    logD(RFX_LOG_TAG, "New establishing ImsCall in slot: %d", slotId);
}

bool RtcCallController::handleImsCallInfoUpdate(const sp<RfxMessage>& msg) {
    /* +ECPI: <call_id>, <msg_type>, <is_ibt>,
     *         <is_tch>, <dir>, <call_mode>, <number>, <type>, "<pau>", [<cause>] */
    int slotId = msg->getSlotId();

    RfxStringsData* data = (RfxStringsData*)msg->getData();
    char** params = (char**)data->getData();
    int callId = atoi(params[0]);
    int msgType = atoi(params[1]);
    logD(RFX_LOG_TAG, "imsCallInfoUpdate slot: %d, callId: %d, ms: %d", slotId, callId, msgType);

    if (msgType == 133 && mPendingSrvccCallCount > 0) {
        --mPendingSrvccCallCount;
        logD(RFX_LOG_TAG, "handleImsCallInfoUpdate decSrvccCallCount: %d", mPendingSrvccCallCount);
    }

    bool ret = true;
    switch (msgType) {
        case 0:
            if (waitCallRingForMT(msg)) ret = false;
            break;
        case 2:
            updateImsCallState(slotId, callId, RtcImsCall::STATE_ESTABLISHED);
            break;
        case 130:
            if (mEstablishingCall[slotId] != NULL) {
                assignImsCallId(slotId, callId);
            } else {
                generateImsConference(slotId, callId);
            }
            break;
        case 133:
            updateImsCallState(slotId, callId, RtcImsCall::STATE_TERMINATED);
            break;
        default:
            break;
    }
    return ret;
}

bool RtcCallController::handleCsCallInfoUpdate(const sp<RfxMessage>& msg) {
    /* +ECPI: <call_id>, <msg_type>, <is_ibt>,
     *         <is_tch>, <dir>, <call_mode>, <number>, <type>, "<pau>", [<cause>] */
    int slotId = msg->getSlotId();

    RfxStringsData* data = (RfxStringsData*)msg->getData();
    char** params = (char**)data->getData();
    int callId = atoi(params[0]);
    int msgType = atoi(params[1]);
    int callMode = atoi(params[5]);
    //logD(RFX_LOG_TAG, "csCallInfoUpdate slot: %d, callId: %d, ms: %d, callMode",
    //        slotId, callId, msgType, callMode);

    if (msgType == 133 && mPendingSrvccCallCount > 0) {
        mPendingSrvccCallCount = 0;
        logD(RFX_LOG_TAG, "handleCsCallInfoUpdate resetSrvccCallCount");
    }

    // Update call exist before poll call done, call end after poll call done
    switch (msgType) {
        case 130: //MO
        case 0: //MT
            mCsCallCount++;
            break;
        default:
            break;
    }

    sp<RfxMessage> urcToRilj;
    urcToRilj = RfxMessage::obtainUrc(m_slot_id, RFX_MSG_UNSOL_RESPONSE_CALL_STATE_CHANGED,
            RfxVoidData());
    responseToRilj(urcToRilj);

    return true;
}

void RtcCallController::handleSrvcc(int slotId, const sp<RfxMessage>& msg) {
    if (!hasImsCall(slotId)) {
        logD(RFX_LOG_TAG, "Abort SRVCC, no Ims Call in slotId: %d", slotId);
        return;
    }

    int state = ((int*)(msg->getData()->getData()))[0];
    if (state == 1) {// SRVCC success
        mPendingSrvccCallCount = getValidImsCallCount();
        releaseEstablishingCall(slotId);
        clearAllImsCalls(slotId);
        clearCallRingCache(slotId);
    }
    logD(RFX_LOG_TAG, "handleSrvcc in slot: %d", slotId, " state: %d", state,
            " pendingSrvccCallCount: %d", mPendingSrvccCallCount);
}

bool RtcCallController::waitCallRingForMT(const sp<RfxMessage>& msg) {
    int slotId = msg->getSlotId();
    // Keep ECPI 0, notify until CRING
    if (!mCallRingIndicated[slotId]) {
        logD(RFX_LOG_TAG, "Keep ECPI 0 for slot: %d", slotId);
        mPendingCallInfoForMT[slotId] = msg;
        return true;
    }

    // clear the CRING cache
    clearCallRingCache(slotId);
    return false;
}

void RtcCallController::updatePendingMTCallMode(const sp<RfxMessage>& msg) {
    if (mPendingCallInfoForMT[m_slot_id] == NULL) return;

    RfxStringsData* pendingMTData = (RfxStringsData*)mPendingCallInfoForMT[m_slot_id]->getData();
    RfxStringsData* callModeData = (RfxStringsData*)msg->getData();

    char** pendingMTParams = (char**)pendingMTData->getData();
    char** callModeParams = (char**)callModeData->getData();
    int pendingMTCallId = getPendingMTCallId();
    int pendingMTCallMode = atoi(pendingMTParams[5]);

    int callModeCallId = atoi(callModeParams[0]);
    int targetCallMode = atoi(callModeParams[1]);

    if (pendingMTCallId != callModeCallId) return;
    if (pendingMTCallMode == targetCallMode) return;

    logD(RFX_LOG_TAG, "updatePendingMTCallMode slot: %d, orig: %d, target: %d"
            , m_slot_id, pendingMTCallMode, targetCallMode);

    // modify the call mode
    pendingMTParams[5] = strdup(callModeParams[1]);

    int countString = pendingMTData->getDataLength() / sizeof(char *);
    RfxStringsData data(pendingMTParams, countString);

    mPendingCallInfoForMT[m_slot_id] =
            RfxMessage::obtainUrc(m_slot_id, RFX_MSG_UNSOL_CALL_INFO_INDICATION, data);

    //free(pendingMTParams[5]);
}

void RtcCallController::handleCallRing(int slotId) {
    if (mPendingCallInfoForMT[slotId] != NULL) {
        logD(RFX_LOG_TAG, "Handle Call Ring, notify ECPI 0 for slot: %d", slotId);
        responseToRilJAndUpdateIsImsCallExist(mPendingCallInfoForMT[slotId]);
        clearCallRingCache(slotId);
    } else {
        mCallRingIndicated[slotId] = true;
    }
}

void RtcCallController::addImsCall(int slotId, RtcImsCall* call) {
    logD(RFX_LOG_TAG, "Add ImsCall to slot: %d, callId: %d", slotId, call->mCallId);

    std::vector<RtcImsCall*> calls = mImsCalls[slotId];
    calls.push_back(call);
    mImsCalls[slotId] = calls;
}

void RtcCallController::removeImsCall(int slotId, RtcImsCall* call) {
    if (mImsCalls[slotId].size() == 0) {
        logD(RFX_LOG_TAG, "Abort remove ImsCall, no Ims Call in slotId: %d", slotId);
        return;
    }
    logD(RFX_LOG_TAG, "Remove ImsCall in slot: %d, callId: %d", slotId, call->mCallId);

    std::vector<RtcImsCall*> calls = mImsCalls[slotId];
    int removeIdx = -1;
    for(int i = 0; i < (int)calls.size(); ++i) {
        if (calls[i]->mCallId == call->mCallId) {
            removeIdx = i;
            break;
        }
    }

    if (removeIdx == -1) {
        logD(RFX_LOG_TAG, "Remove failed in slot: %d, callId: %d");
        return;
    }

    calls.erase(calls.begin() + removeIdx);
    mImsCalls[slotId] = calls;

    if ((int)calls.size() == 0) {
        mImsCalls.erase(slotId);
    }

    delete call;
}

void RtcCallController::updateImsCallState(int slotId, int callId, int state) {
    logD(RFX_LOG_TAG, "updateImsCallState() slotId: %d, id: %d, state: %d", slotId, callId, state);
    std::vector<RtcImsCall*> calls = mImsCalls[slotId];
    RtcImsCall* targetCall = 0;
    for(int i = 0; i < (int)calls.size(); ++i) {
        RtcImsCall* call = calls[i];
        if (call->mCallId == callId) {
            targetCall = call;
            break;
        }
    }

    if (targetCall == 0) {
        logD(RFX_LOG_TAG, "updateCallState() can't find callId: %d, in slot: %d", callId, slotId);
        return;
    }

    targetCall->mCallState = state;

    if (state == RtcImsCall::STATE_TERMINATED) {
        removeImsCall(slotId, targetCall);
        if (callId == getPendingMTCallId()) {
            clearCallRingCache(slotId);
        }
    }
}

void RtcCallController::assignImsCallId(int slotId, int callId) {
    RtcImsCall* call = mEstablishingCall[slotId];
    call->mCallId = callId;
    mEstablishingCall.erase(slotId);
    addImsCall(slotId, call);
}

void RtcCallController::imsCallEstablishFailed(int slotId) {
    RtcImsCall* call = mEstablishingCall[slotId];
    if (call == NULL) {
        logD(RFX_LOG_TAG, "imsCallEstablishFailed() no establishing call in slot: %d", slotId);
        return;
    }
    mEstablishingCall.erase(slotId);
    delete call;
}

void RtcCallController::updateIsImsCallExistToStatusManager(int slotId) {
    if (hasImsCall(slotId)) {
        logD(RFX_LOG_TAG,
               "updateIsImsCallExist() slot: %d, val: %d", slotId, (int)hasImsCall(slotId));
    }
    RfxRootController* rootController = RFX_OBJ_GET_INSTANCE(RfxRootController);
    RfxStatusManager* slotStatusMgr  = rootController->getStatusManager(slotId);
    slotStatusMgr->setBoolValue(RFX_STATUS_KEY_IMS_CALL_EXIST, hasImsCall(slotId));
    updateCallCount();
}

void RtcCallController::clearAllImsCalls(int slotId) {
    if (mImsCalls[slotId].size() == 0) {
        logD(RFX_LOG_TAG, "No Ims Call in slot: %d", slotId);
        return;
    }
    logD(RFX_LOG_TAG, "clearAllImsCalls(): %d", slotId);

    std::vector<RtcImsCall*> calls = mImsCalls[slotId];

    for(int i = 0; i < (int)calls.size(); ++i) {
        RtcImsCall* call = calls[i];
        delete call;
    }

    mImsCalls.erase(slotId);
}

void RtcCallController::releaseEstablishingCall(int slotId) {
    imsCallEstablishFailed(slotId);
}

void RtcCallController::responseToRilJAndUpdateIsImsCallExist(const sp<RfxMessage>& msg) {
    bool updateCallBeforeResponse = false;

    if (msg->getId() == RFX_MSG_UNSOL_INCOMING_CALL_INDICATION) {
        updateCallBeforeResponse = true;
    }

    if (updateCallBeforeResponse) {
        logD(RFX_LOG_TAG, "Update IsImsCallExist before response to RilJ");
        updateIsImsCallExistToStatusManager(msg->getSlotId());
        responseToRilj(msg);
    } else {
        responseToRilj(msg);
        updateIsImsCallExistToStatusManager(msg->getSlotId());
    }
}

void RtcCallController::clearCallRingCache(int slotId) {
    logD(RFX_LOG_TAG, "clearCallRingCache() slot: %d", slotId);
    mPendingCallInfoForMT.erase(slotId);
    mCallRingIndicated[slotId] = false;
}

void RtcCallController::generateImsConference(int slotId, int callId) {
    logD(RFX_LOG_TAG, "generateImsConference() slot: %d", slotId);
    RtcImsCall* call = new RtcImsCall(callId, RtcImsCall::STATE_ESTABLISHED);
    addImsCall(slotId, call);
}
/* IMS Call End */

bool RtcCallController::hasPendingHangupRequest(bool isForegnd) {
    bool ret = false;
    int origSize = mPreciseCallStateList->size();
    for (int i = 0; i < origSize; i++) {
        RfxPreciseCallState* item = mPreciseCallStateList->itemAt(i);
        if ((isForegnd && item->mOrigState == ORIG_FOREGND_DISCONNECTING) ||
                (!isForegnd && item->mOrigState == ORIG_BACKGND_DISCONNECTING)) {
            int msg_data[1];
            msg_data[0] = item->mCallId;
            sp<RfxMessage> msg = RfxMessage::obtainRequest(getSlotId(), RFX_MSG_REQUEST_FORCE_RELEASE_CALL,
                                 RfxIntsData(msg_data, 1));
            requestToMcl(msg);
            ret = true;
        }
    }
    return ret;
}

bool RtcCallController::hasPendingHangupRequest(int hangupCallId) {
    int origSize = mPreciseCallStateList->size();
    for (int i = 0; i < origSize; i++) {
        RfxPreciseCallState* item = mPreciseCallStateList->itemAt(i);
        if (item->mCallId == hangupCallId && item->mOrigState == ORIG_DISCONNECTING) {
            int msg_data[1];
            msg_data[0] = item->mCallId;
            sp<RfxMessage> msg = RfxMessage::obtainRequest(getSlotId(), RFX_MSG_REQUEST_FORCE_RELEASE_CALL,
                                 RfxIntsData(msg_data, 1));
            requestToMcl(msg);
            return true;
        }
    }
    return false;
}

void RtcCallController::handleEmergencyDialRequest(const sp<RfxMessage>& message) {
    RIL_Dial *pDial = (RIL_Dial*) (message->getData()->getData());
    String8 dialNumber = String8::format("%s", pDial->address);

    int numLen = strlen(dialNumber.string());

    memset(mEccNumberBuffer, 0, MAX_ADDRESS_LEN + 1);
    if (numLen > MAX_ADDRESS_LEN) {
        strncpy(mEccNumberBuffer, dialNumber.string(), MAX_ADDRESS_LEN);
    } else {
        strncpy(mEccNumberBuffer, dialNumber.string(), numLen);
    }

    mEccNumber = mEccNumberBuffer;

    //if (RfxRilUtils::isUserLoad() != 1) {
    //    logD(RFX_LOG_TAG, "handleEmergencyDialRequest, eccNumber = %s", mEccNumber);
    //}
}

void RtcCallController::handleCdmaFlashRequest(const sp<RfxMessage>& message) {
    char *address = (char*)(message->getData()->getData());
    if ((address != NULL) && (strlen(address) > 0)) {
        RfxPreciseCallState* preciseCallState = new RfxPreciseCallState();

        preciseCallState->mSlot = getSlotId();
        preciseCallState->mCallId = mPreciseCallStateList->size() + 1;
        preciseCallState->mCallStatus = CALL_STATUS_ACTIVE;
        preciseCallState->mOrigState = ORIG_ACTIVE;
        preciseCallState->mCallType = CALL_TYPE_VOICE;
        preciseCallState->mCallRat = mCallRat;
        preciseCallState->mCallDir = CALL_DIR_MO;

        updatePreciseCallStateList(preciseCallState, mPreciseCallStateList);
    }

    //if (RfxRilUtils::isUserLoad() != 1) {
    //    logD(RFX_LOG_TAG, "handleCdmaFlashRequest, featureCode = %s", address);
    //}
}

void RtcCallController::handleCdmaCallWait() {
    logD(RFX_LOG_TAG, "handleCdmaCallWait");

    RfxPreciseCallState* preciseCallState = new RfxPreciseCallState();

    preciseCallState->mSlot = getSlotId();
    preciseCallState->mCallId = mPreciseCallStateList->size() + 1;
    preciseCallState->mCallStatus = CALL_STATUS_ATTEMPTING;
    preciseCallState->mOrigState = ORIG_WAITING;
    preciseCallState->mCallType = CALL_TYPE_VOICE;
    preciseCallState->mCallRat = mCallRat;
    preciseCallState->mCallDir = CALL_DIR_MT;

    updatePreciseCallStateList(preciseCallState, mPreciseCallStateList);
}

void RtcCallController::handleGetCurrentCallsResponse(const sp<RfxMessage>& message) {
   // logD(RFX_LOG_TAG, "handleGetCurrentCalls() slot: %d", m_slot_id);

    /* To notify the precise call state to MDMI */
    int dataLen = message->getData()->getDataLength();
    int count = dataLen / sizeof(RIL_Call *);

    // Update call exist before poll call done, call end after poll call done
    mCsCallCount = count;
    if (mCsCallCount == 0) {
        mVtCallCount = 0;
        getStatusManager()->setIntValue(RFX_STATUS_KEY_ECC_PREFERRED_RAT, 0);
    } else {
        logD(RFX_LOG_TAG, "CS Call polled, reset SrvccCallCount");
        mPendingSrvccCallCount = 0;
    }

    RIL_Call ** pp_calls = (RIL_Call **) message->getData()->getData();
    Vector<RfxPreciseCallState*>* currentList = parsePreciseCallState(pp_calls, count);
    updateDisconnected(mPreciseCallStateList, currentList);
    freePreciseCallStateList(mPreciseCallStateList);
    mPreciseCallStateList = currentList;
}

Vector<RfxPreciseCallState*>* RtcCallController::parsePreciseCallState(RIL_Call ** pp_calls, int count) {
    //logD(RFX_LOG_TAG, "parsePreciseCallState, count: %d", count);

    Vector<RfxPreciseCallState*>* list = new Vector<RfxPreciseCallState*>();

    int id, type, dir, mpty;
    RIL_CallState status;
    char* callNumber = NULL;
    int nonMptyHeldCount = 0;
    int mptyHeldCount = 0;

    for (int i = 0; i < count; i++) {
        RIL_Call *p_cur = ((RIL_Call **) pp_calls)[i];
        RfxPreciseCallState* preciseCallState = new RfxPreciseCallState();
        status = p_cur->state;  // state
        id = p_cur->index;  // index
        dir = p_cur->isMT;  // isMT
        type = p_cur->isVoice;  // isVoice
        callNumber = p_cur->number;  // number
        mpty = p_cur->isMpty; // isMultiparty

        /// M: CC: Handle 2 hold calls cases for handover. @{
        // [ALPS03579445][ALPS03104249]
        if (status == RIL_CALL_HOLDING) {
            if (mpty == 0) {
                nonMptyHeldCount++;
            }
            if (mpty == 1) {
                mptyHeldCount++;
            }
        }
        /// @}

        preciseCallState->mSlot = getSlotId();
        preciseCallState->mCallId = id;
        preciseCallState->mOrigState = RfxPreciseCallState::RILStateToOrigState(status);
        preciseCallState->mCallStatus = RfxPreciseCallState::RILStateToCallStatus(status);

        if (type == 1) {
            if ((callNumber != NULL) && (mEccNumber != NULL) && (strlen(callNumber) > 0) &&
                    !strcmp(callNumber, mEccNumber)) {
                preciseCallState->mCallType = CALL_TYPE_EMERGENCY;
            } else {
                preciseCallState->mCallType = CALL_TYPE_VOICE;
            }
        } else {
            preciseCallState->mCallType = CALL_TYPE_VIDEO;
        }

        preciseCallState->mCallRat = mCallRat;
        preciseCallState->mCallDir = (CallDirection)dir;

        updatePreciseCallStateList(preciseCallState, list);
    }

    /// M: CC: Handle 2 hold calls cases for handover. @{
    // [ALPS03579445][ALPS03104249]
    if (nonMptyHeldCount >= 2 || (nonMptyHeldCount != 0 && mptyHeldCount >= 2)) {
        logD(RFX_LOG_TAG, "Hangup all calls due to abnormal held call exists, nonMpty=%d, mpty=%d",
            nonMptyHeldCount, mptyHeldCount);
        sp<RfxMessage> msg = RfxMessage::obtainRequest(
                getSlotId(), RFX_MSG_REQUEST_HANGUP_ALL, RfxVoidData());
        requestToMcl(msg);
    }
    /// @}

    return list;
}

void RtcCallController::updateDisconnecting(
        Vector<RfxPreciseCallState*>* origList, int hangupCallId) {
    int origSize = origList->size();
    for (int i = 0; i < origSize; i++) {
        if (origList->itemAt(i)->mCallId == hangupCallId) {
            origList->itemAt(i)->mOrigState = ORIG_DISCONNECTING;
            break;
        }
    }
}

void RtcCallController::updateDisconnecting(
        Vector<RfxPreciseCallState*>* origList, bool isForegnd) {
    int origSize = origList->size();
    for (int i = 0; i < origSize; i++) {
        if (isForegnd) {
            if (origList->itemAt(i)->mOrigState == ORIG_ACTIVE) {
                origList->itemAt(i)->mOrigState = ORIG_FOREGND_DISCONNECTING;
            }
        } else {
            if (origList->itemAt(i)->mOrigState == ORIG_HOLDING ||
                    origList->itemAt(i)->mOrigState == ORIG_WAITING) {
                origList->itemAt(i)->mOrigState = ORIG_BACKGND_DISCONNECTING;
            }
        }
    }
}

void RtcCallController::updateDisconnected(
        Vector<RfxPreciseCallState*>* oldList, Vector<RfxPreciseCallState*>* newList) {
    int oldSize = oldList->size();
    int newSize = newList->size();
    for (int i = 0; i < oldSize; i++) {
        bool disconnected = true;
        for (int j = 0; j < newSize; j++) {
            if (oldList->itemAt(i)->mCallId == newList->itemAt(j)->mCallId) {
                disconnected = false;
                break;
            }
        }
        if (disconnected) {
            RfxPreciseCallState* preciseCallState = oldList->itemAt(i);
            preciseCallState->mCallStatus = CALL_STATUS_INACTIVE;
            preciseCallState->mOrigState = ORIG_DISCONNECTED;
            updatePreciseCallStateList(preciseCallState, newList);
            if (preciseCallState->mCallType == CALL_TYPE_EMERGENCY) {
                mEccNumber = NULL;
            }
        }
    }
}

void RtcCallController::freePreciseCallStateList(Vector<RfxPreciseCallState*>* list) {
    if (list != NULL) {
        int size = list->size();
        for (int i = 0; i < size; i++) {
            delete list->itemAt(i);
        }
        delete list;
    }
}

void RtcCallController::updatePreciseCallStateList(
        RfxPreciseCallState* preciseCallState, Vector<RfxPreciseCallState*>* list) {
    preciseCallState->dump();
#ifdef MTK_MDMI_SUPPORT
    if (apmIsKpiEnabled(KPI_TYPE_CALL_EVENT_WITH_RAT)) {
        ApmCallEventWithRAT event = {0};
        event.callStatus = preciseCallState->mCallStatus;
        event.callType = preciseCallState->mCallType;
        event.ratInfo = preciseCallState->mCallRat;
        event.callDirection = preciseCallState->mCallDir;
        apmSend(KPI_TYPE_CALL_EVENT_WITH_RAT, &event);
    }
#endif
    if (preciseCallState->mCallStatus != CALL_STATUS_INACTIVE) {
        list->add(preciseCallState);
    }
}

void RtcCallController::responseDialFailed(const sp<RfxMessage>& message) {
   sp<RfxMessage> responseMsg = RfxMessage::obtainResponse(RIL_E_GENERIC_FAILURE, message, true);
    if (mRedialCtrl != NULL) {
        mRedialCtrl->notifyRilResponse(responseMsg);
    }
    responseToRilj(responseMsg);
}

bool RtcCallController::rejectDualDialForDSDS() {
    for (int i = 0; i < RfxRilUtils::getSimCount(); i++) {
        if (i != m_slot_id) {
            if (getStatusManager(i)->getIntValue(
                    RFX_STATUS_KEY_VOICE_CALL_COUNT, 0) > 0) {
                logD(RFX_LOG_TAG, "reject dial on slot%d, since slot%d has call",
                        m_slot_id, i);
                mUseLocalCallFailCause = true;
                mDialLastError = CALL_FAIL_ERROR_UNSPECIFIED;
                return true;
            }
        }
    }
    mUseLocalCallFailCause = false;
    mDialLastError = 0;
    return false;
}

bool RtcCallController::rejectIncomingForDSDS(int callId, int seqNo) {
    for(int i = 0; i < RfxRilUtils::getSimCount(); i++){
        if (i != m_slot_id) {
            if (getStatusManager(i)->getIntValue(
                    RFX_STATUS_KEY_VOICE_CALL_COUNT, 0) > 0) {
                logD(RFX_LOG_TAG, "reject MT on slot%d, since slot%d has call",
                        m_slot_id, i);
                //"AT+EAIC=1,callId,seqNo
                int msg_data[3];
                msg_data[0] = 1;  //disapprove
                msg_data[1] = callId;
                msg_data[2] = seqNo;
                sp<RfxMessage> msg = RfxMessage::obtainRequest(getSlotId(),
                        RFX_MSG_REQUEST_SET_CALL_INDICATION,
                        RfxIntsData(msg_data, 3));
                requestToMcl(msg);

                return true;
            }
        }
    }
    return false;
}

void RtcCallController::autoAcceptIncoming(int callId, int seqNo) {
    //"AT+EAIC=0,callId,seqNo
    int msg_data[3];
    msg_data[0] = 0;  //approve
    msg_data[1] = callId;
    msg_data[2] = seqNo;
    sp<RfxMessage> msg = RfxMessage::obtainRequest(getSlotId(),
            RFX_MSG_REQUEST_SET_CALL_INDICATION,
            RfxIntsData(msg_data, 3));
    requestToMcl(msg);
}

/// M: For 3G VT only @{
bool RtcCallController::reject3gVtForMultipartyCall() {
    //3G VT does not allow 1A1H
    if (getStatusManager(m_slot_id)->getIntValue(RFX_STATUS_KEY_VOICE_CALL_COUNT, 0) > 0) {
        if (!hasImsCall(m_slot_id)) { // ViLTE allows 1A1H
            logD(RFX_LOG_TAG, "reject dial, not allow MO VT if a call already exists");
            mUseLocalCallFailCause = true;
            mDialLastError = CALL_FAIL_ERROR_UNSPECIFIED;
            return true;
        }
    }
    return false;
}
/// @}

void RtcCallController::updateCallCount() {
    int callCount = getValidImsCallCount() + mCsCallCount; // PS + CS call count
    if (mCsCallCount == 0) {
        callCount += mPendingSrvccCallCount;
    }
    if (callCount != getStatusManager()->getIntValue(RFX_STATUS_KEY_VOICE_CALL_COUNT, 0)) {
        getStatusManager()->setIntValue(RFX_STATUS_KEY_VOICE_CALL_COUNT, callCount);
    }
    if (callCount != getStatusManager()->getIntValue(RFX_STATUS_KEY_AP_VOICE_CALL_COUNT, 0)) {
        getStatusManager()->setIntValue(RFX_STATUS_KEY_AP_VOICE_CALL_COUNT, callCount);
    }

    if (callCount == 0) {
        int status = getStatusManager()->getIntValue(RFX_STATUS_KEY_BTSAP_STATUS, BT_SAP_INIT);
        if (status == BT_SAP_CONNECTION_SETUP) {
            sp<RfxMessage> msg = RfxMessage::obtainRequest(
                    getSlotId(), RFX_MSG_REQUEST_LOCAL_SIM_SAP_RESET, RfxVoidData());
            requestToMcl(msg);
        }
    }
}

int RtcCallController::getValidImsCallCount() {
    int validImsCallCount = mImsCalls[m_slot_id].size();
    if (mEstablishingCall[m_slot_id] != NULL) {
        ++validImsCallCount;
    }
    return validImsCallCount;
}

int RtcCallController::getPendingMTCallId() {
    int callId = -1;
    if (mPendingCallInfoForMT[m_slot_id] != NULL) {
        RfxStringsData* data = (RfxStringsData*)mPendingCallInfoForMT[m_slot_id]->getData();
        char** params = (char**)data->getData();
        callId = atoi(params[0]);
    }
    logD(RFX_LOG_TAG, "getPendingMTCallId: %d", callId);
    return callId;
}

bool RtcCallController::shouldDoAsyncImsCallControl() {
    if (RfxRilUtils::getOperatorId(m_slot_id) == OPERATOR_KDDI) {
        return true;
    }
    return false;
}
