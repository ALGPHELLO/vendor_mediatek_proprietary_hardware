/* //device/system/rild/rild.c
**
** Copyright 2006 The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <telephony/mtk_ril.h>
#define LOG_TAG "RILD"
#include <log/log.h>
#include <cutils/properties.h>
#include <cutils/sockets.h>
#include <sys/capability.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <librilmtk/ril_ex.h>
#include "sysenv_utils.h"
#include <private/android_filesystem_config.h>

#define LIB_PATH_PROPERTY   "rild.libpath"
#define LIB_ARGS_PROPERTY   "rild.libargs"
#define MAX_LIB_ARGS        16

static void usage(const char *argv0) {
    fprintf(stderr, "Usage: %s -l <ril impl library> [-- <args for impl library>]\n", argv0);
    exit(EXIT_FAILURE);
}

extern char ril_service_name_base[MAX_SERVICE_NAME_LENGTH];
extern char ril_service_name[MAX_SERVICE_NAME_LENGTH];

extern void RIL_register (const RIL_RadioFunctions *callbacks);
extern void rilc_thread_pool ();

extern void RIL_register_socket (RIL_RadioFunctions *(*rilUimInit)
        (const struct RIL_Env *, int, char **), RIL_SOCKET_TYPE socketType, int argc, char **argv);

extern void RIL_onRequestComplete(RIL_Token t, RIL_Errno e,
        void *response, size_t responselen);

extern void RIL_onRequestAck(RIL_Token t);

extern void RIL_setServiceName(char *);

#if defined(ANDROID_MULTI_SIM)
extern void RIL_onUnsolicitedResponse(int unsolResponse, const void *data,
        size_t datalen, RIL_SOCKET_ID socket_id);
#else
extern void RIL_onUnsolicitedResponse(int unsolResponse, const void *data,
        size_t datalen);
#endif

extern void RIL_requestTimedCallback (RIL_TimedCallback callback,
        void *param, const struct timeval *relativeTime);


static struct RIL_Env s_rilEnv = {
    RIL_onRequestComplete,
    RIL_onUnsolicitedResponse,
    RIL_requestTimedCallback,
    RIL_onRequestAck,
    NULL,
    NULL,
    NULL
};

extern void RIL_startEventLoop();

static int isReseted = 0;

int isUserLoad() {
    int isUserLoad = 0;
    char property_value_emulation[PROPERTY_VALUE_MAX] = { 0 };
    char property_value[PROPERTY_VALUE_MAX] = { 0 };
    property_get("ril.emulation.userload", property_value_emulation, "0");
    if(strcmp("1", property_value_emulation) == 0) {
        return 1;
    }
    property_get("ro.build.type", property_value, "");
    isUserLoad = (strcmp("user", property_value) == 0);
    //RLOGD("isUserLoad: %d", isUserLoad);
    return isUserLoad;
}

int isInternalLoad() {
    #ifdef __PRODUCTION_RELEASE__
        return 0;
    #else
        char property_value[PROPERTY_VALUE_MAX] = { 0 };
        property_get("ril.emulation.production", property_value, "0");
        return (strcmp("1", property_value) != 0);
    #endif /* __PRODUCTION_RELEASE__ */
}

/*
* Purpose:  Function responsible by all signal handlers treatment any new signal must be added here
* Input:      param - signal ID
* Return:    -
*/
void signal_treatment(int param)
{
    RLOGD("signal_no=%d", param);
    switch (param) {
        case SIGUSR1:
            RLOGD("SIGUSR1");
            break;
        case SIGUSR2:
            RLOGD("SIGUSR2");
            break;
        case SIGSEGV:
        case SIGPIPE:
            if (!isReseted) {
                RLOGD("SIGSEGV or SIGPIPE");
                property_set("ril.mux.report.case","2");
                property_set("ril.muxreport", "1");
                isReseted = 1;
            } else {
                RLOGD("already reset");
            }
            break;
        default:
            exit(0);
            break;
    }
}

void mtkInit() {
    //signals treatment
    signal(SIGPIPE, signal_treatment);
    signal(SIGUSR1, signal_treatment);
    signal(SIGUSR2, signal_treatment);

    if ((isInternalLoad() == 0) && (isUserLoad() == 1)) {
        RLOGD("Setup SIGSEGV signal handling");
        signal(SIGSEGV, signal_treatment);
    }
}

static int make_argv(char * args, char ** argv) {
    // Note: reserve argv[0]
    int count = 1;
    char * tok;
    char * s = args;

    while ((tok = strtok(s, " \0"))) {
        argv[count] = tok;
        s = NULL;
        count++;
    }
    return count;
}

void setDynamicMsimConfig() {
    char prop_value[PROPERTY_VALUE_MAX] = { 0 };
    // update dynamic multisim config
    const char *pSysenv = sysenv_get_static("msim_config");
    RLOGD("sysenv_get_static: %s\n", pSysenv);
    if (pSysenv != NULL) {
        // for META tool
        if (strcmp(pSysenv, "dsds") == 0) {
            property_set("persist.radio.multisim.config", "dsds");
            property_set("persist.radio.multisimslot", "2");
        } else if (strcmp(pSysenv, "ss") == 0) {
            property_set("persist.radio.multisim.config", "ss");
            property_set("persist.radio.multisimslot", "1");
        }
    } else {
        // for EM
        property_get("persist.radio.multisim.config", prop_value, "dsds");
        if (strcmp(prop_value, "dsda") != 0) {
            property_get("persist.radio.multisimslot", prop_value, "0");
            if (prop_value[0] == '1') {
                property_set("persist.radio.multisim.config", "ss");
            } else if (prop_value[0] == '2') {
                property_set("persist.radio.multisim.config", "dsds");
            } else if (prop_value[0] == '3') {
                property_set("persist.radio.multisim.config", "tsts");
            } else if (prop_value[0] == '4') {
                property_set("persist.radio.multisim.config", "qsqs");
            }
        }
    }
}

