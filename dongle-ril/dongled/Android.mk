ifeq ($(MTK_EXTERNAL_DONGLE_SUPPORT),yes)

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    main.cpp    \
    UsbSelect.cpp \
    NetlinkManager.cpp \
    NetlinkHandler.cpp

LOCAL_SHARED_LIBRARIES := \
    libsysutils \
    libcutils \
    liblog \
    libdl

LOCAL_MODULE:= dongled
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk
LOCAL_MODULE_TAGS := optional
include $(MTK_EXECUTABLE)

endif #(($(MTK_EXTERNAL_DONGLE_SUPPORT),yes)
