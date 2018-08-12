#define DEBUG_LOG_TAG "DEV"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>
#include <sys/ioctl.h>

#include <linux/fb.h>

#include "m4u_lib.h"

#include "ddp_ovl.h"

#include "hwc_priv.h"

#include <hardware/gralloc.h>

#include "utils/debug.h"
#include "utils/tools.h"
#include <utils/Trace.h>
#include "hwdev.h"
#include "display.h"
#include "overlay.h"
#include "platform.h"
#include "hwc.h"

#define HWC_ATRACE_BUFFER_INFO(string, n1, n2, n3, n4)                       \
    if (ATRACE_ENABLED()) {                                                   \
        char ___traceBuf[1024];                                               \
        snprintf(___traceBuf, 1024, "%s(%d:%d): %u %d", (string),             \
            (n1), (n2), (n3), (n4));                                          \
        android::ScopedTrace ___bufTracer(ATRACE_TAG, ___traceBuf);           \
    }

// ---------------------------------------------------------------------------

DISP_SESSION_TYPE g_session_type[DisplayManager::MAX_DISPLAYS] = {
    DISP_SESSION_PRIMARY,
    DISP_SESSION_EXTERNAL,
    DISP_SESSION_MEMORY
};

DISP_FORMAT mapDispInFormat(unsigned int format)
{
    switch (format)
    {
        case HAL_PIXEL_FORMAT_RGBA_8888:
            return DISP_FORMAT_RGBA8888;

        case HAL_PIXEL_FORMAT_RGBX_8888:
            return DISP_FORMAT_RGBX8888;

        case HAL_PIXEL_FORMAT_BGRA_8888:
            return DISP_FORMAT_BGRA8888;

        case HAL_PIXEL_FORMAT_BGRX_8888:
        case HAL_PIXEL_FORMAT_IMG1_BGRX_8888:
            return DISP_FORMAT_BGRX8888;

        case HAL_PIXEL_FORMAT_RGB_888:
            return DISP_FORMAT_RGB888;

        case HAL_PIXEL_FORMAT_RGB_565:
            return DISP_FORMAT_RGB565;

        case HAL_PIXEL_FORMAT_I420:
        case HAL_PIXEL_FORMAT_YV12:
        case HAL_PIXEL_FORMAT_NV12_BLK:
        case HAL_PIXEL_FORMAT_NV12_BLK_FCM:
        case HAL_PIXEL_FORMAT_YUV_PRIVATE:
        case HAL_PIXEL_FORMAT_YUYV:
        case HAL_PIXEL_FORMAT_UFO:
        case HAL_PIXEL_FORMAT_YCbCr_420_888:
        case HAL_PIXEL_FORMAT_NV12_BLK_10BIT_H:
        case HAL_PIXEL_FORMAT_NV12_BLK_10BIT_V:
        case HAL_PIXEL_FORMAT_UFO_10BIT_H:
        case HAL_PIXEL_FORMAT_UFO_10BIT_V:
            return DISP_FORMAT_YUV422;
    }
    HWC_LOGW("Not support input format(%d), use default RGBA8888", format);
    return DISP_FORMAT_ABGR8888;
}

DISP_FORMAT mapDispOutFormat(unsigned int format)
{
    switch (format)
    {
        case HAL_PIXEL_FORMAT_RGBA_8888:
            return DISP_FORMAT_RGBA8888;

        case HAL_PIXEL_FORMAT_YV12:
            return DISP_FORMAT_YV12;

        case HAL_PIXEL_FORMAT_RGBX_8888:
            return DISP_FORMAT_RGBX8888;

        case HAL_PIXEL_FORMAT_BGRA_8888:
            return DISP_FORMAT_BGRA8888;

        case HAL_PIXEL_FORMAT_BGRX_8888:
        case HAL_PIXEL_FORMAT_IMG1_BGRX_8888:
            return DISP_FORMAT_BGRX8888;

        case HAL_PIXEL_FORMAT_RGB_888:
            return DISP_FORMAT_RGB888;

        case HAL_PIXEL_FORMAT_RGB_565:
            return DISP_FORMAT_RGB565;

        case HAL_PIXEL_FORMAT_I420:
        case HAL_PIXEL_FORMAT_NV12_BLK:
        case HAL_PIXEL_FORMAT_NV12_BLK_FCM:
        case HAL_PIXEL_FORMAT_YUV_PRIVATE:
        case HAL_PIXEL_FORMAT_YCbCr_420_888:
            return DISP_FORMAT_YUV422;
    }
    HWC_LOGW("Not support output format(%d), use default RGBA8888", format);
    return DISP_FORMAT_ABGR8888;
}

