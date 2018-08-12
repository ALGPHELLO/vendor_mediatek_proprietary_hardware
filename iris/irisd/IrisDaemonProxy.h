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

#ifndef IRIS_DAEMON_PROXY_H_
#define IRIS_DAEMON_PROXY_H_

#include "IIrisDaemon.h"
#include "IIrisDaemonCallback.h"

namespace android {

class IrisDaemonProxy : public BnIrisDaemon {
    public:
        static IrisDaemonProxy* getInstance() {
            if (sInstance == NULL) {
                sInstance = new IrisDaemonProxy();
            }
            return sInstance;
        }

        // These reflect binder methods.
        virtual void init(const sp<IIrisDaemonCallback>& callback);
        virtual int32_t enroll(const uint8_t *token, ssize_t tokenLength, int32_t groupId,
                int32_t timeout);
        virtual uint64_t preEnroll();
        virtual int32_t postEnroll();
        virtual int32_t stopEnrollment();
        virtual int32_t authenticate(uint64_t sessionId, uint32_t groupId);
        virtual int32_t stopAuthentication();
        virtual int32_t capture(const uint8_t *pPidData, ssize_t pidLen, int32_t pidType,
                int32_t bioType, const uint8_t *pCertChain, ssize_t certLen, int32_t groupId);
        virtual int32_t stopCapture();
        virtual int32_t remove(int32_t irisId, int32_t groupId);
        virtual uint64_t getAuthenticatorId();
        virtual int32_t setActiveGroup(int32_t groupId, const uint8_t *pPath, ssize_t pathLen);
        virtual int32_t setPreviewSurface(int32_t groupId, const sp<Surface>& surface);
        virtual int64_t openHal();
        virtual int32_t closeHal();

    private:
        IrisDaemonProxy();
        virtual ~IrisDaemonProxy();
        void binderDied(const wp<IBinder>& who);
        void notifyKeystore(const uint8_t *auth_token, const size_t auth_token_length);
        static void hal_notify_callback(const iris_msg_t *msg);

        static IrisDaemonProxy* sInstance;
        iris_module_t const* mModule;
        iris_device_t* mDevice;
        sp<IIrisDaemonCallback> mCallback;
        sp<Surface> mSurface;
};

} // namespace android

#endif // IRIS_DAEMON_PROXY_H_
