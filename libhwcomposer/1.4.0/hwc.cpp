#define DEBUG_LOG_TAG "HWC"

#include <cutils/properties.h>

#include "gralloc_mtk_defs.h"

#include "utils/debug.h"
#include "utils/tools.h"

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
#include "service.h"

#include "ui/gralloc_extra.h"
#include "utils/transform.h"
#include <utils/SortedVector.h>
#include <utils/String8.h>

// ---------------------------------------------------------------------------

// init static vars
DevicePlatform::PlatformConfig DevicePlatform::m_config;

#ifdef MTK_HWC_PROFILING
bool g_handle_all_layers = false;
bool g_prepare_all_layers = false;
#endif

// ---------------------------------------------------------------------------

ANDROID_SINGLETON_STATIC_INSTANCE(HWCMediator);

HWCMediator::HWCMediator()
{
    // check features setting
    initFeatures();

    MMUDevice::getInstance();

    DevicePlatform::getInstance();

    char value[PROPERTY_VALUE_MAX];
    sprintf(value, "%d", DevicePlatform::m_config.compose_level);
    property_set("debug.hwc.compose_level", value);

    // check if virtual display could be composed by hwc
    status_t err = DispDevice::getInstance().createOverlaySession(HWC_DISPLAY_VIRTUAL);
    m_features.virtuals = (err == NO_ERROR);
    DispDevice::getInstance().destroyOverlaySession(HWC_DISPLAY_VIRTUAL);

    if (!m_features.gmo)
    {
        // set needed buffer count for bufferqueue
        // gpu driver could use it to change the amounts of buffers with native window
        sprintf(value, "%d", DevicePlatform::m_config.bq_count);
        property_set("debug.hwc.bq_count", value);
    }
    else
    {
        property_set("debug.hwc.bq_count", "3");
    }
}

HWCMediator::~HWCMediator()
{
}

void HWCMediator::open(hwc_private_device_t* device)
{
    // create listener
    struct DisplayListener : public DisplayManager::EventListener
    {
        DisplayListener(hwc_private_device_t* dev) : device(dev) { }
    private:
        hwc_private_device_t* device;
        virtual void onVSync(int dpy, nsecs_t timestamp, bool enabled)
        {
            if (enabled && device && device->procs && device->procs->vsync)
                device->procs->vsync(device->procs, dpy, timestamp);

            HWCDispatcher::getInstance().onVSync(dpy);
        }
        virtual void onPlugIn(int dpy)
        {
            HWCDispatcher::getInstance().onPlugIn(dpy);
        }
        virtual void onPlugOut(int dpy)
        {
            HWCDispatcher::getInstance().onPlugOut(dpy);
        }
        virtual void onHotPlugExt(int dpy, int connected)
        {
            if (device && device->procs && device->procs->hotplug)
                device->procs->hotplug(device->procs, dpy, connected);
        }
    };
    DisplayManager::getInstance().setListener(new DisplayListener(device));

    // initialize DisplayManager
    DisplayManager::getInstance().init();

    // initialize ServiceManager
    ServiceManager::getInstance().init();
}

void HWCMediator::close(hwc_private_device_t* /*device*/)
{
    DisplayManager::getInstance().setListener(NULL);
}

void HWCMediator::dump(char* buff, int buff_len)
{
    memset(buff, 0, buff_len);

    struct dump_buff log = {
        msg:      buff,
        msg_len:  buff_len,
        len:      0,
    };

    dump_printf(&log, "\n[HWC Features Support]\n"
            "  ext=%d vir=%d enhance=%d svp=%d\n",
            m_features.externals, m_features.virtuals, m_features.enhance, m_features.svp);

    DisplayManager::getInstance().dump(&log);

    char value[PROPERTY_VALUE_MAX];
    char default_value[PROPERTY_VALUE_MAX];

    // check compose level
    property_get("debug.hwc.compose_level", value, "0");
    DevicePlatform::m_config.compose_level = atoi(value);

    // check mirror state
    sprintf(default_value, "%d", DevicePlatform::m_config.mirror_state);
    property_get("debug.hwc.mirror_state", value, default_value);
    DevicePlatform::m_config.mirror_state = atoi(value);

    dump_printf(&log, "\n[HWC Platform Config]\n"
            "  plat=%x comp=%d client=%d\n",
            DevicePlatform::m_config.platform,
            DevicePlatform::m_config.compose_level,
            DevicePlatform::m_config.client_id);

    // dynamic change mir format for mhl
    property_get("debug.hwc.mhl_output", value, "0");
    DevicePlatform::m_config.format_mir_mhl = atoi(value);

    // check profile level
    sprintf(default_value, "%d", DisplayManager::m_profile_level);
    property_get("debug.hwc.profile_level", value, default_value);
    DisplayManager::m_profile_level = atoi(value);

    // check the maximum scale ratio of mirror source
    property_get("debug.hwc.mir_scale_ratio", value, "0");

    float scale_ratio = strtof(value, NULL);
    DevicePlatform::m_config.mir_scale_ratio =
        (scale_ratio != 0 && errno != ERANGE) ? scale_ratio : 0;

    // chech debug setting of bandwidth limitation
    property_get("debug.hwc.bytes_limit", value, "0");
    DevicePlatform::m_config.bytes_limit = atoi(value);

    // check debug setting of ovl_overlap_limit
    sprintf(default_value, "%d", DevicePlatform::m_config.ovl_overlap_limit);
    property_get("debug.hwc.ovl_overlap_limit", value, default_value);
    DevicePlatform::m_config.ovl_overlap_limit = atoi(value);

    // check dump level
    property_get("debug.hwc.dump_level", value, "0");
    int dump_level = atoi(value);

    HWCDispatcher::getInstance().dump(&log, (dump_level & (DUMP_MM | DUMP_SYNC)));

    // reset dump level
    property_set("debug.hwc.dump_level", "0");

    // set disp secure for test
    property_get("debug.hwc.force_pri_insecure", value, "0");
    if (0 != atoi(value))
    {
        DisplayManager::getInstance().m_data[HWC_DISPLAY_PRIMARY].secure = false;
        HWC_LOGI("force set primary display insecure");
    }

    property_get("debug.hwc.force_wfd_insecure", value, "0");
    if (0 != atoi(value))
    {
        DisplayManager::getInstance().m_data[HWC_DISPLAY_VIRTUAL].secure = false;
        HWC_LOGI("force set virtual display insecure");
    }

#ifdef MTK_HWC_PROFILING
    property_get("debug.hwc.profile_handle_all", value, "0");
    g_handle_all_layers = (atoi(value) != 0);
#endif

    property_get("debug.hwc.disable.skip.display", value, "0");
    HWCDispatcher::getInstance().m_disable_skip_redundant = atoi(value) ? true : false;
}

