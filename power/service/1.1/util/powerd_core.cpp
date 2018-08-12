//#ifdef __cplusplus
//extern "C" {
//#endif

/*** STANDARD INCLUDES *******************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dlfcn.h>

/*** PROJECT INCLUDES ********************************************************/
#define PERFD

#include "ports.h"
#include "mi_types.h"
#include "mi_util.h"
#include "ptimer.h"

#include "power_util.h"
#include "powerd_int.h"
#include "powerd_core.h"

#include <vendor/mediatek/hardware/power/1.1/IPower.h>
#include <vendor/mediatek/hardware/power/1.1/types.h>

/* customization config */
#include "power_cus.h"

using namespace vendor::mediatek::hardware::power::V1_1;

#define LIB_FULL_NAME "libperfservice.so"

typedef int (*perf_boost_enable)(int);
typedef int (*perf_boost_disable)(int);
typedef int (*perf_notify_state)(const char*, const char*, int, int);
typedef int (*perf_cus_user_scn_reg)(void);
typedef int (*perf_user_scn_reg)(int, int);
typedef int (*perf_user_scn_config)(int, int, int, int, int, int);
typedef int (*perf_user_scn_unreg)(int);
typedef int (*perf_user_enable)(int);
typedef int (*perf_user_disable)(int);
typedef int (*perf_user_scn_disable_all)(void);
typedef int (*perf_user_scn_restore_all)(void);
typedef int (*perf_user_get_capability)(int, int);
typedef int (*perf_get_pack_attr)(const char*, int);
typedef int (*perf_pre_defined_disable)(int);

/* function pointer to perfserv client */
static int (*perfBoostEnable)(int) = NULL;
static int (*perfBoostDisable)(int) = NULL;
static int (*perfNotifyAppState)(const char*, const char*, int, int) = NULL;
static int (*perfCusUserRegScn)(void) = NULL;
static int (*perfUserRegScn)(int, int) = NULL;
static int (*perfUserRegScnConfig)(int, int, int, int, int, int) = NULL;
static int (*perfUserUnregScn)(int) = NULL;
static int (*perfUserScnEnable)(int) = NULL;
static int (*perfUserScnDisable)(int) = NULL;
static int (*perfUserScnDisableAll)(void) = NULL;
static int (*perfUserScnRestoreAll)(void) = NULL;
static int (*perfUserGetCapability)(int, int) = NULL;
static int (*perfGetPackAttr)(const char*, int) = NULL;
static int (*perfPreDefinedScnDisable)(int) = NULL;

static void * _gpTimerMng;
static int pboost_timeout = 0;

#define MAX_WALT_COUNT          3
#define MAX_TIMER_COUNT       256
#define MAX_CUS_HINT_COUNT    128
#define GAME_LAUNCH_DURATION  10000
#define LIBP_CMD_GET_BOOST_TIMEOUT      10
#define LIBP_CMD_GET_WALT_FOLLOW_1      12
#define LIBP_CMD_GET_WALT_DURATION_1    13
#define LIBP_CMD_GET_WALT_FOLLOW_2      14
#define LIBP_CMD_GET_WALT_DURATION_2    15
#define LIBP_CMD_GET_WALT_FOLLOW_3      16
#define LIBP_CMD_GET_WALT_DURATION_3    17
#define LIBP_CMD_GET_PACK_BOOST_TIMEOUT  2

enum {
    TIMER_MSG_POWER_HINT_ENABLE_TIMEOUT = 0,
    TIMER_MSG_USER_SCN_ENABLE_TIMEOUT = 1,
    TIMER_MSG_WHITE_LIST_TIMEOUT = 2,
};

struct tTimer {
    int used;
    int idx;
    int msg;
    int handle;
    void *p_pTimer;
};

struct waltScn {
    int scn;
    int dur;
    int ud;
};

