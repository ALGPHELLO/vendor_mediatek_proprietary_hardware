#ifndef HWC_PLATFORM_COMMON_H_
#define HWC_PLATFORM_COMMON_H_

#include <utils/Singleton.h>

using namespace android;

struct hwc_layer_1;
struct PrivateHandle;

// ---------------------------------------------------------------------------

enum OVL_GEN {
    OVL_GEN_6595 = 0x0008,
    OVL_GEN_6752 = 0x0010,
    OVL_GEN_6755 = 0x0020,
    OVL_GEN_6797 = 0x0040,
    OVL_GEN_6799 = 0x0080,
    OVL_GEN_6759 = 0x0100,
};

enum MDP_GEN {
    MDP_GEN_6595 = 0x0004,
    MDP_GEN_6797 = 0x0008,
    MDP_GEN_6757 = 0x0010,
};

enum PLATFORM_INFO {
    PLATFORM_NOT_DEFINE = 0,

    PLATFORM_MT6595 = OVL_GEN_6595 | (MDP_GEN_6595 << 16),
    PLATFORM_MT6795 = OVL_GEN_6595 | (MDP_GEN_6595 << 16),

    PLATFORM_MT6752 = OVL_GEN_6752 | (MDP_GEN_6595 << 16),
    PLATFORM_MT6755 = OVL_GEN_6755 | (MDP_GEN_6595 << 16),
    PLATFORM_MT6757 = OVL_GEN_6755 | (MDP_GEN_6757 << 16),
    PLATFORM_MT6763 = OVL_GEN_6759 | (MDP_GEN_6595 << 16),
    PLATFORM_MT6797 = OVL_GEN_6797 | (MDP_GEN_6797 << 16),
    PLATFORM_MT6799 = OVL_GEN_6799 | (MDP_GEN_6797 << 16),
    PLATFORM_MT6759 = OVL_GEN_6759 | (MDP_GEN_6797 << 16),
};

enum HWC_MIRROR_STATE {
    MIRROR_UNDEFINED = 0, // reserved to avoid using this state by accident

    MIRROR_ENABLED   = (1 << 0),
    MIRROR_PAUSED    = (1 << 1),
    MIRROR_DISABLED  = (1 << 2),
};

enum HWC_OVL_CAPABILITY {
    OVL_CAP_UNDEFINE = 0,
    OVL_CAP_DIM      = (1 << 0),
    OVL_CAP_DIM_HW   = (1 << 1),
    OVL_CAP_P_FENCE  = (1 << 2),
};

enum HWC_MIR_FORMAT {
    MIR_FORMAT_UNDEFINE = 0,
    MIR_FORMAT_RGB888   = 1,
    MIR_FORMAT_YUYV     = 2,
    MIR_FORMAT_YV12     = 3,
};

// An abstract class of Platform. Each function of Platform must have a condidate in
// PlatformCommon to avoid compilation error except pure virtual functions.
class PlatformCommon
{
public:
    PlatformCommon() { };
    virtual ~PlatformCommon() { };

    // initOverlay() is used to init overlay related setting
    void initOverlay();

    // isUILayerValid() is ued to verify
    // if ui layer could be handled by hwcomposer
    virtual bool isUILayerValid(int dpy, struct hwc_layer_1* layer,
            PrivateHandle* priv_handle);

    // isMMLayerValid() is used to verify
    // if mm layer could be handled by hwcomposer
    virtual bool isMMLayerValid(int dpy, struct hwc_layer_1* layer,
            PrivateHandle* priv_handle, bool& is_high);

    // getUltraVideoSize() is used to return the limitation of video resolution
    // when this device connect with the maximum resolution of external display
    size_t getLimitedVideoSize();

    // getUltraDisplaySize() is used to return the limitation of external display
    // when this device play maximum resolution of video
    size_t getLimitedExternalDisplaySize();

    struct PlatformConfig
    {
        PlatformConfig();

        // platform define related hw family, includes ovl and mdp engine
        int platform;

        // compose_level defines default compose level
        int compose_level;

        // mirror_state defines mirror enhancement state
        int mirror_state;

        // overlay engine's capability
        int overlay_cap;

        // bq_count defines suggested amounts for bufferqueue
        int bq_count;

        // mir_scale_ratio defines the maxinum scale ratio of mirror source
        float mir_scale_ratio;

        // format_mir_mhl defines which color format
        // should be used as mirror result for MHL
        int format_mir_mhl;

        // ovl_overlap_limit is max number of overlap layers in one OVL composition
        // To avoid OVL from unferflow situation
        int ovl_overlap_limit;

        // can UI process prexform buffer
        int prexformUI;

        // can rdma support roi update
        int rdma_roi_update;

        // force full invalidate for partial update debug through setprop
        bool force_full_invalidate;

        // use_async_bliter defines the bliter mode
        bool use_async_bliter;

        // use async bliter ultra
        bool use_async_bliter_ultra;

        // force hwc to wait fence for display
        bool wait_fence_for_display;

        // Smart layer switch
        bool enable_smart_layer;

        // force hwc to wait fence for display
        bool enable_rgba_rotate;

        // hdcp checking id handled by display driver
        bool bypass_hdcp_checking;
    };
    PlatformConfig m_config;
};

#endif // HWC_PLATFORM_H_
