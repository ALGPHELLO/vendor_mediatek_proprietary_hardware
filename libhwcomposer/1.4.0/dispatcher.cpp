#define DEBUG_LOG_TAG "JOB"

#include "hwc_priv.h"

#include "utils/debug.h"
#include "utils/tools.h"

#include "dispatcher.h"
#include "display.h"
#include "overlay.h"
#include "sync.h"
#include "hwdev.h"
#include "platform.h"
#include "hwc.h"

#define HWC_ATRACE_JOB(string, n1, n2, n3, n4)                                \
    if (ATRACE_ENABLED()) {                                                   \
        char ___traceBuf[1024];                                               \
        snprintf(___traceBuf, 1024, "%s(%d): %d %d %d", (string),             \
            (n1), (n2), (n3), (n4));                                          \
        android::ScopedTrace ___bufTracer(ATRACE_TAG, ___traceBuf);           \
    }

// ---------------------------------------------------------------------------

ANDROID_SINGLETON_STATIC_INSTANCE(HWCDispatcher);

HWCDispatcher::HWCDispatcher()
    : m_sequence(1)
    , m_disable_skip_redundant(false)
{
    // bypass getJob then call setJob first time could cause SIGSEGV
    memset(m_curr_jobs, 0, sizeof(m_curr_jobs));
}

HWCDispatcher::~HWCDispatcher()
{
    for (uint32_t i = 0; i < DisplayManager::MAX_DISPLAYS; i++)
    {
        if (m_alloc_disp_ids.hasBit(i))
        {
            onPlugOut(i);
        }
    }
}

DispatcherJob* HWCDispatcher::getJob(int dpy)
{
#ifndef MTK_USER_BUILD
    HWC_ATRACE_CALL();
#endif

    if (dpy >= DisplayManager::MAX_DISPLAYS)
        return NULL;

    DispatcherJob* job = NULL;
    {
        AutoMutex l(m_workers[dpy].plug_lock_main);

        if (!m_workers[dpy].enable)
        {
            HWC_LOGE("Failed to get job: dpy(%d) is not enabled", dpy);
        }
        else if (!m_workers[dpy].ovl_engine->isEnable())
        {
            HWC_LOGI("ovl_%d is not enable, do not produce a job", dpy);
        }
        else
        {
            job = new DispatcherJob();
            job->disp_ori_id = dpy;

            job->num_layers = m_workers[dpy].ovl_engine->getAvailableInputNum();
            if (job->num_layers <= 0)
            {
                // reserve one dump layer for gpu compoisition with FBT
                // if there's np available input layers
                // then post handler would wait until OVL is available
                HWC_LOGW("No available overlay resource (input_num=%d)", job->num_layers);

                job->num_layers = 1;
            }
            else
            {
                job->ovl_valid = true;
            }

            job->hw_layers = (HWLayer*)calloc(1, sizeof(HWLayer) * job->num_layers);
        }

        m_curr_jobs[dpy] = job;
    }

    return job;
}

int HWCDispatcher::verifyType(
    int dpy, PrivateHandle* priv_handle, int idx, int dirty, int type)
{
    if (!m_workers[dpy].hw_layer_types) return true;

    if (dirty & HWC_LAYER_DIRTY_PARAM)
        m_curr_jobs[dpy]->need_flush = true;

    // for stopping Video Recording behavior,
    // SurfaceView would change from ui layer to mm layer
    // to avoid tearing issue, add a work-around for always sync camera layer
    if (dirty & HWC_LAYER_DIRTY_CAMERA)
        m_curr_jobs[dpy]->need_flush = true;

    WorkerCluster::HWLayerTypes* hw_layer_types = m_workers[dpy].hw_layer_types;
    int pool_id = priv_handle->ext_info.pool_id;

    if (hw_layer_types[idx].type != type ||
        hw_layer_types[idx].pool_id != pool_id)
    {
        hw_layer_types[idx].type = type;
        hw_layer_types[idx].pool_id = pool_id;

        m_curr_jobs[dpy]->need_flush = true;

        return HWC_LAYER_DIRTY_PORT;
    }

    return HWC_LAYER_DIRTY_NONE;
}

void HWCDispatcher::setSessionMode(int dpy, bool mirrored)
{
    DISP_MODE prev_session_mode = DISP_INVALID_SESSION_MODE;
    DISP_MODE curr_session_mode = DISP_INVALID_SESSION_MODE;

    // get previous and current session mode
    {
        AutoMutex l(m_workers[dpy].plug_lock_main);
        if (m_workers[dpy].enable && m_workers[dpy].ovl_engine != NULL)
        {
            prev_session_mode = m_workers[dpy].ovl_engine->getOverlaySessionMode();
            if (DISP_INVALID_SESSION_MODE == prev_session_mode)
            {
                HWC_LOGW("Failed to set session mode: dpy(%d)", dpy);
                return;
            }
        }

        curr_session_mode = mirrored ?
            DISP_SESSION_DECOUPLE_MIRROR_MODE : DISP_SESSION_DIRECT_LINK_MODE;
    }

    // flush all pending display jobs of displays before
    // session mode transition occurs (mirror <-> extension)
    // TODO: refine this logic if need to flush jobs for other session mode transition
    bool mode_transition =
            mirrored != ((DISP_SESSION_DECOUPLE_MIRROR_MODE == prev_session_mode) ||
                         (DISP_SESSION_DIRECT_LINK_MIRROR_MODE == prev_session_mode));
    if (mode_transition)
    {
        for (uint32_t i = 0; i < DisplayManager::MAX_DISPLAYS; i++)
        {
            AutoMutex l(m_workers[i].plug_lock_main);

            if (m_workers[i].enable) m_workers[i].dp_thread->wait();
        }
    }

    // set current session mode
    // TODO: should set HWC_DISPLAY_EXTERNAL/HWC_DISPLAY_VIRTUAL separately?
    {
        AutoMutex l(m_workers[dpy].plug_lock_main);

        if (m_workers[dpy].enable && m_workers[dpy].ovl_engine != NULL)
            m_workers[dpy].ovl_engine->setOverlaySessionMode(curr_session_mode);
    }

    if (CC_UNLIKELY(prev_session_mode != curr_session_mode))
    {
        HWC_LOGD("change session mode (dpy=%d/mir=%c/%s -> %s)",
            dpy, mirrored ? 'y' : 'n',
            DispDevice::toString(prev_session_mode),
            DispDevice::toString(curr_session_mode));
    }
}

