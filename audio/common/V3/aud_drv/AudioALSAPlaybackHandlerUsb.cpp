#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "AudioALSAPlaybackHandlerUsb"

#include "AudioALSAPlaybackHandlerUsb.h"

#include "AudioALSAHardwareResourceManager.h"
#include "AudioVolumeFactory.h"
#include "AudioALSASampleRateController.h"

#include "AudioMTKFilter.h"
#include "AudioVUnlockDL.h"
#include "AudioALSADeviceParser.h"
#include "AudioALSADriverUtility.h"
#if defined(MTK_SPEAKER_MONITOR_SUPPORT)
#include "AudioALSASpeakerMonitor.h"
#endif

#undef MTK_HDMI_SUPPORT

#if defined(MTK_HDMI_SUPPORT)
#include "AudioExtDisp.h"
#endif

#ifdef MTK_DYNAMIC_BUFFER_SIZE_SUPPORT
#include "AudioALSAStreamManager.h"
#endif
#include <fstream>
#include "AudioUtility.h"
#include "AudioUSBPhoneCallController.h"
#include "MtkAudioComponent.h"

#ifdef MTK_AURISYS_FRAMEWORK_SUPPORT
#include <audio_memory_control.h>
#include <audio_lock.h>
#include <audio_ringbuf.h>
#include <audio_task.h>
#include <aurisys_scenario.h>
#include <arsi_type.h>
#include <audio_pool_buf_handler.h>
#include <aurisys_controller.h>
#include <aurisys_lib_manager.h>
#endif

#include <AudioALSACaptureDataProviderEchoRefUsb.h>
#include "AudioALSACaptureDataProviderUsb.h"
#include "AudioALSAStreamManager.h"


#if (defined(MTK_AUDIO_HIERARCHICAL_PARAM_SUPPORT) && (MTK_AUDIO_TUNING_TOOL_V2_PHASE >= 2))
#include "AudioParamParser.h"
#endif
extern "C" {
#include  "AudioParamParser.h"
}

extern "C" {
    //#include <tinyalsa/asoundlib.h>
#include "alsa_device_profile.h"
#include "alsa_device_proxy.h"
#include "alsa_logging.h"
#include <audio_utils/channels.h>
}


//#define DEBUG_TIMESTAMP

#ifdef DEBUG_TIMESTAMP
#define SHOW_TIMESTAMP(format, args...) ALOGD(format, ##args)
#else
#define SHOW_TIMESTAMP(format, args...)
#endif

#define calc_time_diff(x,y) ((x.tv_sec - y.tv_sec )+ (double)( x.tv_nsec - y.tv_nsec ) / (double)1000000000)

#define USB_SPH_DEVICE_PARAM_AUDIOTYPE_NAME "USBDevice"

#ifdef DEBUG_LATENCY
// Latency Detect
//#define DEBUG_LATENCY
#define THRESHOLD_FRAMEWORK   0.010
#define THRESHOLD_HAL         0.010
#define THRESHOLD_KERNEL      0.010
#endif

#define calc_time_diff(x,y) ((x.tv_sec - y.tv_sec )+ (double)( x.tv_nsec - y.tv_nsec ) / (double)1000000000)
static   const char PROPERTY_KEY_EXTDAC[PROPERTY_KEY_MAX]  = "af.resouce.extdac_support";

static const uint32_t kPcmDriverBufferSize = 0x20000;
static uint32_t kWriteBufferSize = 0;
static alsa_device_proxy *usbProxy = NULL;