static int wl_hdl = 0;
static int walt_cnt = 0;
static int nPerfSupport = 0;
struct tTimer powerdTimer[MAX_TIMER_COUNT]; // temp
struct waltScn tWaltScn[MAX_WALT_COUNT]; // temp

static int gtCusHintTbl[MAX_CUS_HINT_COUNT];

static int nPwrDebugLogEnable = 0;

#ifdef ALOGD
#undef ALOGD
#define ALOGD(...) do{if(nPwrDebugLogEnable)((void)ALOG(LOG_INFO, LOG_TAG, __VA_ARGS__));}while(0)
#endif

/*****************
   Function
 *****************/
static int load_api(void)
{
    void *handle, *func;

    handle = dlopen(LIB_FULL_NAME, RTLD_NOW);

    func = dlsym(handle, "perfBoostEnable");
    perfBoostEnable = (perf_boost_enable)(func);

    if (perfBoostEnable == NULL) {
        printf("perfBoostEnable error: %s", dlerror());
        dlclose(handle);
        return -1;
    }

    func = dlsym(handle, "perfBoostDisable");
    perfBoostDisable = (perf_boost_disable)(func);

    if (perfBoostDisable == NULL) {
        printf("perfBoostDisable error: %s", dlerror());
        dlclose(handle);
        return -1;
    }

    func = dlsym(handle, "perfNotifyAppState");
    perfNotifyAppState = (perf_notify_state)(func);

    if (perfNotifyAppState == NULL) {
        printf("perfNotifyAppState error: %s", dlerror());
        dlclose(handle);
        return -1;
    }

    func = dlsym(handle, "perfUserGetCapability");
    perfUserGetCapability = (perf_user_get_capability)(func);

    if (perfUserGetCapability == NULL) {
        printf("perfUserGetCapability error: %s", dlerror());
        dlclose(handle);
        return -1;
    }

    func = dlsym(handle, "perfGetPackAttr");
    perfGetPackAttr = (perf_get_pack_attr)(func);

    if (perfGetPackAttr == NULL) {
        printf("perfGetPackAttr error: %s", dlerror());
        dlclose(handle);
        return -1;
    }

    func = dlsym(handle, "perfCusUserRegScn");
    perfCusUserRegScn = (perf_cus_user_scn_reg)(func);

    if (perfCusUserRegScn == NULL) {
        printf("perfCusUserRegScn error: %s", dlerror());
        dlclose(handle);
        return -1;
    }


    func = dlsym(handle, "perfUserRegScn");
    perfUserRegScn = (perf_user_scn_reg)(func);

    if (perfUserRegScn == NULL) {
        printf("perfUserRegScn error: %s", dlerror());
        dlclose(handle);
        return -1;
    }

    func = dlsym(handle, "perfUserRegScnConfig");
    perfUserRegScnConfig = (perf_user_scn_config)(func);

    if (perfUserRegScnConfig == NULL) {
        printf("perfUserRegScnConfig error: %s", dlerror());
        dlclose(handle);
        return -1;
    }

    func = dlsym(handle, "perfUserUnregScn");
    perfUserUnregScn = (perf_user_scn_unreg)(func);

    if (perfUserUnregScn == NULL) {
        printf("perfUserUnregScn error: %s", dlerror());
        dlclose(handle);
        return -1;
    }

    func = dlsym(handle, "perfUserScnEnable");
    perfUserScnEnable = (perf_user_enable)(func);

    if (perfUserScnEnable == NULL) {
        printf("perfUserScnEnable error: %s", dlerror());
        dlclose(handle);
        return -1;
    }

    func = dlsym(handle, "perfUserScnDisable");
    perfUserScnDisable = (perf_user_disable)(func);

    if (perfUserScnDisable == NULL) {
        printf("perfUserScnDisable error: %s", dlerror());
        dlclose(handle);
        return -1;
    }

    func = dlsym(handle, "perfUserScnDisableAll");
    perfUserScnDisableAll = (perf_user_scn_disable_all)(func);

    if (perfUserScnDisableAll == NULL) {
        printf("perfUserScnDisableAll error: %s", dlerror());
        dlclose(handle);
        return -1;
    }

    func = dlsym(handle, "perfPreDefinedScnDisable");
    perfPreDefinedScnDisable = (perf_pre_defined_disable)(func);

    if (perfPreDefinedScnDisable == NULL) {
        printf("perfPreDefinedScnDisable error: %s", dlerror());
        dlclose(handle);
        return -1;
    }

    func = dlsym(handle, "perfUserScnRestoreAll");
    perfUserScnRestoreAll = (perf_user_scn_restore_all)(func);

    if (perfUserScnRestoreAll == NULL) {
        printf("perfUserScnRestoreAll error: %s", dlerror());
        dlclose(handle);
        return -1;
    }

    return 0;
}