void HWCDispatcher::configMirrorJob(DispatcherJob* job)
{
    int mir_dpy = job->disp_mir_id;
    int ori_dpy = job->disp_ori_id;

    if (mir_dpy >= DisplayManager::MAX_DISPLAYS) return;
    // temporary solution to prevent error of MHL plug-out
    if (ori_dpy >= DisplayManager::MAX_DISPLAYS) return;

    AutoMutex lm(m_workers[ori_dpy].plug_lock_main);
    AutoMutex l(m_workers[mir_dpy].plug_lock_main);

    if (m_workers[ori_dpy].enable)
    {
        DispatcherJob* mir_job = m_curr_jobs[mir_dpy];

        if (NULL == mir_job)
        {
            HWC_LOGI("configMirrorJob mir_job is NULL");
            return;
        }

        // enable mirror source
        mir_job->mirrored = true;

        // keep the orientation of mirror source in mind
        job->disp_mir_rot = mir_job->disp_ori_rot;

        // sync secure setting from mirror source
        job->secure = mir_job->secure;
    }
}

void HWCDispatcher::setJob(
    int dpy, struct hwc_display_contents_1* list)
{
#ifndef MTK_USER_BUILD
    HWC_ATRACE_CALL();
#endif

    if (dpy >= DisplayManager::MAX_DISPLAYS) return;

    {
        AutoMutex l(m_workers[dpy].plug_lock_main);

        if (CC_UNLIKELY(!m_workers[dpy].enable))
        {
            // it can happen if SurfaceFlinger tries to set display job
            // right after hotplug thread just removed a display (e.g. via onPlugOut())
            HWC_LOGW("Failed to set job: dpy(%d) is not enabled", dpy);
            clearListFbt(list);
            return;
        }

        if (!m_workers[dpy].ovl_engine->isEnable())
        {
            HWC_LOGD("SET/bypass/dpy=%d/blank", dpy);
            clearListAll(list);
            return;
        }

        DispatcherJob* job = m_curr_jobs[dpy];
        if (NULL == job)
        {
            HWC_LOGW("SET/bypass/dpy=%d/nulljob", dpy);
            clearListFbt(list);
            return;
        }

        // TODO: remove this if MHL display can do partial update by DPI
        if ((HWC_DISPLAY_EXTERNAL == job->disp_ori_id) &&
            (HWC_MIRROR_SOURCE_INVALID == job->disp_mir_id)) // extension mode
        {
            DispatcherJob* mir_job = m_curr_jobs[HWC_DISPLAY_PRIMARY];
            if (NULL != mir_job)
            {
                // record main display orientation in disp_mir_rot for MHL extension mode
                job->disp_mir_rot = mir_job->disp_ori_rot;
                if (job->disp_ori_rot != mir_job->disp_ori_rot)
                {
                    HWC_LOGD("change MHL ori %d->%d", job->disp_ori_rot, mir_job->disp_ori_rot);
                }
            }
        }

        if (dpy == HWC_DISPLAY_VIRTUAL && (HWC_MIRROR_SOURCE_INVALID != job->disp_mir_id))
        {
            // get video timestamp from primary display
            job->timestamp = m_curr_jobs[HWC_DISPLAY_PRIMARY]->timestamp;
        }

        if (DisplayManager::m_profile_level & PROFILE_TRIG)
        {
            job->sequence = m_sequence;
        }

        if (job->need_flush) m_workers[dpy].dp_thread->wait();

        // check if fbt handle is valid
        if (job->fbt_exist)
        {
            hwc_layer_1_t* fbt_layer = &list->hwLayers[list->numHwLayers - 1];
            if (fbt_layer->handle == NULL)
            {
                int idx = job->num_ui_layers + job->num_mm_layers;
                HWLayer* fbt_hw_layer = &job->hw_layers[idx];
                fbt_hw_layer->enable = false;
#ifdef MTK_HWC_PROFILING
                fbt_hw_layer->fbt_input_layers = 0;
                fbt_hw_layer->fbt_input_bytes  = 0;
#endif

                job->fbt_exist = false;

                if (0 == (job->num_mm_layers + job->num_ui_layers))
                {
                    clearListAll(list);
                    job->enable = false;
                    HWC_LOGD("(%d) SET/bypass/no layers", dpy);
                    return;
                }
            }
        }

#ifndef MTK_USER_BUILD
        HWC_ATRACE_JOB("set", dpy, job->fbt_exist, job->num_ui_layers, job->num_mm_layers);
#endif

        HWC_LOGD("SET/dpy=%d/max=%d/fbt=%d/ui=%d/mm=%d/mir=%d",
            dpy, job->num_layers, job->fbt_exist, job->num_ui_layers,
            job->num_mm_layers, job->disp_mir_id);

        // verify
        // 1. if outbuf should be used for virtual display
        // 2. if any input buffer is dirty for physical display
        m_workers[dpy].post_handler->set(list, job);
        if ((job->post_state & HWC_POST_CONTINUE_MASK) == 0)
        {
            if (job->num_mm_layers)
            {
                if (HWC_MIRROR_SOURCE_INVALID != job->disp_mir_id)
                {
                    DispatcherJob* mir_job = m_curr_jobs[job->disp_mir_id];
                    if ((mir_job->post_state & HWC_POST_CONTINUE_MASK) != 0)
                    {
                        int mir_out_acq_fence_fd = mir_job->hw_outbuf.mir_out_acq_fence_fd;
                        if (mir_out_acq_fence_fd != -1) ::close(mir_out_acq_fence_fd);
                    }
                }
            }
            // disable job
            job->enable = false;
            return;
        }

        bool job_enabled = false;

        if (job->num_ui_layers || job->fbt_exist)
        {
            m_workers[dpy].ui_thread->set(list, job);
            job_enabled = true;
        }

        if (job->num_mm_layers)
        {
            int mir_dpy = job->disp_mir_id;
            if (HWC_MIRROR_SOURCE_INVALID == mir_dpy)
            {
                m_workers[dpy].mm_thread->set(list, job);
                job_enabled = true;
            }
            else
            {
                // mirror mode
                DispatcherJob* mir_job = m_curr_jobs[mir_dpy];
                if ((mir_job->post_state & HWC_POST_CONTINUE_MASK) == 0)
                {
                    HWC_LOGD("(%d) skip composition: no dirty layers of mirror source", dpy);
                    // clear all layers' acquire fences
                    clearListAll(list);
                }
                else
                {
                    m_workers[dpy].mm_thread->set(list, job);
                    job_enabled = true;

                    // get mirror source buffer and set as input later
                    m_workers[mir_dpy].post_handler->setMirror(mir_job, job);
                }
            }
        }

        // marked job that should be processed
        job->enable = job_enabled;
    }
}

