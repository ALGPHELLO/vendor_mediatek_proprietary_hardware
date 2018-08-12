# build hwcomposer static library

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

ifeq ($(MTK_HWC_SUPPORT), yes)

ifeq ($(MTK_HWC_VERSION), 1.5.0)

LC_MTK_PLATFORM = $(shell echo $(MTK_PLATFORM) | tr A-Z a-z )

LOCAL_MODULE := hwcomposer.$(MTK_PLATFORM_DIR).1.5.0

LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk

LOCAL_MODULE_CLASS := STATIC_LIBRARIES

LOCAL_ADDITIONAL_DEPENDENCIES := $(LOCAL_PATH)/Android.mk
LOCAL_C_INCLUDES += \
	frameworks/native/services/surfaceflinger \
	$(TOP)/$(MTK_ROOT)/frameworks/av/drm/widevine/libwvdrmengine/hdcpinfo/include \
	$(TOP)/$(MTK_ROOT)/hardware/include \
	$(TOP)/$(MTK_ROOT)/hardware/hwcomposer \
	$(TOP)/$(MTK_ROOT)/hardware/hwcomposer/include \
	$(TOP)/$(MTK_ROOT)/hardware/gralloc_extra/include \
	$(TOP)/$(MTK_ROOT)/hardware/dpframework/include \
	$(TOP)/$(MTK_ROOT)/hardware/gpu_ext/ged/include \
	$(TOP)/$(MTK_ROOT)/hardware/libgem/inc \
	$(TOP)/$(MTK_ROOT)/hardware/m4u/$(MTK_PLATFORM_DIR) \
	$(TOP)/$(MTK_ROOT)/hardware/libhwcomposer/$(MTK_PLATFORM_DIR) \
	$(TOP)/$(MTK_ROOT)/hardware/libhwcomposer \
	$(TOP)/$(MTK_ROOT)/platform/$(MTK_PLATFORM_DIR)/kernel/drivers/dispsys \
	$(TOP)/$(MTK_ROOT)/protect-bsp/hardware/gpu/include \
	$(TOP)/$(MTK_ROOT)/protect-bsp/hardware/gpu/gas/ \
	$(TOP)/$(MTK_ROOT)/protect-bsp/hardware/gpu/include \
	$(TOP)/$(MTK_ROOT)/protect-bsp/hardware/gpu/gas/ \
	$(TOP)/$(MTK_ROOT)/external/include \
	$(TOP)/$(MTK_ROOT)/external/libion_mtk/include \
	$(TOP)/$(MTK_ROOT)/hardware/perfservice/perfservicenative \
	$(TOP)/system/core/libion/include \
	$(TOP)/system/core/libsync/include \
	$(TOP)/system/core/libsync \
	$(TOP)/system/core/base/include

LOCAL_SRC_FILES := \
	hwc.cpp \
	hrt.cpp \
	dispatcher.cpp \
	worker.cpp \
	display.cpp \
	hwdev.cpp \
	event.cpp \
	overlay.cpp \
	queue.cpp \
	sync.cpp \
	composer.cpp \
	bliter.cpp \
	bliter_async.cpp \
	bliter_ultra.cpp \
	cache.cpp \
	platform_common.cpp \
	epaper_post_processing.cpp \
	tcon.cpp \
	post_processing.cpp \
	wakelock.cpp \
	service.cpp \
	../utils/tools.cpp \
	../utils/debug.cpp \
	../utils/transform.cpp \
	../utils/devicenode.cpp

MTK_HWC_PLATFORM_SRC := $(TOP)/$(MTK_ROOT)/hardware/libhwcomposer/$(MTK_PLATFORM_DIR)/platform.cpp
ifdef TARGET_2ND_ARCH
intermediates := $(call local-intermediates-dir)
intermediates_32 := $(call local-intermediates-dir,,2nd)
$(intermediates)/platform.cpp: $(MTK_HWC_PLATFORM_SRC)
	mkdir -p $(dir $@) && cp -f $< $@
$(intermediates_32)/platform.cpp: $(MTK_HWC_PLATFORM_SRC)
	mkdir -p $(dir $@) && cp -f $< $@
LOCAL_GENERATED_SOURCES_64 += $(intermediates)/platform.cpp
LOCAL_GENERATED_SOURCES_32 += $(intermediates_32)/platform.cpp
else
intermediates := $(call local-intermediates-dir)
$(intermediates)/platform.cpp: $(MTK_HWC_PLATFORM_SRC)
	mkdir -p $(dir $@) && cp -f $< $@
LOCAL_GENERATED_SOURCES += $(intermediates)/platform.cpp
endif

LOCAL_CFLAGS:= \
	-DLOG_TAG=\"hwcomposer\"

ifneq ($(strip $(TARGET_BUILD_VARIANT)), eng)
LOCAL_CFLAGS += -DMTK_USER_BUILD
endif

LOCAL_CFLAGS += -DUSE_NATIVE_FENCE_SYNC

LOCAL_CFLAGS += -DUSE_SYSTRACE

LOCAL_CFLAGS += -DMTK_HWC_VER_1_5

#LOCAL_CFLAGS += -DMTK_HWC_PROFILING

LOCAL_SHARED_LIBRARIES += \
	libui \
	libgui

include $(MTK_STATIC_LIBRARY)

endif # MTK_HWC_VERSION

endif # MTK_HWC_SUPPORT
