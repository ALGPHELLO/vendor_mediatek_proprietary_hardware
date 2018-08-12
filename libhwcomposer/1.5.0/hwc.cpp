#define DEBUG_LOG_TAG "HWC"

#include <cutils/properties.h>

#include "gralloc_mtk_defs.h"

#include "utils/debug.h"
#include "utils/tools.h"
#include "utils/devicenode.h"

#include "hwc.h"
#include "hwdev.h"
#include "platform.h"
#include "display.h"
#include "overlay.h"
#include "dispatcher.h"
#include "worker.h"
#include "composer.h"
#include "bliter.h"
#include "bliter_ultra.h"
#include "sync.h"
#include "service.h"

#include "Transform.h"
#include "ui/gralloc_extra.h"
#include "ui/Region.h"
#include <utils/SortedVector.h>
#include <utils/String8.h>
#include "cache.h"
#include "hrt.h"
#include <linux/disp_session.h>

// ---------------------------------------------------------------------------

#ifdef MTK_HWC_PROFILING
bool g_handle_all_layers = false;
bool g_prepare_all_layers = false;
#endif

#define MAX_MDPUI_NUM 1
// ---------------------------------------------------------------------------

struct ListInfo
{
    bool mirror_primary;
    bool has_limited_video;
    int num_video_layer;
};

struct ScenarioInfo
{
    bool mirror_mode;
    bool ultra_scenario;
    bool av_grouping;
};

// ---------------------------------------------------------------------------

ANDROID_SINGLETON_STATIC_INSTANCE(HWCMediator);

HWCMediator::HWCMediator()
{
    Debugger::getInstance();
    Debugger::getInstance().m_logger = new Debugger::LOGGER();

    // check features setting
    initFeatures();

    char value[PROPERTY_VALUE_MAX];

    // 0: All displays' jobs are dispatched when they are added into job queue
    // 1: Only external display's jobs are dispatched when external display's vsync is received
    // 2: external and wfd displays' jobs are dispatched when they receive VSync
    property_get("debug.hwc.trigger_by_vsync", value, "1");
    m_features.trigger_by_vsync = atoi(value);

    Platform::getInstance();

    sprintf(value, "%d", Platform::getInstance().m_config.compose_level);
    property_set("debug.hwc.compose_level", value);

    // check if virtual display could be composed by hwc
    status_t err = DispDevice::getInstance().createOverlaySession(HWC_DISPLAY_VIRTUAL);
    m_features.virtuals = (err == NO_ERROR);
    DispDevice::getInstance().destroyOverlaySession(HWC_DISPLAY_VIRTUAL);

    //check cache features caps and mode
    m_features.cache_caps = CacheBase::getInstance().getCacheCaps();

    m_features.cache_mode = HWC_FEATURES_CACHE_MODE_GPU_PASSIVE;
    if (!m_features.gmo)
    {
        // set needed buffer count for bufferqueue
        // gpu driver could use it to change the amounts of buffers with native window
        sprintf(value, "%d", Platform::getInstance().m_config.bq_count);
        property_set("debug.hwc.bq_count", value);
    }
    else
    {
        property_set("debug.hwc.bq_count", "3");
    }

    if (property_get("debug.hwc.async_bliter", value, NULL) > 0)
    {
        Platform::getInstance().m_config.use_async_bliter = atoi(value);
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
            if (HWC_DISPLAY_PRIMARY == dpy && enabled &&
                device && device->procs && device->procs->vsync)
            {
                device->procs->vsync(device->procs, dpy, timestamp);
            }

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
            "  ext=%d vir=%d enhance=%d svp=%d s3d=%d s3d_debug=%d s3d_depth=%d aod=%d\n",
            m_features.externals,
            m_features.virtuals,
            m_features.enhance,
            m_features.svp,
            m_features.hdmi_s3d,
            m_features.hdmi_s3d_debug,
            m_features.hdmi_s3d_depth,
            m_features.aod);

    DisplayManager::getInstance().dump(&log);

    char value[PROPERTY_VALUE_MAX];
    char default_value[PROPERTY_VALUE_MAX];

    // force full invalidate
    if (property_get("debug.hwc.forceFullInvalidate", value, NULL) > 0)
    {
        Platform::getInstance().m_config.force_full_invalidate = (0 != atoi(value));
    }

    // force hwc to wait fence for display
    if (property_get("debug.hwc.waitFenceForDisplay", value, NULL) > 0)
    {
        Platform::getInstance().m_config.wait_fence_for_display = (0 != atoi(value));
    }

    // check compose level
    if (property_get("debug.hwc.compose_level", value, NULL) > 0)
    {
        Platform::getInstance().m_config.compose_level = atoi(value);
    }

    // enableAsyncBltUltra
    if (property_get("debug.hwc.enableUBL", value, NULL) > 0)
    {
        Platform::getInstance().m_config.use_async_bliter_ultra = (0 != atoi(value));
    }

    // switch AsyncBltUltraDebug
    if (property_get("debug.hwc.debugUBL", value, NULL) > 0)
    {
        UltraBliter::getInstance().debug(0 != atoi(value));
    }

    // check UI's ability to process pre-transformed buffer
    if (property_get("debug.hwc.prexformUI", value, NULL) > 0)
    {
        Platform::getInstance().m_config.prexformUI = atoi(value);
    }

    // set 0, if you want all log to be shown
    if (property_get("debug.hwc.skip_log", value, NULL) > 0)
    {
        Debugger::m_skip_log = atoi(value);
    }

    // check mirror state
    if (property_get("debug.hwc.mirror_state", value, NULL) > 0)
    {
        Platform::getInstance().m_config.mirror_state = atoi(value);
    }

    dump_printf(&log, "\n[HWC Platform Config]\n"
        "  plat=%x comp=%d\n",
        Platform::getInstance().m_config.platform,
        Platform::getInstance().m_config.compose_level);

    HitLayerList current_hit_list = CacheBase::getInstance().getCurrentHitLayers();
    int current_list_size = current_hit_list.current_layer_vector.size();
    dump_printf(&log, "\n[HWC Hit Info]\n"
        "  cache_caps=%d cache_mode=%d gles_count = %d\n",
        m_features.cache_caps,
        m_features.cache_mode,
        current_hit_list.gles_layer_count);
    for (int i = 0; i < current_list_size ; i++)
    {
        HitCompare hit_layer_info = current_hit_list.current_layer_vector[i];
        dump_printf(&log, "  handle=%x\n", hit_layer_info.handle);
    }

    if (property_get("debug.hwc.cache_caps", value, NULL) > 0)
    {
        m_features.cache_caps = atoi(value);
    }
    if (property_get("debug.hwc.cache_mode", value, NULL) > 0)
    {
        m_features.cache_mode = atoi(value);
    }

    // dynamic change mir format for mhl
    if (property_get("debug.hwc.mhl_output", value, NULL) > 0)
    {
        Platform::getInstance().m_config.format_mir_mhl = atoi(value);
    }

    // check profile level
    if (property_get("debug.hwc.profile_level", value, NULL) > 0)
    {
        DisplayManager::m_profile_level = atoi(value);
    }

    // check the maximum scale ratio of mirror source
    property_get("debug.hwc.mir_scale_ratio", value, "0");

    float scale_ratio = strtof(value, NULL);
    Platform::getInstance().m_config.mir_scale_ratio =
        (scale_ratio != 0 && errno != ERANGE) ? scale_ratio : 0;

    // check debug setting of ovl_overlap_limit
    if (property_get("debug.hwc.ovl_overlap_limit", value, NULL) > 0)
    {
        Platform::getInstance().m_config.ovl_overlap_limit = atoi(value);
    }

    // check dump level
    property_get("debug.hwc.dump_level", value, "0");
    int dump_level = atoi(value);

    HWCDispatcher::getInstance().dump(&log, (dump_level & (DUMP_MM | DUMP_SYNC)));

    property_get("persist.debug.hwc.log", value, "0");
    if (value[0] != '0')
        Debugger::getInstance().setLogThreshold(value[0]);

    Debugger::getInstance().dump(&log);

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

    // for S3D feature property
    if (property_get("debug.hwc.hdmi_s3d", value, NULL) > 0)
    {
        m_features.hdmi_s3d = atoi(value);
    }

    if (property_get("debug.hwc.hdmi_s3d_depth", value, NULL) > 0)
    {
        int depth_value = atoi(value);
        if (depth_value <= 100 && depth_value >= -100)
        {
            m_features.hdmi_s3d_depth = depth_value;
        }
    }

    if (property_get("debug.hwc.hdmi_s3d_debug", value, NULL) > 0)
    {
        m_features.hdmi_s3d_debug = atoi(value);
    }

    if (property_get("debug.hwc.smartlayer", value, NULL) > 0)
    {
        Platform::getInstance().m_config.enable_smart_layer = atoi(value);
    }

    if (property_get("debug.hwc.rgba_rotate", value, NULL) > 0)
    {
        Platform::getInstance().m_config.enable_rgba_rotate = atoi(value);
    }

    // 0: All displays' jobs are dispatched when they are added into job queue
    // 1: Only external display's jobs are dispatched when external display's vsync is received
    // 2: external and wfd displays' jobs are dispatched when they receive VSync
    if (property_get("debug.hwc.trigger_by_vsync", value, NULL) > 0)
    {
        m_features.trigger_by_vsync = atoi(value);
    }

    if (property_get("debug.hwc.fbt_bound", value, NULL) > 0)
    {
        m_features.fbt_bound = atoi(value);
    }

#ifdef MTK_HWC_PROFILING
    if (property_get("debug.hwc.profile_handle_all", value, NULL) > 0)
    {
        g_handle_all_layers = (atoi(value) != 0);
    }
#endif

    if (property_get("debug.hwc.disable.skip.display", value, NULL) > 0)
    {
        HWCDispatcher::getInstance().m_disable_skip_redundant = atoi(value) ? true : false;
    }

    // force full invalidate
    if (property_get("debug.hwc.mirrorlegacy", value, NULL) > 0)
    {
        m_features.legacy_mirror_rule = atoi(value);
    }

    // OvlInSizeArbitrator
    if (DispDevice::getInstance().isDispRszSupported())
    {
        OvlInSizeArbitrator::getInstance().dump(&log);
    }

    // disable merge MDP and display
    if (property_get("debug.hwc.merge_md", value, NULL) > 0)
    {
        m_features.merge_mdp_display = atoi(value);
    }
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

        case HWC_VIEWPORT_HINT:
            if (DispDevice::getInstance().isDispRszSupported())
            {
                OvlInSizeArbitrator::getInstance().config(value[0], value[1]);
            }
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
    DisplayManager::getInstance().setDisplayPowerState(dpy, mode);

    // the device state is resume, so we can pause the wakelock timer
    if (mode != HWC_POWER_MODE_OFF)
    {
        DisplayManager::getInstance().setWakelockTimerState(DisplayManager::WAKELOCK_TIMER_PAUSE);
    }

    HWCDispatcher::getInstance().setPowerMode(dpy, mode);

    DisplayManager::getInstance().setPowerMode(dpy, mode);

    // disable mirror mode when display blanks
    if ((Platform::getInstance().m_config.mirror_state & MIRROR_DISABLED) != MIRROR_DISABLED)
    {
        if (mode == HWC_POWER_MODE_OFF || HWC_POWER_MODE_DOZE_SUSPEND == mode)
        {
            Platform::getInstance().m_config.mirror_state |= MIRROR_PAUSED;
        }
        else
        {
            Platform::getInstance().m_config.mirror_state &= ~MIRROR_PAUSED;
        }
    }

    // all display are off, so we have to start wakelock timer to delay that device enter suspend
    // because ePaper may have some frame need to show, we need to keep the bus is power on
    if (DisplayManager::getInstance().isAllDisplayOff())
    {
        DisplayManager::getInstance().setWakelockTimerState(DisplayManager::WAKELOCK_TIMER_START);
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
            case HWC_DISPLAY_COLOR_TRANSFORM:
                values[i] = 0;
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

#define setLineNum(RTLINE, TYPE) ({ \
                            RTLINE = __LINE__; \
                            TYPE; \
                        })