int main(int argc, char **argv) {
    // vendor ril lib path either passed in as -l parameter, or read from rild.libpath property
    const char *rilLibPath = NULL;
    // ril arguments either passed in as -- parameter, or read from rild.libargs property
    char **rilArgv;
    // handle for vendor ril lib
    void *dlHandle;
    // Pointer to ril init function in vendor ril
    const RIL_RadioFunctions *(*rilInit)(const struct RIL_Env *, int, char **);
    // Pointer to sap init function in vendor ril
    RIL_RadioFunctions *(*rilUimInit)(const struct RIL_Env *, int, char **);
    const char *err_str = NULL;

    // functions returned by ril init function in vendor ril
    const RIL_RadioFunctions *funcs;
    // lib path from rild.libpath property (if it's read)
    char libPath[PROPERTY_VALUE_MAX];
    // flat to indicate if -- parameters are present
    unsigned char hasLibArgs = 0;

    int i;
    // ril/socket id received as -c parameter, otherwise set to 0
    const char *clientId = NULL;

    RLOGD("**RIL Proxy Started**");
    RLOGD("**RILd param count=%d**", argc);

    setDynamicMsimConfig();
    mtkInit();

    umask(S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH);
    for (i = 1; i < argc ;) {
        if (0 == strcmp(argv[i], "-l") && (argc - i > 1)) {
            rilLibPath = argv[i + 1];
            i += 2;
        } else if (0 == strcmp(argv[i], "--")) {
            i++;
            hasLibArgs = 1;
            break;
        } else if (0 == strcmp(argv[i], "-c") &&  (argc - i > 1)) {
            clientId = argv[i+1];
            i += 2;
        } else {
            usage(argv[0]);
        }
    }
    if (clientId == NULL) {
        clientId = "0";
    } else if (atoi(clientId) >= MAX_RILDS) {
        RLOGE("Max Number of rild's supported is: %d", MAX_RILDS);
        exit(0);
    }
    if (strncmp(clientId, "0", MAX_CLIENT_ID_LENGTH)) {
        strncpy(ril_service_name, ril_service_name_base, MAX_SERVICE_NAME_LENGTH);
        strncat(ril_service_name, clientId, MAX_SERVICE_NAME_LENGTH);
        RIL_setServiceName(ril_service_name);
    }

    if (rilLibPath == NULL) {
        if ( 0 == property_get(LIB_PATH_PROPERTY, libPath, NULL)) {
            // No lib sepcified on the command line, and nothing set in props.
            // Assume "no-ril" case.
            goto done;
        } else {
            rilLibPath = libPath;
        }
    }
    RLOGD("rilLibPath:%s", rilLibPath);
    /* special override when in the emulator */

    dlHandle = dlopen(rilLibPath, RTLD_NOW);

    if (dlHandle == NULL) {
        RLOGE("dlopen failed: %s", dlerror());
        exit(EXIT_FAILURE);
    }

    RIL_startEventLoop();

    rilInit =
        (const RIL_RadioFunctions *(*)(const struct RIL_Env *, int, char **))
        dlsym(dlHandle, "RIL_Init");

    if (rilInit == NULL) {
        RLOGE("RIL_Init not defined or exported in %s\n", rilLibPath);
        exit(EXIT_FAILURE);
    }

    dlerror(); // Clear any previous dlerror
    rilUimInit =
        (RIL_RadioFunctions *(*)(const struct RIL_Env *, int, char **))
        dlsym(dlHandle, "RIL_SAP_Init");
    err_str = dlerror();
    if (err_str) {
        RLOGW("RIL_SAP_Init not defined or exported in %s: %s\n", rilLibPath, err_str);
    } else if (!rilUimInit) {
        RLOGW("RIL_SAP_Init defined as null in %s. SAP Not usable\n", rilLibPath);
    }

    if (hasLibArgs) {
        rilArgv = argv + i - 1;
        argc = argc -i + 1;
    } else {
        static char * newArgv[MAX_LIB_ARGS];
        static char args[PROPERTY_VALUE_MAX];
        rilArgv = newArgv;
        property_get(LIB_ARGS_PROPERTY, args, "");
        argc = make_argv(args, rilArgv);
    }

    rilArgv[argc++] = "-c";
    rilArgv[argc++] = (char*)clientId;
    RLOGD("RIL_Init argc = %d clientId = %s", argc, rilArgv[argc-1]);

    // Make sure there's a reasonable argv[0]
    rilArgv[0] = argv[0];

    funcs = rilInit(&s_rilEnv, argc, rilArgv);
    RLOGD("RIL_Init rilInit completed");

    RIL_register(funcs);

    RLOGD("RIL_Init RIL_register completed");

    if (rilUimInit) {
        RLOGD("RIL_register_socket started");
        RIL_register_socket(rilUimInit, RIL_SAP_SOCKET, argc, rilArgv);
    }

    RLOGD("RIL_register_socket completed");

done:

    rilc_thread_pool();

    RLOGD("RIL_Init starting sleep loop");
    while (true) {
        sleep(UINT32_MAX);
    }
}
