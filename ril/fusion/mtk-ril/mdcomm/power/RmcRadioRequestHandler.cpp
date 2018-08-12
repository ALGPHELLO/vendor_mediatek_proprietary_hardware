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

#include "RmcRadioRequestHandler.h"
#include "RfxIntsData.h"
#include "RfxVoidData.h"
#include "RfxRilUtils.h"
#include "telephony/librilutilsmtk.h"

#define RFX_LOG_TAG "RmcRadioReq"
#define PROPERTY_AIRPLANE_MODE "persist.radio.airplane.mode.on"
#define PROPERTY_SIM_MODE "persist.radio.sim.mode"
#define MAX_RETRY_COUNT 20

typedef enum {
    RADIO_MODE_SIM1_ONLY = 1,
    RADIO_MODE_SIM2_ONLY = (RADIO_MODE_SIM1_ONLY << 1),
    RADIO_MODE_SIM3_ONLY = (RADIO_MODE_SIM1_ONLY << 2),
    RADIO_MODE_SIM4_ONLY = (RADIO_MODE_SIM1_ONLY << 3),
} RadioMode;

// register handler to channel
RFX_IMPLEMENT_OP_PARENT_HANDLER_CLASS(RmcRadioRequestHandler, RIL_CMD_PROXY_9);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData,
        RFX_MSG_REQUEST_RADIO_POWER);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData,
        RFX_MSG_REQUEST_COMMAND_BEFORE_RADIO_POWER);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData,
        RFX_MSG_REQUEST_BOOT_TURN_ON_RADIO);

RmcRadioRequestHandler::RmcRadioRequestHandler(int slotId, int channelId) :
        RfxBaseHandler (slotId, channelId) {
    logD(RFX_LOG_TAG, "RmcRadioRequestHandler constructor");
    const int request[] = {
        RFX_MSG_REQUEST_RADIO_POWER,
        RFX_MSG_REQUEST_BOOT_TURN_ON_RADIO,
    };

    registerToHandleRequest(request, sizeof(request)/sizeof(int));

    sp<RfxAtResponse> response;
    response = atSendCommand("ATE0Q0V1");
    response = atSendCommand("AT+CMEE=1");
    /*  Enable get +CIEV:7 URC to receive SMS SIM Storage Status*/
    // The command MUST send to modem before AT+EFUN=0
    response = atSendCommand("AT+CMER=1,0,0,2,0");

    // External SIM [Start]
    if (RfxRilUtils::isExternalSimSupport()) {
        queryModemVsimCapability();
        requestSwitchExternalSim();
    }
    // External SIM [End]

    response = atSendCommand("AT+ESIMS");
    /* HEX character set */
    response = atSendCommand("AT+CSCS=\"UCS2\"");
    updateDataCallPrefer();

    int mainSlotId = RfxRilUtils::getMajorSim() - 1;
    if (mainSlotId == slotId) {
        resetIpoProperty();
        enableMdProtocol();
        enableSilentReboot();
        updateSupportDSBP();
        response = atSendCommand("AT+EPOC");
        response = atSendCommand("AT+EFUN=0");
    }
    getMclStatusManager()->setIntValue(RFX_STATUS_KEY_RADIO_STATE, RADIO_STATE_OFF);
}

RmcRadioRequestHandler::~RmcRadioRequestHandler() {
}

void RmcRadioRequestHandler::onHandleRequest(const sp<RfxMclMessage>& msg) {
    int id = msg->getId();
    logD(RFX_LOG_TAG, "onHandleRequest: %s(%d)", idToString(id), id);
    switch(id) {
        case RFX_MSG_REQUEST_RADIO_POWER:
            requestRadioPower(msg);
            break;
        case RFX_MSG_REQUEST_BOOT_TURN_ON_RADIO:
            bootupSetRadioPower(msg);
            break;
        default:
            logE(RFX_LOG_TAG, "Should not be here");
            break;
    }
}