int HWCMediator::query(int what, int* value)
{
    switch (what)
    {
        case HWC_BACKGROUND_LAYER_SUPPORTED:
            // not support background layer
            value[0] = 0;
            break;

        case HWC_VSYNC_PERIOD:
            value[0] = DisplayManager::getInstance().m_data[0].refresh;
            break;

        case HWC_FEATURES_STATE:
            memcpy(value, &m_features, sizeof(hwc_feature_t));
            break;

        default:
            return -EINVAL;
    }

    return 0;
}

int HWCMediator::eventControl(int dpy, int event, int enabled)
{
    switch (event)
    {
        case HWC_EVENT_VSYNC:
            DisplayManager::getInstance().requestVSync(dpy, enabled);
            break;

        default:
            return -EINVAL;
    }

    return 0;
}

int HWCMediator::setPowerMode(int dpy, int mode)
{
    // screen blanking based on early_suspend in the kernel
    HWC_LOGI("SetPowerMode(%d) Display(%d)", mode, dpy);

    HWCDispatcher::getInstance().setPowerMode(dpy, mode);

    DisplayManager::getInstance().setPowerMode(dpy, mode);

    // disable mirror mode when display blanks
    if ((DevicePlatform::m_config.mirror_state & MIRROR_DISABLED) != MIRROR_DISABLED)
    {
        if (mode == HWC_POWER_MODE_OFF || HWC_POWER_MODE_DOZE_SUSPEND == mode)
        {
            DevicePlatform::m_config.mirror_state |= MIRROR_PAUSED;
        }
        else
        {
            DevicePlatform::m_config.mirror_state &= ~MIRROR_PAUSED;
        }
    }

    return 0;
}

int HWCMediator::getConfigs(int dpy, uint32_t* configs, size_t* numConfigs)
{
    HWC_LOGI("getConfigs Display(%d)", dpy);

    if (dpy < 0 || dpy >= DisplayManager::MAX_DISPLAYS)
    {
        HWC_LOGE("Failed to get display configs (dpy=%d)", dpy);
        return -EINVAL;
    }

    if (configs) configs[0] = 0;
    if (numConfigs) *numConfigs = 1;

    return 0;
}

int HWCMediator::getAttributes(
    int dpy, uint32_t config,
    const uint32_t* attributes, int32_t* values)
{
    HWC_LOGI("getAttributes Display(%d)", dpy);

    if (dpy < 0 || dpy >= DisplayManager::MAX_DISPLAYS || config != 0)
    {
        HWC_LOGE("Failed to get display attributes (dpy=%d, config=%d)", dpy, config);
        return -EINVAL;
    }

    const DisplayData* disp_data = &DisplayManager::getInstance().m_data[dpy];

    if (disp_data->connected == false)
    {
        HWC_LOGW("Failed to get display attributes (dpy=%d is not connected)", dpy);
        return -EINVAL;
    }

    int i = 0;
    while (attributes[i] != HWC_DISPLAY_NO_ATTRIBUTE)
    {
        switch (attributes[i])
        {
            case HWC_DISPLAY_VSYNC_PERIOD:
                values[i] = disp_data->refresh;
                break;
            case HWC_DISPLAY_WIDTH:
                values[i] = disp_data->width;
                break;
            case HWC_DISPLAY_HEIGHT:
                values[i] = disp_data->height;
                break;
            case HWC_DISPLAY_DPI_X:
                values[i] = disp_data->xdpi * 1000;
                break;
            case HWC_DISPLAY_DPI_Y:
                values[i] = disp_data->ydpi * 1000;
                break;
            case HWC_DISPLAY_SUBTYPE:
                values[i] = disp_data->subtype;
                break;
            default:
                HWC_LOGE("unknown display attribute[%d] %#x", i, attributes[i]);
                return -EINVAL;
        }

        i++;
    }

    return 0;
}

int HWCMediator::getActiveConfig(int dpy)
{
    // TODO
    if (dpy < 0 || dpy >= DisplayManager::MAX_DISPLAYS)
    {
        HWC_LOGE("Failed to get active configs (dpy=%d)", dpy);
        return -EINVAL;
    }

    return 0;
}

int HWCMediator::setActiveConfig(int dpy, int index)
{
    // TODO
    if (dpy < 0 || dpy >= DisplayManager::MAX_DISPLAYS || index != 0)
    {
        HWC_LOGE("Failed to set active configs (dpy=%d, idx=%d)", dpy, index);
        return -EINVAL;
    }

    return 0;
}

int HWCMediator::setCursorPosition(int /*dpy*/, int /*x*/, int /*y*/)
{
    // TODO
    return 0;
}

