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
#include "embms/RmcEmbmsURCHandler.h"
#include "RmcNetworkUrcHandler.h"
#include "RfxCellInfoData.h"
#include "RfxNetworkScanResultData.h"
/*ADD-BEGIN-JUNGO-20101008-CTZV support */
#include <time.h>
#include "ViaBaseHandler.h"
#include "RfxViaUtils.h"
#include "telephony/librilutilsmtk.h"

// register data
RFX_REGISTER_DATA_TO_URC_ID(RfxVoidData, RFX_MSG_URC_RESPONSE_VOICE_NETWORK_STATE_CHANGED);
RFX_REGISTER_DATA_TO_URC_ID(RfxIntsData, RFX_MSG_URC_SIGNAL_STRENGTH);
RFX_REGISTER_DATA_TO_URC_ID(RfxStringsData, RFX_MSG_URC_RESPONSE_CS_NETWORK_STATE_CHANGED);
RFX_REGISTER_DATA_TO_URC_ID(RfxIntsData, RFX_MSG_URC_CDMA_OTA_PROVISION_STATUS);
RFX_REGISTER_DATA_TO_URC_ID(RfxIntsData, RFX_MSG_URC_CDMA_PRL_CHANGED);
RFX_REGISTER_DATA_TO_URC_ID(RfxStringsData, RFX_MSG_URC_NEIGHBORING_CELL_INFO);
RFX_REGISTER_DATA_TO_URC_ID(RfxStringsData, RFX_MSG_URC_NETWORK_INFO);
RFX_REGISTER_DATA_TO_URC_ID(RfxCellInfoData, RFX_MSG_URC_CELL_INFO_LIST);
RFX_REGISTER_DATA_TO_URC_ID(RfxIntsData, RFX_MSG_URC_GMSS_RAT_CHANGED);
RFX_REGISTER_DATA_TO_URC_ID(RfxStringData, RFX_MSG_URC_NITZ_TIME_RECEIVED);
RFX_REGISTER_DATA_TO_URC_ID(RfxIntsData, RFX_MSG_URC_NETWORK_EVENT);
RFX_REGISTER_DATA_TO_URC_ID(RfxIntsData, RFX_MSG_URC_RESPONSE_MMRR_STATUS_CHANGED);
RFX_REGISTER_DATA_TO_URC_ID(RfxIntsData, RFX_MSG_URC_RESPONSE_ACMT);
// RFX_REGISTER_DATA_TO_URC_ID(RfxIntsData, RFX_MSG_URC_MODULATION_INFO);
RFX_REGISTER_DATA_TO_URC_ID(RfxIntsData, RFX_MSG_URC_RESPONSE_CELLULAR_DATA_REG_STATE);
RFX_REGISTER_DATA_TO_URC_ID(RfxIntsData, RFX_MSG_URC_PSEUDO_CELL_INFO);
RFX_REGISTER_DATA_TO_URC_ID(RfxNetworkScanResultData, RFX_MSG_URC_NETWORK_SCAN_RESULT);

RFX_REGISTER_DATA_TO_EVENT_ID(RfxVoidData, RFX_MSG_EVENT_CONFIRM_RAT_BEGIN);
RFX_REGISTER_DATA_TO_EVENT_ID(RfxVoidData, RFX_MSG_EVENT_FEMTOCELL_UPDATE);
RFX_REGISTER_DATA_TO_EVENT_ID(RfxIntsData, RFX_MSG_EVENT_PS_NETWORK_STATE);

// register handler to channel
RFX_IMPLEMENT_HANDLER_CLASS(RmcNetworkUrcHandler, RIL_CMD_PROXY_URC);

/// M: Local Time VZ_REQ_LTEDATA_6793 @{
int RmcNetworkUrcHandler::bSIB16Received = 0;
int RmcNetworkUrcHandler::bNitzTzAvailble = 0;
int RmcNetworkUrcHandler::bNitzDstAvailble = 0;
char RmcNetworkUrcHandler::ril_nw_nitz_tz[MAX_NITZ_TZ_DST_LENGTH] = {0};
char RmcNetworkUrcHandler::ril_nw_nitz_dst[MAX_NITZ_TZ_DST_LENGTH] = {0};
/// @}

RmcNetworkUrcHandler::RmcNetworkUrcHandler(int slot_id, int channel_id) :
        RmcNetworkHandler(slot_id, channel_id),
        allowed_urc(NULL),
        ril_data_urc_status(4),
        ril_data_urc_rat(0) {
    int m_slot_id = slot_id;
    int m_channel_id = channel_id;
    ViaBaseHandler *mViaHandler = RfxViaUtils::getViaHandler();
    if (mViaHandler != NULL) {
        mViaHandler->registerForViaUrc(this);
        allowed_urc = mViaHandler->getViaAllowedUrcForNw();
    }
    const char* urc[] = {
        "+EREG:",
        "+EGREG:",
        "+PSBEARER:",
        "+ECSQ:",
        "^OTACMSG:",
        "+ERPRAT:",
        "+EGMSS:",
        "+EIPRL:",
        "+EDEFROAM:",
        "+ECELLINFO:",
        "+ENWINFO:",
        "+ECSG:",
        "+EFCELL:",
        "+ECELL:",
        "+CTZEU:",
        "+CIEV: 10",
        "+CIEV: 11",
        "+EREGINFO:",
        "+EMMRRS:",
        "+EWFC:",
        "+EACMT:",
        "+EMODCFG:",
        "+EONS:",
        "+EAPC:"
    };

    registerToHandleURC(urc, sizeof(urc)/sizeof(char *));
}

RmcNetworkUrcHandler::~RmcNetworkUrcHandler() {
}

void RmcNetworkUrcHandler::onHandleUrc(const sp<RfxMclMessage>& msg) {
    ViaBaseHandler *mViaHandler = RfxViaUtils::getViaHandler();
    if (strStartsWith(msg->getRawUrc()->getLine(), "+EREG:")) {
        handleCsNetworkStateChanged(msg);
    } else if (strStartsWith(msg->getRawUrc()->getLine(), "+EGREG:")) {
        handlePsNetworkStateChanged(msg);
    } else if (strStartsWith(msg->getRawUrc()->getLine(), "+PSBEARER:")) {
        handlePsDataServiceCapability();
    } else if (strStartsWith(msg->getRawUrc()->getLine(), "+ECSQ:")) {
        handleSignalStrength(msg);
    } else if (strStartsWith(msg->getRawUrc()->getLine(), "^OTACMSG:")) {
        handleOtaProvisionStatus(msg);
    } else if (strStartsWith(msg->getRawUrc()->getLine(), "+ERPRAT:")) {
        handleConfirmRatBegin(msg);
    } else if (strStartsWith(msg->getRawUrc()->getLine(), "+EGMSS:")) {
        handleGmssRatChanged(msg);
    } else if (strStartsWith(msg->getRawUrc()->getLine(), "+EIPRL:")) {
        handleSystemInPrlIndication(msg);
    }  else if (strStartsWith(msg->getRawUrc()->getLine(), "+EDEFROAM:")) {
        handleDefaultRoamingIndicator(msg);
    } else if (strStartsWith(msg->getRawUrc()->getLine(), "+ECELLINFO:")) {
        //handleNeighboringCellInfo(msg);
    } else if (strStartsWith(msg->getRawUrc()->getLine(), "+ENWINFO:")) {
        handleNetworkInfo(msg);
    } else if (strStartsWith(msg->getRawUrc()->getLine(), "+ECSG:")) {
        handleGsmFemtoCellInfo(msg);
    } else if (strStartsWith(msg->getRawUrc()->getLine(), "+EFCELL:")) {
        handleCdmaFemtoCellInfo(msg);
    } else if (strStartsWith(msg->getRawUrc()->getLine(), "+ECELL:")) {
        handleCellInfoList(msg);
    } else if (strStartsWith(msg->getRawUrc()->getLine(), "+CTZEU:")) {
        handleNitzTzReceived(msg);
    } else if (strStartsWith(msg->getRawUrc()->getLine(), "+CIEV: 10")) {
        handleNitzOperNameReceived(msg);
    } else if (strStartsWith(msg->getRawUrc()->getLine(), "+CIEV: 11")) {
        handleSib16TimeInfoReceived(msg);
    } else if (strStartsWith(msg->getRawUrc()->getLine(), "+EREGINFO:")) {
        handleNetworkEventReceived(msg);
    } else if (strStartsWith(msg->getRawUrc()->getLine(), "+EMMRRS:")) {
        //handleMMRRStatusChanged(msg);
    } else if (strStartsWith(msg->getRawUrc()->getLine(), "+EWFC:")) {
        handleWfcStateChanged(msg);
    } else if (strStartsWith(msg->getRawUrc()->getLine(), "+EACMT:")) {
        //handleACMT(msg);
//    } else if (strStartsWith(msg->getRawUrc()->getLine(), "+EMODCFG:")) {
//        handleModulationInfoReceived(msg);
    } else if (strStartsWith(msg->getRawUrc()->getLine(), "+EONS:")) {
        handleEnhancedOperatorNameDisplay(msg);
    } else if (strStartsWith(msg->getRawUrc()->getLine(), "+EAPC:")) {
        handlePseudoCellInfo(msg);
    } else if (mViaHandler != NULL) {
        mViaHandler-> handleViaUrc(msg, this, m_slot_id);
    }
}

