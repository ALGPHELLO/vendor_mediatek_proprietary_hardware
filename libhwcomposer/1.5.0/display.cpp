#define DEBUG_LOG_TAG "DPY"

#include <stdint.h>

#include "hwc_priv.h"
#include <cutils/properties.h>

#include <linux/disp_session.h>

#include "utils/debug.h"
#include "utils/tools.h"

#include "dispatcher.h"
#include "display.h"
#include "hwdev.h"
#include "overlay.h"
#include "event.h"
#include "sync.h"

// WORKAROUND: boost GPU for MHL on K2
#include "platform.h"
#ifndef MTK_BASIC_PACKAGE
#include "PerfServiceNative.h"
#endif // MTK_BASIC_PACKAGE

#include <WidevineHdcpInfo.h>

int g_mhl_cpu_scenario = -1;

// ---------------------------------------------------------------------------

ANDROID_SINGLETON_STATIC_INSTANCE(DisplayManager);

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
    , m_wake_lock_timer(NULL)
    , m_video_hdcp(UINT_MAX)
{
    m_data = (DisplayData*) calloc(1, MAX_DISPLAYS * sizeof(DisplayData));

    m_data[HWC_DISPLAY_PRIMARY].trigger_by_vsync = false;
    m_data[HWC_DISPLAY_EXTERNAL].trigger_by_vsync = false;
    m_data[HWC_DISPLAY_VIRTUAL].trigger_by_vsync = false;
    switch (HWCMediator::getInstance().m_features.trigger_by_vsync)
    {
        case 1:
            m_data[HWC_DISPLAY_EXTERNAL].trigger_by_vsync = true;
            break;
        case 2:
            m_data[HWC_DISPLAY_EXTERNAL].trigger_by_vsync = true;
            m_data[HWC_DISPLAY_VIRTUAL].trigger_by_vsync = true;
            break;
    }


    g_uevent_thread = new UEventThread();
    if (g_uevent_thread == NULL)
    {
        HWC_LOGE("Failed to initialize UEvent thread!!");
        abort();
    }
    g_uevent_thread->initialize();

    memset(m_state_ultra_display, 0, sizeof(m_state_ultra_display));
    for (int i = 0; i < MAX_DISPLAYS; i++)
    {
        m_display_power_state[i] = false;
    }
}

DisplayManager::~DisplayManager()
{
    m_listener = NULL;
    {
        AutoMutex _l(m_state_lock);
        if (m_wake_lock_timer != NULL)
        {
            m_wake_lock_timer->stop();
            m_wake_lock_timer = NULL;
        }
    }
    free(m_data);
}

void DisplayManager::init()
{
    m_curr_disp_num = 1;

    if (m_listener != NULL) m_listener->onPlugIn(HWC_DISPLAY_PRIMARY);

    createVsyncThread(HWC_DISPLAY_PRIMARY);

    if (HWCMediator::getInstance().m_features.dual_display)
    {
        checkSecondBuildInPanel();
    }

    HWC_LOGI("Display Information:");
    HWC_LOGI("# fo current devices : %d", m_curr_disp_num);
    for (unsigned int i = 0; i < m_curr_disp_num; i++)
    {
        printDisplayInfo(i);
        m_display_power_state[i] = true;
        if (m_wake_lock_timer == NULL && m_data[i].subtype == HWC_DISPLAY_EPAPER)
        {
            m_wake_lock_timer = new WakeLockTimer();
            setWakelockTimerState(WAKELOCK_TIMER_PAUSE);
        }
    }
}

void DisplayManager::checkSecondBuildInPanel()
{
    hotplugExt(HWC_DISPLAY_EXTERNAL, true, false, false);
}

void DisplayManager::createVsyncThread(int dpy)
{
    AutoMutex _l(m_vsyncs[dpy].lock);
    m_vsyncs[dpy].thread = new VSyncThread(dpy);
    if (m_vsyncs[dpy].thread == NULL)
    {
        HWC_LOGE("dpy=%d/Failed to initialize VSYNC thread!!", dpy);
        abort();
    }
    m_vsyncs[dpy].thread->initialize(!m_data[dpy].has_vsync, m_data[dpy].refresh);
}

