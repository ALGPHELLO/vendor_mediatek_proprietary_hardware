#define DEBUG_LOG_TAG "BLT"

#include "hwc_priv.h"
#include "graphics_mtk_defs.h"
#include "gralloc_mtk_defs.h"

#include <hardware/gralloc.h>

#include "utils/debug.h"
#include "utils/tools.h"

#include "utils/transform.h"

#include "bliter.h"
#include "display.h"
#include "platform.h"
#include "overlay.h"
#include "dispatcher.h"
#include "worker.h"
#include "queue.h"
#include "sync.h"
#include "hwdev.h"
#include "hwc.h"

#include <sync/sync.h>

#include <utils/String8.h>

#define ALIGN_FLOOR(x,a)    ((x) & ~((a) - 1L))
#define ALIGN_CEIL(x,a)     (((x) + (a) - 1L) & ~((a) - 1L))

#define NOT_PRIVATE_FORMAT -1
#define UI_PQ_VIDEO_ID 0xFFFFFF00

#define BLOGD(i, x, ...) HWC_LOGD("(%d:%d) " x, m_disp_id, i, ##__VA_ARGS__)
#define BLOGI(i, x, ...) HWC_LOGI("(%d:%d) " x, m_disp_id, i, ##__VA_ARGS__)
#define BLOGW(i, x, ...) HWC_LOGW("(%d:%d) " x, m_disp_id, i, ##__VA_ARGS__)
#define BLOGE(i, x, ...) HWC_LOGE("(%d:%d) " x, m_disp_id, i, ##__VA_ARGS__)

// ---------------------------------------------------------------------------
// Because the transformation of rotation and flipping is NOT COMMUTATIVE,
// if the order of rotation and flipping would affect the final orientation,
// we need to map the relationship between display subsys and GLES.
//
// NOTE:
// display subsys: ROT  -> FLIP
//           GLES: FLIP -> ROT
unsigned int mapDpOrientation(const uint32_t transform)
{
    unsigned int orientation = DpBlitStream::ROT_0;

    // special case
    switch (transform)
    {
        // logically equivalent to (ROT_270 + FLIP_V)
        case (Transform::ROT_90 | Transform::FLIP_H):
            return (DpBlitStream::ROT_90 | DpBlitStream::FLIP_V);

        // logically equivalent to (ROT_270 + FLIP_H)
        case (Transform::ROT_90 | Transform::FLIP_V):
            return (DpBlitStream::ROT_90 | DpBlitStream::FLIP_H);
    }

    // general case
    if (Transform::FLIP_H & transform)
        orientation |= DpBlitStream::FLIP_H;

    if (Transform::FLIP_V & transform)
        orientation |= DpBlitStream::FLIP_V;

    if (Transform::ROT_90 & transform)
        orientation |= DpBlitStream::ROT_90;

    return orientation;
}