void RmcNetworkUrcHandler::handleCsNetworkStateChanged(const sp<RfxMclMessage>& msg) {
    /* +EREG: <stat>[,<lac>,<ci>,[<eAct>],[nwk_existence>],[<roam_indicator>][,<cause_type>,<reject_cause>]] */
    int err;
    int skip;
    int response[6] = {-1};
    char *responseStr[6];
    int i = 0;
    sp<RfxMclMessage> urc;
    RfxAtLine *line = msg->getRawUrc();

    line->atTokStart(&err);
    if (err < 0) goto error;

    response[0] = line->atTokNextint(&err);
    if (err < 0) goto error;

    // For LTE and C2K
    response[0] = convertRegState(response[0], true);

    // set "-1" to indicate "the field is not present"
    for (i = 1; i < 6; i++) {
        asprintf(&responseStr[i], "-1");
    }

    // fill in <state>
    asprintf(&responseStr[0], "%d", response[0]);

    // get <lac>
    response[1] = line->atTokNexthexint(&err);
    if (err >= 0) {
        free(responseStr[1]);
        responseStr[1] = NULL;
        asprintf(&responseStr[1], "%x", response[1]);

        // get <ci>
        response[2] = line->atTokNexthexint(&err);
        if (err >= 0) {
            free(responseStr[2]);
            responseStr[2] = NULL;
            asprintf(&responseStr[2], "%x", response[2]);

            // get <eAct>
            response[3] = line->atTokNextint(&err);
            if (err >= 0) {
                free(responseStr[3]);
                responseStr[3] = NULL;
                asprintf(&responseStr[3], "%d", convertCSNetworkType(response[3]));

                // get <nwk_existence>
                response[5]= line->atTokNextint(&err);

                // ECC support
                if (response[5] != 1) {
                    // if cid is 0x0fffffff or -1 means it is invalid
                    // if lac is 0xffff or -1 means it is invalid
                    if (((response[0] == 0) || (response[0] == 2) || (response[0] == 3))
                            && ((response[2] != 0x0fffffff)  // cid
                            && (response[1] != 0xffff)       // lac
                            && (response[2] != -1)           // cid
                            && (response[1] != -1))          // lac
                            && convertCSNetworkType(response[3]) != 14) {   // not LTE & LET_CA
                        response[5] = 1;
                    }
                }
                if (response[5] != 1) {
                    response[5] = 0;
                }
                free(responseStr[5]);
                responseStr[5] = NULL;
                asprintf(&responseStr[5], "%d", response[5]);

                if (err >= 0) {
                    // get <roam_indicator>
                    skip = line->atTokNextint(&err);
                    if (err >= 0) {
                        // get <cause_type>
                        skip = line->atTokNextint(&err);
                        if (err >= 0) {
                            // get <reject_cause>
                            response[4] = line->atTokNextint(&err);
                            if (err >= 0) {
                                free(responseStr[4]);
                                responseStr[4] = NULL;
                                asprintf(&responseStr[4], "%d", response[4]);
                            }
                        }
                    }
                }
            }
        }
    }

    urc = RfxMclMessage::obtainUrc(RFX_MSG_URC_RESPONSE_CS_NETWORK_STATE_CHANGED,
            m_slot_id, RfxStringsData(responseStr, 6));
    responseToTelCore(urc);

    urc = RfxMclMessage::obtainUrc(RFX_MSG_URC_RESPONSE_VOICE_NETWORK_STATE_CHANGED,
            m_slot_id, RfxVoidData());
    responseToTelCore(urc);

    for (i = 0; i < 6; i++) {
        free(responseStr[i]);
    }
    return;

error:
    logE(LOG_TAG, "There is something wrong with the URC");
}

void RmcNetworkUrcHandler::handlePsDataServiceCapability() {
    sp<RfxMclMessage> urc;

    urc = RfxMclMessage::obtainUrc(RFX_MSG_URC_RESPONSE_VOICE_NETWORK_STATE_CHANGED,
            m_slot_id, RfxVoidData());
    responseToTelCore(urc);
}

void RmcNetworkUrcHandler::handleSignalStrength(const sp<RfxMclMessage>& msg) {
    int resp[14] ={0};
    int err;
    RfxAtLine *line = msg->getRawUrc();
    RIL_SIGNAL_STRENGTH_CACHE signal;

    pthread_mutex_lock(&s_signalStrengthMutex[m_slot_id]);
    memcpy(&resp, &signal_strength_cache[m_slot_id], 14*sizeof(int));

    // go to start position
    line->atTokStart(&err);
    if (err < 0) {
        pthread_mutex_unlock(&s_signalStrengthMutex[m_slot_id]);
        return;
    }

    err = getSignalStrength(line);
    if (err < 0) {
        pthread_mutex_unlock(&s_signalStrengthMutex[m_slot_id]);
        return; // some invalid value from MD.
    }

    // compare the current and previous signal strength
    if (0 == memcmp(&resp, &signal_strength_cache[m_slot_id], sizeof(resp))) {
        logV(LOG_TAG, "The current signal is the same as previous, ignore");
        pthread_mutex_unlock(&s_signalStrengthMutex[m_slot_id]);
        return;
    }

    memcpy(&resp, &signal_strength_cache[m_slot_id], 14*sizeof(int));
    pthread_mutex_unlock(&s_signalStrengthMutex[m_slot_id]);

    logD(LOG_TAG, "%s, sig1:%d, sig2:%d, cdma_dbm:%d, cdma_ecio:%d, evdo_dbm:%d,"
            "evdo_ecio:%d, evdo_snr:%d, lte_sig:%d, lte_rsrp:%d, lte_rsrq:%d, lte_rssnr:%d, lte_cqi:%d,"
            "lte_timing_advance:%d, td_scdma_rscp:%d",
            __FUNCTION__, resp[0], resp[1], resp[2], resp[3], resp[4], resp[5], resp[6], resp[7],
             resp[8], resp[9], resp[10], resp[11], resp[12], resp[13]);

    sp<RfxMclMessage> urc = RfxMclMessage::obtainUrc(RFX_MSG_URC_SIGNAL_STRENGTH, m_slot_id, RfxIntsData(resp, 14));
    responseToTelCore(urc);
}


