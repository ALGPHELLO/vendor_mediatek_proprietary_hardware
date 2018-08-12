#define DEBUG_LOG_TAG "COMP"

#include "hwc_priv.h"

#include "gralloc_mtk_defs.h"
#include <hardware/gralloc.h>

#include "utils/debug.h"
#include "utils/tools.h"

#include "composer.h"
#include "display.h"
#include "overlay.h"
#include "dispatcher.h"
#include "worker.h"
#include "sync.h"
#include "hwdev.h"
#include "platform.h"

#include <sync/sync.h>

#include <ui/GraphicBuffer.h>

#define CLOGV(i, x, ...) HWC_LOGV("(%d:%d) " x, m_disp_id, i, ##__VA_ARGS__)
#define CLOGD(i, x, ...) HWC_LOGD("(%d:%d) " x, m_disp_id, i, ##__VA_ARGS__)
#define CLOGI(i, x, ...) HWC_LOGI("(%d:%d) " x, m_disp_id, i, ##__VA_ARGS__)
#define CLOGW(i, x, ...) HWC_LOGW("(%d:%d) " x, m_disp_id, i, ##__VA_ARGS__)
#define CLOGE(i, x, ...) HWC_LOGE("(%d:%d) " x, m_disp_id, i, ##__VA_ARGS__)

// ---------------------------------------------------------------------------

inline void computeBufferCrop(
    Rect& src_crop, Rect& dst_crop,
    int disp_width, int disp_height)
{
    if (dst_crop.right > disp_width)
    {
        int diff = dst_crop.right - disp_width;
        dst_crop.right = disp_width;
        src_crop.right -= diff;
    }

    if (dst_crop.bottom > disp_height)
    {
        int diff = dst_crop.bottom - disp_height;
        dst_crop.bottom = disp_height;
        src_crop.bottom -= diff;
    }

    if (dst_crop.left >= 0 && dst_crop.top >=0)
        return;

    if (dst_crop.left < 0)
    {
        src_crop.left -= dst_crop.left;
        dst_crop.left = 0;
    }

    if (dst_crop.top < 0)
    {
        src_crop.top -= dst_crop.top;
        dst_crop.top = 0;
    }
}

// ---------------------------------------------------------------------------

LayerHandler::LayerHandler(int dpy, const sp<OverlayEngine>& ovl_engine)
    : m_disp_id(dpy)
    , m_ovl_engine(ovl_engine)
    , m_sync_fence(new SyncFence(dpy))
{
    m_disp_data = &DisplayManager::getInstance().m_data[dpy];
}

LayerHandler::~LayerHandler()
{
    m_ovl_engine = NULL;
    m_sync_fence = NULL;
}

// ---------------------------------------------------------------------------

ComposerHandler::ComposerHandler(int dpy, const sp<OverlayEngine>& ovl_engine)
    : LayerHandler(dpy, ovl_engine)
{ }

void ComposerHandler::set(
    struct hwc_display_contents_1* list,
    DispatcherJob* job)
{
    uint32_t total_num = job->num_layers;

    for (uint32_t i = 0; i < total_num; i++)
    {
        HWLayer* hw_layer = &job->hw_layers[i];

        // this layer is not enable
        if (!hw_layer->enable) continue;

        // skip mm layers
        if (HWC_LAYER_TYPE_MM == hw_layer->type) continue;

        hwc_layer_1_t* layer = &list->hwLayers[hw_layer->index];
        PrivateHandle* priv_handle = &hw_layer->priv_handle;

        if (HWC_LAYER_TYPE_DIM == hw_layer->type)
        {
            memcpy(&hw_layer->layer, layer, sizeof(hwc_layer_1_t));
            priv_handle->format = HAL_PIXEL_FORMAT_RGB_888;

            CLOGV(i, "SET/dim");
            continue;
        }

        // get private handle information
        status_t err = NO_ERROR;
        if (HWC_LAYER_TYPE_FBT != hw_layer->type)
        {
            err = getPrivateHandleBuff(layer->handle, priv_handle);
        }
        else if (HWC_DISPLAY_VIRTUAL <= m_disp_id)
        {
            priv_handle->ion_fd = DISP_NO_ION_FD;
            err = getPrivateHandleFBT(layer->handle, priv_handle);
        }

        if (err != NO_ERROR)
        {
            hw_layer->enable = false;
            continue;
        }

        int type = (priv_handle->ext_info.status & GRALLOC_EXTRA_MASK_TYPE);
        int is_need_flush = (type != GRALLOC_EXTRA_BIT_TYPE_GPU);

        // get release fence from display driver
        {
            OverlayPrepareParam prepare_param;
            prepare_param.id            = i;
            prepare_param.ion_fd        = priv_handle->ion_fd;
            prepare_param.is_need_flush = is_need_flush;

            status_t err = m_ovl_engine->prepareInput(prepare_param);
            if (NO_ERROR != err)
            {
                prepare_param.fence_index = 0;
                prepare_param.fence_fd = -1;
            }
            hw_layer->fence_index = prepare_param.fence_index;

            if (prepare_param.fence_fd <= 0)
            {
                CLOGE(i, "Failed to get releaseFence !!");
            }
            layer->releaseFenceFd = prepare_param.fence_fd;
        }
        memcpy(&hw_layer->layer, layer, sizeof(hwc_layer_1_t));

        CLOGV(i, "SET/rel=%d(%d)/acq=%d/handle=%p/ion=%d/flush=%d",
            layer->releaseFenceFd, hw_layer->fence_index, layer->acquireFenceFd,
            layer->handle, priv_handle->ion_fd, is_need_flush);

        layer->acquireFenceFd = -1;
    }
}

