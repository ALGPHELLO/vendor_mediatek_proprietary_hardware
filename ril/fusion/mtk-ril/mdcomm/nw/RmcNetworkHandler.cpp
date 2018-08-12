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

#include "RmcNetworkHandler.h"
#include <math.h>                          /* log10 */
#include <cutils/properties.h>
#include "RfxViaUtils.h"
#include "ViaBaseHandler.h"

static const char PROPERTY_NW_LTE_SIGNAL[MAX_SIM_COUNT][MAX_PROP_CHARS] = {
    "ril.nw.signalstrength.lte.1",
    "ril.nw.signalstrength.lte.2",
    "ril.nw.signalstrength.lte.3",
    "ril.nw.signalstrength.lte.4",
};

char const *sOp12Plmn[] = {
    "311278", "311483", "310004", "311283", "311488",
    "310890", "311272", "311288", "311277", "311482",
    "311282", "311487", "310590", "311287", "311271",
    "311276", "311481", "311281", "311486", "310013",
    "311286", "311270", "311275", "311480", "311280",
    "311485", "310012", "311285", "311110", "311274",
    "311390", "311279", "311484", "310010", "311284",
    "311489", "310910", "311273", "311289"
};

int RmcNetworkHandler::ECELLext3ext4Support = 1;
pthread_mutex_t RmcNetworkHandler::ril_nw_femtoCell_mutex;
RIL_FEMTO_CELL_CACHE RmcNetworkHandler::m_femto_cell_cache;
pthread_mutex_t RmcNetworkHandler::s_signalStrengthMutex[MAX_SIM_COUNT];
RIL_SIGNAL_STRENGTH_CACHE RmcNetworkHandler::signal_strength_cache[MAX_SIM_COUNT];
pthread_mutex_t RmcNetworkHandler::s_voiceRegStateMutex[MAX_SIM_COUNT];
RIL_VOICE_REG_STATE_CACHE RmcNetworkHandler::voice_reg_state_cache[MAX_SIM_COUNT];
pthread_mutex_t RmcNetworkHandler::s_wfcRegStatusMutex[MAX_SIM_COUNT];
int RmcNetworkHandler::ril_wfc_reg_status[MAX_SIM_COUNT];
RIL_EonsNetworkFeatureInfo RmcNetworkHandler::eons_info[MAX_SIM_COUNT];
int RmcNetworkHandler::mPlmnListOngoing = 0;
int RmcNetworkHandler::mPlmnListAbort = 0;
String8 RmcNetworkHandler::mCurrentLteSignal[MAX_SIM_COUNT];

RmcNetworkHandler::RmcNetworkHandler(int slot_id, int channel_id) :
        RfxBaseHandler(slot_id, channel_id) {
    // logD(LOG_TAG, "RmcNetworkHandler[%d] start", m_slot_id);
    pthread_mutex_init(&s_signalStrengthMutex[m_slot_id], NULL);
    pthread_mutex_init(&s_voiceRegStateMutex[m_slot_id], NULL);
    pthread_mutex_init(&s_wfcRegStatusMutex[m_slot_id], NULL);
    eons_info[m_slot_id] = {EONS_INFO_NOT_RECEIVED, 0};
    mCurrentLteSignal[m_slot_id] = "";
}

/* Android framework expect spec 27.007  AT+CSQ <rssi> 0~31 format when handling 3G signal strength.
   So we convert 3G signal to <rssi> in RILD */
int RmcNetworkHandler::convert3GRssiValue(int rscp_in_dbm)
{
    int rssi = 0;
    int INVALID = 0x7FFFFFFF;

    if (rscp_in_dbm == INVALID) {
        return rssi;
    }

    // logD(LOG_TAG, "convert3GRssiValue rscp_in_dbm = %d", rscp_in_dbm);

    rssi = (rscp_in_dbm + 113) / 2;

    if (rssi > 31) {
        rssi = 31;
    }

    return rssi;
}

