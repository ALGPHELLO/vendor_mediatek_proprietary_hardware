#define DEBUG_LOG_TAG "DPY"

#include <stdint.h>

#include "hwc_priv.h"
#include <cutils/properties.h>

#include <linux/disp_session.h>

#include "utils/debug.h"
#include "utils/tools.h"

#include "display.h"
#include "hwdev.h"
#include "overlay.h"
#include "event.h"

// WORKAROUND: boost GPU for MHL on K2
#include "platform.h"
#ifndef MTK_BASIC_PACKAGE
#include "PerfServiceNative.h"
int g_mhl_cpu_scenario = -1;
#endif

// ---------------------------------------------------------------------------

ANDROID_SINGLETON_STATIC_INSTANCE(DisplayManager);

sp<VSyncThread> g_vsync_thread[DisplayManager::MAX_DISPLAYS] = { NULL };
sp<UEventThread> g_uevent_thread = NULL;

#ifdef MTK_USER_BUILD
int DisplayManager::m_profile_level = PROFILE_NONE;
#else
int DisplayManager::m_profile_level = PROFILE_COMP | PROFILE_BLT | PROFILE_TRIG;
#endif

DisplayManager::DisplayManager()
    : m_curr_disp_num(0)
    , m_fake_disp_num(0)
    , m_listener(NULL)
{
    m_data = (DisplayData*) calloc(1, MAX_DISPLAYS * sizeof(DisplayData));

    g_uevent_thread = new UEventThread();
    if (g_uevent_thread == NULL)
    {
        HWC_LOGE("Failed to initialize UEvent thread!!");
        abort();
    }
    g_uevent_thread->initialize();
}

DisplayManager::~DisplayManager()
{
    m_listener = NULL;
    free(m_data);
}

void DisplayManager::init()
{
    m_curr_disp_num = 1;

    if (m_listener != NULL) m_listener->onPlugIn(HWC_DISPLAY_PRIMARY);

    createVsyncThread(HWC_DISPLAY_PRIMARY);

    HWC_LOGI("Display Information:");
    HWC_LOGI("# fo current devices : %d", m_curr_disp_num);
    printDisplayInfo(HWC_DISPLAY_PRIMARY);
}

void DisplayManager::createVsyncThread(int dpy)
{
    g_vsync_thread[dpy] = new VSyncThread(dpy);
    if (g_vsync_thread[dpy] == NULL)
    {
        HWC_LOGE("dpy=%d/Failed to initialize VSYNC thread!!", dpy);
        abort();
    }
    g_vsync_thread[dpy]->initialize(!m_data[dpy].has_vsync, m_data[dpy].refresh);
}

void DisplayManager::destroyVsyncThread(int dpy)
{
    g_vsync_thread[dpy]->wait();
    g_vsync_thread[dpy]->requestExit();
    g_vsync_thread[dpy]->setLoopAgain();
    g_vsync_thread[dpy]->join();
    g_vsync_thread[dpy] = NULL;
}

void DisplayManager::printDisplayInfo(int dpy)
{
    if (dpy < 0 || dpy >= MAX_DISPLAYS) return;

    DisplayData* disp_data = &m_data[dpy];

    HWC_LOGI("------------------------------------");
    HWC_LOGI("Device id   : %d",   dpy);
    HWC_LOGI("Width       : %d",   disp_data->width);
    HWC_LOGI("Height      : %d",   disp_data->height);
    HWC_LOGI("xdpi        : %f",   disp_data->xdpi);
    HWC_LOGI("ydpi        : %f",   disp_data->ydpi);
    HWC_LOGI("vsync       : %d",   disp_data->has_vsync);
    HWC_LOGI("refresh     : %lld", disp_data->refresh);
    HWC_LOGI("connected   : %d",   disp_data->connected);
    HWC_LOGI("hwrotation  : %d",   disp_data->hwrotation);
    HWC_LOGI("subtype     : %d",   disp_data->subtype);
    HWC_LOGI("aspect      : %1.3f, %1.3f",
        disp_data->aspect_portrait, disp_data->aspect_landscape);
    HWC_LOGI("protrait    : [%4d,%4d,%4d,%4d]",
        disp_data->mir_portrait.left,  disp_data->mir_portrait.top,
        disp_data->mir_portrait.right, disp_data->mir_portrait.bottom);
    HWC_LOGI("landscape   : [%4d,%4d,%4d,%4d]",
        disp_data->mir_landscape.left,  disp_data->mir_landscape.top,
        disp_data->mir_landscape.right, disp_data->mir_landscape.bottom);
}

