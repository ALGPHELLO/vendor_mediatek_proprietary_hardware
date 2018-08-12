/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 *
 * MediaTek Inc. (C) 2010. All rights reserved.
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
#include "rfx_gt_log.h"
#include "RfxRilUtils.h"
#include "utils/String8.h"


/*****************************************************************************
 * Implementation
 *****************************************************************************/
using ::android::String8;

#define    LOG_BUF_SIZE 1024

bool __rfx_is_gt_mode() {
    return RfxRilUtils::getRilRunMode() == RilRunMode::RIL_RUN_MODE_MOCK;
}

void __rfx_android_buffer_print_V(const char *tag, const char *fmt, ...) {
    va_list ap;
    char buf[LOG_BUF_SIZE];
    String8 tagString = String8::format("%s%s", "[GT]", tag);
    va_start(ap, fmt);
    vsnprintf(buf, LOG_BUF_SIZE, fmt, ap);
    va_end(ap);

    __android_log_buf_write(LOG_ID_RADIO, ANDROID_LOG_VERBOSE, tagString.string(), buf);
}

void __rfx_android_buffer_print_D(const char *tag, const char *fmt, ...) {
    va_list ap;
    char buf[LOG_BUF_SIZE];
    String8 tagString = String8::format("%s%s", "[GT]", tag);
    va_start(ap, fmt);
    vsnprintf(buf, LOG_BUF_SIZE, fmt, ap);
    va_end(ap);

   __android_log_buf_write(LOG_ID_RADIO, ANDROID_LOG_DEBUG, tagString.string(), buf);
}
void __rfx_android_buffer_print_E(const char *tag, const char *fmt, ...) {
    va_list ap;
    char buf[LOG_BUF_SIZE];
    String8 tagString = String8::format("%s%s", "[GT]", tag);
    va_start(ap, fmt);
    vsnprintf(buf, LOG_BUF_SIZE, fmt, ap);
    va_end(ap);

    __android_log_buf_write(LOG_ID_RADIO, ANDROID_LOG_ERROR, tagString.string(), buf);
}
void __rfx_android_buffer_print_I(const char *tag, const char *fmt, ...) {
    va_list ap;
    char buf[LOG_BUF_SIZE];
    String8 tagString = String8::format("%s%s", "[GT]", tag);
    va_start(ap, fmt);
    vsnprintf(buf, LOG_BUF_SIZE, fmt, ap);
    va_end(ap);

    __android_log_buf_write(LOG_ID_RADIO, ANDROID_LOG_INFO, tagString.string(), buf);
}
void __rfx_android_buffer_print_W(const char *tag, const char *fmt, ...) {
    va_list ap;
    char buf[LOG_BUF_SIZE];
    String8 tagString = String8::format("%s%s", "[GT]", tag);
    va_start(ap, fmt);
    vsnprintf(buf, LOG_BUF_SIZE, fmt, ap);
    va_end(ap);
     __android_log_buf_write(LOG_ID_RADIO, ANDROID_LOG_WARN, tagString.string(), buf);
}


