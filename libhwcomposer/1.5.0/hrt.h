#ifndef HWC_HRT_H_
#define HWC_HRT_H_

#include <utils/tools.h>
#include <utils/Vector.h>
#include <utils/SortedVector.h>
#include <ui/Rect.h>
#include <dispatcher.h>

using namespace android;

// count overlap of each display
void countDisplaysOverlap(size_t num_display, hwc_display_contents_1_t** displays);

// fill max_ovl_overlap_num of primary display job which will inform display driver
void fillDisplaysOverlapResult(size_t num_display, hwc_display_contents_1_t** displays);

// push layer to gles if exceed overlap limit
bool arrangeDisplaysOverlap(size_t num_display, hwc_display_contents_1_t** displays);

// a layer in OverlapInterval
struct MemberLayer
{
    MemberLayer()
        : layer_idx(-1)
        , weight(0)
    { }

    MemberLayer(int idx, int w)
        : layer_idx(idx)
        , weight(w)
    { }

    bool operator < (const MemberLayer& rhs) const
    {
        return (layer_idx < rhs.layer_idx) ? true : false;
    }

    // the position in he layer list
    int layer_idx;

    // value of weight is determined as (fps of dpy)* Bpp
    // +- is determined if it is the start-point of scanline (projection of the layer).
    int weight;
};

// Store y-overlaypping result as an overlapping interval. Next, HRT algo checks
// x-overlapping status of each interval. The accurate overlapping value is max of
// x-overlapping and y-overlapping value.
struct OverlapInterval
{
    OverlapInterval()
        : value(0)
    { }

    bool operator < (const OverlapInterval& rhs) const
    {
        return (value < rhs.value);
    }

    // a list of layers which locate the same y-overlapping interval.
    SortedVector<MemberLayer> layers;

    // weighted value of overlap internal
    // weighted value = sum ( bpp * fps of each layer in the internal)
    int value;
};

#endif // HWC_HRT_H_