void DisplayManager::destroyVsyncThread(int dpy)
{
    if (m_vsyncs[dpy].thread != NULL)
    {
        m_vsyncs[dpy].thread->requestExit();
        m_vsyncs[dpy].thread->setLoopAgain();
        m_vsyncs[dpy].thread->join();
    }

    {
        // We cannot lock the whole destoryVsyncThread(), or it will cause the deadlock between
        // UEventThread and DispatcherThread. When a secondary display plugged out, UEventThread
        // will wait completion of VSyncThread, and needs to acquire the VSync lock of onVSync().
        // DispatcherThread acquired the lock of onVsync() firstly and request a next VSync.
        // Unfortunately, DispatcherThread cannot get a VSync because the vsync lock is acquired
        // by UEventThread.
        AutoMutex _l(m_vsyncs[dpy].lock);
        m_vsyncs[dpy].thread = NULL;
    }
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
    HWC_LOGI("trigger_by_vsync: %d", disp_data->trigger_by_vsync);
    HWC_LOGI("supportS3D  : %d", disp_data->is_s3d_support);
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
    if (m_vsyncs[HWC_DISPLAY_PRIMARY].thread != NULL)
        m_vsyncs[HWC_DISPLAY_PRIMARY].thread->setProperty();

    if (g_uevent_thread != NULL)
        g_uevent_thread->setProperty();
}

void DisplayManager::setListener(const sp<EventListener>& listener)
{
    m_listener = listener;
}

void DisplayManager::requestVSync(int dpy, bool enabled)
{
    AutoMutex _l(m_vsyncs[dpy].lock);
    if (m_vsyncs[dpy].thread != NULL)
        m_vsyncs[dpy].thread->setEnabled(enabled);
}

