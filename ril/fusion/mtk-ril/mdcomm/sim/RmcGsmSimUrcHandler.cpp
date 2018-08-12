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
#include "RmcSimBaseHandler.h"
#include "RmcGsmSimUrcHandler.h"
#include <telephony/mtk_ril.h>
#include "rfx_properties.h"
#include "RfxTokUtils.h"
#include "RmcCommSimDefs.h"
// External SIM [Start]
#include "RmcCommSimUrcHandler.h"
#include "RfxVsimOpEventData.h"
// External SIM [End]

#ifdef __cplusplus
extern "C"
{
#endif
#include "usim_fcp_parser.h"
#ifdef __cplusplus
}
#endif


using ::android::String8;

static const char* gsmUrcList[] = {
    "+ESIMAPP:",
    // External SIM [Start]
    "+ERSAIND:",
    // External SIM [End]
};

// External SIM [Start]
RFX_REGISTER_DATA_TO_URC_ID(RfxVsimOpEventData, RFX_MSG_URC_SIM_VSIM_OPERATION_INDICATION);
// External SIM [End]


/*****************************************************************************
 * Class RfxController
 *****************************************************************************/
RmcGsmSimUrcHandler::RmcGsmSimUrcHandler(int slot_id, int channel_id) :
        RmcSimBaseHandler(slot_id, channel_id){
    setTag(String8("RmcGsmSimUrc"));
}

RmcGsmSimUrcHandler::~RmcGsmSimUrcHandler() {
}

RmcSimBaseHandler::SIM_HANDLE_RESULT RmcGsmSimUrcHandler::needHandle(
        const sp<RfxMclMessage>& msg) {
    RmcSimBaseHandler::SIM_HANDLE_RESULT result = RmcSimBaseHandler::RESULT_IGNORE;
    char* ss = msg->getRawUrc()->getLine();

    if (strStartsWith(ss, "+ESIMAPP:")) {
        RfxAtLine *urc = new RfxAtLine(ss, NULL);
        int err = 0;
        int app_id = -1;

        urc->atTokStart(&err);
        app_id = urc->atTokNextint(&err);
        // Only handle SIM(3) and USIM(1)
        if (app_id == UICC_APP_USIM || app_id == UICC_APP_SIM) {
            result = RmcSimBaseHandler::RESULT_NEED;
        }

        delete(urc);
    // External SIM [Start]
    } else if (strStartsWith(ss, "+ERSAIND:")) {
        result = RmcSimBaseHandler::RESULT_NEED;
    // External SIM [End]
    }
    return result;
}

void RmcGsmSimUrcHandler::handleUrc(const sp<RfxMclMessage>& msg, RfxAtLine *urc) {
    String8 ss(urc->getLine());

    if (strStartsWith(ss, "+ESIMAPP:")) {
        handleMccMnc(msg);
    // External SIM [Start]
    } else if (strStartsWith(ss, "+ERSAIND:")) {
        handleVsimEventDetected(msg);
    // External SIM [End]
    }
}

const char** RmcGsmSimUrcHandler::queryUrcTable(int *record_num) {
    const char **p = gsmUrcList;
    *record_num = sizeof(gsmUrcList)/sizeof(char*);
    return p;
}

void RmcGsmSimUrcHandler::handleMccMnc(const sp<RfxMclMessage>& msg) {
    int appTypeId = -1, channelId = -1, err = 0;
    char *pMcc = NULL, *pMnc = NULL;
    RfxAtLine *atLine = msg->getRawUrc();
    String8 numeric("");
    String8 prop("gsm.ril.uicc.mccmnc");

    prop.append((m_slot_id == 0)? "" : String8::format(".%d", m_slot_id));

    do {
        atLine->atTokStart(&err);
        if (err < 0) {
            break;
        }

        appTypeId = atLine->atTokNextint(&err);
        if (err < 0) {
            break;
        }

        channelId = atLine->atTokNextint(&err);
        if (err < 0) {
            break;
        }

        pMcc = atLine->atTokNextstr(&err);
        if (err < 0) {
            break;
        }

        pMnc = atLine->atTokNextstr(&err);
        if (err < 0) {
            break;
        }

        numeric.append(String8::format("%s%s", pMcc, pMnc));

        logD(mTag, "numeric: %s", numeric.string());

        rfx_property_set(prop, numeric.string());

        getMclStatusManager()->setString8Value(RFX_STATUS_KEY_UICC_GSM_NUMERIC, numeric);
    } while(0);
}

