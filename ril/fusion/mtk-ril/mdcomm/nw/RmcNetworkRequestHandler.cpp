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

#include "RmcNetworkRequestHandler.h"
#include "rfx_properties.h"
#include "ViaBaseHandler.h"
#include "RfxViaUtils.h"

static SpnTable s_mtk_spn_table[] = {
#include "mtk_spn_table.h"
};

static const int request[] = {
    RFX_MSG_REQUEST_SIGNAL_STRENGTH,
    RFX_MSG_REQUEST_VOICE_REGISTRATION_STATE,
    RFX_MSG_REQUEST_DATA_REGISTRATION_STATE,
    RFX_MSG_REQUEST_OPERATOR,
    RFX_MSG_REQUEST_QUERY_NETWORK_SELECTION_MODE,
    RFX_MSG_REQUEST_QUERY_AVAILABLE_NETWORKS,
    RFX_MSG_REQUEST_QUERY_AVAILABLE_NETWORKS_WITH_ACT,
    RFX_MSG_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC,
    RFX_MSG_REQUEST_SET_NETWORK_SELECTION_MANUAL,
    RFX_MSG_REQUEST_SET_NETWORK_SELECTION_MANUAL_WITH_ACT,
    RFX_MSG_REQUEST_SET_BAND_MODE,
    RFX_MSG_REQUEST_QUERY_AVAILABLE_BAND_MODE,
    RFX_MSG_REQUEST_GET_NEIGHBORING_CELL_IDS,
    RFX_MSG_REQUEST_SET_LOCATION_UPDATES,
    RFX_MSG_REQUEST_GET_CELL_INFO_LIST,
    RFX_MSG_REQUEST_SET_UNSOL_CELL_INFO_LIST_RATE,
    RFX_MSG_REQUEST_GET_POL_CAPABILITY,
    RFX_MSG_REQUEST_GET_POL_LIST,
    RFX_MSG_REQUEST_SET_POL_ENTRY,
    RFX_MSG_REQUEST_CDMA_SET_ROAMING_PREFERENCE,
    RFX_MSG_REQUEST_CDMA_QUERY_ROAMING_PREFERENCE,
    RFX_MSG_REQUEST_GET_FEMTOCELL_LIST,
    RFX_MSG_REQUEST_ABORT_FEMTOCELL_LIST,
    RFX_MSG_REQUEST_SELECT_FEMTOCELL,
    RFX_MSG_REQUEST_SCREEN_STATE,
    RFX_MSG_REQUEST_SET_UNSOLICITED_RESPONSE_FILTER,
    RFX_MSG_REQUEST_QUERY_FEMTOCELL_SYSTEM_SELECTION_MODE,
    RFX_MSG_REQUEST_SET_FEMTOCELL_SYSTEM_SELECTION_MODE,
    // RFX_MSG_REQUEST_VSS_ANTENNA_CONF,
    // RFX_MSG_REQUEST_VSS_ANTENNA_INFO,
    RFX_MSG_REQUEST_SET_SERVICE_STATE,
    RFX_MSG_REQUEST_SET_PSEUDO_CELL_MODE,
    RFX_MSG_REQUEST_GET_PSEUDO_CELL_INFO,
    RFX_MSG_RIL_REQUEST_START_NETWORK_SCAN,
    RFX_MSG_RIL_REQUEST_STOP_NETWORK_SCAN
};

static const int events[] = {
    RFX_MSG_EVENT_EXIT_EMERGENCY_CALLBACK_MODE,
    RFX_MSG_EVENT_FEMTOCELL_UPDATE,
    RFX_MSG_EVENT_CONFIRM_RAT_BEGIN,
    RFX_MSG_EVENT_PS_NETWORK_STATE,
};

// register data
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxIntsData, RFX_MSG_REQUEST_SIGNAL_STRENGTH);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxStringsData, RFX_MSG_REQUEST_VOICE_REGISTRATION_STATE);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxStringsData, RFX_MSG_REQUEST_DATA_REGISTRATION_STATE);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxStringsData, RFX_MSG_REQUEST_OPERATOR);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxIntsData, RFX_MSG_REQUEST_QUERY_NETWORK_SELECTION_MODE);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxStringsData, RFX_MSG_REQUEST_QUERY_AVAILABLE_NETWORKS);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxStringsData, RFX_MSG_REQUEST_QUERY_AVAILABLE_NETWORKS_WITH_ACT);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxVoidData, RFX_MSG_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxStringData, RfxVoidData, RFX_MSG_REQUEST_SET_NETWORK_SELECTION_MANUAL);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxStringsData, RfxVoidData, RFX_MSG_REQUEST_SET_NETWORK_SELECTION_MANUAL_WITH_ACT);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData, RFX_MSG_REQUEST_SET_BAND_MODE);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxIntsData, RFX_MSG_REQUEST_QUERY_AVAILABLE_BAND_MODE);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxNeighboringCellData, RFX_MSG_REQUEST_GET_NEIGHBORING_CELL_IDS);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData, RFX_MSG_REQUEST_SET_LOCATION_UPDATES);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxCellInfoData, RFX_MSG_REQUEST_GET_CELL_INFO_LIST);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData, RFX_MSG_REQUEST_SET_UNSOL_CELL_INFO_LIST_RATE);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxIntsData, RFX_MSG_REQUEST_GET_POL_CAPABILITY);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxStringsData, RFX_MSG_REQUEST_GET_POL_LIST);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxStringsData, RfxVoidData, RFX_MSG_REQUEST_SET_POL_ENTRY);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData, RFX_MSG_REQUEST_CDMA_SET_ROAMING_PREFERENCE);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxIntsData, RFX_MSG_REQUEST_CDMA_QUERY_ROAMING_PREFERENCE);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxStringsData, RFX_MSG_REQUEST_GET_FEMTOCELL_LIST);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxVoidData, RFX_MSG_REQUEST_ABORT_FEMTOCELL_LIST);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxStringsData, RfxVoidData, RFX_MSG_REQUEST_SELECT_FEMTOCELL);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData, RFX_MSG_REQUEST_SCREEN_STATE);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData, RFX_MSG_REQUEST_SET_UNSOLICITED_RESPONSE_FILTER);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxIntsData, RFX_MSG_REQUEST_QUERY_FEMTOCELL_SYSTEM_SELECTION_MODE);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData, RFX_MSG_REQUEST_SET_FEMTOCELL_SYSTEM_SELECTION_MODE);
// RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxIntsData, RFX_MSG_REQUEST_VSS_ANTENNA_CONF);
// RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxIntsData, RFX_MSG_REQUEST_VSS_ANTENNA_INFO);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData, RFX_MSG_REQUEST_SET_SERVICE_STATE);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData, RFX_MSG_REQUEST_SET_PSEUDO_CELL_MODE);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxIntsData, RFX_MSG_REQUEST_GET_PSEUDO_CELL_INFO);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxVoidData, RFX_MSG_REQUEST_SET_ROAMING_ENABLE);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxIntsData, RfxIntsData, RFX_MSG_REQUEST_GET_ROAMING_ENABLE);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxNetworkScanData, RfxVoidData, RFX_MSG_RIL_REQUEST_START_NETWORK_SCAN);
RFX_REGISTER_DATA_TO_REQUEST_ID(RfxVoidData, RfxVoidData, RFX_MSG_RIL_REQUEST_STOP_NETWORK_SCAN);

RFX_REGISTER_DATA_TO_URC_ID(RfxStringsData, RFX_MSG_URC_FEMTOCELL_INFO);
RFX_REGISTER_DATA_TO_URC_ID(RfxIntsData, RFX_MSG_URC_RESPONSE_PS_NETWORK_STATE_CHANGED);

// register handler to channel
RFX_IMPLEMENT_HANDLER_CLASS(RmcNetworkRequestHandler, RIL_CMD_PROXY_3);

RmcNetworkRequestHandler::RmcNetworkRequestHandler(int slot_id, int channel_id) :
        RmcNetworkHandler(slot_id, channel_id),
        mPlmn_list_format(0),
        m_emergency_only(0){
    int err;
    sp<RfxAtResponse> p_response;

    m_slot_id = slot_id;
    m_channel_id = channel_id;
    registerToHandleRequest(request, sizeof(request)/sizeof(int));
    registerToHandleEvent(events, sizeof(events)/sizeof(int));
    resetVoiceRegStateCache(&voice_reg_state_cache[m_slot_id], CACHE_GROUP_ALL);
    resetDataRegStateCache(&data_reg_state_cache);
    pthread_mutex_lock(&s_signalStrengthMutex[m_slot_id]);
    resetSignalStrengthCache(&signal_strength_cache[m_slot_id], CACHE_GROUP_ALL);
    pthread_mutex_unlock(&s_signalStrengthMutex[m_slot_id]);
    updateServiceStateValue();
    updatePseudoCellMode();

    /* Enable network registration, location info and EMM cause value unsolicited result */
    atSendCommand("AT+CEREG=3");
    /* Enable 93 network registration, location info and cause value unsolicited result */
    atSendCommand("AT+EREG=3");
    /* Enable 93 network registration, location info and GMM cause value unsolicited result */
    atSendCommand("AT+EGREG=3");
    /* Enable packet switching data service capability URC */
    atSendCommand("AT+PSBEARER=1");
    /* Enable received signal level indication */
    atSendCommand("AT+ECSQ=1");
    /* Config the Signal notify frequency */
    atSendCommand("AT+ECSQ=3,0,2");
    /* Enable getting NITZ, include TZ and Operator Name*/
    /* To Receive +CTZE and +CIEV: 10*/
    atSendCommand("AT+CTZR=3");
    /* Enable CSG network URC */
    if (isFemtocellSupport()) {
        atSendCommand("AT+ECSG=4,1");
        /* Enable C2K femtocell URC */
        atSendCommand("AT+EFCELL=1");
    }
    /* Check if modem support ECELL ext3,ext4 */
    p_response = atSendCommand("AT+ECELL=4");
    err = p_response->getError();
    if (err < 0 || p_response->getSuccess() == 0) {
        logE(LOG_TAG, "modem does not support AT+ECELL=4.");
        ECELLext3ext4Support = 0;
    }
    /* Config cell info list extend c2k */
    atSendCommand("AT+ECELL=7,1");
    /* To support return EONS if available in RIL_REQUEST_OPERATOR START */
    atSendCommand("AT+EONS=1");
    /* ALPS00574862 Remove redundant +COPS=3,2;+COPS? multiple cmd in REQUEST_OPERATOR */
    atSendCommand("AT+EOPS=3,2");
    p_response = atSendCommand("AT+COPS=3,3");
    err = p_response->getError();
    if (err >= 0 || p_response->getSuccess() != 0) {
        mPlmn_list_format = 1;
    }
    /* check whether calibration data is downloaded or not */
    p_response = atSendCommand("AT+ECAL?");
    err = p_response->getError();
    if (err < 0 || p_response->getSuccess() == 0) {
        logE(LOG_TAG, "bootup get calibration status failed, err=%d", err);
    }
    /* 2G only feature */
    if (isDisable2G()) {
        atSendCommand("AT+EDRAT=1");
    }
    /*Modulation report*/
    if (isEnableModulationReport()) {
        atSendCommand("AT+EMODCFG=1");
    }
    // TODO: consider boot up screen off test scenario
}

RmcNetworkRequestHandler::~RmcNetworkRequestHandler() {
}

void RmcNetworkRequestHandler::onHandleRequest(const sp<RfxMclMessage>& msg) {
    // logD(LOG_TAG, "[onHandleRequest] %s", RFX_ID_TO_STR(msg->getId()));
    int request = msg->getId();
    switch(request) {
        case RFX_MSG_REQUEST_SIGNAL_STRENGTH:
            requestSignalStrength(msg);
            break;
        case RFX_MSG_REQUEST_VOICE_REGISTRATION_STATE:
            requestVoiceRegistrationState(msg);
            break;
        case RFX_MSG_REQUEST_DATA_REGISTRATION_STATE:
            requestDataRegistrationState(msg);
            break;
        case RFX_MSG_REQUEST_OPERATOR:
            requestOperator(msg);
            break;
        case RFX_MSG_REQUEST_QUERY_NETWORK_SELECTION_MODE:
            requestQueryNetworkSelectionMode(msg);
            break;
        case RFX_MSG_REQUEST_QUERY_AVAILABLE_NETWORKS:
            requestQueryAvailableNetworks(msg);
            break;
        case RFX_MSG_REQUEST_QUERY_AVAILABLE_NETWORKS_WITH_ACT:
            requestQueryAvailableNetworksWithAct(msg);
            break;
        case RFX_MSG_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC:
            requestSetNetworkSelectionAutomatic(msg);
            break;
        case RFX_MSG_REQUEST_SET_NETWORK_SELECTION_MANUAL:
            requestSetNetworkSelectionManual(msg);
            break;
        case RFX_MSG_REQUEST_SET_NETWORK_SELECTION_MANUAL_WITH_ACT:
            requestSetNetworkSelectionManualWithAct(msg);
            break;
        case RFX_MSG_REQUEST_SET_BAND_MODE:
            requestSetBandMode(msg);
            break;
        case RFX_MSG_REQUEST_QUERY_AVAILABLE_BAND_MODE:
            requestQueryAvailableBandMode(msg);
            break;
        case RFX_MSG_REQUEST_GET_NEIGHBORING_CELL_IDS:
            requestGetNeighboringCellIds(msg);
            break;
        case RFX_MSG_REQUEST_SET_LOCATION_UPDATES:
            requestSetLocationUpdates(msg);
            break;
        case RFX_MSG_REQUEST_GET_CELL_INFO_LIST:
            requestGetCellInfoList(msg);
            break;
        case RFX_MSG_REQUEST_SET_UNSOL_CELL_INFO_LIST_RATE:
            requestSetCellInfoListRate(msg);
            break;
        case RFX_MSG_REQUEST_GET_POL_CAPABILITY:
            requestGetPOLCapability(msg);
            break;
        case RFX_MSG_REQUEST_GET_POL_LIST:
            requestGetPOLList(msg);
            break;
        case RFX_MSG_REQUEST_SET_POL_ENTRY:
            requestSetPOLEntry(msg);
            break;
        case RFX_MSG_REQUEST_CDMA_SET_ROAMING_PREFERENCE:
            requestSetCdmaRoamingPreference(msg);
            break;
        case RFX_MSG_REQUEST_CDMA_QUERY_ROAMING_PREFERENCE:
            requestQueryCdmaRoamingPreference(msg);
            break;
        case RFX_MSG_REQUEST_GET_FEMTOCELL_LIST:
            requestGetFemtocellList(msg);
            break;
        case RFX_MSG_REQUEST_ABORT_FEMTOCELL_LIST:
            requestAbortFemtocellList(msg);
            break;
        case RFX_MSG_REQUEST_SELECT_FEMTOCELL:
            requestSelectFemtocell(msg);
            break;
        case RFX_MSG_REQUEST_SCREEN_STATE:
            requestScreenState(msg);
            break;
        case RFX_MSG_REQUEST_SET_UNSOLICITED_RESPONSE_FILTER:
            requestSetUnsolicitedResponseFilter(msg);
            break;
        case RFX_MSG_REQUEST_QUERY_FEMTOCELL_SYSTEM_SELECTION_MODE:
            requestQueryFemtoCellSystemSelectionMode(msg);
            break;
        case RFX_MSG_REQUEST_SET_FEMTOCELL_SYSTEM_SELECTION_MODE:
            requestSetFemtoCellSystemSelectionMode(msg);
            break;
        case RFX_MSG_REQUEST_VSS_ANTENNA_CONF:
            requestAntennaConf(msg);
            break;
        case RFX_MSG_REQUEST_VSS_ANTENNA_INFO:
            requestAntennaInfo(msg);
            break;
        case RFX_MSG_REQUEST_SET_SERVICE_STATE:
            requestSetServiceState(msg);
            break;
        case RFX_MSG_REQUEST_SET_PSEUDO_CELL_MODE:
            requestSetPseudoCellMode(msg);
            break;
        case RFX_MSG_REQUEST_GET_PSEUDO_CELL_INFO:
            requestGetPseudoCellInfo(msg);
            break;
        case RFX_MSG_REQUEST_SET_ROAMING_ENABLE:
            setRoamingEnable(msg);
            break;
        case RFX_MSG_REQUEST_GET_ROAMING_ENABLE:
            getRoamingEnable(msg);
            break;
        case RFX_MSG_RIL_REQUEST_START_NETWORK_SCAN:
            requestStartNetworkScan(msg);
            break;
        case RFX_MSG_RIL_REQUEST_STOP_NETWORK_SCAN:
            requestStopNetworkScan(msg);
            break;
        default:
            logE(LOG_TAG, "Should not be here");
            break;
    }
}

void RmcNetworkRequestHandler::requestSignalStrength(const sp<RfxMclMessage>& msg)
{
    sp<RfxAtResponse> p_response;
    RfxAtLine* p_cur;
    int err;
    sp<RfxMclMessage> response;
    int resp[14] = {0};
    pthread_mutex_lock(&s_signalStrengthMutex[m_slot_id]);
    //resetSignalStrengthCache(&signal_strength_cache[m_slot_id], CACHE_GROUP_ALL);

    // send AT command
    p_response = atSendCommandMultiline("AT+ECSQ", "+ECSQ:");

    // check error
    err = p_response->getError();
    if (err != 0 ||
            p_response == NULL ||
            p_response->getSuccess() == 0 ||
            p_response->getIntermediates() == NULL) {
        goto error;
    }

    for (p_cur = p_response->getIntermediates()
         ; p_cur != NULL
         ; p_cur = p_cur->getNext()
         ) {
        err = getSignalStrength(p_cur);

        if (err != 0)
            continue;
    }

    // copy signal strength cache to int array
    memcpy(&resp, &signal_strength_cache[m_slot_id], 14*sizeof(int));
    pthread_mutex_unlock(&s_signalStrengthMutex[m_slot_id]);
    // returns the whole cache, including GSM, WCDMA, TD-SCDMA, CDMA, EVDO, LTE
    response = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_SUCCESS,
            RfxIntsData(resp, 14), msg, false);
    // response to TeleCore
    responseToTelCore(response);

    return;

error:
    pthread_mutex_unlock(&s_signalStrengthMutex[m_slot_id]);
    logE(LOG_TAG, "requestSignalStrength must never return an error when radio is on");
    response = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_GENERIC_FAILURE,
            RfxIntsData(), msg, false);
    // response to TeleCore
    responseToTelCore(response);
    return;
}

void RmcNetworkRequestHandler::combineVoiceRegState(const sp<RfxMclMessage>& msg)
{
    int i;
    char * responseStr[15];

    pthread_mutex_lock(&s_voiceRegStateMutex[m_slot_id]);

    asprintf(&responseStr[0], "%d", voice_reg_state_cache[m_slot_id].registration_state);

    if (voice_reg_state_cache[m_slot_id].lac != 0xffffffff)
        asprintf(&responseStr[1], "%d", voice_reg_state_cache[m_slot_id].lac);
    else
        asprintf(&responseStr[1], "-1");

    if (voice_reg_state_cache[m_slot_id].cid != 0xffffffff)
        asprintf(&responseStr[2], "%d", voice_reg_state_cache[m_slot_id].cid);
    else
        asprintf(&responseStr[2], "-1");

    asprintf(&responseStr[3], "%d", voice_reg_state_cache[m_slot_id].radio_technology);

    asprintf(&responseStr[4], "%d", voice_reg_state_cache[m_slot_id].base_station_id);

    asprintf(&responseStr[5], "%d", voice_reg_state_cache[m_slot_id].base_station_latitude);

    asprintf(&responseStr[6], "%d", voice_reg_state_cache[m_slot_id].base_station_longitude);

    asprintf(&responseStr[7], "%d", voice_reg_state_cache[m_slot_id].css);

    asprintf(&responseStr[8], "%d", voice_reg_state_cache[m_slot_id].system_id);

    asprintf(&responseStr[9], "%d", voice_reg_state_cache[m_slot_id].network_id);

    asprintf(&responseStr[10], "%d", voice_reg_state_cache[m_slot_id].roaming_indicator);

    asprintf(&responseStr[11], "%d", voice_reg_state_cache[m_slot_id].is_in_prl);

    asprintf(&responseStr[12], "%d", voice_reg_state_cache[m_slot_id].default_roaming_indicator);

    asprintf(&responseStr[13], "%d", voice_reg_state_cache[m_slot_id].denied_reason);

    asprintf(&responseStr[14], "%d", voice_reg_state_cache[m_slot_id].psc);

    pthread_mutex_unlock(&s_voiceRegStateMutex[m_slot_id]);

    updateServiceStateValue();

    sp<RfxMclMessage> response = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_SUCCESS,
            RfxStringsData(responseStr, 15), msg, false);

    // response to TeleCore
    responseToTelCore(response);

    for (i=0; i<15; ++i) {
        if (responseStr[i] != NULL)
            free(responseStr[i]);
    }
}

