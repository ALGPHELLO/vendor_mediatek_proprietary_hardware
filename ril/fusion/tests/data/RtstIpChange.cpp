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
 * Initialize
 *****************************************************************************/
RTST_INIT_AT_CMD(A, "AT+CGDCONT=?", 4,
        "+CGDCONT: (0-10),\"IP\",,,(0),(0),(0-1),(0-3),(0-2),(0-1),(0-1)",
        "+CGDCONT: (0-10),\"IPV6\",,,(0),(0),(0-1),(0-3),(0-2),(0-1),(0-1)",
        "+CGDCONT: (0-10),\"IPV4V6\",,,(0),(0),(0-1),(0-3),(0-2),(0-1),(0-1)",
        "OK");

/*****************************************************************************
 * Test Cases
 *****************************************************************************/
TEST(IpChangeTest, RIL_UNSOL_DATA_CALL_LIST_CHANGED_IPV4) {
    RTST_CASE_BEGIN();

    RTST_RIL_REQUEST(RIL_REQUEST_SETUP_DATA_CALL, 8,
            RTST_STRING, "14", // radioType
            RTST_STRING, "0", // profile
            RTST_STRING, "internet", // apn
            RTST_STRING, "", // username
            RTST_STRING, "", // password
            RTST_STRING, "0", // authType
            RTST_STRING, "IP", // protocol
            RTST_STRING, "0" // interfaceId, not used for 93 rild
            );

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=1,\"internet\",\"default\"", 2,
            "+CGEV: ME PDN ACT 1, 2",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=1,\"ifst\",20", 2,
            "+EPDN:1, \"new\", 1, 0, 2454, 1, \"172.22.1.100\"",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP", 2,
            "+CGCONTRDP: 1,5,\"internet\",\"\",\"\",\"172.22.1.201\",\"172.22.1.202\", \
\"\",\"\",0,0,2454", "OK");
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
            RTST_INT32, "11", // ril data call response version
            RTST_INT32, "1", // num
            RTST_INT32, "0", // status
            RTST_INT32, "-1", // suggestedRetryTime
            RTST_INT32, "0", // cid, means interfaceId actually
            RTST_INT32, "2", // active
            RTST_STRING, "IP", // type
            RTST_STRING, "ccmni0", // ifname
            RTST_STRING, "172.22.1.100", // addresses
            RTST_STRING, "172.22.1.201 172.22.1.202", // dnses
            RTST_STRING, "172.22.1.100", // gateways
            RTST_STRING, "", // pcscf
            RTST_INT32, "2454", // mtu
            RTST_INT32, "1" // rat
            );

    RTST_URC_STRING("+EPDN: 1,\"dcchg\",256");

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

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=1,\"ifst\",4", 2,
            "+EPDN: 1, \"update\", 0, 1, \"172.22.1.500\"",
            "OK");

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP=1", 2,
            "+CGCONTRDP: 1,5,\"internet\",\"\",\"\",\"172.22.1.501\",\"172.22.1.502\",\"\",\"\",0,0,2455",
            "OK");

    RTST_EXPECTED_RIL_URC(RIL_UNSOL_DATA_CALL_LIST_CHANGED, 14,
            RTST_INT32, "11", // ril data call response version
            RTST_INT32, "1", // num
            RTST_INT32, "0", // status
            RTST_INT32, "-1", // suggestedRetryTime
            RTST_INT32, "0", // cid, means interfaceId actually
            RTST_INT32, "2", // active
            RTST_STRING, "IP", // type
            RTST_STRING, "ccmni0", // ifname
            RTST_STRING, "172.22.1.500", // addresses
            RTST_STRING, "172.22.1.501 172.22.1.502", // dnses
            RTST_STRING, "172.22.1.500", // gateways
            RTST_STRING, "", // pcscf
            RTST_INT32, "2455", // mtu
            RTST_INT32, "1" // rat
            );

    RTST_CASE_END();
}

