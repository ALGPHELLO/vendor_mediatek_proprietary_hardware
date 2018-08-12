# Copyright (c) 2017 SYKEAN Limited.
#
# All rights are reserved.
# Proprietary and confidential.
# Unauthorized copying of this file, via any medium is strictly prohibited.
# Any use is subject to an appropriate license granted by SYKEAN Company.

LOCAL_PATH:= vendor/mediatek/proprietary/hardware/iris

PRODUCT_COPY_FILES += $(LOCAL_PATH)/sykean/release/armeabi-v7a/IRISLIB32.SIG:$(TARGET_COPY_OUT_VENDOR)/etc/IRISLIB32.SIG
PRODUCT_COPY_FILES += $(LOCAL_PATH)/sykean/release/arm64-v8a/IRISLIB64.SIG:$(TARGET_COPY_OUT_VENDOR)/etc/IRISLIB64.SIG
PRODUCT_COPY_FILES += $(LOCAL_PATH)/sykean/cert-info/IRISCERT.RSA:$(TARGET_COPY_OUT_VENDOR)/etc/IRISCERT.RSA
PRODUCT_COPY_FILES += $(LOCAL_PATH)/sykean/cert-info/IRISCERT.SF:$(TARGET_COPY_OUT_VENDOR)/etc/IRISCERT.SF
PRODUCT_COPY_FILES += $(LOCAL_PATH)/sykean/cert-info/IRISCERT.SF.SIG:$(TARGET_COPY_OUT_VENDOR)/etc/IRISCERT.SF.SIG