// External SIM [Start]
void RmcGsmSimUrcHandler::handleVsimEventDetected(const sp<RfxMclMessage>& msg) {
    RIL_VsimOperationEvent event;
    int event_type;
    int err = 0;
    RfxAtLine *line = msg->getRawUrc();

    memset(&event, 0, sizeof(event));
    // 256 bytes for data + 5 bytes header information: CLA,INS,P1,P2,P3 */
    //  #define APDU_REQ_MAX_LEN (261)
    // 256 bytes for data + 2 bytes status word SW1 and SW2 */
    //  #define APDU_RSP_MAX_LEN (258)
    //  --> Need to *2, since the received data is hex string.
    char *temp_data = NULL;

    /**
     * [+ERSAIND URC Command Usage]
     * +ERSAIND: <msg_id>[, <parameter1> [, <parameter2> [, <\A1K>]]]
     *
     * <msg_id>:
     * 0 (APDU indication) // Send APDU to AP
     *       <parameter1>: command apdu // APDU data need send to card (string)
     * 1 (card reset indication)
     * 2 (power-down indication)
     *       <parameter1>: mode // Not define mode yet, [MTK]0 is default
     * 3 (vsim trigger plug out)
     * 4 (vsim trigger plug in)
     */

    line->atTokStart(&err);
    if (err < 0) {
        goto done;
    }

    event_type = line->atTokNextint(&err);
    if (err < 0) {
        logE(mTag, "[VSIM]onVsimEventDetected get type fail!");
        goto done;
    }

    switch (event_type) {
        case 0:
            RmcCommSimUrcHandler::setMdWaitingResponse(m_slot_id, VSIM_MD_WAITING_APDU);
            event.eventId = MSG_ID_UICC_APDU_REQUEST; //REQUEST_TYPE_APDU_EVENT;

            if (line->atTokHasmore()) {
                event.data = line->atTokNextstr(&err);
                if (err < 0) {
                    logE(mTag, "[VSIM]onVsimEventDetected get string fail!");
                    goto done;
                }

                event.data_length = strlen(event.data);
             }
            break;
        case 1:
            logD(mTag, "[VSIM]onVsimEventDetected: card reset request");
            RmcCommSimUrcHandler::setMdWaitingResponse(m_slot_id, VSIM_MD_WAITING_ATR);
            event.eventId = MSG_ID_UICC_RESET_REQUEST; //REQUEST_TYPE_ATR_EVENT;
            break;
        case 2:
            logD(mTag, "[VSIM]onVsimEventDetected: card power down");
            RmcCommSimUrcHandler::setMdWaitingResponse(m_slot_id, VSIM_MD_WAITING_RESET);
            return;
            /*event.eventId = REQUEST_TYPE_CARD_POWER_DOWN;
            event.result = 0;   //Use result filed to record mode information
            if (line->atTokHasmore()) {
                 event.result = line->atTokNextint(&err);
                 if (err < 0) {
                     logE(mTag, "[VSIM]onVsimEventDetected get power down mode fail!");
                     goto done;
                 }
            }
            break;*/
        case 3:
            logD(mTag, "[VSIM]onVsimEventDetected: VSIM_TRIGGER_PLUG_OUT");
            RmcCommSimUrcHandler::setVsimPlugInOutEvent(m_slot_id, VSIM_TRIGGER_PLUG_OUT);
            return;
        case 4:
            logD(mTag, "[VSIM]onVsimEventDetected: VSIM_TRIGGER_PLUG_IN");
            RmcCommSimUrcHandler::setVsimPlugInOutEvent(m_slot_id, VSIM_TRIGGER_PLUG_IN);
            return;
        default:
            logD(mTag, "[VSIM]onVsimEventDetected: unsupport type");
    }

done:
    sp<RfxMclMessage> unsol = RfxMclMessage::obtainUrc(RFX_MSG_URC_SIM_VSIM_OPERATION_INDICATION,
                        m_slot_id, RfxVsimOpEventData((void*)&event, sizeof(event)));

    responseToTelCore(unsol);
 }
// External SIM [End]

