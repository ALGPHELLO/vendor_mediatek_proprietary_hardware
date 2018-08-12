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
#include "nw/RpNwDefs.h"
/*****************************************************************************
 * Test Cases
 *****************************************************************************/
TEST(CdmaSmsTest, RIL_REQUEST_SET_SMSC_ADDRESS_1) {
    RTST_CASE_BEGIN();
    RtstEnv::get()->setStatus(RFX_SLOT_ID_0,
        RFX_STATUS_KEY_CDMA_CARD_TYPE, RfxVariant(CT_4G_UICC_CARD));
    RtstEnv::get()->setStatus(RFX_SLOT_ID_0, RFX_STATUS_KEY_NWS_MODE, RfxVariant(NWS_MODE_CDMALTE));
    setSmscAddress(RFX_SLOT_ID_0, "+316540942002");
    int token;
    RTST_EXPECTED_RIL_REQUEST_TO_GSM_RIL(RIL_REQUEST_SET_SMSC_ADDRESS, token, 1, RTST_STRING, "+316540942002");
    RTST_RIL_VOID_RESPONSE_FROM_GSM(RIL_REQUEST_SET_SMSC_ADDRESS,RIL_E_SUCCESS, token);
    RTST_EXPECTED_RIL_VOID_RESPONSE(RIL_REQUEST_SET_SMSC_ADDRESS, RIL_E_SUCCESS);
    RTST_CASE_END();
}

TEST(CdmaSmsTest, RIL_REQUEST_SET_SMSC_ADDRESS_2) {
    RTST_CASE_BEGIN();
    RtstEnv::get()->setStatus(RFX_SLOT_ID_0,
        RFX_STATUS_KEY_CDMA_CARD_TYPE, RfxVariant(CT_3G_UIM_CARD));
    RtstEnv::get()->setStatus(RFX_SLOT_ID_0, RFX_STATUS_KEY_NWS_MODE, RfxVariant(NWS_MODE_CDMALTE));
    setSmscAddress(RFX_SLOT_ID_0, "+316540942002");
    int token;
    RTST_EXPECTED_RIL_REQUEST_TO_CDMA_RIL(RIL_REQUEST_SET_SMSC_ADDRESS, token, 1, RTST_STRING, "+316540942002");
    RTST_RIL_VOID_RESPONSE_FROM_CDMA(RIL_REQUEST_SET_SMSC_ADDRESS, RIL_E_REQUEST_NOT_SUPPORTED, token);
    RTST_EXPECTED_RIL_VOID_RESPONSE(RIL_REQUEST_SET_SMSC_ADDRESS, RIL_E_REQUEST_NOT_SUPPORTED);
    RTST_CASE_END();
}


TEST(CdmaSmsTest, RIL_REQUEST_GET_SMSC_ADDRESS_1) {
    RTST_CASE_BEGIN();
    RtstEnv::get()->setStatus(RFX_SLOT_ID_0,
        RFX_STATUS_KEY_CDMA_CARD_TYPE, RfxVariant(CT_4G_UICC_CARD));
    RtstEnv::get()->setStatus(RFX_SLOT_ID_0, RFX_STATUS_KEY_NWS_MODE, RfxVariant(NWS_MODE_CDMALTE));
    getSmscAddress(RFX_SLOT_ID_0);
    int token;
    RTST_EXPECTED_VOID_RIL_REQUEST_TO_GSM_RIL(RIL_REQUEST_GET_SMSC_ADDRESS, token);
    RTST_RIL_RESPONSE_FROM_GSM_NC(RIL_REQUEST_GET_SMSC_ADDRESS,RIL_E_SUCCESS, token,
        1, RTST_STRING, "+316540942002");
    RTST_EXPECTED_RIL_RESPONSE(RIL_REQUEST_GET_SMSC_ADDRESS, RIL_E_SUCCESS,
        1, RTST_STRING, "+316540942002");
    RTST_CASE_END();
}

TEST(CdmaSmsTest, RIL_REQUEST_GET_SMSC_ADDRESS_2) {
    RTST_CASE_BEGIN();
    RtstEnv::get()->setStatus(RFX_SLOT_ID_0,
        RFX_STATUS_KEY_CDMA_CARD_TYPE, RfxVariant(CT_3G_UIM_CARD));
    RtstEnv::get()->setStatus(RFX_SLOT_ID_0, RFX_STATUS_KEY_NWS_MODE, RfxVariant(NWS_MODE_CDMALTE));
    getSmscAddress(RFX_SLOT_ID_0);
    int token;
    RTST_EXPECTED_VOID_RIL_REQUEST_TO_CDMA_RIL(RIL_REQUEST_GET_SMSC_ADDRESS, token);
    RTST_RIL_VOID_RESPONSE_FROM_CDMA(RIL_REQUEST_GET_SMSC_ADDRESS, RIL_E_REQUEST_NOT_SUPPORTED, token);
    RTST_EXPECTED_RIL_VOID_RESPONSE(RIL_REQUEST_GET_SMSC_ADDRESS, RIL_E_REQUEST_NOT_SUPPORTED);
    RTST_CASE_END();
}


TEST(CdmaSmsTest, RIL_REQUEST_IMS_SEND_SMS_1) {
    RTST_CASE_BEGIN();
    send3gppSmsOverIms(RFX_SLOT_ID_0, false, 1, NULL, "01000a81906002019900000cd3e614f4b697e5a064730a");
    int token;
    RTST_EXPECTED_RIL_REQUEST_TO_GSM_RIL(
            RIL_REQUEST_IMS_SEND_SMS,
            token,
            6,
            RTST_INT32, "1",
            RTST_INT32, "0",
            RTST_INT32, "1",
            RTST_INT32, "2",
            RTST_INT32, "-1", // NULL string
            RTST_STRING, "01000a81906002019900000cd3e614f4b697e5a064730a");
    RTST_RIL_RESPONSE_FROM_GSM_NC(RIL_REQUEST_IMS_SEND_SMS,RIL_E_SUCCESS, token,
            3,
            RTST_INT32, "50",
            RTST_INT32, "-1",
            RTST_INT32, "0");
    RTST_EXPECTED_RIL_RESPONSE_NC(RIL_REQUEST_IMS_SEND_SMS, RIL_E_SUCCESS, 3,
            RTST_INT32, "50",
            RTST_INT32, "-1",
            RTST_INT32, "0");
    RTST_CASE_END();
}