void DisplayManager::requestNextVSync(int dpy)
{
    AutoMutex _l(m_vsyncs[dpy].lock);
    if (m_vsyncs[dpy].thread != NULL)
        m_vsyncs[dpy].thread->setLoopAgain();
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

void DisplayManager::hotplugExt(int dpy, bool connected, bool fake, bool notify)
{
    if (dpy != HWC_DISPLAY_EXTERNAL)
    {
        HWC_LOGW("Failed to hotplug external disp(%d) connect(%d) !", dpy, connected);
        return;
    }

    HWC_LOGI("Hotplug external disp(%d) connect(%d) fake(%d)", dpy, connected, fake);

    DisplayData* disp_data = &m_data[HWC_DISPLAY_EXTERNAL];

    disp_data->trigger_by_vsync =
        HWCMediator::getInstance().m_features.trigger_by_vsync > 0 ? true : false;

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

        hotplugPost(dpy, 1, DISP_PLUG_CONNECT, notify);

        if (m_listener != NULL && notify) m_listener->onHotPlugExt(dpy, 1);

        if (disp_data->trigger_by_vsync)
        {
            requestVSync(dpy, true);
        }
        HWCDispatcher::getInstance().ignoreJob(dpy, false);
    }
    else if (!connected && disp_data->connected)
    {
        HWCDispatcher::getInstance().ignoreJob(dpy, true);

        if (fake == true) m_fake_disp_num--;

        if (disp_data->trigger_by_vsync)
        {
            requestVSync(dpy, false);
        }

        destroyVsyncThread(dpy);

        if (m_listener != NULL)
        {
            if (notify)
            {
                m_listener->onHotPlugExt(dpy, 0);
            }
            m_listener->onPlugOut(dpy);
        }

        hotplugPost(dpy, 0, DISP_PLUG_DISCONNECT, notify);
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

    disp_data->trigger_by_vsync =
        HWCMediator::getInstance().m_features.trigger_by_vsync > 1 ? true : false;

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

        // TODO: How HWC receives requests from WFD frameworks
    }
    else
    {
        if (m_listener != NULL) m_listener->onPlugOut(dpy);

        hotplugPost(dpy, 0, DISP_PLUG_DISCONNECT);

        // TODO: How HWC receives requests from WFD frameworks
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
        unsigned int physicalWidth = 0;
        unsigned int physicalHeight = 0;
        getPhysicalPanelSize(&physicalWidth, &physicalHeight, info);
        disp_data->xdpi      = physicalWidth == 0 ? density :
                                (info.displayWidth * 25.4f * 1000 / physicalWidth);
        disp_data->ydpi      = physicalHeight == 0 ? density :
                                (info.displayHeight * 25.4f * 1000 / physicalHeight);
        disp_data->has_vsync = info.isHwVsyncAvailable;
        disp_data->connected = info.isConnected;

        // TODO: ask from display driver
        disp_data->secure    = true;
        disp_data->hdcp_version = UINT_MAX;
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

        disp_data->trigger_by_vsync = false;
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
        disp_data->secure    = Platform::getInstance().m_config.bypass_hdcp_checking ? true : info.isHDCPSupported >= 100;
        disp_data->hdcp_version = info.isHDCPSupported;
        HWC_LOGI("(%d) hdcp version is %d", dpy, disp_data->hdcp_version);
        switch (info.displayType)
        {
            case DISP_IF_HDMI_SMARTBOOK:
                disp_data->subtype = HWC_DISPLAY_SMARTBOOK;
                break;
            case DISP_IF_EPD:
                disp_data->subtype = HWC_DISPLAY_EPAPER;
                break;
            default:
                disp_data->subtype = HWC_DISPLAY_HDMI_MHL;
                break;
        }
        disp_data->is_s3d_support = info.is3DSupport > 0 ? true : false;

        // [NOTE]
        // only if the display without any physical rotation,
        // same ratio can be applied to both portrait and landscape
        disp_data->aspect_portrait  = float(info.displayWidth) / float(info.displayHeight);
        disp_data->aspect_landscape = disp_data->aspect_portrait;

        disp_data->vsync_source = HWC_DISPLAY_EXTERNAL;

        float refreshRate = info.vsyncFPS / 100.0;
        if (0 >= refreshRate) refreshRate = 60.0;
        disp_data->refresh = nsecs_t(1e9 / refreshRate);

        // get physically installed rotation for extension display from property
        property_get("ro.sf.hwrotation.ext", value, "0");
        disp_data->hwrotation = atoi(value) / 90;

        disp_data->pixels = disp_data->width * disp_data->height;

        disp_data->trigger_by_vsync =
            HWCMediator::getInstance().m_features.trigger_by_vsync > 0 ? true : false;
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
            disp_data->hdcp_version = getHdcpInfo();
            HWC_LOGI("(%d) hdcp version is %d", dpy, disp_data->hdcp_version);
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

        disp_data->trigger_by_vsync =
            HWCMediator::getInstance().m_features.trigger_by_vsync > 1 ? true : false;
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

void DisplayManager::hotplugPost(int dpy, bool connected, int state, bool print_info)
{
    DisplayData* disp_data = &m_data[dpy];

    switch (state)
    {
        case DISP_PLUG_CONNECT:
            HWC_LOGI("Added Display Information:");
            setMirrorRegion(dpy);
            if (print_info)
            {
                printDisplayInfo(dpy);
            }
            compareDisplaySize(dpy);
            m_curr_disp_num++;

#ifndef MTK_BASIC_PACKAGE
            // TODO: should be put in scenario manager
            if (Platform::getInstance().m_config.platform == PLATFORM_MT6752 ||
                Platform::getInstance().m_config.platform == PLATFORM_MT6795)
            {
                if (dpy == HWC_DISPLAY_VIRTUAL &&
                    m_data[dpy].subtype != HWC_DISPLAY_WIRELESS)
                    break;

                // reg and enable CPU configuration
                g_mhl_cpu_scenario = PerfServiceNative_userReg(2, 0);
                if (-1 == g_mhl_cpu_scenario)
                {
                    HWC_LOGW("Failed to register to PerfService");
                }
                else
                {
                    PerfServiceNative_userEnable(g_mhl_cpu_scenario);
                    HWC_LOGI("Change Scenario=%d, dpy=%d connected",
                        g_mhl_cpu_scenario, dpy);
                }
            }
            else if (Platform::getInstance().m_config.platform == PLATFORM_MT6755 ||
                     Platform::getInstance().m_config.platform == PLATFORM_MT6763 ||
                     Platform::getInstance().m_config.platform == PLATFORM_MT6797 ||
                     Platform::getInstance().m_config.platform == PLATFORM_MT6799)
            {
                if (dpy == HWC_DISPLAY_VIRTUAL &&
                    m_data[dpy].subtype != HWC_DISPLAY_WIRELESS)
                    break;

                // reg and enable CPU configuration
                // use 2 big core
                g_mhl_cpu_scenario = PerfServiceNative_userRegBigLittle(2, 0, 0, 0);
                if (-1 == g_mhl_cpu_scenario)
                {
                    HWC_LOGW("Failed to register to PerfService");
                }
                else
                {
                    PerfServiceNative_userRegScnConfig(g_mhl_cpu_scenario,
                                                       CMD_SET_SCREEN_OFF_STATE,
                                                       SCREEN_OFF_ENABLE,
                                                       0,
                                                       0,
                                                       0);
                    PerfServiceNative_userEnable(g_mhl_cpu_scenario);
                    HWC_LOGI("Change Scenario=%d, dpy=%d connected",
                        g_mhl_cpu_scenario, dpy);
                }
            }
#endif // MTK_BASIC_PACKAGE

            break;

        case DISP_PLUG_DISCONNECT:
            HWC_LOGI("Removed Display Information:");
            if (print_info)
            {
                printDisplayInfo(dpy);
            }
            memset((void*)disp_data, 0, sizeof(DisplayData));
            compareDisplaySize(dpy);
            m_curr_disp_num--;

#ifndef MTK_BASIC_PACKAGE
            // TODO: should be put in scenario manager
            if (Platform::getInstance().m_config.platform == PLATFORM_MT6752 ||
                Platform::getInstance().m_config.platform == PLATFORM_MT6795)
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
            else if (Platform::getInstance().m_config.platform == PLATFORM_MT6755 ||
                     Platform::getInstance().m_config.platform == PLATFORM_MT6763 ||
                     Platform::getInstance().m_config.platform == PLATFORM_MT6797 ||
                     Platform::getInstance().m_config.platform == PLATFORM_MT6799)
            {
                // disable and unreg CPU configuration
                if (-1 != g_mhl_cpu_scenario)
                {
                    PerfServiceNative_userDisable(g_mhl_cpu_scenario);
                    HWC_LOGI("Change Scenario=%d, dpy=%d disconnected",
                        g_mhl_cpu_scenario, dpy);

                    PerfServiceNative_userUnregScn(g_mhl_cpu_scenario);
                    PerfServiceNative_userUnreg(g_mhl_cpu_scenario);
                    g_mhl_cpu_scenario = -1;
                }
            }
#endif // MTK_BASIC_PACKAGE

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

void DisplayManager::compareDisplaySize(int dpy)
{
    if (dpy >= MAX_DISPLAYS || dpy < HWC_DISPLAY_PRIMARY) return;

    DisplayData* display_data = &m_data[dpy];
    AutoMutex _l(m_state_lock);
    if (display_data->connected)
    {
        // threshold should be the resolution of primary display
        // however, we only care MHL 4k output now.
        size_t size = display_data->width * display_data->height;
        m_state_ultra_display[dpy] = (size >= Platform::getInstance().getLimitedExternalDisplaySize());
    }
    else
    {
        m_state_ultra_display[dpy] = false;
    }
}

bool DisplayManager::isUltraDisplay(int dpy)
{
    AutoMutex _l(m_state_lock);
    return m_state_ultra_display[dpy];
}

bool DisplayManager::isAllDisplayOff()
{
    AutoMutex _l(m_state_lock);
    int res = 0;
    for (int i = 0; i < MAX_DISPLAYS; i++)
    {
        if (m_display_power_state[i])
        {
            res |= 1 << i;
        }
    }

    HWC_LOGD("all panel state: %x", res);
    return res ? false : true;
}

void DisplayManager::setDisplayPowerState(int dpy, int state)
{
    AutoMutex _l(m_state_lock);

    if (state == HWC_POWER_MODE_OFF)
    {
        m_display_power_state[dpy] = false;
    }
    else
    {
        m_display_power_state[dpy] = true;
    }
}

void DisplayManager::setWakelockTimerState(int state)
{
    AutoMutex _l(m_state_lock);
    if (m_wake_lock_timer != NULL)
    {
        switch (state)
        {
            case WAKELOCK_TIMER_PAUSE:
                m_wake_lock_timer->pause();
                break;
            case WAKELOCK_TIMER_START:
                m_wake_lock_timer->count();
                break;
            case WAKELOCK_TIMER_PLAYOFF:
                m_wake_lock_timer->kick();
                break;
            default:
                HWC_LOGE("set wrong state of wakelock timer");
                break;
        }
    }
}

void DisplayManager::getPhysicalPanelSize(unsigned int *width, unsigned int *height,
                                          disp_session_info &info)
{
    if (width != NULL)
    {
        if (info.physicalWidthUm != 0)
        {
            *width = info.physicalWidthUm;
        }
        else
        {
            *width = info.physicalWidth * 1000;
        }
    }

    if (height != NULL)
    {
        if (info.physicalHeightUm != 0)
        {
            *height = info.physicalHeightUm;
        }
        else
        {
            *height = info.physicalHeight * 1000;
        }
    }
}

uint32_t DisplayManager::getVideoHdcp() const
{
    RWLock::AutoRLock _l(m_video_hdcp_rwlock);
    return m_video_hdcp;
}

void DisplayManager::setVideoHdcp(const uint32_t& video_hdcp)
{
    RWLock::AutoWLock _l(m_video_hdcp_rwlock);
    m_video_hdcp = video_hdcp;
}
