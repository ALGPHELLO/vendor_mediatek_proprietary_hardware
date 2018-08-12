/*
* Copyright (C) 2016 The Android Open Source Project
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#define LOG_TAG "DeviceHAL"
//#define LOG_NDEBUG 0

#include <algorithm>
#include <memory.h>
#include <string.h>

#include <android/log.h>

#include "Conversions.h"
#include "Device.h"
#include "HidlUtils.h"
#include "StreamIn.h"
#include "StreamOut.h"
#include "Util.h"

namespace android {
namespace hardware {
namespace audio {
namespace V2_0 {
namespace implementation {

using ::android::hardware::audio::common::V2_0::ThreadInfo;

namespace {
class VOWReadThread : public Thread {
public:
    // VOWReadThread's lifespan never exceeds Device's lifespan.
    VOWReadThread(std::atomic<bool> *stop,
                  audio_hw_device_mtk_t *device,
                  Device::CommandMQVOWRead *commandMQ,
                  Device::VOWDataMQ *dataMQ,
                  Device::StatusMQVOWRead *statusMQ,
                  EventFlag *efGroup)
        : Thread(false /*canCallJava*/),
          mStop(stop),
          mDevice(device),
          mCommandMQ(commandMQ),
          mDataMQ(dataMQ),
          mStatusMQ(statusMQ),
          mEfGroup(efGroup),
          mBuffer(nullptr) {
    }
    bool init() {
        mBuffer.reset(new (std::nothrow) uint8_t[mDataMQ->getQuantumCount()]);
        return mBuffer != nullptr;
    }
    virtual ~VOWReadThread() {}
    struct timespec mDLCurTime;

private:
    std::atomic<bool> *mStop;
    audio_hw_device_mtk_t *mDevice;
    Device::CommandMQVOWRead *mCommandMQ;
    Device::VOWDataMQ *mDataMQ;
    Device::StatusMQVOWRead *mStatusMQ;
    EventFlag *mEfGroup;
    std::unique_ptr<uint8_t[]> mBuffer;
    IMTKPrimaryDevice::VOWReadParameters mParameters;
    IMTKPrimaryDevice::VOWReadStatus mStatus;
    bool threadLoop() override;
    void doRead();
};

void VOWReadThread::doRead() {
    size_t availableToWrite = mDataMQ->availableToWrite();
    size_t requestedToRead = mParameters.params.read;
    if (requestedToRead > availableToWrite) {
        ALOGW("truncating read data from %zu to %zu due to insufficient data queue space",
              requestedToRead, availableToWrite);
        requestedToRead = availableToWrite;
    }
    mDLCurTime = {.tv_sec = 0, .tv_nsec = 0};
    ssize_t readResult = mDevice->read_ref_from_ring(mDevice, &mBuffer[0], requestedToRead, (void *)(&mDLCurTime));
    mStatus.retval = Result::OK;
    uint64_t read = 0;
    if (readResult >= 0) {
        mStatus.reply.read = readResult;
        if (!mDataMQ->write(&mBuffer[0], readResult)) {
            ALOGW("data message queue write failed");
        }
    } else {
        mStatus.retval = Device::analyzeStatus("xway_rec_read", readResult);
    }
}

bool VOWReadThread::threadLoop() {
    // This implementation doesn't return control back to the Thread until it decides to stop,
    // as the Thread uses mutexes, and this can lead to priority inversion.
    while (!std::atomic_load_explicit(mStop, std::memory_order_acquire)) {
        uint32_t efState = 0;
        mEfGroup->wait(static_cast<uint32_t>(MessageQueueFlagBits::NOT_FULL), &efState);
        if (!(efState & static_cast<uint32_t>(MessageQueueFlagBits::NOT_FULL)) || !mCommandMQ->read(&mParameters)) {
            continue;  // Nothing to do.
        }
        mStatus.replyTo = mParameters.command;
        switch (mParameters.command) {
        case IMTKPrimaryDevice::VOWReadCommand::VOW_READ:
            doRead();
            break;
        default:
            ALOGE("Unknown read thread command code %d", mParameters.command);
            mStatus.retval = Result::NOT_SUPPORTED;
            break;
        }
        if (!mStatusMQ->write(&mStatus)) {
            ALOGW("status message queue write failed");
        }
        mEfGroup->wake(static_cast<uint32_t>(MessageQueueFlagBits::NOT_EMPTY));
    }

    return false;
}
}



