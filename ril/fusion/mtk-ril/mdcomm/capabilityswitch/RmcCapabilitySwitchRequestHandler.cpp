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

#include "RmcCapabilitySwitchRequestHandler.h"
#include "RmcCapabilitySwitchUtil.h"
#include <telephony/mtk_ril.h>
#include "RfxRilUtils.h"
#include "RfxRadioCapabilityData.h"
#include "RfxVoidData.h"
#include "ratconfig.h"
#include "telephony/librilutilsmtk.h"

#define RFX_LOG_TAG "RmcCapa"

// register handler to channel
RFX_IMPLEMENT_HANDLER_CLASS(RmcCapabilitySwitchRequestHandler, RIL_CMD_PROXY_9);

RFX_REGISTER_DATA_TO_EVENT_ID(RfxIntsData, RFX_MSG_EVENT_RADIO_CAPABILITY_UPDATED);

Mutex RmcCapabilitySwitchRequestHandler::s_sim_init_state_mutex;
int RmcCapabilitySwitchRequestHandler::s_sim_init_state;

Mutex RmcCapabilitySwitchRequestHandler::s_first_instance_mutex;
bool RmcCapabilitySwitchRequestHandler::s_first_instance = true;

RmcCapabilitySwitchRequestHandler::RmcCapabilitySwitchRequestHandler(int slot_id, int channel_id) :
    RfxBaseHandler(slot_id, channel_id) {
    logD(RFX_LOG_TAG, "constructor entered");
    setSIMInitState(0);
    int old_major_sim = RmcCapabilitySwitchUtil::getMajorSim();
    int main_sim = queryMainProtocol();
    const int request1[] = {
        RFX_MSG_REQUEST_SET_RADIO_CAPABILITY,
        RFX_MSG_REQUEST_GET_RADIO_CAPABILITY,
        RFX_MSG_REQUEST_CAPABILITY_SWITCH_SET_MAJOR_SIM
    };
    registerToHandleRequest(request1, sizeof(request1) / sizeof(int));
    if (old_major_sim != main_sim) {
        logE(RFX_LOG_TAG, "major sim is unsync with main protocol(=%d)", main_sim);
        rfx_property_set("persist.radio.simswitch", String8::format("%d", main_sim).string());
        resetRadio();
        return;
    }
    s_first_instance_mutex.lock();
    if (s_first_instance) {
        s_first_instance = false;
        rfx_property_set("persist.radio.simswitch", String8::format("%d", main_sim).string());
        getNonSlotMclStatusManager()->setIntValue(RFX_STATUS_KEY_MAIN_CAPABILITY_SLOT,
                main_sim - 1, false, false);
        queryNoResetSupport();
        queryTplusWSupport();
        queryKeep3GMode();
        if(false == RfxRilUtils::isTplusWSupport()) {
            queryActiveMode();
        }
    }
    s_first_instance_mutex.unlock();
    queryBearer();
    sendRadioCapabilityDoneIfNeeded();
    sendEGRAT();
    sendEvent(RFX_MSG_EVENT_CAPABILITY_INIT_DONE, RfxVoidData(), m_channel_id, m_slot_id);
}

RmcCapabilitySwitchRequestHandler::~RmcCapabilitySwitchRequestHandler() {
}

void RmcCapabilitySwitchRequestHandler::onHandleRequest(const sp<RfxMclMessage> &msg) {
    //logD(RFX_LOG_TAG, "onHandleRequest: %s", idToString(msg->getId()));
    int request = msg->getId();
    switch (request) {
        case RFX_MSG_REQUEST_SET_RADIO_CAPABILITY:
            logE(RFX_LOG_TAG, "Should not receive RFX_MSG_REQUEST_SET_RADIO_CAPABILITY");
            break;
        case RFX_MSG_REQUEST_CAPABILITY_SWITCH_SET_MAJOR_SIM:
            requestSetRadioCapability(msg);
            break;
        case RFX_MSG_REQUEST_GET_RADIO_CAPABILITY:
            requestGetRadioCapability(msg);
            break;
        default:
            logE(RFX_LOG_TAG, "Should not be here");
            break;
    }
}

