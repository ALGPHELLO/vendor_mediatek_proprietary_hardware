#ifndef HWC_DISPATCHER_H_
#define HWC_DISPATCHER_H_

#include <utils/threads.h>
#include <utils/SortedVector.h>
#include <utils/BitSet.h>
#include <utils/Singleton.h>

#include "hwc_priv.h"

#include "utils/tools.h"

#include "display.h"
#include "worker.h"
#include "composer.h"

using namespace android;

struct dump_buff;
class SyncFence;
class DispatchThread;

// ---------------------------------------------------------------------------

// HWLayer::type values
enum {
    HWC_LAYER_TYPE_INVALID = 0,
    HWC_LAYER_TYPE_FBT     = 1,
    HWC_LAYER_TYPE_UI      = 2,
    HWC_LAYER_TYPE_MM      = 3,
    HWC_LAYER_TYPE_DIM     = 4,
    HWC_LAYER_TYPE_CURSOR  = 5,
    HWC_LAYER_TYPE_MM_HIGH = 6,
};

// HWLayer::dirty values
enum {
    HWC_LAYER_DIRTY_NONE   = 0x0,
    HWC_LAYER_DIRTY_BUFFER = 0x1,
    HWC_LAYER_DIRTY_PARAM  = 0x2,
    HWC_LAYER_DIRTY_PORT   = 0x4,
    HWC_LAYER_DIRTY_CAMERA = 0x8,
};

// DispatcherJob::post_state values
enum HWC_POST_STATE
{
    HWC_POST_INVALID        = 0x0000,
    HWC_POST_OUTBUF_DISABLE = 0x0010,
    HWC_POST_OUTBUF_ENABLE  = 0x0011,
    HWC_POST_INPUT_NOTDIRTY = 0x0100,
    HWC_POST_INPUT_DIRTY    = 0x0101,
    HWC_POST_MIRROR         = 0x1001,

    HWC_POST_CONTINUE_MASK  = 0x0001,
};

enum {
    HWC_MIRROR_SOURCE_INVALID = -1,
};

enum {
    HWC_SEQUENCE_INVALID = 0,
};

// HWLayer is used to store information of layers selected
// to be processed by hardware composer
struct HWLayer
{
    // used to determine if this layer is enabled
    bool enable;

    // hwc_layer index in hwc_display_contents_1
    int index;

    // identify if layer should be handled by UI or MM path
    int type;

    // identify if layer has dirty pixels
    bool dirty;

    union
    {
        // information of UI layer
        struct
        {
#ifdef MTK_HWC_PROFILING
            // amount of layers handled by GLES
            int fbt_input_layers;

            // bytes of layers handled by GLES
            int fbt_input_bytes;
#endif

            // used by fbt of fake display
            // TODO: add implementation of this debug feature
            int fake_ovl_id;
        };

        // information of MM layer
        struct
        {
            // used to queue back to display buffer queue at MM thread processing
            // TODO: used by where?
            int disp_fence;

            // used to identify queued frame
            unsigned int sync_marker;
        };
    };

    // index of release fence from display driver
    unsigned int fence_index;

    // hwc_layer_1 from SurfaceFlinger
    hwc_layer_1 layer;

    // private handle information
    PrivateHandle priv_handle;
};

// HWBuffer is used to store buffer information of
// 1. virtual display
// 2. mirror source
struct HWBuffer
{
    HWBuffer()
        : handle(NULL)
    { }

    union
    {
        // struct used for phyical display
        struct
        {
            // phy_present_fence_fd is used for present fence to notify hw vsync
            int phy_present_fence_fd;

            // phy_present_fence_idx is present fence index from display driver
            unsigned int phy_present_fence_idx;
        };

        // struct used by virtual display
        struct
        {
            // out_acq_fence_fd is used for hwc to know if outbuf could be read
            int out_acquire_fence_fd;

            // out_retire_fence_fd is used to notify producer
            // if output buffer is ready to be written
            int out_retire_fence_fd;

            // out_retire_fence_idx is retire fence index from display driver
            unsigned int out_retire_fence_idx;
        };
    };