int RmcNetworkHandler::getSignalStrength(RfxAtLine *line)
{
    int err;
    MD_SIGNAL_STRENGTH ecsq;

    //Use int max, as -1 is a valid value in signal strength
    int INVALID = 0x7FFFFFFF;

    // 93 modem
    // +ECSQ: <sig1>,<sig2>,<rssi_in_qdbm>,<rscp_in_qdbm>,<ecn0_in_qdbm>,<rsrq_in_qdbm>,<rsrp_in_qdbm>,<Act>,<sig3>,<serv_band>
    // +ECSQ: <sig1>,<sig2>,<rssi_in_qdbm>,<rscp_in_qdbm>,<ecn0_in_qdbm>,<rsrq_in_qdbm>,<rsrp_in_qdbm>,<256>,<sig3>,<serv_band>
    // +ECSQ: <sig1>,<sig2>,<rssi_in_qdbm>,<rscp_in_qdbm>,<ecn0_in_qdbm>,<rsrq_in_qdbm>,<rsrp_in_qdbm>,<Act_EvDo>,<sig3>,<serv_band>

    int rscp_in_dbm;
    float sinr_db;
    float temp;
    int cdma_dbm = -1;
    int cdma_ecio = -1;
    int evdo_dbm = -1;
    int evdo_ecio = -1;

    ViaBaseHandler *mViaHandler = RfxViaUtils::getViaHandler();

    // go to start position
    line->atTokStart(&err);
    if (err < 0) return -1;

    ecsq.sig1 = line->atTokNextint(&err);
    if (err < 0) return -1;

    ecsq.sig2 = line->atTokNextint(&err);
    if (err < 0) return -1;

    ecsq.rssi_in_qdbm = line->atTokNextint(&err);
    if (err < 0) return -1;

    // 3G part
    ecsq.rscp_in_qdbm = line->atTokNextint(&err);
    if (err < 0) {
        return -1;
    } else {
        rscp_in_dbm = (ecsq.rscp_in_qdbm/4)*(-1);
        if (rscp_in_dbm > 120) {
            rscp_in_dbm = 120;
        } else if (rscp_in_dbm < 25) {
            rscp_in_dbm = 25;
        }
    }

    ecsq.ecn0_in_qdbm = line->atTokNextint(&err);
    if (err < 0) return -1;

    // for LTE
    ecsq.rsrq_in_qdbm = line->atTokNextint(&err);
    if (err < 0) return -1;

    ecsq.rsrp_in_qdbm = line->atTokNextint(&err);
    if (err < 0) return -1;

    ecsq.act = line->atTokNextint(&err);
    if (err < 0) return -1;

    ecsq.sig3 = line->atTokNextint(&err);
    if (err < 0) return -1;

    if ((ecsq.act == 0x1000) || (ecsq.act == 0x2000)) {
        ecsq.rsrq_in_qdbm = (ecsq.rsrq_in_qdbm/4) * (-1);
        if (ecsq.rsrq_in_qdbm > 20) {
            ecsq.rsrq_in_qdbm = 20;
        } else if (ecsq.rsrq_in_qdbm < 3) {
            ecsq.rsrq_in_qdbm = 3;
        }

        ecsq.rsrp_in_qdbm = (ecsq.rsrp_in_qdbm/4) * (-1);
        if (ecsq.rsrp_in_qdbm > 140) {
            ecsq.rsrp_in_qdbm = 140;
        } else if (ecsq.rsrp_in_qdbm < 44) {
            ecsq.rsrp_in_qdbm = 44;
        }

        if (ecsq.sig3 != 0x7FFF) {
            ecsq.sig3 = (ecsq.sig3 * 10) / 4;
            if (ecsq.sig3 > 300) {
                ecsq.sig3 = 300;
            } else if (ecsq.sig3 < -200) {
                ecsq.sig3 = -200;
            }
        }
    }

    ecsq.serv_band = line->atTokNextint(&err);
    if (err < 0) return -1;

    // validate information for each AcT
    if ((ecsq.act == 0x0001) || (ecsq.act == 0x0002)) {  // for GSM
        if (ecsq.sig1 < 0  || ecsq.sig1 > 63) {
            logE(LOG_TAG, "Recevice an invalid <rssi> value from modem");
            return -1;
        }
        // It's normal that <ber> is 99 with GSM sometimes.
        if ((ecsq.sig2 < 0 || ecsq.sig2 > 7) && ecsq.sig2 != 99) {
            logE(LOG_TAG, "Recevice an invalid <ber> value from modem");
            return -1;
        }
        resetSignalStrengthCache(&signal_strength_cache[m_slot_id], CACHE_GROUP_GSM);
        signal_strength_cache[m_slot_id].gw_signal_strength = ((ecsq.rssi_in_qdbm/4)+113) / 2;
        signal_strength_cache[m_slot_id].gw_bit_error_rate = ecsq.sig2;
    } else if ((ecsq.act >= 0x0004) && (ecsq.act <= 0x00e0)) { // for UMTS
        if (ecsq.sig1 < 0  || ecsq.sig1 > 96) {
            logE(LOG_TAG, "Recevice an invalid <rscp> value from modem");
            return -1;
        }
        if (ecsq.sig2 < 0 || ecsq.sig2 > 49) {
            logE(LOG_TAG, "Recevice an invalid <ecn0> value from modem");
            return -1;
        }
        resetSignalStrengthCache(&signal_strength_cache[m_slot_id], CACHE_GROUP_GSM);
        int rssi = convert3GRssiValue(rscp_in_dbm*(-1));
        // in order not to change AOSP interface, put both TDD & FDD rscp in td_scdma_rscp
        signal_strength_cache[m_slot_id].td_scdma_rscp = rscp_in_dbm;
        // logD(LOG_TAG, "rscp = %d", signal_strength_cache[m_slot_id].td_scdma_rscp);
    } else if (ecsq.act == 0x0100) {  // for 1xRTT
        resetSignalStrengthCache(&signal_strength_cache[m_slot_id], CACHE_GROUP_1XRTT);
        if (ecsq.sig1 < 0  || ecsq.sig1 > 31) {
            logE(LOG_TAG, "Recevice an invalid <rssi> value from modem");
            return -1;
        }
        if (ecsq.sig2 < -128 || ecsq.sig2 > 0) {
            // logE(LOG_TAG, "Recevice an invalid <ec/io> value from modem");
            return -1;
        }

        if (mViaHandler != NULL) {
            cdma_dbm = mViaHandler->convertCdmaEvdoSig(ecsq.sig1, SIGNAL_CDMA_DBM);
            cdma_ecio = mViaHandler->convertCdmaEvdoSig(ecsq.sig2, SIGNAL_CDMA_ECIO);
        }
        signal_strength_cache[m_slot_id].cdma_dbm = cdma_dbm;
        // logD(LOG_TAG, "signal_strength_cache.cdma_dbm = %d",
        //         signal_strength_cache[m_slot_id].cdma_dbm);
        // -5 => -10 / 2 (-10 follow AOSP ril define, 2 is to normalize MD3 ecio data)
        signal_strength_cache[m_slot_id].cdma_ecio = cdma_ecio;
        // logD(LOG_TAG, "signal_strength_cache.cdma_ecio = %d",
        //         signal_strength_cache[m_slot_id].cdma_ecio);
    } else if ((ecsq.act == 0x0200) || (ecsq.act == 0x0400)) {  // for EVDO
        resetSignalStrengthCache(&signal_strength_cache[m_slot_id], CACHE_GROUP_EVDO);
        if (ecsq.sig1 < 0  || ecsq.sig1 > 31) {
            logE(LOG_TAG, "Recevice an invalid <rssi> value from modem");
            return -1;
        }
        if (ecsq.sig2 < -512 || ecsq.sig2 > 0) {
            // logE(LOG_TAG, "Recevice an invalid <ec/io> value from modem");
            return -1;
        }

        if (mViaHandler != NULL) {
            evdo_dbm = mViaHandler->convertCdmaEvdoSig(ecsq.sig1, SIGNAL_EVDO_DBM);
            evdo_ecio = mViaHandler->convertCdmaEvdoSig(ecsq.sig2, SIGNAL_EVDO_ECIO);
        }
        signal_strength_cache[m_slot_id].evdo_dbm = evdo_dbm;
        // logD(LOG_TAG, "signal_strength_cache.evdo_dbm = %d",
        //         signal_strength_cache[m_slot_id].evdo_dbm);
        signal_strength_cache[m_slot_id].evdo_ecio = evdo_ecio;
        // logD(LOG_TAG, "signal_strength_cache.evdo_ecio = %d",
        //         signal_strength_cache[m_slot_id].evdo_ecio);
        temp = (double)ecsq.sig3 / 512;
        sinr_db = 100 * log10(temp);
        // logD(LOG_TAG, "sinr:%d, sinr_dB:%f", ecsq.sig3, sinr_db);
        if (sinr_db >= 100) {
            signal_strength_cache[m_slot_id].evdo_snr = 8;
        } else if (sinr_db >= 70) {
            signal_strength_cache[m_slot_id].evdo_snr = 7;
        } else if (sinr_db >= 50) {
            signal_strength_cache[m_slot_id].evdo_snr = 6;
        } else if (sinr_db >= 30) {
            signal_strength_cache[m_slot_id].evdo_snr = 5;
        } else if (sinr_db >= -20) {
            signal_strength_cache[m_slot_id].evdo_snr = 4;
        } else if (sinr_db >= -45) {
            signal_strength_cache[m_slot_id].evdo_snr = 3;
        } else if (sinr_db >= -90) {
            signal_strength_cache[m_slot_id].evdo_snr = 2;
        } else if (sinr_db > -120) {
            signal_strength_cache[m_slot_id].evdo_snr = 1;
        } else {
            signal_strength_cache[m_slot_id].evdo_snr = 0;
        }
    } else if ((ecsq.act == 0x1000) || (ecsq.act == 0x2000)) {  // for LTE
        if (ecsq.sig1 < 0  || ecsq.sig1 > 34) {
            // logE(LOG_TAG, "Recevice an invalid <rsrq> value from modem");
            return -1;
        }
        if (ecsq.sig2 < 0 || ecsq.sig2 > 97) {
            logE(LOG_TAG, "Recevice an invalid <rsrp> value from modem");
            return -1;
        }
        resetSignalStrengthCache(&signal_strength_cache[m_slot_id], CACHE_GROUP_GSM);
        // Forces SignalStrength.getGsmLevel() return SIGNAL_STRENGTH_NONE_OR_UNKOWN
        // during the determination of signal strength in framework.
        // sig1 represents mGsmSignalStrength in framework
        signal_strength_cache[m_slot_id].gw_signal_strength = 0;
        signal_strength_cache[m_slot_id].gw_bit_error_rate = ecsq.sig2;
        signal_strength_cache[m_slot_id].lte_rsrp = ecsq.rsrp_in_qdbm;
        // logD(LOG_TAG, "signal_strength_cache.lte_rsrp = %d",
        //         signal_strength_cache[m_slot_id].lte_rsrp);
        signal_strength_cache[m_slot_id].lte_rsrq = ecsq.rsrq_in_qdbm;
        signal_strength_cache[m_slot_id].lte_rssnr = ecsq.sig3;
        updateSignalStrengthProperty();
    } else {
        // logE(LOG_TAG, "Recevice an invalid <eAcT> value from modem");
        return -1;
    }
    return 0;
}