inline int layerType(
    int dpy, PrivateHandle* priv_handle, hwc_layer_1_t* layer,
    int& dirty, bool secure, int& rt_line, int& cal_mdpui_num, const int& layer_index)
{
    int compose_level = Platform::getInstance().m_config.compose_level;
    bool check_dim_layer = (Platform::getInstance().m_config.overlay_cap & OVL_CAP_DIM);

    if (check_dim_layer && (layer->flags & HWC_DIM_LAYER))
    {
        priv_handle->format = HAL_PIXEL_FORMAT_DIM;
        if (compose_level & COMPOSE_DISABLE_UI)
            return setLineNum(rt_line, HWC_LAYER_TYPE_INVALID);

        if (Transform::ROT_INVALID & layer->transform)
            return setLineNum(rt_line, HWC_LAYER_TYPE_INVALID);

        if (getDstWidth(layer) <= 0 || getDstHeight(layer) <= 0)
            return setLineNum(rt_line, HWC_LAYER_TYPE_INVALID);
    }

    // HWC is disabled or some other reasons
    if (layer->flags & HWC_SKIP_LAYER)
    {
        if (!layer->handle)
            return setLineNum(rt_line, HWC_LAYER_TYPE_INVALID);

        getPrivateHandleInfo(layer->handle, priv_handle);

        return setLineNum(rt_line, HWC_LAYER_TYPE_INVALID);
    }

    {
        DisplayData& disp_data = DisplayManager::getInstance().m_data[dpy];
        Rect dpy_size = Rect(disp_data.width, disp_data.height);
        Rect& displayFrame = *(Rect *)&(layer->displayFrame);
        Rect dst_crop(displayFrame);
        displayFrame.intersect(dpy_size, &dst_crop);
        if (displayFrame != dst_crop)
        {
            ++Debugger::getInstance().statistics_displayFrame_over_range;
            return setLineNum(rt_line, HWC_LAYER_TYPE_INVALID);
        }
    }

    if (check_dim_layer && (layer->flags & HWC_DIM_LAYER))
    {
        return setLineNum(rt_line, HWC_LAYER_TYPE_DIM);
    }

    if (!layer->handle)
        return setLineNum(rt_line, HWC_LAYER_TYPE_INVALID);

    getPrivateHandleInfo(layer->handle, priv_handle);

    if (dpy == HWC_DISPLAY_PRIMARY)
    {
        // ext_info is for primary display only
        dirty = layerDirty(priv_handle, layer, dirty);
    }
    else
    {
        // There are no ext_info to judge dirty layer for second display, always set dirty.
        dirty = HWC_LAYER_DIRTY_BUFFER | HWC_LAYER_DIRTY_PARAM;
    }

    int buffer_type = (priv_handle->ext_info.status & GRALLOC_EXTRA_MASK_TYPE);

    if (priv_handle->usage & GRALLOC_USAGE_PROTECTED && !secure)
        return setLineNum(rt_line, HWC_LAYER_TYPE_INVALID);

    if ((priv_handle->usage & GRALLOC_USAGE_SECURE))
    {
        // for MM case
        if ((buffer_type == GRALLOC_EXTRA_BIT_TYPE_VIDEO ||
            buffer_type == GRALLOC_EXTRA_BIT_TYPE_CAMERA ||
            priv_handle->format == HAL_PIXEL_FORMAT_YV12) &&
            DisplayManager::getInstance().getVideoHdcp() >
            DisplayManager::getInstance().m_data[dpy].hdcp_version)
        {
            return setLineNum(rt_line, HWC_LAYER_TYPE_INVALID);
        }
        else
        {
            // for UI case
            if (DisplayManager::getInstance().m_data[dpy].hdcp_version > 0)
                return setLineNum(rt_line, HWC_LAYER_TYPE_INVALID);
        }
    }

    // any layers above layer handled by fbt should also be handled by fbt
    if (compose_level & COMPOSE_DISABLE_UI)
        return setLineNum(rt_line, HWC_LAYER_TYPE_INVALID);

    // check platform capability
    if (Platform::getInstance().isUILayerValid(dpy, layer, priv_handle))
    {
        // always treat CPU layer as dirty
        if (buffer_type != GRALLOC_EXTRA_BIT_TYPE_GPU)
            dirty |= HWC_LAYER_DIRTY_BUFFER;

        dirty &= ~HWC_LAYER_DIRTY_PARAM;

        if (layer->flags & HWC_IS_CURSOR_LAYER)
        {
            return setLineNum(rt_line, HWC_LAYER_TYPE_CURSOR);
        }

        return setLineNum(rt_line, HWC_LAYER_TYPE_UI);
    }

    if ((buffer_type == GRALLOC_EXTRA_BIT_TYPE_VIDEO ||
        buffer_type == GRALLOC_EXTRA_BIT_TYPE_CAMERA ||
        priv_handle->format == HAL_PIXEL_FORMAT_YV12 ||
        Platform::getInstance().m_config.enable_rgba_rotate) &&
        cal_mdpui_num < MAX_MDPUI_NUM)
    {
        if (compose_level & COMPOSE_DISABLE_MM)
            return setLineNum(rt_line, HWC_LAYER_TYPE_INVALID);

        // check platform capability
        bool is_high = false;
        if (Platform::getInstance().isMMLayerValid(dpy, layer, priv_handle, is_high))
        {
            // always treat third party app queued yuv buffer as dirty
            if (buffer_type != GRALLOC_EXTRA_BIT_TYPE_VIDEO &&
                buffer_type != GRALLOC_EXTRA_BIT_TYPE_CAMERA)
            {
                if (MAX_MDPUI_NUM <= cal_mdpui_num)
                    return setLineNum(rt_line, HWC_LAYER_TYPE_INVALID);

                if (!CacheBase::getInstance().mdpuiHit(dpy, layer_index, layer))
                    dirty |= HWC_LAYER_DIRTY_BUFFER;

                cal_mdpui_num++;
            }

            if (buffer_type == GRALLOC_EXTRA_BIT_TYPE_CAMERA)
                dirty |= HWC_LAYER_DIRTY_CAMERA;

            // [WORKAROUND]
            if (is_high) return setLineNum(rt_line, HWC_LAYER_TYPE_MM_HIGH);

            return setLineNum(rt_line, HWC_LAYER_TYPE_MM);
        }
        else
        {
            return setLineNum(rt_line, HWC_LAYER_TYPE_INVALID);
        }
    }
    else
    {
        return setLineNum(rt_line, HWC_LAYER_TYPE_INVALID);
    }
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
                if (!(Platform::getInstance().m_config.overlay_cap & OVL_CAP_DIM_HW))
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
    int ovl_idx, int layer_idx, int ext_sel_layer, int type, int dirty, int& num_ui, int& num_mm)
{
    HWC_ATRACE_FORMAT_NAME("HWC(h:%p)", layer->handle);

    HWLayer* hw_layer = &job->hw_layers[ovl_idx];
    dirty |= HWCDispatcher::getInstance().verifyType(
                  dpy, priv_handle, ovl_idx, dirty, type);

    hw_layer->enable  = true;
    hw_layer->index   = layer_idx;
    hw_layer->type    = type;
    hw_layer->dirty   = (dirty != HWC_LAYER_DIRTY_NONE && dirty != HWC_LAYER_DIRTY_CAMERA);
    hw_layer->ext_sel_layer = Platform::getInstance().m_config.enable_smart_layer ? ext_sel_layer : -1;

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
            hw_layer->mdp_dst_roi = job->layer_info.layer_info_list[layer_idx].mdp_dst_roi;
            if (DispDevice::getInstance().isDispRszSupported() && (HWC_DISPLAY_PRIMARY == dpy))
            {
                hw_layer->dirty |= OvlInSizeArbitrator::getInstance().isConfigurationDirty();
            }
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

static bool listS3d(hwc_display_contents_1_t* list, DispatcherJob* job)
{
    if (NULL == list)
    {
        return false;
    }

    if (!HWCMediator::getInstance().m_features.hdmi_s3d)
    {
        return false;
    }

    for (uint32_t i = 0; i < list->numHwLayers - 1; i++)
    {
        PrivateHandle priv_handle;
        hwc_layer_1_t* layer = &list->hwLayers[i];

        if (layer->handle == NULL) continue;

        getPrivateHandleInfo(layer->handle, &priv_handle);

        int bit_S3D = (priv_handle.ext_info.status & GRALLOC_EXTRA_MASK_S3D);

        if (bit_S3D == GRALLOC_EXTRA_BIT_S3D_SBS || bit_S3D == GRALLOC_EXTRA_BIT_S3D_TAB)
        {
            bool is_sbs = (bit_S3D == GRALLOC_EXTRA_BIT_S3D_SBS) ? true : false ;
            bool is_tab = (bit_S3D == GRALLOC_EXTRA_BIT_S3D_TAB) ? true : false ;


            if (job == NULL)
            {
                return true;
            }

            layer->flags |= HWC_IS_S3D_LAYER;
            if (is_sbs)
            {
                layer->flags |= HWC_IS_S3D_LAYER_SBS;
                job->s3d_type = HWC_IS_S3D_LAYER_SBS;
            }
            else if (is_tab)
            {
                layer->flags |= HWC_IS_S3D_LAYER_TAB;
                job->s3d_type = HWC_IS_S3D_LAYER_TAB;
            }
            else
            {
                job->s3d_type = 0;
            }

            HWC_LOGI("[S3D] hasS3Dbit=%d isSBS=%d isTAB=%d status=%08x",
                bit_S3D, is_sbs, is_tab,
                priv_handle.ext_info.status);

            return true;
        }
    }

    return false;
}

inline void setupHwcLayers(int dpy, DispatcherJob* job, hwc_display_contents_1_t* list)
{
    int gles_head = job->layer_info.gles_head;
    int gles_tail = job->layer_info.gles_tail;
    int layers_num = list->numHwLayers - 1;

    int num_ui = 0;
    int num_mm = 0;

    // ovl_id is that the driver expects HWC assigns the layer to the "N-th" OVL input layer
    int ovl_id = -1;

    int ovl_index = 0;
    for (int i = 0; i < layers_num; i++, ovl_index++)
    {
        if (i == gles_head)
        {
            // skip ovl port for GLES using
            i = gles_tail;
        }
        else
        {
            int ext_sel_layer = -1;
            if (job->layer_info.layer_config_list != NULL)
            {
                ovl_id = job->layer_info.layer_config_list[i].ovl_id;
                ext_sel_layer = job->layer_info.layer_config_list[i].ext_sel_layer;
            }
            ovl_index = (ovl_id < 0) ? ovl_index : ovl_id;
            hwc_layer_1_t* layer = &list->hwLayers[i];
            LayerInfo* layer_info = &job->layer_info.layer_info_list[i];
            layerSet(dpy, job, layer, &layer_info->priv_handle, ovl_index, i, ext_sel_layer,
                       layer_info->type, layer_info->dirty, num_ui, num_mm);
        }
    }

    job->num_ui_layers = num_ui;
    job->num_mm_layers = num_mm;
}

static void setupGlesLayers(int dpy, DispatcherJob* job, hwc_display_contents_1_t* list)
{
#ifdef MTK_HWC_PROFILING
    int total_fbt_bytes = 0;
    int num_fbt = 0;
#endif
    HWC_ATRACE_FORMAT_NAME("BeginGLES");

    LayerInfo* layer_info = job->layer_info.layer_info_list;
    int gles_head = job->layer_info.gles_head;
    int gles_tail = job->layer_info.gles_tail;

    // ovl_id is that the driver expects HWC assigns the layer to the "N-th" OVL input layer
    int ovl_id = -1;
    Rect fbt_roi;

    if (gles_head != -1)
    {
        // To check whether this composition could apply cache algorithm or not.
        // Furthermore, the composition type of layer list are set according to the cache algorithm.
        bool is_fbt_hit = false;
        if (dpy == HWC_DISPLAY_PRIMARY)
        {
            is_fbt_hit = CacheBase::getInstance().layerHit(list, gles_head, gles_tail);
        }

        job->hit_info.is_hit = is_fbt_hit;
        if (is_fbt_hit)
        {
            job->hit_info.hit_layer_head = gles_head;
            job->hit_info.hit_layer_tail = gles_tail;
        }

        LayerList layer_list;
        for (int i = gles_head; i <= gles_tail; i++)
        {
            const hwc_layer_1_t* layer = &list->hwLayers[i];
            sp<LayerInfo> info = new LayerInfo(layer);
            if (info == NULL)
            {
                HWC_LOGD("DispLayerInfo is null, index=%d", i);
                break;
            }
            layer_list.add(info);
        }
        fbt_roi = layer_list.getBounds();
        // [Third Round]
        // fill hwc_layer_1_t structures for GLES layers
        // the GLES layers now are marked as below
        //   |   H     H     G     G     G     H     H        |
        //   |               ^ gles_head ^ gles_tail          |
        if (!is_fbt_hit)
        {
            for (int i = gles_head; i <= gles_tail; i++)
            {
                hwc_layer_1_t* layer = &list->hwLayers[i];
                layer->compositionType = HWC_FRAMEBUFFER;
                layer->hints = 0;
                HWC_ATRACE_FORMAT_NAME("GLES(h:%p)", layer->handle);

                // check if need to set video timestamp
                PrivateHandle* priv_handle = &layer_info[i].priv_handle;
                int buffer_type = (priv_handle->ext_info.status & GRALLOC_EXTRA_MASK_TYPE);
                if (buffer_type == GRALLOC_EXTRA_BIT_TYPE_VIDEO)
                {
                    job->timestamp = priv_handle->ext_info.timestamp;
                }

#ifdef MTK_HWC_PROFILING
                getPrivateHandleInfo(layer->handle, priv_handle);
                total_fbt_bytes += layerBytes(&priv_handle, layer, HWC_LAYER_TYPE_UI);
                num_fbt++;
#endif
            }
        }
    }

#ifdef MTK_HWC_PROFILING
    int disp_bytes = DisplayManager::getInstance().m_data[dpy].pixels * 4;
    HWC_LOGD("Overlap: start(%d), num_fbt(%d), total_fbt_bytes/FULL SCREEN = %f",
                            gles_head, num_fbt, (float)total_fbt_bytes/disp_bytes);
#endif

    // set OVL idx for FBT
    // if only FBT in list, should also send it to OVL
    if (job->layer_info.layer_config_list != NULL && gles_head != -1)
    {
        ovl_id = job->layer_info.layer_config_list[gles_head].ovl_id;
    }

    int fbt_hw_layer_idx = (ovl_id < 0) ? gles_head : ovl_id;
    if (list->numHwLayers == 1)
        fbt_hw_layer_idx = 0;

    if (fbt_hw_layer_idx != -1)
    {
        // set HWLayer for FBT
        hwc_layer_1_t* fbt_layer = &list->hwLayers[list->numHwLayers - 1];
        if (fbt_layer->compositionType == HWC_FRAMEBUFFER_TARGET)
        {
            if (!fbt_roi.isEmpty() && HWCMediator::getInstance().m_features.fbt_bound && !job->has_s3d_layer)
            {
                fbt_layer->displayFrame.left = fbt_roi.left;
                fbt_layer->displayFrame.right = fbt_roi.right;
                fbt_layer->displayFrame.top = fbt_roi.top;
                fbt_layer->displayFrame.bottom = fbt_roi.bottom;
                fbt_layer->sourceCropf.left = fbt_roi.left;
                fbt_layer->sourceCropf.right = fbt_roi.right;
                fbt_layer->sourceCropf.top = fbt_roi.top;
                fbt_layer->sourceCropf.bottom = fbt_roi.bottom;
            }

            HWLayer* fbt_hw_layer = &job->hw_layers[fbt_hw_layer_idx];
            HWC_ATRACE_FORMAT_NAME("FBT(h:%p)", fbt_layer->handle);
            fbt_hw_layer->enable  = true;
            fbt_hw_layer->index   = list->numHwLayers - 1;
            // sw tcon is too slow, so we want to use MDP to backup this framebuffer.
            // then SurfaceFlinger can use this framebuffer immediately.
            // therefore change its composition type to mm
            if (DisplayManager::getInstance().m_data[dpy].subtype == HWC_DISPLAY_EPAPER)
            {
                fbt_hw_layer->type = HWC_LAYER_TYPE_MM_FBT;
                job->mm_fbt = true;
            }
            else
            {
                fbt_hw_layer->type = HWC_LAYER_TYPE_FBT;
            }
            fbt_hw_layer->dirty   = HWCDispatcher::getInstance().verifyFbtType(
                                                                    dpy, fbt_hw_layer_idx);
            fbt_hw_layer->ext_sel_layer = -1;
#ifdef MTK_HWC_PROFILING
            fbt_hw_layer->fbt_input_layers = num_fbt;
            fbt_hw_layer->fbt_input_bytes  = total_fbt_bytes;
#endif
        }
        else
        {
            HWC_LOGE("FRAMEBUFFER_TARGET is not last layer!! dpy(%d) gles[%d,%d:%d,%d] ovlid(%d)", dpy,
                job->layer_info.hwc_gles_head,
                job->layer_info.hwc_gles_tail,
                job->layer_info.gles_head,
                job->layer_info.gles_tail,
                ovl_id);
            fbt_hw_layer_idx = -1;
        }
    }

    job->fbt_exist = (fbt_hw_layer_idx != -1);
}

inline void getLayerInfoList(int dpy, DispatcherJob* job, hwc_display_contents_1_t* list)
{
    bool secure = DisplayManager::getInstance().m_data[dpy].secure;
    bool mirror_disp = (list->flags & HWC_MIRROR_DISPLAY);
    hwc_layer_1_t* layer_list = list->hwLayers;
    int layers_num = list->numHwLayers - 1;

    // TODO: set 3 roughly, need to refine
    static LayerInfo* layer_info_list[3] = { NULL, NULL, NULL};
    static int layer_info_list_len[3] = { 0, 0, 0};

    // reallocate layer_list if it's needed
    if (layers_num > layer_info_list_len[dpy])
    {
        if (NULL != layer_info_list[dpy])
            free(layer_info_list[dpy]);

        layer_info_list_len[dpy] = layers_num;
        layer_info_list[dpy] = (LayerInfo*)malloc(layer_info_list_len[dpy] * sizeof(LayerInfo));
        if (NULL == layer_info_list[dpy])
        {
            layer_info_list_len[dpy] = 0;
            return;
        }
    }

    DbgLogger logger(DbgLogger::TYPE_DUMPSYS, 'I', "LayerInfo(%d): ", dpy);
    // init and get PrivateHandle
    LayerInfo* layer_info = layer_info_list[dpy];
    hwc_layer_1_t* layer = layer_list;
    int mdp_ui_num = 0;
    for (int i = 0; i < layers_num; i++, layer_info++, layer++)
    {
        static const PrivateHandle priv_handle_forInit;

        // reset handle
        PrivateHandle* priv_handle = &layer_info->priv_handle;
        memcpy(priv_handle, &priv_handle_forInit, sizeof(PrivateHandle));

        int rt_line;
        // get data for handle
        layer_info->layer_idx = i;
        layer_info->dirty = (int)mirror_disp;
        layer_info->type = layerType(dpy, priv_handle, layer, layer_info->dirty, secure, rt_line, mdp_ui_num, i);
        if (layer_info->type != HWC_LAYER_TYPE_INVALID && layer_info->type != HWC_LAYER_TYPE_DIM)
        {
            layer_info->bpp = getBitsPerPixel(priv_handle->format) / 8;
        }
        else if (layer_info->type == HWC_LAYER_TYPE_DIM)
        {
            layer_info->priv_handle.format = HAL_PIXEL_FORMAT_DIM;
        }

        if (layer_info->type == HWC_LAYER_TYPE_MM || layer_info->type == HWC_LAYER_TYPE_MM_HIGH)
        {
            Rect& mdp_dst_roi = layer_info->mdp_dst_roi;
            hwc_rect_t& displayFrame = layer->displayFrame;
            mdp_dst_roi.left = displayFrame.left;
            mdp_dst_roi.top = displayFrame.top;
            mdp_dst_roi.right = displayFrame.right;
            mdp_dst_roi.bottom = displayFrame.bottom;
        }

        job->layer_info.disp_dirty |= (layer_info->dirty != 0);

        logger.printf("(%d:%d,L%d) ", i, layer_info->type, rt_line);
    }

    if (0 == mdp_ui_num && 0 == dpy)
    {
        CacheBase::getInstance().clearBlitLayerCompare();
    }

    // TODO: enable this after verification (e.g. layer count changes)
    // always recompose if
    // 1. fbt exists
    // 2. no video layer
    // 3. using mirror mode
    //if (job->fbt_exist || job->num_mm_layers == 0 || use_mirror_mode)
    {
        job->layer_info.disp_dirty = true;
    }

    job->layer_info.layer_info_list = layer_info_list[dpy];
}

static void handleOriginDisp(
    int dpy, hwc_display_contents_1_t* list, DispatcherJob* job, bool /*use_mirror_mode*/)
{
    getLayerInfoList(dpy, job, list);

    HWC_ATRACE_FORMAT_NAME("BeginHWCPrepare");

    // [First Round]
    // scan all layer from first layer until
    // 1. only one ovl input is left and more than one layers are not verified
    // 2. one GLES layer is found as HWC_LAYER_TYPE_INVALID
    // for HWC 1.2, lastest layer is used by opengl
    int layers_num = list->numHwLayers - 1;
    int gles_head = -1;
    int gles_tail = -1;
    int ovl_check_idx = job->num_layers - 1;
    int layer_check_idx = layers_num - 1;

    int curr_ovl_num = 0;
    hwc_layer_1_t* layer_list = list->hwLayers;
    LayerInfo* layer_info_list =  job->layer_info.layer_info_list;

    bool force_gpu_compose = false;
    // force to all GPU compose for S3D case or ePaper case
    if (job->has_s3d_layer ||
        DisplayManager::getInstance().m_data[dpy].subtype == HWC_DISPLAY_EPAPER)
    {
        force_gpu_compose = true;
    }

    for (int i = 0; i < layers_num; i++)
    {
        hwc_layer_1_t* layer = &list->hwLayers[i];
        LayerInfo* layer_info = &layer_info_list[i];
        PrivateHandle *priv_handle = &layer_info->priv_handle;

        int type = HWC_LAYER_TYPE_INVALID;
        if (!((curr_ovl_num == ovl_check_idx) && (i < layer_check_idx)) &&
            !force_gpu_compose)
        {
            type = layer_info->type;

        }

        if (HWC_LAYER_TYPE_INVALID == type)
        {
            gles_head = i;
            break;
        }

        curr_ovl_num++;
    }

    if (gles_head != -1)
    {
        // [Second Round]
        // scan unverified layer from latest layer until
        // 1. only one ovl input is left, we should use this for FBT
        // 2. one GLES layer is found as HWC_LAYER_TYPE_INVALID
        // 3. one MM layer is found as HWC_LAYER_TYPE_MM (this is a workaround)
        int j = ovl_check_idx;
        gles_tail = gles_head;
        for (int i = layers_num - 1; i > gles_head; i--, j--)
        {
            hwc_layer_1_t* layer = &list->hwLayers[i];
            LayerInfo* layer_info = &layer_info_list[i];
            PrivateHandle* priv_handle = &layer_info->priv_handle;

            if (curr_ovl_num == ovl_check_idx)
            {
                gles_tail = i;
                break;
            }

            int type = layer_info->type;

            // force to all GPU compose for S3D case
            if (job->has_s3d_layer)
            {
                type = HWC_LAYER_TYPE_INVALID;
            }

            if ((HWC_LAYER_TYPE_INVALID == type) || (HWC_LAYER_TYPE_MM == type))
            {
                gles_tail = i;
                break;
            }

            curr_ovl_num++;
        }
    }

    job->layer_info.hwc_gles_head = gles_head;
    job->layer_info.hwc_gles_tail = gles_tail;
    job->layer_info.gles_head = gles_head;
    job->layer_info.gles_tail = gles_tail;
}

static inline void check_ultra_mdp(size_t num_display, hwc_display_contents_1_t** displays)
{
    DispatcherJob* main_job = HWCDispatcher::getInstance().getExistJob(0);
    if (NULL != main_job && main_job->enable)
    {
        for (uint32_t i = 0; i < main_job->num_layers; i++)
        {
            HWLayer* main_hw_layer = &main_job->hw_layers[i];
            if (HWC_LAYER_TYPE_MM != main_hw_layer->type) continue;
            if (!main_hw_layer->dirty) continue;

            uint32_t main_idx = main_hw_layer->index;
            buffer_handle_t main_mm_handle = displays[0]->hwLayers[main_idx].handle;

            bool is_ultra_mdp_hit = false;
            for (uint32_t j = 1; j < num_display; j++)
            {
                DispatcherJob* job = HWCDispatcher::getInstance().getExistJob(j);
                if (NULL != job && job->enable && HWC_MIRROR_SOURCE_INVALID == job->disp_mir_id)
                {
                    for (uint32_t k = 0; k < job->num_layers && !is_ultra_mdp_hit; k++)
                    {
                        HWLayer* hw_layer = &job->hw_layers[k];
                        if (HWC_LAYER_TYPE_MM != hw_layer->type) continue;
                        if (!hw_layer->dirty) continue;

                        uint32_t idx = hw_layer->index;
                        buffer_handle_t mm_handle = displays[j]->hwLayers[idx].handle;

                        if (main_mm_handle == mm_handle)
                        {
                            hw_layer->is_ultra_mdp = true;
                            is_ultra_mdp_hit = true;
                            HWC_LOGI("UBL hit (0,%d)-(%d,%d)", i, j, k);
                            break;
                        }
                    }
                }
            }
            if (is_ultra_mdp_hit)
            {
                main_hw_layer->is_ultra_mdp = true;
            }
        }
    }
}

static void finishDisplayList(size_t num_display, hwc_display_contents_1_t** displays)
{
    for (size_t i = 0; i < num_display; i++)
    {
        DispatcherJob* job = HWCDispatcher::getInstance().getExistJob(i);
        if (NULL != job)
        {
            hwc_display_contents_1_t* list = displays[i];

            HWCDispatcher::getInstance().resetCurrentHwLayerState(i);
            if (list->flags & HWC_MIRROR_DISPLAY)
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
                HWCDispatcher::getInstance().setCurrentHwLayerState(i, 0, true);
            }
            else
            {
                setupHwcLayers(i, job, list);
                setupGlesLayers(i, job, list);

                job->disp_mir_id    = HWC_MIRROR_SOURCE_INVALID;
                job->disp_ori_rot   = (list->flags & HWC_ORIENTATION_MASK) >> 16;
                job->post_state     = job->layer_info.disp_dirty ? HWC_POST_INPUT_DIRTY : HWC_POST_INPUT_NOTDIRTY;

                // [WORKAROUND]
                // No need to force wait since UI layer does not exist
                if (job->force_wait && !job->num_ui_layers)
                    job->force_wait = false;

                // NOTE: enable this profiling to observe hwc recomposition
                if (DisplayManager::m_profile_level & PROFILE_TRIG)
                {
                    char tag[16];
                    snprintf(tag, sizeof(tag), "HWC_COMP_%1u", i);
                    ATRACE_INT(tag, job->layer_info.disp_dirty ? 1 : 0);
                }

                // partial update - shoud invalidate full screen when HWC_GEOMETRY_CHANGED
                if (DispDevice::getInstance().isPartialUpdateSupported())
                {
                    job->is_full_invalidate = (0 != (list->flags & HWC_GEOMETRY_CHANGED));
                }
            }

            if (job->layer_info.max_overlap_layer_num == -1)
            {
                job->layer_info.max_overlap_layer_num = job->num_ui_layers
                                                      + job->num_mm_layers
                                                      + (job->fbt_exist ? 1 : 0);
            }

            DbgLogger* logger = Debugger::getInstance().m_logger->pre_info[i];
            logger->printf("(%d) PRE list=%d/max=%d/fbt=%d[%d,%d:%d,%d](%s)/ui=%d/mm=%d/ovlp=%d/flg=0x%x/fi=%d/mir=%d",
                i, list->numHwLayers - 1, job->num_layers,
                job->fbt_exist,job->layer_info.hwc_gles_head, job->layer_info.hwc_gles_tail, job->layer_info.gles_head, job->layer_info.gles_tail, job->mm_fbt ? "MM" : "OVL",
                job->num_ui_layers, job->num_mm_layers, job->layer_info.max_overlap_layer_num,
                list->flags, job->is_full_invalidate, job->disp_mir_id);


            logger->printf("/ext( ");
            for (int i = 0; (i < list->numHwLayers - 1); i++)
            {
                int ext_sel_layer = -1;
                if (job->layer_info.layer_config_list != NULL)
                {
                    ext_sel_layer = job->layer_info.layer_config_list[i].ext_sel_layer;
                }
                logger->printf("%d ", ext_sel_layer);
            }
            logger->printf(")");

            HWCDispatcher::getInstance().storeCurrentHwLayerState(i);
            logger->tryFlush();
        }
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

static bool isMirrorList(const hwc_display_contents_1_t *const *const displays,
                         uint32_t dpy,
                         uint32_t mirrored)
{
    bool res = false;
    DbgLogger logger(DbgLogger::TYPE_DUMPSYS, 'I', "mirror?(%u->%u): ", dpy, mirrored);

    if (dpy == mirrored)
    {
        logger.printf("E-same_dpy");
        return res;
    }

    const hwc_display_contents_1_t* target = displays[dpy];
    const hwc_display_contents_1_t* base = displays[mirrored];
    const hwc_layer_1_t* layer_target = NULL;
    const hwc_layer_1_t* layer_base = NULL;

    logger.printf("I-size(%zu|%zu) ", target->numHwLayers, base->numHwLayers);
    // Mirror path could be applied when handles of two layers are equal.
    for (int target_index = target->numHwLayers - 1; target_index >= 0; target_index--)
    {
        layer_target = &target->hwLayers[target_index];

        if(!layer_target->handle)
            continue;

        for (int base_index = base->numHwLayers - 1; base_index >= 0; base_index--)
        {
            layer_base = &base->hwLayers[base_index];
            if(!layer_base->handle)
                continue;

            if (layer_target->compositionType == HWC_SIDEBAND || layer_base->compositionType == HWC_SIDEBAND)
            {
                if ((layer_target->compositionType == layer_base->compositionType) ||
                    (layer_target->sidebandStream == layer_base->sidebandStream))
                {
                    res = true;
                    logger.printf("T1 ");
                    break;
                }
            }
            else if (layer_target->handle == layer_base->handle &&
                    !(layer_target->compositionType == HWC_FRAMEBUFFER_TARGET &&
                    layer_base->compositionType == HWC_FRAMEBUFFER_TARGET))
            {
                res = true;
                logger.printf("T2 ");
                break;
            }
        }

        if (res)
        {
            break;
        }
    }

    logger.printf("E-%d", res);
    return res;
}

static bool isMirrorListLegacy(const hwc_display_contents_1_t *const *const displays,
                         uint32_t dpy,
                         uint32_t mirrored)
{
    bool res = true;
    DbgLogger logger(DbgLogger::TYPE_DUMPSYS, 'I', "mirror?(%u->%u): ", dpy, mirrored);

    if (dpy == mirrored)
    {
        logger.printf("E-same_dpy");
        return res;
    }

    const hwc_display_contents_1_t* target = displays[dpy];
    const hwc_display_contents_1_t* base = displays[mirrored];
    size_t target_index = 0;
    size_t base_index = 0;

    logger.printf("I-size(%zu|%zu) ", target->numHwLayers, base->numHwLayers);
    while ((target_index < target->numHwLayers) && (base_index < base->numHwLayers))
    {
        const hwc_layer_1_t* lt = NULL;
        const hwc_layer_1_t* lb = NULL;
        // find a non zero size layer from target list
        for (; target_index < target->numHwLayers; target_index++)
        {
            lt = &target->hwLayers[target_index];
            if (SIZE(lt->displayFrame))
            {
                break;
            }
        }

        // find a non zero size layer from base list
        for (; base_index < base->numHwLayers; base_index++)
        {
            lb = &base->hwLayers[base_index];
            if (SIZE(lb->displayFrame))
            {
                break;
            }
        }

        logger.printf("C-(%zu|%zu):", target_index, base_index);
        if ((target_index >= target->numHwLayers) || (base_index >= base->numHwLayers))
        {
            // there is least a list which does not have non zero size layer,
            // so we have to check another one
            logger.printf("OOR ");
            break;
        }

        if (lt->compositionType == HWC_SIDEBAND || lb->compositionType == HWC_SIDEBAND)
        {
            if ((lt->compositionType != lb->compositionType) ||
                (lt->sidebandStream != lb->sidebandStream))
            {
                res = false;
                logger.printf("F1 ");
                break;
            }
        }
        else if (lt->handle != lb->handle &&
                 !(lt->compositionType == HWC_FRAMEBUFFER_TARGET &&
                 lb->compositionType == HWC_FRAMEBUFFER_TARGET))
        {
            res = false;
            logger.printf("F2 ");
            break;
        }

        logger.printf("S ");
        target_index++;
        base_index++;
    }

    logger.printf("E-%d", res);
    return res;
}

static void getListInfo(const hwc_display_contents_1_t *const *const displays,
                        uint32_t dpy,
                        struct ListInfo *info)
{
    info->has_limited_video = false;
    info->num_video_layer = 0;

    const hwc_display_contents_1_t* target = displays[dpy];

    for (size_t i = 0; i < target->numHwLayers; i++)
    {
        const hwc_layer_1_t* lm = &target->hwLayers[i];

        if (!info->has_limited_video)
        {
            if (lm->handle == NULL)
            {
                HWC_LOGI("A NULL handle at disp=%d layer=%d", dpy, i);
                continue;
            }

            PrivateHandle handle;
            getPrivateHandleInfo(lm->handle, &handle);
            int type = (handle.ext_info.status & GRALLOC_EXTRA_MASK_TYPE);
            size_t size = handle.width * handle.height;
            if (type == GRALLOC_EXTRA_BIT_TYPE_VIDEO)
            {
                info->num_video_layer++;
            }
            if ((type == GRALLOC_EXTRA_BIT_TYPE_VIDEO ||
                type == GRALLOC_EXTRA_BIT_TYPE_CAMERA ||
                handle.format == HAL_PIXEL_FORMAT_YV12) &&
                size >= Platform::getInstance().getLimitedVideoSize())
            {
                info->has_limited_video = true;
            }
        }
    }
    if (CC_LIKELY(1 == HWCMediator::getInstance().m_features.legacy_mirror_rule))
    {
        info->mirror_primary = isMirrorListLegacy(displays, dpy, HWC_DISPLAY_PRIMARY);
    }
    else
    {
        info->mirror_primary = isMirrorList(displays, dpy, HWC_DISPLAY_PRIMARY);
    }
}

static int isMultipleDisplayInList(const size_t& num_display, hwc_display_contents_1_t** displays)
{
    int count = 0;

    for (size_t i = 0; i < num_display; i++)
    {
        hwc_display_contents_1_t* list = displays[i];
        if (list) {
            count++;
        }
    }

    return count;
}

// checkScenatio() checks if mirror mode exists
static void checkScenario(
    size_t num_display, hwc_display_contents_1_t** displays, ScenarioInfo *scenario_info)
{
    DbgLogger logger(DbgLogger::TYPE_DUMPSYS, 'I', "chkMir(%d): ", (int)num_display);

    const DisplayData* display_data = DisplayManager::getInstance().m_data;
    int valid_display = isMultipleDisplayInList(num_display, displays);

    hwc_display_contents_1_t* list;

    for (uint32_t i = 0; i < num_display; i++)
    {
        if (DisplayManager::MAX_DISPLAYS <= i) break;

        list = displays[i];

        if (list == NULL || list->numHwLayers <= 0)
        {
            logger.printf("(%d:L%d) ", i, __LINE__);
            continue;
        }

        struct ListInfo info;
        memset(&info, 0, sizeof(info));
        getListInfo(displays, i, &info);

        //assume that only primary panel can be mirrored by others
        if (i == HWC_DISPLAY_PRIMARY)
        {
            if (info.num_video_layer == 1 && valid_display == 1)
            {
                scenario_info->av_grouping = true;
            }
            logger.printf("(%d:L%d) ", i, __LINE__);
            continue;
        }

        // display id of mirror source
        int mir_dpy = HWC_DISPLAY_PRIMARY;

        // if hdcp checking is handled by display driver, the extension path must be applied.
        if (Platform::getInstance().m_config.bypass_hdcp_checking &&
            listSecure(displays[mir_dpy]))
        {
            list->flags &= ~(HWC_MIRROR_DISPLAY);
            logger.printf("(%d:L%d) ", HWC_DISPLAY_PRIMARY, __LINE__);
            continue;
        }

        // 4k mhl has 4k video with mirror mode, so need to block 4k video at primary display
        if (i == HWC_DISPLAY_EXTERNAL && info.mirror_primary
                && DisplayManager::getInstance().isUltraDisplay(i) && info.has_limited_video)
        {
            scenario_info->ultra_scenario = true;
            logger.printf("(%d:L%d) ", i, __LINE__);
            continue;
        }

        if ((Platform::getInstance().m_config.mirror_state & MIRROR_DISABLED) ||
            (Platform::getInstance().m_config.mirror_state & MIRROR_PAUSED))
        {
            // disable mirror mode
            // either the mirror state is disabled or the mirror source is blanked
            list->flags &= ~(HWC_MIRROR_DISPLAY);
            logger.printf("(%d:L%d) ", i, __LINE__);
            continue;
        }

        // the layer list is different with primary display
        if (!info.mirror_primary)
        {
            logger.printf("(%d:L%d) ", i, __LINE__);
            continue;
        }

        if (HWC_DISPLAY_EXTERNAL == i &&
            display_data[i].subtype == HWC_DISPLAY_SMARTBOOK)
        {
            // disable mirror mode
            // since main display would be blanked for Smartbook
            list->flags &= ~(HWC_MIRROR_DISPLAY);
            logger.printf("(%d:L%d) ", i, __LINE__);
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
                list->flags &= ~(HWC_MIRROR_DISPLAY);
                logger.printf("(%d:L%d) ", i, __LINE__);
                continue;
            }
        }

        if (!display_data[i].secure && listSecure(displays[mir_dpy]))
        {
            // disable mirror mode
            // if any secure or protected layer exists in mirror source
            list->flags &= ~(HWC_MIRROR_DISPLAY);
            logger.printf("(%d:L%d) ", i, __LINE__);
            continue;
        }

        if (!display_data[i].secure && hasSkipedLayer(displays[i]))
        {
            // disable mirror mode
            // if any secure or protected layer exists in mirror source
            list->flags &= ~(HWC_MIRROR_DISPLAY);
            logger.printf("(%d:L%d) ", i, __LINE__);
            continue;
        }

        if (!HWCMediator::getInstance().m_features.copyvds &&
            list->numHwLayers <= 1)
        {
            // disable mirror mode
            // since force copy vds is not used
            list->flags &= ~(HWC_MIRROR_DISPLAY);
            logger.printf("(%d:L%d) ", i, __LINE__);
            continue;
        }

        // check enlargement ratio (i.e. scale ratio > 0)
        if (Platform::getInstance().m_config.mir_scale_ratio > 0)
        {
            float scaled_ratio = display_data[i].pixels /
               static_cast<float>(display_data[mir_dpy].pixels);

            if (scaled_ratio > Platform::getInstance().m_config.mir_scale_ratio)
            {
               // disable mirror mode
                // since scale ratio exceeds the maximum one
                list->flags &= ~(HWC_MIRROR_DISPLAY);
                logger.printf("(%d:L%d) ", i, __LINE__);
                continue;
            }
        }

       if (display_data[i].is_s3d_support && HWC_DISPLAY_EXTERNAL == i)
       {
           // job didn't be created , NULL value is OK
           if (listS3d(list, NULL))
           {
               list->flags &= ~(HWC_MIRROR_DISPLAY);
               continue;
           }
       }

        list->flags |= HWC_MIRROR_DISPLAY;
        list->flags |= mir_dpy << 8;
        logger.printf("mir");
        scenario_info->mirror_mode = true;
        return;
    }
    logger.printf("!mir");
}

static void fillOvlId(const size_t num_display, hwc_display_contents_1_t** displays, const disp_layer_info &disp_layer)
{
    DbgLogger logger(DbgLogger::TYPE_DUMPSYS, 'I', "HRT Result:");

    for (size_t i = 0; i < num_display; i++)
    {
        DispatcherJob* job = HWCDispatcher::getInstance().getExistJob(i);
        if (NULL != job)
        {
            hwc_display_contents_1_t* list = displays[i];

            if (list->flags & HWC_MIRROR_DISPLAY)
                continue;

            // only support two display at the same time
            // index 0: primary display; index 1: secondry display(MHL or vds)
            // fill display info
            const int disp_input = (i == HWC_DISPLAY_PRIMARY) ? 0 : 1;

            logger.printf(" (%d)/[%d,%d]", i, job->layer_info.gles_head,
                                              job->layer_info.gles_tail);

            job->layer_info.max_overlap_layer_num = disp_layer.hrt_num;
            job->layer_info.gles_head = disp_layer.gles_head[disp_input];
            job->layer_info.gles_tail = disp_layer.gles_tail[disp_input];

            // fill layer info
            job->layer_info.layer_config_list = disp_layer.input_config[disp_input];

            logger.printf(">[%d,%d]/%d", job->layer_info.gles_head,
                                         job->layer_info.gles_tail,
                                         job->layer_info.max_overlap_layer_num);
        }
    }
}

static void setLayerConfigFromDriver(const size_t num_display, hwc_display_contents_1_t** displays, const disp_layer_info &disp_layer)
{
    for (size_t i = 0; i < num_display; i++)
    {
        DispatcherJob* job = HWCDispatcher::getInstance().getExistJob(i);
        if (NULL != job)
        {
            // only support two display at the same time
            // index 0: primary display; index 1: secondry display(MHL or vds)
            // fill display info
            const int disp_input = (i == HWC_DISPLAY_PRIMARY) ? 0 : 1;
            // fill layer info
            job->layer_info.layer_config_list = disp_layer.input_config[disp_input];
        }
    }
}

int HWCMediator::prepare(size_t num_display, hwc_display_contents_1_t** displays)
{
    Debugger::getInstance().m_logger->dumpsys->flushOut();

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
    struct ScenarioInfo scenario_info;
    memset(&scenario_info, 0, sizeof(scenario_info));
    checkScenario(num_display, displays, &scenario_info);
    HWCDispatcher::getInstance().m_ultra_scenario = scenario_info.ultra_scenario;

    // set session mode
    HWCDispatcher::getInstance().setSessionMode(HWC_DISPLAY_PRIMARY, scenario_info.mirror_mode);

    for (uint32_t i = 0; i < num_display; i++)
    {
        if (DisplayManager::MAX_DISPLAYS <= i) break;

        list = displays[i];

        if (HWC_DISPLAY_VIRTUAL == i)
        {
            if (!m_features.virtuals)
            {
                HWC_LOGD("(%d) PRE/bypass/novir", i);
                continue;
            }
        }

        if (list == NULL) continue;

        // partial update - print surface damage info
        DbgLogger logger(DbgLogger::TYPE_HWC_LOG, 'V', "[DIRTY] pre(ltrb)");
        for (uint32_t j = 0; j < list->numHwLayers - 1; j++)
        {
            hwc_region_t* damage = &list->hwLayers[j].surfaceDamage;
            const uint32_t n = damage->numRects;

            logger.printf(" (L:%d n:%d)", j, n);

            const hwc_rect_t* rect = damage->rects;
            for (uint32_t k = 0; k < n; k++, rect++)
            {
                logger.printf("[%d,%d,%d,%d]", rect->left, rect->top, rect->right, rect->bottom);
            }
        }

        if (list->numHwLayers <= 0)
        {
            // do nothing if list is invalid
            HWC_LOGD("(%d) PRE/bypass/list=%p", i, list);
            continue;
        }

        if (list->numHwLayers == 1)
        {
            if (HWC_DISPLAY_VIRTUAL == i)
            {
                if (!m_features.copyvds)
                {
                    // do nothing when virtual display with "fbt only"
                    HWC_LOGD("(%d) PRE/bypass/num=0", i);
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
            else if (scenario_info.mirror_mode || (HWC_DISPLAY_VIRTUAL == i))
            {
                list->flags &= ~HWC_SKIP_DISPLAY;
            }
            else if (list->flags & HWC_GEOMETRY_CHANGED)
            {
                HWC_LOGD("display_%d has geomerty changed, clear the flag of skip display", i);
                list->flags &= ~HWC_SKIP_DISPLAY;
            }
        }

        DispatcherJob* job = HWCDispatcher::getInstance().getJob(i);
        if (NULL != job)
        {
            if (i == HWC_DISPLAY_EXTERNAL)
            {
                job->has_s3d_layer = listS3d(list, job);
            }
            else
            {
                job->has_s3d_layer = false;
            }
            if (m_features.merge_mdp_display && i == HWC_DISPLAY_PRIMARY)
            {
                job->need_av_grouping = scenario_info.av_grouping;
            }
            else
            {
                job->need_av_grouping = false;
            }

            if (list->flags & HWC_MIRROR_DISPLAY)
                continue;

            handleOriginDisp(i, list, job, scenario_info.mirror_mode);
        }
        else
        {
            HWC_LOGW("(%d) PRE/job=null !!", i);
        }
    }

    // query hrt calculation from display driver
    disp_layer_info disp_layer = {0};
    const bool err = DispDevice::getInstance().queryValidLayer(num_display, displays, &disp_layer);

    // need to set smart layer list, no matter HRT exist or not
    setLayerConfigFromDriver(num_display, displays, disp_layer);

    if (err)
    {
        fillOvlId(num_display, displays, disp_layer);
    }
    else
    {
        // handle hrt by hwc
        // pass 1: count overlap
        countDisplaysOverlap(num_display, displays);

        // set layers as GLES layers if exceed overlap limit
        // in this function, max_ovl_overlap_num of job has set.
        if (arrangeDisplaysOverlap(num_display, displays))
        {
            // handle hrt by hwc
            // pass 2: count overlap again because composition type of layers have been changed due to
            // exceed overlap limit
            countDisplaysOverlap(num_display, displays);

            // in this function, refill max_ovl_overlap_num
            fillDisplaysOverlapResult(num_display, displays);
        }
    }
    // finish DisplayList, fill things to HWLayer
    finishDisplayList(num_display, displays);

    return 0;
}

static void adjustFps(size_t num_display, hwc_display_contents_1_t** displays)
{
    if (num_display < HWC_NUM_PHYSICAL_DISPLAY_TYPES || displays == NULL ||
            displays[HWC_DISPLAY_EXTERNAL] == NULL)
    {
        return;
    }

    static bool degrage_fps = false;
    int size = displays[HWC_DISPLAY_EXTERNAL]->numHwLayers;
    // When HWC decide to use ultra scenario and MHL has UI layer, the bandwidth is not enough.
    // Therefore, we have to degrade the FPS to decrease the usage of GPU. However, we do not
    // want to drop frame when play 4k video in 4k MHL, so we need to keep that SF vsync is 60
    // fps.
    //TODO After create the interface of SF for different period of VSync, keep SF VSync to 60
    //     fps and degrade AP VSync to 30 fps when HWC uses ultra scenario.
    if (HWCDispatcher::getInstance().m_ultra_scenario && size > 2)
    {
        if (!degrage_fps)
        {
            HWC_LOGD("ultra scenario with ui, try to degrade fps");
            set4kMhlInfo2Ged(true);
            degrage_fps = true;
        }
    }
    else if (degrage_fps)
    {
        HWC_LOGD("normal scenario or ultra scenario, try to elevate fps");
        set4kMhlInfo2Ged(false);
        degrage_fps = false;
    }
}

static void adjustVsyncOffset(const size_t& num_display, hwc_display_contents_1_t** displays)
{
    DispatcherJob *job = HWCDispatcher::getInstance().getExistJob(HWC_DISPLAY_PRIMARY);
    if (job != NULL) {
        static bool prev_merge_md = false;
        bool now_merge_md = job->need_av_grouping;
        if (prev_merge_md != now_merge_md)
        {
            prev_merge_md = now_merge_md;
            setMergeMdInfo2Ged(now_merge_md);
        }
    }
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

    adjustFps(num_display, displays);
    adjustVsyncOffset(num_display, displays);

    for (uint32_t i = 0; i < num_display; i++)
    {
        if (DisplayManager::MAX_DISPLAYS <= i) break;

        list = displays[i];

        if (list == NULL) continue;

        if (list->numHwLayers <= 0)
        {
            HWC_LOGD("(%d) SET/bypass/list=%p", i, list);
            continue;
        }

        if (HWC_DISPLAY_VIRTUAL == i)
        {
            if (list->numHwLayers == 1)
            {
                if (!m_features.copyvds)
                {
                    HWC_LOGD("(%d) SET/bypass/num=0", i);
                    clearListFbt(list);
                    continue;
                }
            }
            else if (!m_features.virtuals)
            {
                HWC_LOGD("(%d) SET/bypass/novir", i);
                clearListFbt(list);
                continue;
            }
        }

        HWCDispatcher::getInstance().setJob(i, list);
    }

    if (HWCDispatcher::getInstance().m_ultra_scenario &&
        Platform::getInstance().m_config.use_async_bliter &&
        Platform::getInstance().m_config.use_async_bliter_ultra)
    {
        check_ultra_mdp(num_display, displays);
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