    union
    {
        // struct used by mirror producer (overlay engine)
        struct
        {
            // mir_out_sec_handle is secure handle for mirror output if needed
            SECHAND mir_out_sec_handle;

            // mir_out_rel_fence_fd is fence to notify
            // all producer display in previous round are finished
            // and mirror buffer is ready to be written
            int mir_out_rel_fence_fd;

            // mir_out_acq_fence_fd is fence to notify
            // when mirror buffer's contents are available
            int mir_out_acq_fence_fd;

            // mir_out_acq_fence_idx is fence index from display driver
            unsigned int mir_out_acq_fence_idx;

            // mir_out_if_fence_fd is fence to notify
            // all producer display in previous round are finished
            // and mirror buffer is ready to be written
            int mir_out_if_fence_fd;

            // mir_out_if_fence_idx is fence index from display driver (decouple mode)
            unsigned int mir_out_if_fence_idx;
        };

        // struct used by mirror consumer (DpFramework)
        struct
        {
            // mir_in_acq_fence_fd is used to notify
            // when mirror buffer's contents are available
            int mir_in_acq_fence_fd;

            // mir_in_rel_fence_fd is used to notify
            // mirror source that buffer is ready to be written
            int mir_in_rel_fence_fd;

            // mir_in_sync_marker is used for identifying timeline marker
            unsigned int mir_in_sync_marker;
        };
    };

    // outbuf native handle
    buffer_handle_t handle;

    // private handle information
    PrivateHandle priv_handle;
};

// DispatcherJob is a display job unit and is used by DispatchThread
struct DispatcherJob
{
    DispatcherJob()
        : enable(false)
        , protect(false)
        , secure(false)
        , mirrored(false)
        , disp_ori_id(0)
        , disp_mir_id(HWC_MIRROR_SOURCE_INVALID)
        , disp_ori_rot(0)
        , disp_mir_rot(0)
        , num_layers(0)
        , ovl_valid(false)
        , fbt_exist(false)
        , need_flush(false)
        , need_sync(false)
        , triggered(false)
        , force_wait(false)
        , num_ui_layers(0)
        , num_mm_layers(0)
        , post_state(HWC_POST_INVALID)
        , sequence(HWC_SEQUENCE_INVALID)
        , timestamp(0)
        , prev_present_fence_fd(-1)
    { }

    // check if job should be processed
    bool enable;

    // check if any protect layer exist
    bool protect;

    // check if job should use secure composition
    bool secure;

    // check if job acts as a mirror source
    // if yes, it needs to provide a mirror output buffer to one another
    bool mirrored;

    // check if mirror output buffer has been set by UI/MM composers
    bool mirror_triggered;

    // display id
    int disp_ori_id;

    // display id of mirror source
    int disp_mir_id;

    // orientation of display
    unsigned int disp_ori_rot;

    // orientation of mirror source
    unsigned int disp_mir_rot;

    // amount of the current available overlay inputs
    int num_layers;

    // check if ovl engine has availale inputs
    bool ovl_valid;

    // check if fbt is used
    bool fbt_exist;

    // check if need to wait for previous jobs
    bool need_flush;

    // check if UI and MM composers should be synced
    bool need_sync;

    // check if the current job is triggered
    bool triggered;

    // [WORKAROUND]
    // force to wait until job is finished
    bool force_wait;

    // amount of layers for UI composer
    int num_ui_layers;

    // amount of layers for MM composer
    int num_mm_layers;

    // used to determine if need to trigger UI/MM composers
    int post_state;

    // used as a sequence number for profiling latency purpose
    unsigned int sequence;

    // used for video frame
    unsigned int timestamp;

    // input layers for compositing
    HWLayer* hw_layers;

    // mirror source buffer for bitblit
    HWBuffer hw_mirbuf;

    // the output buffer of
    // 1. virtual display
    // 2. mirror source
    HWBuffer hw_outbuf;

    // store present fence fd
    int prev_present_fence_fd;
};