void RmcCapabilitySwitchRequestHandler::requestSetRadioCapability(const sp<RfxMclMessage> &msg) {
    sp<RfxMclMessage> resMsg;
    int *new_major_slot = (int *)msg->getData()->getData();
    int old_major_slot = getNonSlotMclStatusManager()->getIntValue(
            RFX_STATUS_KEY_MAIN_CAPABILITY_SLOT, 0);
    sp<RfxAtResponse> p_response = NULL;
    int real_channel_id = m_channel_id + RIL_PROXY_OFFSET * m_slot_id;
    bool no_reset_support = RmcCapabilitySwitchUtil::isDssNoResetSupport();
    int response_data[1];
    response_data[0] = new_major_slot[0];

    logI(RFX_LOG_TAG, "RadioCapability old_major_slot=%d, new_major_slot=%d", old_major_slot,
            new_major_slot[0]);
    if (no_reset_support || (!isVsimEnabled() && !isPersistVsim())) {
        for (int i = 0; i < RfxRilUtils::getSimCount(); i++) {
            getMclStatusManager(i)->setIntValue(RFX_STATUS_KEY_RADIO_POWER_MSIM_MODE, 0);
            if (no_reset_support) {
                getMclStatusManager(i)->setIntValue(RFX_STATUS_KEY_RADIO_STATE,
                    RADIO_STATE_OFF, false, false);
            }
        }
        p_response = atSendCommand(String8::format("AT+EFUN=0,%d", RFOFF_CAUSE_SIM_SWITCH));
        if (p_response != NULL && p_response->getSuccess() == 0) {
            resMsg = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_GENERIC_FAILURE,
                            RfxIntsData(response_data, 1), msg, true);
            responseToTelCore(resMsg);
            return;
        }
        p_response = atSendCommand("AT+EBOOT=1");
        if (RfxRilUtils::getRilRunMode() != RIL_RUN_MODE_MOCK) {
            for(int i = 0; i < RfxChannelManager::getSupportChannels(); i++) {
                if (i == real_channel_id || i % RIL_CHANNEL_OFFSET == RIL_CMD_IMS ||
                        i % RIL_CHANNEL_OFFSET == RIL_URC ||
                        (no_reset_support && i % RIL_CHANNEL_OFFSET == RIL_CMD_11)) {
                    continue;
                }
                if (i == 1) {
                    logI(RFX_LOG_TAG, "lock mutex %d!", i);
                } else {
                    logD(RFX_LOG_TAG, "lock mutex %d!", i);
                }
                lockRestartMutex(i);
            }
            if (no_reset_support == false) {
                /* lock URC channels later than sender channels */
                for(int i = RIL_URC; i < RfxChannelManager::getSupportChannels(); i+=RIL_CHANNEL_OFFSET) {
                    lockRestartMutex(i);
                }
                logI(RFX_LOG_TAG, "lock mutex done!");
                switchChannel(RIL_URC, old_major_slot, new_major_slot[0]);
            } else {
                logI(RFX_LOG_TAG, "lock mutex done!");
                getNonSlotMclStatusManager()->setBoolValue(RFX_STATUS_KEY_CAPABILITY_SWITCH_URC_CHANNEL,
                    true, false, false);
                setSIMInitState(0);
            }
        }
        p_response = atSendCommand(String8::format("AT+ESIMMAP=%d", (1 << new_major_slot[0])));
        while (p_response != NULL && p_response->getSuccess() == 0) {
            logE(RFX_LOG_TAG, "ESIMMAP returns ERROR:%d, retry", p_response->getError());
            usleep(200*1000);
            p_response = atSendCommand(String8::format("AT+ESIMMAP=%d", (1 << new_major_slot[0])));
        }
        if (RfxRilUtils::getRilRunMode() != RIL_RUN_MODE_MOCK) {
            queryActiveMode();
            setSimSwitchProp(old_major_slot, new_major_slot[0]);
            if (no_reset_support) {
                if (false == waitSIMInitDone()) {
                    return;
                }
            }
            for(int i = 0; i < RIL_CHANNEL_OFFSET; i++) {
                if (i == RIL_CMD_IMS || i == RIL_URC ||
                        (no_reset_support && i == RIL_CMD_11)) {
                    continue;
                }
                switchChannel(i, old_major_slot, new_major_slot[0]);
            }
            logI(RFX_LOG_TAG, "switchChannel done! old:%d,new:%d", old_major_slot,
                    new_major_slot[0]);
            for(int i = 0; i < RfxChannelManager::getSupportChannels(); i++) {
                if (i % RIL_CHANNEL_OFFSET == RIL_CMD_IMS ||
                    i % RIL_CHANNEL_OFFSET == RIL_URC ||
                    i == real_channel_id ||
                    (no_reset_support && i % RIL_CHANNEL_OFFSET == RIL_CMD_11)) {
                    continue;
                }
                unlockRestartMutex(i);
            }
            if (no_reset_support == false) {
                /* unlock URC channels later than sender channels */
                for(int i = RIL_URC; i < RfxChannelManager::getSupportChannels(); i+=RIL_CHANNEL_OFFSET) {
                    unlockRestartMutex(i);
                }
            }
            //logD(RFX_LOG_TAG, "unlock mutex done!");
        } else {
            setSimSwitchProp(old_major_slot, new_major_slot[0]);
        }
        resMsg = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_SUCCESS,
                        RfxIntsData(response_data, 1), msg, true);
        responseToTelCore(resMsg);

        for(int i = 0; i < RfxRilUtils::getSimCount(); i++) {
            int event_data[1];
            event_data[0] = getMclStatusManager(i)->getIntValue(RFX_STATUS_KEY_SLOT_CAPABILITY, 0);
            sendEvent(RFX_MSG_EVENT_RADIO_CAPABILITY_UPDATED, RfxIntsData(event_data, 1),
                      RIL_CMD_PROXY_9, i);
        }
    } else {
        atSendCommand(String8::format("AT+ES3G = %d,%d", (1 << new_major_slot[0]), 3));
        setSimSwitchProp(old_major_slot, new_major_slot[0]);
        resetRadio();
    }
}
void RmcCapabilitySwitchRequestHandler::resetRadio() {
    logD(RFX_LOG_TAG, "start to reset radio");
    rfx_property_set("gsm.ril.eboot", "1");
    atSendCommand("AT+EBOOT=1");
    atSendCommand("AT+EFUN=0");
    atSendCommand("AT+ECUSD=2,2");
    atSendCommand("AT+EMDT=0");
    atSendCommand("AT+EPOF");
    RfxRilUtils::triggerCCCIIoctl(CCCI_IOC_ENTER_DEEP_FLIGHT_ENHANCED);
    //power on modem
    RfxRilUtils::triggerCCCIIoctl(CCCI_IOC_LEAVE_DEEP_FLIGHT_ENHANCED);
}