DISP_YUV_RANGE_ENUM mapDispColorRange(int range, int format)
{
    switch (format)
    {
        case HAL_PIXEL_FORMAT_I420:
        case HAL_PIXEL_FORMAT_YV12:
        case HAL_PIXEL_FORMAT_NV12_BLK:
        case HAL_PIXEL_FORMAT_NV12_BLK_FCM:
        case HAL_PIXEL_FORMAT_YUV_PRIVATE:
        case HAL_PIXEL_FORMAT_YUYV:
        case HAL_PIXEL_FORMAT_UFO:
        case HAL_PIXEL_FORMAT_NV12_BLK_10BIT_H:
        case HAL_PIXEL_FORMAT_NV12_BLK_10BIT_V:
        case HAL_PIXEL_FORMAT_UFO_10BIT_H:
        case HAL_PIXEL_FORMAT_UFO_10BIT_V:
            switch (range)
            {
                case GRALLOC_EXTRA_BIT_YUV_BT601_NARROW:
                    return DISP_YUV_BT601;

                case GRALLOC_EXTRA_BIT_YUV_BT601_FULL:
                    return DISP_YUV_BT601_FULL;

                case GRALLOC_EXTRA_BIT_YUV_BT709_NARROW:
                    return DISP_YUV_BT709;
            }
            HWC_LOGW("Not support range(%#x), use default BT601", range);
            break;
    }
    return DISP_YUV_BT601;
}

ANDROID_SINGLETON_STATIC_INSTANCE(DispDevice);

DispDevice::DispDevice()
{
    char filename[256];
    sprintf(filename, "/dev/%s", DISP_SESSION_DEVICE);
    m_dev_fd = open(filename, O_RDONLY);
    if (m_dev_fd <= 0)
    {
        HWC_LOGE("Failed to open display device: %s ", strerror(errno));
        abort();
    }

    m_ovl_input_num = getMaxOverlayInputNum();

    int size = sizeof(disp_session_input_config) * DisplayManager::MAX_DISPLAYS;
    memset(m_input_config, 0, size);

    for (int i = 0; i < DisplayManager::MAX_DISPLAYS; i++)
    {
        m_input_config[i].session_id = DISP_INVALID_SESSION;
        m_session_mode[i] = DISP_INVALID_SESSION_MODE;
    }
}

DispDevice::~DispDevice()
{
    close(m_dev_fd);
}

void DispDevice::initOverlay()
{
    DevicePlatform::getInstance().initOverlay();
}

int DispDevice::getMaxOverlayInputNum()
{
    // TODO: deinfe in ddp_ovl.h now.
    // This's header file for kernel, not for userspace. Need to refine later
    return OVL_LAYER_NUM;
}

status_t DispDevice::createOverlaySession(int dpy, DISP_MODE mode)
{
    int session_id = m_input_config[dpy].session_id;
    if (DISP_INVALID_SESSION != session_id)
    {
        HWC_LOGW("(%d) Failed to create existed DispSession (id=0x%x)", dpy, session_id);
        return INVALID_OPERATION;
    }

    disp_session_config config;
    memset(&config, 0, sizeof(disp_session_config));

    config.type       = g_session_type[dpy];
    config.device_id  = dpy;
    config.mode       = mode;
    config.session_id = DISP_INVALID_SESSION;

    int err = ioctl(m_dev_fd, DISP_IOCTL_CREATE_SESSION, &config);
    if (err < 0)
    {
        m_input_config[dpy].session_id = DISP_INVALID_SESSION;
        m_session_mode[dpy] = DISP_INVALID_SESSION_MODE;

        HWC_LOGE("(%d) Failed to create DispSession (mode=%d) err=%s !!", dpy, mode, strerror(errno));

        return BAD_VALUE;
    }

    m_input_config[dpy].session_id = config.session_id;
    m_session_mode[dpy] = mode;

    HWC_LOGD("(%d) Create DispSession (id=0x%x, mode=%d)", dpy, config.session_id, mode);

    return NO_ERROR;
}

