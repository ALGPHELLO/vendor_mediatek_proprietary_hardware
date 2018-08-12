#define DEBUG_LOG_TAG "OVL"

#include <stdlib.h>

#include "hwc_priv.h"

#include "utils/debug.h"
#include "utils/tools.h"

#include "overlay.h"
#include "dispatcher.h"
#include "display.h"
#include "hwdev.h"
#include "queue.h"
#include "composer.h"
#include "bliter.h"
#include "sync.h"

#define OLOGV(x, ...) HWC_LOGV("(%d) " x, m_disp_id, ##__VA_ARGS__)
#define OLOGD(x, ...) HWC_LOGD("(%d) " x, m_disp_id, ##__VA_ARGS__)
#define OLOGI(x, ...) HWC_LOGI("(%d) " x, m_disp_id, ##__VA_ARGS__)
#define OLOGW(x, ...) HWC_LOGW("(%d) " x, m_disp_id, ##__VA_ARGS__)
#define OLOGE(x, ...) HWC_LOGE("(%d) " x, m_disp_id, ##__VA_ARGS__)

// ---------------------------------------------------------------------------

OverlayEngine::OverlayInput::OverlayInput()
    : connected_state(OVL_PORT_DISABLE)
    , connected_type(OVL_INPUT_NONE)
    , queue(NULL)
{ }

OverlayEngine::OverlayOutput::OverlayOutput()
    : connected_state(OVL_PORT_DISABLE)
    , queue(NULL)
{ }

OverlayEngine::OverlayEngine(int dpy, sp<IDevice> device)
    : m_disp_id(dpy)
    , m_async_update(false)
    , m_device(device)
{
    // create overlay session
    status_t err = m_device->createOverlaySession(m_disp_id, DISP_SESSION_DIRECT_LINK_MODE);

    if (err != NO_ERROR)
    {
        m_state = OVL_ENGINE_DISABLED;
        m_max_inputs = 0;

        OLOGE("Failed to create display session");
    }
    else
    {
        m_state = OVL_ENGINE_ENABLED;
        m_max_inputs = m_device->getMaxOverlayInputNum();

        int avail_inputs = m_max_inputs;
        if (HWC_DISPLAY_PRIMARY == m_disp_id)
        {
            avail_inputs = m_device->getAvailableOverlayInput(m_disp_id);
            m_device->initOverlay();
        }

        for (int id = 0; id < m_max_inputs; id++)
        {
            // init input configurations
            OverlayInput* input = new OverlayInput();
            if (id >= avail_inputs)
            {
                input->connected_state = OVL_PORT_ENABLE;
                input->connected_type  = OVL_INPUT_UNKNOWN;
            }
            m_inputs.add(input);
            m_input_params.add(&input->param);
        }

        // initialize display data for physical display
        if (HWC_DISPLAY_VIRTUAL > m_disp_id)
            DisplayManager::getInstance().setDisplayData(dpy);
    }
}

OverlayEngine::~OverlayEngine()
{
    for (int id = 0; id < m_max_inputs; id++)
    {
        m_inputs[id]->queue = NULL;

        delete m_inputs[id];
    }
    m_inputs.clear();
    m_input_params.clear();

    m_output.queue = NULL;

    m_device->destroyOverlaySession(m_disp_id);
}

int OverlayEngine::getAvailableInputNum()
{
    int avail_inputs;

    avail_inputs = m_device->getAvailableOverlayInput(m_disp_id);

    // only main display need to check if fake display exists
    if (HWC_DISPLAY_PRIMARY == m_disp_id)
    {
        int fake_num = DisplayManager::getInstance().getFakeDispNum();
        avail_inputs = (avail_inputs > fake_num) ? (avail_inputs - fake_num) : avail_inputs;
    }

    return avail_inputs;
}

bool OverlayEngine::waitUntilAvailable()
{
#ifndef MTK_USER_BUILD
    char atrace_tag[128];
    sprintf(atrace_tag, "wait_ovl_avail(%d)", m_disp_id);
    HWC_ATRACE_NAME(atrace_tag);
#endif

    disp_session_info info;
    int avail_inputs;
    int try_count = 0;

    // TODO: use synchronous ioctl instead of busy-waiting
    do
    {
        avail_inputs = getAvailableInputNum();

        if (avail_inputs != 0) break;

        try_count++;

        OLOGW("Waiting for available OVL (cnt=%d)", try_count);

        usleep(5000);
    } while (try_count < 1000);

    if (avail_inputs == 0)
    {
        OLOGE("Timed out waiting for OVL (cnt=%d)", try_count);
        return false;
    }

    return true;
}