Device::Device(audio_hw_device_mtk_t *device)
    : mDevice(device), mEfGroupVOWRead(NULL) {
}

Device::~Device() {
    int status = audio_hw_device_close(mDevice);
    ALOGW_IF(status, "Error closing audio hw device %p: %s", mDevice,
             strerror(-status));
    mDevice = nullptr;
}

// static
Result Device::analyzeStatus(const char *funcName, int status) {
    if (status != 0) {
        ALOGW("Device %s: %s", funcName, strerror(-status));
    }
    switch (status) {
    case 0:
        return Result::OK;
    case -EINVAL:
        return Result::INVALID_ARGUMENTS;
    case -ENODATA:
        return Result::INVALID_STATE;
    case -ENODEV:
        return Result::NOT_INITIALIZED;
    case -ENOSYS:
        return Result::NOT_SUPPORTED;
    default:
        return Result::INVALID_STATE;
    }
}

void Device::closeInputStream(audio_stream_in_t *stream) {
    mDevice->close_input_stream(mDevice, stream);
}

void Device::closeOutputStream(audio_stream_out_t *stream) {
    mDevice->close_output_stream(mDevice, stream);
}

char *Device::halGetParameters(const char *keys) {
    return mDevice->get_parameters(mDevice, keys);
}

int Device::halSetParameters(const char *keysAndValues) {
    return mDevice->set_parameters(mDevice, keysAndValues);
}

// Methods from ::android::hardware::audio::V2_0::IDevice follow.
Return<Result> Device::initCheck() {
    return analyzeStatus("init_check", mDevice->init_check(mDevice));
}

Return<Result> Device::setMasterVolume(float volume) {
    if (mDevice->set_master_volume == NULL) {
        return Result::NOT_SUPPORTED;
    }
    if (!isGainNormalized(volume)) {
        ALOGW("Can not set a master volume (%f) outside [0,1]", volume);
        return Result::INVALID_ARGUMENTS;
    }
    return analyzeStatus("set_master_volume",
                         mDevice->set_master_volume(mDevice, volume));
}

Return<void> Device::getMasterVolume(getMasterVolume_cb _hidl_cb) {
    Result retval(Result::NOT_SUPPORTED);
    float volume = 0;
    if (mDevice->get_master_volume != NULL) {
        retval = analyzeStatus("get_master_volume",
                               mDevice->get_master_volume(mDevice, &volume));
    }
    _hidl_cb(retval, volume);
    return Void();
}

Return<Result> Device::setMicMute(bool mute) {
    return analyzeStatus("set_mic_mute", mDevice->set_mic_mute(mDevice, mute));
}

Return<void> Device::getMicMute(getMicMute_cb _hidl_cb) {
    bool mute = false;
    Result retval =
        analyzeStatus("get_mic_mute", mDevice->get_mic_mute(mDevice, &mute));
    _hidl_cb(retval, mute);
    return Void();
}

Return<Result> Device::setMasterMute(bool mute) {
    Result retval(Result::NOT_SUPPORTED);
    if (mDevice->set_master_mute != NULL) {
        retval = analyzeStatus("set_master_mute",
                               mDevice->set_master_mute(mDevice, mute));
    }
    return retval;
}

Return<void> Device::getMasterMute(getMasterMute_cb _hidl_cb) {
    Result retval(Result::NOT_SUPPORTED);
    bool mute = false;
    if (mDevice->get_master_mute != NULL) {
        retval = analyzeStatus("get_master_mute",
                               mDevice->get_master_mute(mDevice, &mute));
    }
    _hidl_cb(retval, mute);
    return Void();
}

Return<void> Device::getInputBufferSize(const AudioConfig &config,
                                        getInputBufferSize_cb _hidl_cb) {
    audio_config_t halConfig;
    HidlUtils::audioConfigToHal(config, &halConfig);
    size_t halBufferSize = mDevice->get_input_buffer_size(mDevice, &halConfig);
    Result retval(Result::INVALID_ARGUMENTS);
    uint64_t bufferSize = 0;
    if (halBufferSize != 0) {
        retval = Result::OK;
        bufferSize = halBufferSize;
    }
    _hidl_cb(retval, bufferSize);
    return Void();
}

