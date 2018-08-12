#
# Copyright (C) 2013-2016, Shenzhen Huiding Technology Co., Ltd.
# All Rights Reserved.
#

# Oswego series:GF316M/GF318M/GF3118M/GF518M/GF5118M/GF516M/GF816M
# Milan E/F/G/L/FN/K series:GF3266/GF3208/GF3206/GF3288/GF3208FN/GF3228/
# Milan J/H   series: GF3226/GF3258
# Milan HV    series: GF8206/GF6226/GF5288
# Milan A/B/C series: GF5206/GF5216/GF5208. etc

LOCAL_PATH := $(call my-dir)

#auto modify it in script(release_for_xxxx.sh)
FACTORY_TEST:=ree

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
ifeq (x$(FACTORY_TEST), xree)
LOCAL_MODULE := libgf_ta_ree
else
LOCAL_MODULE := libgf_ta
endif

LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk

ifeq ($(TARGET_BUILD_VARIANT),eng)
MODE_NAME:=debug
else ifeq ($(TARGET_BUILD_VARIANT),userdebug)
MODE_NAME:=userdebug
else
MODE_NAME:=release
endif

ifeq ($(MTK_FINGERPRINT_SELECT), $(filter $(MTK_FINGERPRINT_SELECT), GF318M GF3118M GF518M GF5118M))
LOCAL_SRC_FILES_64 := $(MODE_NAME)/oswego_m_96/arm64-v8a/$(LOCAL_MODULE).so
LOCAL_SRC_FILES_32 := $(MODE_NAME)/oswego_m_96/armeabi-v7a/$(LOCAL_MODULE).so
endif

ifeq ($(MTK_FINGERPRINT_SELECT), $(filter $(MTK_FINGERPRINT_SELECT), GF316M GF516M GF816M))
LOCAL_SRC_FILES_64 := $(MODE_NAME)/oswego_m_118/arm64-v8a/$(LOCAL_MODULE).so
LOCAL_SRC_FILES_32 := $(MODE_NAME)/oswego_m_118/armeabi-v7a/$(LOCAL_MODULE).so
endif

ifeq ($(MTK_FINGERPRINT_SELECT), $(filter $(MTK_FINGERPRINT_SELECT), GF3208 GF3206 GF3266 GF3288 GF3208FN GF3228))
LOCAL_SRC_FILES_64 := $(MODE_NAME)/milan_f_series/arm64-v8a/$(LOCAL_MODULE).so
LOCAL_SRC_FILES_32 := $(MODE_NAME)/milan_f_series/armeabi-v7a/$(LOCAL_MODULE).so
endif

ifeq ($(MTK_FINGERPRINT_SELECT), $(filter $(MTK_FINGERPRINT_SELECT), GF5206))
LOCAL_SRC_FILES_64 := $(MODE_NAME)/milan_a/arm64-v8a/$(LOCAL_MODULE).so
LOCAL_SRC_FILES_32 := $(MODE_NAME)/milan_a/armeabi-v7a/$(LOCAL_MODULE).so
endif

ifeq ($(MTK_FINGERPRINT_SELECT), $(filter $(MTK_FINGERPRINT_SELECT), GF5216))
LOCAL_SRC_FILES_64 := $(MODE_NAME)/milan_b/arm64-v8a/$(LOCAL_MODULE).so
LOCAL_SRC_FILES_32 := $(MODE_NAME)/milan_b/armeabi-v7a/$(LOCAL_MODULE).so
endif

ifeq ($(MTK_FINGERPRINT_SELECT), $(filter $(MTK_FINGERPRINT_SELECT), GF5208))
LOCAL_SRC_FILES_64 := $(MODE_NAME)/milan_c/arm64-v8a/$(LOCAL_MODULE).so
LOCAL_SRC_FILES_32 := $(MODE_NAME)/milan_c/armeabi-v7a/$(LOCAL_MODULE).so
endif

LOCAL_SHARED_LIBRARIES := liblog
LOCAL_MULTILIB := both
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_SUFFIX := .so
include $(BUILD_PREBUILT)