TEST(CdmaSmsTest, RIL_REQUEST_IMS_SEND_SMS_2) {
    RTST_CASE_BEGIN();
    RIL_CDMA_SMS_Address addr;
    addr.digit_mode = RIL_CDMA_SMS_DIGIT_MODE_4_BIT;
    addr.number_mode = RIL_CDMA_SMS_NUMBER_MODE_NOT_DATA_NETWORK;
    addr.number_type = RIL_CDMA_SMS_NUMBER_TYPE_UNKNOWN;
    addr.number_plan = RIL_CDMA_SMS_NUMBER_PLAN_UNKNOWN;
    addr.number_of_digits = 11;
    unsigned char digit[] = {1, 3, 5, 2, 2, 8, 6, 6, 8, 2, 2};
    memcpy(addr.digits, digit, 11);
    RIL_CDMA_SMS_Subaddress subAddr;
    subAddr.subaddressType = RIL_CDMA_SMS_SUBADDRESS_TYPE_NSAP;
    subAddr.odd = 0;
    subAddr.number_of_digits = 0;
    unsigned char bearerData[] = {0, 3, 32, 0, 16, 1, 4, 16, 18, 68, 128};
    int token;
    send3gpp2SmsOverIms(RFX_SLOT_ID_0, false, 32, 4098, false, 0, addr, subAddr, 11, bearerData);
    RTST_EXPECTED_RIL_REQUEST_TO_GSM_RIL(
            RIL_LOCAL_REQUEST_CDMA_SMS_SPECIFIC_TO_GSM,
            token,
            3,
            RTST_STRING, "MOSMS",
            RTST_STRING, "AT+C2KCMGS=30,\"0000021002040702c4d48a19a088060100080b0003200010010410124480\",\"13522866822\"",
            RTST_STRING, "+C2KCMGS");
    RTST_RIL_RESPONSE_FROM_GSM(RIL_LOCAL_REQUEST_CDMA_SMS_SPECIFIC_TO_GSM,RIL_E_SUCCESS, token,
            2,
            RTST_STRING, "MOSMS",
            RTST_STRING, "+C2KCMGS:32");
    RTST_EXPECTED_RIL_RESPONSE_NC(RIL_REQUEST_IMS_SEND_SMS, RIL_E_SUCCESS, 3,
            RTST_INT32, "32",
            RTST_INT32, "-1",
            RTST_INT32, "0");
    RTST_CASE_END();
}


TEST(CdmaSmsTest, RIL_REQUEST_IMS_SEND_SMS_3) {
    RTST_CASE_BEGIN();
    RIL_CDMA_SMS_Address addr;
    addr.digit_mode = RIL_CDMA_SMS_DIGIT_MODE_4_BIT;
    addr.number_mode = RIL_CDMA_SMS_NUMBER_MODE_NOT_DATA_NETWORK;
    addr.number_type = RIL_CDMA_SMS_NUMBER_TYPE_UNKNOWN;
    addr.number_plan = RIL_CDMA_SMS_NUMBER_PLAN_UNKNOWN;
    addr.number_of_digits = 11;
    unsigned char digit[] = {1, 3, 5, 2, 2, 8, 6, 6, 8, 2, 2};
    memcpy(addr.digits, digit, 11);
    RIL_CDMA_SMS_Subaddress subAddr;
    subAddr.subaddressType = RIL_CDMA_SMS_SUBADDRESS_TYPE_NSAP;
    subAddr.odd = 0;
    subAddr.number_of_digits = 0;
    unsigned char bearerData[] = {0, 3, 32, 0, 16, 1, 4, 16, 18, 68, 128};
    int token;
    send3gpp2SmsOverIms(RFX_SLOT_ID_0, false, 32, 4098, false, 0, addr, subAddr, 11, bearerData);
    RTST_EXPECTED_RIL_REQUEST_TO_GSM_RIL(
            RIL_LOCAL_REQUEST_CDMA_SMS_SPECIFIC_TO_GSM,
            token,
            3,
            RTST_STRING, "MOSMS",
            RTST_STRING, "AT+C2KCMGS=30,\"0000021002040702c4d48a19a088060100080b0003200010010410124480\",\"13522866822\"",
            RTST_STRING, "+C2KCMGS");
    RTST_RIL_RESPONSE_FROM_GSM(RIL_LOCAL_REQUEST_CDMA_SMS_SPECIFIC_TO_GSM,RIL_E_SUCCESS, token,
            2,
            RTST_STRING, "MOSMS",
            RTST_STRING, "+C2K ERROR:2,15");
    RTST_EXPECTED_RIL_RESPONSE_NC(RIL_REQUEST_IMS_SEND_SMS, RIL_E_SMS_SEND_FAIL_RETRY, 3,
            RTST_INT32, "-1",
            RTST_INT32, "-1",
            RTST_INT32, "15");
    RTST_CASE_END();
}




TEST(CdmaSmsTest, RIL_REQUEST_REPORT_SMS_MEMORY_STATUS_1) {
    RTST_CASE_BEGIN();
    RtstEnv::get()->setStatus(RFX_SLOT_ID_0, RFX_STATUS_KEY_NWS_MODE, RfxVariant(NWS_MODE_CSFB));
    reportSmsMemoryStatus(RFX_SLOT_ID_0, true);
    int token;
    RTST_EXPECTED_RIL_REQUEST_TO_GSM_RIL(
            RIL_REQUEST_REPORT_SMS_MEMORY_STATUS,
            token,
            1,
            RTST_INT32,
            "1");
    RTST_RIL_VOID_RESPONSE_FROM_GSM(RIL_REQUEST_REPORT_SMS_MEMORY_STATUS, RIL_E_SUCCESS, token);
    RTST_EXPECTED_RIL_VOID_RESPONSE(RIL_REQUEST_REPORT_SMS_MEMORY_STATUS, RIL_E_SUCCESS);
    RTST_CASE_END();
}

TEST(CdmaSmsTest, RIL_REQUEST_REPORT_SMS_MEMORY_STATUS_2) {
    RTST_CASE_BEGIN();
    RtstEnv::get()->setStatus(RFX_SLOT_ID_0, RFX_STATUS_KEY_NWS_MODE, RfxVariant(NWS_MODE_CDMALTE));
    reportSmsMemoryStatus(RFX_SLOT_ID_0, true);
    int token;
    RTST_EXPECTED_RIL_REQUEST_TO_CDMA_RIL(
            RIL_REQUEST_REPORT_SMS_MEMORY_STATUS,
            token,
            1,
            RTST_INT32,
            "1");
    RTST_RIL_VOID_RESPONSE_FROM_CDMA(RIL_REQUEST_REPORT_SMS_MEMORY_STATUS, RIL_E_SUCCESS, token);
    RTST_EXPECTED_RIL_VOID_RESPONSE(RIL_REQUEST_REPORT_SMS_MEMORY_STATUS, RIL_E_SUCCESS);
    RTST_CASE_END();
}


TEST(CdmaSmsTest, RIL_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION_1) {
    RTST_CASE_BEGIN();
    RTST_CLEAN_CDMA_RIL_SOCKET_DATA();
    setCdmaBroadcastActivation(RFX_SLOT_ID_0, true);
    int token;
    RTST_EXPECTED_RIL_REQUEST_TO_CDMA_RIL(
            RIL_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION,
            token,
            1,
            RTST_INT32,
            "0");
    RTST_RIL_VOID_RESPONSE_FROM_CDMA(RIL_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION, RIL_E_SUCCESS, token);
    RTST_EXPECTED_RIL_VOID_RESPONSE(RIL_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION, RIL_E_SUCCESS);
    RTST_CASE_END();
}