inline int layerDirty(
    PrivateHandle* priv_handle, hwc_layer_1_t* layer, int dirty)
{
    gralloc_extra_ion_sf_info_t* ext_info = &priv_handle->ext_info;
    bool dirty_buff = (ext_info->status & GRALLOC_EXTRA_MASK_SF_DIRTY);
    bool dirty_param = false;

    if (dirty)
    {
        // [NOTE] Use previous check result for secondary display with mirror mode
        dirty_param = (ext_info->status & GRALLOC_EXTRA_MASK_DIRTY_PARAM);
    }
    else
    {
        int src_crop_x = getSrcLeft(layer);
        int src_crop_y = getSrcTop(layer);
        int src_crop_w = getSrcWidth(layer);
        int src_crop_h = getSrcHeight(layer);
        int dst_crop_x = layer->displayFrame.left;
        int dst_crop_y = layer->displayFrame.top;
        int dst_crop_w = WIDTH(layer->displayFrame);
        int dst_crop_h = HEIGHT(layer->displayFrame);

        if (ext_info->src_crop.x != src_crop_x ||
            ext_info->src_crop.y != src_crop_y ||
            ext_info->src_crop.w != src_crop_w ||
            ext_info->src_crop.h != src_crop_h ||
            ext_info->dst_crop.x != dst_crop_x ||
            ext_info->dst_crop.y != dst_crop_y ||
            ext_info->dst_crop.w != dst_crop_w ||
            ext_info->dst_crop.h != dst_crop_h)
        {
            ext_info->src_crop.x = src_crop_x;
            ext_info->src_crop.y = src_crop_y;
            ext_info->src_crop.w = src_crop_w;
            ext_info->src_crop.h = src_crop_h;

            ext_info->dst_crop.x = dst_crop_x;
            ext_info->dst_crop.y = dst_crop_y;
            ext_info->dst_crop.w = dst_crop_w;
            ext_info->dst_crop.h = dst_crop_h;

            dirty_param = true;
        }

        int prev_orient = (ext_info->status & GRALLOC_EXTRA_MASK_ORIENT);
        int prev_alpha  = (ext_info->status & GRALLOC_EXTRA_MASK_ALPHA);
        int prev_blend  = (ext_info->status & GRALLOC_EXTRA_MASK_BLEND);
        int curr_orient = layer->transform << 12;
        int curr_alpha  = layer->planeAlpha << 16;
        int curr_blend  = ((layer->blending && 0x100) +
                          (layer->blending && 0x004) +
                          (layer->blending && 0x001)) << 24;

        dirty_param |= (prev_orient != curr_orient);
        dirty_param |= (prev_alpha != curr_alpha);
        dirty_param |= (prev_blend != curr_blend);

        gralloc_extra_sf_set_status(
            ext_info, GRALLOC_EXTRA_MASK_ORIENT, curr_orient);

        gralloc_extra_sf_set_status(
            ext_info, GRALLOC_EXTRA_MASK_ALPHA, curr_alpha);

        gralloc_extra_sf_set_status(
            ext_info, GRALLOC_EXTRA_MASK_BLEND, curr_blend);

        gralloc_extra_sf_set_status(
            ext_info, GRALLOC_EXTRA_MASK_DIRTY_PARAM, ((int)dirty_param << 26));

        gralloc_extra_perform(
            layer->handle, GRALLOC_EXTRA_SET_IOCTL_ION_SF_INFO, ext_info);
    }

    return (dirty_buff | (dirty_param << 1));
}

inline int layerType(
    int dpy, PrivateHandle* priv_handle, hwc_layer_1_t* layer,
    int& dirty, bool secure)
{
    // HWC is disabled or some other reasons
    if (layer->flags & HWC_SKIP_LAYER)
        return HWC_LAYER_TYPE_INVALID;

    int compose_level = DevicePlatform::m_config.compose_level;
    bool check_dim_layer = (DevicePlatform::m_config.overlay_cap & OVL_CAP_DIM);

    if (check_dim_layer && (layer->flags & HWC_DIM_LAYER))
    {
        if (compose_level & COMPOSE_DISABLE_UI)
            return HWC_LAYER_TYPE_INVALID;

        if (Transform::ROT_INVALID & layer->transform)
            return HWC_LAYER_TYPE_INVALID;

        if (getDstWidth(layer) <= 0 || getDstHeight(layer) <= 0)
            return HWC_LAYER_TYPE_INVALID;

        return HWC_LAYER_TYPE_DIM;
    }

    if (!layer->handle)
        return HWC_LAYER_TYPE_INVALID;

    getPrivateHandleInfo(layer->handle, priv_handle);

    if (((priv_handle->usage & GRALLOC_USAGE_PROTECTED) ||
         (priv_handle->usage & GRALLOC_USAGE_SECURE)) && !secure)
        return HWC_LAYER_TYPE_INVALID;

    dirty = layerDirty(priv_handle, layer, dirty);

    int buffer_type = (priv_handle->ext_info.status & GRALLOC_EXTRA_MASK_TYPE);
    if (buffer_type == GRALLOC_EXTRA_BIT_TYPE_VIDEO ||
        buffer_type == GRALLOC_EXTRA_BIT_TYPE_CAMERA ||
        priv_handle->format == HAL_PIXEL_FORMAT_YV12)
    {
        if (compose_level & COMPOSE_DISABLE_MM)
            return HWC_LAYER_TYPE_INVALID;

        // check platform capability
        bool is_high = false;
        if (DevicePlatform::getInstance().isMMLayerValid(dpy, layer, priv_handle, is_high))
        {
            // always treat third party app queued yuv buffer as dirty
            if (buffer_type != GRALLOC_EXTRA_BIT_TYPE_VIDEO &&
                buffer_type != GRALLOC_EXTRA_BIT_TYPE_CAMERA)
                dirty |= HWC_LAYER_DIRTY_BUFFER;

            if (buffer_type == GRALLOC_EXTRA_BIT_TYPE_CAMERA)
                dirty |= HWC_LAYER_DIRTY_CAMERA;

            // [WORKAROUND]
            if (is_high) return HWC_LAYER_TYPE_MM_HIGH;

            return HWC_LAYER_TYPE_MM;
        }
        else
        {
            return HWC_LAYER_TYPE_INVALID;
        }
    }

    // any layers above layer handled by fbt should also be handled by fbt
    if (compose_level & COMPOSE_DISABLE_UI)
        return HWC_LAYER_TYPE_INVALID;

    // check platform capability
    if (DevicePlatform::getInstance().isUILayerValid(dpy, layer, priv_handle))
    {
        // always treat CPU layer as dirty
        if (buffer_type != GRALLOC_EXTRA_BIT_TYPE_GPU)
            dirty |= HWC_LAYER_DIRTY_BUFFER;

        dirty &= ~HWC_LAYER_DIRTY_PARAM;

        if (layer->flags & HWC_IS_CURSOR_LAYER)
        {
            return HWC_LAYER_TYPE_CURSOR;
        }

        return HWC_LAYER_TYPE_UI;
    }
    else
    {
        return HWC_LAYER_TYPE_INVALID;
    }
}

