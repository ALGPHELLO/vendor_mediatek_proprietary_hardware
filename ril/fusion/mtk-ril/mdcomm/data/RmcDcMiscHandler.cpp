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
#include "RmcDcMiscHandler.h"
#include "RfxMessageId.h"

#define RFX_LOG_TAG "RmcDcMiscHandler"

/*****************************************************************************
 * Class RmcDcMiscHandler
 * Only receive request and event in this class.
 * Create RmcDcMiscImpl object to send AT Command to modem.
 *****************************************************************************/
RFX_IMPLEMENT_HANDLER_CLASS(RmcDcMiscHandler, RIL_CMD_PROXY_5);

RmcDcMiscHandler::RmcDcMiscHandler(int slotId, int channelId)
        : RfxBaseHandler(slotId, channelId), m_pRmcDcMiscImpl(NULL) {

    const int requestList[] = {
        RFX_MSG_REQUEST_START_LCE,
        RFX_MSG_REQUEST_STOP_LCE,
        RFX_MSG_REQUEST_PULL_LCEDATA,
        RFX_MSG_REQUEST_SET_FD_MODE,
    };

    const int eventList[] = {
        RFX_MSG_EVENT_DATA_LCE_STATUS_CHANGED,
        RFX_MSG_EVENT_RADIO_CAPABILITY_UPDATED,
    };

    registerToHandleRequest(requestList, sizeof(requestList) / sizeof(int));
    registerToHandleEvent(eventList, sizeof(eventList) / sizeof(int));

    m_pRmcDcMiscImpl = new RmcDcMiscImpl(this, slotId);
}

RmcDcMiscHandler::~RmcDcMiscHandler() {
    delete m_pRmcDcMiscImpl;
    m_pRmcDcMiscImpl = NULL;
}

void RmcDcMiscHandler::onHandleRequest(const sp<RfxMclMessage>& msg) {
    switch (msg->getId()) {
        case RFX_MSG_REQUEST_START_LCE:
            handleStartLceRequest(msg);
            break;
        case RFX_MSG_REQUEST_STOP_LCE:
            handleStopLceRequest(msg);
            break;
        case RFX_MSG_REQUEST_PULL_LCEDATA:
            handlePullLceDataRequest(msg);
            break;
        case RFX_MSG_REQUEST_SET_FD_MODE:
            handleSetFdModeRequest(msg);
            break;
        default:
            RFX_LOG_W(RFX_LOG_TAG, "[%d][%s]: Unknown request, ignore!", m_slot_id, __FUNCTION__);
            break;
    }
}

void RmcDcMiscHandler::onHandleEvent(const sp<RfxMclMessage>& msg) {
    switch (msg->getId()) {
        case RFX_MSG_EVENT_DATA_LCE_STATUS_CHANGED:
            handleLceStatusChanged(msg);
            break;
        case RFX_MSG_EVENT_RADIO_CAPABILITY_UPDATED:
            init();
            break;
        default:
            RFX_LOG_W(RFX_LOG_TAG, "[%d][%s]: Unknown event, ignore!", m_slot_id, __FUNCTION__);
            break;
    }
}

void RmcDcMiscHandler::init() {
    RFX_ASSERT(m_pRmcDcMiscImpl != NULL);
    m_pRmcDcMiscImpl->init();
}

void RmcDcMiscHandler::handleStartLceRequest(const sp<RfxMclMessage>& msg) {
    RFX_ASSERT(m_pRmcDcMiscImpl != NULL);
    m_pRmcDcMiscImpl->requestStartLce(msg);
}

void RmcDcMiscHandler::handleStopLceRequest(const sp<RfxMclMessage>& msg) {
    RFX_ASSERT(m_pRmcDcMiscImpl != NULL);
    m_pRmcDcMiscImpl->requestStopLce(msg);
}

void RmcDcMiscHandler::handlePullLceDataRequest(const sp<RfxMclMessage>& msg) {
    RFX_ASSERT(m_pRmcDcMiscImpl != NULL);
    m_pRmcDcMiscImpl->requestPullLceData(msg);
}

void RmcDcMiscHandler::handleLceStatusChanged(const sp<RfxMclMessage>& msg) {
    RFX_ASSERT(m_pRmcDcMiscImpl != NULL);
    m_pRmcDcMiscImpl->onLceStatusChanged(msg);
}

void RmcDcMiscHandler::handleSetFdModeRequest(const sp<RfxMclMessage>& msg) {
    RFX_ASSERT(m_pRmcDcMiscImpl != NULL);
    m_pRmcDcMiscImpl->setFdMode(msg);
}