TEST(IpChangeTest, RIL_UNSOL_DATA_CALL_LIST_CHANGED_IPV6) {
    RTST_CASE_BEGIN();

    RTST_RIL_REQUEST(RIL_REQUEST_SETUP_DATA_CALL, 8,
            RTST_STRING, "14", // radioType
            RTST_STRING, "0", // profile
            RTST_STRING, "ctnet", // apn
            RTST_STRING, "", // username
            RTST_STRING, "", // password
            RTST_STRING, "0", // authType
            RTST_STRING, "IPV6", // protocol
            RTST_STRING, "1" // interfaceId, not used for 93 rild
            );

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=1,\"ctnet\",\"default\"", 2,
            "+CGEV: ME PDN ACT 2, 2",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=2,\"ifst\",20", 2,
            "+EPDN:2, \"new\", 1, 1, 2654, 2, \"32.1.13.184.0.0.0.3.61.48.97.182.50.254.113.251\"",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP", 2,
            "+CGCONTRDP: 2,5,\"ctnet\",\"\",\"\",\"32.1.13.184.0.0.0.3.61.48.97.182.255.255.255.255\", \
\"\",\"\",\"\",0,0,2654",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGACT?", 12,
            "+CGACT: 0,0",
            "+CGACT: 1,0",
            "+CGACT: 2,1",
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
            RTST_INT32, "11", // ril data call response version
            RTST_INT32, "1", // num
            RTST_INT32, "0", // status
            RTST_INT32, "-1", // suggestedRetryTime
            RTST_INT32, "1", // cid, means interfaceId actually
            RTST_INT32, "2", // active
            RTST_STRING, "IPV6", // type
            RTST_STRING, "ccmni1", // ifname
            RTST_STRING, "2001:0DB8:0000:0003:3D30:61B6:32FE:71FB", // addresses
            RTST_STRING, "2001:0DB8:0000:0003:3D30:61B6:FFFF:FFFF", // dnses
            RTST_STRING, "::", // gateways
            RTST_STRING, "", // pcscf
            RTST_INT32, "2654", // mtu
            RTST_INT32, "1" // rat
            );

    RTST_URC_STRING("+EPDN: 2,\"dcchg\",256");

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGACT?", 12,
            "+CGACT: 0,0",
            "+CGACT: 1,0",
            "+CGACT: 2,1",
            "+CGACT: 3,0",
            "+CGACT: 4,0",
            "+CGACT: 5,0",
            "+CGACT: 6,0",
            "+CGACT: 7,0",
            "+CGACT: 8,0",
            "+CGACT: 9,0",
            "+CGACT: 10,0",
            "OK");

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=2,\"ifst\",4", 2,
            "+EPDN: 2, \"update\", 1, 2, \"32.1.13.184.0.0.0.3.61.48.97.182.50.254.113.255\"",
            "OK");

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP=2", 2,
            "+CGCONTRDP: 2,5,\"ctnet\",\"\",\"\",\"32.1.13.184.0.0.0.3.61.48.255.255.255.255.255.255\", \
\"\",\"\",\"\",0,0,2655",
            "OK");

    RTST_EXPECTED_RIL_URC(RIL_UNSOL_DATA_CALL_LIST_CHANGED, 14,
            RTST_INT32, "11", // ril data call response version
            RTST_INT32, "1", // num
            RTST_INT32, "0", // status
            RTST_INT32, "-1", // suggestedRetryTime
            RTST_INT32, "1", // cid, means interfaceId actually
            RTST_INT32, "2", // active
            RTST_STRING, "IPV6", // type
            RTST_STRING, "ccmni1", // ifname
            RTST_STRING, "2001:0DB8:0000:0003:3D30:61B6:32FE:71FF", // addresses
            RTST_STRING, "2001:0DB8:0000:0003:3D30:FFFF:FFFF:FFFF", // dnses
            RTST_STRING, "::", // gateways
            RTST_STRING, "", // pcscf
            RTST_INT32, "2655", // mtu
            RTST_INT32, "1" // rat
            );

    RTST_CASE_END();
}