unsigned int RmcNetworkUrcHandler::combineWfcEgregState() {
    unsigned int uiRet = 0;

    if (ril_wfc_reg_status[m_slot_id] == 1) {
        uiRet = ril_wfc_reg_status[m_slot_id];
    } else {
        uiRet = ril_data_urc_status;
    }

    RFX_LOG_V(LOG_TAG, "combineWfcEgregState() slot_id=%d, data_urc=%d, wfc_reg=%d, uiRet=%d",
        m_slot_id, ril_data_urc_status, ril_wfc_reg_status[m_slot_id], uiRet);

    return uiRet;
}

void RmcNetworkUrcHandler::handlePsNetworkStateChanged(const sp<RfxMclMessage>& msg) {
    int err;
    int stat[2];
    unsigned int response[4] = {0};
    sp<RfxMclMessage> urc;
    RfxAtLine *line = msg->getRawUrc();

    // send a copy to eMBMS module first
    char* urc_str = line->getLine();

    // logD(LOG_TAG, "sendEvent RFX_MSG_EVENT_EMBMS_POST_NETWORK_UPDATE");
    sendEvent(RFX_MSG_EVENT_EMBMS_POST_NETWORK_UPDATE, RfxStringData(urc_str, strlen(urc_str)),
        RIL_CMD_PROXY_EMBMS, msg->getSlotId());

    // go to start position
    line->atTokStart(&err);
    if (err < 0) goto error;

    response[0] = line->atTokNextint(&err);
    if (err < 0) goto error;

    // for LTE
    ril_data_urc_status = convertRegState(response[0], false);

    // get <lac>
    response[1] = line->atTokNexthexint(&err);
    if (err >= 0)
    {
        // get <ci>
        response[2] = line->atTokNexthexint(&err);
        if (err >= 0)
        {
            //get <Act>
            response[3] = line->atTokNextint(&err);
            ril_data_urc_rat = response[3];
        }
    }

    stat[0] = combineWfcEgregState();
    if (ril_wfc_reg_status[m_slot_id] == 1) {
        stat[1] = 18;
    } else {
        stat[1] = convertPSNetworkType(response[3]);
    }

    sendEvent(RFX_MSG_EVENT_PS_NETWORK_STATE, RfxIntsData(stat, 2),
        RIL_CMD_PROXY_3, m_slot_id);

    urc = RfxMclMessage::obtainUrc(RFX_MSG_URC_RESPONSE_CELLULAR_DATA_REG_STATE, m_slot_id,
            RfxIntsData(&ril_data_urc_status, 1));
    responseToTelCore(urc);

    return;

error:
    logE(LOG_TAG, "There is something wrong with the URC");
}

void RmcNetworkUrcHandler::handleOtaProvisionStatus(const sp<RfxMclMessage>& msg) {
    // URC ^OTACMSG:<status>
    // <status>: integer type
    //    1 programming started
    //    2 service programming lock unlocked
    //    3 NAM parameters downloaded successfully
    //    4 MDN downloaded successfully
    //    5 IMSI downloaded successfully
    //    6 PRL downloaded successfully
    //    7 commit successfully
    //    8 programming successfully
    //    9 programming unsuccessfully
    //    10 verify SPC failed
    //    11 a key exchanged
    //    12 SSD updated
    //    13 OTAPA started
    //    14 OTAPA stopped

    int err;
    int otaState;
    int convertState;
    RfxAtLine *line = msg->getRawUrc();
    sp<RfxMclMessage> urc;

    // go to start position
    line->atTokStart(&err);
    if (err < 0) goto error;

    otaState = line->atTokNextint(&err);
    if (err < 0) goto error;

    // for OTA STATUS
    getMclStatusManager()->setIntValue(RFX_STATUS_KEY_OTA_STATUS, otaState);

    // for OTA URC
    convertState = convertOtaProvisionStatus(otaState);
    if (convertState >= 0) {
        urc = RfxMclMessage::obtainUrc(RFX_MSG_URC_CDMA_OTA_PROVISION_STATUS, m_slot_id,
                RfxIntsData(&convertState, 1));
        responseToTelCore(urc);
    }

    return;

error:
    logE(LOG_TAG, "handle OTA error: %s", msg->getRawUrc()->getLine());
}

int RmcNetworkUrcHandler::convertOtaProvisionStatus(int rawState) {
    int state;
    switch (rawState) {
        case 2: {  // service programming lock unlocked
            state = CDMA_OTA_PROVISION_STATUS_SPL_UNLOCKED;
            break;
        }
        case 3: {  // NAM parameters downloaded successfully
            state = CDMA_OTA_PROVISION_STATUS_NAM_DOWNLOADED;
            break;
        }
        case 4: {  // MDN downloaded successfully
            state = CDMA_OTA_PROVISION_STATUS_MDN_DOWNLOADED;
            break;
        }
        case 5: {  // IMSI downloaded successfully
            state = CDMA_OTA_PROVISION_STATUS_IMSI_DOWNLOADED;
            break;
        }
        case 6: {  // PRL downloaded successfully
            state = CDMA_OTA_PROVISION_STATUS_PRL_DOWNLOADED;
            break;
        }
        case 7: {  // commit successfully
            state = CDMA_OTA_PROVISION_STATUS_COMMITTED;
            break;
        }
        case 10: {  // verify SPC failed
            state = CDMA_OTA_PROVISION_STATUS_SPC_RETRIES_EXCEEDED;
            break;
        }
        case 11: {  // A key Exchanged
            state = CDMA_OTA_PROVISION_STATUS_A_KEY_EXCHANGED;
            break;
        }
        case 12: {  // SSD updated
            state = CDMA_OTA_PROVISION_STATUS_SSD_UPDATED;
            break;
        }
        case 13: {  // OTAPA strated
            state = CDMA_OTA_PROVISION_STATUS_OTAPA_STARTED;
            break;
        }
        case 14: {  // OTAPA stopped
            state = CDMA_OTA_PROVISION_STATUS_OTAPA_STOPPED;
            break;
        }
        default: {
            state = -1;
            break;
        }
    }
    return state;
}

void RmcNetworkUrcHandler::handleConfirmRatBegin(const sp<RfxMclMessage>& msg) {
    RFX_UNUSED(msg);
    sendEvent(RFX_MSG_EVENT_CONFIRM_RAT_BEGIN, RfxVoidData(),
            m_channel_id, m_slot_id);
}

void RmcNetworkUrcHandler::handleGmssRatChanged(const sp<RfxMclMessage>& msg) {
    int i = 0;
    int err = 0;
    int data[5] = { 0 };
    RfxAtLine *line = msg->getRawUrc();

    // go to start position
    line->atTokStart(&err);
    if (err < 0) {
        logE(LOG_TAG, "handle GMSS RAT Changed error: %s", msg->getRawUrc()->getLine());
        return;
    }

    for (i = 0; i < 5; i++) {
        if (i == 1) {  // MCC
            data[i] = atoi(line->atTokNextstr(&err));
        } else {
            data[i] = line->atTokNextint(&err);
        }
        if (err < 0) {
            logE(LOG_TAG, "handle GMSS RAT Changed, i: %d", i);
            return;
        }
    }

    sp<RfxMclMessage> urc = RfxMclMessage::obtainUrc(RFX_MSG_URC_GMSS_RAT_CHANGED,
            m_slot_id, RfxIntsData(data, 5));
    responseToTelCore(urc);
}

void RmcNetworkUrcHandler::updatePrlInfo(int system, int mode) {
    RFX_LOG_V(LOG_TAG, "updatePrlInfo() system = %d, mode = %d", system, mode);
    if ((system == 0) && (mode < 255)) {
        pthread_mutex_lock(&s_voiceRegStateMutex[m_slot_id]);
        voice_reg_state_cache[m_slot_id].is_in_prl = mode;
        pthread_mutex_unlock(&s_voiceRegStateMutex[m_slot_id]);
    } else {
        // logD(LOG_TAG, "updatePrlInfo() unsupport system = %d", system);
    }
}

