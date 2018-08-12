/*
 * Copyright (C) 2017 The SYKEAN Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "irisd"

#include <binder/IServiceManager.h>
#include <hardware/hardware.h>
#include <hardware/iris.h>
#include <hardware/mtk_hw_auth_token.h>
#include <keystore/IKeystoreService.h>
#include <keystore/keystore.h> // for error codes

#include <utils/Log.h>

#include "IrisDaemonProxy.h"

namespace android {

IrisDaemonProxy* IrisDaemonProxy::sInstance = NULL;

// Supported iris HAL version
static const uint16_t kVersion = HARDWARE_MODULE_API_VERSION(2, 0);

IrisDaemonProxy::IrisDaemonProxy()
    : mModule(NULL)
    , mDevice(NULL)
    , mCallback(NULL)
    , mSurface(NULL) {

}

IrisDaemonProxy::~IrisDaemonProxy() {
    closeHal();
}

void IrisDaemonProxy::hal_notify_callback(const iris_msg_t *msg) {
    IrisDaemonProxy* instance = IrisDaemonProxy::getInstance();
    const sp<IIrisDaemonCallback> callback = instance->mCallback;
    if (callback == NULL) {
        ALOGE("Invalid callback object");
        return;
    }
    const int64_t device = (int64_t) instance->mDevice;
    status_t err = 0;
    switch (msg->type) {
        case IRIS_ERROR:
            ALOGD("onError(code:%d)", msg->data.error);
            err = callback->onError(device,
                    msg->data.error);
            if (NO_ERROR != err)
            {
                ALOGE("onError() fail:%d", err);
            }
            break;
        case IRIS_ACQUIRED:
            ALOGD("onAcquired(%d, %d)",
                    msg->data.acquired.acquired_info_l,
                    msg->data.acquired.acquired_info_r);
            err = callback->onAcquired(device,
                    msg->data.acquired.acquired_info_l,
                    msg->data.acquired.acquired_info_r);
            if (NO_ERROR != err)
            {
                ALOGE("onAcquired() fail:%d", err);
            }
            break;
        case IRIS_AUTHENTICATED:
            ALOGD("onAuthenticated(eid=%d, gid=%d)",
                    msg->data.authenticated.iris.eid,
                    msg->data.authenticated.iris.gid);
            if (msg->data.authenticated.iris.eid != 0) {
                const uint8_t* hat = reinterpret_cast<const uint8_t *>(&msg->data.authenticated.hat);
                instance->notifyKeystore(hat, sizeof(msg->data.authenticated.hat));
            }
            err = callback->onAuthenticated(device,
                    msg->data.authenticated.iris.eid,
                    msg->data.authenticated.iris.gid);
            if (NO_ERROR != err)
            {
                ALOGE("onAuthenticated() fail:%d", err);
            }
            break;
        case IRIS_TEMPLATE_ENROLLING:
            ALOGD("onEnrollResult(eid=%d, gid=%d, rem=%d)",
                    msg->data.enroll.iris.eid,
                    msg->data.enroll.iris.gid,
                    msg->data.enroll.samples_remaining);
            err = callback->onEnrollResult(device,
                    msg->data.enroll.iris.eid,
                    msg->data.enroll.iris.gid,
                    msg->data.enroll.samples_remaining);
            if (NO_ERROR != err)
            {
                ALOGE("onEnrollResult() fail:%d", err);
            }
            break;
        case IRIS_TEMPLATE_REMOVED:
            ALOGD("onRemove(eid=%d, gid=%d)",
                    msg->data.removed.iris.eid,
                    msg->data.removed.iris.gid);
            err = callback->onRemoved(device,
                    msg->data.removed.iris.eid,
                    msg->data.removed.iris.gid);
            if (NO_ERROR != err)
            {
                ALOGE("onRemove() fail:%d", err);
            }
            break;
        case IRIS_DISPLAY:
            ALOGD("onDisplay(size:%zd)",
                    msg->data.display.size);
            err = callback->onDisplay(device,
                    msg->data.display.data,
                    msg->data.display.size,
                    msg->data.display.width,
                    msg->data.display.height,
                    msg->data.display.eyeRectL,
                    msg->data.display.eyeRectR,
                    sizeof(msg->data.display.eyeRectL) /
                    sizeof(msg->data.display.eyeRectL[0]));
            if (NO_ERROR != err)
            {
                ALOGE("onDisplay() fail:%d", err);
            }
            break;
        case IRIS_CAPTURED:
            ALOGD("onCaptureResult(%zd, %zd, %zd)",
                    msg->data.captured.pidSize,
                    msg->data.captured.hmacSize,
                    msg->data.captured.sesKeySize);
            err = callback->onCaptureResult(device,
                    msg->data.captured.pEncrpPid,
                    msg->data.captured.pidSize,
                    msg->data.captured.pEncrpHMAC,
                    msg->data.captured.hmacSize,
                    msg->data.captured.pEncrpSesKey,
                    msg->data.captured.sesKeySize);
            if (NO_ERROR != err)
            {
                ALOGE("onCaptureResult() fail:%d", err);
            }
            break;
        case IRIS_CAPTURE_ERROR:
            ALOGD("onCaptureError(code:%d)",
                    msg->data.capture_error);
            err = callback->onError(device,
                    msg->data.capture_error);
            if (NO_ERROR != err)
            {
                ALOGE("onCaptureError() fail:%d", err);
            }
            break;
        case IRIS_CAPTURE_STAGING:
            ALOGD("onCaptureProgress(%d)",
                    msg->data.capture_staging);
            err = callback->onCaptureProgress(device,
                    msg->data.capture_staging);
            if (NO_ERROR != err)
            {
                ALOGE("onCaptureProgress() fail:%d", err);
            }
            break;
        default:
            ALOGE("invalid msg type: %d", msg->type);
            return;
    }
}

void IrisDaemonProxy::notifyKeystore(const uint8_t *auth_token, const size_t auth_token_length) {
    if (auth_token != NULL && auth_token_length > 0) {
        // TODO : cache service?
        sp < IServiceManager > sm = defaultServiceManager();
        sp < IBinder > binder = sm->getService(String16("android.security.keystore"));
        sp < IKeystoreService > service = interface_cast < IKeystoreService > (binder);
        if (service != NULL) {
            #if 0
            // for android 5.0
            ALOGE("Falure sending auth token to KeyStore");
            #else
            status_t ret = service->addAuthToken(auth_token, auth_token_length);
            if (ret != ResponseCode::NO_ERROR) {
                ALOGE("Falure sending auth token to KeyStore: %d", ret);
            }
            #endif
        } else {
            ALOGE("Unable to communicate with KeyStore");
        }
    }
}

void IrisDaemonProxy::init(const sp<IIrisDaemonCallback>& callback) {
    #if 0
    // for android 5.0
    if (mCallback != NULL && callback->asBinder() != mCallback->asBinder()) {
        mCallback->asBinder()->unlinkToDeath(this);
    }
    callback->asBinder()->linkToDeath(this);
    #else
    if (mCallback != NULL && IInterface::asBinder(callback) != IInterface::asBinder(mCallback)) {
        IInterface::asBinder(mCallback)->unlinkToDeath(this);
    }
    IInterface::asBinder(callback)->linkToDeath(this);
    #endif
    mCallback = callback;
}

int32_t IrisDaemonProxy::enroll(const uint8_t *token, ssize_t tokenSize, int32_t groupId,
        int32_t timeout) {
    ALOG(LOG_VERBOSE, LOG_TAG, "enroll(gid=%d, timeout=%d)\n", groupId, timeout);
    if (tokenSize != sizeof(hw_auth_token_t)) {
        ALOG(LOG_VERBOSE, LOG_TAG, "enroll() : invalid token size %zu\n", tokenSize);
        return -1;
    }
    const hw_auth_token_t* authToken = reinterpret_cast<const hw_auth_token_t*>(token);
    return mDevice->enroll(mDevice, authToken, groupId, timeout);
}

uint64_t IrisDaemonProxy::preEnroll() {
    return mDevice->pre_enroll(mDevice);
}

int32_t IrisDaemonProxy::postEnroll() {
    return mDevice->post_enroll(mDevice);
}

int32_t IrisDaemonProxy::stopEnrollment() {
    ALOG(LOG_VERBOSE, LOG_TAG, "stopEnrollment()\n");
    return mDevice->cancel(mDevice);
}

int32_t IrisDaemonProxy::authenticate(uint64_t sessionId, uint32_t groupId) {
    ALOG(LOG_VERBOSE, LOG_TAG, "authenticate(sid=%" PRId64 ", gid=%d)\n", sessionId, groupId);
    return mDevice->authenticate(mDevice, sessionId, groupId);
}

int32_t IrisDaemonProxy::stopAuthentication() {
    ALOG(LOG_VERBOSE, LOG_TAG, "stopAuthentication()\n");
    return mDevice->cancel(mDevice);
}

int32_t IrisDaemonProxy::capture(const uint8_t *pPidData, ssize_t pidLen, int32_t pidType,
        int32_t bioType, const uint8_t *pCertChain, ssize_t certLen, int32_t groupId) {
    ALOG(LOG_VERBOSE, LOG_TAG, "capture()\n");
    return mDevice->capture(mDevice, pPidData, pidLen,
            pidType, bioType, pCertChain, certLen, groupId);
}

int32_t IrisDaemonProxy::stopCapture() {
    ALOG(LOG_VERBOSE, LOG_TAG, "stopCapture()\n");
    return mDevice->cancel(mDevice);
}

int32_t IrisDaemonProxy::remove(int32_t irisId, int32_t groupId) {
    ALOG(LOG_VERBOSE, LOG_TAG, "remove(eid=%d, gid=%d)\n", irisId, groupId);
    return mDevice->remove(mDevice, groupId, irisId);
}

uint64_t IrisDaemonProxy::getAuthenticatorId() {
    return mDevice->get_authenticator_id(mDevice);
}

int32_t IrisDaemonProxy::setActiveGroup(int32_t groupId, const uint8_t *pPath, ssize_t pathlen) {
    if (pathlen >= PATH_MAX || pathlen <= 0) {
        ALOGE("Bad path length: %zd", pathlen);
        return -1;
    }
    // Convert to null-terminated string
    char path_name[PATH_MAX];
    memcpy(path_name, pPath, pathlen);
    path_name[pathlen] = '\0';
    ALOG(LOG_VERBOSE, LOG_TAG, "setActiveGroup(%d, %s, %zu)", groupId, path_name, pathlen);
    return mDevice->set_active_group(mDevice, groupId, path_name);
}

int32_t IrisDaemonProxy::setPreviewSurface(int32_t groupId, const sp<Surface>& surface) {
    ALOG(LOG_VERBOSE, LOG_TAG, "setPreviewSurface(gid=%d)", groupId);
    if (surface.get()) {
        mSurface = surface;
        ANativeWindow *pANW = static_cast<ANativeWindow *>(mSurface.get());
        return mDevice->set_native_window(mDevice, groupId, (void *)pANW);
    } else {
        mSurface = NULL;
        return mDevice->set_native_window(mDevice, groupId, NULL);
    }
}

int64_t IrisDaemonProxy::openHal() {
    ALOG(LOG_VERBOSE, LOG_TAG, "nativeOpenHal()\n");
    const hw_module_t *hw_module = NULL;
    int err;

    err = hw_get_module(IRIS_HARDWARE_MODULE_ID, &hw_module);
    if (0 != err) {
        ALOGE("Can't open iris HW Module, error: %d", err);
        return 0;
    }
    if (NULL == hw_module) {
        ALOGE("No valid iris module");
        return 0;
    }

    mModule = reinterpret_cast<const iris_module_t*>(hw_module);
    if (mModule->common.methods->open == NULL) {
        ALOGE("No valid open method");
        return 0;
    }

    hw_device_t *device = NULL;
    if (0 != (err = mModule->common.methods->open(hw_module, NULL, &device))) {
        ALOGE("Can't open iris methods, error: %d", err);
        return 0;
    }

    if (kVersion != device->version) {
        ALOGE("Wrong iris version. Expected %d, got %d", kVersion, device->version);
        // return 0; // FIXME
    }

    mDevice = reinterpret_cast<iris_device_t*>(device);
    err = mDevice->set_notify(mDevice, hal_notify_callback);
    if (err < 0) {
        ALOGE("Failed in call to set_notify(), err=%d", err);
        return 0;
    }

    // Sanity check - remove
    if (mDevice->notify != hal_notify_callback) {
        ALOGE("NOTIFY not set properly: %p != %p", mDevice->notify, hal_notify_callback);
    }

    ALOG(LOG_VERBOSE, LOG_TAG, "iris HAL successfully initialized");
    return reinterpret_cast<int64_t>(mDevice); // This is just a handle
}

int32_t IrisDaemonProxy::closeHal() {
    ALOG(LOG_VERBOSE, LOG_TAG, "nativeCloseHal()\n");
    int err;

    if (mDevice == NULL) {
        ALOGE("No valid device");
        return -ENOSYS;
    }

    err = mDevice->common.close(reinterpret_cast<hw_device_t*>(mDevice));
    if (0 != err) {
        ALOGE("Can't close iris module, error: %d", err);
        return err;
    }

    mDevice = NULL;

    return 0;
}

void IrisDaemonProxy::binderDied(const wp<IBinder>& who) {
    ALOGD("binder died");
    int err = 0;

    err = closeHal();
    if (0 != err) {
        ALOGE("Can't close iris device, error: %d", err);
    }
    #if 0
    // for android 5.0
    if (mCallback->asBinder() == who) {
        mCallback = NULL;
    }
    #else
    if (IInterface::asBinder(mCallback) == who) {
        mCallback = NULL;
    }
    #endif
}

}
