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

#ifndef IIRIS_DAEMON_CALLBACK_H_
#define IIRIS_DAEMON_CALLBACK_H_

#include <inttypes.h>
#include <utils/Errors.h>
#include <binder/IInterface.h>
#include <binder/Parcel.h>

namespace android {

/*
* Communication channel back to IrisService.java
*/
class IIrisDaemonCallback : public IInterface {
    public:
        // must be kept in sync with IIrisService.aidl
        enum {
            ON_ENROLL_RESULT = IBinder::FIRST_CALL_TRANSACTION + 0,
            ON_ACQUIRED = IBinder::FIRST_CALL_TRANSACTION + 1,
            ON_AUTHENTICATED = IBinder::FIRST_CALL_TRANSACTION + 2,
            ON_ERROR = IBinder::FIRST_CALL_TRANSACTION + 3,
            ON_REMOVED = IBinder::FIRST_CALL_TRANSACTION + 4,
            ON_ENUMERATE = IBinder::FIRST_CALL_TRANSACTION + 5,
            ON_DISPLAY = IBinder::FIRST_CALL_TRANSACTION + 6,
            ON_CAPTURE_PROGRESS = IBinder::FIRST_CALL_TRANSACTION + 7,
            ON_CAPTURE_RESULT = IBinder::FIRST_CALL_TRANSACTION + 8,
        };

        virtual status_t onEnrollResult(int64_t devId, int32_t irisId, int32_t gpId, int32_t remaining) = 0;
        virtual status_t onAcquired(int64_t devId, int32_t acquiredInfoL, int32_t acquiredInfoR) = 0;
        virtual status_t onAuthenticated(int64_t devId, int32_t irisId, int32_t groupId) = 0;
        virtual status_t onError(int64_t devId, int32_t error) = 0;
        virtual status_t onRemoved(int64_t devId, int32_t irisId, int32_t groupId) = 0;
        virtual status_t onEnumerate(int64_t devId, const int32_t *pIrisIds,
                const int32_t *pGpIds, int32_t idsSize) = 0;
        virtual status_t onDisplay(int64_t devId,
                const uint8_t *pViewData, int32_t viewSize, int32_t width, int32_t height,
                const int32_t *pEyeRectL, const int32_t *pEyeRectR, int32_t eyeRectSize) = 0;
        virtual status_t onCaptureProgress(int64_t devId, int32_t staging) = 0;
        virtual status_t onCaptureResult(int64_t devId,
                const uint8_t *pEncrpPid, int32_t pidSize,
                const uint8_t *pEncrpHMAC, int32_t hmacSize,
                const uint8_t *pEncrpSesKey, int32_t sesKeySize) = 0;

        DECLARE_META_INTERFACE(IrisDaemonCallback);
};

}; // namespace android

#endif // IIRIS_DAEMON_CALLBACK_H_