void DispDevice::destroyOverlaySession(int dpy)
{
    int session_id = m_input_config[dpy].session_id;
    if (DISP_INVALID_SESSION == session_id)
    {
        HWC_LOGW("(%d) Failed to destroy invalid DispSession", dpy);
        return;
    }

    disp_session_config config;
    memset(&config, 0, sizeof(disp_session_config));

    config.type       = g_session_type[dpy];
    config.device_id  = dpy;
    config.session_id = session_id;

    ioctl(m_dev_fd, DISP_IOCTL_DESTROY_SESSION, &config);
    m_input_config[dpy].session_id = DISP_INVALID_SESSION;
    m_session_mode[dpy] = DISP_INVALID_SESSION_MODE;

    HWC_LOGD("(%d) Destroy DispSession (id=0x%x)", dpy, session_id);
}

status_t DispDevice::triggerOverlaySession(int dpy, int present_fence_idx)
{
    int session_id = m_input_config[dpy].session_id;
    if (DISP_INVALID_SESSION == session_id)
    {
        HWC_LOGW("(%d) Failed to trigger invalid DispSession", dpy);
        return INVALID_OPERATION;
    }

    disp_session_config config;
    memset(&config, 0, sizeof(disp_session_config));

    config.type       = g_session_type[dpy];
    config.device_id  = dpy;
    config.session_id = session_id;
    config.present_fence_idx = present_fence_idx;

    int err = ioctl(m_dev_fd, DISP_IOCTL_TRIGGER_SESSION, &config);

    HWC_LOGD("(%d) Trigger DispSession (id=0x%x, stat=0x%x, present fence idx=%d)", dpy, session_id, err, present_fence_idx);

    return err;
}

void DispDevice::disableOverlaySession(
    int dpy,  OverlayPortParam* const* params, int num)
{
    int session_id = m_input_config[dpy].session_id;
    if (DISP_INVALID_SESSION == session_id)
    {
        HWC_LOGW("(%d) Failed to disable invalid DispSession", dpy);
        return;
    }

    {
        for (int i = 0; i < m_ovl_input_num; i++)
        {
            disp_input_config* input = &m_input_config[dpy].config[i];

            if (i >= num)
            {
                input->layer_id = m_ovl_input_num + 1;
                continue;
            }

            input->layer_id     = i;
            input->layer_enable = 0;
            input->next_buff_idx = params[i]->fence_index;

            HWC_LOGD("(%d:%d) Layer- (idx=%d)", dpy, i, input->next_buff_idx);
        }

        m_input_config[dpy].config_layer_num =
            (num < m_ovl_input_num) ? num : m_ovl_input_num;

        ioctl(m_dev_fd, DISP_IOCTL_SET_INPUT_BUFFER, &m_input_config[dpy]);
    }

    {
        disp_session_config config;
        memset(&config, 0, sizeof(disp_session_config));

        config.type       = g_session_type[dpy];
        config.device_id  = dpy;
        config.session_id = session_id;
        config.present_fence_idx = -1;

        ioctl(m_dev_fd, DISP_IOCTL_TRIGGER_SESSION, &config);
    }

    HWC_LOGD("(%d) Disable DispSession (id=0x%x)", dpy, session_id);
}

