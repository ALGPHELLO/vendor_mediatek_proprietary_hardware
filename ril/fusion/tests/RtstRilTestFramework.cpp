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
#include "RfxController.h"
#include "RfxRootController.h"
#include "RfxClassInfo.h"
#include "RfxSlotRootController.h"
#include "RfxIntsData.h"
#include "RfxStatusDefs.h"

/*****************************************************************************
 * Test Cases
 *****************************************************************************/
TEST(RtfTest, RtstInitAt) {
    RTST_INIT_AT_CMD(A, "AT+XXX", 1, "OK");
    RTST_INIT_AT_CMD(B, "AT+YYY=?", 2, "+YYY:1,0", "OK");
    {
        RtstInitAtRsp * p = RtstEnv::get()->getInitAtRsp(String8("AT+XXX\r"));
        const Vector<String8> & v = p->getAtResponse();
        ASSERT_STREQ(v[0].string(), "OK\r");
    }
    {
        const char buf[] = "AT+YYY=?\r";
        RtstInitAtRsp * p = RtstEnv::get()->getInitAtRsp(String8(buf));
        const Vector<String8> & v = p->getAtResponse();
        ASSERT_STREQ(v[0].string(), "+YYY:1,0\r");
        ASSERT_STREQ(v[1].string(), "OK\r");
    }
    RTST_INIT_SYS_PROP("A", "B");
}


TEST(RtfTest, RtstData) {
    {
        RtstData data;
        // Test the UNDEFINED type
        ASSERT_EQ(data.getType(), RtstData::UNDEFINE);
    }

    {
        // Test the VOID type
        RtstVoidData data;
        ASSERT_EQ(data.getType(), RtstData::VOID);
    }
}


TEST(RtfTest, RtstInt32Data) {
    // Test the INT32 type
    RtstInt32Data data;
    data.setData("15");
    ASSERT_EQ(data.getType(), RtstData::INT32);
    ASSERT_EQ(data.toInt32(), 15);
    Parcel p1;
    data.toParcel(p1);
    int32_t intData1;
    p1.setDataPosition(0);
    p1.readInt32(&intData1);
    ASSERT_EQ(intData1, 15);


    data.setData("-16");
    ASSERT_EQ(data.toInt32(), -16);
    Parcel p2;
    data.toParcel(p2);
    int32_t intData2;
    p2.setDataPosition(0);
    p2.readInt32(&intData2);
    ASSERT_EQ(intData2, -16);
}


TEST(RtfTest, RtstInt64Data) {
    RtstInt64Data data;
    // Test the INT64 type
    data.setData("4294967297");
    ASSERT_EQ(data.getType(), RtstData::INT64);
    ASSERT_EQ(data.toInt64(), 0x100000001);

    Parcel p1;
    data.toParcel(p1);
    int64_t intData1;
    p1.setDataPosition(0);
    p1.readInt64(&intData1);
    ASSERT_EQ(intData1, 0x100000001);

    data.setData("-4294967298");
    ASSERT_EQ(data.toInt64(), -4294967298);
    Parcel p2;
    data.toParcel(p2);
    int64_t intData2;
    p2.setDataPosition(0);
    p2.readInt64(&intData2);
    ASSERT_EQ(intData2, -4294967298);
}


TEST(RtfTest, RtstStringData) {
    RtstStringData data;
    // Test the STRING type
    data.setData("AT+ECSCB=0");
    ASSERT_EQ(data.getType(), RtstData::STRING);
    ASSERT_STREQ("AT+ECSCB=0", data.toString());

    Parcel p1;
    data.toParcel(p1);
    p1.setDataPosition(0);
    char *str = RtstUtils::getStringFromParcel(p1);
    ASSERT_TRUE(str != NULL);
    String8 temp(str);
    free(str);
    ASSERT_STREQ("AT+ECSCB=0", temp.string());
}


TEST(RtfTest, RtstRawData) {
    RtstRawData data;
    // Test the RAW type
    data.setData("12345678FF");
    ASSERT_EQ(data.getType(), RtstData::RAW);
    Vector<char> raw;
    data.toRaw(raw);
    ASSERT_EQ((int)raw.size(), 5);

    ASSERT_EQ(raw[0], 0x12);
    ASSERT_EQ(raw[1], 0x34);
    ASSERT_EQ(raw[2], 0x56);
    ASSERT_EQ(raw[3], 0x78);
    ASSERT_EQ(raw[4], 0xFF);

    Parcel p;
    data.toParcel(p);
    p.setDataPosition(0);

    int len;
    p.readInt32(&len);
    ASSERT_EQ(5, len);
    unsigned char *x = (unsigned char *)p.readInplace(len);
    ASSERT_EQ(x[0], 0x12);
    ASSERT_EQ(x[1], 0x34);
    ASSERT_EQ(x[2], 0x56);
    ASSERT_EQ(x[3], 0x78);
    ASSERT_EQ(x[4], 0xFF);
}

