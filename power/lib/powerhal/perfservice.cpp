/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein is
 * confidential and proprietary to MediaTek Inc. and/or its licensors. Without
 * the prior written permission of MediaTek inc. and/or its licensors, any
 * reproduction, modification, use or disclosure of MediaTek Software, and
 * information contained herein, in whole or in part, shall be strictly
 * prohibited.
 *
 * MediaTek Inc. (C) 2010. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER
 * ON AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL
 * WARRANTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR
 * NONINFRINGEMENT. NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH
 * RESPECT TO THE SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY,
 * INCORPORATED IN, OR SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES
 * TO LOOK ONLY TO SUCH THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO.
 * RECEIVER EXPRESSLY ACKNOWLEDGES THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO
 * OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES CONTAINED IN MEDIATEK
 * SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE
 * RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S
 * ENTIRE AND CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE
 * RELEASED HEREUNDER WILL BE, AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE
 * MEDIATEK SOFTWARE AT ISSUE, OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE
 * CHARGE PAID BY RECEIVER TO MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek
 * Software") have been modified by MediaTek Inc. All revisions are subject to
 * any receiver's applicable license agreements with MediaTek Inc.
 */

#define LOG_TAG "libPowerHal"
#define ATRACE_TAG ATRACE_TAG_PERF

#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>
#include <cutils/properties.h>
#include <utils/Log.h>
#include <utils/RefBase.h>
#include <dlfcn.h>
#include <string.h>
#include <utils/Trace.h>
#include "perfservice.h"
#include "perfservice_dec.h"
#include "common.h"
//#include <gui/SurfaceComposerClient.h>
//#include <gui/ISurfaceComposer.h>
//#include <ui/DisplayInfo.h>

#include <iostream>
#include <string>
#include <fstream>
#include <unistd.h>
#include <vector>
#include <expat.h>
#include <dfps/FpsPolicy.h>
#include <dfps/FpsInfo.h>

#include <vendor/mediatek/hardware/power/1.1/IPower.h>
#include <vendor/mediatek/hardware/power/1.1/types.h>

using namespace vendor::mediatek::hardware::power::V1_1;


/* Definition */
#define VERSION "V7.0"

#define STATE_ON 1
#define STATE_OFF 0
#define STATE_WAIT_RESTORE 2

#define PACK_NAME_MAX   128
#define CLASS_NAME_MAX  128
#define GIFTATTR_NAME_MAX  100
#define GIFTATTR_VALUE_MAX  128
#define GIFTATTR_NAME_LIMITE 5
#define PACK_LIST_XMLPATH       "/vendor/etc/power_whitelist_cfg.xml"
#define PACK_LIST_XMLPATH_2     "/data/system/power_whitelist_cfg.xml"
#define CUS_SCN_TABLE           "/vendor/etc/powerscntbl.cfg"
#define CUS_CONFIG_TABLE        "/vendor/etc/powercontable.cfg"
#define CUS_CONFIG_TABLE_T      "/vendor/etc/powercontable_t.cfg"
#define COMM_NAME_SIZE  64
#define PATH_LENGTH     128
#define PPM_MODE_LEN    32
#define FSTB_FPS_LEN    32
#define DCM_MODE_LEN    32
#define DCS_MODE_LEN    32

#define REG_SCN_MAX     256   // user scenario max number
#define CLUSTER_MAX     8
#define CORE_MAX        0xff
#define FREQ_MAX        0xffffff
#define PPM_IGNORE      (-1)
#define THRESHOLD_MAX   (100)   // cpu up, down threshold
#define TIMES_MAX       (100)   // cpu up, down times
#define TIMES_MIN       (1)     // cpu up, down times
#define HPS_NORMAL_MODE  (0)
#define DVFS_NORMAL_MODE (0)
#define DEFAULT_HTASK_THRSHOLD (1000)
#define HTASK_THRESHOLD_MAX    (1023)

#define RENDER_BIT    0x800000
#define RENDER_MASK   0x7FFFFF

#define DVFS_DEFAULT_VALUE (0)
#define NORMALIZE_PERF_IDX_MAX  (1000)

#define FBC_LIB_FULL_NAME  "libpowerhalctl.so"
#define XMLPARSE_GET_ACTIVITYNUM      1
#define XMLPARSE_GET_ACTIVITYLIST     2

//#define PROC_NAME_BROWS "com.android.browser" //TEST

#ifdef max
#undef max
#endif
#define max(a,b) (((a) > (b)) ? (a) : (b))

#ifdef min
#undef min
#endif
#define min(a,b) (((a) < (b)) ? (a) : (b))

#ifdef ALOGD
#undef ALOGD
#define ALOGD(...) do{if(nDebugLogEnable)((void)ALOG(LOG_INFO, LOG_TAG, __VA_ARGS__));}while(0)
#endif

#ifdef ALOGV
#undef ALOGV
#define ALOGV(...) do{if(nVerboseLogEnable)((void)ALOG(LOG_INFO, LOG_TAG, __VA_ARGS__));}while(0)
#endif

using namespace std;

const int FIELD_SIZE           = 64;
const string LESS("less");

typedef struct tScnConTable {
    string  cmdName;
    int     cmdID;
    string  entry;
    int     defaultVal;
    int     curVal;
    string  comp;
    int     maxVal;
    int     minVal;
    int     resetVal; // value to reset this entry
    int     isValid;
    int     normalVal;
    int     sportVal;
} tScnConTable;

tScnConTable tConTable[FIELD_SIZE];

/* Data structure */
typedef struct tClassNode{
    char               className[CLASS_NAME_MAX]; // component name
    int                active;
    struct tClassNode *next;
}tClassNode;

typedef struct tScnNode{
    int  scn_type;
    int  scn_state;
    char pack_name[PACK_NAME_MAX];
    char act_name[CLASS_NAME_MAX];
    int  scn_core_total;
    int  scn_core_min[CLUSTER_MAX];
    int  scn_core_max[CLUSTER_MAX];
    int  scn_core_hard_min[CLUSTER_MAX];
    int  scn_vcore;
    int  scn_freq_min[CLUSTER_MAX];
    int  scn_freq_max[CLUSTER_MAX];
    int  scn_gpu_freq;
    int  scn_gpu_freq_max;    // upper bound
    int  scn_perf_idx;
    int  scn_ppm_mode;
    int  scn_wiphy_cam;
    int  scn_cpuset;
    int  screen_off_action;
    int  boost_mode;
    int  boost_timeout;
    int  dfps_mode;
    int  dfps;
    int  disp_mode;
    int  video_mode;
    int  scn_valid;  // valid of pre-defined scenario
    int  pid;
    int  tid;
    int  scn_fstb_fps_low;
    int  scn_fstb_fps_high;
    int  scn_param[FIELD_SIZE];
    char comm[COMM_NAME_SIZE];
    tClassNode *class_list;
}tScnNode;

typedef struct tPidBoost {
    char p_name[COMM_NAME_SIZE];
    char t_name[COMM_NAME_SIZE];
    int  policy;
    int  param;
    int  ori_policy;
    int  ori_param;
} tPidBoost;

typedef struct tClusterInfo {
    int  cpuNum;
    int  cpuFirstIndex;
    int  cpuMinNow;
    int  cpuMaxNow;
    int  cpuHardMinNow;
    int *pFreqTbl;
    int  freqCount;
    int  freqMax;
    int  freqMinNow;
    int  freqMaxNow;
} tClusterInfo;

typedef struct tDrvInfo {
    int cpuNum;
    int perfmgrLegacy; // /proc/perfmgr/legacy/perfserv_freq
    int ppmSupport;
    int ppmAll;        // userlimit_cpu_freq
    int acao;
    int hmp;
    int hps;
    int dvfsHevc;
    int fliper;
    int sysfs;
    int dvfs;
    int turbo;
    int fstb;
} tDrvInfo;

enum {
    SPORTS_BENCHMARK    = 0,
    SPORTS_USER_NOTIFY  = 1,
};

enum {
    SCN_POWER_HINT      = 0,
    SCN_CUS_POWER_HINT  = 1,
    SCN_USER_HINT       = 2,
    SCN_PACK_HINT       = 3,
};

/* Function prototype */
void setClusterCores(int scenario, int clusterNum, int totalCore, int *pCoreTbl, int *pMaxCoreTbl);
void setClusterFreq(int scenario, int clusterNum, int *pFreqTbl, int *pMaxFreqTbl);
void get_activity_num_XML(const char *path);
void get_activity_list_XML(const char *path);
int get_activity_totalnum(void);
void updateScnListfromXML(tScnNode *pPackList);
int perfScnEnable(int scenario);
int perfScnDisable(int scenario);
void setGpuFreq(int scenario, int level);
void setGpuFreqMax(int scenario, int level);
void setPerfIdx(int scenario, int index);
int updateCusScnTable(const char *path);
//int updateCusBoostTable(const char *path);
void resetScenario(int handle);
void checkDrvSupport(tDrvInfo *ptDrvInfo);
void getCputopoInfo(int, int *, int *, tClusterInfo **);
void getPpmPerfIndexInfo(int *pPerfIdxMin, int *pPerfIdxMax);
int cmdSetting(int, char *, tScnNode *, int, int, int, int);
int initPowerMode(void);
int switchPowerMode(int mode);
int switchSportsMode(int reason, int bEnable);
static int load_fbc_api(void);
typedef int (*fbc_act_switch)(int);
typedef int (*fbc_touch)(int);

/* function pointer to perfserv client */
static int  (*fbcNotifyActSwitch)(int) = NULL;
static int  (*fbcNotifyTouch)(int) = NULL;

/* Global variable */
static int nIsReady = 0;
static char* sProjName = (char*)PROJ_ALL;
static Mutex sMutex;
static int scn_cores_now = 0;
static int scn_vcore_now = 0;
static int scn_wiphy_cam_now = 0;
static int nSetCoreMax = 0;    // set to 1 if anyone set core max
static int nSetFreqMax = 0;    // set to 1 if anyone set cpu freq max
static int nSetGpuFreqMax = 0; // set to 1 if anyone set gpu freq max
static int nSetWiphyCAM = 1;
static int scn_perf_idx_now = 0;
static int scn_cpuset_now = 0;
static int nPpmPerfIdxMin = 0;
static int nPpmPerfIdxMax = 0;

static tClusterInfo *ptClusterTbl = NULL;
static int           nClusterNum = 0;

static tScnNode  *ptScnList = NULL;
static xml_activity  *ptXmlActList = NULL;
static xml_gift      *ptXmlGiftTagList = NULL;

static tDrvInfo   gtDrvInfo;
static int        nPackNum = 0;
static int        nCpuNum = 0;
static int        nCpuSet = 0; // cpuset for all cpu on
static int        nUserScnBase = 0;
static int        SCN_APP_RUN_BASE = (int)MtkPowerHint::MTK_POWER_HINT_NUM + REG_SCN_MAX;
static int        nXmlPackNum = 0;
static int        nXmlGiftTagNum = 0;
static int        nXmlGiftAttrNum = 0;
static int        nXmlActNum = 0;
static int        nXmlCmdNum = 0;
static int        nXmlCmdIndex = 0;
static int        nXmlCommonIndex = 0;
static int        nXmlGiftTagIndex = 0;
static int        xmlPerService_flag = 0;
static char       cXmlPack[PACK_NAME_MAX];
static char       cXmlAct[CLASS_NAME_MAX];

#if 0
static tScnNode   tSportsScnList[SCN_NUM];
static tScnNode   tNormalScnList[SCN_NUM];
static tScnNode  *ptSportsAppList = NULL;
static tScnNode  *ptNormalAppList = NULL;
#endif

static int nGpuFreqCount = 0;
static int nGpuHighestFreqLevel = 0;
static int scn_gpu_freq_now = 0;
static int scn_gpu_freq_max_now = 0;

//static int nDefaultAboveHispeedDelay = -1;

static int system_cgroup_fd = -1;
static int last_boost_tid = -1;
static int foreground_uid = -1;
static int last_from_uid  = 1;
static int scn_check_retry = CHECK_NONE;
static int walt_follow_scn = -1;
static int walt_duration = -1;
static int pboost_timeout = 0;
static char foreground_pack[PACK_NAME_MAX];
static char foreground_act[PACK_NAME_MAX];
static char last_check_pack[PACK_NAME_MAX];
static String8 foregroundName("");

sp<FpsPolicy> mFpsPolicy;
sp<FpsInfo>   mFpsInfo;

static tPidBoost *ptPidBoostList = NULL;
static int        nPidListNum = 0;
static int        nDisplayType = DISPLAY_TYPE_OTHERS;
static int        nDuringFrameUpdate = 0;

static int nCurrPowerMode = PERF_MODE_NORMAL;
static int nCurrBenchmarkMode = 0;
static int nUserNotifyBenchmark = 0;
static int nUserSpecifiedEnable = 0; // GAME + white list + user scenario

int nDebugLogEnable = 0;
int nVerboseLogEnable = 0;
int nSmartDebugEnable = 0;

char pPpmDefaultMode[PPM_MODE_LEN] = "";
char tPpmMode[PPM_MODE_NUM][PPM_MODE_LEN] =
{
    "Low_Power",
    "Just_Make",
    "Performance",
};
static int nPpmCurrMode = PPM_IGNORE;
static char pFstbDefaultFps[FSTB_FPS_LEN];

static int nDefaultRushBoostEnabled = 1;
static int nDefaultHeavyTaskEnabled = 0;
static int nCurrRushBoostEnabled = PPM_IGNORE;
static int nCurrHeavyTaskEnabled = PPM_IGNORE;

static int  nFbcSupport = 1;

void checkConTable(void) {
    ALOGI("Cmd name, Cmd ID, Entry, default value, current value, compare, max value, min value, isValid, normal value, sport value");
    for(int idx = 0; idx < FIELD_SIZE; idx++) {
        if(tConTable[idx].cmdName.length() == 0)
            continue;
        ALOGI("%s, %d, %s, %d, %d, %s, %d, %d %d %d %d", tConTable[idx].cmdName.c_str(),  tConTable[idx].cmdID, tConTable[idx].entry.c_str(), tConTable[idx].defaultVal,
                tConTable[idx].curVal, tConTable[idx].comp.c_str(), tConTable[idx].maxVal, tConTable[idx].minVal, tConTable[idx].isValid, tConTable[idx].normalVal, tConTable[idx].sportVal);
    }
}

/* Function */
void xmlparser_start(void *userData, const char *name, const char *arg[])
{
    int i, AttrIndex, len, AttrNum=0;
    int *xmlaction = (int *)userData;

    if(*xmlaction == XMLPARSE_GET_ACTIVITYNUM)
    {
        if (!strcmp(name, "PerfService")) {
            xmlPerService_flag = 1;
        }

        if((xmlPerService_flag == 1) && !strcmp(name, "Package")){
            nXmlPackNum++;
        }

        if((xmlPerService_flag == 1) && !strcmp(name, "Activity")){
            nXmlActNum++;
        }

        if((xmlPerService_flag == 1) && !strcmp(name, "GiFT")){
            nXmlGiftTagNum++;
            for(i=0;arg[i]!=0;i+=2){
                AttrNum++;
            }
            nXmlGiftAttrNum = max(nXmlGiftAttrNum,AttrNum);
            nXmlGiftAttrNum = min(nXmlGiftAttrNum,GIFTATTR_NAME_LIMITE);
        }

        if((xmlPerService_flag == 1) && !strncmp(name, "CMD_SET", 7)){
            nXmlCmdNum++;
        }
    }
    else
    if(*xmlaction == XMLPARSE_GET_ACTIVITYLIST) {
        if (!strcmp(name, "PerfService")) {
            xmlPerService_flag = 1;
        }

        if((xmlPerService_flag == 1) && !strcmp(name, "Package")){
            memset(cXmlPack, 0, PACK_NAME_MAX);
            len = strlen(arg[1]);
            if(len >= PACK_NAME_MAX)
                len = PACK_NAME_MAX-1;
            strncpy(cXmlPack, arg[1], len);
        }

        if((xmlPerService_flag == 1) && !strcmp(name, "GiFT")){
            if(nXmlGiftTagIndex >= nXmlGiftTagNum){
                ALOGI("[xmlparser_start] nXmlGiftTagIndex > nXmlGiftTagNum");
            }
            else{
                memset(ptXmlGiftTagList[nXmlGiftTagIndex].packName, 0, PACK_NAME_MAX);
                len = strlen(cXmlPack);
                if(len >= PACK_NAME_MAX)
                    len = PACK_NAME_MAX-1;
                strncpy(ptXmlGiftTagList[nXmlGiftTagIndex].packName, cXmlPack, len);

                ALOGI("[xmlparser_start] nXmlGiftTagIndex:%d pack:%s",nXmlGiftTagIndex, ptXmlGiftTagList[nXmlGiftTagIndex].packName);
                AttrIndex=0;
                for(i=0;arg[i]!=0;i+=2){
                    if((AttrIndex >= GIFTATTR_NAME_LIMITE) || (AttrIndex >= nXmlGiftAttrNum)){
                        ALOGI("[xmlparser_start] AttrIndex > GIFTATTR_NAME_LIMITE");
                        break;
                    }

                    len = strlen(arg[i]);
                    if(len >= GIFTATTR_NAME_MAX)
                        len = GIFTATTR_NAME_MAX-1;
                    strncpy(ptXmlGiftTagList[nXmlGiftTagIndex].AttrName[AttrIndex], arg[i], len);

                    len = strlen(arg[i+1]);
                    if(len >= GIFTATTR_VALUE_MAX)
                        len = GIFTATTR_VALUE_MAX-1;
                    strncpy(ptXmlGiftTagList[nXmlGiftTagIndex].AttrValue[AttrIndex], arg[i+1], len);

                    ALOGI("[xmlparser_start] AttrIndex:%d AttrName:%s AttrValue:%s",AttrIndex,
                    ptXmlGiftTagList[nXmlGiftTagIndex].AttrName[AttrIndex],
                    ptXmlGiftTagList[nXmlGiftTagIndex].AttrValue[AttrIndex]
                    );

                    AttrIndex++;
                }
                nXmlGiftTagIndex++;
            }
        }

        if((xmlPerService_flag == 1) && !strcmp(name, "Activity")){
            if(!strcmp(arg[1], "Common")){
                sprintf(cXmlAct, "Common%d", nXmlCommonIndex);
                nXmlCommonIndex++;
            }
            else{
                memset(cXmlAct, 0, CLASS_NAME_MAX);
                len = strlen(arg[1]);
                if(len >= CLASS_NAME_MAX)
                len = CLASS_NAME_MAX-1;
                strncpy(cXmlAct, arg[1], len);
            }
        }

        if((xmlPerService_flag == 1) && !strncmp(name, "CMD_SET", 7)){
            memset(ptXmlActList[nXmlCmdIndex].cmd, 0, 128);
            len = strlen(name);
            if(len>=128)
               len=127;
            strncpy(ptXmlActList[nXmlCmdIndex].cmd, name,len);

            memset(ptXmlActList[nXmlCmdIndex].packName, 0, 128);
            len = strlen(cXmlPack);
            if(len>=128)
                len=127;
            strncpy(ptXmlActList[nXmlCmdIndex].packName, cXmlPack, len);

            memset(ptXmlActList[nXmlCmdIndex].actName, 0, 128);
            len = strlen(cXmlAct);
            if(len>=128)
                len=127;
            strncpy(ptXmlActList[nXmlCmdIndex].actName, cXmlAct, len);
            for(i=0;arg[i]!=0;i+=2) {
                if(i==0)
                    ptXmlActList[nXmlCmdIndex].param1 = atoi(arg[1]);
                if(i==2)
                    ptXmlActList[nXmlCmdIndex].param2 = atoi(arg[3]);
                if(i==4)
                    ptXmlActList[nXmlCmdIndex].param3 = atoi(arg[5]);
                if(i==6)
                    ptXmlActList[nXmlCmdIndex].param4 = atoi(arg[7]);
            }
            ALOGI("[xmlparser_start] XMLPARSE_GET_ACTIVITYLIST CmdIndex:%d cmd:%s pack:%s, activity:%s p1:%d p2:%d p3:%d p4:%d",nXmlCmdIndex,
            ptXmlActList[nXmlCmdIndex].cmd,
            ptXmlActList[nXmlCmdIndex].packName,
            ptXmlActList[nXmlCmdIndex].actName,
            ptXmlActList[nXmlCmdIndex].param1,
            ptXmlActList[nXmlCmdIndex].param2,
            ptXmlActList[nXmlCmdIndex].param3,
            ptXmlActList[nXmlCmdIndex].param4
            );
            nXmlCmdIndex++;
        }
    }
}