// TODO: should check if need to add a lock that protecting against m_curr_disp_num
// to avoid race condition (e.g. when handling the hotplug event)
int DisplayManager::query(int what, int* value)
{
    switch (what)
    {
        case DISP_CURRENT_NUM:
            *value = m_curr_disp_num;
            break;

        default:
            return -EINVAL;
    }

    return 0;
}

void DisplayManager::dump(struct dump_buff* /*log*/)
{
    if (g_vsync_thread[HWC_DISPLAY_PRIMARY] != NULL)
        g_vsync_thread[HWC_DISPLAY_PRIMARY]->setProperty();

    if (g_uevent_thread != NULL)
        g_uevent_thread->setProperty();
}

void DisplayManager::setListener(const sp<EventListener>& listener)
{
    m_listener = listener;
}

void DisplayManager::requestVSync(int dpy, bool enabled)
{
    g_vsync_thread[dpy]->setEnabled(enabled);
}

void DisplayManager::requestNextVSync(int dpy)
{
    g_vsync_thread[dpy]->setLoopAgain();
}

void DisplayManager::vsync(int dpy, nsecs_t timestamp, bool enabled)
{
    if (m_listener != NULL)
    {
        // check if primary display needs to use external vsync source
        if (HWC_DISPLAY_PRIMARY != dpy)
        {
            Mutex::Autolock _l(m_power_lock);

            if (m_data[HWC_DISPLAY_PRIMARY].vsync_source == dpy)
                m_listener->onVSync(HWC_DISPLAY_PRIMARY, timestamp, enabled);
        }

        m_listener->onVSync(dpy, timestamp, enabled);
    }
}

void DisplayManager::hotplugExt(int dpy, bool connected, bool fake)
{
    if (dpy != HWC_DISPLAY_EXTERNAL)
    {
        HWC_LOGW("Failed to hotplug external disp(%d) connect(%d) !", dpy, connected);
        return;
    }

    HWC_LOGI("Hotplug external disp(%d) connect(%d) fake(%d)", dpy, connected, fake);

    DisplayData* disp_data = &m_data[HWC_DISPLAY_EXTERNAL];

    if (connected && !disp_data->connected)
    {
        if (m_listener != NULL) m_listener->onPlugIn(dpy);

        if (fake == true)
        {
            static int _s_shrink_size = 4;
            _s_shrink_size = (_s_shrink_size == 2) ? 4 : 2;
            memcpy(disp_data, &m_data[0], sizeof(DisplayData));
            disp_data->width   = m_data[0].width / _s_shrink_size;
            disp_data->height  = m_data[0].height / _s_shrink_size;
            disp_data->subtype = FAKE_DISPLAY;

            m_fake_disp_num++;
        }

        createVsyncThread(dpy);

        hotplugPost(dpy, 1, DISP_PLUG_CONNECT);

        if (m_listener != NULL) m_listener->onHotPlugExt(dpy, 1);
    }
    else if (!connected && disp_data->connected)
    {
        if (fake == true) m_fake_disp_num--;

        destroyVsyncThread(dpy);

        if (m_listener != NULL)
        {
            m_listener->onHotPlugExt(dpy, 0);
            m_listener->onPlugOut(dpy);
        }

        hotplugPost(dpy, 0, DISP_PLUG_DISCONNECT);
    }
}

