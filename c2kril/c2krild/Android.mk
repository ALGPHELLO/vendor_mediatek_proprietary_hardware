# Copyright 2006 The Android Open Source Project

ifneq ($(MTK_RIL_MODE), c6m_1rild)

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	rild.c


LOCAL_SHARED_LIBRARIES := \
    libcutils \
    liblog \
    libc2kril \
    libc2kutils

ifeq ($(TARGET_ARCH),arm64)
LOCAL_SHARED_LIBRARIES += libdl
else 
ifeq ($(TARGET_ARCH),arm)
LOCAL_SHARED_LIBRARIES += libdl
endif #arm
endif # arm64

LOCAL_SHARED_LIBRARIES += librilutilsmtk

LOCAL_CFLAGS += -DRIL_SHLIB

LOCAL_CFLAGS += -DC2K_RIL

ifeq ($(HAS_VIA_ADDED_RIL_REQUEST), true)
LOCAL_CFLAGS += -DVIA_ADDED_RIL_REQUEST
endif

LOCAL_INIT_RC := init.viarild.rc

## Note: Suffix will be temp if compile the module by mm in the directory
ifeq ($(strip $(REPO_VERSION)),)
LOCAL_CFLAGS += -DVIA_SUFFIX_VERSION=\"temp\"
else
LOCAL_CFLAGS += -DVIA_SUFFIX_VERSION=$(REPO_VERSION)
endif

LOCAL_C_INCLUDES += $(MTK_PATH_SOURCE)/hardware/ril/include
LOCAL_C_INCLUDES += $(MTK_PATH_SOURCE)/hardware/ccci/include \
                    system/core/include

#ALOG_VERSION := 4.1.1 4.2.1 4.2.2
#ifeq ($(findstring $(PLATFORM_VERSION),$(ALOG_VERSION)), $(PLATFORM_VERSION))
#LOCAL_CFLAGS += -DANDROID_JB
#endif

LOCAL_MODULE_TAGS := optional
LOCAL_MULTILIB := 32
LOCAL_MODULE:= viarild
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk
LOCAL_PRELINK_MODULE := false

include $(MTK_EXECUTABLE)

# For radiooptions binary
# =======================
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	radiooptions.c

LOCAL_SHARED_LIBRARIES := \
	libcutils \

LOCAL_CFLAGS := \

ifeq ($(HAS_VIA_ADDED_RIL_REQUEST), true)
LOCAL_CFLAGS += -DVIA_ADDED_RIL_REQUEST
endif

LOCAL_MODULE:= viaradiooptions
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk
LOCAL_MULTILIB := 32
LOCAL_MODULE_TAGS := debug
#ifeq ($(PLATFORM_VERSION), 4.1.1)
#LOCAL_CFLAGS += -DANDROID_JB
#endif
LOCAL_PRELINK_MODULE := false
include $(MTK_EXECUTABLE)

ifeq ($(C2K_GEMINI),yes)
    LOCAL_CFLAGS += -DANDROID_MULTI_SIM
endif

ifeq ($(C2K_MODEM_SUPPORT), 2)
    LOCAL_CFLAGS += -DANDROID_SIM_COUNT_2
endif

ifeq ($(C2K_MODEM_SUPPORT), 3)
    LOCAL_CFLAGS += -DANDROID_SIM_COUNT_3
endif

ifeq ($(C2K_MODEM_SUPPORT), 4)
    LOCAL_CFLAGS += -DANDROID_SIM_COUNT_4
endif
endif
