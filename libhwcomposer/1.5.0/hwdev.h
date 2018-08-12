#ifndef HWC_HWDEV_H_
#define HWC_HWDEV_H_

#include <ui/Rect.h>
#include <utils/Singleton.h>
#include <utils/threads.h>

#include <linux/disp_session.h>

#include "display.h"

#define DISP_NO_PRESENT_FENCE  ((int)(~0U>>1))
#define DISP_DO_NOTHING -1

using namespace android;

struct OverlayPrepareParam;
struct OverlayPortParam;
class MTKM4UDrv;

// ---------------------------------------------------------------------------

class OvlInSizeArbitrator : public Singleton<OvlInSizeArbitrator>
{
public:
    OvlInSizeArbitrator();

    enum CONFIG
    {
        CONFIG_FIXED_HI,
        CONFIG_FIXED_LO,
        CONFIG_ADAPTIVE,
        CONFIG_ADAPTIVE_HILO,
        CONFIG_COUNT,
    };

    struct InputSize
    {
        InputSize()
            : w(0), h(0)
        { }
        uint32_t w;
        uint32_t h;
    };

    void dump(struct dump_buff* log);
    void config(uint32_t width, uint32_t height);
    bool isConfigurationDirty();
    void adjustMdpDstRoi(DispatcherJob* job, hwc_display_contents_1_t* list);

private:
    void switchBackHi();
    void switchToLo(uint32_t width, uint32_t height);
    void getMediumRoi(hwc_frect_t* medium_roi, const InputSize& medium_base, hwc_rect_t& frame);
    void getFixedMediumRoi(Rect* medium_size, hwc_frect_t& f_medium_size);

    uint32_t m_disp_w;

    uint32_t m_disp_h;

    uint32_t m_present_config;

    // for checking config dirty
    uint32_t m_prior_config;

    // is resolution set to lo
    bool m_is_lo;

    // configuration for lo resolution mode
    uint32_t m_lr_config;

    // configuration for lo resolution mode
    uint32_t m_hr_config;

    // for each configuration, there a resolution list used to do arbitration
    KeyedVector<int, InputSize> m_input_size[CONFIG_COUNT];
};

// ---------------------------------------------------------------------------

class DispDevice : public Singleton<DispDevice>
{
public:
    DispDevice();
    ~DispDevice();

    // initOverlay() initializes overlay related hw setting
    void initOverlay();

    // get supported resolutions
    void getSupportedResolution(KeyedVector<int, OvlInSizeArbitrator::InputSize>& config);

    // isDispRszSupported() is used to query if display rsz is supported
    bool isDispRszSupported();

    // isPartialUpdateSupported() is used to query if PartialUpdate is supported
    bool isPartialUpdateSupported();

    // isFenceWaitSupported() is used to query if FenceWait is supported
    bool isFenceWaitSupported();

    // getMaxOverlayInputNum() gets overlay supported input amount
    int getMaxOverlayInputNum();

    // createOverlaySession() creates overlay composition session
    status_t createOverlaySession(
        int dpy, DISP_MODE mode = DISP_SESSION_DIRECT_LINK_MODE);

    // destroyOverlaySession() destroys overlay composition session
    void destroyOverlaySession(int dpy);

    // truggerOverlaySession() used to trigger overlay engine to do composition
    status_t triggerOverlaySession(int dpy, int present_fence_idx, int ovlp_layer_num,
                                   int prev_present_fence_fd,
                                   EXTD_TRIGGER_MODE trigger_mode = TRIGGER_NORMAL);

    // disableOverlaySession() usd to disable overlay session to do composition
    void disableOverlaySession(int dpy,  OverlayPortParam* const* params, int num);

    // setOverlaySessionMode() sets the overlay session mode
    status_t setOverlaySessionMode(int dpy, DISP_MODE mode);

    // getOverlaySessionMode() gets the overlay session mode
    DISP_MODE getOverlaySessionMode(int dpy);

    // getOverlaySessionInfo() gets specific display device information
    status_t getOverlaySessionInfo(int dpy, disp_session_info* info);

    // getAvailableOverlayInput gets available amount of overlay input
    // for different session
    int getAvailableOverlayInput(int dpy);

    // prepareOverlayInput() gets timeline index and fence fd of overlay input layer
    void prepareOverlayInput(int dpy, OverlayPrepareParam* param);

    // updateOverlayInputs() updates multiple overlay input layers
    void updateOverlayInputs(int dpy, OverlayPortParam* const* params, int num);

    // prepareOverlayOutput() gets timeline index and fence fd for overlay output buffer
    void prepareOverlayOutput(int dpy, OverlayPrepareParam* param);

    // disableOverlayOutput() disables overlay output buffer
    void disableOverlayOutput(int dpy);

    // enableOverlayOutput() enables overlay output buffer
    void enableOverlayOutput(int dpy, OverlayPortParam* param);

    // prepareOverlayPresentFence() gets present timeline index and fence
    void prepareOverlayPresentFence(int dpy, OverlayPrepareParam* param);

    // waitVSync() is used to wait vsync signal for specific display device
    status_t waitVSync(int dpy, nsecs_t *ts);

    // setPowerMode() is used to switch power setting for display
    void setPowerMode(int dpy, int mode);

    // toString returns the string literal of session mode
    static char const* toString(DISP_MODE mode);

    inline disp_caps_info* getCapsInfo() { return &m_caps_info; }

    void fillLayerConfigList(const int dpy, DispatcherJob* job, hwc_display_contents_1_t* list);

    // to query valid layers which can handled by OVL
    bool queryValidLayer(const size_t num_display, hwc_display_contents_1_t** displays, disp_layer_info* disp_layer);

    // waitAllJobDone() use to wait driver for processing all job
    status_t waitAllJobDone(const int dpy);

    // MAX_DIRTY_RECT_CNT hwc supports
    enum
    {
        MAX_DIRTY_RECT_CNT = 10,
    };

private:

    // for new driver API from MT6755
    status_t frameConfig(int dpy, int present_fence_idx, int ovlp_layer_num,
                         int prev_present_fence_fd,
                         EXTD_TRIGGER_MODE trigger_mode);

    // query hw capabilities through ioctl and store in m_caps_info
    status_t queryCapsInfo();

    // get the correct device id for extension display when enable dual display
    unsigned int getDeviceId(int dpy);

    enum
    {
        DISP_INVALID_SESSION = -1,
    };

    int m_dev_fd;

    int m_ovl_input_num;

    disp_frame_cfg_t m_frame_cfg[DisplayManager::MAX_DISPLAYS];

    disp_caps_info m_caps_info;

    layer_config* m_layer_config_list[DisplayManager::MAX_DISPLAYS];

    int m_layer_config_len[DisplayManager::MAX_DISPLAYS];

    layer_dirty_roi** m_hwdev_dirty_rect[DisplayManager::MAX_DISPLAYS];
};

// --------------------------------------------------------------------------

class GrallocDevice : public Singleton<GrallocDevice>
{
public:
    GrallocDevice();
    ~GrallocDevice();

    struct AllocParam
    {
        AllocParam()
            : width(0), height(0), format(0)
            , usage(0), handle(NULL), stride(0)
        { }

        unsigned int width;
        unsigned int height;
        int format;
        int usage;

        buffer_handle_t handle;
        int stride;
    };

    // allocate memory by gralloc driver
    status_t alloc(AllocParam& param);

    // free a previously allocated buffer
    status_t free(buffer_handle_t handle);

    // dump information of allocated buffers
    void dump() const;

private:
    alloc_device_t* m_dev;
};

#endif // HWC_HWDEV_H_