int RmcNetworkRequestHandler::convertToAndroidRegState(unsigned int uiRegState)
{
    unsigned int uiRet = 0;

    /* RIL defined RIL_REQUEST_VOICE_REGISTRATION_STATE response state 0-6,
     *              0 - Not registered, MT is not currently searching
     *                  a new operator to register
     *              1 - Registered, home network
     *              2 - Not registered, but MT is currently searching
     *                  a new operator to register
     *              3 - Registration denied
     *              4 - Unknown
     *              5 - Registered, roaming
     *             10 - Same as 0, but indicates that emergency calls
     *                  are enabled.
     *             12 - Same as 2, but indicates that emergency calls
     *                  are enabled.
     *             13 - Same as 3, but indicates that emergency calls
     *                  are enabled.
     *             14 - Same as 4, but indicates that emergency calls
     *                  are enabled.
     */
    if (m_emergency_only == 1) {
        switch (uiRegState)
        {
            case 0:
                uiRet = 10;
                break;
            case 2:
                uiRet = 12;
                break;
            case 3:
                uiRet = 13;
                break;
            case 4:
                uiRet = 14;
                break;
            default:
                uiRet = uiRegState;
                break;
        }
    } else {
        uiRet = uiRegState;
    }
    return uiRet;
}

void RmcNetworkRequestHandler::requestVoiceRegistrationState(const sp<RfxMclMessage>& msg)
{
    int exist = 0;
    int err;
    int i = 0;
    sp<RfxAtResponse> p_response;
    RfxAtLine* line;
    int skip;
    int count = 3, cntCdma = 0;
    int cause_type;
    bool m4gRat = false;

    resetVoiceRegStateCache(&voice_reg_state_cache[m_slot_id], CACHE_GROUP_COMMON_REQ);

    // send AT command
    p_response = atSendCommandSingleline("AT+EREG?", "+EREG:");

    // check error
    err = p_response->getError();
    if (err != 0 ||
            p_response == NULL ||
            p_response->getSuccess() == 0 ||
            p_response->getIntermediates() == NULL) goto error;

    // handle intermediate
    line = p_response->getIntermediates();

    // go to start position
    line->atTokStart(&err);
    if (err < 0) goto error;

    /* +EREG: <n>,<stat> */
    /* +EREG: <n>,<stat>,<lac>,<ci>,<eAct>,<nwk_existence >,<roam_indicator> */
    /* +EREG: <n>,<stat>,<lac>,<ci>,<eAct>,<nwk_existence >,<roam_indicator>,<cause_type>,<reject_cause> */

    /* <n> */
    skip = line->atTokNextint(&err);
    if (err < 0) goto error;

    pthread_mutex_lock(&s_voiceRegStateMutex[m_slot_id]);

    /* <stat> */
    voice_reg_state_cache[m_slot_id].registration_state = line->atTokNextint(&err);

    if (err < 0 || voice_reg_state_cache[m_slot_id].registration_state > 10 )  //for LTE
    {
        // TODO: add C2K flag
        if (voice_reg_state_cache[m_slot_id].registration_state < 101 ||
                voice_reg_state_cache[m_slot_id].registration_state > 104)  // for C2K
        {
            logE(LOG_TAG, "The value in the field <stat> is not valid: %d",
                 voice_reg_state_cache[m_slot_id].registration_state);
            goto error;
        }
    }

    //For LTE and C2K
    voice_reg_state_cache[m_slot_id].registration_state = convertRegState(voice_reg_state_cache[m_slot_id].registration_state, true);

    if (line->atTokHasmore())
    {
        /* <lac> */
        voice_reg_state_cache[m_slot_id].lac = line->atTokNexthexint(&err);
        if ( err < 0 ||
                (voice_reg_state_cache[m_slot_id].lac > 0xffff && voice_reg_state_cache[m_slot_id].lac != 0xffffffff) )
        {
            logE(LOG_TAG, "The value in the field <lac> or <stat> is not valid. <stat>:%d, <lac>:%d",
                 voice_reg_state_cache[m_slot_id].registration_state, voice_reg_state_cache[m_slot_id].lac);
            goto error;
        }

        /* <ci> */
        voice_reg_state_cache[m_slot_id].cid = line->atTokNexthexint(&err);
        if (err < 0 ||
                (voice_reg_state_cache[m_slot_id].cid > 0x0fffffff && voice_reg_state_cache[m_slot_id].cid != 0xffffffff))
        {
            logE(LOG_TAG, "The value in the field <ci> is not valid: %d", voice_reg_state_cache[m_slot_id].cid);
            goto error;
        }

        /* <eAct> */
        voice_reg_state_cache[m_slot_id].radio_technology = line->atTokNextint(&err);
        if (err < 0)
        {
            logE(LOG_TAG, "No eAct in command");
            goto error;
        }
        count = 4;
        if (voice_reg_state_cache[m_slot_id].radio_technology > 0x2000)  // LTE-CA is 0x2000
        {
            logE(LOG_TAG, "The value in the eAct is not valid: %d",
                 voice_reg_state_cache[m_slot_id].radio_technology);
            goto error;
        }

        /* <eAct> mapping */
        if (!isInService(voice_reg_state_cache[m_slot_id].registration_state))
        {
            if (convertCSNetworkType(voice_reg_state_cache[m_slot_id].radio_technology) == 14)
            {
                m4gRat = true;
            }
            voice_reg_state_cache[m_slot_id].radio_technology = 0;
        }
        else
        {
            voice_reg_state_cache[m_slot_id].radio_technology = convertCSNetworkType(voice_reg_state_cache[m_slot_id].radio_technology);
        }

        voice_reg_state_cache[m_slot_id].network_exist = line->atTokNextint(&err);
        if (err < 0) {
            goto error;
        }

        voice_reg_state_cache[m_slot_id].roaming_indicator = line->atTokNextint(&err);
        if (err < 0) {
            goto error;
        }
        if (voice_reg_state_cache[m_slot_id].roaming_indicator > 12) {
            voice_reg_state_cache[m_slot_id].roaming_indicator = 1;
        }

        if (line->atTokHasmore())
        {
            /* <cause_type> */
            cause_type = line->atTokNextint(&err);
            if (err < 0 || cause_type != 0)
            {
                logE(LOG_TAG, "The value in the field <cause_type> is not valid: %d", cause_type);
                goto error;
            }

            /* <reject_cause> */
            voice_reg_state_cache[m_slot_id].denied_reason = line->atTokNextint(&err);
            if (err < 0)
            {
                logE(LOG_TAG, "The value in the field <reject_cause> is not valid: %d",
                     voice_reg_state_cache[m_slot_id].denied_reason );
                goto error;
            }
            count = 14;
        }
    }
    else
    {
        /* +CREG: <n>, <stat> */
        voice_reg_state_cache[m_slot_id].lac = -1;
        voice_reg_state_cache[m_slot_id].cid = -1;
        voice_reg_state_cache[m_slot_id].radio_technology = 0;
        //BEGIN mtk03923 [20120119][ALPS00112664]
        count = 4;
        //END   mtk03923 [20120119][ALPS00112664]
    }
    if (isFemtocellSupport()) {
        isFemtoCell(voice_reg_state_cache[m_slot_id].registration_state,
                voice_reg_state_cache[m_slot_id].cid, voice_reg_state_cache[m_slot_id].radio_technology);
    }
    // TODO: restricted state for dual SIM
    // ECC support - START
    if(voice_reg_state_cache[m_slot_id].registration_state == 4) {
        /* registration_state(4) is 'Unknown' */
        // logD(LOG_TAG, "No valid info to distinguish limited service and no service");
    } else {
        // if cid is 0x0fffffff means it is invalid
        // if lac is 0xffff means it is invalid
        if (((voice_reg_state_cache[m_slot_id].registration_state == 0) ||
                (voice_reg_state_cache[m_slot_id].registration_state == 2) ||
                (voice_reg_state_cache[m_slot_id].registration_state == 3)) &&
                ((voice_reg_state_cache[m_slot_id].cid != 0x0fffffff) &&
                (voice_reg_state_cache[m_slot_id].lac != 0xffff) &&
                (voice_reg_state_cache[m_slot_id].cid != (unsigned int)-1) &&
                (voice_reg_state_cache[m_slot_id].lac != (unsigned int)-1)) &&
                // do not set ECC when it is LTE. ECC depends IMS.
                (!m4gRat)) {
            if (m_emergency_only == 0) {
                m_emergency_only = 1;
                logD(LOG_TAG, "Set slot%d m_emergencly_only to true", m_slot_id);
            }
            if (voice_reg_state_cache[m_slot_id].network_exist != 1) {
                voice_reg_state_cache[m_slot_id].network_exist = 1;
                // logD(LOG_TAG, "Set slot%d network_exist to true", m_slot_id);
            }
        } else {
            if(m_emergency_only == 1) {
                m_emergency_only = 0;
                logD(LOG_TAG, "Set slot%d s_emergencly_only to false", m_slot_id);
            }
        }
    }

    voice_reg_state_cache[m_slot_id].registration_state = convertToAndroidRegState(voice_reg_state_cache[m_slot_id].registration_state);
    // ECC support -END

    pthread_mutex_unlock(&s_voiceRegStateMutex[m_slot_id]);

    // TODO: only query cdma state if C2K support and on cdma slot
    cntCdma = requestVoiceRegistrationStateCdma(msg);
    if (cntCdma == 0) {
        // response contains error
        resetVoiceRegStateCache(&voice_reg_state_cache[m_slot_id], CACHE_GROUP_C2K);
        // logD(LOG_TAG, "C2K voice response error");
    } else if (cntCdma > count) {
        count = cntCdma;
    }
    combineVoiceRegState(msg);

    return;

error:
    pthread_mutex_unlock(&s_voiceRegStateMutex[m_slot_id]);
    logE(LOG_TAG, "requestVoiceRegistrationState must never return an error when radio is on");
    sp<RfxMclMessage> response = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_GENERIC_FAILURE,
            RfxStringsData(), msg, false);
    // response to TeleCore
    responseToTelCore(response);
    return;
}

int RmcNetworkRequestHandler::requestVoiceRegistrationStateCdma(const sp<RfxMclMessage>& msg) {
    RFX_UNUSED(msg);
    int err = 0;
    ViaBaseHandler *mViaHandler = RfxViaUtils::getViaHandler();
    pthread_mutex_lock(&s_voiceRegStateMutex[m_slot_id]);
    if (mViaHandler != NULL) {
        err = mViaHandler->getCdmaLocationInfo(this, &voice_reg_state_cache[m_slot_id]);
        if (err < 0) {
            goto error;
        }
    } else goto error;
    pthread_mutex_unlock(&s_voiceRegStateMutex[m_slot_id]);
    return 16;

error:
    pthread_mutex_unlock(&s_voiceRegStateMutex[m_slot_id]);
    // logE(LOG_TAG, "AT+VLOCINFO? response error");
    return 0;
}

static unsigned int convertCellSpeedSupport(unsigned int uiResponse)
{
    // Cell speed support is bitwise value of cell capability:
    // bit7 0x80  bit6 0x40  bit5 0x20  bit4 0x10  bit3 0x08  bit2 0x04  bit1 0x02  bit0 0x01
    // Dual-Cell  HSUPA+     HSDPA+     HSUPA      HSDPA      UMTS       EDGE       GPRS
    unsigned int uiRet = 0;

    unsigned int RIL_RADIO_TECHNOLOGY_MTK = 128;

    if ((uiResponse & 0x2000) != 0) {
        //support carrier aggregation (LTEA)
        uiRet = 19; // ServiceState.RIL_RADIO_TECHNOLOGY_LTE_CA
    } else if ((uiResponse & 0x1000) != 0) {
        uiRet = 14; // ServiceState.RIL_RADIO_TECHNOLOGY_LTE
    } else if ((uiResponse & 0x80) != 0 ||
            (uiResponse & 0x40) != 0 ||
            (uiResponse & 0x20) != 0) {
        uiRet = 15; // ServiceState.RIL_RADIO_TECHNOLOGY_HSPAP
    } else if ((uiResponse & 0x10) != 0 &&
            (uiResponse & 0x08) != 0) {
        uiRet = 11; // ServiceState.RIL_RADIO_TECHNOLOGY_HSPA
    } else if ((uiResponse & 0x10) != 0) {
        uiRet = 10; // ServiceState.RIL_RADIO_TECHNOLOGY_HSUPA
    } else if ((uiResponse & 0x08) != 0) {
        uiRet = 9;  // ServiceState.RIL_RADIO_TECHNOLOGY_HSDPA
    } else if ((uiResponse & 0x04) != 0) {
        uiRet = 3;  // ServiceState.RIL_RADIO_TECHNOLOGY_UMTS
    } else if ((uiResponse & 0x02) != 0) {
        uiRet = 2;  // ServiceState.RIL_RADIO_TECHNOLOGY_EDGE
    } else if ((uiResponse & 0x01) != 0) {
        uiRet = 1;  // ServiceState.RIL_RADIO_TECHNOLOGY_GPRS
    } else {
        uiRet = 0;  // ServiceState.RIL_RADIO_TECHNOLOGY_UNKNOWN
    }

    return uiRet;
}

static unsigned int convertPSBearerCapability(unsigned int uiResponse)
{
    /*
     *typedef enum
     *{
     *    L4C_NONE_ACTIVATE = 0,
     *    L4C_GPRS_CAPABILITY,
     *    L4C_EDGE_CAPABILITY,
     *    L4C_UMTS_CAPABILITY,
     *    L4C_HSDPA_CAPABILITY, //mac-hs
     *    L4C_HSUPA_CAPABILITY, //mac-e/es
     *    L4C_HSDPA_HSUPA_CAPABILITY, //mac-hs + mac-e/es
     *
     *    L4C_HSDPAP_CAPABILITY, //mac-ehs
     *    L4C_HSDPAP_UPA_CAPABILITY, //mac-ehs + mac-e/es
     *    L4C_HSUPAP_CAPABILITY, //mac-i/is
     *    L4C_HSUPAP_DPA_CAPABILITY, //mac-hs + mac-i/is
     *    L4C_HSPAP_CAPABILITY, //mac-ehs + mac-i/is
     *    L4C_DC_DPA_CAPABILITY, //(DC) mac-hs
     *    L4C_DC_DPA_UPA_CAPABILITY, //(DC) mac-hs + mac-e/es
     *    L4C_DC_HSDPAP_CAPABILITY, //(DC) mac-ehs
     *    L4C_DC_HSDPAP_UPA_CAPABILITY, //(DC) mac-ehs + mac-e/es
     *    L4C_DC_HSUPAP_DPA_CAPABILITY, //(DC) mac-hs + mac-i/is
     *    L4C_DC_HSPAP_CAPABILITY, //(DC) mac-ehs + mac-i/is
     *    L4C_LTE_CAPABILITY
     *} l4c_data_bearer_capablility_enum;
     */

    unsigned int uiRet = 0;

    unsigned int RIL_RADIO_TECHNOLOGY_MTK = 128;

    switch (uiResponse)
    {
        case 1:
            uiRet = 1;                           // ServiceState.RIL_RADIO_TECHNOLOGY_GPRS
            break;
        case 2:
            uiRet = 2;                           // ServiceState.RIL_RADIO_TECHNOLOGY_EDGE
            break;
        case 3:
            uiRet = 3;                           // ServiceState.RIL_RADIO_TECHNOLOGY_UMTS
            break;
        case 4:
            uiRet = 9;                           // ServiceState.RIL_RADIO_TECHNOLOGY_HSDPA
            break;
        case 5:
            uiRet = 10;                          // ServiceState.RIL_RADIO_TECHNOLOGY_HSUPA
            break;
        case 6:
            uiRet = 11;                          // ServiceState.RIL_RADIO_TECHNOLOGY_HSPA
            break;
        case 7:
            uiRet = RIL_RADIO_TECHNOLOGY_MTK+1;  // ServiceState.RIL_RADIO_TECHNOLOGY_HSDPAP
            break;
        case 8:
            uiRet = RIL_RADIO_TECHNOLOGY_MTK+2;  // ServiceState.RIL_RADIO_TECHNOLOGY_HSDPAP_UPA
            break;
        case 9:
            uiRet = RIL_RADIO_TECHNOLOGY_MTK+3;  // ServiceState.RIL_RADIO_TECHNOLOGY_HSUPAP
            break;
        case 10:
            uiRet = RIL_RADIO_TECHNOLOGY_MTK+4;  // ServiceState.RIL_RADIO_TECHNOLOGY_HSUPAP_DPA
            break;
        case 11:
            uiRet = 15;                          // ServiceState.RIL_RADIO_TECHNOLOGY_HSPAP
            break;
        case 12:
            uiRet = RIL_RADIO_TECHNOLOGY_MTK+5;  // ServiceState.RIL_RADIO_TECHNOLOGY_DC_DPA
            break;
        case 13:
            uiRet = RIL_RADIO_TECHNOLOGY_MTK+6;  // ServiceState.RIL_RADIO_TECHNOLOGY_DC_UPA
            break;
        case 14:
            uiRet = RIL_RADIO_TECHNOLOGY_MTK+7;  // ServiceState.RIL_RADIO_TECHNOLOGY_DC_HSDPAP
            break;
        case 15:
            uiRet = RIL_RADIO_TECHNOLOGY_MTK+8;  // ServiceState.RIL_RADIO_TECHNOLOGY_DC_HSDPAP_UPA
            break;
        case 16:
            uiRet = RIL_RADIO_TECHNOLOGY_MTK+9;  // ServiceState.RIL_RADIO_TECHNOLOGY_DC_HSDPAP_DPA
            break;
        case 17:
            uiRet = RIL_RADIO_TECHNOLOGY_MTK+10; // ServiceState.RIL_RADIO_TECHNOLOGY_DC_HSPAP
            break;
        case 18:
            uiRet = 14;                          // ServiceState.RIL_RADIO_TECHNOLOGY_LTE
            break;
        default:
            uiRet = 0;                           // ServiceState.RIL_RADIO_TECHNOLOGY_UNKNOWN
            break;
    }

    return uiRet;
}

void RmcNetworkRequestHandler::combineDataRegState(const sp<RfxMclMessage>& msg)
{
    logD(LOG_TAG, "combineDataRegState(): ril_wfc_reg_status=%d registration_state=%d",
            ril_wfc_reg_status[m_slot_id],
            data_reg_state_cache.registration_state);
    int i;
    char * responseStr[6];

    /// M: EPDG feature. @{
    if (ril_wfc_reg_status[m_slot_id] == 1) {
        resetDataRegStateCache(&data_reg_state_cache);
        data_reg_state_cache.registration_state = ril_wfc_reg_status[m_slot_id]; // use +EWFC value
        data_reg_state_cache.lac = -1;
        data_reg_state_cache.cid = -1;
        data_reg_state_cache.radio_technology = 18;
    }
    /// @}

    asprintf(&responseStr[0], "%d", data_reg_state_cache.registration_state);

    // set combined result to responseStr
    if (data_reg_state_cache.lac != 0xffffffff) {
        asprintf(&responseStr[1], "%d", data_reg_state_cache.lac);
    } else {
        asprintf(&responseStr[1], "-1");
    }

    if (data_reg_state_cache.cid != 0xffffffff) {
        asprintf(&responseStr[2], "%d", data_reg_state_cache.cid);
    } else {
        asprintf(&responseStr[2], "-1");
    }

    asprintf(&responseStr[3], "%d", data_reg_state_cache.radio_technology);

    asprintf(&responseStr[4], "%d", data_reg_state_cache.denied_reason);

    asprintf(&responseStr[5], "%d", data_reg_state_cache.max_simultaneous_data_call);

    updateServiceStateValue();

    sp<RfxMclMessage> response = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_SUCCESS,
            RfxStringsData(responseStr, 6), msg, false);

    // response to TeleCore
    responseToTelCore(response);

    for (i=0; i<6; ++i) {
        if (responseStr[i] != NULL)
            free(responseStr[i]);
    }
}

void RmcNetworkRequestHandler::printDataCache() {
    logD(LOG_TAG, "printDataCache: reg=%d, lac=%d, cid=%d, rat=%d, reason=%d",
                 data_reg_state_cache.registration_state,
                 data_reg_state_cache.lac,
                 data_reg_state_cache.cid,
                 data_reg_state_cache.radio_technology,
                 data_reg_state_cache.denied_reason);
}

