#define DEBUG_LOG_TAG "SYNC"
//#define LOG_NDEBUG 0

#include "hwc_priv.h"

#include <sync/sync.h>
#include <sw_sync.h>

#include "utils/debug.h"

#include "dispatcher.h"
#include "sync.h"
#include "queue.h"
#include "hwdev.h"
#include "overlay.h"

//#define DUMP_SYNC_INFO

#ifndef USE_NATIVE_FENCE_SYNC
//#error "Native Fence Sync is not supported!!"
#endif

#define SYNC_LOGV(x, ...) HWC_LOGV("(%p) " x, this, ##__VA_ARGS__)
#define SYNC_LOGD(x, ...) HWC_LOGD("(%p) " x, this, ##__VA_ARGS__)
#define SYNC_LOGI(x, ...) HWC_LOGI("(%p) " x, this, ##__VA_ARGS__)
#define SYNC_LOGW(x, ...) HWC_LOGW("(%p) " x, this, ##__VA_ARGS__)
#define SYNC_LOGE(x, ...) HWC_LOGE("(%p) " x, this, ##__VA_ARGS__)

// ---------------------------------------------------------------------------

SyncFence::SyncFence(int client)
    : m_client(client)
    , m_sync_timeline_fd(-1)
    , m_curr_marker(0)
    , m_last_marker(0)
{
}

SyncFence::~SyncFence()
{
    if (m_sync_timeline_fd != -1)
    {
        SYNC_LOGD("Close timeline(%d) Clear Marker(%u->%u)",
            m_sync_timeline_fd, m_curr_marker, m_last_marker);
        int diff = m_last_marker - m_curr_marker;
        while (diff--) inc(-1);
        close(m_sync_timeline_fd);
    }
}

status_t SyncFence::initLocked()
{
#ifdef USE_NATIVE_FENCE_SYNC
    m_sync_timeline_fd = sw_sync_timeline_create();
    if (m_sync_timeline_fd < 0)
    {
        SYNC_LOGE("Failed to create sw_sync_timeline: %s", strerror(errno));
        m_sync_timeline_fd = -1;
        return INVALID_OPERATION;
    }

    SYNC_LOGD("Open timeline(%d)", m_sync_timeline_fd);
#endif

    return NO_ERROR;
}

status_t SyncFence::wait(int fd, int timeout, const char* log_name)
{
    char atrace_tag[256];
    sprintf(atrace_tag, "wait_fence(%d)\n", fd);
    HWC_ATRACE_NAME(atrace_tag);

    if (fd == -1) return NO_ERROR;

    int err = sync_wait(fd, timeout);
    if (err < 0 && errno == ETIME)
    {
        HWC_ATRACE_NAME("timeout");

        SYNC_LOGE("[%s] (%d) fence %d didn't signal in %u ms",
            log_name, m_client, fd, timeout);

        dumpLocked(fd);
    }

    close(fd);

    SYNC_LOGV("[%s] (%d) wait and close fence %d within %d",
        log_name, m_client, fd, timeout);

    return err < 0 ? -errno : status_t(NO_ERROR);
}

status_t SyncFence::waitForever(int fd, int warning_timeout, const char* log_name)
{
    if (fd == -1) return NO_ERROR;

    int err = sync_wait(fd, warning_timeout);
    if (err < 0 && errno == ETIME)
    {
        SYNC_LOGE("[%s] (%d) fence %d didn't signal in %u ms",
            log_name, m_client, fd, warning_timeout);

        dumpLocked(fd);
        err = sync_wait(fd, TIMEOUT_NEVER);
    }

    close(fd);

    SYNC_LOGV("[%s] (%d) wait and close fence %d", log_name, m_client, fd);

    return err < 0 ? -errno : status_t(NO_ERROR);
}

void SyncFence::dump(int fd)
{
    AutoMutex l(m_lock);
    dumpLocked(fd);
}

