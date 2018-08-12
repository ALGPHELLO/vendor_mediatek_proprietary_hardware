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
#include <GraphicBufferUtil.h>

// ---------------------------------------------------------------------------
class TempBuffer : public Singleton<TempBuffer>
{
public:
    TempBuffer();
    ~TempBuffer();
    buffer_handle_t getHandle(PrivateHandle** priv_hand);
    void dump();

private:
    int m_standby;
    int m_cnt;
    buffer_handle_t* m_handle;
    PrivateHandle* m_priv_handle;
};

ANDROID_SINGLETON_STATIC_INSTANCE(TempBuffer);

TempBuffer::TempBuffer() : m_standby(0), m_cnt(2)
{
    m_handle        = (buffer_handle_t*)calloc(m_cnt, sizeof(buffer_handle_t));
    m_priv_handle   = (PrivateHandle*)calloc(m_cnt, sizeof(PrivateHandle));

    const DisplayData* disp_data = &DisplayManager::getInstance().m_data[0];

    for (int i = 0; i < m_cnt; i++)
    {
        GrallocDevice::AllocParam param;
        param.width  = disp_data->width;
        param.height = disp_data->height;
        param.format = HAL_PIXEL_FORMAT_RGBA_8888;
        param.usage  = GRALLOC_USAGE_HW_COMPOSER;

        // allocate
        if (NO_ERROR != GrallocDevice::getInstance().alloc(param))
        {
            MULPASSLOGE("TempBuf", "allocate temp buf fail");
            return;
        }
        m_handle[i] = param.handle;

        getPrivateHandle(m_handle[i], &m_priv_handle[i]);
    }
}

TempBuffer::~TempBuffer()
{
    for (int i = 0; i < m_cnt; i++)
    {
        if (m_handle[i] != 0)
            GrallocDevice::getInstance().free(m_handle[i]);
    }
}

buffer_handle_t TempBuffer::getHandle(PrivateHandle** priv_hand)
{
    buffer_handle_t handle;
    handle = m_handle[m_standby];
    *priv_hand = &m_priv_handle[m_standby];
    MULPASSLOGV("TempBuf", "get temp buffer: %d (h:%x)", m_standby, handle);
    m_standby = (m_standby + 1) % m_cnt;
    return handle;
}

void TempBuffer::dump()
{
    String8 file_name;
    for (int i = 0; i < m_cnt; i++)
    {
         file_name = String8::format("temp_buf_%d", i);
         getGraphicBufferUtil().dump(m_handle[i], file_name.string());
    }
}

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
    Debugger::getInstance();
    Debugger::getInstance().m_logger = new Debugger::LOGGER();

    // check features setting
    initFeatures();

    MMUDevice::getInstance();

    DevicePlatform::getInstance();

    char value[PROPERTY_VALUE_MAX];
    sprintf(value, "%d", DevicePlatform::m_config.compose_level);
    property_set("debug.hwc.compose_level", value);

    // check if virtual display could be composed by hwc
    status_t err = DispDevice::getInstance().createOverlaySession(HWC_DISPLAY_VIRTUAL, DISP_SESSION_DIRECT_LINK_MODE);
    m_features.virtuals = (err == NO_ERROR);
    DispDevice::getInstance().destroyOverlaySession(HWC_DISPLAY_VIRTUAL);

    DispDevice::getInstance().setCapsInfo();

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

    // set 0, if you want all log to be shown
    sprintf(default_value, "%d", Debugger::m_skip_log);
    property_get("debug.hwc.skip_log", value, default_value);
    Debugger::m_skip_log = atoi(value);

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

    // check debug setting of bandwidth limitation
    property_get("debug.hwc.bytes_limit", value, "0");
    DevicePlatform::m_config.bytes_limit = atoi(value);

    // check debug setting of ovl_overlap_limit
    sprintf(default_value, "%d", DevicePlatform::m_config.ovl_overlap_limit);
    property_get("debug.hwc.ovl_overlap_limit", value, default_value);
    DevicePlatform::m_config.ovl_overlap_limit = atoi(value);

    // check debug setting of present fence
    property_get("debug.hwc.disable_p_fence", value, "0");
    int disable_p_fence = atoi(value);
    if (disable_p_fence)
    {
        DevicePlatform::m_config.overlay_cap &= ~OVL_CAP_P_FENCE;
    }

    // check dump level
    property_get("debug.hwc.dump_level", value, "0");
    int dump_level = atoi(value);

    HWCDispatcher::getInstance().dump(&log, (dump_level & (DUMP_MM | DUMP_SYNC)));

    Debugger::getInstance().dump(&log);

    // dump temp buffer
    DISP_CAP_OUTPUT_PASS query_pass = DispDevice::getInstance().getCapabilitiesPass();
    if ((0 != dump_level) && (DISP_OUTPUT_CAP_MULTI_PASS == query_pass))
        TempBuffer::getInstance().dump();

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

    // query hw cap and reset caps info in DispDevice
    DispDevice::getInstance().setCapsInfo();