void DisplayManager::hotplugVir(int dpy, hwc_display_contents_1_t* list)
{
    if (dpy != HWC_DISPLAY_VIRTUAL)
    {
        HWC_LOGW("Failed to hotplug virtual disp(%d) !", dpy);
        return;
    }

    DisplayData* disp_data = &m_data[HWC_DISPLAY_VIRTUAL];
    bool connected = (list != NULL);

    if (connected == disp_data->connected)
    {
        if (!connected) return;

        PrivateHandle priv_handle;
        getPrivateHandleInfo(list->outbuf, &priv_handle);

        if ((disp_data->width == priv_handle.width) &&
            (disp_data->height == priv_handle.height) &&
            (disp_data->format == priv_handle.format))
        {
            return;
        }
        else
        {
            HWC_LOGI("Format changed for virtual disp(%d)", dpy);

            if (m_listener != NULL) m_listener->onPlugOut(dpy);

            hotplugPost(dpy, 0, DISP_PLUG_DISCONNECT);
        }
    }

    HWC_LOGW("Hotplug virtual disp(%d) connect(%d)", dpy, connected);

    if (connected)
    {
        if (list == NULL)
        {
            HWC_LOGE("Failed to add display, virtual list is NULL");
            return;
        }

        if (list->outbuf == NULL)
        {
            HWC_LOGE("Failed to add display, virtual outbuf is NULL");
            return;
        }

        setDisplayData(HWC_DISPLAY_VIRTUAL, list->outbuf);

        hotplugPost(dpy, 1, DISP_PLUG_CONNECT);

        if (m_listener != NULL) m_listener->onPlugIn(dpy);
    }
    else
    {
        if (m_listener != NULL) m_listener->onPlugOut(dpy);

        hotplugPost(dpy, 0, DISP_PLUG_DISCONNECT);
    }
}

void DisplayManager::setDisplayData(int dpy, buffer_handle_t outbuf)
{
    DisplayData* disp_data = &m_data[dpy];

    char value[PROPERTY_VALUE_MAX];
    property_get("ro.sf.lcd_density", value, "160");
    float density = atof(value);

    if (dpy == HWC_DISPLAY_PRIMARY)
    {
        disp_session_info info;
        DispDevice::getInstance().getOverlaySessionInfo(HWC_DISPLAY_PRIMARY, &info);

        disp_data->width     = info.displayWidth;
        disp_data->height    = info.displayHeight;
        disp_data->format    = info.displayFormat;
        disp_data->xdpi      = info.physicalWidth == 0 ? density :
                                (info.displayWidth * 25.4f / info.physicalWidth);
        disp_data->ydpi      = info.physicalHeight == 0 ? density :
                                (info.displayHeight * 25.4f / info.physicalHeight);
        disp_data->has_vsync = info.isHwVsyncAvailable;
        disp_data->connected = info.isConnected;

        // TODO: ask from display driver
        disp_data->secure    = true;
        disp_data->subtype   = HWC_DISPLAY_LCM;

        disp_data->aspect_portrait  = float(info.displayWidth) / float(info.displayHeight);
        disp_data->aspect_landscape = float(info.displayHeight) / float(info.displayWidth);
        disp_data->mir_portrait     = Rect(info.displayWidth, info.displayHeight);
        disp_data->mir_landscape    = Rect(info.displayWidth, info.displayHeight);

        disp_data->vsync_source = HWC_DISPLAY_PRIMARY;

        float refreshRate = info.vsyncFPS / 100.0;
        if (0 >= refreshRate) refreshRate = 60.0;
        disp_data->refresh = nsecs_t(1e9 / refreshRate);

        // get physically installed rotation for primary display from property
        property_get("ro.sf.hwrotation", value, "0");
        disp_data->hwrotation = atoi(value) / 90;

        disp_data->pixels = disp_data->width * disp_data->height;
    }
    else if (dpy == HWC_DISPLAY_EXTERNAL)
    {
        disp_session_info info;
        DispDevice::getInstance().getOverlaySessionInfo(HWC_DISPLAY_EXTERNAL, &info);
        if (!info.isConnected)
        {
            HWC_LOGE("Failed to add display, hdmi is not connected!");
            return;
        }

        disp_data->width     = info.displayWidth;
        disp_data->height    = info.displayHeight;
        disp_data->format    = info.displayFormat;
        disp_data->xdpi      = info.physicalWidth == 0 ? density :
                                (info.displayWidth * 25.4f / info.physicalWidth);
        disp_data->ydpi      = info.physicalHeight == 0 ? density :
                                (info.displayHeight * 25.4f / info.physicalHeight);
        disp_data->has_vsync = info.isHwVsyncAvailable;
        disp_data->connected = info.isConnected;
        disp_data->secure    = DevicePlatform::m_config.bypass_hdcp_checking ? true : false;
        disp_data->subtype   = (info.displayType == DISP_IF_HDMI_SMARTBOOK) ?
                                HWC_DISPLAY_SMARTBOOK : HWC_DISPLAY_HDMI_MHL;

        // [NOTE]
        // only if the display without any physical rotation,
        // same ratio can be applied to both portrait and landscape
        disp_data->aspect_portrait  = float(info.displayWidth) / float(info.displayHeight);
        disp_data->aspect_landscape = disp_data->aspect_portrait;

        disp_data->vsync_source = HWC_DISPLAY_EXTERNAL;

        float refreshRate = info.vsyncFPS / 100.0;
        if (0 >= refreshRate) refreshRate = 60.0;
        disp_data->refresh = nsecs_t(1e9 / refreshRate);

        // currently no need for ext disp
        disp_data->hwrotation = 0;

        disp_data->pixels = disp_data->width * disp_data->height;
    }
    else if (dpy == HWC_DISPLAY_VIRTUAL)
    {
        PrivateHandle priv_handle;
        getPrivateHandleInfo(outbuf, &priv_handle);

        disp_data->width     = priv_handle.width;
        disp_data->height    = priv_handle.height;
        disp_data->format    = priv_handle.format;
        disp_data->xdpi      = m_data[HWC_DISPLAY_PRIMARY].xdpi;
        disp_data->ydpi      = m_data[HWC_DISPLAY_PRIMARY].ydpi;
        disp_data->has_vsync = false;
        disp_data->connected = true;
        disp_data->timeSharing = false;

        // check if virtual display is WFD
        if (priv_handle.usage & GRALLOC_USAGE_HW_VIDEO_ENCODER)
        {
            char value[PROPERTY_VALUE_MAX];
            property_get("media.wfd.hdcp", value, "0");
            disp_data->secure  = (atoi(value) == 1);

            property_get("debug.hwc.force_wfd_insecure", value, "0");
            if (0 != atoi(value))
            {
                disp_data->secure = false;
                HWC_LOGI("force set virtual display insecure");
            }

            disp_data->subtype = HWC_DISPLAY_WIRELESS;
        }
        else
        {
            disp_data->secure  = false;
            disp_data->subtype = HWC_DISPLAY_MEMORY;
        }

        // [NOTE]
        // only if the display without any physical rotation,
        // same ratio can be applied to both portrait and landscape
        disp_data->aspect_portrait  = float(disp_data->width) / float(disp_data->height);
        disp_data->aspect_landscape = disp_data->aspect_portrait;

        // TODO
        //disp_data->vsync_source = HWC_DISPLAY_VIRTUAL;

        // currently no need for vir disp
        disp_data->hwrotation = 0;

        disp_data->pixels = disp_data->width * disp_data->height;
    }
}

