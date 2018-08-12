#define DEBUG_LOG_TAG "NOD"

#include <GraphicBufferUtil.h>
#include <cutils/properties.h>

#include "utils/debug.h"
#include "utils/tools.h"
#include "bliter_ultra.h"
#include "overlay.h"
#include "dispatcher.h"
#include "Transform.h"
#include "sync.h"
#include "display.h"

#define NLOGD(x, ...) HWC_LOGD("(%d) " x, m_dpy, ##__VA_ARGS__)
#define NLOGI(x, ...) HWC_LOGI("(%d) " x, m_dpy, ##__VA_ARGS__)
#define NLOGW(x, ...) HWC_LOGW("(%d) " x, m_dpy, ##__VA_ARGS__)
#define NLOGE(x, ...) HWC_LOGE("(%d) " x, m_dpy, ##__VA_ARGS__)

ANDROID_SINGLETON_STATIC_INSTANCE(UltraBliter);

BliterNode::BliterNode(DpAsyncBlitStream* blit_stream, uint32_t dpy)
    : m_blit_stream(blit_stream)
    , m_dpy(dpy)
{
    const uint32_t dbg_flag = DbgLogger::TYPE_HWC_LOG;

    m_buffer_logger     = new DbgLogger(dbg_flag, 'D');
    m_config_logger     = new DbgLogger(dbg_flag | DbgLogger::TYPE_PERIOD, 'D');
    m_geometry_logger   = new DbgLogger(dbg_flag | DbgLogger::TYPE_PERIOD, 'D');

    char value[PROPERTY_VALUE_MAX];

    // Read property for bypass MDP
    property_get("debug.hwc.bypassMDP", value, "0");
    m_bypass_mdp_for_debug = (0 != atoi(value));
}

BliterNode::~BliterNode()
{
    delete m_buffer_logger;
    delete m_config_logger;
    delete m_geometry_logger;
}

void BliterNode::cancelJob(uint32_t job_id)
{
    closeFenceFd(&m_src_param.bufInfo.fence_fd);
    for (uint32_t i = 0; i < MAX_OUT_CNT; i++)
    {
        DstInvalidateParam& dst_param = m_dst_param[i];

        if (!dst_param.enable)
            continue;

        closeFenceFd(&dst_param.bufInfo.fence_fd);
    }

    m_blit_stream->cancelJob(job_id);
}

status_t BliterNode::errorCheck()
{
    SrcInvalidateParam& src_param = m_src_param;
    BufferInfo& src_buf = src_param.bufInfo;

    if (m_bypass_mdp_for_debug)
    {
        NLOGE("errorCheck / Bypass MDP");
        return -EINVAL;
    }

    if (src_param.is_secure)
    {
        if (src_buf.sec_handle == 0)
        {
            NLOGE("errorCheck / no src handle");
            return -EINVAL;
        }
    }
    else
    {
        if (src_buf.ion_fd < 0)
        {
            NLOGE("errorCheck / no src ion fd");
            return -EINVAL;
        }
    }

    uint32_t output_cnt = 0;
    for (uint32_t i = 0; i < MAX_OUT_CNT; i++)
    {
        DstInvalidateParam& dst_param = m_dst_param[i];
        BufferInfo& dst_buf = dst_param.bufInfo;
        if (!dst_param.enable)
            continue;

        if (src_param.is_secure)
        {
            if (dst_buf.sec_handle == 0)
            {
                NLOGE("errorCheck(%d) / no dst handle", i);
                return -EINVAL;
            }
        }
        else
        {
            if (dst_buf.ion_fd < 0)
            {
                NLOGE("errorCheck(%d) / no dst ion fd", i);
                return -EINVAL;
            }
        }

        Rect& src_crop = dst_param.src_crop;
        Rect& dst_crop = dst_param.dst_crop;
        if ((src_crop.getWidth() <= 1) || (src_crop.getHeight() <= 1) ||
            (dst_crop.getWidth() <= 0) || (dst_crop.getHeight() <= 0))
        {
            NLOGE("errorCheck(%d) / unexpectedWH / src(%d,%d) dst(%d,%d)", i,
                    src_crop.getWidth(), src_crop.getHeight(),
                    dst_crop.getWidth(), dst_crop.getHeight());
            return -EINVAL;
        }

        output_cnt++;
    }

    if (0 == output_cnt)
    {
       NLOGW("errorCheck / no output");
       return -EINVAL;
    }

    return NO_ERROR;
}

