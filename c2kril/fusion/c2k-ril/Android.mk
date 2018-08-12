# Copyright 2006 The Android Open Source Project

# XXX using libutils for simulator build only...
#

ifeq ($(MTK_RIL_MODE), c6m_1rild)

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    ViaHandler.cpp

LOCAL_SHARED_LIBRARIES := \
    liblog libcutils libutils libmtk-ril librilutils

LOCAL_STATIC_LIBRARIES := \
    libprotobuf-c-nano-enable_malloc

LOCAL_C_INCLUDES += $(TOP)/system/core/include/utils \
    $(TOP)/$(MTK_PATH_SOURCE)/hardware/ril/include \
    $(TOP)/$(MTK_PATH_SOURCE)/hardware/ril/fusion/mtk-ril/framework/include/core \
    $(TOP)/$(MTK_PATH_SOURCE)/hardware/ril/fusion/mtk-ril/framework/include \
    $(TOP)/$(MTK_PATH_SOURCE)/hardware/ril/fusion/mtk-ril/framework/include/base \
    $(TOP)/$(MTK_PATH_SOURCE)/hardware/ril/fusion/mtk-ril/framework/port/android/include \
    $(TOP)/$(MTK_PATH_SOURCE)/hardware/ril/fusion/mtk-ril/ \
    $(TOP)/$(MTK_PATH_SOURCE)/hardware/ril/fusion/mtk-ril/telcore \
    $(TOP)/$(MTK_PATH_SOURCE)/hardware/ril/fusion/mtk-ril/mdcomm \
    $(MTK_PATH_SOURCE)/hardware/ccci/include

# for asprinf
LOCAL_CFLAGS := -D_GNU_SOURCE

LOCAL_CFLAGS += -DMTK_MUX_CHANNEL_64
LOCAL_CFLAGS += -DMTK_IMS_CHANNEL_SUPPORT

LOCAL_C_INCLUDES += external/nanopb-c

#build shared library
LOCAL_CFLAGS += -DRIL_SHLIB
LOCAL_MODULE:= libvia-ril
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk
include $(MTK_SHARED_LIBRARY)

endif
