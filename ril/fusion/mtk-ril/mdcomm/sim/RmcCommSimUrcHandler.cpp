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
#include "RfxStatusDefs.h"
#include "RfxVoidData.h"
#include "RfxStringData.h"
#include "RmcSimBaseHandler.h"
#include "RmcCommSimUrcHandler.h"
#include "RmcCommSimDefs.h"
#include <telephony/mtk_ril.h>
#include "utils/String8.h"
#include "rfx_properties.h"
#include "RfxMessageId.h"
#include "RfxMisc.h"
#include "RfxRilUtils.h"
#include <cutils/properties.h>
#include "RmcCapabilitySwitchUtil.h"
#include "RmcCapabilitySwitchRequestHandler.h"

#ifdef __cplusplus
extern "C"
{
#endif
#include <hardware_legacy/power.h>
#ifdef __cplusplus
}
#endif


using ::android::String8;

static const char* commUrcList[] = {
    "+EUSIM:",
    "+ETESTSIM:",
    "+ESIMS:",
    "+ECPIN:",
    "+ESIMIND:",
};

int RmcCommSimUrcHandler::mTrayPluginCount = 0;

// External SIM [Start]
int RmcCommSimUrcHandler::mVsimPlugInOut[MAX_SIM_COUNT] = {0};
int RmcCommSimUrcHandler::mVsimMdWaiting[MAX_SIM_COUNT] = {-1, -1, -1, -1};
// External SIM [End]

/*****************************************************************************
 * Class RfxController
 *****************************************************************************/
RmcCommSimUrcHandler::RmcCommSimUrcHandler(int slot_id, int channel_id) :
        RmcSimBaseHandler(slot_id, channel_id) {
    setTag(String8("RmcCommSimUrc"));
}

RmcCommSimUrcHandler::~RmcCommSimUrcHandler() {
}

RmcSimBaseHandler::SIM_HANDLE_RESULT RmcCommSimUrcHandler::needHandle(
        const sp<RfxMclMessage>& msg) {
    RmcSimBaseHandler::SIM_HANDLE_RESULT result = RmcSimBaseHandler::RESULT_IGNORE;
    char* ss = msg->getRawUrc()->getLine();

    if (strStartsWith(ss, "+EUSIM:") ||
            strStartsWith(ss, "+ESIMS: 0,0") ||
            strStartsWith(ss, "+ESIMS: 0,10") ||
            strStartsWith(ss, "+ESIMS: 0,11") ||
            strStartsWith(ss, "+ESIMS: 0,13") ||
            strStartsWith(ss, "+ESIMS: 0,15") ||
            // MTK-START: AOSP SIM PLUG IN/OUT
            strStartsWith(ss, "+ESIMS: 0,26") ||
            // MTK-EDN
            strStartsWith(ss, "+ESIMS: 1,2") ||
            strStartsWith(ss, "+ESIMS: 1,9") ||
            strStartsWith(ss, "+ESIMS: 1,12") ||
            strStartsWith(ss, "+ESIMS: 1,14") ||
            strStartsWith(ss, "+ECPIN:") ||
            strStartsWith(ss, "+ESIMIND:")) {
        result = RmcSimBaseHandler::RESULT_NEED;
    }
    return result;
}

void RmcCommSimUrcHandler::handleUrc(const sp<RfxMclMessage>& msg, RfxAtLine *urc) {
    char* ss = urc->getLine();

    if(strStartsWith(ss, "+EUSIM:")) {
        handleCardType(msg, urc);
    } else if (strStartsWith(ss, "+ESIMS:")) {
        handleSimStateChanged(msg, (char*)ss);
    } else if (strStartsWith(ss, "+ECPIN:")) {
        handleEcpin(msg, urc);
    } else if (strStartsWith(ss, "+ESIMIND:")) {
        handleSimIndication(msg, urc);
    }

}

const char** RmcCommSimUrcHandler::queryUrcTable(int *record_num) {
    const char** p = commUrcList;
    *record_num = sizeof(commUrcList)/sizeof(char*);
    return p;
}

