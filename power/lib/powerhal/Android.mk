
LOCAL_PATH := $(call my-dir)

#include $(CLEAR_VARS)
#ifeq ($(strip $(MTK_AFPSGO_FBT_GAME)), yes)
#	  LOCAL_CFLAGS += -DMTK_AFPSGO_FBT_GAME
#endif
#LOCAL_SRC_FILES := perfctl.cpp
#LOCAL_LDLIBS := -llog
#LOCAL_SHARED_LIBRARIES := libc libcutils libdl libgui libui libutils libexpat
#LOCAL_MODULE := libpowerhalctl
#LOCAL_PROPRIETARY_MODULE := true
#LOCAL_MODULE_OWNER := mtk
#include $(MTK_SHARED_LIBRARY)

#include $(CLEAR_VARS)
#ifeq ($(strip $(MTK_AFPSGO_FBT_GAME)), yes)
#	  LOCAL_CFLAGS += -DMTK_AFPSGO_FBT_GAME
#endif
#LOCAL_SRC_FILES := perfctl.cpp
#LOCAL_LDLIBS := -llog
#LOCAL_SHARED_LIBRARIES := libc libcutils libdl libgui libui libutils libexpat
#LOCAL_MODULE := libpowerhalctl_vendor
#LOCAL_MODULE_OWNER := mtk
#include $(MTK_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := perfservice.cpp common.cpp perfservicepriorityadj.cpp perfservice_smart.cpp

LOCAL_SHARED_LIBRARIES := libc libcutils libdl libui libutils liblog libexpat \
	android.hardware.power@1.0 \
	vendor.mediatek.hardware.power@1.1_vendor \
	vendor.mediatek.hardware.dfps@1.0 \

LOCAL_C_INCLUDES := $(MTK_PATH_SOURCE)/hardware/libgem/inc

LOCAL_MODULE := libpowerhal
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk
include $(MTK_SHARED_LIBRARY)

