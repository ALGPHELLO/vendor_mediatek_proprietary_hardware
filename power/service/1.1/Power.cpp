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

#define LOG_TAG "vendor.mediatek.hardware.power@1.1-impl"
#define ATRACE_TAG (ATRACE_TAG_POWER | ATRACE_TAG_HAL)

#include <log/log.h>
#include <cutils/trace.h>
#include <string.h>
#include <hardware/hardware.h>
#include <hardware/power.h>

#include "power_util.h"

#include "Power.h"

namespace vendor {
namespace mediatek {
namespace hardware {
namespace power {
namespace V1_1 {
namespace implementation {

static int nPwrDebugLogEnable = 0;

#ifdef ALOGD
#undef ALOGD
#define ALOGD(...) do{if(nPwrDebugLogEnable)((void)ALOG(LOG_INFO, LOG_TAG, __VA_ARGS__));}while(0)
#endif

Power::Power(power_module_t *module) : mModule(module) {
    if (mModule)
        mModule->init(mModule);
}

Power::~Power() {
    delete(mModule);
}

std::string PowerHintToString(PowerHint hint, int32_t data) {
    char powerHintString[50];
    switch (hint) {
        case PowerHint::VSYNC:
            snprintf(powerHintString, sizeof(powerHintString), "%s %s", "VSYNC",
                     data ? "requested" : "not requested");
            break;
        case PowerHint::INTERACTION:
            snprintf(powerHintString, sizeof(powerHintString), "%s %d ms", "INTERACTION", data);
            break;
        case PowerHint::VIDEO_ENCODE:
            snprintf(powerHintString, sizeof(powerHintString), "%s", "VIDEO_ENCODE");
            break;
        case PowerHint::VIDEO_DECODE:
            snprintf(powerHintString, sizeof(powerHintString), "%s", "VIDEO_DECODE");
            break;
        case PowerHint::LOW_POWER:
            snprintf(powerHintString, sizeof(powerHintString), "%s %s", "LOW_POWER",
                     data ? "activated" : "deactivated");
            break;
        case PowerHint::SUSTAINED_PERFORMANCE:
            snprintf(powerHintString, sizeof(powerHintString), "%s %s", "SUSTAINED_PERFORMANCE",
                     data ? "activated" : "deactivated");
            break;
        case PowerHint::VR_MODE:
            snprintf(powerHintString, sizeof(powerHintString), "%s %s", "VR_MODE",
                     data ? "activated" : "deactivated");
            break;
        case PowerHint::LAUNCH:
            snprintf(powerHintString, sizeof(powerHintString), "%s %s", "LAUNCH",
                     data ? "start" : "end");
            break;
        default:
            snprintf(powerHintString, sizeof(powerHintString), "%s", "UNKNOWN");
            break;
    }
    return powerHintString;
}
// Methods from ::android::hardware::power::V1.0::IPower follow.
Return<void> Power::setInteractive(bool interactive)  {
    /*if (mModule->setInteractive)
        mModule->setInteractive(mModule, interactive ? 1 : 0);
    */
    struct tPowerData vPowerData;
    struct tPowerData *vpRspData = NULL;
    struct tScnData vScnData;

    if (interactive) {
      ALOGI("%s %s", __func__, "Restore All");
      vPowerData.msg  = POWER_MSG_SCN_RESTORE_ALL;
    } else {
      ALOGI("%s %s", __func__, "Disable All");
      vPowerData.msg  = POWER_MSG_SCN_DISABLE_ALL;
    }
    vPowerData.pBuf = (void*)&vScnData;

    power_msg(&vPowerData, (void **) &vpRspData);

    if (vpRspData) {
        if(vpRspData->pBuf)
            free(vpRspData->pBuf);
        free(vpRspData);
    }

    return Void();
}

Return<void> Power::powerHint(PowerHint hint, int32_t data)  {
    int32_t param = data;
    struct tPowerData vPowerData;
    struct tHintData  vHintData;
    struct tPowerData *vpRspData = NULL;

    if (ATRACE_ENABLED()) {
        ATRACE_BEGIN(PowerHintToString(hint, data).c_str());
    }

    switch (static_cast<power_hint_t>(hint)) {
        case POWER_HINT_LAUNCH:
            ALOGI("PowerHint hint:%d, data:%d", hint, data);
            vHintData.hint = (int)hint;

            if (data)
                vHintData.data = (int32_t) MtkHintOp::MTK_HINT_ALWAYS_ENABLE;
            else
                vHintData.data = data;

            vPowerData.msg  = POWER_MSG_MTK_HINT;
            vPowerData.pBuf = (void*)&vHintData;

            power_msg(&vPowerData, (void **) &vpRspData);

            break;
        default:
            break;
    }

    if (vpRspData) {
        if(vpRspData->pBuf)
            free(vpRspData->pBuf);
        free(vpRspData);
    }

    ATRACE_END();

    return Void();
}

Return<void> Power::setFeature(Feature feature, bool activate)  {
    if (mModule->setFeature)
        mModule->setFeature(mModule, static_cast<feature_t>(feature),
                activate ? 1 : 0);
    return Void();
}

Return<void> Power::getPlatformLowPowerStats(getPlatformLowPowerStats_cb _hidl_cb)  {
    hidl_vec<PowerStatePlatformSleepState> states;
    ssize_t number_platform_modes;
    size_t *voters = nullptr;
    power_state_platform_sleep_state_t *legacy_states = nullptr;
    int ret;

    if (mModule->get_number_of_platform_modes == nullptr ||
            mModule->get_voter_list == nullptr ||
            mModule->get_platform_low_power_stats == nullptr)
    {
        _hidl_cb(states, Status::SUCCESS);
        return Void();
    }

    number_platform_modes = mModule->get_number_of_platform_modes(mModule);
    if (number_platform_modes)
    {
       if ((ssize_t) (SIZE_MAX / sizeof(size_t)) <= number_platform_modes)  // overflow
           goto done;
       voters = new (std::nothrow) size_t [number_platform_modes];
       if (voters == nullptr)
           goto done;

       ret = mModule->get_voter_list(mModule, voters);
       if (ret != 0)
           goto done;

       if ((ssize_t) (SIZE_MAX / sizeof(power_state_platform_sleep_state_t))
           <= number_platform_modes)  // overflow
           goto done;
       legacy_states = new (std::nothrow)
           power_state_platform_sleep_state_t [number_platform_modes];
       if (legacy_states == nullptr)
           goto done;

       for (int i = 0; i < number_platform_modes; i++)
       {
          legacy_states[i].voters = nullptr;
          legacy_states[i].voters = new power_state_voter_t [voters[i]];
          if (legacy_states[i].voters == nullptr)
              goto done;
       }

       ret = mModule->get_platform_low_power_stats(mModule, legacy_states);
       if (ret != 0)
           goto done;

       states.resize(number_platform_modes);
       for (int i = 0; i < number_platform_modes; i++)
       {
          power_state_platform_sleep_state_t& legacy_state = legacy_states[i];
          PowerStatePlatformSleepState& state = states[i];
          state.name = legacy_state.name;
          state.residencyInMsecSinceBoot = legacy_state.residency_in_msec_since_boot;
          state.totalTransitions = legacy_state.total_transitions;
          state.supportedOnlyInSuspend = legacy_state.supported_only_in_suspend;
          state.voters.resize(voters[i]);
          for(size_t j = 0; j < voters[i]; j++)
          {
              state.voters[j].name = legacy_state.voters[j].name;
              state.voters[j].totalTimeInMsecVotedForSinceBoot = legacy_state.voters[j].total_time_in_msec_voted_for_since_boot;
              state.voters[j].totalNumberOfTimesVotedSinceBoot = legacy_state.voters[j].total_number_of_times_voted_since_boot;
          }
       }
    }
done:
    if (legacy_states)
    {
        for (int i = 0; i < number_platform_modes; i++)
        {
            if(legacy_states[i].voters)
                delete(legacy_states[i].voters);
        }
    }
    delete[] legacy_states;
    delete[] voters;
    _hidl_cb(states, Status::SUCCESS);
    return Void();
}

Return<void> Power::mtkPowerHint(MtkPowerHint hint, int32_t data)  {
    ALOGD("mtkPowerHint hint:%d, data:%d", hint, data);

    struct tPowerData vPowerData;
    struct tHintData  vHintData;
    struct tPowerData *vpRspData = NULL;

    if(hint < MtkPowerHint::MTK_POWER_HINT_BASE || hint >= MtkPowerHint::MTK_POWER_HINT_NUM)
        return Void();

    vHintData.hint = (int)hint;
    vHintData.data = data;
    vPowerData.msg  = POWER_MSG_MTK_HINT;
    vPowerData.pBuf = (void*)&vHintData;

    //ALOGI("%s %p", __func__, &vPowerData);

    power_msg(&vPowerData, (void **) &vpRspData);

    //ALOGI("%s %p", __func__, vpRspData);
    if (vpRspData) {
        if(vpRspData->pBuf)
            free(vpRspData->pBuf);
        free(vpRspData);
    }

    return Void();
}

Return<void> Power::mtkCusPowerHint(MtkCusPowerHint hint, int32_t data)  {
    ALOGI("mtkCusPowerHint hint:%d, data:%d", hint, data);

    struct tPowerData vPowerData;
    struct tHintData  vHintData;
    struct tPowerData *vpRspData = NULL;

    if(hint >= MtkCusPowerHint::MTK_CUS_HINT_NUM)
        return Void();

    vHintData.hint = (int)hint;
    vHintData.data = data;
    vPowerData.msg  = POWER_MSG_MTK_CUS_HINT;
    vPowerData.pBuf = (void*)&vHintData;

    //ALOGI("%s %p", __func__, &vPowerData);

    power_msg(&vPowerData, (void **) &vpRspData);

    //ALOGI("%s %p", __func__, vpRspData);
    if (vpRspData) {
        if(vpRspData->pBuf)
            free(vpRspData->pBuf);
        free(vpRspData);
    }

    return Void();
}

Return<void> Power::notifyAppState(const hidl_string& packName, const hidl_string& actName, int32_t pid, MtkActState state) {
    ALOGI("notifyAppState pack:%s, act:%s, pid:%d, state:%d", packName.c_str(), actName.c_str(), pid, state);

    struct tPowerData vPowerData;
    struct tAppStateData vStateData;
    struct tPowerData *vpRspData = NULL;

    strncpy(vStateData.pack, packName.c_str(), (MAX_NAME_LEN - 1));
    vStateData.pack[(MAX_NAME_LEN-1)] = '\0';
    strncpy(vStateData.activity, actName.c_str(), (MAX_NAME_LEN - 1));
    vStateData.activity[(MAX_NAME_LEN-1)] = '\0';
    vStateData.pid = pid;
    vStateData.state = (int)state;
    vPowerData.msg  = POWER_MSG_NOTIFY_STATE;
    vPowerData.pBuf = (void*)&vStateData;

    //ALOGI("%s %p", __func__, &vScnData);

    power_msg(&vPowerData, (void **) &vpRspData);

    //ALOGI("%s %p", __func__, vpRspData);

    if (vpRspData) {
        if(vpRspData->pBuf)
            free(vpRspData->pBuf);
        free(vpRspData);
    }

    return Void();
}

Return<int32_t> Power::querySysInfo(MtkQueryCmd cmd, int32_t param)  {
    ALOGD("querySysInfo cmd:%d, param:%d", (int)cmd, param);

    struct tPowerData vPowerData;
    struct tPowerData *vpRspData = NULL;
    struct tQueryInfoData vQueryData;

    vQueryData.cmd = (int)cmd;
    vQueryData.param = param;
    vPowerData.msg  = POWER_MSG_QUERY_INFO;
    vPowerData.pBuf = (void*)&vQueryData;

    //ALOGI("%s %p", __func__, &vPowerData);
    vQueryData.value = -1;
    power_msg(&vPowerData, (void **) &vpRspData);

    /* debug msg */
    if((int)cmd == CMD_SET_DEBUG_MSG) {
        if(param == 0) {
            nPwrDebugLogEnable = 0;
        }
        else if(param == 1) {
            nPwrDebugLogEnable = 1;
        }
    }

    //ALOGI("%s %p", __func__, vpRspData);
    if (vpRspData) {
        if(vpRspData->pBuf) {
            vQueryData.value = ((tQueryInfoData*)(vpRspData->pBuf))->value;
            free(vpRspData->pBuf);
        }
        free(vpRspData);
    }

    return vQueryData.value;
}

Return<int32_t> Power::scnReg() {
    ALOGD("scnReg");
    const int pid = IPCThreadState::self()->getCallingPid();
    const int uid = IPCThreadState::self()->getCallingUid();

    struct tPowerData vPowerData;
    struct tPowerData *vpRspData = NULL;
    struct tScnData   vScnData;

    vScnData.param1 = pid;
    vScnData.param2 = uid;
    vPowerData.msg  = POWER_MSG_SCN_REG;
    vPowerData.pBuf = (void*)&vScnData;

    //ALOGI("%s %p", __func__, &vScnData);
    vScnData.handle = -1;
    power_msg(&vPowerData, (void **) &vpRspData);

    //ALOGI("%s %p", __func__, vpRspData);

    if (vpRspData) {
        if(vpRspData->pBuf) {
            vScnData.handle = ((tScnData*)(vpRspData->pBuf))->handle;
            ALOGI("%s hdl:%d", __func__, vScnData.handle);
            free(vpRspData->pBuf);
        }
        free(vpRspData);
    }

    return vScnData.handle;
}

Return<void> Power::scnConfig(int32_t hdl, MtkPowerCmd cmd, int32_t param1, int32_t param2, int32_t param3, int32_t param4) {
    ALOGD("scnConfig hdl:%d, cmd:%d, param1:%d, param2:%d, param3:%d, param4:%d", hdl, cmd, param1, param2, param3, param4);

    struct tPowerData vPowerData;
    struct tScnData vScnData;
    struct tPowerData *vpRspData = NULL;

    vScnData.handle = hdl;
    vScnData.command = (int)cmd;
    vScnData.param1 = param1;
    vScnData.param2 = param2;
    vScnData.param3 = param3;
    vScnData.param4 = param4;
    vScnData.timeout = 0;
    vPowerData.msg  = POWER_MSG_SCN_CONFIG;
    vPowerData.pBuf = (void*)&vScnData;

    //ALOGI("%s %p", __func__, &vScnData);

    power_msg(&vPowerData, (void **) &vpRspData);

    //ALOGI("%s %p", __func__, vpRspData);

    if (vpRspData) {
        if(vpRspData->pBuf)
            free(vpRspData->pBuf);
        free(vpRspData);
    }

    return Void();
}

Return<void> Power::scnUnreg(int32_t hdl) {
    ALOGI("scnUnreg hdl:%d", hdl);

    struct tPowerData vPowerData;
    struct tScnData vScnData;
    struct tPowerData *vpRspData = NULL;

    vScnData.handle = hdl;
    vScnData.command = 0;
    vScnData.param1 = 0;
    vScnData.param2 = 0;
    vScnData.param3 = 0;
    vScnData.param4 = 0;
    vScnData.timeout = 0;
    vPowerData.msg  = POWER_MSG_SCN_UNREG;
    vPowerData.pBuf = (void*)&vScnData;

    //ALOGI("%s %p", __func__, &vScnData);

    power_msg(&vPowerData, (void **) &vpRspData);

    //ALOGI("%s %p", __func__, vpRspData);

    if (vpRspData) {
        if(vpRspData->pBuf)
            free(vpRspData->pBuf);
        free(vpRspData);
    }

    return Void();
}

Return<void> Power::scnEnable(int32_t hdl, int32_t timeout) {
    ALOGI("scnEnable hdl:%d, timeout:%d", hdl, timeout);

    struct tPowerData vPowerData;
    struct tScnData vScnData;
    struct tPowerData *vpRspData = NULL;

    vScnData.handle = hdl;
    vScnData.timeout = timeout;
    vPowerData.msg  = POWER_MSG_SCN_ENABLE;
    vPowerData.pBuf = (void*)&vScnData;

    //ALOGI("%s %p", __func__, &vScnData);

    power_msg(&vPowerData, (void **) &vpRspData);

    //ALOGI("%s %p", __func__, vpRspData);

    if (vpRspData) {
        if(vpRspData->pBuf)
            free(vpRspData->pBuf);
        free(vpRspData);
    }

    return Void();
}

Return<void> Power::scnDisable(int32_t hdl) {
    ALOGI("scnDisable hdl:%d", hdl);

    struct tPowerData vPowerData;
    struct tScnData vScnData;
    struct tPowerData *vpRspData = NULL;

    vScnData.handle = hdl;
    vPowerData.msg  = POWER_MSG_SCN_DISABLE;
    vPowerData.pBuf = (void*)&vScnData;

    //ALOGI("%s %p", __func__, &vScnData);

    power_msg(&vPowerData, (void **) &vpRspData);

    //ALOGI("%s %p", __func__, vpRspData);

    if (vpRspData) {
        if(vpRspData->pBuf)
            free(vpRspData->pBuf);
        free(vpRspData);
    }

    return Void();
}

IPower* HIDL_FETCH_IPower(const char* /* name */) {
    const hw_module_t* hw_module = nullptr;
    power_module_t* power_module = nullptr;
    int err = hw_get_module(POWER_HARDWARE_MODULE_ID, &hw_module);
    if (err) {
        ALOGE("hw_get_module %s failed: %d", POWER_HARDWARE_MODULE_ID, err);
        return nullptr;
    }

    if (!hw_module->methods || !hw_module->methods->open) {
        power_module = reinterpret_cast<power_module_t*>(
            const_cast<hw_module_t*>(hw_module));
    } else {
        err = hw_module->methods->open(
            hw_module, POWER_HARDWARE_MODULE_ID,
            reinterpret_cast<hw_device_t**>(&power_module));
        if (err) {
            ALOGE("Passthrough failed to load legacy HAL.");
            return nullptr;
        }
    }
    return new Power(power_module);
}

} // namespace implementation
}  // namespace V1_1
}  // namespace power
}  // namespace hardware
}  // namespace mediatek
}  // namespace vendor