void HWCDispatcher::trigger()
{
    for (uint32_t i = 0; i < DisplayManager::MAX_DISPLAYS; i++)
    {
        AutoMutex l(m_workers[i].plug_lock_main);

        if (m_workers[i].enable && m_curr_jobs[i])
        {
            m_workers[i].dp_thread->trigger(m_curr_jobs[i]);
            m_workers[i].force_wait = m_curr_jobs[i]->force_wait;
            m_curr_jobs[i] = NULL;
        }
    }

    // increment sequence number by 1 after triggering all dispatcher threads
    if (DisplayManager::m_profile_level & PROFILE_TRIG)
    {
        m_sequence = m_sequence + 1;
    }

    // [WORKAROUND]
    for (uint32_t i = 0; i < DisplayManager::MAX_DISPLAYS; i++)
    {
        AutoMutex l(m_workers[i].plug_lock_main);

        if (m_workers[i].force_wait)
        {
            m_workers[i].dp_thread->wait();
            m_workers[i].force_wait = false;
        }
    }
}

void HWCDispatcher::releaseResourceLocked(int dpy)
{
    // wait until all threads are idle
    if (m_workers[dpy].dp_thread != NULL)
    {
        // flush pending display job of display before
        // destroy display session
        m_workers[dpy].dp_thread->wait();
        m_workers[dpy].dp_thread->requestExit();
        m_workers[dpy].dp_thread->trigger(m_curr_jobs[dpy]);
        m_curr_jobs[dpy] = NULL;
        m_workers[dpy].dp_thread->join();

        removeVSyncListener(dpy, m_workers[dpy].dp_thread);
        m_workers[dpy].dp_thread = NULL;
    }

    if (m_workers[dpy].ui_thread != NULL)
    {
        m_workers[dpy].ui_thread->wait();
        m_workers[dpy].ui_thread->requestExit();
        m_workers[dpy].ui_thread->trigger(NULL);
        m_workers[dpy].ui_thread->join();
        m_workers[dpy].ui_thread = NULL;
    }

    if (m_workers[dpy].mm_thread != NULL)
    {
        m_workers[dpy].mm_thread->wait();
        m_workers[dpy].mm_thread->requestExit();
        m_workers[dpy].mm_thread->trigger(NULL);
        m_workers[dpy].mm_thread->join();
        m_workers[dpy].mm_thread = NULL;
    }

    free(m_workers[dpy].hw_layer_types);
    m_workers[dpy].hw_layer_types = NULL;

    m_workers[dpy].post_handler = NULL;
    m_workers[dpy].sync_ctrl = NULL;

    m_workers[dpy].ovl_engine->setPowerMode(HWC_POWER_MODE_OFF);
    m_workers[dpy].ovl_engine = NULL;

    HWC_LOGD("Release resource (dpy=%d)", dpy);

    disp_session_info info;

    if (DispDevice::getInstance().getOverlaySessionInfo(dpy, &info) != INVALID_OPERATION)
    {
        // session still exists after destroying overlay engine
        // something goes wrong in display driver?
        HWC_LOGW("Session is not destroyed (dpy=%d)", dpy);
    }
}

void HWCDispatcher::onPlugIn(int dpy)
{
#ifndef MTK_USER_BUILD
    HWC_ATRACE_CALL();
#endif

    if (dpy >= DisplayManager::MAX_DISPLAYS)
    {
        HWC_LOGE("Invalid display(%d) is plugged(%d) !!", dpy);
        return;
    }

    {
        AutoMutex ll(m_workers[dpy].plug_lock_loop);
        AutoMutex lm(m_workers[dpy].plug_lock_main);
        AutoMutex lv(m_workers[dpy].plug_lock_vsync);

        if (m_alloc_disp_ids.hasBit(dpy))
        {
            HWC_LOGE("Display(%d) is already connected !!", dpy);
            return;
        }

        m_workers[dpy].enable = false;

        m_workers[dpy].ovl_engine = new OverlayEngine(dpy);
        if (m_workers[dpy].ovl_engine == NULL ||
            !m_workers[dpy].ovl_engine->isEnable())
        {
            m_workers[dpy].ovl_engine = NULL;
            HWC_LOGE("Failed to create OverlayEngine (dpy=%d) !!", dpy);
            return;
        }

        if (dpy < HWC_DISPLAY_VIRTUAL)
        {
            m_workers[dpy].post_handler =
                new PhyPostHandler(this, dpy, m_workers[dpy].ovl_engine);
        }
        else
        {
            m_workers[dpy].post_handler =
                new VirPostHandler(this, dpy, m_workers[dpy].ovl_engine);
        }

        if (m_workers[dpy].post_handler == NULL)
        {
            HWC_LOGE("Failed to create PostHandler (dpy=%d) !!", dpy);
            releaseResourceLocked(dpy);
            return;
        }

        // create post listener to know when need to set and trigger overlay engine
        struct PostListener : public SyncControl::SyncListener
        {
            PostListener(sp<PostHandler> handler) : m_handler(handler) { }
            ~PostListener() { m_handler = NULL; }
        private:
            sp<PostHandler> m_handler;
            mutable Mutex m_lock;
            virtual void onTrigger(DispatcherJob* job)
            {
                AutoMutex l(m_lock);
                m_handler->process(job);
            }
        };

        m_workers[dpy].sync_ctrl =
            new SyncControl(new PostListener(m_workers[dpy].post_handler));
        if (m_workers[dpy].sync_ctrl == NULL)
        {
            HWC_LOGE("Failed to create SyncControl (dpy=%d) !!", dpy);
            releaseResourceLocked(dpy);
            return;
        }

        m_workers[dpy].dp_thread = new DispatchThread(dpy);
        if (m_workers[dpy].dp_thread == NULL)
        {
            HWC_LOGE("Failed to create DispatchThread (dpy=%d) !!", dpy);
            releaseResourceLocked(dpy);
            return;
        }
        registerVSyncListener(dpy, m_workers[dpy].dp_thread);

        m_workers[dpy].ui_thread =
            new UILayerComposer(dpy, m_workers[dpy].sync_ctrl, m_workers[dpy].ovl_engine);
        if (m_workers[dpy].ui_thread == NULL)
        {
            HWC_LOGE("Failed to create UILayerComposer (dpy=%d) !!", dpy);
            releaseResourceLocked(dpy);
            return;
        }

        m_workers[dpy].mm_thread =
            new MMLayerComposer(dpy, m_workers[dpy].sync_ctrl, m_workers[dpy].ovl_engine);
        if (m_workers[dpy].mm_thread == NULL)
        {
            HWC_LOGE("Failed to create MMLayerComposer (dpy=%d) !!", dpy);
            releaseResourceLocked(dpy);
            return;
        }

        int ovl_in_num = m_workers[dpy].ovl_engine->getMaxInputNum();
        m_workers[dpy].hw_layer_types =
            (WorkerCluster::HWLayerTypes*)calloc(1, sizeof(WorkerCluster::HWLayerTypes) * ovl_in_num);

        m_alloc_disp_ids.markBit(dpy);
        m_workers[dpy].enable = true;

        DisplayData* disp_data = &DisplayManager::getInstance().m_data[dpy];

        // create mirror buffer of main display if needed
        if (HWC_DISPLAY_PRIMARY < dpy)
        {
            int format = (HWC_DISPLAY_VIRTUAL == dpy && HWC_DISPLAY_WIRELESS != disp_data->subtype) ?
                               HAL_PIXEL_FORMAT_RGBA_8888 : HAL_PIXEL_FORMAT_RGB_888;
            m_workers[HWC_DISPLAY_PRIMARY].ovl_engine->createOutputQueue(format, false);
        }
    }
}

