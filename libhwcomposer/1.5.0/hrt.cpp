#define DEBUG_LOG_TAG "HRT"

#include "utils/debug.h"
#include "utils/tools.h"

#include "hrt.h"

#include "hwc.h"
#include "hwdev.h"
#include "platform.h"
#include "display.h"
#include "overlay.h"
#include "dispatcher.h"
#include "worker.h"
#include "composer.h"
#include "bliter.h"
#include "sync.h"

//#define MTK_HWC_HRT_LIMIT_DEBUG 1

static void layerRange(
    hwc_layer_1_t* layer, int type, int max_range, int* begin, int* end, bool sort_by_y)
{
    // check layer height region when sort_by_y is true otherwise check layer width
    const int top = getDstTop(layer);
    const int left = getDstLeft(layer);
    const int bottom = getDstBottom(layer);
    const int right = getDstRight(layer);

    switch (type)
    {
        case HWC_LAYER_TYPE_DIM:
            if (!(Platform::getInstance().m_config.overlay_cap & OVL_CAP_DIM_HW))
            {
                *begin = sort_by_y ? top : left;
                *end = sort_by_y ?  bottom : right;
            }
            break;

        case HWC_LAYER_TYPE_UI:
        case HWC_LAYER_TYPE_MM:
        case HWC_LAYER_TYPE_CURSOR:
            *begin = sort_by_y ? top : left;
            *end = sort_by_y ? bottom : right;
            break;

        case HWC_LAYER_TYPE_INVALID:
            *begin = 0;
            *end = max_range;
            break;

        default:
            *begin = -1;
            *end = -1;
            break;
    }
}

static int scanXRangeOverlap(const SortedVector< key_value_pair_t<int, int> >& overlap_vector)
{
    int overlap_num_w = 0;
    int max_overlap_num_w = 0;

    for (uint32_t k = 0; k < overlap_vector.size(); k++)
    {
        overlap_num_w += overlap_vector[k].value;
        max_overlap_num_w = (overlap_num_w > max_overlap_num_w) ? overlap_num_w : max_overlap_num_w;
    }

    return max_overlap_num_w;
}

static void addXKeyValue(
    SortedVector< key_value_pair_t<int, int> >* overlap_vector, int key, int value)
{
    if (key >= 0)
    {
        ssize_t i = overlap_vector->indexOf(key_value_pair_t<int, int>(key));
        if (i < 0)
        {
            overlap_vector->add(key_value_pair_t<int, int>(key, value));
        }
        else
        {
            int& add_value = overlap_vector->editItemAt(i).value;
            add_value += value;
        }
    }
    else
    {
        HWC_LOGW("Add invalid key(%d) in addKeyValue!", key);
    }
}

static bool addXRangeVector(
    SortedVector< key_value_pair_t<int, int> >* overlap_vector,
    hwc_layer_1_t* layer_list, LayerInfo* layer_info_list, int layer_idx, int dpy)
{
    int sort_by_y = false;
    int add_key = -1;
    int sub_key = -1;
    bool ret = false;

    // get info of layer
    LayerInfo& layer_info = layer_info_list[layer_idx];
    hwc_layer_1_t* layer = &layer_list[layer_idx];
    const int &type = layer_info.type;
    const int default_bpp = getBitsPerPixel(HAL_PIXEL_FORMAT_RGBA_8888) / 8;
    const int bpp = (type == HWC_LAYER_TYPE_INVALID) ? default_bpp : layer_info.bpp;

    // get fps of dpy
    const nsecs_t& refresh = DisplayManager::getInstance().m_data[dpy].refresh;
    const int fps = (refresh > 0) ? (int)(1e9 / refresh) : 60;
    const int weight = bpp * fps;
    const int& max_range = DisplayManager::getInstance().m_data[dpy].width;

    layerRange(layer, type, max_range, &add_key, &sub_key, sort_by_y);

#ifdef MTK_HWC_HRT_LIMIT_DEBUG
    HWC_LOGD("add layer(%d) X range refresh=%d /fps=%d/weight=%d/add=%d/sub=%d",
                 layer_idx, refresh, fps, weight, add_key, sub_key);
#endif

    if (add_key >= 0)
    {
        addXKeyValue(overlap_vector, add_key, weight);
        ret = true;
    }

    if (sub_key >= 0)
    {
        addXKeyValue(overlap_vector, sub_key, -weight);
        ret = true;
    }

    return ret;
}