status_t OverlayEngine::prepareInput(OverlayPrepareParam& param)
{
    AutoMutex l(m_lock);

    int id = param.id;

    if (id >= m_max_inputs)
    {
        OLOGE("Failed to prepare invalid overlay input(%d)", id);
        return BAD_INDEX;
    }

    m_device->prepareOverlayInput(m_disp_id, &param);

    return NO_ERROR;
}

status_t OverlayEngine::setInputQueue(int id, sp<DisplayBufferQueue> queue)
{
    AutoMutex l(m_lock);

    if (id >= m_max_inputs)
    {
        OLOGE("Failed to set invalid overlay input(%d)", id);
        return BAD_INDEX;
    }

    if (OVL_INPUT_QUEUE == m_inputs[id]->connected_type)
    {
        OLOGW("Already set overlay input(%d)", id);
        return BAD_INDEX;
    }

    struct InputListener : public DisplayBufferQueue::ConsumerListener
    {
        InputListener(sp<OverlayEngine> ovl, int id)
            : m_engine(ovl)
            , m_id(id)
        { }
    private:
        sp<OverlayEngine> m_engine;
        int m_id;
        virtual void onBufferQueued()
        {
            m_engine->updateInput(m_id);
        }
    };
    queue->setConsumerListener(new InputListener(this, id));

    m_inputs[id]->queue = queue;
    m_inputs[id]->connected_state = OVL_PORT_ENABLE;
    m_inputs[id]->connected_type = OVL_INPUT_QUEUE;

    return NO_ERROR;
}

status_t OverlayEngine::setInputDirect(int id, OverlayPortParam* param)
{
    AutoMutex l(m_lock);

    if (id >= m_max_inputs)
    {
        OLOGE("Failed to set invalid overlay input(%d)", id);
        return BAD_INDEX;
    }

    if (OVL_INPUT_QUEUE == m_inputs[id]->connected_type)
    {
        OLOGI("Overlay input(%d) was used with queue previously", id);
    }

    m_inputs[id]->queue = NULL;
    m_inputs[id]->connected_state = OVL_PORT_ENABLE;
    m_inputs[id]->connected_type = OVL_INPUT_DIRECT;

    if (param != NULL)
    {
        m_input_params[id]->state = OVL_IN_PARAM_IGNORE;

        if (DisplayManager::m_profile_level & PROFILE_TRIG)
        {
            char atrace_tag[128];
            sprintf(atrace_tag, "set_ovl(%d): input(%d) direct\n", m_disp_id, id);
            HWC_ATRACE_NAME(atrace_tag);

            OLOGI("HWC->OVL: input(%d) direct", id);

            m_device->enableOverlayInput(m_disp_id, param, id);
        }
        else
        {
            m_device->enableOverlayInput(m_disp_id, param, id);
        }
    }

    return NO_ERROR;
}

status_t OverlayEngine::setInputs(int num)
{
    AutoMutex l(m_lock);

    if (DisplayManager::m_profile_level & PROFILE_TRIG)
    {
        char atrace_tag[128];
        sprintf(atrace_tag, "set_ovl(%d): set inputs", m_disp_id);
        HWC_ATRACE_NAME(atrace_tag);
        OLOGV("HWC->OVL: set inputs (max=%d)", num);

        m_device->updateOverlayInputs(
            m_disp_id, m_input_params.array(), num);
    }
    else
    {
        m_device->updateOverlayInputs(
            m_disp_id, m_input_params.array(), num);
    }

    return NO_ERROR;
}

status_t OverlayEngine::disableInput(int id)
{
    AutoMutex l(m_lock);

    if (id >= m_max_inputs)
    {
        OLOGE("Failed to disable invalid overlay input(%d)", id);
        return BAD_INDEX;
    }

    if (OVL_INPUT_NONE == m_inputs[id]->connected_type)
    {
        //OLOGW("Not using overlay input(%d)", id);
        return BAD_INDEX;
    }

    disableInputLocked(id);

    return NO_ERROR;
}

status_t OverlayEngine::disableOutput()
{
    AutoMutex l(m_lock);

    disableOutputLocked();

    return NO_ERROR;
}