bool RmcCapabilitySwitchRequestHandler::isVsimEnabledByRid(int rid) {
    bool enabled = false;
    char vsim_enabled_prop[RFX_PROPERTY_VALUE_MAX] = {0};
    char vsim_inserted_prop[RFX_PROPERTY_VALUE_MAX] = {0};

    getMSimProperty(rid, (char*)"gsm.external.sim.enabled", vsim_enabled_prop);
    getMSimProperty(rid, (char*)"gsm.external.sim.inserted", vsim_inserted_prop);

    if (atoi(vsim_enabled_prop) > 0 && atoi(vsim_inserted_prop) > 0) {
        enabled = true;
    }

    logD(RFX_LOG_TAG, "isVsimEnabled rid:%d is %d.", rid, enabled);

    return enabled;
}
bool RmcCapabilitySwitchRequestHandler::isVsimEnabled() {
    bool enabled = false;

    for (int index = 0; index < RfxRilUtils::getSimCount(); index++) {
        if (1 == isVsimEnabledByRid(index)) {
            enabled = true;
            break;
        }
    }
    logD(RFX_LOG_TAG, "isVsimEnabled=%d", enabled);
    return enabled;
}

bool RmcCapabilitySwitchRequestHandler::isPersistVsim() {
    bool persist = false;
    char persist_vsim_inserted_prop[RFX_PROPERTY_VALUE_MAX] = {0};

    for (int index = 0; index < RfxRilUtils::getSimCount(); index++) {
        getMSimProperty(index, (char*)"persist.radio.external.sim", persist_vsim_inserted_prop);
        if (atoi(persist_vsim_inserted_prop) > 0) {
            persist = true;
            break;
        }
    }
    logD(RFX_LOG_TAG, "isPersistVsim=%d", persist);
    return persist;
}

void RmcCapabilitySwitchRequestHandler::queryTplusWSupport() {
    sp<RfxAtResponse> p_response = NULL;
    int TplusWsupport = 0;
    RfxAtLine *line;
    int err;

    p_response = atSendCommandSingleline("AT+ESBP=7,\"SBP_T_PLUS_W\"", "+ESBP:");
    do {
        if (p_response == NULL || p_response->getSuccess() == 0) {
            logE(RFX_LOG_TAG, "queryTplusWSupport AT+ESBP return ERROR");
            break;
        }
        line = p_response->getIntermediates();
        line->atTokStart(&err);
        if (err < 0) {
            logE(RFX_LOG_TAG, "queryTplusWSupport AT+ESBP atTokStart error:%d", err);
            break;
        }
        TplusWsupport = line->atTokNextint(&err);
        if (err < 0) {
            logE(RFX_LOG_TAG, "queryTplusWSupport AT+ESBP atTokNextint error:%d", err);
            break;
        }
    } while(0);
    logD(RFX_LOG_TAG, "queryTplusWSupport, TplusWSupport=%d", TplusWsupport);
    rfx_property_set("ril.simswitch.tpluswsupport", String8::format("%d", TplusWsupport).string());
}