void RmcNetworkRequestHandler::requestDataRegistrationState(const sp<RfxMclMessage>& msg)
{
    /* +EGREG: <n>, <stat>, <lac>, <cid>, <eAct>, <rac>, <nwk_existence>, <roam_indicator>, <cause_type>, <reject_cause> */

    int exist = 0;
    int err;
    sp<RfxAtResponse> p_response;
    RfxAtLine* line;
    int skip;
    int count = 3;
    int cause_type;

    resetDataRegStateCache(&data_reg_state_cache);

    // send AT command
    p_response = atSendCommandSingleline("AT+EGREG?", "+EGREG:");

    // check error
    err = p_response->getError();
    if (err != 0 ||
            p_response == NULL ||
            p_response->getSuccess() == 0 ||
            p_response->getIntermediates() == NULL) goto error;

    // handle intermediate
    line = p_response->getIntermediates();

    // go to start position
    line->atTokStart(&err);
    if (err < 0) goto error;

    /* <n> */
    skip = line->atTokNextint(&err);
    if (err < 0) goto error;

    /* <stat> */
    data_reg_state_cache.registration_state = line->atTokNextint(&err);

    if (err < 0 || data_reg_state_cache.registration_state > 10 )  //for LTE
    {
        // TODO: add C2K flag
        if (data_reg_state_cache.registration_state < 101 ||
                data_reg_state_cache.registration_state > 104)  // for C2K
        {
            logE(LOG_TAG, "The value in the field <stat> is not valid: %d",
                 data_reg_state_cache.registration_state);
            goto error;
        }
    }

    //For LTE and C2K
    data_reg_state_cache.registration_state = convertRegState(data_reg_state_cache.registration_state, false);

    if (line->atTokHasmore())
    {
        /* <lac> */
        data_reg_state_cache.lac = line->atTokNexthexint(&err);
        if ( err < 0 ||
                (data_reg_state_cache.lac > 0xffff && data_reg_state_cache.lac != 0xffffffff) )
        {
            logE(LOG_TAG, "The value in the field <lac> or <stat> is not valid. <stat>:%d, <lac>:%d",
                 data_reg_state_cache.registration_state, data_reg_state_cache.lac);
            goto error;
        }

        /* <ci> */
        data_reg_state_cache.cid = line->atTokNexthexint(&err);
        if (err < 0 ||
                (data_reg_state_cache.cid > 0x0fffffff && data_reg_state_cache.cid != 0xffffffff))
        {
            logE(LOG_TAG, "The value in the field <ci> is not valid: %d", data_reg_state_cache.cid);
            goto error;
        }

        /* <eAct> */
        data_reg_state_cache.radio_technology = line->atTokNextint(&err);
        if (err < 0)
        {
            logE(LOG_TAG, "No eAct in command");
            goto error;
        }
        count = 4;
        if (data_reg_state_cache.radio_technology > 0x2000)  // LTE-CA is 0x2000
        {
            logE(LOG_TAG, "The value in the eAct is not valid: %d",
                 data_reg_state_cache.radio_technology);
            goto error;
        }

        /* <eAct> mapping */
        if (!isInService(data_reg_state_cache.registration_state))
        {
            data_reg_state_cache.lac = -1;
            data_reg_state_cache.cid = -1;
            data_reg_state_cache.radio_technology = 0;
        }
        else
        {
            data_reg_state_cache.radio_technology = convertPSNetworkType(data_reg_state_cache.radio_technology);
        }

        if (line->atTokHasmore())
        {
            /* <rac> */
            skip = line->atTokNexthexint(&err);
            if (err < 0) goto error;

            skip = line->atTokNextint(&err);
            if (err < 0) {
                goto error;
            }

            exist = line->atTokNextint(&err);
            if (err < 0) {
                goto error;
            }
            // logD(LOG_TAG, "requestDataRegistrationState, exist: %d", exist);

            if (line->atTokHasmore())
            {
                /* <cause_type> */
                cause_type = line->atTokNextint(&err);
                if (err < 0 || cause_type != 0)
                {
                    logE(LOG_TAG, "The value in the field <cause_type> is not valid: %d", cause_type);
                    goto error;
                }

                /* <reject_cause> */
                data_reg_state_cache.denied_reason = line->atTokNextint(&err);
                if (err < 0)
                {
                    logE(LOG_TAG, "The value in the field <reject_cause> is not valid: %d",
                         data_reg_state_cache.denied_reason );
                    goto error;
                }
                count = 5;
            }
        }
        // TODO: maximum number of simultaneous Data Calls
    }
    else
    {
        /* +EGREG: <n>, <stat> */
        data_reg_state_cache.lac = -1;
        data_reg_state_cache.cid = -1;
        data_reg_state_cache.radio_technology = 0;
        //BEGIN mtk03923 [20120119][ALPS00112664]
        count = 4;
        //END mtk03923 [20120119][ALPS00112664]
    }

    // Query PSBEARER when PS registered
    if (isInService(data_reg_state_cache.registration_state)
               && data_reg_state_cache.radio_technology != 6
               && data_reg_state_cache.radio_technology != 8
               && data_reg_state_cache.radio_technology != 13) {
        requestDataRegistrationStateGsm();
    }
    // printDataCache();
    //TODO: restricted state for dual SIM

    combineDataRegState(msg);

    return;

error:
    logE(LOG_TAG, "requestDataRegistrationState must never return an error when radio is on");
    sp<RfxMclMessage> response = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_GENERIC_FAILURE,
            RfxStringsData(), msg, false);
    // response to TeleCore
    responseToTelCore(response);
    return;
}

void RmcNetworkRequestHandler::requestDataRegistrationStateGsm()
{
    int err;
    sp<RfxAtResponse> p_response;
    RfxAtLine* line;
    int psBearerCount = 0;
    int cell_data_speed = 0;
    int max_data_bearer = 0;

    //support carrier aggregation (LTEA)
    int ignoreMaxDataBearerCapability = 0;

    // send AT command
    p_response = atSendCommandSingleline("AT+PSBEARER?", "+PSBEARER:");

    // check error
    err = p_response->getError();
    if (err != 0 ||
            p_response == NULL ||
            p_response->getSuccess() == 0 ||
            p_response->getIntermediates() == NULL) goto skipR7R8;

    // handle intermediate
    line = p_response->getIntermediates();

    // go to start position
    line->atTokStart(&err);
    if (err < 0) goto skipR7R8;

    /* <cell_data_speed_support> */
    cell_data_speed = line->atTokNextint(&err);
    if (err < 0) goto skipR7R8;
    psBearerCount++;

    // <max_data_bearer_capability> is only support on 3G
    if (cell_data_speed >= 0x1000){
        ignoreMaxDataBearerCapability = 1;
    }

    cell_data_speed = convertCellSpeedSupport(cell_data_speed);

    /* <max_data_bearer_capability> */
    max_data_bearer = line->atTokNextint(&err);
    if (err < 0) goto skipR7R8;
    psBearerCount++;

    if (!ignoreMaxDataBearerCapability) {
        max_data_bearer = convertPSBearerCapability(max_data_bearer);
    } else {
        max_data_bearer = 0;  // ServiceState.RIL_RADIO_TECHNOLOGY_UNKNOWN
    }

skipR7R8:

    if (psBearerCount == 2) {
        if (cell_data_speed == 0 && max_data_bearer == 0) {
            return;
        } else {
            data_reg_state_cache.radio_technology =
                    (cell_data_speed > max_data_bearer) ? cell_data_speed : max_data_bearer;
        }
    }

    return;

}

void RmcNetworkRequestHandler::requestOperator(const sp<RfxMclMessage>& msg)
{
    int err;
    int skip;
    char *resp[3];
    sp<RfxMclMessage> response;
    RfxAtLine* line;
    char nemric[MAX_OPER_NAME_LENGTH];
    char longname[MAX_OPER_NAME_LENGTH], shortname[MAX_OPER_NAME_LENGTH];
    sp<RfxAtResponse> p_response;

    memset(resp, 0, sizeof(resp));

    /* Format should be set during initialization */
    p_response = atSendCommandSingleline("AT+EOPS?", "+EOPS:");

    // check error
    err = p_response->getError();
    if (err != 0 ||
            p_response == NULL ||
            p_response->getSuccess() == 0 ||
            p_response->getIntermediates() == NULL) {
        logE(LOG_TAG, "EOPS got error response");
        goto error;
    } else {
        // handle intermediate
        line = p_response->getIntermediates();

        // go to start position
        line->atTokStart(&err);
        if (err >= 0) {
            /* <mode> */
            skip = line->atTokNextint(&err);
            if ((err >= 0) &&( skip >= 0 && skip <= 4 && skip != 2)) {
                // a "+EOPS: 0" response is possible
                if (line->atTokHasmore()) {
                    /* <format> */
                    skip = line->atTokNextint(&err);
                    if (err >= 0 && skip == 2)
                    {
                        /* <oper> */
                        resp[2] = line->atTokNextstr(&err);
                        // logE(LOG_TAG, "EOPS Get operator code %s", resp[2]);

                        /* Modem might response invalid PLMN ex: "", "000000" , "??????", ignore it */
                        if ((!((*resp[2] >= '0') && (*resp[2] <= '9'))) ||
                                (0 == strcmp(resp[2], "000000"))) {
                            // logE(LOG_TAG, "EOPS got invalid plmn response");
                            memset(resp, 0, sizeof(resp));

                            response = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_SUCCESS,
                                    RfxStringsData(resp, 3), msg, false);
                            // response to TeleCore
                            responseToTelCore(response);
                            return;
                        }

                        err = getOperatorNamesFromNumericCode(
                                resp[2], longname, shortname, MAX_OPER_NAME_LENGTH);
                        String8 oper(resp[2]);
                        getMclStatusManager()->setString8Value(RFX_STATUS_KEY_OPERATOR, oper);

                        if(err >= 0)
                        {
                            resp[0] = longname;
                            resp[1] = shortname;
                        }

                        response = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_SUCCESS,
                                RfxStringsData(resp, 3), msg, false);
                        // response to TeleCore
                        responseToTelCore(response);
                        return;
                    }
                }
            }
        }
    }

    response = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_SUCCESS,
            RfxStringsData(resp, 3), msg, false);
    // response to TeleCore
    responseToTelCore(response);

    return;
error:
    logE(LOG_TAG, "requestOperator must never return error when radio is on");
    response = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_GENERIC_FAILURE,
            RfxStringsData(), msg, false);
    // response to TeleCore
    responseToTelCore(response);
}

void RmcNetworkRequestHandler::requestQueryNetworkSelectionMode(const sp<RfxMclMessage>& msg) {
    int err;
    sp<RfxAtResponse> p_response;
    sp<RfxMclMessage> response;
    int resp = 0;
    RfxAtLine* line;

    p_response = atSendCommandSingleline("AT+COPS?", "+COPS:");

    // check error
    err = p_response->getError();
    if (err != 0 ||
            p_response == NULL ||
            p_response->getSuccess() == 0 ||
            p_response->getIntermediates() == NULL) goto error;

    // handle intermediate
    line = p_response->getIntermediates();

    // go to start position
    line->atTokStart(&err);
    if (err < 0) goto error;

    resp = line->atTokNextint(&err);
    if (err < 0 || (resp < 0 || resp > 4 || resp == 3)) {
        goto error;
    }

    response = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_SUCCESS,
            RfxIntsData(&resp, 1), msg, false);
    // response to TeleCore
    responseToTelCore(response);
    return;
error:
    logE(LOG_TAG, "requestQueryNetworkSelectionMode must never return error when radio is on");
    response = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_GENERIC_FAILURE,
            RfxVoidData(), msg, false);
    // response to TeleCore
    responseToTelCore(response);
}

void RmcNetworkRequestHandler::requestQueryAvailableNetworks(const sp<RfxMclMessage>& msg) {
    int err, len, i, j, k, num, num_filter;
    sp<RfxAtResponse> p_response;
    sp<RfxMclMessage> resp;
    RfxAtLine* line;
    char **response = NULL, **response_filter = NULL;
    char *tmp, *block_p = NULL;
    char *lacStr = NULL;
    unsigned int lac = 0;

    // logD(LOG_TAG, "requestQueryAvailableNetworks set plmnListOngoing flag");
    mPlmnListOngoing = 1;
    p_response = atSendCommandSingleline("AT+COPS=?", "+COPS:");
    err = p_response->getError();
    if (err < 0 || p_response->getSuccess() == 0) {
        goto error;
    }

    line = p_response->getIntermediates();
    // count the number of operator
    tmp = line->getLine();
    len = strlen(tmp);
    for(i = 0, num = 0, num_filter = 0; i < len ; i++ ) {
        // here we assume that there is no nested ()
        if (tmp[i] == '(') {
            num++;
            num_filter++;
        } else if (tmp[i] == ',' && tmp[i+1] == ',') {
            break;
        }
    }

    // +COPS: (2,"Far EasTone","FET","46601",0),(...),...,,(0, 1, 3),(0-2)
    // go to start position
    line->atTokStart(&err);
    if (err < 0) goto error;

    response = (char **) calloc(1, sizeof(char*) * num *4); // for string, each one is 20 bytes
    block_p = (char *) calloc(1, num* sizeof(char)*4*MAX_OPER_NAME_LENGTH);
    lacStr = (char *) calloc(1, num* sizeof(char)*4+1);

    if (response == NULL || block_p == NULL || lacStr == NULL) {
        logE(LOG_TAG, "requestQueryAvailableNetworks calloc fail");
        goto error;
    }

    for (i = 0, j=0 ; i < num ; i++, j+=4) {
        /* get "(<stat>" */
        tmp = line->atTokNextstr(&err);
        if (err < 0) goto error;

        response[j+0] = &block_p[(j+0)*MAX_OPER_NAME_LENGTH];
        response[j+1] = &block_p[(j+1)*MAX_OPER_NAME_LENGTH];
        response[j+2] = &block_p[(j+2)*MAX_OPER_NAME_LENGTH];
        response[j+3] = &block_p[(j+3)*MAX_OPER_NAME_LENGTH];

        switch(tmp[1]) {
        case '0':
            sprintf(response[j+3], "unknown");
            break;
        case '1':
            sprintf(response[j+3], "available");
            break;
        case '2':
            sprintf(response[j+3], "current");
            break;
        case '3':
            sprintf(response[j+3], "forbidden");
            break;
        default:
            logE(LOG_TAG, "The %d-th <stat> is an invalid value!!!  : %d", i, tmp[1]);
            goto error;
        }

        /* get long name*/
        tmp = line->atTokNextstr(&err);
        if (err < 0) goto error;
        sprintf(response[j+0], "%s", tmp);

        /* get short name*/
        tmp = line->atTokNextstr(&err);
        if (err < 0) goto error;
        sprintf(response[j+1], "%s", tmp);

        /* get <oper> numeric code*/
        tmp = line->atTokNextstr(&err);
        if (err < 0) goto error;
        sprintf(response[j+2], "%s", tmp);

        // ALPS00353868 START
        /*plmn_list_format.  0: standard +COPS format , 1: standard +COPS format plus <lac> */
        if(mPlmn_list_format == 1) {
            /* get <lac> numeric code*/
                tmp = line->atTokNextstr(&err);
            if (err < 0) {
                logE(LOG_TAG, "No <lac> in +COPS response");
                goto error;
            }
            memcpy(&(lacStr[i*4]), tmp, 4);
            lac = (unsigned int) strtoul(tmp, NULL, 16);
        }
        // ALPS00353868 END

        len = strlen(response[j+2]);
        if (len == 5 || len == 6) {
            if (0 == strcmp(response[j+2], m_ril_nw_nitz_oper_code)) {
                err = getOperatorNamesFromNumericCode(
                        response[j+2], lac, response[j+0], response[j+1], MAX_OPER_NAME_LENGTH);
                if(err < 0) goto error;
            }
        } else {
            logE(LOG_TAG, "The length of the numeric code is incorrect");
            goto error;
        }

        /* get <AcT> 0 is "2G", 2 is "3G", 7 is "4G"*/
        tmp = line->atTokNextstr(&err);
        if (err < 0) goto error;

        // check if this plmn is redundant
        for (k=0; k < j; k+=4)
        {
            // compare numeric
            if (0 == strcmp(response[j+2], response[k+2])) {
                response[j+0] = response[j+1] = response[j+2] = response[j+3] = (char *)"";
                num_filter--;
                break;
            }
        }
    }

    // filter the response
    response_filter = (char**)calloc(1, sizeof(char*) * num_filter * 4);
    if (NULL == response_filter) {
        logE(LOG_TAG, "malloc response_filter failed");
        goto error;
    }

    for (i=0, j=0, k=0; i < num; i++, j+=4) {
        if (0 < strlen(response[j+2])) {
            response_filter[k+0] = response[j+0];
            response_filter[k+1] = response[j+1];
            response_filter[k+2] = response[j+2];
            response_filter[k+3] = response[j+3];
            k += 4;
        }
    }

    logD(LOG_TAG, "requestQueryAvailableNetworks sucess, clear plmnListOngoing and plmnListAbort flag");
    mPlmnListOngoing = 0;
    mPlmnListAbort =0; /* always clear here to prevent race condition scenario */
    resp = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_SUCCESS,
            RfxStringsData(response_filter, num_filter*4), msg, false);
    // response to TeleCore
    responseToTelCore(resp);
    free(response);
    free(response_filter);
    free(block_p);
    free(lacStr);
    return;
error:
    if(response) {
        logE(LOG_TAG, "FREE!!");
        if(block_p) free(block_p);
        free(response);
        if(lacStr) free(lacStr);
    }
    logE(LOG_TAG, "requestQueryAvailableNetworks must never return error when radio is on, plmnListAbort=%d",
            mPlmnListAbort);
    if (mPlmnListAbort == 1){
        resp = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_CANCELLED,
                RfxVoidData(), msg, false);
    } else {
        resp = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_MODEM_ERR,
                RfxVoidData(), msg, false);
    }
    mPlmnListOngoing = 0;
    mPlmnListAbort =0; /* always clear here to prevent race condition scenario */
    // response to TeleCore
    responseToTelCore(resp);
}

void RmcNetworkRequestHandler::requestQueryAvailableNetworksWithAct(const sp<RfxMclMessage>& msg) {
    int err, len, i, j, num;
    char **response = NULL;
    char *tmp, *block_p = NULL;
    char *lacStr = NULL;
    sp<RfxAtResponse> p_response;
    sp<RfxMclMessage> resp;
    RfxAtLine* line;
    unsigned int lac = 0;

    mPlmnListOngoing = 1;
    p_response = atSendCommandSingleline("AT+COPS=?", "+COPS:");
    err = p_response->getError();
    if (err < 0 || p_response->getSuccess() == 0) {
        goto error;
    }

    line = p_response->getIntermediates();
    // count the number of operator
    tmp = line->getLine();
    len = strlen(tmp);
    for(i = 0, num = 0; i < len ; i++ ) {
        // here we assume that there is no nested ()
        if (tmp[i] == '(') {
            num++;
        } else if (tmp[i] == ',' && tmp[i+1] == ',') {
            break;
        }
    }

    // +COPS: (2,"Far EasTone","FET","46601",0),(...),...,,(0, 1, 3),(0-2)
    line->atTokStart(&err);
    if (err < 0) goto error;

    response = (char **) calloc(1, sizeof(char*) * num *6); // for string, each one is 25 bytes
    block_p = (char *) calloc(1, num* sizeof(char)*6*MAX_OPER_NAME_LENGTH);
    lacStr = (char *) calloc(1, num* sizeof(char)*4+1);

    if (response == NULL || block_p == NULL || lacStr == NULL) {
        logE(LOG_TAG, "requestQueryAvailableNetworksWithAct calloc fail");
        goto error;
    }

    for (i = 0, j=0 ; i < num ; i++, j+=6) {
        /* get "(<stat>" */
        tmp = line->atTokNextstr(&err);
        if (err < 0) goto error;

        response[j+0] = &block_p[(j+0)*MAX_OPER_NAME_LENGTH];
        response[j+1] = &block_p[(j+1)*MAX_OPER_NAME_LENGTH];
        response[j+2] = &block_p[(j+2)*MAX_OPER_NAME_LENGTH];
        response[j+3] = &block_p[(j+3)*MAX_OPER_NAME_LENGTH];
        response[j+4] = &block_p[(j+4)*MAX_OPER_NAME_LENGTH];
        response[j+5] = &block_p[(j+5)*MAX_OPER_NAME_LENGTH];

        switch(tmp[1]) {
        case '0':
            sprintf(response[j+3], "unknown");
            break;
        case '1':
            sprintf(response[j+3], "available");
            break;
        case '2':
            sprintf(response[j+3], "current");
            break;
        case '3':
            sprintf(response[j+3], "forbidden");
            break;
        default:
            logE(LOG_TAG, "The %d-th <stat> is an invalid value!!! : %d", i, tmp[1]);
            goto error;
        }

        /* skip long name*/
        tmp = line->atTokNextstr(&err);
        if (err < 0) goto error;

        /* skip short name*/
        tmp = line->atTokNextstr(&err);
        if (err < 0) goto error;

        /* get <oper> numeric code*/
        tmp = line->atTokNextstr(&err);
        if (err < 0) goto error;
        sprintf(response[j+2], "%s", tmp);

        // ALPS00353868 START
        /*plmn_list_format.  0: standard +COPS format , 1: standard +COPS format plus <lac> */
        if (mPlmn_list_format == 1) {
            /* get <lac> numeric code*/
            tmp = line->atTokNextstr(&err);
            if (err < 0){
            logE(LOG_TAG, "No <lac> in +COPS response");
                goto error;
            }
            memcpy(&(lacStr[i*4]), tmp, 4);
            lac = (unsigned int) strtoul(tmp, NULL, 16);
            sprintf(response[j+4], "%s", tmp);
        }
        // ALPS00353868 END

        len = strlen(response[j+2]);
        if (len == 5 || len == 6) {
            err = getOperatorNamesFromNumericCode(
                      response[j+2], lac, response[j+0], response[j+1], MAX_OPER_NAME_LENGTH);
            if (err < 0) goto error;
        } else {
            logE(LOG_TAG, "The length of the numeric code is incorrect");
            goto error;
        }

        /* get <AcT> 0 is "2G", 2 is "3G", 7 is "4G"*/
        tmp = line->atTokNextstr(&err);
        if (err < 0) goto error;

        switch(tmp[0]) {
        case '0':
            sprintf(response[j+5], "2G");
            break;
        case '2':
            sprintf(response[j+5], "3G");
            break;
        case '7':    //for  LTE
            sprintf(response[j+5], "4G");
            break;
        default:
            logE(LOG_TAG, "The %d-th <Act> is an invalid value!!! : %d", i, tmp[1]);
            goto error;
        }
    }

    // ALPS00353868 START : save <lac1><lac2><lac3>.. in the property
    if (mPlmn_list_format == 1) {
        //logD(LOG_TAG, "Set lacStr %s to property", lacStr);
        //rfx_property_set(PROPERTY_GSM_CURRENT_COPS_LAC, lacStr);
    }
    // ALPS00353868 END

    logD(LOG_TAG, "requestQueryAvailableNetworksWithAct sucess, clear plmnListOngoing and plmnListAbort flag");
    mPlmnListOngoing = 0;
    mPlmnListAbort =0; /* always clear here to prevent race condition scenario */
    resp = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_SUCCESS,
            RfxStringsData(response, num*6), msg, false);
    // response to TeleCore
    responseToTelCore(resp);

    free(response);
    free(block_p);
    free(lacStr);
    return;