void OverlayEngine::disableInputLocked(int id)
{
    // set overlay params
    m_input_params[id]->state = OVL_IN_PARAM_DISABLE;

    // clear input infomation
    m_inputs[id]->queue = NULL;
    m_inputs[id]->connected_state = OVL_PORT_DISABLE;
    m_inputs[id]->connected_type = OVL_INPUT_NONE;
}

void OverlayEngine::disableOutputLocked()
{
    // clear output infomation
    m_output.connected_state = OVL_PORT_DISABLE;
    memset(&m_output.param, 0, sizeof(OverlayPortParam));
}

status_t OverlayEngine::ignoreInput(int id, int type)
{
    AutoMutex l(m_lock);

    if (id >= m_max_inputs)
    {
        OLOGE("Failed to ignore invalid overlay input(%d)", id);
        return BAD_INDEX;
    }

    if (OVL_INPUT_NONE == m_inputs[id]->connected_type)
    {
        return BAD_INDEX;
    }

    if (IGNORE_DEFAULT != type &&
        type != m_inputs[id]->connected_type)
    {
        return BAD_INDEX;
    }

    if (!m_async_update)
        m_input_params[id]->state = OVL_IN_PARAM_IGNORE;

    return NO_ERROR;
}

status_t OverlayEngine::prepareOutput(OverlayPrepareParam& param)
{
    AutoMutex l(m_lock);

    param.id = m_max_inputs;
    m_device->prepareOverlayOutput(m_disp_id, &param);

    return NO_ERROR;
}

status_t OverlayEngine::setOutput(OverlayPortParam* param, bool mirrored)
{
    AutoMutex l(m_lock);

    if (CC_UNLIKELY(param == NULL))
    {
        OLOGE("HWC->OVL: output param is NULL, disable output");
        disableOutputLocked();
        return INVALID_OPERATION;
    }

    if (mirrored && (m_output.queue != NULL))
    {
        DisplayBufferQueue::DisplayBuffer mir_buffer;
        m_output.queue->acquireBuffer(&mir_buffer, true);
        m_output.queue->releaseBuffer(mir_buffer.index, param->mir_rel_fence_fd);
        m_cond.signal();
    }

    m_output.connected_state = OVL_PORT_ENABLE;

    memcpy(&m_output.param, param, sizeof(OverlayPortParam));

    if (DisplayManager::m_profile_level & PROFILE_TRIG)
    {
        char atrace_tag[128];
        sprintf(atrace_tag, "set_ovl(%d): set output", m_disp_id);
        HWC_ATRACE_NAME(atrace_tag);
        OLOGI("HWC->OVL: set output (mir=%c)", mirrored ? 'y' : 'n');

        m_device->enableOverlayOutput(m_disp_id, param);
    }
    else
    {
        m_device->enableOverlayOutput(m_disp_id, param);
    }

    return NO_ERROR;
}

status_t OverlayEngine::preparePresentFence(OverlayPrepareParam& param)
{
    AutoMutex l(m_lock);

    if (HWC_DISPLAY_PRIMARY == m_disp_id)
    {
        m_device->prepareOverlayPresentFence(m_disp_id, &param);

        if (param.fence_fd <= 0)
        {
            OLOGD("Failed to get presentFence fd(%d)!!", param.fence_fd);
        }
    }
    else
    {
        param.fence_fd = -1;
        param.fence_index = 0;
    }

    return NO_ERROR;
}

status_t OverlayEngine::createOutputQueue(int format, bool secure)
{
    AutoMutex l(m_lock);
    return createOutputQueueLocked(format, secure);
}