TEST(RtfTest, RtstDataSequency_String) {
    RtstDataSequency seq1;
    seq1.appendWith(1, RTST_STRING, "AT+EGTYPE=4");
    Parcel p1;
    seq1.toParcel(p1);
    p1.setDataPosition(0);
    char *str = RtstUtils::getStringFromParcel(p1);
    ASSERT_TRUE(str != NULL);
    String8 temp(str);
    free(str);
    ASSERT_STREQ("AT+EGTYPE=4", temp.string());
    ASSERT_EQ(p1.dataSize(), p1.dataPosition());

    seq1.appendWith(1, RTST_STRING, "AT+EGTYPE=3");

    Parcel p2;
    seq1.toParcel(p2);
    p2.setDataPosition(0);

    int len;
    p2.readInt32(&len);
    ASSERT_EQ(2, len);
    str = RtstUtils::getStringFromParcel(p2);
    ASSERT_TRUE(str != NULL);
    String8 temp2(str);
    free(str);
    ASSERT_STREQ("AT+EGTYPE=4", temp2.string());

    str = RtstUtils::getStringFromParcel(p2);
    ASSERT_TRUE(str != NULL);
    String8 temp3(str);
    free(str);
    ASSERT_STREQ("AT+EGTYPE=3", temp3.string());
    ASSERT_EQ(p2.dataSize(), p2.dataPosition());

    Vector<const char *> datas;
    seq1.getDatas(datas);
    ASSERT_STREQ(datas[0], "AT+EGTYPE=4");
    ASSERT_STREQ(datas[1], "AT+EGTYPE=3");
    ASSERT_EQ(datas.size(), 2UL);
}


TEST(RtfTest, RtstDataSequency_Int32) {
    RtstDataSequency seq1;
    seq1.appendWith(1, RTST_INT32, "15");
    Parcel p1;
    seq1.toParcel(p1);
    p1.setDataPosition(0);

    int len;
    p1.readInt32(&len);
    ASSERT_EQ(1, len);

    int value;
    p1.readInt32(&value);
    ASSERT_EQ(15, value);
    ASSERT_EQ(p1.dataSize(), p1.dataPosition());

    seq1.appendWith(2, RTST_INT32, "-126", RTST_INT32, "65535");

    Parcel p2;
    seq1.toParcel(p2);
    p2.setDataPosition(0);

    p2.readInt32(&len);
    ASSERT_EQ(3, len);

    p2.readInt32(&value);
    ASSERT_EQ(15, value);

    p2.readInt32(&value);
    ASSERT_EQ(-126, value);

    p2.readInt32(&value);
    ASSERT_EQ(65535, value);
    ASSERT_EQ(p2.dataSize(), p2.dataPosition());

    Vector<const char *> datas;
    seq1.getDatas(datas);
    ASSERT_STREQ(datas[0], "15");
    ASSERT_STREQ(datas[1], "-126");
    ASSERT_STREQ(datas[2], "65535");
    ASSERT_EQ(datas.size(), 3UL);
}


TEST(RtfTest, RtstDataSequency_VOID) {
    RtstDataSequency seq1;
    seq1.appendWith(1, RTST_VOID, "");
    Parcel p1;
    seq1.toParcel(p1);
    ASSERT_EQ(0, (int)p1.dataPosition());

    Vector<const char *> datas;
    seq1.getDatas(datas);
    ASSERT_STREQ(datas[0], "");
    ASSERT_EQ(datas.size(), 1UL);
}


TEST(RtfTest, RtstDataSequency_MIXED) {
    RtstDataSequency seq1;
    seq1.appendWith(4, RTST_INT32, "250", RTST_INT64, "-4294967298", RTST_STRING, "AT+EGTYPE=2", RTST_RAW, "FED359C3ABDc");
    Parcel p1;
    seq1.toParcel(p1);
    p1.setDataPosition(0);

    int32_t int32Value;
    p1.readInt32(&int32Value);
    ASSERT_EQ(250, int32Value);

    int64_t int64Value;
    p1.readInt64(&int64Value);
    ASSERT_EQ(-4294967298, int64Value);

    char *str = RtstUtils::getStringFromParcel(p1);
    ASSERT_TRUE(str != NULL);
    String8 temp(str);
    free(str);
    ASSERT_STREQ("AT+EGTYPE=2", temp.string());

    int32_t len;
    p1.readInt32(&len);
    ASSERT_EQ(len, 6);
    unsigned char *x = (unsigned char *)p1.readInplace(len);

    ASSERT_EQ(x[0], 0xFE);
    ASSERT_EQ(x[1], 0xD3);
    ASSERT_EQ(x[2], 0x59);
    ASSERT_EQ(x[3], 0xC3);
    ASSERT_EQ(x[4], 0xAB);
    ASSERT_EQ(x[5], 0xDC);
    ASSERT_EQ(p1.dataSize(), p1.dataPosition());

    Vector<const char *> datas;
    seq1.getDatas(datas);
    ASSERT_STREQ(datas[0], "250");
    ASSERT_STREQ(datas[1], "-4294967298");
    ASSERT_STREQ(datas[2], "AT+EGTYPE=2");
    ASSERT_STREQ(datas[3], "FED359C3ABDc");
    ASSERT_EQ(datas.size(), 4UL);
}