inline bool exceedOverlapLimit(
    SortedVector< key_value_pair_t<int, int> >& overlap_vector, int ovl_overlap_limit)
{
    int sum = 0;

    for(uint32_t k = 0; k < overlap_vector.size(); k++)
    {
        sum += overlap_vector[k].value;
        if (sum > ovl_overlap_limit)
        {
            return true;
        }
    }

    return false;
}

inline void addKeyValue(
    SortedVector< key_value_pair_t<int, int> >& overlap_vector, int key, int value)
{
    if (key >= 0)
    {
        ssize_t i = overlap_vector.indexOf(overlap_vector.itemAt(key));
        if (i < 0)
        {
            overlap_vector.add(key_value_pair_t<int, int>(key, value));
        }
        else
        {
            int& add_value = overlap_vector.editItemAt(i).value;
            add_value += value;
        }
    }
    else
    {
        HWC_LOGW("Add invalid key(%d) in addKeyValue!", key);
    }
}

inline void layerRange(
    hwc_layer_1_t* layer, int type, int max_range, int& top, int& bottom)
{
    switch (type)
    {
        case HWC_LAYER_TYPE_DIM:
            if (!(DevicePlatform::m_config.overlay_cap & OVL_CAP_DIM_HW))
            {
                top = getDstTop(layer);
                bottom = getDstBottom(layer);
            }
            break;

        case HWC_LAYER_TYPE_UI:
        case HWC_LAYER_TYPE_MM:
        case HWC_LAYER_TYPE_CURSOR:
            top = getDstTop(layer);
            bottom = getDstBottom(layer);
            break;

        case HWC_LAYER_TYPE_INVALID:
            top = 0;
            bottom = max_range;
            break;

        default:
            top = -1;
            bottom = -1;
            break;
    }
}

inline bool addOverlapVector(
    SortedVector< key_value_pair_t<int, int> >& overlap_vector, hwc_layer_1_t* layer, int type, int max_range)
{
    int add_key = -1;
    int sub_key = -1;
    bool ret = false;

    layerRange(layer, type, max_range, add_key, sub_key);

    if (add_key >= 0)
    {
        addKeyValue(overlap_vector, add_key, 1);
        ret = true;
    }

    if (sub_key >= 0)
    {
        addKeyValue(overlap_vector, sub_key, -1);
        ret = true;
    }

    return ret;
}

inline bool subOverlapVector(
    SortedVector< key_value_pair_t<int, int> >& overlap_vector, hwc_layer_1_t* layer, int type, int max_range)
{
    int add_key = -1;
    int sub_key = -1;
    bool ret = false;

    layerRange(layer, type, max_range, sub_key, add_key);

    if (add_key >= 0)
    {
        addKeyValue(overlap_vector, add_key, 1);
        ret = true;
    }

    if (sub_key >= 0)
    {
        addKeyValue(overlap_vector, sub_key, -1);
        ret = true;
    }

    return ret;
}

inline void layerBytes(
    PrivateHandle* priv_handle, hwc_layer_1_t* layer, int type)
{
    int roi_bytes = 0;

    if (layer->handle)
    {
        switch (type)
        {
            case HWC_LAYER_TYPE_DIM:
                // check capability to make sure if HWC need to read dim pixels
                if (!(DevicePlatform::m_config.overlay_cap & OVL_CAP_DIM_HW))
                {
                    // TODO: check bpp from display driver
                    int bpp = 16;
                    int size = getDstWidth(layer) * getDstHeight(layer);

                    roi_bytes = (int)(size * bpp / 8);
                }
                break;

            case HWC_LAYER_TYPE_UI:
            case HWC_LAYER_TYPE_MM:
            case HWC_LAYER_TYPE_CURSOR:
                {
                    int bpp = getBitsPerPixel(priv_handle->format);
                    int size = getSrcWidth(layer) * getSrcHeight(layer);

                    roi_bytes = (int)(size * bpp / 8);
                }
                break;

            case HWC_LAYER_TYPE_INVALID:
                roi_bytes = -1;
                break;
        }
    }

    priv_handle->roi_bytes = roi_bytes;
}

inline void layerSet(
    int dpy, DispatcherJob* job, hwc_layer_1_t* layer, PrivateHandle* priv_handle,
    int ovl_idx, int layer_idx, int type, int dirty, int& num_ui, int& num_mm)
{
    HWLayer* hw_layer = &job->hw_layers[ovl_idx];
    dirty |= HWCDispatcher::getInstance().verifyType(
                  dpy, priv_handle, ovl_idx, dirty, type);

    hw_layer->enable  = true;
    hw_layer->index   = layer_idx;
    hw_layer->type    = type;
    hw_layer->dirty   = (dirty != HWC_LAYER_DIRTY_NONE && dirty != HWC_LAYER_DIRTY_CAMERA);
    memcpy(&hw_layer->priv_handle, priv_handle, sizeof(PrivateHandle));

    // label layer that is used by hwc
    layer->compositionType = HWC_OVERLAY;
    layer->hints |= HWC_HINT_CLEAR_FB;

    // check if any protect layer exist
    job->protect |= (priv_handle->usage & GRALLOC_USAGE_PROTECTED);

    // check if need to enable secure composition
    job->secure |= (priv_handle->usage & GRALLOC_USAGE_SECURE);

    // check if need to set video timestamp
    int buffer_type = (priv_handle->ext_info.status & GRALLOC_EXTRA_MASK_TYPE);
    if (buffer_type == GRALLOC_EXTRA_BIT_TYPE_VIDEO)
    {
        job->timestamp = priv_handle->ext_info.timestamp;
    }

    switch (type)
    {
        case HWC_LAYER_TYPE_UI:
        case HWC_LAYER_TYPE_DIM:
        case HWC_LAYER_TYPE_CURSOR:
            num_ui++;
            break;

        case HWC_LAYER_TYPE_MM_HIGH:
            // [WORKAROUND]`
            job->force_wait = true;
            hw_layer->type = HWC_LAYER_TYPE_MM;
        case HWC_LAYER_TYPE_MM:
            num_mm++;
            break;
    }
}

