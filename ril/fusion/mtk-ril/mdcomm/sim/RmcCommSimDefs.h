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

#ifndef __RMC_COMM_SIM_DEF_H__
#define __RMC_COMM_SIM_DEF_H__

/*****************************************************************************
 * Include
 *****************************************************************************/


/*****************************************************************************
 * Enum
 *****************************************************************************/
typedef enum {
    UICC_ABSENT = 0,
    UICC_NOT_READY = 1,
    UICC_READY = 2, /* SIM_READY means the radio state is RADIO_STATE_SIM_READY */
    UICC_PIN = 3,
    UICC_PUK = 4,
    UICC_NETWORK_PERSONALIZATION = 5,
    UICC_BUSY = 9,
    UICC_NP = 10,
    UICC_NSP = 11,
    UICC_SP = 12,
    UICC_CP = 13,
    UICC_SIMP =14,
    UICC_PERM_BLOCKED = 15, // PERM_DISABLED
    // MTK-START: AOSP SIM PLUG IN/OUT
    UICC_NO_INIT = 16,
    // MTK-END
} UICC_Status;

typedef enum {
    UICC_APP_ISIM = 0,
    UICC_APP_USIM = 1,
    UICC_APP_CSIM = 2, /* SIM_READY means the radio state is RADIO_STATE_SIM_READY */
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
} UICC_Security_Operation;

/*****************************************************************************
 * Define
 *****************************************************************************/

#define MAX_AUTH_RSP   (256*2+27)
#define MAX_SIM_ME_LOCK_CAT_NUM 7
#define PROPERTY_GSM_GCF_TEST_MODE  "gsm.gcf.testmode"
#define PROPERTY_ICCID_PREIFX "ril.iccid.sim"
#define PROPERTY_COMMON_SLOT_SUPPORT "ro.mtk_sim_hot_swap_common_slot"
#define PROPERTY_FULL_UICC_TYPE "gsm.ril.fulluicctype"
#define PROPERTY_EF_ECC "ril.ef.ecc.support"
#define PROPERTY_EXTERNAL_SIM_ENABLED "gsm.external.sim.enabled"
// MTK-START: AOSP SIM PLUG IN/OUT
#define ESIMS_CAUSE_SIM_NO_INIT 26
// MTK-END

class RmcSimPinPukCount {

    public:
        int pin1;
        int pin2;
        int puk1;
        int puk2;
};


#endif /* __RMC_COMM_SIM_DEF_H__ */