TEST(CdmaSmsTest, RIL_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION_2) {
    RTST_CASE_BEGIN();
    RTST_CLEAN_CDMA_RIL_SOCKET_DATA();
    setCdmaBroadcastActivation(RFX_SLOT_ID_0, false);
    int token;
    RTST_EXPECTED_RIL_REQUEST_TO_CDMA_RIL(
            RIL_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION,
            token,
            1,
            RTST_INT32,
            "1");
    RTST_RIL_VOID_RESPONSE_FROM_CDMA(RIL_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION, RIL_E_SUCCESS, token);
    RTST_EXPECTED_RIL_VOID_RESPONSE(RIL_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION, RIL_E_SUCCESS);
    RTST_CASE_END();
}


TEST(CdmaSmsTest, RIL_REQUEST_CDMA_DELETE_SMS_ON_RUIM_1) {
    RTST_CASE_BEGIN();
    RTST_CLEAN_CDMA_RIL_SOCKET_DATA();
    deleteSmsOnRuim(RFX_SLOT_ID_0, 1);
    int token;
    RTST_EXPECTED_RIL_REQUEST_TO_CDMA_RIL(
            RIL_REQUEST_CDMA_DELETE_SMS_ON_RUIM,
            token,
            1,
            RTST_INT32,
            "1");
    RTST_RIL_VOID_RESPONSE_FROM_CDMA(RIL_REQUEST_CDMA_DELETE_SMS_ON_RUIM, RIL_E_SUCCESS, token);
    RTST_EXPECTED_RIL_VOID_RESPONSE(RIL_REQUEST_CDMA_DELETE_SMS_ON_RUIM, RIL_E_SUCCESS);
    RTST_CASE_END();
}


TEST(CdmaSmsTest, RIL_REQUEST_CDMA_DELETE_SMS_ON_RUIM_2) {
    RTST_CASE_BEGIN();
    RTST_CLEAN_CDMA_RIL_SOCKET_DATA();
    deleteSmsOnRuim(RFX_SLOT_ID_0, -1);
    int token;
    RTST_EXPECTED_RIL_REQUEST_TO_CDMA_RIL(
            RIL_REQUEST_CDMA_DELETE_SMS_ON_RUIM,
            token,
            1,
            RTST_INT32,
            "-1");
    RTST_RIL_VOID_RESPONSE_FROM_CDMA(RIL_REQUEST_CDMA_DELETE_SMS_ON_RUIM, RIL_E_SUCCESS, token);
    RTST_EXPECTED_RIL_VOID_RESPONSE(RIL_REQUEST_CDMA_DELETE_SMS_ON_RUIM, RIL_E_SUCCESS);
    RTST_CASE_END();
}


TEST(CdmaSmsTest, RIL_REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG_1) {
    RTST_CASE_BEGIN();
    RTST_CLEAN_CDMA_RIL_SOCKET_DATA();
    int token;
    int config[] = {4096, 1, 0, 4098, 1, 0, 4099, 1, 0};
    setCdmaBroadcastConfig(RFX_SLOT_ID_0, config, 3);
    RTST_EXPECTED_RIL_REQUEST_TO_CDMA_RIL_NC(RIL_REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG,
         token,
         10,
         RTST_INT32, "3",
         RTST_INT32, "4096",
         RTST_INT32, "1",
         RTST_INT32, "0",
         RTST_INT32, "4098",
         RTST_INT32, "1",
         RTST_INT32, "0",
         RTST_INT32, "4099",
         RTST_INT32, "1",
         RTST_INT32, "0"
         );

    RTST_RIL_VOID_RESPONSE_FROM_CDMA(RIL_REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG, RIL_E_SUCCESS, token);
    RTST_EXPECTED_RIL_VOID_RESPONSE(RIL_REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG, RIL_E_SUCCESS);
    RTST_CASE_END();
}


TEST(CdmaSmsTest, RIL_REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG_2) {
    RTST_CASE_BEGIN();
    RTST_CLEAN_CDMA_RIL_SOCKET_DATA();
    int token;
    int config[] = {4096, 1, 1, 4098, 1, 1, 4099, 1, 1};
    setCdmaBroadcastConfig(RFX_SLOT_ID_0, config, 3);
    RTST_EXPECTED_RIL_REQUEST_TO_CDMA_RIL_NC(RIL_REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG,
         token,
         10,
         RTST_INT32, "3",
         RTST_INT32, "4096",
         RTST_INT32, "1",
         RTST_INT32, "1",
         RTST_INT32, "4098",
         RTST_INT32, "1",
         RTST_INT32, "1",
         RTST_INT32, "4099",
         RTST_INT32, "1",
         RTST_INT32, "1"
         );

    RTST_RIL_VOID_RESPONSE_FROM_CDMA(RIL_REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG, RIL_E_SUCCESS, token);
    RTST_EXPECTED_RIL_VOID_RESPONSE(RIL_REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG, RIL_E_SUCCESS);
    RTST_CASE_END();
}

TEST(CdmaSmsTest, RIL_REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG_3) {
    RTST_CASE_BEGIN();
    RTST_CLEAN_CDMA_RIL_SOCKET_DATA();
    int token;
    int config[] = {0, 0, 0};
    setCdmaBroadcastConfig(RFX_SLOT_ID_0, config, 1);
    RTST_EXPECTED_RIL_REQUEST_TO_CDMA_RIL_NC(RIL_REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG,
         token,
         4,
         RTST_INT32, "1",
         RTST_INT32, "0",
         RTST_INT32, "0",
         RTST_INT32, "0"
         );

    RTST_RIL_VOID_RESPONSE_FROM_CDMA(RIL_REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG, RIL_E_SUCCESS, token);
    RTST_EXPECTED_RIL_VOID_RESPONSE(RIL_REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG, RIL_E_SUCCESS);
    RTST_CASE_END();
}


TEST(CdmaSmsTest, RIL_REQUEST_CDMA_GET_BROADCAST_SMS_CONFIG_1) {
    RTST_CASE_BEGIN();
    getCdmaBroadcastConfig(RFX_SLOT_ID_0);
    int token;
    RTST_EXPECTED_VOID_RIL_REQUEST_TO_CDMA_RIL(RIL_REQUEST_CDMA_GET_BROADCAST_SMS_CONFIG, token);
    RTST_RIL_RESPONSE_FROM_CDMA_NC(RIL_REQUEST_CDMA_GET_BROADCAST_SMS_CONFIG, RIL_E_SUCCESS, token,
         4,
         RTST_INT32, "1",
         RTST_INT32, "0",
         RTST_INT32, "0",
         RTST_INT32, "0"
         );
    RTST_EXPECTED_RIL_RESPONSE_NC(RIL_REQUEST_CDMA_GET_BROADCAST_SMS_CONFIG, RIL_E_SUCCESS, 4,
         RTST_INT32, "1",
         RTST_INT32, "0",
         RTST_INT32, "0",
         RTST_INT32, "0"
         );
    RTST_CASE_END();
}