void RmcRadioRequestHandler::onHandleTimer() {
    // do something
}

void RmcRadioRequestHandler::onHandleEvent(const sp<RfxMclMessage>& msg) {
    int id = msg->getId();
    logD(RFX_LOG_TAG, "onHandleEvent: %d", id);
    switch(id) {
        default:
            logE(RFX_LOG_TAG, "should not be here");
            break;
    }
}

void RmcRadioRequestHandler::requestRadioPower(const sp<RfxMclMessage> &msg) {
    sp<RfxAtResponse> response;
    RIL_RadioState curState = (RIL_RadioState) getMclStatusManager()->getIntValue(
            RFX_STATUS_KEY_RADIO_STATE, 0);
    RIL_RadioState newState;
    bool notSent = false;
    AT_CME_Error cause;
    RIL_Errno errNo = RIL_E_SUCCESS;
    int isRadioOn;
    sp<RfxMclMessage> resMsg;

    int onOff = ((int *)msg->getData()->getData())[0];
    int caller =  msg->getData()->getDataLength()/sizeof(int) > 1
            ? ((int *)msg->getData()->getData())[1] : -1;
    int slotId = msg->getSlotId();
    getMclStatusManager(slotId)->setIntValue(RFX_STATUS_KEY_RADIO_POWER_MSIM_MODE, onOff);

    int targetMode = 0;
    for (int i = 0; i < RfxRilUtils::getSimCount(); i++) {
        targetMode |= getMclStatusManager(i)->getIntValue(RFX_STATUS_KEY_RADIO_POWER_MSIM_MODE, 0) << i;
    }

    // EFUN affected by SIM switch
    int mainSlotId = getNonSlotMclStatusManager()->getIntValue(
                    RFX_STATUS_KEY_MAIN_CAPABILITY_SLOT, 0);
    if(RFX_SLOT_ID_0 != mainSlotId) {
        int sim1mode = targetMode & RADIO_MODE_SIM1_ONLY;
        int sim3Gmode = ((targetMode & (RADIO_MODE_SIM1_ONLY<< mainSlotId)) > 0) ?1 :0;
        logD(RFX_LOG_TAG, "switched : original mode=%d, sim1mode=%d, sim3Gmode=%d , need switch",
                targetMode, sim1mode, sim3Gmode);

        targetMode &= ~(RADIO_MODE_SIM1_ONLY);
        targetMode &= ~(RADIO_MODE_SIM1_ONLY << mainSlotId);
        targetMode |= (sim1mode << mainSlotId);
        targetMode |= sim3Gmode;
    }
    logI(RFX_LOG_TAG, "requestRadioPower, desired power = %d, target mode = %d, caller: %d",
            onOff, targetMode, caller);

    /*
    * RFOFF_CAUSE_UNSPECIFIED = 0,
    * RFOFF_CAUSE_DUPLEX_MODE = 1,
    * RFOFF_CAUSE_POWER_OFF = 2,
    * RFOFF_CAUSE_SIM_SWITCH = 3,
    */
    if (targetMode == 0 /*&& !s_md_off*/) {
        if (caller >= 0) {
            response = atSendCommand(String8::format("AT+EFUN=0,%d", caller));
        } else {
            response = atSendCommand("AT+EFUN=0");
        }
        newState = RADIO_STATE_OFF;
    } else if (targetMode > 0) {
        if (!onOff) {
            response = atSendCommand(String8().format(("AT+EFUN=%d,%d"), targetMode, caller));
        } else {
            response = atSendCommand(String8().format(("AT+EFUN=%d"), targetMode));
        }
        newState = (onOff==1)? RADIO_STATE_ON: RADIO_STATE_OFF;
    } else {
        newState = curState;
        notSent = true;
    }

    // error handle
    if (!notSent && response->getSuccess() == 0) {
        cause = response->atGetCmeError();
        errNo = RIL_E_GENERIC_FAILURE;
        newState = curState;

        logD(RFX_LOG_TAG, "Get error cause: %d",cause);

        if (cause ==  CME_ERROR_NON_CME) {
            logW(RFX_LOG_TAG, "There is something wrong with the returned CME ERROR, please help to check");
        } else if (cause == CME_OPERATION_NOT_ALLOWED_ERR) {
            /* RIL_E_OEM_ERROR_1 indicates that EFUN conflicts with ERAT */
            errNo = RIL_E_OEM_ERROR_1;
        } else {
            /*********************************
                    * The reason of the error:
                    * 1. the radio state of the modem and rild is not sync.
                    * 2. There is any action or call existed in the modem
                    **********************************/

            isRadioOn = isModemRadioOn();
            if (isRadioOn == -1) {
                logD(RFX_LOG_TAG, "AT+CFUN? can't be executed normally");
                goto error;
            } else if(!isRadioOn && curState != RADIO_STATE_OFF) {
                // phone off
                logW(RFX_LOG_TAG, "The state of the modem is not synchronized with the state in the RILD: phone off");
                newState = RADIO_STATE_OFF;
                errNo = RIL_E_SUCCESS;
            } else if ( isRadioOn && curState == RADIO_STATE_OFF) {
                // phone on
                logW(RFX_LOG_TAG, "The state of the modem is not synchronized with the state in the RILD: phone on");
                newState = RADIO_STATE_ON;
                errNo = RIL_E_SUCCESS;
            } else {
                // The execution of the AT+CFUN is not success.
                logD(RFX_LOG_TAG, "AT+CFUN=<fun> can't be executed");
            }
        }
    }

    getMclStatusManager(slotId)->setIntValue(RFX_STATUS_KEY_RADIO_STATE, newState);
    resMsg = RfxMclMessage::obtainResponse(msg->getId(), errNo, RfxVoidData(), msg,
            false);
    responseToTelCore(resMsg);
    return;

error:
    resMsg = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_GENERIC_FAILURE,
            RfxVoidData(), msg, false);
    responseToTelCore(resMsg);
    return;
}

