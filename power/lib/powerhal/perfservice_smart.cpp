
#define LOG_TAG "libPowerHal"

#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>
#include <cutils/properties.h>
#include <utils/Log.h>
#include "perfservice.h"
#include "common.h"
#include <linux/sched.h>
#include <linux/mt_sched.h>
#include <errno.h>

/**************/
/* definition */
/**************/
#define VERBOSE 1

#define PACK_NUM_MAX    128
#define PACK_NAME_MAX   128

#define BENCHMARK_SCORE_THRESHOLD   75
#define HPS_CHECK_DURATION          (160)

#if 0
#define PATH_SMART_HPS_IS_HEAVY         "/sys/class/misc/m_smart_misc/hps_is_heavy"
#define PATH_SMART_HPS_CHECK_DURATION   "/sys/class/misc/m_smart_misc/hps_check_duration"
#define PATH_SMART_HPS_VALID_DURATION   "/sys/class/misc/m_smart_misc/hps_valid_duration"
#else
#define PATH_SMART_HPS_IS_HEAVY             "/proc/perfmgr/smart/hps_is_heavy"
#define PATH_SMART_HPS_CHECK_DURATION       "/proc/perfmgr/smart/hps_check_duration"
#define PATH_SMART_HPS_VALID_DURATION       "/proc/perfmgr/smart/hps_check_last_duration"
#define PATH_SMART_APP_IS_BENCHMARK         "/proc/perfmgr/smart/app_is_sports"
//#define PATH_SMART_FLIPER_IS_HEAVY          "/proc/fliper_is_heavy"
//#define PATH_SMART_FLIPER_CHECK_DURATION    "/proc/fliper_check_duration"
#endif

#define CUS_BENCHMARK_TABLE         "/data/vendor/powerhal/smart"
#define CUS_GAME_TABLE              "/data/vendor/powerhal/game"
#define CUS_DEBUG_TABLE             "/data/vendor/powerhal/smart_debug"
#define CUS_DEBUG_TABLE_TMP         "/data/vendor/powerhal/smart_debug_tmp"

/*************/
/* data type */
/*************/
int index_weight[INDEX_NUM] = {
    -300,   /* INDEX_SYSTEM_APK     */
    -300,   /* INDEX_USER_SPECIFIED */
    -300,   /* INDEX_GAME_HINT      */
      50,   /* INDEX_ARM_NATIVE_LIB */
      50,   /* INDEX_HPS    */
      50,   /* INDEX_VCORE  */
      50,   /* INDEX_TLP    */
};

int index_valid[INDEX_NUM] = {
       1,   /* INDEX_SYSTEM_APK     */
       1,   /* INDEX_USER_SPECIFIED */
       1,   /* INDEX_GAME_HINT      */
       0,   /* INDEX_ARM_NATIVE_LIB */
      50,   /* INDEX_HPS    */
       0,   /* INDEX_VCORE  */
      50,   /* INDEX_TLP    */
};

typedef struct tPackNode {
    char pack_name[PACK_NAME_MAX];
    int  score[INDEX_NUM];
    int  total_score;
    int  pid;
    int  uid;
} tPackNode;

typedef struct tPackList {
    tPackNode list[PACK_NUM_MAX];
    int num;
} tPackList;

/*******************/
/* global variable */
/*******************/
struct tPackNode appDefaultBenchmark[] = {
    /* example
    * {"com.android.blah.blah", {}, 0, 0, 0},
    */
    {"", {}, 0, 0, 0},
};

struct tPackNode appNotBenchmark[] = {
    /* example
    * {"com.android.blah.blah", {}, 0, 0, 0},
    */
    {"", {}, 0, 0, 0},
};

static int bSmartDetEnabled = 0;
static int bSmartDebugMode = 0;

static tPackList gtBenchmarkList;
static tPackList gtGameList;
static tPackList gtNotBenchmarkList; // APP list which should be not benchmark
//static tPackList gtBebugAppList;   // remove redundant variable
static tPackNode gtCurrPack;