void RmcNetworkUrcHandler::handleSystemInPrlIndication(const sp<RfxMclMessage>& msg) {
    // +EIPRL:<system1>,<mode> [,<system2>,<mode>]
    // <system> : integer type; system type
    // 0    1xRTT
    // 1    EvDO
    // <mode>:integer type
    // 0    The system is not in PRL
    // 1    The system is in PRL
    // 255  Invalid value(used in network lost)

    int err;
    RfxAtLine* line;
    int system = -1;
    int mode = 255;

    line = msg->getRawUrc();

    // go to start position
    line->atTokStart(&err);
    if (err < 0) return;

    system = line->atTokNextint(&err);
    if (err < 0) return;

    mode = line->atTokNextint(&err);
    if (err < 0) return;

    updatePrlInfo(system, mode);

    if (line->atTokHasmore()) {
        system = line->atTokNextint(&err);
        if (err < 0) return;

        mode = line->atTokNextint(&err);
        if (err < 0) return;

        updatePrlInfo(system, mode);
    }

    sp<RfxMclMessage> urc = RfxMclMessage::obtainUrc(
            RFX_MSG_URC_RESPONSE_VOICE_NETWORK_STATE_CHANGED, m_slot_id, RfxVoidData());
    responseToTelCore(urc);
    return;
}

void RmcNetworkUrcHandler::handleDefaultRoamingIndicator(const sp<RfxMclMessage>& msg) {
    int err;
    RfxAtLine* line = msg->getRawUrc();

    // go to start position
    line->atTokStart(&err);
    if (err < 0) return;
    pthread_mutex_lock(&s_voiceRegStateMutex[m_slot_id]);
    voice_reg_state_cache[m_slot_id].default_roaming_indicator = line->atTokNextint(&err);
    pthread_mutex_unlock(&s_voiceRegStateMutex[m_slot_id]);
    if (err < 0) return;

    sp<RfxMclMessage> urc = RfxMclMessage::obtainUrc(
            RFX_MSG_URC_RESPONSE_VOICE_NETWORK_STATE_CHANGED, m_slot_id, RfxVoidData());
    responseToTelCore(urc);
    return;
}

void RmcNetworkUrcHandler::handleNeighboringCellInfo(const sp<RfxMclMessage>& msg) {
    /* +ECELLINFO: <valid>,<rat>,<cell_info> */
    int err;
    int valid, rat;
    char *responseStr[2];
    RfxAtLine *line = msg->getRawUrc();

    line->atTokStart(&err);
    if (err < 0) return;

    valid = line->atTokNextint(&err);
    if (err < 0) return;

    if (valid != 1) {
        logD(LOG_TAG, "Reveive invalid +ECELLINFO");
        return;
    }

    responseStr[0] = line->atTokNextstr(&err);

    if (strcmp(responseStr[0], "1") &&
            strcmp(responseStr[0], "2")) {
        logD(LOG_TAG, "Abnormal RAT");
        return;
    }

    // get raw data of of Neighbor cell info
    responseStr[1] = line->atTokNextstr(&err);
    if ( err < 0 ) return;

    logD(LOG_TAG, "NbrCellInfo: %s, len:%d ,%s", responseStr[0], strlen(responseStr[1]), responseStr[1]);

    sp<RfxMclMessage> urc = RfxMclMessage::obtainUrc(RFX_MSG_URC_NEIGHBORING_CELL_INFO,
            m_slot_id, RfxStringsData(responseStr, 2));
    responseToTelCore(urc);
}

void RmcNetworkUrcHandler::handleNetworkInfo(const sp<RfxMclMessage>& msg) {
    /* +ENWINFO: <type>,<nw_info> */
    int err;
    int type;
    char *responseStr[2];
    RfxAtLine *line = msg->getRawUrc();
    sp<RfxMclMessage> urc;

    line->atTokStart(&err);
    if (err < 0) goto error;

    type = line->atTokNextint(&err);
    if (err < 0) goto error;

    asprintf(&responseStr[0], "%d", type);

    // get raw data of structure of NW info
    responseStr[1] = line->atTokNextstr(&err);
    if (err < 0) goto error;

    logD(LOG_TAG, "NWInfo: %s, len:%d ,%s", responseStr[0], strlen(responseStr[1]),
            responseStr[1]);

    urc = RfxMclMessage::obtainUrc(RFX_MSG_URC_NETWORK_INFO,
            m_slot_id, RfxStringsData(responseStr, 2));
    responseToTelCore(urc);
    free(responseStr[0]);
    return;

error:
    logE(LOG_TAG, "handle Network Info error: %s", msg->getRawUrc()->getLine());
}

void RmcNetworkUrcHandler::handlePseudoCellInfo(const sp<RfxMclMessage>& msg) {
    /*
     * 1. URC when EAPC mode is 1
     *	  +EAPC: <num>,<type>,<plmn>,<lac>,<cid>,<arfcn>,<bsic>[
     *                ,<type>,<plmn>,<lac>,<cid>,<arfcn>,<bsic>]
     *	   num: 1 or 2
     *   type = 1: detected pseudo cells info
     *
     * 2. URC when EAPC mode is 2
     *  +EAPC: <num>,<type>[,<plmn>,<lac>,<cid>,<arfcn>,<bsic>]
     *   num: 1
     *   type = 2: In to a pseudo cell
     *   type = 3: Out of a pseudo cell
     */
    int err;
    int num;
    int response[13] = {0};
    RfxAtLine *line = msg->getRawUrc();
    sp<RfxMclMessage> urc;

    line->atTokStart(&err);
    if (err < 0) goto error;

    num = line->atTokNextint(&err);
    if (err < 0) goto error;

    response[0] = num;
    if (num != 1 && num != 2) goto error;

    // get raw data of structure of pseudo cell info
    for (int i = 0; i < num; i++) {
        // type
        response[i*6 + 1] = line->atTokNextint(&err);
        if (err < 0) goto error;

        if (response[i*6 + 1] == 2) {
            logD(LOG_TAG, "PseudoCellInfo: attached to a pseudo cell");
        } else if (response[i*6 + 1] == 3) {
            logD(LOG_TAG, "PseudoCellInfo: attached cell changed and is not a pseudo cell");
        }

        if (line->atTokHasmore()) {
            // plmn
            response[i*6 + 2] = line->atTokNextint(&err);
            if (err < 0) goto error;
            // lac
            response[i*6 + 3] = line->atTokNextint(&err);
            if (err < 0) goto error;
            // cid
            response[i*6 + 4] = line->atTokNextint(&err);
            if (err < 0) goto error;
            // arfcn
            response[i*6 + 5] = line->atTokNextint(&err);
            if (err < 0) goto error;
            // bsic
            response[i*6 + 6] = line->atTokNextint(&err);
            if (err < 0) goto error;
        }
    }

    logD(LOG_TAG, "PseudoCellInfo: %s", response);
    urc = RfxMclMessage::obtainUrc(RFX_MSG_URC_PSEUDO_CELL_INFO,
            m_slot_id, RfxIntsData(response, 13));
    responseToTelCore(urc);
    return;

error:
    logE(LOG_TAG, "handle Pseudo Cell Info error: %s", msg->getRawUrc()->getLine());
}