void ComposerHandler::process(DispatcherJob* job)
{
#ifndef MTK_USER_BUILD
    HWC_ATRACE_CALL();
#endif

    uint32_t total_num = job->num_layers;
    uint32_t i = 0;

    OverlayPortParam* const* ovl_params = m_ovl_engine->getInputParams();

    // wait until each layer is ready, then set to overlay
    for (i = 0; i < total_num; i++)
    {
        HWLayer* hw_layer = &job->hw_layers[i];

        // this layer is not enable
        if (!hw_layer->enable) continue;

        // skip mm layers
        if (HWC_LAYER_TYPE_MM == hw_layer->type) continue;

        hwc_layer_1_t* layer = &hw_layer->layer;
        m_sync_fence->wait(layer->acquireFenceFd, 1000, DEBUG_LOG_TAG);

        m_ovl_engine->setInputDirect(i);
    }

    // fill overlay engine setting
    int param_count = 0;
    for (i = 0; i < total_num; i++)
    {
        HWLayer* hw_layer = &job->hw_layers[i];

        // this layer is not enable
        if (!hw_layer->enable)
        {
            ovl_params[i]->state = OVL_IN_PARAM_DISABLE;
            ovl_params[i]->sequence = HWC_SEQUENCE_INVALID;
            continue;
        }

        // skip mm layers
        if (HWC_LAYER_TYPE_MM == hw_layer->type) continue;

        hwc_layer_1_t* layer = &hw_layer->layer;
        PrivateHandle* priv_handle = &hw_layer->priv_handle;

        int l, t, r, b;
        if (HWC_LAYER_TYPE_DIM != hw_layer->type)
        {
            // [NOTE]
            // Since OVL does not support float crop, adjust coordinate to interger
            // as what SurfaceFlinger did with hwc before version 1.2
            hwc_frect_t* src_cropf = &layer->sourceCropf;
            l = (int)(ceilf(src_cropf->left));
            t = (int)(ceilf(src_cropf->top));
            r = (int)(floorf(src_cropf->right));
            b = (int)(floorf(src_cropf->bottom));
        }
        else
        {
            l = layer->displayFrame.left;
            t = layer->displayFrame.top;
            r = layer->displayFrame.right;
            b = layer->displayFrame.bottom;
        }
        Rect src_crop(l, t, r, b);
        Rect dst_crop(*(Rect *)&(layer->displayFrame));
        computeBufferCrop(src_crop, dst_crop, m_disp_data->width, m_disp_data->height);

        OverlayPortParam* param = ovl_params[i];

        if (HWC_LAYER_TYPE_FBT == hw_layer->type && false == hw_layer->enable)
            param->state = OVL_IN_PARAM_IGNORE;
        else
            param->state = OVL_IN_PARAM_ENABLE;

        param->mva          = (void*)priv_handle->fb_mva;
        param->pitch        = priv_handle->y_stride;
        param->format       = priv_handle->format;
        param->src_crop     = src_crop;
        param->dst_crop     = dst_crop;
        param->is_sharpen   = false;
        param->fence_index  = hw_layer->fence_index;
        // use hw layer type as identity
        param->identity     = hw_layer->type;
        param->protect      = (priv_handle->usage & GRALLOC_USAGE_PROTECTED);
        param->secure       = (priv_handle->usage & GRALLOC_USAGE_SECURE);
        param->alpha_enable = (layer->blending != HWC_BLENDING_NONE);
        param->alpha        = layer->planeAlpha;
        param->blending     = layer->blending;
        param->dim          = (hw_layer->type == HWC_LAYER_TYPE_DIM);
        param->sequence     = job->sequence;
#ifdef MTK_HWC_PROFILING
        if (HWC_LAYER_TYPE_FBT == hw_layer->type)
        {
            param->fbt_input_layers = hw_layer->fbt_input_layers;
            param->fbt_input_bytes  = hw_layer->fbt_input_bytes;
        }
#endif
        param->ion_fd      = priv_handle->ion_fd;
    }
}