/** returns 1 if on, 0 if off, and -1 on error */
int RmcRadioRequestHandler::isModemRadioOn() {
    sp<RfxAtResponse> response;
    RfxAtLine *atLine = NULL;
    int err, ret;

    response = atSendCommandSingleline("AT+CFUN?", "+CFUN:");
    if (response->getError() < 0 || response->getSuccess() == 0) {
        // assume radio is off
        goto error;
    }

    atLine = response->getIntermediates();
    atLine->atTokStart(&err);
    if (err < 0) goto error;

    ret = atLine->atTokNextint(&err);
    if (err < 0) goto error;

    ret = (ret == 4 || ret == 0) ? 0 :    // phone off
          (ret == 1) ? 1 :              // phone on
          -1;                           // invalid value

    return ret;

error:
    return -1;
}

void RmcRadioRequestHandler::enableMdProtocol() {
    sp<RfxAtResponse> response;

    switch (RfxRilUtils::getSimCount()) {
        case 1:
            response = atSendCommand("AT+ESADM=1");
            break;
        case 2:
            response = atSendCommand("AT+ESADM=3");
            break;
        case 3:
            response = atSendCommand("AT+ESADM=7");
            break;
        case 4:
            response = atSendCommand("AT+ESADM=15");
            break;
    }
}

