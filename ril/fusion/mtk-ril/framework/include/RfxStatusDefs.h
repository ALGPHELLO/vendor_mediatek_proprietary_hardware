/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 *
 * MediaTek Inc. (C) 2010. All rights reserved.
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
/*
 * File name:  RfxStatusDef.h
 * Author: Jun Liu (MTK80064)
 * Description:
 *  Define the keys of shared status.
 */

#ifndef __RFX_STATUS_DEFS_H__
#define __RFX_STATUS_DEFS_H__

/*****************************************************************************
 * Enum
 *****************************************************************************/

enum RfxStatusKeyEnum {
    RFX_STATUS_KEY_START,
    /*Please add your keys below this line*/

    #define RFX_WAIT_FOR_ECPIN 1
    #define RFX_ECPIN_DONE 0


    #define RFX_UICC_APPLIST_UNKNOWN  -1
    #define RFX_UICC_APPLIST_NONE     0x00
    #define RFX_UICC_APPLIST_USIM     0x02
    #define RFX_UICC_APPLIST_CSIM     0x04

    /* CDMA card type */
    #define UIM_CARD  1
    #define SIM_CARD  2
    #define UIM_SIM_CARD  3
    #define UNKOWN_CARD  4
    #define CT_3G_UIM_CARD  5
    #define CT_UIM_SIM_CARD  6
    #define NEED_TO_INPUT_PIN  7
    #define CT_4G_UICC_CARD  8
    #define NOT_CT_UICC_CARD  9
    #define CT_EXCEL_GG_CARD  10
    #define LOCKED_CARD  18
    #define IVSR_LOST  19
    #define CARD_NOT_INSERTED  255

    #define RFX_CDMA_CARD_READY_DEFAULT  0x00
    #define RFX_CDMA_CARD_EUSIM_READY    0x01
    #define RFX_CDMA_CARD_ECT3G_READY    0x02
    #define RFX_CDMA_CARD_MCCMNC_READY   0x04
    #define RFX_CDMA_CARD_LOCKCARD_READY 0x08

    /**
     * The card type of SIM card.
     * value type : int
     * RFX_CARD_TYPE_SIM  0x01
     * RFX_CARD_TYPE_USIM 0x02
     * RFX_CARD_TYPE_CSIM 0x04
     * RFX_CARD_TYPE_RUIM 0x08
     * RFX_CARD_TYPE_ISIM 0x10
     */
    RFX_STATUS_KEY_CARD_TYPE,

    /**
     * The card type of CDMA card.
     * value type : int
     * #define UIM_CARD  1  // CDMA only card but not CT card
     * #define SIM_CARD  2  // GSM card
     * #define UIM_SIM_CARD  3  // CDMA dual mode card but not CT card
     * #define UNKOWN_CARD  4  //unkonw card
     * #define CT_3G_UIM_CARD  5  // CT sigle mode card
     * #define CT_UIM_SIM_CARD  6  // CT dual mode card
     * #define NEED_TO_INPUT_PIN  7  // pin locked card
     * #define CT_4G_UICC_CARD  8  // CT 4G dual mode card
     * #define NOT_CT_UICC_CARD  9  // 4G dual mode card but not CT card
     * #define CT_EXCEL_GG_CARD  10 // CT excel GG card
     * #define LOCKED_CARD  18  // card is locked by modem
     * #define IVSR_LOST  19  // invalid sim recovery
     * #define CARD_NOT_INSERTED  255  // no card inserted
     */
    RFX_STATUS_KEY_CDMA_CARD_TYPE,

    /**
     * Use below flag to generate CDMA card type.
     * value type : int
     * RFX_CDMA_CARD_READY_DEFAULT  0x00
     * RFX_CDMA_CARD_EUSIM_READY  0x01
     * RFX_CDMA_CARD_ECT3G_READY 0x02
     * RFX_CDMA_CARD_MCCMNC_READY 0x04
     * RFX_CDMA_CARD_LOCKCARD_READY 0x08
     */
    RFX_STATUS_KEY_CDMA_CARD_READY,