void RmcCommSimUrcHandler::handleEcpin(const sp<RfxMclMessage>& msg, RfxAtLine *urc) {
    int err = 0, cpin_status = 0, vsim_init = -1;
    RfxAtLine *atLine = urc;
    sp<RfxMclMessage> unsol = NULL;

    atLine->atTokStart(&err);
    if (err < 0) {
        goto error;
    }

    cpin_status = atLine->atTokNextint(&err);
    if (err < 0) {
        goto error;
    }

    vsim_init = getMclStatusManager()->getIntValue(RFX_STATUS_KEY_ECPIN_STATE,
            RFX_ECPIN_DONE);
    logD(mTag, "CPIN status %d, VSIM initialization state %d", cpin_status, vsim_init);
    if ((cpin_status == 1) && (vsim_init == RFX_WAIT_FOR_ECPIN)) {
        getMclStatusManager()->setIntValue(RFX_STATUS_KEY_ECPIN_STATE, RFX_ECPIN_DONE);
        unsol = RfxMclMessage::obtainUrc(RFX_MSG_URC_RESPONSE_SIM_STATUS_CHANGED,
                m_slot_id, RfxVoidData());
        responseToTelCore(unsol);
    }
    return;
error:
    logE(mTag, "Parse ECPIN fail: %s/n", msg->getRawUrc()->getLine());
}

void RmcCommSimUrcHandler::handleCardType(const sp<RfxMclMessage>& msg, RfxAtLine *urc) {
    int type3gpp = 0, type3gpp2 = 0, typeIsim = 0, typeValue = -1, err = 0;
    RfxAtLine *atLine = urc;
    String8 type("");
    String8 fullUiccCardType("gsm.ril.fulluicctype");
    String8 propUicc("gsm.ril.uicctype");

    propUicc.append((m_slot_id == 0)? "" : String8::format(".%d", (m_slot_id + 1)));
    fullUiccCardType.append((m_slot_id == 0)? "" : String8::format(".%d", (m_slot_id + 1)));

    atLine->atTokStart(&err);
    if (err < 0) {
        goto error;
    }

    type3gpp = atLine->atTokNextint(&err);
    if (err < 0) {
        goto error;
    }

    if (type3gpp == 1) {
        rfx_property_set(propUicc, "USIM");
        type.append("USIM");
        typeValue = RFX_CARD_TYPE_USIM;
    } else if (type3gpp == 0) {
        rfx_property_set(propUicc, "SIM");
        type.append("SIM");
        typeValue = RFX_CARD_TYPE_SIM;
    } else {
        logD(mTag, "The SIM card is neither USIM nor SIM!");
        typeValue = 0;
    }

    /* Check CDMA card type */
    if (atLine->atTokHasmore()) {
        type3gpp2 = atLine->atTokNextint(&err);
        if (err < 0) {
            logE(mTag, "Get CDMA type fail!");
        } else {
            switch (type3gpp2) {
                case 0:
                    rfx_property_set(propUicc, "RUIM");
                    if (type.length() > 0) {
                        type.append(",");
                    }
                    type.append("RUIM");
                    typeValue |= RFX_CARD_TYPE_RUIM;
                    break;
                case 1:
                    rfx_property_set(propUicc, "CSIM");
                    if (type.length() > 0) {
                        type.append(",");
                    }
                    type.append("CSIM");
                    typeValue |= RFX_CARD_TYPE_CSIM;
                    break;
                case 3:
                    rfx_property_set(propUicc, "CSIM");
                    if (type.length() > 0) {
                        type.append(",");
                    }
                    type.append("RUIM,CSIM");
                    typeValue |= RFX_CARD_TYPE_RUIM;
                    typeValue |= RFX_CARD_TYPE_CSIM;
                    break;
                default:
                    logD(mTag, "The SIM card is neither RUIM nor CSIM!");
                    break;
            }
        }
    }

    // Check ISIM
    if (atLine->atTokHasmore()) {
        typeIsim = atLine->atTokNextint(&err);
        if (err < 0) {
            logE(mTag, "Fail to get ISIM state!");
        } else {
            switch (typeIsim) {
                case 1:
                    typeValue |= RFX_CARD_TYPE_ISIM;
                    break;
                default:
                    break;
            }
        }
    }

    if ((type3gpp == 2 && type3gpp2 == 2) || (type3gpp == 2 && type3gpp2 == -1)) {
        logD(mTag, "There is no card type!!!");
        type.append("N/A");
        rfx_property_set(fullUiccCardType, "");
        rfx_property_set(propUicc, "");
        typeValue = 0;
    } else {
        logD(mTag, "The card type is %s(%d)", type.string(), typeValue);
        // property_set(propUicc, card_type);
        rfx_property_set(fullUiccCardType, type.string());
    }

    getMclStatusManager()->setIntValue(RFX_STATUS_KEY_CARD_TYPE, typeValue);
    if (typeValue >= 0) {
        getMclStatusManager()->setBoolValue(RFX_STATUS_KEY_MODEM_SIM_TASK_READY, true, true);
    }

    // To query ICCID
    sendEvent(RFX_MSG_EVENT_SIM_QUERY_ICCID, RfxVoidData(), RIL_CMD_PROXY_1, m_slot_id);
    acquire_wake_lock(PARTIAL_WAKE_LOCK, "sim_hot_plug");
    return;
error:
    logE(mTag, "Parse EUSIM fail: %s/n", msg->getRawUrc()->getLine());
}