void RmcRadioRequestHandler::enableSilentReboot() {
    sp<RfxAtResponse> response;
    int muxreport_case = 0;
    char property_value[RFX_PROPERTY_VALUE_MAX] = { 0 };
    int auto_unlock_pin = -1;
    int isSilentReboot = -1;

    rfx_property_get("ril.mux.report.case", property_value, "0");
    muxreport_case = atoi(property_value);
    logD(RFX_LOG_TAG, "getprop ril.mux.report.case %d", muxreport_case);
    switch (muxreport_case) {
        case 0:
            isSilentReboot = 0;
            break;
        case 1:
        case 2:
        case 5:
        case 6:
            isSilentReboot = 1;
            break;
    }
    rfx_property_set("ril.mux.report.case", "0");
    // eboot property will be set to 0 when ipo shutdown, no needs to silent reboot in this case
    // ebbot property will be set to 1 when flight mode turn on, and 3g switch reset modem
    rfx_property_get("gsm.ril.eboot", property_value, "0");
    auto_unlock_pin = atoi(property_value);
    logD(RFX_LOG_TAG, "getprop gsm.ril.eboot %d", auto_unlock_pin);
    isSilentReboot |= auto_unlock_pin;

    /********************************
     * AT+EBOOT=<mode>
     *
     * 0: Normal boot up
     * 1: Silent boot up (Verify PIN by modem internally)
     *********************************/
    switch (isSilentReboot) {
        case 0:
            response = atSendCommand("AT+EBOOT=0");
            break;
        case 1:
            response = atSendCommand("AT+EBOOT=1");
            break;
        default:
            response = atSendCommand("AT+EBOOT=0");
            break;
    }
    rfx_property_set("gsm.ril.eboot", "0");
}

void RmcRadioRequestHandler::resetIpoProperty() {
    rfx_property_set((char *) "ril.ipo.radiooff", "0");
}

void RmcRadioRequestHandler::updateSupportDSBP() {
    char prop[RFX_PROPERTY_VALUE_MAX] = {0};
    rfx_property_get((char *) "persist.radio.mtk_dsbp_support", prop, "0");
    atSendCommand(String8::format("AT+EDSBP=%s", prop));
}

void RmcRadioRequestHandler::updateDataCallPrefer() {
    // set data/call prefer
    // 0 : call prefer
    // 1 : data prefer
    char gprsPrefer[RFX_PROPERTY_VALUE_MAX] = { 0 };
    rfx_property_get("persist.radio.gprs.prefer", gprsPrefer, "0");
    if ((atoi(gprsPrefer) == 0)) {
        // call prefer
        atSendCommand("AT+EGTP=1");
        atSendCommand("AT+EMPPCH=1");
    } else {
        // data prefer
        atSendCommand("AT+EGTP=0");
        atSendCommand("AT+EMPPCH=0");
    }
}

// External SIM [Start]
int RmcRadioRequestHandler::queryModemVsimCapability()
{
    sp<RfxAtResponse> p_response;
    int err, temp_cap = 0;
    String8 cmd("");
    RfxAtLine *line = NULL;
    char *capability = NULL;

    /**
     * Query if the VSIM has been enabled
     * AT+EAPVSIM?
     * success: +EAPVSIM: <capability and status>
     *          APVSIM Capability or Status Query (bit mask)
     *          0x01 : APVSIM ON/OFF status
     *          0x02 : APVSIM Support enable/disable via Hot Swap Mechanism
     * fail: ERROR
     *
     */

    cmd.append(String8::format("AT+EAPVSIM?"));
    p_response = atSendCommandSingleline(cmd, "+EAPVSIM:");
    cmd.clear();

    if (p_response == NULL || p_response->getError() < 0) {
        LOGE("queryModemVsimCapability fail");
         goto done;
    }

    switch (p_response->atGetCmeError()) {
        LOGD("p_response = %d /n", p_response->atGetCmeError());
        case CME_SUCCESS:
            if (p_response->getError() < 0 || 0 == p_response->getSuccess()) {
                goto done;
            } else {
                line = p_response->getIntermediates();

                line->atTokStart(&err);
                if(err < 0) goto done;

                temp_cap = line->atTokNextint(&err);
                if(err < 0) goto done;
            }
            break;
        case CME_UNKNOWN:
            LOGD("queryModemVsimCapability p_response: CME_UNKNOWN");
            break;
        default:
            LOGD("queryModemVsimCapability fail");
    }

done:
    LOGD("queryModemVsimCapability done, capability: %d", temp_cap);
    asprintf(&capability, "%d", temp_cap);
    setMSimProperty(m_slot_id, (char*)"gsm.modem.vsim.capability", capability);
    free(capability);

    return temp_cap;
}


