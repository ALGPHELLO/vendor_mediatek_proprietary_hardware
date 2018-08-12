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
#include "rfx_properties.h"

/*****************************************************************************
 * Initialize
 *****************************************************************************/
RTST_INIT_AT_CMD(A, "AT+CGDCONT=?", 4,
        "+CGDCONT: (0-10),\"IP\",,,(0),(0),(0-1),(0-3),(0-2),(0-1),(0-1)",
        "+CGDCONT: (0-10),\"IPV6\",,,(0),(0),(0-1),(0-3),(0-2),(0-1),(0-1)",
        "+CGDCONT: (0-10),\"IPV4V6\",,,(0),(0),(0-1),(0-3),(0-2),(0-1),(0-1)",
        "OK");

class DataConnectionTest: public ::testing::Test {
public:
    DataConnectionTest() {
    }

    void SetUp() {
        RFX_LOG_D(TAG, "%s+", ::testing::UnitTest::GetInstance()->current_test_info()->name());
        rfx_property_set("persist.operator.optr", "");
        rfx_property_set("gsm.operator.numeric", "");
        rfx_property_set("gsm.ril.uicc.mccmnc", "");
    }

    void TearDown() {
        RtstCase _caseril("ril", rfx_assert, step_test);
        rfx_property_set("persist.operator.optr", "");
        rfx_property_set("gsm.operator.numeric", "");
        rfx_property_set("gsm.ril.uicc.mccmnc", "");
        RTST_URC_STRING("+EUTTEST: CLEAR ALL PDN TABLE");
        RTST_URC_STRING("+EUTTEST: AT");
        RTST_AT_CMD(RIL_CMD_PROXY_5, "AT", 1, "OK");
        RFX_LOG_D(TAG, "%s-", ::testing::UnitTest::GetInstance()->current_test_info()->name());
    }

    ~DataConnectionTest() {
    }

    void setupDataCall(const char* reqtype, const char* rsptype) {
        RtstCase _caseril("ril", rfx_assert, step_test);
        RTST_RIL_REQUEST(RIL_REQUEST_SETUP_DATA_CALL, 8,
                RTST_STRING, "14",  // radioType
                RTST_STRING, "0",  // profile
                RTST_STRING, "internet",  // apn
                RTST_STRING, "",  // username
                RTST_STRING, "",  // password
                RTST_STRING, "0",  // authType
                RTST_STRING, reqtype,  // protocol
                RTST_STRING, "1");  // interfaceId, not used for 93 rild

        RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=1,\"internet\",\"default\",0", 2,
                "+CGEV: ME PDN ACT 1, 2",
                "OK");

        if (strncmp("IP", rsptype, strlen(rsptype)) == 0) {
            RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=1,\"ifst\",20", 2,
                    "+EPDN:1, \"new\", 1, 0, 2454, 1, \"172.22.1.100\"",
                    "OK");
            RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP", 2,
                    "+CGCONTRDP: 1,5,\"internet\",\"\",\"\",\"172.22.1.201\",\"\",\"\",\"\",0,0,2454,,,",
                    "OK");
        } else if (strncmp("IPV4V6", rsptype, strlen(rsptype)) == 0) {
            RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=1,\"ifst\",20", 2,
                    "+EPDN:1, \"new\", 1, 0, 2454, 3, \"172.22.1.100\", \"38.7.251.144.32.14.171.78.0.0.0.81.202.39.147.1/64\"",
                    "OK");
            RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP", 3,
                    "+CGCONTRDP: 1,5,\"internet\",\"\",\"\",\"172.22.1.201\",\"\",\"\",\"\",0,0,2454,,,",
                    "+CGCONTRDP: 1,5,\"internet\",\"\",\"\",\"35.1.251.144.32.14.171.78.0.0.0.81.202.39.83.25\",\"\",\"\",\"\",0,0,2454,,,",
                    "OK");
        }

        RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGACT?", 12,
                "+CGACT: 0,0",
                "+CGACT: 1,1",
                "+CGACT: 2,0",
                "+CGACT: 3,0",
                "+CGACT: 4,0",
                "+CGACT: 5,0",
                "+CGACT: 6,0",
                "+CGACT: 7,0",
                "+CGACT: 8,0",
                "+CGACT: 9,0",
                "+CGACT: 10,0",
                "OK");

        if (strncmp("IP", rsptype, strlen(rsptype)) == 0) {
            RTST_EXPECTED_RIL_RESPONSE(RIL_REQUEST_SETUP_DATA_CALL, RIL_E_SUCCESS, 14,
                    RTST_INT32, "11",  // ril data call response version
                    RTST_INT32, "1",  // num
                    RTST_INT32, "0",  // status
                    RTST_INT32, "-1",  // suggestedRetryTime
                    RTST_INT32, "0",  // cid, means interfaceId actually
                    RTST_INT32, "2",  // active
                    RTST_STRING, rsptype,  // type
                    RTST_STRING, "ccmni0",  // ifname
                    RTST_STRING, "172.22.1.100",  // addresses
                    RTST_STRING, "172.22.1.201",  // dnses
                    RTST_STRING, "172.22.1.100",  // gateways
                    RTST_STRING, "",  // pcscf
                    RTST_INT32, "2454",  // mtu
                    RTST_INT32, "1");  // rat
        } else if (strncmp("IPV4V6", rsptype, strlen(rsptype)) == 0) {
            RTST_EXPECTED_RIL_RESPONSE(RIL_REQUEST_SETUP_DATA_CALL, RIL_E_SUCCESS, 14,
                RTST_INT32, "11",  // ril data call response version
                RTST_INT32, "1",  // num
                RTST_INT32, "0",  // status
                RTST_INT32, "-1",  // suggestedRetryTime
                RTST_INT32, "0",  // cid, means interfaceId actually
                RTST_INT32, "2",  // active
                RTST_STRING, rsptype, // type
                RTST_STRING, "ccmni0",  // ifname
                RTST_STRING, "172.22.1.100 2607:FB90:200E:AB4E:0000:0051:CA27:9301/64",  // addresses
                RTST_STRING, "172.22.1.201 2301:FB90:200E:AB4E:0000:0051:CA27:5319",  // dnses
                RTST_STRING, "172.22.1.100 ::",  // gateways
                RTST_STRING, "",  // pcscf
                RTST_INT32, "2454",  // mtu
                RTST_INT32, "1");  // rat
        }
    }

    void pcoPropertySetup() {
        rfx_property_set("persist.operator.optr", "OP12");
        rfx_property_set("gsm.ril.uicc.mccmnc", "311480 , 46697");
    }

    const char *TAG = "[GT]DcTest";
    const bool rfx_assert = false;
    const bool step_test = true;
};
/*****************************************************************************
 * Test Cases
 *****************************************************************************/
TEST_F(DataConnectionTest, RIL_REQUEST_SETUP_DATA_CALL_IPV4_FAIL_1) {
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

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=1,\"internet\",\"default\",0", 1,
            "+CME ERROR: 100");

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EDRETRY=0,\"internet\"", 2,
            "+EDRETRY: 0",
            "OK");

    RTST_EXPECTED_RIL_RESPONSE(RIL_REQUEST_SETUP_DATA_CALL, RIL_E_GENERIC_FAILURE, 14,
            RTST_INT32, "11",  // ril data call response version
            RTST_INT32, "1",  // num
            RTST_INT32, "65535",  // status
            RTST_INT32, "-1",  // suggestedRetryTime
            RTST_INT32, "-1",  // cid, means interfaceId actually
            RTST_INT32, "0",  // active
            RTST_STRING, "",  // type
            RTST_STRING, "",  // ifname
            RTST_STRING, "",  // addresses
            RTST_STRING, "",  // dnses
            RTST_STRING, "",  // gateways
            RTST_STRING, "",  // pcscf
            RTST_INT32, "0",  // mtu
            RTST_INT32, "1");  // rat

    RTST_CASE_END();
}

TEST_F(DataConnectionTest, RIL_REQUEST_SETUP_DATA_CALL_IPV4_FAIL_2) {
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

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=1,\"internet\",\"default\",0", 1,
            "+CME ERROR: 0");

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EDRETRY=0,\"internet\"", 2,
            "+EDRETRY: 0",
            "OK");

    RTST_EXPECTED_RIL_RESPONSE(RIL_REQUEST_SETUP_DATA_CALL, RIL_E_GENERIC_FAILURE, 14,
            RTST_INT32, "11",  // ril data call response version
            RTST_INT32, "1",  // num
            RTST_INT32, "65535",  // status
            RTST_INT32, "-1",  // suggestedRetryTime
            RTST_INT32, "-1",  // cid, means interfaceId actually
            RTST_INT32, "0",  // active
            RTST_STRING, "",  // type
            RTST_STRING, "",  // ifname
            RTST_STRING, "",  // addresses
            RTST_STRING, "",  // dnses
            RTST_STRING, "",  // gateways
            RTST_STRING, "",  // pcscf
            RTST_INT32, "0",  // mtu
            RTST_INT32, "1");  // rat

    RTST_CASE_END();
}

TEST_F(DataConnectionTest, RIL_REQUEST_SETUP_DATA_CALL_IPV4_FAIL_3) {
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

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=1,\"internet\",\"default\",0", 1,
            "+CME ERROR: 3361");

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EDRETRY=0,\"internet\"", 2,
            "+EDRETRY: 2,60",
            "OK");

    RTST_EXPECTED_RIL_RESPONSE(RIL_REQUEST_SETUP_DATA_CALL, RIL_E_GENERIC_FAILURE, 14,
            RTST_INT32, "11",  // ril data call response version
            RTST_INT32, "1",  // num
            RTST_INT32, "33",  // status
            RTST_INT32, "60000",  // suggestedRetryTime
            RTST_INT32, "-1",  // cid, means interfaceId actually
            RTST_INT32, "0",  // active
            RTST_STRING, "",  // type
            RTST_STRING, "",  // ifname
            RTST_STRING, "",  // addresses
            RTST_STRING, "",  // dnses
            RTST_STRING, "",  // gateways
            RTST_STRING, "",  // pcscf
            RTST_INT32, "0",  // mtu
            RTST_INT32, "1");  // rat

    RTST_CASE_END();
}