void HWCDispatcher::onPlugOut(int dpy)
{
#ifndef MTK_USER_BUILD
    HWC_ATRACE_CALL();
#endif

    if (dpy >= DisplayManager::MAX_DISPLAYS)
    {
        HWC_LOGE("Invalid display(%d) is unplugged !!", dpy);
    }

    if (dpy == HWC_DISPLAY_PRIMARY)
    {
        HWC_LOGE("Should not disconnect primary display !!");
        return;
    }

    {
        AutoMutex lm(m_workers[dpy].plug_lock_main);
        AutoMutex lv(m_workers[dpy].plug_lock_vsync);

        // flush pending display job of mirror source before
        // destroy display session
        {
            DispatcherJob* ori_job = m_curr_jobs[dpy];

            if ( ori_job && (HWC_MIRROR_SOURCE_INVALID != ori_job->disp_mir_id))
            {
                const int mir_dpy = ori_job->disp_mir_id;

                AutoMutex l(m_workers[mir_dpy].plug_lock_main);

                if (m_workers[mir_dpy].enable)
                {
                    DispatcherJob* mir_job = m_curr_jobs[mir_dpy];

                    if (mir_job && mir_job->enable && mir_job->mirrored)
                    {
                        m_workers[mir_dpy].dp_thread->trigger(mir_job);
                        m_curr_jobs[mir_dpy] = NULL;
                    }

                    m_workers[mir_dpy].dp_thread->wait();
                }
            }
        }

        if (m_workers[dpy].enable)
        {
            releaseResourceLocked(dpy);
        }
        else
        {
            HWC_LOGE("Failed to disconnect invalid display(%d) !!", dpy);
        }

        AutoMutex ll(m_workers[dpy].plug_lock_loop);

        m_alloc_disp_ids.clearBit(dpy);
        m_workers[dpy].enable = false;

        // release mirror buffer of main display if needed
        if (HWC_DISPLAY_PRIMARY < dpy && (m_alloc_disp_ids.count() == 1))
        {
            m_workers[HWC_DISPLAY_PRIMARY].ovl_engine->releaseOutputQueue();
        }
    }
}

void HWCDispatcher::setPowerMode(int dpy, int mode)
{
#ifndef MTK_USER_BUILD
    HWC_ATRACE_CALL();
#endif

    if (HWC_DISPLAY_VIRTUAL > dpy)
    {
        if (HWC_POWER_MODE_OFF == mode || HWC_POWER_MODE_DOZE_SUSPEND == mode)
        {
            if (m_workers[dpy].dp_thread != NULL)
                m_workers[dpy].dp_thread->wait();
        }

        AutoMutex l(m_workers[dpy].plug_lock_main);

        if (m_workers[dpy].enable)
        {
            m_workers[dpy].ovl_engine->setPowerMode(mode);
        }

        if (HWC_POWER_MODE_OFF == mode || HWC_POWER_MODE_DOZE_SUSPEND == mode)
        {
            // clear mm thread state at POWER OFF or DOZE SUSPEND
            sp<MMLayerComposer> mm_thread = m_workers[dpy].mm_thread;
            if (mm_thread != NULL) mm_thread->nullop();
        }
    }
}

void HWCDispatcher::onVSync(int dpy)
{
#ifndef MTK_USER_BUILD
    HWC_ATRACE_CALL();
#endif

    {
        AutoMutex l(m_vsync_lock);

        // dispatch vsync signal to listeners
        const size_t count = m_vsync_callbacks.size();
        for (size_t i = 0; i < count; i++)
        {
            const sp<VSyncListener>& callback(m_vsync_callbacks[i]);
            callback->onVSync();
        }
    }

    {
        HWC_ATRACE_NAME("vsync_flip");

        AutoMutex l(m_workers[dpy].plug_lock_vsync);

        if (m_workers[dpy].enable)
        {
            m_workers[dpy].ovl_engine->flip();
        }
    }
}

void HWCDispatcher::registerVSyncListener(int dpy, const sp<VSyncListener>& listener)
{
    AutoMutex l(m_vsync_lock);

    m_vsync_callbacks.add(listener);
    HWC_LOGD("(%d) register VSyncListener", dpy);
}

void HWCDispatcher::removeVSyncListener(int dpy, const sp<VSyncListener>& listener)
{
    AutoMutex l(m_vsync_lock);

    m_vsync_callbacks.remove(listener);
    HWC_LOGD("(%d) remove VSyncListener", dpy);
}