TEST(CdmaSmsTest, RIL_REQUEST_CDMA_GET_BROADCAST_SMS_CONFIG_2) {
    RTST_CASE_BEGIN();
    getCdmaBroadcastConfig(RFX_SLOT_ID_0);
    int token;
    RTST_EXPECTED_VOID_RIL_REQUEST_TO_CDMA_RIL(RIL_REQUEST_CDMA_GET_BROADCAST_SMS_CONFIG, token);
    RTST_RIL_RESPONSE_FROM_CDMA_NC(RIL_REQUEST_CDMA_GET_BROADCAST_SMS_CONFIG, RIL_E_SUCCESS, token,
         19,
         RTST_INT32, "6",
         RTST_INT32, "4096",
         RTST_INT32, "0",
         RTST_INT32, "1",
         RTST_INT32, "4098",
         RTST_INT32, "0",
         RTST_INT32, "1",
         RTST_INT32, "4099",
         RTST_INT32, "0",
         RTST_INT32, "1",
         RTST_INT32, "4100",
         RTST_INT32, "0",
         RTST_INT32, "1",
         RTST_INT32, "0",
         RTST_INT32, "1",
         RTST_INT32, "1",
         RTST_INT32, "0",
         RTST_INT32, "3",
         RTST_INT32, "1"
         );
    RTST_EXPECTED_RIL_RESPONSE_NC(RIL_REQUEST_CDMA_GET_BROADCAST_SMS_CONFIG, RIL_E_SUCCESS, 19,
         RTST_INT32, "6",
         RTST_INT32, "4096",
         RTST_INT32, "0",
         RTST_INT32, "1",
         RTST_INT32, "4098",
         RTST_INT32, "0",
         RTST_INT32, "1",
         RTST_INT32, "4099",
         RTST_INT32, "0",
         RTST_INT32, "1",
         RTST_INT32, "4100",
         RTST_INT32, "0",
         RTST_INT32, "1",
         RTST_INT32, "0",
         RTST_INT32, "1",
         RTST_INT32, "1",
         RTST_INT32, "0",
         RTST_INT32, "3",
         RTST_INT32, "1"
         );
    RTST_CASE_END();
}

TEST(CdmaSmsTest, RIL_REQUEST_CDMA_SEND_SMS_1) {
    RTST_CASE_BEGIN();
    RIL_CDMA_SMS_Address addr;
    addr.digit_mode = RIL_CDMA_SMS_DIGIT_MODE_4_BIT;
    addr.number_mode = RIL_CDMA_SMS_NUMBER_MODE_NOT_DATA_NETWORK;
    addr.number_type = RIL_CDMA_SMS_NUMBER_TYPE_UNKNOWN;
    addr.number_plan = RIL_CDMA_SMS_NUMBER_PLAN_UNKNOWN;
    addr.number_of_digits = 11;
    unsigned char digit[] = {1, 3, 5, 2, 2, 8, 6, 6, 8, 2, 2};
    memcpy(addr.digits, digit, 11);
    RIL_CDMA_SMS_Subaddress subAddr;
    subAddr.subaddressType = RIL_CDMA_SMS_SUBADDRESS_TYPE_NSAP;
    subAddr.odd = 0;
    subAddr.number_of_digits = 0;
    unsigned char bearerData[] = {0, 3, 32, 0, 16, 1, 4, 16, 18, 68, 128};
    sendCdmaSms(RFX_SLOT_ID_0, 4098, false, 0, addr, subAddr, 11, bearerData);
    int token;
    RTST_EXPECTED_RIL_REQUEST_TO_CDMA_RIL_NC(RIL_REQUEST_CDMA_SEND_SMS,
        token,
        34,
        RTST_INT32, "4098",  // teleServiceId
        RTST_INT32, "0",     // servicePresent
        RTST_INT32, "0",     // serviceCategory
        RTST_INT32, "0",     // address_digit_mode
        RTST_INT32, "0",     // address_nbr_mode
        RTST_INT32, "0",     // address_nbr_type
        RTST_INT32, "0",     // address_nbr_plan
        RTST_INT32, "11",    // address_nbr_of_digits
        RTST_INT32, "1",     // number 13522866822
        RTST_INT32, "3",
        RTST_INT32, "5",
        RTST_INT32, "2",
        RTST_INT32, "2",
        RTST_INT32, "8",
        RTST_INT32, "6",
        RTST_INT32, "6",
        RTST_INT32, "8",
        RTST_INT32, "2",
        RTST_INT32, "2",
        RTST_INT32, "0",    // sub address type
        RTST_INT32, "0",    // subaddr_odd
        RTST_INT32, "0",    // sub address digit
        RTST_INT32, "11",    // bearer data length
        RTST_INT32, "0",
        RTST_INT32, "3",
        RTST_INT32, "32",
        RTST_INT32, "0",
        RTST_INT32, "16",
        RTST_INT32, "1",
        RTST_INT32, "4",
        RTST_INT32, "16",
        RTST_INT32, "18",
        RTST_INT32, "68",
        RTST_INT32, "128"
        );
    RTST_RIL_RESPONSE_FROM_CDMA_NC(RIL_REQUEST_CDMA_SEND_SMS, RIL_E_SUCCESS, token, 3,
            RTST_INT32, "32",
            RTST_INT32, "-1",
            RTST_INT32, "0"
            );
    RTST_EXPECTED_RIL_RESPONSE_NC(RIL_REQUEST_CDMA_SEND_SMS, RIL_E_SUCCESS, 3,
            RTST_INT32, "32",
            RTST_INT32, "-1",
            RTST_INT32, "0"
            );
    RTST_CASE_END();
}

TEST(CdmaSmsTest, RIL_REQUEST_CDMA_SEND_SMS_2) {
    RTST_CASE_BEGIN();
    RIL_CDMA_SMS_Address addr;
    addr.digit_mode = RIL_CDMA_SMS_DIGIT_MODE_4_BIT;
    addr.number_mode = RIL_CDMA_SMS_NUMBER_MODE_NOT_DATA_NETWORK;
    addr.number_type = RIL_CDMA_SMS_NUMBER_TYPE_UNKNOWN;
    addr.number_plan = RIL_CDMA_SMS_NUMBER_PLAN_UNKNOWN;
    addr.number_of_digits = 11;
    unsigned char digit[] = {1, 3, 5, 2, 2, 8, 6, 6, 8, 2, 2};
    memcpy(addr.digits, digit, 11);
    RIL_CDMA_SMS_Subaddress subAddr;
    subAddr.subaddressType = RIL_CDMA_SMS_SUBADDRESS_TYPE_NSAP;
    subAddr.odd = 0;
    subAddr.number_of_digits = 0;
    unsigned char bearerData[] = {0, 3, 32, 0, 16, 1, 4, 16, 18, 68, 128};
    sendCdmaSms(RFX_SLOT_ID_0, 4098, false, 0, addr, subAddr, 11, bearerData);
    int token;
    RTST_EXPECTED_RIL_REQUEST_TO_CDMA_RIL_NC(RIL_REQUEST_CDMA_SEND_SMS,
        token,
        34,
        RTST_INT32, "4098",  // teleServiceId
        RTST_INT32, "0",     // servicePresent
        RTST_INT32, "0",     // serviceCategory
        RTST_INT32, "0",     // address_digit_mode
        RTST_INT32, "0",     // address_nbr_mode
        RTST_INT32, "0",     // address_nbr_type
        RTST_INT32, "0",     // address_nbr_plan
        RTST_INT32, "11",    // address_nbr_of_digits
        RTST_INT32, "1",     // number 13522866822
        RTST_INT32, "3",
        RTST_INT32, "5",
        RTST_INT32, "2",
        RTST_INT32, "2",
        RTST_INT32, "8",
        RTST_INT32, "6",
        RTST_INT32, "6",
        RTST_INT32, "8",
        RTST_INT32, "2",
        RTST_INT32, "2",
        RTST_INT32, "0",    // sub address type
        RTST_INT32, "0",    // subaddr_odd
        RTST_INT32, "0",    // sub address digit
        RTST_INT32, "11",    // bearer data length
        RTST_INT32, "0",
        RTST_INT32, "3",
        RTST_INT32, "32",
        RTST_INT32, "0",
        RTST_INT32, "16",
        RTST_INT32, "1",
        RTST_INT32, "4",
        RTST_INT32, "16",
        RTST_INT32, "18",
        RTST_INT32, "68",
        RTST_INT32, "128"
        );
    RTST_RIL_RESPONSE_FROM_CDMA_NC(RIL_REQUEST_CDMA_SEND_SMS, RIL_E_SMS_SEND_FAIL_RETRY, token, 3,
            RTST_INT32, "0",
            RTST_INT32, "-1",
            RTST_INT32, "15"
            );
    RTST_EXPECTED_RIL_RESPONSE_NC(RIL_REQUEST_CDMA_SEND_SMS, RIL_E_SMS_SEND_FAIL_RETRY, 3,
            RTST_INT32, "0",
            RTST_INT32, "-1",
            RTST_INT32, "15"
            );
    RTST_CASE_END();
}