void RmcCapabilitySwitchRequestHandler::queryKeep3GMode() {
    sp<RfxAtResponse> p_response = NULL;
    int keep_3g_mode = 0;
    RfxAtLine *line;
    int err;

    p_response = atSendCommandSingleline("AT+ESBP=7,\"SBP_GEMINI_LG_WG_MODE\"", "+ESBP:");
    do {
        if (p_response == NULL || p_response->getSuccess() == 0) {
            logE(RFX_LOG_TAG, "queryKeep3GMode AT+ESBP return ERROR");
            break;
        }
        line = p_response->getIntermediates();
        line->atTokStart(&err);
        if (err < 0) {
            logE(RFX_LOG_TAG, "queryKeep3GMode AT+ESBP atTokStart error:%d", err);
            break;
        }
        keep_3g_mode = line->atTokNextint(&err);
        if (err < 0) {
            logE(RFX_LOG_TAG, "queryKeep3GMode AT+ESBP atTokNextint error:%d", err);
            break;
        }
    } while(0);
    logD(RFX_LOG_TAG, "queryKeep3GMode, keep_3g_mode=%d", keep_3g_mode);
    //keep_3g_mode, 0:keep TD-SCDMA, 1:keep WCDMA
    rfx_property_set("ril.nw.worldmode.keep_3g_mode", String8::format("%d", keep_3g_mode).string());
}

void RmcCapabilitySwitchRequestHandler::queryNoResetSupport() {
    char feature[] = "DSS_NO_RESET";
    int support = getFeatureVersion(feature);
    //no reset support, 0:disable, 1:enable
    logD(RFX_LOG_TAG, "queryNoResetSupport, %s=%d",feature, support);
    if (support == 1) {
        rfx_property_set("ril.simswitch.no_reset_support", String8::format("%d", support).string());
    } else {
        rfx_property_set("ril.simswitch.no_reset_support", String8::format("%d", 0).string());
    }
}

void RmcCapabilitySwitchRequestHandler::queryActiveMode() {
    sp<RfxAtResponse> p_response = NULL;
    int csraaResponse[3] = {0};
    RfxAtLine *line;
    int err;

    p_response = atSendCommandMultiline("AT+CSRA?", "+CSRAA:");

    do {
        if (p_response == NULL || p_response->getSuccess() == 0) {
            logE(RFX_LOG_TAG, "AT+CSRAA return ERROR");
            break;
        }
        line = p_response->getIntermediates();
        line->atTokStart(&err);
        if (err < 0) {
            break;
        }
        csraaResponse[0] = line->atTokNextint(&err);
        if (err < 0) {
            break;
        }
        csraaResponse[1] = line->atTokNextint(&err);
        if (err < 0) {
            break;
        }
        //logD(RFX_LOG_TAG, "+CSRAA:<UTRANFDD> = %d", csraaResponse[1]);
        csraaResponse[2] = line->atTokNextint(&err);
        if (err < 0) {
            break;
        }
        //logD(RFX_LOG_TAG, "+CSRAA:<UTRAN-TDD-LCR> = %d", csraaResponse[2]);
        if ((csraaResponse[1] == 1) && (csraaResponse[2] == 0)) {
            //FDD mode
            rfx_property_set("ril.nw.worldmode.activemode", String8::format("%d", 1).string());
        }
        else if ((csraaResponse[1] == 0) && (csraaResponse[2] == 1)) {
            //TDD mode
            rfx_property_set("ril.nw.worldmode.activemode", String8::format("%d", 2).string());
        }
    } while (0);
}

int RmcCapabilitySwitchRequestHandler::getActiveMode() {
    char world_mode_prop[RFX_PROPERTY_VALUE_MAX] = {0};
    int world_mode = 0;

    rfx_property_get("ril.nw.worldmode.activemode", world_mode_prop, "1");
    world_mode = atoi(world_mode_prop);
    return world_mode;
}

