LOCAL_PATH:= $(call my-dir)

# linux scribo
include $(CLEAR_VARS)
LOCAL_C_INCLUDES:= \
    $(LOCAL_PATH)/climax/inc \
    $(LOCAL_PATH)/climax/src/lxScribo \
    $(LOCAL_PATH)/climax/src/lxScribo/scribosrv \
    $(LOCAL_PATH)/Tfa98xxhal/inc \
    $(LOCAL_PATH)/Tfa98xxhal/src/lxScribo \
    $(LOCAL_PATH)/Tfa98xxhal/inc \
    $(LOCAL_PATH)/Tfa98xx/inc \
    $(LOCAL_PATH)/Tfa98xx/src

LOCAL_SRC_FILES:= \
    Tfa98xxhal/src/lxScribo/lxScribo.c \
    Tfa98xxhal/src/lxScribo/lxDummy.c \
    Tfa98xxhal/src/lxScribo/lxScriboSerial.c \
    Tfa98xxhal/src/lxScribo/lxScriboSocket.c \
    Tfa98xxhal/src/lxScribo/lxI2c.c \
    Tfa98xxhal/src/NXP_I2C_linux.c \
    Tfa98xxhal/src/lxScribo/i2cserver.c \
    Tfa98xxhal/src/lxScribo/cmd.c

LOCAL_MODULE := liblxScribo9887
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk
LOCAL_SHARED_LIBRARIES:= libcutils libutils
LOCAL_MODULE_TAGS := optional
include $(MTK_STATIC_LIBRARY)


# libtfa98xx
include $(CLEAR_VARS)

LOCAL_C_INCLUDES+= \
    $(LOCAL_PATH)/climax/inc \
    $(LOCAL_PATH)/climax/src/lxScribo \
    $(LOCAL_PATH)/climax/src/lxScribo/scribosrv \
    $(LOCAL_PATH)/Tfa98xxhal/inc \
    $(LOCAL_PATH)/Tfa98xxhal/src/lxScribo \
    $(LOCAL_PATH)/Tfa98xxhal/inc \
    $(LOCAL_PATH)/Tfa98xx/inc \
    $(LOCAL_PATH)/Tfa98xx/src

LOCAL_SRC_FILES:= \
    Tfa98xx/src/Tfa98xx.c \
    Tfa98xx/src/Tfa98xx_TextSupport.c \
    Tfa98xx/src/Tfa9887.c \
    Tfa98xx/src/initTfa9887.c

LOCAL_MODULE := libtfa9887
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk
LOCAL_SHARED_LIBRARIES:= libcutils libutils
LOCAL_STATIC_LIBRARIES:= liblxScribo9887
LOCAL_MODULE_TAGS := optional
include $(MTK_STATIC_LIBRARY)

# cli app
include $(CLEAR_VARS)

LOCAL_C_INCLUDES:= \
    $(LOCAL_PATH)/climax/inc \
    $(LOCAL_PATH)/climax/src/lxScribo \
    $(LOCAL_PATH)/climax/src/lxScribo/scribosrv \
    $(LOCAL_PATH)/Tfa98xxhal/inc \
    $(LOCAL_PATH)/Tfa98xxhal/src/lxScribo \
    $(LOCAL_PATH)/Tfa98xxhal/inc \
    $(LOCAL_PATH)/Tfa98xx/inc \
    $(LOCAL_PATH)/Tfa98xx/src

LOCAL_SRC_FILES:= \
    climax/src/climax.c \
    climax/src/cliCommands.c \
    climax/src/cli/cmdline.c \
    climax/src/nxpTfa98xx.c \
    climax/src/tfa98xxCalibration.c \
    climax/src/tfa98xxDiagnostics.c \
    climax/src/tfa98xxLiveData.c \
    climax/src/tfa98xxRuntime.c

LOCAL_SHARED_LIBRARIES:= libcutils libutils
LOCAL_STATIC_LIBRARIES:= libtfa9887 liblxScribo9887
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := climaxtest
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := mtk
include $(MTK_EXECUTABLE)

include $(call all-makefiles-under,$(LOCAL_PATH))