status_t OverlayEngine::createOutputQueueLocked(int format, bool secure)
{
#ifndef MTK_USER_BUILD
    char atrace_tag[128];
    sprintf(atrace_tag, "create_out_queue(%d)", m_disp_id);
    HWC_ATRACE_NAME(atrace_tag);
#endif

    bool need_init = false;

    // verify if need to create output queue
    if (m_output.queue == NULL)
    {
        need_init = true;

        m_output.queue = new DisplayBufferQueue(DisplayBufferQueue::QUEUE_TYPE_OVL);
        m_output.queue->setSynchronousMode(true);

        OLOGD("Create output queue");
    }

    int bpp = getBitsPerPixel(format);

    DisplayData* disp_data = &DisplayManager::getInstance().m_data[m_disp_id];
    DisplayBufferQueue::BufferParam buffer_param;
    buffer_param.width  = disp_data->width;
    buffer_param.height = disp_data->height;
    buffer_param.pitch  = disp_data->width;
    buffer_param.format = mapGrallocFormat(format);
    buffer_param.size   = (disp_data->width * disp_data->height * bpp / 8);
    buffer_param.dequeue_block = false;
    m_output.queue->setBufferParam(buffer_param);

    if (need_init)
    {
        // allocate buffers
        const size_t buffer_slots = DisplayBufferQueue::NUM_BUFFER_SLOTS;
        DisplayBufferQueue::DisplayBuffer mir_buffer[buffer_slots];
        for (size_t i = 0; i < buffer_slots; i++)
        {
            m_output.queue->dequeueBuffer(&mir_buffer[i], false, secure);
        }

        for (size_t i = 0; i < buffer_slots; i++)
        {
            m_output.queue->cancelBuffer(mir_buffer[i].index);
        }

        OLOGD("Initialize buffers for output queue");
    }

    return NO_ERROR;
}

status_t OverlayEngine::releaseOutputQueue()
{
    AutoMutex l(m_lock);

    m_output.connected_state = OVL_PORT_DISABLE;
    m_output.queue = NULL;

    OLOGD("Output buffer queue is relased");

    return NO_ERROR;
}

status_t OverlayEngine::configMirrorOutput(HWBuffer* outbuf, bool secure)
{
#ifndef MTK_USER_BUILD
    char atrace_tag[128];
    sprintf(atrace_tag, "set_mirror(%d)", m_disp_id);
    HWC_ATRACE_NAME(atrace_tag);
#endif

    AutoMutex l(m_lock);

    // if virtial display is used as mirror source
    // no need to use extra buffer since it already has its own output buffer
    if ((HWC_DISPLAY_VIRTUAL == m_disp_id) || (outbuf == NULL))
        return NO_ERROR;

    // it can happen if SurfaceFlinger tries to access output buffer queue
    // right after hotplug thread just released it (e.g. via onPlugOut())
    if (CC_UNLIKELY(m_output.queue == NULL))
    {
        OLOGW("output buffer queue has been released");
        return INVALID_OPERATION;
    }

    // prepare overlay output buffer
    DisplayBufferQueue::DisplayBuffer out_buffer;
    unsigned int acq_fence_idx = 0;
    int if_fence_fd = -1;
    unsigned int if_fence_idx = 0;
    {
        status_t err;

        do
        {
            err = m_output.queue->dequeueBuffer(&out_buffer, true, secure);
            if (NO_ERROR != err)
            {
                OLOGW("cannot find available buffer, wait...");
                m_cond.wait(m_lock);
                OLOGW("wake up to find available buffer");
            }
        } while (NO_ERROR != err);

        OverlayPrepareParam prepare_param;
        prepare_param.id            = m_max_inputs;
        prepare_param.ion_fd        = out_buffer.out_ion_fd;
        prepare_param.is_need_flush = 0;
        prepare_param.secure        = out_buffer.secure;
        m_device->prepareOverlayOutput(m_disp_id, &prepare_param);
        if (prepare_param.fence_fd <= 0)
        {
            OLOGW("Failed to get mirror acquireFence !!");
        }

        out_buffer.acquire_fence = prepare_param.fence_fd;
        acq_fence_idx            = prepare_param.fence_index;
        if_fence_fd              = prepare_param.if_fence_fd;
        if_fence_idx             = prepare_param.if_fence_index;

        m_output.queue->queueBuffer(&out_buffer);
    }

    // fill mirror output buffer info
    outbuf->mir_out_sec_handle    = out_buffer.out_sec_handle;
    outbuf->mir_out_rel_fence_fd  = out_buffer.release_fence;
    outbuf->mir_out_acq_fence_fd  = out_buffer.acquire_fence;
    outbuf->mir_out_acq_fence_idx = acq_fence_idx;
    outbuf->mir_out_if_fence_fd   = if_fence_fd;
    outbuf->mir_out_if_fence_idx  = if_fence_idx;
    outbuf->handle                = out_buffer.out_handle;
    getPrivateHandle(outbuf->handle, &outbuf->priv_handle);

    if (DisplayManager::m_profile_level & PROFILE_TRIG)
    {
        OLOGI("HWC->OVL: config output (rel_fd=%d acq_fd=%d/idx=%u)",
            out_buffer.release_fence, out_buffer.acquire_fence, acq_fence_idx);
    }

    return NO_ERROR;
}