int RmcCapabilitySwitchRequestHandler::queryMainProtocol() {
    sp<RfxAtResponse> p_response = NULL;
    int err;
    RfxAtLine *line;
    int ret = 0;
    int main_sim = 1;

    logD(RFX_LOG_TAG, "queryMainProtocol");
    p_response = atSendCommandSingleline("AT+ES3G?", "+ES3G:");
    do {
        if (p_response == NULL || p_response->getSuccess() == 0) {
            logE(RFX_LOG_TAG, "AT+ES3G return ERROR");
            break;
        }

        line = p_response->getIntermediates();

        line->atTokStart(&err);
        if (err < 0) {
            break;
        }
        ret = line->atTokNextint(&err);
        if (err < 0) {
            break;
        }
        /* Gemini+ , +ES3G response 1: SIM1 , 2: SIM2 , 4:SIM3 ,8: SIM4. For SIM3 and SIM4 we convert to 3 and 4 */
        if (ret == 2) {
            main_sim = 2;
        } else if (ret == 4) {
            main_sim = 3;
        } else if (ret == 8) {
            main_sim = 4;
        }
    } while (0);
    logD(RFX_LOG_TAG, "queryMainProtocol, ret=%d, main_sim=%d", ret, main_sim);
    return main_sim;
}

void RmcCapabilitySwitchRequestHandler::queryBearer() {
    sp<RfxAtResponse> p_response = NULL;
    int err;
    RfxAtLine *line;
    int ret = 0;
    int modem_rat = RAF_GPRS;
    int ap_max_rat = RAF_GPRS;
    int radio_capability;
    int main_slot = RmcCapabilitySwitchUtil::getMajorSim() - 1;
    char tempstr[RFX_PROPERTY_VALUE_MAX] = { 0 };

    logD(RFX_LOG_TAG, "queryBearer");

    if (main_slot == m_slot_id) {
        rfx_property_get(rat_properties[0], tempstr, "G");
    } else if (main_slot != 0 && m_slot_id == 0) {
        rfx_property_get(rat_properties[main_slot], tempstr, "G");
    } else {
        rfx_property_get(rat_properties[m_slot_id], tempstr, "G");
    }
    if (strchr(tempstr, 'G') != NULL && RatConfig_isGsmSupported()) {
        ap_max_rat |= RAF_GSM;
    }
    if (strchr(tempstr, 'W') != NULL) {
        if (RatConfig_isWcdmaSupported()) {
            ap_max_rat |= RAF_UMTS;
        }
        if (RatConfig_isTdscdmaSupported()) {
            ap_max_rat |= RAF_TD_SCDMA;
        }
    }
    if (strchr(tempstr, 'L') != NULL && RfxRilUtils::isLteSupport() &&
        (RatConfig_isLteFddSupported() || RatConfig_isLteTddSupported())) {
        ap_max_rat |= RAF_LTE;
    }
    // query modem capability
    p_response = atSendCommandSingleline("AT+EPSB?", "+EPSB:");

    do {
        if (p_response == NULL || p_response->getSuccess() == 0) {
            modem_rat = (RAF_GSM | RAF_LTE | RAF_TD_SCDMA | RAF_UMTS); // If modem error try max rat
            logE(RFX_LOG_TAG, "AT+EPSB return ERROR");
            break;
        }

        line = p_response->getIntermediates();

        line->atTokStart(&err);
        if (err < 0) {
            break;
        }
        ret = line->atTokNextint(&err);
        if (err < 0) {
            break;
        }

        if (getNonSlotMclStatusManager()->getIntValue(RFX_STATUS_KEY_MAIN_CAPABILITY_SLOT, 0) == m_slot_id) {
            #ifdef MTK_RIL_MD2
            rfx_property_set("gsm.baseband.capability.md2", String8::format("%d", ret).string());
            #else
            rfx_property_set("gsm.baseband.capability", String8::format("%d", ret).string());
            #endif
            // +EPSB: <bearer> LTE CA: 0x0200
            if ((ret & 0x0200) > 0) {
                rfx_property_set("gsm.lte.ca.support", String8::format("%d", 1).string());
            } else {
                rfx_property_set("gsm.lte.ca.support", String8::format("%d", 0).string());
            }
        }

        modem_rat = RAF_GSM;
        if (((ret & 0x0100) > 0) || ((ret & 0x0080) > 0)) {
            modem_rat |= RAF_LTE;
        }
        if ((ret & 0x0008) > 0) {
            modem_rat |= RAF_TD_SCDMA;
        }
        if ((ret & 0x0004) > 0) {
            modem_rat |= RAF_UMTS;
        }
    } while (0);
    logD(RFX_LOG_TAG, "queryBearer, ret=%d", ret);
    int major_slot = RfxRilUtils::getMajorSim() - 1;
    if (major_slot == m_slot_id) {
        modem_rat |= RAF_GPRS;   //use RAF_GPRS as the major SIM flag
    }

    radio_capability = (modem_rat & ap_max_rat);
    getMclStatusManager(m_slot_id)->setIntValue(RFX_STATUS_KEY_SLOT_FIXED_CAPABILITY, radio_capability, false, false);

    if (major_slot != m_slot_id && getActiveMode() == 2 && RfxRilUtils::isTplusWSupport() == false &&
        RfxRilUtils::getKeep3GMode() == 0) {
        radio_capability &= ~RAF_UMTS; // Remove 3G raf for non major SIMs in TDD mode
    }
    if(RmcCapabilitySwitchUtil::isDisableC2kCapability() == false &&
       RfxRilUtils::isC2kSupport() && RatConfig_isC2kSupported()) {
        memset(tempstr, 0, sizeof(tempstr));
        rfx_property_get("persist.radio.c_capability_slot", tempstr, "1");
        int cslot = atoi(tempstr) - 1;
        logI(RFX_LOG_TAG, "queryBearer, cslot=%d", cslot);
        if (cslot == m_slot_id || (cslot < 0 && m_slot_id == 0)) {
            radio_capability |= (RAF_CDMA_GROUP | RAF_EVDO_GROUP);
        }
    }
    logD(RFX_LOG_TAG, "radio_capability=%d", radio_capability);
    getMclStatusManager(m_slot_id)->setIntValue(RFX_STATUS_KEY_SLOT_CAPABILITY, radio_capability, false, false);
}

