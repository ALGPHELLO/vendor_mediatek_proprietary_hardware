#include "AudioALSAStreamOut.h"

#include <cutils/atomic.h>
#include <inttypes.h>
#include <math.h>

#include "AudioALSAPlaybackHandlerBase.h"
#include "AudioMixerOut.h"
#include "AudioUtility.h"

#include "AudioALSASampleRateController.h"
#include "AudioALSAFMController.h"
#include "AudioALSAHardwareResourceManager.h"

#include <hardware/audio_mtk.h>
#ifdef MTK_MAXIM_SPEAKER_SUPPORT
#include "AudioALSAPlaybackHandlerSpeakerProtection.h"
#endif

#define BUFFER_FRAME_COUNT_PER_ACCESS (1024)

// ALPS03595920 : CTS limits hal buffer size
#ifndef MAX_BUFFER_FRAME_COUNT_PER_ACCESS
#define MAX_BUFFER_FRAME_COUNT_PER_ACCESS (2048)
#endif


#define BUFFER_FRAME_COUNT_PER_ACCESSS_HDMI (1024)

#ifndef FRAME_COUNT_MIN_PER_ACCESSS
#define FRAME_COUNT_MIN_PER_ACCESSS (256)
#endif
//#define DOWNLINK_LOW_LATENCY_CPU_SPEED  (1300000)
//#define DOWNLINK_NORMAL_CPU_SPEED       ( 715000)

#ifndef KERNEL_BUFFER_SIZE_DL1_DATA2_NORMAL
#define KERNEL_BUFFER_SIZE_DL1_DATA2_NORMAL         KERNEL_BUFFER_SIZE_DL1_NORMAL
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG  "AudioALSAStreamOut"

#ifdef MTK_AUDIO_TUNNELING_SUPPORT
#include "AudioALSAPlaybackHandlerOffload.h"
int offloadflag = 1;
#else
int offloadflag = 0;
#endif

#define calc_time_diff(x,y) ((x.tv_sec - y.tv_sec )+ (double)( x.tv_nsec - y.tv_nsec ) / (double)1000000000)