void xmlparser_end(void *userData, const char *name)
{
    ALOGV("[xmlparser_end] userData:%p", userData);
    if (!strcmp(name, "PerfService")) {
        xmlPerService_flag = 0;
    }
}

void updateScnListfromXML(tScnNode *pPackList)
{
    char p_name[128], a_name[128];
    char **act_name;;
    int   i = 0, j = 0, num = 0, lines = 0;
    //int   mode = PERF_MODE_NORMAL;

    for (j = 0; perf_app[j].cmd != -1; j++)
        lines++;

    lines += nXmlCmdNum;

    act_name = (char **) malloc(lines * sizeof(char*));
    for (i = 0; i < lines; i++)
        act_name[i] = (char *) malloc(CLASS_NAME_MAX * sizeof(char));

    for (j = 0; perf_app[j].cmd != -1; j++) {
        set_str_cpy(p_name, perf_app[j].packName, 128);
         set_str_cpy(a_name, perf_app[j].actName, 128);
         for(i = 0; i < num && strcmp(a_name, act_name[i]) != 0; i++);

        if(i == num) { // new pack
            set_str_cpy(act_name[i], a_name, CLASS_NAME_MAX);
            pPackList[i].scn_type     = SCN_APP_RUN_BASE;
            pPackList[i].scn_state    = STATE_OFF;
            pPackList[i].pack_name[0] = '\0';
            pPackList[i].act_name[0]  = '\0';
            pPackList[i].class_list   = NULL;
            pPackList[i].screen_off_action = STATE_WAIT_RESTORE;
            pPackList[i].scn_valid    = 1;
            set_str_cpy(pPackList[i].pack_name, p_name, PACK_NAME_MAX);
            set_str_cpy(pPackList[i].act_name, a_name, CLASS_NAME_MAX);
        }

        cmdSetting(perf_app[j].cmd, NULL, &pPackList[i], perf_app[j].param1, perf_app[j].param2, 0, 0);

        if (i == num){
            num++;
        }
    }

    ALOGI("[updateScnListfromXML] new activity from perf_app, num:%d", num);

    for(j = 0; j < nXmlCmdNum; j++){
        if(strncmp(ptXmlActList[j].cmd, "CMD_SET", 7) == 0){
            for(i = 0; i < num && strcmp(ptXmlActList[j].actName, act_name[i]) != 0; i++);
            if(i == num) { // new pack
                set_str_cpy(act_name[i], ptXmlActList[j].actName, CLASS_NAME_MAX);
                pPackList[i].scn_type     = SCN_APP_RUN_BASE;
                pPackList[i].scn_state    = STATE_OFF;
                pPackList[i].pack_name[0] = '\0';
                pPackList[i].act_name[0]  = '\0';
                pPackList[i].class_list   = NULL;
                pPackList[i].screen_off_action = STATE_WAIT_RESTORE;
                //pPackList[i].scn_valid    = (mode == PERF_MODE_NORMAL) ? 1 : 0;  // only normal mode app are default valid
                pPackList[i].scn_valid    = 1;
                set_str_cpy(pPackList[i].pack_name, ptXmlActList[j].packName, PACK_NAME_MAX);
                set_str_cpy(pPackList[i].act_name, ptXmlActList[j].actName, CLASS_NAME_MAX);
            }

            // normal mode table
            cmdSetting(-1, ptXmlActList[j].cmd, &pPackList[i], ptXmlActList[j].param1, ptXmlActList[j].param2, 0, 0);
            if (i == num)
                num++;
        }
        else{
            continue;
        }
    }

    ALOGI("[updateScnListfromXML] new activity from perf_app and whiteList, num:%d", num);

    for (i = 0; i < lines; i++)
        free(act_name[i]);
    free(act_name);
}


void get_activity_num_XML(const char *path)
{
    int len = 0, xmlaction = XMLPARSE_GET_ACTIVITYNUM;
    char val[512];
    FILE *fh = NULL;

    XML_Parser parser = XML_ParserCreate(NULL);
    XML_SetUserData(parser, (void *)&xmlaction);
    XML_SetElementHandler(parser, xmlparser_start, xmlparser_end);
    nXmlPackNum = nXmlActNum = nXmlCmdNum = 0;
    nXmlGiftTagNum = nXmlGiftAttrNum = 0;
    xmlPerService_flag = 0;

    fh = fopen(path, "r");

    if (fh){
        len = fread(val, 1, 512, fh);
        while(len)
        {
            if(0 == XML_Parse(parser, val, len, feof(fh)))
            {
                int code = XML_GetErrorCode(parser);
                const char *msg = (const char *)XML_ErrorString((XML_Error)code);
                ALOGI("[get_activity_num_XML] Parsing error code %d message \"%s\"\n",code, msg);
                break;
            }
            len = fread(val, 1, 512, fh);
        }

        XML_ParserFree(parser);
        fclose(fh);
    }
    else{
        ALOGI("[get_activity_num_XML] whitelist_cfg.xml does not exist");
    }

}

void get_activity_list_XML(const char *path)
{
    int len = 0, xmlaction = XMLPARSE_GET_ACTIVITYLIST;
    char val[512];
    FILE *fh = NULL;

    XML_Parser parser = XML_ParserCreate(NULL);
    XML_SetUserData(parser, (void *)&xmlaction);
    XML_SetElementHandler(parser, xmlparser_start, xmlparser_end);
    xmlPerService_flag = 0;
    nXmlCommonIndex = 0;
    nXmlCmdIndex = nXmlGiftTagIndex = 0;

    fh = fopen(path, "r");
    if (fh)
    {
        len = fread(val, 1, 512, fh);
        while(len)
        {
            if(0 == XML_Parse(parser, val, len, feof(fh)))
            {
                int code = XML_GetErrorCode(parser);
                const char *msg = (const char *)XML_ErrorString((XML_Error)code);
                ALOGI("[get_activity_list_XML] Parsing error code %d message \"%s\"\n",code, msg);
                break;
            }
            len = fread(val, 1, 512, fh);
        }

        XML_ParserFree(parser);
        fclose(fh);
    }
    else {
        ALOGI("[get_activity_list_XML] whitelist_cfg.xml does not exist");
    }
}

int get_activity_totalnum(void)
{
    char **act_name;
    int num = 0, lines = 0, i, j;

    for (j = 0; perf_app[j].cmd != -1; j++)
        lines++;

    lines += nXmlCmdNum;

    act_name = (char **) malloc(lines * sizeof(char*));
    for (i = 0; i < lines; i++)
        act_name[i] = (char *) malloc(CLASS_NAME_MAX * sizeof(char));

    for (j = 0; perf_app[j].cmd != -1; j++) {
        for(i = 0; i < num && strcmp(perf_app[j].actName, act_name[i]) != 0; i++);

        if(i == num){
            set_str_cpy(act_name[i], perf_app[j].actName, CLASS_NAME_MAX);
            num++;
        }
    }

    for(j = 0; j < nXmlCmdNum; j++)	{
        if(strncmp(ptXmlActList[j].cmd, "CMD_SET", 7) == 0){
            for(i = 0; i < num && strcmp(ptXmlActList[j].actName, act_name[i]) != 0; i++);

            if(i == num) { // new pack
                num++;
                set_str_cpy(act_name[i], ptXmlActList[j].actName, CLASS_NAME_MAX);
            }
        }
        else{
            if(!strncmp(ptXmlActList[j].cmd, "[SPORTS]", 8))
                continue;

            for(i = 0; i < num && strcmp(ptXmlActList[j].actName, act_name[i]) != 0; i++);

            if (i == num){ //new pack
                num++;
                set_str_cpy(act_name[i], ptXmlActList[j].actName, CLASS_NAME_MAX);
            }
        }
    }
    ALOGI("[get_activity_totalnum]  total ture activity num :%d\n", num);

    for (i = 0; i < lines; i++)
        free(act_name[i]);
    free(act_name);

    return num;
}

int init_Gift_buf(void){
    int i, j, result = -1;

    if(nXmlGiftTagNum != 0 && nXmlGiftAttrNum != 0){
        if((ptXmlGiftTagList = (xml_gift*)malloc(sizeof(xml_activity)*(nXmlGiftTagNum))) == NULL) {
            ALOGE("Can't allocate memory");
            result = -1;
        }

        for(i = 0; i < nXmlGiftTagNum; i++){
            if((ptXmlGiftTagList[i].AttrName = (char **)malloc(sizeof(char*)*nXmlGiftAttrNum)) == NULL){
                ALOGE("Can't allocate memory");
                result = -1;
            }

            if((ptXmlGiftTagList[i].AttrValue = (char **)malloc(sizeof(char*)*nXmlGiftAttrNum)) == NULL){
                ALOGE("Can't allocate memory");
                result = -1;
            }

            for(j=0;j<nXmlGiftAttrNum;j++){
                ptXmlGiftTagList[i].AttrName[j] = (char*)malloc(sizeof(char)* GIFTATTR_NAME_MAX);
                if(ptXmlGiftTagList[i].AttrName[j] == NULL){
                    ALOGE("Can't allocate memory");
                    result = -1;
                }
                memset(ptXmlGiftTagList[i].AttrName[j], 0, GIFTATTR_NAME_MAX);

                ptXmlGiftTagList[i].AttrValue[j]= (char*)malloc(sizeof(char)* GIFTATTR_VALUE_MAX);
                if(ptXmlGiftTagList[i].AttrValue[j] == NULL){
                    ALOGE("Can't allocate memory");
                    result = -1;
                }
                memset(ptXmlGiftTagList[i].AttrValue[j], 0, GIFTATTR_VALUE_MAX);
            }
        }
        result = 0;
    }
    else{
        ALOGE("No GiFT Tags");
        result = 0;
    }

    return result;
}

int init()
{
    int i, index = 0, debug_mode;
    int coresToSet[CLUSTER_MAX], freqToSet[CLUSTER_MAX];
    char str[PPM_MODE_LEN], str2[FSTB_FPS_LEN];
    char value[PROPERTY_VALUE_MAX];
    struct stat stat_buf;

    if (!nIsReady) {
        ALOGI("perfservice ver:%s", VERSION);

        /* check HMP support */
        checkDrvSupport(&gtDrvInfo);
        if(gtDrvInfo.sysfs == 0 && gtDrvInfo.dvfs == 0) // /sys/devices/system/cpu/possible is not existed
            return 0;
        getCputopoInfo(gtDrvInfo.hmp, &nCpuNum, &nClusterNum, &ptClusterTbl);
        scn_cpuset_now = nCpuSet = (1 << nCpuNum) - 1;

        if(gtDrvInfo.ppmSupport && !gtDrvInfo.acao) {
            getPpmPerfIndexInfo(&nPpmPerfIdxMin, &nPpmPerfIdxMax);
            get_str_value(PATH_PPM_MODE, str, sizeof(str)-1);
            sscanf(str, "%31s", pPpmDefaultMode);
            ALOGI("pPpmDefaultMode:%s, %d", pPpmDefaultMode, nPpmCurrMode);
        }

        /* temp */
        if (nClusterNum == 1) gtDrvInfo.hmp = 0;

        if(gtDrvInfo.fstb) {
            get_str_value(PATH_FSTB_FPS, str2, sizeof(str2)-1);
            strncpy(pFstbDefaultFps, str2, FSTB_FPS_LEN - 1);
            ALOGI("pFstbDefaultFps:%s", pFstbDefaultFps);
        }

        /* init value */
#if 0
        for (i=0; i<nClusterNum; i++) {
            coresToSet[i] = 0;
            freqToSet[i] = 0;
        }
        setClusterCores(0, nClusterNum, nCpuNum, coresToSet);
        setClusterFreq(0, nClusterNum, freqToSet);
        for (i=0; i<nClusterNum; i++) {
            coresToSet[i] = CORE_MAX;
            freqToSet[i] = FREQ_MAX;
        }
        setClusterMaxCores(0, nClusterNum, coresToSet);
        setClusterMaxFreq(0, nClusterNum, freqToSet);
#endif

        /* GPU info */
        get_gpu_freq_level_count(&nGpuFreqCount);
        nGpuHighestFreqLevel = scn_gpu_freq_max_now = nGpuFreqCount - 1;
        ALOGI("nGpuFreqCount:%d", nGpuFreqCount);
        /* GPU init value */
        //setGpuFreq(0, 0);
        //setGpuFreqMax(0, nGpuHighestFreqLevel);

        /* get file */
        if (access(PACK_LIST_XMLPATH_2, F_OK) != -1)
            get_activity_num_XML(PACK_LIST_XMLPATH_2);
        else
            get_activity_num_XML(PACK_LIST_XMLPATH);

        /* init Gift buf*/
        init_Gift_buf();

        if(nXmlCmdNum != 0){
            ALOGI("[init] nXmlPackNum:%d nXmlActivityNum:%d nXmlCmdNum:%d nXmlGiftTagNum:%d nXmlGiftAttrNum:%d", nXmlPackNum, nXmlActNum, nXmlCmdNum, nXmlGiftTagNum, nXmlGiftAttrNum);
            if((ptXmlActList = (xml_activity*)malloc(sizeof(xml_activity)*(nXmlCmdNum))) == NULL) {
                ALOGE("Can't allocate memory");
                return 0;
            }

            memset(ptXmlActList, 0, sizeof(xml_activity)*nXmlCmdNum);
            if (access(PACK_LIST_XMLPATH_2, F_OK) != -1)
                get_activity_list_XML(PACK_LIST_XMLPATH_2);
            else
                get_activity_list_XML(PACK_LIST_XMLPATH);
        }
        else{
            ALOGI("[init] No activity data from white list!!");
        }

        nPackNum = get_activity_totalnum();
        ALOGI("[init] nPackNum:%d", nPackNum);

        if((ptScnList = (tScnNode*)malloc(sizeof(tScnNode) * (SCN_APP_RUN_BASE + nPackNum))) == NULL) {
            ALOGE("Can't allocate memory");
            return 0;
        }

        SCN_APP_RUN_BASE = (int)(MtkPowerHint::MTK_POWER_HINT_NUM) + REG_SCN_MAX;
        memset(ptScnList, 0, sizeof(tScnNode)*(SCN_APP_RUN_BASE + nPackNum));
        nIsReady = 1;

        if(gtDrvInfo.turbo && (0 == stat(CUS_CONFIG_TABLE_T, &stat_buf)))
            loadConTable(CUS_CONFIG_TABLE_T);
        else
            loadConTable(CUS_CONFIG_TABLE);

        smart_init(); // init before white list
        smart_control(1); // it is always enable

        // configure the sProjName if needed.
        for (i = 0; i < (int)(MtkPowerHint::MTK_POWER_HINT_NUM); i++) {
            resetScenario(i); // reset all scenarios
            ptScnList[i].scn_type = SCN_POWER_HINT;
        }
        for (i = SCN_APP_RUN_BASE; i < SCN_APP_RUN_BASE + nPackNum; i++) {
            resetScenario(i); // reset all scenarios
            ptScnList[i].scn_type = SCN_PACK_HINT;
        }

        updateScnListfromXML(ptScnList+SCN_APP_RUN_BASE);

        if(ptXmlActList!=NULL)
            free(ptXmlActList);


        // empty list for user registration
        nUserScnBase = (int)MtkPowerHint::MTK_POWER_HINT_NUM;
        for (i = nUserScnBase; i < nUserScnBase + REG_SCN_MAX; i++) {
            resetScenario(i);
            ptScnList[i].class_list          = NULL;
        }

        char* filename;
        filename = (char*)"/dev/cpuctl/tasks";
        system_cgroup_fd = open(filename, O_WRONLY | O_CLOEXEC);
        if (system_cgroup_fd < 0) {
            ALOGI("open of %s failed: %s\n", filename, strerror(errno));
        }

        //set_value(PATH_CPUHOTPLUG_POWER_MODE, 1);  // workaround: get default value of normal mode
        //nDefaultAboveHispeedDelay = get_int_value(PATH_CPUFREQ_ABOVE_HISPEED_DELAY);

        updateCusScnTable(CUS_SCN_TABLE);
        //updateCusBoostTable(CUS_BOOST_TABLE);

        // power mode
        initPowerMode();

        // debug info
        debug_mode = atoi(value);
        nDebugLogEnable = debug_mode & 0x1;
        nVerboseLogEnable = debug_mode & 0x2;
        nSmartDebugEnable = debug_mode & 0x4;

        //smart_init();

        /* perfd */
        if(load_fbc_api() < 0) {
            nFbcSupport = 0; // fbc is supported
        }
        ALOGI("[init] nFbcSupport:%d", nFbcSupport);

        /* FPS policy */
        String8 string("");
        string.appendFormat("perfservice");
        mFpsPolicy = new FpsPolicy(FpsPolicy::API_WHITELIST, string);
        mFpsInfo = new FpsInfo();

//      close(system_cgroup_fd);
//      system_cgroup_fd = -1;
    }
    return 1;
}

static int load_fbc_api(void)
{
    void *handle, *func;

    handle = dlopen(FBC_LIB_FULL_NAME, RTLD_NOW);

    func = dlsym(handle, "fbcNotifyActSwitch");
    fbcNotifyActSwitch = reinterpret_cast<fbc_act_switch>(func);

    if (fbcNotifyActSwitch == NULL) {
        printf("fbcNotifyActSwitch error: %s", dlerror());
        dlclose(handle);
        return -1;
    }

    func = dlsym(handle, "fbcNotifyTouch");
    fbcNotifyTouch = reinterpret_cast<fbc_touch>(func);

    if (fbcNotifyTouch == NULL) {
        printf("fbcNotifyTouch error: %s", dlerror());
        dlclose(handle);
        return -1;
    }

    return 0;
}