static int nTableIsReady = 0;
static int bHpsDetect = 0;

extern int nDebugLogEnable;
extern int nVerboseLogEnable;

#ifdef ALOGD
#undef ALOGD
#define ALOGD(...) do{if(nDebugLogEnable)((void)ALOG(LOG_INFO, LOG_TAG, __VA_ARGS__));}while(0)
#endif

#ifdef ALOGV
#undef ALOGV
#define ALOGV(...) do{if(nVerboseLogEnable)((void)ALOG(LOG_INFO, LOG_TAG, __VA_ARGS__));}while(0)
#endif

/******************/
/* local function */
/******************/
static int add_new_pack(int type, tPackNode *curr, int set) // set:1 => write to file
{
    tPackList *pList = NULL;
    int num = 0;
    char file[64];
    FILE *ifp;

    if(type < 0 || type > APK_GAME)
        return -1;

    if(type == APK_BENCHMARK) {
        pList = &gtBenchmarkList;
        set_str_cpy(file, CUS_BENCHMARK_TABLE, 64);
    }
    else {
        pList = &gtGameList;
        set_str_cpy(file, CUS_GAME_TABLE, 64);
    }
    num = pList->num;

    ALOGD("[add_new_pack] type:%d, num:%d, pack:%s", type, num, curr->pack_name);

    if(num >= PACK_NUM_MAX)
        return -1;

    set_str_cpy(pList->list[num].pack_name, curr->pack_name, PACK_NAME_MAX);
    pList->list[num].total_score = curr->total_score;
    memcpy(pList->list[num].score, curr->score, INDEX_NUM * sizeof(int));
    pList->num += 1;

    /* write to file */
    if(set) {
        if((ifp = fopen(file,"a")) == NULL) {
            ALOGI("[add_new_pack] open file fail");
            return 0;
        }

        fprintf(ifp, "%s %d %d %d %d %d %d %d %d\n", curr->pack_name, curr->score[0], curr->score[1], curr->score[2], curr->score[3], \
            curr->score[4], curr->score[5], curr->score[6], curr->total_score);
        fclose(ifp);
    }

    return 0;
}

#if 0 // Remove redundant operation. It will be deleted in next Android version
static int add_new_debug_pack(tPackNode *curr, int set) // set:1 => write to file
{
    int num = gtBebugAppList.num, i, j, match = 0;
    FILE *ifp, *ofp;
    char buf[128];

    ALOGI("[add_new_debug_pack] num:%d, pack:%s", num, curr->pack_name);

    if(num >= PACK_NUM_MAX)
        return -1;

    for(i=0; i<gtBebugAppList.num; i++) {
        if(!strcmp(gtBebugAppList.list[i].pack_name, curr->pack_name)) {
            if(curr->total_score <= gtBebugAppList.list[i].total_score)
                return 0;

            for(j=0; j<INDEX_NUM; j++)
                gtBebugAppList.list[i].score[j] = curr->score[j];
            gtBebugAppList.list[i].pid = curr->pid;
            gtBebugAppList.list[i].total_score = curr->total_score;

            if(set) {
                if((ifp = fopen(CUS_DEBUG_TABLE, "r")) == NULL) {
                    ALOGI("[add_new_debug_pack] open file fail");
                    return 0;
                }
                if((ofp = fopen(CUS_DEBUG_TABLE_TMP, "w")) == NULL) {
                    ALOGI("[add_new_debug_pack] open file fail");
                    fclose(ifp);
                    return 0;
                }
                while(fgets(buf, 128, ifp) != NULL) {
                    if(strncmp(buf, curr->pack_name, strlen(curr->pack_name)))
                        fputs(buf, ofp);
                    else
                        fprintf(ofp, "%s %d %d %d %d %d %d %d %d\n", curr->pack_name, curr->score[0], curr->score[1], curr->score[2], curr->score[3], \
                        curr->score[4], curr->score[5], curr->score[6], curr->total_score);
                }
                fclose(ifp);
                fclose(ofp);
                if(remove(CUS_DEBUG_TABLE) == 0) {
                    if(rename(CUS_DEBUG_TABLE_TMP, CUS_DEBUG_TABLE) != 0) {
                        ALOGI("[add_new_debug_pack] rename fail");
                    }
                }
            }

            return 0;
        }
    }

    set_str_cpy(gtBebugAppList.list[num].pack_name, curr->pack_name, 128);
    gtBebugAppList.list[num].total_score = curr->total_score;
    memcpy(gtBebugAppList.list[num].score, curr->score, INDEX_NUM * sizeof(int));
    gtBebugAppList.num += 1;

    /* write to file */
    if(set) {
        if((ifp = fopen(CUS_DEBUG_TABLE,"a")) == NULL) {
            ALOGI("[add_new_pack] open file fail");
            return 0;
        }

        fprintf(ifp, "%s %d %d %d %d %d %d %d %d\n", curr->pack_name, curr->score[0], curr->score[1], curr->score[2], curr->score[3], \
            curr->score[4], curr->score[5], curr->score[6], curr->total_score);
        fclose(ifp);
    }

    return 0;
}
#endif