void RmcNetworkUrcHandler::handleGsmFemtoCellInfo(const sp<RfxMclMessage>& msg) {
    int err = 0;
    char *responseStr = NULL;
    // Use INT_MAX: 0x7FFFFFFF denotes invalid value
    int INVALID = 0x7FFFFFFF;
    RfxAtLine *line = msg->getRawUrc();
    int is_csg_cell = 0;

    /* +ECSG:  <domain>,<state>,<plmn_id>,<act>,<is_csg_cell>,<csg_id>,<csg_icon_type>,<hnb_name>,<cause> */
    pthread_mutex_lock(&ril_nw_femtoCell_mutex);
    line->atTokStart(&err);
    if (err < 0) goto error;

    // domain
    m_femto_cell_cache.domain = line->atTokNextint(&err);
    if (err < 0) goto error;

    // state
    m_femto_cell_cache.state = line->atTokNextint(&err);
    if (err < 0) goto error;

    // plmn
    m_femto_cell_cache.plmn_id = line->atTokNextint(&err);
    if (err < 0) goto error;

    // act
    m_femto_cell_cache.act = line->atTokNextint(&err);
    if (err < 0) goto error;

    // is CSG
    is_csg_cell = line->atTokNextint(&err);
    if (err < 0) goto error;

    if (is_csg_cell != m_femto_cell_cache.is_csg_cell) {
        m_femto_cell_cache.is_csg_cell = is_csg_cell;
        if (m_femto_cell_cache.is_csg_cell == 1) {
            // CSG id
            m_femto_cell_cache.csg_id = line->atTokNextint(&err);
            if (err < 0) goto error;

            // CSG icon type
            m_femto_cell_cache.csg_icon_type = line->atTokNextint(&err);
            if (err < 0) goto error;

            // hnb name
            responseStr = line->atTokNextstr(&err);
            if (err < 0) goto error;

            strncpy(m_femto_cell_cache.hnbName, responseStr, strlen(responseStr));
        } else {
            /* <csg_id>,<csg_icon_type>,<hnb_name> are only avaliable when <is_csg_cell> is 1. */
            m_femto_cell_cache.csg_id = INVALID;
            m_femto_cell_cache.csg_icon_type = INVALID;
            sprintf(m_femto_cell_cache.hnbName, "%d", INVALID);
        }

        // cause
        m_femto_cell_cache.cause = line->atTokNextint(&err);
        if (err < 0) goto error;

        // trigger event
        sendEvent(RFX_MSG_EVENT_FEMTOCELL_UPDATE, RfxVoidData(),
                m_channel_id, m_slot_id);
    }
    pthread_mutex_unlock(&ril_nw_femtoCell_mutex);
    return;

error:
    pthread_mutex_unlock(&ril_nw_femtoCell_mutex);
    logE(LOG_TAG, "There is something wrong with the onFemtoCellInfo URC, err=%d", err);
}

void RmcNetworkUrcHandler::handleCdmaFemtoCellInfo(const sp<RfxMclMessage>& msg) {
    // +EFCELL:<n>,<system1>,<state>,<system2>,<state>
    // system: 0: 1xRTT, 1: EVDO
    // state:  0: not femtocell, 1: femtorcell
    int skip;
    int err;
    int system, c2kfemto, evdofemto, is_femtocell = 0;
    RfxAtLine *line = msg->getRawUrc();

    line->atTokStart(&err);
    if (err < 0) goto error;

    skip = line->atTokNextint(&err);
    if (err < 0) goto error;

    system = line->atTokNextint(&err);
    if (err < 0) goto error;

    c2kfemto = line->atTokNextint(&err);
    if (err < 0) goto error;

    system = line->atTokNextint(&err);
    if (err < 0) goto error;

    evdofemto = line->atTokNextint(&err);
    if (err < 0) goto error;

    if (c2kfemto == 1 || evdofemto == 1) {
        is_femtocell = 2;
    }

    pthread_mutex_lock(&ril_nw_femtoCell_mutex);
    if (is_femtocell != m_femto_cell_cache.is_femtocell) {
        memset(&m_femto_cell_cache, 0, sizeof(RIL_FEMTO_CELL_CACHE));
        m_femto_cell_cache.is_femtocell = is_femtocell;
        // trigger event
        sendEvent(RFX_MSG_EVENT_FEMTOCELL_UPDATE, RfxVoidData(),
                m_channel_id, m_slot_id);
    }
    pthread_mutex_unlock(&ril_nw_femtoCell_mutex);
    return;

error:
    logE(LOG_TAG, "handle Cdma femtocell error: %s", msg->getRawUrc()->getLine());
}

void RmcNetworkUrcHandler::handleCellInfoList(const sp<RfxMclMessage>& msg) {
    int err = 0;
    int num = 0;
    RIL_CellInfo_v12 * response = NULL;
    RfxAtLine *line = msg->getRawUrc();
    sp<RfxMclMessage> urc;
    // +ECELL: <num_of_cell>...
    line->atTokStart(&err);
    if (err < 0) goto error;

    num = line->atTokNextint(&err);
    if (err < 0) goto error;

    if (num < 1) {
            logE(LOG_TAG, "No cell info listed, num=%d", num);
            goto error;
    }

    logD(LOG_TAG, "Cell Info listed, number =%d", num);
    response = (RIL_CellInfo_v12 *) alloca(num * sizeof(RIL_CellInfo_v12));
    memset(response, 0, num * sizeof(RIL_CellInfo_v12));
    err = getCellInfoListV12(line, num, response);
    if (err < 0) goto error;

    urc = RfxMclMessage::obtainUrc(RFX_MSG_URC_CELL_INFO_LIST,
            m_slot_id, RfxCellInfoData(response, num * sizeof(RIL_CellInfo_v12)));
    responseToTelCore(urc);
    return;

error:
    logE(LOG_TAG, "onCellInfoList parse error");
}

void RmcNetworkUrcHandler::handleNitzTzReceived(const sp<RfxMclMessage>& msg) {
    int err;
    char *utct;
    char *tz;
    int dst = 0;
    RfxAtLine *line = msg->getRawUrc();
    sp<RfxMclMessage> urc;

    char *nitz_string;
    time_t calendar_time;
    struct tm *t_info = NULL;

    /*+CTZEU: <tz>,<dst>,[<utime>].
    <utime>: string type value representing the universal time. The format is "YYYY/MM/DD,hh:mm:ss".
    <tz>: string type value representing the sum of the local time zone *NOT* plus dst.
          e.g. "-09", "+00" and "+09".
    <dst>: integer type value indicating whether <tz> includes daylight savings adjustment;*/

    /* Final format :  "yy/mm/dd,hh:mm:ss(+/-)tz[,dt]" */

    line->atTokStart(&err);
    if (err < 0) goto error;

    // <tz>
    tz = line->atTokNextstr(&err);
    if (err < 0) {
        logE(LOG_TAG, "There is no valid <tz>");
        goto error;
    }
    else {
        bNitzTzAvailble = 1;
        strncpy(ril_nw_nitz_tz, tz, MAX_NITZ_TZ_DST_LENGTH-1);
    }
    // <dst>
    dst = line->atTokNextint(&err);
    if (err < 0) {
        logE(LOG_TAG, "There is no valid <dst>");
        goto error;
    }
    else {
        bNitzDstAvailble = 1;
        snprintf(ril_nw_nitz_dst, MAX_NITZ_TZ_DST_LENGTH, "%d", dst);
    }
    // <time>
    if (line->atTokHasmore()) {
        utct = line->atTokNextstr(&err);
        if (err < 0) goto error;

        struct tm utc_tm;
        memset(&utc_tm, 0, sizeof(struct tm));
        strptime(utct, "%Y/%m/%d,%H:%M:%S", &utc_tm);

        // "yy/mm/dd,hh:mm:ss(+/-)tz,dst"
        asprintf(&nitz_string, "%02d/%02d/%02d,%02d:%02d:%02d%s,%d",
                (utc_tm.tm_year)%100,
                utc_tm.tm_mon+1,
                utc_tm.tm_mday,
                utc_tm.tm_hour,
                utc_tm.tm_min,
                utc_tm.tm_sec,
                tz,
                dst);
        RFX_LOG_V(LOG_TAG, "NITZ:%s", nitz_string);
    } else {
        // get the system time to fullfit the NITZ string format
        calendar_time = time(NULL);
        if (-1 == calendar_time) return;

        t_info = gmtime(&calendar_time);
        if (NULL == t_info) return;

        // "yy/mm/dd,hh:mm:ss(+/-)tz,dst"
        asprintf(&nitz_string, "%02d/%02d/%02d,%02d:%02d:%02d%s,%d",
                (t_info->tm_year)%100,
                t_info->tm_mon+1,
                t_info->tm_mday,
                t_info->tm_hour,
                t_info->tm_min,
                t_info->tm_sec,
                tz,
                dst);
        RFX_LOG_V(LOG_TAG, "NITZ:%s", nitz_string);
    }
    // ignore local time information in the EMM INFORMATION if SIB16 is broadcast by the network
    if (!bSIB16Received) {
        urc = RfxMclMessage::obtainUrc(RFX_MSG_URC_NITZ_TIME_RECEIVED, m_slot_id,
                RfxStringData(nitz_string));
        responseToTelCore(urc);
    }
    free(nitz_string);
    return;

error:
    logE(LOG_TAG, "handle NITZ error: %s", msg->getRawUrc()->getLine());
    bNitzTzAvailble = 0;
    bNitzDstAvailble = 0;
}