    /**
     * Ready to read CDMA card file after C2K modem sends ciev 107.
     * value type : bool
     * false : CDMA card file is not ready to read. It is default value.
     * true : CDMA card file is ready to read.
     */
    RFX_STATUS_KEY_CDMA_FILE_READY,

    /**
     * CDMA 3g dualmode card flag.
     * value type : bool
     * false : it is not a CDMA 3g dualmode card. It is default value.
     * true : it is a CDMA 3g dualmode card.
     */
    RFX_STATUS_KEY_CDMA3G_DUALMODE_CARD,

    /**
     * Report uicc application list in OP09 A project for slot2.
     * value type : int
     * uicc_app_list = is_csim_exist | is_usim_exist | is_isim_exist (currently isim always 0)
     * is_usim_exist:2 is_csim_exist:4 (is_csim_exist | is_usim_exist): 6
     * For icc card uicc_app_list:0
     */
    RFX_STATUS_KEY_ESIMIND_APPLIST,

    /**
     * It shows if the card is locked in OP09 A project.
     * value type : bool
     * false : Card is not locked.
     * true : Card is locked.
     */
    RFX_STATUS_KEY_CDMA_LOCKED_CARD,

    /**
     * The uicc subscription changed status
     * value type : int
     * Init: -1
     * Deactivate: 0
     * Activate: 1
     */
    RFX_STATUS_KEY_UICC_SUB_CHANGED_STATUS,

    /**
     * The IMSI of CDMA application.
     * value type : String8
     * "" or IMSI
     */
    RFX_STATUS_KEY_C2K_IMSI,

    /**
      * Notify GSM MCC+MNC
      */
    RFX_STATUS_KEY_UICC_GSM_NUMERIC,

    /**
      * Notify CDMA MCC+MNC
      */
    RFX_STATUS_KEY_UICC_CDMA_NUMERIC,

    /**
     * The current BTSAP status
     * Refer to BtSapStatus
     */
    RFX_STATUS_KEY_BTSAP_STATUS,

    /**
     * The current protocol
     */
    RFX_STATUS_KEY_BTSAP_CURRENT_PROTOCOL,

    /**
     * The support protocol
     */
    RFX_STATUS_KEY_BTSAP_SUPPORT_PROTOCOL,

    /**
     * Save ATR for BTSAP
     */
    RFX_STATUS_KEY_BTSAP_ATR,

    /**
     * The SIM ESIMS state. It will be updated upon receiving +ESIMS.
     * value type : int
     * 0: SIM Missing
     * 9: Virtual SIM on
     * 10: Virtual SIM off
     * 11: SIM plug out
     * 12: SIM plug in
     * 13: Recovery start
     * 14: Recovery end
     * 15: IMEI Lock
     */
    RFX_STATUS_KEY_SIM_ESIMS_CAUSE,

    /**
     * The flag is used for the indication of ECPIN
     */
    RFX_STATUS_KEY_ECPIN_STATE,

    /**
     * This is used to check the modem SIM task is ready or not.
     * NOTE: It is not SIM_STATE_CHANGED READY!!
     * value type : bool
     * false: modem SIM task is not ready. It is also default value
     * true: modem SIM task is ready.
     */
    RFX_STATUS_KEY_MODEM_SIM_TASK_READY,

    RFX_STATUS_KEY_SERVICE_STATE,
    RFX_STATUS_KEY_VOICE_TYPE,
    RFX_STATUS_KEY_DATA_TYPE,
    RFX_STATUS_KEY_RADIO_STATE,
    RFX_STATUS_KEY_REQUEST_RADIO_POWER,
    RFX_STATUS_KEY_MODEM_POWER_OFF,
  /**
   * Modem off state.
   * NOTE: Belong to non slot controller, use getNonSlotScopeStatusManager().
   * value type : int
   * MODEM_OFF_IN_IDLE
   * MODEM_OFF_BY_MODE_SWITCH
   * MODEM_OFF_BY_POWER_OFF
   * MODEM_OFF_BY_SIM_SWITCH
   */
    RFX_STATUS_KEY_MODEM_OFF_STATE,

