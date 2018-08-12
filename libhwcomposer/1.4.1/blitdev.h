#ifndef HWC_BlitDEV_H_
#define HWC_BlitDEV_H_

#include "idevice.h"

#include <utils/KeyedVector.h>

using namespace android;

class SyncFence;
class DpBlitStream;
struct InvalidParams;

// --------------------------------------------------------------------------

class BlitDevice : public IDevice
{
public:
    BlitDevice();
    ~BlitDevice();

    // initOverlay() initializes overlay related hw setting
    void initOverlay() {}

    // getMaxOverlayInputNum() gets overlay supported input amount
    int getMaxOverlayInputNum();

    // createOverlaySession() creates overlay composition session
    status_t createOverlaySession(
        int dpy, int mode);

    // destroyOverlaySession() destroys overlay composition session
    void destroyOverlaySession(int dpy);

    // truggerOverlaySession() used to trigger overlay engine to do composition
    status_t triggerOverlaySession(int dpy, int present_fence_idx, DISP_DC_TYPE type);

    // disableOverlaySession() usd to disable overlay session to do composition
    void disableOverlaySession(int dpy,  OverlayPortParam* const* params, int num);

    // setOverlaySessionMode() sets the overlay session mode
    status_t setOverlaySessionMode(int dpy, int mode);

    // getOverlaySessionMode() gets the overlay session mode
    int getOverlaySessionMode(int dpy);

    // getOverlaySessionInfo() gets specific display device information
    status_t getOverlaySessionInfo(int dpy, disp_session_info* info);

    // getAvailableOverlayInput gets available amount of overlay input
    // for different session
    int getAvailableOverlayInput(int dpy);

    // prepareOverlayInput() gets timeline index and fence fd of overlay input layer
    void prepareOverlayInput(int dpy, OverlayPrepareParam* param);

    // enableOverlayInput() enables single overlay input layer
    void enableOverlayInput(int dpy, OverlayPortParam* param, int id);

    // updateOverlayInputs() updates multiple overlay input layers
    void updateOverlayInputs(int dpy, OverlayPortParam* const* params, int num);

    // prepareOverlayOutput() gets timeline index and fence fd for overlay output buffer
    void prepareOverlayOutput(int dpy, OverlayPrepareParam* param);

    // enableOverlayOutput() enables overlay output buffer
    void enableOverlayOutput(int dpy, OverlayPortParam* param);

    // prepareOverlayPresentFence() gets present timeline index and fence
    void prepareOverlayPresentFence(int dpy, OverlayPrepareParam* param);

    // setPowerMode() is used to switch power setting for display
    void setPowerMode(int dpy, int mode);

private:

    // m_sync_fence is used to create or wait fence
    sp<SyncFence> m_sync_input_fence;
	sp<SyncFence> m_sync_output_fence;

    // m_blit_stream is a bit blit stream
    DpBlitStream* m_blit_stream;

    int m_session_id;

    DefaultKeyedVector< int, bool> m_ion_flush_vector;
    mutable Mutex m_vector_lock;

    InvalidParams* m_cur_params;

    int m_state;
};

// --------------------------------------------------------------------------

#endif // HWC_BlitDEV_H_