static int checkXOverlap(
    hwc_layer_1_t* layer_list, LayerInfo* layer_info_list,
    SortedVector<MemberLayer> layer_member_list, int dpy)
{
    int overlap_num_w = 0;

    // x_overlap scan (second scan)
    // (key) used to keep l-value and r-value of layers(l,t,r,b) for scanning x-overlap
    // sort these l-value and r-value (x-point)
    // in x_overlap scan, no need to use value to keep layer-idx
    SortedVector< key_value_pair_t<int, int> > overlap_vector;

    for (uint32_t i = 0; i < layer_member_list.size(); i++)
    {
        // add vector without keep layer idx
        addXRangeVector(&overlap_vector, layer_list, layer_info_list, layer_member_list[i].layer_idx, dpy);
    }
    overlap_num_w = scanXRangeOverlap(overlap_vector);
    return overlap_num_w;
}

static void scanYRangeOverlap(
    const SortedVector< key_value_pair_t<int, Vector<MemberLayer> > >& overlap_vector,
    SortedVector< OverlapInterval >& sort_overlap_interval)
{
    int overlap_num_w = 0;
    SortedVector<MemberLayer> tmp_vector;

    for (uint32_t k = 0; k < overlap_vector.size(); k++)
    {
        Vector<MemberLayer> layer_member_vector = overlap_vector[k].value;
        for (uint32_t l = 0; l < layer_member_vector.size(); l++)
        {
            int find_index = -1;
            MemberLayer insert_member = layer_member_vector[l];
            find_index = tmp_vector.indexOf(insert_member);

            if (insert_member.weight > 0)
            {
                if (find_index < 0)
                {
                    tmp_vector.add(insert_member);
                    overlap_num_w += insert_member.weight;

#ifdef MTK_HWC_HRT_LIMIT_DEBUG
                    HWC_LOGD("add layer(%d) w=%d pos=%d", insert_member.layer_idx, insert_member.weight, overlap_vector[k].key);
#endif

                }
            }
            else
            {
                if (find_index >= 0)
                {
                    tmp_vector.removeAt(find_index);
                    overlap_num_w += insert_member.weight;

#ifdef MTK_HWC_HRT_LIMIT_DEBUG
                    HWC_LOGD("remove layer(%d) w=%d pos=%d", insert_member.layer_idx, insert_member.weight, overlap_vector[k].key);
#endif

                }
            }
        }

        // remeber y-overlapping group to vector and its' weighted-overlap num
        OverlapInterval add_group_vector;
        add_group_vector.layers = tmp_vector;
        add_group_vector.value = overlap_num_w;
        sort_overlap_interval.add(add_group_vector);

#ifdef MTK_HWC_HRT_LIMIT_DEBUG
        HWC_LOGD("ADD Overlap Group(y-overlap = %d)", overlap_num_w);
#endif

    }

}