int reset_timer(int i)
{
    powerdTimer[i].used = 0;
    powerdTimer[i].msg = -1;
    powerdTimer[i].handle = -1;
    powerdTimer[i].p_pTimer = NULL;
    return 0;
}

int allocate_timer(void)
{
    int i;
    for(i=0; i<MAX_TIMER_COUNT; i++) {
        if(powerdTimer[i].used == 0) {
            powerdTimer[i].used = 1;
            return i;
        }
    }
    return -1;
}

int find_timer(int handle)
{
    int i;
    for(i=0; i<MAX_TIMER_COUNT; i++) {
        if(powerdTimer[i].used && powerdTimer[i].handle == handle)
            return i;
    }
    return -1;
}

int remove_scn_timer(int handle)
{
    int idx;

    if((idx = find_timer(handle)) >= 0) {
        ptimer_stop(powerdTimer[idx].p_pTimer);
        ptimer_delete(powerdTimer[idx].p_pTimer);
        reset_timer(idx);
    }
    return 0;
}

int start_scn_timer(int msg, int handle, int timeout)
{
    int idx;
    if((idx = allocate_timer()) >= 0) {
        //ALOGI("[start_scn_timer] idx:%d, handle:%d, timeout:%d", idx, handle, timeout);
        powerdTimer[idx].msg = msg;
        powerdTimer[idx].handle = handle;
        ptimer_create(&(powerdTimer[idx].p_pTimer));
        ptimer_start(_gpTimerMng, powerdTimer[idx].p_pTimer, timeout, &(powerdTimer[idx]));
    }
    return 0;
}

int cus_hint_register(struct tCusConfig *mCusHintConfig)
{
    int i, hint = -1, hdl = 0, tmp_hint = 0;
    int cusHintTblSize = 0;

    for(i=0; mCusHintConfig[i].hint != -1; i++) {
        tmp_hint = mCusHintConfig[i].hint;
        ALOGD("[powerd_core_pre_init] i:%d, tmp_hint:%d", i, tmp_hint);

            if (tmp_hint != hint) {
                cusHintTblSize++;
                hint = tmp_hint;
                /* cus hint */
                if(MAX_CUS_HINT_COUNT <= cusHintTblSize) {
                    ALOGE("[powerd_core_pre_init] ERROR!!! gtCusHintTbl size is not enough");
                    return 0;
                }
                gtCusHintTbl[hint] = perfCusUserRegScn();
                ALOGI("[powerd_core_pre_init] gtCusHintTbl[%d]:%d", hint, gtCusHintTbl[hint]);
            }

            if (hint != -1) {
                hdl = gtCusHintTbl[hint];
                perfUserRegScnConfig(hdl, mCusHintConfig[i].command, mCusHintConfig[i].param1,
                           mCusHintConfig[i].param2, mCusHintConfig[i].param3, mCusHintConfig[i].param4);
            }
    }

    return cusHintTblSize;
}

