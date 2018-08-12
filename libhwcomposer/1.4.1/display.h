#ifndef HWC_DISPLAY_H_
#define HWC_DISPLAY_H_

#include <ui/Rect.h>
#include <utils/Singleton.h>
#include "hwc_priv.h"

#include "overlay.h"

#define FAKE_DISPLAY -30

using namespace android;

struct dump_buff;

// ---------------------------------------------------------------------------

struct DisplayData
{
    uint32_t width;
    uint32_t height;
    uint32_t format;
    float xdpi;
    float ydpi;
    nsecs_t refresh;
    bool has_vsync;
    bool connected;
    bool secure;

    // hwrotation is physical display rotation
    uint32_t hwrotation;

    // pixels is the area of the display
    uint32_t pixels;

    int subtype;

    float aspect_portrait;
    float aspect_landscape;
    Rect mir_portrait;
    Rect mir_landscape;

    int vsync_source;
    bool timeSharing;
};

class DisplayManager : public Singleton<DisplayManager>
{
public:
    DisplayManager();
    ~DisplayManager();

    enum
    {
        MAX_DISPLAYS = HWC_NUM_DISPLAY_TYPES,
    };

    enum DISP_QUERY_TYPE
    {
        DISP_CURRENT_NUM  = 0x00000001,
    };

    // query() is used for client to get capability
    int query(int what, int* value);

    // dump() for debug prupose
    void dump(struct dump_buff* log);

    // init() is used to initialize DisplayManager
    void init();

    struct EventListener : public virtual RefBase
    {
        // onVSync() is called to notify vsync signal
        virtual void onVSync(int dpy, nsecs_t timestamp, bool enabled) = 0;

        // onPlugIn() is called to notify a display is plugged
        virtual void onPlugIn(int dpy) = 0;

        // onPlugOut() is called to notify a display is unplugged
        virtual void onPlugOut(int dpy) = 0;

        // onHotPlug() is called to notify external display hotplug event
        virtual void onHotPlugExt(int dpy, int connected) = 0;
    };

    // setListener() is used for client to register listener to get event
    void setListener(const sp<EventListener>& listener);

    // requestVSync() is used for client to request vsync signal
    void requestVSync(int dpy, bool enabled);

    // requestNextVSync() is used by HWCDispatcher to request next vsync
    void requestNextVSync(int dpy);

    // vsync() is callback by vsync thread
    void vsync(int dpy, nsecs_t timestamp, bool enabled);

    // hotplugExt() is called to insert or remove extrenal display
    void hotplugExt(int dpy, bool connected, bool fake = false);

    // hotplugVir() is called to insert or remove virtual display
    void hotplugVir(int dpy, hwc_display_contents_1_t* list);

    // setDisplayData() is called to init display data in DisplayManager
    void setDisplayData(int dpy, buffer_handle_t outbuf = NULL);

    // setPowerMode() notifies which display's power mode
    void setPowerMode(int dpy, int mode);

    // getFakeDispNum() gets amount of fake external displays
    int getFakeDispNum() { return m_fake_disp_num; }

    // m_data is the detailed information for each display device
    DisplayData* m_data;

    // m_profile_level is used for profiling purpose
    static int m_profile_level;

private:
    enum
    {
        DISP_PLUG_NONE       = 0,
        DISP_PLUG_CONNECT    = 1,
        DISP_PLUG_DISCONNECT = 2,
    };

    // setMirrorRect() is used to calculate valid region for mirror mode
    void setMirrorRegion(int dpy);

    // hotplugPost() is used to do post jobs after insert/remove display
    void hotplugPost(int dpy, bool connected, int state);

    // createVsyncThread() is used to create vsync thread
    void createVsyncThread(int dpy);

    // destroyVsyncThread() is used to destroy vsync thread
    void destroyVsyncThread(int dpy);

    // printDisplayInfo() used to print out display data
    void printDisplayInfo(int dpy);

    // m_curr_disp_num is current amount of displays
    unsigned int m_curr_disp_num;

    // m_fake_disp_num is amount of fake external displays
    unsigned int m_fake_disp_num;

    // m_listener is used for listen vsync event
    sp<EventListener> m_listener;

    mutable Mutex m_power_lock;
};

#endif // HWC_DISPLAY_H_
