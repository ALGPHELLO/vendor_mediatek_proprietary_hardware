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

#define LOG_TAG "IIrisDaemonCallback"
#include <stdint.h>
#include <sys/types.h>
#include <utils/Log.h>
#include <binder/Parcel.h>

#include "IIrisDaemonCallback.h"

namespace android {

class BpIrisDaemonCallback : public BpInterface<IIrisDaemonCallback>
{
public:
    BpIrisDaemonCallback(const sp<IBinder>& impl) :
            BpInterface<IIrisDaemonCallback>(impl) {
    }
    virtual status_t onEnrollResult(int64_t devId, int32_t irisId, int32_t gpId, int32_t remaining) {
        Parcel data, reply;
        data.writeInterfaceToken(IIrisDaemonCallback::getInterfaceDescriptor());
        data.writeInt64(devId);
        data.writeInt32(irisId);
        data.writeInt32(gpId);
        data.writeInt32(remaining);
        return remote()->transact(ON_ENROLL_RESULT, data, &reply, IBinder::FLAG_ONEWAY);
    }

    virtual status_t onAcquired(int64_t devId, int32_t acquiredInfoL, int32_t acquiredInfoR) {
        Parcel data, reply;
        data.writeInterfaceToken(IIrisDaemonCallback::getInterfaceDescriptor());
        data.writeInt64(devId);
        data.writeInt32(acquiredInfoL);
        data.writeInt32(acquiredInfoR);
        return remote()->transact(ON_ACQUIRED, data, &reply, IBinder::FLAG_ONEWAY);
    }

    virtual status_t onAuthenticated(int64_t devId, int32_t irisId, int32_t gpId) {
        Parcel data, reply;
        data.writeInterfaceToken(IIrisDaemonCallback::getInterfaceDescriptor());
        data.writeInt64(devId);
        data.writeInt32(irisId);
        data.writeInt32(gpId);
        return remote()->transact(ON_AUTHENTICATED, data, &reply, IBinder::FLAG_ONEWAY);
    }

    virtual status_t onError(int64_t devId, int32_t error) {
        Parcel data, reply;
        data.writeInterfaceToken(IIrisDaemonCallback::getInterfaceDescriptor());
        data.writeInt64(devId);
        data.writeInt32(error);
        return remote()->transact(ON_ERROR, data, &reply, IBinder::FLAG_ONEWAY);
    }

    virtual status_t onRemoved(int64_t devId, int32_t irisId, int32_t gpId) {
        Parcel data, reply;
        data.writeInterfaceToken(IIrisDaemonCallback::getInterfaceDescriptor());
        data.writeInt64(devId);
        data.writeInt32(irisId);
        data.writeInt32(gpId);
        return remote()->transact(ON_REMOVED, data, &reply, IBinder::FLAG_ONEWAY);
    }

    virtual status_t onEnumerate(int64_t devId, const int32_t *pIrisIds, const int32_t *pGpIds,
            int32_t idsSize) {
        Parcel data, reply;
        data.writeInterfaceToken(IIrisDaemonCallback::getInterfaceDescriptor());
        data.writeInt64(devId);
        data.writeInt32Array(idsSize, pIrisIds);
        data.writeInt32Array(idsSize, pGpIds);
        return remote()->transact(ON_ENUMERATE, data, &reply, IBinder::FLAG_ONEWAY);
    }

    virtual status_t onDisplay(int64_t devId,
            const uint8_t *pViewData, int32_t viewSize, int32_t width, int32_t height,
            const int32_t *pEyeRectL, const int32_t *pEyeRectR, int32_t eyeRectSize) {
        Parcel data, reply;
        data.writeInterfaceToken(IIrisDaemonCallback::getInterfaceDescriptor());
        data.writeInt64(devId);
        data.writeByteArray(viewSize, pViewData);
        data.writeInt32(width);
        data.writeInt32(height);
        data.writeInt32Array(eyeRectSize, pEyeRectL);
        data.writeInt32Array(eyeRectSize, pEyeRectR);
        return remote()->transact(ON_DISPLAY, data, &reply, IBinder::FLAG_ONEWAY);
    }

    virtual status_t onCaptureProgress(int64_t devId, int32_t staging) {
        Parcel data, reply;
        data.writeInterfaceToken(IIrisDaemonCallback::getInterfaceDescriptor());
        data.writeInt64(devId);
        data.writeInt32(staging);
        return remote()->transact(ON_CAPTURE_PROGRESS, data, &reply, IBinder::FLAG_ONEWAY);
    }

    virtual status_t onCaptureResult(int64_t devId,
            const uint8_t *pEncrpPid, int32_t pidSize,
            const uint8_t *pEncrpHMAC, int32_t hmacSize,
            const uint8_t *pEncrpSesKey, int32_t sesKeySize) {
        Parcel data, reply;
        data.writeInterfaceToken(IIrisDaemonCallback::getInterfaceDescriptor());
        data.writeInt64(devId);
        data.writeByteArray(pidSize, pEncrpPid);
        data.writeByteArray(hmacSize, pEncrpHMAC);
        data.writeByteArray(sesKeySize, pEncrpSesKey);
        return remote()->transact(ON_CAPTURE_RESULT, data, &reply, IBinder::FLAG_ONEWAY);
    }
};

IMPLEMENT_META_INTERFACE(IrisDaemonCallback,
        "com.mediatek.hardware.iris.IIrisDaemonCallback");

}; // namespace android