void DisplayManager::setMirrorRegion(int dpy)
{
    DisplayData* main_disp_data = &m_data[HWC_DISPLAY_PRIMARY];
    DisplayData* disp_data = &m_data[dpy];

    // calculate portrait region
    if (main_disp_data->aspect_portrait > disp_data->aspect_portrait)
    {
        // calculate for letterbox
        int portrait_h = disp_data->width / main_disp_data->aspect_portrait;
        int portrait_y = (disp_data->height - portrait_h) / 2;
        disp_data->mir_portrait.left = 0;
        disp_data->mir_portrait.top = portrait_y;
        disp_data->mir_portrait.right = disp_data->width;
        disp_data->mir_portrait.bottom = portrait_y + portrait_h;
    }
    else
    {
        // calculate for pillarbox
    int portrait_w = disp_data->height * main_disp_data->aspect_portrait;
        int portrait_x = (disp_data->width - portrait_w) / 2;
        disp_data->mir_portrait.left = portrait_x;
    disp_data->mir_portrait.top = 0;
        disp_data->mir_portrait.right = portrait_x + portrait_w;
    disp_data->mir_portrait.bottom = disp_data->height;
    }

    // calculate landscape region
    if (main_disp_data->aspect_landscape > disp_data->aspect_landscape)
    {
        // calculate for letterbox
        int landscape_h = disp_data->width / main_disp_data->aspect_landscape;
        int landscape_y = (disp_data->height - landscape_h) / 2;
        disp_data->mir_landscape.left = 0;
        disp_data->mir_landscape.top = landscape_y;
        disp_data->mir_landscape.right = disp_data->width;
        disp_data->mir_landscape.bottom = landscape_y + landscape_h;
    }
    else
    {
        // calculate for pillarbox
        int landscape_w = disp_data->height * main_disp_data->aspect_landscape;
        int landscape_x = (disp_data->width - landscape_w) / 2;
        disp_data->mir_landscape.left = landscape_x;
        disp_data->mir_landscape.top = 0;
        disp_data->mir_landscape.right = landscape_x + landscape_w;
        disp_data->mir_landscape.bottom = disp_data->height;
    }
}

