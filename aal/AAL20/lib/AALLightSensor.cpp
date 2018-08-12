#define LOG_TAG "AALLightSensor"

#define MTK_LOG_ENABLE 1
#include <cutils/log.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <time.h>
#include <linux/sensors_io.h>
#include "AALLightSensor.h"


#define MILLI_TO_NANO(v) (static_cast<nsecs_t>(v) * static_cast<nsecs_t>(1000L * 1000L))
#define MILLI_TO_MICRO(v) (static_cast<nsecs_t>(v) * static_cast<nsecs_t>(1000L))

namespace android
{

millisec_t AALLightSensor::DEBOUNCE_TIME = 4000L;
millisec_t AALLightSensor::SHORT_TERM_PERIOD = 100L;
millisec_t AALLightSensor::LONG_TERM_PERIOD = 500L;
int AALLightSensor::mOriginalLux = -1;

int AALLightSensor::updateSensorValue(__unused int fd, __unused int events, void* data)
{
    int lux = -1;

    sp<SensorEventQueue> q((SensorEventQueue*)data);
    ssize_t n;
    ASensorEvent buffer[1];

    while ((n = q->read(buffer, 1)) > 0) {
        /* ALOGD("sensor_receiver %f\n", buffer[0].data[0]); */
    }

    if (n < 0 && n != -EAGAIN) {
        ALOGE("Get als from sensor hal fail");
    } else {
        mOriginalLux = int(buffer[0].data[0]);
        /* ALOGD("sensor_receiver(ori) %d\n", mOriginalLux); */
    }

    return 1;
}

AALLightSensor::AALLightSensor() :
    mIsEnabled(false),
    mLastObservedLuxTime(0),
    mShortTermAverageLux(-1),
    mLongTermAverageLux(-1),
    mDebounceLuxTime(0),
    mDebouncedLux(-1),
    mNotifiedLux(-1),
    mContRunning(true),
    mListenerCallback(NULL),
    mListenerUser(NULL),
    mSensorMgr(NULL),
    mSensorList(NULL),
    mSensorCount(-1),
    mLightsensor(NULL)

{
    mContRunning = false;
}


AALLightSensor::~AALLightSensor()
{
    mContRunning = false;
    setEnabled(false);
    mWaitCond.broadcast();
    join();
}


void AALLightSensor::loadCustParameters(CustParameters &cust) {
    if (cust.isGood()) {
        unsigned long alsParam;

        if (cust.loadVar("AlsDebounceTime", &alsParam))
            DEBOUNCE_TIME = alsParam;
        if (cust.loadVar("AlsShortTermPeriod", &alsParam))
            SHORT_TERM_PERIOD = alsParam;
        if (cust.loadVar("AlsLongTermPeriod", &alsParam))
            LONG_TERM_PERIOD = alsParam;
    }
}


millisec_t AALLightSensor::getTime()
{
    struct timeval time;
    gettimeofday(&time, NULL);

    return static_cast<millisec_t>(time.tv_sec) * 1000L +
        static_cast<millisec_t>(time.tv_usec / 1000L);
}


void AALLightSensor::setEnabled(bool enabled)
{
    ALOGD("AALLightSensor setEnabled %d-->%d", mIsEnabled, enabled);
    if (enabled != mIsEnabled) {
        Mutex::Autolock _l(mLock);

        if (enabled) {
            mShortTermAverageLux = -1;
            mLongTermAverageLux = -1;
            mDebounceLuxTime = 0;
        }

        int enableVal = (enabled ? 1 : 0);
        status_t ret = NO_ERROR;
        if (enableVal == 1) {
            if (getTid() == -1) {
                run("AALLightSensor");
            } else {
                if (mContRunning == true) {
                    ret = mSensorEventQueue->enableSensor(mLightsensor, MILLI_TO_MICRO(200));
                    ALOGD("AALLightSensor Enabled ret=%d", ret);
                } else {
                    ALOGE("connect sensor service fail");
                }
            }
        } else {
            if (getTid() == -1) {
                /* no action */
            } else {
                if (mContRunning == true) {
                    ret = mSensorEventQueue->disableSensor(mLightsensor);
                    ALOGD("AALLightSensor Disabled ret=%d", ret);
                } else {
                    ALOGE("connect sensor service fail");
                }
            }
        }

        if (ret >= 0) {
            mDebouncedLux = -1;
            mIsEnabled = enabled;
            mWaitCond.broadcast();
        }

    }
}


void AALLightSensor::updateLuxValue(int lux)
{
    millisec_t currentTime = getTime();

    if (mShortTermAverageLux < 0) {
        mShortTermAverageLux = lux;
        mLongTermAverageLux = lux;
        mDebouncedLux = lux;
    } else {
        millisec_t timeDelta = currentTime - mLastObservedLuxTime;
        if (timeDelta > 0) {
            mShortTermAverageLux =
                (mShortTermAverageLux * SHORT_TERM_PERIOD + lux * timeDelta) /
                (SHORT_TERM_PERIOD + timeDelta);
            mLongTermAverageLux =
                (mLongTermAverageLux * LONG_TERM_PERIOD + lux * timeDelta) /
                (LONG_TERM_PERIOD + timeDelta);
        }
    }

    mLastObservedLuxTime = currentTime;


    // Check brightening or darkening
    int brighteningThreshold = mDebouncedLux + mDebouncedLux / 5;     // * 1.2
    int darkeningThreshold = mDebouncedLux - (mDebouncedLux * 2 / 5); // * 0.6

    bool brightening, darkening;
    brightening = (mShortTermAverageLux > brighteningThreshold &&
            mLongTermAverageLux > brighteningThreshold);
    darkening = (mShortTermAverageLux < darkeningThreshold &&
            mLongTermAverageLux < darkeningThreshold);

    if (!(brightening || darkening)) {
        mDebounceLuxTime = currentTime;
        return;
    }


    // Check debounce time
    millisec_t debounceTime = mDebounceLuxTime + DEBOUNCE_TIME;
    if (currentTime < debounceTime)
        return;

    mDebounceLuxTime = currentTime;
    mDebouncedLux = mShortTermAverageLux;
}


bool AALLightSensor::threadLoop()
{
    millisec_t logPrintTime = 0;

INIT:
    while (defaultServiceManager()->checkService(String16("sensorservice")) == NULL)
    {
        ALOGD("sensorservice is not ready\n");
        mContRunning = false;
        sleep(1);
    }

    ALOGD("sensorservice is ready\n");

    mSensorMgr = &SensorManager::getInstanceForPackage(String16("AALLightSensor"));
    mSensorCount = mSensorMgr->getSensorList(&mSensorList);
    mSensorEventQueue = mSensorMgr->createEventQueue();
    mLightsensor = mSensorMgr->getDefaultSensor(SENSOR_TYPE_LIGHT);
    if (mLightsensor != NULL) {
        mSensorEventQueue->enableSensor(mLightsensor,MILLI_TO_MICRO(200));
        mContRunning = true;
    } else {
        mContRunning = false;
    }

    sp<IBinder> sensorBinder = defaultServiceManager()->getService(String16("sensorservice"));

    class DeathObserver : public IBinder::DeathRecipient {
        volatile bool *m_reconnect;
        virtual void binderDied(const wp<IBinder>& who) {
            ALOGW("sensorservice died ");
            *m_reconnect = true;
        }
    public:
        explicit DeathObserver(volatile bool *reconnect) : m_reconnect(reconnect){ }
    };

    volatile bool  reconnectSensorService = false;

    sp<IBinder::DeathRecipient> sensorObserver;
    sensorObserver = new DeathObserver(&reconnectSensorService);

    sensorBinder->linkToDeath(sensorObserver, 0);

    sp<Looper> loop = new Looper(false);
    loop->addFd(mSensorEventQueue->getFd(), 0, ALOOPER_EVENT_INPUT, updateSensorValue, mSensorEventQueue.get());

    while (mContRunning) {
        if (reconnectSensorService)
        {
            mSensorMgr = NULL;
            loop = NULL;
            mSensorEventQueue = NULL;
            sensorObserver = NULL;

            goto INIT;
        }

        int debouncedLux;
        {
            Mutex::Autolock _l(mLock);

            if (mIsEnabled) {
                int newLux = mDebouncedLux;
                int32_t ret = loop->pollOnce(200);

                if (ret != ALOOPER_POLL_CALLBACK && ret != ALOOPER_POLL_TIMEOUT)
                    ALOGE("Poll als abnormal, ret=%d", ret);

                updateLuxValue(mOriginalLux);

                millisec_t current = getTime();
                if (current - logPrintTime >= 5000) { // 5 sec
                    ALOGD("newLux = %d, [%d, %d] -> %d",
                        newLux, mShortTermAverageLux, mLongTermAverageLux, mDebouncedLux);
                    logPrintTime = current;
                }
            }
            debouncedLux = mDebouncedLux;
        }

        if (mListenerCallback != NULL && debouncedLux != mNotifiedLux) {
            mListenerCallback(mListenerUser, debouncedLux);
            mNotifiedLux = debouncedLux;
        }

        {
            Mutex::Autolock _l(mLock);

            if (mIsEnabled) {
                if (mDebouncedLux < 0)
                    mWaitCond.waitRelative(mLock, MILLI_TO_NANO(100));
                else
                    mWaitCond.waitRelative(mLock, MILLI_TO_NANO(200));
            } else {
                // 10 sec
                mWaitCond.waitRelative(mLock, MILLI_TO_NANO(10 * 1000));
            }
        }

    }

    return mContRunning;
}

};
