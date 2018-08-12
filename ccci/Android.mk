LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

ifdef MTK_MD_SBP_CUSTOM_VALUE
ifneq ($(MTK_MD_SBP_CUSTOM_VALUE), "")
LOCAL_CFLAGS += -DMD_SBP_CUSTOM_VALUE=$(subst ",,$(MTK_MD_SBP_CUSTOM_VALUE))
endif
endif

ifdef MTK_MD2_SBP_CUSTOM_VALUE
ifneq ($(MTK_MD2_SBP_CUSTOM_VALUE), "")
LOCAL_CFLAGS += -DMD2_SBP_CUSTOM_VALUE=$(subst ",,$(MTK_MD2_SBP_CUSTOM_VALUE))
endif
endif

ifeq ($(MTK_ECCCI_C2K),yes)
LOCAL_CFLAGS += -DMTK_ECCCI_C2K
endif

LOCAL_SRC_FILES := ccci_lib.c

LOCAL_STATIC_LIBRARIES := libcutils
LOCAL_HEADER_LIBRARIES := libnvram_headers
LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include

LOCAL_EXPORT_C_INCLUDE_DIRS := \
	$(LOCAL_PATH)/include

LOCAL_SHARED_LIBRARIES := libc liblog

LOCAL_MODULE := libccci_util
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk
LOCAL_MULTILIB := both

LOCAL_MODULE_TAGS := optional

include $(MTK_SHARED_LIBRARY)

###################clone system partion lib###########################

include $(CLEAR_VARS)

ifdef MTK_MD_SBP_CUSTOM_VALUE
ifneq ($(MTK_MD_SBP_CUSTOM_VALUE), "")
LOCAL_CFLAGS += -DMD_SBP_CUSTOM_VALUE=$(subst ",,$(MTK_MD_SBP_CUSTOM_VALUE))
endif
endif

ifdef MTK_MD2_SBP_CUSTOM_VALUE
ifneq ($(MTK_MD2_SBP_CUSTOM_VALUE), "")
LOCAL_CFLAGS += -DMD2_SBP_CUSTOM_VALUE=$(subst ",,$(MTK_MD2_SBP_CUSTOM_VALUE))
endif
endif

ifeq ($(MTK_ECCCI_C2K),yes)
LOCAL_CFLAGS += -DMTK_ECCCI_C2K
endif

LOCAL_SRC_FILES := ccci_lib.c

LOCAL_STATIC_LIBRARIES := libcutils
LOCAL_HEADER_LIBRARIES := libnvram_headers
LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include

LOCAL_EXPORT_C_INCLUDE_DIRS := \
	$(LOCAL_PATH)/include

LOCAL_SHARED_LIBRARIES := libc liblog

LOCAL_MODULE := libccci_util_sys
LOCAL_MODULE_OWNER := mtk
LOCAL_MULTILIB := both

LOCAL_MODULE_TAGS := optional

include $(MTK_SHARED_LIBRARY)
