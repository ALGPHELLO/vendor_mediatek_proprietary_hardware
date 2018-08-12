#define LOG_TAG "AALClient"

#include <cutils/log.h>
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <AAL20/IAALService.h>
#include <AALClient.h>

namespace android {


ANDROID_SINGLETON_STATIC_INSTANCE(AALClient);

AALClient::AALClient()
{
    assertStateLocked();
}

status_t AALClient::assertStateLocked() const
{
    int count = 0;
    if (mAALService == NULL)
    {
        // try for one second
        const String16 name("AAL");
        do {
            status_t err = getService(name, &mAALService);
            if (err == NAME_NOT_FOUND) {
                if (count < 3)
                {
                    ALOGW("AALService not published, waiting...");
                    usleep(100000);
                    count++;
                    continue;
                }
                return err;
            }
            if (err != NO_ERROR) {
                return err;
            }
            break;
        } while (true);

        class DeathObserver : public IBinder::DeathRecipient {
            AALClient & mAALClient;
            virtual void binderDied(const wp<IBinder>& who) {
                ALOGW("AAL Service died [%p]", who.unsafe_get());
                mAALClient.serviceDied();
            }
        public:
            DeathObserver(AALClient & service) : mAALClient(service) { }
        };

        mDeathObserver = new DeathObserver(*const_cast<AALClient*>(this));
        IInterface::asBinder(mAALService)->linkToDeath(mDeathObserver);
    }
    return NO_ERROR;
}

void AALClient::serviceDied()
{
    Mutex::Autolock _l(mLock);
    mAALService.clear();
}

status_t AALClient::setMode(int32_t mode)
{
    uint32_t funcFlags;

    switch (mode) {
    case IAALService::SCREEN_BRIGHTNESS_MODE_MANUAL:
        funcFlags = IAALService::FUNC_NONE;
        break;
    case IAALService::SCREEN_BRIGHTNESS_MODE_AUTOMATIC:
        funcFlags = (IAALService::FUNC_CABC | IAALService::FUNC_DRE);
        break;
    case IAALService::SCREEN_BRIGHTNESS_ECO_MODE_AUTOMATIC:
        funcFlags = IAALService::FUNC_CABC;
        break;
    default:
        funcFlags = (mode >> 16) & (IAALService::FUNC_CABC | IAALService::FUNC_DRE);
        break;
    }

    return setFunction(funcFlags);
}


status_t AALClient::setFunction(uint32_t funcFlags)
{
    status_t err;
    Mutex::Autolock _l(mLock);
    err = assertStateLocked();

    if (err == NO_ERROR)
        err = mAALService->setFunction(funcFlags);

    return err;
}


status_t AALClient::setLightSensorValue(int32_t value)
{
    status_t err;
    Mutex::Autolock _l(mLock);
    err = assertStateLocked();
    if (err == NO_ERROR)
        err = mAALService->setLightSensorValue(value);

    return err;
}


status_t AALClient::setScreenState(int32_t state, int32_t brightness)
{
    status_t err;
    Mutex::Autolock _l(mLock);
    err = assertStateLocked();
    if (err == NO_ERROR)
        err = mAALService->setScreenState(state, brightness);

    return err;
}


status_t AALClient::setSmartBacklightStrength(int32_t level)
{
    status_t err;
    Mutex::Autolock _l(mLock);
    err = assertStateLocked();
    if (err == NO_ERROR)
        err = mAALService->setSmartBacklightStrength(level);

    return err;
}

status_t AALClient::setSmartBacklightRange(int32_t level)
{
    status_t err;
    Mutex::Autolock _l(mLock);
    err = assertStateLocked();
    if (err == NO_ERROR)
        err = mAALService->setSmartBacklightRange(level);

    return err;
}

status_t AALClient::setReadabilityLevel(int32_t level)
{
    status_t err;
    Mutex::Autolock _l(mLock);
    err = assertStateLocked();
    if (err == NO_ERROR)
        err = mAALService->setReadabilityLevel(level);

    return err;
}

status_t AALClient::setLowBLReadabilityLevel(int32_t level)
{
    status_t err;
    Mutex::Autolock _l(mLock);
    err = assertStateLocked();
    if (err == NO_ERROR)
        err = mAALService->setLowBLReadabilityLevel(level);

    return err;
}

status_t AALClient::getParameters(AALParameters *outParam)
{
    status_t err;
    Mutex::Autolock _l(mLock);
    err = assertStateLocked();
    if (err == NO_ERROR)
        err = mAALService->getParameters(outParam);

    return err;
}


status_t AALClient::custInvoke(int32_t cmd, int64_t arg)
{
    status_t err;
    Mutex::Autolock _l(mLock);
    err = assertStateLocked();
    if (err == NO_ERROR)
        err = mAALService->custInvoke(cmd, arg);

    return err;
}


status_t AALClient::readField(uint32_t field, uint32_t *value)
{
    status_t err;
    Mutex::Autolock _l(mLock);
    err = assertStateLocked();
    if (err == NO_ERROR)
        err = mAALService->readField(field, value);

    return err;
}


status_t AALClient::writeField(uint32_t field, uint32_t value)
{
    status_t err;
    Mutex::Autolock _l(mLock);
    err = assertStateLocked();
    if (err == NO_ERROR)
        err = mAALService->writeField(field, value);

    return err;

}


status_t AALClient::setAdaptField(IAALService::AdaptFieldId field, void *data, int32_t size, uint32_t *serial)
{
    status_t err;
    Mutex::Autolock _l(mLock);
    err = assertStateLocked();
    if (err == NO_ERROR)
        err = mAALService->setAdaptField(field, data, size, serial);

    return err;
}


status_t AALClient::getAdaptSerial(IAALService::AdaptFieldId field, uint32_t *value)
{
    status_t err;
    Mutex::Autolock _l(mLock);
    err = assertStateLocked();
    if (err == NO_ERROR)
        err = mAALService->getAdaptSerial(field, value);

    return err;
}


status_t AALClient::getAdaptField(IAALService::AdaptFieldId field, void *data, int32_t size, uint32_t *serial)
{
    status_t err;
    Mutex::Autolock _l(mLock);
    err = assertStateLocked();
    if (err == NO_ERROR)
        err = mAALService->getAdaptField(field, data, size, serial);

    return err;
}



};
