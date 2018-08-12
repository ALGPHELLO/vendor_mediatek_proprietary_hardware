#ifndef HWC_BLITER_ULTRA_H_
#define HWC_BLITER_ULTRA_H_

#include "queue.h"
#include "bliter_async.h"
#include "utils/tools.h"

#define IS_MASTER(dpy) (dpy == HWC_DISPLAY_PRIMARY)
#define ID(ISMASTER) (ISMASTER ? ID_MASTER : ID_SLAVE)

using namespace android;

typedef struct AsyncBliterHandler::BufferConfig BufferConfig;
typedef struct DisplayBufferQueue::DisplayBuffer DisplayBuffer;

struct HWLayer;

class BliterNode
{
public:
    enum
    {
        MAX_OUT_CNT = 4,
    };

    struct BufferInfo
    {
        BufferInfo()
            : fence_fd(-1)
            , ion_fd(-1)
            , handle(nullptr)
            , sec_handle(0)
            , dpformat(DP_COLOR_YUYV)
            , pitch(0)
            , pitch_uv(0)
            , plane(1)
        {}
        int ion_fd;
        SECHAND sec_handle;
        buffer_handle_t handle;
        Rect rect;
        int fence_fd;

        // dst buffer setting for DpFramework
        DP_COLOR_ENUM dpformat;
        unsigned int  pitch;
        unsigned int  pitch_uv;
        unsigned int  plane;
        unsigned int  size[3];
    };

    struct SrcInvalidateParam
    {
        BufferInfo bufInfo;

        unsigned int gralloc_color_range;
        bool deinterlace;
        bool is_secure;
        bool is_flush;

        int32_t pool_id;
        uint32_t time_stamp;
    };

    struct DstInvalidateParam
    {
        DstInvalidateParam()
            : enable(false)
            , xform(0)
            , is_enhance(false)
        {}
        bool enable;
        BufferInfo bufInfo;
        uint32_t xform;
        Rect src_crop;
        Rect dst_crop;
        bool is_enhance;
    };

    struct Parameter
    {
        Rect            src_roi;
        Rect            dst_roi;
        BufferConfig*   config;
        uint32_t        xform;
        bool            pq_enhance;
    };

    BliterNode(DpAsyncBlitStream* blit_stream, uint32_t dpy);

    ~BliterNode();

    SrcInvalidateParam& getSrcInvalidateParam() { return m_src_param; }

    DstInvalidateParam& getDstInvalidateParam(uint32_t i) { return m_dst_param[i >= MAX_OUT_CNT ? 0 : i]; }

    void setSrc(BufferConfig* config, PrivateHandle& src_priv_handle, int* src_fence_fd = NULL);

    void setDst(uint32_t idx, Parameter* param, int ion_fd, SECHAND sec_handle, int* dst_fence_fd = NULL);

    void cancelJob(uint32_t job_id);

    status_t invalidate(uint32_t job_id);

private:
    static status_t calculateROI(DpRect* src_roi, DpRect* dst_roi,
        Rect& src_crop, Rect& dst_crop,
        Rect& dst_buf, bool deinterlace, const int& color_format);

    unsigned int mapDpOrientation(const uint32_t transform);

    DP_PROFILE_ENUM mapDpColorRange(const uint32_t range);

    status_t errorCheck();

    DpAsyncBlitStream* m_blit_stream;

    SrcInvalidateParam m_src_param;

    DstInvalidateParam m_dst_param[MAX_OUT_CNT];

    DbgLogger* m_config_logger;

    DbgLogger* m_geometry_logger;

    DbgLogger* m_buffer_logger;

    uint32_t m_dpy;

    bool m_bypass_mdp_for_debug;
};

class UltraBliter : public Singleton<UltraBliter>
{
public:
    enum INDEX
    {
        ID_MASTER,
        ID_SLAVE,
    };

    enum SyncState
    {
        SyncState_init,
        SyncState_start,
        SyncState_set_buf_done,
        SyncState_trig_done,
    };

    enum TrigAction
    {
        DoJob,
        CancelJob,
    };

    UltraBliter();

    ~UltraBliter();

    void setBliter(BliterNode* blit_node);

    void masterSetSrcBuf(HWLayer* buf, uint32_t job_seq);

    void setDstBuf(bool is_master, DisplayBuffer* buf);

    void config(int is_master, BliterNode::Parameter* param);

    status_t trig(bool is_master, TrigAction action, int* mdp_fence_fd = NULL);

    inline void debug(bool on_off) { m_is_debug_on = on_off; }

private:
    status_t processJob();

    void barrier(bool is_master, SyncState sync_state);

    BliterNode* m_bliter_node;
    DpAsyncBlitStream* m_blit_stream;

    HWLayer* m_src_buf;
    DisplayBuffer* m_dst_buf[2];
    BliterNode::Parameter* m_param[2];

    uint32_t m_job_seq;

    mutable Mutex m_lock;
    Condition m_condition;
    SyncState m_sync_state;

    mutable Mutex m_lock_inverse;
    Condition m_condition_inverse;

    status_t m_status;
    TrigAction m_action[2];

    bool m_is_debug_on;
};
#endif