    /**
     * The SIM state. It will be updated upon receiving the response of GET_SIM_STATUS.
     * And be set as NOT_READY when RADIO_UNAVAILABLE.
     * value type : int
     * RFX_SIM_STATE_NOT_READY  0
     * RFX_SIM_STATE_READY 1
     * RFX_SIM_STATE_LOCKED 2
     * RFX_SIM_STATE_ABSENT 3
     */
    RFX_STATUS_KEY_SIM_STATE,
    /**
     * The SIM inserted state. It will be updated upon receiving the response of GET_SIM_STATUS.
     * And be set as NOT_READY when RADIO_UNAVAILABLE.
     * value type : int
     */
    RFX_STATUS_KEY_SIM_INSERT_STATE,

    /**
     * The IMSI of GSM application.
     * value type : String8
     * "" or IMSI
     */
    RFX_STATUS_KEY_GSM_IMSI,

    /**
     * Indicate the voice call count
     */
    RFX_STATUS_KEY_VOICE_CALL_COUNT,

    /**
      * Indicate the AP voice call count
      */
    RFX_STATUS_KEY_AP_VOICE_CALL_COUNT,

    /**
     * The raido acess family for each slot
     * value type : int
     * Return the networktype like RAF_LTE+RAF_UMTS+RAF_GSM
     */
    RFX_STATUS_KEY_SLOT_CAPABILITY,

    /**
     * Slot data connection status is changed.
     * status will be notified only if status is changed.
     * value type : int
     * DISCONNECTED: no active data connection exist.
     * CONNECTED: at least one data connection exist.
     */
    RFX_STATUS_KEY_DATA_CONNECTION,
    /**
     * The world mode switching state
     * value type : int
     * Switching:   0
     * Switch done: 1
     * Switch done but modem failure: -1
     */
    RFX_STATUS_KEY_WORLD_MODE_STATE,

    /**
     * The GSM world mode switching state
     * value type : int
     * Switching:   0
     * Switch done: 1
     */
    RFX_STATUS_KEY_GSM_WORLD_MODE_STATE,

    /**
     * The C2k world mode switching state
     * value type : int
     * Switching:   0
     * Switch done: 1
     */
    RFX_STATUS_KEY_CDMA_WORLD_MODE_STATE,

    /**
     * The world mode block state
     * value type : int
     * blocked:   1
     * not blocked: 0
     */
    RFX_STATUS_KEY_WORLD_MODE_BLOCKED_STATE,

    /**
     * The world mode block switching state
     * value type : int
     * block Switching:   1
     * not block switching: 1
     */
    RFX_STATUS_KEY_WORLD_MODE_BLOCKED_CHANGING_STATE,

    /**
     * The main capability slot id
     * value type : int
     * Return the main capability slot Id
     */
    RFX_STATUS_KEY_MAIN_CAPABILITY_SLOT,

    /**
     * The CDMA OTA provsison state
     * value type : int
     * programming started                       1
     * service programming lock unlocked         2
     * NAM parameters downloaded successfully    3
     * MDN downloaded successfully               4
     * IMSI downloaded successfully              5
     * PRL downloaded successfully               6
     * commit successfully                       7
     * programming successfully                  8
     * programming unsuccessfully                9
     * verify SPC failed                         10
     * a key exchanged                           11
     * SSD updated                               12
     * OTAPA started                             13
     * OTAPA stopped                             14
     */
    RFX_STATUS_KEY_OTA_STATUS,

    /**
      * Notify the current call state
      */
    RFX_STATUS_KEY_CALL_STATE,


    /**
     * Indicate whether the UE is in emergency mdoe
     * value type: boolean
     *  true:  In emergency mode
     *  false: Not in emergency mode
     */
    RFX_STATUS_KEY_EMERGENCY_MODE,

    /**
     * Emergency callback mode
     * value type: integer
     *  0: Not in emergency callback mode
     *  1: in emergency callback mode
     */
    RFX_STATUS_KEY_EMERGENCY_CALLBACK_MODE,


