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

#ifndef __RMC_NETWORK_HANDLER_H__
#define __RMC_NETWORK_HANDLER_H__

#undef NDEBUG
#ifdef LOG_NDEBUG
#undef LOG_NDEBUG
#endif

#include "RfxBaseHandler.h"
#include "RfxStringData.h"
#include "RfxStringsData.h"
#include "RfxVoidData.h"
#include "RfxIntsData.h"
#include "RfxMessageId.h"
#include <telephony/mtk_ril.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "RmcNwHdlr"

#define MAX_OPER_NAME_LENGTH 50

typedef enum {
    CACHE_GROUP_GSM = 0,
    CACHE_GROUP_C2K = 1,
    CACHE_GROUP_1XRTT = 2,
    CACHE_GROUP_EVDO = 3,
    CACHE_GROUP_COMMON_REQ = 4,
    CACHE_GROUP_ALL
} RIL_CACHE_GROUP;

typedef struct {
    int gw_signal_strength;
    int gw_bit_error_rate;
    int cdma_dbm;
    int cdma_ecio;
    int evdo_dbm;
    int evdo_ecio;
    int evdo_snr;
    int lte_signal_strength;
    int lte_rsrp;
    int lte_rsrq;
    int lte_rssnr;
    int lte_cqi;
    int lte_timing_advance;
    int td_scdma_rscp;
} RIL_SIGNAL_STRENGTH_CACHE;

typedef struct {
    int sig1;
    int sig2;
    int rssi_in_qdbm;
    int rscp_in_qdbm;
    int ecn0_in_qdbm;
    int rsrq_in_qdbm;
    int rsrp_in_qdbm;
    int act;
    int sig3;
    int serv_band;
} MD_SIGNAL_STRENGTH;

typedef struct {
    int registration_state;
    unsigned int lac;
    unsigned int cid;
    int radio_technology;
    int base_station_id;
    int base_station_latitude;
    int base_station_longitude;
    int css;
    int system_id;
    int network_id;
    int roaming_indicator;
    int is_in_prl;
    int default_roaming_indicator;
    int denied_reason;
    int psc;
    int network_exist;
} RIL_VOICE_REG_STATE_CACHE;

typedef struct {
    int domain;
    int state;
    int plmn_id;
    int act;
    int is_femtocell;
    int is_csg_cell;
    int csg_id;
    int csg_icon_type;
    char hnbName[MAX_OPER_NAME_LENGTH];
    int cause;
} RIL_FEMTO_CELL_CACHE;

/* EONS status reported from modem */
typedef enum
{
    EONS_INFO_NOT_RECEIVED,
    EONS_INFO_RECEIVED_DISABLED,
    EONS_INFO_RECEIVED_ENABLED
} RIL_EonsStatusInfo;

// Defines EONS network feature support info.
typedef struct {
    RIL_EonsStatusInfo eons_status;
    unsigned int lac;
} RIL_EonsNetworkFeatureInfo;

static pthread_mutex_t ril_nw_nitzName_mutex;

#define PROPERTY_NITZ_OPER_CODE     "persist.radio.nitz_oper_code"
#define PROPERTY_NITZ_OPER_LNAME    "persist.radio.nitz_oper_lname"
#define PROPERTY_NITZ_OPER_SNAME    "persist.radio.nitz_oper_sname"

class RmcNetworkHandler : public RfxBaseHandler {
    public:
        RmcNetworkHandler(int slot_id, int channel_id);
        virtual ~RmcNetworkHandler() {}

        /**
         * Convert registration state
         * @param uiRegState registration state from modem
         * @param isVoiceState whether is voice state
         * @return registration state to framework
         */
        unsigned int convertRegState(unsigned int uiRegState, bool isVoiceState);
        int getSignalStrength(RfxAtLine *line);
        void resetSignalStrengthCache(RIL_SIGNAL_STRENGTH_CACHE *sigCache, RIL_CACHE_GROUP source);
        bool isTdd3G();
        int isFemtocellSupport();
        void updateSignalStrengthProperty();
        static String8 getCurrentLteSignal(int slotId);
        int isOp12Plmn(const char* plmn);
        int getCellInfoListV12(RfxAtLine* line, int num, RIL_CellInfo_v12 * response);
        int convert3GRssiValue(int rscp_in_dbm);
        int convertToModUtf8Encode(int src);
        void convertToUtf8String(char *src);
        unsigned int convertCSNetworkType(unsigned int uiResponse);
        unsigned int convertPSNetworkType(unsigned int uiResponse);
        void updateWfcState(int status);

        char const *sourceToString(int srcId);

        static RIL_EonsNetworkFeatureInfo eons_info[MAX_SIM_COUNT];
        /* modem ECELL ext3 ext4 support
         * value: 0 not surrpot
         *        1 support
         **/
        static int ECELLext3ext4Support;
        static RIL_FEMTO_CELL_CACHE m_femto_cell_cache;

        static pthread_mutex_t ril_nw_femtoCell_mutex;

        static pthread_mutex_t s_signalStrengthMutex[MAX_SIM_COUNT];
        static RIL_SIGNAL_STRENGTH_CACHE signal_strength_cache[MAX_SIM_COUNT];
        static pthread_mutex_t s_voiceRegStateMutex[MAX_SIM_COUNT];
        static RIL_VOICE_REG_STATE_CACHE voice_reg_state_cache[MAX_SIM_COUNT];

        /* WFC registration status */
        static int ril_wfc_reg_status[MAX_SIM_COUNT];
        static pthread_mutex_t s_wfcRegStatusMutex[MAX_SIM_COUNT];

        static int mPlmnListOngoing;
        static int mPlmnListAbort;

    protected:
        char m_ril_nw_nitz_oper_code[MAX_OPER_NAME_LENGTH];
        char m_ril_nw_nitz_oper_lname[MAX_OPER_NAME_LENGTH];
        char m_ril_nw_nitz_oper_sname[MAX_OPER_NAME_LENGTH];

    private:
        static String8 mCurrentLteSignal[MAX_SIM_COUNT];

};

#endif
