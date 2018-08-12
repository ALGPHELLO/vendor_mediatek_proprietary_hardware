#ifndef HWC_CACHE_H_
#define HWC_CACHE_H_

#include <ui/Rect.h>
#include <utils/Singleton.h>
#include <utils/Vector.h>
#include <system/window.h>

#include "hwc_priv.h"
using namespace android;

struct hwc_display_contents_1;

// Layer infomation are used in cache algorithm
struct HitCompare
{
    HitCompare()
        : src_crop(0,0,0,0)
          , dst_crop(0,0,0,0)
          , prev_orient(0)
          , prev_alpha(false)
          , prev_blend(false)
          , curr_orient(false)
          , curr_alpha(false)
          , curr_blend(false)
          , index(-1)
          , dpy(-1)
          , handle(NULL)
    { }

    // current layer info
    Rect src_crop;
    Rect dst_crop;
    int prev_orient;
    int prev_alpha;
    int prev_blend;
    int curr_orient;
    int curr_alpha;
    int curr_blend;
    int transform;
    int index;
    int dpy;
    buffer_handle_t handle;
};

struct HitLayerList
{
    HitLayerList()
        : gles_layer_count(0)
    {
        current_layer_vector.clear();
    }

    //keep GLES layer count
    int gles_layer_count;
    //keep hit layer info
    Vector<HitCompare> current_layer_vector;
};

// CacheBase is used to determin hitLayer, and store layer comparison info.
// These info can be used in next layer list
class CacheBase : public Singleton<CacheBase>
{
public:
    CacheBase();
    ~CacheBase();

    // to determine the layer list could apply cache algorithm
    // It will change the composition type of cache hit layers from GLES -> HWC
    bool layerHit(hwc_display_contents_1* list, int gles_head, int gles_tail);

    void clearBlitLayerCompare();
    bool mdpuiHit(const int& dpy,const int& layer_index, hwc_layer_1_t* layer);

    // get cache feature capability
    int getCacheCaps();

    // get current hit layer info
    HitLayerList getCurrentHitLayers() { return m_current_hitlayer_info; }

private:
    HitLayerList m_current_hitlayer_info;
    HitCompare m_mdp_hitlayer;
    // store the range of layers appling cache algorithm
    int m_glesLayer_head;
    int m_glesLayer_tail;
};
#endif // HWC_CACHE_H_