TEST_F(DataConnectionTest, RIL_REQUEST_SETUP_DATA_CALL_IPV4_FAIL_4) {
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

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=1,\"internet\",\"default\",0", 2,
            "+CGEV: ME PDN ACT 1, 2",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=1,\"ifst\",20", 1,
            "+CME ERROR: 100");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=0,1", 2,
            "+CGEV: ME PDN DEACT 1",
            "OK");

    RTST_EXPECTED_RIL_RESPONSE(RIL_REQUEST_SETUP_DATA_CALL, RIL_E_GENERIC_FAILURE, 14,
            RTST_INT32, "11",  // ril data call response version
            RTST_INT32, "1",  // num
            RTST_INT32, "65535",  // status
            RTST_INT32, "-1",  // suggestedRetryTime
            RTST_INT32, "-1",  // cid, means interfaceId actually
            RTST_INT32, "0",  // active
            RTST_STRING, "",  // type
            RTST_STRING, "",  // ifname
            RTST_STRING, "",  // addresses
            RTST_STRING, "",  // dnses
            RTST_STRING, "",  // gateways
            RTST_STRING, "",  // pcscf
            RTST_INT32, "0",  // mtu
            RTST_INT32, "1");  // rat

    RTST_CASE_END();
}

TEST_F(DataConnectionTest, RIL_REQUEST_SETUP_DATA_CALL_IPV4_FAIL_5) {
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

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=1,\"internet\",\"default\",0", 2,
            "+CGEV: ME PDN ACT 1, 2",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=1,\"ifst\",20", 2,
            "+EPDN: 1,\"err\",257",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=0,1", 2,
            "+CGEV: ME PDN DEACT 1",
            "OK");

    RTST_EXPECTED_RIL_RESPONSE(RIL_REQUEST_SETUP_DATA_CALL, RIL_E_GENERIC_FAILURE, 14,
            RTST_INT32, "11",  // ril data call response version
            RTST_INT32, "1",  // num
            RTST_INT32, "65535",  // status
            RTST_INT32, "-1",  // suggestedRetryTime
            RTST_INT32, "-1",  // cid, means interfaceId actually
            RTST_INT32, "0",  // active
            RTST_STRING, "",  // type
            RTST_STRING, "",  // ifname
            RTST_STRING, "",  // addresses
            RTST_STRING, "",  // dnses
            RTST_STRING, "",  // gateways
            RTST_STRING, "",  // pcscf
            RTST_INT32, "0",  // mtu
            RTST_INT32, "1");  // rat

    RTST_CASE_END();
}

TEST_F(DataConnectionTest, RIL_REQUEST_SETUP_DATA_CALL_IPV4_FAIL_6) {
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

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=1,\"internet\",\"default\",0", 2,
            "+CGEV: ME PDN ACT 1, 2",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=1,\"ifst\",20", 2,
            "+EPDN:1, \"test\", 1, 0, 2454, 1, \"172.22.1.100\"",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=0,1", 2,
            "+CGEV: ME PDN DEACT 1",
            "OK");

    RTST_EXPECTED_RIL_RESPONSE(RIL_REQUEST_SETUP_DATA_CALL, RIL_E_GENERIC_FAILURE, 14,
            RTST_INT32, "11",  // ril data call response version
            RTST_INT32, "1",  // num
            RTST_INT32, "65535",  // status
            RTST_INT32, "-1",  // suggestedRetryTime
            RTST_INT32, "-1",  // cid, means interfaceId actually
            RTST_INT32, "0",  // active
            RTST_STRING, "",  // type
            RTST_STRING, "",  // ifname
            RTST_STRING, "",  // addresses
            RTST_STRING, "",  // dnses
            RTST_STRING, "",  // gateways
            RTST_STRING, "",  // pcscf
            RTST_INT32, "0",  // mtu
            RTST_INT32, "1");  // rat

    RTST_CASE_END();
}

TEST_F(DataConnectionTest, RIL_REQUEST_SETUP_DATA_CALL_IPV4_FAIL_7) {
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

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=1,\"internet\",\"default\",0", 2,
            "+CGEV: ME PDN ACT 1, 2",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=1,\"ifst\",20", 1,
            "+CME ERROR: 5842");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=0,1", 2,
            "+CGEV: ME PDN DEACT 1",
            "OK");

    RTST_EXPECTED_RIL_RESPONSE(RIL_REQUEST_SETUP_DATA_CALL, RIL_E_GENERIC_FAILURE, 14,
            RTST_INT32, "11",  // ril data call response version
            RTST_INT32, "1",  // num
            RTST_INT32, "65535",  // status
            RTST_INT32, "-1",  // suggestedRetryTime
            RTST_INT32, "-1",  // cid, means interfaceId actually
            RTST_INT32, "0",  // active
            RTST_STRING, "",  // type
            RTST_STRING, "",  // ifname
            RTST_STRING, "",  // addresses
            RTST_STRING, "",  // dnses
            RTST_STRING, "",  // gateways
            RTST_STRING, "",  // pcscf
            RTST_INT32, "0",  // mtu
            RTST_INT32, "1");  // rat

    RTST_CASE_END();
}

TEST_F(DataConnectionTest, RIL_REQUEST_SETUP_DATA_CALL_IPV4_FAIL_8) {
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

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=1,\"internet\",\"default\",0", 3,
            "+CGEV: ME PDN ACT 1, 2",
            "+CGEV: ME PDN ACT 2, 2",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=1,\"ifst\",20", 2,
            "+EPDN:1, \"new\", 1, 0, 2454, 1, \"172.22.1.100\"",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=2,\"ifst\",20", 2,
            "+EPDN:1,\"err\",257",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=0,1", 2,
            "+CGEV: ME PDN DEACT 1",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=0,2", 2,
            "+CGEV: ME PDN DEACT 2",
            "OK");

    RTST_EXPECTED_RIL_RESPONSE(RIL_REQUEST_SETUP_DATA_CALL, RIL_E_GENERIC_FAILURE, 14,
            RTST_INT32, "11",  // ril data call response version
            RTST_INT32, "1",  // num
            RTST_INT32, "65535",  // status
            RTST_INT32, "-1",  // suggestedRetryTime
            RTST_INT32, "-1",  // cid, means interfaceId actually
            RTST_INT32, "0",  // active
            RTST_STRING, "",  // type
            RTST_STRING, "",  // ifname
            RTST_STRING, "",  // addresses
            RTST_STRING, "",  // dnses
            RTST_STRING, "",  // gateways
            RTST_STRING, "",  // pcscf
            RTST_INT32, "0",  // mtu
            RTST_INT32, "1");  // rat

    RTST_CASE_END();
}

TEST_F(DataConnectionTest, RIL_REQUEST_SETUP_DATA_CALL_IPV4_FAIL_9) {
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

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=1,\"internet\",\"default\",0", 3,
            "+CGEV: ME PDN ACT 1, 2",
            "+CGEV: ME PDN ACT 2, 2",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=1,\"ifst\",20", 2,
            "+EPDN: 1, \"new\", 1, 0, 2454, 1, \"172.22.1.100\"",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=2,\"ifst\",20", 2,
            "+EPDN: 2, \"new\", 1, 1, 2454, 1, \"182.23.6.500\"",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=0,1", 2,
            "+CGEV: ME PDN DEACT 1",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=0,2", 2,
            "+CGEV: ME PDN DEACT 2",
            "OK");

    RTST_EXPECTED_RIL_RESPONSE(RIL_REQUEST_SETUP_DATA_CALL, RIL_E_GENERIC_FAILURE, 14,
            RTST_INT32, "11",  // ril data call response version
            RTST_INT32, "1",  // num
            RTST_INT32, "65535",  // status
            RTST_INT32, "-1",  // suggestedRetryTime
            RTST_INT32, "-1",  // cid, means interfaceId actually
            RTST_INT32, "0",  // active
            RTST_STRING, "",  // type
            RTST_STRING, "",  // ifname
            RTST_STRING, "",  // addresses
            RTST_STRING, "",  // dnses
            RTST_STRING, "",  // gateways
            RTST_STRING, "",  // pcscf
            RTST_INT32, "0",  // mtu
            RTST_INT32, "1");  // rat

    RTST_CASE_END();
}

TEST_F(DataConnectionTest, RIL_REQUEST_SETUP_DATA_CALL_IPV4_FAIL_10) {
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

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=1,\"internet\",\"default\",0", 3,
            "+CGEV: ME PDN ACT 1, 2",
            "+CGEV: ME PDN ACT 2, 2",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=1,\"ifst\",20", 2,
            "+EPDN: 1, \"new\", 1, 0, 2454, 1, \"172.22.1.100\"",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=2,\"ifst\",20", 2,
            "+EPDN: 2, \"new\", 1, 0, 2454, 1, \"182.23.6.500\"",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP", 1,
            "+CME ERROR: 5842");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=0,1", 2,
            "+CGEV: ME PDN DEACT 1",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=0,2", 2,
            "+CGEV: ME PDN DEACT 2",
            "OK");

    RTST_EXPECTED_RIL_RESPONSE(RIL_REQUEST_SETUP_DATA_CALL, RIL_E_GENERIC_FAILURE, 14,
            RTST_INT32, "11",  // ril data call response version
            RTST_INT32, "1",  // num
            RTST_INT32, "65535",  // status
            RTST_INT32, "-1",  // suggestedRetryTime
            RTST_INT32, "-1",  // cid, means interfaceId actually
            RTST_INT32, "0",  // active
            RTST_STRING, "",  // type
            RTST_STRING, "",  // ifname
            RTST_STRING, "",  // addresses
            RTST_STRING, "",  // dnses
            RTST_STRING, "",  // gateways
            RTST_STRING, "",  // pcscf
            RTST_INT32, "0",  // mtu
            RTST_INT32, "1");  // rat

    RTST_CASE_END();
}