void RmcCommSimUrcHandler::handleSimStateChanged(const sp<RfxMclMessage>& msg, char* urc) {
    bool common_slot_no_changed = false;

    RFX_UNUSED(msg);

    //+ESIMS: 0,0: SIM Missing
    //+ESIMS: 0,13: Recovery start
    //+ESIMS: 0,10: Virtual SIM off
    //+ESIMS: 0,11: SIM plug out
    //+ESIMS: 0.15: IMEI Lock
    // MTK-START: AOSP SIM PLUG IN/OUT
    //+ESIMS: 0,26: SIM plug in, but no init
    // MTK-END
    //+ESIMS: 1,9: Virtual SIM on
    //+ESIMS: 1,14: Recovery end
    //+ESIMS: 1,12: SIM plug in
    sp<RfxMclMessage> unsol = NULL;

    char* p_esim_cause = NULL;
    int esim_cause = -1;
    p_esim_cause = strchr(urc, ',');
    esim_cause = atoi(p_esim_cause + 1);
    getMclStatusManager()->setIntValue(RFX_STATUS_KEY_SIM_ESIMS_CAUSE, esim_cause);

    if (strStartsWith(urc,"+ESIMS: 0,0") || strStartsWith(urc,"+ESIMS: 0,10") ||
            strStartsWith(urc,"+ESIMS: 0,11") || strStartsWith(urc,"+ESIMS: 0,13") ||
            strStartsWith(urc,"+ESIMS: 0,15")) {
        if (strStartsWith(urc,"+ESIMS: 0,11") &&
                 RfxRilUtils::getSimCount() >= 2 &&
                 isCommontSlotSupport() == true) {
            if (!isSimInserted()) {
                common_slot_no_changed = true;
            }
        }
        if (strStartsWith(urc, "+ESIMS: 0,11") || strStartsWith(urc, "+ESIMS: 0,13") ||
                strStartsWith(urc, "+ESIMS: 0,10")) {
            logD(mTag, "common_slot_no_changed: %d", common_slot_no_changed);
            String8 iccId(PROPERTY_ICCID_PREIFX);
            resetSimProp();
            iccId.append(String8::format("%d", (m_slot_id + 1)));
            rfx_property_set(iccId.string(), "N/A");
            // TODO: Stk patch plug in ?
            // TODO: Need reset Aid info?
            getMclStatusManager()->setIntValue(RFX_STATUS_KEY_CARD_TYPE, 0);
            getMclStatusManager()->setBoolValue(RFX_STATUS_KEY_MODEM_SIM_TASK_READY, true, true);
            // reset SIM oeprator numeric
            if (strStartsWith(urc, "+ESIMS: 0,11")) {
                String8 prop("gsm.ril.uicc.mccmnc");
                prop.append((m_slot_id == 0)? "" : String8::format(".%d", m_slot_id));
                rfx_property_set(prop.string(), "");
                getMclStatusManager()->setString8Value(RFX_STATUS_KEY_UICC_GSM_NUMERIC,
                        String8(""));
            }
            if (common_slot_no_changed == true) {
                // External SIM [Start]
                if (RfxRilUtils::isVsimEnabled()) {
                    logD(mTag, "Ingore no changed event during vsim enabled on common slot project.");
                } else {
                // External SIM [End]
                    unsol = RfxMclMessage::obtainUrc(RFX_MSG_URC_SIM_COMMON_SLOT_NO_CHANGED,
                            m_slot_id, RfxVoidData());
                    responseToTelCore(unsol);
                }
            } else {
                // External SIM [Start]
                if ((isCommontSlotSupport() == true) && (getVsimPlugInOutEvent(m_slot_id) == VSIM_TRIGGER_PLUG_OUT)) {
                    logD(mTag, "vsim trigger plug out on common slot project.");
                    setVsimPlugInOutEvent(m_slot_id, VSIM_TRIGGER_RESET);
                } else if ((isCommontSlotSupport() == true) && RfxRilUtils::isVsimEnabled()) {
                    logD(mTag, "Ingore plug out event during vsim enabled on common slot project.");
                } else {
                // External SIM [End]
                    unsol = RfxMclMessage::obtainUrc(RFX_MSG_URC_SIM_PLUG_OUT,
                            m_slot_id, RfxVoidData());

                    if (isCommontSlotSupport() == true) {
                        // To send the event plug out only when card removed in common slot project
                        if (strStartsWith(urc, "+ESIMS: 0,11")) {
                            responseToTelCore(unsol);
                        }
                    } else {
                        responseToTelCore(unsol);
                    }
                }

                unsol = RfxMclMessage::obtainUrc(RFX_MSG_URC_RESPONSE_SIM_STATUS_CHANGED,
                        m_slot_id, RfxVoidData());
                responseToTelCore(unsol);
            }
        } else if (strStartsWith(urc, "+ESIMS: 0,15")) {
            unsol = RfxMclMessage::obtainUrc(RFX_MSG_URC_SIM_IMEI_LOCK,
                    m_slot_id, RfxVoidData());
            responseToTelCore(unsol);
        } else {
            unsol = RfxMclMessage::obtainUrc(RFX_MSG_URC_RESPONSE_SIM_STATUS_CHANGED,
                    m_slot_id, RfxVoidData());
            responseToTelCore(unsol);
        }
    // MTK-START: AOSP SIM PLUG IN/OUT
    } else if (strStartsWith(urc, "+ESIMS: 0,26")) {
        logD(mTag, "SIM Plug in but no init.");
        unsol = RfxMclMessage::obtainUrc(RFX_MSG_URC_RESPONSE_SIM_STATUS_CHANGED,
                m_slot_id, RfxVoidData());
        responseToTelCore(unsol);
    // MTK-END
    } else if (strStartsWith(urc, "+ESIMS: 1,9") || strStartsWith(urc, "+ESIMS: 1,14") || strStartsWith(urc, "+ESIMS: 1,12")) {
        if (RfxRilUtils::getSimCount() >= 2) {
            if(isCommontSlotSupport() == true && (strStartsWith(urc, "+ESIMS: 1,12"))) {
               // External SIM [Start]
                if ((isCommontSlotSupport() == true) && (getVsimPlugInOutEvent(m_slot_id) == VSIM_TRIGGER_PLUG_IN)) {
                    logD(mTag, "vsim trigger tray plug in on common slot project.");
                    //setVsimPlugInOutEvent(m_slot_id, VSIM_TRIGGER_RESET);
                } else if ((isCommontSlotSupport() == true) && RfxRilUtils::isVsimEnabled()) {
                    logD(mTag, "Ingore tray plug in event during vsim enabled on common slot project.");
                } else {
                // External SIM [End]

                    // In this feature, when we receive "ESIMS: 1, 12", it does not mean SIM card plug,
                    // but means slot plug in. That is, it might be no SIM card in this slot.
                    // Thus, we need to query SIM state when detect SIM missing and update flag at that time.
                    logD(mTag, "Receive plug in in common slot project so do not set sim inserted status here");
                    String8 iccId(PROPERTY_ICCID_PREIFX);
                    iccId.append(String8::format("%d", (m_slot_id + 1)));
                    rfx_property_set(iccId.string(), "");

                    int simCount = RfxRilUtils::getSimCount();
                    logD(mTag, "mTrayPluginCount: %d (slot %d)",
                            mTrayPluginCount, m_slot_id);
                    // Use static variable mTrayPluginCount to count the sim number and clear all
                    // of slot's task key vaule for the first tray_plug_in coming.It uses to reduce
                    // mode switch times for common slot plug in.
                    if (mTrayPluginCount == 0) {
                        mTrayPluginCount = simCount - 1;
                        for (int i = 0; i < simCount; i++) {
                            getMclStatusManager(i)->setBoolValue(RFX_STATUS_KEY_MODEM_SIM_TASK_READY,
                                    false);
                        }
                    } else {
                        mTrayPluginCount--;
                    }

                    unsol = RfxMclMessage::obtainUrc(RFX_MSG_URC_TRAY_PLUG_IN, m_slot_id, RfxVoidData());
                    responseToTelCore(unsol);
                }
            }
        }
        sendEvent(RFX_MSG_EVENT_SIM_DETECT_SIM, RfxStringData(urc), RIL_CMD_PROXY_1, m_slot_id);
        //RLOGD("detectSim before acquire_wake_lock");
        acquire_wake_lock(PARTIAL_WAKE_LOCK, "sim_hot_plug");
        //RLOGD("detectSim after acquire_wake_lock");
    } else if (strStartsWith(urc, "+ESIMS: 1,2")) {
        logD(mTag, "SIM_REFRESH_DONE");
        unsol = RfxMclMessage::obtainUrc(RFX_MSG_URC_RESPONSE_SIM_STATUS_CHANGED,
                m_slot_id, RfxVoidData());
        responseToTelCore(unsol);
        unsol = RfxMclMessage::obtainUrc(RFX_MSG_URC_SIM_IMSI_REFRESH_DONE,
                m_slot_id, RfxVoidData());
        responseToTelCore(unsol);
    }

    // TODO: is needed to plug in this data function?
    //onSimInsertChangedForData(rid, s);
}