TEST(RtfTest, RTST_CASE_BEGIN_END) {
    RtstEnv::get()->setMode(RtstEnv::TEST_DATA_ONLY);

    // default begin, rfx_assert = false, step_run = true
    RTST_CASE_BEGIN_FOR_TEST(false, true);
    ASSERT_EQ(_caseril.getName(), String8("ril"));
    ASSERT_FALSE(_caseril.isUseRfxAssert());
    ASSERT_TRUE(_caseril.isStepRun());
    RTST_CASE_END();

    // rfx_assert = true, step_run = true
    RTST_CASE_BEGIN_FOR_TEST(true, true);
    ASSERT_EQ(_caseril.getName(), String8("ril"));
    ASSERT_TRUE(_caseril.isUseRfxAssert());
    ASSERT_TRUE(_caseril.isStepRun());
    RTST_CASE_END();

    // rfx_assert = true, step_run = false
    RTST_CASE_BEGIN_FOR_TEST(true, false);
    ASSERT_EQ(_caseril.getName(), String8("ril"));
    ASSERT_TRUE(_caseril.isUseRfxAssert());
    ASSERT_FALSE(_caseril.isStepRun());
    RTST_CASE_END();

    // rfx_assert = false, step_run = false
    RTST_CASE_BEGIN_FOR_TEST(false, false);
    ASSERT_EQ(_caseril.getName(), String8("ril"));
    ASSERT_FALSE(_caseril.isStepRun());
    ASSERT_FALSE(_caseril.isUseRfxAssert());
    RTST_CASE_END();
    RtstEnv::get()->setMode(RtstEnv::WORK);
}


TEST(RtfTest, RTST_RIL_REQUEST_AT_RIL_RESPONSE_1) {
    RtstEnv::get()->setMode(RtstEnv::TEST_DATA_ONLY);

    RTST_CASE_BEGIN_FOR_TEST(false, false);

    RTST_RIL_REQUEST(RIL_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION, 1, "int32", "0");
    RTST_AT_CMD(RIL_CMD_PROXY_1, "AT+ECSCB=0", 1, "OK");
    RTST_EXPECTED_RIL_RESPONSE(RIL_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION, RIL_E_SUCCESS, 1, "string", "victor");

    Vector<RtstItemBase *> items = _caseril.getItems();
    ASSERT_EQ(4U, items.size());
    {
        RtstRilReqItem *item = (RtstRilReqItem *)items[0];
        ASSERT_EQ(item->getRilCmdId(), RIL_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION);
        ASSERT_EQ(item->getSlotId(), RFX_SLOT_ID_0);
        ASSERT_TRUE(item->isMockType());
        ASSERT_FALSE(item->isExpectedType());
        Parcel p;
        item->getDataSequency().toParcel(p);
        p.setDataPosition(0);
        int len;
        p.readInt32(&len);
        ASSERT_EQ(len, 1);
        int value = 15;
        p.readInt32(&value);
        ASSERT_EQ(value, 0);
    }

    {
        RtstExpectedAtItem *item = (RtstExpectedAtItem*)items[1];
        ASSERT_EQ(item->getChannelId(), RIL_CMD_PROXY_1);
        ASSERT_EQ(item->getSlotId(), RFX_SLOT_ID_0);
        ASSERT_FALSE(item->isMockType());
        ASSERT_TRUE(item->isExpectedType());
        Vector<const char *> datas;
        item->getDataSequency().getDatas(datas);
        ASSERT_EQ(1UL, datas.size());
        ASSERT_STREQ("AT+ECSCB=0", datas[0]);
    }

    {
        RtstAtResponseItem *item = (RtstAtResponseItem *)items[2];
        ASSERT_EQ(item->getChannelId(), RIL_CMD_PROXY_1);
        ASSERT_EQ(item->getSlotId(), RFX_SLOT_ID_0);
        ASSERT_TRUE(item->isMockType());
        ASSERT_FALSE(item->isExpectedType());
        Vector<const char *> datas;
        item->getDataSequency().getDatas(datas);
        ASSERT_EQ(1UL, datas.size());
        ASSERT_STREQ("OK", datas[0]);
    }

    {
        RtstExpectedRilRspItem * item = (RtstExpectedRilRspItem *)items[3];
        ASSERT_EQ(item->getRilCmdId(), RIL_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION);
        ASSERT_EQ(item->getSlotId(), RFX_SLOT_ID_0);
        ASSERT_EQ(item->getErrorCode(), RIL_E_SUCCESS);
        ASSERT_FALSE(item->isMockType());
        ASSERT_TRUE(item->isExpectedType());
        Parcel p;
        item->getDataSequency().toParcel(p);
        p.setDataPosition(0);
        char *str = RtstUtils::getStringFromParcel(p);
        ASSERT_TRUE(str != NULL);
        String8 temp(str);
        free(str);
        ASSERT_STREQ("victor", temp.string());
    }

    RTST_CASE_END();
    RtstEnv::get()->setMode(RtstEnv::WORK);
}

