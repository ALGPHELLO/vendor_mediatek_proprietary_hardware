/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 */
/* MediaTek Inc. (C) 2010. All rights reserved.
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

#ifndef RIL_SIM_H
#define RIL_SIM_H 1

#include <stdbool.h>
// MTK-START: BT SIM Access Profile
#include <hardware/ril/librilutils/proto/sap-api.pb.h>
//#include <vendor/mediatek/proprietary/hardware/ril/rilproxy/librilutils/proto/sap-api.pb.h>
// MTK-END

#define AID_PREFIX_LEN 14
#define PROPERTY_MCC_MNC "gsm.ril.uicc.mccmnc"

// MTK-START: AOSP SIM PLUG IN/OUT
#define PROPERTY_ESIMS_CAUSE "gsm.ril.uicc.esims.cause"
#define ESIMS_CAUSE_SIM_NO_INIT 26
// MTK-END

static void *noopRemoveSimWarning( void *a ) { return a; }
#define RIL_SIM_UNUSED_PARM(a) noopRemoveSimWarning((void *)&(a));

/* -1: invalid, 0: non test sim, 1:  test sim */
static int isTestSim[4] = {-1,-1,-1,-1};

static const char PROPERTY_RIL_TEST_SIM[4][25] = {
    "gsm.sim.ril.testsim",
    "gsm.sim.ril.testsim.2",
    "gsm.sim.ril.testsim.3",
    "gsm.sim.ril.testsim.4",
};

static const char PROPERTY_RIL_SIM_PIN1[4][25] = {
    "gsm.sim.retry.pin1",
    "gsm.sim.retry.pin1.2",
    "gsm.sim.retry.pin1.3",
    "gsm.sim.retry.pin1.4",
};

static const char PROPERTY_RIL_SIM_PUK1[4][25] = {
    "gsm.sim.retry.puk1",
    "gsm.sim.retry.puk1.2",
    "gsm.sim.retry.puk1.3",
    "gsm.sim.retry.puk1.4",
};

static const char PROPERTY_RIL_SIM_PIN2[4][25] = {
    "gsm.sim.retry.pin2",
    "gsm.sim.retry.pin2.2",
    "gsm.sim.retry.pin2.3",
    "gsm.sim.retry.pin2.4",
};

static const char PROPERTY_RIL_SIM_PUK2[4][25] = {
    "gsm.sim.retry.puk2",
    "gsm.sim.retry.puk2.2",
    "gsm.sim.retry.puk2.3",
    "gsm.sim.retry.puk2.4",
};

static const char PROPERTY_RIL_SIM_OPERATOR_DEFAULT_NAME_LIST[4][35] = {
    "gsm.sim.operator.default-name",
    "gsm.sim.operator.default-name.2",
    "gsm.sim.operator.default-name.3",
    "gsm.sim.operator.default-name.4",
};

static const char PROPERTY_RIL_UICC_TYPE[4][25] = {
    "gsm.ril.uicctype",
    "gsm.ril.uicctype.2",
    "gsm.ril.uicctype.3",
    "gsm.ril.uicctype.4",
};

static const char PROPERTY_RIL_FULL_UICC_TYPE[4][25] = {
    "gsm.ril.fulluicctype",
    "gsm.ril.fulluicctype.2",
    "gsm.ril.fulluicctype.3",
    "gsm.ril.fulluicctype.4",
};

static const char PROPERTY_RIL_CT3G[4][25] = {
    "gsm.ril.ct3g",
    "gsm.ril.ct3g.2",
    "gsm.ril.ct3g.3",
    "gsm.ril.ct3g.4",
};

static const char PROPERTY_RIL_CT3G_ROAMING[4][25] = {
    "gsm.ril.ct3groaming",
    "gsm.ril.ct3groaming.2",
    "gsm.ril.ct3groaming.3",
    "gsm.ril.ct3groaming.4",
};

static const char PROPERTY_RIL_CT3G_ROAMING2[4][25] = {
    "gsm.ril.ct3groaming2",
    "gsm.ril.ct3groaming2.2",
    "gsm.ril.ct3groaming2.3",
    "gsm.ril.ct3groaming2.4",
};

static const char PROPERTY_ECC_LIST[4][25] = {
    "ril.ecclist",
    "ril.ecclist1",
    "ril.ecclist2",
    "ril.ecclist3",
};

