#define LOG_TAG "IAALService"

#define MTK_LOG_ENABLE 1
#include <cutils/log.h>
#include <AAL20/IAALService.h>

namespace android {

// client : proxy AAL class
class BpAALService : public BpInterface<IAALService>
{

public:
    BpAALService(const sp<IBinder>& impl) : BpInterface<IAALService>(impl)
    {
    }

    virtual status_t setFunction(uint32_t funcFlags)
    {
        Parcel data, reply;
        data.writeInt32(funcFlags);
        if (remote()->transact(AAL_SET_FUNCTION, data, &reply) != NO_ERROR) {
            ALOGE("setFunction could not contact remote\n");
            return -1;
        }

        return reply.readInt32();
    }

    virtual status_t setLightSensorValue(int32_t value)
    {
        Parcel data, reply;
        data.writeInt32(value);
        if (remote()->transact(AAL_SET_LIGHT_SENSOR_VALUE, data, &reply) != NO_ERROR) {
            ALOGE("setLightSensorValue could not contact remote\n");
            return -1;
        }

        return reply.readInt32();
    }

    virtual status_t setScreenState(int32_t state, int32_t brightness)
    {
        Parcel data, reply;
        data.writeInt32(state);
        data.writeInt32(brightness);
        if (remote()->transact(AAL_SET_SCREEN_STATE, data, &reply) != NO_ERROR) {
            ALOGE("setScreenState could not contact remote\n");
            return -1;
        }

        return reply.readInt32();
    }

    virtual status_t setSmartBacklightStrength(int32_t level)
    {
        Parcel data, reply;
        data.writeInt32(level);
        if (remote()->transact(AAL_SET_SMART_BACKLIGHT_STRENGTH, data, &reply) != NO_ERROR) {
            ALOGE("setSmartBacklightStrength could not contact remote\n");
            return -1;
        }

        return reply.readInt32();
    }

    virtual status_t setSmartBacklightRange(int32_t level)
    {
        Parcel data, reply;
        data.writeInt32(level);
        if (remote()->transact(AAL_SET_SMART_BACKLIGHT_RANGE, data, &reply) != NO_ERROR) {
            ALOGE("setSmartBacklightRange could not contact remote\n");
            return -1;
        }

        return reply.readInt32();
    }

    virtual status_t setReadabilityLevel(int32_t level)
    {
        Parcel data, reply;
        data.writeInt32(level);
        if (remote()->transact(AAL_SET_READABILITY_LEVEL, data, &reply) != NO_ERROR) {
            ALOGE("setReadabilityLevel could not contact remote\n");
            return -1;
        }

        return reply.readInt32();
    }

    virtual status_t setLowBLReadabilityLevel(int32_t level)
    {
        Parcel data, reply;
        data.writeInt32(level);
        if (remote()->transact(AAL_SET_LOW_BL_READABILITY_LEVEL, data, &reply) != NO_ERROR) {
            ALOGE("setLowBLReadabilityLevel could not contact remote\n");
            return -1;
        }

        return reply.readInt32();
    }


    virtual status_t getParameters(AALParameters *outParam)
    {
        Parcel data, reply;
        if (remote()->transact(AAL_GET_PARAMETERS, data, &reply) != NO_ERROR) {
            ALOGE("getParamters could not contact remote\n");
            return -1;
        }

        status_t ret = static_cast<status_t>(reply.readInt32());
        outParam->readabilityLevel= reply.readInt32();
        outParam->lowBLReadabilityLevel= reply.readInt32();
        outParam->smartBacklightStrength = reply.readInt32();
        outParam->smartBacklightRange = reply.readInt32();

        return ret;
    }

    virtual status_t custInvoke(int32_t cmd, int64_t arg)
    {
        Parcel data, reply;
        data.writeInt32(cmd);
        data.writeInt64(arg);
        if (remote()->transact(AAL_CUST_INVOKE, data, &reply) != NO_ERROR) {
            ALOGE("custInvoke could not contact remote\n");
            return -1;
        }

        return reply.readInt32();
    }

    virtual status_t readField(uint32_t field, uint32_t *value)
    {
        Parcel data, reply;
        data.writeInt32(field);
        if (remote()->transact(AAL_READ_FIELD, data, &reply) != NO_ERROR) {
            ALOGE("custInvoke could not contact remote\n");
            return -1;
        }

        status_t ret = reply.readInt32();
        *value = static_cast<uint32_t>(reply.readInt32());

        return ret;
    }

    virtual status_t writeField(uint32_t field, uint32_t value)
    {
        Parcel data, reply;
        data.writeInt32(field);
        data.writeInt32(value);
        if (remote()->transact(AAL_WRITE_FIELD, data, &reply) != NO_ERROR) {
            ALOGE("custInvoke could not contact remote\n");
            return -1;
        }

        return reply.readInt32();
    }

    virtual status_t setAdaptField(AdaptFieldId field, void *data, int32_t size, uint32_t *serial)
    {
        Parcel send, reply;
        send.writeInt32(field);
        send.writeInt32(size);
        send.write(data, size);
        if (remote()->transact(AAL_SET_ADAPT_FIELD, send, &reply) != NO_ERROR) {
            ALOGE("setAdaptField could not contact remote\n");
            if (serial != NULL)
                *serial = 0;
            return -1;
        }

        status_t ret = reply.readInt32();
        if (serial != NULL)
            *serial = static_cast<uint32_t>(reply.readInt32());

        return ret;
    }