void RmcCapabilitySwitchRequestHandler::sendRadioCapabilityDoneIfNeeded() {
    int radio_capability = getMclStatusManager(m_slot_id)->getIntValue(RFX_STATUS_KEY_SLOT_CAPABILITY, 0);
    int event_data[1];

    event_data[0] = radio_capability;
    sendEvent(RFX_MSG_EVENT_RADIO_CAPABILITY_UPDATED, RfxIntsData(event_data, 1),
              m_channel_id, m_slot_id);
}

void RmcCapabilitySwitchRequestHandler::requestGetRadioCapability(const sp<RfxMclMessage> &msg) {
    int radio_capability;
    int session_id = -1;
    RIL_RadioCapability rc;
    memset(&rc, 0, sizeof(RIL_RadioCapability));
    rc.version = RIL_RADIO_CAPABILITY_VERSION;
    rc.session = session_id;
    rc.phase = RC_PHASE_UNSOL_RSP;
    rc.status = RC_STATUS_SUCCESS;
    radio_capability = getMclStatusManager(m_slot_id)->getIntValue(RFX_STATUS_KEY_SLOT_CAPABILITY, 0);
    logD(RFX_LOG_TAG, "requestGetRadioCapability, capability[%d] = %d, sizeof(RIL_RadioCapability)=%d", m_slot_id, radio_capability, sizeof(RIL_RadioCapability));
    rc.rat = radio_capability;
    sp<RfxMclMessage> response = RfxMclMessage::obtainResponse(RIL_E_SUCCESS,
                                 RfxRadioCapabilityData(&rc, sizeof(RIL_RadioCapability)), msg);
    responseToTelCore(response);
}
void RmcCapabilitySwitchRequestHandler::setSimSwitchProp(int old_major_slot, int new_major_slot) {
    switchCapability(old_major_slot, new_major_slot);
    rfx_property_set("persist.radio.simswitch", String8::format("%d", new_major_slot + 1).string());
    getNonSlotMclStatusManager()->setIntValue(RFX_STATUS_KEY_MAIN_CAPABILITY_SLOT, new_major_slot, false, false);
}

void RmcCapabilitySwitchRequestHandler::lockRestartMutex(int channel_id) {
    RfxChannel *p_channel;
    RfxChannelContext *p_channel_context;

    p_channel = RfxChannelManager::getChannel(channel_id);
    p_channel_context = p_channel->getContext();
    p_channel_context->setNeedWaitRestartCondition(true);
    p_channel_context->m_restartMutex.lock();
    p_channel_context->m_readerMutex.lock();
}

void RmcCapabilitySwitchRequestHandler::unlockRestartMutex(int channel_id) {
    RfxChannel *p_channel;
    RfxChannelContext *p_channel_context;

    p_channel = RfxChannelManager::getChannel(channel_id);
    p_channel_context = p_channel->getContext();
    p_channel_context->setNeedWaitRestartCondition(false);
    p_channel_context->m_readerMutex.unlock();
    p_channel_context->m_restartCondition.signal();
    p_channel_context->m_restartMutex.unlock();
}

