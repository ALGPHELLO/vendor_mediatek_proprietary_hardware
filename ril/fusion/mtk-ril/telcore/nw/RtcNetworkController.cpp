/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 *
 * MediaTek Inc. (C) 2017. All rights reserved.
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
#include "RtcNetworkController.h"
#include <cutils/properties.h>

/*****************************************************************************
 * Class RfxController
 *****************************************************************************/

#define NW_CTRL_TAG "RtcNwCtrl"

RFX_IMPLEMENT_CLASS("RtcNetworkController", RtcNetworkController, RfxController);

RtcNetworkController::RtcNetworkController() {
}

RtcNetworkController::~RtcNetworkController() {
}

void RtcNetworkController::onInit() {
    // Required: invoke super class implementation
    RfxController::onInit();
    RFX_LOG_D(NW_CTRL_TAG, "[onInit]");
    const int request_id_list[] = {
        RFX_MSG_REQUEST_SET_BAND_MODE,
        RFX_MSG_REQUEST_GET_NEIGHBORING_CELL_IDS,
        RFX_MSG_REQUEST_SET_UNSOL_CELL_INFO_LIST_RATE,
        RFX_MSG_REQUEST_VOICE_REGISTRATION_STATE,
        RFX_MSG_REQUEST_DATA_REGISTRATION_STATE,
        RFX_MSG_REQUEST_OPERATOR,
        RFX_MSG_REQUEST_QUERY_NETWORK_SELECTION_MODE,
        RFX_MSG_REQUEST_SCREEN_STATE,
        RFX_MSG_REQUEST_SET_UNSOLICITED_RESPONSE_FILTER,
        RFX_MSG_REQUEST_SET_SERVICE_STATE,
        RFX_MSG_REQUEST_CDMA_SET_ROAMING_PREFERENCE,
        RFX_MSG_REQUEST_CDMA_QUERY_ROAMING_PREFERENCE
    };

    const int urc_id_list[] = {
    };

    // register request & URC id list
    // NOTE. one id can only be registered by one controller
    registerToHandleRequest(request_id_list, sizeof(request_id_list)/sizeof(const int), DEFAULT);
    registerToHandleUrc(urc_id_list, sizeof(urc_id_list)/sizeof(const int));

}

bool RtcNetworkController::onHandleRequest(const sp<RfxMessage>& message) {
    // logD(NW_CTRL_TAG, "[onHandleRequest] %s", RFX_ID_TO_STR(message->getId()));
    requestToMcl(message);
    return true;
}

bool RtcNetworkController::onHandleUrc(const sp<RfxMessage>& message) {
    responseToRilj(message);
    return true;
}

bool RtcNetworkController::onHandleResponse(const sp<RfxMessage>& response) {
    responseToRilj(response);
    return true;
}

bool RtcNetworkController::onCheckIfRejectMessage(const sp<RfxMessage>& message,
        bool isModemPowerOff,int radioState) {

    /* Reject the request in radio unavailable or modem off */
    if (radioState == (int)RADIO_STATE_UNAVAILABLE ||
            isModemPowerOff == true) {
        RFX_LOG_D(NW_CTRL_TAG, "onCheckIfRejectMessage, id = %d, isModemPowerOff = %d, rdioState = %d",
                message->getId(), isModemPowerOff, radioState);
        return true;
    }

    return false;
}