TEST_F(DataConnectionTest, RIL_REQUEST_LAST_DATA_CALL_FAIL_CAUSE_1) {
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

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=1,\"internet\",\"default\",0", 1,
            "+CME ERROR: 3361");

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EDRETRY=0,\"internet\"", 2,
            "+EDRETRY: 2,60",
            "OK");

    RTST_EXPECTED_RIL_RESPONSE(RIL_REQUEST_SETUP_DATA_CALL, RIL_E_GENERIC_FAILURE, 14,
            RTST_INT32, "11",  // ril data call response version
            RTST_INT32, "1",  // num
            RTST_INT32, "33",  // status
            RTST_INT32, "60000",  // suggestedRetryTime
            RTST_INT32, "-1",  // cid, means interfaceId actually
            RTST_INT32, "0",  // active
            RTST_STRING, "",  // type
            RTST_STRING, "",  // ifname
            RTST_STRING, "",  // addresses
            RTST_STRING, "",  // dnses
            RTST_STRING, "",  // gateways
            RTST_STRING, "",  // pcscf
            RTST_INT32, "0",  // mtu
            RTST_INT32, "1");  // rat

    RTST_RIL_VOID_REQUEST(RIL_REQUEST_LAST_DATA_CALL_FAIL_CAUSE);

    RTST_EXPECTED_RIL_RESPONSE(RIL_REQUEST_LAST_DATA_CALL_FAIL_CAUSE, RIL_E_SUCCESS, 1,
            RTST_INT32, "33");  // RIL_DataCallFailCause

    RTST_CASE_END();
}

TEST_F(DataConnectionTest, RIL_REQUEST_SETUP_DATA_CALL_IPV4_1) {
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

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=1,\"internet\",\"default\",0", 2,
            "+CGEV: ME PDN ACT 1, 2",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=1,\"ifst\",20", 2,
            "+EPDN:1, \"new\", 1, 0, 2454, 1, \"172.22.1.100\"",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP", 2,
            "+CGCONTRDP: 1,5,\"internet\",\"\",\"\",\"172.22.1.201\",\"\",\"\",\"\",0,0,2454,,,",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGACT?", 12,
            "+CGACT: 0,0",
            "+CGACT: 1,1",
            "+CGACT: 2,0",
            "+CGACT: 3,0",
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
            RTST_INT32, "0",  // cid, means interfaceId actually
            RTST_INT32, "2",  // active
            RTST_STRING, "IP",  // type
            RTST_STRING, "ccmni0",  // ifname
            RTST_STRING, "172.22.1.100",  // addresses
            RTST_STRING, "172.22.1.201",  // dnses
            RTST_STRING, "172.22.1.100",  // gateways
            RTST_STRING, "",  // pcscf
            RTST_INT32, "2454",  // mtu
            RTST_INT32, "1");  // rat

    RTST_CASE_END();
}

TEST_F(DataConnectionTest, RIL_REQUEST_SETUP_DATA_CALL_IPV4_2) {
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

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=1,\"internet\",\"default\",0", 2,
            "+CGEV: ME PDN ACT 3, 2",
            "OK");
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

TEST_F(DataConnectionTest, RIL_REQUEST_SETUP_DATA_CALL_IPV4_3) {
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

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=1,\"internet\",\"default\",0", 2,
            "+CGEV: ME PDN ACT 1, 2",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=1,\"ifst\",20", 2,
            "+EPDN:1, \"new\", 1, 1311, 2454, 1, \"172.22.1.100\"",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP", 2,
            "+CGCONTRDP: 1,5,\"internet\",\"\",\"\",\"172.22.1.201\",\"\",\"\",\"\",0,0,2454,,,",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGACT?", 12,
            "+CGACT: 0,0",
            "+CGACT: 1,1",
            "+CGACT: 2,0",
            "+CGACT: 3,0",
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
            RTST_INT32, "1311",  // cid, means interfaceId actually
            RTST_INT32, "2",  // active
            RTST_STRING, "IP",  // type
            RTST_STRING, "ccmni11",  // ifname
            RTST_STRING, "172.22.1.100",  // addresses
            RTST_STRING, "172.22.1.201",  // dnses
            RTST_STRING, "172.22.1.100",  // gateways
            RTST_STRING, "",  // pcscf
            RTST_INT32, "2454",  // mtu
            RTST_INT32, "1");  // rat

    RTST_CASE_END();
}

TEST_F(DataConnectionTest, RIL_REQUEST_SETUP_DATA_CALL_IPV4_4) {
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

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=1,\"internet\",\"default\",0", 2,
            "+CGEV: ME PDN ACT 1, 2",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=1,\"ifst\",20", 2,
            "+EPDN:1, \"new\", 1, 1311, 2454, 1, \"172.22.1.100\"",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP", 2,
            "+CGCONTRDP: 1,5,\"internet\",\"\",\"\",\"172.22.1.201\",\"\",\"\",\"\",0,0,2454,,,",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGACT?", 12,
            "+CGACT: 0,0",
            "+CGACT: 1,1",
            "+CGACT: 2,0",
            "+CGACT: 3,0",
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
            RTST_INT32, "1311",  // cid, means interfaceId actually
            RTST_INT32, "2",  // active
            RTST_STRING, "IP",  // type
            RTST_STRING, "ccmni11",  // ifname
            RTST_STRING, "172.22.1.100",  // addresses
            RTST_STRING, "172.22.1.201",  // dnses
            RTST_STRING, "172.22.1.100",  // gateways
            RTST_STRING, "",  // pcscf
            RTST_INT32, "2454",  // mtu
            RTST_INT32, "1");  // rat

    RTST_CASE_END();
}

TEST_F(DataConnectionTest, RIL_REQUEST_SETUP_DATA_CALL_IPV4_5) {
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

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=1,\"internet\",\"default\",0", 2,
            "+CGEV: ME PDN ACT 1, 2",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=1,\"ifst\",20", 2,
            "+EPDN:1, \"new\", 1, 1311, 2454, 1, \"172.22.1.100\"",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP", 2,
            "+CGCONTRDP: 1,5,\"internet\",\"\",\"\",\"172.22.1.201\",\"\",\"\",\"\",0,0,2454,,,",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGACT?", 12,
            "+CGACT: 0,0",
            "+CGACT: 1,1",
            "+CGACT: 2,0",
            "+CGACT: 3,0",
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
            RTST_INT32, "1311",  // cid, means interfaceId actually
            RTST_INT32, "2",  // active
            RTST_STRING, "IP",  // type
            RTST_STRING, "ccmni11",  // ifname
            RTST_STRING, "172.22.1.100",  // addresses
            RTST_STRING, "172.22.1.201",  // dnses
            RTST_STRING, "172.22.1.100",  // gateways
            RTST_STRING, "",  // pcscf
            RTST_INT32, "2454",  // mtu
            RTST_INT32, "1");  // rat

    RTST_CASE_END();
}

TEST_F(DataConnectionTest, RIL_REQUEST_SETUP_DATA_CALL_IPV6_1) {
    RTST_CASE_BEGIN();
    RTST_RIL_REQUEST(RIL_REQUEST_SETUP_DATA_CALL, 8,
            RTST_STRING, "14",  // radioType
            RTST_STRING, "0",  // profile
            RTST_STRING, "internet",  // apn
            RTST_STRING, "",  // username
            RTST_STRING, "",  // password
            RTST_STRING, "0",  // authType
            RTST_STRING, "IPV6",  // protocol
            RTST_STRING, "1");  // interfaceId, not used for 93 rild

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=1,\"internet\",\"default\",0", 2,
            "+CGEV: ME PDN ACT 1, 2",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=1,\"ifst\",20", 2,
            "+EPDN:1, \"new\", 1, 0, 2454, 2, \"38.7.251.144.32.14.171.78.0.0.0.81.202.39.147.1\"",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP", 2,
            "+CGCONTRDP: 1,5,\"internet\",\"\",\"\",\"35.1.251.144.32.14.171.78.0.0.0.81.202.39.83.25\",\"\",\"\",\"\",0,0,2454,,,",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGACT?", 12,
            "+CGACT: 0,0",
            "+CGACT: 1,1",
            "+CGACT: 2,0",
            "+CGACT: 3,0",
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
            RTST_INT32, "0",  // cid, means interfaceId actually
            RTST_INT32, "2",  // active
            RTST_STRING, "IPV6",  // type
            RTST_STRING, "ccmni0",  // ifname
            RTST_STRING, "2607:FB90:200E:AB4E:0000:0051:CA27:9301",  // addresses
            RTST_STRING, "2301:FB90:200E:AB4E:0000:0051:CA27:5319",  // dnses
            RTST_STRING, "::",  // gateways
            RTST_STRING, "",  // pcscf
            RTST_INT32, "2454",  // mtu
            RTST_INT32, "1");  // rat

    RTST_CASE_END();
}

TEST_F(DataConnectionTest, RIL_REQUEST_SETUP_DATA_CALL_IPV6_2) {
    RTST_CASE_BEGIN();
    RTST_RIL_REQUEST(RIL_REQUEST_SETUP_DATA_CALL, 8,
            RTST_STRING, "14",  // radioType
            RTST_STRING, "0",  // profile
            RTST_STRING, "internet",  // apn
            RTST_STRING, "",  // username
            RTST_STRING, "",  // password
            RTST_STRING, "0",  // authType
            RTST_STRING, "IPV6",  // protocol
            RTST_STRING, "1");  // interfaceId, not used for 93 rild

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=1,\"internet\",\"default\",0", 2,
            "+CGEV: ME PDN ACT 1, 2",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=1,\"ifst\",20", 2,
            "+EPDN:1, \"new\", 1, 0, 2454, 2, \"38.7.251.144.32.14.171.78.0.0.0.81.202.39.147.1/64\"",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP", 2,
            "+CGCONTRDP: 1,5,\"internet\",\"\",\"\",\"35.1.251.144.32.14.171.78.0.0.0.81.202.39.83.25\",\"\",\"\",\"\",0,0,2454,,,",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGACT?", 12,
            "+CGACT: 0,0",
            "+CGACT: 1,1",
            "+CGACT: 2,0",
            "+CGACT: 3,0",
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
            RTST_INT32, "0",  // cid, means interfaceId actually
            RTST_INT32, "2",  // active
            RTST_STRING, "IPV6",  // type
            RTST_STRING, "ccmni0",  // ifname
            RTST_STRING, "2607:FB90:200E:AB4E:0000:0051:CA27:9301/64",  // addresses
            RTST_STRING, "2301:FB90:200E:AB4E:0000:0051:CA27:5319",  // dnses
            RTST_STRING, "::",  // gateways
            RTST_STRING, "",  // pcscf
            RTST_INT32, "2454",  // mtu
            RTST_INT32, "1");  // rat

    RTST_CASE_END();
}