void HWCDispatcher::dump(struct dump_buff* log, int dump_level)
{
    for (int dpy = 0; dpy < DisplayManager::MAX_DISPLAYS; dpy++)
    {
        AutoMutex l(m_workers[dpy].plug_lock_main);

        if (m_workers[dpy].enable)
        {
            m_workers[dpy].ovl_engine->dump(log, dump_level);
        }
    }
    dump_printf(log, "\n");
}

bool HWCDispatcher::saveFbtHandle(int dpy, buffer_handle_t handle)
{
    bool res = m_prev_fbt[dpy] != handle;
    m_prev_fbt[dpy] = handle;
    return res;
}

// ---------------------------------------------------------------------------

DispatchThread::DispatchThread(int dpy)
    : m_disp_id(dpy)
    , m_vsync_signaled(false)
{
    snprintf(m_thread_name, sizeof(m_thread_name), "Dispatcher_%d", dpy);
}

void DispatchThread::onFirstRef()
{
    run(m_thread_name, PRIORITY_URGENT_DISPLAY);
}

void DispatchThread::trigger(DispatcherJob* job)
{
#ifndef MTK_USER_BUILD
    HWC_ATRACE_NAME("dispatcher_set");
#endif

    AutoMutex l(m_lock);

    if (job != NULL)
    {
        m_job_queue.push_back(job);
    }

    m_state = HWC_THREAD_TRIGGER;

    sem_post(&m_event);
}

bool DispatchThread::threadLoop()
{
    sem_wait(&m_event);

    bool need_check_next_period = false;

    while (1)
    {
        DispatcherJob* job = NULL;

        {
            AutoMutex l(m_lock);
            if (m_job_queue.empty())
            {
                HWC_LOGD("(%d) Job is empty...", m_disp_id);
                break;
            }

            Fifo::iterator front(m_job_queue.begin());
            job = *front;

            m_job_queue.erase(front);
        }

#ifndef MTK_USER_BUILD
        HWC_ATRACE_NAME("dispatcher_loop");
#endif

        // need to wait vsync since get another job from job queue directly
        //
        // TODO:
        // if ovl is decoupled, need to make sure if ovl could have internal queue
        // if yes, there is no need to wait for next vsync for handling next composition
        {
            AutoMutex l(m_vsync_lock);

            if (m_disp_id < HWC_DISPLAY_VIRTUAL && (need_check_next_period || !m_vsync_signaled))
            {
                HWC_LOGD("(%d) Wait to handle next job...", m_disp_id);

#ifndef MTK_USER_BUILD
                HWC_ATRACE_NAME("dispatcher_wait");
#endif

                waitNextVSyncLocked(m_disp_id);
            }

            m_vsync_signaled = false;
        }

        bool need_sync = true;

        // handle jobs
        // 1. set synchronization between composer threads
        // 2. trigger ui/mm threads to compose layers
        // 3. wait until the composition of ui/mm threads is done
        // 4. clear used job
        {
            HWCDispatcher::WorkerCluster& worker(
                HWCDispatcher::getInstance().m_workers[m_disp_id]);

            AutoMutex l(worker.plug_lock_loop);

            HWC_LOGD("(%d) Handle Job...", m_disp_id);

            sp<UILayerComposer> ui_thread = worker.ui_thread;
            sp<MMLayerComposer> mm_thread = worker.mm_thread;
            sp<SyncControl> sync_ctrl = worker.sync_ctrl;
            if (job->enable && ui_thread != NULL && mm_thread != NULL && sync_ctrl != NULL)
            {
                {
#ifndef MTK_USER_BUILD
                    HWC_ATRACE_NAME("dispatcher_handle");
                    HWC_ATRACE_JOB("trigger",
                        m_disp_id, job->fbt_exist, job->num_ui_layers, job->num_mm_layers);
#endif

                    // set sync control
                    sync_ctrl->setSync(job);

                    // trigger ui thread if there are ui layers or fbt
                    if (job->num_ui_layers || job->fbt_exist)
                        ui_thread->trigger(job);

                    // trigger mm thread if there are mm layers
                    if (job->num_mm_layers)
                        mm_thread->trigger(job);
                    else
                        mm_thread->nullop();

                    if (m_disp_id < HWC_DISPLAY_VIRTUAL && job->num_mm_layers)
                        need_sync = false;
                }

                {
#ifndef MTK_USER_BUILD
                    HWC_ATRACE_JOB("wait",
                        m_disp_id, job->fbt_exist, job->num_ui_layers, job->num_mm_layers);
#endif

                    // wait until composition is finished
                    if (job->num_mm_layers)
                        mm_thread->wait();

                    if (job->num_ui_layers || job->fbt_exist)
                        ui_thread->wait();
                }
            }
#ifndef MTK_USER_BUILD
            else if (CC_UNLIKELY(!job->enable))
            {
                HWC_ATRACE_NAME("dispatcher_bypass");
            }
#endif

            // clear used job
            if (job != NULL)
            {
                free(job->hw_layers);
                delete job;
            }
        }

        need_check_next_period = need_sync;
    }

    {
        AutoMutex l(m_lock);
        m_state = HWC_THREAD_IDLE;
        m_condition.signal();
    }

    return true;
}

void DispatchThread::waitNextVSyncLocked(int dpy)
{
    // TODO: pass display id to DisplayManager to get the corresponding vsync signal

    // request next vsync
    DisplayManager::getInstance().requestNextVSync(HWC_DISPLAY_PRIMARY);

    // wait for vsync signal
    if (m_vsync_cond.waitRelative(m_vsync_lock, ms2ns(16)) == TIMED_OUT)
    {
        HWC_LOGW("(%d) Timed out waiting for vsync...", dpy);
    }
}

void DispatchThread::onVSync()
{
#ifndef MTK_USER_BUILD
    HWC_ATRACE_CALL();
#endif

    AutoMutex l(m_vsync_lock);
    m_vsync_signaled = true;
    m_vsync_cond.signal();
}

// ---------------------------------------------------------------------------

#define PLOGD(x, ...) HWC_LOGD("(%d) " x, m_disp_id, ##__VA_ARGS__)
#define PLOGI(x, ...) HWC_LOGI("(%d) " x, m_disp_id, ##__VA_ARGS__)
#define PLOGW(x, ...) HWC_LOGW("(%d) " x, m_disp_id, ##__VA_ARGS__)
#define PLOGE(x, ...) HWC_LOGE("(%d) " x, m_disp_id, ##__VA_ARGS__)

