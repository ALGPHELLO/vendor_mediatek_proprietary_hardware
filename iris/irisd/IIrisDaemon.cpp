/*
 * Copyright (C) 2017 The SYKEAN Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/
#define LOG_TAG "IIrisDaemon"

#include <inttypes.h>

#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <binder/PermissionCache.h>
#include <utils/String16.h>
#include <utils/Looper.h>
#include <keystore/IKeystoreService.h>
#include <keystore/keystore.h> // for error code
#include <hardware/hardware.h>
#include <hardware/iris.h>
#include <hardware/mtk_hw_auth_token.h>

#include <log/log.h>

#include "IIrisDaemon.h"
#include "IIrisDaemonCallback.h"

namespace android {

static const String16 USE_IRIS_PERMISSION("com.mediatek.permission.USE_IRIS");
static const String16 MANAGE_IRIS_PERMISSION("com.mediatek.permission.MANAGE_IRIS");
static const String16 HAL_IRIS_PERMISSION("com.mediatek.permission.MANAGE_IRIS"); // TODO
static const String16 DUMP_PERMISSION("android.permission.DUMP");

const android::String16
IIrisDaemon::descriptor("com.mediatek.hardware.iris.IIrisDaemon");

const android::String16&
IIrisDaemon::getInterfaceDescriptor() const {
    return IIrisDaemon::descriptor;
}

status_t BnIrisDaemon::onTransact(uint32_t code, const Parcel& data, Parcel* reply,
        uint32_t flags) {
    switch(code) {
        case AUTHENTICATE: {
            CHECK_INTERFACE(IIrisDaemon, data, reply);
            if (!checkPermission(HAL_IRIS_PERMISSION)) {
                return PERMISSION_DENIED;
            }
            const uint64_t sessionId = data.readInt64();
            const uint32_t groupId = data.readInt32();
            const int32_t ret = authenticate(sessionId, groupId);
            reply->writeNoException();
            reply->writeInt32(ret);
            return NO_ERROR;
        };
        case CANCEL_AUTHENTICATION: {
            CHECK_INTERFACE(IIrisDaemon, data, reply);
            if (!checkPermission(HAL_IRIS_PERMISSION)) {
                return PERMISSION_DENIED;
            }
            const int32_t ret = stopAuthentication();
            reply->writeNoException();
            reply->writeInt32(ret);
            return NO_ERROR;
        }
        case ENROLL: {
            CHECK_INTERFACE(IIrisDaemon, data, reply);
            if (!checkPermission(HAL_IRIS_PERMISSION)) {
                return PERMISSION_DENIED;
            }
            const ssize_t tokenSize = data.readInt32();
            const uint8_t* token = static_cast<const uint8_t *>(data.readInplace(tokenSize));
            const int32_t groupId = data.readInt32();
            const int32_t timeout = data.readInt32();
            const int32_t ret = enroll(token, tokenSize, groupId, timeout);
            reply->writeNoException();
            reply->writeInt32(ret);
            return NO_ERROR;
        }
        case CANCEL_ENROLLMENT: {
            CHECK_INTERFACE(IIrisDaemon, data, reply);
            if (!checkPermission(HAL_IRIS_PERMISSION)) {
                return PERMISSION_DENIED;
            }
            const int32_t ret = stopEnrollment();
            reply->writeNoException();
            reply->writeInt32(ret);
            return NO_ERROR;
        }
        case PRE_ENROLL: {
            CHECK_INTERFACE(IIrisDaemon, data, reply);
            if (!checkPermission(HAL_IRIS_PERMISSION)) {
                return PERMISSION_DENIED;
            }
            const uint64_t ret = preEnroll();
            reply->writeNoException();
            reply->writeInt64(ret);
            return NO_ERROR;
        }
        case POST_ENROLL: {
            CHECK_INTERFACE(IIrisDaemon, data, reply);
            if (!checkPermission(HAL_IRIS_PERMISSION)) {
                return PERMISSION_DENIED;
            }
            const int32_t ret = postEnroll();
            reply->writeNoException();
            reply->writeInt32(ret);
            return NO_ERROR;
        }
        case REMOVE: {
            CHECK_INTERFACE(IIrisDaemon, data, reply);
            if (!checkPermission(HAL_IRIS_PERMISSION)) {
                return PERMISSION_DENIED;
            }
            const int32_t irisId = data.readInt32();
            const int32_t groupId = data.readInt32();
            const int32_t ret = remove(irisId, groupId);
            reply->writeNoException();
            reply->writeInt32(ret);
            return NO_ERROR;
        }
        case GET_AUTHENTICATOR_ID: {
            CHECK_INTERFACE(IIrisDaemon, data, reply);
            if (!checkPermission(HAL_IRIS_PERMISSION)) {
                return PERMISSION_DENIED;
            }
            const uint64_t ret = getAuthenticatorId();
            reply->writeNoException();
            reply->writeInt64(ret);
            return NO_ERROR;
        }
        case SET_ACTIVE_GROUP: {
            CHECK_INTERFACE(IIrisDaemon, data, reply);
            if (!checkPermission(HAL_IRIS_PERMISSION)) {
                return PERMISSION_DENIED;
            }
            const int32_t group = data.readInt32();
            const ssize_t pathSize = data.readInt32();
            const uint8_t* path = static_cast<const uint8_t *>(data.readInplace(pathSize));
            const int32_t ret = setActiveGroup(group, path, pathSize);
            reply->writeNoException();
            reply->writeInt32(ret);
            return NO_ERROR;
        }
        case SET_PREVIEW_QUEUE: {
            CHECK_INTERFACE(IIrisDaemon, data, reply);
            if (!checkPermission(HAL_IRIS_PERMISSION)) {
                return PERMISSION_DENIED;
            }

            // read group id & surface from the received parcel
            const int32_t groupId = data.readInt32();

            sp<Surface> surface = [&data]() -> Surface*
            {
                // NOTE: This data is written by the generated IIrisDaemon.java
                // 1 for a valid surface and 0 for a nullity
                if (data.readInt32() != 1) {
                    ALOGW("parcel(%" PRIxPTR ") surface(null)",
                            reinterpret_cast<intptr_t>(&data));
                    return nullptr;
                }

                // NOTE: This must be kept synchronized with the native parceling code
                // in frameworks/native/libs/Surface.cpp
                android::view::Surface surfaceShim;
                surfaceShim.readFromParcel(&data, /*nameAlreadyRead*/false);

                if (surfaceShim.graphicBufferProducer != nullptr) {
                    // we have a new IGraphicBufferProducer, create a new Surface for it
                    return new Surface(surfaceShim.graphicBufferProducer, false);
                }

                return nullptr;
            }();

            const int32_t ret = setPreviewSurface(groupId, surface);
            reply->writeNoException();
            reply->writeInt32(ret);
            return NO_ERROR;
        }
        case CAPTURE: {
            CHECK_INTERFACE(IIrisDaemon, data, reply);
            if (!checkPermission(HAL_IRIS_PERMISSION)) {
                return PERMISSION_DENIED;
            }
            const ssize_t pidSize = data.readInt32();
            const uint8_t *pPidData = static_cast<const uint8_t *>(data.readInplace(pidSize));
            const int32_t pidType = data.readInt32();
            const int32_t bioType = data.readInt32();
            const ssize_t certSize = data.readInt32();
            const uint8_t *pCertChain = static_cast<const uint8_t *>(data.readInplace(certSize));
            const int32_t groupId = data.readInt32();
            const int32_t ret = capture(pPidData, pidSize, pidType, bioType,
                                        pCertChain, certSize, groupId);
            reply->writeNoException();
            reply->writeInt32(ret);
            return NO_ERROR;
        }
        case CANCEL_CAPTURE: {
            CHECK_INTERFACE(IIrisDaemon, data, reply);
            if (!checkPermission(HAL_IRIS_PERMISSION)) {
                return PERMISSION_DENIED;
            }
            const int32_t ret = stopCapture();
            reply->writeNoException();
            reply->writeInt32(ret);
            return NO_ERROR;
        }
        case OPEN_HAL: {
            CHECK_INTERFACE(IIrisDaemon, data, reply);
            if (!checkPermission(HAL_IRIS_PERMISSION)) {
                return PERMISSION_DENIED;
            }
            const int64_t ret = openHal();
            reply->writeNoException();
            reply->writeInt64(ret);
            return NO_ERROR;
        }
        case CLOSE_HAL: {
            CHECK_INTERFACE(IIrisDaemon, data, reply);
            if (!checkPermission(HAL_IRIS_PERMISSION)) {
                return PERMISSION_DENIED;
            }
            const int32_t ret = closeHal();
            reply->writeNoException();
            reply->writeInt32(ret);
            return NO_ERROR;
        }
        case INIT: {
            CHECK_INTERFACE(IIrisDaemon, data, reply);
            if (!checkPermission(HAL_IRIS_PERMISSION)) {
                return PERMISSION_DENIED;
            }
            sp<IIrisDaemonCallback> callback =
                    interface_cast<IIrisDaemonCallback>(data.readStrongBinder());
            init(callback);
            reply->writeNoException();
            return NO_ERROR;
        }
        default:
            return BBinder::onTransact(code, data, reply, flags);
    }
};

bool BnIrisDaemon::checkPermission(const String16& permission) {
    const IPCThreadState* ipc = IPCThreadState::self();
    const int calling_pid = ipc->getCallingPid();
    const int calling_uid = ipc->getCallingUid();
    #if 0
    // for android 5.0
    if ((calling_uid != AID_ROOT) && (calling_uid != AID_SYSTEM)) {
        return PermissionCache::checkPermission(permission, calling_pid, calling_uid);
    } else {
        return true;
    }
    #else
    return PermissionCache::checkPermission(permission, calling_pid, calling_uid);
    #endif
}


}; // namespace android
