# Copyright Statement:
#
# This software/firmware and related documentation ("MediaTek Software") are
# protected under relevant copyright laws. The information contained herein
# is confidential and proprietary to MediaTek Inc. and/or its licensors.
# Without the prior written permission of MediaTek inc. and/or its licensors,
# any reproduction, modification, use or disclosure of MediaTek Software,
# and information contained herein, in whole or in part, shall be strictly prohibited.
#
# MediaTek Inc. (C) 2010. All rights reserved.
#
# BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
# THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
# RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
# AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
# NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
# SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
# SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
# THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
# THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
# CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
# SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
# STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
# CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
# AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
# OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
# MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.


#
# Copyright (C) 2010 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := \
    MJCFramework.cpp \
    MJCScaler.cpp

LOCAL_CFLAGS += -DUSE_MTK_HW_VCODEC

LOCAL_CFLAGS += -DDYNAMIC_PRIORITY_ADJUSTMENT

LOCAL_CFLAGS += -DMTK_CLEARMOTION_SUPPORT

LOCAL_C_INCLUDES:= \
        $(TOP)/frameworks/av/include/media/stagefright \
        $(TOP)/$(MTK_ROOT)/frameworks/native/include/media/openmax \
        $(TOP)/$(MTK_ROOT)/frameworks/av/media/libstagefright/include/omx_core \
        $(TOP)/$(MTK_ROOT)/frameworks/av/media/libstagefright/include/ \
        $(TOP)/frameworks/av/media/libstagefright/include \
        $(TOP)/$(MTK_ROOT)/frameworks/av/include/media/omx_core \
        $(LOCAL_PATH)/../../../omx/inc \
        $(LOCAL_PATH)/../../../omx/osal \
        $(LOCAL_PATH)/../../inc \
        $(LOCAL_PATH)/../../osal \
        $(TOP)/$(MTK_ROOT)/kernel/include \
        $(MTK_PATH_SOURCE)/external/emi/inc \
        $(TOP)/frameworks/native/include/media/hardware \
        $(TOP)/frameworks/native/include/ \
        $(TOP)/system/core/libutils/include/utils \
        $(TOP)/system/core/include/system \
        $(TOP)/$(MTK_ROOT)/external/mhal/src/core/drv/inc \
        $(TOP)/$(MTK_ROOT)/kernel/include/linux \
        $(TOP)/$(MTK_ROOT)/kernel/include/linux/vcodec \
        $(MTK_PATH_PLATFORM)/hardware/vcodec/inc \
        $(TOP)/$(MTK_ROOT)/hardware/omx/inc \
        $(LOCAL_PATH)/../MtkOmxVdecEx \
        $(TOP)/$(MTK_ROOT)/hardware/rrc/inc \
        $(TOP)/vendor/mediatek/proprietary/external/libion_mtk/include \
        $(TOP)/vendor/mediatek/proprietary/external/include \
        $(TOP)/$(MTK_ROOT)/hardware/include \
        $(MTK_ROOT)/external/aee/binary/inc

LOCAL_C_INCLUDES += $(TOP)/$(MTK_ROOT)/hardware/perfservice/perfservicenative
LOCAL_C_INCLUDES += $(TOP)/$(MTK_PATH_SOURCE)/hardware/jpeg/include/mhal

LOCAL_SHARED_LIBRARIES :=       \
        libutils                \
        libcutils               \
        liblog                  \
        libui \
        libion \
        libdpframework \
        libdl \
	libvcodecdrv

LOCAL_STATIC_LIBRARIES := libMtkOmxOsalUtils

ifneq ($(strip $(MTK_BASIC_PACKAGE)), yes)
LOCAL_REQUIRED_MODULES += libmjc
LOCAL_REQUIRED_MODULES += libmjcFakeEngine
LOCAL_SHARED_LIBRARIES += librrc
endif

LOCAL_MODULE := libClearMotionFW
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk
LOCAL_MULTILIB := 32

include $(MTK_SHARED_LIBRARY)
