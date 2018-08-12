LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE := merged_hal_service
LOCAL_INIT_RC := merged_hal_service.rc

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/powerd \
    $(TOP)/$(MTK_PATH_SOURCE)/hardware/power/config/common/intf_types \
    $(TOP)/$(MTK_PATH_SOURCE)/hardware/power/config/$(MTK_PLATFORM_DIR)/cus_hint \
    $(MTK_PATH_SOURCE)/hardware/connectivity/gps/lbs_hidl_service \
    $(MTK_PATH_SOURCE)/hardware/connectivity/gps/lbs_hidl_service/mtk_socket_utils/inc \
    hardware/interfaces/drm

LOCAL_SRC_FILES := \
    service.cpp \
    powerd/mi_util.cpp \
    powerd/ptimer.cpp \
    powerd/ports.cpp \
    powerd/power_ipc.cpp \
    powerd/powerd.cpp \
    powerd/powerd_core.cpp \
    powerd/powerd_cmd.cpp


# TODO(b/18948909) Some legacy DRM plugins only support 32-bit. They need to be
# migrated to 64-bit. Once all of a device's legacy DRM plugins support 64-bit,
# that device can turn on TARGET_ENABLE_MEDIADRM_64 to build this service as
# 64-bit.
ifneq ($(TARGET_ENABLE_MEDIADRM_64), true)
    LOCAL_32_BIT_ONLY := true
endif

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libcutils \
    libdl \
    libbase \
    libutils \
    libhardware \

LOCAL_SHARED_LIBRARIES += \
    libhidlbase \
    libhidltransport \
    android.hardware.power@1.0 \
    vendor.mediatek.hardware.power@1.1_vendor \
    android.hardware.vibrator@1.0 \
    android.hardware.thermal@1.0 \
    android.hardware.memtrack@1.0 \
    android.hardware.light@2.0 \
    android.hardware.graphics.allocator@2.0 \
    vendor.mediatek.hardware.mtkcodecservice@1.1_vendor \
    android.hardware.gnss@1.0 \
    vendor.mediatek.hardware.gnss@1.1_vendor \
    vendor.mediatek.hardware.lbs@1.0_vendor \
    lbs_hidl_service-impl \
    vendor.mediatek.hardware.nvram@1.0_vendor \
    android.hardware.drm@1.0

LOCAL_STATIC_LIBRARIES := \
    android.hardware.drm@1.0-helper \

include $(BUILD_EXECUTABLE)