void setClusterCores(int scenario, int clusterNum, int totalCore, int *pCoreTbl, int *pMaxCoreTbl)
{
    int coreToSet = 0, maxToSet = 0, coreToSetBig = 0, maxToSetBig = 0, clusterToSet[CLUSTER_MAX];
    int i;
    char str[128], buf[32];

    if(gtDrvInfo.acao)
        return;

    ALOGV("[setClusterCores] scn:%d, total:%d, cores:%d, %d", scenario, totalCore, pCoreTbl[0], pCoreTbl[1]);
    if (gtDrvInfo.perfmgrLegacy) {
        str[0] = '\0';

        for (i=0; i<clusterNum; i++) {
            coreToSet = (pCoreTbl[i] <= 0 || pCoreTbl[i] > ptClusterTbl[i].cpuNum) ? PPM_IGNORE : pCoreTbl[i];
            maxToSet = (pMaxCoreTbl[i] < 0 || pMaxCoreTbl[i] >= ptClusterTbl[i].cpuNum) ? PPM_IGNORE : pMaxCoreTbl[i];
            sprintf(buf, "%d %d ", coreToSet, maxToSet);
            /*strcat(str, buf);*/
            strncat(str, buf, strlen(buf));
        }
        str[strlen(str)-1] = '\0'; // remove last space
        set_value(PATH_PERFMGR_CORE_CTRL, str);
        ALOGI("%d: legacy set: %s", scenario, str);
    }
    else if (gtDrvInfo.ppmAll) {
        str[0] = '\0';

        for (i=0; i<clusterNum; i++) {
            coreToSet = (pCoreTbl[i] <= 0 || pCoreTbl[i] > ptClusterTbl[i].cpuNum) ? PPM_IGNORE : pCoreTbl[i];
            maxToSet = (pMaxCoreTbl[i] < 0 || pMaxCoreTbl[i] >= ptClusterTbl[i].cpuNum) ? PPM_IGNORE : pMaxCoreTbl[i];
            sprintf(buf, "%d %d ", coreToSet, maxToSet);
            strncat(str, buf, strlen(buf));
        }
        str[strlen(str)-1] = '\0'; // remove last space
        set_value(PATH_PPM_CORE_CTRL, str);
        ALOGI("%d: ppmall set: %s", scenario, str);
    }
    else if (gtDrvInfo.ppmSupport) {
        str[0] = '\0';

        for (i=0; i<clusterNum; i++) {
            coreToSet = (pCoreTbl[i] <= 0 || pCoreTbl[i] > ptClusterTbl[i].cpuNum) ? PPM_IGNORE : pCoreTbl[i];
            maxToSet = (pMaxCoreTbl[i] < 0 || pMaxCoreTbl[i] >= ptClusterTbl[i].cpuNum) ? PPM_IGNORE : pMaxCoreTbl[i];
            sprintf(buf, "%d %d ", coreToSet, maxToSet);
            strncat(str, buf, strlen(buf));
            set_value(PATH_PPM_CORE_BASE, i, coreToSet);
            set_value(PATH_PPM_CORE_LIMIT, i, maxToSet);
        }
        str[strlen(str)-1] = '\0'; // remove last space
        ALOGI("%d: ppmsupport set: %s", scenario, str);
    }
    else {
        // HPS: cpu_num_base must be >= 1
        coreToSet = (pCoreTbl[0] <= 0 || pCoreTbl[0] > ptClusterTbl[0].cpuNum) ? 1 : pCoreTbl[0];
        maxToSet = (pMaxCoreTbl[0] < 0 || pMaxCoreTbl[0] >= ptClusterTbl[0].cpuNum) ? ptClusterTbl[0].cpuNum : pMaxCoreTbl[0];
        if (gtDrvInfo.hmp) {
            coreToSetBig = (pCoreTbl[1] <= 0 || pCoreTbl[1] > ptClusterTbl[1].cpuNum) ? 0 : pCoreTbl[1];
            maxToSetBig = (pMaxCoreTbl[1] < 0 || pMaxCoreTbl[1] >= ptClusterTbl[1].cpuNum) ? ptClusterTbl[1].cpuNum : pMaxCoreTbl[1];
            set_value(PATH_CPUHOTPLUG_HPS_MIN, coreToSet, coreToSetBig);
            set_value(PATH_CPUHOTPLUG_HPS_MAX, maxToSet, maxToSetBig);
            ALOGI("%d: set: %d, %d", scenario, coreToSet, coreToSetBig);
            ALOGI("%d: set max: %d, %d", scenario, maxToSet, maxToSetBig);
        }
        else if (gtDrvInfo.hps) {
            set_value(PATH_CPUHOTPLUG_HPS_MIN, coreToSet);
            set_value(PATH_CPUHOTPLUG_HPS_MAX, maxToSet);
            ALOGI("%d: set: %d, set max:%d", scenario, coreToSet, maxToSet);
        }
        else {
            set_value(PATH_CPUHOTPLUG_CFG, coreToSet);
            ALOGI("%d: set: %d", scenario, coreToSet);
        }
    }
}

void setClusterFreq(int scenario, int clusterNum, int *pFreqTbl, int *pMaxFreqTbl)
{
    int freqToSet = 0, maxToSet = 0, freqToSetBig = 0, maxToSetBig = 0;
    int i;
    char str[128], buf[32];

    ALOGV("[setClusterFreq] scn:%d, freq:%d, %d", scenario, pFreqTbl[0], pFreqTbl[1]);
    if (gtDrvInfo.perfmgrLegacy) {
        str[0] = '\0';
        for (i=0; i<clusterNum; i++) {
            //freqToSet = (pFreqTbl[i] <= 0) ? PPM_IGNORE : (pFreqTbl[i] > ptClusterTbl[i].freqMax) ? ptClusterTbl[i].freqMax : pFreqTbl[i];
            freqToSet = (pFreqTbl[i] <= 0) ? PPM_IGNORE : pFreqTbl[i];
            maxToSet = (pMaxFreqTbl[i] <= 0 || pMaxFreqTbl[i] >= ptClusterTbl[i].freqMax) ? PPM_IGNORE : pMaxFreqTbl[i];
            sprintf(buf, "%d %d ", freqToSet, maxToSet);
            /*strcat(str, buf);*/
            strncat(str, buf, strlen(buf));
        }
        str[strlen(str)-1] = '\0'; // remove last space
        set_value(PATH_PERFMGR_FREQ_CTRL, str);
        ALOGI("%d: legacy set freq: %s", scenario, str);
    }
    else if (gtDrvInfo.ppmAll) {
        str[0] = '\0';
        for (i=0; i<clusterNum; i++) {
            //freqToSet = (pFreqTbl[i] <= 0) ? PPM_IGNORE : (pFreqTbl[i] > ptClusterTbl[i].freqMax) ? ptClusterTbl[i].freqMax : pFreqTbl[i];
            freqToSet = (pFreqTbl[i] <= 0) ? PPM_IGNORE : pFreqTbl[i];
            maxToSet = (pMaxFreqTbl[i] <= 0 || pMaxFreqTbl[i] >= ptClusterTbl[i].freqMax) ? PPM_IGNORE : pMaxFreqTbl[i];
            sprintf(buf, "%d %d ", freqToSet, maxToSet);
            /*strcat(str, buf);*/
            strncat(str, buf, strlen(buf));
        }
        str[strlen(str)-1] = '\0'; // remove last space
        set_value(PATH_PPM_FREQ_CTRL, str);
        ALOGI("%d: ppmall set freq: %s", scenario, str);
    }
    else if (gtDrvInfo.ppmSupport) {
        str[0] = '\0';
        for (i=0; i<clusterNum; i++) {
            //freqToSet = (pFreqTbl[i] <= 0) ? PPM_IGNORE : (pFreqTbl[i] > ptClusterTbl[i].freqMax) ? ptClusterTbl[i].freqMax : pFreqTbl[i];
            freqToSet = (pFreqTbl[i] <= 0) ? PPM_IGNORE : pFreqTbl[i];
            maxToSet = (pMaxFreqTbl[i] <= 0 || pMaxFreqTbl[i] >= ptClusterTbl[i].freqMax) ? PPM_IGNORE : pMaxFreqTbl[i];
            sprintf(buf, "%d %d ", freqToSet, maxToSet);
            /*strcat(str, buf);*/
            strncat(str, buf, strlen(buf));
            set_value(PATH_PPM_FREQ_BASE, i, freqToSet);
            set_value(PATH_PPM_FREQ_LIMIT, i, maxToSet);
        }
        str[strlen(str)-1] = '\0'; // remove last space
        ALOGI("%d: ppmsupport set freq: %s", scenario, str);
    }
    else {
        freqToSet = (pFreqTbl[0] <= 0) ? 0 : ((pFreqTbl[0] > ptClusterTbl[0].freqMax) ? ptClusterTbl[0].freqMax : pFreqTbl[0]);
        maxToSet = (pMaxFreqTbl[0] <= 0 || pMaxFreqTbl[0] >= ptClusterTbl[0].freqMax) ? 0 : pMaxFreqTbl[0];
        if (gtDrvInfo.hmp) {
            freqToSetBig = (pFreqTbl[1] <= 0) ? 0 : ((pFreqTbl[1] > ptClusterTbl[1].freqMax) ? ptClusterTbl[1].freqMax : pFreqTbl[1]);
            maxToSetBig = (pMaxFreqTbl[1] <= 0 || pMaxFreqTbl[1] >= ptClusterTbl[1].freqMax) ? 0 : pMaxFreqTbl[1];
            set_value(PATH_CPUFREQ_LIMIT, freqToSet);
            set_value(PATH_CPUFREQ_BIG_LIMIT, freqToSetBig);
            set_value(PATH_CPUFREQ_MAX_FREQ, maxToSet);
            set_value(PATH_CPUFREQ_MAX_FREQ_BIG, maxToSetBig);
            ALOGI("%d: set freq: %d, %d", scenario, freqToSet, freqToSetBig);
            ALOGI("%d: set freq max: %d, %d", scenario, maxToSet, maxToSetBig);
        }
        else if (gtDrvInfo.dvfsHevc) {
            set_value(PATH_CPUFREQ_LIMIT, freqToSet);
            set_value(PATH_CPUFREQ_MAX_FREQ, maxToSet);
            ALOGI("%d: set freq: %d, freq max:%d", scenario, freqToSet, maxToSet);
        }
        else {
            maxToSet = (pMaxFreqTbl[0] <= 0 || pMaxFreqTbl[0] >= ptClusterTbl[0].freqMax) ? ptClusterTbl[0].freqMax : pMaxFreqTbl[0];
            set_value(PATH_CPUFREQ_MIN_FREQ_CPU0, freqToSet);
            set_value(PATH_CPUFREQ_MAX_FREQ_CPU0, maxToSet);
            ALOGI("%d: set freq: %d, freq max:%d", scenario, freqToSet, maxToSet);
	    }
    }
}

void setVcore(int scenario, int level)
{
    if (gtDrvInfo.fliper) {
        ALOGD("%d: set vcore level: %d", scenario, level);
        set_vcore_level(level); // 0: default mode, 1: low power mode, 2: just make mode, 3: performance mode
   }
}

void setWiphyCAM(int scenario, int level)
{
    ALOGI("%d: set wiphy CAM level: %d", scenario, level);
    set_wiphy_cam_level(level); // 0: default mode, 1: disable power save (for wiphy performance)
}

void setFstbFps(int scenario, int fps_high, int fps_low)
{
    char fstb_fps[FSTB_FPS_LEN] = "";
    if (gtDrvInfo.fstb) {
        if (fps_high == -1 && fps_low == -1) {
            ALOGV("%d: set fstb_fps: %s", scenario, pFstbDefaultFps);
            set_value(PATH_FSTB_FPS, pFstbDefaultFps);
        } else {
            sprintf(fstb_fps, "1 %d-%d", fps_high, fps_low);
            ALOGV("%d: set fstb_fps: %s", scenario, fstb_fps);
            set_value(PATH_FSTB_FPS, fstb_fps);
        }
   }
}

void setGpuFreq(int scenario, int level)
{
    int levelToSet = 0;
    static int nSetFreqInit = 0;
    static int nIsGpuFreqSupport = 0;
    struct stat stat_buf;

    if(!nSetFreqInit) {
        nIsGpuFreqSupport = (0 == stat(PATH_GPUFREQ_COUNT, &stat_buf)) ? 1 : 0;
        nSetFreqInit = 1;
    }

    if(!nIsGpuFreqSupport)
        return;

    if(level > levelToSet) levelToSet = level;
    ALOGI("%d: set gpu level: %d", scenario, levelToSet);
    set_gpu_freq_level(levelToSet); // 0 means disable
}

void setGpuFreqMax(int scenario, int level)
{
    int levelToSet = nGpuHighestFreqLevel;

    if(level < levelToSet) levelToSet = level;
    ALOGI("%d: set gpu level max: %d", scenario, levelToSet);
    set_gpu_freq_level_max(levelToSet); // 0 means disable
}

void setPerfIdx(int scenario, int index)
{
    if (gtDrvInfo.ppmSupport) {
        ALOGI("%d: set perf index: %d", scenario, index);
        set_value(PATH_PPM_PERF_IDX, index);
   }
}

void checkDrvSupport(tDrvInfo *ptDrvInfo)
{
    struct stat stat_buf;
    int ppmCore;

    ptDrvInfo->perfmgrLegacy = (0 == stat(PATH_PERFMGR_FREQ_CTRL, &stat_buf)) ? 1 : 0;
    ptDrvInfo->ppmSupport = (0 == stat(PATH_PPM_FREQ_LIMIT, &stat_buf)) ? 1 : 0;
    ptDrvInfo->ppmAll = (0 == stat(PATH_PPM_FREQ_CTRL, &stat_buf)) ? 1 : 0;
    ptDrvInfo->hmp = (get_int_value(PATH_CPUTOPO_CHECK_HMP)==1) ? 1 : 0;
    ptDrvInfo->hps = (0 == stat(PATH_CPUHOTPLUG_HPS_MIN, &stat_buf)) ? 1 : 0;
    if(ptDrvInfo->hps == 0) {
        ALOGI("checkDrvSupport hps failed: %s\n", strerror(errno));
    }
    ptDrvInfo->dvfsHevc = (0 == stat(PATH_CPUFREQ_LIMIT, &stat_buf)) ? 1 : 0;
    if(ptDrvInfo->dvfsHevc == 0) {
        ALOGI("checkDrvSupport cpufreq failed: %s\n", strerror(errno));
    }
    ptDrvInfo->fliper = (0 == stat(PATH_VCORE, &stat_buf)) ? 1 : 0;
    ptDrvInfo->sysfs = (0 == stat(PATH_CPU_CPUFREQ, &stat_buf)) ? 1 : 0;
    ptDrvInfo->dvfs = (0 == stat(PATH_CPUFREQ_ROOT, &stat_buf)) ? 1 : 0;
    ptDrvInfo->fstb = (0 == stat(PATH_FSTB_FPS, &stat_buf)) ? 1 : 0;

    ppmCore = (0 == stat(PATH_PPM_CORE_LIMIT, &stat_buf)) ? 1 : 0;
    if(ptDrvInfo->ppmSupport)
        ptDrvInfo->acao = (ppmCore) ? 0 : 1; // PPM not support core => ACAO
    else
        ptDrvInfo->acao = 0; // no PPM => no ACAO

    ptDrvInfo->turbo = (get_int_value(PATH_TURBO_SUPPORT)==1) ? 1 : 0;

    ALOGI("checkDrvSupport - perfmgr:%d, ppm:%d, ppmAll:%d, acao:%d, hmp:%d, hps:%d, hevc:%d, fliper:%d, sysfs:%d, dvfs:%d, turbo:%d, fstb:%d",
        ptDrvInfo->perfmgrLegacy, ptDrvInfo->ppmSupport, ptDrvInfo->ppmAll,ptDrvInfo->acao, ptDrvInfo->hmp,
        ptDrvInfo->hps, ptDrvInfo->dvfsHevc, ptDrvInfo->fliper, ptDrvInfo->sysfs, ptDrvInfo->dvfs, ptDrvInfo->turbo, ptDrvInfo->fstb);
}

void getCputopoInfo(int isHmpSupport, int *pnCpuNum, int *pnClusterNum, tClusterInfo **pptClusterTbl)
{
    int i, j;
    int cpu_num[CLUSTER_MAX], cpu_index[CLUSTER_MAX];
    int cputopoClusterNum;

    *pnCpuNum = get_cpu_num();
    cputopoClusterNum = get_int_value(PATH_CPUTOPO_NR_CLUSTER);
    *pnClusterNum = (isHmpSupport == 0) ? 1 : cputopoClusterNum;
    ALOGI("getCputopoInfo - cpuNum:%d, cluster:%d, cputopoCluster:%d", *pnCpuNum, *pnClusterNum, cputopoClusterNum);

    if((*pnClusterNum) < 0 || (*pnClusterNum) > CLUSTER_MAX) {
        ALOGE("wrong cluster number:%d", *pnClusterNum);
        return;
    }

    *pptClusterTbl = (tClusterInfo*)malloc(sizeof(tClusterInfo) * (*pnClusterNum));
    if (*pptClusterTbl  == NULL) {
        ALOGE("Can't allocate memory for pptClusterTbl");
        return;
    }

    get_cputopo_cpu_info(*pnClusterNum, cpu_num, cpu_index);

    for (i=0; i<*pnClusterNum; i++) {
        (*pptClusterTbl)[i].cpuNum = cpu_num[i];
        (*pptClusterTbl)[i].cpuFirstIndex = cpu_index[i];
        (*pptClusterTbl)[i].cpuMinNow = 0;
        (*pptClusterTbl)[i].cpuMaxNow = cpu_num[i];
        (*pptClusterTbl)[i].cpuHardMinNow = 0;
        if (gtDrvInfo.ppmSupport)
            get_ppm_cpu_freq_info(i, &((*pptClusterTbl)[i].freqMax),&((*pptClusterTbl)[i].freqCount), &((*pptClusterTbl)[i].pFreqTbl));
        else
            get_cpu_freq_info(0, &((*pptClusterTbl)[i].freqMax), &((*pptClusterTbl)[i].freqCount), &((*pptClusterTbl)[i].pFreqTbl));
        (*pptClusterTbl)[i].freqMinNow = 0;
        (*pptClusterTbl)[i].freqMaxNow = (*pptClusterTbl)[i].freqMax;
        ALOGI("[cluster %d]: cpu:%d, first:%d, freq count:%d, max_freq:%d", i, (*pptClusterTbl)[i].cpuNum, (*pptClusterTbl)[i].cpuFirstIndex, (*pptClusterTbl)[i].freqCount, (*pptClusterTbl)[i].freqMax);
        for (j=0; j<(*pptClusterTbl)[i].freqCount; j++)
            ALOGI("  %d: %d", j, (*pptClusterTbl)[i].pFreqTbl[j]);
    }

    /* special case for Denali-3: 2 clusters but SMP */
    if(gtDrvInfo.hmp == 0 && cputopoClusterNum > 1) {
        (*pptClusterTbl)[0].cpuNum = *pnCpuNum;
        ALOGI("[cluster 0]: cpu:%d",(*pptClusterTbl)[0].cpuNum);
        (*pptClusterTbl)[0].cpuMaxNow = *pnCpuNum;
        ALOGI("[cluster 0]: cpu_max_now:%d",(*pptClusterTbl)[0].cpuMaxNow);
    }
}

void getPpmPerfIndexInfo(int *pPerfIdxMin, int *pPerfIdxMax)
{
    *pPerfIdxMin = get_int_value(PATH_PPM_PERF_IDX_MIN);
    *pPerfIdxMax = get_int_value(PATH_PPM_PERF_IDX_MAX);
}

inline int checkSuccess(int scenario)
{
    return (((scenario < (int)MtkPowerHint::MTK_POWER_HINT_NUM || scenario >= (int)MtkPowerHint::MTK_POWER_HINT_NUM + REG_SCN_MAX) && (scenario > 0)) && ptScnList[scenario].scn_valid) || (scenario >= nUserScnBase && scenario < nUserScnBase + REG_SCN_MAX);
}

void dump_white_list(void)
{
    tScnNode *pPackList;
    int i;

    pPackList = ptScnList + SCN_APP_RUN_BASE;
    ALOGI("======== white list ========");
    ALOGI("nPackNum:%d", nPackNum);
    ALOGI("======== white list ========");
    for(i=0; i<nPackNum; i++) {
        //if(pPackList[i].scn_valid == 1)
            ALOGI("pack:%s, valid:%d", pPackList[i].pack_name,pPackList[i].scn_valid);
    }
}