status_t OverlayEngine::setOverlaySessionMode(DISP_MODE mode)
{
    return m_device->setOverlaySessionMode(m_disp_id, mode);
}

DISP_MODE OverlayEngine::getOverlaySessionMode()
{
    return (DISP_MODE)m_device->getOverlaySessionMode(m_disp_id);
}

status_t OverlayEngine::trigger(int present_fence_idx, DISP_DC_TYPE type)
{
    AutoMutex l(m_lock);

    if (HWC_DISPLAY_VIRTUAL <= m_disp_id)
    {
        if (OVL_PORT_ENABLE != m_output.connected_state)
        {
            if (DisplayManager::m_profile_level & PROFILE_TRIG)
            {
                char atrace_tag[128];
                sprintf(atrace_tag, "trig_ovl(%d): fail w/o output", m_disp_id);
                HWC_ATRACE_NAME(atrace_tag);
            }

            OLOGE("Try to trigger w/o set output port !!");
            return -EINVAL;
        }
    }

    status_t err;

    int fence_idx = present_fence_idx;
    if (DisplayManager::m_profile_level & PROFILE_TRIG)
    {
        char atrace_tag[128];
        sprintf(atrace_tag, "trig_ovl(%d)", m_disp_id);
        HWC_ATRACE_NAME(atrace_tag);
        OLOGV("HWC->OVL: trig");

        err = m_device->triggerOverlaySession(m_disp_id, fence_idx, type);
    }
    else
    {
        err = m_device->triggerOverlaySession(m_disp_id, fence_idx, type);
    }

    m_async_update = false;

    return err;
}

OverlayPortParam* const* OverlayEngine::getInputParams()
{
    return m_input_params.array();
}

void OverlayEngine::setPowerMode(int mode)
{
    AutoMutex l(m_lock);

    switch (mode)
    {
        case HWC_POWER_MODE_OFF:
            {
                m_state = OVL_ENGINE_PAUSED;

                int num = m_device->getAvailableOverlayInput(m_disp_id);

                if (HWC_DISPLAY_VIRTUAL > m_disp_id)
                {
                    m_device->disableOverlaySession(
                        m_disp_id, m_input_params.array(), num);
                }

                for (int id = 0; id < m_max_inputs; id++)
                {
                    if (OVL_INPUT_NONE != m_inputs[id]->connected_type)
                        disableInputLocked(id);
                }
            }
            break;

        case HWC_POWER_MODE_DOZE:
        case HWC_POWER_MODE_NORMAL:
            m_state = OVL_ENGINE_ENABLED;
            break;

        case HWC_POWER_MODE_DOZE_SUSPEND:
            m_state = OVL_ENGINE_PAUSED;
            break;
    }

    if (HWC_DISPLAY_VIRTUAL > m_disp_id)
    {
        m_device->setPowerMode(m_disp_id, mode);
    }
}

sp<DisplayBufferQueue> OverlayEngine::getInputQueue(int id) const
{
    AutoMutex l(m_lock);

    if (id >= m_max_inputs)
    {
        OLOGE("Failed to get overlay input queue(%d)", id);
        return NULL;
    }

    if (OVL_INPUT_QUEUE != m_inputs[id]->connected_type)
    {
        OLOGW("No overlay input queue(%d)", id);
        return NULL;
    }

    return m_inputs[id]->queue;
}