    RFX_STATUS_KEY_ATCI_IS_NUMERIC,
    /**
     * Indicate the cellular network PS state
     */
    RFX_STATUS_KEY_CELLULAR_PS_STATE,
    /**
     * Indicate the WFC state
     */
    RFX_STATUS_KEY_WFC_STATE,

    /**
     * IMS call status
     * value type: boolean
     *  true: IMS call ongoing
     *  false: No IMS call
     */
    RFX_STATUS_KEY_IMS_CALL_EXIST,

    /*
     * CDMA_SMS_INBOUND_NONE(0)
     * CDMA_SMS_INBOUND_IMS(1)
     * CDMA_SMS_INBOUND_CS(2)
     * CDMA_SMS_INBOUND_COMM(3)
     * CDMA_SMS_INBOUND_VMI(4)
     */
    RFX_STATUS_KEY_CDMA_INBOUND_SMS_TYPE,
    RFX_STATUS_KEY_CDMA_SMS_REPLY_SEQ_NO,
    RFX_STATUS_KEY_CDMA_SMS_ADDR,
    RFX_STATUS_KEY_CDMA_PENDING_VMI,

    /*
     * CDMA_MO_SMS_SENDING(0)
     * CDMA_MO_SMS_SENDED(1)
     */
    RFX_STATUS_KEY_CDMA_MO_SMS_STATE,

    RFX_STATUS_KEY_REPLACE_VT_CALL,

    /**
    * Store default data SIM.
    * -1: Unset
    * 0:  Slot 0
    * 1:  Slot 1
    */
    RFX_STATUS_KEY_DEFAULT_DATA_SIM,

    /**
     * ECC preferred RAT
     * value type: integer
     *  0: unknown
     *  1: gsm
     *  2: cdma
     */
    RFX_STATUS_KEY_ECC_PREFERRED_RAT,

    /**
     * Capability switch internal use
     * Indicate the fixed capability for a specific slot
     * value type: int
     */
    RFX_STATUS_KEY_SLOT_FIXED_CAPABILITY,

    /**
     * Store radio power of each protocol
     */
    RFX_STATUS_KEY_RADIO_POWER_MSIM_MODE,

    /**
     * Mutex lock for radio power and world mode.
     * 0: idle
     * 1: lock by radio power
     * 2: lock by world mode
     */
    RFX_STATUS_KEY_RADIO_LOCK,

    RFX_STATUS_KEY_GCF_TEST_MODE,

    /*
      * SMS_INBOUND_NONE (0)
      * SMS_INBOUND_IMS_3GPP (1)
      * SMS_INBOUND_CS_3GPP (2)
      */
    RFX_STATUS_KEY_GSM_INBOUND_SMS_TYPE,

    /**
      * SMS_PHONE_STORAGE_AVAILABLE (0)
      * SMS_PHONE_STORAGE_FULL (1)
      */
    RFX_STATUS_KEY_SMS_PHONE_STORAGE,

    /**
     * rat controller preferred network type.
     * value type : int
     */
    RFX_STATUS_KEY_PREFERRED_NW_TYPE,

    RFX_STATUS_KEY_IS_RAT_MODE_SWITCHING,

    /**
     * 5 or 6 digit operator numeric code (MCC + MNC)
     * value type : String8
     * "" or MCC+MNC
     */
    RFX_STATUS_KEY_OPERATOR,

    RFX_STATUS_KEY_MODESWITCH_FINISHED,

    /**
     * SIM can set the key to switch cdma 3G card. It shows who will trigger to switch: AP or GMSS
     * and switching to which card type: SIM or RUIM.
     * value type: int
     *  -1: default
     *  1: AP_TRIGGER_SWITCH_SIM
     *  2: GMSS_TRIGGER_SWITCH_SIM
     *  3: AP_TRIGGER_SWITCH_RUIM
     *  4: GMSS_TRIGGER_SWITCH_RUIM
     */
    RFX_STATUS_KEY_CDMA3G_SWITCH_CARD,