void getCpusetArgv(int mask, char argv[])
{
    int i=0, last_begin = -1, last_set = -1;
    char buf[32];
    argv[0] = '\0';
    ALOGV("[getCpusetArgv] nCpuNum:%d, mask:%x", nCpuNum, mask);

    while(i<=nCpuNum) {
        if(mask & (1<<i)) {  // bit i is set
            //ALOGI("[getCpusetArgv] bit[%d] is 1, last_begin:%d, last_set:%d", i, last_begin, last_set);
            if(last_begin == -1) {
                last_begin = i;
                last_set = i;
                sprintf(buf, "%d", i);
                /*strcat(argv, buf);*/
                strncat(argv, buf, strlen(buf));
            }
            else if(last_set == i-1) {
                last_set = i;
            }
            else if(last_set < i-1) {
                last_begin = i;
                last_set = i;
                sprintf(buf, ",%d", i);
                /*strcat(argv, buf);*/
                strncat(argv, buf, strlen(buf));
            }
        }
        else { // bit i is 0
            ALOGV("[getCpusetArgv] bit[%d] is 0, last_begin:%d, last_set:%d", i, last_begin, last_set);
            if(last_set!=-1 && last_set!=last_begin && last_set==i-1) {
                sprintf(buf, "-%d", last_set);
                /*strcat(argv, buf);*/
                strncat(argv, buf, strlen(buf));
            }
        }
        i++;
    }
    ALOGI("[getCpusetArgv] argv:%s", argv);
}

int getFpsPolicyMode(int mode)
{
    int policyMode;

    switch(mode) {
    case (int)MtkDfpsMode::DFPS_MODE_DEFAULT:
        policyMode = FpsPolicy::MODE_DEFAULT;
        break;

    case (int)MtkDfpsMode::DFPS_MODE_FRR:
        policyMode = FpsPolicy::MODE_FRR;
        break;

    case (int)MtkDfpsMode::DFPS_MODE_ARR:
        policyMode = FpsPolicy::MODE_FRR;
        break;

    case (int)MtkDfpsMode::DFPS_MODE_INTERNAL_SW:
        policyMode = FpsPolicy::MODE_INTERNAL_SW;
        break;

    default:
        policyMode = FpsPolicy::MODE_DEFAULT;
        break;
    }
    return policyMode;
}

int perfScnEnable(int scenario)
{
    int needUpdateCores = 0, needUpdateCoresMax = 0, needUpdateFreq = 0, needUpdateFreqMax = 0, i = 0, perfdUpdate = 0;
    signed long ret;
    int scn_core_min[CLUSTER_MAX], actual_core_min[CLUSTER_MAX], totalCore, coreToSet;
    int scn_core_max[CLUSTER_MAX];
    int scn_core_hard_min[CLUSTER_MAX];
    int scn_freq_min[CLUSTER_MAX];
    int scn_freq_max[CLUSTER_MAX];
    int cpusetToSet;
    char cpusetMask[64];
    int needUpdateWiphyCAM = 0;
    //ALOGI("[perfScnEnable] scn:%d, scn_cores_now:%d, scn_cores_big_now:%d", scenario, scn_cores_now, scn_cores_big_now);
    //ALOGI("[perfScnEnable] scn:%d, scn_cores_little_max_now:%d, scn_cores_big_max_now:%d", scenario, scn_cores_little_max_now, scn_cores_big_max_now);

    if (checkSuccess(scenario)) {
        if (STATE_ON == ptScnList[scenario].scn_state)
            return 0;
        ALOGD("[perfScnEnable] scn:%d", scenario);

        ptScnList[scenario].scn_state = STATE_ON;
        //perfUpdateScnInfo(scenario, STATE_ON);

        ALOGV("[perfScnEnable] scn:%d, scn_cores_now:%d, scn_core_total:%d",  scenario, scn_cores_now, ptScnList[scenario].scn_core_total);
        if (scn_cores_now < ptScnList[scenario].scn_core_total) {
            scn_cores_now = ptScnList[scenario].scn_core_total;
            needUpdateCores = 1;
        }

        for (i=0; i<nClusterNum; i++) {
            ALOGV("[perfScnEnable] scn:%d, i:%d, cpuMinNow:%d, scn_core_min:%d",  scenario, i, ptClusterTbl[i].cpuMinNow, ptScnList[scenario].scn_core_min[i]);
            if (ptClusterTbl[i].cpuMinNow < ptScnList[scenario].scn_core_min[i]) {
                ptClusterTbl[i].cpuMinNow = ptScnList[scenario].scn_core_min[i];
                needUpdateCores = 1;
            }
            scn_core_min[i] = ptClusterTbl[i].cpuMinNow;

            ALOGV("[perfScnEnable] scn:%d, i:%d, cpuMaxNow:%d, scn_core_max:%d", scenario, i, ptClusterTbl[i].cpuMaxNow, ptScnList[scenario].scn_core_max[i]);
            if (nSetCoreMax && (ptClusterTbl[i].cpuMaxNow > ptScnList[scenario].scn_core_max[i] || ptClusterTbl[i].cpuMaxNow == PPM_IGNORE)) {
                ptClusterTbl[i].cpuMaxNow = ptScnList[scenario].scn_core_max[i];
                needUpdateCoresMax = 1;
            }

            ALOGV("[perfScnEnable] scn:%d, i:%d, cpuHardMinNow:%d, scn_core_hard_min:%d",  scenario, i, ptClusterTbl[i].cpuHardMinNow, ptScnList[scenario].scn_core_hard_min[i]);
            if (ptClusterTbl[i].cpuHardMinNow < ptScnList[scenario].scn_core_hard_min[i]) {
                ptClusterTbl[i].cpuHardMinNow = ptScnList[scenario].scn_core_hard_min[i];
                needUpdateCores = 1;
            }
            scn_core_hard_min[i] = ptClusterTbl[i].cpuHardMinNow;

            ALOGV("[perfScnEnable] scn:%d, i:%d, freqMinNow:%d, scn_freq_min:%d",  scenario, i, ptClusterTbl[i].freqMinNow, ptScnList[scenario].scn_freq_min[i]);
            if (ptClusterTbl[i].freqMinNow < ptScnList[scenario].scn_freq_min[i]) {
                ptClusterTbl[i].freqMinNow = ptScnList[scenario].scn_freq_min[i];
                needUpdateFreq = 1;
            }
            scn_freq_min[i] = ptClusterTbl[i].freqMinNow;

            ALOGV("[perfScnEnable] scn:%d, i:%d, freqMaxNow:%d, scn_freq_max:%d",  scenario, i, ptClusterTbl[i].freqMaxNow, ptScnList[scenario].scn_freq_max[i]);
            if (nSetFreqMax && ptClusterTbl[i].freqMaxNow > ptScnList[scenario].scn_freq_max[i]) {
                ptClusterTbl[i].freqMaxNow = ptScnList[scenario].scn_freq_max[i];
                needUpdateFreqMax = 1;
            }
            if (ptClusterTbl[i].freqMaxNow < scn_freq_min[i]) { // if max < min => align max with min
                ptClusterTbl[i].freqMaxNow = scn_freq_min[i];
                needUpdateFreqMax = 1;
            }
            scn_freq_max[i] = ptClusterTbl[i].freqMaxNow;

        }

        // check perf index
        if (ptScnList[scenario].scn_perf_idx > scn_perf_idx_now) {
            scn_perf_idx_now = ptScnList[scenario].scn_perf_idx;
            setPerfIdx(scenario, scn_perf_idx_now);
        }

        // check vcore
        //ALOGI("enable: scn_vcore_now=%d,scn_vcore=%d",scn_vcore_now,ptScnList[scenario].scn_vcore);
        if (scn_vcore_now < ptScnList[scenario].scn_vcore){
            scn_vcore_now = ptScnList[scenario].scn_vcore;
            setVcore(scenario, scn_vcore_now);
        }

        // check wiphy_cam
        if(nSetWiphyCAM) {
            if (scn_wiphy_cam_now < ptScnList[scenario].scn_wiphy_cam) {
                scn_wiphy_cam_now = ptScnList[scenario].scn_wiphy_cam;
                needUpdateWiphyCAM = 1;
            }
        }

        // check gpu
        if (scn_gpu_freq_now < ptScnList[scenario].scn_gpu_freq) {
            scn_gpu_freq_now = ptScnList[scenario].scn_gpu_freq;
             setGpuFreq(scenario, scn_gpu_freq_now);

        }

        // check freq max
        if (nSetGpuFreqMax) {
            if(scn_gpu_freq_max_now > ptScnList[scenario].scn_gpu_freq_max) {
                scn_gpu_freq_max_now = ptScnList[scenario].scn_gpu_freq_max;
                setGpuFreqMax(scenario, scn_gpu_freq_max_now);
            }
        }

        // fine tune max
        totalCore = scn_cores_now;
        for (i=nClusterNum-1; i>=0; i--) {
            coreToSet = (scn_core_min[i] <= 0 || scn_core_min[i] > ptClusterTbl[i].cpuNum || totalCore <= 0) ? PPM_IGNORE : ((scn_core_min[i] > totalCore) ? totalCore : scn_core_min[i]);
            if(coreToSet >= 0)
                totalCore -= coreToSet;
            actual_core_min[i] = max(coreToSet, scn_core_hard_min[i]);

            if (ptClusterTbl[i].cpuMaxNow < actual_core_min[i]) { // min priority is higher than max
                ptClusterTbl[i].cpuMaxNow = actual_core_min[i];
                needUpdateCoresMax = 1;
            }
            scn_core_max[i] = ptClusterTbl[i].cpuMaxNow;
        }

        // L and LL: only one cluster can set max cpu = 0
        if (nClusterNum > 1 && ptClusterTbl[1].cpuMaxNow == 0 && ptClusterTbl[0].cpuMaxNow == 0) {
            ptClusterTbl[0].cpuMaxNow = scn_core_max[0] = PPM_IGNORE;
            needUpdateCoresMax = 1;
        }

        if (needUpdateFreq || needUpdateFreqMax) {
            setClusterFreq(scenario, nClusterNum, scn_freq_min, scn_freq_max);
        }

        if (needUpdateCoresMax || needUpdateCores) {
            setClusterCores(scenario, nClusterNum, scn_cores_now, actual_core_min, scn_core_max);
        }

        if (needUpdateWiphyCAM)
            setWiphyCAM(scenario, scn_wiphy_cam_now);

        /* CPUSET */
        if(ptScnList[scenario].scn_cpuset < nCpuSet) {
            ALOGI("[perfScnEnable] scn:%d, cpuset:%x, scn_cpuset:%x", scenario, scn_cpuset_now, ptScnList[scenario].scn_cpuset);
            cpusetToSet = scn_cpuset_now & ptScnList[scenario].scn_cpuset;
            if(cpusetToSet != scn_cpuset_now) {
                scn_cpuset_now = (cpusetToSet == 0) ? 1 : cpusetToSet; // at least 1LL
                getCpusetArgv(scn_cpuset_now, cpusetMask);
                set_value(PATH_GLOBAL_CPUSET_EN, 1);
                set_value(PATH_GLOBAL_CPUSET, cpusetMask);
            }
        }

        /* FPS policy is only used in white list */
        if (ptScnList[scenario].dfps_mode != (int)MtkDfpsMode::DFPS_MODE_DEFAULT && ptScnList[scenario].dfps != -1) {
            ALOGI("[perfScnEnable] scn:%d, dfps mode:%d, fps:%d", scenario, ptScnList[scenario].dfps_mode, ptScnList[scenario].dfps);
            if(mFpsPolicy != NULL) {
                int mode = getFpsPolicyMode(ptScnList[scenario].dfps_mode);
                mFpsPolicy->setFps(ptScnList[scenario].dfps, mode);
            }
        }

        /* DISP DeCouple is only used in white list */
        if (ptScnList[scenario].disp_mode != (int)MtkDispMode::DISP_MODE_DEFAULT) {
            ALOGI("[perfScnEnable] scn:%d, disp mode:%d", scenario, ptScnList[scenario].disp_mode);
            set_disp_ctl(ptScnList[scenario].disp_mode);
        }

        if (ptScnList[scenario].scn_fstb_fps_high != -1 && ptScnList[scenario].scn_fstb_fps_low != -1) {
            setFstbFps(scenario, ptScnList[scenario].scn_fstb_fps_high, ptScnList[scenario].scn_fstb_fps_low);
            ALOGV("[perfScnEnable] scn:%d, fstb_fps:1 %d-%d", scenario, ptScnList[scenario].scn_fstb_fps_high, ptScnList[scenario].scn_fstb_fps_low);
        }

        /* video mode is only used in white list */
        if (ptScnList[scenario].video_mode != 0) {
            ALOGI("[perfScnEnable] scn:%d, video mode:%d", scenario, ptScnList[scenario].video_mode);
            if(mFpsPolicy != NULL) {
                mFpsPolicy->setVideoMode(true);
            }
        }

        /*
            scan control table(perfcontable.txt) and judge which setting of scene is beeter
            and then replace it.
            less is meaning system setting less than current scene value is better
            more is meaning system setting more than current scene value is better
        */
        for(int idx = 0; idx < FIELD_SIZE; idx++) {
            if(tConTable[idx].entry.length() == 0)
                break;

            if(tConTable[idx].isValid == -1)
                continue;

            ALOGV("[perfScnEnable] scn:%d, cmd:%d, cur:%d, param:%d", scenario, tConTable[idx].cmdID, tConTable[idx].curVal, ptScnList[scenario].scn_param[idx]);
            if(tConTable[idx].comp.compare(LESS) == 0 ) {
                if(ptScnList[scenario].scn_param[idx] < tConTable[idx].curVal )
                {
                    tConTable[idx].curVal = ptScnList[scenario].scn_param[idx];
                    set_value(tConTable[idx].entry.c_str(),
                              ptScnList[scenario].scn_param[idx]);
                }
            }
            else {
                if(ptScnList[scenario].scn_param[idx] > tConTable[idx].curVal)
                {
                    tConTable[idx].curVal = ptScnList[scenario].scn_param[idx];
                    set_value(tConTable[idx].entry.c_str(),
                              ptScnList[scenario].scn_param[idx]);
                }
            }
        }

        // ppm mode
        if(ptScnList[scenario].scn_ppm_mode != PPM_IGNORE) {
            set_value(PATH_PPM_MODE, tPpmMode[ptScnList[scenario].scn_ppm_mode]);
            nPpmCurrMode = ptScnList[scenario].scn_ppm_mode;
            ALOGI("[perfScnEnable] scn:%d, ppm mode:%d", scenario, ptScnList[scenario].scn_ppm_mode);
        }
        else if(nPpmCurrMode != PPM_IGNORE && needUpdateCores && scn_core_min[2] > 0) { // somebody enable big
            set_value(PATH_PPM_MODE, pPpmDefaultMode);
            ALOGI("[perfScnEnable] scn:%d, ppm mode:%s", scenario, pPpmDefaultMode);
        }
    }

    //ALOGI("[perfScnEnable] scenario:%d, max_freq:%d, %d", scenario, ptClusterTbl[0].freqMax, ptClusterTbl[1].freqMax);
    return 0;
}