namespace android {

AudioALSAPlaybackHandlerUsb::AudioALSAPlaybackHandlerUsb(const stream_attribute_t *stream_attribute_source) :
    AudioALSAPlaybackHandlerBase(stream_attribute_source),
    mTotalEchoRefBufSize(0),
    mDataProviderEchoRefUsb(NULL) {
    ALOGD("%s()", __FUNCTION__);
    mPlaybackHandlerType = PLAYBACK_HANDLER_USB;
    memset((void *)&mStreamAttributeTarget, 0, sizeof(mStreamAttributeTarget));
    memset((void *)&mStreamAttributeTargetEchoRef, 0, sizeof(mStreamAttributeTargetEchoRef));

    memset((void *)&mNewtime, 0, sizeof(mNewtime));
    memset((void *)&mOldtime, 0, sizeof(mOldtime));
    /* Init EchoRef Resource */
    memset(&mEchoRefStartTime, 0, sizeof(mEchoRefStartTime));
    memset(&mUSBOutStream, 0, sizeof(struct USBStream));
    memset(&mParam, 0, sizeof(struct USBCallParam));
}


AudioALSAPlaybackHandlerUsb::~AudioALSAPlaybackHandlerUsb() {
    ALOGD("%s()", __FUNCTION__);
}

void AudioALSAPlaybackHandlerUsb::initUsbInfo(stream_attribute_t mStreamAttributeTargetUSB, alsa_device_proxy *proxy, size_t buffer_size) {
    ALOGD("+%s()", __FUNCTION__);
    usbProxy = proxy;
    mStreamAttributeTarget = mStreamAttributeTargetUSB;
    mStreamAttributeTargetEchoRef = mStreamAttributeTargetUSB;
    kWriteBufferSize = buffer_size;
    ALOGD("-%s()", __FUNCTION__);
}


status_t AudioALSAPlaybackHandlerUsb::open() {
    ALOGD("+%s(), mDevice = 0x%x", __FUNCTION__, mStreamAttributeSource->output_devices);
    //AL_AUTOLOCK(*AudioALSADriverUtility::getInstance()->getStreamSramDramLock());

    // debug pcm dump
    OpenPCMDump(LOG_TAG);

    int ret = proxy_open(usbProxy);
    if (ret != 0) {
        ALOGD("%s(),proxy_open : BAD_VALUE", __FUNCTION__);
        return BAD_VALUE;
    }
    mPcm = usbProxy->pcm;

    profile_init(&mUSBOutStream.profile, PCM_OUT);
    alsa_device_profile *profile = usbProxy->profile;
    mUSBOutStream.profile.card = usbProxy->profile->card;
    mUSBOutStream.profile.device = usbProxy->profile->device;
    loadUSBDeviceParam();
    getDeviceId(&mUSBOutStream);
    getDeviceParam(&mUSBOutStream);
#ifdef MTK_AURISYS_FRAMEWORK_SUPPORT
    CreateAurisysLibManager();
#endif

    /* Reset software timestamp information */
    mTotalEchoRefBufSize = 0;
    memset((void *)&mEchoRefStartTime, 0, sizeof(mEchoRefStartTime));
    mTimeStampValid = false;
    mBytesWriteKernel = 0;
    ALOGD("-%s()", __FUNCTION__);
    return NO_ERROR;
}


status_t AudioALSAPlaybackHandlerUsb::close() {
    ALOGD("+%s()", __FUNCTION__);

    proxy_close(usbProxy);
    mPcm = NULL;

#ifdef MTK_AURISYS_FRAMEWORK_SUPPORT
    DestroyAurisysLibManager();
#endif

    // debug pcm dump
    ClosePCMDump();

    ALOGD("-%s()", __FUNCTION__);
    return NO_ERROR;
}


status_t AudioALSAPlaybackHandlerUsb::routing(const audio_devices_t output_devices __unused) {
    return NO_ERROR;
}

int AudioALSAPlaybackHandlerUsb::pause() {
    return -ENODATA;
}

int AudioALSAPlaybackHandlerUsb::resume() {
    return -ENODATA;
}

int AudioALSAPlaybackHandlerUsb::flush() {
    return 0;
}

status_t AudioALSAPlaybackHandlerUsb::setVolume(uint32_t vol __unused) {
    return INVALID_OPERATION;
}


int AudioALSAPlaybackHandlerUsb::drain(audio_drain_type_t type __unused) {
    return 0;
}


status_t AudioALSAPlaybackHandlerUsb::setScreenState(bool mode __unused, size_t buffer_size __unused, size_t reduceInterruptSize __unused, bool bforce __unused) {
    return NO_ERROR;
}

ssize_t AudioALSAPlaybackHandlerUsb::write(const void *buffer, size_t bytes) {
    //ALOGD("%s(), buffer = %p, bytes = %d", __FUNCTION__, buffer, bytes);

    // const -> to non const
    void *pBuffer = const_cast<void *>(buffer);
    ASSERT(pBuffer != NULL);

#ifdef DEBUG_LATENCY
    clock_gettime(CLOCK_REALTIME, &mNewtime);
    latencyTime[0] = calc_time_diff(mNewtime, mOldtime);
    mOldtime = mNewtime;
#endif

    WritePcmDumpData(buffer, bytes);
    // stereo to mono for speaker
    doStereoToMonoConversionIfNeed(pBuffer, bytes);

#ifdef MTK_AURISYS_FRAMEWORK_SUPPORT
    audio_ringbuf_copy_from_linear(&mAudioPoolBufDlIn->ringbuf, (char *)pBuffer, bytes);

    // post processing + SRC + Bit conversion
    aurisys_process_dl_only(mAurisysLibManager, mAudioPoolBufDlIn, mAudioPoolBufDlOut);

    // data pending: sram is device memory, need word size align 64 byte for 64 bit platform
    uint32_t data_size = audio_ringbuf_count(&mAudioPoolBufDlOut->ringbuf);
    if (data_size > mTransferredBufferSize) {
        data_size = mTransferredBufferSize;
    }
    data_size &= 0xFFFFFFC0;
    audio_ringbuf_copy_to_linear(mLinearOut, &mAudioPoolBufDlOut->ringbuf, data_size);
    //ALOGD("aurisys process data_size: %u", data_size);

    // wrap to original playback handler
    void *pBufferAfterPending = (void *)mLinearOut;
    uint32_t bytesAfterpending = data_size;
#endif


#ifdef DEBUG_LATENCY
    clock_gettime(CLOCK_REALTIME, &mNewtime);
    latencyTime[1] = calc_time_diff(mNewtime, mOldtime);
    mOldtime = mNewtime;
#endif

#ifdef MTK_AURISYS_FRAMEWORK_SUPPORT
    proxy_write(usbProxy, pBufferAfterPending, bytesAfterpending);
    updateStartTimeStamp();
    uint32_t echoRefDataSize = bytesAfterpending;
    const char *pEchoRefBuffer = (const char *)pBufferAfterPending;
    writeEchoRefDataToDataProvider(mDataProviderEchoRefUsb, pEchoRefBuffer, echoRefDataSize);
#else
    proxy_write(usbProxy, buffer, bytes);
#endif



#ifdef DEBUG_LATENCY
    clock_gettime(CLOCK_REALTIME, &mNewtime);
    latencyTime[2] = calc_time_diff(mNewtime, mOldtime);
    mOldtime = mNewtime;
#endif



#ifdef DEBUG_LATENCY
    if (latencyTime[0] > THRESHOLD_FRAMEWORK || latencyTime[1] > THRESHOLD_HAL || latencyTime[2] > (mStreamAttributeTarget.mInterrupt - latencyTime[0] - latencyTime[1] + THRESHOLD_KERNEL)) {
        ALOGD("latency_in_s,%1.3lf,%1.3lf,%1.3lf, interrupt,%1.3lf", latencyTime[0], latencyTime[1], latencyTime[2], mStreamAttributeTarget.mInterrupt);
    }
#endif

#ifdef MTK_AURISYS_FRAMEWORK_SUPPORT
    return bytesAfterpending;
#else
    return bytes;
#endif
}


status_t AudioALSAPlaybackHandlerUsb::setFilterMng(AudioMTKFilterManager *pFilterMng) {
    ALOGD("+%s() mAudioFilterManagerHandler [%p]", __FUNCTION__, pFilterMng);
    mAudioFilterManagerHandler = pFilterMng;
    ALOGD("-%s()", __FUNCTION__);
    return NO_ERROR;
}

template<class T>
status_t getParam(AppOps *appOps, ParamUnit *_paramUnit, T *_param, const char *_paramName) {
    Param *param;
    param = appOps->paramUnitGetParamByName(_paramUnit, _paramName);
    if (!param) {
        ALOGE("error: get param fail, param_name = %s", _paramName);
        return BAD_VALUE;
    } else {
        *_param = *(T *)param->data;
    }

    return NO_ERROR;
}

status_t AudioALSAPlaybackHandlerUsb::loadUSBDeviceParam() {
    ALOGD("%s()", __FUNCTION__);

    AppOps *appOps = appOpsGetInstance();
    if (appOps == NULL) {
        ALOGE("%s(), Error: AppOps == NULL", __FUNCTION__);
        ASSERT(false);
        return UNKNOWN_ERROR;
    }

    // define xml names
    char audioTypeName[] = USB_SPH_DEVICE_PARAM_AUDIOTYPE_NAME;

    // extract parameters from xml
    AudioType *audioType;
    audioType = appOps->appHandleGetAudioTypeByName(appOps->appHandleGetInstance(), audioTypeName);
    if (!audioType) {
        ALOGE("%s(), get audioType fail, audioTypeName = %s", __FUNCTION__, audioTypeName);
        return BAD_VALUE;
    }

    std::string categoryTypeName = "Device";
    CategoryType *categoryType = appOps->audioTypeGetCategoryTypeByName(audioType, categoryTypeName.c_str());
    if (!audioType) {
        ALOGE("%s(), get categoryType fail, categoryTypeName = %s", __FUNCTION__, categoryTypeName.c_str());
        return BAD_VALUE;
    }

    // Read lock
    appOps->audioTypeReadLock(audioType, __FUNCTION__);

    size_t categoryNum = appOps->categoryTypeGetNumOfCategory(categoryType);
    mParam.deviceParam.resize(categoryNum);

    mParam.maxCaptureLatencyUs = 0;
    for (size_t i = 0; i < categoryNum; i++) {
        Category *category = appOps->categoryTypeGetCategoryByIndex(categoryType, i);
        mParam.deviceParam[i].id = category->name;

        std::string paramPath = categoryTypeName + "," + category->name;

        ParamUnit *paramUnit;
        paramUnit = appOps->audioTypeGetParamUnit(audioType, paramPath.c_str());
        if (!paramUnit) {
            ALOGE("%s(), get paramUnit fail, paramPath = %s", __FUNCTION__, paramPath.c_str());
            return BAD_VALUE;
        }

        // spec
        getParam<int>(appOps, paramUnit, &mParam.deviceParam[i].playbackLatencyUs, "playback_latency_us");
        getParam<int>(appOps, paramUnit, &mParam.deviceParam[i].captureLatencyUs, "capture_latency_us");

        if (mParam.deviceParam[i].captureLatencyUs > mParam.maxCaptureLatencyUs) {
            mParam.maxCaptureLatencyUs = mParam.deviceParam[i].captureLatencyUs;
        }

        ALOGD("%s(), i %zu, device id %s, playbackLatencyUs %d, captureLatencyUs %d",
              __FUNCTION__, i, mParam.deviceParam[i].id.c_str(),
              mParam.deviceParam[i].playbackLatencyUs, mParam.deviceParam[i].captureLatencyUs);
    }

    ALOGV("%s(), mParam.maxCaptureLatencyUs %d", __FUNCTION__, mParam.maxCaptureLatencyUs);

    // Unlock
    appOps->audioTypeUnlock(audioType);

    return NO_ERROR;

}

status_t AudioALSAPlaybackHandlerUsb::getDeviceId(struct USBStream *stream) {
    if (!profile_is_initialized(&stream->profile)) {
        ALOGE("%s(), dir %d not initialized", __FUNCTION__, stream->direction);
        ASSERT(0);
        stream->deviceId.clear();
        return BAD_VALUE;
    }

    // get device id
#define DEVICE_ID_SIZE 32
    char deviceId[DEVICE_ID_SIZE] = "default";
    std::string usbidPath = "proc/asound/card";
    usbidPath += std::to_string(stream->profile.card);
    usbidPath += "/usbid";

    std::ifstream is(usbidPath.c_str(), std::ifstream::in);
    if (is) {
        is >> deviceId;
        is.close();
    } else {
        ALOGE("%s(), open path %s failed, use default", __FUNCTION__, usbidPath.c_str());
    }

    stream->deviceId = deviceId;

    return NO_ERROR;
}

uint32_t AudioALSAPlaybackHandlerUsb::getUSBDeviceLatency(size_t deviceParamIdx) {
    int delayMs = 0;
    delayMs = mParam.deviceParam[deviceParamIdx].playbackLatencyUs / 1000 ;
    ALOGD("%s(), deviceParamIdx %zu, playbackLatencyUs %d", __FUNCTION__, deviceParamIdx, delayMs);
    return delayMs;
}

status_t AudioALSAPlaybackHandlerUsb::getDeviceParam(struct USBStream *stream) {
    int dir = stream->direction;
    if (stream->deviceId.empty()) {
        ALOGE("%s(), dir %d, deviceId empty", __FUNCTION__, dir);
        ASSERT(0);
        return BAD_VALUE;
    }

    size_t defaultIdx = 9999;

    for (size_t i = 0; i < mParam.deviceParam.size(); i++) {
        if (mParam.deviceParam[i].id.compare(std::string(stream->deviceId, 0, mParam.deviceParam[i].id.size())) == 0) {
            ALOGD("%s(), dir %d, param found for deviceId %s", __FUNCTION__, dir, stream->deviceId.c_str());
            stream->deviceParamIdx = i;
            return NO_ERROR;
        }

        if (mParam.deviceParam[i].id.compare("default") == 0) {
            defaultIdx = i;
        }
    }

    if (defaultIdx >= mParam.deviceParam.size()) {
        ALOGE("%s(), dir %d, invalid defaultIdx %zu", __FUNCTION__, dir, defaultIdx);
        ASSERT(0);
        return BAD_VALUE;
    }

    ALOGD("%s(), dir %d, use default param for deviceId %s", __FUNCTION__, dir, stream->deviceId.c_str());
    stream->deviceParamIdx = defaultIdx;
    return NO_ERROR;
}

status_t AudioALSAPlaybackHandlerUsb::updateStartTimeStamp() {
    if (mDataProviderEchoRefUsb == NULL) {
        ALOGD("+%s mDataProviderEchoRefUsb == NULL, need to reset timestamp", __FUNCTION__);
        memset(&mEchoRefStartTime, 0, sizeof(mEchoRefStartTime));
        mDataProviderEchoRefUsb = AudioALSACaptureDataProviderEchoRefUsb::getInstance();
    }
    if (mEchoRefStartTime.tv_sec == 0 && mEchoRefStartTime.tv_nsec == 0) {
        time_info_struct_t HW_Buf_Time_Info;
        memset(&HW_Buf_Time_Info, 0, sizeof(HW_Buf_Time_Info));

        //status_t status = getHardwareBufferInfo(&HW_Buf_Time_Info);
        int ret = pcm_get_htimestamp(mPcm, &HW_Buf_Time_Info.frameInfo_get, &HW_Buf_Time_Info.timestamp_get);
        if (ret != 0) {
            ALOGD("-%s pcm_get_htimestamp fail, ret = %d, pcm_get_error = %s", __FUNCTION__, ret, pcm_get_error(mPcm));
            return UNKNOWN_ERROR;
        }
        mStreamAttributeTarget.Time_Info.timestamp_get = HW_Buf_Time_Info.timestamp_get;
        mEchoRefStartTime = mStreamAttributeTarget.Time_Info.timestamp_get;

        int delayMs = getUSBDeviceLatency(mUSBOutStream.deviceParamIdx);

        struct timespec origStartTime = mEchoRefStartTime;
        adjustTimeStamp(&mEchoRefStartTime, delayMs);

        ALOGD("%s(), Set start timestamp (%ld.%09ld->%ld.%09ld (%ld.%09ld)), mTotalEchoRefBufSize = %d, delayMs = %d (audio_mode = %d)",
              __FUNCTION__,
              origStartTime.tv_sec,
              origStartTime.tv_nsec,
              mEchoRefStartTime.tv_sec,
              mEchoRefStartTime.tv_nsec,
              mStreamAttributeTarget.Time_Info.timestamp_get.tv_sec,
              mStreamAttributeTarget.Time_Info.timestamp_get.tv_nsec,
              mTotalEchoRefBufSize,
              delayMs,
              mStreamAttributeSource->audio_mode);
    } else {
        ALOGV("%s(), start timestamp (%ld.%09ld), mTotalEchoRefBufSize = %d", __FUNCTION__, mEchoRefStartTime.tv_sec, mEchoRefStartTime.tv_nsec, mTotalEchoRefBufSize);
    }

    return NO_ERROR;
}

bool AudioALSAPlaybackHandlerUsb::writeEchoRefDataToDataProvider(AudioALSACaptureDataProviderEchoRefUsb *dataProvider, const char *echoRefData, uint32_t dataSize) {
    if ((mDataProviderEchoRefUsb != NULL) && (dataProvider->isEnable())) {
        /* Calculate buffer's time stamp */
        struct timespec newTimeStamp;
        calculateTimeStampByBytes(mEchoRefStartTime, mTotalEchoRefBufSize, mStreamAttributeTargetEchoRef, &newTimeStamp);
        SHOW_TIMESTAMP("%s(), mTotalEchoRefBufSize = %d, write size = %d, newTimeStamp = %ld.%09ld -> %ld.%09ld",
                       __FUNCTION__, mTotalEchoRefBufSize, dataSize, mEchoRefStartTime.tv_sec, mEchoRefStartTime.tv_nsec,
                       newTimeStamp.tv_sec, newTimeStamp.tv_nsec);

        // TODO(JH): Consider the close case, need to free EchoRef data from provider
        dataProvider->writeData(echoRefData, dataSize, &newTimeStamp);

        //WritePcmDumpData(echoRefData, dataSize);
        mTotalEchoRefBufSize += dataSize;
    } else {
        mTotalEchoRefBufSize += dataSize;
        SHOW_TIMESTAMP("%s(), data provider is not enabled, Do not write echo ref data to provider", __FUNCTION__);
    }

    return true;
}

} // end of namespace android
