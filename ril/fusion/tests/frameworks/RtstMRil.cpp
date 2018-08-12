/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 */
/* MediaTek Inc. (C) 2016. All rights reserved.
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
 #include "RtstMRil.h"
 #include "RtstEnv.h"
 #include "RfxChannelManager.h"
 #include "RfxRilUtils.h"
 #include "RtstData.h"
 #include "RtstUtils.h"
#include <telephony/librilutilsmtk.h>

/*****************************************************************************
 * Define
 *****************************************************************************/
#define TAG "RTF"

/*****************************************************************************
 * Local Function
 *****************************************************************************/
extern "C"
void onRequestComplete(
        RIL_Token t, RIL_Errno e, void *response, size_t responselen) {
    RequestInfo *pRI = (RequestInfo *)t;
    RIL_SOCKET_ID socket_id = pRI->socket_id;
    int32_t id = pRI->pCI->requestNumber;
    RFX_LOG_E(TAG, "onRequestComplete id = %d, slot = %d, e = %d, responselen = %zu, response = %p",
        id, socket_id, e, responselen, response);
    Parcel p;
    p.writeInt32(id);
    p.writeInt32(e);
    if (response != NULL) {
        pRI->pCI->responseFunction(p, response, responselen);
    }
    Parcel q;
    q.writeInt32(p.dataSize());
    RtstEnv::get()->getRilSocket2((int)socket_id)
            .write((void *)q.data(), q.dataSize());
    RtstEnv::get()->getRilSocket2((int)socket_id)
            .write((void *)p.data(), p.dataSize());
}


extern "C"
#if defined(ANDROID_MULTI_SIM)
void onUnsolicitedResponse(
        int unsolResponse, const void *data,
        size_t datalen, RIL_SOCKET_ID socket_id) {
#else
void onUnsolicitedResponse(
        int unsolResponse, const void *data, size_t datalen) {
    RIL_SOCKET_ID socket_id = 0;
#endif
    Parcel p;
    p.writeInt32(unsolResponse);
    RFX_LOG_D(TAG, "onUnsolicitedResponse id = %d, slot = %d, datalen = %zu, %p", unsolResponse,
        socket_id, datalen, data);
    UnsolResponseInfo *pURI = RtstGRil::getUrcInfo(unsolResponse);
    if (pURI == NULL) {
        AssertionFailure() << "String value mismatch!"
                                  << " Expected: UrcInfo is NULL";
        return;
    }
    if (data != NULL) {
        pURI->responseFunction(p, (void *)data, datalen);
    }
    Parcel q;
    q.writeInt32(p.dataSize());
    RtstEnv::get()->getRilSocket2((int)socket_id)
            .write((void *)q.data(), q.dataSize());
    RtstEnv::get()->getRilSocket2((int)socket_id)
            .write((void *)p.data(), p.dataSize());
}

extern "C"
void onUpdateValueForGT(int slotId,  const RfxStatusKeyEnum key,
    const RfxVariant &value) {
    RFX_LOG_V(TAG, "onUpdateValueForGT, slotId:%d, Key:%d", slotId, key);
    RtstUtils::setStatusValueForGT(slotId, key, value, false);
}

extern "C"
void onRequestAck(RIL_Token t) {
    RequestInfo *pRI;
    RIL_SOCKET_ID socket_id = RIL_SOCKET_1;
    pRI = (RequestInfo *)t;
    socket_id = pRI->socket_id;
    RFX_LOG_D(TAG, "onRequestAck id = %d, slot = %d", pRI->pCI->requestNumber, socket_id);
    if (pRI->cancelled == 0) {
        Parcel p;
        p.writeInt32(pRI->pCI->requestNumber);
        p.writeInt32 (RESPONSE_SOLICITED_ACK);
        RtstEnv::get()->getRilReqAckSocket2((int)socket_id)
                .write((void *)p.data(), p.dataSize());
    }
}

/*****************************************************************************
 * Structure
 *****************************************************************************/
extern "C"  {

static struct RIL_Env s_rtstRilEnv = {
    onRequestComplete,
    onUnsolicitedResponse,
    NULL,
    onRequestAck
};

}

/*****************************************************************************
 * External Functions Declaration
 *****************************************************************************/
extern "C"  void RIL_setRilEnvForGT(const struct RIL_Env *env);

/*****************************************************************************
 * class RtstMRil
 *****************************************************************************/
void RtstMRil::setEmulatorMode() {
    RfxRilUtils::setRilRunMode(RilRunMode::RIL_RUN_MODE_MOCK);
}

void RtstMRil::setChannelFd(int fd[MAX_SIM_COUNT][RFX_MAX_CHANNEL_NUM]) {
    int fdNum = getSimCount()*RFX_MAX_CHANNEL_NUM;
    if (fdNum <= 0) {
        return;
    }
    int fdGT[fdNum];
    for (int index =0; index < getSimCount(); index++) {
        for (int j = 0; j < RFX_MAX_CHANNEL_NUM; j++) {
            fdGT[index * RFX_MAX_CHANNEL_NUM + j] = fd[index][j];
        }
    }
    RfxChannelManager::setChannelFdForGT(fdGT);
}

void RtstMRil::setRilEnv() {
    RIL_setRilEnvForGT(&s_rtstRilEnv);
}

void RtstMRil::setCallbackForStatusUpdate() {
    RtstUtils::setStatusCallbackForGT(onUpdateValueForGT);
}