// listSecure() checks if there is any secure content in the display dist
static bool listSecure(hwc_display_contents_1_t* list)
{
    int usage;

    for (uint32_t i = 0; i < list->numHwLayers - 1; i++)
    {
        hwc_layer_1_t* layer = &list->hwLayers[i];

        if (layer->handle == NULL) continue;

        gralloc_extra_query(layer->handle, GRALLOC_EXTRA_GET_USAGE, &usage);

        if (usage & (GRALLOC_USAGE_PROTECTED | GRALLOC_USAGE_SECURE))
            return true;
    }

    return false;
}

#define first_loop_reset() \
    do { \
        /* overlap_limit reset */ \
        overlap_vector.clear(); \
        overlap_limit_reset = true; \
        /* bw_limit reset */ \
        total_layer_bytes = 0; \
        bw_limit_reset = true; \
        /* reset and select layers again */ \
        gles_head = -1; \
        i = -1; num_ui = 0; num_mm = 0; \
        memset(job->hw_layers, 0, sizeof(HWLayer) * job->num_layers); \
    } while(0)

static void handleOriginDisp(
    int dpy, hwc_display_contents_1_t* list, DispatcherJob* job, bool /*use_mirror_mode*/)
{
#ifdef MTK_HWC_PROFILING
    int total_fbt_bytes = 0;
    int num_fbt = 0;
#endif

    bool disp_dirty = false;

    int max_ovl_num = job->num_layers;

    int num_ui = 0;
    int num_mm = 0;

    bool secure = DisplayManager::getInstance().m_data[dpy].secure;
    bool mirror_disp = (list->flags & HWC_MIRROR_DISPLAY);

    bool bw_limit_reset = false;
    int bw_limit_bytes = DevicePlatform::getInstance().getBandwidthLimit();
    if (DevicePlatform::m_config.bytes_limit != 0)
    {
        bw_limit_bytes = DevicePlatform::m_config.bytes_limit;
        HWC_LOGW("Manual Limitation: %d", bw_limit_bytes);
    }

    // TODO: decouple mode judgement
    DISP_MODE session_mode = DispDevice::getInstance().getOverlaySessionMode(dpy);
    bool decouple = ((DISP_SESSION_DECOUPLE_MODE == session_mode) ||
                     (DISP_SESSION_DECOUPLE_MIRROR_MODE == session_mode));
    int ovl_overlap_limit = decouple ? -1 : DevicePlatform::m_config.ovl_overlap_limit;
    int forcing_gpu_idx = list->numHwLayers;
    int disp_top = 0;
    int disp_bottom = DisplayManager::getInstance().m_data[dpy].height;
    bool overlap_limit_reset = false;
    SortedVector< key_value_pair_t<int, int> > overlap_vector;

    // TODO: check bpp of display output format
    int disp_bytes = DisplayManager::getInstance().m_data[dpy].pixels * 4;
    int total_layer_bytes = 0;

    // [First Round]
    // scan all layer from first layer until
    // 1. only one ovl input is left and more than one layers are not verified
    // 2. one GLES layer is found as HWC_LAYER_TYPE_INVALID
    // for HWC 1.2, lastest layer is used by opengl
    int layers_num = list->numHwLayers - 1;
    int gles_head = -1;
    int ovl_check_idx = max_ovl_num - 1;
    int layer_check_idx = layers_num - 1;

    if (HWCMediator::getInstance().m_features.od)
        DevicePlatform::getInstance().controlOD(list);

    for (int i = 0; i < layers_num; i++)
    {
        PrivateHandle priv_handle;
        int dirty = (int)mirror_disp;
        int type = HWC_LAYER_TYPE_INVALID;

        hwc_layer_1_t* layer = &list->hwLayers[i];

        getTimestamp(layer->handle, &priv_handle);

        int curr_ovl_num = num_ui + num_mm;
        if (!((curr_ovl_num == ovl_check_idx) && (i < layer_check_idx)))
        {
            // get priv_handle and layerType
            type = layerType(dpy, &priv_handle, layer, dirty, secure);
            disp_dirty |= (dirty != 0);
        }

        if (ovl_overlap_limit > 0 && (layers_num > ovl_overlap_limit))
        {
            if (addOverlapVector(overlap_vector, layer, type, disp_bottom))
            {
                if (exceedOverlapLimit(overlap_vector, ovl_overlap_limit))
                {
                    if (!overlap_limit_reset)
                    {
                        HWC_LOGW("Exceed Overlap Limit1(%d), cur(%d)",
                            ovl_overlap_limit, i);
                        ovl_overlap_limit -= 1;

                        first_loop_reset();

                        continue;
                    }
                    else
                    {
                        subOverlapVector(overlap_vector, layer, type, disp_bottom);
                        type = HWC_LAYER_TYPE_INVALID;
                    }
                }
            }
        }

        if (bw_limit_bytes >= 0)
        {
            layerBytes(&priv_handle, layer, type);

            int roi_bytes = priv_handle.roi_bytes;
            int layer_bytes = (roi_bytes >= 0 && roi_bytes < disp_bytes) ? roi_bytes : disp_bytes;
            total_layer_bytes += layer_bytes;
            if (total_layer_bytes >= bw_limit_bytes)
            {
                if (!bw_limit_reset)
                {
                    HWC_LOGW("Exceed Bytes Limit: %d, %d", total_layer_bytes, bw_limit_bytes);
                    bw_limit_bytes -= disp_bytes;

                    first_loop_reset();

                    continue;
                }
                else
                {
                    total_layer_bytes -= layer_bytes;
                    type = HWC_LAYER_TYPE_INVALID;
                }
            }
        }

        if (HWC_LAYER_TYPE_INVALID == type)
        {
            gles_head = i;
            break;
        }

        layerSet(dpy, job, layer, &priv_handle, i, i,
                            type, dirty, num_ui, num_mm);
    }

    if (gles_head != -1)
    {
        // [Second Round]
        // scan unverified layer from latest layer until
        // 1. only one ovl input is left, we should use this for FBT
        // 2. one GLES layer is found as HWC_LAYER_TYPE_INVALID
        // 3. one MM layer is found as HWC_LAYER_TYPE_MM (this is a workaround)
        int j = ovl_check_idx;
        int gles_tail = gles_head;
        for (int i = layers_num - 1; i > gles_head; i--, j--)
        {
            PrivateHandle priv_handle;
            hwc_layer_1_t* layer = &list->hwLayers[i];

            getTimestamp(layer->handle, &priv_handle);

            int curr_ovl_num = num_ui + num_mm;
            if (curr_ovl_num == ovl_check_idx)
            {
                gles_tail = i;
                break;
            }

            int dirty = (int)mirror_disp;
            int type = layerType(dpy, &priv_handle, layer, dirty, secure);
            disp_dirty |= (dirty != 0);

            if ((HWC_LAYER_TYPE_INVALID == type) || (HWC_LAYER_TYPE_MM == type))
            {
                gles_tail = i;
                break;
            }

            if (ovl_overlap_limit > 0 && (layers_num > ovl_overlap_limit))
            {
                if (addOverlapVector(overlap_vector, layer, type, disp_bottom))
                {
                    if (exceedOverlapLimit(overlap_vector, ovl_overlap_limit))
                    {
                        HWC_LOGW("Exceed Overlap Limit2(%d), cur(%d)", ovl_overlap_limit, i);
                        gles_tail = i;
                        break;
                    }
                }
            }

            if (bw_limit_bytes >= 0)
            {
                layerBytes(&priv_handle, layer, type);

                int roi_bytes = priv_handle.roi_bytes;
                total_layer_bytes += roi_bytes;
                if (total_layer_bytes >= bw_limit_bytes)
                {
                    gles_tail = i;
                    break;
                }
            }

            layerSet(dpy, job, layer, &priv_handle, j,  i,
                                type, dirty, num_ui, num_mm);
        }

        // [Third Round]
        // fill hwc_layer_1_t structures for GLES layers
        // the GLES layers now are marked as below
        //   |   H     H     G     G     G     H     H        |
        //   |               ^ gles_head ^ gles_tail          |
        for (int i = gles_head; i <= gles_tail; i++)
        {
            hwc_layer_1_t* layer = &list->hwLayers[i];
            layer->compositionType = HWC_FRAMEBUFFER;
            layer->hints = 0;

            // check if need to set video timestamp
            PrivateHandle priv_handle;
            getTimestamp(layer->handle, &priv_handle);
            int buffer_type = (priv_handle.ext_info.status & GRALLOC_EXTRA_MASK_TYPE);
            if (buffer_type == GRALLOC_EXTRA_BIT_TYPE_VIDEO)
            {
                job->timestamp = priv_handle.ext_info.timestamp;
            }

#ifdef MTK_HWC_PROFILING
            getPrivateHandleInfo(layer->handle, priv_handle);
            total_fbt_bytes += layerBytes(&priv_handle, layer, HWC_LAYER_TYPE_UI);
            num_fbt++;
#endif
        }
    }

//#define MTK_HWC_OVERLAP_LIMIT_DEBUG
#ifdef MTK_HWC_OVERLAP_LIMIT_DEBUG
    {
#ifdef MTK_HWC_PROFILING
        HWC_LOGD("Overlap: start(%d), num_fbt(%d), total_fbt_bytes/FULL SCREEN = %f",
            gles_head, num_fbt, (float)total_fbt_bytes/disp_bytes);
#endif
        String8 temp;

        for(uint32_t k=0;k < overlap_vector.size();k++)
        {
            temp.appendFormat(" %d:%d,", overlap_vector[k].key, overlap_vector[k].value);
        }
        HWC_LOGD("Overlap: limit(%d), layers_num(%d), overlap_vector size(%d),[%s]",
            ovl_overlap_limit, layers_num, overlap_vector.size(), temp.string());
    }
#endif

    // set OVL idx for FBT
    // if only FBT in list, should also send it to OVL
    int fbt_hw_layer_idx = gles_head;
    if (list->numHwLayers == 1)
        fbt_hw_layer_idx = 0;

    if (fbt_hw_layer_idx != -1)
    {
        // set HWLayer for FBT
        hwc_layer_1_t* fbt_layer = &list->hwLayers[list->numHwLayers - 1];
        if (fbt_layer->compositionType == HWC_FRAMEBUFFER_TARGET)
        {
            HWLayer* fbt_hw_layer = &job->hw_layers[fbt_hw_layer_idx];
            fbt_hw_layer->enable  = true;
            fbt_hw_layer->index   = list->numHwLayers - 1;
            fbt_hw_layer->type    = HWC_LAYER_TYPE_FBT;

#ifdef MTK_HWC_PROFILING
            fbt_hw_layer->fbt_input_layers = num_fbt;
            fbt_hw_layer->fbt_input_bytes  = total_fbt_bytes;
#endif
        }
        else
        {
            HWC_LOGE("FRAMEBUFFER_TARGET is not last layer!! dpy(%d)", dpy);
            fbt_hw_layer_idx = -1;
        }
    }

    // prepare job in job group
    job->fbt_exist     = (fbt_hw_layer_idx != -1);
    job->num_ui_layers = num_ui;
    job->num_mm_layers = num_mm;
    job->disp_mir_id   = HWC_MIRROR_SOURCE_INVALID;
    job->disp_ori_rot  = (list->flags & HWC_ORIENTATION_MASK) >> 16;

    // [WORKAROUND]
    // No need to force wait since UI layer does not exist
    if (job->force_wait && !job->num_ui_layers)
        job->force_wait = false;

    // TODO: enable this after verification (e.g. layer count changes)
    // always recompose if
    // 1. fbt exists
    // 2. no video layer
    // 3. using mirror mode
    //if (job->fbt_exist || job->num_mm_layers == 0 || use_mirror_mode)
    {
        disp_dirty = true;
    }

    job->post_state = disp_dirty ? HWC_POST_INPUT_DIRTY : HWC_POST_INPUT_NOTDIRTY;

    // NOTE: enable this profiling to observe hwc recomposition
    if (DisplayManager::m_profile_level & PROFILE_TRIG)
    {
        char tag[16];
        snprintf(tag, sizeof(tag), "HWC_COMP_%1u", dpy);
        ATRACE_INT(tag, disp_dirty ? 1 : 0);
    }
}

