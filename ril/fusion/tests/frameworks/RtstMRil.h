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
#ifndef __RTST_M_RIL_H__
#define __RTST_M_RIL_H__
/*****************************************************************************
 * Include
 *****************************************************************************/
#include "RfxDefs.h"

#define RFX_MAX_CHANNEL_NUM (RIL_CHANNEL_OFFSET)


//
typedef enum {
    RIL_URC,
    RIL_CMD_1,
    RIL_CMD_2,
    RIL_CMD_3,
    RIL_CMD_4, /* split data and nw command channel */
    RIL_ATCI,
#ifdef MTK_MUX_CHANNEL_64
    RIL_CMD_7,
    RIL_CMD_8,
    RIL_CMD_9,
    RIL_CMD_10,
    RIL_CMD_11,
#endif
#ifdef MTK_IMS_CHANNEL_SUPPORT
    RIL_CMD_IMS,
#endif
    RIL_CHANNEL_OFFSET,

    RIL_URC2 = RIL_CHANNEL_OFFSET,
    RIL_CMD2_1,
    RIL_CMD2_2,
    RIL_CMD2_3,
    RIL_CMD2_4, /* split data and nw command channel */
    RIL_ATCI2,
#ifdef MTK_MUX_CHANNEL_64
    RIL_CMD2_7,
    RIL_CMD2_8,
    RIL_CMD2_9,
    RIL_CMD2_10,
    RIL_CMD2_11,
#endif
#ifdef MTK_IMS_CHANNEL_SUPPORT
    RIL_CMD2_IMS,
#endif
    RIL_CHANNEL_SET3_OFFSET,
    RIL_URC3 = RIL_CHANNEL_SET3_OFFSET,
    RIL_CMD3_1,
    RIL_CMD3_2,
    RIL_CMD3_3,
    RIL_CMD3_4,
    RIL_ATCI3,
#ifdef MTK_MUX_CHANNEL_64
    RIL_CMD3_7,
    RIL_CMD3_8,
    RIL_CMD3_9,
    RIL_CMD3_10,
    RIL_CMD3_11,
#endif
#ifdef MTK_IMS_CHANNEL_SUPPORT
    RIL_CMD3_IMS,
#endif
    RIL_CHANNEL_SET4_OFFSET,
    RIL_URC4 = RIL_CHANNEL_SET4_OFFSET,
    RIL_CMD4_1,
    RIL_CMD4_2,
    RIL_CMD4_3,
    RIL_CMD4_4,
    RIL_ATCI4,
#ifdef MTK_MUX_CHANNEL_64
    RIL_CMD4_7,
    RIL_CMD4_8,
    RIL_CMD4_9,
    RIL_CMD4_10,
    RIL_CMD4_11,
#endif
#ifdef MTK_IMS_CHANNEL_SUPPORT
    RIL_CMD4_IMS,
#endif
    RIL_SUPPORT_CHANNELS
} RILChannelId;

/*****************************************************************************
 * class RtstMRil
 *****************************************************************************/
/*
 * The class that is used to configure the vendor ril
 */
class RtstMRil {
// External Method
public:
    /*
     * Enable emulator mode for vendor ril
     *
     * RETURNS: void
     */
    static void setEmulatorMode();

    /*
     * Replace the channel fd when emulator mode for vendor ril
     *
     * RETURNS: void
     */
    static void setChannelFd(
        int fd[MAX_SIM_COUNT][RFX_MAX_CHANNEL_NUM] // [IN] the channel fd to replace with
    );

    /*
     * Set the RIL Env to vendor ril
     *
     * RETURNS: void
     */
    static void setRilEnv();

    static void setCallbackForStatusUpdate();
};
#endif /* __RTST_M_RIL_H__ */