TEST(CdmaSmsTest, MO_CONFILICT_1) {
    RTST_CASE_BEGIN();
    {
        RIL_CDMA_SMS_Address addr;
        addr.digit_mode = RIL_CDMA_SMS_DIGIT_MODE_4_BIT;
        addr.number_mode = RIL_CDMA_SMS_NUMBER_MODE_NOT_DATA_NETWORK;
        addr.number_type = RIL_CDMA_SMS_NUMBER_TYPE_UNKNOWN;
        addr.number_plan = RIL_CDMA_SMS_NUMBER_PLAN_UNKNOWN;
        addr.number_of_digits = 11;
        unsigned char digit[] = {1, 3, 5, 2, 2, 8, 6, 6, 8, 2, 2};
        memcpy(addr.digits, digit, 11);
        RIL_CDMA_SMS_Subaddress subAddr;
        subAddr.subaddressType = RIL_CDMA_SMS_SUBADDRESS_TYPE_NSAP;
        subAddr.odd = 0;
        subAddr.number_of_digits = 0;
        unsigned char bearerData[] = {0, 3, 32, 0, 16, 1, 4, 16, 18, 68, 128};
        send3gpp2SmsOverIms(RFX_SLOT_ID_0, false,
                32, 4098, false, 0, addr, subAddr, 11, bearerData);
    }
    int token;
    RTST_EXPECTED_RIL_REQUEST_TO_GSM_RIL(
            RIL_LOCAL_REQUEST_CDMA_SMS_SPECIFIC_TO_GSM,
            token,
            3,
            RTST_STRING, "MOSMS",
            RTST_STRING, "AT+C2KCMGS=30,\"0000021002040702c4d48a19a088060100080b0003200010010410124480\",\"13522866822\"",
            RTST_STRING, "+C2KCMGS");
    {
        RIL_CDMA_SMS_Address addr;
        addr.digit_mode = RIL_CDMA_SMS_DIGIT_MODE_4_BIT;
        addr.number_mode = RIL_CDMA_SMS_NUMBER_MODE_NOT_DATA_NETWORK;
        addr.number_type = RIL_CDMA_SMS_NUMBER_TYPE_UNKNOWN;
        addr.number_plan = RIL_CDMA_SMS_NUMBER_PLAN_UNKNOWN;
        addr.number_of_digits = 11;
        unsigned char digit[] = {1, 3, 5, 2, 2, 8, 6, 6, 8, 2, 2};
        memcpy(addr.digits, digit, 11);
        RIL_CDMA_SMS_Subaddress subAddr;
        subAddr.subaddressType = RIL_CDMA_SMS_SUBADDRESS_TYPE_NSAP;
        subAddr.odd = 0;
        subAddr.number_of_digits = 0;
        unsigned char bearerData[] = {0, 3, 32, 0, 16, 1, 4, 16, 18, 68, 128};
        sendCdmaSms(RFX_SLOT_ID_0, 4098, false, 0, addr, subAddr, 11, bearerData);
    }

    RTST_RIL_RESPONSE_FROM_GSM(RIL_LOCAL_REQUEST_CDMA_SMS_SPECIFIC_TO_GSM,RIL_E_SUCCESS, token,
            2,
            RTST_STRING, "MOSMS",
            RTST_STRING, "+C2KCMGS:32");
    RTST_EXPECTED_RIL_RESPONSE_NC(RIL_REQUEST_IMS_SEND_SMS, RIL_E_SUCCESS, 3,
            RTST_INT32, "32",
            RTST_INT32, "-1",
            RTST_INT32, "0");
    RTST_EXPECTED_RIL_REQUEST_TO_CDMA_RIL_NC(RIL_REQUEST_CDMA_SEND_SMS,
        token,
        34,
        RTST_INT32, "4098",  // teleServiceId
        RTST_INT32, "0",     // servicePresent
        RTST_INT32, "0",     // serviceCategory
        RTST_INT32, "0",     // address_digit_mode
        RTST_INT32, "0",     // address_nbr_mode
        RTST_INT32, "0",     // address_nbr_type
        RTST_INT32, "0",     // address_nbr_plan
        RTST_INT32, "11",    // address_nbr_of_digits
        RTST_INT32, "1",     // number 13522866822
        RTST_INT32, "3",
        RTST_INT32, "5",
        RTST_INT32, "2",
        RTST_INT32, "2",
        RTST_INT32, "8",
        RTST_INT32, "6",
        RTST_INT32, "6",
        RTST_INT32, "8",
        RTST_INT32, "2",
        RTST_INT32, "2",
        RTST_INT32, "0",    // sub address type
        RTST_INT32, "0",    // subaddr_odd
        RTST_INT32, "0",    // sub address digit
        RTST_INT32, "11",    // bearer data length
        RTST_INT32, "0",
        RTST_INT32, "3",
        RTST_INT32, "32",
        RTST_INT32, "0",
        RTST_INT32, "16",
        RTST_INT32, "1",
        RTST_INT32, "4",
        RTST_INT32, "16",
        RTST_INT32, "18",
        RTST_INT32, "68",
        RTST_INT32, "128"
        );
    RTST_RIL_RESPONSE_FROM_CDMA_NC(RIL_REQUEST_CDMA_SEND_SMS, RIL_E_SUCCESS, token, 3,
            RTST_INT32, "32",
            RTST_INT32, "-1",
            RTST_INT32, "0"
            );
    RTST_EXPECTED_RIL_RESPONSE_NC(RIL_REQUEST_CDMA_SEND_SMS, RIL_E_SUCCESS, 3,
            RTST_INT32, "32",
            RTST_INT32, "-1",
            RTST_INT32, "0"
            );

    RTST_CASE_END();
}