static bool hasSkipedLayer(hwc_display_contents_1_t* list)
{
    for (uint32_t i = 0; i < list->numHwLayers - 1; i++)
    {
        hwc_layer_1_t* layer = &list->hwLayers[i];
        if (layer->flags & HWC_SKIP_LAYER)
        {
            return true;
        }
    }

    return false;
}

static void handleMirrorDisp(
    int /*dpy*/, hwc_display_contents_1_t* list, DispatcherJob* job)
{
    for (uint32_t i = 0; i < list->numHwLayers - 1; i++)
    {
        hwc_layer_1_t* layer = &list->hwLayers[i];
        layer->compositionType = HWC_OVERLAY;
        layer->hints |= HWC_HINT_CLEAR_FB;
    }

    // prepare job in job group
    job->fbt_exist     = false;
    job->num_ui_layers = 0;
    // [NOTE] treat mirror source as mm layer
    job->num_mm_layers = 1;
    job->hw_layers[0].enable = true;
    job->hw_layers[0].type = HWC_LAYER_TYPE_MM;
    job->disp_mir_id   = (list->flags & HWC_MIRRORED_DISP_MASK) >> 8;
    job->disp_ori_rot  = (list->flags & HWC_ORIENTATION_MASK) >> 16;

    HWCDispatcher::getInstance().configMirrorJob(job);
}

