#ifndef HWC_PLATFORM_H_
#define HWC_PLATFORM_H_

#include <utils/Singleton.h>
#include <ui/Rect.h>

using namespace android;

struct hwc_layer_1;
struct PrivateHandle;

// ---------------------------------------------------------------------------

enum OVL_GEN {
    OVL_GEN_6589 = 0x0001,
    OVL_GEN_6582 = 0x0002,
    OVL_GEN_6592 = 0x0004,
    OVL_GEN_6595 = 0x0008,
    OVL_GEN_6752 = 0x0010,
    OVL_GEN_6735 = 0x0011,
};

enum MDP_GEN {
    MDP_GEN_6589 = 0x0001,
    MDP_GEN_6582 = 0x0002,
    MDP_GEN_6595 = 0x0004,
};

enum PLATFORM_INFO {
    PLATFORM_NOT_DEFINE = 0,

    PLATFORM_MT6589 = OVL_GEN_6589 | (MDP_GEN_6589 << 16),
    PLATFORM_MT8135 = OVL_GEN_6589 | (MDP_GEN_6589 << 16),

    PLATFORM_MT6572 = OVL_GEN_6582 | (MDP_GEN_6582 << 16),
    PLATFORM_MT6582 = OVL_GEN_6582 | (MDP_GEN_6582 << 16),
    PLATFORM_MT8127 = OVL_GEN_6582 | (MDP_GEN_6582 << 16),

    PLATFORM_MT6571 = OVL_GEN_6592 | (MDP_GEN_6582 << 16),
    PLATFORM_MT6592 = OVL_GEN_6592 | (MDP_GEN_6582 << 16),

    PLATFORM_MT6595 = OVL_GEN_6595 | (MDP_GEN_6595 << 16),
    PLATFORM_MT6795 = OVL_GEN_6595 | (MDP_GEN_6595 << 16),

    PLATFORM_MT6752 = OVL_GEN_6752 | (MDP_GEN_6595 << 16),

    PLATFORM_MT6735 = OVL_GEN_6735 | (MDP_GEN_6595 << 16),
};

enum HWC_MIRROR_STATE {
    MIRROR_UNDEFINED = 0, // reserved to avoid using this state by accident

    MIRROR_ENABLED   = (1 << 0),
    MIRROR_PAUSED    = (1 << 1),
    MIRROR_DISABLED  = (1 << 2),
};

enum HWC_OVL_CAPABILITY {
    OVL_CAP_UNDEFINE    = 0,
    OVL_CAP_DIM         = (1 << 0),
    OVL_CAP_DIM_HW      = (1 << 1),
    OVL_CAP_P_FENCE     = (1 << 2),
    OVL_CAP_EXTRA_OVL   = (1 << 3),
    OVL_CAP_TIMESHARING = (1 << 4),
};

enum HWC_MIR_FORMAT {
    MIR_FORMAT_UNDEFINE = 0,
    MIR_FORMAT_RGB888   = 1,
    MIR_FORMAT_YUYV     = 2,
    MIR_FORMAT_YV12     = 3,
};

class DevicePlatform : public Singleton<DevicePlatform>
{
public:
    // COND_NOT_ALIGN:
    //   pitch of buffers do not align any value
    // COND_ALIGNED:
    //   pitch of buffers is aligned
    // COND_ALIGNED_SHIFT:
    //   pitch of buffers is aligned, and start address of each line is shifted
    enum ALIGN_CONDITION {
        COND_NOT_ALIGN     = 0,
        COND_ALIGNED       = 1,
        COND_ALIGNED_SHIFT = 2
    };

    DevicePlatform();
    ~DevicePlatform() { };

    // initOverlay() is used to init overlay related setting
    void initOverlay();

    // isUILayerValid() is ued to verify
    // if ui layer could be handled by hwcomposer
    bool isUILayerValid(int dpy, struct hwc_layer_1* layer,
            PrivateHandle* priv_handle);

    // isMMLayerValid() is used to verify
    // if mm layer could be handled by hwcomposer
    bool isMMLayerValid(int dpy, struct hwc_layer_1* layer,
            PrivateHandle* priv_handle, bool& is_high);

    // getBandwidthLimit() is used to get
    // limitataion of bandwith
    // return -1 if there is no limitation
    int getBandwidthLimit();

    // For 82 ovl hw limitation, pitch of buffers must allign 128 bytes.
    int computePitch(const int& pitch_bytes, const int& width_bytes, int* status);

    int getRotateInfo(buffer_handle_t handle, gralloc_rotate_info_t* rotate_info);

    int setLayerOffsetInfo(Rect &rect,int dx,int dy);

    struct PlatformConfig
    {
        PlatformConfig()
            : platform(PLATFORM_NOT_DEFINE)
            , compose_level(COMPOSE_DISABLE_ALL)
            , mirror_state(MIRROR_DISABLED)
            , overlay_cap(OVL_CAP_UNDEFINE)
            , client_id(-1)
            , bq_count(3)
            , mir_scale_ratio(0.0f)
            , format_mir_mhl(MIR_FORMAT_UNDEFINE)
            , bytes_limit(0)
            , shift_bytes(0)
            , bypass_hdcp_checking(false)
            , alloc_sec_decouple_buf(true)
        { }

        // platform define related hw family, includes ovl and mdp engine
        int platform;

        // compose_level defines default compose level
        int compose_level;

        // mirror_state defines mirror enhancement state
        int mirror_state;

        // overlay engine's capability
        int overlay_cap;

        // client_id identifies m4u client id
        int client_id;

        // bq_count defines suggested amounts for bufferqueue
        int bq_count;

        // mir_scale_ratio defines the maxinum scale ratio of mirror source
        float mir_scale_ratio;

        // format_mir_mhl defines which color format
        // should be used as mirror result for MHL
        int format_mir_mhl;

        // bytes_limit is used for debug purpose
        int bytes_limit;

        // ovl_overlap_limit is max number of overlap layers in one OVL composition
        // To avoid OVL from unferflow situation
        int ovl_overlap_limit;

        // TODO: should be removed after mt6582 phasing out
        int shift_bytes;

        // hdcp checking id handled by display driver
        bool bypass_hdcp_checking;

        // alloc secure decouple buffer for display driver
        bool alloc_sec_decouple_buf;
    };
    static PlatformConfig m_config;
};

#endif // HWC_PLATFORM_H_