int perfScnDisable(int scenario)
{
    int needUpdateCores = 0, needUpdateCoresMax = 0, wiphyCAMToSet = 0, perfdUpdate = 0;
    int totalCoresToSet = 0, VcoreToSet = 0, gpuFreqToSet = 0, gpuFreqMaxToSet = nGpuHighestFreqLevel;
    int perfIdxToSet = 0;
    int needUpdate = 0;
    int coresToSet[CLUSTER_MAX], actual_core_min[CLUSTER_MAX], maxCoresToSet[CLUSTER_MAX], hardCoresToSet[CLUSTER_MAX], lastCore[CLUSTER_MAX];
    int freqToSet[CLUSTER_MAX], lastFreq[CLUSTER_MAX], maxFreqToSet[CLUSTER_MAX];
    int totalCore, coreToSet, numToSet, cpusetToSet;
    char cpusetMask[64];
    int i, j;
    long ret;

    //ALOGI("[perfScnDisable] scn:%d, scn_cores_now:%d, scn_cores_big_now:%d", scenario, scn_cores_now, scn_cores_big_now);

    if (checkSuccess(scenario)) {
        if (STATE_OFF == ptScnList[scenario].scn_state)
            return 0;
        ALOGD("[perfScnDisable] scn:%d", scenario);

        ptScnList[scenario].scn_state = STATE_OFF;
        //perfUpdateScnInfo(scenario, STATE_OFF);

        // check core
        ALOGV("[perfScnDisable] scenario:%d, scn_cores_now:%d, scn_core_total:%d", scenario, scn_cores_now, ptScnList[scenario].scn_core_total);

        needUpdateCores = 0;
        if (scn_cores_now <= ptScnList[scenario].scn_core_total) {
            for (i = 0; i < SCN_APP_RUN_BASE + nPackNum; i++) {
                if (ptScnList[i].scn_state == STATE_ON)
                    totalCoresToSet = max(totalCoresToSet, ptScnList[i].scn_core_total);
            }

            if (scn_cores_now != totalCoresToSet) {
                ALOGV("[perfScnDisable] scn_cores_now:%d, totalCoresToSet:%d", scn_cores_now, totalCoresToSet);
                scn_cores_now = totalCoresToSet;
                needUpdateCores = 1;
            }
        }
        else {
            totalCoresToSet = scn_cores_now;
        }

        for (i=0; i<nClusterNum; i++) {
            ALOGV("[perfScnDisable] i:%d, cpuMinNow:%d, scn_core_min:%d", i, ptClusterTbl[i].cpuMinNow, ptScnList[scenario].scn_core_min[i]);
            lastCore[i] = ptClusterTbl[i].cpuMinNow;
            if (ptClusterTbl[i].cpuMinNow <= ptScnList[scenario].scn_core_min[i]) {
                coresToSet[i] = 0;
                for (j = 0; j < SCN_APP_RUN_BASE + nPackNum; j++) {
                    if (ptScnList[j].scn_state == STATE_ON) {
                        //ALOGI("[perfScnDisable] i:%d, j:%d, scn_core_min:%d", i, j, ptScnList[j].scn_core_min[i]);
                        coresToSet[i] = max(coresToSet[i], ptScnList[j].scn_core_min[i]);
                    }
                }
                if(coresToSet[i] != ptClusterTbl[i].cpuMinNow) {
                    ALOGV("[perfScnDisable] i:%d, cpuMinNow:%d, coresToSet:%d", i, ptClusterTbl[i].cpuMinNow, coresToSet[i]);
                    ptClusterTbl[i].cpuMinNow = coresToSet[i];
                    needUpdateCores = 1;
                }
            }
            else {
                coresToSet[i] = ptClusterTbl[i].cpuMinNow;
            }

            ALOGV("[perfScnDisable] i:%d, cpuHardMinNow:%d, scn_core_hard_min:%d", i, ptClusterTbl[i].cpuHardMinNow, ptScnList[scenario].scn_core_hard_min[i]);
            if (ptClusterTbl[i].cpuHardMinNow <= ptScnList[scenario].scn_core_hard_min[i]) {
                hardCoresToSet[i] = 0;
                for (j = 0; j < SCN_APP_RUN_BASE + nPackNum; j++) {
                    if (ptScnList[j].scn_state == STATE_ON) {
                        //ALOGI("[perfScnDisable] i:%d, j:%d, scn_core_min:%d", i, j, ptScnList[j].scn_core_min[i]);
                        hardCoresToSet[i] = max(hardCoresToSet[i], ptScnList[j].scn_core_hard_min[i]);
                    }
                }
                if(hardCoresToSet[i] != ptClusterTbl[i].cpuHardMinNow) {
                    ALOGV("[perfScnDisable] i:%d, cpuHardMinNow:%d, hardCoresToSet:%d", i, ptClusterTbl[i].cpuHardMinNow, hardCoresToSet[i]);
                    ptClusterTbl[i].cpuHardMinNow = hardCoresToSet[i];
                    needUpdateCores = 1;
                }
            }
            else {
                hardCoresToSet[i] = ptClusterTbl[i].cpuHardMinNow;
            }
        }

        needUpdateCoresMax = 0;
        for (i=0; nSetCoreMax && i<nClusterNum; i++) {
            ALOGV("[perfScnDisable] scn:%d, i:%d, cpuMaxNow:%d, scn_core_max:%d",  scenario, i, ptClusterTbl[i].cpuMaxNow, ptScnList[scenario].scn_core_max[i]);
            if (ptClusterTbl[i].cpuMaxNow >= ptScnList[scenario].scn_core_max[i] || ptClusterTbl[i].cpuMaxNow == lastCore[i]) {
                maxCoresToSet[i] = CORE_MAX;
                for (j = 0; j < SCN_APP_RUN_BASE + nPackNum; j++) {
                    if (ptScnList[j].scn_state == STATE_ON)
                        maxCoresToSet[i] = min(maxCoresToSet[i], ptScnList[j].scn_core_max[i]);
                }
                if(maxCoresToSet[i] != ptClusterTbl[i].cpuMaxNow) {
                    ptClusterTbl[i].cpuMaxNow = maxCoresToSet[i];
                    needUpdateCoresMax = 1;
                }
            }
        }

        // fine tune max
        totalCore = scn_cores_now;
        for (i=nClusterNum-1; i>=0; i--) {
            coreToSet = (coresToSet[i] <= 0 || coresToSet[i] > ptClusterTbl[i].cpuNum || totalCore <= 0) ? PPM_IGNORE : ((coresToSet[i] > totalCore) ? totalCore : coresToSet[i]);
            if(coreToSet >= 0)
                totalCore -= coreToSet;
            actual_core_min[i] = max(coreToSet, hardCoresToSet[i]);;

            if (ptClusterTbl[i].cpuMaxNow < actual_core_min[i]) { // min priority is higher than max
                ptClusterTbl[i].cpuMaxNow = actual_core_min[i];
                needUpdateCoresMax = 1;
            }
            maxCoresToSet[i] = ptClusterTbl[i].cpuMaxNow;
        }

        /* update core max */
        if(needUpdateCoresMax || needUpdateCores) {
            setClusterCores(scenario, nClusterNum, scn_cores_now, actual_core_min, maxCoresToSet);
        }

        needUpdate = 0;
        for (i=0; i<nClusterNum; i++) {
            lastFreq[i] = ptClusterTbl[i].freqMinNow;
            maxFreqToSet[i] = ptClusterTbl[i].freqMaxNow; // initial value
            if (ptClusterTbl[i].freqMinNow <= ptScnList[scenario].scn_freq_min[i]) {
                freqToSet[i] = 0;
                for (j = 0; j < SCN_APP_RUN_BASE + nPackNum; j++) {
                    if (ptScnList[j].scn_state == STATE_ON)
                        freqToSet[i] = max(freqToSet[i], ptScnList[j].scn_freq_min[i]);
                }
                if(freqToSet[i] != ptClusterTbl[i].freqMinNow) {
                    ptClusterTbl[i].freqMinNow = freqToSet[i];
                    needUpdate = 1;
                }
            }
            else {
                freqToSet[i] = ptClusterTbl[i].freqMinNow;
            }
        }

        for (i=0; nSetFreqMax && i<nClusterNum; i++) {
            ALOGV("[perfScnDisable] scn:%d, i:%d, global_min:%d, global_max:%d, max:%d",  scenario, i, lastFreq[i], ptClusterTbl[i].freqMaxNow, ptScnList[scenario].scn_freq_max[i]);
            if (ptClusterTbl[i].freqMaxNow >= ptScnList[scenario].scn_freq_max[i] || \
                ptClusterTbl[i].freqMaxNow == lastFreq[i]) { // perfservice might ignore someone's setting before
                maxFreqToSet[i] = FREQ_MAX;
                for (j = 0; j < SCN_APP_RUN_BASE + nPackNum; j++) {
                    if (ptScnList[j].scn_state == STATE_ON)
                        maxFreqToSet[i] = min(maxFreqToSet[i], ptScnList[j].scn_freq_max[i]);
                }
                if(maxFreqToSet[i] < freqToSet[i]) { // if max < min => align max with min
                    maxFreqToSet[i] = freqToSet[i];
                }
                if(maxFreqToSet[i] != ptClusterTbl[i].freqMaxNow) {
                    ptClusterTbl[i].freqMaxNow = maxFreqToSet[i];
                    needUpdate = 1;
                }
            }
            else {
                maxFreqToSet[i] = ptClusterTbl[i].freqMaxNow;
            }
        }
        if(needUpdate) {
            setClusterFreq(scenario, nClusterNum, freqToSet, maxFreqToSet);
        }

        // perf index
        if (ptScnList[scenario].scn_perf_idx > 0 && ptScnList[scenario].scn_perf_idx >= scn_perf_idx_now) {
            for (i = 0; i < SCN_APP_RUN_BASE + nPackNum; i++) {
                if (ptScnList[i].scn_state == STATE_ON) {
                    perfIdxToSet = max(perfIdxToSet, ptScnList[i].scn_perf_idx);
                }
            }
            if (scn_perf_idx_now != perfIdxToSet) {
                scn_perf_idx_now = perfIdxToSet;
                setPerfIdx(scenario, scn_perf_idx_now);
            }
        }

        // check vcore
        //ALOGI("scn_vcore_now=%d,scn_vcore=%d",scn_vcore_now,ptScnList[scenario].scn_vcore);
        if(ptScnList[scenario].scn_vcore > 0 && scn_vcore_now >= ptScnList[scenario].scn_vcore) {
            for (i = 0; i < SCN_APP_RUN_BASE + nPackNum; i++) {
                if (ptScnList[i].scn_state == STATE_ON) {
                    VcoreToSet = max(VcoreToSet, ptScnList[i].scn_vcore);
                }
            }
            if (scn_vcore_now != VcoreToSet) {
                scn_vcore_now = VcoreToSet;
                setVcore(scenario, scn_vcore_now);
            }
        }

       // check wiphy_cam
        if (nSetWiphyCAM) {
            if(scn_wiphy_cam_now >= ptScnList[scenario].scn_wiphy_cam) {
                for (i = 0; i < nUserScnBase+REG_SCN_MAX; i++) {
                    if (ptScnList[i].scn_state == STATE_ON) {
                        wiphyCAMToSet = max(wiphyCAMToSet, ptScnList[i].scn_wiphy_cam);
                    }
                }
                if (scn_wiphy_cam_now != wiphyCAMToSet) {
                    scn_wiphy_cam_now = wiphyCAMToSet;
                    setWiphyCAM(scenario, scn_wiphy_cam_now);
                }
            }
        }


        /* FPS policy is only used in white list */
        if (ptScnList[scenario].dfps_mode != (int)MtkDfpsMode::DFPS_MODE_DEFAULT && ptScnList[scenario].dfps != -1) {
            ALOGI("[perfScnDisable] scn:%d, dfps mode:%d, fps:%d", scenario, ptScnList[scenario].dfps_mode, ptScnList[scenario].dfps);
            if(mFpsPolicy != NULL) {
                mFpsPolicy->setFps(-1, FpsPolicy::MODE_DEFAULT);
            }
        }

        /* DISP DeCouple is only used in white list */
        if (ptScnList[scenario].disp_mode != (int)MtkDispMode::DISP_MODE_DEFAULT) {
            ALOGI("[perfScnDisable] scn:%d, disp mode:%d", scenario, ptScnList[scenario].disp_mode);
            set_disp_ctl((int)MtkDispMode::DISP_MODE_DEFAULT);
        }

        /* video mode is only used in white list */
        if (ptScnList[scenario].video_mode != 0) {
            ALOGI("[perfScnDisable] scn:%d, video mode:%d, fps:%d", scenario, ptScnList[scenario].video_mode);
            if(mFpsPolicy != NULL) {
                mFpsPolicy->setVideoMode(false);
            }
        }

        /* CPUSET */
        if(ptScnList[scenario].scn_cpuset < nCpuSet) {
            ALOGI("[perfScnDisable] scn:%d, cpuset:%x, scn_cpuset:%x", scenario, scn_cpuset_now, ptScnList[scenario].scn_cpuset);
            cpusetToSet = nCpuSet;
            for (i = 0; i < SCN_APP_RUN_BASE + nPackNum; i++) {
                if (ptScnList[i].scn_state == STATE_ON) {
                    cpusetToSet = cpusetToSet & ptScnList[i].scn_cpuset;
                }
            }

            if(cpusetToSet != scn_cpuset_now) {
                scn_cpuset_now = (cpusetToSet == 0) ? 1 : cpusetToSet; // at least 1LL
                if(scn_cpuset_now == nCpuSet) {
                    set_value(PATH_GLOBAL_CPUSET_EN, 0);
                }
                else {
                    getCpusetArgv(scn_cpuset_now, cpusetMask);
                    set_value(PATH_GLOBAL_CPUSET_EN, 1);
                    set_value(PATH_GLOBAL_CPUSET, cpusetMask);
                }
            }
        }

        // check gpu
        if(scn_gpu_freq_now <= ptScnList[scenario].scn_gpu_freq) {
            for (i = 0; i < SCN_APP_RUN_BASE + nPackNum; i++) {
                if (ptScnList[i].scn_state == STATE_ON) {
                    gpuFreqToSet = max(gpuFreqToSet, ptScnList[i].scn_gpu_freq);
                }
            }
            scn_gpu_freq_now = gpuFreqToSet;
            setGpuFreq(scenario, scn_gpu_freq_now);
        }

        // check gpu max
        if(nSetGpuFreqMax) {
            if(scn_gpu_freq_max_now >= ptScnList[scenario].scn_gpu_freq_max) {
                for (i = 0; i < SCN_APP_RUN_BASE + nPackNum; i++) {
                    if (ptScnList[i].scn_state == STATE_ON) {
                        gpuFreqMaxToSet = min(gpuFreqMaxToSet, ptScnList[i].scn_gpu_freq_max);
                    }
                }
                scn_gpu_freq_max_now = gpuFreqMaxToSet;
                setGpuFreqMax(scenario, scn_gpu_freq_max_now);
            }
        }

        if (ptScnList[scenario].scn_fstb_fps_high != -1 && ptScnList[scenario].scn_fstb_fps_low != -1) {
            setFstbFps(scenario, -1, -1);
            ALOGV("[perfScnDisable] scn:%d, fstb_fps:1 %d-%d", scenario, -1, -1);
        }

        for(int idx = 0; idx < FIELD_SIZE; idx++) {
            if( tConTable[idx].entry.length() == 0 )
                break;

            if( tConTable[idx].isValid == -1)
                continue;

            ALOGV("[perfScnDisable] scn:%d, cmd:%d, reset:%d, default:%d, cur:%d, param:%d", scenario, tConTable[idx].cmdID,
                tConTable[idx].resetVal, tConTable[idx].defaultVal, tConTable[idx].curVal, ptScnList[scenario].scn_param[idx]);
            if(tConTable[idx].comp.compare(LESS) == 0) {
                if(ptScnList[scenario].scn_param[idx] < tConTable[idx].resetVal
                        && ptScnList[scenario].scn_param[idx] <= tConTable[idx].curVal ) {
                    numToSet = tConTable[idx].resetVal;

                    for (i = 0; i < SCN_APP_RUN_BASE + nPackNum; i++) {
                        if (ptScnList[i].scn_state == STATE_ON) {
                            numToSet = min(numToSet, ptScnList[i].scn_param[idx]);
                        }
                    }

                    tConTable[idx].curVal = numToSet;

                    if(tConTable[idx].curVal == tConTable[idx].resetVal)
                        set_value(tConTable[idx].entry.c_str(),
                                        tConTable[idx].defaultVal);
                    else
                        set_value(tConTable[idx].entry.c_str(),
                                        tConTable[idx].curVal);
                }
            }
            else {
                if(ptScnList[scenario].scn_param[idx] > tConTable[idx].resetVal
                        && ptScnList[scenario].scn_param[idx] >= tConTable[idx].curVal) {
                    numToSet = tConTable[idx].resetVal;

                    for (i = 0; i < SCN_APP_RUN_BASE + nPackNum; i++) {
                        if (ptScnList[i].scn_state == STATE_ON) {
                            numToSet = max(numToSet, ptScnList[i].scn_param[idx]);
                        }
                    }

                    tConTable[idx].curVal = numToSet;

                    if(tConTable[idx].curVal == tConTable[idx].resetVal)
                        set_value(tConTable[idx].entry.c_str(),
                                        tConTable[idx].defaultVal);
                    else
                        set_value(tConTable[idx].entry.c_str(),
                                        tConTable[idx].curVal);
                }
            }
        }

        // ppm mode
        if(ptScnList[scenario].scn_ppm_mode != PPM_IGNORE) {
            set_value(PATH_PPM_MODE, pPpmDefaultMode);
            nPpmCurrMode = PPM_IGNORE;
            ALOGI("[perfScnDisable] scn:%d, ppm mode:%s", scenario, pPpmDefaultMode);
        }
        else if(nPpmCurrMode != PPM_IGNORE && needUpdateCores && ptScnList[scenario].scn_core_min[2] > 0 && coresToSet[2] == 0) { // somebody enable big before
            set_value(PATH_PPM_MODE, tPpmMode[nPpmCurrMode]);
            ALOGI("[perfScnDisable] scn:%d, ppm mode:%d", scenario, ptScnList[scenario].scn_ppm_mode);
        }
    }

    //ALOGI("[perfScnDisable] scenario:%d, max_freq:%d, %d", scenario, ptClusterTbl[0].freqMax, ptClusterTbl[1].freqMax);
    return 0;
}

int perfScnUpdate(int scenario) // support cpu freq only
{
    ALOGV("[perfScnUpdate] scn:%d, do nothing", scenario);

#if 0
    int needUpdateCores = 0, needUpdateCoresMax = 0, needUpdateFreq = 0, needUpdateFreqMax = 0, i, j;
    int scn_core_min[CLUSTER_MAX];
    int scn_core_max[CLUSTER_MAX];
    int scn_freq_min[CLUSTER_MAX];
    int scn_freq_max[CLUSTER_MAX];
    int up_threshold = THRESHOLD_MAX, down_threshold = THRESHOLD_MAX, rush_boost = PPM_IGNORE, heavy_task = PPM_IGNORE, vcore = 0;

    //ALOGI("[perfScnUpdate] scn:%d, scn_cores_now:%d", scenario, scn_cores_now);
    //ALOGI("[perfScnUpdate] total:%d, [0]:%d, [1]:%d", ptScnList[scenario].scn_core_total, ptScnList[scenario].scn_core_min[0], ptScnList[scenario].scn_core_min[1]);

    if (checkSuccess(scenario)) {
        if (STATE_OFF == ptScnList[scenario].scn_state)
            return 0;
        ALOGI("[perfScnUpdate] scn:%d", scenario);
        ptScnList[scenario].scn_state = STATE_ON;

        scn_cores_now = 0;
        for (i=0; i<nClusterNum; i++) {
            scn_core_min[i] = -1;
            scn_core_max[i] = CORE_MAX;
            scn_freq_min[i] = -1;
            scn_freq_max[i] = FREQ_MAX;

            for (j = 0; j<SCN_APP_RUN_BASE + nPackNum; j++) {
                if (ptScnList[j].scn_state == STATE_ON) {
                    //ALOGI("[perfScnUpdate] i:%d, j:%d, scn_core_min[i]:%d, new:%d", i, j, scn_core_min[i], ptScnList[j].scn_core_min[i]);
                    scn_cores_now = max(scn_cores_now, ptScnList[j].scn_core_total);
                    scn_core_min[i] = max(scn_core_min[i], ptScnList[j].scn_core_min[i]);
                    scn_core_max[i] = min(scn_core_max[i], ptScnList[j].scn_core_max[i]);
                    scn_freq_min[i] = max(scn_freq_min[i], ptScnList[j].scn_freq_min[i]);
                    scn_freq_max[i] = min(scn_freq_max[i], ptScnList[j].scn_freq_max[i]);
                }
            }

            if(scn_core_min[i] != ptClusterTbl[i].cpuMinNow) {
                //ALOGI("[perfScnUpdate] scn_core_min[%d]:%d != %d", i, scn_core_min[i], ptClusterTbl[i].cpuMinNow);
                needUpdateCores = 1;
                ptClusterTbl[i].cpuMinNow = scn_core_min[i];
            }
            if(scn_core_max[i] != ptClusterTbl[i].cpuMaxNow) {
                //ALOGI("[perfScnUpdate] scn_core_max[%d]:%d != %d", i, scn_core_max[i], ptClusterTbl[i].cpuMaxNow);
                needUpdateCoresMax = 1;
                ptClusterTbl[i].cpuMaxNow = scn_core_max[i];
            }
            if(scn_freq_min[i] != ptClusterTbl[i].freqMinNow) {
                //ALOGI("[perfScnUpdate] scn_freq_min[%d]:%d != %d", i, scn_freq_min[i], ptClusterTbl[i].freqMinNow);
                needUpdateFreq = 1;
                ptClusterTbl[i].freqMinNow = scn_freq_min[i];
            }
            if(scn_freq_max[i] != ptClusterTbl[i].freqMaxNow) {
                //ALOGI("[perfScnUpdate] scn_freq_max[%d]:%d != %d", i, scn_freq_max[i], ptClusterTbl[i].freqMaxNow);
                needUpdateFreqMax = 1;
                ptClusterTbl[i].freqMaxNow = scn_freq_max[i];
            }
            if(scn_core_max[i] < scn_core_min[i]) { // min priority is higher than max
                needUpdateFreqMax = 1;
                ptClusterTbl[i].cpuMaxNow = scn_core_max[i] = PPM_IGNORE;
            }

            //scn_cores_now += scn_core_min[i];
        }

        if (needUpdateFreq)
            setClusterFreq(scenario, nClusterNum, scn_freq_min, scn_freq_max);

        if (needUpdateCores)
            setClusterCores(scenario, nClusterNum, scn_cores_now, scn_core_min, scn_core_max);

    }

    //ALOGI("[perfScnUpdate] scenario:%d, max_freq:%d, %d", scenario, ptClusterTbl[0].freqMax, ptClusterTbl[1].freqMax);
#endif
    return 0;
}

#if 0
int isUserScnEnable(void)
{
    int i;
    //ALOGI("[isUserScnEnable] check");

    for (i = nUserScnBase; i < nUserScnBase+REG_SCN_MAX; i++) {
        if (ptScnList[i].scn_state == STATE_ON && ptScnList[i].scn_core_total > 0) { // also check core setting of scenarios
            //ALOGI("[isUserScnEnable] scn:%d", i);
            return 1;
        }
    }
    return 0;
}
#endif

