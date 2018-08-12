#ifndef __AAL_LIGHT_SENSOR__
#define __AAL_LIGHT_SENSOR__

#include <utils/threads.h>
#include <AAL20/AALCust.h>
#include <inttypes.h>
#include <sensor/Sensor.h>
#include <sensor/SensorManager.h>
#include <sensor/SensorEventQueue.h>
#include <utils/Looper.h>
#include <cutils/log.h>
#include <time.h>
#include "linux/hwmsensor.h"
#include <sensor/ISensorServer.h>

namespace android
{

typedef long long millisec_t;

class AALLightSensor : public Thread
{
public:
    typedef void (*ListenerCallback)(void *user, int ali);

private:
    static millisec_t DEBOUNCE_TIME;
    static millisec_t SHORT_TERM_PERIOD, LONG_TERM_PERIOD;

    Mutex mLock;
    Condition mWaitCond;
    bool mIsEnabled;
    millisec_t mLastObservedLuxTime;
    int mShortTermAverageLux;
    int mLongTermAverageLux;
    millisec_t mDebounceLuxTime;
    static int mOriginalLux;
    int mDebouncedLux;
    int mNotifiedLux;
    volatile bool mContRunning;

    void updateLuxValue(int lux);
    static int updateSensorValue(__unused int fd, __unused int events, void* data);

    static millisec_t getTime();

    ListenerCallback mListenerCallback;
    void *mListenerUser;

    SensorManager* mSensorMgr;
    Sensor const* const* mSensorList;
    ssize_t mSensorCount;
    sp<SensorEventQueue> mSensorEventQueue;
    Sensor const* mLightsensor;

public:

    AALLightSensor();
    ~AALLightSensor();

    void setListener(ListenerCallback callback, void *user) {
        mListenerCallback = callback;
        mListenerUser = user;
    }

    static void loadCustParameters(CustParameters &cust);

    bool isEnabled() {
        return mIsEnabled;
    }

    void setEnabled(bool enabled);

    int getLuxValue() {
        Mutex::Autolock _l(mLock);
        return mDebouncedLux;
    }

    virtual bool threadLoop();
};

};

#endif