TEST(IpChangeTest, RIL_UNSOL_DATA_CALL_LIST_CHANGED_IPV4V6) {
    RTST_CASE_BEGIN();

    RTST_RIL_REQUEST(RIL_REQUEST_SETUP_DATA_CALL, 8,
            RTST_STRING, "14", // radioType
            RTST_STRING, "0", // profile
            RTST_STRING, "ctlte", // apn
            RTST_STRING, "", // username
            RTST_STRING, "", // password
            RTST_STRING, "0", // authType
            RTST_STRING, "IPV4V6", // protocol
            RTST_STRING, "2" // interfaceId, not used for 93 rild
            );

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=1,\"ctlte\",\"default\"", 2,
            "+CGEV: ME PDN ACT 3, 2",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=3,\"ifst\",20", 2,
            "+EPDN:3, \"new\", 1, 2, 2054, 3, \"172.22.1.100\", \
\"32.1.13.184.0.0.0.3.61.48.97.182.50.254.113.251\"",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP", 3,
            "+CGCONTRDP: 3,5,\"ctlte\",\"\",\"\",\"172.22.1.201\",\"\",\"\",\"\",0,0,2054",
            "+CGCONTRDP: 3,5,\"ctlte\",\"\",\"\",\"32.1.13.184.0.0.0.3.61.48.97.182.255.255.255.255\", \
\"\",\"\",\"\",0,0,2054",
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
            RTST_INT32, "11", // ril data call response version
            RTST_INT32, "1", // num
            RTST_INT32, "0", // status
            RTST_INT32, "-1", // suggestedRetryTime
            RTST_INT32, "2", // cid, means interfaceId actually
            RTST_INT32, "2", // active
            RTST_STRING, "IPV4V6", // type
            RTST_STRING, "ccmni2", // ifname
            RTST_STRING, "172.22.1.100 2001:0DB8:0000:0003:3D30:61B6:32FE:71FB", // addresses
            RTST_STRING, "172.22.1.201 2001:0DB8:0000:0003:3D30:61B6:FFFF:FFFF", // dnses
            RTST_STRING, "172.22.1.100 ::", // gateways
            RTST_STRING, "", // pcscf
            RTST_INT32, "2054", // mtu
            RTST_INT32, "1" // rat
            );

    RTST_URC_STRING("+EPDN: 3,\"dcchg\",256");

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

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=3,\"ifst\",4", 2,
            "+EPDN: 3, \"update\", 2, 3, \"172.22.1.200\", \
\"32.1.13.184.0.0.0.3.61.48.97.182.50.254.113.255\"",
            "OK");

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP=3", 3,
            "+CGCONTRDP: 3,5,\"ctlte\",\"\",\"\",\"172.22.1.202\",\"\",\"\",\"\",0,0,2055",
            "+CGCONTRDP: 3,5,\"ctlte\",\"\",\"\",\"32.1.13.184.0.0.0.3.61.48.255.255.255.255.255.255\", \
\"\",\"\",\"\",0,0,2055",
            "OK");

    RTST_EXPECTED_RIL_URC(RIL_UNSOL_DATA_CALL_LIST_CHANGED, 14,
            RTST_INT32, "11", // ril data call response version
            RTST_INT32, "1", // num
            RTST_INT32, "0", // status
            RTST_INT32, "-1", // suggestedRetryTime
            RTST_INT32, "2", // cid, means interfaceId actually
            RTST_INT32, "2", // active
            RTST_STRING, "IPV4V6", // type
            RTST_STRING, "ccmni2", // ifname
            RTST_STRING, "172.22.1.200 2001:0DB8:0000:0003:3D30:61B6:32FE:71FF", // addresses
            RTST_STRING, "172.22.1.202 2001:0DB8:0000:0003:3D30:FFFF:FFFF:FFFF", // dnses
            RTST_STRING, "172.22.1.200 ::", // gateways
            RTST_STRING, "", // pcscf
            RTST_INT32, "2055", // mtu
            RTST_INT32, "1" // rat
            );

    RTST_CASE_END();
}