DP_PROFILE_ENUM BliterNode::mapDpColorRange(const uint32_t range)
{
    switch (range)
    {
        case GRALLOC_EXTRA_BIT_YUV_BT601_NARROW:
            return DP_PROFILE_BT601;

        case GRALLOC_EXTRA_BIT_YUV_BT601_FULL:
            return DP_PROFILE_FULL_BT601;

        case GRALLOC_EXTRA_BIT_YUV_BT709_NARROW:
            return DP_PROFILE_BT709;
    }

    HWC_LOGW("Not support color range(%#x), use default BT601", range);
    return DP_PROFILE_BT601;
}

status_t BliterNode::calculateROI(DpRect* src_roi, DpRect* dst_roi,
                                    Rect& src_crop, Rect& dst_crop,
                                    Rect& dst_buf, bool deinterlace,
                                    const int& color_format)
{
    // [NOTE] width and height for DP should be 2 byte-aligned
    src_roi->x = src_crop.left;
    src_roi->y = src_crop.top;

    switch(color_format)
    {
        case DP_COLOR_RGBA8888:
        case DP_COLOR_BGRA8888:
        case DP_COLOR_RGB888:
        case DP_COLOR_RGB565:
            src_roi->w = src_crop.getWidth();
            src_roi->h = src_crop.getHeight();

            dst_roi->x = dst_crop.left;
            dst_roi->w = dst_crop.getWidth();

            dst_roi->y = dst_crop.top;
            dst_roi->h = dst_crop.getHeight();
            break;
        default:
            src_roi->w = ALIGN_FLOOR(src_crop.getWidth(), 2);
            src_roi->h = ALIGN_FLOOR(src_crop.getHeight(), 2);

            // x should be 2 byte-aligned in YUYV format
            dst_roi->x = ALIGN_FLOOR(dst_crop.left, 2);
            dst_roi->w = ALIGN_CEIL(dst_crop.getWidth() + (dst_crop.left & 0x1), 2);

            dst_roi->y = ALIGN_FLOOR(dst_crop.top, 2);
            dst_roi->h = ALIGN_CEIL(dst_crop.getHeight() + (dst_crop.top & 0x1), 2);
            break;
    }

    if (deinterlace) src_roi->h /= 2;

    // check for OVL limitation
    // if dst region is out of boundary, should adjust it
    if ((dst_roi->x + dst_roi->w) > dst_buf.getWidth())
    {
        HWC_LOGW("out of boundary W %d+%d>%d", dst_roi->x, dst_roi->w, dst_buf.getWidth());
        dst_roi->w -= 2;
    }

    if ((dst_roi->y + dst_roi->h) > dst_buf.getHeight())
    {
        HWC_LOGW("out of boundary H %d+%d>%d", dst_roi->y, dst_roi->h, dst_buf.getHeight());
        dst_roi->h -= 2;
    }
    return NO_ERROR;
}

unsigned int BliterNode::mapDpOrientation(const uint32_t transform)
{
    unsigned int orientation = DpAsyncBlitStream::ROT_0;

    // special case
    switch (transform)
    {
        // logically equivalent to (ROT_270 + FLIP_V)
        case (Transform::ROT_90 | Transform::FLIP_H):
            return (DpAsyncBlitStream::ROT_90 | DpAsyncBlitStream::FLIP_V);

        // logically equivalent to (ROT_270 + FLIP_H)
        case (Transform::ROT_90 | Transform::FLIP_V):
            return (DpAsyncBlitStream::ROT_90 | DpAsyncBlitStream::FLIP_H);
    }

    // general case
    if (Transform::FLIP_H & transform)
        orientation |= DpAsyncBlitStream::FLIP_H;

    if (Transform::FLIP_V & transform)
        orientation |= DpAsyncBlitStream::FLIP_V;

    if (Transform::ROT_90 & transform)
        orientation |= DpAsyncBlitStream::ROT_90;

    return orientation;
}

