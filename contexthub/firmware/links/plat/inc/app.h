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

#ifndef _PLAT_MT6XXX_APP_H_
#define _PLAT_MT6XXX_APP_H_

#include "FreeRTOS.h"
#include "task.h"

#define CFG_CHRE_SET_INTERRUPT
//#define asm   __asm
#define osLog(level, x...)      \
({                              \
    if (level <= LOG_INFO)      \
        printf(x);              \
})

//#define heapAlloc(x) pvPortMalloc(x)
//#define heapFree(x) vPortFree(x)
#define mpuAllowRamExecution(true)  // todo
#define mpuAllowRomWrite(true)  // todo

#define asm    __asm

extern TaskHandle_t CHRE_TaskHandle;

#define APP_ID_VENDOR_MTK       0x204d544b20ULL // " MTK "
#define MTK_APP_ID_WRAP(SENSOR_TYPE, DEVICE_ID, DEVICE_SN) ((SENSOR_TYPE << 16) | (DEVICE_ID << 8) | DEVICE_SN) // get SENS_TYPE from sensType.h

struct PlatAppInfo {
    void *got;
};

#endif // _PLAT_MT6XXX_APP_H_