void DisplayManager::hotplugPost(int dpy, bool connected, int state)
{
    DisplayData* disp_data = &m_data[dpy];

    switch (state)
    {
        case DISP_PLUG_CONNECT:
            HWC_LOGI("Added Display Information:");
            setMirrorRegion(dpy);
            printDisplayInfo(dpy);
            m_curr_disp_num++;

            // TODO: should be put in scenario manager
#ifndef MTK_BASIC_PACKAGE
            if (DevicePlatform::m_config.platform == PLATFORM_MT6735 ||
                DevicePlatform::m_config.platform == PLATFORM_MT6752 ||
                DevicePlatform::m_config.platform == PLATFORM_MT6795)
            {
                // MHL and WFD will change scenario,
                // so we break here when HWC_DISPLAY_VIRTUAL and not WFD
                if (dpy == HWC_DISPLAY_VIRTUAL &&
                    m_data[dpy].subtype != HWC_DISPLAY_WIRELESS)
                    break;

                // reg and enable CPU configuration
                if (dpy == HWC_DISPLAY_HDMI_MHL)
                {
                    g_mhl_cpu_scenario = PerfServiceNative_userRegBigLittle(0, 0, 2, 819000);
                }
                else
                {
                    g_mhl_cpu_scenario = PerfServiceNative_userRegBigLittle(2, 0, 0, 0);
                }
                if (-1 == g_mhl_cpu_scenario)
                {
                    HWC_LOGW("Failed to register to PerfService");
                }
                else
                {
                    // enable perf scenario for multi-display session case
                    // need to set auto restore for suspend/resume
                    PerfServiceNative_userRegScnConfig(
                            g_mhl_cpu_scenario,
                            CMD_SET_SCREEN_OFF_STATE,
                            SCREEN_OFF_WAIT_RESTORE,
                            0, 0, 0);
                    PerfServiceNative_userEnable(g_mhl_cpu_scenario);
                    HWC_LOGI("Change Scenario=%d, dpy=%d connected",
                        g_mhl_cpu_scenario, dpy);
                }
            }
#endif

            break;

        case DISP_PLUG_DISCONNECT:
            HWC_LOGI("Removed Display Information:");
            printDisplayInfo(dpy);
            memset((void*)disp_data, 0, sizeof(DisplayData));
            m_curr_disp_num--;

            // TODO: should be put in scenario manager
#ifndef MTK_BASIC_PACKAGE
            {
                // disable and unreg CPU configuration
                if (-1 != g_mhl_cpu_scenario)
                {
                    PerfServiceNative_userDisable(g_mhl_cpu_scenario);
                    HWC_LOGI("Change Scenario=%d, dpy=%d disconnected",
                        g_mhl_cpu_scenario, dpy);

                    PerfServiceNative_userUnreg(g_mhl_cpu_scenario);
                    g_mhl_cpu_scenario = -1;
                }
            }
#endif

            break;

        case DISP_PLUG_NONE:
            HWC_LOGW("Unexpected hotplug: disp(%d:%d) connect(%d)",
                dpy, disp_data->connected, connected);
            return;
    };
}

void DisplayManager::setPowerMode(int dpy, int mode)
{
    Mutex::Autolock _l(m_power_lock);

    if (HWC_DISPLAY_PRIMARY != dpy) return;

    if (m_data[HWC_DISPLAY_EXTERNAL].connected &&
        m_data[HWC_DISPLAY_EXTERNAL].subtype == HWC_DISPLAY_SMARTBOOK)
    {
        m_data[dpy].vsync_source = (HWC_POWER_MODE_OFF == mode) ?
                                        HWC_DISPLAY_EXTERNAL : HWC_DISPLAY_PRIMARY;
    }
}