namespace android {

uint32_t AudioALSAStreamOut::mDumpFileNum = 0;

// TODO(Harvey): Query this
static const audio_format_t       kDefaultOutputSourceFormat      = AUDIO_FORMAT_PCM_16_BIT;
static const audio_channel_mask_t kDefaultOutputSourceChannelMask = AUDIO_CHANNEL_OUT_STEREO;
static const uint32_t             kDefaultOutputSourceSampleRate  = 44100;

uint32_t AudioALSAStreamOut::mSuspendStreamOutHDMIStereoCount = 0;

AudioALSAStreamOut *AudioALSAStreamOut::mStreamOutHDMIStereo = NULL;
uint32_t mStreamOutHDMIStereoCount = 0;


AudioALSAStreamOut::AudioALSAStreamOut() :
    mStreamManager(AudioALSAStreamManager::getInstance()),
    mPlaybackHandler(NULL),
    mPCMDumpFile(NULL),
    mLockCount(0),
    mIdentity(0xFFFFFFFF),
    mSuspendCount(0),
    mMuteForRouting(0),
    mStandby(true),
    mStreamOutType(STREAM_OUT_PRIMARY),
    mPresentedBytes(0),
    mPresentFrames(0),
    mLowLatencyMode(true),
    mOffload(false),
    mPaused(false),
    mStreamCbk(NULL),
    mCbkCookie(NULL),
    mOffloadVol(0x10000),
    mMixerOut(AudioMixerOut::getInstance()),
    mMixerOutInUse(false) {
    ALOGD("%s()", __FUNCTION__);

#ifdef CONFIG_MT_ENG_BUILD
    mLogEnable = 1;
#else
    mLogEnable = __android_log_is_loggable(ANDROID_LOG_DEBUG, LOG_TAG, ANDROID_LOG_INFO);
#endif
    memset(&mStreamAttributeSource, 0, sizeof(mStreamAttributeSource));
    memset(&mPresentedTime, 0, sizeof(timespec));

    memset(&mMuteTime, 0, sizeof(struct timespec));
    memset(&mMuteCurTime, 0, sizeof(struct timespec));
}


AudioALSAStreamOut::~AudioALSAStreamOut() {
    ALOGD("%s() %d", __FUNCTION__, mStreamOutHDMIStereoCount);

    if (!mStandby) {
        ALOGW("%s(), not standby, mStandby %d, mPlaybackHandler %p",
              __FUNCTION__, mStandby, mPlaybackHandler);
        standbyStreamOut();
    }

    if (mStreamOutHDMIStereo == this) {
        mStreamOutHDMIStereoCount--;
    }

    if ((mStreamOutHDMIStereo == this) && (mStreamOutHDMIStereoCount <= 0)) {
        mStreamOutHDMIStereo = NULL;
        ALOGD("%s() mStreamOutHDMIStereo = NULL %d", __FUNCTION__, mStreamOutHDMIStereoCount);
    }
}


status_t AudioALSAStreamOut::set(
    uint32_t devices,
    int *format,
    uint32_t *channels,
    uint32_t *sampleRate,
    status_t *status,
    uint32_t flags) {
    ALOGD("%s(), devices = 0x%x, format = 0x%x, channels = 0x%x, sampleRate = %d, flags = 0x%x",
          __FUNCTION__, devices, *format, *channels, *sampleRate, flags);

    AL_AUTOLOCK(mLock);

    *status = NO_ERROR;

    // device
    mStreamAttributeSource.output_devices = static_cast<audio_devices_t>(devices);
    mStreamAttributeSource.policyDevice = mStreamAttributeSource.output_devices;

    // check format
    if (*format == AUDIO_FORMAT_PCM_16_BIT ||
        *format == AUDIO_FORMAT_PCM_8_24_BIT ||
        *format == AUDIO_FORMAT_PCM_32_BIT) {
        mStreamAttributeSource.audio_format = static_cast<audio_format_t>(*format);
    } else if (*format == AUDIO_FORMAT_MP3) {
        ALOGD("%s(), format mp3", __FUNCTION__);
        mStreamAttributeSource.audio_format = static_cast<audio_format_t>(*format);
    } else {
        ALOGE("%s(), wrong format 0x%x, use 0x%x instead.", __FUNCTION__, *format, kDefaultOutputSourceFormat);

        *format = kDefaultOutputSourceFormat;
        *status = BAD_VALUE;
    }

    // check channel mask
    if (mStreamAttributeSource.output_devices == AUDIO_DEVICE_OUT_AUX_DIGITAL) { // HDMI
        if (*channels == AUDIO_CHANNEL_OUT_STEREO) {
            mStreamOutType = STREAM_OUT_HDMI_STEREO;

            mStreamAttributeSource.audio_channel_mask = *channels;
            mStreamAttributeSource.num_channels = popcount(*channels);

            mStreamOutHDMIStereo = this;
            mStreamOutHDMIStereoCount++;
            ALOGD("%s(), mStreamOutHDMIStereoCount =%d", __FUNCTION__, mStreamOutHDMIStereoCount);
        } else if (*channels == AUDIO_CHANNEL_OUT_5POINT1 ||
                   *channels == AUDIO_CHANNEL_OUT_7POINT1) {
            mStreamOutType = STREAM_OUT_HDMI_MULTI_CHANNEL;

            mStreamAttributeSource.audio_channel_mask = *channels;
            mStreamAttributeSource.num_channels = popcount(*channels);
        } else {
            ALOGE("%s(), wrong channels 0x%x, use 0x%x instead.", __FUNCTION__, *channels, kDefaultOutputSourceChannelMask);

            *channels = kDefaultOutputSourceChannelMask;
            *status = BAD_VALUE;
        }
    } else if (devices == AUDIO_DEVICE_OUT_SPEAKER_SAFE) { // Primary
        mStreamOutType = STREAM_OUT_VOICE_DL;
        mStreamAttributeSource.audio_channel_mask = *channels;
        mStreamAttributeSource.num_channels = popcount(*channels);
    } else if (*channels == kDefaultOutputSourceChannelMask || *channels == AUDIO_CHANNEL_OUT_MONO) { // Primary
        mStreamAttributeSource.audio_channel_mask = *channels;
        mStreamAttributeSource.num_channels = popcount(*channels);
    } else {
        ALOGE("%s(), wrong channels 0x%x, use 0x%x instead.", __FUNCTION__, *channels, kDefaultOutputSourceChannelMask);

        *channels = kDefaultOutputSourceChannelMask;
        *status = BAD_VALUE;
    }

    // check sample rate
    if (SampleRateSupport(*sampleRate) == true) {
        if ((mStreamAttributeSource.num_channels == 2) && (mStreamAttributeSource.output_devices == AUDIO_DEVICE_OUT_AUX_DIGITAL)) {
            mStreamAttributeSource.sample_rate = 44100;
        } else {
            if (flags & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD) {
                if (*sampleRate <= 44100) {
                    *sampleRate = 44100;
                } else {
                    *sampleRate = 48000;
                }
            }
            mStreamAttributeSource.sample_rate = *sampleRate;
        }

        if (mStreamOutType == STREAM_OUT_PRIMARY || mStreamOutType == STREAM_OUT_VOICE_DL) {
            AudioALSASampleRateController::getInstance()->setPrimaryStreamOutSampleRate(*sampleRate);
        }
    } else {
        ALOGE("%s(), wrong sampleRate %d, use %d instead.", __FUNCTION__, *sampleRate, kDefaultOutputSourceSampleRate);

        *sampleRate = kDefaultOutputSourceSampleRate;
        *status = BAD_VALUE;
    }



    mStreamAttributeSource.mAudioOutputFlags = (audio_output_flags_t)flags;
    collectPlatformOutputFlags(mStreamAttributeSource.mAudioOutputFlags);

#ifdef MTK_HYBRID_NLE_SUPPORT
    if (isIsolatedDeepBuffer(mStreamAttributeSource.mAudioOutputFlags)) {
        AudioALSAHyBridNLEManager::setSupportRunNLEHandler(PLAYBACK_HANDLER_DEEP_BUFFER);
    }
#endif

    // debug for PowerHAL
    char value[PROPERTY_VALUE_MAX];
    if (mStreamAttributeSource.mAudioOutputFlags & AUDIO_OUTPUT_FLAG_FAST) {
        (void) property_get("audio.powerhal.latency.dl", value, "1");
    } else {
        (void) property_get("audio.powerhal.power.dl", value, "1");
    }
    int debuggable = atoi(value);
    mStreamAttributeSource.mPowerHalEnable = debuggable ? true : false;


    // audio low latency param - playback - hal buffer size
    setBufferSize();

    ALOGD("%s(), mStreamAttributeSource.latency %d, mStreamAttributeSource.buffer_size =%d, channels %d.", __FUNCTION__, mStreamAttributeSource.latency, mStreamAttributeSource.buffer_size, mStreamAttributeSource.num_channels);

    return *status;
}


uint32_t AudioALSAStreamOut::sampleRate() const {
    ALOGV("%s(), return %d", __FUNCTION__, mStreamAttributeSource.sample_rate);
    return mStreamAttributeSource.sample_rate;
}


size_t AudioALSAStreamOut::bufferSize() const {
    ALOGV("%s(), return 0x%x, flag %d", __FUNCTION__, mStreamAttributeSource.buffer_size, mStreamAttributeSource.mAudioOutputFlags);
    return mStreamAttributeSource.buffer_size;
}

uint32_t AudioALSAStreamOut::bufferSizeTimeUs() const {
    uint32_t bufferTime = 0;

    const uint8_t size_per_channel = (mStreamAttributeSource.audio_format == AUDIO_FORMAT_PCM_8_BIT ? 1 :
                                      (mStreamAttributeSource.audio_format == AUDIO_FORMAT_PCM_16_BIT ? 2 :
                                       (mStreamAttributeSource.audio_format == AUDIO_FORMAT_PCM_32_BIT ? 4 :
                                        (mStreamAttributeSource.audio_format == AUDIO_FORMAT_PCM_8_24_BIT ? 4 :
                                         2))));
    const uint8_t size_per_frame = mStreamAttributeSource.num_channels * size_per_channel;
    bufferTime = (mStreamAttributeSource.buffer_size * 1000 * 1000) / (mStreamAttributeSource.sample_rate * size_per_frame);

    ALOGV("%s(), return %d", __FUNCTION__, bufferTime);
    return bufferTime;
}

uint32_t AudioALSAStreamOut::channels() const {
    ALOGV("%s(), return 0x%x", __FUNCTION__, mStreamAttributeSource.audio_channel_mask);
    return mStreamAttributeSource.audio_channel_mask;
}


int AudioALSAStreamOut::format() const {
    ALOGV("%s(), return 0x%x", __FUNCTION__, mStreamAttributeSource.audio_format);
    return mStreamAttributeSource.audio_format;
}


uint32_t AudioALSAStreamOut::latency() const {
    uint32_t latency = 0;

    if (mPlaybackHandler == NULL || mStandby) {
        latency = mStreamAttributeSource.latency;
    } else {
        const stream_attribute_t *pStreamAttributeTarget = mPlaybackHandler->getStreamAttributeTarget();
        const uint8_t size_per_channel = (pStreamAttributeTarget->audio_format == AUDIO_FORMAT_PCM_8_BIT ? 1 :
                                          (pStreamAttributeTarget->audio_format == AUDIO_FORMAT_PCM_16_BIT ? 2 :
                                           (pStreamAttributeTarget->audio_format == AUDIO_FORMAT_PCM_32_BIT ? 4 :
                                            (pStreamAttributeTarget->audio_format == AUDIO_FORMAT_PCM_8_24_BIT ? 4 :
                                             2))));
        const uint8_t size_per_frame = pStreamAttributeTarget->num_channels * size_per_channel;

        latency = (pStreamAttributeTarget->buffer_size * 1000) / (pStreamAttributeTarget->sample_rate * size_per_frame);
    }

    ALOGV("%s(), flags %d, return %d", __FUNCTION__, mStreamAttributeSource.mAudioOutputFlags, latency);
    return latency;
}


status_t AudioALSAStreamOut::setVolume(float left, float right) {
    if (left < 0.0f || left > 1.0f || isnan(left) ||
        right < 0.0f || right > 1.0f || isnan(right)) {
        ALOGE("%s(), invalid volume, left %f, right %f", __FUNCTION__, left, right);
        return BAD_VALUE;
    }

    //  mOffloadVol = mStreamManager->GetOffloadGain(left);
    //  ALOGD("%s(), 0x%x", __FUNCTION__,mOffloadVol);
    ALOGD("%s(), vl = %lf, vr = %lf", __FUNCTION__, left, right);
    // make as 8_24 fotmat
    uint32_t vl = (uint32_t)(left * (1 << 24));
    uint32_t vr = (uint32_t)(right * (1 << 24));
    mOffloadVol = vl;

    if (mPlaybackHandler != NULL) {
        mPlaybackHandler->setVolume(vl);
        return NO_ERROR;
    }

    ALOGE("%s(), playbackhandler NULL", __FUNCTION__);
    return NO_ERROR;
}


ssize_t AudioALSAStreamOut::write(const void *buffer, size_t bytes) {
    ALOGV("%s(), buffer = %p, bytes = %zu, flags %d", __FUNCTION__, buffer, bytes, mStreamAttributeSource.mAudioOutputFlags);

    if (mStandby && mSuspendCount == 0) {
        // update policy device without holding stream out lock.
        mStreamManager->updateOutputDeviceForAllStreamIn(mStreamAttributeSource.output_devices);

        if (mStreamAttributeSource.usePolicyDevice) {
            mStreamAttributeSource.output_devices = mStreamAttributeSource.policyDevice;
            mStreamAttributeSource.usePolicyDevice = false;
            mStreamManager->syncSharedOutDevice(mStreamAttributeSource.output_devices, this);
        }

        // set volume before open device to avoid pop, and without holding stream out lock.
        mStreamManager->setMasterVolume(mStreamManager->getMasterVolume(), getIdentity());
    }

    /* fast output is RT thread and keep streamout lock for write kernel.
       so other thread can't get streamout lock. if necessary, output will active release CPU. */
    int tryCount = 10;
    while (mLockCount && tryCount--) {
        ALOGD_IF(tryCount == 0, "%s, free CPU, mLockCount %d, tryCount %d", __FUNCTION__, mLockCount, tryCount);
        usleep(300);
    }

    AL_AUTOLOCK(mSuspendLock);

    size_t outputSize = 0;
    if (mSuspendCount > 0 ||
        (mStreamOutType == STREAM_OUT_HDMI_STEREO && mSuspendStreamOutHDMIStereoCount > 0) ||
        (mStreamManager->isModeInPhoneCall() == true && mStreamOutType  != STREAM_OUT_PRIMARY && mStreamOutType  != STREAM_OUT_VOICE_DL)) {
        // here to sleep a buffer size latency and return.
        ALOGV("%s(), mStreamOutType = %d, mSuspendCount = %u, mSuspendStreamOutHDMIStereoCount = %d",
              __FUNCTION__, mStreamOutType, mSuspendCount, mSuspendStreamOutHDMIStereoCount);
        usleep(bufferSizeTimeUs());
        mPresentedBytes += bytes;
        return bytes;
    }

    AL_AUTOLOCK(mLock);

    status_t status = NO_ERROR;

    /// check open
    if (mStandby == true) {
        status = open();
        setScreenState_l();

        if (!mMixerOutInUse) {
            if (mPlaybackHandler->getPlaybackHandlerType() == PLAYBACK_HANDLER_OFFLOAD &&
                status != NO_ERROR) {
                mStreamCbk(STREAM_CBK_EVENT_ERROR, 0, mCbkCookie);
                return 0;
            }

            mPlaybackHandler->setFirstDataWriteFlag(true);
        }
    } else {
        if (!mMixerOutInUse) {
            mPlaybackHandler->setFirstDataWriteFlag(false);
        }
    }

    if (bytes == 0) {
        return 0;
    }

    WritePcmDumpData(buffer, bytes);

    dataProcessForMute(buffer, bytes);

    /// write pcm data
    if (mMixerOutInUse) {
        outputSize = mMixerOut->write(this, buffer, bytes);
    } else {
        ASSERT(mPlaybackHandler != NULL);
        mPlaybackHandler->preWriteOperation(buffer, bytes);
        outputSize = mPlaybackHandler->write(buffer, bytes);
    }
    mPaused = false;
    mPresentedBytes += outputSize;
    //ALOGD("%s(), outputSize = %d, bytes = %d,mPresentedBytes=%d", __FUNCTION__, outputSize, bytes, mPresentedBytes);
    return outputSize;
}

status_t AudioALSAStreamOut::standby() {
    // standby is used by framework to close stream out.
    ALOGD("%s(), flag %d", __FUNCTION__, mStreamAttributeSource.mAudioOutputFlags);
    status_t status = NO_ERROR;

    mStreamAttributeSource.usePolicyDevice = true;
    status = standbyStreamOut();

    return status;
}

status_t AudioALSAStreamOut::syncPolicyDevice() {
    mStreamAttributeSource.usePolicyDevice = true;
    return NO_ERROR;
}

status_t AudioALSAStreamOut::standbyStreamOut() {
    ALOGD("%s(), flag %d", __FUNCTION__, mStreamAttributeSource.mAudioOutputFlags);
    int oldCount;
    oldCount = android_atomic_inc(&mLockCount);
    AL_AUTOLOCK(mLock);

    status_t status = NO_ERROR;

    /// check close
    if (mStandby == false) {
        status = close();
    }

    oldCount = android_atomic_dec(&mLockCount);
    return status;
}


status_t AudioALSAStreamOut::dump(int fd __unused, const Vector<String16> &args __unused) {
    ALOGV("%s()", __FUNCTION__);
    return NO_ERROR;
}

int AudioALSAStreamOut::setStreamOutSampleRate(const uint32_t sampleRate) {
    AL_AUTOLOCK(mLock);
    int ret = 0;
    /* primary and deep buffer support hifi playback */
    if (((mStreamAttributeSource.mAudioOutputFlags & AUDIO_OUTPUT_FLAG_PRIMARY) ||
         (mStreamAttributeSource.mAudioOutputFlags & AUDIO_OUTPUT_FLAG_DEEP_BUFFER)) > 0) {
        mStreamAttributeSource.sample_rate = sampleRate;
        ALOGD("%s(), flag:0x%x, HIFI_AUDIO_SAMPLERATE = %u",
              __FUNCTION__, mStreamAttributeSource.mAudioOutputFlags, sampleRate);
    }
    return ret;
}

bool AudioALSAStreamOut::SampleRateSupport(uint32_t sampleRate) {
    // ALPS02409284, fast output don't support HIFI
#ifdef DOWNLINK_LOW_LATENCY
    if (mStreamAttributeSource.mAudioOutputFlags & AUDIO_OUTPUT_FLAG_FAST) {
        return (sampleRate == 44100 || sampleRate == 48000) ? true : false;
    }
#endif

    if (sampleRate == 8000  || sampleRate == 11025 || sampleRate == 12000
        || sampleRate == 16000 || sampleRate == 22050 || sampleRate == 24000
        || sampleRate == 32000 || sampleRate == 44100 || sampleRate == 48000
        || sampleRate == 88200 || sampleRate == 96000 || sampleRate == 176400 || sampleRate == 192000) {
        return true;
    } else {
        return false;
    }
}

status_t AudioALSAStreamOut::UpdateSampleRate(int sampleRate) {
    ALOGD("%s() sampleRate = %d", __FUNCTION__, sampleRate);
    // check sample rate
    if (SampleRateSupport(sampleRate) == true) {
        AudioALSASampleRateController::getInstance()->setPrimaryStreamOutSampleRate(sampleRate);
        mStreamAttributeSource.sample_rate = sampleRate;
        setBufferSize();
    } else {
        ALOGE("%s(), wrong sampleRate %d, use %d instead.", __FUNCTION__, sampleRate, kDefaultOutputSourceSampleRate);
        sampleRate = kDefaultOutputSourceSampleRate;
    }
    return NO_ERROR;
}



status_t AudioALSAStreamOut::setParameters(const String8 &keyValuePairs) {
    AudioParameter param = AudioParameter(keyValuePairs);

    /// keys
    const String8 keyRouting = String8(AudioParameter::keyRouting);
    const String8 keySampleRate = String8(AudioParameter::keySamplingRate);
    const String8 keyDynamicSampleRate = String8("DynamicSampleRate");
    const String8 keyLowLatencyMode = String8("LowLatencyMode");
    const String8 keyDSM = String8("DSM");
    const String8 keyMP3PCM_DUMP   = String8("MP3_PCMDUMP");

    //#ifdef MTK_BASIC_PACKAGE
#if 1
    const String8 keyRoutingToNone = String8(AUDIO_PARAMETER_KEY_ROUTING_TO_NONE);
    const String8 keyFmDirectControl = String8(AUDIO_PARAMETER_KEY_FM_DIRECT_CONTROL);
#else
    const String8 keyRoutingToNone = String8(AudioParameter::keyRoutingToNone);
    const String8 keyFmDirectControl = String8(AudioParameter::keyFmDirectControl);
#endif
    /// parse key value pairs
    status_t status = NO_ERROR;
    int value = 0;
    String8 value_str;
    int oldCount;

    /// routing
    if (param.getInt(keyRouting, value) == NO_ERROR) {
        param.remove(keyRouting);

        oldCount = android_atomic_inc(&mLockCount);
        if (mStreamOutType == STREAM_OUT_PRIMARY || mStreamOutType == STREAM_OUT_VOICE_DL) {
#if defined(MTK_MAXIM_SPEAKER_SUPPORT)
            if (mStreamManager->IsSphStrmSupport()) {
                ALOGD("%s():Check Speech OutputDevice, %d -> %d", __FUNCTION__, mStreamAttributeSource.output_devices, value);
                if (mStreamAttributeSource.output_devices != static_cast<audio_devices_t>(value)) {
                    mStreamManager->DisableSphStrmByDevice(static_cast<audio_devices_t>(value));
                }
            }
#endif

            updatePolicyDevice(value);
            status = mStreamManager->routingOutputDevice(this, mStreamAttributeSource.output_devices, static_cast<audio_devices_t>(value));

            if (mMuteForRouting) {
                setMuteForRouting(false);
            }

#if defined(MTK_MAXIM_SPEAKER_SUPPORT)
            if (mStreamManager->IsSphStrmSupport()) {
                mStreamManager->EnableSphStrmByDevice(static_cast<audio_devices_t>(value));
            }
#endif
        } else if ((mStreamOutType == STREAM_OUT_HDMI_STEREO) || (mStreamOutType == STREAM_OUT_HDMI_MULTI_CHANNEL)) {
            ALOGD("%s(), HDMI  \"%s\"", __FUNCTION__, param.toString().string());
            updatePolicyDevice(value);
            status = mStreamManager->routingOutputDevice(this, mStreamAttributeSource.output_devices, static_cast<audio_devices_t>(value));
        } else {
            ALOGW("%s(), NUM_STREAM_OUT_TYPE \"%s\"", __FUNCTION__, param.toString().string());
            status = INVALID_OPERATION;
        }
        oldCount = android_atomic_dec(&mLockCount);
    } else {
        ALOGD("+%s(): flag %d, %s", __FUNCTION__, mStreamAttributeSource.mAudioOutputFlags, keyValuePairs.string());
    }

    if (param.getInt(keyFmDirectControl, value) == NO_ERROR) {
        param.remove(keyFmDirectControl);

        oldCount = android_atomic_inc(&mLockCount);
        AL_AUTOLOCK(mLock);
        AudioALSAFMController::getInstance()->setUseFmDirectConnectionMode(value ? true : false);
        oldCount = android_atomic_dec(&mLockCount);
    }
    // routing none, for no stream but has device change. e.g. vow path change
    if (param.getInt(keyRoutingToNone, value) == NO_ERROR) {
        param.remove(keyRoutingToNone);

        oldCount = android_atomic_inc(&mLockCount);
        AL_AUTOLOCK(mLock);
        status = mStreamManager->DeviceNoneUpdate();
        oldCount = android_atomic_dec(&mLockCount);
    }
    /// sample rate
    if (param.getInt(keyDynamicSampleRate, value) == NO_ERROR) {
        param.remove(keyDynamicSampleRate);
        oldCount = android_atomic_inc(&mLockCount);
        AL_AUTOLOCK(mLock);
        if (mStreamOutType == STREAM_OUT_PRIMARY || mStreamOutType == STREAM_OUT_VOICE_DL) {
            status = NO_ERROR; //AudioALSASampleRateController::getInstance()->setPrimaryStreamOutSampleRate(value); // TODO(Harvey): enable it later
        } else {
            ALOGW("%s(), HDMI bypass \"%s\"", __FUNCTION__, param.toString().string());
            status = INVALID_OPERATION;
        }
        oldCount = android_atomic_dec(&mLockCount);
    }

#if defined(MTK_HIFIAUDIO_SUPPORT)
    if (param.getInt(keySampleRate, value) == NO_ERROR) {
        param.remove(keySampleRate);
        status = mStreamManager->setAllStreamHiFi(this, value);
    }
#else
    if (param.getInt(keySampleRate, value) == NO_ERROR) {
        param.remove(keySampleRate);
        oldCount = android_atomic_inc(&mLockCount);
        AL_AUTOLOCK(mLock);
        if (mStandby) {
            UpdateSampleRate(value);
        } else {
            status = INVALID_OPERATION;
        }
        oldCount = android_atomic_dec(&mLockCount);
    }
#endif

#ifdef MTK_MAXIM_SPEAKER_SUPPORT
    if (param.get(keyDSM, value_str) == NO_ERROR) {
        ALOGD(" setParameters(keyValuePairs); = %s", keyValuePairs.string());
        AudioALSAPlaybackHandlerSpeakerProtection *mSpkinstance = (AudioALSAPlaybackHandlerSpeakerProtection *)mPlaybackHandler;
        if ((mPlaybackHandler != NULL) && mPlaybackHandler->getPlaybackHandlerType() == PLAYBACK_HANDLER_SPEAKERPROTECTION) {
            AudioALSAPlaybackHandlerSpeakerProtection *mSpkinstance = (AudioALSAPlaybackHandlerSpeakerProtection *)mPlaybackHandler;
            mSpkinstance->setParameters(keyValuePairs);
        }
        param.remove(keyDSM);
    }
#endif
#ifdef MTK_AUDIO_TUNNELING_SUPPORT
    if (param.getInt(keyMP3PCM_DUMP, value) == NO_ERROR) {
        AudioALSAPlaybackHandlerOffload *mOffloadinstance = (AudioALSAPlaybackHandlerOffload *)mPlaybackHandler;
        if ((mPlaybackHandler != NULL) && mPlaybackHandler->getPlaybackHandlerType() == PLAYBACK_HANDLER_OFFLOAD) {
            mOffloadinstance->set_pcmdump(value);
        }
        param.remove(keyMP3PCM_DUMP);
    }
#endif

#if defined(MTK_AUDIO_GAIN_TABLE) || defined(MTK_AUDIO_HIERARCHICAL_PARAM_SUPPORT)
    static String8 keyVolumeStreamType    = String8("volumeStreamType");
    static String8 keyVolumeDevice        = String8("volumeDevice");
    static String8 keyVolumeIndex         = String8("volumeIndex");
    if (param.getInt(keyVolumeStreamType, value) == NO_ERROR) {
        int device = 0;
        int index = 0;
        if (param.getInt(keyVolumeDevice, device) == NO_ERROR) {
            if (param.getInt(keyVolumeIndex, index) == NO_ERROR) {
#ifdef MTK_AUDIO_GAIN_TABLE
                mStreamManager->setAnalogVolume(value, device, index, 0);
#endif
#if defined(MTK_AUDIO_HIERARCHICAL_PARAM_SUPPORT)
                mStreamManager->setMDVolumeIndex(value, device, index);
#endif
            }
        }
        param.remove(keyVolumeStreamType);
        param.remove(keyVolumeDevice);
        param.remove(keyVolumeIndex);
    }
#endif

    if (param.get(String8(AUDIO_PARAMETER_DEVICE_CONNECT), value_str) == NO_ERROR) {
        if (param.getInt(String8(AUDIO_PARAMETER_DEVICE_CONNECT), value) == NO_ERROR) {
            audio_devices_t device = (audio_devices_t)value;
            mStreamManager->updateDeviceConnectionState(device, true);
        }

        if (param.get(String8("card"), value_str) == NO_ERROR) {
            param.remove(String8("card"));
        }

        if (param.get(String8("device"), value_str) == NO_ERROR) {
            param.remove(String8("device"));
        }

        param.remove(String8(AUDIO_PARAMETER_DEVICE_CONNECT));
    }

    if (param.get(String8(AUDIO_PARAMETER_DEVICE_DISCONNECT), value_str) == NO_ERROR) {
        if (param.getInt(String8(AUDIO_PARAMETER_DEVICE_DISCONNECT), value) == NO_ERROR) {
            audio_devices_t device = (audio_devices_t)value;
            mStreamManager->updateDeviceConnectionState(device, false);
        }

        if (param.get(String8("card"), value_str) == NO_ERROR) {
            param.remove(String8("card"));
        }

        if (param.get(String8("device"), value_str) == NO_ERROR) {
            param.remove(String8("device"));
        }

        param.remove(String8(AUDIO_PARAMETER_DEVICE_DISCONNECT));
    }

    if (param.size()) {
        ALOGW("%s(), still have param.size() = %zu, remain param = \"%s\"",
              __FUNCTION__, param.size(), param.toString().string());
        status = BAD_VALUE;
    }

    ALOGV("-%s(): %s ", __FUNCTION__, keyValuePairs.string());
    return status;
}


String8 AudioALSAStreamOut::getParameters(const String8 &keys) {
    ALOGD("%s, keyvalue %s", __FUNCTION__, keys.string());

    String8 value;
    String8 keyLowLatency = String8("LowLatency");
    String8 keyDSM = String8("DSM");

    AudioParameter param = AudioParameter(keys);
    AudioParameter returnParam = AudioParameter();

#ifdef MTK_MAXIM_SPEAKER_SUPPORT
    if (param.get(keyDSM, value) == NO_ERROR) {
        ALOGD(" getParameters(keys); = %s", keys.string());
        param.remove(keyDSM);
        AudioALSAPlaybackHandlerSpeakerProtection *mTemp = (AudioALSAPlaybackHandlerSpeakerProtection *)mPlaybackHandler;
        value = mTemp->getParameters(keys);
        returnParam = AudioParameter(value);
    }
#endif
#ifdef MTK_AUDIO_TUNNELING_SUPPORT
    static String8 keyOffloadAudioCheckSupport = String8("OffloadAudio_Check_Support");
    char result[PROPERTY_VALUE_MAX];

    if (param.get(keyOffloadAudioCheckSupport, value) == NO_ERROR) {
        if (!offloadflag) {
            property_get(allow_offload_propty, result, "0");
            offloadflag = atoi(result);
        }
        if (offloadflag) {
            value = "yes";
        } else {
            value = "no";
        }
        param.remove(keyOffloadAudioCheckSupport);
        returnParam.add(keyOffloadAudioCheckSupport, value);
    }
#endif

    const String8 keyValuePairs = returnParam.toString();
    ALOGD("-%s(), return \"%s\"", __FUNCTION__, keyValuePairs.string());
    return keyValuePairs;
}


int AudioALSAStreamOut::getRenderPosition(uint32_t *dspFrames) {
    if (mPlaybackHandler == NULL) {
        ALOGE("%s() handler NULL, frames: %" PRIu64 "", __FUNCTION__, mPresentFrames);
        *dspFrames = mPresentFrames;
        return -ENOSYS;
    }

    if (mPlaybackHandler->getPlaybackHandlerType() == PLAYBACK_HANDLER_OFFLOAD) {
        unsigned long codec_io_frame;
        unsigned int codec_samplerate;
        if (NO_ERROR == mPlaybackHandler->get_timeStamp(&codec_io_frame, &codec_samplerate)) {
            if (codec_samplerate == 0) {
                ALOGE("%s(), Compress Not Ready", __FUNCTION__);
                return -ENODATA;
            }
            *dspFrames = codec_io_frame;
            mPresentFrames = codec_io_frame;
            ALOGV("%s(), get_tstamp done:%d ", __FUNCTION__, *dspFrames);
            return 0;
        } else {
            *dspFrames = mPresentFrames;
            ALOGE("%s(), get_tstamp fail, frame:%" PRIu64 "", __FUNCTION__, mPresentFrames);
            return -ENODATA;
        }
    }
    return -ENOSYS;
}

int AudioALSAStreamOut::getPresentationPosition(uint64_t *frames, struct timespec *timestamp) {
    ALOGV("+%s()", __FUNCTION__);
    AL_AUTOLOCK(mLock);

    time_info_struct_t HW_Buf_Time_Info;
    memset(&HW_Buf_Time_Info, 0, sizeof(HW_Buf_Time_Info));

    const uint8_t size_per_channel = (mStreamAttributeSource.audio_format == AUDIO_FORMAT_PCM_8_BIT ? 1 :
                                      (mStreamAttributeSource.audio_format == AUDIO_FORMAT_PCM_16_BIT ? 2 :
                                       (mStreamAttributeSource.audio_format == AUDIO_FORMAT_PCM_32_BIT ? 4 :
                                        2)));
    ALOGV("%s(), size_per_channel = %u, mStreamAttributeSource.num_channels = %d, mStreamAttributeSource.audio_channel_mask = %x,mPresentedBytes = %" PRIu64 "\n",
          __FUNCTION__, size_per_channel, mStreamAttributeSource.num_channels, mStreamAttributeSource.audio_channel_mask, mPresentedBytes);

    if (mPlaybackHandler == NULL) {
        *frames = mPresentedBytes / (uint64_t)(mStreamAttributeSource.num_channels * size_per_channel);
        if (mMixerOutInUse) {
            clock_gettime(CLOCK_MONOTONIC, timestamp);
            return NO_ERROR;
        } else {
            timestamp->tv_sec = 0;
            timestamp->tv_nsec = 0;
            ALOGV("-%s(), no playback handler, *frames = %" PRIu64 ", return", __FUNCTION__, *frames);
            return -EINVAL;
        }
    }


    //query remaining hardware buffer size
    int status = 0;
    if (mPlaybackHandler->getPlaybackHandlerType() == PLAYBACK_HANDLER_OFFLOAD) {
        unsigned long codec_io_frame;
        unsigned int codec_samplerate;
        unsigned long time;
        if (NO_ERROR == mPlaybackHandler->get_timeStamp(&codec_io_frame, &codec_samplerate)) {
            if (codec_samplerate == 0) {
                *frames = 0;
                timestamp->tv_sec = 0;
                timestamp->tv_nsec = 0;
                ALOGE("%s(), Compress Not Ready", __FUNCTION__);
                return -EINVAL;
            }
            *frames = codec_io_frame;
            mPresentFrames = codec_io_frame;
            time = codec_io_frame / codec_samplerate;
            timestamp->tv_sec = time;
            time = codec_io_frame % codec_samplerate;
            timestamp->tv_nsec = time * 1000000000 / codec_samplerate;
            ALOGV("%s(), get_tstamp done:%" PRIu64 "", __FUNCTION__, *frames);
        } else {
            *frames = mPresentFrames;
            ALOGD("%s(), get_tstamp fail, frames:%" PRIu64 "", __FUNCTION__, mPresentFrames);
            return -EINVAL;
        }
    } else {

        if (mPlaybackHandler->getHardwareBufferInfo(&HW_Buf_Time_Info) != NO_ERROR) {
            *frames = mPresentedBytes / (uint64_t)(mStreamAttributeSource.num_channels * size_per_channel);
            status = -EINVAL;
            ALOGV("-%s(), getHardwareBufferInfo fail, *frames = %" PRIu64 ", return -EINVAL", __FUNCTION__, *frames);
        } else {
            uint64_t presentedFrames = mPresentedBytes / (uint64_t)(mStreamAttributeSource.num_channels * size_per_channel);

            const stream_attribute_t *pStreamAttributeTarget = mPlaybackHandler->getStreamAttributeTarget();

            uint64_t remainInKernel = (uint64_t)(HW_Buf_Time_Info.buffer_per_time - HW_Buf_Time_Info.frameInfo_get);
            remainInKernel = (remainInKernel * mStreamAttributeSource.sample_rate) / pStreamAttributeTarget->sample_rate;

            long long remainInHal = HW_Buf_Time_Info.halQueuedFrame;
            remainInHal = (remainInHal * mStreamAttributeSource.sample_rate) / pStreamAttributeTarget->sample_rate;

            *frames = (uint64_t)presentedFrames - remainInKernel - remainInHal;
            *timestamp = HW_Buf_Time_Info.timestamp_get;
            status = 0;

            ALOGV("-%s(), *frames = %" PRIu64 ", timestamp %ld.%ld, remainInKernel %" PRIu64 ", remainInHal %lld, presentedFrames %" PRIu64 "",
                  __FUNCTION__, *frames, (long)timestamp->tv_sec, (long)timestamp->tv_nsec, remainInKernel, remainInHal, presentedFrames);
        }
    }
    return status;
}

status_t AudioALSAStreamOut::getNextWriteTimestamp(int64_t *timestamp __unused) {
    return INVALID_OPERATION;
}

status_t AudioALSAStreamOut::setCallBack(stream_callback_t callback, void *cookie) {
    //TODO : new for KK
    mStreamCbk = callback;
    mCbkCookie = cookie;
    return NO_ERROR;
}


status_t AudioALSAStreamOut::open() {
    // call open() only when mLock is locked.
    ASSERT(AL_TRYLOCK(mLock) != 0);

    ALOGD("%s(), flags %d", __FUNCTION__, mStreamAttributeSource.mAudioOutputFlags);

    status_t status = NO_ERROR;

#if defined(MTK_POWERHAL_AUDIO_LATENCY)
    if ((mStreamAttributeSource.mAudioOutputFlags & AUDIO_OUTPUT_FLAG_FAST) &&
        mStreamAttributeSource.mPowerHalEnable) {
        power_hal_hint(POWERHAL_LATENCY_DL, true);
    }
#endif


    if (mStandby == true) {
        // HDMI stereo + HDMI multi-channel => disable HDMI stereo
        if (mStreamOutType == STREAM_OUT_HDMI_MULTI_CHANNEL) {
            ALOGD("Force disable mStreamOutHDMIStereo");
            AudioALSAStreamOut::setSuspendStreamOutHDMIStereo(true);
            if (mStreamOutHDMIStereo != NULL) {
                ALOGD("mStreamOutHDMIStereo->standby");
                mStandby = false;
                mStreamOutHDMIStereo->standbyStreamOut();
            }
        }

        AudioALSASampleRateController::getInstance()->setScenarioStatus(PLAYBACK_SCENARIO_STREAM_OUT);

        if (isBtSpkDevice(mStreamAttributeSource.output_devices) || audio_is_bluetooth_sco_device(mStreamAttributeSource.output_devices)) {
            mMixerOut->attach(this, &mStreamAttributeSource);
            mMixerOutInUse = true;
        } else {
            // create playback handler
            ASSERT(mPlaybackHandler == NULL);
            mPlaybackHandler = mStreamManager->createPlaybackHandler(&mStreamAttributeSource);

            // open audio hardware
            status = mPlaybackHandler->open();

            // offload allow return fail
            if (mPlaybackHandler->getPlaybackHandlerType() == PLAYBACK_HANDLER_OFFLOAD) {
                if (status == NO_ERROR) {
                    mPlaybackHandler->setComprCallback(mStreamCbk, mCbkCookie);
                    mPlaybackHandler->setVolume(mOffloadVol);
                }
            } else {
                ASSERT(status == NO_ERROR);
            }
        }

        OpenPCMDump(LOG_TAG);

        mStandby = false;
    }

    return status;
}


status_t AudioALSAStreamOut::close() {
    // call close() only when mLock is locked.
    ASSERT(AL_TRYLOCK(mLock) != 0);

    ALOGD("%s(), flags %d, mMixerOutInUse %d", __FUNCTION__, mStreamAttributeSource.mAudioOutputFlags, mMixerOutInUse);

    status_t status = NO_ERROR;

    if (mStandby == false) {
        // HDMI stereo + HDMI multi-channel => disable HDMI stereo
        if (mStreamOutType == STREAM_OUT_HDMI_MULTI_CHANNEL) {
            ALOGD("Recover mStreamOutHDMIStereo");
            AudioALSAStreamOut::setSuspendStreamOutHDMIStereo(false);
        }

        ClosePCMDump();

        if (mMixerOutInUse) {
            mMixerOut->detach(this);
            mMixerOutInUse = false;
        } else {
            // close audio hardware
            ASSERT(mPlaybackHandler != NULL);
            status = mPlaybackHandler->close();
            if (status != NO_ERROR) {
                ALOGE("%s(), close() fail!!", __FUNCTION__);
            }

            // destroy playback handler
            mStreamManager->destroyPlaybackHandler(mPlaybackHandler);
            mPlaybackHandler = NULL;
        }

        AudioALSASampleRateController::getInstance()->resetScenarioStatus(PLAYBACK_SCENARIO_STREAM_OUT);

        mStandby = true;
        setMuteForRouting(false);
    }

#if defined(MTK_POWERHAL_AUDIO_LATENCY)
    if ((mStreamAttributeSource.mAudioOutputFlags & AUDIO_OUTPUT_FLAG_FAST) &&
        mStreamAttributeSource.mPowerHalEnable) {
        power_hal_hint(POWERHAL_LATENCY_DL, false);
    }
#endif

    ASSERT(mPlaybackHandler == NULL);
    return status;
}

int AudioALSAStreamOut::pause() {
    ALOGD("%s() %p", __FUNCTION__, mPlaybackHandler);
    if (mPlaybackHandler != NULL) {
        mPaused = true;
        return mPlaybackHandler->pause();
    }
    return -ENODATA;
}

int AudioALSAStreamOut::resume() {
    ALOGD("%s() %p", __FUNCTION__, mPlaybackHandler);
    if (mPlaybackHandler != NULL) {
        mPaused = false;
        return mPlaybackHandler->resume();
    }
    return -ENODATA;
}

int AudioALSAStreamOut::flush() {
    ALOGD("%s() %p", __FUNCTION__, mPlaybackHandler);
    if (mPlaybackHandler != NULL) {
        return mPlaybackHandler->flush();
    }
    return 0;
}

int AudioALSAStreamOut::drain(audio_drain_type_t type) {
    ALOGD("%s() %p", __FUNCTION__, mPlaybackHandler);
    if (mPlaybackHandler != NULL) {
        return mPlaybackHandler->drain(type);
    }
    return 0;
}

status_t AudioALSAStreamOut::routing(audio_devices_t output_devices) {
    AL_AUTOLOCK(mLock);

    status_t status = NO_ERROR;

    if (output_devices == mStreamAttributeSource.output_devices) {
        ALOGW("%s(), warning, flag %d, routing to same device(%x) is not necessary",
              __FUNCTION__, mStreamAttributeSource.mAudioOutputFlags, output_devices);
        return status;
    }

    ALOGD("+%s(), route output device from %x to %x, flag %d", __FUNCTION__,
          mStreamAttributeSource.output_devices, output_devices, mStreamAttributeSource.mAudioOutputFlags);

    if (mStandby == false) {
        if (mMixerOutInUse) {
            status = close();
        } else {
            ASSERT(mPlaybackHandler != NULL);

            if (mPlaybackHandler->getPlaybackHandlerType() != PLAYBACK_HANDLER_OFFLOAD) {
                status = close();
            }
        }
    }
    mStreamAttributeSource.output_devices = output_devices;

    ALOGD("-%s()", __FUNCTION__);
    return status;
}

void AudioALSAStreamOut::updatePolicyDevice(audio_devices_t outputDevice) {
    AL_AUTOLOCK(mLock);
    ALOGV("%s(), device: %d", __FUNCTION__, outputDevice);
    mStreamAttributeSource.policyDevice = outputDevice;
}

status_t AudioALSAStreamOut::setScreenState(bool mode) {
    ALOGD("+%s(), flag %d, mode %d", __FUNCTION__, mStreamAttributeSource.mAudioOutputFlags, mode);
    int oldCount;
    oldCount = android_atomic_inc(&mLockCount);
    AL_AUTOLOCK(mLock);
    oldCount = android_atomic_dec(&mLockCount);

    mLowLatencyMode = mode;

    return setScreenState_l();
}

int AudioALSAStreamOut::updateAudioMode(audio_mode_t mode) {
    int ret = 0;

    ALOGV("%s(), mode %d, flags %d", __FUNCTION__, mode, mStreamAttributeSource.mAudioOutputFlags);

    int oldCount;
    oldCount = android_atomic_inc(&mLockCount);
    AL_AUTOLOCK(mLock);
    oldCount = android_atomic_dec(&mLockCount);

    if (!mStandby) {
        // update source attribute
        mStreamAttributeSource.audio_mode = mode;
        mStreamAttributeSource.mVoIPEnable = (mode == AUDIO_MODE_IN_COMMUNICATION) ? true : false;

        if (!mMixerOutInUse) {
            ret = mPlaybackHandler->updateAudioMode(mode);
        }
    }
    return ret;
}

status_t AudioALSAStreamOut::setSuspend(const bool suspend_on) {
    ALOGD("+%s(), mSuspendCount = %u, suspend_on = %d, flags %d, mMixerOutInUse %d",
          __FUNCTION__, mSuspendCount, suspend_on, mStreamAttributeSource.mAudioOutputFlags, mMixerOutInUse);
    int oldCount;
    oldCount = android_atomic_inc(&mLockCount);
    AL_AUTOLOCK(mSuspendLock);
    oldCount = android_atomic_dec(&mLockCount);

    if (suspend_on == true) {
        mSuspendCount++;

        if (mMixerOutInUse && mSuspendCount == 1) {
            mMixerOut->setSuspend(this, true);
        }
    } else if (suspend_on == false) {
        ASSERT(mSuspendCount > 0);
        mSuspendCount--;

        if (mMixerOutInUse && mSuspendCount == 0) {
            mMixerOut->setSuspend(this, false);
        }
    }

    ALOGV("-%s(), mSuspendCount = %u", __FUNCTION__, mSuspendCount);
    return NO_ERROR;
}


status_t AudioALSAStreamOut::setSuspendStreamOutHDMIStereo(const bool suspend_on) {
    ALOGD("+%s(), mSuspendStreamOutHDMIStereoCount = %u, suspend_on = %d",
          __FUNCTION__, mSuspendStreamOutHDMIStereoCount, suspend_on);

    if (suspend_on == true) {
        mSuspendStreamOutHDMIStereoCount++;
    } else if (suspend_on == false) {
        ASSERT(mSuspendStreamOutHDMIStereoCount > 0);
        mSuspendStreamOutHDMIStereoCount--;
    }

    ALOGD("-%s(), mSuspendStreamOutHDMIStereoCount = %u", __FUNCTION__, mSuspendStreamOutHDMIStereoCount);
    return NO_ERROR;
}

status_t AudioALSAStreamOut::setMuteForRouting(bool mute) {
    ALOGD("%s(), mute %d, flags %d", __FUNCTION__, mute, mStreamAttributeSource.mAudioOutputFlags);
    mMuteForRouting = mute;
    if (mute) {
        clock_gettime(CLOCK_MONOTONIC, &mMuteTime);
    }
    return NO_ERROR;
}

void AudioALSAStreamOut::OpenPCMDump(const char *class_name) {
    ALOGV("%s()", __FUNCTION__);
    char mDumpFileName[128];
    sprintf(mDumpFileName, "%s.%d.%s.pid%d.tid%d.pcm", streamout, mDumpFileNum, class_name, getpid(), gettid());

    mPCMDumpFile = NULL;
    mPCMDumpFile = AudioOpendumpPCMFile(mDumpFileName, streamout_propty);

    if (mPCMDumpFile != NULL) {
        ALOGD("%s DumpFileName = %s", __FUNCTION__, mDumpFileName);

        mDumpFileNum++;
        mDumpFileNum %= MAX_DUMP_NUM;
    }
}

void AudioALSAStreamOut::ClosePCMDump() {
    ALOGV("%s()", __FUNCTION__);
    if (mPCMDumpFile) {
        AudioCloseDumpPCMFile(mPCMDumpFile);
        ALOGD("%s(), close it", __FUNCTION__);
    }
}

void  AudioALSAStreamOut::WritePcmDumpData(const void *buffer, ssize_t bytes) {
    if (mPCMDumpFile) {
        //ALOGD("%s()", __FUNCTION__);
        AudioDumpPCMData((void *)buffer, bytes, mPCMDumpFile);
    }
}

status_t AudioALSAStreamOut::dataProcessForMute(const void *buffer, size_t bytes) {
    void *pBuffer = const_cast<void *>(buffer);

    if (mMuteForRouting) {
        // check mute timeout
        bool isNeedUnmuteRamp = false;
        clock_gettime(CLOCK_MONOTONIC, &mMuteCurTime);
        double totalMuteTime = calc_time_diff(mMuteCurTime, mMuteTime);

        ALOGW("%s(), flag %d, mMuteForRouting %d, totalMuteTime %f",
              __FUNCTION__, mStreamAttributeSource.mAudioOutputFlags,  mMuteForRouting, totalMuteTime);

        if (totalMuteTime > 0.300) {
            setMuteForRouting(false);
            isNeedUnmuteRamp = true;
        }

        if (isNeedUnmuteRamp) {
            ALOGW("%s(), mute timeout, unmute and ramp, format %d", __FUNCTION__, mStreamAttributeSource.audio_format);
            if (mStreamAttributeSource.audio_format == AUDIO_FORMAT_PCM_32_BIT ||
                mStreamAttributeSource.audio_format == AUDIO_FORMAT_PCM_16_BIT) {
                size_t tmpBytes = bytes;
                int formatBytes = audio_bytes_per_sample(mStreamAttributeSource.audio_format);
                int32_t *sample = (int32_t *)buffer;
                int16_t *sample16 = (int16_t *)buffer;
                while (tmpBytes > 0) {
                    if (mStreamAttributeSource.audio_format == AUDIO_FORMAT_PCM_32_BIT) {
                        *sample = (*sample) * ((float)(bytes - tmpBytes) / bytes);
                        sample++;
                    } else {
                        *sample16 = (*sample16) * ((float)(bytes - tmpBytes) / bytes);
                        sample16++;
                    }
                    tmpBytes -= formatBytes;
                }
            }
        } else {
            memset(pBuffer, 0, bytes);
        }
    }

    return NO_ERROR;
}

void  AudioALSAStreamOut::setBufferSize() {

    // set default value here. and change it when open by different type of handlers
    if (mStreamAttributeSource.audio_channel_mask == AUDIO_CHANNEL_OUT_5POINT1 ||
        mStreamAttributeSource.audio_channel_mask == AUDIO_CHANNEL_OUT_7POINT1) {
        size_t sizePerFrame = getSizePerFrame(mStreamAttributeSource.audio_format,
                                              mStreamAttributeSource.num_channels);

        mStreamAttributeSource.buffer_size = BUFFER_FRAME_COUNT_PER_ACCESSS_HDMI * sizePerFrame;
        mStreamAttributeSource.latency = (KERNEL_BUFFER_SIZE_DL1_NORMAL * 1000) /
                                         (mStreamAttributeSource.sample_rate * sizePerFrame);
    } else {
        mStreamAttributeSource.buffer_size = BUFFER_FRAME_COUNT_PER_ACCESS *
                                             getSizePerFrame(mStreamAttributeSource.audio_format,
                                                             mStreamAttributeSource.num_channels);

#ifdef PLAYBACK_USE_24BITS_ONLY
        size_t sizePerFrame = getSizePerFrame(AUDIO_FORMAT_PCM_8_24_BIT,
                                              mStreamAttributeSource.num_channels);
#else
        size_t sizePerFrame = getSizePerFrame(mStreamAttributeSource.audio_format,
                                              mStreamAttributeSource.num_channels);
#endif
        mStreamAttributeSource.latency = isIsolatedDeepBuffer(mStreamAttributeSource.mAudioOutputFlags) ?
                                         KERNEL_BUFFER_SIZE_DL1_DATA2_NORMAL :
                                         KERNEL_BUFFER_SIZE_DL1_NORMAL;

        if (isIsolatedDeepBuffer(mStreamAttributeSource.mAudioOutputFlags)) {
            size_t sizePerFrame = getSizePerFrame(mStreamAttributeSource.audio_format,
                                                  mStreamAttributeSource.num_channels);

            uint32_t max_buffer_size = MAX_BUFFER_FRAME_COUNT_PER_ACCESS * sizePerFrame;

            mStreamAttributeSource.buffer_size = mStreamAttributeSource.latency -
                                                 (KERNEL_BUFFER_FRAME_COUNT_REMAIN * sizePerFrame);
            if (mStreamAttributeSource.buffer_size > max_buffer_size) {
                ALOGD("reduce hal buffer size %d -> %d", mStreamAttributeSource.buffer_size, max_buffer_size);
                mStreamAttributeSource.buffer_size = max_buffer_size;
            }
        }

        mStreamAttributeSource.latency = (mStreamAttributeSource.latency * 1000) /
                                         (mStreamAttributeSource.sample_rate * sizePerFrame);
    }

#ifdef DOWNLINK_LOW_LATENCY
    if (mStreamAttributeSource.mAudioOutputFlags & AUDIO_OUTPUT_FLAG_FAST) {
        if (mStreamAttributeSource.sample_rate <= 48000) {
            mStreamAttributeSource.buffer_size = FRAME_COUNT_MIN_PER_ACCESSS;
        } else if (mStreamAttributeSource.sample_rate <= 96000) {
            mStreamAttributeSource.buffer_size = FRAME_COUNT_MIN_PER_ACCESSS << 1;
        } else if (mStreamAttributeSource.sample_rate <= 192000) {
            mStreamAttributeSource.buffer_size = FRAME_COUNT_MIN_PER_ACCESSS << 2;
        } else {
            ASSERT(0);
        }
        mStreamAttributeSource.latency = (mStreamAttributeSource.buffer_size * 2 * 1000) / mStreamAttributeSource.sample_rate;
        mStreamAttributeSource.buffer_size *= mStreamAttributeSource.num_channels *
                                              audio_bytes_per_sample(mStreamAttributeSource.audio_format);
    }
#endif
#ifdef MTK_AUDIO_TUNNELING_SUPPORT
    if (mStreamAttributeSource.mAudioOutputFlags & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD) {
        mStreamAttributeSource.buffer_size = OFFLOAD_BUFFER_SIZE_PER_ACCESSS;
    }
#endif

    size_t sizePerFrame = getSizePerFrame(mStreamAttributeSource.audio_format, mStreamAttributeSource.num_channels);
    mStreamAttributeSource.frame_count = mStreamAttributeSource.buffer_size / sizePerFrame;
}

}