    virtual status_t getAdaptSerial(AdaptFieldId field, uint32_t *value)
    {
        Parcel send, reply;
        send.writeInt32(field);
        if (remote()->transact(AAL_GET_ADAPT_SERIAL, send, &reply) != NO_ERROR) {
            ALOGE("getAdaptSerial could not contact remote\n");
            *value = 0;
            return -1;
        }

        status_t ret = reply.readInt32();
        *value = static_cast<uint32_t>(reply.readInt32());

        return ret;
    }

    virtual status_t getAdaptField(AdaptFieldId field, void *data, int32_t size, uint32_t *serial)
    {
        Parcel send, reply;
        send.writeInt32(field);
        send.writeInt32(size);
        if (remote()->transact(AAL_GET_ADAPT_FIELD, send, &reply) != NO_ERROR) {
            ALOGE("getAdaptField could not contact remote\n");
            if (serial != NULL)
                *serial = 0;
            return -1;
        }

        status_t ret = reply.readInt32();
        if (ret == NO_ERROR)
            reply.read(data, size);
        if (serial != NULL)
            *serial = static_cast<uint32_t>(reply.readInt32());

        return ret;
    }

};


IMPLEMENT_META_INTERFACE(AALService, "AALService");

status_t BnAALService::onTransact(uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    status_t ret = 0;
    //ALOGD("receieve the command code %d", code);

    switch(code)
    {
        case AAL_SET_FUNCTION:
            {
                int32_t funcFlags;
                data.readInt32(&funcFlags);
                ret = setFunction((uint32_t)funcFlags);
                reply->writeInt32(ret);
            }
            break;
        case AAL_SET_LIGHT_SENSOR_VALUE:
            {
                int value;
                data.readInt32(&value);
                ret = setLightSensorValue(value);
                reply->writeInt32(ret);
            }
            break;
        case AAL_SET_SCREEN_STATE:
            {
                int state, brightness;
                data.readInt32(&state);
                data.readInt32(&brightness);
                ret = setScreenState(state, brightness);
                reply->writeInt32(ret);
            }
            break;
        case AAL_SET_SMART_BACKLIGHT_STRENGTH:
            {
                int level;
                data.readInt32(&level);
                ret = setSmartBacklightStrength(level);
                reply->writeInt32(ret);
            }
            break;
        case AAL_SET_SMART_BACKLIGHT_RANGE:
            {
                int level;
                data.readInt32(&level);
                ret = setSmartBacklightRange(level);
                reply->writeInt32(ret);
            }
            break;
        case AAL_SET_READABILITY_LEVEL:
            {
                int level;
                data.readInt32(&level);
                ret = setReadabilityLevel(level);
                reply->writeInt32(ret);
            }
            break;
        case AAL_SET_LOW_BL_READABILITY_LEVEL:
            {
                int level;
                data.readInt32(&level);
                ret = setLowBLReadabilityLevel(level);
                reply->writeInt32(ret);
            }
            break;
        case AAL_GET_PARAMETERS:
            {
                AALParameters param;
                ret = getParameters(&param);
                reply->writeInt32(ret);
                reply->writeInt32(param.readabilityLevel);
                reply->writeInt32(param.lowBLReadabilityLevel);
                reply->writeInt32(param.smartBacklightStrength);
                reply->writeInt32(param.smartBacklightRange);
            }
            break;
        case AAL_CUST_INVOKE:
            {
                int32_t cmd;
                int64_t arg;
                data.readInt32(&cmd);
                ret = data.readInt64(&arg);
                if (ret == NO_ERROR)
                    ret = custInvoke(cmd, arg);
                reply->writeInt32(ret);
            }
            break;
        case AAL_READ_FIELD:
            {
                int32_t field;
                uint32_t value;
                data.readInt32(&field);
                ret = readField(field, &value);
                reply->writeInt32(ret);
                reply->writeInt32(static_cast<int32_t>(value));
            }
            break;
        case AAL_WRITE_FIELD:
            {
                int32_t field;
                int32_t value;
                data.readInt32(&field);
                data.readInt32(&value);
                ret = writeField(field, static_cast<uint32_t>(value));
                reply->writeInt32(ret);
            }
            break;
        case AAL_SET_ADAPT_FIELD:
            {
                int32_t field;
                int32_t size;
                uint32_t serial;
                data.readInt32(&field);
                data.readInt32(&size);
                char *bdata = new char[size];
                data.read(bdata, size);
                ret = setAdaptField(static_cast<AdaptFieldId>(field), bdata, size, &serial);
                delete [] bdata;
                reply->writeInt32(ret);
                reply->writeInt32(static_cast<int32_t>(serial));
            }
            break;
        case AAL_GET_ADAPT_SERIAL:
            {
                int32_t field;
                uint32_t value;
                data.readInt32(&field);
                ret = getAdaptSerial(static_cast<AdaptFieldId>(field), &value);
                reply->writeInt32(ret);
                reply->writeInt32(static_cast<int32_t>(value));
            }
            break;
        case AAL_GET_ADAPT_FIELD:
            {
                int32_t field;
                int32_t size;
                uint32_t serial;
                data.readInt32(&field);
                data.readInt32(&size);
                char *bdata = new char[size];
                ret = getAdaptField(static_cast<AdaptFieldId>(field), bdata, size, &serial);
                reply->writeInt32(ret);
                if (ret == NO_ERROR)
                    reply->write(bdata, size);
                reply->writeInt32(static_cast<int32_t>(serial));
                delete [] bdata;
            }
            break;
        default:
            return BBinder::onTransact(code, data, reply, flags);
    }
    return ret;
}
};
