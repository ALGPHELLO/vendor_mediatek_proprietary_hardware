#ifndef HWC_OVERLAY_H_
#define HWC_OVERLAY_H_

#include <ui/Rect.h>
#include <utils/Singleton.h>
#include <utils/Vector.h>

#include <linux/disp_session.h>

#include <hwc_common/pool.h>

#define HWLAYER_ID_NONE -1
#define HWLAYER_ID_DBQ  -20

using namespace android;

class DisplayBufferQueue;
struct HWBuffer;
struct dump_buff;

// ---------------------------------------------------------------------------

struct OverlayPrepareParam
{
    OverlayPrepareParam()
        : id(-1)
        , ion_fd(-1)
        , is_need_flush(0)
        , fence_index(0)
        , fence_fd(-1)
        , if_fence_index(0)
        , if_fence_fd(-1)
    { }

    int id;
    int ion_fd;
    unsigned int is_need_flush;

    unsigned int fence_index;
    int fence_fd;
    // in decoupled mode,
    // interface fence of frame N would be released when
    // RDMA starts to read frame (N+1)
    unsigned int if_fence_index;
    int if_fence_fd;
};

enum INPUT_PARAM_STATE
{
    OVL_IN_PARAM_IGNORE  = -1,
    OVL_IN_PARAM_DISABLE = 0,
    OVL_IN_PARAM_ENABLE  = 1,
};

struct OverlayPortParam
{
    OverlayPortParam()
        : state(OVL_IN_PARAM_DISABLE)
        , va(NULL)
        , mva(NULL)
        , pitch(0)
        , format(0)
        , color_range(0)
        , is_sharpen(0)
        , fence_index(0)
        , if_fence_index(0)
        , identity(HWLAYER_ID_NONE)
        , connected_type(0)
        , protect(false)
        , secure(false)
        , alpha_enable(0)
        , alpha(0xFF)
        , blending(0)
        , dim(false)
        , sequence(0)
        , ion_fd(-1)
        , mir_rel_fence_fd(-1)
    { }
    int state;
    void* va;
    void* mva;
    unsigned int pitch;
    unsigned int format;
    unsigned int color_range;
    Rect src_crop;
    Rect dst_crop;
    unsigned int is_sharpen;
    unsigned int fence_index;
    unsigned int if_fence_index;
    int identity;
    int connected_type;
    bool protect;
    bool secure;
    unsigned int alpha_enable;
    unsigned char alpha;
    int blending;
    bool dim;
    unsigned int sequence;
#ifdef MTK_HWC_PROFILING
    int fbt_input_layers;
    int fbt_input_bytes;
#endif
    int ion_fd;
    int mir_rel_fence_fd;
};

// OverlayEngine is used for UILayerComposer and MMLayerComposer
// to config overlay input layers
class OverlayEngine : public LightRefBase<OverlayEngine>
{
public:
    // prepareInput() is used to preconfig a buffer with specific input port
    status_t prepareInput(OverlayPrepareParam& param);

    // setInputQueue() is used to config a buffer queue with a specific input port
    status_t setInputQueue(int id, sp<DisplayBufferQueue> queue);

    // setInputDirect() is used to set a input port as direct type
    status_t setInputDirect(int id, OverlayPortParam* param = NULL);

    // setInputs() is used to update configs of multiple overlay input layers to driver
    status_t setInputs(int num);

    // disableInput() is used to disable specific input port
    status_t disableInput(int id);

    // disableOutput() is used to disable output port
    //   in regard to performance, the overlay output buffers would not be released.
    //   please use releaseOutputQueue() to release overlay output buffers
    status_t disableOutput();

    enum
    {
        IGNORE_DEFAULT = 1, // OVL_INPUT_UNKNOWN,
        IGNORE_DIRECT = 2,  // OVL_INPUT_DIRECT,
        IGNORE_QUEUE = 3,   // OVL_INPUT_QUEUE,
    };

    // ignoreInput() is used to bypass config for specific input layer
    status_t ignoreInput(int id, int type = IGNORE_DEFAULT);

    // prepareOutput() is used to preconfig a buffer with output port
    status_t prepareOutput(OverlayPrepareParam& param);

    // setOutput() is used to set output port
    status_t setOutput(OverlayPortParam* param, bool mirrored = false);

    // createOutputQueue() is used to allocate overlay output buffers
    status_t createOutputQueue(int format, bool secure);