HWCDispatcher::PostHandler::PostHandler(
    HWCDispatcher* dispatcher, int dpy, const sp<OverlayEngine>& ovl_engine)
    : m_dispatcher(dispatcher)
    , m_disp_id(dpy)
    , m_ovl_engine(ovl_engine)
    , m_sync_fence(new SyncFence(dpy))
    , m_curr_present_fence_fd(-1)
{ }

HWCDispatcher::PostHandler::~PostHandler()
{
    m_ovl_engine = NULL;
    m_sync_fence = NULL;
}

// ---------------------------------------------------------------------------

void HWCDispatcher::PhyPostHandler::set(
    struct hwc_display_contents_1* list, DispatcherJob* job)
{
    job->hw_outbuf.phy_present_fence_fd = -1;
    job->hw_outbuf.phy_present_fence_idx = -1;

    if (list->flags & HWC_SKIP_DISPLAY)
    {
        PLOGD("skip composition: display has skip flag");
        job->post_state = HWC_POST_INPUT_NOTDIRTY;
        clearListAll(list);
        return;
    }

    if (HWC_MIRROR_SOURCE_INVALID != job->disp_mir_id)
    {
        job->post_state = HWC_POST_MIRROR;
        return;
    }

    status_t err;
    uint32_t total_num = job->num_layers;

    bool is_dirty = (job->post_state & HWC_POST_CONTINUE_MASK) != 0;

    if (is_dirty)
    {
        for (uint32_t i = 0; i < total_num; i++)
        {
            HWLayer* hw_layer = &job->hw_layers[i];

            if (!hw_layer->enable) continue;

            if (HWC_LAYER_TYPE_DIM == hw_layer->type) continue;

            hwc_layer_1_t* layer = &list->hwLayers[hw_layer->index];
            PrivateHandle* priv_handle = &hw_layer->priv_handle;

            // check if fbt is dirty
            if (HWC_LAYER_TYPE_FBT == hw_layer->type)
            {
                priv_handle->ion_fd = DISP_NO_ION_FD;
                err = NO_ERROR;

                err = getPrivateHandleFBT(layer->handle, priv_handle);

                if (err != NO_ERROR)
                {
                    // need to clear fbt to prevent fd leak
                    clearListFbt(list);
                    hw_layer->enable = false;
                    continue;
                }

                hw_layer->dirty = m_dispatcher->verifyType(
                                        m_disp_id, priv_handle, i,
                                        HWC_LAYER_DIRTY_NONE,
                                        HWC_LAYER_TYPE_FBT);

                if (priv_handle->ext_info.status & GRALLOC_EXTRA_MASK_SF_DIRTY)
                {
                    hw_layer->dirty |= HWC_LAYER_DIRTY_BUFFER;
                }
            }

            // check if any layer is dirty
            is_dirty |= (hw_layer->dirty != HWC_LAYER_DIRTY_NONE);

            gralloc_extra_sf_set_status(
                &priv_handle->ext_info, GRALLOC_EXTRA_MASK_SF_DIRTY, GRALLOC_EXTRA_BIT_UNDIRTY);

            gralloc_extra_perform(
                layer->handle, GRALLOC_EXTRA_SET_IOCTL_ION_SF_INFO, &priv_handle->ext_info);
        }

        if (job->mirrored)
        {
            err = m_ovl_engine->configMirrorOutput(&job->hw_outbuf, job->secure);
            if (CC_LIKELY(err == NO_ERROR))
            {
                is_dirty = true;
            }
            else
            {
                // cancel mirror mode if failing to configure mirror output
                int dpy = job->disp_ori_id;
                job->mirrored = false;

                m_ovl_engine->setOverlaySessionMode(DISP_SESSION_DIRECT_LINK_MODE);
            }
        }
    }

    if (is_dirty)
    {
        job->post_state = HWC_POST_INPUT_DIRTY;

        {
            // get present fence from display driver
            OverlayPrepareParam prepare_param;
            {
                status_t err = m_ovl_engine->preparePresentFence(prepare_param);
                if (NO_ERROR != err)
                {
                    prepare_param.fence_index = 0;
                    prepare_param.fence_fd = -1;
                }

                if (prepare_param.fence_fd <= 0)
                {
                    PLOGE("Failed to get presentFence !!");
                }
            }

            HWBuffer* hw_outbuf = &job->hw_outbuf;
            hw_outbuf->phy_present_fence_fd  = prepare_param.fence_fd;
            hw_outbuf->phy_present_fence_idx = prepare_param.fence_index;
            hw_outbuf->handle                = NULL;

            list->retireFenceFd = prepare_param.fence_fd;
            job->prev_present_fence_fd = m_curr_present_fence_fd;
            m_curr_present_fence_fd = ::dup(prepare_param.fence_fd);

            PLOGD("presentFence (fd=%d, idx=%d, curr_pf_fd=%d)",
                hw_outbuf->phy_present_fence_fd, hw_outbuf->phy_present_fence_idx, m_curr_present_fence_fd);
        }
    }
    else
    {
        // set as nodirty since could not find any dirty layers
        job->post_state = HWC_POST_INPUT_NOTDIRTY;

        PLOGD("skip composition: no dirty layers");
        // clear all layers' acquire fences
        clearListAll(list);
    }
}

