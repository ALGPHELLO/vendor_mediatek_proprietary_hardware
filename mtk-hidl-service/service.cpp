/*
 * Copyright 2016 The Android Open Source Project
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

#define LOG_TAG "mtk-hidl-service"

//#include <android/hardware/power/1.0/IPower.h>
#include <vendor/mediatek/hardware/power/1.1/IPower.h>
#include <android/hardware/memtrack/1.0/IMemtrack.h>
#include <android/hardware/vibrator/1.0/IVibrator.h>
#include <android/hardware/light/2.0/ILight.h>
#include <android/hardware/thermal/1.0/IThermal.h>
#include <hidl/LegacySupport.h>
#include <log/log.h>
#include <lbs_hidl_service.h>
#include <pthread.h>
#include "powerd_int.h"
#include <vendor/mediatek/hardware/nvram/1.0/INvram.h>
#include <vendor/mediatek/hardware/gnss/1.1/IMtkGnss.h>
#include <android/hardware/graphics/allocator/2.0/IAllocator.h>
#include <vendor/mediatek/hardware/mtkcodecservice/1.1/IMtkCodecService.h>
#include <1.0/default/CryptoFactory.h>
#include <1.0/default/DrmFactory.h>


using ::android::hardware::configureRpcThreadpool;
using ::android::hardware::joinRpcThreadpool;
//using ::android::hardware::power::V1_0::IPower;
using ::vendor::mediatek::hardware::power::V1_1::IPower;
using ::android::hardware::memtrack::V1_0::IMemtrack;
using ::android::hardware::vibrator::V1_0::IVibrator;
using ::android::hardware::light::V2_0::ILight;
using ::android::hardware::thermal::V1_0::IThermal;
using ::vendor::mediatek::hardware::nvram::V1_0::INvram;
using ::vendor::mediatek::hardware::gnss::V1_1::IMtkGnss;
using ::android::hardware::graphics::allocator::V2_0::IAllocator;
using vendor::mediatek::hardware::mtkcodecservice::V1_1::IMtkCodecService;
using ::android::hardware::drm::V1_0::ICryptoFactory;
using ::android::hardware::drm::V1_0::IDrmFactory;

using ::android::hardware::registerPassthroughServiceImplementation;
using ::android::OK;
using ::android::status_t;


#define register(service) do { \
    status_t err = registerPassthroughServiceImplementation<service>(); \
    if (err != OK) { \
        ALOGE("Err %d while registering " #service, err); \
    } \
} while(false)

void* mtkHidlDaemon(void *data)
{
    powerd_core_pre_init();
    ALOGI("mtkHidlDaemon - data:%p", data);
    powerd_main(0, NULL);

    return NULL;
}

void* mtkHidlService(void *data)
{
    int ret = 0;
    ALOGI("mtkHidlService - data:%p", data);

    configureRpcThreadpool(1, true /* will call join */);

    register(IMemtrack);

    register(IVibrator);
    register(ILight);
    register(IThermal);

    register(INvram);
    register(IMtkGnss);
    register(IPower);
    register(IAllocator);
    register(IMtkCodecService);

    register(IDrmFactory);
    register(ICryptoFactory);

    ::vendor::mediatek::hardware::lbs::V1_0::implementation::cpp_main();

    joinRpcThreadpool();

    ALOGI("mtkHidlService - ret:%d", ret);

    return NULL;
}

int main() {
    ALOGE("Threadpool start run process!");
    pthread_t daemonThread, serviceThread;
    pthread_attr_t attr;

    /* handler */
    pthread_attr_init(&attr);
    pthread_create(&daemonThread, &attr, mtkHidlDaemon, NULL);

    /* service */
    pthread_attr_init(&attr);
    pthread_create(&serviceThread, &attr, mtkHidlService, NULL);

    pthread_join(daemonThread, NULL);
    pthread_join(serviceThread, NULL);

    ALOGE("Threadpool exited!!!");
    return -1;
}