unsigned int RmcNetworkHandler::convertRegState(unsigned int uiRegState, bool isVoiceState)
{
    unsigned int uiRet = 0;

    switch (uiRegState)
    {
        case 6:         // Registered for "SMS only", home network
            uiRet = 1;  // Registered
            break;
        case 7:         // Registered for "SMS only", roaming
            uiRet = 5;  // roaming
            break;
        case 8:         // attached for emergency bearer service only
            uiRet = 0;  // not registered
            break;
        case 9:         // registered for "CSFB not prefereed", home network
            uiRet = 1;  // Registered
            break;
        case 10:        // registered for "CSFB not prefereed", roaming
            uiRet = 5;  // roaming
            break;
        case 101:       // no NW, but need to find NW
            if (isVoiceState) {
                uiRet = 1;  // 1xRTT normal service, Mapping '+VSER:0'
            } else {
                uiRet = 0;  // not registered
            }
            break;
        case 102:       // not registered, but MT find 1X NW existence
            if (isVoiceState) {
                uiRet = 2; // 1x is searching
            } else {
                uiRet = 0; // not registered
            }
            break;
        case 103:       // not registered, but MT find Do NW existence
            uiRet = 0;  // not registered
            break;
        case 104:       // not registered, but MT find Do&1X NW existence
            uiRet = 0;  // not registered
            break;
        default:
            uiRet = uiRegState;
            break;
    }

    return uiRet;
}

