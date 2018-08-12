/*
 * Copyright (C) 2016 The Android Open Source Project
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

#define LOG_TAG "vendor.mediatek.hardware.power@1.1-service"

//#include <android/hardware/power/1.0/IPower.h>
#include <vendor/mediatek/hardware/power/1.1/IPower.h>
#include <hidl/LegacySupport.h>
#include <pthread.h>
#include "PowerManager.h"

//using android::hardware::power::V1_0::IPower;
using vendor::mediatek::hardware::power::V1_1::IPower;
using android::hardware::defaultPassthroughServiceImplementation;

pthread_mutex_t g_mutex;
pthread_cond_t  g_cond;
bool powerd_done = false;

#if 0
void* mtkPowerHandler(void *data)
{
    ALOGI("mtkPowerHandler - data:%p", data);

    while(1) {
        pthread_mutex_lock(&g_mutex);
        pthread_cond_wait(&g_cond, &g_mutex);
        ALOGI("mtkPowerHandler - TODO");
        pthread_mutex_unlock(&g_mutex);
    }
}
#endif

void* mtkPowerService(void *data)
{
    int ret;
    ALOGI("mtkPowerService - data:%p", data);

    pthread_mutex_lock(&g_mutex);
    while (powerd_done == false) {
       pthread_cond_wait(&g_cond, &g_mutex);
    }
    pthread_mutex_unlock(&g_mutex);

    ret = defaultPassthroughServiceImplementation<IPower>();
    ALOGI("mtkPowerService - ret:%d", ret);
    return NULL;
}

int main() {
    pthread_t handlerThread, serviceThread;
    pthread_attr_t attr;
    //struct sched_param param;

    /* init */
    pthread_mutex_init(&g_mutex, NULL);
    pthread_cond_init(&g_cond, NULL);

    /* handler */
    pthread_attr_init(&attr);
    pthread_create(&handlerThread, &attr, mtkPowerManager, NULL);

    /* service */
    pthread_attr_init(&attr);
    pthread_create(&serviceThread, &attr, mtkPowerService, NULL);

    pthread_join(handlerThread, NULL);
    pthread_join(serviceThread, NULL);
    return 0;
    //return defaultPassthroughServiceImplementation<IPower>();
}

