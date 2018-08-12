#include <sys/types.h>
#include <utils/Log.h>
#include <utils/Timers.h>
#include <fcntl.h>      /* open */
#include <unistd.h>     /* exit */
#include <sys/ioctl.h>  /* ioctl */
#include "perfctl.h"
#include <linux/types.h>
#include <errno.h>
#include <string.h>
#include <utils/Timers.h>

#define LOG_TAG "libPowerHalCtl"
#ifdef ALOGD
#undef ALOGD
#define ALOGD(...) do{((void)ALOG(LOG_INFO, LOG_TAG, __VA_ARGS__));}while(0)
#endif

#define RENDER_THREAD_UPDATE_DURATION   250000000
#define PATH_PERF_IOCTL "/proc/perfmgr/perf_ioctl"

int devfd = -1;

static inline int check_perf_ioctl_valid(void)
{
    if (devfd >= 0) {
        return 0;
    } else if (devfd == -1) {
        devfd = open(PATH_PERF_IOCTL, O_RDONLY);
        // file not exits
        if (devfd < 0 && errno == ENOENT)
            devfd = -2;
        // file exist, but can't open
        if (devfd == -1) {
            ALOGD("Can't open %s: %s", PATH_PERF_IOCTL, strerror(errno));
            return -1;
        }
    // file not exist
    } else if (devfd == -2) {
        //ALOGD("Can't open %s: %s", PATH_PERF_IOCTL, strerror(errno));
        return -2;
    }
    return 0;
}

extern "C"
int fbcNotifyActSwitch(int enable)
{
#if defined(MTK_AFPSGO_FBT_GAME)
    FPSGO_PACKAGE msg;
    msg.tid = gettid();
    msg.frame_time = enable;
    msg.render_method = SWUI;
    if (check_perf_ioctl_valid() == 0)
        ioctl(devfd, FPSGO_ACT_SWITCH, &msg);

#endif
    return 0;
}

extern "C"
int fbcNotifyGame(int enable)
{
#if defined(MTK_AFPSGO_FBT_GAME)
    FPSGO_PACKAGE msg;
    msg.tid = gettid();
    msg.frame_time = enable;
    msg.render_method = SWUI;
    if (check_perf_ioctl_valid() == 0)
        ioctl(devfd, FPSGO_GAME, &msg);

#endif
    return 0;
}

extern "C"
int fbcNotifyTouch(int enable)
{
    FPSGO_PACKAGE msg;
    msg.tid = gettid();
    msg.frame_time = enable;
    msg.render_method = SWUI;
    if (check_perf_ioctl_valid() == 0)
        ioctl(devfd, FPSGO_TOUCH, &msg);

    return 0;
}

#if defined(MTK_AFPSGO_FBT_GAME)
static int gl_tid = 0;
static int qudeq_time = 0;
static int64_t qubeg_ts = 0, deqbeg_ts = 0;
#endif

extern "C"
int fbcNotifyFrameComplete(int tid, int duration, __u32 type)
{
#if defined(MTK_AFPSGO_FBT_GAME)
    FPSGO_PACKAGE msg;
    msg.tid = tid;
    msg.frame_time = duration;
    msg.render_method = type;

    if (type == GLSURFACE) {
        if (gl_tid) {
            msg.frame_time -= qudeq_time;
            qudeq_time = 0;
        } else
            gl_tid = tid;
    } else
        gl_tid = qubeg_ts = deqbeg_ts = 0;

    if (check_perf_ioctl_valid() == 0)
        ioctl(devfd, FPSGO_FRAME_COMPLETE, &msg);
#else
    FPSGO_PACKAGE msg;
    static nsecs_t mPreviousTime = 0;
    nsecs_t now = systemTime(CLOCK_MONOTONIC);

    msg.tid = tid;
    msg.frame_time = duration;
    msg.render_method = type;

    if(mPreviousTime == 0 || (now - mPreviousTime) > RENDER_THREAD_UPDATE_DURATION) { // 400ms
        if (check_perf_ioctl_valid() == 0)
            ioctl(devfd, FPSGO_FRAME_COMPLETE, &msg);
        mPreviousTime = now;
    }
#endif
    return 0;
}