void RmcNetworkUrcHandler::handleNitzOperNameReceived(const sp<RfxMclMessage>& msg) {
    int err;
    int length, i, id;
    char *oper_code;
    char *oper_lname;
    char *oper_sname;
    char *str;
    int is_lname_hex_str = 0;
    int is_sname_hex_str = 0;
    char temp_oper_name[MAX_OPER_NAME_LENGTH]={0};
    sp<RfxMclMessage> urc;

    /* +CIEV: 10,"PLMN","long_name","short_name" */
    RfxAtLine *line = msg->getRawUrc();

    line->atTokStart(&err);
    if (err < 0) goto error;

    id = line->atTokNextint(&err);
    if (err < 0 || id != 10) return;

    if (!line->atTokHasmore()) {
        logE(LOG_TAG, "There is no NITZ data");
        return;
    }

    oper_code  = m_ril_nw_nitz_oper_code;
    oper_lname = m_ril_nw_nitz_oper_lname;
    oper_sname = m_ril_nw_nitz_oper_sname;

    /* FIXME: it is more good to transfer the OPERATOR NAME to the Telephony Framework directly */
    pthread_mutex_lock(&ril_nw_nitzName_mutex);
    // logD(LOG_TAG, "Get ril_nw_nitzName_mutex in the onNitzOperNameReceived");

    str = line->atTokNextstr(&err);
    if (err < 0) goto error;
    strncpy(oper_code, str, MAX_OPER_NAME_LENGTH);
    oper_code[MAX_OPER_NAME_LENGTH-1] = '\0';

    str = line->atTokNextstr(&err);
    if (err < 0) goto error;
    strncpy(oper_lname, str, MAX_OPER_NAME_LENGTH);
    oper_lname[MAX_OPER_NAME_LENGTH-1] = '\0';

    str = line->atTokNextstr(&err);
    if (err < 0) goto error;
    strncpy(oper_sname, str, MAX_OPER_NAME_LENGTH);
    oper_sname[MAX_OPER_NAME_LENGTH-1] = '\0';

    /* ALPS00459516 start */
    if ((strlen(oper_lname)%8) == 0) {
        logD(LOG_TAG, "strlen(oper_lname)=%d", strlen(oper_lname));

        length = strlen(oper_lname);
        if (oper_lname[length-1] == '@') {
            oper_lname[length-1] = '\0';
            logD(LOG_TAG, "remove @ new oper_lname:%s", oper_lname);
        }
    }

    if ((strlen(oper_sname)%8) == 0) {
        logD(LOG_TAG, "strlen(oper_sname)=%d", strlen(oper_sname));

        length = strlen(oper_sname);
        if (oper_sname[length-1] == '@') {
            oper_sname[length-1] = '\0';
            logD(LOG_TAG, "remove @ new oper_sname:%s", oper_sname);
        }
    }
    /* ALPS00459516 end */

    /* ALPS00262905 start
       +CIEV: 10, <plmn_str>,<full_name_str>,<short_name_str>,<is_full_name_hex_str>,<is_short_name_hex_str> for UCS2 string */
    is_lname_hex_str = line->atTokNextint(&err);
    if (err >= 0) {
        // logD(LOG_TAG, "is_lname_hex_str=%d", is_lname_hex_str);
        if (is_lname_hex_str == 1) {
            /* ALPS00273663 Add specific prefix "uCs2" to identify this operator name is UCS2 format.
                   prefix + hex string ex: "uCs2806F767C79D1"  */
            memset(temp_oper_name, 0, sizeof(temp_oper_name));
            strncpy(temp_oper_name, "uCs2", 4);
            strncpy(&(temp_oper_name[4]), oper_lname, MAX_OPER_NAME_LENGTH-4);
            memset(oper_lname, 0, MAX_OPER_NAME_LENGTH);
            strncpy(oper_lname, temp_oper_name, MAX_OPER_NAME_LENGTH-1);
            logD(LOG_TAG, "lname add prefix uCs2");
        } else {
            convertToUtf8String(oper_lname);
        }

        is_sname_hex_str = line->atTokNextint(&err);
        // logD(LOG_TAG, "is_sname_hex_str=%d", is_sname_hex_str);
        if ((err >= 0) && (is_sname_hex_str == 1)) {
            /* ALPS00273663 Add specific prefix "uCs2" to identify this operator name is UCS2 format.
                   prefix + hex string ex: "uCs2806F767C79D1"  */
            memset(temp_oper_name, 0, sizeof(temp_oper_name));
            strncpy(temp_oper_name, "uCs2", 4);
            strncpy(&(temp_oper_name[4]), oper_sname, MAX_OPER_NAME_LENGTH-4);
            memset(oper_sname, 0, MAX_OPER_NAME_LENGTH);
            strncpy(oper_sname, temp_oper_name, MAX_OPER_NAME_LENGTH-1);
            logD(LOG_TAG, "sname Add prefix uCs2");
        } else {
            convertToUtf8String(oper_sname);
        }
    } else {
        convertToUtf8String(oper_lname);
        convertToUtf8String(oper_sname);
    }
    /* ALPS00262905 end */

    logD(LOG_TAG, "Get NITZ Operator Name of RIL %d: %s %s %s", m_slot_id+1, oper_code, oper_lname, oper_sname);
    if (m_slot_id >= 0) {
        setMSimProperty(m_slot_id, (char *)PROPERTY_NITZ_OPER_CODE, oper_code);
        setMSimProperty(m_slot_id, (char *)PROPERTY_NITZ_OPER_LNAME, oper_lname);
        setMSimProperty(m_slot_id, (char *)PROPERTY_NITZ_OPER_SNAME, oper_sname);
    }

    urc = RfxMclMessage::obtainUrc(RFX_MSG_URC_RESPONSE_VOICE_NETWORK_STATE_CHANGED, m_slot_id, RfxVoidData());
    responseToTelCore(urc);
error:
    pthread_mutex_unlock(&ril_nw_nitzName_mutex);
    return;
}