Return<void> Device::openOutputStream(int32_t ioHandle,
                                      const DeviceAddress &device,
                                      const AudioConfig &config,
                                      AudioOutputFlag flags,
                                      openOutputStream_cb _hidl_cb) {
    audio_config_t halConfig;
    HidlUtils::audioConfigToHal(config, &halConfig);
    audio_stream_out_t *halStream;
    ALOGV(
        "open_output_stream handle: %d devices: %x flags: %#x "
        "srate: %d format %#x channels %x address %s",
        ioHandle, static_cast<audio_devices_t>(device.device),
        static_cast<audio_output_flags_t>(flags), halConfig.sample_rate,
        halConfig.format, halConfig.channel_mask,
        deviceAddressToHal(device).c_str());
    int status = mDevice->open_output_stream(
                     mDevice, ioHandle, static_cast<audio_devices_t>(device.device),
                     static_cast<audio_output_flags_t>(flags), &halConfig, &halStream,
                     deviceAddressToHal(device).c_str());
    ALOGV("open_output_stream status %d stream %p", status, halStream);
    sp<IStreamOut> streamOut;
    if (status == OK) {
        streamOut = new StreamOut(this, halStream);
    }
    AudioConfig suggestedConfig;
    HidlUtils::audioConfigFromHal(halConfig, &suggestedConfig);
    _hidl_cb(analyzeStatus("open_output_stream", status), streamOut,
             suggestedConfig);
    return Void();
}

Return<void> Device::openInputStream(int32_t ioHandle,
                                     const DeviceAddress &device,
                                     const AudioConfig &config,
                                     AudioInputFlag flags, AudioSource source,
                                     openInputStream_cb _hidl_cb) {
    audio_config_t halConfig;
    HidlUtils::audioConfigToHal(config, &halConfig);
    audio_stream_in_t *halStream;
    ALOGV(
        "open_input_stream handle: %d devices: %x flags: %#x "
        "srate: %d format %#x channels %x address %s source %d",
        ioHandle, static_cast<audio_devices_t>(device.device),
        static_cast<audio_input_flags_t>(flags), halConfig.sample_rate,
        halConfig.format, halConfig.channel_mask,
        deviceAddressToHal(device).c_str(),
        static_cast<audio_source_t>(source));
    int status = mDevice->open_input_stream(
                     mDevice, ioHandle, static_cast<audio_devices_t>(device.device),
                     &halConfig, &halStream, static_cast<audio_input_flags_t>(flags),
                     deviceAddressToHal(device).c_str(),
                     static_cast<audio_source_t>(source));
    ALOGV("open_input_stream status %d stream %p", status, halStream);
    sp<IStreamIn> streamIn;
    if (status == OK) {
        streamIn = new StreamIn(this, halStream);
    }
    AudioConfig suggestedConfig;
    HidlUtils::audioConfigFromHal(halConfig, &suggestedConfig);
    _hidl_cb(analyzeStatus("open_input_stream", status), streamIn,
             suggestedConfig);
    return Void();
}

Return<bool> Device::supportsAudioPatches() {
    return version() >= AUDIO_DEVICE_API_VERSION_3_0;
}

Return<void> Device::createAudioPatch(const hidl_vec<AudioPortConfig> &sources,
                                      const hidl_vec<AudioPortConfig> &sinks,
                                      createAudioPatch_cb _hidl_cb) {
    Result retval(Result::NOT_SUPPORTED);
    AudioPatchHandle patch = 0;
    if (version() >= AUDIO_DEVICE_API_VERSION_3_0) {
        std::unique_ptr<audio_port_config[]> halSources(
            HidlUtils::audioPortConfigsToHal(sources));
        std::unique_ptr<audio_port_config[]> halSinks(
            HidlUtils::audioPortConfigsToHal(sinks));
        audio_patch_handle_t halPatch = AUDIO_PATCH_HANDLE_NONE;
        retval = analyzeStatus(
                     "create_audio_patch",
                     mDevice->create_audio_patch(mDevice, sources.size(), &halSources[0],
                                                 sinks.size(), &halSinks[0], &halPatch));
        if (retval == Result::OK) {
            patch = static_cast<AudioPatchHandle>(halPatch);
        }
    }
    _hidl_cb(retval, patch);
    return Void();
}