status_t DispDevice::setOverlaySessionMode(int dpy, DISP_MODE mode)
{
    int session_id = m_input_config[dpy].session_id;
    if (DISP_INVALID_SESSION == session_id)
    {
        HWC_LOGW("(%d) Failed to set invalid DispSession (mode)", dpy);
        return INVALID_OPERATION;
    }

    disp_session_config config;
    memset(&config, 0, sizeof(disp_session_config));

    config.device_id  = dpy;
    config.session_id = session_id;
    config.mode       = mode;

    int err = ioctl(m_dev_fd, DISP_IOCTL_SET_SESSION_MODE, &config);
    if (err < 0)
    {
        HWC_LOGE("(%d) Failed to set DispSession (mode=%d) err=%s !!", dpy, mode, strerror(errno));

        return BAD_VALUE;
    }

    m_session_mode[dpy] = mode;

    HWC_LOGD("(%d) Set DispSessionMode (id=0x%x mode=%s)", dpy, session_id, toString(mode));

    return NO_ERROR;
}

DISP_MODE DispDevice::getOverlaySessionMode(int dpy)
{
    int session_id = m_input_config[dpy].session_id;
    if (DISP_INVALID_SESSION == session_id)
    {
        HWC_LOGW("(%d) Failed to get invalid DispSession", dpy);
        return DISP_INVALID_SESSION_MODE;
    }

    return m_session_mode[dpy];
}

status_t DispDevice::getOverlaySessionInfo(int dpy, disp_session_info* info)
{
    int session_id = m_input_config[dpy].session_id;
    if (DISP_INVALID_SESSION == session_id)
    {
        HWC_LOGW("(%d) Failed to get info for invalid DispSession", dpy);
        return INVALID_OPERATION;
    }

    info->session_id = session_id;

    ioctl(m_dev_fd, DISP_IOCTL_GET_SESSION_INFO, info);

    return NO_ERROR;
}

int DispDevice::getAvailableOverlayInput(int dpy)
{
    int session_id = m_input_config[dpy].session_id;
    if (DISP_INVALID_SESSION == session_id)
    {
        HWC_LOGW("(%d) Failed to get info for invalid DispSession", dpy);
        return 0;
    }

    disp_session_info info;
    memset(&info, 0, sizeof(disp_session_info));

    info.session_id = session_id;

    ioctl(m_dev_fd, DISP_IOCTL_GET_SESSION_INFO, &info);

    return info.maxLayerNum;
}

void DispDevice::prepareOverlayInput(
    int dpy, OverlayPrepareParam* param)
{
    int session_id = m_input_config[dpy].session_id;
    if (DISP_INVALID_SESSION == session_id)
    {
        HWC_LOGW("(%d) Failed to preapre invalid DispSession (input)", dpy);
        return;
    }

    disp_buffer_info buffer;
    memset(&buffer, 0, sizeof(disp_buffer_info));

    buffer.session_id = session_id;
    buffer.layer_id   = param->id;
    buffer.layer_en   = 1;
    buffer.ion_fd     = param->ion_fd;
    buffer.cache_sync = param->is_need_flush;
    buffer.index      = -1;
    buffer.fence_fd   = -1;

    ioctl(m_dev_fd, DISP_IOCTL_PREPARE_INPUT_BUFFER, &buffer);

    param->fence_index = buffer.index;
    param->fence_fd    = buffer.fence_fd;

    HWC_ATRACE_BUFFER_INFO("pre_input",
        dpy, param->id, param->fence_index, param->fence_fd);
}

void DispDevice::enableOverlayInput(
    int /*dpy*/, OverlayPortParam* /*param*/, int /*id*/)
{
    // TODO
}

