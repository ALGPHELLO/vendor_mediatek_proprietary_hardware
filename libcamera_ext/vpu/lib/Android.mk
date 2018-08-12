LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

vpu_binary_folder := $(LOCAL_PATH)/$(MTK_PLATFORM_DIR)

ifneq (,$(wildcard $(vpu_binary_folder)/main_imggen))

LOCAL_MODULE := cam_vpu2.img
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_OWNER := mtk
LOCAL_MODULE_PATH := $(PRODUCT_OUT)
include $(BUILD_SYSTEM)/base_rules.mk

vpu_binaries := $(shell find $(vpu_binary_folder) -type f)

$(LOCAL_BUILT_MODULE): PRIVATE_VPU_BINARY_FOLDER := $(vpu_binary_folder)
$(LOCAL_BUILT_MODULE): $(vpu_binaries)
	@echo Pack vpu binaries: $@
	$(hide) mkdir -p $(dir $@)
	$(hide) $(PRIVATE_VPU_BINARY_FOLDER)/main_imggen -i $(PRIVATE_VPU_BINARY_FOLDER)/ -o $@ 
endif