TEST(IpChangeTest, RIL_UNSOL_DATA_CALL_LIST_CHANGED_IPV4_2) {
    RTST_CASE_BEGIN();

    RTST_RIL_REQUEST(RIL_REQUEST_SETUP_DATA_CALL, 8,
            RTST_STRING, "14", // radioType
            RTST_STRING, "0", // profile
            RTST_STRING, "internet", // apn
            RTST_STRING, "", // username
            RTST_STRING, "", // password
            RTST_STRING, "0", // authType
            RTST_STRING, "IP", // protocol
            RTST_STRING, "0" // interfaceId, not used for 93 rild
            );

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=1,\"internet\",\"default\"", 2,
            "+CGEV: ME PDN ACT 1, 2",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=1,\"ifst\",20", 2,
            "+EPDN:1, \"new\", 1, 0, 2454, 1, \"172.22.1.100\"",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP", 2,
            "+CGCONTRDP: 1,5,\"internet\",\"\",\"\",\"172.22.1.201\",\"172.22.1.202\", \
\"\",\"\",0,0,2454", "OK");
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
            RTST_INT32, "11", // ril data call response version
            RTST_INT32, "1", // num
            RTST_INT32, "0", // status
            RTST_INT32, "-1", // suggestedRetryTime
            RTST_INT32, "0", // cid, means interfaceId actually
            RTST_INT32, "2", // active
            RTST_STRING, "IP", // type
            RTST_STRING, "ccmni0", // ifname
            RTST_STRING, "172.22.1.100", // addresses
            RTST_STRING, "172.22.1.201 172.22.1.202", // dnses
            RTST_STRING, "172.22.1.100", // gateways
            RTST_STRING, "", // pcscf
            RTST_INT32, "2454", // mtu
            RTST_INT32, "1" // rat
            );

    RTST_URC_STRING("+EPDN: 1,\"dcchg\",256");

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


    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=1,\"ifst\",4", 2,
            "+EPDN:1, \"update\", 0, 1, \"172.22.1.500\"",
            "OK");

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP=1", 2,
            "+CGCONTRDP: 1,5,\"internet\",\"\",\"\",\"172.22.1.501\",\"172.22.1.502\",\"\",\"\",0,0,2455",
            "OK");

    RTST_EXPECTED_RIL_URC(RIL_UNSOL_DATA_CALL_LIST_CHANGED, 14,
            RTST_INT32, "11", // ril data call response version
            RTST_INT32, "1", // num
            RTST_INT32, "0", // status
            RTST_INT32, "-1", // suggestedRetryTime
            RTST_INT32, "0", // cid, means interfaceId actually
            RTST_INT32, "2", // active
            RTST_STRING, "IP", // type
            RTST_STRING, "ccmni0", // ifname
            RTST_STRING, "172.22.1.500", // addresses
            RTST_STRING, "172.22.1.501 172.22.1.502", // dnses
            RTST_STRING, "172.22.1.500", // gateways
            RTST_STRING, "", // pcscf
            RTST_INT32, "2455", // mtu
            RTST_INT32, "1" // rat
            );

    RTST_CASE_END();
}

TEST(IpChangeTest, RIL_UNSOL_DATA_CALL_LIST_CHANGED_IPV6_2) {
    RTST_CASE_BEGIN();

    RTST_RIL_REQUEST(RIL_REQUEST_SETUP_DATA_CALL, 8,
            RTST_STRING, "14", // radioType
            RTST_STRING, "0", // profile
            RTST_STRING, "ctnet", // apn
            RTST_STRING, "", // username
            RTST_STRING, "", // password
            RTST_STRING, "0", // authType
            RTST_STRING, "IPV6", // protocol
            RTST_STRING, "1" // interfaceId, not used for 93 rild
            );

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=1,\"ctnet\",\"default\"", 2,
            "+CGEV: ME PDN ACT 2, 2",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=2,\"ifst\",20", 2,
            "+EPDN:2, \"new\", 1, 1, 2654, 2, \"32.1.13.184.0.0.0.3.61.48.97.182.50.254.113.251\"",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP", 2,
            "+CGCONTRDP: 2,5,\"ctnet\",\"\",\"\",\"32.1.13.184.0.0.0.3.61.48.97.182.255.255.255.255\", \
\"\",\"\",\"\",0,0,2654",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGACT?", 12,
            "+CGACT: 0,0",
            "+CGACT: 1,0",
            "+CGACT: 2,1",
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
            RTST_INT32, "11", // ril data call response version
            RTST_INT32, "1", // num
            RTST_INT32, "0", // status
            RTST_INT32, "-1", // suggestedRetryTime
            RTST_INT32, "1", // cid, means interfaceId actually
            RTST_INT32, "2", // active
            RTST_STRING, "IPV6", // type
            RTST_STRING, "ccmni1", // ifname
            RTST_STRING, "2001:0DB8:0000:0003:3D30:61B6:32FE:71FB", // addresses
            RTST_STRING, "2001:0DB8:0000:0003:3D30:61B6:FFFF:FFFF", // dnses
            RTST_STRING, "::", // gateways
            RTST_STRING, "", // pcscf
            RTST_INT32, "2654", // mtu
            RTST_INT32, "1" // rat
            );

    RTST_URC_STRING("+EPDN: 2,\"dcchg\",256");

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGACT?", 12,
            "+CGACT: 0,0",
            "+CGACT: 1,0",
            "+CGACT: 2,1",
            "+CGACT: 3,0",
            "+CGACT: 4,0",
            "+CGACT: 5,0",
            "+CGACT: 6,0",
            "+CGACT: 7,0",
            "+CGACT: 8,0",
            "+CGACT: 9,0",
            "+CGACT: 10,0",
            "OK");

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=2,\"ifst\",4", 2,
            "+EPDN:2, \"update\", 1, 2, \"32.1.13.184.0.0.0.3.61.48.97.182.50.254.113.255\"",
            "OK");

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP=2", 2,
            "+CGCONTRDP: 2,5,\"ctnet\",\"\",\"\",\"32.1.13.184.0.0.0.3.61.48.255.255.255.255.255.255\", \
\"\",\"\",\"\",0,0,2655",
            "OK");

    RTST_EXPECTED_RIL_URC(RIL_UNSOL_DATA_CALL_LIST_CHANGED, 14,
            RTST_INT32, "11", // ril data call response version
            RTST_INT32, "1", // num
            RTST_INT32, "0", // status
            RTST_INT32, "-1", // suggestedRetryTime
            RTST_INT32, "1", // cid, means interfaceId actually
            RTST_INT32, "2", // active
            RTST_STRING, "IPV6", // type
            RTST_STRING, "ccmni1", // ifname
            RTST_STRING, "2001:0DB8:0000:0003:3D30:61B6:32FE:71FF", // addresses
            RTST_STRING, "2001:0DB8:0000:0003:3D30:FFFF:FFFF:FFFF", // dnses
            RTST_STRING, "::", // gateways
            RTST_STRING, "", // pcscf
            RTST_INT32, "2655", // mtu
            RTST_INT32, "1" // rat
            );

    RTST_CASE_END();
}