TEST_F(DataConnectionTest, RIL_REQUEST_SETUP_DATA_CALL_IPV4V6_1) {
    RTST_CASE_BEGIN();
    RTST_RIL_REQUEST(RIL_REQUEST_SETUP_DATA_CALL, 8,
            RTST_STRING, "14",  // radioType
            RTST_STRING, "0",  // profile
            RTST_STRING, "internet",  // apn
            RTST_STRING, "",  // username
            RTST_STRING, "",  // password
            RTST_STRING, "0",  // authType
            RTST_STRING, "IPV4V6",  // protocol
            RTST_STRING, "1");  // interfaceId, not used for 93 rild

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=1,\"internet\",\"default\",0", 2,
            "+CGEV: ME PDN ACT 1, 2",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=1,\"ifst\",20", 2,
            "+EPDN:1, \"new\", 1, 0, 2454, 3, \"172.22.1.100\", \"38.7.251.144.32.14.171.78.0.0.0.81.202.39.147.1/64\"",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP", 3,
            "+CGCONTRDP: 1,5,\"internet\",\"\",\"\",\"172.22.1.201\",\"\",\"\",\"\",0,0,2454,,,",
            "+CGCONTRDP: 1,5,\"internet\",\"\",\"\",\"35.1.251.144.32.14.171.78.0.0.0.81.202.39.83.25\",\"\",\"\",\"\",0,0,2454,,,",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGACT?", 12,
            "+CGACT: 0,0",
            "+CGACT: 1,1",
            "+CGACT: 2,0",
            "+CGACT: 3,0",
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
            RTST_INT32, "0",  // cid, means interfaceId actually
            RTST_INT32, "2",  // active
            RTST_STRING, "IPV4V6",  // type
            RTST_STRING, "ccmni0",  // ifname
            RTST_STRING, "172.22.1.100 2607:FB90:200E:AB4E:0000:0051:CA27:9301/64",  // addresses
            RTST_STRING, "172.22.1.201 2301:FB90:200E:AB4E:0000:0051:CA27:5319",  // dnses
            RTST_STRING, "172.22.1.100 ::",  // gateways
            RTST_STRING, "",  // pcscf
            RTST_INT32, "2454",  // mtu
            RTST_INT32, "1");  // rat

    RTST_CASE_END();
}

TEST_F(DataConnectionTest, RIL_REQUEST_SETUP_DATA_CALL_IPV4V6_FALLBACK_1) {
    RTST_CASE_BEGIN();
    RTST_RIL_REQUEST(RIL_REQUEST_SETUP_DATA_CALL, 8,
            RTST_STRING, "14",  // radioType
            RTST_STRING, "0",  // profile
            RTST_STRING, "internet",  // apn
            RTST_STRING, "",  // username
            RTST_STRING, "",  // password
            RTST_STRING, "0",  // authType
            RTST_STRING, "IPV4V6",  // protocol
            RTST_STRING, "1");  // interfaceId, not used for 93 rild

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=1,\"internet\",\"default\",0", 2,
            "+CGEV: ME PDN ACT 1, 2",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=1,\"ifst\",20", 2,
            "+EPDN:1, \"new\", 1, 0, 2454, 1, \"172.22.1.100\"",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP", 2,
            "+CGCONTRDP: 1,5,\"internet\",\"\",\"\",\"172.22.1.201\",\"\",\"\",\"\",0,0,2454,,,",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGACT?", 12,
            "+CGACT: 0,0",
            "+CGACT: 1,1",
            "+CGACT: 2,0",
            "+CGACT: 3,0",
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
            RTST_INT32, "0",  // cid, means interfaceId actually
            RTST_INT32, "2",  // active
            RTST_STRING, "IP",  // type
            RTST_STRING, "ccmni0",  // ifname
            RTST_STRING, "172.22.1.100",  // addresses
            RTST_STRING, "172.22.1.201",  // dnses
            RTST_STRING, "172.22.1.100",  // gateways
            RTST_STRING, "",  // pcscf
            RTST_INT32, "2454",  // mtu
            RTST_INT32, "1");  // rat

    RTST_URC_STRING("+CGEV: ME PDN ACT 1, 2, 4");

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=3,4", 1,
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=4,\"ifst\",4", 2,
            "+EPDN: 4, \"new\", 1, 0, 2454, 2, \"38.7.251.144.32.14.171.78.0.0.0.81.202.39.147.1/64\"",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP=4", 2,
            "+CGCONTRDP: 4,8,\"internet\",\"\",\"\",\"35.1.251.144.32.14.171.78.0.0.0.81.202.39.83.25\""
            ",\"\",\"\",\"\",0,0,2454,,,",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGACT?", 12,
            "+CGACT: 0,0",
            "+CGACT: 1,1",
            "+CGACT: 2,0",
            "+CGACT: 3,0",
            "+CGACT: 4,1",
            "+CGACT: 5,0",
            "+CGACT: 6,0",
            "+CGACT: 7,0",
            "+CGACT: 8,0",
            "+CGACT: 9,0",
            "+CGACT: 10,0",
            "OK");

    RTST_EXPECTED_RIL_URC(RIL_UNSOL_DATA_CALL_LIST_CHANGED, 14,
            RTST_INT32, "11",  // ril data call response version
            RTST_INT32, "1",  // num
            RTST_INT32, "0",  // status
            RTST_INT32, "-1",  // suggestedRetryTime
            RTST_INT32, "0",  // cid, means interfaceId actually
            RTST_INT32, "2",  // active
            RTST_STRING, "IPV4V6",  // type
            RTST_STRING, "ccmni0",  // ifname
            RTST_STRING, "172.22.1.100 2607:FB90:200E:AB4E:0000:0051:CA27:9301/64",  // addresses
            RTST_STRING, "172.22.1.201 2301:FB90:200E:AB4E:0000:0051:CA27:5319",  // dnses
            RTST_STRING, "172.22.1.100 ::",  // gateways
            RTST_STRING, "",  // pcscf
            RTST_INT32, "2454",  // mtu
            RTST_INT32, "1");  // rat

    RTST_CASE_END();
}

TEST_F(DataConnectionTest, RIL_REQUEST_DEACTIVATE_DATA_CALL_1) {
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

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=1,\"internet\",\"default\",0", 2,
            "+CGEV: ME PDN ACT 1, 2",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=1,\"ifst\",20", 2,
            "+EPDN:1, \"new\", 1, 0, 2454, 1, \"172.22.1.100\"",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP", 2,
            "+CGCONTRDP: 1,5,\"internet\",\"\",\"\",\"172.22.1.201\",\"\",\"\",\"\",0,0,2454,,,",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGACT?", 12,
            "+CGACT: 0,0",
            "+CGACT: 1,1",
            "+CGACT: 2,0",
            "+CGACT: 3,0",
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
            RTST_INT32, "0",  // cid, means interfaceId actually
            RTST_INT32, "2",  // active
            RTST_STRING, "IP",  // type
            RTST_STRING, "ccmni0",  // ifname
            RTST_STRING, "172.22.1.100",  // addresses
            RTST_STRING, "172.22.1.201",  // dnses
            RTST_STRING, "172.22.1.100",  // gateways
            RTST_STRING, "",  // pcscf
            RTST_INT32, "2454",  // mtu
            RTST_INT32, "1");  // rat

    RTST_RIL_REQUEST(RIL_REQUEST_DEACTIVATE_DATA_CALL, 2,
            RTST_STRING, "0",  // cid, means interfaceId actually
            RTST_STRING, "0");  // reason

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=0,1", 2,
            "+CGEV: ME PDN DEACT 1",
            "OK");

    RTST_EXPECTED_RIL_VOID_RESPONSE(RIL_REQUEST_DEACTIVATE_DATA_CALL, RIL_E_SUCCESS);

    RTST_CASE_END();
}

TEST_F(DataConnectionTest, RIL_REQUEST_DEACTIVATE_DATA_CALL_2) {
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

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=1,\"internet\",\"default\",0", 2,
            "+CGEV: ME PDN ACT 3, 2",
            "OK");
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

    RTST_RIL_REQUEST(RIL_REQUEST_DEACTIVATE_DATA_CALL, 2,
            RTST_STRING, "1412",  // cid, means interfaceId actually
            RTST_STRING, "0");  // reason

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=0,3", 2,
            "+CGEV: ME PDN DEACT 3",
            "OK");

    RTST_EXPECTED_RIL_VOID_RESPONSE(RIL_REQUEST_DEACTIVATE_DATA_CALL, RIL_E_SUCCESS);

    RTST_CASE_END();
}

TEST_F(DataConnectionTest, RIL_REQUEST_DATA_CALL_LIST_1) {
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

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=1,\"internet\",\"default\",0", 2,
            "+CGEV: ME PDN ACT 1, 2",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=1,\"ifst\",20", 2,
            "+EPDN:1, \"new\", 1, 1311, 2454, 1, \"172.22.1.100\"",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP", 2,
            "+CGCONTRDP: 1,5,\"internet\",\"\",\"\",\"172.22.1.201\",\"\",\"\",\"\",0,0,2454,,,",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGACT?", 12,
            "+CGACT: 0,0",
            "+CGACT: 1,1",
            "+CGACT: 2,0",
            "+CGACT: 3,0",
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
            RTST_INT32, "1311",  // cid, means interfaceId actually
            RTST_INT32, "2",  // active
            RTST_STRING, "IP",  // type
            RTST_STRING, "ccmni11",  // ifname
            RTST_STRING, "172.22.1.100",  // addresses
            RTST_STRING, "172.22.1.201",  // dnses
            RTST_STRING, "172.22.1.100",  // gateways
            RTST_STRING, "",  // pcscf
            RTST_INT32, "2454",  // mtu
            RTST_INT32, "1");  // rat

    RTST_RIL_VOID_REQUEST(RIL_REQUEST_DATA_CALL_LIST);

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGACT?", 12,
            "+CGACT: 0,0",
            "+CGACT: 1,1",
            "+CGACT: 2,0",
            "+CGACT: 3,0",
            "+CGACT: 4,0",
            "+CGACT: 5,0",
            "+CGACT: 6,0",
            "+CGACT: 7,0",
            "+CGACT: 8,0",
            "+CGACT: 9,0",
            "+CGACT: 10,0",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=0,\"addr\"", 1,
            "+CME ERROR: 5842");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=1,\"addr\"", 2,
            "+EPDN: 1,\"addr\",1,\"172.22.1.100\"",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=2,\"addr\"", 1,
            "+CME ERROR: 5842");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=3,\"addr\"", 1,
            "+CME ERROR: 5842");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=4,\"addr\"", 1,
            "+CME ERROR: 5842");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=5,\"addr\"", 2,
            "+EPDN: 5,\"err\",5842",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=6,\"addr\"", 2,
            "+EPDN: 6,\"err\",5842",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=7,\"addr\"", 2,
            "+EPDN: 7,\"err\",5842",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=8,\"addr\"", 2,
            "+EPDN: 8,\"err\",5842",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=9,\"addr\"", 2,
            "+EPDN: 9,\"addr\",0",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=10,\"addr\"", 2,
            "+EPDN: 10,\"addr\",0",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP", 2,
            "+CGCONTRDP: 1,5,\"internet\",\"\",\"\",\"172.22.1.201\",\"\",\"\",\"\",0,0,2454,,,",
            "OK");

    RTST_EXPECTED_RIL_RESPONSE(RIL_REQUEST_DATA_CALL_LIST, RIL_E_SUCCESS, 14,
            RTST_INT32, "11",  // ril data call response version
            RTST_INT32, "1",  // num
            RTST_INT32, "0",  // status
            RTST_INT32, "-1",  // suggestedRetryTime
            RTST_INT32, "1311",  // cid, means interfaceId actually
            RTST_INT32, "2",  // active
            RTST_STRING, "IP",  // type
            RTST_STRING, "ccmni11",  // ifname
            RTST_STRING, "172.22.1.100",  // addresses
            RTST_STRING, "172.22.1.201",  // dnses
            RTST_STRING, "172.22.1.100",  // gateways
            RTST_STRING, "",  // pcscf
            RTST_INT32, "2454",  // mtu
            RTST_INT32, "1");  // rat

    RTST_CASE_END();
}