int powerd_core_pre_init(void)
{
    int i = 0, j = 0;
    int cusHintTblSize = 0;

    if(load_api() == 0)
        nPerfSupport = 1;
    else
        return 0; // libperfservice is not supported

    if (cusHintConfig[0].hint != -1) {
        cus_hint_register(cusHintConfig);
    } else {
        cus_hint_register(cusHintConfigImpl);
    }

    for(i=0; i < MAX_WALT_COUNT; i++) {
           tWaltScn[i].scn = 0;
           tWaltScn[i].dur = 0;
           tWaltScn[i].ud = 0;
    }

    j = 0;
    for(i= LIBP_CMD_GET_WALT_FOLLOW_1; i <= LIBP_CMD_GET_WALT_FOLLOW_3; (i+=2)) {
       if (j < MAX_WALT_COUNT) {
           tWaltScn[j].scn = perfUserGetCapability(i, 0);
           tWaltScn[j].dur = perfUserGetCapability((i+1), 0);
           tWaltScn[j].ud = (tWaltScn[j].scn > 0) ? ((tWaltScn[j].dur > 0) ? 1 : 0): 0;
           ALOGI("[powerd_core_pre_init] i:%d, j:%d, walt scn:%d, t:%d, ud:%d",
                                i, j, tWaltScn[j].scn, tWaltScn[j].dur, tWaltScn[j].ud);
           if (tWaltScn[j].ud)
              j++;
       }
    }
    walt_cnt = j;
    pboost_timeout = perfUserGetCapability(LIBP_CMD_GET_BOOST_TIMEOUT, 0);
    ALOGI("[powerd_core_pre_init] pboost_timeout: %d", pboost_timeout);

    return 0;
}

int powerd_core_init(void * pTimerMng)
{
    int i, hdl;

    _gpTimerMng = pTimerMng;

#if 0
    if(load_api() == 0)
        nPerfSupport = 1;
    else
        return 0; // libperfservice is not supported
#endif

    for(i=0; i<MAX_TIMER_COUNT; i++) {
        powerdTimer[i].used = 0;
        powerdTimer[i].idx = i;
        powerdTimer[i].msg = -1;
        powerdTimer[i].handle = -1;
        powerdTimer[i].p_pTimer = NULL;
    }

#if 0
    /* cus hint */
    if(MAX_CUS_HINT_COUNT <= (int)MtkCusPowerHint::MTK_CUS_HINT_NUM) {
        ALOGE("[powerd_core_init] ERROR!!! gtCusHintTbl size is not enough");
        return 0;
    }

    for(i=0; i<(int)MtkCusPowerHint::MTK_CUS_HINT_NUM; i++) {
        gtCusHintTbl[i] = perfUserRegScn();
        ALOGI("[powerd_core_init] gtCusHintTbl[%d]:%d", i, gtCusHintTbl[i]);
    }

    for(i=0; cusHintConfig[i].hint != -1; i++) {
        hdl = gtCusHintTbl[cusHintConfig[i].hint];
        perfUserRegScnConfig(hdl, cusHintConfig[i].command, cusHintConfig[i].param1, cusHintConfig[i].param2, cusHintConfig[i].param3, cusHintConfig[i].param4);
    }
    //powerd_cus_init(gtCusHintTbl, perfUserRegScnConfig);
#endif

    return 0;
}