status_t BliterNode::invalidate(uint32_t job_id)
{
    HWC_ATRACE_CALL();

    DbgLogger& buf_logger = *m_buffer_logger;
    DbgLogger& cfg_logger = *m_config_logger;
    DbgLogger& geo_logger = *m_geometry_logger;

    buf_logger.printf("[NOD] (%d, %d)", m_dpy, job_id);
    cfg_logger.printf("[NOD] (%d)cfg", m_dpy);
    geo_logger.printf("[NOD] (%d)geo", m_dpy);

    if (NO_ERROR != errorCheck())
    {
        cancelJob(job_id);
        return -EINVAL;
    }

    SrcInvalidateParam& src_param = m_src_param;
    BufferInfo& src_buf = src_param.bufInfo;

    DpSecure dp_secure = src_param.is_secure ? DP_SECURE : DP_SECURE_NONE;
    DP_PROFILE_ENUM range = mapDpColorRange(src_param.gralloc_color_range);

    m_blit_stream->setConfigBegin(job_id);

    buf_logger.printf(" S/fence=%d", src_buf.fence_fd);
    if (DP_SECURE == dp_secure)
    {
        void* src_addr[3];
        src_addr[0] = (void*)(uintptr_t)src_buf.sec_handle;
        src_addr[1] = (void*)(uintptr_t)src_buf.sec_handle;
        src_addr[2] = (void*)(uintptr_t)src_buf.sec_handle;

        m_blit_stream->setSrcBuffer(src_addr, src_buf.size,
                                    src_buf.plane, src_buf.fence_fd);

        buf_logger.printf("/buf=0x%x", src_buf.sec_handle);
    }
    else
    {
        int src_ion_fd = src_buf.ion_fd;
        m_blit_stream->setSrcBuffer(src_ion_fd, src_buf.size,
                                    src_buf.plane, src_buf.fence_fd);

        buf_logger.printf("/buf=%d", src_ion_fd);
    }
    src_buf.fence_fd = -1;

    // [NOTE] setSrcConfig provides y and uv pitch configuration
    // if uv pitch is 0, DP would calculate it according to y pitch
    m_blit_stream->setSrcConfig(src_buf.rect.getWidth(), src_buf.rect.getHeight(),
                                        src_buf.pitch, src_buf.pitch_uv, src_buf.dpformat, range,
                                        eInterlace_None, dp_secure, src_param.is_flush);

    geo_logger.printf(" S(%d,%d)", src_buf.rect.getWidth(), src_buf.rect.getHeight());
    cfg_logger.printf(" sec=%d/flush=%d/range=%d S/fmt=%d",
                            src_param.is_secure, src_param.is_flush, range, src_buf.dpformat);

    for (uint32_t i = 0; i < MAX_OUT_CNT; i++)
    {
        DstInvalidateParam& dst_param = m_dst_param[i];
        BufferInfo& dst_buf = dst_param.bufInfo;
        if (!dst_param.enable)
            continue;

        buf_logger.printf(" D%d/fence=%d", i, dst_buf.fence_fd);
        if (DP_SECURE == dp_secure)
        {
            void* dst_addr[3];
            dst_addr[0] = (void*)(uintptr_t)dst_buf.sec_handle;
            dst_addr[1] = (void*)(uintptr_t)dst_buf.sec_handle;
            dst_addr[2] = (void*)(uintptr_t)dst_buf.sec_handle;

            m_blit_stream->setDstBuffer(i, dst_addr, dst_buf.size,
                                        dst_buf.plane, dst_buf.fence_fd);
            buf_logger.printf("/buf=0x%x", dst_buf.sec_handle);
        }
        else
        {
            m_blit_stream->setDstBuffer(i, dst_buf.ion_fd, dst_buf.size,
                                        dst_buf.plane, dst_buf.fence_fd);
            buf_logger.printf("/buf=%d", dst_buf.ion_fd);
        }
        dst_buf.fence_fd = -1;

        DpRect src_roi;
        DpRect dst_roi;

        calculateROI(&src_roi, &dst_roi, dst_param.src_crop, dst_param.dst_crop,
                        dst_buf.rect, src_param.deinterlace, src_buf.dpformat);

        cfg_logger.printf(" D%d/fmt=%d", i , dst_buf.dpformat);
        geo_logger.printf(" D%d/(%d,%d)/xform=%d/(%d,%d,%d,%d)->(%d,%d,%d,%d)", i,
                                  dst_buf.rect.getWidth(), dst_buf.rect.getHeight(), dst_param.xform,
                                  src_roi.x, src_roi.y, src_roi.w, src_roi.h,
                                  dst_roi.x, dst_roi.y, dst_roi.w, dst_roi.h);

        {
            // TODO:workaround for MDP issue
            static DpRect main_src;
            if (i == 0)
               main_src = src_roi;
            else
               src_roi = main_src;
        }

        m_blit_stream->setSrcCrop(i, src_roi);

        // [NOTE] setDstConfig provides y and uv pitch configuration
        // if uv pitch is 0, DP would calculate it according to y pitch
        // ROI designates the dimension and the position of the bitblited image
        m_blit_stream->setDstConfig(i, dst_roi.w, dst_roi.h, dst_buf.pitch, dst_buf.pitch_uv,
                            dst_buf.dpformat, range, eInterlace_None, &dst_roi, dp_secure, false);

        m_blit_stream->setOrientation(i, mapDpOrientation(dst_param.xform));

        DpPqParam dppq_param;
        dppq_param.enable = dst_param.is_enhance;
        dppq_param.scenario = MEDIA_VIDEO;
        dppq_param.u.video.id = src_param.pool_id;
        dppq_param.u.video.timeStamp = src_param.time_stamp;
        dppq_param.u.video.grallocExtraHandle = src_param.bufInfo.handle;
        const bool is_multi_displays = DisplayManager::getInstance().m_data[HWC_DISPLAY_EXTERNAL].connected ||
            DisplayManager::getInstance().m_data[HWC_DISPLAY_VIRTUAL].connected;
        dppq_param.u.video.isHDR2SDR = is_multi_displays;
        m_blit_stream->setPQParameter(i, dppq_param);
    }

    m_blit_stream->setConfigEnd();

    buf_logger.tryFlush();
    cfg_logger.tryFlush();
    geo_logger.tryFlush();

    for (uint32_t i = 0; i < MAX_OUT_CNT; i++)
    {
        m_dst_param[i].enable = false;
    }

    DP_STATUS_ENUM status = m_blit_stream->invalidate();
    if (DP_STATUS_RETURN_SUCCESS != status)
    {
        NLOGE("errorCheck /blit fail/err=%d", status);
        return -EINVAL;
    }

    return NO_ERROR;
}