int updateCusScnTable(const char *path)
{
    FILE *ifp;
    char  buf[128], cmd[64], par1[16], par2[16], *str;
    int   i = 0, scn = (int)MtkPowerHint::MTK_POWER_HINT_NUM, param_1 = 0, param_2 = 0, width, height;
    int   mode = PERF_MODE_NORMAL;

    if (!path) {
        return 0;
    }

    if((ifp = fopen(path,"r")) == NULL)
        return 0;

    while(fgets(buf, 128, ifp)) {
        if(strlen(buf) < 3 || buf[0]=='#') // at least 3 characters, e.g., "a b"
            continue;

        str = strtok(buf, " ,");

        if(strlen(str) < 64)
            set_str_cpy(cmd, str, 64);

        str = strtok(NULL, " ,");

        if(!strcmp(str, "MTK_POWER_HINT_VSYNC"))
            scn = (int)MtkPowerHint::MTK_POWER_HINT_VSYNC;
        else if(!strcmp(str, "MTK_POWER_HINT_INTERACTION"))
            scn = (int)MtkPowerHint::MTK_POWER_HINT_INTERACTION;
        else if(!strcmp(str, "MTK_POWER_HINT_VIDEO_ENCODE"))
            scn = (int)MtkPowerHint::MTK_POWER_HINT_VIDEO_ENCODE;
        else if(!strcmp(str, "MTK_POWER_HINT_VIDEO_DECODE"))
            scn = (int)MtkPowerHint::MTK_POWER_HINT_VIDEO_DECODE;
        else if(!strcmp(str, "MTK_POWER_HINT_LOW_POWER"))
            scn = (int)MtkPowerHint::MTK_POWER_HINT_LOW_POWER;
        else if(!strcmp(str, "MTK_POWER_HINT_SP"))
            scn = (int)MtkPowerHint::MTK_POWER_HINT_SP;
        else if(!strcmp(str, "MTK_POWER_HINT_VR"))
            scn = (int)MtkPowerHint::MTK_POWER_HINT_VR;
        else if(!strcmp(str, "MTK_POWER_HINT_LAUNCH"))
            scn = (int)MtkPowerHint::MTK_POWER_HINT_LAUNCH;
        else if(!strcmp(str, "MTK_POWER_HINT_ACT_SWITCH"))
            scn = (int)MtkPowerHint::MTK_POWER_HINT_ACT_SWITCH;
        else if(!strcmp(str, "MTK_POWER_HINT_PACK_SWITCH"))
            scn = (int)MtkPowerHint::MTK_POWER_HINT_PACK_SWITCH;
        else if(!strcmp(str, "MTK_POWER_HINT_PROCESS_CREATE"))
            scn = (int)MtkPowerHint::MTK_POWER_HINT_PROCESS_CREATE;
        else if(!strcmp(str, "MTK_POWER_HINT_GAME_LAUNCH"))
            scn = (int)MtkPowerHint::MTK_POWER_HINT_GAME_LAUNCH;
        else if(!strcmp(str, "MTK_POWER_HINT_APP_ROTATE"))
            scn = (int)MtkPowerHint::MTK_POWER_HINT_APP_ROTATE;
        else if(!strcmp(str, "MTK_POWER_HINT_APP_TOUCH"))
            scn = (int)MtkPowerHint::MTK_POWER_HINT_APP_TOUCH;
        else if(!strcmp(str, "MTK_POWER_HINT_FRAME_UPDATE"))
            scn = (int)MtkPowerHint::MTK_POWER_HINT_FRAME_UPDATE;
        else if(!strcmp(str, "MTK_POWER_HINT_GAMING"))
            scn = (int)MtkPowerHint::MTK_POWER_HINT_GAMING;
        else if(!strcmp(str, "MTK_POWER_HINT_GALLERY_BOOST"))
            scn = (int)MtkPowerHint::MTK_POWER_HINT_GALLERY_BOOST;
        else if(!strcmp(str, "MTK_POWER_HINT_GALLERY_STEREO_BOOST"))
            scn = (int)MtkPowerHint::MTK_POWER_HINT_GALLERY_STEREO_BOOST;
        else if(!strcmp(str, "MTK_POWER_HINT_SPORTS"))
            scn = (int)MtkPowerHint::MTK_POWER_HINT_SPORTS;
        else if(!strcmp(str, "MTK_POWER_HINT_TEST_MODE"))
            scn = (int)MtkPowerHint::MTK_POWER_HINT_TEST_MODE;
        else if(!strcmp(str, "MTK_POWER_HINT_FPSGO"))
            scn = (int)MtkPowerHint::MTK_POWER_HINT_FPSGO;
        else if(!strcmp(str, "MTK_POWER_HINT_EXT_LAUNCH"))
            scn = (int)MtkPowerHint::MTK_POWER_HINT_EXT_LAUNCH;
        else if(!strcmp(str, "MTK_POWER_HINT_PMS_INSTALL"))
            scn = (int)MtkPowerHint::MTK_POWER_HINT_PMS_INSTALL;
        else if(!strcmp(str, "MTK_POWER_HINT_WIPHY_SPEED_DL"))
            scn = (int)MtkPowerHint::MTK_POWER_HINT_WIPHY_SPEED_DL;
        else if(!strcmp(str, "MTK_POWER_HINT_RESV_I"))
            scn = (int)MtkPowerHint::MTK_POWER_HINT_RESV_I;
        else
            continue;

        str = strtok(NULL, " ,");
        set_str_cpy(par1, str, 16);
        param_1 = atoi(par1);
        str = strtok(NULL, " ,");
        if (str != NULL) {
            set_str_cpy(par2, str, 16);
            param_2 = atoi(par2);
        }
        ALOGI("[updateCusScnTable] cmd:%s, scn:%d, param_1:%d, param_2:%d",
                cmd, scn, param_1, param_2);

        cmdSetting(-1, cmd, &ptScnList[scn], param_1, param_2, 0, 0);
    }

    fclose(ifp);

    return 0;
}

#if 0
int updateCusBoostTable(const char *path)
{
    FILE *ifp;
    char  buf[128], cmd[64], p_name[64], t_name[64], set_func[64], param[64];
    char *str, *thread;
    int   i = 0, priority=99, nice = 0, j = 0, num = 0;

    if (!path) {
        return 0;
    }

    if((ifp = fopen(path,"r")) == NULL)
        return 0;

    while(fgets(buf, 128, ifp)) {
        if(strlen(buf) < 3) // at least 3 characters, e.g., "a b"
            continue;

        str = strtok(buf, " ,");
        set_str_cpy(cmd, str, 64);

        str = strtok(NULL, " ,");
        set_str_cpy(p_name, str, 64);

        str = strtok(NULL, " ,");
        set_str_cpy(t_name, str, 64);

        str = strtok(NULL, " ,");
        set_str_cpy(set_func, str, 64);

        str = strtok(NULL, " ,\r\n");
        set_str_cpy(param, str, 64);

        if(!strcmp(t_name, "NULL"))
            thread = NULL;
        else
            thread = t_name;

        ALOGI("[updateCusBoostTable] cmd:%s, p:%s, t:%s, set:%s, param:%s", cmd, p_name, thread, set_func, param);

        if(!strcmp(cmd, "CMD_SET_BOOST_PROCESS_PID")) { // boost after get pid
            num++;
        }
        else {
            if(!strcmp(set_func, "SETFUNC_RT_FIFO")) { // boost immediately
                    if(param[0]) priority = atoi(param);
                    boost_process_by_name(p_name, thread, SETFUNC_RT_FIFO, priority);
            }
            else if(!strcmp(set_func, "SETFUNC_RT_RR")) {
                    if(param[0]) priority = atoi(param);
                    boost_process_by_name(p_name, thread, SETFUNC_RT_RR, priority);
            }
            else if(!strcmp(set_func, "SETFUNC_ROOTGROUP"))
                    boost_process_by_name(p_name, thread, SETFUNC_ROOTGROUP, system_cgroup_fd);
            else if(!strcmp(set_func, "SETFUNC_NICE")) {
                    if(param[0]) nice = atoi(param);
                    boost_process_by_name(p_name, thread, SETFUNC_NICE, nice);
            }
        }
    }

    // handle CMD_SET_BOOST_PROCESS_PID
    rewind(ifp);
    nPidListNum = num;
    i = 0;
    if((ptPidBoostList = (tPidBoost*)malloc(sizeof(tPidBoost) * nPidListNum)) == NULL) {
        ALOGE("Can't allocate memory");
        fclose(ifp);
        return 0;
    }

    while(fgets(buf, 128, ifp)) {
        if(strlen(buf) < 3) // at least 3 characters, e.g., "a b"
            continue;

        str = strtok(buf, " ,");
        set_str_cpy(cmd, str, 64);

        str = strtok(NULL, " ,");
        set_str_cpy(p_name, str, 64);

        str = strtok(NULL, " ,");
        set_str_cpy(t_name, str, 64);

        str = strtok(NULL, " ,");
        set_str_cpy(set_func, str, 64);

        str = strtok(NULL, " ,\r\n");
        set_str_cpy(param, str, 64);
        /*
        if(!strcmp(t_name, "NULL"))
            thread = NULL;
        else
            thread = t_name;
        */

        if(!strcmp(cmd, "CMD_SET_BOOST_PROCESS_PID")) {
            strncpy(ptPidBoostList[i].p_name, p_name, COMM_NAME_SIZE);
            strncpy(ptPidBoostList[i].t_name, t_name, COMM_NAME_SIZE);

            if(!strcmp(set_func, "SETFUNC_RT_FIFO")) {
                if(param[0]) priority = atoi(param);
                    ptPidBoostList[i].policy = SETFUNC_RT_FIFO;
                    ptPidBoostList[i].param = priority;
            }
            else if(!strcmp(set_func, "SETFUNC_RT_RR")) {
                if(param[0]) priority = atoi(param);
                    ptPidBoostList[i].policy = SETFUNC_RT_RR;
                ptPidBoostList[i].param = priority;
            }
            else if(!strcmp(set_func, "SETFUNC_ROOTGROUP")) {
                ptPidBoostList[i].policy = SETFUNC_ROOTGROUP;
                ptPidBoostList[i].param = system_cgroup_fd;
            }
            else if(!strcmp(set_func, "SETFUNC_NICE")) {
                if(param[0]) nice = atoi(param);
                    ptPidBoostList[i].policy = SETFUNC_NICE;
                ptPidBoostList[i].param = nice;
            }
            ptPidBoostList[i].ori_policy = ptPidBoostList[i].ori_param = -1;
            ALOGI("[updateCusBoostTable] ptPidBoostList[i], %s, %s, %d, %d", ptPidBoostList[i].p_name, ptPidBoostList[i].t_name, ptPidBoostList[i].policy, ptPidBoostList[i].param);
            i++;
        }
    }

    fclose(ifp);
    return 0;
}
#endif

void resetScenario(int handle)
{
    int i;
    ptScnList[handle].pack_name[0]      = '\0';
    ptScnList[handle].scn_type          = -1;
    ptScnList[handle].scn_core_total    = 0;
    ptScnList[handle].scn_state         = STATE_OFF;
    ptScnList[handle].scn_gpu_freq      = -1;
    ptScnList[handle].scn_gpu_freq_max  = nGpuHighestFreqLevel;
    ptScnList[handle].scn_perf_idx      = 0;
    ptScnList[handle].scn_valid         = 1; // default valid
    ptScnList[handle].pid               = ptScnList[handle].tid = -1;
    ptScnList[handle].scn_vcore         = 0;
    ptScnList[handle].scn_ppm_mode      = PPM_IGNORE;
    ptScnList[handle].scn_wiphy_cam     = 0;
    ptScnList[handle].dfps_mode         = (int)MtkDfpsMode::DFPS_MODE_DEFAULT;
    ptScnList[handle].dfps              = -1;
    ptScnList[handle].disp_mode         = (int)MtkDispMode::DISP_MODE_DEFAULT;
    ptScnList[handle].video_mode        = 0;
    ptScnList[handle].screen_off_action = (int)MtkScreenState::SCREEN_OFF_DISABLE;
    ptScnList[handle].boost_mode = ptScnList[handle].boost_timeout = -1;
    ptScnList[handle].scn_cpuset = nCpuSet;
    ptScnList[handle].scn_fstb_fps_high = -1;
    ptScnList[handle].scn_fstb_fps_low = -1;

    strncpy(ptScnList[handle].comm, "   ", COMM_NAME_SIZE);

    for (i=0; i<nClusterNum; i++) {
        ptScnList[handle].scn_core_min[i] = -1;
        ptScnList[handle].scn_core_max[i] = CORE_MAX;
        ptScnList[handle].scn_core_hard_min[i] = -1;
        ptScnList[handle].scn_freq_min[i] = -1;
        ptScnList[handle].scn_freq_max[i] = FREQ_MAX;
    }

    for (i = 0; i < FIELD_SIZE; i++) {
        ptScnList[handle].scn_param[i] = tConTable[i].resetVal;
    }
}

int cmdSetting(int icmd, char *scmd, tScnNode *scenario,
        int param_1, int param_2, int param_3, int param_4)
{
    int i = 0, ret = 0;

    if ((icmd < 0) && !scmd) {
        ALOGE("cmdSetting - scmd is NULL");
        return -1;
    }

    if (((icmd == -1) && !strcmp(scmd, "CMD_SET_GPU_FREQ_MIN")) ||
            icmd == (int)MtkPowerCmd::CMD_SET_GPU_FREQ_MIN) {
        scenario->scn_gpu_freq =
            (nGpuHighestFreqLevel < param_1) ? nGpuHighestFreqLevel : param_1;
    }
    else if (((icmd == -1) && !strcmp(scmd, "CMD_SET_GPU_FREQ_MAX")) ||
            icmd == (int)MtkPowerCmd::CMD_SET_GPU_FREQ_MAX) {
        scenario->scn_gpu_freq_max =
            (param_1 >= 0 && param_1 <= nGpuHighestFreqLevel) ? param_1 : nGpuHighestFreqLevel;
        nSetGpuFreqMax = 1;
    }
    else if (((icmd == -1) && !strcmp(scmd, "CMD_SET_CLUSTER_CPU_CORE_MIN")) ||
            icmd == (int)MtkPowerCmd::CMD_SET_CLUSTER_CPU_CORE_MIN) {
        if (param_1 >= nClusterNum || param_1 < 0)
            return -1;
        scenario->scn_core_min[param_1] =
            (param_2 <= ptClusterTbl[param_1].cpuNum) ?
            ((param_2 < 0) ? 0 : param_2) : ptClusterTbl[param_1].cpuNum;

        scenario->scn_core_total = 0;
        for (i=0; i<nClusterNum; i++) {
            if (scenario->scn_core_min[i] >= 0 && scenario->scn_core_min[i] <= ptClusterTbl[i].cpuNum)
                scenario->scn_core_total += scenario->scn_core_min[i];
        }
    }
    else if (((icmd == -1) && !strcmp(scmd, "CMD_SET_CLUSTER_CPU_CORE_MAX")) ||
            icmd == (int)MtkPowerCmd::CMD_SET_CLUSTER_CPU_CORE_MAX) {
        if (param_1 >= nClusterNum || param_1 < 0)
            return -1;
        scenario->scn_core_max[param_1] = (param_2 >= 0) ?
            param_2 : ptClusterTbl[param_1].cpuNum;
        nSetCoreMax =1;
    }
    else if (((icmd == -1) && !strcmp(scmd, "CMD_SET_CLUSTER_CPU_FREQ_MIN")) ||
            icmd == (int)MtkPowerCmd::CMD_SET_CLUSTER_CPU_FREQ_MIN) {
        if (param_1 >= nClusterNum || param_1 < 0)
            return -1;
        scenario->scn_freq_min[param_1] =
            (param_2 >= ptClusterTbl[param_1].freqMax) ? ptClusterTbl[param_1].freqMax : param_2;
    }
    else if (((icmd == -1) && !strcmp(scmd, "CMD_SET_CLUSTER_CPU_FREQ_MAX")) ||
            icmd == (int)MtkPowerCmd::CMD_SET_CLUSTER_CPU_FREQ_MAX) {
        if (param_1 >= nClusterNum || param_1 < 0)
            return -1;
        scenario->scn_freq_max[param_1] =
            (param_2 >= ptClusterTbl[param_1].freqMax) ? ptClusterTbl[param_1].freqMax : param_2;
        nSetFreqMax =1;
    }
    else if (((icmd == -1) && !strcmp(scmd, "CMD_SET_CPU_PERF_MODE")) ||
            icmd == (int)MtkPowerCmd::CMD_SET_CPU_PERF_MODE) {
        if(param_1 != 1) // param_1 must be 1
            return 0;

        for (i=0; i<nClusterNum; i++) {
            //ALOGI("cmdSetting - cmd:%d, i:%d, cpu:%d, freq:%d", icmd, i, ptClusterTbl[i].cpuNum, ptClusterTbl[i].freqMax);
            scenario->scn_core_min[i] = ptClusterTbl[i].cpuNum;
            scenario->scn_core_total += scenario->scn_core_min[i];
            scenario->scn_freq_min[i] = ptClusterTbl[i].freqMax;
        }
    }
    else if (((icmd == -1) && !strcmp(scmd, "CMD_SET_SCREEN_OFF_STATE")) ||
            icmd == (int)MtkPowerCmd::CMD_SET_SCREEN_OFF_STATE) {
            scenario->screen_off_action = param_1;
    }
    else if (((icmd == -1) && !strcmp(scmd, "CMD_SET_GLOBAL_CPUSET")) ||
            icmd == (int)MtkPowerCmd::CMD_SET_GLOBAL_CPUSET) {
        scenario->scn_cpuset = (param_1 <= nCpuSet) ? param_1 : nCpuSet;
    }
    else if (((icmd == -1) && !strcmp(scmd, "CMD_SET_SPORTS_MODE")) ||
            icmd == (int)MtkPowerCmd::CMD_SET_SPORTS_MODE) {
        if(param_1 == 1) {
            smart_reset(scenario->pack_name, 0);
            if(smart_check_pack_existed(APK_BENCHMARK, scenario->pack_name, foreground_uid, last_from_uid) == -1) {
                smart_add_benchmark();
            }
        }
    }
    else if (((icmd == -1) && !strcmp(scmd, "CMD_SET_FSTB_FPS")) ||
            icmd == (int)MtkPowerCmd::CMD_SET_FSTB_FPS) {
        scenario->scn_fstb_fps_high = param_1;
        scenario->scn_fstb_fps_low = param_2;
    }
    else if (((icmd == -1) && !strcmp(scmd, "CMD_SET_DFPS")) ||
            icmd == (int)MtkPowerCmd::CMD_SET_DFPS) {
        scenario->dfps_mode = (param_1 >= (int)MtkDfpsMode::DFPS_MODE_DEFAULT && param_1 < (int)MtkDfpsMode::DFPS_MODE_MAXIMUM) ? param_1 : (int)MtkDfpsMode::DFPS_MODE_DEFAULT;
        scenario->dfps = (param_2 > 0) ? param_2 : -1;
    }
    else if (((icmd == -1) && !strcmp(scmd, "CMD_SET_DISP_DECOUPLE")) ||
            icmd == (int)MtkPowerCmd::CMD_SET_DISP_DECOUPLE) {
        scenario->disp_mode = (param_1 >= (int)MtkDispMode::DISP_MODE_DEFAULT && param_1 < (int)MtkDispMode::DISP_MODE_NUM) ? param_1 : (int)MtkDispMode::DISP_MODE_DEFAULT;
    }
    else if (((icmd == -1) && !strcmp(scmd, "CMD_SET_VIDEO_MODE")) ||
            icmd == (int)MtkPowerCmd::CMD_SET_VIDEO_MODE) {
        scenario->video_mode = param_1 != 0 ? 1 : 0;
    }
    else if (((icmd == -1) && !strcmp(scmd, "CMD_SET_PACK_BOOST_TIMEOUT")) ||
            icmd == (int)MtkPowerCmd::CMD_SET_PACK_BOOST_TIMEOUT) {
        scenario->boost_timeout  = (param_1 >= 0) ? param_1 : -1;
        pboost_timeout = 1;
    }
    else if (((icmd == -1) && !strcmp(scmd, "CMD_SET_WALT_FOLLOW")) ||
            icmd == (int)MtkPowerCmd::CMD_SET_WALT_FOLLOW) {
            walt_follow_scn = param_1;
            walt_duration = param_2;
    }
    else if ((icmd < 0 && !strcmp(scmd, "CMD_SET_WIPHY_CAM")) ||
            icmd == (int)MtkPowerCmd::CMD_SET_WIPHY_CAM) {
        scenario->scn_wiphy_cam = (param_1 > 0) ? param_1 : 0;
        nSetWiphyCAM = 1;
    }
    else {
        ret = -1;
    }

    if (ret == 0)
        return ret;

    for(int idx = 0; idx < FIELD_SIZE; idx++) {
        if(((icmd == -1) && !strcmp(scmd, tConTable[idx].cmdName.c_str())) ||
                icmd == tConTable[idx].cmdID) {
            if(param_1 >= tConTable[idx].minVal && param_1 <= tConTable[idx].maxVal)
                scenario->scn_param[idx] = param_1;
            else
                ALOGI("input parameter exceed reasonable range %d %d %d %d", tConTable[idx].cmdID, param_1, tConTable[idx].maxVal,
                        tConTable[idx].minVal);

            ret = 0;
        }
    }

    if (ret == -1)
        ALOGI("cmdSetting - unknown cmd:%d, p1:%d, p2:%d, p3:%d, p4:%d", icmd, param_1, param_2, param_3, param_4);

    return ret;
}