void DispDevice::updateOverlayInputs(
    int dpy, OverlayPortParam* const* params, int num)
{
    int session_id = m_input_config[dpy].session_id;
    if (DISP_INVALID_SESSION == session_id)
    {
        HWC_LOGW("(%d) Failed to update invalid DispSession (input)", dpy);
        return;
    }

    int config_layer_num = 0;
    for (int i = 0; i < m_ovl_input_num; i++)
    {
        disp_input_config* input = &m_input_config[dpy].config[config_layer_num];

        if (i >= num || (OVL_IN_PARAM_IGNORE == params[i]->state))
        {
            HWC_LOGV("(%d:%d) Layer*", dpy, i);
            continue;
        }

        config_layer_num++;

        if (OVL_IN_PARAM_DISABLE == params[i]->state)
        {
            input->layer_id      = i;
            input->layer_enable  = 0;
            input->next_buff_idx = params[i]->fence_index;

            HWC_LOGV("(%d:%d) Layer- (idx=%d)", dpy, i, input->next_buff_idx);
            continue;
        }

        input->layer_id       = i;
        input->layer_enable   = 1;

        if (params[i]->dim)
        {
            input->buffer_source = DISP_BUFFER_ALPHA;
        }
        else if (params[i]->ion_fd == DISP_NO_ION_FD)
        {
            input->buffer_source = DISP_BUFFER_MVA;
        }
        else
        {
            input->buffer_source = DISP_BUFFER_ION;
        }

        input->layer_rotation = DISP_ORIENTATION_0;
        input->layer_type     = DISP_LAYER_2D;
        input->src_base_addr  = params[i]->va;
        input->src_phy_addr   = params[i]->mva;
        input->src_pitch      = params[i]->pitch;
        input->src_fmt        = mapDispInFormat(params[i]->format);
        input->src_offset_x   = params[i]->src_crop.left;
        input->src_offset_y   = params[i]->src_crop.top;
        input->src_width      = params[i]->src_crop.getWidth();
        input->src_height     = params[i]->src_crop.getHeight();
        input->tgt_offset_x   = params[i]->dst_crop.left;
        input->tgt_offset_y   = params[i]->dst_crop.top;
        input->tgt_width      = params[i]->dst_crop.getWidth();
        input->tgt_height     = params[i]->dst_crop.getHeight();
        input->isTdshp        = params[i]->is_sharpen;
        input->next_buff_idx  = params[i]->fence_index;
        input->identity       = params[i]->identity;
        input->connected_type = params[i]->connected_type;
        input->alpha_enable   = params[i]->alpha_enable;
        input->alpha          = params[i]->alpha;
        input->frm_sequence   = params[i]->sequence;
        input->yuv_range      = mapDispColorRange(params[i]->color_range, params[i]->format);

        if (params[i]->blending == HWC_BLENDING_PREMULT)
        {
            input->sur_aen = 1;
            input->src_alpha = DISP_ALPHA_ONE;
            input->dst_alpha = DISP_ALPHA_SRC_INVERT;
        }
        else
        {
            input->sur_aen = 0;
        }

        if (params[i]->secure)
            input->security = DISP_SECURE_BUFFER;
        else if (params[i]->protect)
            input->security = DISP_PROTECT_BUFFER;
        else
            input->security = DISP_NORMAL_BUFFER;

        HWC_LOGD("(%d:%d) Layer+ ("
                 "mva=%p/ion=%d/idx=%d/sec=%d/prot=%d/"
                 "alpha=%d:0x%02x/blend=%04x/dim=%d/fmt=%d/range=%d/"
                 "x=%d y=%d w=%d h=%d s=%d -> x=%d y=%d w=%d h=%d"
                 ")",
                 dpy, i, params[i]->mva, params[i]->ion_fd,
                 params[i]->fence_index, params[i]->secure, params[i]->protect,
                 params[i]->alpha_enable, params[i]->alpha, params[i]->blending,
                 params[i]->dim, input->src_fmt, params[i]->color_range,
                 params[i]->src_crop.left, params[i]->src_crop.top,
                 params[i]->src_crop.getWidth(), params[i]->src_crop.getHeight(), params[i]->pitch,
                 params[i]->dst_crop.left, params[i]->dst_crop.top,
                 params[i]->dst_crop.getWidth(), params[i]->dst_crop.getHeight());

        HWC_ATRACE_BUFFER_INFO("set_input",
            dpy, i, params[i]->fence_index, params[i]->if_fence_index);
    }

    m_input_config[dpy].config_layer_num = config_layer_num;

    ioctl(m_dev_fd, DISP_IOCTL_SET_INPUT_BUFFER, &m_input_config[dpy]);
}