void BliterNode::setSrc(BufferConfig* config, PrivateHandle& src_priv_handle, int* src_fence_fd)
{
    BliterNode::BufferInfo& src_buf = m_src_param.bufInfo;

    src_buf.ion_fd        = src_priv_handle.ion_fd;
    src_buf.sec_handle    = src_priv_handle.sec_handle;
    src_buf.handle        = src_priv_handle.handle;

    if (NULL != src_fence_fd)
        passFenceFd(&src_buf.fence_fd, src_fence_fd);
    else
        src_buf.fence_fd = -1;

    src_buf.dpformat    = config->src_dpformat;
    src_buf.pitch       = config->src_pitch;
    src_buf.pitch_uv    = config->src_pitch_uv;
    src_buf.plane       = config->src_plane;
    src_buf.rect        = Rect(config->src_width, config->src_height);
    memcpy(src_buf.size, config->src_size, sizeof(src_buf.size));

    m_src_param.deinterlace   = config->deinterlace;
    m_src_param.is_secure     = isSecure(&src_priv_handle);
    m_src_param.gralloc_color_range = config->gralloc_color_range;

    m_src_param.pool_id       = src_priv_handle.ext_info.pool_id;
    m_src_param.time_stamp    = src_priv_handle.ext_info.timestamp;

    m_src_param.is_flush = false;
    if (src_priv_handle.usage & GRALLOC_USAGE_SW_WRITE_MASK)
    {
        if (!(src_priv_handle.ext_info.status & GRALLOC_EXTRA_MASK_FLUSH))
            m_src_param.is_flush = true;
    }
}

void BliterNode::setDst(uint32_t idx, Parameter* param, int ion_fd, SECHAND sec_handle, int* dst_fence_fd)
{
    if (idx >= MAX_OUT_CNT)
        return;

    DstInvalidateParam& dst_param = m_dst_param[idx];

    BufferConfig* config = param->config;
    BliterNode::BufferInfo& dst_buf = dst_param.bufInfo;

    dst_buf.ion_fd      = ion_fd;
    dst_buf.sec_handle  = sec_handle;
    dst_buf.dpformat    = config->dst_dpformat;
    dst_buf.pitch       = config->dst_pitch;
    dst_buf.pitch_uv    = config->dst_pitch_uv;
    dst_buf.plane       = config->dst_plane;
    dst_buf.rect        = Rect(config->dst_width, config->dst_height);
    dst_buf.size[0]     = config->dst_size[0];
    dst_buf.size[1]     = config->dst_size[1];
    dst_buf.size[2]     = config->dst_size[2];

    if (NULL != dst_fence_fd)
        passFenceFd(&dst_buf.fence_fd, dst_fence_fd);
    else
        dst_buf.fence_fd = -1;

    dst_param.xform     = param->xform;
    dst_param.src_crop  = param->src_roi;
    dst_param.dst_crop  = param->dst_roi;
    // enable PQ when feature support, and buffer source type is video

    dst_param.is_enhance = param->pq_enhance;
    dst_param.enable = true;
}
// ===============================================================================================

