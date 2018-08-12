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
#include "RfxMclDispatcherThread.h"
#include "RfxMclMessage.h"
#include "RfxMessageId.h"
#include "RfxIntsData.h"

/*****************************************************************************
 * Test Cases
 *****************************************************************************/
TEST(FastDormancyTest, INITIALIZATION) {
        rfx_property_set("persist.mtk_epdg_support", "1");
        RTST_CASE_BEGIN();

        int capability = RAF_LTE;
        sp<RfxMclMessage> msg = RfxMclMessage::obtainEvent(RFX_MSG_EVENT_RADIO_CAPABILITY_UPDATED,
                RfxIntsData(&capability, 1), RIL_CMD_PROXY_5, 0);
        RfxMclDispatcherThread::enqueueMclMessage(msg);

        RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EFD=?", 2, "+EFD:(0-3)", "OK");
        RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EFD=2,0,50", 1, "OK");
        RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EFD=2,2,50", 1, "OK");
        RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EFD=2,1,150", 1, "OK");
        RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EFD=2,3,150", 1, "OK");
        RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EFD=1", 1, "OK");
        RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EEPDG=1", 1, "OK");

        RTST_CASE_END();
}


TEST(FastDormancyTest, RIL_REQUEST_SET_FD_MODE_ENABLE) {
    RTST_CASE_BEGIN();
    RTST_RIL_REQUEST(RIL_REQUEST_SET_FD_MODE, 1, RTST_INT32, "1");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EFD=1", 1, "OK");
    RTST_EXPECTED_RIL_VOID_RESPONSE(RIL_REQUEST_SET_FD_MODE, RIL_E_SUCCESS);
    RTST_CASE_END();
}


TEST(FastDormancyTest, RIL_REQUEST_SET_FD_MODE_SET_TIMER) {
    RTST_CASE_BEGIN();
    RTST_RIL_REQUEST(RIL_REQUEST_SET_FD_MODE, 3,
            RTST_INT32, "2",
            RTST_INT32, "2", // timerId=2: Screen Off + R8 FD
            RTST_INT32, "100" // 10s
            );
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EFD=2,2,100", 1, "OK");
    RTST_EXPECTED_RIL_VOID_RESPONSE(RIL_REQUEST_SET_FD_MODE, RIL_E_SUCCESS);
    RTST_CASE_END();
}


TEST(FastDormancyTest, RIL_REQUEST_SET_FD_MODE_SCREEN_STATUS) {
    RTST_CASE_BEGIN();
    RTST_RIL_REQUEST(RIL_REQUEST_SET_FD_MODE, 2,
            RTST_INT32, "3",
            RTST_INT32, "1" // Screen on
            );

    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EFD=3,1", 1, "OK");
    RTST_EXPECTED_RIL_VOID_RESPONSE(RIL_REQUEST_SET_FD_MODE, RIL_E_SUCCESS);
    RTST_CASE_END();
}

TEST(FastDormancyTest, RIL_REQUEST_SET_FD_MODE_ERROR) {
    RTST_CASE_BEGIN();
    RTST_RIL_REQUEST(RIL_REQUEST_SET_FD_MODE, 1, RTST_INT32, "0");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EFD=0", 1, "ERROR");
    RTST_EXPECTED_RIL_VOID_RESPONSE(RIL_REQUEST_SET_FD_MODE, RIL_E_MODEM_ERR);
    RTST_CASE_END();
}

TEST(FastDormancyTest, RIL_REQUEST_SET_FD_MODE_DISABLE) {
    RTST_CASE_BEGIN();
    RTST_RIL_REQUEST(RIL_REQUEST_SET_FD_MODE, 1, RTST_INT32, "0");
    RTST_AT_CMD(RIL_CMD_PROXY_5, "AT+EFD=0", 1, "OK");
    RTST_EXPECTED_RIL_VOID_RESPONSE(RIL_REQUEST_SET_FD_MODE, RIL_E_SUCCESS);
    RTST_CASE_END();
}
