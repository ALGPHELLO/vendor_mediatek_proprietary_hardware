#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#define LOG_TAG "PowerTest"

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

using namespace vendor::mediatek::hardware::power::V1_1;

//namespace android {

enum {
    CMD_POWER_HINT = 1,
    CMD_CUS_POWER_HINT,
    CMD_QUERY_INFO,
    CMD_SCN_REG,
    CMD_SCN_CONFIG,
    CMD_SCN_UNREG,
    CMD_SCN_ENABLE,
    CMD_SCN_DISABLE,
};

#if 0
mtkPowerHint(MtkPowerHint hint, int32_t data);
scnReg() generates (int32_t hdl);
scnConfig(int32_t hdl, MtkPowerCmd cmd, int32_t param1, int32_t param2, int32_t param3, int32_t param4);
scnUnreg(int32_t hdl);
scnEnable(int32_t hdl, int32_t timeout);
scnDisable(int32_t hdl);
#endif

static void usage(char *cmd);

int main(int argc, char* argv[])
{
    int command=0, hint=0, timeout=0, data=0;
    int cmd=0, p1=0, p2=0, p3=0, p4=0;
    int handle = -1;
    android::sp<IPower> gPowerHal;

    if(argc < 2) {
        usage(argv[0]);
        return 0;
    }

    gPowerHal = IPower::getService();
    if (gPowerHal == nullptr)
        return -1;

    command = atoi(argv[1]);
    //printf("argc:%d, command:%d\n", argc, command);
    switch(command) {
        case CMD_SCN_REG:
            if(argc!=2) {
                usage(argv[0]);
                return -1;
            }
            break;

        case CMD_SCN_UNREG:
        case CMD_SCN_DISABLE:
            if(argc!=3) {
                usage(argv[0]);
                return -1;
            }
            break;

        case CMD_POWER_HINT:
        case CMD_CUS_POWER_HINT:
        case CMD_QUERY_INFO:
        case CMD_SCN_ENABLE:
            if(argc!=4) {
                usage(argv[0]);
                return -1;
            }
            break;

        case CMD_SCN_CONFIG:
            if(argc!=8) {
                usage(argv[0]);
                return -1;
            }
            break;

        default:
            usage(argv[0]);
            return -1;
    }

    if(command == CMD_POWER_HINT || command == CMD_CUS_POWER_HINT) {
        hint = atoi(argv[2]);
        data = atoi(argv[3]);
    }
    else if(command == CMD_QUERY_INFO) {
        cmd = atoi(argv[2]);
        p1 = atoi(argv[3]);
    }
    else if(command == CMD_SCN_UNREG || command == CMD_SCN_DISABLE) {
        handle = atoi(argv[2]);
    }
    else if(command == CMD_SCN_ENABLE) {
        handle = atoi(argv[2]);
        timeout = atoi(argv[3]);
    }
    else if(command == CMD_SCN_CONFIG) {
        handle = atoi(argv[2]);
        cmd = atoi(argv[3]);
        p1 = atoi(argv[4]);
        p2 = atoi(argv[5]);
        p3 = atoi(argv[6]);
        p4 = atoi(argv[7]);
    }

    /* command */
    if(command == CMD_POWER_HINT)
        gPowerHal->mtkPowerHint((enum MtkPowerHint)hint, data);
    else if(command == CMD_CUS_POWER_HINT)
        gPowerHal->mtkCusPowerHint((enum MtkCusPowerHint)hint, data);
    else if(command == CMD_QUERY_INFO) {
        data = gPowerHal->querySysInfo((enum MtkQueryCmd)cmd, p1);
        printf("data:%d\n", data);
    }
    else if(command == CMD_SCN_REG) {
        handle = gPowerHal->scnReg();
        printf("handle:%d\n", handle);
    }
    else if(command == CMD_SCN_CONFIG)
        gPowerHal->scnConfig(handle, (enum MtkPowerCmd)cmd, p1, p2, p3, p4);
    else if(command == CMD_SCN_UNREG)
        gPowerHal->scnUnreg(handle);
    else if(command == CMD_SCN_ENABLE)
        gPowerHal->scnEnable(handle, timeout);
    else if(command == CMD_SCN_DISABLE)
        gPowerHal->scnDisable(handle);

    return 0;
}

static void usage(char *cmd) {
    fprintf(stderr, "\nUsage: %s command scenario\n"
                    "    command\n"
                    "        1: MTK power hint\n"
                    "        2: MTK cus power hint\n"
                    "        3: query info\n"
                    "        4: scn register\n"
                    "        5: scn config\n"
                    "        6: scn unregister\n"
                    "        7: scn enable\n"
                    "        8: scn disable\n", cmd);
}



//} // namespace