unsigned int SyncFence::dumpLocked(int fd)
{
    // sync fence info
    SYNC_LOGI("timeline fd(%d) curr(%u) last(%u)",
        m_sync_timeline_fd, m_curr_marker, m_last_marker);

    if (-1 == fd) return -1;

    // sync point info
    int timeline_count = -1;
    struct sync_fence_info_data *info = sync_fence_info(fd);
    if (info)
    {
        struct sync_pt_info *pt_info = NULL;
        // status: active(0) signaled(1) error(<0)
        SYNC_LOGI("fence(%s) status(%d)\n", info->name, info->status);

        // iterate all sync points
        while ((pt_info = sync_pt_info(info, pt_info)))
        {
            if (NULL != pt_info)
            {
                int ts_sec = pt_info->timestamp_ns / 1000000000LL;
                int ts_usec = (pt_info->timestamp_ns % 1000000000LL) / 1000LL;

                SYNC_LOGI("sync point: timeline(%s) drv(%s) status(%d) sync_drv(%u) timestamp(%d.%06d)",
                        pt_info->obj_name,
                        pt_info->driver_name,
                        pt_info->status,
                        *(uint32_t *)pt_info->driver_data,
                        ts_sec, ts_usec);

                timeline_count = *(uint32_t *)pt_info->driver_data;
            }
        }
        sync_fence_info_free(info);
    }

    return timeline_count;
}

int SyncFence::create()
{
    AutoMutex l(m_lock);

    if (m_sync_timeline_fd < 0)
    {
        SYNC_LOGW("create fence fail: timeline doesn't exist, try to create");
        if (NO_ERROR != initLocked()) return -1;
        SYNC_LOGD("timeline is created");
    }

    m_last_marker = m_last_marker + 1;

    int fd = sw_sync_fence_create(m_sync_timeline_fd, DEBUG_LOG_TAG, m_last_marker);
    if (fd < 0)
    {
        SYNC_LOGE("can't create sync point: %s", strerror(errno));
        return -1;
    }

    SYNC_LOGD("create fence(%d) curr(%u) last(%u)", fd, m_curr_marker, m_last_marker);

#ifdef DUMP_SYNC_INFO
    dumpLocked(fd);
#endif

    return fd;
}

status_t SyncFence::inc(int fd)
{
    AutoMutex l(m_lock);

    if (m_sync_timeline_fd < 0)
    {
        SYNC_LOGE("signal fence fail: timeline doesn't exist");
        return INVALID_OPERATION;
    }

    m_curr_marker = m_curr_marker + 1;

    int err = sw_sync_timeline_inc(m_sync_timeline_fd, 1);
    if (err < 0)
    {
        SYNC_LOGE("can't increment sync object: %s", strerror(errno));
        // align sync counter to driver data
        m_curr_marker = dumpLocked(fd);
        return -errno;
    }

    SYNC_LOGD("inc fence(%d) curr(%u) last(%u)", fd, m_curr_marker, m_last_marker);

#ifdef DUMP_SYNC_INFO
    dumpLocked(fd);
#endif

    return NO_ERROR;
}

int SyncFence::merge(int fd1, int fd2, const char* name)
{
    int fd3;

    if (fd1 >= 0 && fd2 >= 0)
    {
        fd3 = sync_merge(name, fd1, fd2);
    }
    else if (fd1 >= 0)
    {
        fd3 = sync_merge(name, fd1, fd1);
    }
    else if (fd2 >= 0)
    {
        fd3 = sync_merge(name, fd2, fd2);
    }
    else
    {
        return -1;
    }

    // check status of merged fence
    if (fd3 < 0)
    {
        HWC_LOGE("merge fences(%d, %d) fail: %s (%d)", fd1, fd2, strerror(errno), -errno);
        return -1;
    }

    HWC_LOGD("merge fences(%d, %d) into fence(%d)", fd1, fd2, fd3);

    return fd3;
}

// ---------------------------------------------------------------------------

SyncControl::~SyncControl()
{
    m_listener = NULL;
}

void SyncControl::setSync(DispatcherJob* job)
{
    job->need_sync = ((int)job->fbt_exist + job->num_ui_layers) && job->num_mm_layers;
    return;
}

void SyncControl::wait(DispatcherJob* job)
{
    HWC_ATRACE_NAME("wait_sync");

    AutoMutex l(m_lock);

    while (job->need_sync)
    {
        HWC_LOGD("(%d) Wait bliter done", job->disp_ori_id);
        m_condition.waitRelative(m_lock, ms2ns(5));
    }
}

void SyncControl::setOverlay(DispatcherJob* job)
{
    if (job->need_sync)
    {
        job->need_sync = false;
        m_condition.signal();
        return;
    }

    if (!job->triggered)
    {
        m_listener->onTrigger(job);
        job->triggered = true;
    }
    else
    {
        if (job->disp_ori_id < HWC_DISPLAY_VIRTUAL)
            m_listener->onTrigger(job);
    }
}