void RmcRadioRequestHandler::requestSwitchExternalSim() {
    sp<RfxAtResponse> p_response;
    int err, ret;
    String8 cmd("");
    char vsim_enabled_prop[RFX_PROPERTY_VALUE_MAX] = {0};
    char vsim_inserted_prop[RFX_PROPERTY_VALUE_MAX] = {0};
    char persist_vsim_inserted_prop[RFX_PROPERTY_VALUE_MAX] = {0};

    getMSimProperty(m_slot_id, (char*)"gsm.external.sim.enabled", vsim_enabled_prop);
    getMSimProperty(m_slot_id, (char*)"gsm.external.sim.inserted", vsim_inserted_prop);

    if (RfxRilUtils::isPersistExternalSimDisabled()) {
        rfx_property_set("persist.radio.external.sim", "");
    } else {
        getMSimProperty(m_slot_id, (char*)"persist.radio.external.sim",
                persist_vsim_inserted_prop);
    }

    if (atoi(persist_vsim_inserted_prop) > 0) {
        logD(RFX_LOG_TAG, "[VSIM] persist.radio.external.sim is 1.");
    }

    /* When to set true and when to set false ? */
    if (atoi(vsim_enabled_prop) > 0 && atoi(vsim_inserted_prop) > 0) {
        // Mean VSIM enabled and modem reset only rild reset case,
        // should recover to previous status
        if (RfxRilUtils::isExternalSimOnlySlot(m_slot_id) > 0) {
            cmd.append(String8::format("AT+EAPVSIM=1,1"));
        } else {
            cmd.append(String8::format("AT+EAPVSIM=1"));
        }
    } else {
        // Might reboot or VSIM did't be enabled
        if (atoi(persist_vsim_inserted_prop) > 0) {
            // Case 1. Device reboot and VSIM enabled before reboot. Keep slot to VSIM only.
            cmd.append(String8::format("AT+EAPVSIM=1,0"));
            logD(RFX_LOG_TAG,
                    "[VSIM] Case 1. Device reboot and VSIM enabled before reboot. Keep slot to VSIM only.");
        } else if (RfxRilUtils::isExternalSimOnlySlot(m_slot_id) > 0) {
            // Case 2. Device reboot and the slot has been set to VSIM only in configuration.
            cmd.append(String8::format("AT+EAPVSIM=1,0"));
            logD(RFX_LOG_TAG,
                    "[VSIM] Case 2. Device reboot and the slot has been set to VSIM only in configuration.");
        } else {
            // Case 3. Others. VSIM disabled and it is not VSIM only protocol.
            cmd.append(String8::format("AT+EAPVSIM=0"));
        }
    }
    p_response = atSendCommand(cmd);
    cmd.clear();

    if (p_response->getError() < 0) {
        logE(RFX_LOG_TAG, "[VSIM] requestSwitchExternalSim Fail");
        ret = RIL_E_GENERIC_FAILURE;
        goto done;
    }

    if (0 == p_response->getSuccess()) {
        switch (p_response->atGetCmeError()) {
            logD(RFX_LOG_TAG, "[VSIM] requestSwitchExternalSim p_response = %d /n", p_response->atGetCmeError());
            default:
                ret = RIL_E_GENERIC_FAILURE;
                goto done;
        }
    }

done:
    logE(RFX_LOG_TAG, "[VSIM] requestSwitchExternalSim Done");
}
// External SIM [End]