DP_PROFILE_ENUM mapDpColorRange(const uint32_t range)
{
    DP_PROFILE_ENUM dp_range = DP_PROFILE_BT601;

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

// ---------------------------------------------------------------------------

BliterHandler::BliterHandler(int dpy, const sp<OverlayEngine>& ovl_engine)
    : LayerHandler(dpy, ovl_engine)
{
    int num = m_ovl_engine->getMaxInputNum();
    m_dp_configs = (BufferConfig*)calloc(1, sizeof(BufferConfig) * num);

    m_blit_stream.setUser(dpy);
}

BliterHandler::~BliterHandler()
{
    for (size_t i = 0; i < m_frame_queues.size(); i++)
    {
        delete m_frame_queues.valueAt(i);
    }

    free(m_dp_configs);
}

void BliterHandler::set(
    struct hwc_display_contents_1* list,
    DispatcherJob* job)
{
    if (HWC_MIRROR_SOURCE_INVALID != job->disp_mir_id)
    {
        setMirror(list, job);
        return;
    }

    uint32_t total_num = job->num_layers;

    for (uint32_t i = 0; i < total_num; i++)
    {
        // MULTIPASS: MM must be composed by one pass
        HWLayer* hw_layer = &job->hw_layers_multi[0][i];

        // skip non mm layers
        if (HWC_LAYER_TYPE_MM != hw_layer->type) continue;

        // this layer is not enable
        if (!hw_layer->enable) continue;

        hwc_layer_1_t* layer = &list->hwLayers[hw_layer->index];
        PrivateHandle* priv_handle = &hw_layer->priv_handle;

        status_t err = getPrivateHandleBuff(layer->handle, priv_handle);
        if (err != NO_ERROR)
        {
            hw_layer->enable = false;
            continue;
        }

        if (priv_handle->ion_fd > 0)
        {
            // TODO: should integrate prepare hehavior to DpBlitStream
            IONDevice::getInstance().ionImport(&priv_handle->ion_fd);
        }

        if (hw_layer->dirty)
        {
            // get release fence from sync fence
            layer->releaseFenceFd = m_sync_fence->create();
            if (-1 == layer->releaseFenceFd)
            {
                BLOGE(i, "Failed to create releaseFence");
                hw_layer->sync_marker = 0;
            }
            else
            {
                hw_layer->sync_marker = m_sync_fence->getLastMarker();
            }

            memcpy(&hw_layer->layer, layer, sizeof(hwc_layer_1_t));

            int pool_id = hw_layer->priv_handle.ext_info.pool_id;
            if (pool_id != 0)
            {
                // queuing lastest frame in order to show video frame asap
                AutoMutex l(m_frame_queue_lock);
                FrameQueue* frame_queue = m_frame_queues.valueFor(pool_id);
                if (frame_queue == NULL)
                {
                    frame_queue = new FrameQueue;
                    m_frame_queues.add(pool_id, frame_queue);
                }

                frame_queue->queue.push_back(hw_layer);

                BLOGD(i, "SET/async=que/pool=%d/rel=%d(%d)/acq=%d/handle=%p",
                    pool_id, layer->releaseFenceFd, hw_layer->sync_marker,
                    layer->acquireFenceFd, layer->handle);
            }
            else
            {
                BLOGD(i, "SET/rel=%d/acq=%d/handle=%p",
                    layer->releaseFenceFd, layer->acquireFenceFd, layer->handle);
            }

            if (DisplayManager::m_profile_level & PROFILE_BLT)
            {
                char atrace_tag[256];
                sprintf(atrace_tag, "mm set:%p", layer->handle);
                HWC_ATRACE_NAME(atrace_tag);
            }
        }
        else
        {
            if (layer->acquireFenceFd != -1) ::close(layer->acquireFenceFd);
            layer->releaseFenceFd = -1;

            memcpy(&hw_layer->layer, layer, sizeof(hwc_layer_1_t));

            BLOGD(i,"SET/async=bypass/acq=%d/handle=%p",
                layer->acquireFenceFd, layer->handle);

            if (DisplayManager::m_profile_level & PROFILE_BLT)
            {
                char atrace_tag[256];
                sprintf(atrace_tag, "mm bypass:%p", layer->handle);
                HWC_ATRACE_NAME(atrace_tag);
            }
        }

        layer->acquireFenceFd = -1;
    }
}

void BliterHandler::setMirror(
    struct hwc_display_contents_1* list,
    DispatcherJob* job)
{
    // clear all layer's acquire fence and retire fence
    if (list != NULL)
    {
        if (HWC_DISPLAY_VIRTUAL == job->disp_ori_id)
        {
            for (uint32_t i = 0; i < list->numHwLayers; i++)
            {
                hwc_layer_1_t* layer = &list->hwLayers[i];
                layer->releaseFenceFd = -1;
                if (layer->acquireFenceFd != -1) ::close(layer->acquireFenceFd);
            }
        }
        else
        {
            clearListAll(list);
        }
    }

    // get release fence from sync fence
    HWBuffer* hw_mirbuf = &job->hw_mirbuf;
    hw_mirbuf->mir_in_rel_fence_fd = m_sync_fence->create();
    if (-1 == hw_mirbuf->mir_in_rel_fence_fd)
    {
        BLOGE(0, "Failed to create mirror releaseFence");
        hw_mirbuf->mir_in_sync_marker = 0;
    }
    else
    {
        hw_mirbuf->mir_in_sync_marker = m_sync_fence->getLastMarker();
    }

    // [NOTE]
    // there are two users who uses the retireFenceFd
    // 1) SurfaceFlinger 2) HWComposer
    // hence, the fence file descriptor MUST be DUPLICATED before
    // passing to SurfaceFlinger;
    // otherwise, HWComposer may wait for the WRONG fence file descriptor that
    // has been closed by SurfaceFlinger.
    //
    // we would let bliter output to virtual display's outbuf directly.
    // So retire fence is as same as bliter's release fence
    if (HWC_DISPLAY_VIRTUAL == job->disp_ori_id)
    {
        list->retireFenceFd = ::dup(hw_mirbuf->mir_in_rel_fence_fd);
    }
}

void BliterHandler::releaseFence(int fd, unsigned int sync_marker, int ovl_in)
{
    if (fd != -1)
    {
        unsigned int curr_marker = m_sync_fence->getCurrMarker();
        while (sync_marker > curr_marker)
        {
            BLOGD(ovl_in, "Release releaseFence (fd=%d) (%d->%d)",
                fd, curr_marker, sync_marker);

            status_t err = m_sync_fence->inc(fd);
            if (err)
            {
                BLOGE(ovl_in, "Failed to release releaseFence (fd=%d): %s(%d)",
                    fd, strerror(-err), err);
            }

            curr_marker++;
        }
    }
}

bool BliterHandler::bypassBlit(HWLayer* hw_layer, int ovl_in)
{
    hwc_layer_1_t* layer = &hw_layer->layer;
    int pool_id = hw_layer->priv_handle.ext_info.pool_id;

    if (pool_id != 0)
    {
        // get queued frame for processing
        AutoMutex l(m_frame_queue_lock);

        FrameQueue* frame_queue = m_frame_queues.valueFor(pool_id);
        if (frame_queue != NULL)
        {
            HWLayer* next_hw_layer = NULL;

            if (!frame_queue->queue.empty())
            {
                HWLayerQueue::iterator front(frame_queue->queue.begin());
                next_hw_layer = *front;

                releaseFence(next_hw_layer->layer.releaseFenceFd,
                    next_hw_layer->sync_marker - 1, ovl_in);

                frame_queue->queue.erase(front);
                frame_queue->curr_hw_layer = next_hw_layer;
            }
            else if (hw_layer->dirty)
            {
                if (hw_layer->sync_marker < frame_queue->curr_hw_layer->sync_marker)
                {
                    next_hw_layer = frame_queue->curr_hw_layer;
                }
            }

            if (next_hw_layer != NULL)
            {
                // only use new native handle and private handle
                // should not use unpdated transform or src/dst roi
                layer->handle = next_hw_layer->layer.handle;
                layer->acquireFenceFd = next_hw_layer->layer.acquireFenceFd;
                layer->releaseFenceFd = next_hw_layer->layer.releaseFenceFd;
                memcpy(&hw_layer->priv_handle,
                       &next_hw_layer->priv_handle,
                       sizeof(PrivateHandle));

                // need to reserve last buffer
                if (hw_layer->sync_marker != next_hw_layer->sync_marker)
                    hw_layer->sync_marker = next_hw_layer->sync_marker - 1;

                BLOGD(ovl_in, "BLT/async=deq/pool=%d/rel=%d(%d)/acq=%d/handle=%p",
                    pool_id, layer->releaseFenceFd, hw_layer->sync_marker,
                    layer->acquireFenceFd, layer->handle);

                return false;
            }
        }
    }

    // there's no queued frame and current frame is not dirty
    if (hw_layer->dirty)
    {
        BLOGD(ovl_in, "BLT/async=curr/pool=%d/rel=%d(%d)/acq=%d/handle=%p",
            pool_id, layer->releaseFenceFd, hw_layer->sync_marker,
            layer->acquireFenceFd, layer->handle);

        return false;
    }

    status_t err = m_ovl_engine->ignoreInput(ovl_in, OverlayEngine::IGNORE_QUEUE);
    if (NO_ERROR != err)
    {
        BLOGD(ovl_in, "BLT/async=again/pool=%d/rel=%d(%d)/acq=%d/handle=%p",
            pool_id, layer->releaseFenceFd, hw_layer->sync_marker,
            layer->acquireFenceFd, layer->handle);

        return false;
    }

    HWC_ATRACE_NAME("bypass");

    BLOGD(ovl_in, "BLT/async=nop/pool=%d/handle=%p", pool_id, layer->handle);

    return true;
}

sp<DisplayBufferQueue> BliterHandler::getDisplayBufferQueue(
    PrivateHandle* priv_handle, BufferConfig* config, int ovl_in) const
{
    sp<DisplayBufferQueue> queue = m_ovl_engine->getInputQueue(ovl_in);
    if (queue == NULL)
    {
        queue = new DisplayBufferQueue(DisplayBufferQueue::QUEUE_TYPE_BLT);
        queue->setSynchronousMode(false);

        // connect to OverlayEngine
        m_ovl_engine->setInputQueue(ovl_in, queue);
    }

    int format = priv_handle->format;
    const int bpp = getBitsPerPixel(format);

    // set buffer format to buffer queue
    // TODO: should refer to layer->displayFrame
    DisplayBufferQueue::BufferParam buffer_param;
    buffer_param.disp_id = m_disp_id;
    buffer_param.pool_id = priv_handle->ext_info.pool_id;
    buffer_param.width   = m_disp_data->width;
    buffer_param.height  = m_disp_data->height;
    buffer_param.pitch   = m_disp_data->width;
    buffer_param.format  = mapGrallocFormat(format);
    // TODO: should calculate the size from the information of gralloc?
    buffer_param.size    = (m_disp_data->width * m_disp_data->height * bpp / 8);
    buffer_param.protect = (priv_handle->usage & GRALLOC_USAGE_PROTECTED);
    queue->setBufferParam(buffer_param);

    // TODO: should refer to layer->displayFrame
    config->dst_width = m_disp_data->width;
    config->dst_height = m_disp_data->height;
    config->dst_pitch = m_disp_data->width * bpp / 8;
    config->dst_pitch_uv = 0;

    return queue;
}

status_t BliterHandler::setDpConfig(
    PrivateHandle* priv_handle, BufferConfig* config, int ovl_in)
{
    // check if private color format is changed
    bool private_format_change = false;
    int curr_private_format = NOT_PRIVATE_FORMAT;
    if (HAL_PIXEL_FORMAT_YUV_PRIVATE == priv_handle->format ||
        HAL_PIXEL_FORMAT_YCbCr_420_888 == priv_handle->format)
    {
        curr_private_format = (priv_handle->ext_info.status & GRALLOC_EXTRA_MASK_CM);
        if (HAL_PIXEL_FORMAT_YUV_PRIVATE == config->gralloc_format ||
            HAL_PIXEL_FORMAT_YCbCr_420_888 == config->gralloc_format)
        {
            private_format_change = (config->gralloc_private_format != curr_private_format);
        }
    }

    bool ufo_align_change = false;
    int curr_ufo_align = 0;
    if (HAL_PIXEL_FORMAT_UFO == priv_handle->format ||
        GRALLOC_EXTRA_BIT_CM_UFO == curr_private_format)
    {
        curr_ufo_align = ((priv_handle->ext_info.status & GRALLOC_EXTRA_MASK_UFO_ALIGN) >> 2);
        ufo_align_change = (config->gralloc_ufo_align_type != curr_ufo_align);
    }

    if (config->gralloc_y_stride == priv_handle->y_stride &&
        config->gralloc_width == priv_handle->width &&
        config->gralloc_height == priv_handle->height &&
        config->gralloc_cbcr_align == priv_handle->cbcr_align &&
        config->gralloc_vertical_stride == priv_handle->vstride &&
        config->gralloc_format == priv_handle->format &&
        private_format_change  == false &&
        ufo_align_change == false)
    {
        // data format is not changed
        if (!config->is_valid)
        {
            BLOGW(ovl_in, "Format is not changed, but config in invalid !");
            return -EINVAL;
        }

        return NO_ERROR;
    }

    BLOGD(ovl_in, "Format Change (w=%d h=%d f=0x%x) -> (w=%d h=%d s=%d vs=%d, f=0x%x pf=0x%x ua=%d)",
        config->gralloc_width, config->gralloc_height, config->gralloc_format,
        priv_handle->width, priv_handle->height, priv_handle->y_stride, priv_handle->vstride, priv_handle->format,
        curr_private_format, curr_ufo_align);

    // remember current buffer data format for next comparison
    config->gralloc_width  = priv_handle->width;
    config->gralloc_height = priv_handle->height;
    config->gralloc_y_stride = priv_handle->y_stride;
    config->gralloc_cbcr_align = priv_handle->cbcr_align;
    config->gralloc_vertical_stride = priv_handle->vstride;
    config->gralloc_format = priv_handle->format;
    config->gralloc_private_format = curr_private_format;
    config->gralloc_ufo_align_type = curr_ufo_align;

    //
    // set DpFramework configuration
    //
    unsigned int src_size_luma = 0;
    unsigned int src_size_chroma = 0;

    // user (video/camera) must in chage of width/height alignment
    unsigned int width  = priv_handle->width;
    unsigned int height = priv_handle->height;
    unsigned int y_stride = priv_handle->y_stride;
    unsigned int vertical_stride = priv_handle->vstride;

    // reset uv pitch since RGB does not need it
    config->src_pitch_uv = 0;

    unsigned int input_format = grallocColor2HalColor(priv_handle->format, curr_private_format);
    if (input_format == 0)
    {
        BLOGE(ovl_in, "Private Format is invalid (0x%x)", curr_private_format);
        memset(config, 0, sizeof(BufferConfig));
        return -EINVAL;
    }

    // remember real height since height should be aligned with 32 for NV12_BLK
    config->src_height = height;
    config->deinterlace = false;

    switch (input_format)
    {
        case HAL_PIXEL_FORMAT_RGBA_8888:
        case HAL_PIXEL_FORMAT_RGBX_8888:
            config->src_pitch = y_stride * 4;
            config->src_plane = 1;
            config->src_size[0] = config->src_pitch * height;
            config->src_dpformat = DP_COLOR_RGBA8888;
            config->dst_dpformat = DP_COLOR_RGBX8888;
            config->dst_plane = 1;
            break;

        case HAL_PIXEL_FORMAT_BGRA_8888:
        case HAL_PIXEL_FORMAT_BGRX_8888:
        case HAL_PIXEL_FORMAT_IMG1_BGRX_8888:
            config->src_pitch = y_stride * 4;
            config->src_plane = 1;
            config->src_size[0] = config->src_pitch * height;
            config->src_dpformat = DP_COLOR_BGRA8888;
            config->dst_dpformat = DP_COLOR_RGBX8888;
            config->dst_plane    = 1;
            break;

        case HAL_PIXEL_FORMAT_RGB_888:
            config->src_pitch = y_stride * 3;
            config->src_plane = 1;
            config->src_size[0] = config->src_pitch * height;
            config->src_dpformat = DP_COLOR_RGB888;
            config->dst_dpformat = DP_COLOR_RGB888;
            config->dst_plane    = 1;
            break;

        case HAL_PIXEL_FORMAT_RGB_565:
            config->src_pitch = y_stride * 2;
            config->src_plane = 1;
            config->src_size[0] = config->src_pitch * height;
            config->src_dpformat = DP_COLOR_RGB565;
            config->dst_dpformat = DP_COLOR_RGB565;
            config->dst_plane    = 1;
            break;

        case HAL_PIXEL_FORMAT_NV12_BLK_FCM:
            config->src_pitch    = y_stride * 32;
            config->src_pitch_uv = ALIGN_CEIL((y_stride / 2), priv_handle->cbcr_align ? priv_handle->cbcr_align : 16) * 2 * 16;
            config->src_plane = 2;
            config->src_height = vertical_stride;
            src_size_luma = y_stride * vertical_stride;
            config->src_size[0] = src_size_luma;
            config->src_size[1] = src_size_luma / 4 * 2;
            config->src_dpformat = DP_COLOR_420_BLKI;
            config->dst_dpformat = DP_COLOR_YUYV;
            config->dst_plane    = 1;
            break;

        case HAL_PIXEL_FORMAT_NV12_BLK:
            config->src_pitch    = y_stride * 32;
            config->src_pitch_uv = ALIGN_CEIL((y_stride / 2), priv_handle->cbcr_align ? priv_handle->cbcr_align : 16) * 2 * 16;
            config->src_plane = 2;
            config->src_height = vertical_stride;
            src_size_luma = y_stride * vertical_stride;
            config->src_size[0] = src_size_luma;
            config->src_size[1] = src_size_luma / 4 * 2;
            config->src_dpformat = DP_COLOR_420_BLKP;
            config->dst_dpformat = DP_COLOR_YUYV;
            config->dst_plane    = 1;
            break;

        case HAL_PIXEL_FORMAT_I420:
            config->src_pitch    = y_stride;
            config->src_pitch_uv = ALIGN_CEIL((y_stride / 2), priv_handle->cbcr_align ? priv_handle->cbcr_align : 1);
            config->src_plane = 3;
            src_size_luma = y_stride * vertical_stride;
            src_size_chroma = config->src_pitch_uv * vertical_stride / 2;
            config->src_size[0] = src_size_luma;
            config->src_size[1] = src_size_chroma;
            config->src_size[2] = src_size_chroma;
            config->src_dpformat = DP_COLOR_I420;
            config->dst_dpformat = DP_COLOR_YUYV;
            config->dst_plane    = 1;
            break;

        case HAL_PIXEL_FORMAT_I420_DI:
            config->src_pitch    = y_stride * 2;
            config->src_pitch_uv = ALIGN_CEIL(y_stride, priv_handle->cbcr_align ? priv_handle->cbcr_align : 1);
            config->src_plane = 3;
            src_size_luma = y_stride * height;
            src_size_chroma = src_size_luma / 4;
            config->src_size[0] = src_size_luma;
            config->src_size[1] = src_size_chroma;
            config->src_size[2] = src_size_chroma;
            config->src_dpformat = DP_COLOR_I420;
            config->dst_dpformat = DP_COLOR_YUYV;
            config->dst_plane    = 1;
            config->deinterlace = true;
            break;

        case HAL_PIXEL_FORMAT_YV12:
            config->src_pitch    = y_stride;
            config->src_pitch_uv = ALIGN_CEIL((y_stride / 2), priv_handle->cbcr_align ? priv_handle->cbcr_align : 16);
            config->src_plane = 3;
            src_size_luma = y_stride * vertical_stride;
            src_size_chroma = config->src_pitch_uv * (vertical_stride / 2);
            config->src_size[0] = src_size_luma;
            config->src_size[1] = src_size_chroma;
            config->src_size[2] = src_size_chroma;
            config->src_dpformat = DP_COLOR_YV12;
            config->dst_dpformat = DP_COLOR_YUYV;
            config->dst_plane    = 1;
            break;

        case HAL_PIXEL_FORMAT_YV12_DI:
            config->src_pitch    = y_stride * 2;
            config->src_pitch_uv = ALIGN_CEIL(y_stride, priv_handle->cbcr_align ? priv_handle->cbcr_align : 16);
            config->src_plane = 3;
            src_size_luma = y_stride * height;
            src_size_chroma = src_size_luma / 4;
            config->src_size[0] = src_size_luma;
            config->src_size[1] = src_size_chroma;
            config->src_size[2] = src_size_chroma;
            config->src_dpformat = DP_COLOR_YV12;
            config->dst_dpformat = DP_COLOR_YUYV;
            config->dst_plane    = 1;
            config->deinterlace = true;
            break;

        case HAL_PIXEL_FORMAT_YUYV:
        case HAL_PIXEL_FORMAT_YCbCr_422_I:
            config->src_pitch    = y_stride * 2;
            config->src_pitch_uv = 0;
            config->src_plane = 1;
            config->src_size[0] = y_stride * height * 2;
            config->src_dpformat = DP_COLOR_YUYV;
            config->dst_dpformat = DP_COLOR_YUYV;
            config->dst_plane    = 1;
            break;

        case HAL_PIXEL_FORMAT_UFO:
            {
                typedef struct { int w; int h; } UFO_ALIGN;
                const UFO_ALIGN ufo_align_table[] =
                {
                    {64, 64}, {16, 32}, {32, 32}, {64, 64},
                };

                width = ALIGN_CEIL(width, ufo_align_table[curr_ufo_align].w);
                height = ALIGN_CEIL(height, ufo_align_table[curr_ufo_align].h);
                config->src_height   = height;
                config->src_pitch    = width * 32;
                config->src_pitch_uv = width * 16;
                config->src_plane = 2;
                // calculate PIC_SIZE_Y, need align 512
                src_size_luma = ALIGN_CEIL(width * height, 512);
                config->src_size[0] = src_size_luma;
                config->src_size[1] = src_size_luma;
                config->src_dpformat = DP_COLOR_420_BLKP_UFO;
                config->dst_dpformat = DP_COLOR_YUYV;
                config->dst_plane    = 1;
            }
            break;

        default:
            BLOGE(ovl_in, "Color format for DP is invalid (0x%x)", input_format);
            //config->is_valid = false;
            memset(config, 0, sizeof(BufferConfig));
            return -EINVAL;
    }

    // get color range configuration
    config->gralloc_color_range =
        (priv_handle->ext_info.status & GRALLOC_EXTRA_MASK_YUV_COLORSPACE);

    config->is_valid = true;
    return NO_ERROR;
}

status_t BliterHandler::computeBufferCrop(
    InvalidateParam* param, BufferConfig* config, int ovl_in, bool full_screen_update)
{
    if (param->src_base.isEmpty())
    {
        BLOGE(ovl_in, "empty src base:w=%d h=%d",
            param->src_base.getWidth(), param->src_base.getHeight());
        return -EINVAL;
    }

    Rect dst_rect = Rect(config->dst_width, config->dst_height);

    // calculate dst crop
    dst_rect.intersect(param->dst_base, &param->dst_crop);
    if (param->dst_crop.isEmpty())
    {
        BLOGE(ovl_in, "empty dst crop:w=%d h=%d",
            param->dst_crop.getWidth(), param->dst_crop.getHeight());
        return -EINVAL;
    }

    Rect src_crop;
    if (param->dst_base == param->dst_crop)
    {
        // no crop happened, skip
        src_crop = param->src_base;
    }
    else
    {
        // check inverse transform
        Rect in_base(param->dst_base);
        Rect in_crop(param->dst_crop);
        uint32_t inv_transform = param->transform;
        if (Transform::ROT_0 != inv_transform && in_base != in_crop)
        {
            if (Transform::ROT_90 & inv_transform)
                inv_transform ^= (Transform::FLIP_H | Transform::FLIP_V);

            Transform tr(inv_transform);
            in_base = tr.transform(in_base);
            in_crop = tr.transform(in_crop);
        }

        // map dst crop to src crop

        // calculate rectangle ratio between two rectangles
        // horizontally and vertically
        const float ratio_h = param->src_base.getWidth() /
            static_cast<float>(in_base.getWidth());
        const float ratio_v = param->src_base.getHeight() /
            static_cast<float>(in_base.getHeight());

        // get result of the corresponding crop rectangle
        // add 0.5f to round the result to the nearest whole number
        src_crop.left = param->src_base.left + 0.5f +
            (in_crop.left - in_base.left) * ratio_h;
        src_crop.top  = param->src_base.top + 0.5f +
            (in_crop.top - in_base.top) * ratio_v;
        src_crop.right = param->src_base.left + 0.5f +
            (in_crop.right - in_base.left) * ratio_h;
        src_crop.bottom = param->src_base.top + 0.5f +
            (in_crop.bottom - in_base.top) * ratio_v;
    }

    // fill src roi for DpBlitStream
    {
        // [NOTE] width and height for DP should be 2 byte-aligned
        param->src_dp_roi.x = src_crop.left;
        param->src_dp_roi.y = src_crop.top;
        param->src_dp_roi.w = ALIGN_FLOOR(src_crop.getWidth(), 2);
        param->src_dp_roi.h = ALIGN_FLOOR(src_crop.getHeight(), 2);

        if (config->deinterlace) param->src_dp_roi.h /= 2;
    }

    // calculate dst roi for DpBlitStream
    {
        // [NOTE]
        // width and height for DP should be 2 byte-aligned
        // use ALIGN_CEIL() for dst size
        // let MDP to blit from (0, 0) and OVL move to dst position
        // it could avoid wasting bandwidth

        if (full_screen_update)
        {
            // let MDP to blit at proper position and OVL display full screen buffer
            // [NOTE]
            // x should be 2 byte-aligned in YUYV format
            // no strick format in condition beacuse this only for MHL extension mode with RDMA1
            param->dst_dp_roi.x = ALIGN_FLOOR(param->dst_crop.left, 2);
            param->dst_dp_roi.y = param->dst_crop.top;
        }
        else
        {
            // let MDP to blit from (0, 0) and OVL move to dst position
            // it could avoid wasting bandwidth
            param->dst_dp_roi.x = 0;
            param->dst_dp_roi.y = 0;
        }

        // while VP screen rotating, mm layer coverd by screenshot.
        // use ALIGN_FLOOR instead of ALIGN_CEIL to prevent strange line
        // param->dst_dp_roi.w = ALIGN_FLOOR(param->dst_crop.getWidth(), 2);
        // param->dst_dp_roi.h = ALIGN_FLOOR(param->dst_crop.getHeight(), 2);
        // change to ALIGN_CEIL to prevent whilte line in camera introduction film.
        param->dst_dp_roi.w = ALIGN_CEIL(param->dst_crop.getWidth(), 2);
        param->dst_dp_roi.h = ALIGN_CEIL(param->dst_crop.getHeight(), 2);

        // check for OVL limitation
        // if dst region is out of boundary, should adjust it
        if ((param->dst_crop.left + param->dst_dp_roi.w) > (int32_t)m_disp_data->width)
            param->dst_dp_roi.w -= 2;

        if ((param->dst_crop.top + param->dst_dp_roi.h) > (int32_t)m_disp_data->height)
            param->dst_dp_roi.h -= 2;

        if (param->dst_dp_roi.w == 0 || param->dst_dp_roi.h == 0)
        {
            BLOGE(ovl_in, "empty dst roi: w=%d h=%d (dst crop: w=%d h=%d)",
                param->dst_dp_roi.w, param->dst_dp_roi.h,
                param->dst_crop.getWidth(), param->dst_crop.getHeight());
            return -EINVAL;
        }

        if (param->src_dp_roi.w == 0 || param->src_dp_roi.h == 0)
        {
            BLOGE(ovl_in, "empty src roi: w=%d h=%d (src base: w=%d h=%d)",
                param->src_dp_roi.w, param->src_dp_roi.h,
                param->src_base.getWidth(), param->src_base.getHeight());
            return -EINVAL;
        }
    }

    return NO_ERROR;
}

status_t BliterHandler::invalidate(
    InvalidateParam* param, BufferConfig* config, int ovl_in)
{
    HWC_ATRACE_CALL();

    PrivateHandle* src_priv_handle = param->src_priv_handle;
    DpSecure is_dp_secure = DP_SECURE_NONE;

    if (isSecure(src_priv_handle))
    {
        // verify dst secure handle
        if (param->dst_sec_handle == 0)
        {
            BLOGE(ovl_in, "INVALIDATE/sec/no handle");
            return -EINVAL;
        }

        void* src_addr[3];
        src_addr[0] = (void*)(uintptr_t)src_priv_handle->sec_handle;
        src_addr[1] = (void*)(uintptr_t)src_priv_handle->sec_handle;
        src_addr[2] = (void*)(uintptr_t)src_priv_handle->sec_handle;

        BLOGD(ovl_in, "INVALIDATE/sec/handle=%p/sec=0x%x",
            param->src_handle, src_priv_handle->sec_handle);

        m_blit_stream.setSrcBuffer(src_addr, config->src_size, config->src_plane);

        is_dp_secure = DP_SECURE;
    }
    else
    {
        // verify dst ion fd
        if (param->dst_ion_fd < 0)
        {
            BLOGE(ovl_in, "INVALIDATE/no dst ion fd");
            return -EINVAL;
        }

        int src_ion_fd = src_priv_handle->ion_fd;
        m_blit_stream.setSrcBuffer(src_ion_fd, config->src_size, config->src_plane);
    }

    bool is_need_flush = false;
    if (src_priv_handle->usage & GRALLOC_USAGE_SW_WRITE_MASK)
    {
        if (!(src_priv_handle->ext_info.status & GRALLOC_EXTRA_MASK_FLUSH))
        {
            is_need_flush = true;
        }
    }

    DP_PROFILE_ENUM dp_range = DP_PROFILE_BT601;
    if (HWCMediator::getInstance().m_features.global_pq &&
        isUIPq(src_priv_handle))
    {
        dp_range = DP_PROFILE_JPEG;
    }
    else
    {
        dp_range = mapDpColorRange(config->gralloc_color_range);
    }

    // [NOTE] setSrcConfig provides y and uv pitch configuration
    // if uv pitch is 0, DP would calculate it according to y pitch
    m_blit_stream.setSrcConfig(
        config->gralloc_y_stride, config->src_height,
        config->src_pitch, config->src_pitch_uv,
        config->src_dpformat, dp_range,
        eInterlace_None, &param->src_dp_roi,
        is_dp_secure, is_need_flush);

    if (is_dp_secure == DP_SECURE)
    {
        void* dst_addr[3];
        dst_addr[0] = (void*)(uintptr_t)param->dst_sec_handle;
        dst_addr[1] = (void*)(uintptr_t)param->dst_sec_handle;
        dst_addr[2] = (void*)(uintptr_t)param->dst_sec_handle;

        m_blit_stream.setDstBuffer(dst_addr, config->dst_size, config->dst_plane);
    }
    else
    {
        m_blit_stream.setDstBuffer(param->dst_ion_fd, config->dst_size, config->dst_plane);
    }

    // [NOTE] setDstConfig provides y and uv pitch configuration
    // if uv pitch is 0, DP would calculate it according to y pitch
    // ROI designates the dimension and the position of the bitblited image
    m_blit_stream.setDstConfig(
        param->dst_dp_roi.w, param->dst_dp_roi.h,
        config->dst_pitch, config->dst_pitch_uv,
        config->dst_dpformat, dp_range,
        eInterlace_None, &param->dst_dp_roi, is_dp_secure, false);

    m_blit_stream.setOrientation(mapDpOrientation(param->transform));

    // setup for PQ
    DpPqParam dppq_param;
    dppq_param.enable = param->is_enhance;
    dppq_param.scenario = MEDIA_VIDEO;
    if (HWCMediator::getInstance().m_features.global_pq &&
        isUIPq(src_priv_handle))
    {
        dppq_param.u.video.id = UI_PQ_VIDEO_ID;
    }
    else
    {
        dppq_param.u.video.id = src_priv_handle->ext_info.pool_id;
    }

    dppq_param.u.video.timeStamp = src_priv_handle->ext_info.timestamp;
    m_blit_stream.setPQParameter(dppq_param);

    {
        BLOGD(ovl_in, "INVALIDATE/s_flush=%x/range=%d"
            "/s_acq=%d/s_ion=%d/s_fmt=%x"
            "/d_rel=%d/d_ion=%d/d_fmt=%x"
            "/(%d,%d,%d,%d)->(%d,%d,%d,%d)",
            is_need_flush, dp_range,
            param->src_acq_fence_fd, src_priv_handle->ion_fd, config->src_dpformat,
            param->dst_rel_fence_fd, param->dst_ion_fd, config->dst_dpformat,
            param->src_dp_roi.x, param->src_dp_roi.y, param->src_dp_roi.w, param->src_dp_roi.h,
            param->dst_dp_roi.x, param->dst_dp_roi.y, param->dst_dp_roi.w, param->dst_dp_roi.h);

        // wait until src buffer is ready
        m_sync_fence->wait(param->src_acq_fence_fd, 1000, "BLT SRC");

        if (param->is_mirror && (DisplayManager::m_profile_level & PROFILE_BLT))
        {
            char atrace_tag[256];
            sprintf(atrace_tag, "OVL0-MDP");
            HWC_ATRACE_ASYNC_END(atrace_tag, param->sequence);

            sprintf(atrace_tag, "MDP-SMS");
            HWC_ATRACE_ASYNC_BEGIN(atrace_tag, param->sequence);
        }

        // wait until dst buffer is ready
        m_sync_fence->wait(param->dst_rel_fence_fd, 1000, "BLT DST");

        char atrace_tag[256];
        sprintf(atrace_tag, "dp_invalidate:%p", param->src_handle);
        HWC_ATRACE_NAME(atrace_tag);

        DP_STATUS_ENUM status = m_blit_stream.invalidate();
        if (DP_STATUS_RETURN_SUCCESS != status)
        {
            BLOGE(ovl_in, "INVALIDATE/blit fail/err=%d", status);
            return -EINVAL;
        }
    }

    return NO_ERROR;
}

void BliterHandler::process(DispatcherJob* job)
{
    if (HWC_MIRROR_SOURCE_INVALID != job->disp_mir_id)
    {
        if (HWC_DISPLAY_VIRTUAL == job->disp_ori_id)
        {
            processVirMirror(job);
        }
        else
        {
            processPhyMirror(job);
        }
        return;
    }

    uint32_t total_num = job->num_layers;
    // TODO: If skip_ovl, output full screen data to driver now.
    //        Ask RDMA to accept partial update data later.
    bool full_screen_update = ((job->num_layers == 1) & (job->ovl_valid == true));

    for (uint32_t i = 0; i < total_num; i++)
    {
        // MULTIPASS: MM must be composed by one pass
        HWLayer* hw_layer = &job->hw_layers_multi[0][i];

        // skip non mm layers
        if (HWC_LAYER_TYPE_MM != hw_layer->type) continue;

        // this layer is not enable
        if (!hw_layer->enable) continue;

        // TODO: should be removed after intergrating prepare hehavior to DpBlitStream
        int temp_import_fd = hw_layer->priv_handle.ion_fd;

        // this layer is not dirty and there is no latest frame could be used
        if (bypassBlit(hw_layer, i))
        {
            if (temp_import_fd > 0)
            {
                // TODO: should be removed after intergrating prepare hehavior to DpBlitStream
                IONDevice::getInstance().ionCloseAndSet(&temp_import_fd);
            }
            continue;
        }

        hwc_layer_1_t* layer = &hw_layer->layer;
        BufferConfig* config = &m_dp_configs[i];

        // get display buffer queue
        sp<DisplayBufferQueue> queue =
            getDisplayBufferQueue(&hw_layer->priv_handle, config, i);

        // prepare configuration for later usage
        if (NO_ERROR == setDpConfig(&hw_layer->priv_handle, config, i))
        {
            // dequeue display buffer as target
            bool is_secure = isSecure(&hw_layer->priv_handle);
            DisplayBufferQueue::DisplayBuffer disp_buffer;
            status_t err = queue->dequeueBuffer(&disp_buffer, true, is_secure);
            if (NO_ERROR != err)
            {
                BLOGE(i, "Failed to dequeue buffer...");
            }
            else
            {
                InvalidateParam inval_param;
                {
                    inval_param.transform = layer->transform;

                    // [NOTE]
                    // Since OVL does not support float crop, adjust coordinate to interger
                    // as what SurfaceFlinger did with hwc before version 1.2
                    hwc_frect_t* src_cropf = &layer->sourceCropf;
                    int l = (int)(ceilf(src_cropf->left));
                    int t = (int)(ceilf(src_cropf->top));
                    int r = (int)(floorf(src_cropf->right));
                    int b = (int)(floorf(src_cropf->bottom));
                    inval_param.src_base = Rect(l, t, r, b);
                    inval_param.dst_base = Rect(*(Rect *)&(layer->displayFrame));
                }

                // calculate valid src and dst region
                err = computeBufferCrop(&inval_param, config, i, full_screen_update);
                if (NO_ERROR == err)
                {
                    inval_param.src_acq_fence_fd = layer->acquireFenceFd;
                    inval_param.src_handle       = layer->handle;
                    inval_param.src_priv_handle  = &hw_layer->priv_handle;

                    inval_param.dst_rel_fence_fd = disp_buffer.release_fence;
                    inval_param.dst_ion_fd       = disp_buffer.out_ion_fd;
                    inval_param.dst_sec_handle   = disp_buffer.out_sec_handle;

                    // [NOTE] configure the dstination buffer size explicitly
                    // in case DpFramework changes its behavior in the future
                    config->dst_size[0] = config->dst_pitch * inval_param.dst_dp_roi.h;

                    if (full_screen_update)
                    {
                        clearBackground(&disp_buffer, &inval_param);
                    }

                    if (DisplayManager::m_profile_level & PROFILE_BLT)
                    {
                        inval_param.is_mirror = false;
                        inval_param.sequence  = job->sequence;
                    }

                    // enable PQ when feature support, and buffer source type
                    // is video or layer is uipq
                    inval_param.is_enhance =
                        (HWCMediator::getInstance().m_features.enhance) &&
                        ((GRALLOC_EXTRA_BIT_TYPE_VIDEO ==
                            (hw_layer->priv_handle.ext_info.status & GRALLOC_EXTRA_MASK_TYPE)) ||
                            (HWCMediator::getInstance().m_features.global_pq &&
                            isUIPq(&hw_layer->priv_handle)));

                    // trigger bliting
                    err = invalidate(&inval_param, config, i);
                }
                else
                {
                    // if computeBufferCrop has error, still need to wait fence
                    m_sync_fence->wait(layer->acquireFenceFd, SyncFence::TIMEOUT_NEVER, "BLT SRC");
                    m_sync_fence->wait(disp_buffer.release_fence, SyncFence::TIMEOUT_NEVER, "BLT DST");
                }

                if (NO_ERROR == err)
                {
                    // queue display buffer
                    int dst_crop_x = full_screen_update ?
                        0 : inval_param.dst_crop.left;
                    int dst_crop_y = full_screen_update ?
                        0 : inval_param.dst_crop.top;
                    int dst_crop_w = full_screen_update ?
                        m_disp_data->width : inval_param.dst_dp_roi.w;
                    int dst_crop_h = full_screen_update ?
                        m_disp_data->height : inval_param.dst_dp_roi.h;

                    Rect base_crop(Rect(0, 0, dst_crop_w, dst_crop_h));
                    disp_buffer.data_info.src_crop   = base_crop;
                    disp_buffer.data_info.dst_crop   = base_crop.offsetTo(dst_crop_x, dst_crop_y);
                    disp_buffer.data_info.is_sharpen = false;
                    disp_buffer.alpha_enable         = 1;
                    disp_buffer.alpha                = layer->planeAlpha;
                    disp_buffer.blending             = layer->blending;
                    disp_buffer.sequence             = job->sequence;
                    // TODO: fill acquire_fence when DpBlitStream supports fence
                    disp_buffer.acquire_fence        = -1;
                    disp_buffer.src_handle           = layer->handle;

                    disp_buffer.data_color_range     = config->gralloc_color_range;

                    queue->queueBuffer(&disp_buffer);
                }
                else
                {
                    queue->cancelBuffer(disp_buffer.index);
                    err = m_ovl_engine->ignoreInput(i, OverlayEngine::IGNORE_QUEUE);
                    if (NO_ERROR != err)
                    {
                        BLOGW(i, "process ignoreInput failed!");
                    }
                }
            }
        }
        else
        {
            // TODO: after intergrating prepare hehavior to DpBlitStream
            // should notify DpBlitStream to cancel prepare
        }

        // TODO: should be removed after intergrating prepare hehavior to DpBlitStream

        if (temp_import_fd > 0)
        {
            IONDevice::getInstance().ionCloseAndSet(&temp_import_fd);
        }
        // release releaseFence
        releaseFence(layer->releaseFenceFd, hw_layer->sync_marker, i);
    }
}

void BliterHandler::nullop()
{
    unsigned int last_marker = m_sync_fence->getLastMarker();

    releaseFence(0, last_marker, -1);
}

static int transform_table[4][4] =
{
    { 0x00, 0x07, 0x03, 0x04 },
    { 0x04, 0x00, 0x07, 0x03 },
    { 0x03, 0x04, 0x00, 0x07 },
    { 0x07, 0x03, 0x04, 0x00 }
};

void BliterHandler::processPhyMirror(DispatcherJob* job)
{
    BufferConfig* config = &m_dp_configs[0];

    // mirror source buffer
    HWBuffer* hw_mirbuf = &job->hw_mirbuf;

    // mirror output buffer
    HWBuffer* hw_outbuf = &job->hw_outbuf;

    // get display buffer queue
    sp<DisplayBufferQueue> queue =
        getDisplayBufferQueue(&hw_outbuf->priv_handle, config, 0);

    // prepare configuration for later usage
    if (NO_ERROR == setDpConfig(&hw_mirbuf->priv_handle, config, 0))
    {
        status_t err = NO_ERROR;
        DisplayBufferQueue::DisplayBuffer disp_buffer;
        InvalidateParam inval_param;

        // TODO: If skip_ovl, output full screen data to driver now.
        //        Ask RDMA to accept partial update data later.
        bool full_screen_update = (job->num_layers == 1);

        // dequeue display buffer as target
        err = queue->dequeueBuffer(&disp_buffer, true);
        if (NO_ERROR != err)
        {
            BLOGE(0, "Failed to dequeue mirror buffer...");
        }

        if (NO_ERROR == err)
        {
            // set src buffer information
            {
                inval_param.src_acq_fence_fd = hw_mirbuf->mir_in_acq_fence_fd;
                inval_param.src_handle       = hw_mirbuf->handle;
                inval_param.src_priv_handle  = &hw_mirbuf->priv_handle;

                // ROT   0 = 000
                // ROT  90 = 100
                // ROT 180 = 011
                // ROT 270 = 111
                // count num of set bit as index for transform_table
                int ori_rot = job->disp_ori_rot;
                ori_rot = (ori_rot & 0x1) + ((ori_rot>>1) & 0x1) + ((ori_rot>>2) & 0x1);
                int mir_rot = job->disp_mir_rot;
                mir_rot = (mir_rot & 0x1) + ((mir_rot>>1) & 0x1) + ((mir_rot>>2) & 0x1);

                DisplayData* ori_disp_data =
                    &DisplayManager::getInstance().m_data[job->disp_ori_id];

                DisplayData* mir_disp_data =
                    &DisplayManager::getInstance().m_data[job->disp_mir_id];

                // correct ori_disp transform with its hwrotation
                if (0 != ori_disp_data->hwrotation)
                {
                    ori_rot = (ori_rot + ori_disp_data->hwrotation) % 4;
                }

                // correct ori_disp trasform with fo "MTK_LCM_PHYSICAL_ROTATION_HW"
                if (1 == HWCMediator::getInstance().m_features.phy_rotation_180)
                {
                    ori_rot = (ori_rot + 2) % 4;
                }

                // correct mir_disp transform with its hwrotation
                if (0 != mir_disp_data->hwrotation)
                {
                    mir_rot = (mir_rot + mir_disp_data->hwrotation) % 4;
                }

                inval_param.transform = transform_table[ori_rot][mir_rot];

                Rect src_crop;

                int rect_sel = abs(ori_rot - mir_rot);
                if (rect_sel & 0x1)
                {
                    src_crop = mir_disp_data->mir_landscape;
                    inval_param.dst_crop = m_disp_data->mir_landscape;
                }
                else
                {
                    src_crop = mir_disp_data->mir_portrait;
                    inval_param.dst_crop = m_disp_data->mir_portrait;
                }

                inval_param.src_dp_roi.x = src_crop.left;
                inval_param.src_dp_roi.y = src_crop.top;
                inval_param.src_dp_roi.w = ALIGN_FLOOR(src_crop.getWidth(), 2);
                inval_param.src_dp_roi.h = ALIGN_FLOOR(src_crop.getHeight(), 2);
            }

            // set dst buffer information
            {
                PrivateHandle* priv_handle = &hw_outbuf->priv_handle;

                inval_param.dst_rel_fence_fd = disp_buffer.release_fence;
                inval_param.dst_ion_fd       = disp_buffer.out_ion_fd;
                inval_param.dst_sec_handle   = disp_buffer.out_sec_handle;

                if (full_screen_update)
                {
                    // let MDP to blit at proper position and OVL display full screen buffer
                    inval_param.dst_dp_roi.x = inval_param.dst_crop.left;
                    inval_param.dst_dp_roi.y = inval_param.dst_crop.top;
                }
                else
                {
                    // let MDP to blit from (0, 0) and OVL move to dst position
                    // it could avoid wasting bandwidth
                    inval_param.dst_dp_roi.x = 0;
                    inval_param.dst_dp_roi.y = 0;
                }
                inval_param.dst_dp_roi.w = ALIGN_CEIL(inval_param.dst_crop.getWidth(), 2);
                inval_param.dst_dp_roi.h = ALIGN_CEIL(inval_param.dst_crop.getHeight(), 2);

                // adjust dst config here if mirrored to physical display
                switch (priv_handle->format)
                {
                    case HAL_PIXEL_FORMAT_RGB_888:
                        config->dst_dpformat = DP_COLOR_RGB888;
                        config->dst_plane    = 1;
                        config->dst_size[0]  = config->dst_pitch * config->dst_height;
                        config->gralloc_color_range = GRALLOC_EXTRA_BIT_YUV_BT601_FULL;
                        break;

                    case HAL_PIXEL_FORMAT_YUYV:
                        config->dst_dpformat = DP_COLOR_YUYV;
                        config->dst_plane    = 1;
                        config->dst_size[0]  = config->dst_pitch * config->dst_height;
                        config->gralloc_color_range = GRALLOC_EXTRA_BIT_YUV_BT601_FULL;

                        // YUYV dst_roi x must be 2 bytes align
                        inval_param.dst_dp_roi.x = ALIGN_FLOOR(inval_param.dst_dp_roi.x, 2);

                        break;

                    default:
                        BLOGE(0, "Color format for MIRROR is invalid (0x%x)", priv_handle->format);
                        memset(config, 0, sizeof(BufferConfig));
                        err = -EINVAL;
                }
            }
        }

        if (NO_ERROR == err)
        {
            if (full_screen_update)
            {
                clearBackground(&disp_buffer, &inval_param);
            }

            if (DisplayManager::m_profile_level & PROFILE_BLT)
            {
                inval_param.is_mirror = true;
                inval_param.sequence = job->sequence;
            }

            inval_param.is_enhance = false;

            // trigger bliting
            err = invalidate(&inval_param, config, 0);
        }

        if (NO_ERROR == err)
        {
            int dst_crop_x = full_screen_update ?
                0 : inval_param.dst_crop.left;
            int dst_crop_y = full_screen_update ?
                0 : inval_param.dst_crop.top;
            int dst_crop_w = full_screen_update ?
                m_disp_data->width : inval_param.dst_dp_roi.w;
            int dst_crop_h = full_screen_update ?
                m_disp_data->height : inval_param.dst_dp_roi.h;

            Rect base_crop(Rect(0, 0, dst_crop_w, dst_crop_h));
            disp_buffer.data_info.src_crop   = base_crop;
            disp_buffer.data_info.dst_crop   = base_crop.offsetTo(dst_crop_x, dst_crop_y);
            disp_buffer.data_info.is_sharpen = false;
            disp_buffer.alpha_enable         = 1;
            disp_buffer.alpha                = 0xFF;
            disp_buffer.sequence             = job->sequence;
            // TODO: fill acquire_fence when DpBlitStream supports fence
            disp_buffer.acquire_fence        = -1;
            disp_buffer.src_handle           = inval_param.src_handle;

            disp_buffer.data_color_range     = config->gralloc_color_range;

            queue->queueBuffer(&disp_buffer);
        }
        else
        {
            queue->cancelBuffer(disp_buffer.index);
        }
    }
    else
    {
        BLOGE(0, "Failed to get mirror buffer info !!");
        m_ovl_engine->ignoreInput(0);
    }

    // release releaseFence
    releaseFence(hw_mirbuf->mir_in_rel_fence_fd, hw_mirbuf->mir_in_sync_marker, 0);
}

void BliterHandler::processVirMirror(DispatcherJob* job)
{
    BufferConfig* config = &m_dp_configs[0];

    // mirror source buffer
    HWBuffer* hw_mirbuf = &job->hw_mirbuf;

    // prepare configuration for later usage
    if (NO_ERROR == setDpConfig(&hw_mirbuf->priv_handle, config, 0))
    {
        status_t err = NO_ERROR;
        InvalidateParam inval_param;
        Rect src_crop;

        // set src buffer information
        {
            inval_param.src_acq_fence_fd = hw_mirbuf->mir_in_acq_fence_fd;
            inval_param.src_handle       = hw_mirbuf->handle;
            inval_param.src_priv_handle  = &hw_mirbuf->priv_handle;

            // ROT   0 = 000
            // ROT  90 = 100
            // ROT 180 = 011
            // ROT 270 = 111
            // count num of set bit as index for transform_table
            int ori_rot = job->disp_ori_rot;
            ori_rot = (ori_rot & 0x1) + ((ori_rot>>1) & 0x1) + ((ori_rot>>2) & 0x1);
            int mir_rot = job->disp_mir_rot;
            mir_rot = (mir_rot & 0x1) + ((mir_rot>>1) & 0x1) + ((mir_rot>>2) & 0x1);

            DisplayData* ori_disp_data =
                &DisplayManager::getInstance().m_data[job->disp_ori_id];

            DisplayData* mir_disp_data =
                &DisplayManager::getInstance().m_data[job->disp_mir_id];

            // correct ori_disp transform with its hwrotation
            if (0 != ori_disp_data->hwrotation)
            {
                ori_rot = (ori_rot + ori_disp_data->hwrotation) % 4;
            }

            // correct ori_disp trasform with fo "MTK_LCM_PHYSICAL_ROTATION_HW"
            if (1 == HWCMediator::getInstance().m_features.phy_rotation_180)
            {
                ori_rot = (ori_rot + 2) % 4;
            }

            // correct mir_disp transform with its hwrotation
            if (0 != mir_disp_data->hwrotation)
            {
                mir_rot = (mir_rot + mir_disp_data->hwrotation) % 4;
            }

            inval_param.transform = transform_table[ori_rot][mir_rot];

            int rect_sel = abs(ori_rot - mir_rot);
            if (rect_sel & 0x1)
            {
                src_crop = mir_disp_data->mir_landscape;
                inval_param.dst_crop = m_disp_data->mir_landscape;
            }
            else
            {
                src_crop = mir_disp_data->mir_portrait;
                inval_param.dst_crop = m_disp_data->mir_portrait;
            }

            inval_param.src_dp_roi.x = src_crop.left;
            inval_param.src_dp_roi.y = src_crop.top;

            // we need to bliter to virtual outbuf directly
            // so we need MDP to blit to correct position
            inval_param.dst_dp_roi.x = inval_param.dst_crop.left;
            inval_param.dst_dp_roi.y = inval_param.dst_crop.top;
        }

        // set dst buffer information
        {
            // mirror output buffer
            HWBuffer* hw_outbuf = &job->hw_outbuf;
            PrivateHandle* priv_handle = &hw_outbuf->priv_handle;

            inval_param.dst_rel_fence_fd = hw_outbuf->mir_in_rel_fence_fd;
            inval_param.dst_ion_fd       = priv_handle->ion_fd;

            if (isSecure(priv_handle))
            {
                inval_param.dst_sec_handle = priv_handle->sec_handle;
            }

            // adjust dst config here if mirrored to virtual display
            switch (priv_handle->format)
            {
                case HAL_PIXEL_FORMAT_RGBA_8888:
                    inval_param.src_dp_roi.w = src_crop.getWidth();
                    inval_param.src_dp_roi.h = src_crop.getHeight();
                    inval_param.dst_dp_roi.w = inval_param.dst_crop.getWidth();
                    inval_param.dst_dp_roi.h = inval_param.dst_crop.getHeight();

                    config->dst_dpformat = DP_COLOR_RGBA8888;
                    config->dst_width    = priv_handle->width;
                    config->dst_height   = priv_handle->height;
                    config->dst_pitch    = priv_handle->y_stride * 4;
                    config->dst_pitch_uv = 0;
                    config->dst_plane    = 1;
                    config->dst_size[0]  = config->dst_pitch * config->dst_height;
                    break;

                case HAL_PIXEL_FORMAT_YV12:
                    inval_param.src_dp_roi.w = ALIGN_FLOOR(src_crop.getWidth(), 2);
                    inval_param.src_dp_roi.h = ALIGN_FLOOR(src_crop.getHeight(), 2);
                    inval_param.dst_dp_roi.x = ALIGN_FLOOR(inval_param.dst_dp_roi.x, 2);
                    inval_param.dst_dp_roi.y = ALIGN_FLOOR(inval_param.dst_dp_roi.y, 2);
                    inval_param.dst_dp_roi.w = ALIGN_CEIL(inval_param.dst_crop.getWidth(), 2);
                    inval_param.dst_dp_roi.h = ALIGN_CEIL(inval_param.dst_crop.getHeight(), 2);

                    config->dst_dpformat = DP_COLOR_YV12;
                    config->dst_width    = priv_handle->width;
                    config->dst_height   = priv_handle->height;
                    config->dst_pitch    = priv_handle->y_stride;
                    config->dst_pitch_uv = ALIGN_CEIL((priv_handle->y_stride / 2), 16);
                    config->dst_plane    = 3;
                    config->dst_size[0]  = config->dst_pitch * config->dst_height;
                    config->dst_size[1]  = config->dst_pitch_uv * (config->dst_height / 2);
                    config->dst_size[2]  = config->dst_size[1];

                    // WORKAROUND: VENC only accpet BT601 limit range
                    config->gralloc_color_range = GRALLOC_EXTRA_BIT_YUV_BT601_NARROW;
                    break;

                default:
                    BLOGE(0, "Color format for MIRROR is invalid (0x%x)", priv_handle->format);
                    memset(config, 0, sizeof(BufferConfig));
                    err = -EINVAL;
            }
        }

        if (NO_ERROR == err)
        {
            clearBackground(job->hw_outbuf.handle,
                            job->disp_mir_rot,
                            &inval_param.dst_rel_fence_fd);

            if (DisplayManager::m_profile_level & PROFILE_BLT)
            {
                inval_param.is_mirror = true;
                inval_param.sequence = job->sequence;
            }

            inval_param.is_enhance = false;

            // trigger bliting
            err = invalidate(&inval_param, config, 0);
        }
    }
    else
    {
        BLOGE(0, "Failed to get mirror buffer info !!");
        m_ovl_engine->ignoreInput(0);
    }

    // release releaseFence
    releaseFence(hw_mirbuf->mir_in_rel_fence_fd, hw_mirbuf->mir_in_sync_marker, 0);
}

int BliterHandler::dump(char* /*buff*/, int /*buff_len*/, int dump_level)
{
    if (dump_level & DUMP_SYNC)
        m_sync_fence->dump(-1);

    return 0;
}

void BliterHandler::processFillBlack(PrivateHandle* priv_handle, int* fence)
{
    status_t err;
    BufferConfig  config;
    memset(&config, 0, sizeof(BufferConfig));

    AutoMutex l(BlackBuffer::getInstance().m_lock);

    // get BlackBuffer handle
    buffer_handle_t src_handle = BlackBuffer::getInstance().getHandle();
    if (src_handle == 0)
    {
        HWC_LOGE("processFillBlack(BlackBuffer): get handle fail");
        return;
    }

    // check is_sec
    bool is_sec = isSecure(priv_handle);
    if (is_sec)
    {
        BlackBuffer::getInstance().setSecure();
    }

    // get BlackBuffer priv handle
    PrivateHandle src_priv_handle;
    err = getPrivateHandle(src_handle, &src_priv_handle);
    if (NO_ERROR != err)
    {
        HWC_LOGE("processFillBlack(BlackBuffer): get priv handle fail");
        return;
    }

    Rect src_crop(src_priv_handle.width, src_priv_handle.height);
    Rect dst_crop(priv_handle->width, priv_handle->height);

    if (NO_ERROR == setDpConfig(&src_priv_handle, &config, 0))
    {
        InvalidateParam inval_param;
        {
            inval_param.transform       = 0;

            inval_param.src_acq_fence_fd    = -1;
            inval_param.src_handle          = src_handle;
            inval_param.src_priv_handle     = &src_priv_handle;

            inval_param.src_dp_roi.x = 0;
            inval_param.src_dp_roi.y = 0;
            inval_param.src_dp_roi.w = ALIGN_FLOOR(src_crop.getWidth(), 2);
            inval_param.src_dp_roi.h = ALIGN_FLOOR(src_crop.getHeight(), 2);

            inval_param.dst_rel_fence_fd = fence == NULL ? -1 : *fence;
            inval_param.dst_ion_fd       = priv_handle->ion_fd;

            inval_param.dst_crop = dst_crop;
            inval_param.dst_dp_roi.x    = 0;
            inval_param.dst_dp_roi.y    = 0;
            inval_param.dst_dp_roi.w = ALIGN_CEIL(inval_param.dst_crop.getWidth(), 2);
            inval_param.dst_dp_roi.h = ALIGN_CEIL(inval_param.dst_crop.getHeight(), 2);

            if (is_sec)
                inval_param.dst_sec_handle = priv_handle->sec_handle;

            config.dst_width    = priv_handle->width;
            config.dst_height   = priv_handle->height;
            config.dst_pitch    = priv_handle->width * getBitsPerPixel(priv_handle->format) / 8;
            config.dst_pitch_uv = 0;

            switch (priv_handle->format)
            {
                case HAL_PIXEL_FORMAT_RGBA_8888:
                    config.dst_pitch    = priv_handle->y_stride * 4;
                    config.dst_pitch_uv = 0;
                    config.dst_dpformat = DP_COLOR_RGBA8888;
                    config.dst_plane    = 1;
                    config.dst_size[0]  = config.dst_pitch * config.dst_height;
                    break;

                case HAL_PIXEL_FORMAT_YV12:
                    config.dst_pitch    = priv_handle->y_stride;
                    config.dst_pitch_uv = ALIGN_CEIL((priv_handle->y_stride / 2), 16);
                    config.dst_dpformat = DP_COLOR_YV12;
                    config.dst_plane    = 3;
                    config.dst_size[0]  = config.dst_pitch * config.dst_height;
                    config.dst_size[1]  = config.dst_pitch_uv * (config.dst_height / 2);
                    config.dst_size[2]  = config.dst_size[1];

                    // WORKAROUND: VENC only accpet BT601 limit range
                    config.gralloc_color_range = GRALLOC_EXTRA_BIT_YUV_BT601_NARROW;
                    break;

                case HAL_PIXEL_FORMAT_RGB_888:
                    config.dst_dpformat = DP_COLOR_RGB888;
                    config.dst_plane    = 1;
                    config.dst_size[0]  = config.dst_pitch * config.dst_height;
                    config.gralloc_color_range = GRALLOC_EXTRA_BIT_YUV_BT601_FULL;
                    break;

                case HAL_PIXEL_FORMAT_YUYV:
                    config.dst_dpformat = DP_COLOR_YUYV;
                    config.dst_plane    = 1;
                    config.dst_size[0]  = config.dst_pitch * config.dst_height;
                    config.gralloc_color_range = GRALLOC_EXTRA_BIT_YUV_BT601_FULL;
                    break;

                default:
                    config.dst_size[0]  = config.dst_pitch * inval_param.dst_dp_roi.h;
                    HWC_LOGW("processFillBlack format(0x%x) unexpected", priv_handle->format);
            }
        }

        if (NO_ERROR == err)
        {
            inval_param.is_enhance = false;
            err = invalidate(&inval_param, &config, 0);
            if (fence != NULL)
               *fence = -1;
        }
    }
    else
    {
        HWC_LOGE("processFillBlack setDpConfig Fail");
    }

    if (is_sec)
    {
        BlackBuffer::getInstance().setNormal();
    }
}

void BliterHandler::fillBlack(buffer_handle_t handle, PrivateHandle* priv_handle, int* fence)
{
    int err;
    bool is_sec = isSecure(priv_handle);

    // clear normal buf
    if (is_sec)
    {
        setSecExtraSfStatus(false, handle);
        err = getPrivateHandle(handle, priv_handle);
        if (0 != err)
        {
            HWC_LOGE("fillBlack - Failed to get priv handle - normal(%p)", handle);
            return;
        }
    }
    processFillBlack(priv_handle, fence);

    // clear sec buf if exists
    unsigned int tmp_sec_handle;
    err = gralloc_extra_query(handle, GRALLOC_EXTRA_GET_SECURE_HANDLE_HWC, &tmp_sec_handle);
    if ((GRALLOC_EXTRA_OK != err) || (tmp_sec_handle == 0))
        return;

    setSecExtraSfStatus(true, handle);
    err = getPrivateHandle(handle, priv_handle);
    if (0 != err)
    {
        HWC_LOGE("fillBlack - Failed to get priv handle - secure(%p)", handle);
        return;
    }
    processFillBlack(priv_handle, fence);

    // set back
    if (!is_sec)
        setSecExtraSfStatus(false, handle);
}

void BliterHandler::clearBackground(buffer_handle_t handle, int curr_orient, int* fence)
{
    PrivateHandle priv_handle;

    int err = getPrivateHandle(handle, &priv_handle);
    if (0 != err)
    {
        HWC_LOGE("Failed to get handle(%p)", handle);
        return;
    }

    gralloc_extra_ion_sf_info_t* ext_info = &priv_handle.ext_info;
    int prev_orient = (ext_info->status & GRALLOC_EXTRA_MASK_ORIENT) >> 12;
    HWC_LOGV("clearBackGround: prev_orient(%x), curr_orient(%x), ext_info->status(%x)",
        prev_orient, curr_orient, ext_info->status);

    // INIT    = 0xxxb
    // ROT   0 = 1000b
    // ROT  90 = 1100b
    // ROT 180 = 1011b
    // ROT 270 = 1111b
    // USED    = 1xxxb
    if ((((prev_orient >> 3) & 0x01) == 0) ||
        (((prev_orient >> 2) & 0x01) != ((curr_orient >> 2) & 0x01)))
    {

        fillBlack(handle, &priv_handle, fence);

        gralloc_extra_sf_set_status(
            ext_info, GRALLOC_EXTRA_MASK_ORIENT, (curr_orient << 12) | (0x01 << 15));

        HWC_LOGD("clearBufferBlack ext_info->status(%x)",ext_info->status);

        gralloc_extra_perform(
            handle, GRALLOC_EXTRA_SET_IOCTL_ION_SF_INFO, ext_info);
    }
}

void BliterHandler::clearBackground(DisplayBufferQueue::DisplayBuffer* disp_buffer, InvalidateParam* inval_param)
{
    buffer_handle_t handle = disp_buffer->out_handle;
    Rect prev_roi = disp_buffer->inval_roi;
    Rect curr_roi(inval_param->dst_dp_roi.w, inval_param->dst_dp_roi.h);
    curr_roi = curr_roi.offsetTo(inval_param->dst_dp_roi.x, inval_param->dst_dp_roi.y);

    if ((prev_roi.left   < curr_roi.left) ||
        (prev_roi.top    < curr_roi.top) ||
        (prev_roi.right  > curr_roi.right) ||
        (prev_roi.bottom > curr_roi.bottom))
    {
        HWC_LOGD("clearBufferBlack");

        PrivateHandle priv_handle;

        int err = getPrivateHandle(handle, &priv_handle);
        if (0 != err)
        {
            HWC_LOGE("Failed to get handle(%p)", handle);
            return;
        }

        fillBlack(handle, &priv_handle, NULL);
    }
    disp_buffer->inval_roi = curr_roi;
}

