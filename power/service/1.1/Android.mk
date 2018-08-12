
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/util

LOCAL_SRC_FILES := Power.cpp \
					util/mi_util.cpp \
        	util/ptimer.cpp \
        	util/ports.cpp \
        	util/power_ipc.cpp \
        	util/powerc.cpp \
        	util/powerd_cmd.cpp \

LOCAL_SHARED_LIBRARIES := liblog \
				libhardware \
        libhwbinder \
        libhidlbase \
        libhidltransport \
        libutils \
        libcutils \
        android.hardware.power@1.0 \
        vendor.mediatek.hardware.power@1.1_vendor

LOCAL_MODULE := vendor.mediatek.hardware.power@1.1-impl
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_OWNER := mtk
include $(MTK_SHARED_LIBRARY)

include $(CLEAR_VARS)

ifneq ($(wildcard $(LOCAL_PATH)/../../config/$(MTK_PLATFORM_DIR)/cus_hint),)

LOCAL_C_INCLUDES := $(LOCAL_PATH)/util \
                    $(LOCAL_PATH)/../../config/common/intf_types \
                    $(LOCAL_PATH)/../../config/$(MTK_PLATFORM_DIR)/cus_hint

else # mt[xxxx] folder exist

LOCAL_C_INCLUDES := $(LOCAL_PATH)/util \
                    $(LOCAL_PATH)/../../config/common/cus_hint

endif # mt[xxxx] folder not exist

LOCAL_SRC_FILES := service.cpp \
					PowerManager.cpp \
          util/mi_util.cpp \
          util/ptimer.cpp \
          util/ports.cpp \
          util/power_ipc.cpp \
          util/powerd.cpp \
          util/powerd_core.cpp \
          util/powerd_cmd.cpp

LOCAL_SHARED_LIBRARIES := liblog \
        liblog \
        libdl \
        libutils \
        libcutils \
        libhwbinder \
        libhardware \
        libhidlbase \
        libhidltransport \
        android.hardware.power@1.0 \
        vendor.mediatek.hardware.power@1.1_vendor

LOCAL_MODULE := vendor.mediatek.hardware.power@1.1-service
LOCAL_INIT_RC := vendor.mediatek.hardware.power@1.1-service.rc
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_OWNER := mtk
include $(MTK_EXECUTABLE)