TEST_F(DataConnectionTest, RIL_REQUEST_DATA_CALL_LIST_2) {
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

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=1,\"internet\",\"default\",0", 2,
            "+CGEV: ME PDN ACT 1, 2",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=1,\"ifst\",20", 2,
            "+EPDN:1, \"new\", 1, 1311, 2454, 1, \"172.22.1.100\"",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP", 2,
            "+CGCONTRDP: 1,5,\"internet\",\"\",\"\",\"172.22.1.201\",\"\",\"\",\"\",0,0,2454,,,",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGACT?", 12,
            "+CGACT: 0,0",
            "+CGACT: 1,1",
            "+CGACT: 2,0",
            "+CGACT: 3,0",
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
            RTST_INT32, "1311",  // cid, means interfaceId actually
            RTST_INT32, "2",  // active
            RTST_STRING, "IP",  // type
            RTST_STRING, "ccmni11",  // ifname
            RTST_STRING, "172.22.1.100",  // addresses
            RTST_STRING, "172.22.1.201",  // dnses
            RTST_STRING, "172.22.1.100",  // gateways
            RTST_STRING, "",  // pcscf
            RTST_INT32, "2454",  // mtu
            RTST_INT32, "1");  // rat

    RTST_RIL_VOID_REQUEST(RIL_REQUEST_DATA_CALL_LIST);

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGACT?", 12,
            "+CGACT: 0,0",
            "+CGACT: 1,1",
            "+CGACT: 2,0",
            "+CGACT: 3,0",
            "+CGACT: 4,0",
            "+CGACT: 5,0",
            "+CGACT: 6,0",
            "+CGACT: 7,0",
            "+CGACT: 8,0",
            "+CGACT: 9,0",
            "+CGACT: 10,0",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=0,\"addr\"", 2,
            "+EPDN: 0,\"addr\",0",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=1,\"addr\"", 2,
            "+EPDN: 1,\"addr\",1,\"172.22.1.100\"",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=2,\"addr\"", 2,
            "+EPDN: 2,\"addr\",0",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=3,\"addr\"", 2,
            "+EPDN: 3,\"addr\",0",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=4,\"addr\"", 2,
            "+EPDN: 4,\"addr\",0",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=5,\"addr\"", 2,
            "+EPDN: 5,\"addr\",0",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=6,\"addr\"", 2,
            "+EPDN: 6,\"addr\",0",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=7,\"addr\"", 2,
            "+EPDN: 7,\"addr\",0",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=8,\"addr\"", 2,
            "+EPDN: 8,\"addr\",0",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=9,\"addr\"", 2,
            "+EPDN: 9,\"addr\",0",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=10,\"addr\"", 2,
            "+EPDN: 10,\"addr\",0",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP", 2,
            "+CGCONTRDP: 1,5,\"internet\",\"\",\"\",\"172.22.1.201\",\"\",\"\",\"\",0,0,2454,,,",
            "OK");

    RTST_EXPECTED_RIL_RESPONSE(RIL_REQUEST_DATA_CALL_LIST, RIL_E_SUCCESS, 14,
            RTST_INT32, "11",  // ril data call response version
            RTST_INT32, "1",  // num
            RTST_INT32, "0",  // status
            RTST_INT32, "-1",  // suggestedRetryTime
            RTST_INT32, "1311",  // cid, means interfaceId actually
            RTST_INT32, "2",  // active
            RTST_STRING, "IP",  // type
            RTST_STRING, "ccmni11",  // ifname
            RTST_STRING, "172.22.1.100",  // addresses
            RTST_STRING, "172.22.1.201",  // dnses
            RTST_STRING, "172.22.1.100",  // gateways
            RTST_STRING, "",  // pcscf
            RTST_INT32, "2454",  // mtu
            RTST_INT32, "1");  // rat

    RTST_CASE_END();
}


TEST_F(DataConnectionTest, RFX_MSG_EVENT_DATA_PDN_DEACT_1) {
    RTST_CASE_BEGIN();
    RTST_URC_STRING("+CGEV: NW PDN DEACT 1");

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=1,\"deact_info\"", 2,
            "+EPDN:1,\"deact_info\",0,0",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=2,1", 1,
            "OK");
    RTST_EXPECTED_RIL_VOID_URC(RIL_UNSOL_DATA_CALL_LIST_CHANGED);

    RTST_CASE_END();
}

TEST_F(DataConnectionTest, RFX_MSG_EVENT_DATA_PDN_DEACT_2) {
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

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=1,\"internet\",\"default\",0", 2,
            "+CGEV: ME PDN ACT 1, 2",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=1,\"ifst\",20", 2,
            "+EPDN:1, \"new\", 1, 1311, 2454, 1, \"172.22.1.100\"",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP", 2,
            "+CGCONTRDP: 1,5,\"internet\",\"\",\"\",\"172.22.1.201\",\"\",\"\",\"\",0,0,2454,,,",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGACT?", 12,
            "+CGACT: 0,0",
            "+CGACT: 1,1",
            "+CGACT: 2,0",
            "+CGACT: 3,0",
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
            RTST_INT32, "1311",  // cid, means interfaceId actually
            RTST_INT32, "2",  // active
            RTST_STRING, "IP",  // type
            RTST_STRING, "ccmni11",  // ifname
            RTST_STRING, "172.22.1.100",  // addresses
            RTST_STRING, "172.22.1.201",  // dnses
            RTST_STRING, "172.22.1.100",  // gateways
            RTST_STRING, "",  // pcscf
            RTST_INT32, "2454",  // mtu
            RTST_INT32, "1");  // rat

    RTST_URC_STRING("+CGEV: NW PDN DEACT 1");

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=1,\"deact_info\"", 2,
            "+EPDN:1,\"err\",5872",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=2,1", 1,
            "OK");
    RTST_EXPECTED_RIL_VOID_URC(RIL_UNSOL_DATA_CALL_LIST_CHANGED);

    RTST_CASE_END();
}

TEST_F(DataConnectionTest, RFX_MSG_EVENT_DATA_PDN_DEACT_3) {
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

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=1,\"internet\",\"default\",0", 2,
            "+CGEV: ME PDN ACT 1, 2",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=1,\"ifst\",20", 2,
            "+EPDN:1, \"new\", 1, 1311, 2454, 1, \"172.22.1.100\"",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP", 2,
            "+CGCONTRDP: 1,5,\"internet\",\"\",\"\",\"172.22.1.201\",\"\",\"\",\"\",0,0,2454,,,",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGACT?", 12,
            "+CGACT: 0,0",
            "+CGACT: 1,1",
            "+CGACT: 2,0",
            "+CGACT: 3,0",
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
            RTST_INT32, "1311",  // cid, means interfaceId actually
            RTST_INT32, "2",  // active
            RTST_STRING, "IP",  // type
            RTST_STRING, "ccmni11",  // ifname
            RTST_STRING, "172.22.1.100",  // addresses
            RTST_STRING, "172.22.1.201",  // dnses
            RTST_STRING, "172.22.1.100",  // gateways
            RTST_STRING, "",  // pcscf
            RTST_INT32, "2454",  // mtu
            RTST_INT32, "1");  // rat

    RTST_URC_STRING("+CGEV: NW PDN DEACT 1");

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=1,\"deact_info\"", 1,
            "+CME ERROR: 5842");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=2,1", 1,
            "OK");
    RTST_EXPECTED_RIL_VOID_URC(RIL_UNSOL_DATA_CALL_LIST_CHANGED);

    RTST_CASE_END();
}