error:
    if (response) {
        logE(LOG_TAG, "FREE!!");
        if (block_p) free(block_p);
        free(response);
        if (lacStr) free(lacStr);
    }
    RFX_LOG_V(LOG_TAG, "requestQueryAvailableNetworksWithAct must never return error when radio is on, plmnListAbort=%d",
            mPlmnListAbort);
    if (mPlmnListAbort == 1){
        resp = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_CANCELLED,
                RfxVoidData(), msg, false);
    } else {
        resp = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_GENERIC_FAILURE,
                RfxVoidData(), msg, false);
    }
    mPlmnListOngoing = 0;
    mPlmnListAbort =0; /* always clear here to prevent race condition scenario */
    responseToTelCore(resp);
}

void RmcNetworkRequestHandler::requestSetNetworkSelectionAutomatic(const sp<RfxMclMessage>& msg) {
    int err;
    sp<RfxAtResponse> p_response;
    sp<RfxMclMessage> response;
    RIL_Errno ril_errno;
    char optr[PROPERTY_VALUE_MAX] = {0};

    // Use +EOPS to do auto selection mode if auto_selection.mode is 1.
    rfx_property_get("ril.nw.auto_selection.mode", optr, "0");
    if (strcmp("1", optr) == 0) {
        rfx_property_set("ril.nw.auto_selection.mode", "0");
        p_response = atSendCommand("AT+EOPS=0");
    } else {
        p_response = atSendCommand("AT+COPS=0");
    }

    err = p_response->getError();
    if (err < 0 || p_response->getSuccess() == 0) {
        ril_errno = RIL_E_REQUEST_NOT_SUPPORTED;
    } else {
        ril_errno = RIL_E_SUCCESS;
    }

    response = RfxMclMessage::obtainResponse(msg->getId(), ril_errno,
            RfxVoidData(), msg, false);
    // response to TeleCore
    responseToTelCore(response);
}

void RmcNetworkRequestHandler::requestSetNetworkSelectionManual(const sp<RfxMclMessage>& msg) {
    int err = 0;
    const char *numeric_code;
    sp<RfxAtResponse> p_response;
    sp<RfxMclMessage> response;
    RIL_Errno ril_errno = RIL_E_GENERIC_FAILURE;
    int len, i;

    numeric_code = (char*)(msg->getData()->getData());

    if (NULL == numeric_code) {
        logE(LOG_TAG, "numeric is null!");
        ril_errno = RIL_E_INVALID_ARGUMENTS;
        goto error;
    }

    len = strlen(numeric_code);
    if (len == 5 || len == 6) {
        // check if the numeric code is valid digit or not
        for(i = 0; i < len ; i++) {
            if(numeric_code[i] < '0' || numeric_code[i] > '9')
                break;
        }
        if (i == len) {
            if (err >= 0) {
                p_response = atSendCommand(String8::format("AT+COPS=1, 2, \"%s\"", numeric_code));
                err = p_response->getError();
                if (!(err < 0 || p_response->getSuccess() == 0)) {
                    ril_errno = RIL_E_SUCCESS;
                } else {
                    ril_errno = RIL_E_INVALID_STATE;
                }
            }
        } else {
            logE(LOG_TAG, "the numeric code contains invalid digits");
        }
    } else {
        logE(LOG_TAG, "the data length is invalid for Manual Selection");
    }

error:
    response = RfxMclMessage::obtainResponse(msg->getId(), ril_errno,
            RfxVoidData(), msg, false);
    // response to TeleCore
    responseToTelCore(response);
}

void RmcNetworkRequestHandler::requestSetNetworkSelectionManualWithAct(const sp<RfxMclMessage>& msg) {
    int err, len, i;;
    const char *numeric_code, *act, *mode;
    RIL_Errno ril_errno = RIL_E_GENERIC_FAILURE;
    bool isSemiAutoMode = false;
    sp<RfxAtResponse> p_response;
    sp<RfxMclMessage> response;
    const char **pReqData = (const char **)msg->getData()->getData();

    numeric_code = pReqData[0];
    act = pReqData[1];

    if (NULL == numeric_code || NULL == act) {
        logE(LOG_TAG, "numeric or act is null!");
        ril_errno = RIL_E_INVALID_ARGUMENTS;
        goto error;
    }

    if (msg->getData()->getDataLength()/sizeof(char *) == 3) {
        mode = pReqData[2];
        if (mode != NULL && !strcmp(mode, "1")) {
            logD(LOG_TAG, "Semi auto network selection mode");
            isSemiAutoMode = true;
        }
    }

    len = strlen(numeric_code);
    if (len == 5 || len == 6) {
        // check if the numeric code is valid digit or not
        for(i = 0; i < len ; i++) {
            if( numeric_code[i] < '0' || numeric_code[i] > '9')
                break;
        }

        if (i == len) {
            if (strlen(act) == 1 && '0' <= act[0] && act[0] <= '9') {
                if(isSemiAutoMode == true) {
                    p_response = atSendCommand(String8::format("AT+EOPS=5, 2, \"%s\", %s", numeric_code, act));
                } else {
                    p_response = atSendCommand(String8::format("AT+COPS=1, 2, \"%s\", %s", numeric_code, act));
                }
            } else {
                if (isSemiAutoMode == true) {
                    p_response = atSendCommand(String8::format("AT+EOPS=5, 2, \"%s\"", numeric_code));
                } else {
                    p_response = atSendCommand(String8::format("AT+COPS=1, 2, \"%s\"", numeric_code));
                }
            }
            err = p_response->getError();
            if (!(err < 0 || p_response->getSuccess() == 0)) {
                ril_errno = RIL_E_SUCCESS;
            }
        } else {
            logE(LOG_TAG, "the numeric code contains invalid digits");
        }
    } else {
        logE(LOG_TAG, "the data length is invalid for Manual Selection");
    }

error:
    response = RfxMclMessage::obtainResponse(msg->getId(), ril_errno,
            RfxVoidData(), msg, false);
    // response to TeleCore
    responseToTelCore(response);
}

void RmcNetworkRequestHandler::requestSetBandMode(const sp<RfxMclMessage>& msg) {
    int req, err, gsm_band, umts_band;
    unsigned int lte_band_1_32, lte_band_33_64;
    char *cmd;
    RIL_Errno ril_errno = RIL_E_REQUEST_NOT_SUPPORTED;
    sp<RfxAtResponse> p_response;
    sp<RfxMclMessage> resp;
    int *pInt = (int *)msg->getData()->getData();

    req = pInt[0];
    switch (req) {
        case BM_AUTO_MODE: //"unspecified" (selected by baseband automatically)
            gsm_band = 0xff;
            umts_band = 0xffff;
            break;
        case BM_EURO_MODE: //"EURO band" (GSM-900 / DCS-1800 / WCDMA-IMT-2000)
            gsm_band = GSM_BAND_900 | GSM_BAND_1800;
            umts_band = UMTS_BAND_I;
            break;
        case BM_US_MODE: //"US band" (GSM-850 / PCS-1900 / WCDMA-850 / WCDMA-PCS-1900)
            gsm_band = GSM_BAND_850 | GSM_BAND_1900;
            umts_band = UMTS_BAND_II | UMTS_BAND_V;
            break;
        case BM_JPN_MODE: //"JPN band" (WCDMA-800 / WCDMA-IMT-2000)
            gsm_band = 0;
            umts_band = UMTS_BAND_I | UMTS_BAND_VI;
            break;
        case BM_AUS_MODE: //"AUS band" (GSM-900 / DCS-1800 / WCDMA-850 / WCDMA-IMT-2000)
            gsm_band = GSM_BAND_900 | GSM_BAND_1800;
            umts_band = UMTS_BAND_I | UMTS_BAND_V;
            break;
        case BM_AUS2_MODE: //"AUS band 2" (GSM-900 / DCS-1800 / WCDMA-850)
            gsm_band = GSM_BAND_900 | GSM_BAND_1800;
            umts_band = UMTS_BAND_V;
            break;
        case BM_40_BROKEN:
        case BM_CELLULAR_MODE: //"Cellular (800-MHz Band)"
        case BM_PCS_MODE: //"PCS (1900-MHz Band)"
        case BM_CLASS_3: //"Band Class 3 (JTACS Band)"
        case BM_CLASS_4: //"Band Class 4 (Korean PCS Band)"
        case BM_CLASS_5: //"Band Class 5 (450-MHz Band)"
        case BM_CLASS_6: // "Band Class 6 (2-GMHz IMT2000 Band)"
        case BM_CLASS_7: //"Band Class 7 (Upper 700-MHz Band)"
        case BM_CLASS_8: //"Band Class 8 (1800-MHz Band)"
        case BM_CLASS_9: //"Band Class 9 (900-MHz Band)"
        case BM_CLASS_10: //"Band Class 10 (Secondary 800-MHz Band)"
        case BM_CLASS_11: //"Band Class 11 (400-MHz European PAMR Band)"
        case BM_CLASS_15: //"Band Class 15 (AWS Band)"
        case BM_CLASS_16: //"Band Class 16 (US 2.5-GHz Band)"
        default:
            gsm_band = -1;
            umts_band = -1;
            break;
    }

    if (gsm_band != -1 && umts_band != -1) {
        /******************************************************
        * If the modem doesn't support certain group of bands, ex. GSM or UMTS
        * It might just ignore the parameter.
        *******************************************************/
        p_response = atSendCommand(String8::format("AT+EPBSE=%d, %d", gsm_band, umts_band));
        if (p_response->getError() >= 0 && p_response->getSuccess() != 0) {
            ril_errno = RIL_E_SUCCESS;
        }
    } else if (req == BM_40_BROKEN) {
        lte_band_1_32 = pInt[1];
        lte_band_33_64 = pInt[2];
        p_response = atSendCommand(String8::format("AT+EPBSE=,,%u,%u", lte_band_1_32, lte_band_33_64));
        if (p_response->getError() >= 0 && p_response->getSuccess() != 0) {
            ril_errno = RIL_E_SUCCESS;
        }
    } else if (req == BM_FOR_DESENSE_RADIO_ON || req == BM_FOR_DESENSE_RADIO_OFF
            || req == BM_FOR_DESENSE_RADIO_ON_ROAMING || req == BM_FOR_DESENSE_B8_OPEN) {
        requestQueryCurrentBandMode();
        int c2k_radio_on;
        int need_config_umts = 0;
        int force_switch = pInt[1];
        gsm_band = bands[0];
        umts_band = bands[1];
        lte_band_1_32 = bands[2];
        lte_band_33_64 = bands[3];
        logE(LOG_TAG, "BM FOR DESENCE, gsm_band:%d, umts_band : %d, lte_band_1_32 : %d, lte_band_33_64: %d, req: %d ",
                gsm_band, umts_band, lte_band_1_32, lte_band_33_64, req);
        if (req == BM_FOR_DESENSE_RADIO_ON) {
            if (umts_band & 0x00000080) {
                need_config_umts = 1;
                umts_band = umts_band & 0xffffff7f;
            }
        } else {
            if ((umts_band & 0x00000080) == 0) {
                need_config_umts = 1;
                umts_band = umts_band | 0x00000080;
            }
        }
        if (req == BM_FOR_DESENSE_RADIO_OFF) {
            c2k_radio_on = 0;
        } else {
            c2k_radio_on = 1;
        }
        logE(LOG_TAG, "BM FOR DESENCE, need_config_umts: %d, force_switch : %d", need_config_umts, force_switch);
        if (need_config_umts == 1 || force_switch == 1) {
            int skipDetach = 0;
            int detachCount = 0;
            while (skipDetach == 0 && detachCount < 10) {
                if (req == BM_FOR_DESENSE_B8_OPEN) {
                    p_response = atSendCommand(String8::format("AT+EPBSE=%d,%d,%d,%d", gsm_band, umts_band,
                        lte_band_1_32, lte_band_33_64));
                } else {
                    p_response = atSendCommand(String8::format("AT+EPBSE=%d,%d,%d,%d,%d", gsm_band, umts_band,
                        lte_band_1_32, lte_band_33_64, c2k_radio_on));
                }
                if (p_response->getError() >= 0 && p_response->getSuccess() != 0) {
                    logE(LOG_TAG, "Set band mode: success");
                    skipDetach = 1;
                    ril_errno = RIL_E_SUCCESS;
                } else {
                    detachCount++;
                    logE(LOG_TAG, "Set band mode: fail, count=%d", detachCount);
                    sleep(1);
                }
            }
        }
    }
    resp = RfxMclMessage::obtainResponse(msg->getId(), ril_errno,
            RfxVoidData(), msg, false);
    responseToTelCore(resp);
}

void RmcNetworkRequestHandler::requestQueryCurrentBandMode() {
    int err, gsm_band, umts_band;
    sp<RfxAtResponse> p_response;
    RfxAtLine* line;

    p_response = atSendCommandSingleline("AT+EPBSE?", "+EPBSE:");
    if (p_response->getError() < 0 || p_response->getSuccess() == 0) {
        logE(LOG_TAG, "Query current band mode: fail, err=%d", p_response->getError());
        return;
    }

    line = p_response->getIntermediates();

    line->atTokStart(&err);
    if (err < 0) return;

    // get supported GSM bands
    gsm_band = line->atTokNextint(&err);
    if (err < 0) return;

    // get supported UMTS bands
    umts_band = line->atTokNextint(&err);
    if (err < 0) return;

    bands[0] = gsm_band;
    bands[1] = umts_band;
    bands[2] = line->atTokNextint(&err);
    bands[3] = line->atTokNextint(&err);
    logE(LOG_TAG, "requestQueryCurrentBandMode, gsm_band:%d, umts_band : %d, lte_band_1_32 : %d, lte_band_33_64: %d",
            bands[0], bands[1], bands[2], bands[3]);
}

void RmcNetworkRequestHandler::requestQueryAvailableBandMode(const sp<RfxMclMessage>& msg) {
    int err, gsm_band, umts_band;
    int band_mode[10], index=1;
    sp<RfxAtResponse> p_response;
    sp<RfxMclMessage> resp;
    RfxAtLine* line;

    p_response = atSendCommandSingleline("AT+EPBSE?", "+EPBSE:");

    if (p_response->getError() < 0 || p_response->getSuccess() == 0)
        goto error;

    line = p_response->getIntermediates();

    line->atTokStart(&err);
    if (err < 0) goto error;

    // get supported GSM bands
    gsm_band = line->atTokNextint(&err);
    if (err < 0) goto error;

    // get supported UMTS bands
    umts_band = line->atTokNextint(&err);
    if (err < 0) goto error;

    //0 for "unspecified" (selected by baseband automatically)
    band_mode[index++] = BM_AUTO_MODE;

    if (gsm_band !=0 || umts_band != 0) {
        // 1 for "EURO band" (GSM-900 / DCS-1800 / WCDMA-IMT-2000)
        if ((gsm_band == 0 || (gsm_band | GSM_BAND_900 | GSM_BAND_1800) == gsm_band) &&
                (umts_band == 0 || (umts_band | UMTS_BAND_I) == umts_band)) {
            band_mode[index++] = BM_EURO_MODE;
        }

        // 2 for "US band" (GSM-850 / PCS-1900 / WCDMA-850 / WCDMA-PCS-1900)
        if ((gsm_band == 0 || (gsm_band | GSM_BAND_850 | GSM_BAND_1900) == gsm_band) &&
                (umts_band == 0 || (umts_band | UMTS_BAND_II | UMTS_BAND_V) == umts_band)) {
            band_mode[index++] = BM_US_MODE;
        }

        // 3 for "JPN band" (WCDMA-800 / WCDMA-IMT-2000)
        if ((umts_band | UMTS_BAND_I | UMTS_BAND_VI) == umts_band) {
            band_mode[index++] = BM_JPN_MODE;
        }

        // 4 for "AUS band" (GSM-900 / DCS-1800 / WCDMA-850 / WCDMA-IMT-2000)
        if ((gsm_band == 0 || (gsm_band | GSM_BAND_900 | GSM_BAND_1800)==gsm_band) &&
                (umts_band == 0 || (umts_band | UMTS_BAND_I | UMTS_BAND_V)==umts_band)) {
            band_mode[index++] = BM_AUS_MODE;
        }

        // 5 for "AUS band 2" (GSM-900 / DCS-1800 / WCDMA-850)
        if ((gsm_band == 0 || (gsm_band | GSM_BAND_900 | GSM_BAND_1800)==gsm_band) &&
                (umts_band == 0 || (umts_band | UMTS_BAND_V)==umts_band)) {
            band_mode[index++] = BM_AUS2_MODE;
        }
    }
    band_mode[0] = index - 1;
    resp = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_SUCCESS,
            RfxIntsData(band_mode, index), msg, false);
    responseToTelCore(resp);
    return;

error:
    resp = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_GENERIC_FAILURE,
            RfxVoidData(), msg, false);
    responseToTelCore(resp);
}


void RmcNetworkRequestHandler::requestGetNeighboringCellIds(const sp<RfxMclMessage>& msg) {
    int err, skip, nt_type;
    sp<RfxAtResponse> p_response;
    sp<RfxMclMessage> resp;
    RfxAtLine* line;

    int rat,rssi,ci,lac,psc;
    int i = 0;
    int j = 0;
    RIL_NeighboringCell nbr[6];
    RIL_NeighboringCell *p_nbr[6];

    // logD(LOG_TAG, "Enter requestGetNeighboringCellIds()");
    p_response = atSendCommandMultiline("AT+ENBR", "+ENBR:");
    err = p_response->getError();

    if (err < 0 || p_response->getSuccess() == 0)
        goto error;

    line = p_response->getIntermediates();
    while(line != NULL) {
        line->atTokStart(&err);
        if (err < 0) goto error;

        rat = line->atTokNextint(&err);
        if (err < 0) goto error;

        rssi = line->atTokNextint(&err);
        if (err < 0) goto error;

        if (((rat == 1) && (rssi < 0 || rssi > 31) && (rssi != 99))
                || ((rat == 2) && (rssi < 0 || rssi > 91))) {
            logE(LOG_TAG, "The rssi of the %d-th is invalid: %d", i, rssi);
            goto error;
        }

        nbr[i].rssi = rssi;

        if (rat == 1) {
            ci = line->atTokNextint(&err);
            if (err < 0) goto error;

            lac = line->atTokNextint(&err);
            if (err < 0) goto error;

            err = asprintf(&nbr[i].cid, "%04X%04X", lac, ci);
            if (err < 0) {
                logE(LOG_TAG, "Using asprintf and getting ERROR");
                goto error;
            }

            //ALPS00269882 : to bring 'rat' info without changing the interface between RILJ (for backward compatibility concern)
            rfx_property_set(PROPERTY_GSM_CURRENT_ENBR_RAT, "1"); //NETWORK_TYPE_GPRS = 1
            logD(LOG_TAG, "CURRENT_ENBR_RAT 1 :: NC[%d], rssi:%d, cid:%s", i, nbr[i].rssi, nbr[i].cid);
        } else if (rat == 2) {
            psc = line->atTokNextint(&err);
            if (err < 0) goto error;

            err = asprintf(&nbr[i].cid, "%08X", psc);
            if (err < 0) {
                logE(LOG_TAG, "Using asprintf and getting ERROR");
                goto error;
            }

            //ALPS00269882 : to bring 'rat' info without changing the interface between RILJ (for backward compatibility concern)
            rfx_property_set(PROPERTY_GSM_CURRENT_ENBR_RAT, "3"); //NETWORK_TYPE_UMTS = 3
            logD(LOG_TAG, "CURRENT_ENBR_RAT 3 :: NC[%d], rssi:%d, psc:%d", i, rssi, psc);
        } else {
            goto error;
        }
        p_nbr[i] = &nbr[i];
        i++;
        line = line->getNext();
    }

    resp = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_SUCCESS,
            RfxNeighboringCellData(p_nbr, i), msg, false);
    responseToTelCore(resp);
    for(j=0;j<i;j++)
        free(nbr[j].cid);
    return;

error:
    logE(LOG_TAG, "requestGetNeighboringCellIds has error occur!!");
    resp = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_GENERIC_FAILURE,
            RfxVoidData(), msg, false);
    responseToTelCore(resp);
    for(j=0;j<i;j++)
        free(nbr[j].cid);
}

