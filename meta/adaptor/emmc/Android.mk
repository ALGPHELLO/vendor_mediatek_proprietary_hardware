LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := meta_clr_emmc.c
LOCAL_C_INCLUDES := $(MTK_PATH_SOURCE)/hardware/meta/common/inc \
                    $(MTK_PATH_SOURCE)/hardware/meta/adaptor/storageutil
LOCAL_SHARED_LIBRARIES := libcutils libc
LOCAL_STATIC_LIBRARIES := libstorageutil
LOCAL_CFLAGS += -DUSE_EXT4
ifeq ($(MNTL_SUPPORT), yes)
LOCAL_CFLAGS += -DMNTL_SUPPORT
endif
LOCAL_C_INCLUDES += system/core/fs_mgr/include
LOCAL_STATIC_LIBRARIES += libext4_utils libz libft libselinux
LOCAL_STATIC_LIBRARIES += libfs_mgr
LOCAL_MODULE := libmeta_clr_emmc
LOCAL_PRELINK_MODULE := false
include $(MTK_STATIC_LIBRARY)