void RmcNetworkHandler::resetSignalStrengthCache(RIL_SIGNAL_STRENGTH_CACHE *sigCache, RIL_CACHE_GROUP source) {
    // logD(LOG_TAG, "resetSignalStrengthCache(): src=%s", sourceToString(source));
    if (source == CACHE_GROUP_GSM) {
        (*sigCache).gw_signal_strength = 99;
        (*sigCache).gw_bit_error_rate = -1;
        (*sigCache).lte_signal_strength = 99;
        (*sigCache).lte_rsrp = 0x7FFFFFFF;
        (*sigCache).lte_rsrq = 0x7FFFFFFF;
        (*sigCache).lte_rssnr = 0x7FFFFFFF;
        (*sigCache).lte_cqi = 0x7FFFFFFF;
        (*sigCache).lte_timing_advance = 0x7FFFFFFF;
        (*sigCache).td_scdma_rscp = 0x7FFFFFFF;
        updateSignalStrengthProperty();
    } else if (source == CACHE_GROUP_C2K) {
        (*sigCache).cdma_dbm = -1;
        (*sigCache).cdma_ecio = -1;
        (*sigCache).evdo_dbm = -1;
        (*sigCache).evdo_ecio = -1;
        (*sigCache).evdo_snr = -1;
    } else if (source == CACHE_GROUP_1XRTT) {
        (*sigCache).cdma_dbm = -1;
        (*sigCache).cdma_ecio = -1;
    } else if (source == CACHE_GROUP_EVDO) {
        (*sigCache).evdo_dbm = -1;
        (*sigCache).evdo_ecio = -1;
        (*sigCache).evdo_snr = -1;
    } else if (source == CACHE_GROUP_ALL) {
        (*sigCache).gw_signal_strength = 99;
        (*sigCache).gw_bit_error_rate = -1;
        (*sigCache).cdma_dbm = -1;
        (*sigCache).cdma_ecio = -1;
        (*sigCache).evdo_dbm = -1;
        (*sigCache).evdo_ecio = -1;
        (*sigCache).evdo_snr = -1;
        (*sigCache).lte_signal_strength = 99;
        (*sigCache).lte_rsrp = 0x7FFFFFFF;
        (*sigCache).lte_rsrq = 0x7FFFFFFF;
        (*sigCache).lte_rssnr = 0x7FFFFFFF;
        (*sigCache).lte_cqi = 0x7FFFFFFF;
        (*sigCache).lte_timing_advance = 0x7FFFFFFF;
        (*sigCache).td_scdma_rscp = 0x7FFFFFFF;
        updateSignalStrengthProperty();
    } else {
        // source type invalid
    }
}

bool RmcNetworkHandler::isTdd3G() {
    bool isTdd3G = false;
    char worldMode_prop[RFX_PROPERTY_VALUE_MAX] = {0};
    int worldMode = 0;

    rfx_property_get("ril.nw.worldmode.activemode", worldMode_prop, "1");
    worldMode = atoi(worldMode_prop);
    if (worldMode == 2) {
        isTdd3G = true;
    }
    return isTdd3G;
}

int RmcNetworkHandler::isFemtocellSupport() {
    int isFemtocellSupport = 0;
    char optr[RFX_PROPERTY_VALUE_MAX] = {0};

    rfx_property_get("ro.mtk_femto_cell_support", optr, "0");
    isFemtocellSupport = atoi(optr);

    return isFemtocellSupport;
}

String8 RmcNetworkHandler::getCurrentLteSignal(int slotId) {
    if (mCurrentLteSignal[slotId] == "") {
        char tempstr[RFX_PROPERTY_VALUE_MAX];
        memset(tempstr, 0, sizeof(tempstr));
        rfx_property_get(PROPERTY_NW_LTE_SIGNAL[slotId], tempstr, "");
        mCurrentLteSignal[slotId] = String8(tempstr);
    }
    return mCurrentLteSignal[slotId];
}