void RmcNetworkRequestHandler::requestSetLocationUpdates(const sp<RfxMclMessage>& msg) {
    int enabled;
    RIL_Errno err_no = RIL_E_GENERIC_FAILURE;
    sp<RfxAtResponse> p_response;
    sp<RfxMclMessage> resp;
    int *pInt = (int *)msg->getData()->getData();

    enabled = pInt[0];
    if (enabled == 1 || enabled == 0) {
        p_response = atSendCommand(String8::format("AT+CREG=%d", enabled ? 2 : 1));
        if (p_response->getError() >= 0 && p_response->getSuccess() > 0) {
            err_no = RIL_E_SUCCESS;
        }
    }

    resp = RfxMclMessage::obtainResponse(msg->getId(), err_no,
            RfxVoidData(), msg, false);
    responseToTelCore(resp);
}

void RmcNetworkRequestHandler::requestGetCellInfoList(const sp<RfxMclMessage>& msg) {
    int err = 0;
    int num = 0;
    RIL_CellInfo_v12 *response = NULL;
    sp<RfxAtResponse> p_response;
    sp<RfxMclMessage> resp;
    RfxAtLine* line;

    p_response = atSendCommandSingleline("AT+ECELL", "+ECELL:");

    // +ECELL: <num_of_cell>...
    if (p_response->getError() < 0 || p_response->getSuccess() == 0)
        goto error;

    line = p_response->getIntermediates();

    line->atTokStart(&err);
    if (err < 0) goto error;

    num = line->atTokNextint(&err);
    if (err < 0) goto error;
    if (num < 1) {
        logD(LOG_TAG, "No cell info listed, num=%d", num);
        goto error;
    }
    // logD(LOG_TAG, "Cell info listed, number=%d",num);

    response = (RIL_CellInfo_v12 *) alloca(num * sizeof(RIL_CellInfo_v12));
    memset(response, 0, num * sizeof(RIL_CellInfo_v12));

    err = getCellInfoListV12(line, num, response);
    if (err < 0) goto error;

    resp = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_SUCCESS,
            RfxCellInfoData(response, num * sizeof(RIL_CellInfo_v12)), msg, false);
    responseToTelCore(resp);
    return;

error:
    logE(LOG_TAG, "requestGetCellInfoList must never return error when radio is on");
    resp = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_NO_NETWORK_FOUND,
            RfxVoidData(), msg, false);
    responseToTelCore(resp);
}

void RmcNetworkRequestHandler::requestSetCellInfoListRate(const sp<RfxMclMessage>& msg) {
    int time = -1;
    RIL_Errno err_no = RIL_E_GENERIC_FAILURE;
    sp<RfxAtResponse> p_response;
    sp<RfxMclMessage> resp;
    int *pInt = (int *)msg->getData()->getData();

    time = pInt[0];

    // logE(LOG_TAG, "requestSetCellInfoListRate:%d", time);
    if (time == 0) {
        p_response = atSendCommand(String8::format("AT+ECELL=1"));
    } else if (time > 0 && time <= 0x7fffffff) {
        p_response = atSendCommand(String8::format("AT+ECELL=0"));
    } else {
        goto finish;
    }

    if (p_response->getError() >= 0 &&
            p_response->getSuccess() > 0) {
        err_no = RIL_E_SUCCESS;
    }

finish:
    resp = RfxMclMessage::obtainResponse(msg->getId(), err_no,
            RfxVoidData(), msg, false);
    responseToTelCore(resp);
}

void RmcNetworkRequestHandler::requestGetPOLCapability(const sp<RfxMclMessage>& msg) {
    int err;
    char *mClose, *mOpen, *mHyphen;
    int result[4] = {0};
    sp<RfxAtResponse> p_response;
    sp<RfxMclMessage> resp;
    RfxAtLine* line;

    p_response = atSendCommandSingleline("AT+CPOL=?", "+CPOL:");

    if (p_response->getError() < 0) {
        logE(LOG_TAG, "requestGetPOLCapability Fail");
        goto error;
    }

    if (p_response->getSuccess() == 0) {
        logE(LOG_TAG, "CME ERROR: %d/n", p_response->atGetCmeError());
        goto error;
    }

    //+CPOL: (<bIndex>-<eIndex>), (<bformatValue>-<eformatValue>)
    line = p_response->getIntermediates();

    // AT< +CPOL: (0-39), (0-2)
    line->atTokStart(&err);
    if (err < 0) goto error;

    // AT< +CPOL: (0-39), (0-2)
    //            ^
    mOpen = line->atTokChar(&err);
    if (err < 0) goto error;

    mHyphen = strchr(mOpen, '-');
    if (mHyphen != NULL && mOpen < mHyphen ) {
        // AT< +CPOL: (0-39), (0-2)
        //             ^
        result[0] = strtol((mOpen+1), NULL, 10);
        logD(LOG_TAG, "requestGetPOLCapability result 0: %d", result[0]);
    } else {
        goto error;
    }

    mClose = strchr(mHyphen, ')');
    if (mClose != NULL && mHyphen < mClose) {
        // AT< +CPOL: (0-39), (0-2)
        //               ^^
        result[1] = strtol((mHyphen+1), NULL, 10);
        logD(LOG_TAG, "requestGetPOLCapability result 1: %d", result[1]);
    } else {
        goto error;
    }

    // AT< +CPOL: (0-39), (0-2)
    //                    ^
    mOpen = line->atTokChar(&err);
    if (err < 0) goto error;

    mHyphen = strchr(mOpen, '-');
    if (mHyphen != NULL && mOpen < mHyphen ) {
        // AT< +CPOL: (0-39), (0-2)
        //                     ^
        result[2] = strtol((mOpen+1), NULL, 10);
        logD(LOG_TAG, "requestGetPOLCapability result 2: %d", result[2]);
    } else {
        goto error;
    }

    mClose = strchr(mHyphen, ')');
    if (mClose != NULL && mHyphen < mClose) {
        // AT< +CPOL: (0-39), (0-2)
        //                       ^
        result[3] = strtol((mHyphen+1), NULL, 10);
        logD(LOG_TAG, "requestGetPOLCapability result 3: %d", result[3]);
    } else {
        goto error;
    }

    logD(LOG_TAG, "requestGetPOLCapability: %d %d %d %d", result[0],
            result[1], result[2], result[3]);

    resp = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_SUCCESS,
            RfxIntsData(result, 4), msg, false);
    responseToTelCore(resp);
    return;

error:
    logD(LOG_TAG, "requestGetPOLCapability: goto error");
    resp = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_GENERIC_FAILURE,
            RfxVoidData(), msg, false);
    responseToTelCore(resp);
}

void RmcNetworkRequestHandler::requestGetPOLList(const sp<RfxMclMessage>& msg) {
    int err, i, j, count, len, nAct, tmpAct;
    char **response = NULL;
    char *tmp, *block_p = NULL;
    sp<RfxAtResponse> p_response;
    RfxAtLine *p_cur;
    sp<RfxMclMessage> resp;

    p_response = atSendCommandMultiline("AT+CPOL?", "+CPOL:");
    err = p_response->getError();
    if (err < 0 || p_response->getSuccess() == 0) {
        goto error;
    }

    /* count the entries */
    for (count = 0, p_cur = p_response->getIntermediates()
            ; p_cur != NULL
            ; p_cur = p_cur->getNext()) {
        count++;
    }
    logD(LOG_TAG, "requestGetPOLList!! count is %d", count);

    response = (char **) calloc(1, sizeof(char*) * count *4); // for string, each one is 25 bytes
    /* In order to support E-UTRAN, nAct will be 2 digital,
    changed from 60 to 62 for addition 1 digital and buffer.*/
    block_p = (char *) calloc(1, count* sizeof(char)*62);

    if (response == NULL || block_p == NULL) {
        logE(LOG_TAG, "requestGetPOLList calloc fail");
        goto error;
    }
    //+CPOL: <index>, <format>, <oper>, <GSM_Act>, <GSM_Compact_Act>, <UTRAN_Act>, <E-UTRAN Act>
    for (i = 0,j=0, p_cur = p_response->getIntermediates()
            ; p_cur != NULL
            ; p_cur = p_cur->getNext(), i++,j+=4) {
        logD(LOG_TAG, "requestGetPOLList!! line is %s", p_cur->getLine());

        p_cur->atTokStart(&err);
        if (err < 0) goto error;

        /* get index*/
        tmp = p_cur->atTokNextstr(&err);
        if (err < 0) goto error;

        response[j+0] = &block_p[i*62];
        response[j+1] = &block_p[i*62+8];
        response[j+2] = &block_p[i*62+10];
        response[j+3] = &block_p[i*62+58];

        sprintf(response[j+0], "%s", tmp);

        logD(LOG_TAG, "requestGetPOLList!! index is %s",response[j+0]);
        /* get format*/
        tmp = p_cur->atTokNextstr(&err);
        if (err < 0) goto error;

        sprintf(response[j+1], "%s", tmp);
        logD(LOG_TAG, "requestGetPOLList!! format is %s",response[j+1]);
        /* get oper*/
        tmp = p_cur->atTokNextstr(&err);
        if (err < 0) goto error;

        sprintf(response[j+2], "%s", tmp);
        logD(LOG_TAG, "requestGetPOLList!! oper is %s",response[j+2]);
        nAct = 0;

        if(p_cur->atTokHasmore()) {
            /* get <GSM AcT> */
            tmpAct = p_cur->atTokNextint(&err);
            if (err < 0) goto error;

            if (tmpAct == 1) {
                nAct = 1;
            }

            /*get <GSM compact AcT> */
            tmpAct = p_cur->atTokNextint(&err);
            if (err < 0) goto error;

            if(tmpAct == 1) {
                nAct |= 0x02;
            }

            /*get <UTRAN AcT> */
            tmpAct = p_cur->atTokNextint(&err);
            if (err < 0) goto error;

            if (tmpAct == 1) {
                nAct |= 0x04;
            }

            /*get <E-UTRAN AcT> */
            if (p_cur->atTokHasmore()) {
                logD(LOG_TAG, "get E-UTRAN AcT");
                tmpAct = p_cur->atTokNextint(&err);
                if (err < 0) goto error;

                if(tmpAct == 1) {
                    nAct |= 0x08;
                }
            }
        }
        /* ALPS00368351 To distinguish SIM file without <AcT> support, we set AcT to zero */
        // if(nAct == 0) { nAct = 1;} // No act value for SIM. set to GSM
        logD(LOG_TAG, "Act = %d",nAct);
        sprintf(response[j+3], "%d", nAct);
        logD(LOG_TAG, "requestGetPOLList!! act is %s",response[j+3]);
    }
    resp = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_SUCCESS,
            RfxStringsData(response, count*4), msg, false);
    responseToTelCore(resp);
    free(response);
    free(block_p);
    return;

error:
    logE(LOG_TAG, "requestGetPOLList return error");
    if (response) {
        if (block_p) free(block_p);
        free(response);
    }
    resp = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_GENERIC_FAILURE,
            RfxVoidData(), msg, false);
    responseToTelCore(resp);
}

void RmcNetworkRequestHandler::requestSetPOLEntry(const sp<RfxMclMessage>& msg) {
    int i;
    int nAct[4] = {0};
    int nActTmp = 0;
    const char **strings = (const char **)msg->getData()->getData();
    RIL_Errno ret = RIL_E_GENERIC_FAILURE;

    sp<RfxAtResponse> p_response;
    RfxAtLine *p_cur;
    sp<RfxMclMessage> resp;

    if (msg->getData()->getDataLength() < (int)(3 * sizeof(char*))) {
        logE(LOG_TAG, "requestSetPOLEntry no enough input params. datalen is %d, size of char* is %d",
                msg->getData()->getDataLength(), sizeof(char*));
        p_response = atSendCommand(String8::format("AT+CPOL=%s", strings[0]));
    } else if (strings[1] == NULL) { // no PLMN, then clean the entry
        p_response = atSendCommand(String8::format("AT+CPOL=%s", strings[0]));
    } else {
        nActTmp = atoi(strings[2]);
        logD(LOG_TAG, "requestSetPOLEntry Act = %d", nActTmp);

        for (i = 0; i < 4; i++) {
            if (((nActTmp >> i) & 1) == 1) {
                logD(LOG_TAG, "i = %d",i);
                nAct[i] = 1;
            }
        }

        /* ALPS00368351: To update file without <AcT> support, modem suggest not to set any nAcT parameter */
        if (nActTmp == 0) {
            logD(LOG_TAG, "requestSetPOLEntry no Act assigned,strings[2]=%s",strings[2]);
            p_response = atSendCommand(String8::format("AT+CPOL=%s,2,\"%s\"", strings[0], strings[1]));
        } else {
            logD(LOG_TAG, "R8, MOLY and LR9 can supoort 7 arguments");
            p_response = atSendCommand(String8::format("AT+CPOL=%s,2,\"%s\",%d,%d,%d,%d", strings[0], strings[1], nAct[0], nAct[1], nAct[2], nAct[3]));
        }
    }
    if (p_response->getError() < 0) {
        logE(LOG_TAG, "requestSetPOLEntry Fail");
        goto finish;
    }

    if (p_response->getSuccess() == 0) {
        switch (p_response->atGetCmeError()) {
            logD(LOG_TAG, "p_response = %d/n", p_response->atGetCmeError());
            case CME_SUCCESS:
                ret = RIL_E_GENERIC_FAILURE;
            break;
            case CME_UNKNOWN:
                logD(LOG_TAG, "p_response: CME_UNKNOWN");
            break;
            default:
            break;
        }
    } else {
        ret = RIL_E_SUCCESS;
    }

finish:
    resp = RfxMclMessage::obtainResponse(msg->getId(), ret,
            RfxVoidData(), msg, false);
    responseToTelCore(resp);
}

void RmcNetworkRequestHandler::requestSetCdmaRoamingPreference(const sp<RfxMclMessage>& msg) {
    int err;
    int reqRoamingType = -1;
    int roamingType = -1;
    RIL_Errno ril_errno = RIL_E_REQUEST_NOT_SUPPORTED;
    sp<RfxAtResponse> p_response;
    sp<RfxMclMessage> response;

    int *pInt = (int *) msg->getData()->getData();
    reqRoamingType = pInt[0];

    // AT$ROAM=<type>
    // <type>=0: set the device to Sprint only mode
    // <type>=1: set the device to automatic mode

    if (reqRoamingType == CDMA_ROAMING_MODE_HOME) {
        // for Home Networks only
        roamingType = 0;
    } else if (reqRoamingType == CDMA_ROAMING_MODE_ANY) {
        // for Roaming on Any Network
        roamingType = 1;
    } else {
        logE(LOG_TAG, "requestSetCdmaRoamingPreference, Not support reqRoamingType=%d", reqRoamingType);
    }

    if (roamingType >= 0) {
        p_response = atSendCommand(String8::format("AT$ROAM=%d", roamingType));
        err = p_response->getError();
        if (err != 0 || p_response == NULL || p_response->getSuccess() == 0) {
            logE(LOG_TAG, "requestSetCdmaRoamingPreference error, reqRoamingType=%d", reqRoamingType);
        } else {
            ril_errno = RIL_E_SUCCESS;
        }
    }

    response = RfxMclMessage::obtainResponse(msg->getId(), ril_errno, RfxVoidData(), msg, false);
    responseToTelCore(response);
}

void RmcNetworkRequestHandler::requestQueryCdmaRoamingPreference(const sp<RfxMclMessage>& msg) {
    int err;
    int roamingType = -1;
    sp<RfxAtResponse> p_response;
    sp<RfxMclMessage> response;
    RfxAtLine* line;

    // AT$ROAM=<type>
    // <type>=0: set the device to Sprint only mode
    // <type>=1: set the device to automatic mode

    p_response = atSendCommandSingleline("AT$ROAM?", "$ROAM:");
    err = p_response->getError();
    if (err != 0 ||
            p_response == NULL ||
            p_response->getSuccess() == 0 ||
            p_response->getIntermediates() == NULL) goto error;

    line = p_response->getIntermediates();

    line->atTokStart(&err);
    if (err < 0) goto error;

    // <type>
    roamingType = line->atTokNextint(&err);
    if (err < 0) goto error;

    if (roamingType == 0) {
        // for Home Networks only
        roamingType = CDMA_ROAMING_MODE_HOME;
    } else if (roamingType == 1) {
        // for Roaming on Any Network
        roamingType = CDMA_ROAMING_MODE_ANY;
    } else {
        logE(LOG_TAG, "requestQueryCdmaRoamingPreference, Not support roamingType=%d", roamingType);
        goto error;
    }

    response = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_SUCCESS,
            RfxIntsData(&roamingType, 1), msg, false);
    responseToTelCore(response);
    return;
error:
    logE(LOG_TAG, "requestQueryCdmaRoamingPreference error");
    response = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_REQUEST_NOT_SUPPORTED,
            RfxIntsData(&roamingType, 1), msg, false);
    responseToTelCore(response);
}

void RmcNetworkRequestHandler::updateSignalStrength()
{
    sp<RfxAtResponse> p_response;
    RfxAtLine* p_cur;
    int err_cnt = 0;
    int err;
    sp<RfxMclMessage> urc;
    int resp[14] = {0};
    pthread_mutex_lock(&s_signalStrengthMutex[m_slot_id]);
    //resetSignalStrengthCache(&signal_strength_cache[m_slot_id], CACHE_GROUP_ALL);

    // send AT command
    p_response = atSendCommandMultiline("AT+ECSQ", "+ECSQ:");

    // check error
    err = p_response->getError();
    if (err != 0 ||
            p_response == NULL ||
            p_response->getSuccess() == 0 ||
            p_response->getIntermediates() == NULL) {
        goto error;
    }

    for (p_cur = p_response->getIntermediates()
         ; p_cur != NULL
         ; p_cur = p_cur->getNext()
         ) {
        err = getSignalStrength(p_cur);

        if (err != 0)
            continue;
    }

    // copy signal strength cache to int array
    memcpy(&resp, &signal_strength_cache[m_slot_id], 14*sizeof(int));
    pthread_mutex_unlock(&s_signalStrengthMutex[m_slot_id]);

    logD(LOG_TAG, "%s, sig1:%d, sig2:%d, cdma_dbm:%d, cdma_ecio:%d, evdo_dbm:%d,"
            "evdo_ecio:%d, evdo_snr:%d, lte_sig:%d, lte_rsrp:%d, lte_rsrq:%d, lte_rssnr:%d, lte_cqi:%d,"
            "lte_timing_advance:%d, td_scdma_rscp:%d",
            __FUNCTION__, resp[0], resp[1], resp[2], resp[3], resp[4], resp[5], resp[6], resp[7],
             resp[8], resp[9], resp[10], resp[11], resp[12], resp[13]);

    // returns the whole cache, including GSM, WCDMA, TD-SCDMA, CDMA, EVDO, LTE
    urc = RfxMclMessage::obtainUrc(RFX_MSG_URC_SIGNAL_STRENGTH,
            m_slot_id, RfxIntsData(resp, 14));
    // response to TeleCore
    responseToTelCore(urc);

    return;

error:
    pthread_mutex_unlock(&s_signalStrengthMutex[m_slot_id]);
    logE(LOG_TAG, "updateSignalStrength ERROR: %d", err);
}

void RmcNetworkRequestHandler::setUnsolResponseFilterSignalStrength(bool enable)
{
    sp<RfxAtResponse> p_response;

    // The ePDG/RNS framework need to monitoring the LTE RSRP signal strength across the threshold.
    char threshold[PROPERTY_VALUE_MAX] = {0};

    if (enable) {
        // enable
        /* Enable get ECSQ URC */
        p_response = atSendCommand("AT+ECSQ=1");
        if (p_response->getError() != 0 || p_response->getSuccess() == 0)
            logW(LOG_TAG, "There is something wrong with the exectution of AT+ECSQ=1");

        // The ePDG/RNS framework need to monitoring the LTE RSRP signal strength across the threshold.
        // So we send command to adjust signaling threshold to MD1 whenever screen on/off.
        rfx_property_get("net.handover.thlte", threshold, "");
        /*
        if (strlen(threshold) == 0) {
            logD(LOG_TAG, "net.handover.thlte is empty");
        }
        */
    } else {
        // disable
        /* Disable get ECSQ URC */
        p_response = atSendCommand("AT+ECSQ=0");
        if (p_response->getError() != 0 || p_response->getSuccess() == 0)
            logW(LOG_TAG, "There is something wrong with the exectution of AT+ECSQ=0");

        // The ePDG/RNS framework need to monitoring the LTE RSRP signal strength across the threshold.
        // So we send command to adjust signaling threshold to MD1 whenever screen on/off.
        rfx_property_get("net.handover.thlte", threshold, "");
        if (strlen(threshold) != 0) {
            p_response = atSendCommand(String8::format("AT+ECSQ=3,3,%s", threshold));
            logD(LOG_TAG, "requestScreenState(), adjust signaling threshold %s", threshold);
            if (p_response->getError() != 0 || p_response->getSuccess() == 0)
                logW(LOG_TAG, "There is something wrong with the exectution of AT+ECSQ=3,3..");
        } else {
            // logD(LOG_TAG, "net.handover.thlte is empty");
        }
    }
}

