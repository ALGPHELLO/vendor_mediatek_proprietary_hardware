LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := pre_meta.cpp
LOCAL_MODULE := pre_meta

LOCAL_SHARED_LIBRARIES := libc libcutils

include $(MTK_EXECUTABLE)