void RmcCommSimUrcHandler::resetSimProp() {
    String8 pin1("gsm.sim.retry.pin1");
    String8 pin2("gsm.sim.retry.pin2");
    String8 puk1("gsm.sim.retry.puk1");
    String8 puk2("gsm.sim.retry.puk2");
    String8 iccId(PROPERTY_ICCID_PREIFX);
    String8 fullIccType(PROPERTY_FULL_UICC_TYPE);
    String8 ecc(PROPERTY_EF_ECC);

    pin1.append((m_slot_id == 0)? "" : String8::format(".%d", (m_slot_id + 1)));
    pin2.append((m_slot_id == 0)? "" : String8::format(".%d", (m_slot_id + 1)));
    puk1.append((m_slot_id == 0)? "" : String8::format(".%d", (m_slot_id + 1)));
    puk2.append((m_slot_id == 0)? "" : String8::format(".%d", (m_slot_id + 1)));

    rfx_property_set(pin1.string(), "");
    rfx_property_set(pin2.string(), "");
    rfx_property_set(puk1.string(), "");
    rfx_property_set(puk2.string(), "");

    for (int i = 1; i <= RfxRilUtils::getSimCount(); i++) {
        iccId.append(String8::format("%d", (m_slot_id + i)));
        rfx_property_set(iccId.string(), "");
        fullIccType.append((m_slot_id == 0)? "" : String8::format(".%d", (m_slot_id + 1)));
        rfx_property_set(fullIccType.string(), "");
        ecc.append((m_slot_id == 0)? "" : String8::format(".%d", m_slot_id));
        rfx_property_set(ecc.string(), "");
    }
}