void DispDevice::prepareOverlayOutput(int dpy, OverlayPrepareParam* param)
{
    int session_id = m_input_config[dpy].session_id;
    if (DISP_INVALID_SESSION == session_id)
    {
        HWC_LOGW("(%d) Failed to prepare invalid DispSession (output)", dpy);
        return;
    }

    disp_buffer_info buffer;
    memset(&buffer, 0, sizeof(disp_buffer_info));

    buffer.session_id = session_id;
    buffer.layer_id   = param->id;
    buffer.layer_en   = 1;
    buffer.ion_fd     = param->ion_fd;
    buffer.cache_sync = param->is_need_flush;
    buffer.index      = -1;
    buffer.fence_fd   = -1;

    ioctl(m_dev_fd, DISP_IOCTL_PREPARE_OUTPUT_BUFFER, &buffer);

    param->fence_index = buffer.index;
    param->fence_fd    = buffer.fence_fd;

    param->if_fence_index = buffer.interface_index;
    param->if_fence_fd    = buffer.interface_fence_fd;

    HWC_ATRACE_BUFFER_INFO("pre_output",
        dpy, param->id, param->fence_index, param->fence_fd);
}

void DispDevice::enableOverlayOutput(int dpy, OverlayPortParam* param)
{
    int session_id = m_input_config[dpy].session_id;
    if (DISP_INVALID_SESSION == session_id)
    {
        HWC_LOGW("(%d) Failed to update invalid DispSession (output)", dpy);
        return;
    }

    disp_session_output_config output_config;
    output_config.session_id    = session_id;
    output_config.config.va     = param->va;
    output_config.config.pa     = param->mva;
    output_config.config.fmt    = mapDispOutFormat(param->format);
    output_config.config.x      = param->dst_crop.left;
    output_config.config.y      = param->dst_crop.top;
    output_config.config.width  = param->dst_crop.getWidth();
    output_config.config.height = param->dst_crop.getHeight();
    output_config.config.pitch  = param->pitch;

    if (param->secure)
        output_config.config.security = DISP_SECURE_BUFFER;
    else
        output_config.config.security = DISP_NORMAL_BUFFER;

    output_config.config.buff_idx = param->fence_index;

    output_config.config.interface_idx = param->if_fence_index;

    output_config.config.frm_sequence  = param->sequence;

    HWC_LOGD("(%d) Output+ (ion=%d/idx=%d/if_idx=%d/sec=%d/fmt=%d"
             "/x=%d y=%d w=%d h=%d s=%d)",
             dpy, param->ion_fd, param->fence_index, param->if_fence_index, param->secure,
             output_config.config.fmt, param->dst_crop.left, param->dst_crop.top,
             param->dst_crop.getWidth(), param->dst_crop.getHeight(), param->pitch);

    ioctl(m_dev_fd, DISP_IOCTL_SET_OUTPUT_BUFFER, &output_config);
}

void DispDevice::prepareOverlayPresentFence(int dpy, OverlayPrepareParam* param)
{
    int session_id = m_input_config[dpy].session_id;
    if (DISP_INVALID_SESSION == session_id)
    {
        HWC_LOGW("(%d) Failed to preapre invalid DispSession (present)", dpy);
        return;
    }

    {
        disp_present_fence fence;

        fence.session_id = session_id;

        int err = ioctl(m_dev_fd, DISP_IOCTL_GET_PRESENT_FENCE  , &fence);

        param->fence_index = fence.present_fence_index;
        param->fence_fd    = fence.present_fence_fd;

        HWC_LOGD("(%d) Prepare Present Fence (id=0x%x, stat=0x%x)", dpy, session_id, err);
    }
}

status_t DispDevice::waitVSync(int dpy, nsecs_t *ts)
{
    int session_id = m_input_config[dpy].session_id;
    if (DISP_INVALID_SESSION == session_id)
    {
        HWC_LOGW("Failed to wait vsync for invalid DispSession (dpy=%d)", dpy);
        return BAD_VALUE;
    }

    disp_session_vsync_config config;
    memset(&config, 0, sizeof(config));

    config.session_id = session_id;

    int err = ioctl(m_dev_fd, DISP_IOCTL_WAIT_FOR_VSYNC, &config);
    if (err < 0)
    {
        HWC_LOGE("Failed to request vsync !! (id=0x%x, dpy=%d, err=%s)", session_id, dpy, strerror(errno));
        return BAD_VALUE;
    }

    *ts = config.vsync_ts;

    return NO_ERROR;
}