    /**
     * Indicate the capability switch states
     * value type: CapabilitySwitchState enum
     */
    RFX_STATUS_KEY_CAPABILITY_SWITCH_STATE,

    /**
     * Modules can set this status key to do some handling before SIM switch,
     * please don't do long time consuming operation for it will cause performance and timeout
     * issues for SIM switch.
     * value type: int
     */
    RFX_STATUS_KEY_CAPABILITY_SWITCH_WAIT_MODULE,

    /**
     * SIM switch will set current time stamp to this status key when set radio unavailable,
     * modules can use it to know when the latest radio unavailable was set by SIM switch.
     * value type: int64_t
     */
    RFX_STATUS_KEY_SIM_SWITCH_RADIO_UNAVAIL_TIME,

    /**
     * Indicate the STK service state
     */
    RFX_STATUS_KEY_IS_CAT_RUNNING,

    /**
     * Cache current stk command type
     */
    RFX_STATUS_KEY_STK_CACHE_CMD_TYPE,

    /**
     * Capability switch internal use for sync status
     * value type: bool
     */
    RFX_STATUS_KEY_CAPABILITY_SWITCH_URC_CHANNEL,

    /**
     * Store PCO status for radio manager.
     * value type: String
     */
    RFX_STATUS_KEY_PCO_STATUS,

    /*
    * indicate AP has power off modem and power on yet
    */
    RFX_STATUS_KEY_HAD_POWER_OFF_MD,

    /**
     * TRN call ID
     * value type: integer
     */
    RFX_STATUS_KEY_TRN_CALLID,

    /**
     * Store TRN for Digits Service
     * value type: String
     */
    RFX_STATUS_KEY_TRN,

    /*Please add your keys above this line*/
    RFX_STATUS_KEY_END_OF_ENUM
};

#define RFX_CARD_TYPE_SIM  0x01
#define RFX_CARD_TYPE_USIM 0x02
#define RFX_CARD_TYPE_CSIM 0x04
#define RFX_CARD_TYPE_RUIM 0x08
#define RFX_CARD_TYPE_ISIM 0x10

#define RFX_SIM_STATE_NOT_READY 0
#define RFX_SIM_STATE_READY 1
#define RFX_SIM_STATE_LOCKED 2
#define RFX_SIM_STATE_ABSENT 3

/* MODEM_OFF_STATE*/
#define MODEM_OFF_IN_IDLE             (0)
#define MODEM_OFF_BY_MODE_SWITCH      (1)
#define MODEM_OFF_BY_POWER_OFF        (2)
#define MODEM_OFF_BY_SIM_SWITCH       (3)
#define MODEM_OFF_BY_RESET_RADIO      (4)
#define MODEM_OFF_BY_WORLD_PHONE      (5)

/*SMS type*/
#define SMS_INBOUND_NONE (0)
#define SMS_INBOUND_3GPP_CMT (1)
#define SMS_INBOUND_3GPP_CDS (2)
#define SMS_INBOUND_3GPP_CMTI (3)

/*SMS phone storage status*/
#define SMS_PHONE_STORAGE_AVAILABLE (0)
#define SMS_PHONE_STORAGE_FULL (1)

#define CDMA_MO_SMS_SENDING (0)
#define CDMA_MO_SMS_SENT (1)

#define CDMA_SMS_INBOUND_NONE    (0)
#define CDMA_SMS_INBOUND_COMM    (1)
#define CDMA_SMS_INBOUND_VMI     (2)

#define AP_TRIGGER_SWITCH_SIM (1)
#define GMSS_TRIGGER_SWITCH_SIM (2)
#define AP_TRIGGER_SWITCH_RUIM (3)
#define GMSS_TRIGGER_SWITCH_RUIM (4)

