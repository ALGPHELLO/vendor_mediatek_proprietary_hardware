
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

#define VERBOSE 1

#ifndef SCHED_FIFO
#define SCHED_FIFO              1
#endif

#ifndef SCHED_RESET_ON_FORK
#define SCHED_RESET_ON_FORK     0x40000000
#endif

static int set_rootgroup(int tid, int fd);
static int set_nice(int tid, int nice);
static int set_fifo_policy(int tid, int priority);
static int set_rr_policy(int tid, int priority);

typedef int(*SETFUNC)(int tid, int param);
SETFUNC setfunc[SETFUNC_NUM] = {set_fifo_policy, set_rr_policy, set_rootgroup, set_nice};

static const char *nexttoksep(char **strp, const char *sep)
{
    char *p = strsep(strp, sep);
    return (p == 0) ? "" : p;
}
static const char *nexttok(char **strp)
{
    return nexttoksep(strp, " ");
}

//static int boost_tid_to_cgroup(int tid)
static int set_rootgroup(int tid, int fd)
{
//  int fd = system_cgroup_fd;
    int tid_backup = tid;
#if VERBOSE
    ALOGI("set_rootgroup - tid: %d",tid);
#endif

    if (fd < 0) {
        SLOGE("set_rootgroup failed due to fd error, tid:%d\n", tid_backup);
        return -1;
    }

    // specialized itoa -- works for tid > 0
    char text[22];
    char *end = text + sizeof(text) - 1;
    char *ptr = end;
    *ptr = '\0';
    while (tid > 0) {
        *--ptr = '0' + (tid % 10);
        tid = tid / 10;
    }

    if (write(fd, ptr, end - ptr) < 0) {
        if (errno == ESRCH)
            return 0;

        SLOGW("set_rootgroup failed to write '%s' (%s) \n", ptr, strerror(errno));
        return -1;
    }

    return 0;
}

static int set_nice(int tid, int nice)
{
    struct sched_param param;
    int setpolicy = 0;

#if VERBOSE
    ALOGI("set_nice - tid: %d, nice: %d", tid, nice);
#endif
    // temporary only configure RT1 pri.
    setpolicy |= SCHED_RESET_ON_FORK;
    param.sched_priority = 0;

    if (sched_setscheduler(tid, setpolicy, &param))
        ALOGI("set_normal_policy - set RT fail, %d, %s", tid, strerror(errno));


    if (setpriority(PRIO_PROCESS, tid, nice) < 0) {
        ALOGI("set_nice fail - tid: %d, nice: %d", tid, nice);
        return -1;
    }
    return 0;
}

static int set_fifo_policy(int tid, int priority)
{
    struct sched_param param;
    int setpolicy = 1;
#if VERBOSE
    ALOGI("set_fifo_policy - tid: %d, priority:%d", tid, priority);
#endif

    // temporary only configure RT1 pri.
    setpolicy |= SCHED_RESET_ON_FORK;
    param.sched_priority = priority;


    if (sched_setscheduler(tid, setpolicy, &param))
        ALOGI("set_fifo_policy - set RT fail, %d, %s", tid, strerror(errno));

    return 0;
}

static int set_rr_policy(int tid, int priority)
{
    struct sched_param param;
    int setpolicy = 2;
#if VERBOSE
    ALOGI("set_rr_policy - tid: %d, priority:%d", tid, priority);
#endif

    // temporary only configure RT1 pri.
    setpolicy |= SCHED_RESET_ON_FORK;
    param.sched_priority = priority;


    if (sched_setscheduler(tid, setpolicy, &param))
        ALOGI("set_rr_policy - set RT fail, %d, %s", tid, strerror(errno));

    return 0;
}

int check_name_by_tid (int tid, const char* proc_name)
{
    int r, fd;
    char statline[1024];
    struct stat stats;
    char *ptr, *name;
    char *strpos = NULL;

    memset(statline, 0, sizeof(char)*1024);
    //sprintf(statline, "/proc/%d", tid);
    //stat(statline, &stats);
    sprintf(statline, "/proc/%d/stat", tid);

    fd = open(statline, O_RDONLY);
    if(fd == -1) return -1;
    r = read(fd, statline, 1023);
    close(fd);
    if(r < 0) return -1;
    statline[r] = 0;

    ptr = statline;

    nexttok(&ptr); // skip pid
    ptr++;                  // skip "("

    name = ptr;
    ptr = strrchr(ptr, ')'); // Skip to *last* occurence of ')',
    if (ptr == NULL) return -1;
    *ptr++ = '\0';                   // and null-terminate name.

    strpos = strcasestr(name, PROC_NAME_REINIT); //reverse check
    if (strpos != NULL)
        return CHECK_RETRY;

    strpos = strcasestr(proc_name, name);
    if (strpos != NULL)
        return CHECK_PASS;
    else
        return CHECK_FAIL;
}


int boost_process_by_name(char* proc_name, char* thread_name, int setfuncid, int params)
{
    DIR *d;
    DIR *d2;
    struct dirent *de;
    struct dirent *de2;
    char tmp[128];
    int full = 0;

    if (setfuncid >= SETFUNC_NUM)
        return -1;

    if (!proc_name)
        return -1;

    if (!thread_name)
        full = 1;

    d = opendir("/proc");
    if(d == 0) return -1;
    while((de = readdir(d)) != 0){
        if(isdigit(de->d_name[0])){
            int tid = atoi(de->d_name);
            // check process name
            if(CHECK_PASS == check_name_by_tid(tid, proc_name)) {
                sprintf(tmp,"/proc/%d/task",tid);
                d2 = opendir(tmp);
                if(d2 == 0) {
                    closedir(d);
                    return -1;
                }
                while ((de2 = readdir(d2)) != 0) {
                    if (isdigit(de2->d_name[0])) {
                        int thread = atoi(de2->d_name);
                        if (full)
                            setfunc[setfuncid](thread, params);
                        else {
                            if (CHECK_PASS == check_name_by_tid(thread, thread_name))
                                setfunc[setfuncid](thread, params);
                        }
                    }
                }
                closedir(d2);
            }
        }
    }
    closedir(d);
    return 0;

}

int boost_process_by_tid(int tid, int full, int setfuncid, int params)
{
    DIR *d;
    struct dirent *de;
    char tmp[128];

    if (setfuncid >= SETFUNC_NUM || setfuncid < 0)
        return -1;

    if (!full) {
        setfunc[setfuncid](tid, params);
        return 0;
    }

    sprintf(tmp,"/proc/%d/task",tid);
    d = opendir(tmp);
    if(d == 0) return -1;

    while ((de = readdir(d)) != 0) {
        if (isdigit(de->d_name[0])) {
            int thread = atoi(de->d_name);
            int thread_prio = getpriority(PRIO_PROCESS, thread);
            if (thread_prio <= 0) // under such condition is required to get boost
                setfunc[setfuncid](thread, params);
        }
    }
    closedir(d);
    return 0;
}