void DispDevice::setPowerMode(int dpy,int mode)
{
    if (HWCMediator::getInstance().m_features.control_fb)
    {
        char filename[32] = {0};
        snprintf(filename, sizeof(filename), "/dev/graphics/fb%d", dpy);
        int fb_fd = open(filename, O_RDWR);
        if (fb_fd <= 0) {
            HWC_LOGE("Failed to open fb%d device: %s", dpy, strerror(errno));
            return;
        }

        int err = ioctl(fb_fd, FBIOBLANK, mode ? FB_BLANK_UNBLANK : FB_BLANK_POWERDOWN);
        if (err != NO_ERROR) {
            HWC_LOGE("Failed to %s fb%d device: %s", mode ? "unblank" : "blank", dpy, strerror(errno));
        }

        close(fb_fd);
    }
}

void DispDevice::setODStage(const DISP_OD_ENABLE_STAGE stage)
{
    DISP_OD_CMD od_cmd;
    od_cmd.type = OD_CTL_ENABLE;
    od_cmd.param0 = stage;
    switch (stage)
    {
        case OD_CTL_ENABLE_OFF:
                HWC_LOGD("disable OD");
            break;
        case OD_CTL_ENABLE_ON:
                HWC_LOGD("enable OD");
            break;
        default:
            HWC_LOGE("To set unknown OD stage");
            return;
    }
    int ret = ioctl(m_dev_fd, DISP_IOCTL_OD_CTL, &od_cmd);

    if (ret != 0)
        HWC_LOGE("ioctl(DISP_IOCTL_OD_CTL) failed ret=%d", ret);
}

char const* DispDevice::toString(DISP_MODE mode)
{
    // NOTE: these string literals need to match those in linux/disp_session.h
    switch (mode)
    {
        case DISP_SESSION_DIRECT_LINK_MODE:
            return "DL";

        case DISP_SESSION_DECOUPLE_MODE:
            return "DC";

        case DISP_SESSION_DIRECT_LINK_MIRROR_MODE:
            return "DLM";

        case DISP_SESSION_DECOUPLE_MIRROR_MODE:
            return "DCM";

        case DISP_INVALID_SESSION_MODE:
            return "INV";

        default:
            return "N/A";
    }
}

// ---------------------------------------------------------------------------

ANDROID_SINGLETON_STATIC_INSTANCE(MMUDevice);

MMUDevice::MMUDevice()
{
    m_dev = new MTKM4UDrv();

    if (NULL == m_dev)
    {
        HWC_LOGE("Failed to initialize m4u driver");
        abort();
    }
}

MMUDevice::~MMUDevice()
{
    delete m_dev;
}

int MMUDevice::enable(int client)
{
    return m_dev->m4u_enable_m4u_func((M4U_MODULE_ID_ENUM) client);
}

int MMUDevice::map(int client, unsigned int vaddr, int size, unsigned int* mva)
{
    M4U_MODULE_ID_ENUM module = (M4U_MODULE_ID_ENUM) client;
    M4U_STATUS_ENUM err;

    err = m_dev->m4u_alloc_mva(module, vaddr, size, 0, 0, mva);
    if (M4U_STATUS_OK != err)
    {
        HWC_LOGE("Failed to allocate MVA, client=%d, va=0x%08x, size=%d",
              client, vaddr, size);
        return -1;
    }

    err = m_dev->m4u_insert_tlb_range(module, *mva, *mva + size - 1,
                                      RT_RANGE_HIGH_PRIORITY, 1);
    if (M4U_STATUS_OK != err)
    {
        HWC_LOGE("Failed to insert TLB, client=%d, va=0x%08x, size=%d",
              client, vaddr, size);
        m_dev->m4u_dealloc_mva(module, vaddr, size, *mva);
        return -1;
    }

    // do cache flush once for first usage
    m_dev->m4u_cache_sync(module, M4U_CACHE_FLUSH_BEFORE_HW_READ_MEM, vaddr, size);

    HWC_LOGD("Map MVA(0x%x) with VA(0x%x) Size(%d)", *mva, vaddr, size);

    return 0;
}

