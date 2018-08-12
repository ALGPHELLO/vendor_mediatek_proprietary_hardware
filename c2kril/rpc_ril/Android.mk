# Copyright 2006 The Android Open Source Project

ifneq ($(MTK_RIL_MODE), c6m_1rild)

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES:= \
    rpc_ril.cpp

LOCAL_SHARED_LIBRARIES := \
    libutils \
    libbinder \
    libcutils \
    libc2kutils \
    liblog

LOCAL_C_INCLUDES += $(MTK_PATH_SOURCE)/hardware/include/c2k

LOCAL_PRELINK_MODULE := false

LOCAL_MODULE:= librpcril
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk
LOCAL_MULTILIB := 32

#LOCAL_LDLIBS += -lpthread
LOCAL_PRELINK_MODULE := false

include $(MTK_SHARED_LIBRARY)

endif