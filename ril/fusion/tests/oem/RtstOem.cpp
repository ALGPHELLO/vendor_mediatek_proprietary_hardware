/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 */
/* MediaTek Inc. (C) 2016. All rights reserved.
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
#include "Rtst.h"

/*****************************************************************************
 * Test Cases
 *****************************************************************************/
TEST(OemTest, RIL_REQUEST_DEVICE_IDENTITY) {
    RTST_CASE_BEGIN();
    RTST_RIL_VOID_REQUEST(RIL_REQUEST_DEVICE_IDENTITY);
    RTST_AT_CMD(RIL_CMD_PROXY_3, "AT+CGSN", 2, "575684362355444", "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_3, "AT+EGMR=0,9", 2, "+EGMR:1234", "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_3, "AT+GSN", 2, "+GSN:0x804c1d82", "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_3, "AT^MEID", 2, "^MEID:0xa10000452ec54a", "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_3, "AT+CCID?", 2, "+CCID:0x8099dc36", "OK");
    RTST_EXPECTED_RIL_RESPONSE(RIL_REQUEST_DEVICE_IDENTITY, RIL_E_SUCCESS, 7,
        RTST_STRING, "575684362355444",
        RTST_STRING, "1234",
        RTST_STRING, "804c1d82",
        RTST_STRING, "A10000452EC54A",
        RTST_STRING, "0x8099dc36",
        RTST_STRING, "",
        RTST_STRING, "");
    RTST_CASE_END();
}

TEST(OemTest, OEM_TEST_TX_POWER) {
    RTST_CASE_BEGIN();
    RTST_URC_STRING_WITH_SLOT(RFX_SLOT_ID_0, "+ETXPWR: 1, 100");
    RTST_EXPECTED_RIL_URC_WITH_SLOT(RIL_UNSOL_TX_POWER, RFX_SLOT_ID_0, 2,
        RTST_INT32, "1",
        RTST_INT32, "100");
    RTST_CASE_END();
}