    // createOutputQueue() is used to release overlay output buffers
    status_t releaseOutputQueue();

    // preparePresentFence() is used to get present fence
    // in order to know when screen is updated
    status_t preparePresentFence(OverlayPrepareParam& param);

    // configMirrorOutput() is used to configure output buffer of mirror source
    //   if virtial display is used as mirror source,
    //   nothing is done and retun with NO_ERROR
    status_t configMirrorOutput(HWBuffer* outbuf, bool /*secure*/);

    // setOverlaySessionMode() sets the overlay session mode
    status_t setOverlaySessionMode(DISP_MODE mode);

    // setOverlaySessionMode() gets the overlay session mode
    DISP_MODE getOverlaySessionMode();

    // trigger() is used to nofity engine to start doing composition
    status_t trigger(int present_fence_idx);

    // getInputParams() is used for client to get input params for configuration
    OverlayPortParam* const* getInputParams();

    void setPowerMode(int mode);

    // getInputQueue() returns the buffer queue of a specific input port
    sp<DisplayBufferQueue> getInputQueue(int id) const;

    // getMaxInputNum() is used for getting max amount of overlay inputs
    int getMaxInputNum() { return m_max_inputs; }

    // getAvailableInputNum() is used for
    // getting current available overlay inputs
    int getAvailableInputNum();

    // waitUntilAvailable() is used for waiting until OVL resource is available
    //   during overlay session mode transition,
    //   it is possible for HWC getting transition result from display driver;
    //   this function is used to wait untill
    //   the overlay session mode transition is finisned
    bool waitUntilAvailable();

    // flip() is used to notify OverlayEngine to do its job after notify framework
    void flip();

    // dump() is used to dump each input data to OverlayEngine
    void dump(struct dump_buff* log, int dump_level);

    bool isEnable()
    {
        AutoMutex l(m_lock);
        return (m_state == OVL_ENGINE_ENABLED);
    }

    OverlayEngine(int dpy);
    ~OverlayEngine();

private:
    enum INPUT_TYPE
    {
        OVL_INPUT_NONE    = 0,
        OVL_INPUT_UNKNOWN = 1,
        OVL_INPUT_DIRECT  = 2,
        OVL_INPUT_QUEUE   = 3,
    };

    enum PORT_STATE
    {
        OVL_PORT_DISABLE = 0,
        OVL_PORT_ENABLE  = 1,
    };

    struct OverlayInput
    {
        OverlayInput();

        // connected_state points the input port usage state
        int connected_state;

        // connected_type is connecting type of input port
        int connected_type;

        // param is used for configure input parameters
        OverlayPortParam param;

        // queue is used to acquire and release buffer from client
        sp<DisplayBufferQueue> queue;
    };

    struct OverlayOutput
    {
        OverlayOutput();

        // connected_state points the output port usage state
        int connected_state;

        // param is used for configure output parameters
        OverlayPortParam param;

        // queue is used to acquire and release buffer from client
        sp<DisplayBufferQueue> queue;
    };

    // createOutputQueueLocked() is used to preallocate overlay output buffers
    status_t createOutputQueueLocked(int format, bool secure);

    // updateInput() would notify OverlayEngine to consume new queued buffer
    void updateInput(int id);

    // disableInputLocked is used to clear status of m_inputs
    void disableInputLocked(int id);

    // disableOutputLocked is used to clear status of m_output
    void disableOutputLocked();

    // access must be protected by m_lock
    mutable Mutex m_lock;

    // m_condition is used to wait (block) for a certain condition to become true
    mutable Condition m_cond;

    // m_disp_id is display id
    int m_disp_id;

    enum
    {
        OVL_ENGINE_DISABLED = 0,
        OVL_ENGINE_ENABLED  = 1,
        OVL_ENGINE_PAUSED   = 2,
    };
    // m_state used to verify if OverlayEngine could be use
    int m_state;

    // m_max_inputs is overlay engine max amount of input layer
    // it is platform dependent
    int m_max_inputs;

    // m_async_update is used to nofity that need to set async input buffers
    bool m_async_update;

    // m_inputs is input information array
    // it needs to be initialized with runtime information
    Vector<OverlayInput*> m_inputs;

    // m_input_params is an array which
    // points to all input configurations
    // which would be set to display driver
    Vector<OverlayPortParam*> m_input_params;

    OverlayOutput m_output;
};

#endif // HWC_OVERLAY_H_