void RmcNetworkHandler::updateSignalStrengthProperty() {
    int rsrp = 0x7FFFFFFF;
    String8 propString;
    if (signal_strength_cache[m_slot_id].lte_rsrp != 0x7FFFFFFF) {
        rsrp = signal_strength_cache[m_slot_id].lte_rsrp * (-1);
    }
    propString = String8::format("%d,%d", rsrp, signal_strength_cache[m_slot_id].lte_rssnr/10);

    if (getCurrentLteSignal(m_slot_id) != propString) {
        rfx_property_set(PROPERTY_NW_LTE_SIGNAL[m_slot_id], propString.string());
        mCurrentLteSignal[m_slot_id] = propString;
    }
}

int RmcNetworkHandler::isOp12Plmn(const char* plmn) {
    unsigned long i = 0;
    if (plmn != NULL) {
        for (i = 0 ; i < (sizeof(sOp12Plmn)/sizeof(sOp12Plmn[0])) ; i++) {
            if (strcmp(plmn, sOp12Plmn[i]) == 0) {
                //LOGD("[isOp12Plmn] plmn:%s", plmn);
                return true;
            }
        }
    }
    return false;
}

char const *RmcNetworkHandler::sourceToString(int srcId) {
    switch (srcId) {
        case CACHE_GROUP_GSM:
            return "GSM";
        case CACHE_GROUP_C2K:
            return "C2K";
        case CACHE_GROUP_1XRTT:
            return "1XRTT";
        case CACHE_GROUP_EVDO:
            return "EVDO";
        case CACHE_GROUP_ALL:
            return "ALL";
        default:
            return "INVALID SRC";
    }
}