#ifdef MTK_HWC_PROFILING
    property_get("debug.hwc.profile_handle_all", value, "0");
    g_handle_all_layers = (atoi(value) != 0);
#endif

    property_get("debug.hwc.disable.skip.display", value, "0");
    HWCDispatcher::getInstance().m_disable_skip_redundant = atoi(value) ? true : false;

    property_get("debug.uipq.layer", value, "0");
    HWCDispatcher::getInstance().m_uipq_debug = atoi(value) ? true : false;
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

// for hwc 1.3/1.4 api compatible
// just call setPowerMode to do blank function
int HWCMediator::blank(int dpy, int blank)
{
    int mode = blank ? HWC_POWER_MODE_OFF : HWC_POWER_MODE_NORMAL;
    return setPowerMode(dpy, mode);
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

#define setLineNum(RTLINE, TYPE) ({ \
                            RTLINE = __LINE__; \
                            TYPE; \
                        })

inline int layerType(
    int dpy, PrivateHandle* priv_handle, hwc_layer_1_t* layer,
    int& dirty, bool secure, int& rt_line)
{
    // HWC is disabled or some other reasons
    if (layer->flags & HWC_SKIP_LAYER)
        return setLineNum(rt_line, HWC_LAYER_TYPE_INVALID);

    int compose_level = DevicePlatform::m_config.compose_level;
    bool check_dim_layer = (DevicePlatform::m_config.overlay_cap & OVL_CAP_DIM);

    if (check_dim_layer && (layer->flags & HWC_DIM_LAYER))
    {
        if (compose_level & COMPOSE_DISABLE_UI)
            return setLineNum(rt_line, HWC_LAYER_TYPE_INVALID);

        if (Transform::ROT_INVALID & layer->transform)
            return setLineNum(rt_line, HWC_LAYER_TYPE_INVALID);

        if (getDstWidth(layer) <= 0 || getDstHeight(layer) <= 0)
            return setLineNum(rt_line, HWC_LAYER_TYPE_INVALID);

        return setLineNum(rt_line, HWC_LAYER_TYPE_DIM);
    }

    if (!layer->handle)
        return setLineNum(rt_line, HWC_LAYER_TYPE_INVALID);

    getPrivateHandleInfo(layer->handle, priv_handle);

    if (((priv_handle->usage & GRALLOC_USAGE_PROTECTED) ||
         (priv_handle->usage & GRALLOC_USAGE_SECURE)) && !secure)
        return setLineNum(rt_line, HWC_LAYER_TYPE_INVALID);

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
    if (buffer_type == GRALLOC_EXTRA_BIT_TYPE_VIDEO ||
        buffer_type == GRALLOC_EXTRA_BIT_TYPE_CAMERA ||
        priv_handle->format == HAL_PIXEL_FORMAT_YV12)
    {
        if (compose_level & COMPOSE_DISABLE_MM)
            return setLineNum(rt_line, HWC_LAYER_TYPE_INVALID);

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
            if (is_high)
                return setLineNum(rt_line, HWC_LAYER_TYPE_MM_HIGH);

            return setLineNum(rt_line, HWC_LAYER_TYPE_MM);
        }
        else
        {
            return setLineNum(rt_line, HWC_LAYER_TYPE_INVALID);
        }
    }

    // any layers above layer handled by fbt should also be handled by fbt
    if (compose_level & COMPOSE_DISABLE_UI)
        return setLineNum(rt_line, HWC_LAYER_TYPE_INVALID);

    // to debug which layer has been selected as UIPQ layer
    if (HWCMediator::getInstance().m_features.global_pq &&
        HWC_DISPLAY_PRIMARY == dpy &&
        (priv_handle->ext_info.status2 & GRALLOC_EXTRA_BIT2_UI_PQ_ON) &&
        HWCDispatcher::getInstance().m_uipq_debug)
    {
        return setLineNum(rt_line, HWC_LAYER_TYPE_UIPQ_DEBUG);
    }

    // check platform capability
    if (DevicePlatform::getInstance().isUILayerValid(dpy, layer, priv_handle))
    {
        // always treat CPU layer as dirty
        if (buffer_type != GRALLOC_EXTRA_BIT_TYPE_GPU)
            dirty |= HWC_LAYER_DIRTY_BUFFER;

        dirty &= ~HWC_LAYER_DIRTY_PARAM;

        if (layer->flags & HWC_IS_CURSOR_LAYER)
        {
            return setLineNum(rt_line, HWC_LAYER_TYPE_CURSOR);
        }

        if (HWCMediator::getInstance().m_features.global_pq &&
            HWC_DISPLAY_PRIMARY == dpy &&
            (priv_handle->ext_info.status2 & GRALLOC_EXTRA_BIT2_UI_PQ_ON) &&
            WIDTH(layer->displayFrame) > 1 &&
            HEIGHT(layer->displayFrame) > 1)
        {
            return setLineNum(rt_line, HWC_LAYER_TYPE_UIPQ);
        }

        return setLineNum(rt_line, HWC_LAYER_TYPE_UI);
    }
    else
    {
        return setLineNum(rt_line, HWC_LAYER_TYPE_INVALID);
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
    top = -1;
    bottom = -1;

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
            HWC_LOGW("layerRange: unknown type(%d)", type);
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
    int ovl_idx, int layer_idx, int type, int dirty, int& num_ui, int& num_mm, int pass = 0)
{
    HWLayer* hw_layer = &job->hw_layers_multi[pass][ovl_idx];
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

        case HWC_LAYER_TYPE_UIPQ_DEBUG:
            if (HWCMediator::getInstance().m_features.global_pq)
            {
                hw_layer->type   = HWC_LAYER_TYPE_UI;
                hw_layer->enable = false;
                job->uipq_index  = layer_idx;
                num_ui++;
            }
            else
            {
                HWC_LOGE("global pq feature not support!");
            }
            break;

        case HWC_LAYER_TYPE_UIPQ:
            if (HWCMediator::getInstance().m_features.global_pq)
            {
                hw_layer->type  = HWC_LAYER_TYPE_MM;
                hw_layer->dirty = true;
                job->uipq_index = layer_idx;
                num_mm++;
            }
            else
            {
                HWC_LOGE("global pq feature not support!");
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
        memset(job->hw_layers_multi[0], 0, sizeof(HWLayer) * job->num_layers); \
    } while(0)

static void handleOriginDisp(
    int dpy, hwc_display_contents_1_t* list, DispatcherJob* job, bool /*use_mirror_mode*/)
{
    DbgLogger logger(DbgLogger::TYPE_DUMPSYS, 'I', "LayerInfo(%d): ", dpy);
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
    DISP_MODE session_mode = (DISP_MODE)HWCDispatcher::getInstance().getSessionMode(dpy);
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
            int rt_line;
            type = layerType(dpy, &priv_handle, layer, dirty, secure, rt_line);
            disp_dirty |= (dirty != 0);

            logger.printf("(%d:%d,L%d) ", i, type, rt_line);
        }

        if (ovl_overlap_limit > 0 && (layers_num > ovl_overlap_limit))
        {
            if (addOverlapVector(overlap_vector, layer, type, disp_bottom))
            {
                if (exceedOverlapLimit(overlap_vector, ovl_overlap_limit))
                {
                    if (!overlap_limit_reset)
                    {
                        HWC_LOGI("Exceed Overlap Limit1(%d), cur(%d)",
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
                    HWC_LOGI("Exceed Bytes Limit: %d, %d", total_layer_bytes, bw_limit_bytes);
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
            int rt_line;
            int type = layerType(dpy, &priv_handle, layer, dirty, secure, rt_line);
            disp_dirty |= (dirty != 0);

            logger.printf("(%d:%d,L%d) ", i, type, rt_line);

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
                        HWC_LOGI("Exceed Overlap Limit2(%d), cur(%d)", ovl_overlap_limit, i);
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
                    HWC_LOGI("Exceed Bytes Limit2: %d, %d", total_layer_bytes, bw_limit_bytes);
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
            HWLayer* fbt_hw_layer = &job->hw_layers_multi[0][fbt_hw_layer_idx];
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

#define HWC_ALWAYS_COMPOSE
#ifdef HWC_ALWAYS_COMPOSE
    disp_dirty = true;
#else
    // TODO: enable this after verification (e.g. layer count changes)
    // always recompose if
    // 1. fbt exists
    // 2. no video layer
    // 3. using mirror mode
    if (job->fbt_exist || job->num_mm_layers == 0 || use_mirror_mode)
    {
        disp_dirty = true;
    }

    // NOTE: enable this profiling to observe hwc recomposition
    if (DisplayManager::m_profile_level & PROFILE_TRIG)
    {
        char tag[16];
        snprintf(tag, sizeof(tag), "HWC_COMP_%1u", dpy);
        ATRACE_INT(tag, disp_dirty ? 1 : 0);
    }
#endif

    job->post_state = disp_dirty ? HWC_POST_INPUT_DIRTY : HWC_POST_INPUT_NOTDIRTY;
}

//====================================================================
struct OVL_SETTING
{
    int pass;
    int port;
};

struct LAYER_INFO
{
    int type;
    int dirty;
    //int area;
    PrivateHandle priv_handle;

    // layering reslut
    bool    is_gles;
    OVL_SETTING ovl;
};

struct LAYERING_RESULT
{
    bool has_gles;
    int ovl_pass_count;
    OVL_SETTING fbt;
    OVL_SETTING tmp_buf[MAX_OVL_PASS];
};

inline LAYER_INFO* getLayerInfoListInit(int dpy, bool secure,  bool mirror_disp,
                                            hwc_layer_1_t* layer_list, int layer_count, bool* disp_dirty)
{
    static LAYER_INFO* layer_info_list = NULL;
    static int layer_info_list_len = 0;

    // reallocate layer_list if it's needed
    if (layer_count > layer_info_list_len)
    {
        if (NULL != layer_info_list)
            free(layer_info_list);
        layer_info_list_len = layer_count;
        layer_info_list = (LAYER_INFO*)malloc(layer_info_list_len * sizeof(LAYER_INFO));
        if (NULL == layer_info_list)
        {
            layer_info_list_len = 0;
            return NULL;
        }
    }

    // init and get PrivateHandle
    LAYER_INFO* layer_info = layer_info_list;
    hwc_layer_1_t* layer = layer_list;
    for (int i = 0; i < layer_count; i++, layer_info++, layer++)
    {
        static const PrivateHandle priv_handle_forInit;

        // reset handle
        PrivateHandle* priv_handle = &layer_info->priv_handle;
        memcpy(priv_handle, &priv_handle_forInit, sizeof(PrivateHandle));

        int rt_line;
        // get data for handle
        // getPrivateHandleInfo(layer_list[i].handle, priv_handle);
        layer_info->dirty = (int)mirror_disp;
        layer_info->type = layerType(dpy, priv_handle, layer, layer_info->dirty, secure, rt_line);
        *disp_dirty |= (layer_info->dirty != 0);
    }
    return layer_info_list;
}

inline void fillLayeringConfg(int dpy,
                                  DispatcherJob* job,
                                  hwc_layer_1_t* layer_list,
                                  LAYER_INFO* layer_info_list,
                                  int layer_count,
                                  LAYERING_RESULT* result)
{
    MULPASSLOGV("layering", "configLayeringConfig");
    LAYER_INFO* layer_info = layer_info_list;
    hwc_layer_1_t* layer = layer_list;
    int num_ui = 0;
    int num_mm = 0;
    int num_fb = 0;
    for (int i = 0; i < layer_count; i++, layer_info++, layer++)
    {
        if (layer_info->is_gles)
        {
            MULPASSLOGV("layering", "configLayeringConfig layer(%d) fb fmt:%x", i, layer_info->priv_handle.format);
            layer->compositionType = HWC_FRAMEBUFFER;
            layer->hints = 0;
            num_fb++;
            continue;
        }

        PrivateHandle* priv_handle = &layer_info->priv_handle;
        int pass    = layer_info->ovl.pass;
        int port    = layer_info->ovl.port;
        int type    = layer_info->type;
        int dirty   = layer_info->dirty;

        layerSet(dpy, job, layer, priv_handle, port, i,
                            type, dirty, num_ui, num_mm, pass);
        MULPASSLOGV("layering", "configLayeringConfig layer(%d) ovl(%d,%d) fmt:%x", i, pass, port, priv_handle->format);
    }

    // config temp buffer
    MULPASSLOGV("layering", "config temp buffer");
    int ovl_pass_count = result->ovl_pass_count;
    for (int i = 0; i < ovl_pass_count - 1 ; i++)
    {
        PrivateHandle* priv_handle;
        buffer_handle_t tmp_hand = TempBuffer::getInstance().getHandle(&priv_handle);

        if (tmp_hand != 0)
        {
            // set tmp buf output
            job->tmp_buf[i].handle = tmp_hand;
            memcpy(&job->tmp_buf[i].priv_handle, priv_handle, sizeof(PrivateHandle));

            // set input for tmp buffer
            int pass = result->tmp_buf[i].pass;
            int port = result->tmp_buf[i].port;
            MULPASSLOGV("layering", "config tmpBuf ovl(%d,%d)", pass, port);
            HWLayer* hw_layer = &job->hw_layers_multi[pass][port];
            hw_layer->enable  = true;
            hw_layer->type    = HWC_LAYER_TYPE_TMP;
            hw_layer->dirty   = true;
            memcpy(&hw_layer->priv_handle, priv_handle, sizeof(PrivateHandle));
        }
    }

    if (result->has_gles)
    {
        // set HWLayer for FBT
        hwc_layer_1_t* fbt_layer = &layer_list[layer_count];
        if (fbt_layer->compositionType == HWC_FRAMEBUFFER_TARGET)
        {
            int pass = result->fbt.pass;
            int port = result->fbt.port;

            HWLayer* fbt_hw_layer = &job->hw_layers_multi[pass][port];
            fbt_hw_layer->enable  = true;
            fbt_hw_layer->index   = layer_count;
            fbt_hw_layer->type    = HWC_LAYER_TYPE_FBT;
            MULPASSLOGV("layering", "config FBT ovl(%d,%d)", pass, port);
        }
        else
        {
            MULPASSLOGE("layering", "FRAMEBUFFER_TARGET is not last layer!! dpy(%d)", dpy);
            result->has_gles = false;
        }
    }
    MULPASSLOGD("layering", "pass_cnt:%d", ovl_pass_count);

    job->hw_layers_pass_cnt = ovl_pass_count;
    // prepare job in job group
    job->fbt_exist     = result->has_gles;
    job->num_ui_layers = num_ui;
    job->num_mm_layers = num_mm;

}

inline bool mustFinalPass(LAYER_INFO* info)
{
    int type = info->type;
    if (HWC_LAYER_TYPE_UI == type)
        return (info->priv_handle.format == HAL_PIXEL_FORMAT_RGBX_8888);
    else
        return (HWC_LAYER_TYPE_DIM != type);
}

inline void layering(int max_pass_cnt,
                       int max_ovl_num,
                       LAYER_INFO* layer_info_list,
                       int layer_count,
                       LAYERING_RESULT* result)
{
    if (layer_count == 0)
    {
       result->has_gles = true;
       result->ovl_pass_count = 1;
       result->fbt.pass = 0;
       result->fbt.port = 0;
       return;
    }

    // layering algo implementation
    int ovl_used_count = 0;
    int gles_head = -1;

    int ovl_check_idx = max_ovl_num - 1;
    int layer_check_idx = layer_count - 1;
    bool hasMM = false;

    for (int i = 0; i < layer_count; i++)
    {
        if ((ovl_used_count == ovl_check_idx) && (i < layer_check_idx))
        {
            gles_head = i;
            break;
        }

        LAYER_INFO* layer_info = &layer_info_list[i];

        if (HWC_LAYER_TYPE_MM == layer_info->type)
            hasMM = true;

        if (HWC_LAYER_TYPE_INVALID == layer_info->type)
        {
            gles_head = i;
            break;
        }
        ovl_used_count++;
    }

    MULPASSLOGV("layering", "layer_cnt:%d gles_head:%d", layer_count, gles_head);
    if (gles_head == -1)
    {
        result->has_gles = false;
        result->ovl_pass_count = 1;
        for (int i = 0; i < layer_count; i++)
        {
            LAYER_INFO* layer_info = &layer_info_list[i];
            layer_info->is_gles  = false;
            layer_info->ovl.pass = 0;
            layer_info->ovl.port = i;
        }
        return;
    }

    int j = ovl_check_idx;
    int gles_tail = gles_head;
    for (int i = layer_count - 1; i > gles_head; i--, j--)
    {
        LAYER_INFO* layer_info = &layer_info_list[i];
        if (ovl_used_count == ovl_check_idx)
        {
            gles_tail = i;
            break;
        }

        int type = layer_info->type;
        if ((HWC_LAYER_TYPE_INVALID == type) || (HWC_LAYER_TYPE_MM == type))
        {
            gles_tail = i;
            break;
        }
        ovl_used_count++;
    }

    MULPASSLOGV("layering", "layer_cnt:%d GLES:[%d, %d]",layer_count, gles_head, gles_tail);
#if 1
    // two pass
    if ((max_pass_cnt > 1) && (!hasMM) && (gles_head != 0))
    {
        if (!mustFinalPass(&layer_info_list[gles_head - 1]))
        {
            // try to use additional pass
            int ovl_cnt = 1;
            int new_gles_head = gles_head;

            for (int i = gles_head; i <= gles_tail; i++)
            {
                MULPASSLOGV("layering", "add to addtional pass (i:%d type:%d)", i, layer_info_list[i].type);
                if (mustFinalPass(&layer_info_list[i]))
                    break;

                ovl_cnt++;
                new_gles_head++;

                if (ovl_cnt == max_ovl_num)
                    break;
            }

            if (ovl_cnt > 1)
            {
                MULPASSLOGV("layering", "2Pass - head(%d) newhead(%d) tail(%d)", gles_head, new_gles_head, gles_tail);
                result->ovl_pass_count = 2;
                // PASS 1 head --------------------------------------------
                MULPASSLOGV("layering", "Pass 1: [0, %d)", gles_head - 1);
                for (int i = 0; i < gles_head - 1; i++)
                {
                    LAYER_INFO* layer_info = &layer_info_list[i];
                    layer_info->is_gles  = false;
                    layer_info->ovl.pass = 1;
                    layer_info->ovl.port = i;
                }

                // PASS 0 --------------------------------------------
                int ovl_port = 0;
                MULPASSLOGV("layering", "Pass 0: [%d, %d)", gles_head - 1, new_gles_head);
                for (int i = gles_head - 1; i < new_gles_head; i++, ovl_port++)
                {
                    LAYER_INFO* layer_info = &layer_info_list[i];
                    layer_info->is_gles  = false;
                    layer_info->ovl.pass = 0;
                    layer_info->ovl.port = ovl_port;
                }
                result->tmp_buf[0].pass = 1;
                result->tmp_buf[0].port = gles_head - 1;

                // PASS 1 FBT --------------------------------------------
                ovl_port = gles_head;
                if ((new_gles_head == gles_tail))
                {
                    if (layer_info_list[gles_tail].type != HWC_LAYER_TYPE_INVALID)
                        gles_tail--;
                }
                if (new_gles_head > gles_tail)
                {
                    result->has_gles = false;
                }
                else
                {
                    result->has_gles = true;
                    result->fbt.pass = 1;
                    result->fbt.port = gles_head;
                    MULPASSLOGV("layering", "GLES  : [%d, %d]", new_gles_head, gles_tail);
                    for (int i = new_gles_head; i <= gles_tail; i++)
                    {
                        LAYER_INFO* layer_info = &layer_info_list[i];
                        layer_info->is_gles = true;
                    }
                    ovl_port++;
                }

                // PASS 1 tail --------------------------------------------
                MULPASSLOGV("layering", "Pass 1: [%d, %d)", gles_tail + 1, layer_count);
                for (int i = gles_tail + 1; i < layer_count; i++, ovl_port++)
                {
                    LAYER_INFO* layer_info = &layer_info_list[i];
                    layer_info->is_gles  = false;
                    layer_info->ovl.pass = 1;
                    layer_info->ovl.port = ovl_port;
                }
                return;
            }
        }
    }
#endif
    // just one pass
    result->has_gles = true;
    result->ovl_pass_count = 1;
    result->fbt.pass = 0;
    result->fbt.port = gles_head;
    for (int i = 0; i < gles_head; i++)
    {
        LAYER_INFO* layer_info = &layer_info_list[i];
        layer_info->is_gles  = false;
        layer_info->ovl.pass = 0;
        layer_info->ovl.port = i;
    }
    for (int i = gles_head; i <= gles_tail; i++)
    {
        LAYER_INFO* layer_info = &layer_info_list[i];
        layer_info->is_gles = true;
    }

    int ovl_port = gles_head + 1;
    for (int i = gles_tail + 1; i < layer_count; i++, ovl_port++)
    {
        LAYER_INFO* layer_info = &layer_info_list[i];
        layer_info->is_gles  = false;
        layer_info->ovl.pass = 0;
        layer_info->ovl.port = ovl_port;
    }
}

static void handleOriginDispMultiPass(int dpy, hwc_display_contents_1_t* list, DispatcherJob* job, bool use_mirror_mode)
{
    MULPASSLOGV("layering", "NEW LAYERING ALGO     !!!!!!!!!!!");
    // get info
    bool secure = DisplayManager::getInstance().m_data[dpy].secure;
    bool disp_dirty = false;
    bool mirror_disp = (list->flags & HWC_MIRROR_DISPLAY);
    int max_ovl_num = job->num_layers;
    int max_pass_cnt = use_mirror_mode ? 1 : job->hw_layers_pass_max;

    // force 4 ovl layers for test
    if (max_ovl_num > 4)
        max_ovl_num = 4;

    // get layer_list and layer_count
    hwc_layer_1_t* layer_list = list->hwLayers;
    int layer_count = list->numHwLayers - 1;

    // get layer_info_list and init
    LAYER_INFO* layer_info_list = getLayerInfoListInit(dpy, secure, mirror_disp, layer_list, layer_count, &disp_dirty);
    if (NULL == layer_info_list)
    {
        MULPASSLOGE("layering", "layer_info_list allocate Fail");
        return;
    }

    // layering
    LAYERING_RESULT result;
    layering(max_pass_cnt, max_ovl_num, layer_info_list, layer_count, &result);

    // fill config
    fillLayeringConfg(dpy, job, layer_list, layer_info_list, layer_count, &result);
    job->disp_mir_id   = HWC_MIRROR_SOURCE_INVALID;
    job->disp_ori_rot  = (list->flags & HWC_ORIENTATION_MASK) >> 16;

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
// ------------------------------------------------------------------------------------------------

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
    job->hw_layers_multi[0][0].enable = true;
    job->hw_layers_multi[0][0].type = HWC_LAYER_TYPE_MM;
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
        DISP_CAP_OUTPUT_PASS query_pass = DispDevice::getInstance().getCapabilitiesPass();
        if (DISP_OUTPUT_CAP_SINGLE_PASS == query_pass)
            handleOriginDisp(dpy, list, job, use_mirror_mode);
        else
            handleOriginDispMultiPass(dpy, list, job, use_mirror_mode);
    }
}

// checkMirrorMode() checks if mirror mode exists
static bool checkMirrorMode(
    size_t num_display, hwc_display_contents_1_t** displays)
{
    DbgLogger logger(DbgLogger::TYPE_DUMPSYS, 'I', "chkMir(%d): ", (int)num_display);

    const DisplayData* display_data = DisplayManager::getInstance().m_data;

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

        if ((DevicePlatform::m_config.mirror_state & MIRROR_DISABLED) ||
            (DevicePlatform::m_config.mirror_state & MIRROR_PAUSED))
        {
            // disable mirror mode
            // either the mirror state is disabled or the mirror source is blanked
            logger.printf("(%d:L%d) ", i, __LINE__);
            continue;
        }

        // assume that only primary panel can be mirrored by others
        if (i == HWC_DISPLAY_PRIMARY) continue;

        if (i == HWC_DISPLAY_EXTERNAL &&
            DevicePlatform::m_config.bypass_hdcp_checking &&
            listSecure(displays[HWC_DISPLAY_PRIMARY]))
        {
            // if hdcp checking is handled by display driver, the extension path must be applied.
            logger.printf("(%d:L%d) ", i, __LINE__);
            continue;
        }

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
                logger.printf("(%d:L%d) ", i, __LINE__);
                continue;
            }
        }

        // display id of mirror source
        int mir_dpy = HWC_DISPLAY_PRIMARY;

        if (!display_data[i].secure && listSecure(displays[mir_dpy]))
        {
            // disable mirror mode
            // if any secure or protected layer exists in mirror source
            logger.printf("(%d:L%d) ", i, __LINE__);
            continue;
        }

        if (!HWCMediator::getInstance().m_features.copyvds &&
            list->numHwLayers <= 1)
        {
            // disable mirror mode
            // since force copy vds is not used
            logger.printf("(%d:L%d) ", i, __LINE__);
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
                logger.printf("(%d:L%d) ", i, __LINE__);
                continue;
            }
        }

        list->flags |= HWC_MIRROR_DISPLAY;
        list->flags |= mir_dpy << 8;

        logger.printf("mir");
        return true;
    }
    logger.printf("!mir");
    return false;
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
    bool use_mirror_mode = checkMirrorMode(num_display, displays);

    // set session mode - if single pass only keep old path, setSessionMode before layering
    DISP_CAP_OUTPUT_PASS query_pass = DispDevice::getInstance().getCapabilitiesPass();
    bool is_single_pass_only = (DISP_OUTPUT_CAP_SINGLE_PASS == query_pass);
    if (is_single_pass_only)
        HWCDispatcher::getInstance().setSessionMode(HWC_DISPLAY_PRIMARY, use_mirror_mode);

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
        }

        DispatcherJob* job = HWCDispatcher::getInstance().getJob(i);
        if (NULL != job)
        {
            handleDisplayList(i, list, job, use_mirror_mode);

            DbgLogger* logger = Debugger::getInstance().m_logger->pre_info[i];
            logger->printf("(%d) PRE list=%d/max=%d/fbt=%d/ui=%d/mm=%d/mir=%d",
                i, list->numHwLayers - 1, job->num_layers, job->fbt_exist,
                job->num_ui_layers, job->num_mm_layers, job->disp_mir_id);
            logger->tryFlush();
        }
        else
        {
            HWC_LOGW("(%d) PRE/job=null !!", i);
        }
    }

    // set session mode - multipass, setSessionMode after layering
    if (!is_single_pass_only)
        HWCDispatcher::getInstance().setSessionMode(HWC_DISPLAY_PRIMARY, use_mirror_mode);

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

    // trigger DispatchThread
    HWCDispatcher::getInstance().trigger();

    // clean all flags which was added by ourself
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
