/*
 * Copyright (c) 2017 SYKEAN Limited.
 *
 * All rights are reserved.
 * Proprietary and confidential.
 * Unauthorized copying of this file, via any medium is strictly prohibited.
 * Any use is subject to an appropriate license granted by SYKEAN Company.
 */

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "IrisHal"

#include <errno.h>
#include <malloc.h>
#include <string.h>
#include <cutils/log.h>
#include <hardware/hardware.h>
#include <hardware/iris.h>

#include "sykeaniris_hal.h"
#include "IrisCamOpsCb.h"
#include "IrisLedOpsCb.h"


//
#define MAX_FAILED_ATTEMPTS     (3)
//

static int iris_close(hw_device_t *dev)
{
    if (dev)
    {
        iris_hal_led_exit(dev);
        iris_hal_cam_exit(dev);
        iris_hal_exit(dev);
        free(dev);
        return 0;
    }
    else
    {
        return -1;
    }
}

static uint64_t iris_pre_enroll(struct iris_device *dev)
{
    return iris_hal_pre_enroll(dev);
}

static int iris_enroll(struct iris_device *dev,
        const hw_auth_token_t *hat,
        uint32_t gid, uint32_t timeout_sec)
{
    return iris_hal_enroll(dev, hat, gid, timeout_sec);
}

static int iris_post_enroll(struct iris_device *dev)
{
    return iris_hal_post_enroll(dev);
}

static uint64_t iris_get_auth_id(struct iris_device *dev)
{
    return iris_hal_get_auth_id(dev);
}

static int iris_cancel(struct iris_device *dev)
{
    return iris_hal_cancel(dev);
}

static int iris_remove(struct iris_device *dev,
        uint32_t gid, uint32_t eid)
{
    return iris_hal_remove(dev, gid, eid);
}

static int iris_set_active_group(struct iris_device *dev,
        uint32_t gid, const char __unused *store_path)
{
    return iris_hal_set_active_group(dev, gid, store_path);
}

static int iris_set_native_window(struct iris_device *dev,
        uint32_t gid, void *anw)
{
    return iris_hal_set_window(dev, gid, anw);
}

static int iris_authenticate(struct iris_device *dev,
        uint64_t operation_id, uint32_t gid)
{
    return iris_hal_authenticate(dev, operation_id, gid);
}

static int iris_capture(struct iris_device *dev, const uint8_t *pid_data,
        uint32_t pid_size, int32_t pid_type, int32_t bio_type,
        const uint8_t *cert_chain, uint32_t cert_size, uint32_t gid)
{
    return iris_hal_capture(dev, pid_data, pid_size, pid_type, bio_type,
            cert_chain, cert_size, gid);
}

static int iris_set_notify_callback(struct iris_device *dev, iris_notify_t notify)
{
    /* Decorate with locks */
    dev->notify = notify;
    return iris_hal_set_callback(dev);
}

static int iris_open(const hw_module_t *module,
        const char __unused *id, hw_device_t **device)
{
    if (device == NULL)
    {
        ALOGE("NULL device on open");
        return -EINVAL;
    }

    iris_device_t *dev = (iris_device_t *)malloc(sizeof(iris_device_t));
    memset(dev, 0, sizeof(iris_device_t));

    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = IRIS_MODULE_API_VERSION_2_0;
    dev->common.module = (struct hw_module_t *) module;
    dev->common.close = iris_close;

    dev->pre_enroll = iris_pre_enroll;
    dev->enroll = iris_enroll;
    dev->post_enroll = iris_post_enroll;
    dev->get_authenticator_id = iris_get_auth_id;
    dev->cancel = iris_cancel;
    dev->remove = iris_remove;
    dev->set_active_group = iris_set_active_group;
    dev->set_native_window = iris_set_native_window;
    dev->authenticate = iris_authenticate;
    dev->capture = iris_capture;
    dev->set_notify = iris_set_notify_callback;
    dev->notify = NULL;

    *device = (hw_device_t *) dev;

    sykean_iris_error_t err = SYKEAN_IRIS_SUCCESS;
    err = iris_hal_led_init(dev); // led init before iris init
    if (SYKEAN_IRIS_SUCCESS != err)
    {
        ALOGE("Iris hal set led operation fail!");
        return -EINVAL;
    }
    err = iris_hal_cam_init(dev); // camera init before iris init
    if (SYKEAN_IRIS_SUCCESS != err)
    {
        ALOGE("Iris hal set camera operation fail!");
        return -EINVAL;
    }
    err = iris_hal_init(dev, MAX_FAILED_ATTEMPTS);
    if (SYKEAN_IRIS_SUCCESS != err)
    {
        return -EINVAL;
    }

    return 0;
}

static struct hw_module_methods_t iris_module_methods =
{
    .open = iris_open,
};

iris_module_t HAL_MODULE_INFO_SYM =
{
    .common =
    {
        .tag                = HARDWARE_MODULE_TAG,
        .module_api_version = IRIS_MODULE_API_VERSION_2_0,
        .hal_api_version    = HARDWARE_HAL_API_VERSION,
        .id                 = IRIS_HARDWARE_MODULE_ID,
        .name               = "Demo Iris HAL",
        .author             = "The Android Open Source Project",
        .methods            = &iris_module_methods,
    },
};