int powerd_core_timer_handle(void * pTimer, void * pData)
{
    int i = 0;
    struct tTimer *ptTimer = (struct tTimer *)pData;

    if(ptTimer->p_pTimer != pTimer)
        return -1;

    switch(ptTimer->msg) {
    case TIMER_MSG_POWER_HINT_ENABLE_TIMEOUT:
         if ((walt_cnt >= 1) && (tWaltScn[0].scn != 0)) {
             for (i = 0; i < walt_cnt; i++) {
                 if (ptTimer->handle == tWaltScn[i].scn) {
                     ALOGI("[TIMER] POWER_MSG_MTK_HINT_WALT ENABLE EXPIRE");
                     start_scn_timer(TIMER_MSG_POWER_HINT_ENABLE_TIMEOUT,
                          (int)MtkPowerHint::MTK_POWER_HINT_EXT_LAUNCH, tWaltScn[i].dur);
                     perfBoostEnable((int)MtkPowerHint::MTK_POWER_HINT_EXT_LAUNCH);
                 }
             }
         }

        perfBoostDisable(ptTimer->handle);
        ptimer_delete(ptTimer->p_pTimer);
        reset_timer(ptTimer->idx);
        break;

    case TIMER_MSG_USER_SCN_ENABLE_TIMEOUT:
        perfUserScnDisable(ptTimer->handle);
        ptimer_delete(ptTimer->p_pTimer);
        reset_timer(ptTimer->idx);
        break;

    case TIMER_MSG_WHITE_LIST_TIMEOUT:
        perfPreDefinedScnDisable(ptTimer->handle);
        ptimer_delete(ptTimer->p_pTimer);
        reset_timer(ptTimer->idx);
        break;
    }

    return 0;
}