static void addYKeyValue(
    SortedVector< key_value_pair_t<int, Vector<MemberLayer> > >* overlap_vector,
    int key, int value, int layer_idx)
{

#ifdef MTK_HWC_HRT_LIMIT_DEBUG
    HWC_LOGD("Insert layer(%d) w=%d pos=%d", layer_idx, value, key);
#endif

    if (key >= 0)
    {
        MemberLayer layer_member(layer_idx, value);
        ssize_t i = overlap_vector->indexOf(key_value_pair_t<int, Vector<MemberLayer> >(key));

        if (i < 0)
        {
            Vector<MemberLayer> new_vector;
            new_vector.add(layer_member);
            overlap_vector->add(key_value_pair_t<int, Vector<MemberLayer> >(key, new_vector));
        }
        else
        {
            Vector<MemberLayer>& new_vector = overlap_vector->editItemAt(i).value;
            int find_index = -1;
            for (uint32_t k = 0; k < new_vector.size(); k++)
            {
                if (layer_idx == new_vector[k].layer_idx)
                {
                    find_index = k;
                }
            }
            if (find_index < 0)
                new_vector.add(layer_member);
            else
                new_vector.removeAt(find_index);
        }

#ifdef MTK_HWC_HRT_LIMIT_DEBUG
        i = overlap_vector->indexOf(key);
        if (i < 0)
        {
            HWC_LOGW("Fail to add vector to pos(%d)", key);
        }
        else
        {
            Vector<MemberLayer>& new_vector = overlap_vector->editItemAt(i).value;
            for (uint32_t k = 0; k < new_vector.size(); k++)
            {
                HWC_LOGD("Check vector layer(%d) w=%d pos=%d", new_vector[k].layer_idx, new_vector[k].weight, key);
            }
        }
#endif

    }
    else
    {
        HWC_LOGW("Add invalid key(%d) in addKeyValue!", key);
    }
}

static bool addYRangeVector(SortedVector< key_value_pair_t<int, Vector<MemberLayer> > >* overlap_vector,
                      hwc_layer_1_t* layer_list, LayerInfo* layer_info_list,
                      int layer_idx, int dpy)
{
    int sort_by_y = true;
    int add_key = -1;
    int sub_key = -1;
    bool ret = false;

    // get info of layer
    LayerInfo& layer_info = layer_info_list[layer_idx];
    hwc_layer_1_t* layer = &layer_list[layer_idx];
    const int& type = layer_info.type;
    const int default_bpp = getBitsPerPixel(HAL_PIXEL_FORMAT_RGBA_8888) / 8;
    const int bpp = (type == HWC_LAYER_TYPE_INVALID) ? default_bpp : layer_info.bpp;

    // get fps of dpy
    const int& max_range = DisplayManager::getInstance().m_data[dpy].height;
    const nsecs_t& refresh = DisplayManager::getInstance().m_data[dpy].refresh;
    const int fps = (refresh > 0) ? (int)(1e9 / refresh) : 60;
    const int weight = bpp * fps;

    layerRange(layer, type, max_range, &add_key, &sub_key, sort_by_y);

#ifdef MTK_HWC_HRT_LIMIT_DEBUG
    HWC_LOGD("add layer(%d) Y range refresh=%d /fps=%d/weight=%d/add=%d/sub=%d",
                 layer_idx, refresh, fps, weight, add_key, sub_key);
#endif

    if (add_key >= 0)
    {
        addYKeyValue(overlap_vector, add_key, weight, layer_idx);
        ret = true;
    }

    if (sub_key >= 0)
    {
        addYKeyValue(overlap_vector, sub_key, -weight, layer_idx);
        ret = true;
    }

    return ret;
}

static int checkXYOverlap(hwc_layer_1_t* layer_list, LayerListInfo layer_info, int dpy, const int& layers_num)
{
    // key used to keep t-value and b-value of layers(l,t,r,b) for scanning y-overlap
    // sort these t-value and b-value (y-point)
    // Vector<MemberLayer> used to keep layer-idxs of which t-value or b-value
    // is equal to key
    SortedVector< key_value_pair_t<int, Vector<MemberLayer> > > overlap_vector;

    // used to keep the group of layers overlap set during overlap scan
    // the group is sorted by weighted-overlap num of a layer-group
    SortedVector<OverlapInterval> sort_overlap_interval;

    int overlap_num_w = 0;

    LayerInfo* layer_info_list = layer_info.layer_info_list;
    if (layer_info_list == NULL)
    {
        HWC_LOGW("NULL layer_info_list");
        return -1;
    }

    for (int32_t i = 0; i < layers_num; i++)
    {

#ifdef MTK_HWC_HRT_LIMIT_DEBUG
        int fps = (int)(DisplayManager::getInstance().m_data[dpy].refresh);
        HWC_LOGD("add layer(%d) for dpy[%d] fps=%d", i, dpy, fps);
#endif

        addYRangeVector(&overlap_vector, layer_list, layer_info_list, i, dpy);
        i = (i == layer_info.gles_head) ? layer_info.gles_tail : i;
    }

    scanYRangeOverlap(overlap_vector, sort_overlap_interval);

    // check x-overlap of group_vector based on y-overlap result
    // larger y-overlap num of group has high priority to be check
    // get max x+y-overlap after for-loop
    for (int32_t k = sort_overlap_interval.size() - 1; k >= 0; k--)
    {
        SortedVector<MemberLayer> group_vector = sort_overlap_interval[k].layers;

        // if x-overlap >= y-overlap of next checked group
        // this x-overlap is result of max overlap num
        if (overlap_num_w >= sort_overlap_interval[k].value) break;
        overlap_num_w = checkXOverlap(layer_list, layer_info_list, group_vector, dpy);
    }

    return overlap_num_w;
}

