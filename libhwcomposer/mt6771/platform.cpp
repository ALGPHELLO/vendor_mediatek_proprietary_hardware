#define DEBUG_LOG_TAG "PLAT"

#include "gralloc_mtk_defs.h"

#include <hardware/hwcomposer.h>
#include <hardware/gralloc.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "utils/debug.h"
#include "utils/tools.h"
#include "utils/transform.h"

#include "hwdev.h"
#include "platform.h"

#include "m4u_lib.h"
#include "DpBlitStream.h"

#include <ui/GraphicBuffer.h>
#include <ui/GraphicBufferMapper.h>
#include "ui/gralloc_extra.h"

#ifdef USE_HWC2
#include "hwc2.h"
#endif
extern unsigned int mapDpOrientation(const uint32_t transform);

// ---------------------------------------------------------------------------

ANDROID_SINGLETON_STATIC_INSTANCE(Platform);

Platform::Platform()
{
    m_config.platform = PLATFORM_MT6771;

    m_config.compose_level = COMPOSE_ENABLE_ALL;

    m_config.mirror_state = MIRROR_ENABLED;

    m_config.overlay_cap = (OVL_CAP_DIM | OVL_CAP_DIM_HW | OVL_CAP_P_FENCE);

    m_config.ovl_overlap_limit = 6;

    m_config.bq_count = 4;

    m_config.rdma_roi_update = 1;

    m_config.use_async_bliter = true;

    m_config.use_async_bliter_ultra = true;

    m_config.enable_smart_layer = true;

    m_config.enable_rgba_rotate = true;

    m_config.support_color_transform = true;

    m_config.mdp_scale_percentage = 0.5f;

    m_config.extend_mdp_capacity = true;

    m_config.rpo_ui_max_src_width = 730;

    m_config.pass_dim_to_hrt = true;

    m_config.av_grouping = false;
}

bool Platform::isUILayerValid(
    int /*dpy*/, struct hwc_layer_1* layer, PrivateHandle* priv_handle)
{
    if (priv_handle->format != HAL_PIXEL_FORMAT_YUYV &&
        priv_handle->format != HAL_PIXEL_FORMAT_YCbCr_422_I &&
        priv_handle->format != HAL_PIXEL_FORMAT_IMG1_BGRX_8888 &&
        (priv_handle->format < HAL_PIXEL_FORMAT_RGBA_8888 ||
         priv_handle->format > HAL_PIXEL_FORMAT_BGRA_8888))
        return false;

    // hw does not support HWC_BLENDING_COVERAGE
    if (layer->blending == HWC_BLENDING_COVERAGE)
        return false;

#ifdef MTK_BASIC_PACKAGE
    // [NOTE]
    // For the case of RGBA layer with constant alpha != 0xFF and premult blending,
    // ovl would process wrong formula if opaque info is not passing to hwc.
    // In bsp/tk load, opaque info can be passed by SF
    // Therefore, don't set  if the opaque flags is not passing to hwc in basic load
    if (layer->planeAlpha != 0xFF &&
        (priv_handle->format == HAL_PIXEL_FORMAT_RGBA_8888 || priv_handle->format == HAL_PIXEL_FORMAT_BGRA_8888) &&
        layer->blending == HWC_BLENDING_PREMULT)
        return false;
#endif

    // opaqaue layer should ignore alpha channel
    if (layer->blending == HWC_BLENDING_NONE || layer->flags & HWC_IS_OPAQUE)
    {
        switch (priv_handle->format)
        {
            case HAL_PIXEL_FORMAT_RGBA_8888:
                priv_handle->format = HAL_PIXEL_FORMAT_RGBX_8888;
                break;

            case HAL_PIXEL_FORMAT_BGRA_8888:
                return false;

            default:
                break;
        }
    }

    int w = getSrcWidth(layer);
    int h = getSrcHeight(layer);

    // ovl cannot accept <=0
    if (w <= 0 || h <= 0)
        return false;

    // [NOTE]
    // Since OVL does not support float crop, adjust coordinate to interger
    // as what SurfaceFlinger did with hwc before version 1.2
    int src_left = getSrcLeft(layer);
    int src_top = getSrcTop(layer);

    // cannot handle source negative offset
    if (src_left < 0 || src_top < 0)
        return false;

    // switch width and height for prexform with ROT_90
    if (0 != priv_handle->prexform)
    {
        DbgLogger logger(DbgLogger::TYPE_DUMPSYS, 'I',
            "prexformUI:%d x:%d, prex:%d, f:%d/%d, s:%d/%d",
            m_config.prexformUI, layer->transform, priv_handle->prexform,
            WIDTH(layer->displayFrame), HEIGHT(layer->displayFrame), w, h);

        if (0 == m_config.prexformUI)
            return false;

        if (0 != (priv_handle->prexform & HAL_TRANSFORM_ROT_90))
            SWAP(w, h);
    }

    // cannot handle rotation
    if (layer->transform != priv_handle->prexform)
        return false;

    // cannot handle scaling
    if (WIDTH(layer->displayFrame) != w || HEIGHT(layer->displayFrame) != h)
        return false;

    return true;
}