static int smart_reset_flags(void)
{
    if(bHpsDetect)
        set_value(PATH_SMART_HPS_IS_HEAVY, 0);
    //set_int_value(PATH_SMART_FLIPER_IS_HEAVY, 0);
    return 0;
}

/*******************/
/* global function */
/*******************/
int smart_init(void)
{
    int i;
    FILE *ifp;
    tPackNode curr;
    char buf[256];

    /* check kernel function */
    if(access(PATH_SMART_HPS_IS_HEAVY, W_OK) != -1)
        bHpsDetect = 1;

    gtBenchmarkList.num = 0;
    for(i=0; i<PACK_NUM_MAX; i++) {
        gtBenchmarkList.list[i].pack_name[0] = '\0';
        gtBenchmarkList.list[i].total_score = 0;
        gtBenchmarkList.list[i].pid = -1;
        gtBenchmarkList.list[i].uid = -1;
        memset(gtBenchmarkList.list[i].score, 0, INDEX_NUM * sizeof(int));
    }

    /* detect native benchmark */
    set_value("/proc/perfmgr/smart/native_is_running", 0);

    set_value(PATH_SMART_HPS_CHECK_DURATION, HPS_CHECK_DURATION); // 200ms
    //set_int_value(PATH_SMART_FLIPER_CHECK_DURATION, 1000); // 1000ms

    /* mkdir */
    /*if(mkdir("/data/tmp", S_IRWXU | S_IRWXG | S_IROTH)!=0) {
        ALOGD("[smart_init] mkdir fail");
    }*/ /*--mkdir in rc file--*/

    /* white list (all apps listed here are benchmark) */
    memset(&curr, 0, sizeof(tPackNode));
    for(i=0; strlen(appDefaultBenchmark[i].pack_name) > 0; i++) {
        strncpy(curr.pack_name, appDefaultBenchmark[i].pack_name, PACK_NAME_MAX-1);
        add_new_pack(APK_BENCHMARK, &curr, 0);
    }

    /* black list (all apps listed here are not benchmark) */
    for(i=0; strlen(appNotBenchmark[i].pack_name) > 0; i++) {
        strncpy(gtNotBenchmarkList.list[i].pack_name, appNotBenchmark[i].pack_name, PACK_NAME_MAX-1);
        gtNotBenchmarkList.num += 1;
    }

    /* benchmark: read from file */
    /* move to smart_table_init */
    return 0;
}

int smart_table_init_flag(void)
{
    return nTableIsReady;
}