void OverlayEngine::updateInput(int id)
{
    AutoMutex l(m_lock);

    if (m_inputs[id]->connected_state == OVL_PORT_ENABLE)
    {
        // acquire next buffer
        DisplayBufferQueue::DisplayBuffer buffer;
        m_inputs[id]->queue->acquireBuffer(&buffer);

        // get release fence for input queue
        OverlayPrepareParam prepare_param;
        {
            prepare_param.id            = id;
            prepare_param.ion_fd        = buffer.out_ion_fd;
            prepare_param.is_need_flush = 0;

            // special usage for MDP behaves as OVL
            prepare_param.secure        = buffer.secure;

            m_device->prepareOverlayInput(m_disp_id, &prepare_param);

            if (prepare_param.fence_fd <= 0)
            {
                OLOGE("(%d) Failed to get releaseFence for input queue", id);
            }

            // special usage for MDP behaves as OVL
            // replace out_ion_fd by new imported ion_fd
            if (buffer.out_ion_fd != prepare_param.ion_fd)
                buffer.out_ion_fd = prepare_param.ion_fd;
        }

        // fill struct for enable layer
        OverlayPortParam* param = m_input_params[id];
        param->state          = OVL_IN_PARAM_ENABLE;
        if (buffer.secure)
        {
            param->va         = (void*)(uintptr_t)buffer.out_sec_handle;
            param->mva        = (void*)(uintptr_t)buffer.out_sec_handle;
        }
        else
        {
            param->va         = NULL;
            param->mva        = NULL;
        }
        param->pitch          = buffer.data_pitch;
        param->format         = buffer.data_format;
        param->color_range    = buffer.data_color_range;
        param->src_crop       = buffer.data_info.src_crop;
        param->dst_crop       = buffer.data_info.dst_crop;
        param->is_sharpen     = buffer.data_info.is_sharpen;
        param->fence_index    = prepare_param.fence_index;
        param->identity       = HWLAYER_ID_DBQ;
        param->connected_type = OVL_INPUT_QUEUE;
        param->protect        = buffer.protect;
        param->secure         = buffer.secure;
        param->alpha_enable   = buffer.alpha_enable;
        param->alpha          = buffer.alpha;
        param->blending       = buffer.blending;
        param->sequence       = buffer.sequence;
        param->dim            = false;
        param->ion_fd         = buffer.out_ion_fd;

        if (DisplayManager::m_profile_level & PROFILE_TRIG)
        {
            char atrace_tag[128];
            sprintf(atrace_tag, "set_ovl: input(%d) queue\n", id);
            HWC_ATRACE_NAME(atrace_tag);

            OLOGI("HWC->OVL: input(%d) queue", id);
        }

        // release buffer
        m_inputs[id]->queue->releaseBuffer(buffer.index, prepare_param.fence_fd);

        m_async_update = true;
    }
}

void OverlayEngine::flip()
{
    // check AEE layer for primary display
    if (HWC_DISPLAY_PRIMARY == m_disp_id)
    {
        AutoMutex l(m_lock);
        int avail_inputs = m_device->getAvailableOverlayInput(m_disp_id);
        for (int id = avail_inputs; id < m_max_inputs; id++)
        {
            if (OVL_INPUT_NONE != m_inputs[id]->connected_type)
                disableInputLocked(id);
        }
    }
}

void OverlayEngine::dump(struct dump_buff* log, int dump_level)
{
    AutoMutex l(m_lock);

    int total_size = 0;

    dump_printf(log, "\n[HWC Compose State (%d)]\n", m_disp_id);
    for (int id = 0; id < m_max_inputs; id++)
    {
        if (dump_level & DUMP_MM)
        {
            if (OVL_INPUT_QUEUE == m_inputs[id]->connected_type &&
                m_inputs[id]->queue != NULL)
            {
                m_inputs[id]->queue->dump(DisplayBufferQueue::QUEUE_DUMP_LAST_ACQUIRED);
            }
        }

        if (m_inputs[id]->connected_state == OVL_PORT_ENABLE)
        {
            OverlayPortParam* param = m_input_params[id];

            dump_printf(log, "  (%d) f=%#x x=%d y=%d w=%d h=%d\n",
                id, param->format, param->dst_crop.left, param->dst_crop.top,
                param->dst_crop.getWidth(), param->dst_crop.getHeight());

            int layer_size = param->dst_crop.getWidth() * param->dst_crop.getHeight() * getBitsPerPixel(param->format) / 8;
            total_size += layer_size;

#ifdef MTK_HWC_PROFILING
            if (HWC_LAYER_TYPE_FBT == param->identity)
            {
                dump_printf(log, "  FBT(n=%d, bytes=%d)\n",
                    param->fbt_input_layers, param->fbt_input_bytes + layer_size);
            }
#endif
        }
    }

    if (dump_level & DUMP_MM)
    {
        if (OVL_PORT_ENABLE == m_output.connected_state &&
            m_output.queue != NULL)
        {
            m_output.queue->dump(DisplayBufferQueue::QUEUE_DUMP_LAST_ACQUIRED);
        }
    }

    dump_printf(log, "  Total size: %d bytes\n", total_size);
}