TEST_F(DataConnectionTest, RFX_MSG_EVENT_DATA_PDN_DEACT_4) {
    RTST_CASE_BEGIN();
    RTST_RIL_REQUEST(RIL_REQUEST_SETUP_DATA_CALL, 8,
            RTST_STRING, "14",  // radioType
            RTST_STRING, "0",  // profile
            RTST_STRING, "internet",  // apn
            RTST_STRING, "",  // username
            RTST_STRING, "",  // password
            RTST_STRING, "0",  // authType
            RTST_STRING, "IPV4V6",  // protocol
            RTST_STRING, "1");  // interfaceId, not used for 93 rild

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=1,\"internet\",\"default\",0", 2,
            "+CGEV: ME PDN ACT 1, 2",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=1,\"ifst\",20", 2,
            "+EPDN:1, \"new\", 1, 0, 2454, 1, \"172.22.1.100\"",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP", 2,
            "+CGCONTRDP: 1,5,\"internet\",\"\",\"\",\"172.22.1.201\",\"\",\"\",\"\",0,0,2454,,,",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGACT?", 12,
            "+CGACT: 0,0",
            "+CGACT: 1,1",
            "+CGACT: 2,0",
            "+CGACT: 3,0",
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
            RTST_INT32, "0",  // cid, means interfaceId actually
            RTST_INT32, "2",  // active
            RTST_STRING, "IP",  // type
            RTST_STRING, "ccmni0",  // ifname
            RTST_STRING, "172.22.1.100",  // addresses
            RTST_STRING, "172.22.1.201",  // dnses
            RTST_STRING, "172.22.1.100",  // gateways
            RTST_STRING, "",  // pcscf
            RTST_INT32, "2454",  // mtu
            RTST_INT32, "1");  // rat

    RTST_URC_STRING("+CGEV: ME PDN ACT 1, 2, 4");

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=3,4", 1,
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=4,\"ifst\",4", 2,
            "+EPDN: 4, \"new\", 1, 0, 2454, 2, \"38.7.251.144.32.14.171.78.0.0.0.81.202.39.147.1/64\"",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP=4", 2,
            "+CGCONTRDP: 4,8,\"internet\",\"\",\"\",\"35.1.251.144.32.14.171.78.0.0.0.81.202.39.83.25\""
            ",\"\",\"\",\"\",0,0,2454,,,",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGACT?", 12,
            "+CGACT: 0,0",
            "+CGACT: 1,1",
            "+CGACT: 2,0",
            "+CGACT: 3,0",
            "+CGACT: 4,1",
            "+CGACT: 5,0",
            "+CGACT: 6,0",
            "+CGACT: 7,0",
            "+CGACT: 8,0",
            "+CGACT: 9,0",
            "+CGACT: 10,0",
            "OK");

    RTST_EXPECTED_RIL_URC(RIL_UNSOL_DATA_CALL_LIST_CHANGED, 14,
            RTST_INT32, "11",  // ril data call response version
            RTST_INT32, "1",  // num
            RTST_INT32, "0",  // status
            RTST_INT32, "-1",  // suggestedRetryTime
            RTST_INT32, "0",  // cid, means interfaceId actually
            RTST_INT32, "2",  // active
            RTST_STRING, "IPV4V6",  // type
            RTST_STRING, "ccmni0",  // ifname
            RTST_STRING, "172.22.1.100 2607:FB90:200E:AB4E:0000:0051:CA27:9301/64",  // addresses
            RTST_STRING, "172.22.1.201 2301:FB90:200E:AB4E:0000:0051:CA27:5319",  // dnses
            RTST_STRING, "172.22.1.100 ::",  // gateways
            RTST_STRING, "",  // pcscf
            RTST_INT32, "2454",  // mtu
            RTST_INT32, "1");  // rat

    RTST_URC_STRING("+CGEV: NW PDN DEACT 1");

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=1,\"deact_info\"", 2,
            "+EPDN:1,\"deact_info\",36,2454",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=2,1", 1,
            "OK");
    RTST_EXPECTED_RIL_URC(RIL_UNSOL_DATA_CALL_LIST_CHANGED, 14,
            RTST_INT32, "11",  // ril data call response version
            RTST_INT32, "1",  // num
            RTST_INT32, "0",  // status
            RTST_INT32, "-1",  // suggestedRetryTime
            RTST_INT32, "0",  // cid, means interfaceId actually
            RTST_INT32, "2",  // active
            RTST_STRING, "IPV6",  // type
            RTST_STRING, "ccmni0",  // ifname
            RTST_STRING, "2607:FB90:200E:AB4E:0000:0051:CA27:9301/64",  // addresses
            RTST_STRING, "2301:FB90:200E:AB4E:0000:0051:CA27:5319",  // dnses
            RTST_STRING, "::",  // gateways
            RTST_STRING, "",  // pcscf
            RTST_INT32, "2454",  // mtu
            RTST_INT32, "1");  // rat

    RTST_CASE_END();
}

TEST_F(DataConnectionTest, RFX_MSG_EVENT_DATA_PDN_ACT_1) {
    RTST_CASE_BEGIN();
    RTST_URC_STRING("+CGEV: NW PDN ACT 1");

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=2,1", 1,
            "OK");

    RTST_CASE_END();
}

TEST_F(DataConnectionTest, RFX_MSG_EVENT_DATA_ME_PDN_IP_CHANGE_1) {
    RTST_CASE_BEGIN();
    RTST_RIL_REQUEST(RIL_REQUEST_SETUP_DATA_CALL, 8,
            RTST_STRING, "14",  // radioType
            RTST_STRING, "0",  // profile
            RTST_STRING, "internet",  // apn
            RTST_STRING, "",  // username
            RTST_STRING, "",  // password
            RTST_STRING, "0",  // authType
            RTST_STRING, "IPV6",  // protocol
            RTST_STRING, "1");  // interfaceId, not used for 93 rild

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=1,\"internet\",\"default\",0", 2,
            "+CGEV: ME PDN ACT 1, 2",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=1,\"ifst\",20", 2,
            "+EPDN:1, \"new\", 1, 0, 2454, 2, \"38.7.251.144.32.14.171.78.0.0.0.81.202.39.147.1\"",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP", 2,
            "+CGCONTRDP: 1,5,\"internet\",\"\",\"\",\"35.1.251.144.32.14.171.78.0.0.0.81.202.39.83.25\",\"\",\"\",\"\",0,0,2454,,,",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGACT?", 12,
            "+CGACT: 0,0",
            "+CGACT: 1,1",
            "+CGACT: 2,0",
            "+CGACT: 3,0",
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
            RTST_INT32, "0",  // cid, means interfaceId actually
            RTST_INT32, "2",  // active
            RTST_STRING, "IPV6",  // type
            RTST_STRING, "ccmni0",  // ifname
            RTST_STRING, "2607:FB90:200E:AB4E:0000:0051:CA27:9301",  // addresses
            RTST_STRING, "2301:FB90:200E:AB4E:0000:0051:CA27:5319",  // dnses
            RTST_STRING, "::",  // gateways
            RTST_STRING, "",  // pcscf
            RTST_INT32, "2454",  // mtu
            RTST_INT32, "1");  // rat

    RTST_URC_STRING("+EPDN: 1,\"dcchg\",257");  // no ra event

    RTST_EXPECTED_RIL_URC(RIL_UNSOL_DATA_CALL_LIST_CHANGED, 14,
            RTST_INT32, "11",  // ril data call response version
            RTST_INT32, "1",  // num
            RTST_INT32, "0",  // status
            RTST_INT32, "-1",  // suggestedRetryTime
            RTST_INT32, "0",  // cid, means interfaceId actually
            RTST_INT32, "2",  // active
            RTST_STRING, "IP",  // type
            RTST_STRING, "ccmni0",  // ifname
            RTST_STRING, "",  // addresses
            RTST_STRING, "",  // dnses
            RTST_STRING, "",  // gateways
            RTST_STRING, "",  // pcscf
            RTST_INT32, "2454",  // mtu
            RTST_INT32, "1");  // rat

    RTST_CASE_END();
}

TEST_F(DataConnectionTest, RFX_MSG_EVENT_DATA_ME_PDN_IP_CHANGE_2) {
    RTST_CASE_BEGIN();
    RTST_RIL_REQUEST(RIL_REQUEST_SETUP_DATA_CALL, 8,
            RTST_STRING, "14",  // radioType
            RTST_STRING, "0",  // profile
            RTST_STRING, "internet",  // apn
            RTST_STRING, "",  // username
            RTST_STRING, "",  // password
            RTST_STRING, "0",  // authType
            RTST_STRING, "IPV4V6",  // protocol
            RTST_STRING, "1");  // interfaceId, not used for 93 rild

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=1,\"internet\",\"default\",0", 2,
            "+CGEV: ME PDN ACT 1, 2",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=1,\"ifst\",20", 2,
            "+EPDN:1, \"new\", 1, 0, 2454, 3, \"172.22.1.100\", \"38.7.251.144.32.14.171.78.0.0.0.81.202.39.147.1/64\"",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP", 3,
            "+CGCONTRDP: 1,5,\"internet\",\"\",\"\",\"172.22.1.201\",\"\",\"\",\"\",0,0,2454,,,",
            "+CGCONTRDP: 1,5,\"internet\",\"\",\"\",\"35.1.251.144.32.14.171.78.0.0.0.81.202.39.83.25\",\"\",\"\",\"\",0,0,2454,,,",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGACT?", 12,
            "+CGACT: 0,0",
            "+CGACT: 1,1",
            "+CGACT: 2,0",
            "+CGACT: 3,0",
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
            RTST_INT32, "0",  // cid, means interfaceId actually
            RTST_INT32, "2",  // active
            RTST_STRING, "IPV4V6",  // type
            RTST_STRING, "ccmni0",  // ifname
            RTST_STRING, "172.22.1.100 2607:FB90:200E:AB4E:0000:0051:CA27:9301/64",  // addresses
            RTST_STRING, "172.22.1.201 2301:FB90:200E:AB4E:0000:0051:CA27:5319",  // dnses
            RTST_STRING, "172.22.1.100 ::",  // gateways
            RTST_STRING, "",  // pcscf
            RTST_INT32, "2454",  // mtu
            RTST_INT32, "1");  // rat


    RTST_URC_STRING("+EPDN: 1,\"dcchg\",257");  // no ra event

    RTST_EXPECTED_RIL_URC(RIL_UNSOL_DATA_CALL_LIST_CHANGED, 14,
            RTST_INT32, "11",  // ril data call response version
            RTST_INT32, "1",  // num
            RTST_INT32, "0",  // status
            RTST_INT32, "-1",  // suggestedRetryTime
            RTST_INT32, "0",  // cid, means interfaceId actually
            RTST_INT32, "2",  // active
            RTST_STRING, "IP",  // type
            RTST_STRING, "ccmni0",  // ifname
            RTST_STRING, "172.22.1.100",  // addresses
            RTST_STRING, "172.22.1.201",  // dnses
            RTST_STRING, "172.22.1.100",  // gateways
            RTST_STRING, "",  // pcscf
            RTST_INT32, "2454",  // mtu
            RTST_INT32, "1");  // rat

    RTST_CASE_END();
}