static const char PROPERTY_RIL_CARD_TYPE_SET[25] = "gsm.ril.cardtypeset";
static const char PROPERTY_RIL_CARD_TYPE_SET_2[25] = "gsm.ril.cardtypeset.2";

typedef enum {
    SIM_ABSENT = 0,
    SIM_NOT_READY = 1,
    SIM_READY = 2, /* SIM_READY means the radio state is RADIO_STATE_SIM_READY */
    SIM_PIN = 3,
    SIM_PUK = 4,
    SIM_NETWORK_PERSONALIZATION = 5,
    /* Add for USIM support */
    USIM_READY = 6,
    USIM_PIN = 7,
    USIM_PUK = 8,
    SIM_BUSY = 9,
    SIM_NP = 10,
    SIM_NSP = 11,
    SIM_SP = 12,
    SIM_CP = 13,
    SIM_SIMP =14,
    SIM_PERM_BLOCKED = 15, // PERM_DISABLED
    USIM_PERM_BLOCKED = 16, // PERM_DISABLED
    RUIM_ABSENT = 17,
    RUIM_NOT_READY = 18,
    RUIM_READY = 19,
    RUIM_PIN = 20,
    RUIM_PUK = 21,
    RUIM_NETWORK_PERSONALIZATION = 22,
    USIM_NP = 23,
    USIM_NSP = 24,
    USIM_SP = 25,
    USIM_CP = 26,
    USIM_SIMP =27,
    USIM_NOT_READY =28,
    // MTK-START: AOSP SIM PLUG IN/OUT
    UICC_NO_INIT = 29
    // MTK-END
} SIM_Status;

// MTK-START: ISIM
typedef enum {
    ISIM_ABSENT = 0,
    ISIM_NOT_READY = 1,
    ISIM_READY = 2, /* SIM_READY means the radio state is RADIO_STATE_SIM_READY */
    ISIM_PIN = 3,
    ISIM_PUK = 4,
    ISIM_BUSY = 5,
    ISIM_PERM_BLOCKED = 6, // PERM_DISABLED
} ISIM_Status;
// MTK-END

typedef enum {
    UICC_APP_ISIM = 0,
    UICC_APP_USIM = 1,
    UICC_APP_CSIM = 2,
    UICC_APP_SIM = 3,
    UICC_APP_RUIM = 4,

    UICC_APP_ID_END
} App_Id;

typedef enum {
    ENTER_PIN1,
    ENTER_PIN2,
    ENTER_PUK1,
    ENTER_PUK2,
    CHANGE_PIN1,
    CHANGE_PIN2
} SIM_Operation;

typedef enum {
    PINUNKNOWN,
    PINCODE1,
    PINCODE2,
    PUKCODE1,
    PUKCODE2
}SimPinCodeE;

typedef enum {
    AID_USIM,
    //AID_SIM,
    AID_ISIM,

    AID_MAX
} AidIndex;


typedef struct {
    int pin1;
    int pin2;
    int puk1;
    int puk2;
} SimPinCount;

typedef struct{
    RIL_SimMeLockCatInfo catagory[7];
    char imsi[16];
    int isgid1;
    char gid1[16];
    int isgid2;
    char gid2[16];
    int mnclength;
}SimLockInfo;

typedef struct {
    int rid;
    char* urc;
} TimedCallbackParam;


//ISIM
#define MAX_AID_LEN 33
#define MAX_AID_LABEL_LEN 256

typedef struct {
    int appId;
    int session;
}SessionInfo;

typedef struct {
    int aid_len;
    int app_label_len;
    char aid[MAX_AID_LEN];
    char appLabel[MAX_AID_LABEL_LEN];
}AidInfo;


extern void pollSIMState(void * param);
extern void getPINretryCount(SimPinCount *result, RIL_Token t, RIL_SOCKET_ID rid);

extern int rilSimMain(int request, void *data, size_t datalen, RIL_Token t);
extern int rilSimUnsolicited(const char *s, const char *sms_pdu, RILChannelCtx* p_channel);
extern int requrstSimChannelAccess(RILChannelCtx *p_channel, int sessionid, char * senddata, RIL_SIM_IO_Response* output); // NFC SEEK

extern void resetSIMProperties(RIL_SOCKET_ID rid);

/* MTK proprietary start */
extern int sim_inserted_status;
extern int isSimInserted(RIL_SOCKET_ID rid);