int MMUDevice::unmap(int client, unsigned int vaddr, int size, unsigned int mva)
{
    M4U_MODULE_ID_ENUM module = (M4U_MODULE_ID_ENUM) client;
    M4U_STATUS_ENUM err;

    err = m_dev->m4u_invalid_tlb_range(module, mva, mva + size - 1);
    if (M4U_STATUS_OK != err)
    {
        HWC_LOGE("Failed to invalid TLB, client=%d, va=0x%08x, mva=0x%08x, size=%d",
              client, vaddr, mva, size);
        return -1;
    }

    err = m_dev->m4u_dealloc_mva(module, vaddr, size, mva);
    if (M4U_STATUS_OK != err)
    {
        HWC_LOGE("Failed to dealloc MVA, client=%d, va=0x%08x, mva=0x%08x, size=%d",
              client, vaddr, mva, size);
        return -1;
    }

    HWC_LOGD("Unmap MVA(0x%x) with VA(0x%x) Size(%d)", mva, vaddr, size);

    return 0;
}

int MMUDevice::flush(int client, unsigned int vaddr, int size)
{
    M4U_MODULE_ID_ENUM module = (M4U_MODULE_ID_ENUM) client;

    m_dev->m4u_cache_sync(module, M4U_CACHE_FLUSH_BEFORE_HW_READ_MEM, vaddr, size);

    return 0;
}

int MMUDevice::config(int port, bool enabled)
{
    M4U_PORT_STRUCT m4u_port;
    M4U_STATUS_ENUM err;

    m4u_port.ePortID    = (M4U_PORT_ID_ENUM)port;
    m4u_port.Virtuality = (int) enabled;
    m4u_port.Security   = 0;
    m4u_port.Distance   = 1;
    m4u_port.Direction  = 0;

    err = m_dev->m4u_config_port(&m4u_port);
    if (M4U_STATUS_OK != err)
    {
        HWC_LOGE("Failed to config M4U port(%d)", port);
        return -1;
    }

    return 0;
}

// ---------------------------------------------------------------------------

ANDROID_SINGLETON_STATIC_INSTANCE(GrallocDevice);

GrallocDevice::GrallocDevice()
    : m_dev(NULL)
{
    const hw_module_t* module;
    int err = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module);
    if (err)
    {
        HWC_LOGE("Failed to open gralloc device: %s (%d)", strerror(-err), err);
        return;
    }

    gralloc_open(module, &m_dev);
}

GrallocDevice::~GrallocDevice()
{
    if (m_dev) gralloc_close(m_dev);
}

status_t GrallocDevice::alloc(AllocParam& param)
{
    HWC_ATRACE_CALL();
    status_t err;

    if (!m_dev) return NO_INIT;

    if ((param.width == 0) || (param.height == 0))
    {
        HWC_LOGE("Empty buffer(w=%u h=%u) allocation is not allowed",
                param.width, param.height);
        return INVALID_OPERATION;
    }

    err = m_dev->alloc(m_dev,
        param.width, param.height, param.format,
        param.usage, &param.handle, &param.stride);
    if (err == NO_ERROR)
    {
        HWC_LOGD("alloc gralloc memory(%p)", param.handle);
        // TODO: add allocated buffer record
    }
    else
    {
        HWC_LOGE("Failed to allocate buffer(w=%u h=%u f=%d usage=%08x): %s (%d)",
                param.width, param.height, param.format, param.usage,
                strerror(-err), err);
    }

    return err;
}

status_t GrallocDevice::free(buffer_handle_t handle)
{
    HWC_ATRACE_CALL();
    status_t err;

    if (!m_dev) return NO_INIT;

    err = m_dev->free(m_dev, handle);
    if (err == NO_ERROR)
    {
        HWC_LOGD("free gralloc memory(%p)", handle);
        // TODO: remove allocated buffer record
    }
    else
    {
        HWC_LOGE("Failed to free buffer handle(%p): %s (%d)",
                handle, strerror(-err), err);
    }

    return err;
}

void GrallocDevice::dump() const
{
    // TODO: dump allocated buffer record
}
