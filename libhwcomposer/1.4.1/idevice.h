#ifndef HWC_IDEVICE_H_
#define HWC_IDEVICE_H_

using namespace android;

struct OverlayPrepareParam;
struct OverlayPortParam;

class IDevice : public virtual RefBase
{
public:
    //IDevice();
    virtual ~IDevice() {}

    // initOverlay() initializes overlay related hw setting
    virtual void initOverlay() = 0;

    // getMaxOverlayInputNum() gets overlay supported input amount
    virtual int getMaxOverlayInputNum() = 0;

    // createOverlaySession() creates overlay composition session
    virtual status_t createOverlaySession(
        int dpy, int mode) = 0;

    // destroyOverlaySession() destroys overlay composition session
    virtual void destroyOverlaySession(int dpy) = 0;

    // truggerOverlaySession() used to trigger overlay engine to do composition
    virtual status_t triggerOverlaySession(int dpy, int present_fence_idx, DISP_DC_TYPE type) = 0;

    // disableOverlaySession() usd to disable overlay session to do composition
    virtual void disableOverlaySession(int dpy,  OverlayPortParam* const* params, int num) = 0;

    // setOverlaySessionMode() sets the overlay session mode
    virtual status_t setOverlaySessionMode(int dpy, int mode) = 0;

    // getOverlaySessionMode() gets the overlay session mode
    virtual int getOverlaySessionMode(int dpy) = 0;

    // getOverlaySessionInfo() gets specific display device information
    virtual status_t getOverlaySessionInfo(int dpy, struct disp_session_info_t* info) = 0;

    // getAvailableOverlayInput gets available amount of overlay input
    // for different session
    virtual int getAvailableOverlayInput(int dpy) = 0;

    // prepareOverlayInput() gets timeline index and fence fd of overlay input layer
    virtual void prepareOverlayInput(int dpy, OverlayPrepareParam* param) = 0;

    // enableOverlayInput() enables single overlay input layer
    virtual void enableOverlayInput(int dpy, OverlayPortParam* param, int id) = 0;

    // updateOverlayInputs() updates multiple overlay input layers
    virtual void updateOverlayInputs(int dpy, OverlayPortParam* const* params, int num) = 0;

    // prepareOverlayOutput() gets timeline index and fence fd for overlay output buffer
    virtual void prepareOverlayOutput(int dpy, OverlayPrepareParam* param) = 0;

    // enableOverlayOutput() enables overlay output buffer
    virtual void enableOverlayOutput(int dpy, OverlayPortParam* param) = 0;

    // prepareOverlayPresentFence() gets present timeline index and fence
    virtual void prepareOverlayPresentFence(int dpy, OverlayPrepareParam* param) = 0;

    // setPowerMode() is used to switch power setting for display
    virtual void setPowerMode(int dpy, int mode) = 0;

};


#endif // HWC_IDEVICE_H_