extern void requestIccId(void *data, size_t datalen, RIL_Token t);

extern void resetAidInfo(RIL_SOCKET_ID rid);
extern AidInfo* getAidInfo(RIL_SOCKET_ID rid, AidIndex index);
extern void queryEfDir(RIL_SOCKET_ID rid);
extern void setSimInsertedStatus(RIL_SOCKET_ID rid, int isInserted);
// MTK-START: ISIM
extern int queryIccApplicationChannel(int appId, RIL_Token t);
extern void requestOpenIccApplication(void *data, size_t datalen, RIL_Token t);
extern void requestGetIccApplicationStatus(void *data, size_t datalen, RIL_Token t);
extern void onSessionIdChanged(const char *s, RIL_SOCKET_ID rid);
extern SessionInfo* getIsimSessionInfo(RIL_SOCKET_ID rid);
extern int turnOnIsimApplication(RIL_Token t, RIL_SOCKET_ID rid);
extern int queryAppType(char* pAid);
extern int getIccApplicationStatus(RIL_CardStatus_v6 **pp_card_status, RIL_SOCKET_ID rid,
        int sessionId);
// MTK-END
// NFC SEEK start
extern void requestSIM_OpenChannel(void *data, size_t datalen, RIL_Token t);
extern void requestSIM_CloseChannel(void *data, size_t datalen, RIL_Token t);
extern void requestSIM_TransmitBasic(void *data, size_t datalen, RIL_Token t);
extern void requestSIM_OpenChannelWithSw(void *data, size_t datalen, RIL_Token t);
extern void requestSIM_TransmitChannel(void *data, size_t datalen, RIL_Token t);
extern void requestSIM_GetATR(void *data, size_t datalen, RIL_Token t);
// NFC SEEK end
extern void requestSimInterfaceSwitch(void *data, size_t datalen, RIL_Token t);
extern void onUsimDetected(const char *s, RIL_SOCKET_ID rid);
extern void onTestSimDetected(const char *s, RIL_SOCKET_ID rid);
extern void onCt3gDetected(const char *s, RIL_SOCKET_ID rid);
extern void detectTestSim(RIL_SOCKET_ID rid);
extern bool RIL_isTestSim(RIL_SOCKET_ID rid);

extern void requestSimReset(RIL_SOCKET_ID rid);
extern void resetSimForCt3g(RIL_SOCKET_ID rid);
extern void requestSimInsertStatus(RIL_SOCKET_ID rid);

extern void requestSetSimCardPower(void *data, size_t datalen, RIL_Token t);

// SIM power [Start]
extern void requestSetSimPower(void *data, size_t datalen, RIL_Token t);
// SIM power [End]

extern void requestSwitchCardType(void *data, size_t datalen, RIL_Token t);
/* MTK proprietary end */
// MTK-START: SIM GBA
extern void requestGeneralSimAuth(void *data, size_t datalen, RIL_Token t);
// MTK-END
// MTK-START: SIM ME LOCK
extern void queryNetworkLock(void *data, size_t datalen, RIL_Token t);
extern void simNetworkLock(void *data, size_t datalen, RIL_Token t);
// MTK-END

// MTK-START: BT SIM Access Profile
extern int rilBtSapMain(int request, void *data, size_t datalen, RIL_Token t,
        RIL_SOCKET_ID rid);
extern bool isBtSapConnectionSetup(RIL_SOCKET_ID rid);
extern void notifyBtSapStatusInd(RIL_SIM_SAP_STATUS_IND_Status message,
        RIL_SOCKET_ID socket_id);
extern void resetBtSapContext(RIL_SOCKET_ID rid);

extern void requestSwitchCardType(void *data, size_t datalen, RIL_Token t);

extern void releaseExtraWakeLock();

typedef enum
{
   BT_SAP_INIT,
   BT_SAP_CONNECTION_SETUP,
   BT_SAP_ONGOING_CONNECTION,
   BT_SAP_DISCONNECT,
   BT_SAP_POWER_ON,
   BT_SAP_POWER_OFF,
} BtSapStatus;

typedef struct _LocalBtSapMsgHeader {
    RIL_SOCKET_ID socket_id;
    RIL_Token t;
    void *param;
} LocalBtSapMsgHeader;

extern BtSapStatus queryBtSapStatus(RIL_SOCKET_ID rid);

// MTK-END
#endif /* RIL_SIM_H */