//extern "C"
int powerd_req(void * pMsg, void ** pRspMsg)
{
    struct tScnData    *vpScnData = NULL, *vpRspScn = NULL;
    struct tHintData   *vpHintData = NULL;
    struct tAppStateData *vpAppState = NULL;
    struct tQueryInfoData  *vpQueryData = NULL, *vpRspQuery = NULL;
    struct tPowerData * vpData = (struct tPowerData *) pMsg;
    struct tPowerData * vpRsp = NULL;
    int hdl, hint = 0, value = 0, wl_time = 0;
    //struct tHintData        *vpRspHint;
    //struct tAppStateDate    *vpRspAppState;

    //TWPCDBGP("%s %d\n", __func__, vpData->msg);
    //TWPCDBGP("%s %p\n", __func__, vpData);
    //TWPCDBGP("%s %p\n", __func__, vpRsp);

    if(!nPerfSupport) {
        if(vpData->msg != POWER_MSG_MTK_HINT) // log reduction
            ALOGI("libperfservice not supported\n");
        return -1;
    }

    if((vpRsp = (struct tPowerData *) malloc(sizeof(struct tPowerData))) == NULL) {
        ALOGI("%s malloc failed\n", __func__);
        return -1;
    }

    if(vpRsp) {
        vpRsp->msg = vpData->msg;
        vpRsp->pBuf = NULL;
    }

    if(vpData) {
        switch(vpData->msg) {
        case POWER_MSG_MTK_HINT:
            if(vpData->pBuf) {
                vpHintData = (struct tHintData*)(vpData->pBuf);
                #if 0
                if(vpHintData->hint != 1)
                    ALOGI("[powerd_req] POWER_MSG_MTK_HINT: hint:%d, data:%d", vpHintData->hint, vpHintData->data);
                #endif
                if(vpHintData->data) {
                    /* extend launch hint end */
                    if ((walt_cnt >= 1) && (tWaltScn[0].scn != 0)) {
                        if (vpHintData->hint == (int)MtkPowerHint::MTK_POWER_HINT_LAUNCH) {
                            ALOGD("[powerd_req] POWER_MSG_MTK_HINT_WALT DISABLE");
                            remove_scn_timer((int)MtkPowerHint::MTK_POWER_HINT_EXT_LAUNCH);
                            perfBoostDisable((int)MtkPowerHint::MTK_POWER_HINT_EXT_LAUNCH);
                        }
                    }
                    /* ---------------------- */

                    /* Only MTK power hint support timeout */
                    if(vpHintData->hint >= (int)(MtkPowerHint::MTK_POWER_HINT_BASE)) {
                        remove_scn_timer(vpHintData->hint);
                        if(vpHintData->data != (int)(MtkHintOp::MTK_HINT_ALWAYS_ENABLE)) // not MTK_HINT_ALWAYS_ENABLE
                            start_scn_timer(TIMER_MSG_POWER_HINT_ENABLE_TIMEOUT, vpHintData->hint, vpHintData->data);
                    }

                    perfBoostEnable(vpHintData->hint);
                    /* ----------------------------------- */

                } else {
                    /* Only MTK power hint support timeout */
                    if(vpHintData->hint >= (int)(MtkPowerHint::MTK_POWER_HINT_BASE))
                        remove_scn_timer(vpHintData->hint);

                    perfBoostDisable(vpHintData->hint);
                    /* ----------------------------------- */
                }
            }
            break;

        case POWER_MSG_MTK_CUS_HINT:
            if(vpData->pBuf) {
                vpHintData = (struct tHintData*)(vpData->pBuf);
                hdl = gtCusHintTbl[vpHintData->hint];
                ALOGI("[powerd_req] POWER_MSG_MTK_CUS_HINT: hint:%d, hdl:%d, data:%d", vpHintData->hint, hdl, vpHintData->data);
                if(vpHintData->data) {
                    remove_scn_timer(hdl);
                    if(vpHintData->data != (int)(MtkHintOp::MTK_HINT_ALWAYS_ENABLE)) // not MTK_HINT_ALWAYS_ENABLE
                    start_scn_timer(TIMER_MSG_USER_SCN_ENABLE_TIMEOUT, hdl, vpHintData->data);
                    perfUserScnEnable(hdl);
                }
                else {
                    remove_scn_timer(hdl);
                    perfUserScnDisable(hdl);
                }
            }
            break;

        case POWER_MSG_NOTIFY_STATE:
            ALOGD("[powerd_req] POWER_MSG_NOTIFY_STATE");
            if(vpData->pBuf) {
                hint = (int)(MtkPowerHint::MTK_POWER_HINT_GAME_LAUNCH);
/*
                if (pboost_timeout && wl_hdl > 0) {
                    ALOGD("[powerd_req] POWER_MSG_NOTIFY_STATE rm wl_hdl:%d", wl_hdl);
                    remove_scn_timer(wl_hdl);
                }
*/
                vpAppState = (struct tAppStateData*)(vpData->pBuf);
                /*--especially for WL that have set timeout --*/
                wl_hdl = perfNotifyAppState(vpAppState->pack, vpAppState->activity, vpAppState->state, vpAppState->pid);
/*                if (pboost_timeout && wl_hdl > 0) {
                    wl_time = perfGetPackAttr(vpAppState->pack, LIBP_CMD_GET_PACK_BOOST_TIMEOUT);
                    ALOGD("[powerd_req] POWER_MSG_NOTIFY_STATE start wl_hdl:%d, wl_time:%d", wl_hdl, wl_time);
                    start_scn_timer(TIMER_MSG_WHITE_LIST_TIMEOUT, wl_hdl, wl_time);
                }
*/
                remove_scn_timer(hint);
                perfBoostDisable(hint);

                value = perfUserGetCapability((int)(MtkQueryCmd::CMD_GET_FOREGROUND_TYPE), 0);
                if (value == 1) {
                    start_scn_timer(TIMER_MSG_POWER_HINT_ENABLE_TIMEOUT, hint, GAME_LAUNCH_DURATION);
                    perfBoostEnable(hint);
                }
            }
            break;

        case POWER_MSG_QUERY_INFO:
            ALOGD("[powerd_req] POWER_MSG_QUERY_INFO");
            if((vpRspQuery = (struct tQueryInfoData*)malloc(sizeof(struct tQueryInfoData))) == NULL)
                break;

            if(vpData->pBuf) {
                vpQueryData = (struct tQueryInfoData*)(vpData->pBuf);
                ALOGD("[powerd_req] POWER_MSG_QUERY_INFO: cmd:%d, param:%d", vpQueryData->cmd, vpQueryData->param);

                vpRspQuery->value = perfUserGetCapability(vpQueryData->cmd, vpQueryData->param);
                vpRsp->pBuf = (void*)vpRspQuery;

                /* debug msg */
                if(vpQueryData->cmd == CMD_SET_DEBUG_MSG) {
                    if(vpQueryData->param == 0) {
                        nPwrDebugLogEnable = 0;
                    }
                    else if(vpQueryData->param == 1) {
                        nPwrDebugLogEnable = 1;
                    }
                }
            }
            break;

        case POWER_MSG_SCN_REG:
            ALOGD("[powerd_req] POWER_MSG_SCN_REG");
            if((vpRspScn = (struct tScnData*)malloc(sizeof(struct tScnData))) == NULL)
                break;

            if(vpData->pBuf) {
                vpScnData = (struct tScnData*)(vpData->pBuf);
                ALOGI("[powerd_req] POWER_MSG_SCN_REG: pid:%d, uid:%d", vpScnData->param1, vpScnData->param2);

                vpRspScn->handle = perfUserRegScn(vpScnData->param1,vpScnData->param2); // allocate handle
                vpRsp->pBuf = (void*)vpRspScn;
            }
            break;

        case POWER_MSG_SCN_CONFIG:
            if(vpData->pBuf) {
                vpScnData = (struct tScnData*)(vpData->pBuf);
                ALOGD("[powerd_req] POWER_MSG_SCN_CONFIG: hdl:%d, cmd:%d, param1:%d, param2:%d",
                    vpScnData->handle, vpScnData->command, vpScnData->param1, vpScnData->param2);
                perfUserRegScnConfig(vpScnData->handle, vpScnData->command, vpScnData->param1, vpScnData->param2, vpScnData->param3, vpScnData->param4);
            }
            break;

        case POWER_MSG_SCN_UNREG:
            if(vpData->pBuf) {
                vpScnData = (struct tScnData*)(vpData->pBuf);
                ALOGI("[powerd_req] POWER_MSG_SCN_UNREG: hdl:%d", vpScnData->handle);
                perfUserUnregScn(vpScnData->handle);
            }
            break;

        case POWER_MSG_SCN_ENABLE:
            if(vpData->pBuf) {
                vpScnData = (struct tScnData*)(vpData->pBuf);
                ALOGI("[powerd_req] POWER_MSG_SCN_ENABLE: hdl:%d, timeout:%d", vpScnData->handle, vpScnData->timeout);

                remove_scn_timer(vpScnData->handle);
                if(vpScnData->timeout > 0) {
                    start_scn_timer(TIMER_MSG_USER_SCN_ENABLE_TIMEOUT, vpScnData->handle, vpScnData->timeout);
                }
                perfUserScnEnable(vpScnData->handle);
            }
            break;

        case POWER_MSG_SCN_DISABLE:
            if(vpData->pBuf) {
                vpScnData = (struct tScnData*)(vpData->pBuf);

                remove_scn_timer(vpScnData->handle);
                perfUserScnDisable(vpScnData->handle);
            }
            break;

        case POWER_MSG_SCN_DISABLE_ALL:
            if(vpData->pBuf) {
                ALOGI("[powerd_req] POWER_MSG_SCN_DISABLE_ALL");
                vpScnData = (struct tScnData*)(vpData->pBuf);
                perfUserScnDisableAll();
            }
            break;

        case POWER_MSG_SCN_RESTORE_ALL:
            if(vpData->pBuf) {
                ALOGI("[powerd_req] POWER_MSG_SCN_RESTORE_ALL");
                vpScnData = (struct tScnData*)(vpData->pBuf);
                perfUserScnRestoreAll();
            }
            break;

        default:
            ALOGI("unknown message");
            break;
        }

        *pRspMsg = (void *) vpRsp;
    }

    return 0;
}

//#ifdef __cplusplus
//}
//#endif


