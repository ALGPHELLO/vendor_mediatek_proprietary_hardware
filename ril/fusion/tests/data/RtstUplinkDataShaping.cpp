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
TEST(UplinkDataShapingTest, RIL_REQUEST_SET_LTE_ACCESS_STRATUM_REPORT_1) {
    RTST_CASE_BEGIN();
    RTST_RIL_REQUEST(RIL_REQUEST_SET_LTE_ACCESS_STRATUM_REPORT, 1,
            RTST_INT32, "0");  // 0: turn off URC +EDRBSTATE, 1: turn on URC +EDRBSTATE.
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EDRB=0", 1,
            "ERROR");
    RTST_EXPECTED_RIL_VOID_RESPONSE(RIL_REQUEST_SET_LTE_ACCESS_STRATUM_REPORT, RIL_E_GENERIC_FAILURE);
    RTST_CASE_END();
}

TEST(UplinkDataShapingTest, RIL_REQUEST_SET_LTE_ACCESS_STRATUM_REPORT_2) {
    RTST_CASE_BEGIN();
    RTST_RIL_REQUEST(RIL_REQUEST_SET_LTE_ACCESS_STRATUM_REPORT, 1,
            RTST_INT32, "0");  // 0: turn off URC +EDRBSTATE, 1: turn on URC +EDRBSTATE.
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EDRB=0", 1,
            "+CME ERROR: 100");
    RTST_EXPECTED_RIL_VOID_RESPONSE(RIL_REQUEST_SET_LTE_ACCESS_STRATUM_REPORT, RIL_E_GENERIC_FAILURE);
    RTST_CASE_END();
}

TEST(UplinkDataShapingTest, RIL_REQUEST_SET_LTE_ACCESS_STRATUM_REPORT_3) {
    RTST_CASE_BEGIN();
    RTST_RIL_REQUEST(RIL_REQUEST_SET_LTE_ACCESS_STRATUM_REPORT, 1,
            RTST_INT32, "0");  // 0: turn off URC +EDRBSTATE, 1: turn on URC +EDRBSTATE.
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EDRB=0", 1,
            "OK");
    RTST_EXPECTED_RIL_VOID_RESPONSE(RIL_REQUEST_SET_LTE_ACCESS_STRATUM_REPORT, RIL_E_SUCCESS);
    RTST_CASE_END();
}

TEST(UplinkDataShapingTest, RIL_REQUEST_SET_LTE_ACCESS_STRATUM_REPORT_4) {
    RTST_CASE_BEGIN();
    RTST_RIL_REQUEST(RIL_REQUEST_SET_LTE_ACCESS_STRATUM_REPORT, 1,
            RTST_INT32, "1");  // 0: turn off URC +EDRBSTATE, 1: turn on URC +EDRBSTATE.
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EDRB=1", 1,
            "OK");
    RTST_EXPECTED_RIL_VOID_RESPONSE(RIL_REQUEST_SET_LTE_ACCESS_STRATUM_REPORT, RIL_E_SUCCESS);
    RTST_CASE_END();
}

TEST(UplinkDataShapingTest, RFX_MSG_EVENT_LTE_ACCESS_STRATUM_STATE_CHANGE_1) {
    RTST_CASE_BEGIN();
    RTST_URC_STRING("+EDRBSTATE: 1");
    RTST_EXPECTED_RIL_URC(RIL_UNSOL_LTE_ACCESS_STRATUM_STATE_CHANGE, 1,
            RTST_INT32, "1");  // drb_state
    RTST_CASE_END();
}

TEST(UplinkDataShapingTest, RFX_MSG_EVENT_LTE_ACCESS_STRATUM_STATE_CHANGE_2) {
    RTST_CASE_BEGIN();
    RTST_URC_STRING("+EDRBSTATE: 0,7");
    RTST_EXPECTED_RIL_URC(RIL_UNSOL_LTE_ACCESS_STRATUM_STATE_CHANGE, 2,
            RTST_INT32, "0",  // drb_state
            RTST_INT32, "14");  // act
    RTST_CASE_END();
}

TEST(UplinkDataShapingTest, RIL_REQUEST_SET_LTE_UPLINK_DATA_TRANSFER_1) {
    RTST_CASE_BEGIN();
    RTST_RIL_REQUEST(RIL_REQUEST_SET_LTE_UPLINK_DATA_TRANSFER, 2,
            RTST_INT32, "39321600",  // state: 10 mins
            RTST_INT32, "1412");  // transIntfId
    RTST_EXPECTED_RIL_VOID_RESPONSE(RIL_REQUEST_SET_LTE_UPLINK_DATA_TRANSFER, RIL_E_GENERIC_FAILURE);
    RTST_CASE_END();
}