TEST(CdmaSmsTest, RIL_REQUEST_CDMA_WRITE_SMS_TO_RUIM_1) {
    RTST_CASE_BEGIN();
    RIL_CDMA_SMS_Address addr;
    addr.digit_mode = RIL_CDMA_SMS_DIGIT_MODE_4_BIT;
    addr.number_mode = RIL_CDMA_SMS_NUMBER_MODE_NOT_DATA_NETWORK;
    addr.number_type = RIL_CDMA_SMS_NUMBER_TYPE_UNKNOWN;
    addr.number_plan = RIL_CDMA_SMS_NUMBER_PLAN_UNKNOWN;
    addr.number_of_digits = 11;
    unsigned char digit[] = {1, 3, 5, 2, 2, 8, 6, 6, 8, 2, 2};
    memcpy(addr.digits, digit, 11);
    RIL_CDMA_SMS_Subaddress subAddr;
    subAddr.subaddressType = RIL_CDMA_SMS_SUBADDRESS_TYPE_NSAP;
    subAddr.odd = 0;
    subAddr.number_of_digits = 0;
    unsigned char bearerData[] = {0, 3, 32, 0, 16, 1, 4, 16, 18, 68, 128};
    writeSmsToRuim(RFX_SLOT_ID_0, 1, 4098, false, 0, addr, subAddr, 11, bearerData);
    int token;
    RTST_EXPECTED_RIL_REQUEST_TO_CDMA_RIL_NC(RIL_REQUEST_CDMA_WRITE_SMS_TO_RUIM,
        token,
        35,
        RTST_INT32, "1",     // Status 1
        RTST_INT32, "4098",  // teleServiceId
        RTST_INT32, "0",     // servicePresent
        RTST_INT32, "0",     // serviceCategory
        RTST_INT32, "0",     // address_digit_mode
        RTST_INT32, "0",     // address_nbr_mode
        RTST_INT32, "0",     // address_nbr_type
        RTST_INT32, "0",     // address_nbr_plan
        RTST_INT32, "11",    // address_nbr_of_digits
        RTST_INT32, "1",     // number 13522866822
        RTST_INT32, "3",
        RTST_INT32, "5",
        RTST_INT32, "2",
        RTST_INT32, "2",
        RTST_INT32, "8",
        RTST_INT32, "6",
        RTST_INT32, "6",
        RTST_INT32, "8",
        RTST_INT32, "2",
        RTST_INT32, "2",
        RTST_INT32, "0",    // sub address type
        RTST_INT32, "0",    // subaddr_odd
        RTST_INT32, "0",    // sub address digit
        RTST_INT32, "11",    // bearer data length
        RTST_INT32, "0",
        RTST_INT32, "3",
        RTST_INT32, "32",
        RTST_INT32, "0",
        RTST_INT32, "16",
        RTST_INT32, "1",
        RTST_INT32, "4",
        RTST_INT32, "16",
        RTST_INT32, "18",
        RTST_INT32, "68",
        RTST_INT32, "128"
        );
    RTST_RIL_RESPONSE_FROM_CDMA(RIL_REQUEST_CDMA_WRITE_SMS_TO_RUIM, RIL_E_SUCCESS, token, 1,
            RTST_INT32, "32");
    RTST_EXPECTED_RIL_RESPONSE(RIL_REQUEST_CDMA_WRITE_SMS_TO_RUIM, RIL_E_SUCCESS, 1,
            RTST_INT32, "32");
    RTST_CASE_END();
}


TEST(CdmaSmsTest, RIL_UNSOL_CDMA_RUIM_SMS_STORAGE_FULL_1) {
    RTST_CASE_BEGIN();
    RTST_RIL_VOID_URC_FROM_CDMA(RIL_UNSOL_CDMA_RUIM_SMS_STORAGE_FULL);
    RTST_EXPECTED_RIL_VOID_URC(RIL_UNSOL_CDMA_RUIM_SMS_STORAGE_FULL);
    RTST_CASE_END();
}


TEST(CdmaSmsTest, RIL_UNSOL_RESPONSE_CDMA_NEW_SMS_1) {
    RTST_CASE_BEGIN();
    RTST_RIL_URC_FROM_CDMA_NC(RIL_UNSOL_RESPONSE_CDMA_NEW_SMS, 34,
        RTST_INT32, "4098",  // teleServiceId
        RTST_INT32, "0",     // servicePresent
        RTST_INT32, "0",     // serviceCategory
        RTST_INT32, "0",     // address_digit_mode
        RTST_INT32, "0",     // address_nbr_mode
        RTST_INT32, "0",     // address_nbr_type
        RTST_INT32, "0",     // address_nbr_plan
        RTST_INT32, "11",    // address_nbr_of_digits
        RTST_INT32, "1",     // number 13522866822
        RTST_INT32, "3",
        RTST_INT32, "5",
        RTST_INT32, "2",
        RTST_INT32, "2",
        RTST_INT32, "8",
        RTST_INT32, "6",
        RTST_INT32, "6",
        RTST_INT32, "8",
        RTST_INT32, "2",
        RTST_INT32, "2",
        RTST_INT32, "0",    // sub address type
        RTST_INT32, "0",    // subaddr_odd
        RTST_INT32, "0",    // sub address digit
        RTST_INT32, "11",    // bearer data length
        RTST_INT32, "0",
        RTST_INT32, "3",
        RTST_INT32, "32",
        RTST_INT32, "0",
        RTST_INT32, "16",
        RTST_INT32, "1",
        RTST_INT32, "4",
        RTST_INT32, "16",
        RTST_INT32, "18",
        RTST_INT32, "68",
        RTST_INT32, "128");
    RTST_EXPECTED_RIL_URC_NC(RIL_UNSOL_RESPONSE_CDMA_NEW_SMS, 34,
        RTST_INT32, "4098",  // teleServiceId
        RTST_INT32, "0",     // servicePresent
        RTST_INT32, "0",     // serviceCategory
        RTST_INT32, "0",     // address_digit_mode
        RTST_INT32, "0",     // address_nbr_mode
        RTST_INT32, "0",     // address_nbr_type
        RTST_INT32, "0",     // address_nbr_plan
        RTST_INT32, "11",    // address_nbr_of_digits
        RTST_INT32, "1",     // number 13522866822
        RTST_INT32, "3",
        RTST_INT32, "5",
        RTST_INT32, "2",
        RTST_INT32, "2",
        RTST_INT32, "8",
        RTST_INT32, "6",
        RTST_INT32, "6",
        RTST_INT32, "8",
        RTST_INT32, "2",
        RTST_INT32, "2",
        RTST_INT32, "0",    // sub address type
        RTST_INT32, "0",    // subaddr_odd
        RTST_INT32, "0",    // sub address digit
        RTST_INT32, "11",    // bearer data length
        RTST_INT32, "0",
        RTST_INT32, "3",
        RTST_INT32, "32",
        RTST_INT32, "0",
        RTST_INT32, "16",
        RTST_INT32, "1",
        RTST_INT32, "4",
        RTST_INT32, "16",
        RTST_INT32, "18",
        RTST_INT32, "68",
        RTST_INT32, "128");
    RTST_CLEAN_CDMA_RIL_SOCKET_DATA();
    acknowledgeLastIncomingCdmaSms(RFX_SLOT_ID_0, 0, 0);
    int token;
    RTST_EXPECTED_RIL_REQUEST_TO_CDMA_RIL_NC(
            RIL_REQUEST_CDMA_SMS_ACKNOWLEDGE,
            token,
            2,
            RTST_INT32,
            "0",
            RTST_INT32,
            "0");
    RTST_RIL_VOID_RESPONSE_FROM_CDMA(RIL_REQUEST_CDMA_SMS_ACKNOWLEDGE, RIL_E_SUCCESS, token);
    RTST_EXPECTED_RIL_VOID_RESPONSE(RIL_REQUEST_CDMA_SMS_ACKNOWLEDGE, RIL_E_SUCCESS);
    RTST_CASE_END();
}