void HWCDispatcher::PhyPostHandler::setMirror(
    DispatcherJob* src_job, DispatcherJob* dst_job)
{
    HWBuffer* phy_outbuf = &src_job->hw_outbuf;
    HWBuffer* dst_mirbuf = &dst_job->hw_mirbuf;
    HWBuffer* dst_outbuf = &dst_job->hw_outbuf;

    dst_mirbuf->mir_in_acq_fence_fd = phy_outbuf->mir_out_acq_fence_fd;
    dst_mirbuf->handle              = phy_outbuf->handle;
    memcpy(&dst_mirbuf->priv_handle, &phy_outbuf->priv_handle, sizeof(PrivateHandle));

    if (dst_job->disp_ori_id == HWC_DISPLAY_EXTERNAL)
    {
        unsigned int dst_format = phy_outbuf->priv_handle.format;
        switch (DevicePlatform::m_config.format_mir_mhl)
        {
            case MIR_FORMAT_RGB888:
                dst_format = HAL_PIXEL_FORMAT_RGB_888;
                break;
            case MIR_FORMAT_YUYV:
                dst_format = HAL_PIXEL_FORMAT_YUYV;
                break;
            case MIR_FORMAT_YV12:
                dst_format = HAL_PIXEL_FORMAT_YV12;
                break;
            default:
                HWC_LOGW("Not support mir format(%d), use same format as source(%x)",
                    DevicePlatform::m_config.format_mir_mhl,
                    phy_outbuf->priv_handle.format);
            case MIR_FORMAT_UNDEFINE:
                break;
         }

        dst_outbuf->priv_handle.format = dst_format;
        dst_outbuf->priv_handle.usage = phy_outbuf->priv_handle.usage;
    }

    // in decouple mode, need to wait for
    // both display and MDP finish reading this buffer
    {
        char name[32];
        snprintf(name, sizeof(name), "merged_fence(%d/%d)\n",
            phy_outbuf->mir_out_if_fence_fd, dst_mirbuf->mir_in_rel_fence_fd);

        int merged_fd = SyncFence::merge(
            phy_outbuf->mir_out_if_fence_fd,
            dst_mirbuf->mir_in_rel_fence_fd,
            name);

        ::close(phy_outbuf->mir_out_if_fence_fd);
        ::close(dst_mirbuf->mir_in_rel_fence_fd);

        // TODO: merge fences from different virtual displays to phy_outbuf->mir_out_mer_fence_fd
        phy_outbuf->mir_out_if_fence_fd = merged_fd;
    }

    PLOGD("set mirror (rel_fd=%d(%u)/handle=%p/ion=%d -> acq_fd=%d/handle=%p/ion=%d)",
        dst_mirbuf->mir_in_rel_fence_fd, dst_mirbuf->mir_in_sync_marker,
        dst_mirbuf->handle, dst_mirbuf->priv_handle.ion_fd,
        dst_outbuf->mir_in_rel_fence_fd, dst_outbuf->handle, dst_outbuf->priv_handle.ion_fd);
}

void HWCDispatcher::PhyPostHandler::process(DispatcherJob* job)
{
    // wait untill HW status has been switched to split mode
    if (!job->ovl_valid)
    {
        job->ovl_valid = m_ovl_engine->waitUntilAvailable();
        if (!job->ovl_valid) return;
    }

    // wait until previous presentfence has been signaled
    {
        m_sync_fence->wait(job->prev_present_fence_fd, 1000, "PF");
        job->prev_present_fence_fd = -1;
    }

    // disable unused input layer
    for (int i = 0; i < job->num_layers; i++)
    {
        if (!job->hw_layers[i].enable)
            m_ovl_engine->disableInput(i);
    }

    // update all input configs to driver
    m_ovl_engine->setInputs(job->num_layers);

    // set mirror output buffer if job is a mirror source
    if (job->mirrored)
    {
        HWBuffer* hw_outbuf = &job->hw_outbuf;
        PrivateHandle* priv_handle = &hw_outbuf->priv_handle;

        OverlayPortParam param;

        bool is_secure = isSecure(priv_handle);
        if (is_secure)
        {
            param.va           = (void*)(uintptr_t)hw_outbuf->mir_out_sec_handle;
            param.mva          = (void*)(uintptr_t)hw_outbuf->mir_out_sec_handle;
        }
        else
        {
            param.va           = NULL;
            param.mva          = NULL;
        }

        param.pitch            = priv_handle->y_stride;
        param.format           = priv_handle->format;
        param.dst_crop         = Rect(priv_handle->width, priv_handle->height);
        param.fence_index      = hw_outbuf->mir_out_acq_fence_idx;
        param.if_fence_index   = hw_outbuf->mir_out_if_fence_idx;
        param.secure           = is_secure;
        param.sequence         = job->sequence;
        param.ion_fd           = priv_handle->ion_fd;
        param.mir_rel_fence_fd = hw_outbuf->mir_out_if_fence_fd;

        m_sync_fence->wait(hw_outbuf->mir_out_rel_fence_fd, 1000, DEBUG_LOG_TAG);
        hw_outbuf->mir_out_rel_fence_fd = -1;

        m_ovl_engine->setOutput(&param, job->mirrored);

        if (DisplayManager::m_profile_level & PROFILE_TRIG)
        {
            char atrace_tag[256];
            sprintf(atrace_tag, "OVL0-MDP");
            HWC_ATRACE_ASYNC_BEGIN(atrace_tag, job->sequence);
        }
    }
    else if (!job->mirrored)
    {
        // disable overlay output
        m_ovl_engine->disableOutput();
    }

    // trigger overlay engine
    m_ovl_engine->trigger(job->hw_outbuf.phy_present_fence_idx);
}

// ---------------------------------------------------------------------------

void HWCDispatcher::VirPostHandler::setError(DispatcherJob* job)
{
    for (int i = 0; i < job->num_layers; i++)
    {
        m_ovl_engine->disableInput(i);
    }
}

void HWCDispatcher::VirPostHandler::set(
    struct hwc_display_contents_1* list, DispatcherJob* job)
{
    if (list->flags & HWC_SKIP_DISPLAY)
    {
        PLOGD("skip composition: display has skip flag");
        job->post_state = HWC_POST_INPUT_NOTDIRTY;
        clearListAll(list);
        return;
    }

    // For WFD extension mode without OVL is available, let GPU to queue buffer to encoder directly.
    if ((!HWCMediator::getInstance().m_features.copyvds) &&
        ((job->num_ui_layers + job->num_mm_layers) == 0))
    {
        PLOGD("No need to handle outbuf with GLES mode");
        job->post_state = HWC_POST_OUTBUF_DISABLE;
        clearListFbt(list);
        setError(job);
        return;
    }

    if (list->outbuf == NULL)
    {
        PLOGE("Fail to get outbuf");
        job->post_state = HWC_POST_INVALID;
        clearListAll(list);
        setError(job);
        return;
    }

    HWBuffer* hw_outbuf = &job->hw_outbuf;

    PrivateHandle* priv_handle = &hw_outbuf->priv_handle;
    priv_handle->ion_fd = DISP_NO_ION_FD;
    status_t err = getPrivateHandle(list->outbuf, priv_handle);
    if (err != NO_ERROR)
    {
        PLOGE("Failed to get private handle !! (outbuf=%p) !!", list->outbuf);
        job->post_state = HWC_POST_INVALID;
        clearListAll(list);
        setError(job);
        return;
    }

