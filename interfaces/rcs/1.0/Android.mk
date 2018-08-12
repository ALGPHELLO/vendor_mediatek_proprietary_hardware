# This file is autogenerated by hidl-gen. Do not edit manually.

LOCAL_PATH := $(call my-dir)

################################################################################

include $(CLEAR_VARS)
LOCAL_MODULE := vendor.mediatek.hardware.rcs-V1.0-java
LOCAL_MODULE_CLASS := JAVA_LIBRARIES

intermediates := $(call local-generated-sources-dir, COMMON)

HIDL := $(HOST_OUT_EXECUTABLES)/hidl-gen$(HOST_EXECUTABLE_SUFFIX)

LOCAL_JAVA_LIBRARIES := \
    android.hidl.base-V1.0-java \


#
# Build IRcs.hal
#
GEN := $(intermediates)/vendor/mediatek/hardware/rcs/V1_0/IRcs.java
$(GEN): $(HIDL)
$(GEN): PRIVATE_HIDL := $(HIDL)
$(GEN): PRIVATE_DEPS := $(LOCAL_PATH)/IRcs.hal
$(GEN): PRIVATE_DEPS += $(LOCAL_PATH)/IRcsIndication.hal
$(GEN): $(LOCAL_PATH)/IRcsIndication.hal
$(GEN): PRIVATE_OUTPUT_DIR := $(intermediates)
$(GEN): PRIVATE_CUSTOM_TOOL = \
        $(PRIVATE_HIDL) -o $(PRIVATE_OUTPUT_DIR) \
        -Ljava \
        -randroid.hidl:system/libhidl/transport \
        -rvendor.mediatek.hardware:vendor/mediatek/proprietary/hardware/interfaces \
        vendor.mediatek.hardware.rcs@1.0::IRcs

$(GEN): $(LOCAL_PATH)/IRcs.hal
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)

#
# Build IRcsIndication.hal
#
GEN := $(intermediates)/vendor/mediatek/hardware/rcs/V1_0/IRcsIndication.java
$(GEN): $(HIDL)
$(GEN): PRIVATE_HIDL := $(HIDL)
$(GEN): PRIVATE_DEPS := $(LOCAL_PATH)/IRcsIndication.hal
$(GEN): PRIVATE_OUTPUT_DIR := $(intermediates)
$(GEN): PRIVATE_CUSTOM_TOOL = \
        $(PRIVATE_HIDL) -o $(PRIVATE_OUTPUT_DIR) \
        -Ljava \
        -randroid.hidl:system/libhidl/transport \
        -rvendor.mediatek.hardware:vendor/mediatek/proprietary/hardware/interfaces \
        vendor.mediatek.hardware.rcs@1.0::IRcsIndication

$(GEN): $(LOCAL_PATH)/IRcsIndication.hal
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)
include $(BUILD_JAVA_LIBRARY)


################################################################################

include $(CLEAR_VARS)
LOCAL_MODULE := vendor.mediatek.hardware.rcs-V1.0-java-static
LOCAL_MODULE_CLASS := JAVA_LIBRARIES

intermediates := $(call local-generated-sources-dir, COMMON)

HIDL := $(HOST_OUT_EXECUTABLES)/hidl-gen$(HOST_EXECUTABLE_SUFFIX)

LOCAL_STATIC_JAVA_LIBRARIES := \
    android.hidl.base-V1.0-java-static \


#
# Build IRcs.hal
#
GEN := $(intermediates)/vendor/mediatek/hardware/rcs/V1_0/IRcs.java
$(GEN): $(HIDL)
$(GEN): PRIVATE_HIDL := $(HIDL)
$(GEN): PRIVATE_DEPS := $(LOCAL_PATH)/IRcs.hal
$(GEN): PRIVATE_DEPS += $(LOCAL_PATH)/IRcsIndication.hal
$(GEN): $(LOCAL_PATH)/IRcsIndication.hal
$(GEN): PRIVATE_OUTPUT_DIR := $(intermediates)
$(GEN): PRIVATE_CUSTOM_TOOL = \
        $(PRIVATE_HIDL) -o $(PRIVATE_OUTPUT_DIR) \
        -Ljava \
        -randroid.hidl:system/libhidl/transport \
        -rvendor.mediatek.hardware:vendor/mediatek/proprietary/hardware/interfaces \
        vendor.mediatek.hardware.rcs@1.0::IRcs

$(GEN): $(LOCAL_PATH)/IRcs.hal
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)

#
# Build IRcsIndication.hal
#
GEN := $(intermediates)/vendor/mediatek/hardware/rcs/V1_0/IRcsIndication.java
$(GEN): $(HIDL)
$(GEN): PRIVATE_HIDL := $(HIDL)
$(GEN): PRIVATE_DEPS := $(LOCAL_PATH)/IRcsIndication.hal
$(GEN): PRIVATE_OUTPUT_DIR := $(intermediates)
$(GEN): PRIVATE_CUSTOM_TOOL = \
        $(PRIVATE_HIDL) -o $(PRIVATE_OUTPUT_DIR) \
        -Ljava \
        -randroid.hidl:system/libhidl/transport \
        -rvendor.mediatek.hardware:vendor/mediatek/proprietary/hardware/interfaces \
        vendor.mediatek.hardware.rcs@1.0::IRcsIndication

$(GEN): $(LOCAL_PATH)/IRcsIndication.hal
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)
include $(BUILD_STATIC_JAVA_LIBRARY)



include $(call all-makefiles-under,$(LOCAL_PATH))