void RmcCapabilitySwitchRequestHandler::switchChannelByRealId(int channel_id1, int channel_id2) {
    RfxChannel *channel1, *channel2;
    RfxReader *reader1, *reader2;
    RfxSender *sender1, *sender2;
    RfxChannelContext *tmp_context;
    int tmp_fd;
    if (channel_id1 == channel_id2) {
        return;
    }
    //RFX_LOG_D(RFX_LOG_TAG, "switchChannel:%d <-> %d", channel_id1, channel_id2);
    channel1 = RfxChannelManager::getChannel(channel_id1);
    channel2 = RfxChannelManager::getChannel(channel_id2);
    reader1 = channel1->getReader();
    reader2 = channel2->getReader();
    sender1 = channel1->getSender();
    sender2 = channel2->getSender();

    tmp_context = reader1->getChannelContext();
    reader1->setChannelContext(reader2->getChannelContext());
    reader2->setChannelContext(tmp_context);

    reader1->setChannelId(channel_id2);
    reader2->setChannelId(channel_id1);

    channel1->setReader(reader2);
    channel2->setReader(reader1);
    tmp_fd = sender1->getFd();
    sender1->setFd(sender2->getFd());
    sender2->setFd(tmp_fd);
}

void RmcCapabilitySwitchRequestHandler::switchChannel(int channel, int old_major_slot, int new_major_slot) {
    if (old_major_slot == new_major_slot) {
        return;
    }
    //Switch main protocol channel from old main slot to slot 0
    switchChannelByRealId(channel, channel + old_major_slot * RIL_CHANNEL_OFFSET);
    //Switch main protocol channel from 0 to new main slot id
    switchChannelByRealId(channel, channel + new_major_slot * RIL_CHANNEL_OFFSET);
}

void RmcCapabilitySwitchRequestHandler::switchCapability(int old_major_slot, int new_major_slot) {
    int tmp_capability;
    int cslot = -1;
    char tempstr[RFX_PROPERTY_VALUE_MAX] = { 0 };

    if (old_major_slot != 0) {
        tmp_capability = getMclStatusManager(old_major_slot)->getIntValue(RFX_STATUS_KEY_SLOT_FIXED_CAPABILITY, 0);
        getMclStatusManager(old_major_slot)->setIntValue(RFX_STATUS_KEY_SLOT_FIXED_CAPABILITY,
                getMclStatusManager(0)->getIntValue(RFX_STATUS_KEY_SLOT_FIXED_CAPABILITY, 0), false, false);
        getMclStatusManager(0)->setIntValue(RFX_STATUS_KEY_SLOT_FIXED_CAPABILITY, tmp_capability, false, false);
    }
    if (new_major_slot != 0) {
        tmp_capability = getMclStatusManager(new_major_slot)->getIntValue(RFX_STATUS_KEY_SLOT_FIXED_CAPABILITY, 0);
        getMclStatusManager(new_major_slot)->setIntValue(RFX_STATUS_KEY_SLOT_FIXED_CAPABILITY,
                getMclStatusManager(0)->getIntValue(RFX_STATUS_KEY_SLOT_FIXED_CAPABILITY, 0), false, false);
        getMclStatusManager(0)->setIntValue(RFX_STATUS_KEY_SLOT_FIXED_CAPABILITY, tmp_capability, false, false);
    }
    if (RmcCapabilitySwitchUtil::isDisableC2kCapability() == false &&
        RfxRilUtils::isC2kSupport() && RatConfig_isC2kSupported()) {
        memset(tempstr, 0, sizeof(tempstr));
        rfx_property_get("persist.radio.c_capability_slot", tempstr, "1");
        cslot = atoi(tempstr) - 1;
        logI(RFX_LOG_TAG, "switchCapability, cslot=%d", cslot);
    }
    if (old_major_slot != 0 && new_major_slot != 0) {
        tmp_capability = getMclStatusManager(0)->getIntValue(RFX_STATUS_KEY_SLOT_FIXED_CAPABILITY, 0);
        if (getActiveMode() == 2 && RfxRilUtils::isTplusWSupport() == false && RfxRilUtils::getKeep3GMode() == 0) {
            tmp_capability &= ~RAF_UMTS; // Remove 3G raf for non major SIMs in TDD mode
        }
        if (cslot == 0) {
            tmp_capability |= (RAF_CDMA_GROUP | RAF_EVDO_GROUP);
        }
        getMclStatusManager(0)->setIntValue(RFX_STATUS_KEY_SLOT_CAPABILITY, tmp_capability, false, false);
    }

    tmp_capability = getMclStatusManager(old_major_slot)->getIntValue(RFX_STATUS_KEY_SLOT_FIXED_CAPABILITY, 0);
    if (getActiveMode() == 2 && RfxRilUtils::isTplusWSupport() == false && RfxRilUtils::getKeep3GMode() == 0) {
        tmp_capability &= ~RAF_UMTS; // Remove 3G raf for non major SIMs in TDD mode
    }
    if (cslot == old_major_slot) {
        tmp_capability |= (RAF_CDMA_GROUP | RAF_EVDO_GROUP);
    }
    getMclStatusManager(old_major_slot)->setIntValue(RFX_STATUS_KEY_SLOT_CAPABILITY, tmp_capability, false, false);

    tmp_capability = getMclStatusManager(new_major_slot)->getIntValue(RFX_STATUS_KEY_SLOT_FIXED_CAPABILITY, 0);
    if (cslot == new_major_slot) {
        tmp_capability |= (RAF_CDMA_GROUP | RAF_EVDO_GROUP);
    }
    getMclStatusManager(new_major_slot)->setIntValue(RFX_STATUS_KEY_SLOT_CAPABILITY, tmp_capability, false, false);
}