    if (1 == HWCMediator::getInstance().m_features.svp)
    {
        //PLOGD(" #SVP ------------------------------------------------ handleSecureBuffer outBuf");
        err = handleSecureBuffer(job->secure, list->outbuf, &priv_handle->sec_handle, &priv_handle->ext_info);
        if (NO_ERROR != err)
        {
            PLOGE("alloc sec outbuf fail!! (err=%d, h=%x)", err, list->outbuf);
            job->post_state = HWC_POST_INVALID;
            clearListAll(list);
            setError(job);
            return;
        }
    }

    job->post_state = HWC_POST_OUTBUF_ENABLE;

    if (HWC_MIRROR_SOURCE_INVALID != job->disp_mir_id)
    {
        // mirror mode
        //
        // set mirror output buffer
        hw_outbuf->mir_in_rel_fence_fd = list->outbufAcquireFenceFd;
        hw_outbuf->handle              = list->outbuf;

        // warning if TARGET_FORCE_HWC_FOR_VIRTUAL_DISPLAYS is set
        // but vir buffer format is not YV12
        if (HWCMediator::getInstance().m_features.copyvds && priv_handle->format != HAL_PIXEL_FORMAT_YV12)
        {
            PLOGW("copyvds is ture but vir buffer format is not HAL_PIXEL_FORMAT_YV12");
        }
    }
    else
    {
        // extension mode
        //
        // get retire fence from display driver
        OverlayPrepareParam prepare_param;
        {
            prepare_param.ion_fd        = priv_handle->ion_fd;
            prepare_param.is_need_flush = 0;

            err = m_ovl_engine->prepareOutput(prepare_param);
            if (NO_ERROR != err)
            {
                prepare_param.fence_index = 0;
                prepare_param.fence_fd = -1;
            }

            if (prepare_param.fence_fd <= 0)
            {
                PLOGE("Failed to get retireFence !!");
            }
        }

        hw_outbuf->out_acquire_fence_fd = list->outbufAcquireFenceFd;
        hw_outbuf->out_retire_fence_fd  = prepare_param.fence_fd;
        hw_outbuf->out_retire_fence_idx = prepare_param.fence_index;
        hw_outbuf->handle               = list->outbuf;

        list->retireFenceFd = prepare_param.fence_fd;

        PLOGD("Outbuf (ret_fd=%d(%u)/acq_fd=%d/handle=%p/ion=%d)",
            hw_outbuf->out_retire_fence_fd, hw_outbuf->out_retire_fence_idx,
            hw_outbuf->out_acquire_fence_fd, hw_outbuf->handle, priv_handle->ion_fd);

        if (!job->fbt_exist)
        {
            hwc_layer_1_t* layer = &list->hwLayers[list->numHwLayers - 1];
            layer->releaseFenceFd = -1;
            if (layer->acquireFenceFd != -1) ::close(layer->acquireFenceFd);
        }
    }

    // set video usage and timestamp into output buffer handle
    gralloc_extra_ion_sf_info_t* ext_info = &hw_outbuf->priv_handle.ext_info;
    ext_info->timestamp = job->timestamp;
    if (DisplayManager::m_profile_level & PROFILE_TRIG)
    {
        // set token to buffer handle for profiling latency purpose
        ext_info->sequence = job->sequence;
    }
    gralloc_extra_perform(
        hw_outbuf->handle, GRALLOC_EXTRA_SET_IOCTL_ION_SF_INFO, ext_info);
}

void HWCDispatcher::VirPostHandler::setMirror(
    DispatcherJob* /*src_job*/, DispatcherJob* /*dst_job*/)
{
}

void HWCDispatcher::VirPostHandler::process(DispatcherJob* job)
{
    if (HWC_MIRROR_SOURCE_INVALID == job->disp_mir_id)
    {
        // extension mode

        // wait untill HW status has been switched to split mode
        if (!job->ovl_valid)
        {
            job->ovl_valid = m_ovl_engine->waitUntilAvailable();
            if (!job->ovl_valid) return;
        }

        // disable unused input layer
        for (int i = 0; i < job->num_layers; i++)
        {
            if (!job->hw_layers[i].enable)
                m_ovl_engine->disableInput(i);
        }

        // update all input configs to driver
        m_ovl_engine->setInputs(job->num_layers);

        // set output buffer for virtual display
        {
            HWBuffer* hw_outbuf = &job->hw_outbuf;
            PrivateHandle* priv_handle = &hw_outbuf->priv_handle;

            // Reset used bit for mirror mode setBackGround
            // USED = 1xxx
            gralloc_extra_ion_sf_info_t* ext_info = &priv_handle->ext_info;
            int prev_orient = (ext_info->status & GRALLOC_EXTRA_MASK_ORIENT) >> 12;
            if (((prev_orient>>3) & 0x01) == 1)
            {
                gralloc_extra_sf_set_status(
                    ext_info, GRALLOC_EXTRA_MASK_ORIENT, (prev_orient<<12) & ~(0x01<<15));

                gralloc_extra_perform(
                    hw_outbuf->handle, GRALLOC_EXTRA_SET_IOCTL_ION_SF_INFO, ext_info);
            }

            OverlayPortParam param;

            bool is_secure = isSecure(priv_handle);
            if (is_secure)
            {
                param.va      = (void*)(uintptr_t)priv_handle->sec_handle;
                param.mva     = (void*)(uintptr_t)priv_handle->sec_handle;
            }
            else
            {
                param.va      = NULL;
                param.mva     = NULL;
            }
            param.pitch       = priv_handle->y_stride;
            param.format      = priv_handle->format;
            param.dst_crop    = Rect(priv_handle->width, priv_handle->height);
            param.fence_index = hw_outbuf->out_retire_fence_idx;
            param.secure      = is_secure;
            param.sequence    = job->sequence;
            param.ion_fd      = priv_handle->ion_fd;

            m_sync_fence->wait(hw_outbuf->out_acquire_fence_fd, 1000, DEBUG_LOG_TAG);

            m_ovl_engine->setOutput(&param);
        }

        // trigger overlay engine
        m_ovl_engine->trigger(DISP_NO_PRESENT_FENCE);
    }
    else
    {
        // mirror mode
        if (DisplayManager::m_profile_level & PROFILE_TRIG)
        {
            char atrace_tag[256];
            sprintf(atrace_tag, "MDP-SMS");
            HWC_ATRACE_ASYNC_END(atrace_tag, job->sequence);
        }
    }
}