Return<Result> Device::releaseAudioPatch(int32_t patch) {
    if (version() >= AUDIO_DEVICE_API_VERSION_3_0) {
        return analyzeStatus(
                   "release_audio_patch",
                   mDevice->release_audio_patch(
                       mDevice, static_cast<audio_patch_handle_t>(patch)));
    }
    return Result::NOT_SUPPORTED;
}

Return<void> Device::getAudioPort(const AudioPort &port,
                                  getAudioPort_cb _hidl_cb) {
    audio_port halPort;
    HidlUtils::audioPortToHal(port, &halPort);
    Result retval = analyzeStatus("get_audio_port",
                                  mDevice->get_audio_port(mDevice, &halPort));
    AudioPort resultPort = port;
    if (retval == Result::OK) {
        HidlUtils::audioPortFromHal(halPort, &resultPort);
    }
    _hidl_cb(retval, resultPort);
    return Void();
}

Return<Result> Device::setAudioPortConfig(const AudioPortConfig &config) {
    if (version() >= AUDIO_DEVICE_API_VERSION_3_0) {
        struct audio_port_config halPortConfig;
        HidlUtils::audioPortConfigToHal(config, &halPortConfig);
        return analyzeStatus(
                   "set_audio_port_config",
                   mDevice->set_audio_port_config(mDevice, &halPortConfig));
    }
    return Result::NOT_SUPPORTED;
}

Return<AudioHwSync> Device::getHwAvSync() {
    int halHwAvSync;
    Result retval = getParam(AudioParameter::keyHwAvSync, &halHwAvSync);
    return retval == Result::OK ? halHwAvSync : AUDIO_HW_SYNC_INVALID;
}

Return<Result> Device::setScreenState(bool turnedOn) {
    return setParam(AudioParameter::keyScreenState, turnedOn);
}

Return<void> Device::getParameters(const hidl_vec<hidl_string> &keys,
                                   getParameters_cb _hidl_cb) {
    getParametersImpl(keys, _hidl_cb);
    return Void();
}

Return<Result> Device::setParameters(
    const hidl_vec<ParameterValue> &parameters) {
    return setParametersImpl(parameters);
}

Return<void> Device::debugDump(const hidl_handle &fd) {
    if (fd.getNativeHandle() != nullptr && fd->numFds == 1) {
        analyzeStatus("dump", mDevice->dump(mDevice, fd->data[0]));
    }
    return Void();
}

Return<Result> Device::setupParametersCallback(const sp<IMTKPrimaryDeviceCallback> &callback) {
    if (mDevice->setup_parameters_callback == NULL) { return Result::NOT_SUPPORTED; }
    int result = mDevice->setup_parameters_callback(mDevice, Device::syncCallback, this);
    if (result == 0) {
        mCallback = callback;
    }
    return Stream::analyzeStatus("set_parameters_callback", result);
}

int Device::syncCallback(device_parameters_callback_event_t event, audio_hw_device_set_parameters_callback_t *param, void *cookie) {
    wp<Device> weakSelf(reinterpret_cast<Device *>(cookie));
    sp<Device> self = weakSelf.promote();
    if (self == nullptr || self->mCallback == nullptr) { return 0; }
    ALOGV("syncCallback() event %d", event);
    switch (event) {
    case DEVICE_CBK_EVENT_SETPARAMETERS: {
        IMTKPrimaryDeviceCallback::SetParameters *pData = new IMTKPrimaryDeviceCallback::SetParameters();
        pData->paramchar_len = param->paramchar_len;
        uint8_t *dst = reinterpret_cast<uint8_t *>(&pData->paramchar[0]);
        const uint8_t *src = reinterpret_cast<uint8_t *>(&param->paramchar[0]);
        memcpy(dst, src, param->paramchar_len);
        self->mCallback->setParametersCallback(*pData);
        delete pData;
        break;
    }
    default:
        ALOGW("syncCallback() unknown event %d", event);
        break;
    }
    return 0;
}