TEST(CdmaSmsTest, UNSOL_CDMA_SMS_SPECIFIC_FROM_GSM_1) {
    RTST_CASE_BEGIN();
    RTST_RIL_URC_FROM_GSM(RIL_LOCAL_GSM_UNSOL_CDMA_SMS_SPECIFIC_FROM_GSM, 1, RTST_STRING,
            "0000021002020702c4d48a19a088060100080b0003200010010410124480");
    RTST_EXPECTED_RIL_URC_NC(RIL_UNSOL_RESPONSE_CDMA_NEW_SMS, 34,
        RTST_INT32, "4098",  // teleServiceId
        RTST_INT32, "0",     // servicePresent
        RTST_INT32, "0",     // serviceCategory
        RTST_INT32, "0",     // address_digit_mode
        RTST_INT32, "0",     // address_nbr_mode
        RTST_INT32, "0",     // address_nbr_type
        RTST_INT32, "0",     // address_nbr_plan
        RTST_INT32, "11",    // address_nbr_of_digits
        RTST_INT32, "1",     // number 13522866822
        RTST_INT32, "3",
        RTST_INT32, "5",
        RTST_INT32, "2",
        RTST_INT32, "2",
        RTST_INT32, "8",
        RTST_INT32, "6",
        RTST_INT32, "6",
        RTST_INT32, "8",
        RTST_INT32, "2",
        RTST_INT32, "2",
        RTST_INT32, "0",    // sub address type
        RTST_INT32, "0",    // subaddr_odd
        RTST_INT32, "0",    // sub address digit
        RTST_INT32, "11",    // bearer data length
        RTST_INT32, "0",
        RTST_INT32, "3",
        RTST_INT32, "32",
        RTST_INT32, "0",
        RTST_INT32, "16",
        RTST_INT32, "1",
        RTST_INT32, "4",
        RTST_INT32, "16",
        RTST_INT32, "18",
        RTST_INT32, "68",
        RTST_INT32, "128");
    RTST_CLEAN_CDMA_RIL_SOCKET_DATA();
    RTST_CLEAN_GSM_RIL_SOCKET_DATA();

    acknowledgeLastIncomingCdmaSms(RFX_SLOT_ID_0, 0, 0);
    int token;
    RTST_EXPECTED_RIL_REQUEST_TO_GSM_RIL(
            RIL_LOCAL_REQUEST_CDMA_SMS_SPECIFIC_TO_GSM,
            token,
            3,
            RTST_STRING, "MTCNMASMS",
            RTST_STRING, "AT+C2KCNMA=13, \"02040702c4d48a19a088070100\"",
            RTST_STRING, "");
    RTST_RIL_RESPONSE_FROM_GSM(RIL_LOCAL_REQUEST_CDMA_SMS_SPECIFIC_TO_GSM, RIL_E_SUCCESS,
        token,
        2,
        RTST_STRING, "MTCNMASMS",
        RTST_STRING, "OK");
    RTST_EXPECTED_RIL_VOID_RESPONSE(RIL_REQUEST_CDMA_SMS_ACKNOWLEDGE, RIL_E_SUCCESS);
    RTST_CASE_END();
}

TEST(CdmaSmsTest, MT_CONFLICT_1) {
    RTST_CASE_BEGIN();
    RTST_RIL_URC_FROM_CDMA_NC(RIL_UNSOL_RESPONSE_CDMA_NEW_SMS, 34,
        RTST_INT32, "4098",  // teleServiceId
        RTST_INT32, "0",     // servicePresent
        RTST_INT32, "0",     // serviceCategory
        RTST_INT32, "0",     // address_digit_mode
        RTST_INT32, "0",     // address_nbr_mode
        RTST_INT32, "0",     // address_nbr_type
        RTST_INT32, "0",     // address_nbr_plan
        RTST_INT32, "11",    // address_nbr_of_digits
        RTST_INT32, "1",     // number 13522866822
        RTST_INT32, "3",
        RTST_INT32, "5",
        RTST_INT32, "2",
        RTST_INT32, "2",
        RTST_INT32, "8",
        RTST_INT32, "6",
        RTST_INT32, "6",
        RTST_INT32, "8",
        RTST_INT32, "2",
        RTST_INT32, "2",
        RTST_INT32, "0",    // sub address type
        RTST_INT32, "0",    // subaddr_odd
        RTST_INT32, "0",    // sub address digit
        RTST_INT32, "11",    // bearer data length
        RTST_INT32, "0",
        RTST_INT32, "3",
        RTST_INT32, "32",
        RTST_INT32, "0",
        RTST_INT32, "16",
        RTST_INT32, "1",
        RTST_INT32, "4",
        RTST_INT32, "16",
        RTST_INT32, "18",
        RTST_INT32, "68",
        RTST_INT32, "128");
    RTST_EXPECTED_RIL_URC_NC(RIL_UNSOL_RESPONSE_CDMA_NEW_SMS, 34,
        RTST_INT32, "4098",  // teleServiceId
        RTST_INT32, "0",     // servicePresent
        RTST_INT32, "0",     // serviceCategory
        RTST_INT32, "0",     // address_digit_mode
        RTST_INT32, "0",     // address_nbr_mode
        RTST_INT32, "0",     // address_nbr_type
        RTST_INT32, "0",     // address_nbr_plan
        RTST_INT32, "11",    // address_nbr_of_digits
        RTST_INT32, "1",     // number 13522866822
        RTST_INT32, "3",
        RTST_INT32, "5",
        RTST_INT32, "2",
        RTST_INT32, "2",
        RTST_INT32, "8",
        RTST_INT32, "6",
        RTST_INT32, "6",
        RTST_INT32, "8",
        RTST_INT32, "2",
        RTST_INT32, "2",
        RTST_INT32, "0",    // sub address type
        RTST_INT32, "0",    // subaddr_odd
        RTST_INT32, "0",    // sub address digit
        RTST_INT32, "11",    // bearer data length
        RTST_INT32, "0",
        RTST_INT32, "3",
        RTST_INT32, "32",
        RTST_INT32, "0",
        RTST_INT32, "16",
        RTST_INT32, "1",
        RTST_INT32, "4",
        RTST_INT32, "16",
        RTST_INT32, "18",
        RTST_INT32, "68",
        RTST_INT32, "128");

    RTST_CLEAN_CDMA_RIL_SOCKET_DATA();
    RTST_CLEAN_GSM_RIL_SOCKET_DATA();
    int token;
    RTST_RIL_URC_FROM_GSM(RIL_LOCAL_GSM_UNSOL_CDMA_SMS_SPECIFIC_FROM_GSM, 1, RTST_STRING,
            "0000021002020702c4d48a19a088060100080b0003200010010410124480");

    RTST_EXPECTED_RIL_REQUEST_TO_GSM_RIL(
            RIL_LOCAL_REQUEST_CDMA_SMS_SPECIFIC_TO_GSM,
            token,
            3,
            RTST_STRING, "MTCNMASMS",
            RTST_STRING, "AT+C2KCNMA=14, \"02040702c4d48a19a08807020221\"",
            RTST_STRING, "");
    RTST_RIL_RESPONSE_FROM_GSM(RIL_LOCAL_REQUEST_CDMA_SMS_SPECIFIC_TO_GSM, RIL_E_SUCCESS,
        token,
        2,
        RTST_STRING, "MTCNMASMS",
        RTST_STRING, "OK");

    acknowledgeLastIncomingCdmaSms(RFX_SLOT_ID_0, 0, 0);
    RTST_EXPECTED_RIL_REQUEST_TO_CDMA_RIL_NC(
            RIL_REQUEST_CDMA_SMS_ACKNOWLEDGE,
            token,
            2,
            RTST_INT32,
            "0",
            RTST_INT32,
            "0");
    RTST_RIL_VOID_RESPONSE_FROM_CDMA(RIL_REQUEST_CDMA_SMS_ACKNOWLEDGE, RIL_E_SUCCESS, token);
    RTST_EXPECTED_RIL_VOID_RESPONSE(RIL_REQUEST_CDMA_SMS_ACKNOWLEDGE, RIL_E_SUCCESS);
    RTST_CASE_END();
}