int RmcNetworkHandler::getCellInfoListV12(RfxAtLine* line, int num,
        RIL_CellInfo_v12 * response) {
    int INVALID = INT_MAX; // 0x7FFFFFFF;
    int err=0, i=0,act=0 ,cid=0,mcc=0,mnc=0,lacTac=0,pscPci=0;
    int sig1=0,sig2=0,rsrp=0,rsrq=0,rssnr=0,cqi=0,timingAdvance=0;
    int bsic=0;
    int arfcn=INVALID;
    /* C2K related cell info. */
    int nid = 0;
    int sid = 0;
    int base_station_id = 0;
    int base_station_longitude = 0;
    int base_station_latitude = 0;
    int cdma_dbm = INVALID;
    int cdma_ecio = INVALID;
    int evdo_dbm = INVALID;
    int evdo_ecio = INVALID;
    int snr = 0;
    int evdo_snr = INVALID;

     /* +ECELL: <num_of_cell>[,<act>,<cid>,<lac_or_tac>,<mcc>,<mnc>,
             <psc_or_pci>, <sig1>,<sig2>,<sig1_in_dbm>,<sig2_in_dbm>,<ta>,
             <ext1>,<ext2>,]
             [<ext3>,<ext4>]
             [,...]
             [,<Act>,<nid>,<sid>,<bs_id>,<bs_long>,<bs_lat>,<1xRTT_rssi>,
             <1xRTT_ec/io>,<EVDO_rssi>,<EVDO_ec/io>,<EVDO_snr>]
    */
     // ext3 is for gsm bsic
     // ext4 is for arfcn, uarfcn, earfch
    int ext3_ext4_enabled = ECELLext3ext4Support; //set to 1 if modem support

    for(i=0;i<num;i++){
        /* Registered field is used to tell serving cell or neighboring cell.
           The first cell info returned is the serving cell,
           others are neighboring cell */
        if(i==0)
            response[i].registered = 1;

        act = line->atTokNextint(&err);
        if (err < 0) goto error;

        if (act == 0 || act == 2 || act == 7) {
            cid = line->atTokNexthexint(&err);
            if (err < 0)
                goto error;

            lacTac = line->atTokNexthexint(&err);
            if (err < 0) {
                lacTac = INVALID;
            }
            mcc = line->atTokNextint(&err);
            if (err < 0) {
                mcc = INVALID;
            }
            mnc = line->atTokNextint(&err);
            if (err < 0) {
                mnc = INVALID;
            }
            pscPci = line->atTokNextint(&err);
            if (err < 0) {
                pscPci = INVALID;
            }
            sig1 = line->atTokNextint(&err);
            if (err < 0) {
                sig1 = INVALID;
            }
            sig2 = line->atTokNextint(&err);
            if (err < 0) {
                sig2 = INVALID;
            }
            rsrp = line->atTokNextint(&err);
            if (err < 0) {
                rsrp = INVALID;
            }
            rsrq = line->atTokNextint(&err);
            if (err < 0) {
                rsrq = INVALID;
            }
            timingAdvance = line->atTokNextint(&err);
            if (err < 0) {
                timingAdvance = INVALID;
            }
            rssnr = line->atTokNextint(&err);
            if (err < 0) {
                rssnr = INVALID;
            }
            cqi = line->atTokNextint(&err);
            if (err < 0) {
                cqi = INVALID;
            }
            if (ext3_ext4_enabled) {
                bsic = line->atTokNextint(&err);
                if (err < 0) {
                    bsic = 0;
                }
                arfcn = line->atTokNextint(&err);
                if (err < 0) {
                    arfcn = INVALID;
                }
            }
            logD(LOG_TAG, "act=%d,cid=%d,mcc=%d,mnc=%d,lacTac=%d,pscPci=%d,sig1=%d,sig2=%d,"
                    "sig1_dbm=%d,sig1_dbm=%d,ta=%d,rssnr=%d,cqi=%d,bsic=%d,arfcn=%d",
                    act, cid, mcc, mnc, lacTac, pscPci, sig1, sig2, rsrp, rsrq,
                    timingAdvance, rssnr, cqi, bsic, arfcn);
        } else if (act == 256) {
            response[i].registered = 1;
            nid = line->atTokNextint(&err);
            if (err < 0) {
                nid = INVALID;
            }
            sid = line->atTokNextint(&err);
            if (err < 0) {
                sid = INVALID;
            }
            base_station_id = line->atTokNextint(&err);
            if (err < 0) {
                base_station_id = INVALID;
            }
            base_station_longitude = line->atTokNextint(&err);
            if (err < 0) {
                base_station_longitude = INVALID;
            }
            base_station_latitude = line->atTokNextint(&err);
            if (err < 0) {
                base_station_latitude = INVALID;
            }
            cdma_dbm = line->atTokNextint(&err);
            if (err < 0 || cdma_dbm < 0  || cdma_dbm > 31) {
                cdma_dbm = INVALID;
            }
            cdma_ecio = line->atTokNextint(&err);
            if (err < 0 || cdma_ecio < -128 || cdma_ecio > 0) {
                cdma_ecio = INVALID;
            }
            evdo_dbm = line->atTokNextint(&err);
            if (err < 0 || evdo_dbm < 0  || evdo_dbm > 31) {
                evdo_dbm = INVALID;
            }
            evdo_ecio = line->atTokNextint(&err);
            if (err < 0 || evdo_ecio < -512 || evdo_ecio > 0) {
                evdo_ecio = INVALID;
            }
            snr = line->atTokNextint(&err);
            if (err < 0) {
                snr = INVALID;
            }
            if (snr != INVALID) {
                float temp = (double) snr / 512;
                float snr_db = 100 * log10(temp);
                if (snr_db >= 100) {
                    evdo_snr = 8;
                } else if (snr_db >= 70) {
                    evdo_snr = 7;
                } else if (snr_db >= 50) {
                    evdo_snr = 6;
                } else if (snr_db >= 30) {
                    evdo_snr = 5;
                } else if (snr_db >= -20) {
                    evdo_snr = 4;
                } else if (snr_db >= -45) {
                    evdo_snr = 3;
                } else if (snr_db >= -90) {
                    evdo_snr = 2;
                } else if (snr_db > -120) {
                    evdo_snr = 1;
                } else {
                    evdo_snr = 0;
                }
            }
            logD(LOG_TAG, "nid=%d,sid=%d,base_station_id=%d,"
                    "base_station_longitude=%d,base_station_latitude=%d,"
                    "cdma_dbm=%d,cdma_ecio=%d,evdo_dbm=%d,evdo_ecio=%d,"
                    "snr=%d,evdo_snr=%d",
                    nid, sid, base_station_id, base_station_longitude,
                    base_station_latitude, cdma_dbm, cdma_ecio, evdo_dbm,
                    evdo_ecio, snr, evdo_snr);
        } else {
            logD(LOG_TAG, "RIL_CELL_INFO_TYPE invalid act=%d", act);
        }

        /* <Act>  0: GSM , 2: UMTS , 7: LTE, 256: 1x */
        if(act == 7) {
            response[i].cellInfoType = RIL_CELL_INFO_TYPE_LTE;
            response[i].CellInfo.lte.cellIdentityLte.ci = cid;
            response[i].CellInfo.lte.cellIdentityLte.mcc = mcc;
            response[i].CellInfo.lte.cellIdentityLte.mnc = mnc;
            response[i].CellInfo.lte.cellIdentityLte.tac = lacTac;
            response[i].CellInfo.lte.cellIdentityLte.pci = pscPci;
            response[i].CellInfo.lte.cellIdentityLte.earfcn = arfcn;
            response[i].CellInfo.lte.signalStrengthLte.signalStrength = sig1;
            response[i].CellInfo.lte.signalStrengthLte.rsrp = -rsrp / 4;
            response[i].CellInfo.lte.signalStrengthLte.rsrq = -rsrq / 4;
            response[i].CellInfo.lte.signalStrengthLte.timingAdvance = timingAdvance;
            response[i].CellInfo.lte.signalStrengthLte.rssnr = rssnr;
            response[i].CellInfo.lte.signalStrengthLte.cqi = cqi;
        } else if(act == 2) {
            response[i].cellInfoType = RIL_CELL_INFO_TYPE_WCDMA;
            response[i].CellInfo.wcdma.cellIdentityWcdma.cid = cid;
            response[i].CellInfo.wcdma.cellIdentityWcdma.mcc = mcc;
            response[i].CellInfo.wcdma.cellIdentityWcdma.mnc = mnc;
            response[i].CellInfo.wcdma.cellIdentityWcdma.lac = lacTac;
            response[i].CellInfo.wcdma.cellIdentityWcdma.psc = pscPci;
            response[i].CellInfo.wcdma.cellIdentityWcdma.uarfcn = arfcn;
            response[i].CellInfo.wcdma.signalStrengthWcdma.signalStrength = sig1;
            response[i].CellInfo.wcdma.signalStrengthWcdma.bitErrorRate = sig2;
        } else if (act == 0) {
            response[i].cellInfoType = RIL_CELL_INFO_TYPE_GSM;
            response[i].CellInfo.gsm.cellIdentityGsm.cid = cid;
            response[i].CellInfo.gsm.cellIdentityGsm.mcc = mcc;
            response[i].CellInfo.gsm.cellIdentityGsm.mnc = mnc;
            response[i].CellInfo.gsm.cellIdentityGsm.lac = lacTac;
            response[i].CellInfo.gsm.cellIdentityGsm.arfcn = arfcn;
            response[i].CellInfo.gsm.cellIdentityGsm.bsic = bsic;
            response[i].CellInfo.gsm.signalStrengthGsm.signalStrength = sig1;
            response[i].CellInfo.gsm.signalStrengthGsm.bitErrorRate = sig2;
            response[i].CellInfo.gsm.signalStrengthGsm.timingAdvance = timingAdvance;
        } else if (act == 256) {
            response[i].cellInfoType = RIL_CELL_INFO_TYPE_CDMA;
            response[i].CellInfo.cdma.cellIdentityCdma.networkId = nid;
            response[i].CellInfo.cdma.cellIdentityCdma.systemId = sid;
            response[i].CellInfo.cdma.cellIdentityCdma.basestationId = base_station_id;
            response[i].CellInfo.cdma.cellIdentityCdma.longitude = base_station_longitude;
            response[i].CellInfo.cdma.cellIdentityCdma.latitude = base_station_latitude;
            ViaBaseHandler *mViaHandler = RfxViaUtils::getViaHandler();
            if (mViaHandler != NULL && cdma_dbm != INVALID) {
                response[i].CellInfo.cdma.signalStrengthCdma.dbm =
                        mViaHandler->convertCdmaEvdoSig(cdma_dbm,
                                SIGNAL_CDMA_DBM);
            } else {
                response[i].CellInfo.cdma.signalStrengthCdma.dbm = INVALID;
            }
            if (mViaHandler != NULL && cdma_ecio != INVALID) {
                response[i].CellInfo.cdma.signalStrengthCdma.ecio =
                        mViaHandler->convertCdmaEvdoSig(cdma_ecio,
                                SIGNAL_CDMA_ECIO);
            } else {
                response[i].CellInfo.cdma.signalStrengthCdma.ecio = INVALID;
            }
            if (mViaHandler != NULL && evdo_dbm != INVALID) {
                response[i].CellInfo.cdma.signalStrengthEvdo.dbm =
                        mViaHandler->convertCdmaEvdoSig(evdo_dbm,
                                SIGNAL_EVDO_DBM);
            } else {
                response[i].CellInfo.cdma.signalStrengthEvdo.dbm = INVALID;

            }
            if (mViaHandler != NULL && evdo_ecio != INVALID) {
                response[i].CellInfo.cdma.signalStrengthEvdo.ecio =
                        mViaHandler->convertCdmaEvdoSig(evdo_ecio,
                                SIGNAL_EVDO_ECIO);
            } else {
                response[i].CellInfo.cdma.signalStrengthEvdo.ecio = INVALID;

            }
            response[i].CellInfo.cdma.signalStrengthEvdo.signalNoiseRatio = evdo_snr;
            logD(LOG_TAG, "RIL_CELL_INFO_TYPE_C2K act=%d, cdma_dbm=%d, "
                    "cdma_ecio=%d, evdo_dbm=%d, evdo_ecio=%d, evdo_snr=%d ",
                    act, response[i].CellInfo.cdma.signalStrengthCdma.dbm,
                    response[i].CellInfo.cdma.signalStrengthCdma.ecio,
                    response[i].CellInfo.cdma.signalStrengthEvdo.dbm,
                    response[i].CellInfo.cdma.signalStrengthEvdo.ecio,
                    response[i].CellInfo.cdma.signalStrengthEvdo.signalNoiseRatio);
        } else {
            logD(LOG_TAG, "RIL_CELL_INFO_TYPE invalid act=%d", act);
        }
    }
    return 0;
error:
    return -1;
}