static void handleSkipDisplay(
    int /*dpy*/, hwc_display_contents_1_t* list, DispatcherJob* job)
{
    for (uint32_t i = 0; i < list->numHwLayers - 1; i++)
    {
        hwc_layer_1_t* layer = &list->hwLayers[i];
        layer->compositionType = HWC_OVERLAY;
        layer->hints |= HWC_HINT_CLEAR_FB;
    }
    job->fbt_exist     = false;
    job->num_ui_layers = 0;
    job->num_mm_layers = 0;
}

static void handleDisplayList(
    int dpy, hwc_display_contents_1_t* list, DispatcherJob* job, bool use_mirror_mode)
{
    if (list->flags & HWC_MIRROR_DISPLAY)
    {
        handleMirrorDisp(dpy, list, job);
    }
    else if (list->flags & HWC_SKIP_DISPLAY)
    {
        handleSkipDisplay(dpy, list, job);
    }
    else
    {
        handleOriginDisp(dpy, list, job, use_mirror_mode);
    }
}

// checkMirrorMode() checks if mirror mode exists
static bool checkMirrorMode(
    size_t num_display, hwc_display_contents_1_t** displays)
{
    const DisplayData* display_data = DisplayManager::getInstance().m_data;

    hwc_display_contents_1_t* list;

    for (uint32_t i = 0; i < num_display; i++)
    {
        if (DisplayManager::MAX_DISPLAYS <= i) break;

        list = displays[i];

        if (list == NULL || list->numHwLayers <= 0) continue;

        if ((DevicePlatform::m_config.mirror_state & MIRROR_DISABLED) ||
            (DevicePlatform::m_config.mirror_state & MIRROR_PAUSED))
        {
            // disable mirror mode
            // either the mirror state is disabled or the mirror source is blanked
            continue;
        }

        // assume that only primary panel can be mirrored by others
        if (i == HWC_DISPLAY_PRIMARY) continue;

        // the number of layer is different
        if (list->numHwLayers != displays[HWC_DISPLAY_PRIMARY]->numHwLayers) continue;

        // make sure all layer is the same with primary panel
        size_t j;
        for (j = 0; j < list->numHwLayers; j++)
        {
            hwc_layer_1_t* lp = &displays[HWC_DISPLAY_PRIMARY]->hwLayers[j];
            hwc_layer_1_t* lm = &list->hwLayers[j];
            if (lp->compositionType == HWC_SIDEBAND || lm->compositionType == HWC_SIDEBAND)
            {
                // HWC_SIDEBAND is set before prepare, so we have to check this composition type
                if (lp->compositionType != lm->compositionType) break;

                if (lp->sidebandStream != lm->sidebandStream) break;
            }
            else if (lp->handle != lm->handle && !(lp->compositionType == HWC_FRAMEBUFFER_TARGET &&
                     lm->compositionType == HWC_FRAMEBUFFER_TARGET))
            {
                break;
            }
        }
        if (j != list->numHwLayers)
        {
            continue;
        }

        if (HWC_DISPLAY_EXTERNAL == i &&
            display_data[i].subtype == HWC_DISPLAY_SMARTBOOK)
        {
            // disable mirror mode
            // since main display would be blanked for Smartbook
            continue;
        }

        if (HWC_DISPLAY_VIRTUAL == i)
        {
            PrivateHandle priv_handle;
            getPrivateHandleInfo(list->outbuf, &priv_handle);
            if (isTransparentFormat(priv_handle.format))
            {
                // disable mirror mode
                // if the sink still need alpha value
                // let overlay engine handle alpha channel directly
                continue;
            }
        }

        // display id of mirror source
        int mir_dpy = HWC_DISPLAY_PRIMARY;

        if (!display_data[i].secure && listSecure(displays[mir_dpy]))
        {
            // disable mirror mode
            // if any secure or protected layer exists in mirror source
            continue;
        }

        if (!HWCMediator::getInstance().m_features.copyvds &&
            list->numHwLayers <= 1)
        {
            // disable mirror mode
            // since force copy vds is not used
            continue;
        }

        // check enlargement ratio (i.e. scale ratio > 0)
        if (DevicePlatform::m_config.mir_scale_ratio > 0)
        {
            float scaled_ratio = display_data[i].pixels /
                static_cast<float>(display_data[mir_dpy].pixels);

            if (scaled_ratio > DevicePlatform::m_config.mir_scale_ratio)
            {
                // disable mirror mode
                // since scale ratio exceeds the maximum one
                continue;
            }
        }

        list->flags |= HWC_MIRROR_DISPLAY;
        list->flags |= mir_dpy << 8;

        return true;
    }

    return false;
}