TEST(IpChangeTest, RIL_UNSOL_DATA_CALL_LIST_CHANGED_IPV4V6_2) {
    RTST_CASE_BEGIN();

    RTST_RIL_REQUEST(RIL_REQUEST_SETUP_DATA_CALL, 8,
            RTST_STRING, "14", // radioType
            RTST_STRING, "0", // profile
            RTST_STRING, "ctlte", // apn
            RTST_STRING, "", // username
            RTST_STRING, "", // password
            RTST_STRING, "0", // authType
            RTST_STRING, "IPV4V6", // protocol
            RTST_STRING, "2" // interfaceId, not used for 93 rild
            );

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EAPNACT=1,\"ctlte\",\"default\"", 2,
            "+CGEV: ME PDN ACT 3, 2",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=3,\"ifst\",20", 2,
            "+EPDN:3, \"new\", 1, 2, 2054, 3, \"172.22.1.100\", \
\"32.1.13.184.0.0.0.3.61.48.97.182.50.254.113.251\"",
            "OK");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP", 3,
            "+CGCONTRDP: 3,5,\"ctlte\",\"\",\"\",\"172.22.1.201\",\"\",\"\",\"\",0,0,2054",
            "+CGCONTRDP: 3,5,\"ctlte\",\"\",\"\",\"32.1.13.184.0.0.0.3.61.48.97.182.255.255.255.255\", \
\"\",\"\",\"\",0,0,2054",
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
            RTST_INT32, "11", // ril data call response version
            RTST_INT32, "1", // num
            RTST_INT32, "0", // status
            RTST_INT32, "-1", // suggestedRetryTime
            RTST_INT32, "2", // cid, means interfaceId actually
            RTST_INT32, "2", // active
            RTST_STRING, "IPV4V6", // type
            RTST_STRING, "ccmni2", // ifname
            RTST_STRING, "172.22.1.100 2001:0DB8:0000:0003:3D30:61B6:32FE:71FB", // addresses
            RTST_STRING, "172.22.1.201 2001:0DB8:0000:0003:3D30:61B6:FFFF:FFFF", // dnses
            RTST_STRING, "172.22.1.100 ::", // gateways
            RTST_STRING, "", // pcscf
            RTST_INT32, "2054", // mtu
            RTST_INT32, "1" // rat
            );

    RTST_URC_STRING("+EPDN: 3,\"dcchg\",256");

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

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=3,\"ifst\",4", 2,
            "+EPDN:3, \"update\", 2, 1, \"172.22.1.200\"",
            "OK");

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP=3", 3,
            "+CGCONTRDP: 3,5,\"ctlte\",\"\",\"\",\"172.22.1.202\",\"\",\"\",\"\",0,0,2055",
            "+CGCONTRDP: 3,5,\"ctlte\",\"\",\"\",\"32.1.13.184.0.0.0.3.61.48.97.182.255.255.255.255\", \
\"\",\"\",\"\",0,0,2055",
            "OK");

    RTST_EXPECTED_RIL_URC(RIL_UNSOL_DATA_CALL_LIST_CHANGED, 14,
            RTST_INT32, "11", // ril data call response version
            RTST_INT32, "1", // num
            RTST_INT32, "0", // status
            RTST_INT32, "-1", // suggestedRetryTime
            RTST_INT32, "2", // cid, means interfaceId actually
            RTST_INT32, "2", // active
            RTST_STRING, "IPV4V6", // type
            RTST_STRING, "ccmni2", // ifname
            RTST_STRING, "172.22.1.200 2001:0DB8:0000:0003:3D30:61B6:32FE:71FB", // addresses
            RTST_STRING, "172.22.1.202 2001:0DB8:0000:0003:3D30:61B6:FFFF:FFFF", // dnses
            RTST_STRING, "172.22.1.200 ::", // gateways
            RTST_STRING, "", // pcscf
            RTST_INT32, "2055", // mtu
            RTST_INT32, "1" // rat
            );

    RTST_URC_STRING("+EPDN: 3,\"dcchg\",256");

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

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EPDN=3,\"ifst\",4", 2,
            "+EPDN: 3, \"update\", 2, 2, \"32.1.13.184.0.0.0.3.61.48.97.182.50.254.113.255\"",
            "OK");

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+CGCONTRDP=3", 3,
            "+CGCONTRDP: 3,5,\"ctlte\",\"\",\"\",\"172.22.1.202\",\"\",\"\",\"\",0,0,2055",
            "+CGCONTRDP: 3,5,\"ctlte\",\"\",\"\",\"32.1.13.184.0.0.0.3.61.48.255.255.255.255.255.255\", \
\"\",\"\",\"\",0,0,2055",
            "OK");

    RTST_EXPECTED_RIL_URC(RIL_UNSOL_DATA_CALL_LIST_CHANGED, 14,
            RTST_INT32, "11", // ril data call response version
            RTST_INT32, "1", // num
            RTST_INT32, "0", // status
            RTST_INT32, "-1", // suggestedRetryTime
            RTST_INT32, "2", // cid, means interfaceId actually
            RTST_INT32, "2", // active
            RTST_STRING, "IPV4V6", // type
            RTST_STRING, "ccmni2", // ifname
            RTST_STRING, "172.22.1.200 2001:0DB8:0000:0003:3D30:61B6:32FE:71FF", // addresses
            RTST_STRING, "172.22.1.202 2001:0DB8:0000:0003:3D30:FFFF:FFFF:FFFF", // dnses
            RTST_STRING, "172.22.1.200 ::", // gateways
            RTST_STRING, "", // pcscf
            RTST_INT32, "2055", // mtu
            RTST_INT32, "1" // rat
            );

    RTST_CASE_END();
}