int Device::audioParameterChangedCallback(const char *param, void *cookie) {
    ALOGV("Device::%s(), audioType = %s, cookie = %p", __FUNCTION__, param, cookie);
    wp<Device> weakSelf(reinterpret_cast<Device*>(cookie));
    sp<Device> self = weakSelf.promote();
    hidl_string audioTypeName = param;
    self->mAudioParameterChangedCallback->audioParameterChangedCallback(audioTypeName);
    return 0;
}

Return<Result> Device::setAudioParameterChangedCallback(const sp<IAudioParameterChangedCallback>& callback) {
    ALOGV("Device::%s()", __FUNCTION__);
    if (mDevice->set_audio_parameter_changed_callback == NULL) return Result::NOT_SUPPORTED;
    int result = mDevice->set_audio_parameter_changed_callback(mDevice, Device::audioParameterChangedCallback, this);
    if (result != 0) {
        ALOGE("%s(), result != 0 (%d)", __FUNCTION__, result);
    }
    mAudioParameterChangedCallback = callback;
    return Stream::analyzeStatus("set_audio_parameter_changed_callback", result);
}

Return<Result> Device::clearAudioParameterChangedCallback() {
    ALOGV("Device::%s()", __FUNCTION__);
    if (mDevice->clear_audio_parameter_changed_callback == NULL) return Result::NOT_SUPPORTED;
    int result = mDevice->clear_audio_parameter_changed_callback(mDevice, this);
    if (result != 0) {
        ALOGE("%s(), result != 0 (%d)", __FUNCTION__, result);
    }
    return Stream::analyzeStatus("clear_audio_parameter_changed_callback", result);
}

Return<void> Device::prepareForVOWReading(uint32_t frameSize, uint32_t framesCount,
                                          IMTKPrimaryDevice::prepareForVOWReading_cb _hidl_cb)  {
    status_t status;
    ThreadInfo threadInfo = { 0, 0 };
    // Create message queues.
    if (mDataMQVOWRead) {
        ALOGE("the client attempts to call prepareForReading twice");
        _hidl_cb(Result::INVALID_STATE,
                 CommandMQVOWRead::Descriptor(), VOWDataMQ::Descriptor(), StatusMQVOWRead::Descriptor(), threadInfo, 0, 0);
        return Void();
    }
    std::unique_ptr<CommandMQVOWRead> tempCommandMQ(new CommandMQVOWRead(1));
    if (frameSize > std::numeric_limits<size_t>::max() / framesCount) {
        ALOGE("Requested buffer is too big, %d*%d can not fit in size_t", frameSize, framesCount);
        _hidl_cb(Result::INVALID_ARGUMENTS,
                 CommandMQVOWRead::Descriptor(), VOWDataMQ::Descriptor(), StatusMQVOWRead::Descriptor(), threadInfo, 0, 0);
        return Void();
    }
    std::unique_ptr<DataMQ> tempDataMQ(new DataMQ(frameSize * framesCount, true));//EventFlag

    std::unique_ptr<StatusMQVOWRead> tempStatusMQ(new StatusMQVOWRead(1));
    if (!tempCommandMQ->isValid() || !tempDataMQ->isValid() || !tempStatusMQ->isValid()) {
        ALOGE_IF(!tempCommandMQ->isValid(), "command MQ is invalid");
        ALOGE_IF(!tempDataMQ->isValid(), "data MQ is invalid");
        ALOGE_IF(!tempStatusMQ->isValid(), "status MQ is invalid");
        _hidl_cb(Result::INVALID_ARGUMENTS,
                 CommandMQVOWRead::Descriptor(), VOWDataMQ::Descriptor(), StatusMQVOWRead::Descriptor(), threadInfo, 0, 0);
        return Void();
    }
    EventFlag *tempRawEfGroup{};
    status = EventFlag::createEventFlag(tempDataMQ->getEventFlagWord(), &tempRawEfGroup);
    std::unique_ptr<EventFlag, void(*)(EventFlag *)> tempElfGroup(tempRawEfGroup, [](auto * ef) {
        EventFlag::deleteEventFlag(&ef);
    });
    if (status != OK || !tempElfGroup) {
        ALOGE("failed creating event flag for data MQ: %s", strerror(-status));
        _hidl_cb(Result::INVALID_ARGUMENTS,
                 CommandMQVOWRead::Descriptor(), VOWDataMQ::Descriptor(), StatusMQVOWRead::Descriptor(), threadInfo, 0, 0);
        return Void();
    }

    // Create and launch the thread.
    auto tempReadThread = std::make_unique<VOWReadThread>(&mStopReadThread,
                                                          mDevice,
                                                          tempCommandMQ.get(),
                                                          tempDataMQ.get(),
                                                          tempStatusMQ.get(),
                                                          tempElfGroup.get());
    if (!tempReadThread->init()) {
        _hidl_cb(Result::INVALID_ARGUMENTS,
                 CommandMQVOWRead::Descriptor(), VOWDataMQ::Descriptor(), StatusMQVOWRead::Descriptor(), threadInfo, 0, 0);
        return Void();
    }
    status = tempReadThread->run("reader", PRIORITY_URGENT_AUDIO);
    if (status != OK) {
        ALOGW("failed to start reader thread: %s", strerror(-status));
        _hidl_cb(Result::INVALID_ARGUMENTS,
                 CommandMQVOWRead::Descriptor(), DataMQ::Descriptor(), StatusMQVOWRead::Descriptor(), threadInfo, 0, 0);
        return Void();
    }

    mCommandMQVOWRead = std::move(tempCommandMQ);
    mDataMQVOWRead = std::move(tempDataMQ);
    mStatusMQVOWRead = std::move(tempStatusMQ);
    mReadThread = tempReadThread.release();
    mEfGroupVOWRead = tempElfGroup.release();
    threadInfo.pid = getpid();
    threadInfo.tid = mReadThread->getTid();
    _hidl_cb(Result::OK,
             *mCommandMQVOWRead->getDesc(), *mDataMQVOWRead->getDesc(), *mStatusMQVOWRead->getDesc(),
             threadInfo, tempReadThread->mDLCurTime.tv_sec, tempReadThread->mDLCurTime.tv_nsec);
    return Void();
}