void countDisplaysOverlap(size_t num_display, hwc_display_contents_1_t** displays)
{
    if (Platform::getInstance().m_config.ovl_overlap_limit <= 0)
        return;

    // overlap num * fps * bpp
    int overlap_num_w = 0;

    // HRT overlap counting for displays
    for (int i = 0; i < num_display; i++)
    {
        if (HWC_DISPLAY_VIRTUAL == i)
            continue;

        DispatcherJob* job = HWCDispatcher::getInstance().getExistJob(i);
        if (job == NULL) continue;

        hwc_display_contents_1_t* list = displays[i];
        if (list->flags & HWC_MIRROR_DISPLAY) continue;

        LayerListInfo &layer_info = job->layer_info;
        hwc_layer_1_t* layer_list = list->hwLayers;
        const int layers_num = list->numHwLayers - 1;

        overlap_num_w = checkXYOverlap(layer_list, layer_info, i, layers_num);
        if (overlap_num_w < 0)
            continue;

        // the max_overlap_layer_num is not ready to normalized overlap num without weight
        job->layer_info.max_overlap_layer_num_w = overlap_num_w;
    }

}

void fillDisplaysOverlapResult(size_t num_display, hwc_display_contents_1_t** displays)
{
    if (Platform::getInstance().m_config.ovl_overlap_limit <= 0)
        return;

    hwc_display_contents_1_t* list;
    const int bpp = getBitsPerPixel(HAL_PIXEL_FORMAT_RGBA_8888) / 8;

    // hrt overlap limit is based on the fps of primary display
    const int main_dpy_fps = (int)(1e9 / DisplayManager::getInstance().m_data[HWC_DISPLAY_PRIMARY].refresh);
    const int weight = main_dpy_fps * bpp;

    // count total overlap of displays
    int total_overlap_w = 0;

    for (int i = 0; i < num_display; i++)
    {
        if (HWC_DISPLAY_VIRTUAL == i) continue;

        DispatcherJob* job = HWCDispatcher::getInstance().getExistJob(i);
        if (job == NULL) continue;

        list = displays[i];
        if (list->flags & HWC_MIRROR_DISPLAY) continue;

        total_overlap_w += job->layer_info.max_overlap_layer_num_w;
    }

    // set total (main dpy + MHL) overlap num to primary display driver
    DispatcherJob* job = HWCDispatcher::getInstance().getExistJob(HWC_DISPLAY_PRIMARY);
    if ((job != NULL) && (weight > 0))
    {
        if (total_overlap_w > 0)
        {
            job->layer_info.max_overlap_layer_num = (int)(ceilf((float) total_overlap_w / weight));
            //HWC_LOGD("HRT/overlap=%d", job->layer_info.max_overlap_layer_num);
        }
    }
}