// setup data for IPV4V6, check if dnsv6 is preferred first for special networks
TEST_F(DataConnectionTest, PREFER_DNSV6_FIRST_1) {
    RTST_CASE_BEGIN();

    rfx_property_set("gsm.operator.numeric", "311480");

    RTST_RIL_REQUEST(RIL_REQUEST_SETUP_DATA_CALL, 8,
            RTST_STRING, "14",  // radioType
            RTST_STRING, "0",  // profile
            RTST_STRING, "internet",  // apn
            RTST_STRING, "",  // username
            RTST_STRING, "",  // password
            RTST_STRING, "0",  // authType
            RTST_STRING, "IPV4V6",  // protocol
            RTST_STRING, "1");  // interfaceId, not used for 93 rild

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=1,\"internet\",\"default\",0", 2,
            "+CGEV: ME PDN ACT 1, 2",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=1,\"ifst\",20", 2,
            "+EPDN:1, \"new\", 1, 0, 2454, 3, \"172.22.1.100\", \"38.7.251.144.32.14.171.78.0.0.0.81.202.39.147.1/64\"",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP", 3,
            "+CGCONTRDP: 1,5,\"internet\",\"\",\"\",\"172.22.1.201\",\"\",\"\",\"\",0,0,2454,,,",
            "+CGCONTRDP: 1,5,\"internet\",\"\",\"\",\"35.1.251.144.32.14.171.78.0.0.0.81.202.39.83.25\",\"\",\"\",\"\",0,0,2454,,,",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGACT?", 12,
            "+CGACT: 0,0",
            "+CGACT: 1,1",
            "+CGACT: 2,0",
            "+CGACT: 3,0",
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
            RTST_INT32, "0",  // cid, means interfaceId actually
            RTST_INT32, "2",  // active
            RTST_STRING, "IPV4V6",  // type
            RTST_STRING, "ccmni0",  // ifname
            RTST_STRING, "172.22.1.100 2607:FB90:200E:AB4E:0000:0051:CA27:9301/64",  // addresses
            RTST_STRING, "2301:FB90:200E:AB4E:0000:0051:CA27:5319 172.22.1.201",  // dnses
            RTST_STRING, "172.22.1.100 ::",  // gateways
            RTST_STRING, "",  // pcscf
            RTST_INT32, "2454",  // mtu
            RTST_INT32, "1");  // rat

    RTST_CASE_END();
}

// setup data for IPV4, verify PCO results
TEST_F(DataConnectionTest, QUERY_PCO_1) {
    RTST_CASE_BEGIN();
    pcoPropertySetup();

    setupDataCall("IP", "IP");

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EGPCORDP=1,\"FF00\"", 2,
            "+EGPCORDP: 1,\"FF00\",\"311480:3\"",
            "OK");

    usleep(250e3);
    RTST_EXPECTED_STATUS_VALUE_WITH_DELAY(RFX_SLOT_ID_0, RFX_STATUS_KEY_PCO_STATUS,
              RfxVariant(String8("FF00:3")), 0);

    RTST_EXPECTED_RIL_URC(RIL_UNSOL_PCO_DATA, 5,
            RTST_INT32, "0", // interface id
            RTST_STRING, "IP", // bearer_proto
            RTST_INT32, "65280", // pco_id (FF00)
            RTST_INT32, "1", // contents_length
            RTST_STRING, "3"); // contents

    RTST_STATUS_VALUE(RFX_SLOT_ID_0, RFX_STATUS_KEY_PCO_STATUS, RfxVariant(String8("")));
    RTST_CASE_END();
}

// setup data for IPV4V6, verify PCO results
TEST_F(DataConnectionTest, QUERY_PCO_2) {
    RTST_CASE_BEGIN();
    pcoPropertySetup();

    setupDataCall("IPV4V6", "IPV4V6");

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EGPCORDP=1,\"FF00\"", 2,
            "+EGPCORDP: 1,\"FF00\",\"311480:3\"",
            "OK");

    usleep(250e3);
    RTST_EXPECTED_STATUS_VALUE_WITH_DELAY(RFX_SLOT_ID_0, RFX_STATUS_KEY_PCO_STATUS,
              RfxVariant(String8("FF00:3")), 0);

    RTST_EXPECTED_RIL_URC(RIL_UNSOL_PCO_DATA, 5,
            RTST_INT32, "0", // interface id
            RTST_STRING, "IPV4V6", // bearer_proto
            RTST_INT32, "65280", // pco_id (FF00)
            RTST_INT32, "1", // contents_length
            RTST_STRING, "3"); // contents

    RTST_STATUS_VALUE(RFX_SLOT_ID_0, RFX_STATUS_KEY_PCO_STATUS, RfxVariant(String8("")));
    RTST_CASE_END();
}

// URC +CGEV: ME MODIFY trigger PCO, verify PCO results
TEST_F(DataConnectionTest, QUERY_PCO_3) {
    RTST_CASE_BEGIN();
    pcoPropertySetup();

    setupDataCall("IP", "IP");

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EGPCORDP=1,\"FF00\"", 2,
            "+EGPCORDP: 1,\"FF00\",\"311480:3\"",
            "OK");

    RTST_EXPECTED_RIL_URC(RIL_UNSOL_PCO_DATA, 5,
            RTST_INT32, "0", // interface id
            RTST_STRING, "IP", // bearer_proto
            RTST_INT32, "65280", // pco_id (FF00)
            RTST_INT32, "1", // contents_length
            RTST_STRING, "3"); // contents

    RTST_URC_STRING("+CGEV: ME MODIFY 1");

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EGPCORDP=1,\"FF00\"", 2,
            "+EGPCORDP: 1,\"FF00\",\"311480:3\"",
            "OK");

    usleep(250e3);
    RTST_EXPECTED_STATUS_VALUE_WITH_DELAY(RFX_SLOT_ID_0, RFX_STATUS_KEY_PCO_STATUS,
              RfxVariant(String8("FF00:3")), 0);

    RTST_EXPECTED_RIL_URC(RIL_UNSOL_PCO_DATA, 5,
            RTST_INT32, "0", // interface id
            RTST_STRING, "IP", // bearer_proto
            RTST_INT32, "65280", // pco_id (FF00)
            RTST_INT32, "1", // contents_length
            RTST_STRING, "3"); // contents

    RTST_STATUS_VALUE(RFX_SLOT_ID_0, RFX_STATUS_KEY_PCO_STATUS, RfxVariant(String8("")));
    RTST_CASE_END();
}

// URC +CGEV: NW MODIFY trigger PCO, verify PCO results
TEST_F(DataConnectionTest, QUERY_PCO_4) {
    RTST_CASE_BEGIN();
    pcoPropertySetup();

    setupDataCall("IP", "IP");

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EGPCORDP=1,\"FF00\"", 2,
            "+EGPCORDP: 1,\"FF00\",\"311480:3\"",
            "OK");

    usleep(250e3);
    RTST_EXPECTED_STATUS_VALUE_WITH_DELAY(RFX_SLOT_ID_0, RFX_STATUS_KEY_PCO_STATUS,
              RfxVariant(String8("FF00:3")), 0);

    RTST_EXPECTED_RIL_URC(RIL_UNSOL_PCO_DATA, 5,
            RTST_INT32, "0", // interface id
            RTST_STRING, "IP", // bearer_proto
            RTST_INT32, "65280", // pco_id (FF00)
            RTST_INT32, "1", // contents_length
            RTST_STRING, "3"); // contents

    RTST_URC_STRING("+CGEV: NW MODIFY 1, 2, 0");

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGACT?", 12,
            "+CGACT: 0,0",
            "+CGACT: 1,1",
            "+CGACT: 2,0",
            "+CGACT: 3,0",
            "+CGACT: 4,0",
            "+CGACT: 5,0",
            "+CGACT: 6,0",
            "+CGACT: 7,0",
            "+CGACT: 8,0",
            "+CGACT: 9,0",
            "+CGACT: 10,0",
            "OK");

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGACT?", 12,
            "+CGACT: 0,0",
            "+CGACT: 1,1",
            "+CGACT: 2,0",
            "+CGACT: 3,0",
            "+CGACT: 4,0",
            "+CGACT: 5,0",
            "+CGACT: 6,0",
            "+CGACT: 7,0",
            "+CGACT: 8,0",
            "+CGACT: 9,0",
            "+CGACT: 10,0",
            "OK");

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=1,\"addr\"", 2,
            "+EPDN:1,\"addr\",0,\"172.22.1.100\"",
            "OK");

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP=1", 2,
            "+CGCONTRDP: 1,5,\"internet\",\"\",\"\",\"172.22.1.201\",\"\",\"\",\"\",0,0,2454,,,",
            "OK");

    RTST_EXPECTED_RIL_URC(RIL_UNSOL_DATA_CALL_LIST_CHANGED, 14,
            RTST_INT32, "11",  // ril data call response version
            RTST_INT32, "1",  // num
            RTST_INT32, "0",  // status
            RTST_INT32, "-1",  // suggestedRetryTime
            RTST_INT32, "0",  // cid, means interfaceId actually
            RTST_INT32, "2",  // active
            RTST_STRING, "IP",  // type
            RTST_STRING, "ccmni0",  // ifname
            RTST_STRING, "",  // addresses
            RTST_STRING, "",  // dnses
            RTST_STRING, "",  // gateways
            RTST_STRING, "",  // pcscf
            RTST_INT32, "2454",  // mtu
            RTST_INT32, "1");  // rat

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EGPCORDP=1,\"FF00\"", 2,
            "+EGPCORDP: 1,\"FF00\",\"311480:3\"",
            "OK");

    usleep(250e3);
    RTST_EXPECTED_STATUS_VALUE_WITH_DELAY(RFX_SLOT_ID_0, RFX_STATUS_KEY_PCO_STATUS,
              RfxVariant(String8("FF00:3")), 0);

    RTST_EXPECTED_RIL_URC(RIL_UNSOL_PCO_DATA, 5,
            RTST_INT32, "0", // interface id
            RTST_STRING, "IP", // bearer_proto
            RTST_INT32, "65280", // pco_id (FF00)
            RTST_INT32, "1", // contents_length
            RTST_STRING, "3"); // contents

    RTST_STATUS_VALUE(RFX_SLOT_ID_0, RFX_STATUS_KEY_PCO_STATUS, RfxVariant(String8("")));
    RTST_CASE_END();
}