int initPowerMode(void)
{
    return 0;
}

/*
check which mode need to change (sport and normal)

*/
void switchNormalAndSportMode(int mode) {

    for(int idx = 0; idx < FIELD_SIZE; idx++) {
        if(tConTable[idx].cmdName.length() == 0)
            continue;

        if(tConTable[idx].isValid == -1)
            continue;

        if(tConTable[idx].sportVal == -1)
            continue;

        ALOGV("[switchNormalAndSportMode] idx:%d, id:%d", idx, tConTable[idx].cmdID);

        if(mode == PERF_MODE_SPORTS)
            tConTable[idx].defaultVal = tConTable[idx].sportVal;
        else
            tConTable[idx].defaultVal = tConTable[idx].normalVal;

        if(tConTable[idx].curVal != tConTable[idx].resetVal)
            continue;

        set_value(tConTable[idx].entry.c_str(), tConTable[idx].defaultVal);
    }
}

int switchPowerMode(int mode) // call by perfNotifyUserStatus
{
//    int i;
    ALOGI("[switchPowerMode] mode:%d", mode);

    if(mode < 0 || mode > PERF_MODE_SPORTS)
        return 0;

    if(mode == PERF_MODE_SPORTS) {
        switchSportsMode(SPORTS_USER_NOTIFY, 1);
    }
    else {
        switchSportsMode(SPORTS_USER_NOTIFY, 0);
    }

    return 0;
}

int switchSportsMode(int reason, int bEnable) // for smart detection
{
    int newPowerMode;

    ALOGD("[switchSportsMode] reason:%d, enable:%d, turbo:%d", reason, bEnable, gtDrvInfo.turbo);

    if(reason == SPORTS_BENCHMARK) // smart detect
        nCurrBenchmarkMode = bEnable;
    else if(reason == SPORTS_USER_NOTIFY) // user notify
        nUserNotifyBenchmark = bEnable;
    else
        return 0;

    if(nCurrBenchmarkMode || nUserNotifyBenchmark)
        newPowerMode = PERF_MODE_SPORTS;
    else
        newPowerMode = PERF_MODE_NORMAL;

    if(nCurrPowerMode == newPowerMode) // does not change
        return 0; // do nothing

    nCurrPowerMode = newPowerMode;

    if(nCurrPowerMode == PERF_MODE_SPORTS) {
        switchNormalAndSportMode(PERF_MODE_SPORTS);
        smart_set_benchmark_mode(1);
        perfScnEnable((int)MtkPowerHint::MTK_POWER_HINT_SPORTS);
    }
    else {
        switchNormalAndSportMode(PERF_MODE_NORMAL);
        perfScnDisable((int)MtkPowerHint::MTK_POWER_HINT_SPORTS); // only enable if app is running and it's benchmark mdoe
        smart_set_benchmark_mode(0);
    }

    return 0;
}

int gameNotifyDisplayType(int type)
{
    ALOGI("gameNotifyDisplayType - type:%d", type);

    if(type == DISPLAY_TYPE_GAME ) {
        perfScnDisable((int)MtkPowerHint::MTK_POWER_HINT_APP_TOUCH);
        smart_update_score(INDEX_GAME_MODE, 1); // game mode
        if (nFbcSupport)
            fbcNotifyTouch(0);

        perfScnEnable((int)MtkPowerHint::MTK_POWER_HINT_GAMING);

        if(smart_check_pack_existed(APK_GAME, foreground_pack, foreground_uid, last_from_uid) == -1) {
            smart_add_game();
        }
    }
    else {
        if(type == DISPLAY_TYPE_NO_TOUCH_BOOST)
            perfScnDisable((int)MtkPowerHint::MTK_POWER_HINT_APP_TOUCH);

        perfScnDisable((int)MtkPowerHint::MTK_POWER_HINT_GAMING);
    }
    return 0;
}

extern "C"
int perfBoostEnable(int scenario)
{
    Mutex::Autolock lock(sMutex);
    if (!nIsReady)
        if(!init()) return 0;

    if(scenario == (int)MtkPowerHint::MTK_POWER_HINT_APP_TOUCH && nDisplayType == DISPLAY_TYPE_GAME)
        return 0;  // disable touch boost

    if (scenario < (int)MtkPowerHint::MTK_POWER_HINT_NUM)
        perfScnEnable(scenario);

    switch(scenario) {
    case (int)MtkPowerHint::MTK_POWER_HINT_FRAME_UPDATE:
        nDuringFrameUpdate = 1;
        break;

    case (int)MtkPowerHint::MTK_POWER_HINT_GAMING:
        perfScnDisable((int)MtkPowerHint::MTK_POWER_HINT_APP_TOUCH);
        nDisplayType = DISPLAY_TYPE_GAME;
        gameNotifyDisplayType(nDisplayType);
        break;

    case (int)MtkPowerHint::MTK_POWER_HINT_APP_TOUCH:
        if (nFbcSupport)
            fbcNotifyTouch(1);
        break;

    case (int)MtkPowerHint::MTK_POWER_HINT_SPORTS:
        switchSportsMode(SPORTS_USER_NOTIFY, 1);
        break;

    case (int)MtkPowerHint::MTK_POWER_HINT_FPSGO:
        /*ALOGI("[perfBoostEnable] FPSGO");*/
        break;

    default:
        break;
    }

    return 0;
}

extern "C"
int perfBoostDisable(int scenario)
{
    Mutex::Autolock lock(sMutex);
    if (!nIsReady)
        if(!init()) return 0;

    if (scenario < (int)MtkPowerHint::MTK_POWER_HINT_NUM)
        perfScnDisable(scenario);

    switch(scenario) {
    case (int)MtkPowerHint::MTK_POWER_HINT_FRAME_UPDATE:
        nDuringFrameUpdate = 0;
        break;

    case (int)MtkPowerHint::MTK_POWER_HINT_GAMING:
        nDisplayType = DISPLAY_TYPE_OTHERS;
        gameNotifyDisplayType(nDisplayType);
        break;

    case (int)MtkPowerHint::MTK_POWER_HINT_APP_TOUCH:
        if (nFbcSupport)
           fbcNotifyTouch(0);
        break;

    case (int)MtkPowerHint::MTK_POWER_HINT_SPORTS:
        switchSportsMode(SPORTS_USER_NOTIFY, 0);
        break;

    case (int)MtkPowerHint::MTK_POWER_HINT_FPSGO:
        /*ALOGI("[perfBoostDisable] FPSGO");*/
        break;

    default:
        break;
    }

    return 0;
}

extern "C"
int perfNotifyAppState(const char *packName, const char *actName, int state, int pid)
{
    int i,j,ret = 0;
    int Act_Match = 0,Common_index=-1;
    tScnNode *pPackList = NULL;

    Mutex::Autolock lock(sMutex);
    if (!nIsReady)
        if(!init()) return 0;

    if (!smart_table_init_flag())
        if(!smart_table_init()) return 0;

    //ALOGI("[perfNotifyAppState] nPackNum:%d, pack:%s, com:%s, state:%d, pid:%d, last_boost_tid:%d", nPackNum, packName, actName, state, pid, last_boost_tid);

    if(state == STATE_DEAD && pid != last_boost_tid)
        return 0;

    pPackList = ptScnList + SCN_APP_RUN_BASE;

    if(!packName) {
        return 0;
    }

    /* foreground change: update pack name */
    if(state == STATE_RESUMED && strcmp(foreground_act, actName)) {
        strncpy(foreground_act, actName, CLASS_NAME_MAX-1); // update pack name

        // check white list
        Act_Match = 0;
        Common_index = -1;
        for(i=0; i<nPackNum; i++) {
            if(strncmp(pPackList[i].act_name, "Common", 6)!=0){
                if(!strcmp(pPackList[i].pack_name, packName) && !strcmp(pPackList[i].act_name, actName)){
                    Act_Match = 1;
                    if(pPackList[i].boost_timeout >= 0) {
                       ret = (SCN_APP_RUN_BASE + i);
                       ALOGI("[perfNotifyAppState] Activity policy match scn=%d time=%d!!!", ret, pPackList[i].boost_timeout);
                    } else {
                       ALOGI("[perfNotifyAppState] Activity policy match !!!");
                    }

                    perfScnEnable(SCN_APP_RUN_BASE + i);
                }
                else{
                    perfScnDisable(SCN_APP_RUN_BASE + i);
                }
            }
            else{
                if(!strcmp(pPackList[i].pack_name, packName)){
                    Common_index = i;
                }
                else{
                    perfScnDisable(SCN_APP_RUN_BASE + i);
                }
            }
        }

        if(Act_Match !=1 && Common_index != -1){
            if(pPackList[Common_index].boost_timeout >= 0) {
                  ret = (SCN_APP_RUN_BASE + Common_index);
                  ALOGI("[perfNotifyAppState] Package common policy match scn=%d time=%d!!!",
                                                    ret, pPackList[Common_index].boost_timeout);
            } else {
                  ALOGI("[perfNotifyAppState] Package common policy match !!!");
            }

            perfScnEnable(SCN_APP_RUN_BASE + Common_index);
        }
        else
        if(Act_Match ==1 && Common_index != -1){
            perfScnDisable(SCN_APP_RUN_BASE + Common_index);
        }
    }

    /* foreground change: update pack name */
    if(state == STATE_RESUMED && strcmp(foreground_pack, packName)) {
        // update debug info
        if(nSmartDebugEnable && smart_is_det_enable()) {
            smart_update_pack();
        }

        strncpy(foreground_pack, packName, PACK_NAME_MAX-1); // update pack name
        last_boost_tid = pid;
        foregroundName.setTo(packName);
        mFpsInfo->setForegroundInfo(last_boost_tid, foregroundName); // notify fpsInfo

        ALOGI("[perfNotifyAppState] foreground:%s, pid:%d", foreground_pack, last_boost_tid);
        if(smart_check_pack_existed(APK_BENCHMARK, foreground_pack, foreground_uid, last_from_uid) != -1) {
            ALOGI("[perfNotifyAppState] match !!!");
            switchSportsMode(SPORTS_BENCHMARK, 1);
        }
        else {
            smart_reset(foreground_pack, last_boost_tid); // start detection after get request
            switchSportsMode(SPORTS_BENCHMARK, 0);
        }
        //smart_control(1); // it is always enable

        // notify other modules
        set_value(PATH_THERMAL_PID, pid); // notify thermal
        set_value(PATH_GX_PID, pid); // notify thermal
    }

    return ret;
}

extern "C"
int perfUserScnEnable(int handle)
{
    Mutex::Autolock lock(sMutex);
    if (!nIsReady)
        if(!init()) return 0;
    //ALOGI("perfUserScnEnable - handle:%d", handle);

    if(handle < nUserScnBase || handle >= nUserScnBase + REG_SCN_MAX)
        return -1;

    if(nDisplayType != DISPLAY_TYPE_GAME && ptScnList[handle].scn_core_total > 0) {
        smart_update_score(INDEX_USER_SPECIFIED, 1); // user specified
    }

    if(ptScnList[handle].scn_type == SCN_USER_HINT)
        ALOGI("perfUserScnEnable - handle:%d", handle);
    else if (ptScnList[handle].scn_type == SCN_CUS_POWER_HINT)
        ALOGI("perfCusUserScnEnable - handle:%d", handle);

    perfScnEnable(handle);
    return 0;
}

extern "C"
int perfUserScnDisable(int handle)
{
    Mutex::Autolock lock(sMutex);
    if (!nIsReady)
        if(!init()) return 0;
    //ALOGI("perfUserScnDisable - handle:%d", handle);

    if(handle < nUserScnBase || handle >= nUserScnBase + REG_SCN_MAX)
        return -1;

    perfScnDisable(handle);
    return 0;
}

extern "C"
int perfPreDefinedScnDisable(int handle)
{
    Mutex::Autolock lock(sMutex);
    if (!nIsReady)
        if(!init()) return 0;
    ALOGI("perfPreDefinedScnDisable - handle:%d", handle);

    if(handle < SCN_APP_RUN_BASE || handle >= (SCN_APP_RUN_BASE + nPackNum))
        return -1;

    perfScnDisable(handle);
    return 0;
}

extern "C"
int perfUserScnResetAll(void)
{
    int i;

    Mutex::Autolock lock(sMutex);
    if (!nIsReady)
        if(!init()) return 0;
    //ALOGI("perfUserScnResetAll");

    for(i=nUserScnBase; i<nUserScnBase+REG_SCN_MAX; i++) {
        if(ptScnList[i].scn_type != -1 && ptScnList[i].scn_state == STATE_ON)
            perfScnDisable(i);
        resetScenario(i);
    }
    return 0;
}

extern "C"
int perfUserScnDisableAll(void)
{
    int i;
    struct stat stat_buf;
    int exist;
    char proc_path[128];

    Mutex::Autolock lock(sMutex);
    if (!nIsReady)
        if(!init()) return 0;
    //ALOGI("perfUserScnDisableAll");

    for(i=0; i<SCN_APP_RUN_BASE + nPackNum; i++) {
        if(ptScnList[i].scn_type != -1) {
            if(ptScnList[i].scn_state == STATE_ON && ptScnList[i].screen_off_action != SCREEN_OFF_ENABLE) {
                ALOGI("perfUserScnDisableAll, h:%d, s:%d, a:%d", i, ptScnList[i].scn_state, ptScnList[i].screen_off_action);
                perfScnDisable(i);
                if(ptScnList[i].screen_off_action == SCREEN_OFF_WAIT_RESTORE)
                    ptScnList[i].scn_state = STATE_WAIT_RESTORE;
            }
            // kill handle if process is dead
            if(i >= nUserScnBase && i < SCN_APP_RUN_BASE) {
              if(ptScnList[i].scn_type == SCN_USER_HINT) {
                sprintf(proc_path, "/proc/%d", ptScnList[i].pid);
                exist = (0 == stat(proc_path, &stat_buf)) ? 1 : 0;
                if(!exist) {
                    ALOGI("perfUserScnDisableAll, hdl:%d, pid:%d is not existed", i, ptScnList[i].pid);
                    perfScnDisable(i);
                    resetScenario(i);
                }
              }
            }
        }
    }

    return 0;
}

extern "C"
int perfUserScnRestoreAll(void)
{
    int i;

    Mutex::Autolock lock(sMutex);
    if (!nIsReady)
        if(!init()) return 0;
    //ALOGI("perfUserScnRestoreAll");

    for(i=0; i<SCN_APP_RUN_BASE + nPackNum; i++) {
        if(ptScnList[i].scn_type != -1 && ptScnList[i].scn_state == STATE_WAIT_RESTORE) {
            ALOGI("perfUserScnRestoreAll, h:%d, s:%d, a:%d", i, ptScnList[i].scn_state, ptScnList[i].screen_off_action);
            perfScnEnable(i);
        }
    }
    return 0;
}

extern "C"
int perfDumpAll(void)
{
    int i, j;

    Mutex::Autolock lock(sMutex);
    if (!nIsReady)
        if(!init()) return 0;
    ALOGI("perfDumpAll");

    // check predefined scenario
    for (i = 0; i < nUserScnBase; i++) {
        if (ptScnList[i].scn_state == STATE_ON) {
            ALOGI("perfDumpAll (predefined) - type:%d", ptScnList[i].scn_type);
            for (j = 0; j < nClusterNum; j++) {
                ALOGI("            - cpu:%d, freq:%d", ptScnList[i].scn_core_min[j], ptScnList[i].scn_freq_min[j]);
            }
        }
    }
    // check user defined scenario
    for (i = nUserScnBase; i < nUserScnBase+REG_SCN_MAX; i++) {
        if (ptScnList[i].scn_state == STATE_ON) {
            ALOGI("perfDumpAll (user register)- type:%d, pid:%d, tid:%d, comm:%s", ptScnList[i].scn_type, ptScnList[i].pid, ptScnList[i].tid, ptScnList[i].comm);
            for (j = 0; j < nClusterNum; j++) {
                ALOGI("            - cpu:%d, freq:%d", ptScnList[i].scn_core_min[j], ptScnList[i].scn_freq_min[j]);
            }
        }
    }
    // check white list scenario
    for (i = SCN_APP_RUN_BASE; i < SCN_APP_RUN_BASE+nPackNum; i++) {
        if (ptScnList[i].scn_state == STATE_ON) {
            ALOGI("perfDumpAll (app list)- type:%d, pack_name:%s, act_name:%s", ptScnList[i].scn_type, ptScnList[i].pack_name, ptScnList[i].act_name);
            for (j = 0; j < nClusterNum; j++) {
                ALOGI("            - cpu:%d, freq:%d", ptScnList[i].scn_core_min[j], ptScnList[i].scn_freq_min[j]);
            }
        }
    }
    return 0;
}

#define CMD_GET_BOOST_TIMEOUT 10

extern "C"
int perfUserGetCapability(int cmd, int id)
{
    int value = -1;

    Mutex::Autolock lock(sMutex);
    if (!nIsReady)
        if(!init()) return 0;

    if (id >= nClusterNum || id < 0)
        return value;

    switch(cmd) {
    case (int)MtkQueryCmd::CMD_GET_CLUSTER_NUM:
        value = nClusterNum;
        break;

    case (int)MtkQueryCmd::CMD_GET_CLUSTER_CPU_NUM:
        value = ptClusterTbl[id].cpuNum;
        break;

    case (int)MtkQueryCmd::CMD_GET_CLUSTER_CPU_FREQ_MIN:
        value = ptClusterTbl[id].pFreqTbl[0];
        break;

    case (int)MtkQueryCmd::CMD_GET_CLUSTER_CPU_FREQ_MAX:
        value = ptClusterTbl[id].freqMax;
        break;

    case (int)MtkQueryCmd::CMD_GET_FOREGROUND_PID:
        value = last_boost_tid;
        break;
    case (int)MtkQueryCmd::CMD_GET_FOREGROUND_TYPE:
        if(smart_check_pack_existed(APK_GAME, foreground_pack, foreground_uid, last_from_uid) != -1)
            value = APK_GAME;
        break;
    case (int)MtkQueryCmd::CMD_GET_WALT_FOLLOW:
            value = walt_follow_scn;
        break;
    case (int)MtkQueryCmd::CMD_GET_WALT_DURATION:
            value = walt_duration;
        break;
    case CMD_GET_BOOST_TIMEOUT:
            value = pboost_timeout;
        break;
    default:
        break;
    }

    ALOGI("perfUserGetCapability - cmd:%d, id:%d, val:%d", cmd, id, value);
    return value;
}

