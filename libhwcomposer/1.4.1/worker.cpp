#define DEBUG_LOG_TAG "WKR"

#include "hwc_priv.h"

#include "utils/debug.h"
#include "dispatcher.h"
#include "overlay.h"
#include "worker.h"
#include "display.h"
#include "composer.h"
#include "bliter.h"
#include "sync.h"

// ---------------------------------------------------------------------------

HWCThread::~HWCThread()
{
    HWC_LOGI("Destroy thread %s", m_thread_name);
}

status_t HWCThread::readyToRun()
{
    m_state = HWC_THREAD_IDLE;

    return NO_ERROR;
}

void HWCThread::waitLocked()
{
    // make sure m_state is not out of expect
    assert((m_state == HWC_THREAD_TRIGGER) || (m_state == HWC_THREAD_IDLE));

    int timeout_count = 0;
    while (m_state == HWC_THREAD_TRIGGER)
    {
        if (m_condition.waitRelative(m_lock, ms2ns(16)) == TIMED_OUT)
        {
            int sem_value;
            sem_getvalue(&m_event, &sem_value);

            if (timeout_count & 0x1)
            {
                HWC_LOGW("Timed out waiting for %s (cnt=%d/val=%d)",
                        m_thread_name, timeout_count, sem_value);
            }

            timeout_count++;
        }
    }
}

void HWCThread::wait()
{
    HWC_ATRACE_CALL();
    AutoMutex l(m_lock);
    HWC_LOGD("Waiting for %s...", m_thread_name);
    waitLocked();
}

// ---------------------------------------------------------------------------

ComposeThreadBase::ComposeThreadBase(int dpy, const sp<SyncControl>& sync_ctrl)
    : HWCThread()
    , m_disp_id(dpy)
    , m_handler(NULL)
    , m_sync_ctrl(sync_ctrl)
    , m_next_job(NULL)
{ }

ComposeThreadBase::~ComposeThreadBase()
{
    m_handler = NULL;
    m_sync_ctrl = NULL;
}

void ComposeThreadBase::onFirstRef()
{
    run(m_thread_name, PRIORITY_URGENT_DISPLAY);
}

void ComposeThreadBase::loopHandlerLocked()
{
    if (m_next_job == NULL)
        return;

    // check if need to wait other compose thread
    barrier(m_next_job);

    m_handler->process(m_next_job);

    m_sync_ctrl->setOverlay(m_next_job);

    m_next_job = NULL;
}

bool ComposeThreadBase::threadLoop()
{
    sem_wait(&m_event);

    {
        AutoMutex l(m_lock);

#ifndef MTK_USER_BUILD
        HWC_ATRACE_NAME(m_trace_tag);
#endif

        loopHandlerLocked();

        m_state = HWC_THREAD_IDLE;
        m_condition.signal();
    }

    return true;
}

void ComposeThreadBase::set(
    struct hwc_display_contents_1* list,
    DispatcherJob* job)
{
    if (m_handler != NULL) m_handler->set(list, job);
}

void ComposeThreadBase::trigger(DispatcherJob* job)
{
    AutoMutex l(m_lock);

    m_next_job = job;

    m_state = HWC_THREAD_TRIGGER;

    sem_post(&m_event);
}

// ---------------------------------------------------------------------------

UILayerComposer::UILayerComposer(
    int dpy, const sp<SyncControl>& sync_ctrl, const sp<OverlayEngine>& ovl_engine)
    : ComposeThreadBase(dpy, sync_ctrl)
{
    m_handler = new ComposerHandler(m_disp_id, ovl_engine);

    snprintf(m_trace_tag, sizeof(m_trace_tag), "compose1_%d", dpy);

    snprintf(m_thread_name, sizeof(m_thread_name), "UICompThread_%d", dpy);
}

void UILayerComposer::barrier(DispatcherJob* job)
{
    m_sync_ctrl->wait(job);
}

// MULTIPASS: UIThread process multipass
void UILayerComposer::loopHandlerLocked()
{
    if (m_next_job == NULL)
        return;

    MULPASSLOGV("UILayerComposer", "TotalPassCnt: %d", m_next_job->hw_layers_pass_cnt);
    // check if need to wait other compose thread
    barrier(m_next_job);

    if (m_next_job->hw_layers_pass_cnt != 1)
    {
        int final_pass = m_next_job->hw_layers_pass_cnt - 1;
        for (int i = 0; i < final_pass; i++)
        {
            MULPASSLOGV("UILayerComposer", "PASS: %d", i);
            m_next_job->hw_layers_pass_now = i;
            m_handler->process(m_next_job);
            m_sync_ctrl->setOverlay(m_next_job);
        }
        m_next_job->hw_layers_pass_now = final_pass;

    }
    MULPASSLOGV("UILayerComposer", "PASS: %d (final)", m_next_job->hw_layers_pass_now);

    m_handler->process(m_next_job);

    m_sync_ctrl->setOverlay(m_next_job);

    m_next_job = NULL;
}

// ---------------------------------------------------------------------------

MMLayerComposer::MMLayerComposer(
    int dpy, const sp<SyncControl>& sync_ctrl, const sp<OverlayEngine>& ovl_engine)
    : ComposeThreadBase(dpy, sync_ctrl)
{
    m_handler = new BliterHandler(m_disp_id, ovl_engine);

    snprintf(m_trace_tag, sizeof(m_trace_tag), "compose2_%d", dpy);

    snprintf(m_thread_name, sizeof(m_thread_name), "MMCompThread_%d", dpy);
}

void MMLayerComposer::nullop()
{
    if (m_handler != NULL) m_handler->nullop();
}