extern "C"
int fbcNotifyIntendedVsync(void)
{
#if defined(MTK_AFPSGO_FBT_GAME)
    FPSGO_PACKAGE msg;
    msg.tid = gettid();
    msg.frame_time = 0;
    msg.render_method = SWUI;
    if (check_perf_ioctl_valid() == 0)
        ioctl(devfd, FPSGO_INTENDED_VSYNC, &msg);

#endif
    return 0;
}

extern "C"
int fbcNotifyNoRender(void)
{
#if defined(MTK_AFPSGO_FBT_GAME)
    FPSGO_PACKAGE msg;
    msg.tid = gettid();
    msg.frame_time = 0;
    msg.render_method = SWUI;
    if (check_perf_ioctl_valid() == 0)
        ioctl(devfd, FPSGO_NO_RENDER, &msg);

#endif
    return 0;
}

extern "C"
int fbcNotifySwapBuffers(void)
{
#if defined(MTK_AFPSGO_FBT_GAME)
    FPSGO_PACKAGE msg;
    msg.tid = gettid();
    msg.frame_time = 0;
    msg.render_method = SWUI;
    if (check_perf_ioctl_valid() == 0)
        ioctl(devfd, FPSGO_SWAP_BUFFER, &msg);

#endif
    return 0;
}

extern "C"
int xgfNotifyQueue(__u32 value, __u64 bufID)
{
#if defined(MTK_AFPSGO_FBT_GAME)
    FPSGO_PACKAGE msg;
    msg.tid = gettid();
    msg.start = value;
    msg.bufID = bufID;

    if (gl_tid) {
        int64_t now_ts = systemTime(CLOCK_MONOTONIC);
        if (value == 1)
            qubeg_ts = systemTime(CLOCK_MONOTONIC);
        else if (value == 0 && qubeg_ts != 0 && now_ts > qubeg_ts)
            qudeq_time += (now_ts - qudeq_time);
    }

    if (check_perf_ioctl_valid() == 0)
        ioctl(devfd, FPSGO_QUEUE, &msg);

#endif
    return 0;
}

extern "C"
int xgfNotifyDequeue(__u32 value, __u64 bufID)
{
#if defined(MTK_AFPSGO_FBT_GAME)
    FPSGO_PACKAGE msg;
    msg.tid = gettid();
    msg.start = value;
    msg.bufID = bufID;

    if (gl_tid) {
        int64_t now_ts = systemTime(CLOCK_MONOTONIC);
        if (value == 1)
            deqbeg_ts = systemTime(CLOCK_MONOTONIC);
        else if (value == 0 && deqbeg_ts != 0 && now_ts > deqbeg_ts)
            qudeq_time += (now_ts - deqbeg_ts);
    }

    if (check_perf_ioctl_valid() == 0)
        ioctl(devfd, FPSGO_DEQUEUE, &msg);

#endif
    return 0;
}

extern "C"
int xgfNotifyConnect(__u32 value, __u64 bufID, __u32 connectedAPI)
{
#if defined(MTK_AFPSGO_FBT_GAME)
    FPSGO_PACKAGE msg;
    msg.tid = gettid();
    msg.bufID = bufID;
    if (value)
        msg.connectedAPI = connectedAPI;
    else
        msg.connectedAPI = 0;
    if (check_perf_ioctl_valid() == 0)
        ioctl(devfd, FPSGO_QUEUE_CONNECT, &msg);

#endif
    return 0;
}

extern "C"
int xgfNotifyVsync(__u32 value)
{
#if defined(MTK_AFPSGO_FBT_GAME)
    FPSGO_PACKAGE msg;
    msg.tid = gettid();
    msg.frame_time = value;
    msg.render_method = SWUI;
    if (check_perf_ioctl_valid() == 0)
        ioctl(devfd, FPSGO_VSYNC, &msg);

#endif
    return 0;
}