void RmcCapabilitySwitchRequestHandler::sendEGRAT() {
    int raf = getMclStatusManager(m_slot_id)->getIntValue(RFX_STATUS_KEY_SLOT_FIXED_CAPABILITY, 0);
    int rat;
    if(RmcCapabilitySwitchUtil::isDisableC2kCapability() == false &&
       RfxRilUtils::isC2kSupport() && RatConfig_isC2kSupported()) {
        raf |= RAF_CDMA_GROUP;
    }
    raf = RmcCapabilitySwitchUtil::getAdjustedRaf(raf);
    switch (raf) {
        case RAF_GSM_GROUP:
            rat = 0;
            break;
        case RAF_WCDMA_GROUP:
            rat = 1;
            break;
        case RAF_GSM_GROUP | RAF_WCDMA_GROUP:
            rat = 2;
            break;
        case RAF_LTE:
            rat = 3;
            break;
        case RAF_GSM_GROUP | RAF_LTE:
            rat = 4;
            break;
        case RAF_WCDMA_GROUP | RAF_LTE:
            rat = 5;
            break;
        case RAF_GSM_GROUP | RAF_WCDMA_GROUP | RAF_LTE:
            rat = 6;
            break;
        case RAF_CDMA_GROUP:
            rat = 7;
            break;
        case RAF_GSM_GROUP | RAF_CDMA_GROUP:
            rat = 8;
            break;
        case RAF_GSM_GROUP | RAF_WCDMA_GROUP | RAF_CDMA_GROUP:
            rat = 10;
            break;
        case RAF_LTE | RAF_CDMA_GROUP:
            rat = 11;
            break;
        case RAF_GSM_GROUP | RAF_LTE | RAF_CDMA_GROUP:
            rat = 12;
            break;
        case RAF_GSM_GROUP | RAF_WCDMA_GROUP | RAF_LTE | RAF_CDMA_GROUP:
            rat = 14;
            break;
        default:
            rat = 14;
            break;
    }
    logI(RFX_LOG_TAG, "sendEGRAT: raf=%d, rat=%d", raf, rat);
    atSendCommand(String8::format("AT+EGRAT=%d", rat));
}

void RmcCapabilitySwitchRequestHandler::notifySIMInitDone(int slot_id) {
    RFX_LOG_I(RFX_LOG_TAG, "notifySIMInitDone, slot:%d, sim_init_state:0x%x", slot_id,
        s_sim_init_state);
    s_sim_init_state_mutex.lock();
    s_sim_init_state |= (1 << slot_id);
    s_sim_init_state_mutex.unlock();
}

int RmcCapabilitySwitchRequestHandler::getSIMInitState() {
    int ret;
    s_sim_init_state_mutex.lock();
    ret = s_sim_init_state;
    s_sim_init_state_mutex.unlock();
    return ret;
}

void RmcCapabilitySwitchRequestHandler::setSIMInitState(int val) {
    s_sim_init_state_mutex.lock();
    s_sim_init_state = val;
    s_sim_init_state_mutex.unlock();
}

bool RmcCapabilitySwitchRequestHandler::waitSIMInitDone() {
    for (int i = 0; i < RfxRilUtils::getSimCount(); i++) {
        int retry_count = 0;
        while ((getSIMInitState() & (1 << i)) == 0) {
            usleep(100*1000);
            retry_count++;
            //logD(RFX_LOG_TAG, "wait SIM[%d] init done, retry:%d", i, retry_count);
            if (retry_count > 50) {
                resetRadio();
                return false;
            }
        }
    }
    return true;
}