TEST(RtfTest, RTST_RIL_REQUEST_AT_RIL_RESPONSE_2) {
    RtstEnv::get()->setMode(RtstEnv::TEST_DATA_ONLY);
    RTST_CASE_BEGIN_FOR_TEST(false, false);
    RTST_RIL_REQUEST_WITH_SLOT_NC(
         RIL_REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG,
         RFX_SLOT_ID_1, 10,
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

    RTST_AT_CMD_WITH_SLOT(RFX_SLOT_ID_1, RIL_CMD_PROXY_1,
        "AT+ECSCBCHA?", 2, "+ECSCBCHA:1,\"4096-4097\"", "OK");
    RTST_AT_CMD_WITH_SLOT(RFX_SLOT_ID_1, RIL_CMD_PROXY_1,
        "AT+ECSCBCHA=0,\"4096-4097\"", 1, "OK");
    RTST_AT_CMD_WITH_SLOT(RFX_SLOT_ID_1, RIL_CMD_PROXY_1,
        "AT+ECSCBLAN?", 2, "+ECSCBLAN:1,\"1-1\"", "OK");
    RTST_AT_CMD_WITH_SLOT(RFX_SLOT_ID_1, RIL_CMD_PROXY_1,
        "AT+ECSCBLAN=0,\"1-1\"", 1, "OK");
    RTST_AT_CMD_WITH_SLOT(RFX_SLOT_ID_1, RIL_CMD_PROXY_1,
        "AT+ECSCBCHA=1,\"4096-4096\"", 1, "OK");
    RTST_AT_CMD_WITH_SLOT(RFX_SLOT_ID_1, RIL_CMD_PROXY_1,
        "AT+ECSCBCHA=1,\"4098-4099\"", 1, "OK");
    RTST_AT_CMD_WITH_SLOT(RFX_SLOT_ID_1, RIL_CMD_PROXY_1,
        "AT+ECSCBLAN=1,\"1-1\"", 1, "OK");
    RTST_EXPECTED_RIL_VOID_RESPONSE_WITH_SLOT(
        RIL_REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG,
        RFX_SLOT_ID_1,
        RIL_E_SUCCESS);

    Vector<RtstItemBase *> items = _caseril.getItems();
    ASSERT_EQ(16U, items.size());

    {
        RtstRilReqItem *item = (RtstRilReqItem *)items[0];
        ASSERT_EQ(item->getRilCmdId(), RIL_REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG);
        ASSERT_EQ(item->getSlotId(), RFX_SLOT_ID_1);
        ASSERT_TRUE(item->isMockType());
        ASSERT_FALSE(item->isExpectedType());
        Parcel p;
        item->getDataSequency().toParcel(p);
        p.setDataPosition(0);
        int32_t v[10];
        int32_t c[10] = {3, 4096, 1, 1, 4098, 1, 1, 4099, 1, 1};
        for (int i = 0; i < 10; i++) {
            p.readInt32(&v[i]);
            ASSERT_EQ(v[i], c[i]);
        }
    }
    {
        for (int i = 0; i < 7; i++ ) {
            const char *at[] = {
                "AT+ECSCBCHA?",
                "AT+ECSCBCHA=0,\"4096-4097\"",
                "AT+ECSCBLAN?",
                "AT+ECSCBLAN=0,\"1-1\"",
                "AT+ECSCBCHA=1,\"4096-4096\"",
                "AT+ECSCBCHA=1,\"4098-4099\"",
                "AT+ECSCBLAN=1,\"1-1\""};
            RtstExpectedAtItem *item = (RtstExpectedAtItem*)items[1 + 2 * i];
            ASSERT_EQ(item->getChannelId(), RIL_CMD_PROXY_1);
            ASSERT_EQ(item->getSlotId(), RFX_SLOT_ID_1);
            ASSERT_FALSE(item->isMockType());
            ASSERT_TRUE(item->isExpectedType());
            Vector<const char *> datas;
            item->getDataSequency().getDatas(datas);
            ASSERT_EQ(1U, datas.size());
            ASSERT_STREQ(at[i], datas[0]);
        }
    }

    {
        int count = 0;
        const char *rsp[] = {"+ECSCBCHA:1,\"4096-4097\"", "+ECSCBLAN:1,\"1-1\""};
        for (int i = 0; i < 7; i++) {
            size_t nums[] = {2U, 1U, 2U, 1U, 1U, 1U, 1U};
            RtstAtResponseItem *item = (RtstAtResponseItem *)items[2 + 2 * i];
            ASSERT_EQ(item->getChannelId(), RIL_CMD_PROXY_1);
            ASSERT_EQ(item->getSlotId(), RFX_SLOT_ID_1);

            ASSERT_TRUE(item->isMockType());
            ASSERT_FALSE(item->isExpectedType());

            Vector<const char *> datas;
            item->getDataSequency().getDatas(datas);
            ASSERT_EQ(nums[i], datas.size());
            if (nums[i] == 1) {
                ASSERT_STREQ("OK", datas[0]);
            } else {
                ASSERT_STREQ(rsp[count++], datas[0]);
                ASSERT_STREQ("OK", datas[1]);
            }
        }
    }

    {
        RtstExpectedRilRspItem * item = (RtstExpectedRilRspItem *)items[15];
        ASSERT_EQ(item->getRilCmdId(), RIL_REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG);
        ASSERT_EQ(item->getSlotId(), RFX_SLOT_ID_1);
        ASSERT_EQ(item->getErrorCode(), RIL_E_SUCCESS);
        ASSERT_FALSE(item->isMockType());
        ASSERT_TRUE(item->isExpectedType());

        Parcel p;
        item->getDataSequency().toParcel(p);
        ASSERT_EQ(p.dataSize(), 0U);
    }
    RTST_CASE_END();
    RtstEnv::get()->setMode(RtstEnv::WORK);
}


TEST(RtfTest, RTST_URC_STRING_RIL_URC) {
    RtstEnv::get()->setMode(RtstEnv::TEST_DATA_ONLY);

    RTST_CASE_BEGIN_FOR_TEST(false, false);
    RTST_URC_STRING("+ECARDESNME:00000000,12345678");
    RTST_EXPECTED_RIL_URC(RIL_UNSOL_CDMA_CARD_INITIAL_ESN_OR_MEID, 1,
        RTST_STRING,
        "00000000,12345678");

    Vector<RtstItemBase *> items = _caseril.getItems();
    ASSERT_EQ(2U, items.size());
    {
        RtstUrcStringItem *item = (RtstUrcStringItem *)items[0];
        ASSERT_EQ(item->getChannelId(), RIL_CMD_PROXY_URC);
        ASSERT_EQ(item->getSlotId(), RFX_SLOT_ID_0);
        ASSERT_TRUE(item->isMockType());
        ASSERT_FALSE(item->isExpectedType());

        Vector<const char *> datas;
        item->getDataSequency().getDatas(datas);
        ASSERT_EQ(1U, datas.size());
        ASSERT_STREQ("+ECARDESNME:00000000,12345678", datas[0]);
    }
    {
        RtstExpectedRilUrcItem *item = (RtstExpectedRilUrcItem *)items[1];
        ASSERT_EQ(item->getRilCmdId(), RIL_UNSOL_CDMA_CARD_INITIAL_ESN_OR_MEID);
        ASSERT_EQ(item->getSlotId(), RFX_SLOT_ID_0);
        ASSERT_FALSE(item->isMockType());
        ASSERT_TRUE(item->isExpectedType());
        Parcel p;
        item->getDataSequency().toParcel(p);
        p.setDataPosition(0);
        char *str = RtstUtils::getStringFromParcel(p);
        ASSERT_TRUE(str != NULL);
        String8 temp(str);
        free(str);
        ASSERT_STREQ("00000000,12345678", temp.string());
    }
    RTST_CASE_END();
    RtstEnv::get()->setMode(RtstEnv::WORK);
}

TEST(RtfTest, RTST_SYSTEM_PROP) {
    RtstEnv::get()->setMode(RtstEnv::TEST_DATA_ONLY);

    RTST_CASE_BEGIN_FOR_TEST(false, false);
    RTST_SYSTEM_PROPERTY("ro.mtk_fd_support", "1");
    RTST_EXPECTED_SYSTEM_PROPERTY("ril.fd.mode", "1");
    Vector<RtstItemBase *> items = _caseril.getItems();
    ASSERT_EQ(2U, items.size());
    {
        RtstSysPropItem *item = (RtstSysPropItem *)items[0];
        ASSERT_STREQ(item->getKey().string(), "ro.mtk_fd_support");
        ASSERT_STREQ(item->getValue().string(), "1");
        ASSERT_TRUE(item->isMockType());
        ASSERT_FALSE(item->isExpectedType());
    }
    {
        RtstExpectedSysPropItem *item = (RtstExpectedSysPropItem *)items[1];
        ASSERT_STREQ(item->getKey().string(), "ril.fd.mode");
        ASSERT_STREQ(item->getValue().string(), "1");
        ASSERT_FALSE(item->isMockType());
        ASSERT_TRUE(item->isExpectedType());
    }
    RTST_CASE_END();
    RtstEnv::get()->setMode(RtstEnv::WORK);
}

/*
TEST(RtfTest, RTST_STATUS_KEY) {
    RtstEnv::get()->setMode(RtstEnv::TEST_DATA_ONLY);

    RTST_CASE_BEGIN_FOR_TEST(false, false);
    RTST_STATUS_VALUE(RFX_SLOT_ID_1, RFX_STATUS_KEY_NWS_MODE, RfxVariant(1));
    RTST_EXPECTED_STATUS_VALUE(RFX_SLOT_ID_UNKNOWN, RFX_STATUS_KEY_MAIN_CAPABILITY_SLOT, RfxVariant(1));
    Vector<RtstItemBase *> items = _caseril.getItems();
    ASSERT_EQ(2U, items.size());
    {
        RtstStatusItem * item = (RtstStatusItem *)items[0];
        ASSERT_EQ(item->getKey(),  RFX_STATUS_KEY_NWS_MODE);
        ASSERT_EQ(item->getSlotId(), RFX_SLOT_ID_1);
        ASSERT_EQ(item->getValue().asInt(), 1);
        ASSERT_TRUE(item->isMockType());
        ASSERT_FALSE(item->isExpectedType());
    }
    {
        RtstExpectedStatusItem *item = (RtstExpectedStatusItem *)items[1];
        ASSERT_EQ(item->getKey(),  RFX_STATUS_KEY_MAIN_CAPABILITY_SLOT);
        ASSERT_EQ(item->getSlotId(), RFX_SLOT_ID_UNKNOWN);
        ASSERT_EQ(item->getValue().asInt(), 1);
        ASSERT_FALSE(item->isMockType());
        ASSERT_TRUE(item->isExpectedType());
    }
    RTST_CASE_END();
    RtstEnv::get()->setMode(RtstEnv::WORK);
}
*/

TEST(RtfTest, RTST_RIL_REQUEST_AT_RIL_RESPONSE_3) {
    RtstEnv::get()->initForTest();
    RtstEnv::get()->setMode(RtstEnv::TEST);
    RtstEnv::get()->initTestThread();
    RTST_CASE_BEGIN_FOR_TEST(false, true);

    RTST_RIL_REQUEST(RIL_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION, 1, "int32", "0");
    RTST_AT_CMD(RIL_CMD_PROXY_1, "AT+ECSCB=0", 1, "OK");
    RTST_EXPECTED_RIL_RESPONSE(RIL_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION, RIL_E_SUCCESS, 1, "string", "victor");

    Vector<RtstItemBase *> items = _caseril.getItems();
    ASSERT_EQ(4U, items.size());
    {
        RtstRilReqItem *item = (RtstRilReqItem *)items[0];
        ASSERT_EQ(item->getRilCmdId(), RIL_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION);
        ASSERT_EQ(item->getSlotId(), RFX_SLOT_ID_0);
        ASSERT_TRUE(item->isMockType());
        ASSERT_FALSE(item->isExpectedType());
        Parcel p;
        item->getDataSequency().toParcel(p);
        p.setDataPosition(0);
        int len;
        p.readInt32(&len);
        ASSERT_EQ(len, 1);
        int value = 15;
        p.readInt32(&value);
        ASSERT_EQ(value, 0);
    }

    {
        RtstExpectedAtItem *item = (RtstExpectedAtItem*)items[1];
        ASSERT_EQ(item->getChannelId(), RIL_CMD_PROXY_1);
        ASSERT_EQ(item->getSlotId(), RFX_SLOT_ID_0);
        ASSERT_FALSE(item->isMockType());
        ASSERT_TRUE(item->isExpectedType());
        Vector<const char *> datas;
        item->getDataSequency().getDatas(datas);
        ASSERT_EQ(1UL, datas.size());
        ASSERT_STREQ("AT+ECSCB=0", datas[0]);
    }

    {
        RtstAtResponseItem *item = (RtstAtResponseItem *)items[2];
        ASSERT_EQ(item->getChannelId(), RIL_CMD_PROXY_1);
        ASSERT_EQ(item->getSlotId(), RFX_SLOT_ID_0);
        ASSERT_TRUE(item->isMockType());
        ASSERT_FALSE(item->isExpectedType());
        Vector<const char *> datas;
        item->getDataSequency().getDatas(datas);
        ASSERT_EQ(1UL, datas.size());
        ASSERT_STREQ("OK", datas[0]);
    }

    {
        RtstExpectedRilRspItem * item = (RtstExpectedRilRspItem *)items[3];
        ASSERT_EQ(item->getRilCmdId(), RIL_REQUEST_CDMA_SMS_BROADCAST_ACTIVATION);
        ASSERT_EQ(item->getSlotId(), RFX_SLOT_ID_0);
        ASSERT_EQ(item->getErrorCode(), RIL_E_SUCCESS);
        ASSERT_FALSE(item->isMockType());
        ASSERT_TRUE(item->isExpectedType());
        Parcel p;
        item->getDataSequency().toParcel(p);
        p.setDataPosition(0);
        char *str = RtstUtils::getStringFromParcel(p);
        ASSERT_TRUE(str != NULL);
        String8 temp(str);
        free(str);
        ASSERT_STREQ("victor", temp.string());
    }

    RTST_CASE_END();
    RtstEnv::get()->setMode(RtstEnv::WORK);
    RtstEnv::get()->deinitFd();
}

TEST(RtfTest, InitURC) {
    RtstEnv::get()->setMode(RtstEnv::TEST);
    Parcel p;
    p.writeInt32(0x55);
    p.writeInt32(2);
    p.writeInt32(66);
    p.writeInt32(77);
    RtstEnv::get()->addInitRilUrcData(RFX_SLOT_ID_0, (unsigned char*)p.data(), p.dataSize());
    RTST_CASE_BEGIN_FOR_TEST(false, true);
    RTST_EXPECTED_INIT_RIL_URC(0x55, 2, RTST_INT32, "66", RTST_INT32, "77");
    RTST_CASE_END();
    RTST_CASE_BEGIN_FOR_TEST(false, true);
    RTST_EXPECTED_INIT_RIL_URC_ALL_SLOT(0x55, 2, RTST_INT32, "66", RTST_INT32, "77");
    RTST_CASE_END();

    RtstEnv::get()->setMode(RtstEnv::WORK);
}


TEST(RtfTest, SystemProp) {
    RTST_INIT_SYS_PROP("A", "B");
    RTST_CASE_BEGIN();
    RTST_EXPECTED_SYSTEM_PROPERTY("A", "B");
    RTST_INIT_SYS_PROP("A", "C");
    RTST_EXPECTED_SYSTEM_PROPERTY("A", "C");
    RTST_CASE_END();
}



class RtstController : public RfxController {
    RFX_DECLARE_CLASS(RtstController);
public:
    virtual void onInit() {
            RfxController::onInit();
            logD("RTF", "RtstController:onInit %p slot = %d", this, getSlotId());
            getStatusManager()->registerStatusChanged(RFX_STATUS_KEY_EMERGENCY_MODE,
            RfxStatusChangeCallback(this, &RtstController::onRestrictedModeChanged));
        }
    virtual void onDeinit() {}
private:
    void onRestrictedModeChanged(RfxStatusKeyEnum key,
            RfxVariant old_value, RfxVariant value) {
            int oldMode = old_value.asInt();
            int Mode = value.asInt();
            sp<RfxMessage> urcToRilj;
            int data[3];
            data[0] = key;
            data[1] = oldMode;
            data[2] = Mode;
            urcToRilj = RfxMessage::obtainUrc(getSlotId(), RFX_MSG_URC_WORLD_MODE_CHANGED,
                    RfxIntsData(data, 3));
            responseToRilj(urcToRilj);

            RFX_LOG_D("RTF", "RtstController::onRestrictedModeChanged(%d, %d, %d)", key, oldMode, Mode);

        }
};
RFX_IMPLEMENT_CLASS("RtstController", RtstController, RfxController);


TEST(RtfTest, invokeStatusCb) {
    RTST_CASE_BEGIN();
    RfxController *root = RFX_OBJ_GET_INSTANCE(RfxRootController);
    RfxController *controller = root->findController(RFX_SLOT_ID_0,
        RFX_OBJ_CLASS_INFO(RfxSlotRootController));
    RtstController *obj;
    RFX_OBJ_CREATE(obj, RtstController, controller);
    RFX_LOG_D("RTF", "RtstController %p, parent = %p", obj, controller);
    RTST_INVOKE_STATUS_CB(
        RFX_SLOT_ID_0,
        RtstController,
        RFX_STATUS_KEY_EMERGENCY_MODE, RfxVariant(4),RfxVariant(3));
    RTST_CLEAN_ALL_RIL_SOCKET_DATA();

    RTST_INVOKE_STATUS_CB(
        RFX_SLOT_ID_0,
        RtstController,
        RFX_STATUS_KEY_EMERGENCY_MODE, RfxVariant(0),RfxVariant(1));
    RTST_EXPECTED_RIL_URC(RIL_UNSOL_WORLD_MODE_CHANGED,
        3,
        RTST_INT32, "38",
        RTST_INT32, "0",
        RTST_INT32, "1");
    RTST_CASE_END();
    sleep(1);
}

TEST(RtfTest, blockedStatus) {
    ASSERT_TRUE(RtstUtils::isBlockedStatusEmpty());
    RtstUtils::registerBlockedStatus(RFX_STATUS_KEY_CARD_TYPE);
    ASSERT_TRUE(RtstUtils::isBlockedStatus(RFX_STATUS_KEY_CARD_TYPE));
    RtstUtils::registerBlockedStatus(RFX_STATUS_KEY_CDMA_CARD_TYPE);
    ASSERT_TRUE(RtstUtils::isBlockedStatus(RFX_STATUS_KEY_CDMA_CARD_TYPE));
    ASSERT_FALSE(RtstUtils::isBlockedStatus(RFX_STATUS_KEY_CDMA_CARD_READY));
    RtstUtils::removeAllBlockedStatus();
    ASSERT_TRUE(RtstUtils::isBlockedStatusEmpty());
    RTST_CASE_BEGIN();
    RTST_REGISTER_BLOCKED_STATUS(RFX_STATUS_KEY_CARD_TYPE);
    RTST_STATUS_VALUE(RFX_SLOT_ID_0, RFX_STATUS_KEY_CARD_TYPE, RfxVariant(RFX_CARD_TYPE_SIM));
    RTST_EXPECTED_STATUS_VALUE(RFX_SLOT_ID_0, RFX_STATUS_KEY_CARD_TYPE, RfxVariant(RFX_CARD_TYPE_SIM));
    RTST_STATUS_VALUE(RFX_SLOT_ID_0, RFX_STATUS_KEY_CARD_TYPE, RfxVariant(RFX_CARD_TYPE_USIM));
    RTST_EXPECTED_STATUS_VALUE(RFX_SLOT_ID_0, RFX_STATUS_KEY_CARD_TYPE, RfxVariant(RFX_CARD_TYPE_USIM));
    RTST_STATUS_VALUE(RFX_SLOT_ID_0, RFX_STATUS_KEY_CARD_TYPE, RfxVariant(RFX_CARD_TYPE_CSIM));
    RTST_EXPECTED_STATUS_VALUE(RFX_SLOT_ID_0, RFX_STATUS_KEY_CARD_TYPE, RfxVariant(RFX_CARD_TYPE_CSIM));
    RTST_STATUS_VALUE(RFX_SLOT_ID_0, RFX_STATUS_KEY_CARD_TYPE, RfxVariant(RFX_CARD_TYPE_RUIM));
    RTST_EXPECTED_STATUS_VALUE(RFX_SLOT_ID_0, RFX_STATUS_KEY_CARD_TYPE, RfxVariant(RFX_CARD_TYPE_RUIM));

    RTST_CASE_END();
}


TEST(RtfTest, expect_At) {
    RTST_CASE_BEGIN();
    RTST_RIL_VOID_REQUEST(RIL_REQUEST_GET_SMS_RUIM_MEM_STATUS);
    RTST_AT_CMD(RIL_CMD_PROXY_1, "AT+EC2KCPMS?", 2, "+EC2KCPMS: \"SM\", 10, 32", "OK");
    RTST_AT_CMD_NO_ASSERT(RIL_CMD_PROXY_1, "AT+CPMS?", 2, "+CPMS: \"SM\", 10, 32", "OK");
    RTST_EXPECTED_RIL_RESPONSE_NC(RIL_REQUEST_GET_SMS_RUIM_MEM_STATUS, RIL_E_SUCCESS, 2,
        RTST_INT32, "10",
        RTST_INT32, "32");
    RTST_CASE_END();

}


TEST(RtfTest, SLEEP) {
    sleep(1);
}