// URC +CGEV: ME PDN ACT trigger PCO, verify PCO results
TEST_F(DataConnectionTest, QUERY_PCO_5) {
    RTST_CASE_BEGIN();
    pcoPropertySetup();

    setupDataCall("IP", "IP");

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EGPCORDP=1,\"FF00\"", 2,
            "+EGPCORDP: 1,\"FF00\",\"311480:3\"",
            "OK");

    usleep(250e3);
    RTST_EXPECTED_STATUS_VALUE_WITH_DELAY(RFX_SLOT_ID_0, RFX_STATUS_KEY_PCO_STATUS,
              RfxVariant(String8("FF00:3")), 0);

    RTST_EXPECTED_RIL_URC(RIL_UNSOL_PCO_DATA, 5,
            RTST_INT32, "0", // interface id
            RTST_STRING, "IP", // bearer_proto
            RTST_INT32, "65280", // pco_id (FF00)
            RTST_INT32, "1", // contents_length
            RTST_STRING, "3"); // contents

    RTST_URC_STRING("+CGEV: ME PDN ACT 1, 2, 4");

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=3,4", 1,
            "OK");

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=4,\"ifst\",4", 2,
            "+EPDN:4, \"new\", 1, 0, 2454, 1, \"172.22.1.104\"",
            "OK");

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP=4", 2,
            "+CGCONTRDP: 4,5,\"internet\",\"\",\"\",\"172.22.1.204\",\"\",\"\",\"\",0,0,2454,,,",
            "OK");

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGACT?", 12,
            "+CGACT: 0,0",
            "+CGACT: 1,1",
            "+CGACT: 2,0",
            "+CGACT: 3,0",
            "+CGACT: 4,1",
            "+CGACT: 5,0",
            "+CGACT: 6,0",
            "+CGACT: 7,0",
            "+CGACT: 8,0",
            "+CGACT: 9,0",
            "+CGACT: 10,0",
            "OK");

    RTST_EXPECTED_RIL_URC(RIL_UNSOL_DATA_CALL_LIST_CHANGED, 14,
            RTST_INT32, "11",  // ril data call response version
            RTST_INT32, "1",  // num
            RTST_INT32, "0",  // status
            RTST_INT32, "-1",  // suggestedRetryTime
            RTST_INT32, "0",  // cid, means interfaceId actually
            RTST_INT32, "2",  // active
            RTST_STRING, "IP",  // type
            RTST_STRING, "ccmni0",  // ifname
            RTST_STRING, "172.22.1.104",  // addresses
            RTST_STRING, "172.22.1.204",  // dnses
            RTST_STRING, "172.22.1.104",  // gateways
            RTST_STRING, "",  // pcscf
            RTST_INT32, "2454",  // mtu
            RTST_INT32, "1");  // rat

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EGPCORDP=1,\"FF00\"", 2,
            "+EGPCORDP: 1,\"FF00\",\"311480:3\"",
            "OK");

    usleep(250e3);
    RTST_EXPECTED_STATUS_VALUE_WITH_DELAY(RFX_SLOT_ID_0, RFX_STATUS_KEY_PCO_STATUS,
              RfxVariant(String8("FF00:3")), 0);

    RTST_EXPECTED_RIL_URC(RIL_UNSOL_PCO_DATA, 5,
            RTST_INT32, "0", // interface id
            RTST_STRING, "IP", // bearer_proto
            RTST_INT32, "65280", // pco_id (FF00)
            RTST_INT32, "1", // contents_length
            RTST_STRING, "3"); // contents

    RTST_STATUS_VALUE(RFX_SLOT_ID_0, RFX_STATUS_KEY_PCO_STATUS, RfxVariant(String8("")));
    RTST_CASE_END();
}

// URC +EIAREG: LTE ME ATTACH trigger PCO, verify PCO results
TEST_F(DataConnectionTest, QUERY_PCO_6) {
    RTST_CASE_BEGIN();
    pcoPropertySetup();

    RTST_URC_STRING("+EIAREG: ME ATTACH \"apnname\",IPTYPE,1");

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+BGPCORDP=\"apnname\",1,\"FF00\"", 2,
            "+BGPCORDP: 1,\"FF00\",\"311480:3\"",
            "OK");

    usleep(250e3);
    RTST_EXPECTED_STATUS_VALUE_WITH_DELAY(RFX_SLOT_ID_0, RFX_STATUS_KEY_PCO_STATUS,
              RfxVariant(String8("FF00:3")), 0);

    RTST_EXPECTED_RIL_URC(RIL_UNSOL_PCO_DATA_AFTER_ATTACHED, 6,
            RTST_INT32, "-1", // interface id
            RTST_STRING, "apnname", // apn
            RTST_STRING, "IPTYPE", // bearer_proto
            RTST_INT32, "65280", // pco_id (FF00)
            RTST_INT32, "1", // contents_length
            RTST_STRING, "3"); // contents

    RTST_STATUS_VALUE(RFX_SLOT_ID_0, RFX_STATUS_KEY_PCO_STATUS, RfxVariant(String8("")));
    RTST_CASE_END();
}

// v4v6 fallback case, PCO prototype will report v4 first, then v4v6
TEST_F(DataConnectionTest, QUERY_PCO_7) {
    RTST_CASE_BEGIN();
    pcoPropertySetup();

    setupDataCall("IPV4V6", "IP");

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EGPCORDP=1,\"FF00\"", 2,
            "+EGPCORDP: 1,\"FF00\",\"311480:3\"",
            "OK");

    usleep(250e3);
    RTST_EXPECTED_STATUS_VALUE_WITH_DELAY(RFX_SLOT_ID_0, RFX_STATUS_KEY_PCO_STATUS,
              RfxVariant(String8("FF00:3")), 0);

    RTST_EXPECTED_RIL_URC(RIL_UNSOL_PCO_DATA, 5,
            RTST_INT32, "0", // interface id
            RTST_STRING, "IP", // bearer_proto
            RTST_INT32, "65280", // pco_id (FF00)
            RTST_INT32, "1", // contents_length
            RTST_STRING, "3"); // contents

    RTST_URC_STRING("+CGEV: ME PDN ACT 1, 2, 4");

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=3,4", 1,
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=4,\"ifst\",4", 2,
            "+EPDN: 4, \"new\", 1, 0, 2454, 2, \"38.7.251.144.32.14.171.78.0.0.0.81.202.39.147.1/64\"",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP=4", 2,
            "+CGCONTRDP: 4,8,\"internet\",\"\",\"\",\"35.1.251.144.32.14.171.78.0.0.0.81.202.39.83.25\""
            ",\"\",\"\",\"\",0,0,2454,,,",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGACT?", 12,
            "+CGACT: 0,0",
            "+CGACT: 1,1",
            "+CGACT: 2,0",
            "+CGACT: 3,0",
            "+CGACT: 4,1",
            "+CGACT: 5,0",
            "+CGACT: 6,0",
            "+CGACT: 7,0",
            "+CGACT: 8,0",
            "+CGACT: 9,0",
            "+CGACT: 10,0",
            "OK");

    RTST_EXPECTED_RIL_URC(RIL_UNSOL_DATA_CALL_LIST_CHANGED, 14,
            RTST_INT32, "11",  // ril data call response version
            RTST_INT32, "1",  // num
            RTST_INT32, "0",  // status
            RTST_INT32, "-1",  // suggestedRetryTime
            RTST_INT32, "0",  // cid, means interfaceId actually
            RTST_INT32, "2",  // active
            RTST_STRING, "IPV4V6",  // type
            RTST_STRING, "ccmni0",  // ifname
            RTST_STRING, "172.22.1.100 2607:FB90:200E:AB4E:0000:0051:CA27:9301/64",  // addresses
            RTST_STRING, "172.22.1.201 2301:FB90:200E:AB4E:0000:0051:CA27:5319",  // dnses
            RTST_STRING, "172.22.1.100 ::",  // gateways
            RTST_STRING, "",  // pcscf
            RTST_INT32, "2454",  // mtu
            RTST_INT32, "1");  // rat

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EGPCORDP=1,\"FF00\"", 2,
            "+EGPCORDP: 1,\"FF00\",\"311480:3\"",
            "OK");

    usleep(250e3);
    RTST_EXPECTED_STATUS_VALUE_WITH_DELAY(RFX_SLOT_ID_0, RFX_STATUS_KEY_PCO_STATUS,
              RfxVariant(String8("FF00:3")), 0);

    RTST_EXPECTED_RIL_URC(RIL_UNSOL_PCO_DATA, 5,
            RTST_INT32, "0", // interface id
            RTST_STRING, "IPV4V6", // bearer_proto
            RTST_INT32, "65280", // pco_id (FF00)
            RTST_INT32, "1", // contents_length
            RTST_STRING, "3"); // contents

    RTST_STATUS_VALUE(RFX_SLOT_ID_0, RFX_STATUS_KEY_PCO_STATUS, RfxVariant(String8("")));
    RTST_CASE_END();
}

// Query PCO format check
TEST_F(DataConnectionTest, QUERY_PCO_8) {
    RTST_CASE_BEGIN();
    pcoPropertySetup();

    RTST_URC_STRING("+EIAREG: ME ATTACH \"apnname\", IPTYPE,1");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+BGPCORDP=\"apnname\",1,\"FF00\"", 2,
            "+BGPCORDP: 1,\"FFFF\",\"311480:3\"",
            "OK");

    RTST_URC_STRING("+EIAREG: ME ATTACH \"apnname\", IPTYPE,1");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+BGPCORDP=\"apnname\",1,\"FF00\"", 2,
            "+BGPCORDP: 1,\"FF00\",\"311480 3\"",
            "OK");

    RTST_URC_STRING("+EIAREG: ME ATTACH \"apnname\", IPTYPE,1");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+BGPCORDP=\"apnname\",1,\"FF00\"", 2,
            "+BGPCORDP: 1,\"FF00\",\"123123:3\"",
            "OK");

    RTST_URC_STRING("+EIAREG: ME ATTACH \"apnname.mnc001.mcc01.gprs\",IPTYPE,1");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+BGPCORDP=\"apnname\",1,\"FF00\"", 2,
            "+BGPCORDP: 1,\"FF00\",\"123123:3\"",
            "OK");
    RTST_CASE_END();
}

// Check PDN manager init AT cmds for PCO IA URC
TEST_F(DataConnectionTest, QUERY_PCO_9) {
    RTST_CASE_BEGIN();

    RTST_URC_STRING("+EUTTEST: INIT AT CMDS");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EIAREG=1", 1, "OK");

    RTST_CASE_END();
}