TEST(CdmaSmsTest, MT_CONFLICT_2) {
    RTST_CASE_BEGIN();
    RTST_RIL_URC_FROM_GSM(RIL_LOCAL_GSM_UNSOL_CDMA_SMS_SPECIFIC_FROM_GSM, 1, RTST_STRING,
            "0000021002020702c4d48a19a088060100080b0003200010010410124480");
    RTST_EXPECTED_RIL_URC_NC(RIL_UNSOL_RESPONSE_CDMA_NEW_SMS, 34,
        RTST_INT32, "4098",  // teleServiceId
        RTST_INT32, "0",     // servicePresent
        RTST_INT32, "0",     // serviceCategory
        RTST_INT32, "0",     // address_digit_mode
        RTST_INT32, "0",     // address_nbr_mode
        RTST_INT32, "0",     // address_nbr_type
        RTST_INT32, "0",     // address_nbr_plan
        RTST_INT32, "11",    // address_nbr_of_digits
        RTST_INT32, "1",     // number 13522866822
        RTST_INT32, "3",
        RTST_INT32, "5",
        RTST_INT32, "2",
        RTST_INT32, "2",
        RTST_INT32, "8",
        RTST_INT32, "6",
        RTST_INT32, "6",
        RTST_INT32, "8",
        RTST_INT32, "2",
        RTST_INT32, "2",
        RTST_INT32, "0",    // sub address type
        RTST_INT32, "0",    // subaddr_odd
        RTST_INT32, "0",    // sub address digit
        RTST_INT32, "11",    // bearer data length
        RTST_INT32, "0",
        RTST_INT32, "3",
        RTST_INT32, "32",
        RTST_INT32, "0",
        RTST_INT32, "16",
        RTST_INT32, "1",
        RTST_INT32, "4",
        RTST_INT32, "16",
        RTST_INT32, "18",
        RTST_INT32, "68",
        RTST_INT32, "128");
    RTST_CLEAN_CDMA_RIL_SOCKET_DATA();
    RTST_CLEAN_GSM_RIL_SOCKET_DATA();

    RTST_RIL_URC_FROM_CDMA_NC(RIL_UNSOL_RESPONSE_CDMA_NEW_SMS, 34,
        RTST_INT32, "4098",  // teleServiceId
        RTST_INT32, "0",     // servicePresent
        RTST_INT32, "0",     // serviceCategory
        RTST_INT32, "0",     // address_digit_mode
        RTST_INT32, "0",     // address_nbr_mode
        RTST_INT32, "0",     // address_nbr_type
        RTST_INT32, "0",     // address_nbr_plan
        RTST_INT32, "11",    // address_nbr_of_digits
        RTST_INT32, "1",     // number 13522866822
        RTST_INT32, "3",
        RTST_INT32, "5",
        RTST_INT32, "2",
        RTST_INT32, "2",
        RTST_INT32, "8",
        RTST_INT32, "6",
        RTST_INT32, "6",
        RTST_INT32, "8",
        RTST_INT32, "2",
        RTST_INT32, "2",
        RTST_INT32, "0",    // sub address type
        RTST_INT32, "0",    // subaddr_odd
        RTST_INT32, "0",    // sub address digit
        RTST_INT32, "11",    // bearer data length
        RTST_INT32, "0",
        RTST_INT32, "3",
        RTST_INT32, "32",
        RTST_INT32, "0",
        RTST_INT32, "16",
        RTST_INT32, "1",
        RTST_INT32, "4",
        RTST_INT32, "16",
        RTST_INT32, "18",
        RTST_INT32, "68",
        RTST_INT32, "128");


    int token;
    RTST_EXPECTED_RIL_REQUEST_TO_CDMA_RIL_NC(RIL_REQUEST_CDMA_SMS_ACKNOWLEDGE,
        token,
        2,
        RTST_INT32, "2",
        RTST_INT32, "33"
        );
    RTST_RIL_VOID_RESPONSE_FROM_CDMA(RIL_REQUEST_CDMA_SMS_ACKNOWLEDGE, RIL_E_SUCCESS, token);
    acknowledgeLastIncomingCdmaSms(RFX_SLOT_ID_0, 0, 0);
    RTST_EXPECTED_RIL_REQUEST_TO_GSM_RIL(
            RIL_LOCAL_REQUEST_CDMA_SMS_SPECIFIC_TO_GSM,
            token,
            3,
            RTST_STRING, "MTCNMASMS",
            RTST_STRING, "AT+C2KCNMA=13, \"02040702c4d48a19a088070100\"",
            RTST_STRING, "");
    RTST_RIL_RESPONSE_FROM_GSM(RIL_LOCAL_REQUEST_CDMA_SMS_SPECIFIC_TO_GSM, RIL_E_SUCCESS,
        token,
        2,
        RTST_STRING, "MTCNMASMS",
        RTST_STRING, "OK");
    RTST_EXPECTED_RIL_VOID_RESPONSE(RIL_REQUEST_CDMA_SMS_ACKNOWLEDGE, RIL_E_SUCCESS);
    RTST_CASE_END();
}