int smart_table_init(void)
{
    int i;
    FILE *ifp;
    tPackNode curr;
    char buf[256];

    memset(&curr, 0, sizeof(tPackNode));

    /* benchmark: read from file */
    do {
        if((ifp = fopen(CUS_BENCHMARK_TABLE,"r")) == NULL) {
            ALOGI("[smart_table_init] open file fail");
            break;
        }

        ALOGI("[smart_table_init] open benchmark file");

        while(fgets(buf, 256, ifp)){
            sscanf(buf, "%127s %d %d %d %d %d %d %d %d", curr.pack_name, curr.score, curr.score+1, curr.score+2, curr.score+3, \
                curr.score+4, curr.score+5, curr.score+6, &(curr.total_score));
            add_new_pack(APK_BENCHMARK, &curr, 0);
        }
        fclose(ifp);
    } while(0);

    /* GAME: read from file */
    do {
        if((ifp = fopen(CUS_GAME_TABLE,"r")) == NULL) {
            ALOGI("[smart_table_init] open file fail");
            break;
        }

        ALOGI("[smart_table_init] open game file");
        while(fgets(buf, 256, ifp)){
            sscanf(buf, "%127s %d %d %d %d %d %d %d %d", curr.pack_name, curr.score, curr.score+1, curr.score+2, curr.score+3, \
                curr.score+4, curr.score+5, curr.score+6, &(curr.total_score));
            add_new_pack(APK_GAME, &curr, 0);
        }
        fclose(ifp);
    } while(0);

    nTableIsReady = 1;

    return 1;
}

int smart_control(int enable)
{
    bSmartDetEnabled = (enable) ? 1 : 0;
    return 0;
}

int smart_reset(const char *pack, int pid)
{
    //smart_control(1); // don't enable detection in this func
    smart_reset_flags();

    /* reset current package */
    strncpy(gtCurrPack.pack_name, pack, PACK_NAME_MAX-1);
    gtCurrPack.pack_name[PACK_NAME_MAX-1] = '\0';
    gtCurrPack.total_score = 0;
    gtCurrPack.pid = pid;
    memset(gtCurrPack.score, 0, sizeof(int)*INDEX_NUM);
    return 0;
}

int smart_check_pack_existed(int type, const char *pack, int uid, int fromUid)
{
    int i;
    tPackList *pList = NULL;
    int num = 0;

    if(type == APK_BENCHMARK) {
        pList = &gtBenchmarkList;
    }
    else if(type == APK_GAME) {
        pList = &gtGameList;
    }
    else if(type == APK_NOT_BENCHMARK) {
        pList = &gtNotBenchmarkList;
    }
    else
        return -1;

    num = pList->num;

    for(i=0; i<num; i++) {
        if(!strncmp(pList->list[i].pack_name, pack, strlen(pack))) {
            ALOGD("[smart_check_pack_existed] type:%d, found", type);
            pList->list[i].uid = uid;
            return i;
        }
        else if(type == APK_BENCHMARK && pList->list[i].uid == fromUid) {  // launch from a benchmark => also enter benchmark mode
            ALOGI("[smart_check_pack_existed] type:%d, pack:%s, from uid found", type, pack);
            return i;
        }
    }
    ALOGD("[smart_check_pack_existed] pack:%s, type:%d, not found", pack, type);
    return -1;
}

int smart_set_benchmark_mode(int benchmark)
{
    if(benchmark)
        set_value(PATH_SMART_APP_IS_BENCHMARK, 1);
    else
        set_value(PATH_SMART_APP_IS_BENCHMARK, 0);
    return 0;
}

int smart_is_det_enable(void)
{
    return bSmartDetEnabled;
}