UltraBliter::UltraBliter()
    : m_bliter_node(NULL)
    , m_blit_stream(NULL)
    , m_src_buf(NULL)
    , m_job_seq(0)
    , m_status(NO_ERROR)
    , m_is_debug_on(false)
{
    m_sync_state = SyncState_init;

    for (uint32_t i = 0; i < 2; i++)
    {
        m_param[i] = NULL;
        m_dst_buf[i] = NULL;
        m_action[i] = CancelJob;
    }
}

UltraBliter::~UltraBliter()
{

}

void UltraBliter::setBliter(BliterNode* blit_node)
{
    m_bliter_node = blit_node;
}

void UltraBliter::masterSetSrcBuf(HWLayer* buf, uint32_t job_seq)
{
    m_src_buf = buf;
    m_job_seq = job_seq;
}
void UltraBliter::setDstBuf(bool is_master, DisplayBuffer* buf)
{
    uint32_t idx = ID(is_master);
    m_dst_buf[idx] = buf;
}

void UltraBliter::config(int is_master, BliterNode::Parameter* param)
{
    uint32_t idx = ID(is_master);
    m_param[idx] = param;
}

void UltraBliter::barrier(bool is_master, SyncState sync_state)
{
    uint32_t idx = ID(is_master);

    if (is_master)
    {
        AutoMutex l(m_lock);
        while (SyncState_start != m_sync_state)
        {
            if (TIMED_OUT == m_condition.waitRelative(m_lock, ms2ns(5)))
            {
                HWC_LOGD("Master is waiting at (%d) ...", sync_state);
            }
        }
        m_sync_state = sync_state;
        m_condition_inverse.signal();
    }
    else
    {
        m_sync_state = SyncState_start;
        m_condition.signal();

        AutoMutex l(m_lock_inverse);
        while (sync_state != m_sync_state)
        {
            if(TIMED_OUT == m_condition_inverse.waitRelative(m_lock_inverse, ms2ns(5)))
            {
                HWC_LOGD("Slave is waiting at (%d) ...", sync_state);
            }
        }
    }
}

status_t UltraBliter::processJob()
{
    HWC_ATRACE_CALL();

    if (NULL == m_bliter_node || NULL == m_param[0] || NULL == m_param[1])
    {
        HWC_LOGE("bliter node hasn't been inintialed !!!");
        return -EINVAL;
    }

    HWLayer* hw_layer = m_src_buf;
    hwc_layer_1_t* layer = &m_src_buf->layer;

    // -------------------------------------------------------------------------------------------
    m_bliter_node->setSrc(m_param[ID_MASTER]->config, hw_layer->priv_handle, &hw_layer->layer.acquireFenceFd);

    for (uint32_t i = 0; i < 2; i++)
    {
        if (CancelJob == m_action[i])
            continue;

        m_bliter_node->setDst(i, m_param[i], m_dst_buf[i]->out_ion_fd,
                                             m_dst_buf[i]->out_sec_handle,
                                             &m_dst_buf[i]->release_fence);
    }

    // draw debug line
    if (m_is_debug_on)
    {
        getGraphicBufferUtil().drawLine(layer->handle, 0xff, 64, 64, (uint32_t)hw_layer->mdp_job_id);
    }

    return m_bliter_node->invalidate(hw_layer->mdp_job_id);
}

status_t UltraBliter::trig(bool is_master, TrigAction action, int* mdp_fence_fd)
{
    HWC_ATRACE_CALL();

    uint32_t idx = ID(is_master);
    m_action[idx] = action;

    // Sync ====================================================================
    barrier(is_master, SyncState_set_buf_done);

    if (is_master)
    {
        m_status = processJob();
    }

    if (NULL != mdp_fence_fd)
    {
        *mdp_fence_fd = ((NO_ERROR == m_status) && (DoJob == action)) ?
                                        dup(m_src_buf->layer.releaseFenceFd) : -1;
    }

    // Sync ====================================================================
    barrier(is_master, SyncState_trig_done);

    return m_status;
}