/* For RAT SWITCH*/
typedef enum {
    RAT_SWITCH_UNKNOWN = -1,
    /* Rat switch for mode controller */
    RAT_SWITCH_INIT = 0,
    /* RAT switch done for NWS */
    RAT_SWITCH_NWS = 1,
    /* RAT switch done for RIL Request and signal */
    RAT_SWITCH_NORMAL = 2,
    /* Rat switch for some restricted mode. ex: ECC redial */
    RAT_SWITCH_RESTRICT = 3
} RatSwitchCaller;

/* RFX_STATUS_KEY_RADIO_LOCK */
typedef enum {
    RADIO_LOCK_IDLE = 0,
    RADIO_LOCK_BY_RADIO,
    RADIO_LOCK_BY_WORLD_MODE,
    RADIO_LOCK_BY_SIM_SWITCH,
} RadioPowerLock;

/* DATA CONNECTION STATE*/
#define DATA_STATE_DISCONNECTED        (0)
#define DATA_STATE_CONNECTED           (1)

#define WORLD_MODE_SWITCHING (0)

/* For Bluetooth SIM Access Profile */
typedef enum
{
   BT_SAP_INIT,
   BT_SAP_CONNECTION_SETUP,
   BT_SAP_ONGOING_CONNECTION,
   BT_SAP_DISCONNECT,
   BT_SAP_POWER_ON,
   BT_SAP_POWER_OFF,
} BtSapStatus;

/* RFX_STATUS_KEY_CAPABILITY_SWITCH_STATE */
typedef enum {
    CAPABILITY_SWITCH_STATE_IDLE = 0,
    CAPABILITY_SWITCH_STATE_START = 1,
} CapabilitySwitchState;

#define RFX_STATUS_DEFAULT_VALUE_ENTRY(key, value) {key, #key, value}