// HWCDispatcher is used to dispatch layers to DispatchThreads
class HWCDispatcher : public Singleton<HWCDispatcher>
{
public:
    // onPlugIn() is used to notify if a new display is added
    void onPlugIn(int dpy);

    // onPlugOut() is used to notify if a new display is removed
    void onPlugOut(int dpy);

    // setPowerMode() is used to wait display thread idle when display changes power mode
    void setPowerMode(int dpy, int mode);

    // onVSync() is used to receive vsync signal
    void onVSync(int dpy);

    // verifyType() is used to verify if layer type is changed then set as dirty
    int verifyType(int dpy, PrivateHandle* priv_handle,
        int idx, int dirty, int type);

    // setSessionMode() is used to set display session mode
    void setSessionMode(int dpy, bool mirrored);

    // configMirrorJob() is used to config job as mirror source
    void configMirrorJob(DispatcherJob* job);

    // getJob() is used for HWCMediator to get a new job for filling in
    DispatcherJob* getJob(int dpy);

    // setJob() is used to update jobs in hwc::set()
    void setJob(int dpy, struct hwc_display_contents_1* list);

    // trigger() is used to queue job and trigger dispatchers to work
    void trigger();

    struct VSyncListener : public virtual RefBase {
        // onVSync() is used to receive the vsync signal
        virtual void onVSync() = 0;
    };

    // registerVSyncListener() is used to register a VSyncListener to HWCDispatcher
    void registerVSyncListener(int dpy, const sp<VSyncListener>& listener);

    // removeVSyncListener() is used to remove a VSyncListener to HWCDispatcher
    void removeVSyncListener(int dpy, const sp<VSyncListener>& listener);

    // dump() is used for debugging purpose
    void dump(struct dump_buff* log, int dump_level);

    // saveFbt() is used to save present FBT handle
    // return true, if present FBT is the different with previous
    bool saveFbtHandle(int dpy, buffer_handle_t handle);

private:
    friend class Singleton<HWCDispatcher>;
    friend class DispatchThread;

    HWCDispatcher();
    ~HWCDispatcher();

    // releaseResourceLocked() is used to release resources in display's WorkerCluster
    void releaseResourceLocked(int dpy);

    // access must be protected by m_vsync_lock
    mutable Mutex m_vsync_lock;
    // m_vsync_callbacks is a queue of VSyncListener registered by DispatchThread
    SortedVector< sp<VSyncListener> > m_vsync_callbacks;

    // m_alloc_disp_ids is a bit set of displays
    // each bit index with a 1 corresponds to an valid display session
    BitSet32 m_alloc_disp_ids;

    // m_curr_jobs holds DispatcherJob of all displays
    // and is used between prepare() and set().
    DispatcherJob* m_curr_jobs[DisplayManager::MAX_DISPLAYS];

    class PostHandler : public LightRefBase<PostHandler>
    {
    public:
        PostHandler(HWCDispatcher* dispatcher,
            int dpy, const sp<OverlayEngine>& ovl_engine);

        virtual ~PostHandler();

        // set() is used to get retired fence from display driver
        virtual void set(struct hwc_display_contents_1* list, DispatcherJob* job) = 0;

        // setMirror() is used to fill dst_job->hw_mirbuf
        // from src_job->hw_outbuf
        virtual void setMirror(DispatcherJob* src_job, DispatcherJob* dst_job) = 0;

        // process() is used to wait outbuf is ready to use
        // and sets output buffer to display driver
        virtual void process(DispatcherJob* job) = 0;

    protected:
        // m_dispatcher is used for callback usage
        HWCDispatcher* m_dispatcher;

        // m_disp_id is used to identify which display
        int m_disp_id;

        // m_ovl_engine is used for config overlay engine
        sp<OverlayEngine> m_ovl_engine;

        // m_sync_fence is used to create or wait fence
        sp<SyncFence> m_sync_fence;

        // store the presentfence fd
        int m_curr_present_fence_fd;
    };