void RmcRadioRequestHandler::bootupSetRadioPower(const sp<RfxMclMessage> &msg) {
    logD(RFX_LOG_TAG, "bootupSetRadioPower");
    RIL_RadioState newState;
    RIL_Errno errNo = RIL_E_GENERIC_FAILURE;

    char filghtMode[RFX_PROPERTY_VALUE_MAX] = { 0 };
    rfx_property_get(PROPERTY_AIRPLANE_MODE, filghtMode, "false");
    if (strcmp("false", filghtMode)) {
        logE(RFX_LOG_TAG, "under airplane mode, return");
        for(int i = 0; i < RfxRilUtils::getSimCount(); i++) {
            getMclStatusManager(i)->setIntValue(RFX_STATUS_KEY_RADIO_STATE, RADIO_STATE_OFF);
        }
        sp<RfxMclMessage> resMsg = RfxMclMessage::obtainResponse(msg->getId(), errNo, RfxVoidData(),
                msg, false);
        responseToTelCore(resMsg);
        return;
    }

    char simMode[RFX_PROPERTY_VALUE_MAX] = { 0 };
    rfx_property_get(PROPERTY_SIM_MODE, simMode, (String8::format("%d", getSimCount())).string());
    int targetMode = atoi(simMode);

    if (0 == targetMode) {
        // never bootup, according to SIM card status
        targetMode = ((int *)msg->getData()->getData())[0];
    } else {
        targetMode = targetMode & ((int *)msg->getData()->getData())[0];
    }
    logD(RFX_LOG_TAG, "bootupSetRadioPower, targetMode = %d", targetMode);
    int originMode = targetMode;

    // EFUN affected by SIM switch
    int mainSlotId = RfxRilUtils::getMajorSim() - 1;
    if(RFX_SLOT_ID_0 != mainSlotId) {
        int sim1mode = targetMode & RADIO_MODE_SIM1_ONLY;
        int sim3Gmode = ((targetMode & (RADIO_MODE_SIM1_ONLY<< mainSlotId)) > 0) ? 1 : 0;
        logD(RFX_LOG_TAG, "switched : original mode=%d, sim1mode=%d, sim3Gmode=%d , need switch",
                targetMode, sim1mode, sim3Gmode);

        targetMode &= ~(RADIO_MODE_SIM1_ONLY);
        targetMode &= ~(RADIO_MODE_SIM1_ONLY << mainSlotId);
        targetMode |= (sim1mode << mainSlotId);
        targetMode |= sim3Gmode;
    }
    logD(RFX_LOG_TAG, "bootupSetRadioPower, target mode = %d", targetMode);

    if (targetMode == 0) {
        for(int i = 0; i < RfxRilUtils::getSimCount(); i++) {
            getMclStatusManager(i)->setIntValue(RFX_STATUS_KEY_RADIO_STATE, RADIO_STATE_OFF);
        }
    } else {
        sp<RfxAtResponse> response = atSendCommand(String8().format(("AT+EFUN=%d"), targetMode));
        // add retry mechanism
        int retryCount = 0;
        while (response->getSuccess() == 0 && retryCount < MAX_RETRY_COUNT) {
            response = atSendCommand(String8().format(("AT+EFUN=%d"), targetMode));
            retryCount++;
            usleep(500*1000);
        }
        if (response->getSuccess() == 1) {
            errNo = RIL_E_SUCCESS;
            for(int i = 0; i < RfxRilUtils::getSimCount(); i++) {
                if (originMode & (RADIO_MODE_SIM1_ONLY << i)) {
                    getMclStatusManager(i)->setBoolValue(RFX_STATUS_KEY_REQUEST_RADIO_POWER, true);
                }
                getMclStatusManager(i)->setIntValue(RFX_STATUS_KEY_RADIO_STATE,
                        (originMode & (RADIO_MODE_SIM1_ONLY << i)) ? RADIO_STATE_ON :
                         RADIO_STATE_OFF);
                getMclStatusManager(i)->setIntValue(RFX_STATUS_KEY_RADIO_POWER_MSIM_MODE,
                        (originMode & (RADIO_MODE_SIM1_ONLY << i)) ? 1 : 0);
            }
        }
    }
    sp<RfxMclMessage> resMsg = RfxMclMessage::obtainResponse(msg->getId(), errNo, RfxVoidData(),
            msg, false);
    responseToTelCore(resMsg);
}