/*TEST(IpChangeTest, INVALID_ADDRTYPE) {
    RTST_CASE_BEGIN();

    RTST_URC_STRING("+EPDN: 1,\"ipchg\",0,2450,0,\"172.22.1.xxx\"");

    RTST_URC_STRING("+EPDN: 1,\"ipchg\",0,2456,4,\"172.22.1.xxx\"");

    RTST_URC_STRING("+EPDN: 2,\"ipchg\",1,2650,0,\"32.1.13.184.0.0.0.3.61.48.97.182.50.254.113.251\"");

    RTST_URC_STRING("+EPDN: 2,\"ipchg\",1,2656,4,\"32.1.13.184.0.0.0.3.61.48.97.182.50.254.113.251\"");

    RTST_URC_STRING("+EPDN: 3,\"ipchg\",2,2050,0,\"172.22.1.xxx\", \
\"32.1.13.184.0.0.0.3.61.48.97.182.50.254.113.251\"");

    RTST_URC_STRING("+EPDN: 3,\"ipchg\",2,2056,4,\"172.22.1.xxx\", \
\"32.1.13.184.0.0.0.3.61.48.97.182.50.254.113.251\"");

    RTST_CASE_END();
}*/

TEST(IpChangeTest, RFX_MSG_EVENT_DATA_TEST_MODE_1) {
    RTST_CASE_BEGIN();
    RTST_URC_STRING("+EUTTEST: CLEAR ALL PDN TABLE");
    RTST_CASE_END();
}

TEST(IpChangeTest, RIL_UNSOL_MD_DATA_RETRY_COUNT_RESET_TEST) {
    RTST_CASE_BEGIN();
    RTST_URC_STRING("+EPDN: , \"dcchg\", 129");
    RTST_EXPECTED_RIL_VOID_URC(RIL_UNSOL_MD_DATA_RETRY_COUNT_RESET);
    RTST_CASE_END();
}
