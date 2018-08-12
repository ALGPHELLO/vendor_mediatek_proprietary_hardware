#define DEBUG_LOG_TAG "CCH"

#include <cutils/properties.h>
#include "ui/gralloc_extra.h"
#include "gralloc_mtk_defs.h"

#include "cache.h"
#include "hwc.h"
#include "utils/debug.h"
#include "utils/tools.h"

ANDROID_SINGLETON_STATIC_INSTANCE(CacheBase);

CacheBase::CacheBase()
    : m_glesLayer_head(-1)
    , m_glesLayer_tail(-1)
{
    m_current_hitlayer_info.gles_layer_count = 0;
    m_current_hitlayer_info.current_layer_vector.clear();
}

CacheBase::~CacheBase()
{
}

bool CacheBase::layerHit(hwc_display_contents_1* list, int gles_head, int gles_tail)
{
    // to check whether the platform supports cache algorithm or not
    if (!(HWCMediator::getInstance().m_features.cache_caps & HWC_FEATURES_CACHE_CAPS_GPU_PASSIVE) ||
        !(HWCMediator::getInstance().m_features.cache_mode & HWC_FEATURES_CACHE_MODE_GPU_PASSIVE))
    {
        m_current_hitlayer_info.gles_layer_count = 0;
        m_current_hitlayer_info.current_layer_vector.clear();
        return false;
    }

    Vector<HitCompare>  saved_hitlayer_info;
    m_glesLayer_head = -1;
    m_glesLayer_tail = -1;
    m_glesLayer_head = gles_head;
    m_glesLayer_tail = gles_tail;
    int hit_status = -1;
    bool is_hit = false;
    int hit_count = 0;
    int saved_gles_layer_count = 0;
    int prev_hit_count = m_current_hitlayer_info.current_layer_vector.size();
    int layers_num = list->numHwLayers - 1;

    if (m_glesLayer_head == -1)
    {
        // no GLES composed layer
        prev_hit_count = hit_count = m_current_hitlayer_info.gles_layer_count = 0;
        saved_hitlayer_info.clear();
        m_current_hitlayer_info.current_layer_vector.clear();

        return is_hit;
    }
    else
    {
        saved_gles_layer_count = m_glesLayer_tail - m_glesLayer_head + 1 ;
    }

    // if any opaqaue layer cover on cache, don't use cache.
    if ((m_glesLayer_tail+1) < layers_num)
    {
        bool opaqaue_layer_exist = false;
        for (int i = m_glesLayer_tail + 1; i < layers_num ; i++)
        {
            hwc_layer_1_t* layer = &list->hwLayers[i];

            if (layer->blending == HWC_BLENDING_NONE)
            {
                opaqaue_layer_exist = true;
                break;
            }
        }

        if (opaqaue_layer_exist)
        {
            prev_hit_count = hit_count = m_current_hitlayer_info.gles_layer_count = 0;
            saved_hitlayer_info.clear();
            m_current_hitlayer_info.current_layer_vector.clear();
            return false;
        }
    }

    // check the layers are composed by GPU is the same as before
    for (int i = m_glesLayer_head; i <= m_glesLayer_tail; ++i)
    {
        hwc_layer_1_t* layer = &list->hwLayers[i];
        buffer_handle_t current_handle = layer->handle;

        // can't handle layer rotate, it should be handle by GPU
        // if any rotate layer exist, skip FBT cache flow
        if (layer->flags & HWC_SKIP_LAYER || current_handle == NULL)
        {
            prev_hit_count = 0;
            hit_count = 0;
            m_current_hitlayer_info.gles_layer_count = 0;
            saved_hitlayer_info.clear();
            m_current_hitlayer_info.current_layer_vector.clear();
            break;
        }

        PrivateHandle priv_handle;
        getPrivateHandleInfo(layer->handle, &priv_handle);
        gralloc_extra_ion_sf_info_t current_ext_info = priv_handle.ext_info;


        // to store GLES layer info first, this info need to use in next frame
        HitCompare compare;
        {
            const int src_crop_x = getSrcLeft(layer);
            const int src_crop_y = getSrcTop(layer);
            const int src_crop_w = getSrcWidth(layer);
            const int src_crop_h = getSrcHeight(layer);
            const int dst_crop_x = layer->displayFrame.left;
            const int dst_crop_y = layer->displayFrame.top;
            const int dst_crop_w = WIDTH(layer->displayFrame);
            const int dst_crop_h = HEIGHT(layer->displayFrame);

            Rect srcRect(src_crop_x, src_crop_y, src_crop_x + src_crop_w, src_crop_y + src_crop_h);
            Rect dstRect(dst_crop_x, dst_crop_y, dst_crop_x + dst_crop_w, dst_crop_y + dst_crop_h);
            compare.src_crop = srcRect;
            compare.dst_crop = dstRect;
            compare.prev_orient = (current_ext_info.status & GRALLOC_EXTRA_MASK_ORIENT);
            compare.prev_alpha  = (current_ext_info.status & GRALLOC_EXTRA_MASK_ALPHA);
            compare.prev_blend  = (current_ext_info.status & GRALLOC_EXTRA_MASK_BLEND);
            compare.curr_orient = layer->transform << 12;
            compare.curr_alpha  = layer->planeAlpha << 16;
            compare.curr_blend  = ((layer->blending && 0x100) +
                (layer->blending && 0x004) +
                (layer->blending && 0x001)) << 24;
            compare.handle      = current_handle;
        }

        // determine this layer is the same with previous GPU composition layer
        bool layer_hit = false;
        for (int j = 0; j < prev_hit_count; ++j)
        {
            HitCompare prev_compare = m_current_hitlayer_info.current_layer_vector[j];

            if (saved_gles_layer_count > 0 &&
                m_current_hitlayer_info.gles_layer_count > 0 &&
                saved_gles_layer_count != m_current_hitlayer_info.gles_layer_count)
            {
                break;
            }

            // if handle and config of the layer are the same as previous hit layer
            // the layer apply cache algorithm.
            if (current_handle == prev_compare.handle && current_handle != 0 && prev_compare.handle != 0)
            {
                if (compare.src_crop == prev_compare.src_crop &&
                    compare.dst_crop == prev_compare.dst_crop &&
                    compare.curr_orient == prev_compare.curr_orient &&
                    compare.curr_alpha == prev_compare.curr_alpha &&
                    compare.curr_blend == prev_compare.curr_blend)
                {
                    layer_hit = true;
                    break;
                }
            }

        }

        if (layer_hit)
        {
            ++hit_count;
        }
        saved_hitlayer_info.add(compare);
    }

    // if hit layer count equal to previous GPU compose layer
    // this frame's FBT can be reuse, and these hit layers don't need to compose by GPU
    // and if current frame's gles layer count not equal previous frame gles count, it' won't contain hit layer
    if (hit_count == prev_hit_count && hit_count > 0 &&
        prev_hit_count > 0 && saved_gles_layer_count > 0 &&
        m_current_hitlayer_info.gles_layer_count > 0 &&
        saved_gles_layer_count == m_current_hitlayer_info.gles_layer_count)
    {
        for (int i = m_glesLayer_head; i <= m_glesLayer_tail; i++)
        {
            hwc_layer_1_t* layer = &list->hwLayers[i];
            layer->compositionType = HWC_OVERLAY;
            /*HWC_LOGD("FBT cache Hit layer rel=%d/acq=%d/handle=%pd",
            layer->releaseFenceFd, layer->acquireFenceFd,
            layer->handle);*/
        }
        m_current_hitlayer_info.gles_layer_count = saved_gles_layer_count;
        is_hit = true;
        HWC_LOGD("FBT frame hit gles_layer_count=%d,hit_layer_count=%d", saved_gles_layer_count, prev_hit_count);
    }
    else
    {
        // if this not hit FBT, replace layerHit info vector
        // then we can use it to compare next frame
        m_current_hitlayer_info.gles_layer_count = saved_gles_layer_count;
        m_current_hitlayer_info.current_layer_vector.clear();
        m_current_hitlayer_info.current_layer_vector.appendVector(saved_hitlayer_info);
    }

    return is_hit;
}

int CacheBase::getCacheCaps()
{
    return HWC_FEATURES_CACHE_CAPS_GPU_PASSIVE;
}

void CacheBase::clearBlitLayerCompare()
{
    m_mdp_hitlayer.index       = -1;
    m_mdp_hitlayer.dpy         = -1;
    m_mdp_hitlayer.handle      = NULL;
}

bool CacheBase::mdpuiHit(const int& dpy,const int& layer_index, hwc_layer_1_t* layer)
{
    if (dpy != 0)
        return false;

    HitCompare compare;
    {
        compare.index       = layer_index;
        compare.dpy         = dpy;
        compare.handle      = layer->handle;
    }

    if (compare.handle == m_mdp_hitlayer.handle
        && compare.handle != 0 && m_mdp_hitlayer.handle != 0
        && compare.index == m_mdp_hitlayer.index
        && compare.dpy == m_mdp_hitlayer.dpy)
    {
        return true;
    }
    else
    {
        m_mdp_hitlayer.index       = layer_index;
        m_mdp_hitlayer.dpy         = dpy;
        m_mdp_hitlayer.handle      = layer->handle;
        return false;
    }

    return false;
}