int HWCMediator::prepare(size_t num_display, hwc_display_contents_1_t** displays)
{
    if (!num_display || NULL == displays)
    {
        HWC_LOGW("PRE/bypass/disp_num=%d/contents=%p", num_display, displays);
        return 0;
    }

    hwc_display_contents_1_t* list;

#ifdef MTK_HWC_PROFILING
    if (g_handle_all_layers)
    {
        g_prepare_all_layers = true;

        for (uint32_t i = 0; i < num_display; i++)
        {
            list = displays[i];
            if (list == NULL || list->numHwLayers <= 0) continue;

            for (uint32_t j = 0; j < list->numHwLayers - 1; j++)
            {
                hwc_layer_1_t* layer = &list->hwLayers[j];
                layer->compositionType = HWC_OVERLAY;
                layer->hints |= HWC_HINT_CLEAR_FB;
            }
        }

        return 0;
    }
#endif
    // clean all flags which was set by HWC
    for (uint32_t i = 0; i < num_display; i++)
    {
        list = displays[i];

        if (list == NULL) continue;

        if (list->flags & HWC_MIRROR_DISPLAY)
        {
            list->flags &= ~(HWC_MIRROR_DISPLAY | HWC_MIRRORED_DISP_MASK);
        }
    }

    // check if need to plug in/out virutal display
    if (HWCMediator::getInstance().m_features.virtuals)
    {
        DisplayManager::getInstance().hotplugVir(HWC_DISPLAY_VIRTUAL, displays[HWC_DISPLAY_VIRTUAL]);
    }

    // check if mirror mode exists
    bool use_mirror_mode = checkMirrorMode(num_display, displays);

    // set session mode
    HWCDispatcher::getInstance().setSessionMode(HWC_DISPLAY_PRIMARY, use_mirror_mode);

    for (uint32_t i = 0; i < num_display; i++)
    {
        if (DisplayManager::MAX_DISPLAYS <= i) break;

        list = displays[i];

        if (HWC_DISPLAY_VIRTUAL == i)
        {
            if (!m_features.virtuals)
            {
                HWC_LOGD("PRE/bypass/dpy=%d/novir", i);
                continue;
            }
        }

        if (list == NULL) continue;

        if (list->numHwLayers <= 0)
        {
            // do nothing if list is invalid
            HWC_LOGD("PRE/bypass/dpy=%d/list=%p", i, list);
            continue;
        }

        if (list->numHwLayers == 1)
        {
            if (HWC_DISPLAY_VIRTUAL == i)
            {
                if (!m_features.copyvds)
                {
                    // do nothing when virtual display with "fbt only"
                    HWC_LOGD("PRE/bypass/dpy=%d/num=0", i);
                    continue;
                }
            }
        }

        hwc_layer_1_t* fbt_layer = &list->hwLayers[list->numHwLayers - 1];
        bool is_fbt_changed = HWCDispatcher::getInstance().saveFbtHandle(i, fbt_layer->handle);
        if (list->flags & HWC_SKIP_DISPLAY)
        {
            if (is_fbt_changed)
            {
                HWC_LOGD("display_%d still updates FBT, clear the flag of skip display", i);
                list->flags &= ~HWC_SKIP_DISPLAY;
            }
            else if (hasSkipedLayer(list))
            {
                HWC_LOGD("display_%d has skip layer, clear the flag of skip display", i);
                list->flags &= ~HWC_SKIP_DISPLAY;
            }
            else if (HWCDispatcher::getInstance().m_disable_skip_redundant)
            {
                list->flags &= ~HWC_SKIP_DISPLAY;
            }
        }

        DispatcherJob* job = HWCDispatcher::getInstance().getJob(i);
        if (NULL != job)
        {
            handleDisplayList(i, list, job, use_mirror_mode);

            HWC_LOGD("PRE/dpy=%d/num=%d/max=%d/fbt=%d/ui=%d/mm=%d/mir=%d",
                i, list->numHwLayers - 1, job->num_layers, job->fbt_exist,
                job->num_ui_layers, job->num_mm_layers, job->disp_mir_id);
        }
        else
        {
            HWC_LOGW("PRE/dpy=%d/job=null !!", i);
        }
    }

    return 0;
}

int HWCMediator::set(size_t num_display, hwc_display_contents_1_t** displays)
{
    if (!num_display || NULL == displays)
    {
        HWC_LOGW("SET/bypass/disp_num=%d/contents=%p", num_display, displays);
        return 0;
    }

    hwc_display_contents_1_t* list;

#ifdef MTK_HWC_PROFILING
    if (g_handle_all_layers || g_prepare_all_layers)
    {
        HWCDispatcher::getInstance().onBlank(HWC_DISPLAY_PRIMARY, true);

        for (uint32_t i = 0; i < num_display; i++)
        {
            list = displays[i];
            if (list == NULL) continue;

            clearListAll(list);
        }

        g_prepare_all_layers = false;
        return 0;
    }
#endif

    for (uint32_t i = 0; i < num_display; i++)
    {
        if (DisplayManager::MAX_DISPLAYS <= i) break;

        list = displays[i];

        if (list == NULL) continue;

        if (list->numHwLayers <= 0)
        {
            HWC_LOGD("SET/bypass/dpy=%d/list=%p", i, list);
            continue;
        }

        if (HWC_DISPLAY_VIRTUAL == i)
        {
            if (list->numHwLayers == 1)
            {
                if (!m_features.copyvds)
                {
                    HWC_LOGD("SET/bypass/dpy=%d/num=0", i);
                    clearListFbt(list);
                    continue;
                }
            }
            else if (!m_features.virtuals)
            {
                HWC_LOGD("SET/bypass/dpy=%d/novir", i);
                clearListFbt(list);
                continue;
            }
        }

        HWCDispatcher::getInstance().setJob(i, list);
    }

    // trigger DispatchThread
    HWCDispatcher::getInstance().trigger();

    // clean all flags which was set by ourself
    for (uint32_t i = 0; i < num_display; i++)
    {
        list = displays[i];

        if (list == NULL) continue;

        if (list->flags & HWC_MIRROR_DISPLAY)
        {
            list->flags &= ~(HWC_MIRROR_DISPLAY | HWC_MIRRORED_DISP_MASK);
        }

        if (list->flags & HWC_SKIP_DISPLAY)
        {
            list->flags &= ~HWC_SKIP_DISPLAY;
        }
    }

    return 0;
}