#define RFX_STATUS_DEFAULT_VALUE_TABLE_BEGIN(class_name)                       \
        const class_name::StatusDefaultValueEntry class_name::s_default_value_table[] = {

#define RFX_STATUS_DEFAULT_VALUE_TABLE_END                         \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_END_OF_ENUM, RfxVariant())}

#define RFX_STATUS_DECLARE_DEFAULT_VALUE_TABLE                     \
        static const StatusDefaultValueEntry s_default_value_table[]

#define RFX_STATUS_IMPLEMENT_DEFAULT_VALUE_TABLE(class_name)                                                    \
    RFX_STATUS_DEFAULT_VALUE_TABLE_BEGIN(class_name)                                                            \
        /*Please add your default value below this line*/                                           \
        /*NOTE. below every line should be ended by "\" */                                          \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_CARD_TYPE, RfxVariant(-1)),                   \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_CDMA_CARD_TYPE, RfxVariant(4)),              \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_CDMA_CARD_READY, RfxVariant(0)),              \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_CDMA_FILE_READY, RfxVariant(false)),          \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_CDMA3G_DUALMODE_CARD, RfxVariant(false)),     \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_ESIMIND_APPLIST, RfxVariant(-1)),               \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_CDMA_LOCKED_CARD, RfxVariant(false)),     \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_UICC_SUB_CHANGED_STATUS, RfxVariant(-1)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_UICC_GSM_NUMERIC, RfxVariant(String8(""))), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_UICC_CDMA_NUMERIC, RfxVariant(String8(""))), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_C2K_IMSI, RfxVariant(String8(""))),      \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_BTSAP_STATUS, RfxVariant(BT_SAP_INIT)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_BTSAP_CURRENT_PROTOCOL, RfxVariant(0)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_BTSAP_SUPPORT_PROTOCOL, RfxVariant(0)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_BTSAP_ATR, RfxVariant(String8(""))), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_SIM_ESIMS_CAUSE, RfxVariant(-1)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_ECPIN_STATE, RfxVariant(RFX_ECPIN_DONE)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_MODEM_SIM_TASK_READY, RfxVariant(false)),     \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_SERVICE_STATE, RfxVariant(RfxNwServiceState())),                 \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_VOICE_TYPE, RfxVariant()),                    \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_DATA_TYPE, RfxVariant()),                     \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_RADIO_STATE, RfxVariant()),                   \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_MODEM_POWER_OFF, RfxVariant(false)),      \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_MODEM_OFF_STATE, RfxVariant(0)),      \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_SIM_STATE, RfxVariant(0)),               \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_SIM_INSERT_STATE, RfxVariant(0)),               \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_GSM_IMSI, RfxVariant(String8(""))),      \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_SLOT_CAPABILITY, RfxVariant(0)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_OTA_STATUS, RfxVariant(-1)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_CALL_STATE, RfxVariant(RfxCallState())), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_VOICE_CALL_COUNT, RfxVariant(0)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_AP_VOICE_CALL_COUNT, RfxVariant(0)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_EMERGENCY_MODE, RfxVariant(false)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_EMERGENCY_CALLBACK_MODE, RfxVariant(0)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_ATCI_IS_NUMERIC, RfxVariant(false)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_IMS_CALL_EXIST, RfxVariant(false)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_DATA_CONNECTION, RfxVariant(0)),      \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_WORLD_MODE_STATE, RfxVariant(1)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_GSM_WORLD_MODE_STATE, RfxVariant(1)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_CDMA_WORLD_MODE_STATE, RfxVariant(1)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_WORLD_MODE_BLOCKED_STATE, RfxVariant(0)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_WORLD_MODE_BLOCKED_CHANGING_STATE, RfxVariant(0)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_CELLULAR_PS_STATE, RfxVariant(0)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_WFC_STATE, RfxVariant(0)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_REPLACE_VT_CALL, RfxVariant(false)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_MAIN_CAPABILITY_SLOT, RfxVariant(0)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_DEFAULT_DATA_SIM, RfxVariant(-1)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_ECC_PREFERRED_RAT, RfxVariant(0)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_SLOT_FIXED_CAPABILITY, RfxVariant(0)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_RADIO_POWER_MSIM_MODE, RfxVariant(0)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_REQUEST_RADIO_POWER, RfxVariant(false)),      \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_RADIO_LOCK, RfxVariant(RADIO_LOCK_IDLE)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_GCF_TEST_MODE, RfxVariant(-1)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_GSM_INBOUND_SMS_TYPE, RfxVariant(0)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_SMS_PHONE_STORAGE, RfxVariant(0)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_CDMA_INBOUND_SMS_TYPE, RfxVariant(0)),\
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_CDMA_SMS_REPLY_SEQ_NO, RfxVariant(-1)),\
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_CDMA_SMS_ADDR, RfxVariant(Vector<char>())),\
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_CDMA_MO_SMS_STATE, RfxVariant(1)),\
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_CDMA_PENDING_VMI, RfxVariant(-1)),\
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_PREFERRED_NW_TYPE, RfxVariant(RfxVariant(-1))),  \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_IS_RAT_MODE_SWITCHING, RfxVariant(0)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_OPERATOR, RfxVariant(String8(""))), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_MODESWITCH_FINISHED, RfxVariant(-1)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_CDMA3G_SWITCH_CARD, RfxVariant(-1)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_CAPABILITY_SWITCH_STATE, RfxVariant(CAPABILITY_SWITCH_STATE_IDLE)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_CAPABILITY_SWITCH_WAIT_MODULE, RfxVariant(0)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_SIM_SWITCH_RADIO_UNAVAIL_TIME, RfxVariant(int64_t(0))), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_IS_CAT_RUNNING, RfxVariant(false)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_STK_CACHE_CMD_TYPE, RfxVariant(0)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_CAPABILITY_SWITCH_URC_CHANNEL, RfxVariant(false)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_PCO_STATUS, RfxVariant(String8(""))), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_HAD_POWER_OFF_MD, RfxVariant(false)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_TRN_CALLID, RfxVariant(-1)), \
        RFX_STATUS_DEFAULT_VALUE_ENTRY(RFX_STATUS_KEY_TRN, RfxVariant(String8(""))), \
        /*Please add your default value above this line*/                                           \
    RFX_STATUS_DEFAULT_VALUE_TABLE_END

#endif /* __RFX_STATUS_DEFS_H__ */