bool Platform::isMMLayerValid(
    int dpy, struct hwc_layer_1* layer, PrivateHandle* priv_handle, bool& /*is_high*/)
{
    // only use MM layer without any blending consumption
    if (layer->blending == HWC_BLENDING_COVERAGE)
        return false;

    int srcWidth = getSrcWidth(layer);
    int srcHeight = getSrcHeight(layer);

    // check src rect is not empty
    if (srcWidth <= 1 || srcHeight <= 1)
        return false;

    int dstWidth = WIDTH(layer->displayFrame);
    int dstHeight = HEIGHT(layer->displayFrame);

    // constraint to prevent bliter making error.
    // bliter would crop w or h to 0
    if (dstWidth <= 1 ||  dstHeight <= 1)
        return false;

    int srcLeft = getSrcLeft(layer);
    int srcTop = getSrcTop(layer);

    // cannot handle source negative offset
    if (srcLeft < 0 || srcTop < 0)
        return false;
    if ((priv_handle->format == HAL_PIXEL_FORMAT_RGBA_8888 ||
        priv_handle->format == HAL_PIXEL_FORMAT_BGRA_8888))
    {
        if (layer->transform != priv_handle->prexform)
        {
            // cannot handle scaling when color format is RGBA
            // Because of RDMA limitation, width of RGBA input layers must be more than eight.
            if ((dstWidth != srcHeight || dstHeight != srcWidth || srcLeft != 0 || srcTop != 0)
                && srcWidth < 9)
            {
                HWC_LOGD("RGBA layers with scaling cannot be processed by HWC because of src(w,h,x,y)=(%d,%d,%d,%d) dst(w,h)=(%d,%d)",
                        srcWidth, srcHeight, srcLeft, srcTop, dstWidth, dstHeight);
                return false;
            }
        }
        else
        {
            // RGBA only support rotate
            return false;
        }
    }

    bool is_blit_valid = DpBlitStream::queryHWSupport(
                            srcWidth, srcHeight,
                            dstWidth, dstHeight, mapDpOrientation(layer->transform));

    if (!is_blit_valid)
        return false;

    int secure = (priv_handle->usage & (GRALLOC_USAGE_PROTECTED | GRALLOC_USAGE_SECURE));

    bool is_disp_valid = (dpy == HWC_DISPLAY_PRIMARY) || secure ||
                         (!(dstWidth & 0x01) && !(dstHeight & 0x01));

    return is_disp_valid;
}

size_t Platform::getLimitedExternalDisplaySize()
{
    // 4k resolution
    return 3840 * 2160;
}


#ifdef USE_HWC2

bool Platform::isUILayerValid(const sp<HWCLayer>& layer, int32_t* line)
{
    return PlatformCommon::isUILayerValid(layer, line);
}


bool Platform::isMMLayerValid(const sp<HWCLayer>& layer, int32_t* line)
{
    return PlatformCommon::isMMLayerValid(layer, line);
}

#endif // USE_HWC2