void RmcCommSimUrcHandler::handleSimIndication(const sp<RfxMclMessage>& msg, RfxAtLine *urc) {
    int err = 0, indEvent = -1;
    RfxAtLine *atLine = urc;
    int applist = 0;
    RFX_UNUSED(msg);

    atLine->atTokStart(&err);
    if (err < 0) {
        goto error;
    }

    indEvent = atLine->atTokNextint(&err);
    if (err < 0) {
        goto error;
    }

    switch (indEvent) {
        case 1:
            // Currently capability switch without RADIO_UNAVAILABLE, but modem SIM task
            // still has to do SIM initialization again.
            // After modem SIM task initialize SIM done, AP will get the URC "+ESIMINIT".
            // We have to notify the capability switch module the event and the proxy channels
            // will unlock
            if (RmcCapabilitySwitchUtil::isDssNoResetSupport()) {
                RmcCapabilitySwitchRequestHandler::notifySIMInitDone(m_slot_id);
            }
            break;
        case 2:
            // +ESIMIND: 2, <uicc_app_list>
            // uicc_app_list = is_csim_exist | is_usim_exist | is_isim_exist (currently isim always 0)
            // is_usim_exist:2 is_csim_exist:4 (is_csim_exist | is_usim_exist): 6
            // For icc card uicc_app_list:0
            applist = atLine->atTokNextint(&err);
            if (err < 0) {
                goto error;
            }

            getMclStatusManager()->setIntValue(RFX_STATUS_KEY_ESIMIND_APPLIST, applist);
            logD(mTag, "Notify uicc app list, applist : %d", applist);
            break;
        default:
            logD(mTag, "Not support the SIM indication event %d", indEvent);
            break;
    }

    return;
error:
    logE(mTag, "handleSimIndication, Invalid parameters");
}

// External SIM [Start]
void RmcCommSimUrcHandler::setVsimPlugInOutEvent(int slotId, int flag) {
    mVsimPlugInOut[slotId] = flag;
}

int RmcCommSimUrcHandler::getVsimPlugInOutEvent(int slotId) {
    return mVsimPlugInOut[slotId];
}

void RmcCommSimUrcHandler::setMdWaitingResponse(int slotId, int waiting)
{
    mVsimMdWaiting[slotId] = waiting;
}

int RmcCommSimUrcHandler::getMdWaitingResponse(int slotId) {
    return mVsimMdWaiting[slotId];
}
// External SIM [End]

