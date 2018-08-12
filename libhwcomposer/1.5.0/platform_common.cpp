#define DEBUG_LOG_TAG "PLTC"

#include <DpBlitStream.h>

#include "utils/tools.h"
#include "platform_common.h"
#include "utils/transform.h"
extern unsigned int mapDpOrientation(const uint32_t transform);

void PlatformCommon::initOverlay()
{
}

bool PlatformCommon::isUILayerValid(
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

    if (!DispDevice::getInstance().isDispRszSupported() ||
        (0 == HWCMediator::getInstance().m_features.resolution_switch))
    {
        // cannot handle scaling
        if (WIDTH(layer->displayFrame) != w || HEIGHT(layer->displayFrame) != h)
            return false;
    }
    return true;
}

bool PlatformCommon::isMMLayerValid(
    int dpy, struct hwc_layer_1* layer, PrivateHandle* priv_handle, bool& /*is_high*/)
{
    // only use MM layer without any blending consumption
    if (layer->blending == HWC_BLENDING_COVERAGE)
        return false;

    const int srcWidth = getSrcWidth(layer);
    const int srcHeight = getSrcHeight(layer);

    // check src rect is not empty
    if (srcWidth <= 1 || srcHeight <= 1)
        return false;

    const int dstWidth = WIDTH(layer->displayFrame);
    const int dstHeight = HEIGHT(layer->displayFrame);

    // constraint to prevent bliter making error.
    // bliter would crop w or h to 0
    if (dstWidth <= 1 ||  dstHeight <= 1)
        return false;

    const int srcLeft = getSrcLeft(layer);
    const int srcTop = getSrcTop(layer);

    // cannot handle source negative offset
    if (srcLeft < 0 || srcTop < 0)
        return false;

    int buffer_type = (priv_handle->ext_info.status & GRALLOC_EXTRA_MASK_TYPE);

    if (!(buffer_type == GRALLOC_EXTRA_BIT_TYPE_VIDEO ||
        buffer_type == GRALLOC_EXTRA_BIT_TYPE_CAMERA ||
        priv_handle->format == HAL_PIXEL_FORMAT_YV12))
    {
        // Because limit is 9, but HWC will change size to align 2. so use 11 to be determine rule
        if (srcWidth < 11 || srcHeight < 11 || dstWidth < 11 || dstHeight < 11)
        {
            HWC_LOGD("layers with scaling cannot be processed by HWC because of src(w,h,x,y)=(%d,%d,%d,%d) dst(w,h)=(%d,%d)",
                srcWidth, srcHeight, srcLeft, srcTop, dstWidth, dstHeight);
            return false;
        }

    }

    if (priv_handle->format == HAL_PIXEL_FORMAT_RGBA_8888 ||
        priv_handle->format == HAL_PIXEL_FORMAT_BGRA_8888 ||
        priv_handle->format == HAL_PIXEL_FORMAT_RGBX_8888 ||
        priv_handle->format == HAL_PIXEL_FORMAT_BGRX_8888 ||
        priv_handle->format == HAL_PIXEL_FORMAT_RGB_888 ||
        priv_handle->format == HAL_PIXEL_FORMAT_RGB_565)
    {
        if (layer->transform != Transform::ROT_0)
        {
            // cannot handle scaling when color format is RGBA
            // Because of RDMA limitation, width of RGBA input layers must be more than eight.
            if (dstWidth != srcHeight || dstHeight != srcWidth || srcLeft != 0 || srcTop != 0 || srcWidth < 9)
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

    // Because MDP can not process odd width and height, we will calculate the
    // crop area and roi later. Then we may adjust the size of source and
    // destination buffer. This behavior may cause that the scaling rate
    // increases, and therefore the scaling rate is over the limitation of MDP.
    const bool is_blit_valid =
        DpBlitStream::queryHWSupport(
            srcWidth, srcHeight, dstWidth, dstHeight, mapDpOrientation(layer->transform)) &&
        DpBlitStream::queryHWSupport(
            srcWidth - 1, srcHeight - 1, dstWidth + 2, dstHeight + 2, mapDpOrientation(layer->transform));
    if (!is_blit_valid)
        return false;

    const int secure = (priv_handle->usage & (GRALLOC_USAGE_PROTECTED | GRALLOC_USAGE_SECURE));

    const bool is_disp_valid = (dpy == HWC_DISPLAY_PRIMARY) || secure ||
                         (!(dstWidth & 0x01) && !(dstHeight & 0x01));

    return is_disp_valid;
}

size_t PlatformCommon::getLimitedVideoSize()
{
    // 4k resolution
    return 3840 * 2160;
}

size_t PlatformCommon::getLimitedExternalDisplaySize()
{
    // 2k resolution
    return 2048 * 1080;
}

PlatformCommon::PlatformConfig::PlatformConfig()
    : platform(PLATFORM_NOT_DEFINE)
    , compose_level(COMPOSE_DISABLE_ALL)
    , mirror_state(MIRROR_DISABLED)
    , overlay_cap(OVL_CAP_UNDEFINE)
    , bq_count(3)
    , mir_scale_ratio(0.0f)
    , format_mir_mhl(MIR_FORMAT_UNDEFINE)
    , ovl_overlap_limit(0)
    , prexformUI(1)
    , rdma_roi_update(0)
    , force_full_invalidate(false)
    , use_async_bliter(false)
    , use_async_bliter_ultra(false)
    , wait_fence_for_display(false)
    , enable_smart_layer(false)
    , enable_rgba_rotate(false)
    , bypass_hdcp_checking(false)
{ }