void RmcNetworkUrcHandler::handleSib16TimeInfoReceived(const sp<RfxMclMessage>& msg) {
    int err = 0;
    int id = 0;
    int dst = 0;
    int ls = 0;
    int lto = 0;
    int dt = 0;
    long long raw_utc = 0;

    // difference in milliseconds of epochs used by Android (1970) and the network (1900)
    long long epochDiffInMillis = 2208988800000;

    // abs_time = <raw_utc>*10 - <epochDiffInMillis>
    long long abs_time = 0;

    // currentUtcTimeMillis = raw_utc*10 - epochDiffInMillis + elapsedTimeSinceBroadcast
    time_t  currentUtcTimeMillis;  // time_t is measured in seconds.
    struct tm *ts;

    char* responseStr[5];
    char nitz_string[30];
    char sib16_time_string[70];
    int i;

    /* +CIEV: 11, <UTC>, [<daylightSavingTime >], [<leapSeconds >], [<localTimeOffset >], <delayTicks>
      <UTC>: The field counts the number of UTC seconds in 10 ms units since 00:00:00 on Gregorian calendar date 1 January, 1900
      <daylightSavingTime>: It indicates if and how daylight saving time (DST) is applied to obtain the local time.
      <leapSeconds>: GPS time - leapSeconds = UTC time.
      <localTimeOffset>: Offset between UTC and local time in units of 15 minutes.
      <delayTicks>: Time difference from AS receive SIB16 to L4 receive notify

      Final format :  "yy/mm/dd,hh:mm:ss(+/-)tz[,dst]" */

    /* +CIEV: 10,"PLMN","long_name","short_name" */
    sp<RfxMclMessage> urc;
    RfxAtLine *line = msg->getRawUrc();

    line->atTokStart(&err);
    if (err < 0) return;

    id = line->atTokNextint(&err);
    if (err < 0 || id != 11) return;

    if (!line->atTokHasmore()) {
        logE(LOG_TAG, "There is no SIB16 time info");
        return;
    }

    // get <UTC>
    raw_utc = line->atTokNextlonglong(&err);
    if (err < 0) {
        logE(LOG_TAG, "ERROR occurs when parsing <UTC> of the SIB16 time info URC");
        return;
    }
    asprintf(&responseStr[0], "%lld", raw_utc);

    // get <daylightSavingTime >
    dst = line->atTokNextint(&err);
    if (err < 0) {
        if (bNitzDstAvailble) {
            logE(LOG_TAG, "Use <daylightSavingTime> from (E)MM INFORMATION");
            asprintf(&responseStr[1], "%s", ril_nw_nitz_dst);
        }
        else {
            logE(LOG_TAG, "ERROR occurs when parsing <daylightSavingTime> of the SIB16 time info URC");
            responseStr[1] = (char*)"";
            dst = -1;
        }
    } else {
        asprintf(&responseStr[1], "%d", dst);
    }

    // get <leapSeconds >
    ls = line->atTokNextint(&err);
    if (err < 0) {
        logE(LOG_TAG, "ERROR occurs when parsing <leapSeconds> of the SIB16 time info URC");
        responseStr[2] = (char*)"";
    } else {
        asprintf(&responseStr[2], "%d", ls);
    }

    // get <localTimeOffset >
    lto = line->atTokNextint(&err);
    if (err < 0) {
        if (bNitzTzAvailble) {
            logE(LOG_TAG, "Use <localTimeOffset> from (E)MM INFORMATION");
            asprintf(&responseStr[3], "%s", ril_nw_nitz_tz);
        }
        else {
            logE(LOG_TAG, "ERROR occurs when parsing <localTimeOffset> of the SIB16 time info URC");
            asprintf(&responseStr[3], "%s", "+00");
        }
    } else {
        if (lto >= 0) {
            asprintf(&responseStr[3], "+%d", lto);
        } else {
            asprintf(&responseStr[3], "%d", lto);
        }
    }

    // get <delayTicks >
    dt = line->atTokNextint(&err);
    if (err < 0) {
        logD(LOG_TAG, "ERROR occurs when parsing <delayTicks> of the SIB16 time info URC");
        return;
    }
    asprintf(&responseStr[4], "%d", dt);

    // logD(LOG_TAG, "SIB16 time info: UTC %s,daylightSavingTime %s,leapSeconds %s,localTimeOffset %s,delayTicks %s",
    //        responseStr[0], responseStr[1], responseStr[2], responseStr[3], responseStr[4]);

    abs_time = (raw_utc * 10) - epochDiffInMillis;

    currentUtcTimeMillis = ((raw_utc * 10) - epochDiffInMillis + dt)/1000;
    logD(LOG_TAG, "currentUtcTimeMillis: %s", ctime(&currentUtcTimeMillis));

    ts = gmtime(&currentUtcTimeMillis);
    if (NULL == ts) return;

    memset(nitz_string, 0, sizeof(nitz_string));
    memset(sib16_time_string, 0, sizeof(sib16_time_string));
    if (dst != -1) {
        // nitz_string with dst
        sprintf(nitz_string, "%02d/%02d/%02d,%02d:%02d:%02d%s,%s",  // "yy/mm/dd,hh:mm:ss(+/-)tz[,dst]"
                (ts->tm_year)%100,
                ts->tm_mon+1,
                ts->tm_mday,
                ts->tm_hour,
                ts->tm_min,
                ts->tm_sec,
                responseStr[3],
                responseStr[1]);

        // sib16_time_string with dst
        // "yy/mm/dd,hh:mm:ss(+/-)tz,dst,abs_time"
        sprintf(sib16_time_string, "%02d/%02d/%02d,%02d:%02d:%02d%s,%s,%lli",
                (ts->tm_year)%100,
                ts->tm_mon+1,
                ts->tm_mday,
                ts->tm_hour,
                ts->tm_min,
                ts->tm_sec,
                responseStr[3],
                responseStr[1],
                abs_time);
    } else {
        // nitz_srting without dst
        sprintf(nitz_string, "%02d/%02d/%02d,%02d:%02d:%02d%s",  // "yy/mm/dd,hh:mm:ss(+/-)tz"
                (ts->tm_year)%100,
                ts->tm_mon+1,
                ts->tm_mday,
                ts->tm_hour,
                ts->tm_min,
                ts->tm_sec,
                responseStr[3]);
        // sib16_time_string with dst=0
        // "yy/mm/dd,hh:mm:ss(+/-)tz,dst,abs_time"
        sprintf(sib16_time_string, "%02d/%02d/%02d,%02d:%02d:%02d%s,0,%lli",  // "yy/mm/dd,hh:mm:ss(+/-)tz"
                (ts->tm_year)%100,
                ts->tm_mon+1,
                ts->tm_mday,
                ts->tm_hour,
                ts->tm_min,
                ts->tm_sec,
                responseStr[3],
                abs_time);
    }
    logD(LOG_TAG, "NITZ:%s, SIB16_Time:%s", nitz_string, sib16_time_string);

    bSIB16Received = 1;
    urc = RfxMclMessage::obtainUrc(RFX_MSG_URC_NITZ_TIME_RECEIVED, m_slot_id, RfxStringData(nitz_string));
    responseToTelCore(urc);

    for (i = 0; i < 5; i++) {
        if ((responseStr[i] != NULL) && strcmp(responseStr[i], "")) {
            free(responseStr[i]);
        }
    }
}

void RmcNetworkUrcHandler::handleNetworkEventReceived(const sp<RfxMclMessage>& msg) {
    /* +EREGINFO: <Act>,<event_type>
     * <Act>: Access technology (RAT)
     * <event_type>: 0: for RAU event
     *               1: for TAU event
     */
    int err;
    int response[2] = {0};
    sp<RfxMclMessage> urc;
    RfxAtLine *line = msg->getRawUrc();

    line->atTokStart(&err);
    if (err < 0) goto error;

    // get <Act>
    response[0] = line->atTokNextint(&err);
    if (err < 0) goto error;

    // get <event_type>
    response[1] = line->atTokNextint(&err);
    if (err < 0) goto error;

    logD(LOG_TAG, "onNetworkEventReceived: <Act>:%d, <event_typ>:%d", response[0], response[1]);
    urc = RfxMclMessage::obtainUrc(RFX_MSG_URC_NETWORK_EVENT, m_slot_id, RfxIntsData(response, 2));
    responseToTelCore(urc);
    return;

error:
    logE(LOG_TAG, "There is something wrong with the URC +EREGINFO");
}