void RmcNetworkRequestHandler::setUnsolResponseFilterNetworkState(bool enable)
{
    sp<RfxAtResponse> p_response;

    if (enable) {
        // enable
        /* disable mtk optimized +CREG URC report mode, as standard */
        p_response = atSendCommand("AT+ECREG=0");
        if (p_response->getError() != 0 || p_response->getSuccess() == 0)
            logW(LOG_TAG, "There is something wrong with the exectution of AT+ECREG=0");

        /* disable mtk optimized +CGREG URC report mode, as standard */
        p_response = atSendCommand("AT+ECGREG=0");
        if (p_response->getError() != 0 || p_response->getSuccess() == 0)
            logW(LOG_TAG, "There is something wrong with the exectution of AT+ECGREG=0");

        /* Enable PSBEARER URC */
        p_response = atSendCommand("AT+PSBEARER=1");
        if (p_response->getError() != 0 || p_response->getSuccess() == 0)
            logW(LOG_TAG, "There is something wrong with the exectution of AT+PSBEARER=1");

        if (RfxRilUtils::isImsSupport()) {
            p_response = atSendCommand("AT+CIREG=2");
            if (p_response->getError() != 0 || p_response->getSuccess() == 0)
                logW(LOG_TAG, "There is something wrong with the exectution of AT+CIREG=2");
        }

        /* disable mtk optimized +CEREG URC report mode, as standard */
        p_response = atSendCommand("AT+ECEREG=0");
        if (p_response->getError() != 0 || p_response->getSuccess() == 0)
            logW(LOG_TAG, "There is something wrong with the exectution of AT+ECEREG=0");

        p_response = atSendCommand("AT+CEREG=3");
        if (p_response->getError() != 0 || p_response->getSuccess() == 0) {
            p_response = atSendCommand("AT+CEREG=2");
            if (p_response->getError() != 0 || p_response->getSuccess() == 0) {
                logW(LOG_TAG, "There is something wrong with the exectution of AT+CEREG=2");
            }
        }

        /* Enable ECSG URC */
        if (isFemtocellSupport()) {
            p_response = atSendCommand("AT+ECSG=4,1");
            if (p_response->getError() != 0 || p_response->getSuccess() == 0)
                logW(LOG_TAG, "There is something wrong with the exectution of AT+ECEREG=0");
        }

        /* Enable EMODCFG URC */
        if (isEnableModulationReport()) {
            p_response = atSendCommand("AT+EMODCFG=1");
            if (p_response->getError() != 0 || p_response->getSuccess() == 0)
                logW(LOG_TAG, "There is something wrong with the exectution of AT+EMODCFG=1");
        }

        /* Enable EREGINFO URC */
        p_response = atSendCommand("AT+EREGINFO=1");
        if (p_response->getError() != 0 || p_response->getSuccess() == 0)
            logW(LOG_TAG, "There is something wrong with the exectution of AT+EREGINFO=1");

        /* Query EIPRL URC */
        p_response = atSendCommand("AT+EIPRL?");
        if (p_response->getError() != 0 || p_response->getSuccess() == 0)
            // logW(LOG_TAG, "There is something wrong with the exectution of AT+EIPRL?");

        /* Enable EFCELL URC */
        if (isFemtocellSupport()) {
            p_response = atSendCommand("AT+EFCELL=1");
            if (p_response->getError() != 0 || p_response->getSuccess() == 0) {
                logW(LOG_TAG, "There is something wrong with the exectution of AT+EFCELL=1");
            } else {
                p_response = atSendCommand("AT+EFCELL?");
                if (p_response->getError() != 0 || p_response->getSuccess() == 0)
                    logW(LOG_TAG, "There is something wrong with the exectution of AT+EFCELL?");
            }
        }

        /* Enable EDEFROAM URC */
        p_response = atSendCommand("AT+EDEFROAM=1");
        if (p_response->getError() != 0 || p_response->getSuccess() == 0) {
            logW(LOG_TAG, "There is something wrong with the exectution of AT+EDEFROAM=1");
        } else {
            p_response = atSendCommand("AT+EDEFROAM?");
            if (p_response->getError() != 0 || p_response->getSuccess() == 0)
                logW(LOG_TAG, "There is something wrong with the exectution of AT+EDEFROAM?");
        }
    } else {
        // disable
        /* enable mtk optimized +EREG URC report mode,
           report +EREG when stat changed for EREG format 2 and 3 */
        // *we need act changed urc, it's important.
        // p_response = atSendCommand("AT+ECREG=2");
        // if (p_response->getError() != 0 || p_response->getSuccess() == 0) {
            // NOT support ECREG 2, set format 1.
            // report +EREG when state or act changed for EREG format 2 and 3
            p_response = atSendCommand("AT+ECREG=1");
            if (p_response->getError() != 0 || p_response->getSuccess() == 0)
                logW(LOG_TAG, "There is something wrong with the exectution of AT+ECREG=1");
        // }

        /* enable mtk optimized +EGREG URC report mode,
           report +EGREG when stat changed, for EGREG format 2 and 3 */
        p_response = atSendCommand("AT+ECGREG=2");
        if (p_response->getError() != 0 || p_response->getSuccess() == 0) {
            // NOT support ECGREG 2, set format 1.
            // report +EGREG when state or act changed for EGREG format 2 and 3
            p_response = atSendCommand("AT+ECGREG=1");
            if (p_response->getError() != 0 || p_response->getSuccess() == 0)
                logW(LOG_TAG, "There is something wrong with the exectution of AT+ECGREG=1");
        }

        /* Disable PSBEARER URC */
        p_response = atSendCommand("AT+PSBEARER=0");
        if (p_response->getError() != 0 || p_response->getSuccess() == 0)
            logW(LOG_TAG, "There is something wrong with the exectution of AT+PSBEARER=0");

        /* enable mtk optimized +CEREG URC report mode,
           report +CGREG when stat changed, for CEREG format 2 and 3 */
        p_response = atSendCommand("AT+ECEREG=2");
        if (p_response->getError() != 0 || p_response->getSuccess() == 0) {
            p_response = atSendCommand("AT+ECEREG=1");
            if (p_response->getError() != 0 || p_response->getSuccess() == 0)
                logW(LOG_TAG, "There is something wrong with the exectution of AT+ECEREG=1");
        }

        if (isFemtocellSupport()) {
            /* Disable ECSG URC */
            p_response = atSendCommand("AT+ECSG=4,0");
            if (p_response->getError() != 0 || p_response->getSuccess() == 0)
                logW(LOG_TAG, "There is something wrong with the exectution of AT+ECSG=4,0");
        }

        /* Disable EMODCFG URC */
        if (isEnableModulationReport()) {
            p_response = atSendCommand("AT+EMODCFG=0");
            if (p_response->getError() != 0 || p_response->getSuccess() == 0)
                logW(LOG_TAG, "There is something wrong with the exectution of AT+EMODCFG=0");
        }

        /* Disable EREGINFO URC */
        p_response = atSendCommand("AT+EREGINFO=0");
        if (p_response->getError() != 0 || p_response->getSuccess() == 0)
            logW(LOG_TAG, "There is something wrong with the exectution of AT+EREGINFO=0");

        /* Disable EFCELL URC */
        if (isFemtocellSupport()) {
            p_response = atSendCommand("AT+EFCELL=0");
            if (p_response->getError() != 0 || p_response->getSuccess() == 0)
                logW(LOG_TAG, "There is something wrong with the exectution of AT+EFCELL=1");
        }

        /* Disable EDEFROAM URC */
        p_response = atSendCommand("AT+EDEFROAM=0");
        if (p_response->getError() != 0 || p_response->getSuccess() == 0)
            logW(LOG_TAG, "There is something wrong with the exectution of AT+EDEFROAM=0");
    }
}

void RmcNetworkRequestHandler::requestScreenState(const sp<RfxMclMessage>& msg) {
    /************************************
    * Control the URC: ECSQ,CREG,CGREG,CEREG
    * CIREG,PSBEARER,ECSG,EMODCFG,EREGINFO
    *************************************/

    int on_off, err;
    sp<RfxAtResponse> p_response;
    sp<RfxMclMessage> response;
    int *pInt = (int *)msg->getData()->getData();

    on_off = pInt[0];

    // The ePDG/RNS framework need to monitoring the LTE RSRP signal strength across the threshold.
    char threshold[PROPERTY_VALUE_MAX] = {0};

    if (on_off)
    {
        // screen is on

        setUnsolResponseFilterNetworkState(true);

        setUnsolResponseFilterSignalStrength(true);
        updateSignalStrength();
    }
    else
    {
        // screen is off

        setUnsolResponseFilterNetworkState(false);

        setUnsolResponseFilterSignalStrength(false);
    }

    response = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_SUCCESS,
            RfxVoidData(), msg, false);
    // response to TeleCore
    responseToTelCore(response);

}

void RmcNetworkRequestHandler::requestSetUnsolicitedResponseFilter(const sp<RfxMclMessage>& msg) {
    /************************************
    * Control the URC: ECSQ,CREG,CGREG,CEREG
    * CIREG,PSBEARER,ECSG,EMODCFG,EREGINFO
    *************************************/

    RIL_UnsolicitedResponseFilter filter;
    int err;
    sp<RfxAtResponse> p_response;
    sp<RfxMclMessage> response;
    RIL_UnsolicitedResponseFilter *pUnsolicitedResponseFilter
            = (RIL_UnsolicitedResponseFilter *)msg->getData()->getData();

    filter = pUnsolicitedResponseFilter[0];

    if ((filter & RIL_UR_SIGNAL_STRENGTH) == RIL_UR_SIGNAL_STRENGTH) {
        // enable
        setUnsolResponseFilterSignalStrength(true);
        updateSignalStrength();
    } else {
        // disable
        setUnsolResponseFilterSignalStrength(false);
    }
    if ((filter & RIL_UR_FULL_NETWORK_STATE) == RIL_UR_FULL_NETWORK_STATE) {
        // enable
        setUnsolResponseFilterNetworkState(true);
    } else {
        // disable
        setUnsolResponseFilterNetworkState(false);
    }

    response = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_SUCCESS,
            RfxVoidData(), msg, false);
    // response to TeleCore
    responseToTelCore(response);
}

int RmcNetworkRequestHandler::isEnableModulationReport()
{
    char optr[PROPERTY_VALUE_MAX] = {0};

    rfx_property_get("persist.operator.optr", optr, "");

    return (strcmp("OP08", optr) == 0) ? 1 : 0;
}

void RmcNetworkRequestHandler::isFemtoCell(int regState, int cid, int act) {
    int isFemtocell = 0;
    int eNBid = 0;
    String8 oper(getMclStatusManager()->getString8Value(RFX_STATUS_KEY_OPERATOR));
    pthread_mutex_lock(&ril_nw_femtoCell_mutex);
    eNBid = cid >> 8;
    if (isOp12Plmn(oper.string()) == true
            && act == 14  // LTE
            && regState == 1  // in home service
            && eNBid >= 1024000 && eNBid <= 1048575) { // OP12: 0xFA000 - 0xFFFFF
        isFemtocell = 2;
    }

    if (isFemtocell != m_femto_cell_cache.is_femtocell) {
        m_femto_cell_cache.is_femtocell = isFemtocell;
        updateFemtoCellInfo();
    }
    pthread_mutex_unlock(&ril_nw_femtoCell_mutex);
}

void RmcNetworkRequestHandler::updateFemtoCellInfo() {
    int err;
    unsigned long i;
    char *responseStr[10];
    char *shortOperName = (char *) calloc(1, sizeof(char)*MAX_OPER_NAME_LENGTH);
    char *longOperName = (char *) calloc(1, sizeof(char)*MAX_OPER_NAME_LENGTH);

    if (shortOperName == NULL || longOperName == NULL) {
        logE(LOG_TAG, "updateFemtoCellInfo calloc fail");
        if (longOperName != NULL) free(longOperName);
        if (shortOperName != NULL) free(shortOperName);
        return;
    }

    asprintf(&responseStr[0], "%d", m_femto_cell_cache.domain);
    asprintf(&responseStr[1], "%d", m_femto_cell_cache.state);
    asprintf(&responseStr[3], "%d", m_femto_cell_cache.plmn_id);

    err = getOperatorNamesFromNumericCode(responseStr[3], longOperName, shortOperName, MAX_OPER_NAME_LENGTH);
    if(err < 0) {
        asprintf(&responseStr[2], "");
    } else {
        asprintf(&responseStr[2], "%s", longOperName);
    }
    free(shortOperName);
    free(longOperName);

    asprintf(&responseStr[4], "%d", m_femto_cell_cache.act);

    if (m_femto_cell_cache.is_femtocell == 2) { // for LTE/C2K femtocell
        asprintf(&responseStr[5], "%d", m_femto_cell_cache.is_femtocell);
    } else if (m_femto_cell_cache.is_csg_cell == 1) { // for GSM femtocell
        asprintf(&responseStr[5], "%d", m_femto_cell_cache.is_csg_cell);
    } else {
        asprintf(&responseStr[5], "0");
    }
    asprintf(&responseStr[6], "%d", m_femto_cell_cache.csg_id);
    asprintf(&responseStr[7], "%d", m_femto_cell_cache.csg_icon_type);
    asprintf(&responseStr[8], "%s", m_femto_cell_cache.hnbName);
    asprintf(&responseStr[9], "%d", m_femto_cell_cache.cause);

    // <domain>,<state>,<lognname>,<plmn_id>,<act>,<is_csg_cell/is_femto_cell>,<csg_id>,<csg_icon_type>,<hnb_name>,<cause> */
    sp<RfxMclMessage> urc = RfxMclMessage::obtainUrc(RFX_MSG_URC_FEMTOCELL_INFO,
            m_slot_id, RfxStringsData(responseStr, 10));
    responseToTelCore(urc);
    // free memory
    for (i=0 ; i<(sizeof(responseStr)/sizeof(char*)) ; i++) {
        if (responseStr[i]) {
            // logD(LOG_TAG, "free responseStr[%d]=%s", i, responseStr[i]);
            free(responseStr[i]);
            responseStr[i] = NULL;
        }
    }
    return;
}

int RmcNetworkRequestHandler::getOperatorNamesFromNumericCode(
    char *code,
    char *longname,
    char *shortname,
    int max_length) {

    char nitz[RFX_PROPERTY_VALUE_MAX];
    char oper_file_path[RFX_PROPERTY_VALUE_MAX];
    char oper[128], name[MAX_OPER_NAME_LENGTH];
    int err;
    char *oper_code, *oper_lname, *oper_sname;

    if (max_length > MAX_OPER_NAME_LENGTH) {
        logE(LOG_TAG, "The buffer size %d is not valid. We only accept the length less than or equal to %d",
                 max_length, MAX_OPER_NAME_LENGTH);
        return -1;
    }
    oper_code = m_ril_nw_nitz_oper_code;
    oper_lname = m_ril_nw_nitz_oper_lname;
    oper_sname = m_ril_nw_nitz_oper_sname;

    longname[0] = '\0';
    shortname[0] = '\0';

    pthread_mutex_lock(&ril_nw_nitzName_mutex);
    // logD(LOG_TAG, "Get ril_nw_nitzName_mutex in the getOperatorNamesFromNumericCode");
    // logD(LOG_TAG, "getOperatorNamesFromNumericCode code:%s oper_code:%s", code, oper_code);

    /* Check if there is a NITZ name*/
    /* compare if the operator code is the same with <oper>*/
    if(strcmp(code, oper_code) == 0) {
        /* there is a NITZ Operator Name */
        /* get operator code and name */
        /* set short name with long name when short name is null and long name isn't, and vice versa */
        int nlnamelen = strlen(oper_lname);
        int nsnamelen = strlen(oper_sname);
        if(nlnamelen != 0 && nsnamelen != 0) {
            strncpy(longname,oper_lname, max_length);
            strncpy(shortname, oper_sname, max_length);
        } else if(strlen(oper_sname) != 0) {
            strncpy(longname, oper_sname, max_length);
            strncpy(shortname, oper_sname, max_length);
        } else if(strlen(oper_lname) != 0) {
            strncpy(longname, oper_lname, max_length);
            strncpy(shortname, oper_lname, max_length);
        }
        logD(LOG_TAG, "Return NITZ Operator Name: %s %s %s, lname length: %d, sname length: %d",
                oper_code, oper_lname, oper_sname, nlnamelen, nsnamelen);
    } else {
        //strcpy(longname, code);
        //strcpy(shortname, code);
        getPLMNNameFromNumeric(code, longname, shortname, max_length);
    }
     pthread_mutex_unlock(&ril_nw_nitzName_mutex);
    return 0;
}

int RmcNetworkRequestHandler::getEonsNamesFromNumericCode(
        char *code,
        unsigned int lac,
        char *longname,
        char *shortname,
        int max_length)
{
    int err;
    int skip;
    sp<RfxAtResponse> p_response;
    sp<RfxMclMessage> resp;
    RfxAtLine* line;
    char *oper_lname = NULL, *oper_sname = NULL;
    int dcs_long = 0;
    int dcs_short = 0;

    if (eons_info[m_slot_id].eons_status == EONS_INFO_RECEIVED_ENABLED &&
        ((lac > 0) && (lac < 0xFFFF))) {
        p_response = atSendCommandSingleline(String8::format("AT+EONS=2,\"%s\",%d", code, lac), "+EONS");
        err = p_response->getError();
        if (err != 0 || p_response->getSuccess() == 0 ||
                p_response->getIntermediates() == NULL) {
            logE(LOG_TAG, "EONS got error response");
            goto error;
        } else {
            line = p_response->getIntermediates();
            line->atTokStart(&err);
            if (err >= 0) {
                skip = line->atTokNextint(&err);
                /* <Result> */
                if (err >= 0 && skip == 1) {
                    /* <dcs_long> */
                    dcs_long = line->atTokNextint(&err);
                    if (err < 0) goto error;

                    /* <long_oper> */
                    oper_lname = line->atTokNextstr(&err);
                    logD(LOG_TAG, "EONS Get operator long %s", longname);
                    if (err < 0) goto error;

                    /* <dcs_short> */
                    dcs_short = line->atTokNextint(&err);
                    if (err < 0) goto error;

                    /* <short_oper> */
                    oper_sname = line->atTokNextstr(&err);
                    logD(LOG_TAG, "EONS Get operator short %s", shortname);
                    if (err < 0) goto error;
                } else {
                    goto error;
                }
                /* Copy operator name */
                strncpy(longname, oper_lname, max_length-1);
                strncpy(shortname, oper_sname, max_length-1);
            }
        }

        logD(LOG_TAG, "Return EONS Operator Name: %s %s %s",
                code,
                oper_lname,
                oper_sname);
        return 0;
    }
error:
    return -1;
}

int RmcNetworkRequestHandler::getPLMNNameFromNumeric(char *numeric, char *longname, char *shortname, int max_length) {
    int i = 0, length = sizeof(s_mtk_spn_table)/sizeof(s_mtk_spn_table[0]);
    // logD(LOG_TAG, "ENTER getPLMNNameFromNumeric(), plmn table length = %d", length);
    longname[0] = '\0';
    shortname[0] = '\0';

    for (i=0; i < length; i++) {
        if (0 == strcmp(numeric, s_mtk_spn_table[i].mccMnc))
        {
            strncpy(longname, s_mtk_spn_table[i].spn, max_length);
            strncpy(shortname, s_mtk_spn_table[i].spn, max_length);
            /*
            logD(LOG_TAG, "getPLMNNameFromNumeric: numeric:%s,"
                    "oper_code:%s longname:%s, shortname:%s",
                    numeric, m_ril_nw_nitz_oper_code,
                    longname, shortname);
            */
            return 0;
        }
    }
    strncpy(longname, numeric, max_length);
    strncpy(shortname, numeric, max_length);
    logD(LOG_TAG, "getPLMNNameFromNumeric: s_mtk_spn_table not found %s", numeric);
    return -1;
}

int RmcNetworkRequestHandler::getOperatorNamesFromNumericCode(
    char *code,
    unsigned int lac,
    char *longname,
    char *shortname,
    int max_length)
{
    char *line, *tmp;
    FILE *list;
    int err;
    char *oper_code, *oper_lname, *oper_sname;

    if (max_length > MAX_OPER_NAME_LENGTH) {
        logE(LOG_TAG, "The buffer size %d is not valid. We only accept the length less than or equal to %d",
             max_length, MAX_OPER_NAME_LENGTH);
        return -1;
    }

    longname[0] = '\0';
    shortname[0] = '\0';

    /* To support return EONS if available in RIL_REQUEST_OPERATOR START */
    if (eons_info[m_slot_id].eons_status == EONS_INFO_RECEIVED_ENABLED) {
        err = getEonsNamesFromNumericCode(
                code, lac, longname, shortname, max_length);
        if (err == 0) {
            logD(LOG_TAG, "Get ril_nw_nitzName_mutex in the getEonsNamesFromNumericCode");
            goto skipNitz;
        }
    }
    /* To support return EONS if available in RIL_REQUEST_OPERATOR END */

    oper_code = m_ril_nw_nitz_oper_code;
    oper_lname = m_ril_nw_nitz_oper_lname;
    oper_sname = m_ril_nw_nitz_oper_sname;

    pthread_mutex_lock(&ril_nw_nitzName_mutex);
    logD(LOG_TAG, "Get ril_nw_nitzName_mutex in the getOperatorNamesFromNumericCode");

    /* Check if there is a NITZ name*/
    /* compare if the operator code is the same with <oper>*/
    if(strcmp(code, oper_code) == 0) {
        /* there is a NITZ Operator Name*/
        /*get operator code and name*/
        /*set short name with long name when short name is null and long name isn't, and vice versa*/
        int nlnamelen = strlen(oper_lname);
        int nsnamelen = strlen(oper_sname);
        if (nlnamelen != 0 && nsnamelen != 0) {
            strncpy(longname,oper_lname, max_length);
            strncpy(shortname, oper_sname, max_length);
        } else if (strlen(oper_sname) != 0) {
            strncpy(longname, oper_sname, max_length);
            strncpy(shortname, oper_sname, max_length);
        } else if (strlen(oper_lname) != 0) {
            strncpy(longname, oper_lname, max_length);
            strncpy(shortname, oper_lname, max_length);
        }
        logD(LOG_TAG, "Return NITZ Operator Name: %s %s %s, lname length: %d, sname length: %d", oper_code,
                                                                                        oper_lname,
                                                                                        oper_sname,
                                                                                        nlnamelen,
                                                                                        nsnamelen);
    }
    else
    {
        //strcpy(longname, code);
        //strcpy(shortname, code);
        getPLMNNameFromNumeric(code, longname, shortname, max_length);
    }

    pthread_mutex_unlock(&ril_nw_nitzName_mutex);
skipNitz:
    return 0;
}