int smart_get_tlp(int pid)
{
    DIR *d;
    struct dirent *de;
    char tmp[128];
    int num = 0, tmp_pid;
    FILE *ifp;
    char  path[128], buf[256], comm[128], tmp2[128], state;

    if(!bSmartDetEnabled)
        return 0;

    sprintf(tmp,"/proc/%d/task",pid);
    d = opendir(tmp);
    if(d == 0) return -1;

    while ((de = readdir(d)) != 0) {
        if (isdigit(de->d_name[0])) {
            int thread = atoi(de->d_name);
            sprintf(path, "/proc/%d/status", thread);
			if((ifp = fopen(path,"r")) == NULL) {
                num = 0;
                continue;
			}
            //fscanf(ifp, "%d (%s) %c", &tmp_pid, comm, &state);
            //ALOGI("[get_tlp] pid:%d, comm:%s, state:%c", tmp_pid, comm, state);
            if(fgets(buf, 256, ifp) == NULL) {
                num = 0;
                fclose(ifp);
                continue;
            }
            if(fgets(buf, 256, ifp) == NULL) {
                num = 0;
                fclose(ifp);
                continue;
            }
            sscanf(buf, "%127s %c", tmp2, &state);
            //ALOGI("[get_tlp] pid:%d, state:%c", thread, state);
            if(state == 'R')
                num++;
            fclose(ifp);
        }
    }
    ALOGD("[smart_get_tlp] pid:%d, TLP:%d", pid, num);

    closedir(d);
    return num;
}

int smart_dump_info(void)
{
    int i;
    tPackNode *pCurr;

    ALOGI("=== benchmark ===");
    for(i=0; i<gtBenchmarkList.num; i++) {
        pCurr = gtBenchmarkList.list + i;
        ALOGI("%s %d %d %d %d %d %d %d %d", pCurr->pack_name, pCurr->score[0], pCurr->score[1], pCurr->score[2], pCurr->score[3], \
            pCurr->score[4], pCurr->score[5], pCurr->score[6], pCurr->total_score);
    }

    ALOGI("=== game ===");
    for(i=0; i<gtGameList.num; i++) {
        pCurr = gtGameList.list + i;
        ALOGI("%s", pCurr->pack_name);
    }
    return 0;
}

int smart_update_score(int index, int set) // set: 1 means this condition is satisfied
{
    int curr_score, new_score, t_num, curr_total = 0;

    // smart det is enables 3sec later. System apk is updated before smart det enabled
    if(!bSmartDetEnabled && index != INDEX_SYSTEM_APK)
        return -1;

    //ALOGI("[smart_update_score] index:%d, set:%d", index, set);

    if(index >= INDEX_NUM)
        return -1;

    curr_score = gtCurrPack.score[index];
    new_score = (set) ? index_weight[index] : 0;
    curr_total = gtCurrPack.total_score;
    if(new_score == curr_score) {
        ALOGD("[smart_update_score] index:%d, set:%d", index, set);
        return 0;
    }
    else {
        gtCurrPack.score[index] = new_score;
        if(index_valid[index])
            gtCurrPack.total_score += (new_score - curr_score);
        ALOGD("[smart_update_score] index:%d, set:%d, total:%d", index, set, gtCurrPack.total_score);
    }

    if(gtCurrPack.total_score > BENCHMARK_SCORE_THRESHOLD && curr_total <= BENCHMARK_SCORE_THRESHOLD) {
        ALOGI("[smart_update_score] detect");
        add_new_pack(APK_BENCHMARK, &gtCurrPack, 1);
        smart_control(0);
        return 1;  // return 1 => benchmark
    }

    return 0;
}

int smart_add_benchmark(void)
{
    ALOGD("smart_add_benchmark");
    if(gtCurrPack.score[INDEX_SYSTEM_APK] >= 0) // not system apk
        add_new_pack(APK_BENCHMARK, &gtCurrPack, 1);  // set:1 => write to file
    return 0;
}

int smart_add_game(void)
{
    ALOGD("smart_add_game");
    if(gtCurrPack.score[INDEX_SYSTEM_APK] >= 0) // not system apk
        add_new_pack(APK_GAME, &gtCurrPack, 1);  // set:1 => write to file
    return 0;
}

int smart_update_pack(void)
{
    //add_new_debug_pack(&gtCurrPack, 1);
    return 0;
}

