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

#ifndef ANDROID_HARDWARE_POWER_V1_0_POWER_H
#define ANDROID_HARDWARE_POWER_V1_0_POWER_H

//#include <android/hardware/power/1.0/IPower.h>
#include <vendor/mediatek/hardware/power/1.1/IPower.h>
#include <hwbinder/IPCThreadState.h>

#include <hidl/Status.h>

#include <hidl/MQDescriptor.h>
namespace vendor {
namespace mediatek {
namespace hardware {
namespace power {
namespace V1_1 {
namespace implementation {

using ::android::hardware::power::V1_0::Feature;
using ::android::hardware::power::V1_0::PowerHint;
using ::android::hardware::power::V1_0::PowerStatePlatformSleepState;
using ::android::hardware::power::V1_0::Status;
using ::vendor::mediatek::hardware::power::V1_1::IPower;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::hidl_vec;
using ::android::hardware::hidl_string;
using ::android::hardware::IPCThreadState;
using ::android::sp;

struct Power : public IPower {
    Power(power_module_t* module);
    ~Power();
    Return<void> setInteractive(bool interactive)  override;
    Return<void> powerHint(PowerHint hint, int32_t data)  override;
    Return<void> setFeature(Feature feature, bool activate)  override;
    Return<void> getPlatformLowPowerStats(getPlatformLowPowerStats_cb _hidl_cb)  override;
    Return<void> mtkPowerHint(MtkPowerHint hint, int32_t data)  override;
    Return<void> mtkCusPowerHint(MtkCusPowerHint hint, int32_t data)  override;
    Return<void> notifyAppState(const hidl_string& packName, const hidl_string& actName, int32_t pid, MtkActState state)  override;
    Return<int32_t> querySysInfo(MtkQueryCmd cmd, int32_t param)  override;
    Return<int32_t> scnReg()  override;
    Return<void> scnConfig(int32_t hdl, MtkPowerCmd cmd, int32_t param1, int32_t param2, int32_t param3, int32_t param4)  override;
    Return<void> scnUnreg(int32_t hdl)  override;
    Return<void> scnEnable(int32_t hdl, int32_t timeout)  override;
    Return<void> scnDisable(int32_t hdl)  override;

  private:
    power_module_t* mModule;
};

extern "C" IPower* HIDL_FETCH_IPower(const char* name);

}  // namespace implementation
}  // namespace V1_1
}  // namespace power
}  // namespace hardware
}  // namespace mediatek
}  // namespace vendor

#endif  // ANDROID_HARDWARE_POWER_V1_0_POWER_H