int RmcNetworkHandler::convertToModUtf8Encode(int src) {
    int rlt = src;
    int byte1 = 0;
    int byte2 = 0;
    int byte3 = 0;

    if (src > 0x7FF) {
        byte1 = (src >> 12) | 0xE0;
        byte2 = ((src >> 6) & 0x3F) | 0x80;
        byte3 = (src & 0x3F) | 0x80;
        rlt = (byte1 << 16) | (byte2 << 8) | byte3;
    } else if (src > 0x7F) {
        byte1 = (src >> 6) | 0xC0;
        byte2 = (src & 0x3F) | 0x80;
        rlt = (byte1 << 8) | byte2;
    }
    return rlt;
}

void RmcNetworkHandler::convertToUtf8String(char *src) {
    int i;
    int idx;
    int len;
    int cvtCode;
    char temp_oper_name[MAX_OPER_NAME_LENGTH] = {0};

    idx = 0;
    len = strlen(src);
    memset(temp_oper_name, 0, sizeof(char) * MAX_OPER_NAME_LENGTH);
    for (i = 0; i < len; i++) {
        cvtCode = convertToModUtf8Encode(src[i]);
        // logD(LOG_TAG, "cvtUTF8 %x", cvtCode);
        if ((cvtCode & 0xFF0000) > 0) {
            if (idx >= MAX_OPER_NAME_LENGTH - 3) {
                break;
            }
            temp_oper_name[idx++] = (cvtCode & 0xFF0000) >> 16;
            temp_oper_name[idx++] = (cvtCode & 0xFF00) >> 8;
            temp_oper_name[idx++] = cvtCode & 0xFF;
        } else if ((cvtCode & 0xFF00) > 0) {
            if (idx >= MAX_OPER_NAME_LENGTH - 2) {
                break;
            }
            temp_oper_name[idx++] = (cvtCode & 0xFF00) >> 8;
            temp_oper_name[idx++] = cvtCode & 0xFF;
        } else {
            temp_oper_name[idx++] = cvtCode;
        }
        if (idx == MAX_OPER_NAME_LENGTH) {
            break;
        }
    }
    temp_oper_name[MAX_OPER_NAME_LENGTH - 1] = '\0';
    strncpy(src, temp_oper_name, MAX_OPER_NAME_LENGTH);
    // logD(LOG_TAG, "convertToUtf8String %s", src);
}