bool arrangeDisplaysOverlap(size_t num_display, hwc_display_contents_1_t** displays)
{
    const int& ovl_overlap_limit = Platform::getInstance().m_config.ovl_overlap_limit;

    // if ovl_overlap_limit or new hrt alg is not enable
    if (ovl_overlap_limit <= 0)
        return false;

    hwc_display_contents_1_t* list;
    const int bpp = getBitsPerPixel(HAL_PIXEL_FORMAT_RGBA_8888) / 8;

    // hrt overlap limit is based on the fps of primary display
    const nsecs_t& main_refresh = DisplayManager::getInstance().m_data[HWC_DISPLAY_PRIMARY].refresh;
    const int main_dpy_fps = (main_refresh > 0) ? (int)(1e9 / main_refresh) : 60;
    const int main_weight = main_dpy_fps * bpp;

    // use to remember how many overlap num remain.
    int remain_overlap_limit_w = ovl_overlap_limit * main_weight;

    fillDisplaysOverlapResult(num_display, displays);
    DispatcherJob* job = HWCDispatcher::getInstance().getExistJob(HWC_DISPLAY_PRIMARY);
    if (job == NULL)
    {
        return false;
    }
    // whithin overlap limit, no need to push layer to gles
    if (ovl_overlap_limit >= job->layer_info.max_overlap_layer_num)
    {
        return false;
    }

    HWC_LOGD("Layering will be changed due to meeting HRT");

    for (int32_t i = num_display - 1; i >= 0; i--)
    {
        if (HWC_DISPLAY_VIRTUAL == i) continue;

        job = HWCDispatcher::getInstance().getExistJob(i);
        if (job == NULL) continue;

        list = displays[i];
        if (list->flags & HWC_MIRROR_DISPLAY) continue;

        int& gles_head = job->layer_info.gles_head;
        int& gles_tail = job->layer_info.gles_tail;
        const int layers_num = list->numHwLayers - 1;
        int& max_overlap_layer_num_w = job->layer_info.max_overlap_layer_num_w;

        // need to preserve at least one ovl input to remaining dpy
        // preserve i ovl inputs for 0 ~ i-1 dpy
        int valid_overlap_limit_w = remain_overlap_limit_w - (i * main_weight);
        if (valid_overlap_limit_w - max_overlap_layer_num_w < 0)
        {
            const nsecs_t& refresh = DisplayManager::getInstance().m_data[i].refresh;
            const int dpy_fps = (refresh > 0) ? (int)(1e9 / refresh) : 60;
            const int dpy_weight = dpy_fps * bpp;
            // max num of ovl input (need to keep one input for gles)
            int push_ovl_num = (int)(floorf((float) valid_overlap_limit_w / dpy_weight));
            HWC_LOGD("dpy[%d] fps%d Hit Limit/cur=%d limit=%d/ghead=%d gtail=%d\ndecision: push_ovl=%d num=%d",
                i, dpy_fps, max_overlap_layer_num_w / dpy_weight, valid_overlap_limit_w / dpy_weight,
                gles_head, gles_tail, push_ovl_num, layers_num);

            // for debug if layering wrong, it will remove after stable
            // TODO: it will remove after feature stable
            int pre_gles_head = gles_head;
            int pre_gles_tail = gles_tail;

            if(gles_head >= push_ovl_num || gles_head == -1)
            {
                gles_head = push_ovl_num - 1;
                gles_tail = layers_num - 1;
            }
            else
            {
                gles_tail = gles_head + (layers_num - push_ovl_num);
            }

            //need to keep i ovl inputs for 0 ~ i-1 dpy
            remain_overlap_limit_w = i * main_weight;
            HWC_LOGD("dpy[%d] GLES Changed /gles_head=%d/gles_tail=%d",
                      i, job->layer_info.gles_head, job->layer_info.gles_tail);

            // for debug if layering wrong
            // TODO: it will remove after feature stable
            if (pre_gles_head >= 0)
            {
                if (pre_gles_head < gles_head || pre_gles_tail > gles_tail)
                {
                      HWC_LOGE("dpy[%d] GLES set unexpected /p_head=%d/p_tail=%d/head=%d/tail=%d",
                      i, pre_gles_head, pre_gles_tail, gles_head, gles_tail);
                      abort();
                }
            }
            if (gles_head < 0 || gles_tail < 0 || gles_head >= layers_num || gles_tail >= layers_num)
            {
                HWC_LOGE("dpy[%d] GLES set out of range /gles_head=%d/gles_tail=%d",
                i, gles_head, gles_tail);
                abort();
            }

        }
        else
        {
            remain_overlap_limit_w -= max_overlap_layer_num_w;
        }
    }
    return true;

}