    class PhyPostHandler : public PostHandler
    {
    public:
        PhyPostHandler(HWCDispatcher* dispatcher,
            int dpy, const sp<OverlayEngine>& ovl_engine)
            : PostHandler(dispatcher, dpy, ovl_engine)
        { }

        virtual void set(struct hwc_display_contents_1* list, DispatcherJob* job);

        virtual void setMirror(DispatcherJob* src_job, DispatcherJob* dst_job);

        virtual void process(DispatcherJob* job);
    };

    class VirPostHandler : public PostHandler
    {
    public:
        VirPostHandler(HWCDispatcher* dispatcher,
            int dpy, const sp<OverlayEngine>& ovl_engine)
            : PostHandler(dispatcher, dpy, ovl_engine)
        { }

        virtual void set(struct hwc_display_contents_1* list, DispatcherJob* job);

        virtual void setMirror(DispatcherJob* src_job, DispatcherJob* dst_job);

        virtual void process(DispatcherJob* job);

    private:
        void setError(DispatcherJob* job);
    };

    // WorkerCluster is used for processing composition of single display.
    // One WokerCluster would creates
    // 1. one thread to handle a job list and
    // 2. two threads to handle UI or MM layers.
    // Different display would use different WorkCluster to handle composition.
    struct WorkerCluster
    {
        WorkerCluster()
            : enable(false)
            , force_wait(false)
            , ovl_engine(NULL)
            , dp_thread(NULL)
            , ui_thread(NULL)
            , mm_thread(NULL)
            , post_handler(NULL)
            , sync_ctrl(NULL)
        { }

        // access must be protected by lock (DispatchThread <-> Hotplug thread)
        mutable Mutex plug_lock_loop;
        // access must be protected by lock (SurfaceFlinger <-> VSyncThread)
        mutable Mutex plug_lock_main;
        // access must be protected by lock (Hotplug thread <-> VSyncThread)
        mutable Mutex plug_lock_vsync;

        bool enable;
        bool force_wait;

        sp<OverlayEngine> ovl_engine;
        sp<DispatchThread> dp_thread;
        sp<UILayerComposer> ui_thread;
        sp<MMLayerComposer> mm_thread;
        sp<PostHandler> post_handler;
        sp<SyncControl> sync_ctrl;

        struct HWLayerTypes
        {
            int type;
            int pool_id;
        };
        HWLayerTypes* hw_layer_types;
    };

    // m_workers is the WorkerCluster array used by different display composition.
    WorkerCluster m_workers[DisplayManager::MAX_DISPLAYS];

    // m_sequence is used as a sequence number for profiling latency purpose
    // initialized to 1, (0 is reserved to be an error code)
    unsigned int m_sequence;

    // used to save the handle of previous fbt. When need to skip redundant
    // composition, compare the present fbt.
    buffer_handle_t m_prev_fbt[DisplayManager::MAX_DISPLAYS];

public:
    // enable/disable skip redundant composition
    bool m_disable_skip_redundant;
};

// DispatchThread handles DispatcherJobs
// from UILayerComposer and MMLayerComposer
class DispatchThread : public HWCThread,
                       public HWCDispatcher::VSyncListener
{
public:
    DispatchThread(int dpy);

    // trigger() is used to add a dispatch job into a job queue,
    // then triggers DispatchThread
    void trigger(DispatcherJob* job);

private:
    virtual void onFirstRef();
    virtual bool threadLoop();

    // waitNextVSyncLocked() requests and waits for the next vsync signal
    void waitNextVSyncLocked(int dpy);

    /* ------------------------------------------------------------------------
     * VSyncListener interface
     */
    void onVSync();

    // m_disp_id is used to identify which display
    // DispatchThread needs to handle
    int m_disp_id;

    // m_job_queue is a job queue
    // which new job would be queued in set()
    typedef Vector<DispatcherJob*> Fifo;
    Fifo m_job_queue;

    // access must be protected by m_vsync_lock
    mutable Mutex m_vsync_lock;
    Condition m_vsync_cond;
    bool m_vsync_signaled;
};

#endif // HWC_DISPATCHER_H_