void RmcNetworkUrcHandler::handleMMRRStatusChanged(const sp<RfxMclMessage>& msg) {
    int err = 0;
    int status = 0;
    sp<RfxMclMessage> urc;
    RfxAtLine *line = msg->getRawUrc();

    line->atTokStart(&err);
    if (err < 0) goto error;

    status = line->atTokNextint(&err);
    if (err < 0) goto error;

    logD(LOG_TAG, "onMMRRStatusChanged= %d", status);
    urc = RfxMclMessage::obtainUrc(RFX_MSG_URC_RESPONSE_MMRR_STATUS_CHANGED, m_slot_id, RfxIntsData(&status, 1));
    responseToTelCore(urc);
    return;

error:
    logE(LOG_TAG, "There is something wrong with the URC");
}

void RmcNetworkUrcHandler::handleWfcStateChanged(const sp<RfxMclMessage>& msg) {
    int err = 0;
    int stat[2];
    int status = 0;
    sp<RfxMclMessage> urc;
    RfxAtLine *line = msg->getRawUrc();

    line->atTokStart(&err);
    if (err < 0) goto error;

    status = line->atTokNextint(&err);
    if (err < 0) goto error;

    // logD(LOG_TAG, "handleWfcStateChanged: <status>:%d", status);

    if (status == 0 || status == 1) {
        updateWfcState(status);
        status = combineWfcEgregState();
        stat[0] = status;
    } else {
        // only 0 and 1 are valid values
        goto error;
    }

    if (ril_wfc_reg_status[m_slot_id] == 1) {
        stat[1] = 18;
    } else {
        stat[1] = convertPSNetworkType(ril_data_urc_rat);
    }

    sendEvent(RFX_MSG_EVENT_PS_NETWORK_STATE, RfxIntsData(stat, 2),
        RIL_CMD_PROXY_3, m_slot_id);
    RFX_LOG_V(LOG_TAG, "Send RFX_MSG_EVENT_PS_NETWORK_STATE");

    return;

error:
    logE(LOG_TAG, "There is something wrong with the URC");
}

void RmcNetworkUrcHandler::handleACMT(const sp<RfxMclMessage>& msg) {
    /* +EACMT: <error_type>,<cause> */
    int err;
    int response[2] = {0};
    sp<RfxMclMessage> urc;
    RfxAtLine *line = msg->getRawUrc();

    line->atTokStart(&err);
    if (err < 0) goto error;

    // get <error_type>
    response[0] = line->atTokNextint(&err);
    if (err < 0) goto error;

    // get <cause>
    response[1] = line->atTokNextint(&err);
    if (err < 0) goto error;

    logD(LOG_TAG, "handleACMT: <error_type>:%d, <cause>:%d", response[0], response[1]);
    urc = RfxMclMessage::obtainUrc(RFX_MSG_URC_RESPONSE_ACMT, m_slot_id, RfxIntsData(response, 2));
    responseToTelCore(urc);
    return;

error:
    logE(LOG_TAG, "There is something wrong with the URC +EACMT");
}

void RmcNetworkUrcHandler::handleModulationInfoReceived(const sp<RfxMclMessage>& msg) {
    /* +EMODCFG: <mode> */
    int err;
    int response = 0;
    sp<RfxMclMessage> urc;
    RfxAtLine *line = msg->getRawUrc();

    line->atTokStart(&err);
    if (err < 0) goto error;

    // get <mode>
    response = line->atTokNextint(&err);
    if (err < 0) goto error;

    if (response > 0xFF) goto error;

    logD(LOG_TAG, "handleModulationInfoReceived: <mode>:%d", response);
    urc = RfxMclMessage::obtainUrc(RFX_MSG_URC_MODULATION_INFO, m_slot_id, RfxIntsData(&response, 1));
    responseToTelCore(urc);
    return;

error:
    logE(LOG_TAG, "There is something wrong with the URC +EMODCFG");
}

void RmcNetworkUrcHandler::handleEnhancedOperatorNameDisplay(const sp<RfxMclMessage>& msg) {
    int err;
    int pnn, opl;
    sp<RfxMclMessage> urc;
    RfxAtLine *line = msg->getRawUrc();

    /* +EONS: <PNN_service>,<OPL_service> */

    line->atTokStart(&err);
    if (err < 0) return;

    pnn = line->atTokNextint(&err);
    if (err < 0 || pnn < 0 || pnn > 1) return;

    opl = line->atTokNextint(&err);
    if (err < 0 || opl < 0 || opl > 1) return;

    logD(LOG_TAG, "Get EONS info of slot %d: %d %d", m_slot_id, pnn, opl);

    if (pnn == 1 && opl == 1) {
        eons_info[m_slot_id].eons_status = EONS_INFO_RECEIVED_ENABLED;
        urc = RfxMclMessage::obtainUrc(RFX_MSG_URC_RESPONSE_VOICE_NETWORK_STATE_CHANGED,
                m_slot_id, RfxVoidData());
        responseToTelCore(urc);
    } else {
        eons_info[m_slot_id].eons_status = EONS_INFO_RECEIVED_DISABLED;
    }
}

void RmcNetworkUrcHandler::handleNetworkScanResult(const sp<RfxMclMessage>& msg) {
    int err = 0;
    sp<RfxMclMessage> urc;
    RIL_NetworkScanResult *response = NULL;
    RfxAtLine *line = msg->getRawUrc();

    line->atTokStart(&err);
    if (err < 0) goto error;

    response = (RIL_NetworkScanResult *) alloca(sizeof(RIL_NetworkScanResult));
    memset(response, 0, sizeof(RIL_NetworkScanResult));

    urc = RfxMclMessage::obtainUrc(RFX_MSG_URC_NETWORK_SCAN_RESULT, m_slot_id,
            RfxNetworkScanResultData(response, sizeof(RIL_NetworkScanResult)));
    responseToTelCore(urc);
    return;

error:
    logE(LOG_TAG, "There is something wrong with the URC");
}

void RmcNetworkUrcHandler::onHandleTimer() {
    // do something
}

void RmcNetworkUrcHandler::onHandleEvent(const sp<RfxMclMessage>& msg) {
    RFX_UNUSED(msg);
    // handle event
}

bool RmcNetworkUrcHandler::onCheckIfRejectMessage(const sp<RfxMclMessage>& msg,
        RIL_RadioState radioState) {
    bool reject = false;
    if (RADIO_STATE_UNAVAILABLE == radioState) {
        if (((strStartsWith(msg->getRawUrc()->getLine(), "+ERPRAT:"))
                || (strStartsWith(msg->getRawUrc()->getLine(), "+EIPRL:")))
                        && (RmcWpRequestHandler::isWorldModeSwitching())) {
            reject = false;
        } else if (strStartsWith(msg->getRawUrc()->getLine(), "+EWFC:")) {
            reject = false;
        } else {
            if (allowed_urc != NULL) {
                int length = sizeof(allowed_urc) / sizeof(char *);
                for (int i = 0; i < length && allowed_urc[i] != NULL; i++) {
                    logD(LOG_TAG, "onCheckIfRejectMessage, urc = %s, i = %d, length = %d. ",
                            allowed_urc[i], i, length);
                    if ((strStartsWith(msg->getRawUrc()->getLine(), allowed_urc[i]))) {
                        if (RmcWpRequestHandler::isWorldModeSwitching()) {
                            reject = false;
                        } else {
                            reject = true;
                        }
                        break;
                    } else {
                        reject = true;
                    }
                }
            }
        }
    }
    return reject;
}