void RmcNetworkRequestHandler::requestGetFemtocellList(const sp<RfxMclMessage>& msg) {
    /* +ECSG: <num_plmn>,<plmn_id>,<act>,<num_csg>,<csg_id>,<csg_icon_type>,<hnb_name>[,...]
       AT Response Example
       +ECSG: 3,"46000",2,1,<csg_id_A>,<csg_type_A>,<hnb_name_A>,"46002",7,1,<csg_id_B>,<csg_type_B>,<hnb_name_B>,"46002",7,1,<csg_id_C>,<csg_type_C>,<hnb_name_C> */
    int err, len, i, j, num, act, csgId ,csgIconType,numCsg;
    sp<RfxMclMessage> response;
    RfxAtLine* line;
    sp<RfxAtResponse> p_response;
    char **femtocellList = NULL;
    char *femtocell = NULL, *plmn_id = NULL, *hnb_name = NULL;
    char shortname[MAX_OPER_NAME_LENGTH];

    m_csgListOngoing = 1;

    // send AT command
    p_response = atSendCommandSingleline("AT+ECSG=0", "+ECSG");

    // check error
    if (p_response == NULL ||
            p_response->getError() != 0 ||
            p_response->getSuccess() == 0 ||
            p_response->getIntermediates() == NULL) {
        goto error;
    }

    // handle intermediate
    line = p_response->getIntermediates();

    // go to start position
    line->atTokStart(&err);
    if (err < 0) goto error;

    // <num_plmn>
    num = line->atTokNextint(&err);
    if (err < 0) goto error;

    // allocate memory
    femtocellList = (char **) calloc(1, sizeof(char*) * num *6);
    femtocell = (char *) calloc(1, num* sizeof(char)*6*MAX_OPER_NAME_LENGTH);

    if (femtocellList == NULL || femtocell == NULL) {
        logE(LOG_TAG, "requestGetFemtocellList calloc fail");
        goto error;
    }

    for (i = 0, j = 0; i < num; i++, j+=6) {
        /* <plmn_id>,<act>,<num_csg>,<csg_id>,<csg_icon_type>,<hnb_name> */
        femtocellList[j+0] = &femtocell[(j+0)*MAX_OPER_NAME_LENGTH];
        femtocellList[j+1] = &femtocell[(j+1)*MAX_OPER_NAME_LENGTH];
        femtocellList[j+2] = &femtocell[(j+2)*MAX_OPER_NAME_LENGTH];
        femtocellList[j+3] = &femtocell[(j+3)*MAX_OPER_NAME_LENGTH];
        femtocellList[j+4] = &femtocell[(j+4)*MAX_OPER_NAME_LENGTH];
        femtocellList[j+5] = &femtocell[(j+5)*MAX_OPER_NAME_LENGTH];

        /* get <plmn_id> numeric code*/
        plmn_id = line->atTokNextstr(&err);
        if (err < 0) goto error;
        sprintf(femtocellList[j+0], "%s", plmn_id);

        int len = strlen(femtocellList[j+0]);
        if (len == 5 || len == 6) {
            err = getOperatorNamesFromNumericCode(
                      femtocellList[j+0], femtocellList[j+1],shortname, MAX_OPER_NAME_LENGTH);
            if (err < 0) goto error;
        } else {
            goto error;
        }

        /* get <AcT> 0 is "2G", 2 is "3G", 7 is "4G"*/
        act = line->atTokNextint(&err);
        if (err < 0) goto error;
        sprintf(femtocellList[j+2], "%d", act);

        /* get <num_csg> fwk no need*/
        numCsg = line->atTokNextint(&err);
        if (err < 0) goto error;

        /* get <csgId> */
        csgId = line->atTokNextint(&err);
        if (err < 0) goto error;
        sprintf(femtocellList[j+3], "%d", csgId);

        /* get <csgIconType> */
        csgIconType = line->atTokNextint(&err);
        if (err < 0) goto error;
        sprintf(femtocellList[j+4], "%d", csgIconType);

        /* get <hnbName> */
        hnb_name = line->atTokNextstr(&err);
        if (err < 0) goto error;
        sprintf(femtocellList[j+5], "%s", hnb_name);

        logD(LOG_TAG, "requestGetFemtocellList (%s, %s, %s, %s, %s, %s)",
                femtocellList[j+0],
                femtocellList[j+1],
                femtocellList[j+2],
                femtocellList[j+3],
                femtocellList[j+4],
                femtocellList[j+5]);
    }
    response = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_SUCCESS,
            RfxStringsData(femtocellList, num*6), msg, false);
    responseToTelCore(response);
    free(femtocellList);
    free(femtocell);

    m_csgListOngoing = 0;
    m_csgListAbort =0; /* always clear here to prevent race condition scenario */
    return;

error:
    logE(LOG_TAG, "requestGetFemtocellList must never return error when radio is on");
    if (m_csgListAbort == 1) {
        // requestGetFemtocellList is canceled
        response = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_CANCELLED,
            RfxStringsData(), msg, false);
    } else {
        response = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_GENERIC_FAILURE,
            RfxStringsData(), msg, false);
    }
    // response to TeleCore
    responseToTelCore(response);
    if (femtocellList != NULL) free(femtocellList);
    if (femtocell) free(femtocell);
    m_csgListOngoing = 0;
    m_csgListAbort =0; /* always clear here to prevent race condition scenario */
}

void RmcNetworkRequestHandler::requestAbortFemtocellList(const sp<RfxMclMessage>& msg) {
    sp<RfxMclMessage> response;
    sp<RfxAtResponse> p_response;

    if (m_csgListOngoing == 1) {
        m_csgListAbort = 1;
        p_response = atSendCommandSingleline("AT+ECSG=2", "+ECSG:");
        // check error
        if (p_response == NULL ||
                p_response->getError() != 0 ||
                p_response->getSuccess() == 0) {
            m_csgListAbort = 0;
            logE(LOG_TAG, "requestAbortFemtocellList fail.");
            goto error;
        }
    }

    response = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_SUCCESS,
            RfxVoidData(), msg, false);
    // response to TeleCore
    responseToTelCore(response);
    return;

error:
    logE(LOG_TAG, "requestAbortFemtocellList must never return error when radio is on");
    response = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_GENERIC_FAILURE,
            RfxVoidData(), msg, false);
    // response to TeleCore
    responseToTelCore(response);
}

void RmcNetworkRequestHandler::requestSelectFemtocell(const sp<RfxMclMessage>& msg) {
    sp<RfxMclMessage> response;
    sp<RfxAtResponse> p_response;
    const char **strings = (const char **)msg->getData()->getData();
    //parameters:  <plmn> , <act> , <csg id>

    // check parameters
    if ((msg->getData()->getDataLength() < (int)(3 * sizeof(char*)))
            || (strings[0] == NULL)
            || (strings[1] == NULL)
            || (strings[2] == NULL)) {
        logE(LOG_TAG, "requestSelectFemtocell parameters wrong datalen = %d",
                msg->getData()->getDataLength());
        goto error;
    }

    // check <plmn> is valid digit
    for (size_t i = 0; i < strlen(strings[0]); i++) {
        if (strings[0][i] < '0' || strings[0][i] > '9') {
            logE(LOG_TAG, "requestSelectFemtocell parameters[0] wrong");
            goto error;
        }
    }

    // check <csg id>
    for (size_t i = 0; i < strlen(strings[2]); i++) {
        if (strings[2][i] < '0' || strings[2][i] > '9') {
            logE(LOG_TAG, "requestSelectFemtocell parameters[2] wrong");
            goto error;
        }
    }

    p_response = atSendCommand(String8::format("AT+ECSG=1,\"%s\",%s,%s", strings[0],strings[2],strings[1]));
    // check error
    if (p_response == NULL ||
            p_response->getError() != 0 ||
            p_response->getSuccess() == 0) {
        goto error;
    }

    response = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_SUCCESS,
            RfxVoidData(), msg, false);
    // response to TeleCore
    responseToTelCore(response);
    return;

error:
    logE(LOG_TAG, "requestSelectFemtocell must never return error when radio is on");
    response = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_GENERIC_FAILURE,
            RfxVoidData(), msg, false);
    // response to TeleCore
    responseToTelCore(response);
}
void RmcNetworkRequestHandler::requestQueryFemtoCellSystemSelectionMode(const sp<RfxMclMessage>& msg) {
    int mode, err;
    int response[2] = { 0 };
    RIL_Errno ril_errno = RIL_E_MODE_NOT_SUPPORTED;
    sp<RfxAtResponse> p_response;
    sp<RfxMclMessage> resp;
    RfxAtLine* line;

    logD(LOG_TAG, "requestQueryFemtoCellSystemSelectionMode sending AT command");
    p_response = atSendCommandSingleline("AT+EFSS?", "+EFSS:");

    err = p_response->getError();
    if (err != 0 ||
            p_response == NULL ||
            p_response->getSuccess() == 0 ||
            p_response->getIntermediates() == NULL) goto error;

    // handle intermediate
    line = p_response->getIntermediates();

    /* +EFSS: <mode>
       AT Response Example
       +EFSS: 0 */

    // go to start position
    line->atTokStart(&err);
    if (err < 0) goto error;

    mode = line->atTokNextint(&err);
    if (err < 0) goto error;

    logD(LOG_TAG, "requestQueryFemtoCellSystemSelectionMode sucess, free memory");
    resp = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_SUCCESS,
            RfxIntsData(&mode, 1), msg, false);
    responseToTelCore(resp);
    return;
error:
    logD(LOG_TAG, "requestGetPOLCapability must never return error when radio is on");
    resp = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_GENERIC_FAILURE,
            RfxIntsData(&mode, 1), msg, false);
    responseToTelCore(resp);
}

void RmcNetworkRequestHandler::requestSetFemtoCellSystemSelectionMode(const sp<RfxMclMessage>& msg) {
    sp<RfxAtResponse> p_response;
    sp<RfxMclMessage> response;
    RIL_Errno ril_errno = RIL_E_GENERIC_FAILURE;
    int *pInt = (int *)msg->getData()->getData();
    int mode = pInt[0];

    logD(LOG_TAG, "requestSetFemtoCellSystemSelectionMode: mode=%d", mode);

    if ((mode >= 0) && (mode <= 2)) {
        p_response = atSendCommand(String8::format("AT+EFSS=%d", mode));
        if (p_response->getError() >= 0 || p_response->getSuccess() != 0) {
            ril_errno = RIL_E_SUCCESS;
        }
    } else {
        logE(LOG_TAG, "mode is invalid");
    }
    response = RfxMclMessage::obtainResponse(msg->getId(), ril_errno,
            RfxVoidData(), msg, false);
    // response to TeleCore
    responseToTelCore(response);
    return;
}

void RmcNetworkRequestHandler::requestAntennaConf(const sp<RfxMclMessage>& msg) {
    int antennaType, err;
    int response[2] = { 0 };
    RIL_Errno ril_errno = RIL_E_MODE_NOT_SUPPORTED;
    sp<RfxAtResponse> p_response;
    sp<RfxMclMessage> resp;
    int *pInt = (int *)msg->getData()->getData();

    antennaType = pInt[0];
    response[0] = antennaType;
    response[1] = 0; // failed

    logD(LOG_TAG, "Enter requestAntennaConf(), antennaType = %d ", antennaType);
    // AT command format as below : (for VZ_REQ_LTEB13NAC_6290)
    // AT+ERFTX=8, <type>[,<param1>,<param2>]
    // <param1> is decoded as below:
    //    1 - Normal dual receiver operation(default UE behaviour)
    //    2 - Single receiver operation 'enable primary receiver only'(disable secondary/MIMO receiver)
    //    3 - Single receiver operation 'enable secondary/MIMO receiver only (disable primary receiver)
    switch(antennaType){
        case 0:    // 0: signal information is not available on all Rx chains
            antennaType = 0;
            break;
        case 1:    // 1: Rx diversity bitmask for chain 0
            antennaType = 2;
            break;
        case 2:    // 2: Rx diversity bitmask for chain 1 is available
            antennaType = 3;
            break;
        case 3:    // 3: Signal information on both Rx chains is available.
            antennaType = 1;
            break;
        default:
            logE(LOG_TAG, "requestAntennaConf: configuration is an invalid");
            break;
    }
    p_response = atSendCommand(String8::format("AT+ERFTX=8,1,%d", antennaType));
    if (p_response->getError() < 0 || p_response->getSuccess() == 0) {
        if (antennaType == 0) {
            // This is special handl for disable all Rx chains
            // <param1>=0 - signal information is not available on all Rx chains
            ril_errno = RIL_E_SUCCESS;
            response[1] = 1;  // success
            antennaTestingType = antennaType;
        }
    } else {
        ril_errno = RIL_E_SUCCESS;
        response[1] = 1; // success
        // Keep this settings for query antenna info.
        antennaTestingType = antennaType;
    }
    resp = RfxMclMessage::obtainResponse(msg->getId(), ril_errno,
            RfxIntsData(response, 2), msg, false);
    responseToTelCore(resp);
}
void RmcNetworkRequestHandler::requestAntennaInfo(const sp<RfxMclMessage>& msg) {
    RIL_Errno ril_errno = RIL_E_MODE_NOT_SUPPORTED;
    sp<RfxAtResponse> p_response;
    sp<RfxMclMessage> resp;
    RfxAtLine* line;

    int param1, param2, err, skip;
    int response[6] = { 0 };
    memset(response, 0, sizeof(response));
    int *primary_antenna_rssi   = &response[0];
    int *relative_phase         = &response[1];
    int *secondary_antenna_rssi = &response[2];
    int *phase1                 = &response[3];
    int *rxState_0              = &response[4];
    int *rxState_1              = &response[5];
    *primary_antenna_rssi   = 0;  // <primary_antenna_RSSI>
    *relative_phase         = 0;  // <relative_phase>
    *secondary_antenna_rssi = 0;  // <secondary_antenna_RSSI>
    *phase1                 = 0;  // N/A
    *rxState_0              = 0;  // rx0 status(0: not vaild; 1:valid)
    *rxState_1              = 0;  // rx1 status(0: not vaild; 1:valid)
    // AT+ERFTX=8, <type> [,<param1>,<param2>]
    // <type>=0 is used for VZ_REQ_LTEB13NAC_6290
    // <param1> represents the A0 bit in ANTENNA INFORMATION REQUEST message
    // <param2> represents the A1 bit in ANTENNA INFORMATION REQUEST message
    switch(antennaTestingType) {
        case 0:    // signal information is not available on all Rx chains
            param1 = 0;
            param2 = 0;
            break;
        case 1:    // Normal dual receiver operation (default UE behaviour)
            param1 = 1;
            param2 = 1;
            break;
        case 2:    // enable primary receiver only
            param1 = 1;
            param2 = 0;
            break;
        case 3:    // enable secondary/MIMO receiver only
            param1 = 0;
            param2 = 1;
            break;
        default:
            logE(LOG_TAG, "requestAntennaInfo: configuration is an invalid, antennaTestingType: %d", antennaTestingType);
            goto error;
    }
    logD(LOG_TAG, "requestAntennaInfo: antennaType=%d, param1=%d, param2=%d", antennaTestingType, param1, param2);
    if (antennaTestingType == 0) {
        p_response = atSendCommand(String8::format("AT+ERFTX=8,0,%d,%d", param1, param2));
        if (p_response->getError() >= 0 || p_response->getSuccess() != 0) {
            ril_errno = RIL_E_SUCCESS;
        }
        resp = RfxMclMessage::obtainResponse(msg->getId(), ril_errno,
                RfxIntsData(response, 6), msg, false);
        responseToTelCore(resp);
        return;
    }
    // set antenna testing type
    p_response = atSendCommand(String8::format("AT+ERFTX=8,1,%d", antennaTestingType));
    if (p_response->getError() >= 0 || p_response->getSuccess() != 0) {
        p_response = atSendCommandSingleline(String8::format("AT+ERFTX=8,0,%d,%d", param1, param2),
                "+ERFTX:");
        if (p_response->getError() >= 0 || p_response->getSuccess() != 0) {
            // handle intermediate
            line = p_response->getIntermediates();
            // go to start position
            line->atTokStart(&err);
            if (err < 0) goto error;
            // skip <op=8>
            skip = line->atTokNextint(&err);
            if (err < 0) goto error;
            // skip <type=0>
            skip = line->atTokNextint(&err);
            if (err < 0) goto error;
            (*primary_antenna_rssi) = line->atTokNextint(&err);
            if (err < 0) {
                // response for AT+ERFTX=8,0,0,1
                // Ex: +ERFTX: 8,0,,100
            } else {
                // response for AT+ERFTX=8,0,1,1 or AT+ERFTX=8,0,1,0
                // Ex: +ERFTX: 8,0,100,200,300 or +ERFTX: 8,0,100
                *rxState_0 = 1;
            }
            if (line->atTokHasmore()) {
                (*secondary_antenna_rssi) = line->atTokNextint(&err);
                if (err < 0) {
                    logE(LOG_TAG, "ERROR occurs <secondary_antenna_rssi> form antenna info request");
                    goto error;
                } else {
                    // response for AT+ERFTX=8,0,1,0
                    // Ex: +ERFTX: 8,0,100
                    *rxState_1 = 1;
                }
                if (line->atTokHasmore()) {
                    // response for AT+ERFTX=8,0,1,1
                    // Ex: +ERFTX: 8,0,100,200,300
                    (*relative_phase) = line->atTokNextint(&err);
                    if (err < 0) {
                        logE(LOG_TAG, "ERROR occurs <relative_phase> form antenna info request");
                        goto error;
                    }
                }
            }
            ril_errno = RIL_E_SUCCESS;
        }
    } else {
        logE(LOG_TAG, "Set antenna testing type getting ERROR");
        goto error;
    }
error:
    resp = RfxMclMessage::obtainResponse(msg->getId(), ril_errno,
            RfxIntsData(response, 6), msg, false);
    responseToTelCore(resp);
}

void RmcNetworkRequestHandler::requestSetServiceState(const sp<RfxMclMessage>& msg) {
    int voice_reg_state, data_reg_state;
    int voice_roaming_type, data_roaming_type;
    int ril_voice_reg_state, ril_rata_reg_state;
    RIL_Errno ril_errno = RIL_E_GENERIC_FAILURE;
    sp<RfxAtResponse> p_response;
    sp<RfxMclMessage> resp;
    int *pInt = (int *)msg->getData()->getData();

    voice_reg_state = pInt[0];
    data_reg_state = pInt[1];
    voice_roaming_type = pInt[2];
    data_roaming_type = pInt[3];
    ril_voice_reg_state = pInt[4];
    ril_rata_reg_state = pInt[5];

    if ((voice_reg_state >= 0 && voice_reg_state <= 3) &&
            (data_reg_state >= 0 && data_reg_state <= 3) &&
            (voice_roaming_type >= 0 && voice_roaming_type <= 3) &&
            (data_roaming_type >= 0 && data_roaming_type <= 3) &&
            (ril_voice_reg_state >= 0 && ril_voice_reg_state <= 14) &&
            (ril_rata_reg_state >= 0 && ril_rata_reg_state <= 14)) {
        /*****************************
        * If all parameters are valid,
        * set to MD
        ******************************/
        p_response = atSendCommand(String8::format("AT+ESRVSTATE=%d,%d,%d,%d,%d,%d",
                voice_reg_state,
                data_reg_state,
                voice_roaming_type,
                data_roaming_type,
                ril_voice_reg_state,
                ril_rata_reg_state));
        if (p_response->getError() >= 0 && p_response->getSuccess() != 0) {
            ril_errno = RIL_E_SUCCESS;
        }
    }

    updateCellularPsState();

    resp = RfxMclMessage::obtainResponse(msg->getId(), ril_errno,
            RfxVoidData(), msg, false);
    responseToTelCore(resp);
}