#if 0
extern "C"
int perfGetClusterInfo(int cmd, int id)
{
    int value = -1;

    Mutex::Autolock lock(sMutex);
    if (!nIsReady)
        if(!init()) return 0;
    ALOGI("perfGetClusterInfo - cmd:%d, id:%d", cmd, id);

    if (id >= nClusterNum)
        return value;

    switch(cmd) {
    case (int)MtkQueryCmd::CMD_GET_CLUSTER_NUM:
        value = nClusterNum;
        break;

    case (int)MtkQueryCmd::CMD_GET_CLUSTER_CPU_NUM:
        value = ptClusterTbl[id].cpuNum;
        break;

    case (int)MtkQueryCmd::CMD_GET_CLUSTER_CPU_FREQ_MIN:
        value = ptClusterTbl[id].pFreqTbl[0];
        break;

    case (int)MtkQueryCmd::CMD_GET_CLUSTER_CPU_FREQ_MAX:
        value = ptClusterTbl[id].freqMax;
        break;

    default:
        break;
    }

    ALOGI("perfGetClusterInfo - value:%d", value);
    return value;
}
#endif

extern "C"
int     perfCusUserRegScn(void)
{
    int i , handle = -1, add_for_cus_power_hint = 0;
    char filepath[64];

    Mutex::Autolock lock(sMutex);
    if (!nIsReady)
        if(!init()) return 0;
    ALOGD("perfCusUserRegScn");

    for (i = nUserScnBase; i < nUserScnBase + REG_SCN_MAX; i++) {
        if(ptScnList[i].scn_type == -1) {
            handle = i;
            resetScenario(i);
            ptScnList[i].scn_type = SCN_CUS_POWER_HINT;
            ptScnList[i].scn_state = STATE_OFF;
            add_for_cus_power_hint = 1;
            break;
        }
    }
    return handle;
}

extern "C"
int     perfUserRegScn(int pid, int tid)
{
    int i , handle = -1;
    char filepath[64];

    Mutex::Autolock lock(sMutex);
    if (!nIsReady)
        if(!init()) return 0;
    ALOGD("perfUserRegScn - pid:%d, tid:%d", pid, tid);

    for (i = nUserScnBase; i < nUserScnBase + REG_SCN_MAX; i++) {
        if(ptScnList[i].scn_type == -1) {
            handle = i;
            resetScenario(i);
            ptScnList[i].scn_type = SCN_USER_HINT;
            ptScnList[i].pid = pid;
            ptScnList[i].tid = tid;
            sprintf(filepath, "/proc/%d/comm", pid);
            get_task_comm(filepath, ptScnList[i].comm);
            ptScnList[i].scn_state = STATE_OFF;
            break;
        }
    }

    return handle;
}

extern "C"
int perfUserRegScnConfig(int handle, int cmd, int param_1, int param_2, int param_3, int param_4)
{
    int i;

    Mutex::Autolock lock(sMutex);
    if (!nIsReady)
        if(!init()) return 0;
    //ALOGI("perfUserRegScnConfig - handle:%d, cmd:%d, p1:%d, p2:%d, p3:%d, p4:%d", handle, cmd, param_1, param_2, param_3, param_4);

    if((handle < nUserScnBase || handle >= nUserScnBase + REG_SCN_MAX) && param_4 != 1) // param_4 == 1 => modify pre-defined scn
        return -1;

    cmdSetting(cmd, NULL, &ptScnList[handle], param_1, param_2, param_3, param_4);
    perfScnUpdate(handle);

    return 0;
}

extern "C"
int perfUserUnregScn(int handle)
{
    Mutex::Autolock lock(sMutex);
    if (!nIsReady)
        if(!init()) return 0;
    ALOGI("perfUserUnregScn - handle:%d", handle);

    if(handle < nUserScnBase || handle >= nUserScnBase + REG_SCN_MAX)
        return -1;

    if(ptScnList[handle].scn_state == STATE_ON) {
        return -1;
    }

    resetScenario(handle);
    return 0;
}

#if 0
extern "C"
int perfSetFavorPid(int pid)
{
    int i, found = 0;
    char pidPath[PATH_LENGTH];
    char comm[COMM_NAME_SIZE];
    struct sched_param param;
    int sched_policy;

    Mutex::Autolock lock(sMutex);
    if (!nIsReady)
        if(!init()) return 0;

    if (last_boost_tid == pid && scn_check_retry != CHECK_RETRY)
        return -1;

    ALOGD("perfSetFavorPid - pid:%d, %x", pid, pid);

    if (pid & RENDER_BIT) {
        pid &= RENDER_MASK;
        //ALOGI("perfSetFavorPid:%d", pid);
        sprintf(pidPath, "/proc/%d/comm", pid);
        get_task_comm(pidPath, comm);
        for(i=0; i<nPidListNum; i++) {
            if(!strcmp(ptPidBoostList[i].t_name, comm)) {
                found = 1;
                break;
            }
        }

        if(found) { // pid is in perfservboosttbl.txt
            // backup original setting
            if((sched_policy = sched_getscheduler(pid) & ~SCHED_RESET_ON_FORK) < 0)
                ALOGI("sched_getscheduler failed: %s\n", strerror(errno));
            switch(sched_policy) {
            case SCHED_OTHER:
                ptPidBoostList[i].ori_policy = SETFUNC_NICE;
                break;

            case SCHED_FIFO:
                ptPidBoostList[i].ori_policy = SETFUNC_RT_FIFO;
                break;

            case SCHED_RR:
                ptPidBoostList[i].ori_policy = SETFUNC_RT_RR;
                break;

            default:
                break;
            }

            if(ptPidBoostList[i].ori_policy == SETFUNC_NICE)
                ptPidBoostList[i].ori_param = getpriority(PRIO_PROCESS, pid);
            else if(sched_getparam(pid, &param)!=0)
                ALOGI("sched_getparam failed: %s\n", strerror(errno));
            else
                ptPidBoostList[i].ori_param = param.sched_priority;
            ALOGI("perfSetFavorPid - pid:%d, ori_policy:%d, ori_param:%d", pid, ptPidBoostList[i].ori_policy, ptPidBoostList[i].ori_param);
            boost_process_by_tid(pid, 0, ptPidBoostList[i].policy, ptPidBoostList[i].param);
        }
        else { // pid is not in perfservboosttbl.txt => default: root
                boost_process_by_tid(pid, 0, SETFUNC_ROOTGROUP, system_cgroup_fd);
        }
        return 0;
    }
    else if (last_boost_tid != pid) {
        set_value(PATH_THERMAL_PID, pid); // notify thermal
    }

    //last_boost_tid = pid;

    return 0;
}
#endif

extern "C"
int perfSetUidInfo(int uid, int fromUid)
{
    Mutex::Autolock lock(sMutex);
    if (!nIsReady)
        if(!init()) return 0;

    if (foreground_uid == uid && last_from_uid == fromUid)
        return -1;

    ALOGI("perfSetUidInfo - uid:%d, fromUid:%d", uid, fromUid);
    foreground_uid = uid;
    last_from_uid = fromUid;

    return 0;
}

extern "C"
int perfNotifyUserStatus(int type, int status)
{
    Mutex::Autolock lock(sMutex);
    if (!nIsReady)
        if(!init()) return 0;
    ALOGV("perfNotifyUserStatus - type:%d, status:%d", type, status);

    switch(type) {

    case NOTIFY_USER_TYPE_SCENARIO_ON:
        if(status == (int)MtkPowerHint::MTK_POWER_HINT_APP_TOUCH)
            set_value(PATH_GPUFREQ_EVENT, "touch_down 1");
        break;

    case NOTIFY_USER_TYPE_SCENARIO_OFF:
        if(status == (int)MtkPowerHint::MTK_POWER_HINT_APP_TOUCH)
            set_value(PATH_GPUFREQ_EVENT, "touch_down 0");
        break;

    case NOTIFY_USER_TYPE_CORE_ONLINE:
        if(status == 8)
            strncpy(last_check_pack, foreground_pack, PACK_NAME_MAX-1);
        break;

    case NOTIFY_USER_TYPE_PERF_MODE:
        if(status >= PERF_MODE_NORMAL && status <= PERF_MODE_SPORTS) {
            if(status == PERF_MODE_NORMAL) {
                switchPowerMode(PERF_MODE_NORMAL);
            }
            else if(status == PERF_MODE_SPORTS) {
                switchPowerMode(PERF_MODE_SPORTS);
            }
        }
        break;

    case NOTIFY_USER_TYPE_OTHERS:
        if(status == 0)
            smart_dump_info();
        else if(status == 1)
            nDebugLogEnable = 1;
        else if(status == 2)
            nVerboseLogEnable = 1;
        else if(status == 3)
            dump_white_list();
        break;

    // start or stop smart detection. Control by PerfServiceManager
    #if 0
    case NOTIFY_USER_TYPE_DETECT:
        if(status == 1) {
            if(smart_check_pack_existed(APK_BENCHMARK, foreground_pack, foreground_uid, last_from_uid) != -1 || \
               smart_check_pack_existed(APK_NOT_BENCHMARK, foreground_pack, foreground_uid, last_from_uid) != -1) {
                break; // benchmark or app which is not benchmark
            }
            //smart_reset(foreground_pack, last_boost_tid); // start to detect
            smart_control(1); // start to detect
        }
        break;
    #endif

    default:
        break;
    }

    return 0;
}

#if 0
extern "C"
int perfNotifyDisplayType(int type)
{
    Mutex::Autolock lock(sMutex);
    if (!nIsReady)
        if(!init()) return 0;
    ALOGI("perfNotifyDisplayType - type:%d", type);

    nDisplayType = type;

    if(type == DISPLAY_TYPE_GAME ) {
        perfScnDisable((int)MtkPowerHint::MTK_POWER_HINT_APP_TOUCH);
        smart_update_score(INDEX_GAME_MODE, 1); // game mode
        if (nFbcSupport)
            fbcNotifyTouch(0);

        perfScnEnable((int)MtkPowerHint::MTK_POWER_HINT_GAMING);

        if(smart_check_pack_existed(APK_GAME, foreground_pack, foreground_uid, last_from_uid) == -1) {
            smart_add_game();
        }
    }
    else {
        if(type == DISPLAY_TYPE_NO_TOUCH_BOOST)
            perfScnDisable((int)MtkPowerHint::MTK_POWER_HINT_APP_TOUCH);

        perfScnDisable((int)MtkPowerHint::MTK_POWER_HINT_GAMING);
        //smart_update_score(INDEX_GAME_MODE, 0); // non-game: don't calculate again
    }

    return 0;
}
#endif

extern "C"
int perfGetLastBoostPid()
{
    Mutex::Autolock lock(sMutex);
    if (!nIsReady)
        if(!init()) return 0;
    //ALOGI("perfGetLastBoostPid %d", last_boost_tid);

    return last_boost_tid;
}

extern "C"
int perfSetPackAttr(int isSystem, int eabiNum)
{
    Mutex::Autolock lock(sMutex);
    if (!nIsReady)
        if(!init()) return 0;
    ALOGD("perfSetPackAttr: %d, %d", isSystem, eabiNum);

    if(isSystem)
        smart_update_score(INDEX_SYSTEM_APK, 1); // system APK
#if 0
    if(eabiNum >= 2)
        smart_update_score(INDEX_ARM_NATIVE_LIB, 1); // arm native library
#endif

    return 0;
}

int get_ppid_comm(int tid, char *ppid_comm, int size)
{
    int r, fd;
    char statline[1024], comm[128], state;
    int pid, ppid;
    ALOGI("get_ppid_comm - tid:%d", tid);

    memset(statline, 0, sizeof(char)*1024);
    sprintf(statline, "/proc/%d/stat", tid);

    fd = open(statline, O_RDONLY);
    if(fd == -1) return -1;
    r = read(fd, statline, 1023);
    close(fd);
    if(r < 0) return -1;
    statline[r] = 0;

    // 1851 (ndroid.settings) S 476
    sscanf(statline, "%d %127s %c %d", &pid, comm, &state, &ppid);
    ALOGV("get_ppid_comm - pid:%d, ppid:%d", pid, ppid);

    memset(statline, 0, sizeof(char)*128);

#if 0
    sprintf(statline, "/proc/%d/comm", ppid);
    fd = open(statline, O_RDONLY);
    if(fd == -1) return -1;
    r = read(fd, comm, 127);
    close(fd);
    if(r < 0) return -1;
    comm[r] = 0;
    ALOGI("get_ppid_comm - ppid:%d, comm:%s", ppid, comm);
#endif

    sprintf(statline, "/proc/%d/cmdline", ppid);
    fd = open(statline, O_RDONLY);
    if(fd == -1) return -1;
    r = read(fd, comm, 127);
    close(fd);
    if(r < 0) return -1;
    comm[r] = 0;
    strncpy(ppid_comm, comm, size-1);
    ALOGI("get_ppid_comm - ppid:%d, cmdline:%s", ppid, comm);

    return 0;
}

extern "C"
int perfGetPackAttr(const char *packName, int cmd)
{
    tScnNode *pPackList = NULL;
    int i, attr = -1;

    Mutex::Autolock lock(sMutex);
    if (!nIsReady)
        if(!init()) return 0;
    //ALOGI("perfGetPackAttr %s, cmd:%d", packName, cmd);

    if(!packName) return -1;

    pPackList = ptScnList + SCN_APP_RUN_BASE;

    /* check white list */
    for(i=0; i<nPackNum; i++) {
        if(strcmp(pPackList[i].pack_name, packName) == 0) {
            break;
        }
    }

    if(i == nPackNum)
        return -1;

    switch(cmd) {
    case CMD_GET_PACK_IN_WHITE_LIST:
        attr = 1;
        break;

    case CMD_GET_PACK_BOOST_MODE:
        attr = pPackList[i].boost_mode;
        break;

    case CMD_GET_PACK_BOOST_TIMEOUT:
        attr = (pPackList[i].boost_timeout)*1000;
        ALOGI("perfGetPackAttr %s, cmd:%d, attr:%d", packName, cmd, attr);
        break;

    default:
        break;
    }

    ALOGD("perfGetPackAttr %s, cmd:%d, attr:%d", packName, cmd, attr);
    return attr;
}

extern "C"
char* perfGetGiftAttr(const char *packName,const char *attrName)
{
    int i, packIndex=-1, AttrIndex=-1;
    char *returnGiftInfo = NULL;

    returnGiftInfo = (char *) malloc(GIFTATTR_VALUE_MAX * sizeof(char));
    if(returnGiftInfo != NULL){
        if(nXmlGiftTagNum == 0){
            set_str_cpy(returnGiftInfo, "GiftEmpty", GIFTATTR_VALUE_MAX);//Note:GiftTagEmpty
        }
        else{
            for(i=0;i<nXmlGiftTagNum;i++){
                if(strcmp(ptXmlGiftTagList[i].packName, packName)==0){
                    packIndex = i;
                    break;
                }
            }

            if(packIndex == -1){
                set_str_cpy(returnGiftInfo, "GiftEmpty", GIFTATTR_VALUE_MAX);//Note:GiftTagEmpty
            }
            else{
                for(i=0;i<nXmlGiftAttrNum;i++){
                    if(strcmp(ptXmlGiftTagList[packIndex].AttrName[i], attrName)==0){
                        AttrIndex = i;
                        break;
                    }
                }
                if(AttrIndex == -1){
                    set_str_cpy(returnGiftInfo, "GiftEmpty", GIFTATTR_VALUE_MAX);//Note:GiftAttrEmpty
                }
                else{
                    set_str_cpy(returnGiftInfo, ptXmlGiftTagList[packIndex].AttrValue[AttrIndex], GIFTATTR_VALUE_MAX);
                }
            }
        }
    }
    else{
        ALOGE("[perfGetGiftAttr] Can't allocate memory");
    }

    return returnGiftInfo;
}


int loadConTable(const char *file_name) {
    ifstream infile(file_name, ifstream::in);

    string line;
    char buf[160], *pch = NULL;
    int idx = 0;


    if(infile.is_open()) {
        while(getline(infile, line)) {
            if(line[0] == '#')
                continue;

            set_str_cpy(buf, line.c_str(), 160);
            pch = strtok(buf, ", ");
            if (pch != NULL)
                tConTable[idx].cmdName.assign(pch);
            else {
                ALOGI("Command Name is empty");
                tConTable[idx].cmdName.assign("");
            }

            pch = strtok(NULL, ", ");
            if (pch != NULL)
                tConTable[idx].cmdID = atoi(pch);
            else {
                ALOGI("Command ID is empty");
                tConTable[idx].cmdID = 0;
            }

            pch = strtok(NULL, ", ");
            if (pch != NULL) {
                tConTable[idx].entry.assign(pch);

                if(access(pch, W_OK) != -1)
                    tConTable[idx].isValid = 0;
                else {
                    tConTable[idx].isValid = -1;
                    ALOGI("%s doesn't have write permission!!!!", tConTable[idx].cmdName.c_str());
                    ALOGI("write of %s failed: %s\n", tConTable[idx].entry.c_str(), strerror(errno));
                }
            }
            else {
                ALOGI("Cannot find entry data");
                tConTable[idx].entry.assign("");
            }

            pch = strtok(NULL, ", ");
            if (pch != NULL)
                tConTable[idx].comp.assign(pch);
            else {
                ALOGI("compare value is empty");
                tConTable[idx].comp.assign("");
            }

            pch = strtok(NULL, ", ");
            if (pch != NULL)
                tConTable[idx].maxVal = atoi(pch);
            else {
                ALOGI("Max value is 0");
                tConTable[idx].maxVal = 0;
            }

            pch = strtok(NULL, ", ");
            if (pch != NULL)
                tConTable[idx].minVal = atoi(pch);
            else {
                ALOGI("Min value is 0");
                tConTable[idx].minVal = 0;
            }

            pch = strtok(NULL, ", ");
            if(pch != NULL)
                tConTable[idx].normalVal = atoi(pch);
            else
                tConTable[idx].normalVal = -1;

            pch = strtok(NULL, ", ");
            if(pch != NULL)
                tConTable[idx].sportVal = atoi(pch);
            else
                tConTable[idx].sportVal = -1;

            if(tConTable[idx].normalVal != -1) {
                tConTable[idx].defaultVal = tConTable[idx].normalVal;

                if(tConTable[idx].isValid == 0)
                    set_value(tConTable[idx].entry.c_str(), tConTable[idx].normalVal);
            }
            else
                tConTable[idx].defaultVal = get_int_value(tConTable[idx].entry.c_str());
            ALOGD("[loadConTable] cmd:%s, path:%s, normal:%d, default:%d", tConTable[idx].cmdName.c_str(),
                tConTable[idx].entry.c_str(), tConTable[idx].normalVal, tConTable[idx].defaultVal);;

            // initial setting should be an invalid value
            if(tConTable[idx].comp == LESS)
                tConTable[idx].resetVal = tConTable[idx].maxVal + 1;
            else
                tConTable[idx].resetVal = tConTable[idx].minVal - 1;
            tConTable[idx].curVal = tConTable[idx].resetVal;

            idx++;
        }
    }
    else {
        ALOGI("Cannot open this file");
        return 0;
    }
    infile.close();

    return 1;
}

