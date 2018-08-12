#include <AAL20/AALService.h>


// For runtime tuning of Java

namespace android {

class AdaptFields {
public:
    struct {
        uint32_t serial;
        int length;
        int *ali;
        int *bli;
    } ali2Bli;

    struct RampRate {
        uint32_t serial;
        int rate;
    };
    RampRate brightenRate, darkenRate;

    void releaseAli2Bli() {
        if (ali2Bli.ali != NULL) {
            delete [] ali2Bli.ali;
            ali2Bli.ali = NULL;
        }
        if (ali2Bli.bli != NULL) {
            delete [] ali2Bli.bli;
            ali2Bli.bli = NULL;
        }
        ali2Bli.length = 0;
    }

    void createAli2Bli(int length) {
        releaseAli2Bli();
        ali2Bli.serial++;
        ali2Bli.length = length;
        ali2Bli.ali = new int[length];
        ali2Bli.bli = new int[length];
    }

    AdaptFields() {
        ali2Bli.serial = 0;
        ali2Bli.length = 0;
        ali2Bli.ali = NULL;
        ali2Bli.bli = NULL;

        brightenRate.serial = 0;
        brightenRate.rate = 40;
        darkenRate.serial = 0;
        darkenRate.rate = 40;
    }

    ~AdaptFields() {
        releaseAli2Bli();
    }
};


void AALService::initAdapt()
{
    mAdaptFields = new AdaptFields;
}


void AALService::deinitAdapt()
{
    delete mAdaptFields;
}


status_t AALService::setAdaptField(IAALService::AdaptFieldId field, void *data, int32_t size, uint32_t *serial)
{
    Mutex::Autolock _l(mLock);

    switch (field) {
    case IAALService::ALI2BLI_CURVE:
        {
        mAdaptFields->releaseAli2Bli();
        int length = (size / sizeof(int)) / 2;
        if (length <= 3 || length > 1024)
            return BAD_VALUE;

        mAdaptFields->createAli2Bli(length);
        memcpy(mAdaptFields->ali2Bli.ali, data, sizeof(int) * length);
        memcpy(mAdaptFields->ali2Bli.bli, (char*)data + sizeof(int) * length, sizeof(int) * length);

        if (serial != NULL)
            *serial = mAdaptFields->ali2Bli.serial;
        }
        break;

    case IAALService::BLI_RAMP_RATE_BRIGHTEN:
    case IAALService::BLI_RAMP_RATE_DARKEN:
        {
        if (size != sizeof(int))
            return BAD_VALUE;
        int rate = *static_cast<int*>(data);
        if (rate <= 0)
            return BAD_VALUE;

        AdaptFields::RampRate *rateField = &(mAdaptFields->brightenRate);
        if (field == IAALService::BLI_RAMP_RATE_DARKEN)
            rateField = &(mAdaptFields->darkenRate);

        rateField->serial++;
        rateField->rate = rate;
        if (serial != NULL)
            *serial = rateField->serial;
        }
        break;

    default:
        return BAD_INDEX;
    }

    return NO_ERROR;
}


status_t AALService::getAdaptSerial(IAALService::AdaptFieldId field, uint32_t *value)
{
    Mutex::Autolock _l(mLock);

    switch (field) {
    case IAALService::ALI2BLI_CURVE_LENGTH:
    case IAALService::ALI2BLI_CURVE:
        *value = mAdaptFields->ali2Bli.serial;
        break;

    case IAALService::BLI_RAMP_RATE_BRIGHTEN:
        *value = mAdaptFields->brightenRate.serial;
        break;

    case IAALService::BLI_RAMP_RATE_DARKEN:
        *value = mAdaptFields->darkenRate.serial;
        break;

    default:
        return BAD_INDEX;
    }

    return NO_ERROR;
}


status_t AALService::getAdaptField(IAALService::AdaptFieldId field, void *data, int32_t size, uint32_t *serial)
{
    Mutex::Autolock _l(mLock);

    switch (field) {
    case IAALService::ALI2BLI_CURVE_LENGTH:
        if (size != sizeof(int))
            return BAD_VALUE;
        memcpy(data, &(mAdaptFields->ali2Bli.length), sizeof(int));
        break;

    case IAALService::ALI2BLI_CURVE:
        {
        int length = mAdaptFields->ali2Bli.length;

        if (size != int(sizeof(int)) * length * 2)
            return BAD_VALUE;

        memcpy(data, mAdaptFields->ali2Bli.ali, sizeof(int) * length);
        memcpy((char*)data + sizeof(int) * length, mAdaptFields->ali2Bli.bli, sizeof(int) * length);
        if (serial != NULL)
            *serial = mAdaptFields->ali2Bli.serial;
        }
        break;

    case IAALService::BLI_RAMP_RATE_BRIGHTEN:
    case IAALService::BLI_RAMP_RATE_DARKEN:
        {
        if (size != sizeof(int))
            return BAD_VALUE;

        int *rate = static_cast<int*>(data);

        AdaptFields::RampRate *rateField = &(mAdaptFields->brightenRate);
        if (field == IAALService::BLI_RAMP_RATE_DARKEN)
            rateField = &(mAdaptFields->darkenRate);

        *rate = rateField->rate;

        if (serial != NULL)
            *serial = rateField->serial;
        }
        break;

    default:
        return BAD_INDEX;
    }

    return NO_ERROR;
}

};

