#include "AudioALSACaptureHandlerSyncIO.h"
#include "AudioALSAHardwareResourceManager.h"
#include "AudioALSACaptureDataClientSyncIO.h"
#include "AudioALSACaptureDataProviderNormal.h"
#include "AudioVUnlockDL.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "AudioALSACaptureHandlerSyncIO"

namespace android {

//static FILE *pOutFile = NULL;

AudioALSACaptureHandlerSyncIO::AudioALSACaptureHandlerSyncIO(stream_attribute_t *stream_attribute_target) :
    AudioALSACaptureHandlerBase(stream_attribute_target) {
    ALOGD("%s()", __FUNCTION__);

    init();
}


AudioALSACaptureHandlerSyncIO::~AudioALSACaptureHandlerSyncIO() {
    ALOGD("+%s()", __FUNCTION__);

    ALOGD("%-s()", __FUNCTION__);
}


status_t AudioALSACaptureHandlerSyncIO::init() {
    ALOGD("%s()", __FUNCTION__);
    mCaptureHandlerType = CAPTURE_HANDLER_SYNCIO;
    return NO_ERROR;
}


status_t AudioALSACaptureHandlerSyncIO::open() {
    ALOGD("+%s(), input_device = 0x%x, input_source = 0x%x, sample_rate=%d, num_channels=%d",
          __FUNCTION__, mStreamAttributeTarget->input_device, mStreamAttributeTarget->input_source, mStreamAttributeTarget->sample_rate,
          mStreamAttributeTarget->num_channels);

    ASSERT(mCaptureDataClient == NULL);
    mCaptureDataClient = new AudioALSACaptureDataClientSyncIO(AudioALSACaptureDataProviderNormal::getInstance(), mStreamAttributeTarget);


    //============Voice UI&Unlock REFERECE=============
    AudioVUnlockDL *VUnlockhdl = AudioVUnlockDL::getInstance();
    if (VUnlockhdl != NULL) {
        struct timespec systemtime;
        memset(&systemtime, 0, sizeof(timespec));
        VUnlockhdl->SetUplinkStartTime(systemtime, 1);
    }
    //===========================================

    ALOGD("-%s()", __FUNCTION__);
    return NO_ERROR;
}


status_t AudioALSACaptureHandlerSyncIO::close() {
    ALOGD("+%s()", __FUNCTION__);

    ASSERT(mCaptureDataClient != NULL);
    delete mCaptureDataClient;

    //============Voice UI&Unlock REFERECE=============
    AudioVUnlockDL *VUnlockhdl = AudioVUnlockDL::getInstance();
    if (VUnlockhdl != NULL) {
        struct timespec systemtime;
        memset(&systemtime, 0, sizeof(timespec));
        VUnlockhdl->SetUplinkStartTime(systemtime, 1);
    }
    //===========================================

    ALOGD("-%s()", __FUNCTION__);
    return NO_ERROR;
}


status_t AudioALSACaptureHandlerSyncIO::routing(const audio_devices_t input_device) {
    mHardwareResourceManager->changeInputDevice(input_device);
    return NO_ERROR;
}


ssize_t AudioALSACaptureHandlerSyncIO::read(void *buffer, ssize_t bytes) {
    ALOGV("%s()", __FUNCTION__);

    mCaptureDataClient->read(buffer, bytes);
#if 0   //remove dump here which might cause process too long due to SD performance
    if (pOutFile != NULL) {
        fwrite(buffer, sizeof(char), bytes, pOutFile);
    }
#endif
    //============Voice UI&Unlock REFERECE=============
    AudioVUnlockDL *VUnlockhdl = AudioVUnlockDL::getInstance();
    if (VUnlockhdl != NULL) {
        struct timespec systemtime;
        memset(&systemtime, 0, sizeof(timespec));
        VUnlockhdl->SetUplinkStartTime(systemtime, 0);
    }
    //===========================================

    return bytes;
}

} // end of namespace android