unsigned int RmcNetworkHandler::convertCSNetworkType(unsigned int uiResponse)
{
    unsigned int uiRet = 0;

    /* mapping */
    switch(uiResponse)
    {
        case 0x0001:     // GPRS
        case 0x0002:     // EDGE
            uiRet = 16;        // GSM
            break;
        case 0x0004:     // UMTS
        case 0x0008:     // HSDPA
        case 0x0010:     // HSUPA
        case 0x0018:     // HSDPA_UPA
        case 0x0020:     // HSDPAP
        case 0x0030:     // HSDPAP_UPA
        case 0x0040:     // HSUPAP
        case 0x0048:     // HSUPAP_DPA
        case 0x0060:     // HSPAP
        case 0x0088:     // DC_DPA
        case 0x0098:     // DC_DPA_UPA
        case 0x00a0:     // DC_HSDPAP
        case 0x00b0:     // DC_HSDPAP_UPA
        case 0x00c8:     // DC_HSUPAP_DPA
        case 0x00e0:     // DC_HSPAP
            uiRet = 3;        // UMTS
            break;
        // for C2K
        case 0x0100:     // 1xRTT
            uiRet = 6;        // 1xRTT
            break;
        case 0x0200:     // HRPD
            uiRet = 8;        // EVDO_A
            break;
        case 0x0400:     // EHRPD
            uiRet = 13;         // EHRPD
            break;
        //for LTE
        case 0x1000:     // LTE
        case 0x2000:     // LTE_CA
            uiRet = 14;       // LTE
            break;
        default:
            uiRet = 0;        // UNKNOWN
            break;
    }

    return uiRet;
}

unsigned int RmcNetworkHandler::convertPSNetworkType(unsigned int uiResponse)
{
    unsigned int uiRet = 0;

    /* mapping */
    switch(uiResponse)
    {
        case 0x0001:     // GPRS
            uiRet = 1;        // GPRS
            break;
        case 0x0002:     // EDGE
            uiRet = 2;        // EDGE
            break;
        case 0x0004:     // UMTS
            uiRet = 3;        // UMTS
            break;
        case 0x0008:     // HSDPA
            uiRet = 9;        // HSDPA
            break;
        case 0x0010:     // HSUPA
            uiRet = 10;        // HSUPA
            break;
        case 0x0018:     // HSDPA_UPA
            uiRet = 11;       // HSPA
            break;
        case 0x0020:     // HSDPAP
        case 0x0030:     // HSDPAP_UPA
        case 0x0040:     // HSUPAP
        case 0x0048:     // HSUPAP_DPA
        case 0x0060:     // HSPAP
        case 0x0088:     // DC_DPA
        case 0x0098:     // DC_DPA_UPA
        case 0x00a0:     // DC_HSDPAP
        case 0x00b0:     // DC_HSDPAP_UPA
        case 0x00c8:     // DC_HSUPAP_DPA
        case 0x00e0:     // DC_HSPAP
            uiRet = 15;       // HSPAP
            break;
        // for C2K
        case 0x0100:     // 1xRTT
            uiRet = 6;        // 1xRTT
            break;
        case 0x0200:     // HRPD
            uiRet = 8;        // EVDO_A
            break;
        case 0x0400:     // EHRPD
            uiRet = 13;         // EHRPD
            break;
        //for LTE
        case 0x1000:     // LTE
            uiRet = 14;       // LTE
            break;
        case 0x2000:     // LTE_CA
            uiRet = 19;       // LTE
            break;
        default:
            uiRet = 0;        // UNKNOWN
            break;
    }

    return uiRet;
}

void RmcNetworkHandler::updateWfcState(int status) {
    RFX_LOG_V(LOG_TAG, "%s, WFC state of slot%d:%d->%d", __FUNCTION__,
            m_slot_id, ril_wfc_reg_status[m_slot_id], status);
    pthread_mutex_lock(&s_wfcRegStatusMutex[m_slot_id]);
    ril_wfc_reg_status[m_slot_id] = status;  // cache wfc status
    getMclStatusManager()->setIntValue(RFX_STATUS_KEY_WFC_STATE,
            ril_wfc_reg_status[m_slot_id]);
    pthread_mutex_unlock(&s_wfcRegStatusMutex[m_slot_id]);
}

