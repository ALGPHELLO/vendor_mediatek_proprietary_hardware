LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    cust_aal_main.cpp

LOCAL_STATIC_LIBRARIES := \
    libaal_config

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    liblog

LC_MTK_PLATFORM := $(shell echo $(MTK_PLATFORM) | tr A-Z a-z )

LOCAL_C_INCLUDES := \
    $(TOP)/$(MTK_PATH_SOURCE)/hardware/aal/include \
    $(TOP)/$(MTK_PATH_SOURCE)/platform/$(LC_MTK_PLATFORM)/kernel/drivers/dispsys

ifeq ($(strip $(MTK_ULTRA_DIMMING_SUPPORT)),yes)
    LOCAL_CFLAGS += -DMTK_ULTRA_DIMMING_SUPPORT
endif

LOCAL_MODULE:= libaal_cust

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    IAALService.cpp \
    AALService.cpp \
    AALClient.cpp \
    AALLightSensor.cpp \
    AALAdapt.cpp \

LOCAL_CFLAGS += -DAAL_ANDROID_PLATFORM

ifneq (,$(filter $(strip $(TARGET_BOARD_PLATFORM)), mt6799))
    LOCAL_CFLAGS += -DHAS_DRE30
endif

ifneq (,$(filter $(strip $(TARGET_BOARD_PLATFORM)), mt6570 mt6580 mt6735 mt6535m mt6737m mt6737t mt6753 mt6750 mt6755 mt6757 mt6797 mt6799 mt8163 mt8167 mt8173))
    LOCAL_CFLAGS += -DSUPPORT_CABC_HW
endif

ifneq (,$(filter $(strip $(TARGET_BOARD_PLATFORM)), mt6755 mt6750 mt6799 mt6763 mt6771 mt6775))
    LOCAL_CFLAGS += -DAAL_SUPPORT_KERNEL_API
endif

LC_MTK_PLATFORM := $(shell echo $(MTK_PLATFORM) | tr A-Z a-z )

LOCAL_C_INCLUDES:= \
    $(TOP)/frameworks/base/include/ \
    $(TOP)/$(MTK_PATH_SOURCE)/hardware/aal/include \
    $(TOP)/$(MTK_PATH_PLATFORM)/kernel/drivers/dispsys \
    $(TOP)/$(MTK_PATH_SOURCE)/kernel/drivers/video \
    $(TOP)/$(MTK_PATH_SOURCE)/kernel/include \
    $(call include-path-for, libhardware)/hardware \
    $(call include-path-for, libhardware_legacy)/hardware_legacy \
    $(TOP)/frameworks/native/libs/sensor/include \

LOCAL_SHARED_LIBRARIES := \
    libutils \
    libcutils \
    libbinder \
    libhardware \
    libhardware_legacy \
    libdl \
    liblog \
    libsensor \
    libaal \

LOCAL_MODULE := libaalservice

include $(BUILD_SHARED_LIBRARY)
