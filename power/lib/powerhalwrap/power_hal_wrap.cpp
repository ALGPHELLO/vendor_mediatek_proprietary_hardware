#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#define LOG_TAG "PowerWrap"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <fcntl.h>
#include <errno.h>
#include <dlfcn.h>
#include <unistd.h>
#include <cutils/log.h>
#include <cutils/properties.h>

#include <vendor/mediatek/hardware/power/1.1/IPower.h>
#include <vendor/mediatek/hardware/power/1.1/types.h>

using android::hardware::Return;
using android::hardware::hidl_death_recipient;
using android::hidl::base::V1_0::IBase;
using namespace vendor::mediatek::hardware::power::V1_1;

#define BOOT_INFO_FILE "/sys/class/BOOT/BOOT/boot/boot_mode"
static bool gMtkPowerHalExists = true;
static android::sp<IPower> gMtkPowerHal;
static std::mutex gMtkPowerHalMutex;
static bool getMtkPowerHal();

static int check_meta_mode(void)
{
    char bootMode[4];
    int fd;
    //check if in Meta mode
    fd = open(BOOT_INFO_FILE, O_RDONLY);
    if(fd < 0) {
        return 0; // not meta mode
    }

    if(read(fd, bootMode, 4) < 1) {
        close(fd);
        return 0;
    }

    if (bootMode[0] == 0x31 || bootMode[0] == 0x34) {
        close(fd);
        return 1; // meta mode, factory mode
    }

    close(fd);
    return 0;
}

static struct PowerDeathRecipient : virtual public hidl_death_recipient
{
   // hidl_death_recipient interface
   virtual void serviceDied(uint64_t cookie, const android::wp<IBase>& who) override {
       ALOGI("Abort due to IPower hidl service failure (power hal)");
       gMtkPowerHal = nullptr;
       gMtkPowerHalMutex.lock();
       getMtkPowerHal();
       gMtkPowerHalMutex.unlock();
       /*LOG_ALWAYS_FATAL("Abort due to IPower hidl service failure,"
             " restarting powerhal");*/
   }
};

static android::sp<PowerDeathRecipient> mtkPowerHalDeathRecipient = nullptr;

static bool getMtkPowerHal() {
    if (gMtkPowerHalExists && gMtkPowerHal == nullptr) {
        gMtkPowerHal = IPower::getService();
        if (gMtkPowerHal != nullptr) {
            ALOGI("Loaded power HAL service");
            mtkPowerHalDeathRecipient = new PowerDeathRecipient();
            android::hardware::Return<bool> linked = gMtkPowerHal->linkToDeath(
            mtkPowerHalDeathRecipient, /*cookie*/ 0);
            if (!linked.isOk()) {
                ALOGE("Transaction error in linking to PowerHAL death: %s",
                linked.description().c_str());
            } else if (!linked) {
               ALOGI("Unable to link to PowerHAL death notifications");
            } else {
               ALOGI("Link to death notification successful");
            }
        } else {
            ALOGI("Couldn't load power HAL service");
            gMtkPowerHalExists = false;
        }
    }
    return gMtkPowerHal != nullptr;
}

static void processReturn(const Return<void> &ret, const char* functionName) {
    if(!ret.isOk()) {
        ALOGE("%s() failed: Power HAL service not available", functionName);
        gMtkPowerHal =nullptr;
    }
}

extern "C"
int PowerHal_Wrap_mtkPowerHint(uint32_t hint, int32_t data)
{
    std::lock_guard<std::mutex> lock(gMtkPowerHalMutex);
    if (getMtkPowerHal()) {
        ALOGI("%s",__FUNCTION__);
        Return<void> ret = gMtkPowerHal->mtkPowerHint((enum MtkPowerHint)hint, data);
        processReturn(ret, __FUNCTION__);
    }

    return 0;
}

extern "C"
int PowerHal_Wrap_mtkCusPowerHint(uint32_t hint, int32_t data)
{
    std::lock_guard<std::mutex> lock(gMtkPowerHalMutex);
    if (getMtkPowerHal()) {
        ALOGI("%s",__FUNCTION__);
        Return<void> ret = gMtkPowerHal->mtkCusPowerHint((enum MtkCusPowerHint)hint, data);
        processReturn(ret, __FUNCTION__);
    }
    return 0;
}

extern "C"
int PowerHal_Wrap_querySysInfo(uint32_t cmd, int32_t param)
{
    int data = 0;

    std::lock_guard<std::mutex> lock(gMtkPowerHalMutex);
    if (getMtkPowerHal()) {
        ALOGI("%s",__FUNCTION__);
        data = gMtkPowerHal->querySysInfo((enum MtkQueryCmd)cmd, param);
    }

    return data;
}

extern "C"
int PowerHal_Wrap_scnReg(void)
{
    int handle = -1;

    std::lock_guard<std::mutex> lock(gMtkPowerHalMutex);
    if (getMtkPowerHal()) {
        ALOGI("%s",__FUNCTION__);
        handle = gMtkPowerHal->scnReg();
    }

    return handle;
}

extern "C"
int PowerHal_Wrap_scnConfig(int32_t hdl, int32_t cmd, int32_t param1, int32_t param2, int32_t param3, int32_t param4)
{

    std::lock_guard<std::mutex> lock(gMtkPowerHalMutex);
    if (getMtkPowerHal()) {
        //ALOGI("%s",__FUNCTION__);
        Return<void> ret = gMtkPowerHal->scnConfig(hdl, (enum MtkPowerCmd)cmd, param1, param2, param3, param4);
        processReturn(ret, __FUNCTION__);
    }

    return 0;
}

extern "C"
int PowerHal_Wrap_scnUnreg(int32_t hdl)
{
    std::lock_guard<std::mutex> lock(gMtkPowerHalMutex);
    if (getMtkPowerHal()) {
        ALOGI("%s",__FUNCTION__);
        Return<void> ret = gMtkPowerHal->scnUnreg(hdl);
        processReturn(ret, __FUNCTION__);
    }

    return 0;
}

extern "C"
int PowerHal_Wrap_scnEnable(int32_t hdl, int32_t timeout)
{
    std::lock_guard<std::mutex> lock(gMtkPowerHalMutex);
    if (getMtkPowerHal()) {
        ALOGI("%s",__FUNCTION__);
        Return<void> ret = gMtkPowerHal->scnEnable(hdl, timeout);
        processReturn(ret, __FUNCTION__);
    }

    return 0;
}

extern "C"
int PowerHal_Wrap_scnDisable(int32_t hdl)
{
    std::lock_guard<std::mutex> lock(gMtkPowerHalMutex);
    if (getMtkPowerHal()) {
        ALOGI("%s",__FUNCTION__);
        Return<void> ret = gMtkPowerHal->scnDisable(hdl);
        processReturn(ret, __FUNCTION__);
    }

    return 0;
}

extern "C"
int PowerHal_TouchBoost(int32_t timeout)
{
    std::lock_guard<std::mutex> lock(gMtkPowerHalMutex);
    if (getMtkPowerHal()) {
        //ALOGI("%s",__FUNCTION__);
        Return<void> ret = gMtkPowerHal->mtkPowerHint(MtkPowerHint::MTK_POWER_HINT_APP_TOUCH, timeout);
        processReturn(ret, __FUNCTION__);
    }

    return 0;
}

//} // namespace