Return<void> Device::getVoiceUnlockULTime(IMTKPrimaryDevice::getVoiceUnlockULTime_cb _hidl_cb)  {
    Result retval(Result::NOT_SUPPORTED);
    struct timespec mULCurTime = {.tv_sec = 0, .tv_nsec = 0};
    if (mDevice->get_vow_ul_time != NULL) {
        retval = analyzeStatus("get_vow_ul_time", mDevice->get_vow_ul_time(mDevice, (void *)(&mULCurTime)));
    }
    _hidl_cb(retval, mULCurTime.tv_sec, mULCurTime.tv_nsec);
    return Void();
}

Return<Result> Device::setVoiceUnlockSRC(uint32_t outSR, uint32_t outChannel) {
    Result retval(Result::NOT_SUPPORTED);
    if (mDevice->set_vow_src_sample_rate != NULL) {
        retval = analyzeStatus("set_vow_src_sample_rate", mDevice->set_vow_src_sample_rate(mDevice, outSR, outChannel));
    }
    return retval;
}
Return<Result> Device::startVoiceUnlockDL() {
    Result retval(Result::NOT_SUPPORTED);
    if (mDevice->start_vow_dl != NULL) {
        retval = analyzeStatus("start_vow_dl", mDevice->start_vow_dl(mDevice));
    }
    return retval;
}

Return<Result> Device::stopVoiceUnlockDL() {
    Result retval(Result::NOT_SUPPORTED);
    if (mDevice->stop_vow_dl != NULL) {
        retval = analyzeStatus("stop_vow_dl", mDevice->stop_vow_dl(mDevice));
    }
    return retval;
}

Return<Result> Device::getVoiceUnlockDLInstance() {
    Result retval(Result::NOT_SUPPORTED);
    if (mDevice->get_vow_dl_instance != NULL) {
        if (mDevice->get_vow_dl_instance(mDevice) == true) {
            retval = Result::OK;
        } else {
            retval = Result::INVALID_STATE;
        }
    }
    return retval;
}

}  // namespace implementation
}  // namespace V2_0
}  // namespace audio
}  // namespace hardware
}  // namespace android