TEST(UplinkDataShapingTest, RIL_REQUEST_SETUP_DATA_CALL_IPV4_1) {
    RTST_CASE_BEGIN();
    RTST_RIL_REQUEST(RIL_REQUEST_SETUP_DATA_CALL, 8,
            RTST_STRING, "14",  // radioType
            RTST_STRING, "0",  // profile
            RTST_STRING, "internet",  // apn
            RTST_STRING, "",  // username
            RTST_STRING, "",  // password
            RTST_STRING, "0",  // authType
            RTST_STRING, "IP",  // protocol
            RTST_STRING, "1");  // interfaceId, not used for 93 rild

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=1,\"internet\",\"default\"", 2,
            "+CGEV: ME PDN ACT 3, 2",
            "OK");
    RTST_URC_STRING("+EUTTEST: SET TRANSACTION INTERFACE ID 1412");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=3,\"ifst\",20", 2,
            "+EPDN:3, \"new\", 1, 1412, 2454, 1, \"172.22.1.100\"",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP", 2,
            "+CGCONTRDP: 3,5,\"internet\",\"\",\"\",\"172.22.1.201\",\"\",\"\",\"\",0,0,2454,,,",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGACT?", 12,
            "+CGACT: 0,0",
            "+CGACT: 1,0",
            "+CGACT: 2,0",
            "+CGACT: 3,1",
            "+CGACT: 4,0",
            "+CGACT: 5,0",
            "+CGACT: 6,0",
            "+CGACT: 7,0",
            "+CGACT: 8,0",
            "+CGACT: 9,0",
            "+CGACT: 10,0",
            "OK");

    RTST_EXPECTED_RIL_RESPONSE(RIL_REQUEST_SETUP_DATA_CALL, RIL_E_SUCCESS, 14,
            RTST_INT32, "11",  // ril data call response version
            RTST_INT32, "1",  // num
            RTST_INT32, "0",  // status
            RTST_INT32, "-1",  // suggestedRetryTime
            RTST_INT32, "1412",  // cid, means interfaceId actually
            RTST_INT32, "2",  // active
            RTST_STRING, "IP",  // type
            RTST_STRING, "ccmni12",  // ifname
            RTST_STRING, "172.22.1.100",  // addresses
            RTST_STRING, "172.22.1.201",  // dnses
            RTST_STRING, "172.22.1.100",  // gateways
            RTST_STRING, "",  // pcscf
            RTST_INT32, "2454",  // mtu
            RTST_INT32, "1");  // rat
    RTST_CASE_END();
}

TEST(UplinkDataShapingTest, RIL_REQUEST_SET_LTE_UPLINK_DATA_TRANSFER_2) {
    RTST_CASE_BEGIN();
    RTST_RIL_REQUEST(RIL_REQUEST_SET_LTE_UPLINK_DATA_TRANSFER, 2,
            RTST_INT32, "39321600",  // state: 10 mins
            RTST_INT32, "1412");  // transIntfId
    RTST_EXPECTED_RIL_VOID_RESPONSE(RIL_REQUEST_SET_LTE_UPLINK_DATA_TRANSFER, RIL_E_SUCCESS);
    RTST_CASE_END();
}

TEST(UplinkDataShapingTest, RIL_REQUEST_SET_LTE_UPLINK_DATA_TRANSFER_3) {
    RTST_CASE_BEGIN();
    RTST_RIL_REQUEST(RIL_REQUEST_SET_LTE_UPLINK_DATA_TRANSFER, 2,
            RTST_INT32, "1",  // state: 10 mins
            RTST_INT32, "1412");  // transIntfId
    RTST_EXPECTED_RIL_VOID_RESPONSE(RIL_REQUEST_SET_LTE_UPLINK_DATA_TRANSFER, RIL_E_SUCCESS);
    RTST_CASE_END();
}

TEST(UplinkDataShapingTest, RFX_MSG_EVENT_DATA_TEST_MODE_1) {
    RTST_CASE_BEGIN();
    RTST_URC_STRING("+EUTTEST: CLEAR ALL PDN TABLE");
    RTST_CASE_END();
}

TEST(UplinkDataShapingTest, RFX_MSG_EVENT_DATA_TEST_MODE_2) {
    RTST_CASE_BEGIN();
    RTST_URC_STRING("+EUTTEST: REMOVE ALL TRANSACTION INTERFACE ID");
    RTST_CASE_END();
}