void RmcNetworkRequestHandler::handleConfirmRatBegin(const sp<RfxMclMessage>& msg) {
    RFX_UNUSED(msg);
    sp<RfxAtResponse> p_response;
    bool ret = true;
    int err;
    int count = 0;

    while (ret) {
        p_response = atSendCommand("AT+ERPRAT");
        err = p_response->getError();

        ret = (err < 0 || 0 == p_response->getSuccess()) ? true:false;
        RFX_LOG_V(LOG_TAG, "confirmRatBegin, send command AT+ERPRAT, err = %d, ret=%d, count=%d",
            err, ret, count);
        count++;
        // If get wrong result, we need to check whether go on or not.
        if (ret) {
            if (count == 10) {
                logD(LOG_TAG, "confirmRatBegin, reach the maximum time, return directly.");
                break;
            }

            RIL_RadioState state = (RIL_RadioState) getMclStatusManager()->getIntValue(RFX_STATUS_KEY_RADIO_STATE);
            if (RADIO_STATE_UNAVAILABLE == state || RADIO_STATE_OFF == state) {
                logD(LOG_TAG, "confirmRatBegin, radio unavliable/off, return directly.");
                break;
            }
            // Go on retry after 5 seconds.
            sleep(5);
        }
    };
}

void RmcNetworkRequestHandler::handlePsNetworkStateEvent(const sp<RfxMclMessage>& msg) {
    // logE(LOG_TAG, "handlePsNetworkStateEvent start");
    int response[3];
    int *pInt = (int *)msg->getData()->getData();
    response[0] = pInt[0];
    response[2] = pInt[1];
    int operNumericLength = 0;

    int err;
    int skip;
    char *eops_response = NULL;
    sp<RfxMclMessage> urc;
    RfxAtLine* line;
    sp<RfxAtResponse> p_response;

    /* Format should be set during initialization */
    p_response = atSendCommandSingleline("AT+EOPS?", "+EOPS:");

    // check error
    err = p_response->getError();
    if (err != 0 ||
            p_response == NULL ||
            p_response->getSuccess() == 0 ||
            p_response->getIntermediates() == NULL) {
        logE(LOG_TAG, "EOPS got error response");
    } else {
        // handle intermediate
        line = p_response->getIntermediates();

        // go to start position
        line->atTokStart(&err);
        if (err >= 0) {
            /* <mode> */
            skip = line->atTokNextint(&err);
            if ((err >= 0) && (skip >= 0 && skip <= 4 && skip != 2)) {
                // a "+EOPS: 0" response is possible
                if (line->atTokHasmore()) {
                    /* <format> */
                    skip = line->atTokNextint(&err);
                    if (err >= 0 && skip == 2)
                    {
                        /* <oper> */
                        eops_response = line->atTokNextstr(&err);
                        // logE(LOG_TAG, "EOPS Get operator code %s", eops_response);
                        /* Modem might response invalid PLMN ex: "", "000000" , "??????", all convert to "000000" */
                        if (!((eops_response[0] >= '0') && (eops_response[0] <= '9'))) {
                            // logE(LOG_TAG, "EOPS got invalid plmn response");
                            memset(eops_response, 0, operNumericLength);
                        }
                    }
                }
            }
        }
    }
    if (eops_response != NULL) {
        response[1] = atoi(eops_response);
    } else {
        response[1] = 0;
    }
    // logD(LOG_TAG, "handlePsNetworkStateEvent, data_reg_state:%d, mcc_mnc:%d, rat:%d", response[0],
    //        response[1], response[2]);
    urc = RfxMclMessage::obtainUrc(RFX_MSG_URC_RESPONSE_PS_NETWORK_STATE_CHANGED,
            m_slot_id, RfxIntsData(response, 3));
    // response to TeleCore
    responseToTelCore(urc);

    urc = RfxMclMessage::obtainUrc(RFX_MSG_URC_RESPONSE_VOICE_NETWORK_STATE_CHANGED,
            m_slot_id, RfxVoidData());
    // response to TeleCore
    responseToTelCore(urc);

    return;
}

void RmcNetworkRequestHandler::resetVoiceRegStateCache(RIL_VOICE_REG_STATE_CACHE *voiceCache, RIL_CACHE_GROUP source) {
    pthread_mutex_lock(&s_voiceRegStateMutex[m_slot_id]);
    if (source == CACHE_GROUP_ALL) {
        (*voiceCache).registration_state = 0;
        (*voiceCache).lac = 0xffffffff;
        (*voiceCache).cid = 0x0fffffff;
        (*voiceCache).radio_technology = 0;
        (*voiceCache).base_station_id = 0;
        (*voiceCache).base_station_latitude = 0;
        (*voiceCache).base_station_longitude = 0;
        (*voiceCache).css = 0;
        (*voiceCache).system_id = 0;
        (*voiceCache).network_id = 0;
        (*voiceCache).roaming_indicator = 0;
        (*voiceCache).is_in_prl = 0;
        (*voiceCache).default_roaming_indicator = 0;
        (*voiceCache).denied_reason = 0;
        (*voiceCache).psc = -1;
        (*voiceCache).network_exist = 0;
    } else if (source == CACHE_GROUP_COMMON_REQ) {
        (*voiceCache).registration_state = 0;
        (*voiceCache).lac = 0xffffffff;
        (*voiceCache).cid = 0x0fffffff;
        (*voiceCache).radio_technology = 0;
        (*voiceCache).roaming_indicator = 0;
        (*voiceCache).denied_reason = 0;
        (*voiceCache).network_exist = 0;
    } else if (source == CACHE_GROUP_C2K) {
        (*voiceCache).base_station_id = 0;
        (*voiceCache).base_station_latitude = 0;
        (*voiceCache).base_station_longitude = 0;
        (*voiceCache).css = 0;
        (*voiceCache).system_id = 0;
        (*voiceCache).network_id = 0;
    } else {
        // source type invalid!!!
        logD(LOG_TAG, "updateVoiceRegStateCache(): source type invalid!!!");
    }
    pthread_mutex_unlock(&s_voiceRegStateMutex[m_slot_id]);
}

void RmcNetworkRequestHandler::resetDataRegStateCache(RIL_DATA_REG_STATE_CACHE *dataCache) {
    (*dataCache).registration_state = 0;
    (*dataCache).lac = 0xffffffff;
    (*dataCache).cid = 0x0fffffff;
    (*dataCache).radio_technology = 0;
    (*dataCache).denied_reason = 0;
    (*dataCache).max_simultaneous_data_call = 1;
    (*dataCache).tac = 0;
    (*dataCache).physical_cid = 0;
    (*dataCache).eci = 0;
    (*dataCache).csgid = 0;
    (*dataCache).tadv = 0;
}

void RmcNetworkRequestHandler::updateServiceStateValue() {
    pthread_mutex_lock(&s_voiceRegStateMutex[m_slot_id]);
    getMclStatusManager()->setServiceStateValue(RFX_STATUS_KEY_SERVICE_STATE,
            RfxNwServiceState(
                    voice_reg_state_cache[m_slot_id].registration_state,
                    data_reg_state_cache.registration_state,
                    voice_reg_state_cache[m_slot_id].radio_technology,
                    data_reg_state_cache.radio_technology));
    pthread_mutex_unlock(&s_voiceRegStateMutex[m_slot_id]);
}

void RmcNetworkRequestHandler::updateCellularPsState() {
}

void RmcNetworkRequestHandler::triggerPollNetworkState() {
    logD(LOG_TAG, "triggerPollNetworkState");

    // update signal strength
    atSendCommand("AT+ECSQ");

    // update voice/data/Operator
    sp<RfxMclMessage> urc = RfxMclMessage::obtainUrc(RFX_MSG_URC_RESPONSE_VOICE_NETWORK_STATE_CHANGED,
            m_slot_id, RfxVoidData());
    responseToTelCore(urc);
}

void RmcNetworkRequestHandler::onHandleTimer() {
    // do something
}

void RmcNetworkRequestHandler::onHandleEvent(const sp<RfxMclMessage>& msg) {
    int id = msg->getId();
    switch (id) {
        case RFX_MSG_EVENT_EXIT_EMERGENCY_CALLBACK_MODE:
            triggerPollNetworkState();
            break;
        case RFX_MSG_EVENT_FEMTOCELL_UPDATE:
            pthread_mutex_lock(&ril_nw_femtoCell_mutex);
            updateFemtoCellInfo();
            pthread_mutex_unlock(&ril_nw_femtoCell_mutex);
            break;
        case RFX_MSG_EVENT_CONFIRM_RAT_BEGIN:
            handleConfirmRatBegin(msg);
            break;
        case RFX_MSG_EVENT_PS_NETWORK_STATE:
            handlePsNetworkStateEvent(msg);
            break;
        default:
            logE(LOG_TAG, "onHandleEvent, should not be here");
            break;
    }
}

void RmcNetworkRequestHandler::printVoiceCache(RIL_VOICE_REG_STATE_CACHE cache) {
    logD(LOG_TAG, " VoiceCache: "
            "registration_state=%d lac=%x cid=%x radio_technology=%d base_station_id=%d "
            "base_station_latitude=%d base_station_longitude=%d css=%d system_id=%d "
            "network_id=%d roaming_indicator=%d is_in_prl=%d default_roaming_indicator=%d "
            "denied_reason=%d psc=%d network_exist=%d", cache.registration_state, cache.lac,
            cache.cid, cache.radio_technology, cache.base_station_id, cache.base_station_latitude,
            cache.base_station_longitude, cache.css, cache.system_id, cache.network_id,
            cache.roaming_indicator, cache.is_in_prl, cache.default_roaming_indicator,
            cache.denied_reason, cache.psc, cache.network_exist);
}

void RmcNetworkRequestHandler::updatePseudoCellMode() {
    sp<RfxAtResponse> p_response;
    char *property;
    char prop[RFX_PROPERTY_VALUE_MAX] = {0};

    p_response = atSendCommandSingleline("AT+EAPC?", "+EAPC:");
    if (p_response->getError() >= 0 && p_response->getSuccess() != 0) {
        // set property if modem support APC, EM will check this property to show APC setting
        rfx_property_set("ril.apc.support", "1");
        // check if the APC mode was set before, if yes, send the same at command again
        // AT+EAPC? was apc query command, if return it, means APC mode was not set before
        asprintf(&property, "persist.radio.apc.mode%d", m_slot_id);
        rfx_property_get(property, prop, "AT+EAPC?");
        RFX_LOG_V(LOG_TAG, "updatePseudoCellMode: %s = %s", property, prop);
        free(property);
        if (strcmp("AT+EAPC?", prop) != 0) {
            atSendCommand(prop);
        }
    }
}

void RmcNetworkRequestHandler::requestSetPseudoCellMode(const sp<RfxMclMessage>& msg) {
    RIL_Errno ril_errno = RIL_E_GENERIC_FAILURE;
    sp<RfxAtResponse> p_response;
    sp<RfxMclMessage> resp;
    char *property;
    char *cmd;
    int *pInt = (int *)msg->getData()->getData();
    int apc_mode = pInt[0];
    int urc_enable = pInt[1];
    int timer = pInt[2];
    /*
    *  apc_mode = 0: disable APC feature
    *  apc_mode = 1: set APC mode I, if detect a pseudo cell, not attach it
    *  apc_mode = 2: set APC mode II, if detect a pseudo cell, also attach it
    */
    asprintf(&cmd, "AT+EAPC=%d,%d,%d", apc_mode, urc_enable, timer);
    p_response = atSendCommand(cmd);
    if (p_response->getError() >= 0 && p_response->getSuccess() != 0) {
        ril_errno = RIL_E_SUCCESS;
    }
    if (ril_errno == RIL_E_SUCCESS) {
        asprintf(&property, "persist.radio.apc.mode%d", m_slot_id);
        rfx_property_set(property, cmd);
        free(property);
    } else {
        logE(LOG_TAG, "requestSetPseudoCellMode failed");
    }
    free(cmd);
    resp = RfxMclMessage::obtainResponse(msg->getId(), ril_errno,
            RfxVoidData(), msg, false);
    responseToTelCore(resp);
}


void RmcNetworkRequestHandler::setRoamingEnable(const sp<RfxMclMessage>& msg) {
    RIL_Errno ril_errno = RIL_E_GENERIC_FAILURE;
    sp<RfxAtResponse> p_response;
    sp<RfxMclMessage> resp;
    char *cmd = NULL;
    int *pInt = (int*)msg->getData()->getData();
    /*
        data[0] : phone id (0,1,2,3,...)
        data[1] : international_voice_text_roaming (0,1)
        data[2] : international_data_roaming (0,1)
        data[3] : domestic_voice_text_roaming (0,1)
        data[4] : domestic_data_roaming (0,1)
        data[5] : domestic_LTE_data_roaming (0,1)

    +EROAMBAR:<protocol_index>, (not ready now)
        <BAR_Dom_Voice_Roaming_Enabled>,
        <BAR_Dom_Data_Roaming_Enabled>,
        <Bar_Int_Voice_Roaming_Enabled>,
        <Bar_Int_Data_Roaming_Enabled>,
        <Bar_LTE_Data_Roaming_Enabled>
        NOTE: The order is different.
    */
    int err = 0;
    int (*p)[6] = (int(*)[6])pInt;
    // rever the setting from enable(fwk) to bar(md)
    for (int i = 1; i < 6; i++) {
        (*p)[i] = (*p)[i] == 0 ? 1: 0;
    }

    asprintf(&cmd, "AT+EROAMBAR=%d,%d,%d,%d,%d"
        , (*p)[3]  // BAR_Dom_Voice_Roaming_Enabled
        , (*p)[4]  // BAR_Dom_Data_Roaming_Enabled
        , (*p)[1]  // Bar_Int_Voice_Roaming_Enabled
        , (*p)[2]  // Bar_Int_Data_Roaming_Enabled
        , (*p)[5]);  // Bar_LTE_Data_Roaming_Enabled
    logD("setRoamingEnable %s", cmd);
    p_response = atSendCommand(cmd);
    if (p_response->getError() >= 0 && p_response->getSuccess() != 0) {
        ril_errno = RIL_E_SUCCESS;
    }
    free(cmd);
    resp = RfxMclMessage::obtainResponse(msg->getId(), ril_errno,
            RfxVoidData(), msg, false);
    responseToTelCore(resp);
}

void RmcNetworkRequestHandler::getRoamingEnable(const sp<RfxMclMessage>& msg) {
    /* +EROAMBAR:<protocol_index>, (not ready now)
        <BAR_Dom_Voice_Roaming_Enabled>,
        <BAR_Dom_Data_Roaming_Enabled>,
        <Bar_Int_Voice_Roaming_Enabled>,
        <Bar_Int_Data_Roaming_Enabled>,
        <Bar_LTE_Data_Roaming_Enabled>
     Expected Result:
     response[0]: phone id (0,1,2,3,...)
     response[1] : international_voice_text_roaming (0,1)
     response[2] : international_data_roaming (0,1)
     response[3] : domestic_voice_text_roaming (0,1)
     response[4] : domestic_data_roaming (0,1)
     response[5] : domestic_LTE_data_roaming (1) */
    RfxAtLine *line = NULL;
    sp<RfxAtResponse> p_response;
    sp<RfxMclMessage> resp;
    int roaming[6] = {1, 0, 1, 1, 1, 1};  // default value
    int err = 0;

    p_response = atSendCommandSingleline("AT+EROAMBAR?", "+EROAMBAR:");

    // check error
    err = p_response->getError();
    if (err != 0 ||
          p_response == NULL ||
          p_response->getSuccess() == 0 ||
          p_response->getIntermediates() == NULL)
        goto error;

    // handle intermediate
    line = p_response->getIntermediates();

    // go to start position
    line->atTokStart(&err);
    if (err < 0) goto error;

    /* //DS
     roaming[0] = line->atTokNextint(&err);
     if (err < 0) goto error; */

    roaming[0] = 0;

    // <BAR_Dom_Voice_Roaming_Enabled>
    roaming[3] = line->atTokNextint(&err);
    if (err < 0) goto error;

    // <BAR_Dom_Data_Roaming_Enabled>
    roaming[4] = line->atTokNextint(&err);
    if (err < 0) goto error;


    // <Bar_Int_Voice_Roaming_Enabled>
    roaming[1] = line->atTokNextint(&err);
    if (err < 0) goto error;


    // <Bar_Int_Data_Roaming_Enabled>
    roaming[2] = line->atTokNextint(&err);
    if (err < 0) goto error;


    // <Bar_LTE_Data_Roaming_Enabled>
    roaming[5] = line->atTokNextint(&err);
    if (err < 0) goto error;


    // rever the setting from enable(fwk) to bar(md)
    for (int i = 1; i < 6; i++) {
        roaming[i] = roaming[i] == 0 ? 1:0;
    }

    resp = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_SUCCESS,
                RfxIntsData(roaming, 6), msg, false);
    // response to TeleCore
    responseToTelCore(resp);
    return;
    error:
    logE(LOG_TAG, "getRoamingEnable must never return error when radio is on");
    resp = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_GENERIC_FAILURE,
                RfxVoidData(), msg, false);
    // response to TeleCore
    responseToTelCore(resp);
}

void RmcNetworkRequestHandler::requestStartNetworkScan(const sp<RfxMclMessage>& msg) {
    sp<RfxMclMessage> resp;
    RIL_NetworkScanRequest* p_args = (RIL_NetworkScanRequest*) msg->getData()->getData();

    logD(LOG_TAG, "requestStartNetworkScan::type:%d interval:%d length:%d",
            p_args->type, p_args->interval, p_args->specifiers_length);
    resp = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_REQUEST_NOT_SUPPORTED,
                RfxVoidData(), msg, false);
    responseToTelCore(resp);
    return;

/*
error:
    resp = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_REQUEST_NOT_SUPPORTED,
                RfxVoidData(), msg, false);
    responseToTelCore(resp);
*/
}

void RmcNetworkRequestHandler::requestStopNetworkScan(const sp<RfxMclMessage>& msg) {
    sp<RfxMclMessage> resp;

    resp = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_REQUEST_NOT_SUPPORTED,
                RfxVoidData(), msg, false);
    responseToTelCore(resp);
    return;
/*
error:
    resp = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_REQUEST_NOT_SUPPORTED,
                RfxVoidData(), msg, false);
    responseToTelCore(resp);
*/
}

void RmcNetworkRequestHandler::requestGetPseudoCellInfo(const sp<RfxMclMessage>& msg) {
    sp<RfxAtResponse> p_response;
    sp<RfxMclMessage> resp;
    RfxAtLine* line;
    int err;
    int apc_mode;
    int urc_enable;
    int timer;
    int num;

    // <apc_mode>[<urc_enable><time>
    // <num>[<type><plmn><lac><cid><arfcn><bsic>[<type><plmn><lac><cid><arfcn><bsic>]]]
    // num: 0 or 1 or 2
    int response[16] = {0};

    p_response = atSendCommandSingleline("AT+EAPC?", "+EAPC:");
    if (p_response->getError() < 0 || p_response->getSuccess() == 0) {
        goto error;
    }

    /*  response:
     *    +EAPC:<apc_mode>[,<urc_enable>,<time>,<count>
     *      [,<type>,<plmn>,<lac>,<cid>,<arfcn>,<bsic>[,<type>,<plmn>,<lac>,<cid>,<arfcn>,<bsic>]]]
     */
    line = p_response->getIntermediates();

    line->atTokStart(&err);
    if (err < 0) goto error;

    apc_mode = line->atTokNextint(&err);
    if (err < 0) goto error;
    response[0] = apc_mode;

    if (line->atTokHasmore()) {
        urc_enable = line->atTokNextint(&err);
        if (err < 0) goto error;
        response[1] = urc_enable;

        timer = line->atTokNextint(&err);
        if (err < 0) goto error;
        response[2] = timer;

        num = line->atTokNextint(&err);
        if (err < 0) goto error;
        response[3] = num;

        for (int i = 0; i < num; i++) {
            response[i*6 + 4] = line->atTokNextint(&err);
            if (err < 0) goto error;

            if (line->atTokHasmore()) {
                response[i*6 + 5] = line->atTokNextint(&err);
                if (err < 0) goto error;

                response[i*6 + 6] = line->atTokNextint(&err);
                if (err < 0) goto error;

                response[i*6 + 7] = line->atTokNextint(&err);
                if (err < 0) goto error;

                response[i*6 + 8] = line->atTokNextint(&err);
                if (err < 0) goto error;

                response[i*6 + 9] = line->atTokNextint(&err);
                if (err < 0) goto error;
            }
        }
    }

    resp = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_SUCCESS,
            RfxIntsData(response, 16), msg, false);
    responseToTelCore(resp);
    return;

error:
    logE(LOG_TAG, "requestGetPseudoCellInfo failed err=%d", p_response->getError());
    resp = RfxMclMessage::obtainResponse(msg->getId(), RIL_E_GENERIC_FAILURE,
            RfxVoidData(), msg, false);
    responseToTelCore(resp);
}

int RmcNetworkRequestHandler::isDisable2G()
{
    int ret = 0;
    char property_value[PROPERTY_VALUE_MAX] = {0};
    char optr[PROPERTY_VALUE_MAX] = {0};

    property_get("persist.operator.optr", optr, "");
    property_get("persist.radio.disable.2g", property_value, "0");
    RFX_LOG_V(LOG_TAG, "[isDisable2G] optr:%s, disable.2g:%s", optr, property_value);

    if (strcmp("OP07", optr) == 0) {
        if (atoi(property_value) == 1) {
            ret = 1;
        } else {
            ret = 0;
        }
    } else {
        ret = 0;
    }

    return ret;
}
