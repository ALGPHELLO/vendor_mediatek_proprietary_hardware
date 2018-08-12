/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 */
/* MediaTek Inc. (C) 2017. All rights reserved.
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
TEST(DcLceTest, RIL_REQUEST_START_LCE_1) {
    RTST_CASE_BEGIN();
    RTST_RIL_REQUEST(RIL_REQUEST_START_LCE, 2,
            RTST_INT32, "200", // reportIntervalMs
            RTST_INT32, "1" // pullMode
            );
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+ELCE=2,200", 2,
            "+ELCE: 1,200",
            "OK");
    RTST_EXPECTED_RIL_RESPONSE(RIL_REQUEST_START_LCE, RIL_E_SUCCESS, 2,
            RTST_INT8, "1", // lceStatus
            RTST_INT32, "200" // actualIntervalMs
            );
    RTST_CASE_END();
}

TEST(DcLceTest, RIL_REQUEST_START_LCE_2) {
    RTST_CASE_BEGIN();
    RTST_RIL_REQUEST(RIL_REQUEST_START_LCE, 2,
            RTST_INT32, "200", // reportIntervalMs
            RTST_INT32, "1" // pullMode
            );
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+ELCE=2,200", 1,
            "ERROR");
    RTST_EXPECTED_RIL_RESPONSE(RIL_REQUEST_START_LCE, RIL_E_SUCCESS, 2,
            RTST_INT8, "-1", // lceStatus
            RTST_INT32, "0" // actualIntervalMs
            );
    RTST_CASE_END();
}

TEST(DcLceTest, RIL_REQUEST_START_LCE_3) {
    RTST_CASE_BEGIN();
    RTST_RIL_REQUEST(RIL_REQUEST_START_LCE, 2,
            RTST_INT32, "200", // reportIntervalMs
            RTST_INT32, "1" // pullMode
            );
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+ELCE=2,200", 2,
            "+ELCE: 1,abc",
            "OK");
    RTST_EXPECTED_RIL_VOID_RESPONSE(RIL_REQUEST_START_LCE, RIL_E_GENERIC_FAILURE);
    RTST_CASE_END();
}

TEST(DcLceTest, RIL_REQUEST_STOP_LCE_1) {
    RTST_CASE_BEGIN();
    RTST_RIL_VOID_REQUEST(RIL_REQUEST_STOP_LCE);
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+ELCE=0", 2,
            "+ELCE: 1,200",
            "OK");
    RTST_EXPECTED_RIL_RESPONSE(RIL_REQUEST_STOP_LCE, RIL_E_SUCCESS, 2,
            RTST_INT8, "1", // lceStatus
            RTST_INT32, "200" // actualIntervalMs
            );
    RTST_CASE_END();
}

TEST(DcLceTest, RIL_REQUEST_STOP_LCE_2) {
    RTST_CASE_BEGIN();
    RTST_RIL_VOID_REQUEST(RIL_REQUEST_STOP_LCE);
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+ELCE=0", 1,
            "ERROR");
    RTST_EXPECTED_RIL_RESPONSE(RIL_REQUEST_STOP_LCE, RIL_E_SUCCESS, 2,
            RTST_INT8, "-1", // lceStatus
            RTST_INT32, "0" // actualIntervalMs
            );
    RTST_CASE_END();
}

TEST(DcLceTest, RIL_REQUEST_STOP_LCE_3) {
    RTST_CASE_BEGIN();
    RTST_RIL_VOID_REQUEST(RIL_REQUEST_STOP_LCE);
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+ELCE=0", 2,
            "+ELCE: abs,200",
            "OK");
    RTST_EXPECTED_RIL_VOID_RESPONSE(RIL_REQUEST_STOP_LCE, RIL_E_GENERIC_FAILURE);
    RTST_CASE_END();
}

TEST(DcLceTest, RIL_REQUEST_PULL_LCEDATA_1) {
    RTST_CASE_BEGIN();
    RTST_RIL_VOID_REQUEST(RIL_REQUEST_PULL_LCEDATA);
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+ELCE?", 2,
            "+ELCE: 2,1314,50,0",
            "OK");
    RTST_EXPECTED_RIL_RESPONSE(RIL_REQUEST_PULL_LCEDATA, RIL_E_SUCCESS, 3,
            RTST_INT32, "1314", // last_hop_capacity_kbps
            RTST_INT8, "50", // confidence_level
            RTST_INT8, "0" // lce_suspended
            );
    RTST_CASE_END();
}

TEST(DcLceTest, RIL_REQUEST_PULL_LCEDATA_2) {
    RTST_CASE_BEGIN();
    RTST_RIL_VOID_REQUEST(RIL_REQUEST_PULL_LCEDATA);
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+ELCE?", 2,
            "+ELCE: 2,4294967295,50,0",
            "OK");
    RTST_EXPECTED_RIL_RESPONSE(RIL_REQUEST_PULL_LCEDATA, RIL_E_SUCCESS, 3,
            RTST_INT32, "4294967295", // last_hop_capacity_kbps
            RTST_INT8, "50", // confidence_level
            RTST_INT8, "0" // lce_suspended
            );
    RTST_CASE_END();
}

TEST(DcLceTest, RIL_REQUEST_PULL_LCEDATA_3) {
    RTST_CASE_BEGIN();
    RTST_RIL_VOID_REQUEST(RIL_REQUEST_PULL_LCEDATA);
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+ELCE?", 2,
            "+ELCE: 2,1314,101,0",
            "OK");
    RTST_EXPECTED_RIL_RESPONSE(RIL_REQUEST_PULL_LCEDATA, RIL_E_SUCCESS, 3,
            RTST_INT32, "1314", // last_hop_capacity_kbps
            RTST_INT8, "101", // confidence_level
            RTST_INT8, "0" // lce_suspended
            );
    RTST_CASE_END();
}

TEST(DcLceTest, RIL_REQUEST_PULL_LCEDATA_4) {
    RTST_CASE_BEGIN();
    RTST_RIL_VOID_REQUEST(RIL_REQUEST_PULL_LCEDATA);
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+ELCE?", 2,
            "+ELCE: 2,1314,50,-1",
            "OK");
    RTST_EXPECTED_RIL_RESPONSE(RIL_REQUEST_PULL_LCEDATA, RIL_E_SUCCESS, 3,
            RTST_INT32, "1314", // last_hop_capacity_kbps
            RTST_INT8, "50", // confidence_level
            RTST_INT8, "-1" // lce_suspended
            );
    RTST_CASE_END();
}

TEST(DcLceTest, RIL_REQUEST_PULL_LCEDATA_5) {
    RTST_CASE_BEGIN();
    RTST_RIL_VOID_REQUEST(RIL_REQUEST_PULL_LCEDATA);
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+ELCE?", 2,
            "+ELCE: 2,abc,50,0",
            "OK");
    RTST_EXPECTED_RIL_VOID_RESPONSE(RIL_REQUEST_PULL_LCEDATA, RIL_E_GENERIC_FAILURE);
    RTST_CASE_END();
}

TEST(DcLceTest, RFX_MSG_EVENT_DATA_LCE_STATUS_CHANGED_1) {
    RTST_CASE_BEGIN();
    RTST_URC_STRING("+ELCE: 1314,50,0");
    RTST_EXPECTED_RIL_URC(RIL_UNSOL_LCEDATA_RECV, 3,
            RTST_INT32, "1314", // last_hop_capacity_kbps
            RTST_INT8, "50", // confidence_level
            RTST_INT8, "0" // lce_suspended
            );
    RTST_CASE_END();
}

TEST(DcLceTest, RFX_MSG_EVENT_DATA_LCE_STATUS_CHANGED_2) {
    RTST_CASE_BEGIN();
    RTST_URC_STRING("+ELCE: abc,50,0");
    RTST_CASE_END();
}