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

#ifndef __RFX_CALL_CONTROLLER_H__
#define __RFX_CALL_CONTROLLER_H__

/*****************************************************************************
 * Include
 *****************************************************************************/
#include <map>
#include <vector>

#include "RfxPreciseCallState.h"
#include "RtcRedialController.h"
#include "utils/String16.h"
#include "utils/String8.h"
#include "utils/Vector.h"
#ifdef MTK_MDMI_SUPPORT
#include <ap_monitor.h>
#include <ap_monitor_defs.h>
#endif

using ::android::String16;
using ::android::String8;
using ::android::Vector;

#define MAX_ADDRESS_LEN     40

/*****************************************************************************
 * Class RfxController
 *****************************************************************************/

class RfxStringsData;

class RtcImsCall {
public:
    enum {
        STATE_ESTABLISHING,
        STATE_ESTABLISHED,
        STATE_TERMINATED,
    };

    int mCallId;
    int mCallState;

    RtcImsCall(int id, int state);
};

class RtcCallController : public RfxController {
    // Required: declare this class
    RFX_DECLARE_CLASS(RtcCallController);

public:
    RtcCallController();
    virtual ~RtcCallController();

// Override
protected:
    virtual void onInit();
    virtual void onDeinit();
    virtual bool onHandleRequest(const sp<RfxMessage>& message);
    virtual bool onHandleUrc(const sp<RfxMessage>& message);
    virtual bool onHandleResponse(const sp<RfxMessage>& message);
    virtual bool onPreviewMessage(const sp<RfxMessage>& message);
    virtual bool onCheckIfResumeMessage(const sp<RfxMessage>& message);
    virtual bool onCheckIfRejectMessage(
        const sp<RfxMessage>& message, bool isModemPowerOff, int radioState);
    virtual void createRedialController();

protected:
    RtcRedialController *mRedialCtrl;

private:
//    void onCsPhoneChanged(RfxStatusKeyEnum key, RfxVariant old_value, RfxVariant value);

    void onServiceStateChanged(RfxStatusKeyEnum key, RfxVariant oldValue, RfxVariant newValue);

    void onCardTypeChanged(RfxStatusKeyEnum key, RfxVariant oldValue, RfxVariant newValue);
    bool isCallExistAndNoEccExist();

    bool handleCsCallInfoUpdate(const sp<RfxMessage>& msg);

    // handle Ims call
    bool hasImsCall(int slotId);
    bool handleIncomingCall(int slotId, RfxStringsData* data);
    bool handleImsCallInfoUpdate(const sp<RfxMessage>& msg);
    void handleSrvcc(int slotId, const sp<RfxMessage>& msg);
    void addImsCall(int slotId, RtcImsCall* call);
    void removeImsCall(int slotId, RtcImsCall* call);
    void updateImsCallState(int slotId, int callId, int state);
    void assignImsCallId(int slotId, int callId);
    void clearAllImsCalls(int slotId);
    void releaseEstablishingCall(int slotId);
    bool waitCallRingForMT(const sp<RfxMessage>& msg);
    void handleCallRing(int slotId);
    void clearCallRingCache(int slotId);
    void generateImsConference(int slotId, int callId);

    // forceRelease
    bool hasPendingHangupRequest(bool isForegnd);
    bool hasPendingHangupRequest(int hangupCallId);

    // call state cache
    void handleEmergencyDialRequest(const sp<RfxMessage>& message);
    void handleCdmaFlashRequest(const sp<RfxMessage>& message);
    void handleCdmaCallWait();
    void handleGetCurrentCallsResponse(const sp<RfxMessage>& message);
    Vector<RfxPreciseCallState*>* parsePreciseCallState(RIL_Call ** pp_calls, int count);
    void updateDisconnecting(Vector<RfxPreciseCallState*>* origList, int hangupCallId);
    void updateDisconnecting(Vector<RfxPreciseCallState*>* origList, bool isForegnd);
    void updateDisconnected(
            Vector<RfxPreciseCallState*>* oldList, Vector<RfxPreciseCallState*>* newList);
    void freePreciseCallStateList(Vector<RfxPreciseCallState*>* list);
    void updatePreciseCallStateList(
            RfxPreciseCallState* preciseCallState, Vector<RfxPreciseCallState*>* list);

    // error handling
    bool rejectIncomingForDSDS(int callId, int seqNo);
    void autoAcceptIncoming(int callId, int seqNo);
    bool reject3gVtForMultipartyCall();

    /* ALPS03346578: Emergency dial can be handled after receiving response of
        RFX_MSG_REQUEST_CURRENT_STATUS */
    bool canHandleEmergencyDialRequest(const sp<RfxMessage>& message);
    void updateCallCount();
    int getValidImsCallCount();

    int getPendingMTCallId();

    void updatePendingMTCallMode(const sp<RfxMessage>& msg);
    void handleAsyncCallControlResult(const sp<RfxMessage>& message);
    void handleAsyncCallControlResponse(const sp<RfxMessage>& message);
    void handleAsyncImsCallControlRequest(const sp<RfxMessage>& message);
    bool shouldDoAsyncImsCallControl();

    CallRat mCallRat;
    char* mEccNumber;
    char mEccNumberBuffer[MAX_ADDRESS_LEN + 1];
    Vector<RfxPreciseCallState*>* mPreciseCallStateList;
    bool mUseLocalCallFailCause;
    int mDialLastError;
    int mVtCallCount;
    int mCsCallCount;

    // key = slot, value = call
    std::map<int, std::vector<RtcImsCall*>> mImsCalls;
    std::map<int, RtcImsCall*> mEstablishingCall;
    std::map<int, sp<RfxMessage>> mPendingCallInfoForMT;
    std::map<int, bool> mCallRingIndicated;
    sp<RfxMessage> mPendingCallControlMessage;

    /* ALPS03346578: Emergency dial can be handled after receiving response of
        RFX_MSG_REQUEST_CURRENT_STATUS */
    bool mWaitForCurrentStatusResponse;
    int mPendingSrvccCallCount;

protected:
    void responseDialFailed(const sp<RfxMessage>& message);
    // error handling
    bool rejectDualDialForDSDS();
    // handle Ims call
    void handleImsDialRequest(int slotId);
    void imsCallEstablishFailed(int slotId);
    void responseToRilJAndUpdateIsImsCallExist(const sp<RfxMessage>& msg);
    void updateIsImsCallExistToStatusManager(int slotId);
};

#endif /* __RFX_CALL_CONTROLLER_H__ */
