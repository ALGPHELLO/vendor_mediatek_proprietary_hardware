/*
* Copyright (C) 2014 MediaTek Inc.
* Modification based on code covered by the mentioned copyright
* and/or permission notice(s).
*/
/*
 * Copyright (C) 2012 The Android Open Source Project
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

#define LOG_TAG "modules.usbaudio.audio_hal"
/*#define LOG_NDEBUG 0*/
#if defined(CONFIG_MT_ENG_BUILD)
#define MTK_AUDIO_DEBUG
#endif

#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <log/log.h>
#include <cutils/list.h>
#include <cutils/str_parms.h>
#include <cutils/properties.h>
#include "AudioUtility.h"

#include <hardware/audio.h>
//#include <hardware/audio_alsaops.h>
#include <hardware/hardware.h>

#include <system/audio.h>

#include <tinyalsa/asoundlib.h>


#ifdef MTK_AURISYS_FRAMEWORK_SUPPORT
#include <AudioALSACaptureDataProviderBase.h>
#include <AudioALSACaptureDataProviderUsb.h>
#include <IAudioALSACaptureDataClient.h>
#include <AudioExternWrapper.h>
#include <AudioALSAPlaybackHandlerUsb.h>
#endif

#define USB_TAG "[USB_AUD] "

extern "C" {
#include "alsa_device_profile.h"
#include "alsa_device_proxy.h"
#include "alsa_logging.h"
#include <audio_utils/channels.h>
}

#include <dlfcn.h>
#define DEFAULT_INPUT_BUFFER_SIZE_MS 20

/* Lock play & record samples rates at or above this threshold */
#define RATELOCK_THRESHOLD 96000
#define HAL_LIBRARY_PATH1 "/system/lib/hw"
#define HAL_LIBRARY_PATH2 "/vendor/lib/hw"
#define AUDIO_HAL_PREFIX "audio.primary"
#define PLATFORM_ID "ro.board.platform"
#define BOARD_PLATFORM_ID "ro.board.platform"


#ifdef MTK_AURISYS_FRAMEWORK_SUPPORT
static android::AudioALSACaptureDataProviderUsb *gAudioMTKAudioUSBProvider = NULL;
static android::AudioALSACaptureDataProviderEchoRefUsb *gAudioMTKAudioUSBProviderEchoRef = NULL;
static android::IAudioALSACaptureDataClient *gAudioMTKAudioUSBDataClient = NULL;
static android::AudioALSAPlaybackHandlerUsb*gAudioMTKAudioUSBPlaybackHandler = NULL;
static void *AudioPrimaryHAL = NULL;
static bool AurisysEnabled = false;
static bool AurisysRecordEnabled = false;
static bool AurisysAECRecordEnabled = false;
static bool AurisysPlaybackEnabled = false;
static bool AurisysRecordReopen = false;
static stream_attribute_t gStreamAttributeRecord;
static stream_attribute_t gStreamAttributePlayback;
static stream_attribute_t *gStreamAttributeRecordTarget = NULL;
static stream_attribute_t *gStreamAttributePlaybackSource = NULL;
#endif
volatile int32_t  mReadLockCount = 0;
volatile int32_t  mWriteLockCount = 0;


namespace android
{
extern "C" {

struct audio_device {
    struct audio_hw_device hw_device;

    pthread_mutex_t lock; /* see note below on mutex acquisition order */

    /* output */
    alsa_device_profile out_profile;
    struct listnode output_stream_list;

    /* input */
    alsa_device_profile in_profile;
    struct listnode input_stream_list;

    /* lock input & output sample rates */
    /*FIXME - How do we address multiple output streams? */
    uint32_t device_sample_rate;

    bool mic_muted;

    bool standby;
};

struct stream_lock {
    pthread_mutex_t lock;               /* see note below on mutex acquisition order */
    pthread_mutex_t pre_lock;           /* acquire before lock to avoid DOS by playback thread */
};

struct stream_out {
    struct audio_stream_out stream;

    struct stream_lock  lock;

    bool standby;

    struct audio_device *adev;           /* hardware information - only using this for the lock */

    alsa_device_profile * profile;      /* Points to the alsa_device_profile in the audio_device */
    alsa_device_proxy proxy;            /* state of the stream */

    unsigned hal_channel_count;         /* channel count exposed to AudioFlinger.
                                         * This may differ from the device channel count when
                                         * the device is not compatible with AudioFlinger
                                         * capabilities, e.g. exposes too many channels or
                                         * too few channels. */
    audio_channel_mask_t hal_channel_mask;  /* USB devices deal in channel counts, not masks
                                             * so the proxy doesn't have a channel_mask, but
                                             * audio HALs need to talk about channel masks
                                             * so expose the one calculated by
                                             * adev_open_output_stream */

    struct listnode list_node;

    void * conversion_buffer;           /* any conversions are put into here
                                         * they could come from here too if
                                         * there was a previous conversion */
    size_t conversion_buffer_size;      /* in bytes */
};

struct stream_in {
    struct audio_stream_in stream;

    struct stream_lock  lock;

    bool standby;

    struct audio_device *adev;           /* hardware information - only using this for the lock */

    alsa_device_profile * profile;      /* Points to the alsa_device_profile in the audio_device */
    alsa_device_proxy proxy;            /* state of the stream */

    unsigned hal_channel_count;         /* channel count exposed to AudioFlinger.
                                         * This may differ from the device channel count when
                                         * the device is not compatible with AudioFlinger
                                         * capabilities, e.g. exposes too many channels or
                                         * too few channels. */
    audio_channel_mask_t hal_channel_mask;  /* USB devices deal in channel counts, not masks
                                             * so the proxy doesn't have a channel_mask, but
                                             * audio HALs need to talk about channel masks
                                             * so expose the one calculated by
                                             * adev_open_input_stream */

    struct listnode list_node;

    /* We may need to read more data from the device in order to data reduce to 16bit, 4chan */
    void * conversion_buffer;           /* any conversions are put into here
                                         * they could come from here too if
                                         * there was a previous conversion */
    size_t conversion_buffer_size;      /* in bytes */

    audio_source_t input_source;        /* MTK add */
};

//MTK +++
//usb output pcm dump
const char * usbstreamout = "/sdcard/mtklog/audio_dump/usbstreamout.pcm";
const char * usbstreamout_propty = "streamout.pcm.dump";
//usb input pcm dump
const char * usbstreamin = "/sdcard/mtklog/audio_dump/usbstreamin.pcm";
const char * usbstreamin_propty = "streamin.pcm.dump";
//MTK ---

/*
 * Locking Helpers
 */
/*
 * NOTE: when multiple mutexes have to be acquired, always take the
 * stream_in or stream_out mutex first, followed by the audio_device mutex.
 * stream pre_lock is always acquired before stream lock to prevent starvation of control thread by
 * higher priority playback or capture thread.
 */

static void stream_lock_init(struct stream_lock *lock) {
    pthread_mutex_init(&lock->lock, (const pthread_mutexattr_t *) NULL);
    pthread_mutex_init(&lock->pre_lock, (const pthread_mutexattr_t *) NULL);
}

static void stream_lock(struct stream_lock *lock) {
    pthread_mutex_lock(&lock->pre_lock);
    pthread_mutex_lock(&lock->lock);
    pthread_mutex_unlock(&lock->pre_lock);
}

static void stream_unlock(struct stream_lock *lock) {
    pthread_mutex_unlock(&lock->lock);
}

static void device_lock(struct audio_device *adev) {
    pthread_mutex_lock(&adev->lock);
}

static int device_try_lock(struct audio_device *adev) {
    return pthread_mutex_trylock(&adev->lock);
}

static void device_unlock(struct audio_device *adev) {
    pthread_mutex_unlock(&adev->lock);
}

/*
 * streams list management
 */
static void adev_add_stream_to_list(
    struct audio_device* adev, struct listnode* list, struct listnode* stream_node) {
    device_lock(adev);

    list_add_tail(list, stream_node);

    device_unlock(adev);
}

static void adev_remove_stream_from_list(
    struct audio_device* adev, struct listnode* stream_node) {
    device_lock(adev);

    list_remove(stream_node);

    device_unlock(adev);
}


/*
 * Extract the card and device numbers from the supplied key/value pairs.
 *   kvpairs    A null-terminated string containing the key/value pairs or card and device.
 *              i.e. "card=1;device=42"
 *   card   A pointer to a variable to receive the parsed-out card number.
 *   device A pointer to a variable to receive the parsed-out device number.
 * NOTE: The variables pointed to by card and device return -1 (undefined) if the
 *  associated key/value pair is not found in the provided string.
 *  Return true if the kvpairs string contain a card/device spec, false otherwise.
 */
#ifdef MTK_AURISYS_FRAMEWORK_SUPPORT
 static bool in_load_primary_hal()
{
    AurisysEnabled = false;
    if (AudioPrimaryHAL == NULL) {
        char prop[PATH_MAX];
        char path[PATH_MAX];
        do {
            if (property_get(PLATFORM_ID, prop, NULL) == 0) {
                snprintf(path, sizeof(path), "%s/%s.default.so",
                         HAL_LIBRARY_PATH1, prop);
                if (access(path, R_OK) == 0) break;

                snprintf(path, sizeof(path), "%s/%s.default.so",
                     HAL_LIBRARY_PATH2, prop);
                if (access(path, R_OK) == 0) break;
            } else {
                snprintf(path, sizeof(path), "%s/%s.%s.so",
                         HAL_LIBRARY_PATH1, AUDIO_HAL_PREFIX, prop);
                if (access(path, R_OK) == 0) break;

                snprintf(path, sizeof(path), "%s/%s.%s.so",
                     HAL_LIBRARY_PATH2, AUDIO_HAL_PREFIX, prop);
                if (access(path, R_OK) == 0) break;

                if (property_get(BOARD_PLATFORM_ID, prop, NULL) == 0) {
                    snprintf(path, sizeof(path), "%s/%s.default.so",
                             HAL_LIBRARY_PATH1, prop);
                    if (access(path, R_OK) == 0) break;

                    snprintf(path, sizeof(path), "%s/%s.default.so",
                         HAL_LIBRARY_PATH2, prop);
                    if (access(path, R_OK) == 0) break;
                } else {
                    snprintf(path, sizeof(path), "%s/%s.%s.so",
                         HAL_LIBRARY_PATH1, AUDIO_HAL_PREFIX, prop);
                     if (access(path, R_OK) == 0) break;

                     snprintf(path, sizeof(path), "%s/%s.%s.so",
                          HAL_LIBRARY_PATH2, AUDIO_HAL_PREFIX, prop);
                     if (access(path, R_OK) == 0) break;
                }
            }
        } while(0);
        ALOGD ("Load %s",path);
        AudioPrimaryHAL = dlopen(path, RTLD_NOW);
    }
    if (AudioPrimaryHAL != NULL) {
        AurisysEnabled = true;
    }
    ALOGD("%s %d AurisysEnabled = %d, gAudioMTKAudioUSBProvider = %p, gAudioMTKAudioUSBDataClient = %p",__FUNCTION__,__LINE__,AurisysEnabled, gAudioMTKAudioUSBProvider, gAudioMTKAudioUSBDataClient);
    return AurisysEnabled;
}

  static bool init_primary_playback(alsa_device_proxy *proxy, size_t WriteBufferSize)
 {
     AurisysPlaybackEnabled = false;
     if (gAudioMTKAudioUSBPlaybackHandler == NULL) {

         memset(&gStreamAttributePlayback, 0, sizeof(gStreamAttributePlayback));
         gStreamAttributePlayback.audio_format = audio_format_from_pcm_format(proxy->alsa_config.format);
         gStreamAttributePlayback.num_channels = proxy->alsa_config.channels;
         gStreamAttributePlayback.sample_rate = proxy->alsa_config.rate;
         gStreamAttributePlayback.mAudioInputFlags = AUDIO_INPUT_FLAG_NONE;
         gStreamAttributePlayback.input_source = AUDIO_SOURCE_MIC;
         gStreamAttributePlayback.input_device = AUDIO_DEVICE_IN_USB_DEVICE;
         gStreamAttributePlayback.output_devices = AUDIO_DEVICE_OUT_USB_DEVICE;
         gStreamAttributePlayback.audio_mode = AUDIO_MODE_NORMAL;
         gStreamAttributePlayback.acoustics_mask = AUDIO_IN_ACOUSTICS_NONE;
         gStreamAttributePlayback.BesRecord_Info.besrecord_enable = false;
         gStreamAttributePlayback.BesRecord_Info.besrecord_voip_enable = false;
         gStreamAttributePlaybackSource = &gStreamAttributePlayback;
         ALOGD(USB_TAG"%s ,gStreamAttributePlayback.input_source = %d, gStreamAttributePlayback.sample_rate = %d, gStreamAttributePlayback.audio_format = %d, gStreamAttributePlayback.num_channels = %d,",__FUNCTION__, gStreamAttributePlayback.input_source, gStreamAttributePlayback.sample_rate, gStreamAttributePlayback.audio_format, gStreamAttributePlayback.num_channels);

         ASSERT(AudioPrimaryHAL != NULL);
         create_AudioMTKUSBPlaybackHandler *func1 = (create_AudioMTKUSBPlaybackHandler*)dlsym(AudioPrimaryHAL, "createMTKAudioUSBPlaybackHandler");
         ALOGV(USB_TAG"%s %d func1 %p",__FUNCTION__,__LINE__,func1);
         const char* dlsym_error1 = dlerror();
         if (func1 == NULL) {
             ALOGE(USB_TAG"-dlsym createMTKAudioUSBPlaybackHandler fail");
             return AurisysPlaybackEnabled;
         }
         gAudioMTKAudioUSBPlaybackHandler = func1(gStreamAttributePlaybackSource);
         gAudioMTKAudioUSBPlaybackHandler->initUsbInfo(gStreamAttributePlayback, proxy, WriteBufferSize);
         AurisysPlaybackEnabled = true;
         if ((AurisysRecordEnabled == true) && (AurisysAECRecordEnabled == false) &&
             (gAudioMTKAudioUSBProvider != NULL) && (gAudioMTKAudioUSBProvider->isNeedEchoRefData() == true))
         {
            AurisysRecordReopen = true;
         }
     }
     ALOGD(USB_TAG"%s %d AurisysPlaybackEnabled = %d, gAudioMTKAudioUSBPlaybackHandler = %p, AurisysRecordReopen = %d",__FUNCTION__,__LINE__,AurisysPlaybackEnabled, gAudioMTKAudioUSBPlaybackHandler, AurisysRecordReopen);
     return AurisysPlaybackEnabled;
 }

 static bool init_primary_record(struct stream_in *in, size_t ReadBufferSize)
{
    AurisysRecordEnabled = false;
    AurisysAECRecordEnabled = false;
    if (gAudioMTKAudioUSBDataClient == NULL || gAudioMTKAudioUSBProvider == NULL) {
        alsa_device_proxy* proxy = &in->proxy;
        memset(&gStreamAttributeRecord, 0, sizeof(gStreamAttributeRecord));
        gStreamAttributeRecord.audio_format = audio_format_from_pcm_format(proxy->alsa_config.format);
        gStreamAttributeRecord.num_channels = proxy->alsa_config.channels;
        gStreamAttributeRecord.sample_rate = proxy->alsa_config.rate;
        gStreamAttributeRecord.mAudioInputFlags = AUDIO_INPUT_FLAG_NONE;
        gStreamAttributeRecord.input_source = in->input_source;
        gStreamAttributeRecord.input_device = AUDIO_DEVICE_IN_USB_DEVICE;
        gStreamAttributeRecord.output_devices = AUDIO_DEVICE_OUT_USB_DEVICE;
        gStreamAttributeRecord.audio_mode = AUDIO_MODE_NORMAL;
        gStreamAttributeRecord.acoustics_mask = AUDIO_IN_ACOUSTICS_NONE;
        gStreamAttributeRecord.BesRecord_Info.besrecord_enable = true;
        gStreamAttributeRecord.BesRecord_Info.besrecord_voip_enable = false;
        ALOGD(USB_TAG"%s ,gStreamAttributeRecord.input_source = %d, gStreamAttributeRecord.sample_rate = %d, gStreamAttributeRecord.audio_format = %d, gStreamAttributeRecord.num_channels = %d,",__FUNCTION__, gStreamAttributeRecord.input_source, gStreamAttributeRecord.sample_rate, gStreamAttributeRecord.audio_format, gStreamAttributeRecord.num_channels);
        ASSERT(AudioPrimaryHAL != NULL);
        create_AudioMTKUSBProvider *func1 = (create_AudioMTKUSBProvider *)dlsym(AudioPrimaryHAL, "createMTKAudioUSBProvider");
        ALOGV(USB_TAG"%s %d func1 %p",__FUNCTION__,__LINE__,func1);
        const char* dlsym_error1 = dlerror();
        if (func1 == NULL) {
            ALOGE(USB_TAG"-dlsym createMTKAudioUSBProvider fail");
            return AurisysRecordEnabled;
        }
        gAudioMTKAudioUSBProvider = func1();
        gAudioMTKAudioUSBProvider->initUsbInfo(gStreamAttributeRecord, proxy, ReadBufferSize, AurisysPlaybackEnabled);
        bool echorefon = gAudioMTKAudioUSBProvider->isNeedEchoRefData();
        //If VOIP Mode, but Playback is not ready, no need to do AEC
        if (AurisysPlaybackEnabled == true && echorefon == true)
        {
            create_AudioMTKUSBProviderEchoRef *func2 = (create_AudioMTKUSBProviderEchoRef *)dlsym(AudioPrimaryHAL, "createMTKAudioUSBProviderEchoRef");
            if (func2 == NULL) {
                ALOGE(USB_TAG"-dlsym createMTKAudioUSBProviderEchoRef fail");
                return AurisysRecordEnabled;
                }
            gStreamAttributeRecord.BesRecord_Info.besrecord_voip_enable = true;
            gStreamAttributeRecord.mVoIPEnable = true;
            gStreamAttributeRecord.audio_mode = AUDIO_MODE_IN_COMMUNICATION;
            gAudioMTKAudioUSBProviderEchoRef = func2();
            gAudioMTKAudioUSBProviderEchoRef->initEchoRefInfo(gStreamAttributePlayback);
            AurisysAECRecordEnabled = true;
            ALOGV(USB_TAG"%s VOIP MODE",__FUNCTION__);
        }
        if (AurisysAECRecordEnabled == false && echorefon == true)
        {
            // LIB Parser error handling
            gStreamAttributeRecord.input_source = AUDIO_SOURCE_MIC;
        }
        gStreamAttributeRecordTarget = &gStreamAttributeRecord;
        create_AudioMTKDataClient* func2 = (create_AudioMTKDataClient *)dlsym(AudioPrimaryHAL, "createMTKAudioDataClient");
        ALOGV(USB_TAG"%s %d func2 %p",__FUNCTION__,__LINE__,func2);
        const char* dlsym_error2 = dlerror();
        if (func2 == NULL) {
            ALOGE(USB_TAG"-dlsym createMTKAudioDataClient fail");
            return AurisysRecordEnabled;
        }

        gAudioMTKAudioUSBDataClient = func2(gAudioMTKAudioUSBProvider, gStreamAttributeRecordTarget, gAudioMTKAudioUSBProviderEchoRef);
        if (gAudioMTKAudioUSBProvider->getPcmStatus() != NO_ERROR)
        {
            delete gAudioMTKAudioUSBDataClient;
            if (AurisysAECRecordEnabled)
            {
                gAudioMTKAudioUSBProviderEchoRef = NULL;
                AurisysAECRecordEnabled = false;
                ALOGD(USB_TAG"usb: delete gAudioMTKAudioUSBProviderEchoRef");
            }
            gAudioMTKAudioUSBProvider = NULL;
            gAudioMTKAudioUSBDataClient = NULL;
            gStreamAttributeRecordTarget = NULL;
            AurisysRecordEnabled = false;
            ALOGD(USB_TAG"%s AurisysRecordEnabled = %d, AurisysAECRecordEnabled = %d, can not open pcm",__FUNCTION__,AurisysRecordEnabled, AurisysAECRecordEnabled);
            return AurisysRecordEnabled;
        }
        AurisysRecordEnabled = true;
    }
    ALOGD(USB_TAG"%s %d AurisysRecordEnabled = %d, AurisysAECRecordEnabled = %d, gAudioMTKAudioUSBProvider = %p, gAudioMTKAudioUSBDataClient = %p",__FUNCTION__,__LINE__,AurisysRecordEnabled, AurisysAECRecordEnabled, gAudioMTKAudioUSBProvider, gAudioMTKAudioUSBDataClient);
    return AurisysRecordEnabled;
}
#endif

static bool parse_card_device_params(const char *kvpairs, int *card, int *device)
{
    struct str_parms * parms = str_parms_create_str(kvpairs);
    char value[32];
    int param_val;

    // initialize to "undefined" state.
    *card = -1;
    *device = -1;

    param_val = str_parms_get_str(parms, "card", value, sizeof(value));
    if (param_val >= 0) {
        *card = atoi(value);
    }

    param_val = str_parms_get_str(parms, "device", value, sizeof(value));
    if (param_val >= 0) {
        *device = atoi(value);
    }

    str_parms_destroy(parms);

    return *card >= 0 && *device >= 0;
}

static char * device_get_parameters(alsa_device_profile * profile, const char * keys)
{
#ifdef MTK_AUDIO_DEBUG
    ALOGD("usb:audio_hal::device_get_parameters() keys:%s", keys);
#else
    ALOGV("usb:audio_hal::device_get_parameters() keys:%s", keys);
#endif

    if (profile->card < 0 || profile->device < 0) {
        return strdup("");
    }

    struct str_parms *query = str_parms_create_str(keys);
    struct str_parms *result = str_parms_create();

    /* These keys are from hardware/libhardware/include/audio.h */
    /* supported sample rates */
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES)) {
        char* rates_list = profile_get_sample_rate_strs(profile);
        str_parms_add_str(result, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES,
                          rates_list);
        free(rates_list);
    }

    /* supported channel counts */
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_CHANNELS)) {
        char* channels_list = profile_get_channel_count_strs(profile);
        str_parms_add_str(result, AUDIO_PARAMETER_STREAM_SUP_CHANNELS,
                          channels_list);
        free(channels_list);
    }

    /* supported sample formats */
    if (str_parms_has_key(query, AUDIO_PARAMETER_STREAM_SUP_FORMATS)) {
        char * format_params = profile_get_format_strs(profile);
        str_parms_add_str(result, AUDIO_PARAMETER_STREAM_SUP_FORMATS,
                          format_params);
        free(format_params);
    }
    str_parms_destroy(query);

    char* result_str = str_parms_to_str(result);
    str_parms_destroy(result);

    ALOGV("device_get_parameters = %s", result_str);
#ifdef MTK_AUDIO_DEBUG
    ALOGD("usb:audio_hal::device_get_parameters = %s", result_str);
#else
    ALOGV("usb:audio_hal::device_get_parameters = %s", result_str);
#endif

    return result_str;
}

//MTK+++
#define calc_time_diff(x, y) ((x.tv_sec - y.tv_sec )+ (double)( x.tv_nsec - y.tv_nsec ) / (double)1000000000)
struct timespec mNewtime, mOldtime;  // for calculate latency
double timerec[3];  // 0=>threadloop, 1=>kernel delay, 2=>process delay
bool bDebugEnable = false;

int checkAndCreateDirectory(const char * pC)
{
    char tmp[PATH_MAX];
    int i = 0;

    while(*pC)
    {
        tmp[i] = *pC;

        if(*pC == '/' && i)
        {
            tmp[i] = '\0';
            if(access(tmp, F_OK) != 0)
            {
                if(mkdir(tmp, 0770) == -1)
                {
                	ALOGE("AudioDumpPCM: mkdir error! %s\n",(char*)strerror(errno));
                    return -1;
                }
            }
            tmp[i] = '/';
        }
        i++;
        pC++;
    }
	return 0;
}


void dumpPcmData(const char * filepath, void * buffer, int count,const char * propty)
{
    char value[PROPERTY_VALUE_MAX];
    int ret;
    property_get(propty, value, "0");
    int bflag=atoi(value);
    bDebugEnable = bflag;
    if(bflag)
    {
       ret = checkAndCreateDirectory(filepath);
	   if(ret<0)
	   {
	       ALOGE("dumpPcmData checkAndCreateDirectory() fail!!!");
	   }
	   else
       {
           FILE * fp= fopen(filepath, "ab+");
           if(fp!=NULL)
           {
               fwrite(buffer,1,count,fp);
               fclose(fp);
           }
	   }
    }
}
//MTK---

/*
 * HAl Functions
 */
/**
 * NOTE: when multiple mutexes have to be acquired, always respect the
 * following order: hw device > out stream
 */

/*
 * OUT functions
 */
static uint32_t out_get_sample_rate(const struct audio_stream *stream)
{
    uint32_t rate = proxy_get_sample_rate(&((struct stream_out*)stream)->proxy);
    ALOGV("out_get_sample_rate() = %d", rate);
    return rate;
}

static int out_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
#ifdef MTK_AUDIO_DEBUG
    ALOGD(USB_TAG"out_set_sample_rate");
#endif
    return 0;
}

static size_t out_get_buffer_size(const struct audio_stream *stream)
{
    const struct stream_out* out = (const struct stream_out*)stream;
    size_t buffer_size =
        proxy_get_period_size(&out->proxy) * audio_stream_out_frame_size(&(out->stream));
#ifdef MTK_AUDIO_DEBUG
    ALOGD(USB_TAG"out_get_buffer_size %zu, period size=%d", buffer_size,proxy_get_period_size(&out->proxy));
#endif
    return buffer_size;
}

static uint32_t out_get_channels(const struct audio_stream *stream)
{
    const struct stream_out *out = (const struct stream_out*)stream;
    return out->hal_channel_mask;
}

static audio_format_t out_get_format(const struct audio_stream *stream)
{
    /* Note: The HAL doesn't do any FORMAT conversion at this time. It
     * Relies on the framework to provide data in the specified format.
     * This could change in the future.
     */
    alsa_device_proxy * proxy = &((struct stream_out*)stream)->proxy;
    audio_format_t format = audio_format_from_pcm_format(proxy_get_format(proxy));
    return format;
}

static int out_set_format(struct audio_stream *stream, audio_format_t format)
{
    return 0;
}

static int out_standby(struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;
    int oldCount;
    oldCount = android_atomic_inc(&mWriteLockCount);

    stream_lock(&out->lock);
    ALOGD(USB_TAG"out_standby");
    if (!out->standby) {
        device_lock(out->adev);

#ifdef MTK_AURISYS_FRAMEWORK_SUPPORT
        if (AurisysPlaybackEnabled == true) {
            gAudioMTKAudioUSBPlaybackHandler->close();
            delete gAudioMTKAudioUSBPlaybackHandler;
            gAudioMTKAudioUSBPlaybackHandler = NULL;
            gStreamAttributePlaybackSource = NULL;
            AurisysPlaybackEnabled = false;
        }else
#endif
        {
            proxy_close(&out->proxy);
        }
        device_unlock(out->adev);
        out->standby = true;
    }
    stream_unlock(&out->lock);
    oldCount = android_atomic_dec(&mWriteLockCount);
    return 0;
}

static int out_dump(const struct audio_stream *stream, int fd) {
    const struct stream_out* out_stream = (const struct stream_out*) stream;

    if (out_stream != NULL) {
        dprintf(fd, "Output Profile:\n");
        profile_dump(out_stream->profile, fd);

        dprintf(fd, "Output Proxy:\n");
        proxy_dump(&out_stream->proxy, fd);
    }

    return 0;
}

static int out_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
#ifdef MTK_AUDIO_DEBUG
    ALOGD("usb:audio_hal::out out_set_parameters() keys:%s", kvpairs);
#else
    ALOGV("out_set_parameters() keys:%s", kvpairs);
#endif

    struct stream_out *out = (struct stream_out *)stream;

    int routing = 0;
    int ret_value = 0;
    int card = -1;
    int device = -1;
    int oldCount;

    if (!parse_card_device_params(kvpairs, &card, &device)) {
        // nothing to do
        return ret_value;
    }

    oldCount = android_atomic_inc(&mWriteLockCount);
    stream_lock(&out->lock);
    /* Lock the device because that is where the profile lives */
    device_lock(out->adev);
    oldCount = android_atomic_dec(&mWriteLockCount);

    if (!profile_is_cached_for(out->profile, card, device)) {
        /* cannot read pcm device info if playback is active */
        if (!out->standby)
            ret_value = -ENOSYS;
        else {
            int saved_card = out->profile->card;
            int saved_device = out->profile->device;
            out->profile->card = card;
            out->profile->device = device;
#ifdef MTK_AUDIO_DEBUG
            ALOGD("usb:audio_hal::out out_set_parameters() profile_read_device_info, saved_card=%d,saved_device=%d", saved_card,saved_device);
#endif
            ret_value = profile_read_device_info(out->profile) ? 0 : -EINVAL;
            if (ret_value != 0) {
                out->profile->card = saved_card;
                out->profile->device = saved_device;
            }
        }
    }

    device_unlock(out->adev);
    stream_unlock(&out->lock);

    return ret_value;
}

static char * out_get_parameters(const struct audio_stream *stream, const char *keys)
{
    struct stream_out *out = (struct stream_out *)stream;
    stream_lock(&out->lock);
#ifdef MTK_AUDIO_DEBUG
    ALOGD("usb:audio_hal::out out_get_parameters() keys:%s", keys);
#endif
    device_lock(out->adev);

    char * params_str =  device_get_parameters(out->profile, keys);

    device_unlock(out->adev);
    stream_unlock(&out->lock);
    return params_str;
}

static uint32_t out_get_latency(const struct audio_stream_out *stream)
{
    alsa_device_proxy * proxy = &((struct stream_out*)stream)->proxy;
    return proxy_get_latency(proxy);
}

static int out_set_volume(struct audio_stream_out *stream, float left, float right)
{
    return -ENOSYS;
}

/* must be called with hw device and output stream mutexes locked */
static int start_output_stream(struct stream_out *out)
{
    ALOGD("start_output_stream(card:%d device:%d)", out->profile->card, out->profile->device);

#ifdef MTK_AURISYS_FRAMEWORK_SUPPORT
    int ret = 0;
    ASSERT(AurisysPlaybackEnabled == false);

    //need to modify this restrict
    if ((out->proxy.alsa_config.channels <= 2) && (out->proxy.alsa_config.format != PCM_FORMAT_S24_3LE)) {
        size_t WriteBufferSize = out_get_buffer_size((const struct audio_stream*)out);
        AurisysPlaybackEnabled = init_primary_playback(&out->proxy,WriteBufferSize);
        ASSERT(AurisysPlaybackEnabled == true);
        ret = gAudioMTKAudioUSBPlaybackHandler->open();
        if (ret != 0)
        {
            ALOGD(USB_TAG"start_output_stream fail");
            delete gAudioMTKAudioUSBPlaybackHandler;
            gAudioMTKAudioUSBPlaybackHandler = NULL;
            gStreamAttributePlaybackSource = NULL;
            AurisysPlaybackEnabled = false;
        }
        return ret;
    }
    else
#endif
    {
        ALOGD(USB_TAG"usb:audio_hal::start_output_stream: Not use Aurisys Record");
        return proxy_open(&out->proxy);
    }
}

static ssize_t out_write(struct audio_stream_out *stream, const void* buffer, size_t bytes)
{
    int ret;
    struct stream_out *out = (struct stream_out *)stream;

    clock_gettime(CLOCK_REALTIME, &mNewtime);
    timerec[0] = calc_time_diff(mNewtime, mOldtime);
    mOldtime = mNewtime;
    int handle;
    int tryCount = 10;

    while (mWriteLockCount != 0 && tryCount--) {
        ALOGV("%s, free CPU, mWriteLockCount = %d, tryCount %d", __FUNCTION__, mWriteLockCount, tryCount);
        usleep(300);
        if (tryCount == 0) {
            ALOGD("%s, free CPU, mWriteLockCount = %d, tryCount %d", __FUNCTION__, mWriteLockCount, tryCount);
        }
    }

    stream_lock(&out->lock);
    ALOGV(USB_TAG"+out_write(%zu)",bytes);
    if (out->standby) {
        device_lock(out->adev);
        ALOGD(USB_TAG"+out_write(%zu), start_output_stream",bytes);
        ret = start_output_stream(out);
        device_unlock(out->adev);
        if (ret != 0) {
            //goto err;
            ALOGE(USB_TAG"-out_write(error)");
            stream_unlock(&out->lock);
            usleep(bytes * 1000000 / audio_stream_out_frame_size(stream) /
                   out_get_sample_rate(&stream->common));
            return bytes;
        }
        out->standby = false;
    }

    alsa_device_proxy* proxy = &out->proxy;
    const void * write_buff = buffer;
    int num_write_buff_bytes = bytes;
    const int num_device_channels = proxy_get_channel_count(proxy); /* what we told alsa */
    const int num_req_channels = out->hal_channel_count; /* what we told AudioFlinger */
    if (num_device_channels != num_req_channels) {
        ALOGV(USB_TAG"num_write_buff_bytes = %d, num_device_channels = %d, num_req_channels = %d", num_write_buff_bytes, num_device_channels, num_req_channels);
        /* allocate buffer */
        const size_t required_conversion_buffer_size =
                 bytes * num_device_channels / num_req_channels;
        if (required_conversion_buffer_size > out->conversion_buffer_size) {
            out->conversion_buffer_size = required_conversion_buffer_size;
            out->conversion_buffer = realloc(out->conversion_buffer,
                                             out->conversion_buffer_size);
        }
        /* convert data */
        const audio_format_t audio_format = out_get_format(&(out->stream.common));
        const unsigned sample_size_in_bytes = audio_bytes_per_sample(audio_format);
        num_write_buff_bytes =
                adjust_channels(write_buff, num_req_channels,
                                out->conversion_buffer, num_device_channels,
                                sample_size_in_bytes, num_write_buff_bytes);
        write_buff = out->conversion_buffer;
    }

    clock_gettime(CLOCK_REALTIME, &mNewtime);
    timerec[2] = calc_time_diff(mNewtime, mOldtime);
    mOldtime = mNewtime;

    if (write_buff != NULL && num_write_buff_bytes != 0) {
#ifdef MTK_AURISYS_FRAMEWORK_SUPPORT
        if (AurisysPlaybackEnabled == true){
            gAudioMTKAudioUSBPlaybackHandler->write(write_buff, num_write_buff_bytes);
        }
        else
#endif
        {
            proxy_write(&out->proxy, write_buff, num_write_buff_bytes);
        }
        clock_gettime(CLOCK_REALTIME, &mNewtime);
        timerec[1] = calc_time_diff(mNewtime, mOldtime);
        mOldtime = mNewtime;
        //Dump debug data
        dumpPcmData(usbstreamout,(void*)write_buff,num_write_buff_bytes,usbstreamout_propty);
    }
    else {
        clock_gettime(CLOCK_REALTIME, &mOldtime);
        ALOGD(USB_TAG"out_write() no proxy_write?");
    }

    stream_unlock(&out->lock);

    if (bDebugEnable) {
        ALOGD(USB_TAG"%s, latency_in_us,%1.6lf,%1.6lf,%1.6lf", __FUNCTION__, timerec[0], timerec[1], timerec[2]);
    }
    ALOGV(USB_TAG"-out_write(%zu)",bytes);
    return bytes;
}

static int out_get_render_position(const struct audio_stream_out *stream, uint32_t *dsp_frames)
{
    return -EINVAL;
}

static int out_get_presentation_position(const struct audio_stream_out *stream,
                                         uint64_t *frames, struct timespec *timestamp)
{
    struct stream_out *out = (struct stream_out *)stream; // discard const qualifier
    stream_lock(&out->lock);

    const alsa_device_proxy *proxy = &out->proxy;
    const int ret = proxy_get_presentation_position(proxy, frames, timestamp);

    stream_unlock(&out->lock);

    if (ret != 0) {
        return -EINVAL;
    } else {
        return ret;
    }
}

static int out_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    return 0;
}

static int out_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    return 0;
}

static int out_get_next_write_timestamp(const struct audio_stream_out *stream, int64_t *timestamp)
{
    return -EINVAL;
}

static int adev_open_output_stream(struct audio_hw_device *hw_dev,
                                   audio_io_handle_t handle,
                                   audio_devices_t devicesSpec __unused,
                                   audio_output_flags_t flags,
                                   struct audio_config *config,
                                   struct audio_stream_out **stream_out,
                                   const char *address /*__unused*/)
{
    ALOGV("adev_open_output_stream() handle:0x%X, devicesSpec:0x%X, flags:0x%X, addr:%s",
          handle, devicesSpec, flags, address);
#ifdef MTK_AUDIO_DEBUG
    ALOGD("usb:audio_hal::out adev_open_output_stream() handle:0x%X, devicesSpec:0x%X, flags:0x%X",
          handle, devicesSpec, flags);
#endif

    struct stream_out *out;
    ALOGD(USB_TAG"adev_open_output_stream");
    out = (struct stream_out *)calloc(1, sizeof(struct stream_out));
    if (out == NULL) {
        return -ENOMEM;
    }

    /* setup function pointers */
    out->stream.common.get_sample_rate = out_get_sample_rate;
    out->stream.common.set_sample_rate = out_set_sample_rate;
    out->stream.common.get_buffer_size = out_get_buffer_size;
    out->stream.common.get_channels = out_get_channels;
    out->stream.common.get_format = out_get_format;
    out->stream.common.set_format = out_set_format;
    out->stream.common.standby = out_standby;
    out->stream.common.dump = out_dump;
    out->stream.common.set_parameters = out_set_parameters;
    out->stream.common.get_parameters = out_get_parameters;
    out->stream.common.add_audio_effect = out_add_audio_effect;
    out->stream.common.remove_audio_effect = out_remove_audio_effect;
    out->stream.get_latency = out_get_latency;
    out->stream.set_volume = out_set_volume;
    out->stream.write = out_write;
    out->stream.get_render_position = out_get_render_position;
    out->stream.get_presentation_position = out_get_presentation_position;
    out->stream.get_next_write_timestamp = out_get_next_write_timestamp;

    stream_lock_init(&out->lock);

    out->adev = (struct audio_device *)hw_dev;
    device_lock(out->adev);
    out->profile = &out->adev->out_profile;

    // build this to hand to the alsa_device_proxy
    struct pcm_config proxy_config;
    memset(&proxy_config, 0, sizeof(proxy_config));

    /* Pull out the card/device pair */
    parse_card_device_params(address, &(out->profile->card), &(out->profile->device));

    profile_read_device_info(out->profile);

    int ret = 0;
    ALOGD(USB_TAG"adev_open_output_stream, config.rate=%d, config.format=%d, channel mask=%x",config->sample_rate,config->format,config->channel_mask);
    /* Rate */
    if (config->sample_rate == 0) {
        proxy_config.rate = config->sample_rate = profile_get_default_sample_rate(out->profile);
    } else if (profile_is_sample_rate_valid(out->profile, config->sample_rate)) {
        proxy_config.rate = config->sample_rate;
    } else {
        proxy_config.rate = config->sample_rate = profile_get_default_sample_rate(out->profile);
        ret = -EINVAL;
    }

    out->adev->device_sample_rate = config->sample_rate;
    device_unlock(out->adev);

    /* Format */
    if (config->format == AUDIO_FORMAT_DEFAULT) {
        proxy_config.format = profile_get_default_format(out->profile);
        config->format = audio_format_from_pcm_format(proxy_config.format);
    } else {
        enum pcm_format fmt = pcm_format_from_audio_format(config->format);
        if (profile_is_format_valid(out->profile, fmt)) {
            proxy_config.format = fmt;
        } else {
            proxy_config.format = profile_get_default_format(out->profile);
            config->format = audio_format_from_pcm_format(proxy_config.format);
            ret = -EINVAL;
        }
    }

    /* Channels */
    bool calc_mask = false;
    if (config->channel_mask == AUDIO_CHANNEL_NONE) {
        /* query case */
        out->hal_channel_count = profile_get_default_channel_count(out->profile);
        calc_mask = true;
    } else {
        /* explicit case */
        out->hal_channel_count = audio_channel_count_from_out_mask(config->channel_mask);
    }

    /* The Framework is currently limited to no more than this number of channels */
    if (out->hal_channel_count > FCC_8) {
        out->hal_channel_count = FCC_8;
        calc_mask = true;
    }

    if (calc_mask) {
        /* need to calculate the mask from channel count either because this is the query case
         * or the specified mask isn't valid for this device, or is more then the FW can handle */
        config->channel_mask = out->hal_channel_count <= FCC_2
            /* position mask for mono and stereo*/
            ? audio_channel_out_mask_from_count(out->hal_channel_count)
            /* otherwise indexed */
            : audio_channel_mask_for_index_assignment_from_count(out->hal_channel_count);
    }

    out->hal_channel_mask = config->channel_mask;

    // Validate the "logical" channel count against support in the "actual" profile.
    // if they differ, choose the "actual" number of channels *closest* to the "logical".
    // and store THAT in proxy_config.channels
    proxy_config.channels = profile_get_closest_channel_count(out->profile, out->hal_channel_count);
    proxy_prepare(&out->proxy, out->profile, &proxy_config);

    /* TODO The retry mechanism isn't implemented in AudioPolicyManager/AudioFlinger. */
    ret = 0;

    out->conversion_buffer = NULL;
    out->conversion_buffer_size = 0;

    out->standby = true;

    /* Save the stream for adev_dump() */
    adev_add_stream_to_list(out->adev, &out->adev->output_stream_list, &out->list_node);

    *stream_out = &out->stream;

    return ret;
}

static void adev_close_output_stream(struct audio_hw_device *hw_dev,
                                     struct audio_stream_out *stream)
{
    struct stream_out *out = (struct stream_out *)stream;
    ALOGV("adev_close_output_stream(c:%d d:%d)", out->profile->card, out->profile->device);
    ALOGD(USB_TAG"adev_close_output_stream");
    adev_remove_stream_from_list(out->adev, &out->list_node);

    /* Close the pcm device */
    out_standby(&stream->common);

    free(out->conversion_buffer);

    out->conversion_buffer = NULL;
    out->conversion_buffer_size = 0;

    device_lock(out->adev);
    out->adev->device_sample_rate = 0;
    device_unlock(out->adev);

    free(stream);
}

static size_t adev_get_input_buffer_size(const struct audio_hw_device *hw_dev,
                                         const struct audio_config *config)
{
    /* TODO This needs to be calculated based on format/channels/rate */
    return 320;
}

/*
 * IN functions
 */
static uint32_t in_get_sample_rate(const struct audio_stream *stream)
{
    uint32_t rate = proxy_get_sample_rate(&((const struct stream_in *)stream)->proxy);
    ALOGV("in_get_sample_rate() = %d", rate);
    return rate;
}

static int in_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    ALOGV("in_set_sample_rate(%d) - NOPE", rate);
    return -ENOSYS;
}

static size_t in_get_buffer_size(const struct audio_stream *stream)
{
    const struct stream_in * in = ((const struct stream_in*)stream);
    return proxy_get_period_size(&in->proxy) * audio_stream_in_frame_size(&(in->stream));
}

static uint32_t in_get_channels(const struct audio_stream *stream)
{
    const struct stream_in *in = (const struct stream_in*)stream;
    return in->hal_channel_mask;
}

static audio_format_t in_get_format(const struct audio_stream *stream)
{
     alsa_device_proxy *proxy = &((struct stream_in*)stream)->proxy;
     audio_format_t format = audio_format_from_pcm_format(proxy_get_format(proxy));
     return format;
}

static int in_set_format(struct audio_stream *stream, audio_format_t format)
{
    ALOGV("in_set_format(%d) - NOPE", format);

    return -ENOSYS;
}

static int in_standby(struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;
    ALOGD(USB_TAG"usb: in_standby");
    int oldCount;
    oldCount = android_atomic_inc(&mReadLockCount);
    stream_lock(&in->lock);
    if (!in->standby) {
        device_lock(in->adev);
#ifdef MTK_AURISYS_FRAMEWORK_SUPPORT
        if (AurisysRecordEnabled == true) {
            delete gAudioMTKAudioUSBDataClient;
            proxy_close(&in->proxy);
            if (AurisysAECRecordEnabled)
            {
                gAudioMTKAudioUSBProviderEchoRef = NULL;
                AurisysAECRecordEnabled = false;
                ALOGD(USB_TAG"usb: delete gAudioMTKAudioUSBProviderEchoRef");
            }
            gAudioMTKAudioUSBProvider = NULL;
            gAudioMTKAudioUSBDataClient = NULL;
            gStreamAttributeRecordTarget = NULL;
            AurisysRecordEnabled = false;
        }else
#endif
        {
            proxy_close(&in->proxy);
        }
        device_unlock(in->adev);
        in->standby = true;
    }
    stream_unlock(&in->lock);
    oldCount = android_atomic_dec(&mReadLockCount);
    return 0;
}

static int in_dump(const struct audio_stream *stream, int fd)
{
  const struct stream_in* in_stream = (const struct stream_in*)stream;
  if (in_stream != NULL) {
      dprintf(fd, "Input Profile:\n");
      profile_dump(in_stream->profile, fd);

      dprintf(fd, "Input Proxy:\n");
      proxy_dump(&in_stream->proxy, fd);
  }

  return 0;
}

static int in_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
#ifdef MTK_AUDIO_DEBUG
    ALOGD("usb: audio_hal::in in_set_parameters() keys:%s", kvpairs);
#else
    ALOGV("in_set_parameters() keys:%s", kvpairs);
#endif

    struct stream_in *in = (struct stream_in *)stream;

    char value[32];
    int param_val;
    int routing = 0;
    int ret_value = 0;
    int card = -1;
    int device = -1;
    int oldCount;

    if (!parse_card_device_params(kvpairs, &card, &device)) {
        // nothing to do
        return ret_value;
    }

    oldCount = android_atomic_inc(&mReadLockCount);
    stream_lock(&in->lock);
    device_lock(in->adev);
    oldCount = android_atomic_dec(&mReadLockCount);

    if (card >= 0 && device >= 0 && !profile_is_cached_for(in->profile, card, device)) {
        /* cannot read pcm device info if playback is active */
        if (!in->standby)
            ret_value = -ENOSYS;
        else {
            int saved_card = in->profile->card;
            int saved_device = in->profile->device;
            in->profile->card = card;
            in->profile->device = device;
            ret_value = profile_read_device_info(in->profile) ? 0 : -EINVAL;
            if (ret_value != 0) {
                in->profile->card = saved_card;
                in->profile->device = saved_device;
            }
        }
    }

    device_unlock(in->adev);
    stream_unlock(&in->lock);

    return ret_value;
}

static char * in_get_parameters(const struct audio_stream *stream, const char *keys)
{
    struct stream_in *in = (struct stream_in *)stream;

    stream_lock(&in->lock);
    device_lock(in->adev);

    char * params_str =  device_get_parameters(in->profile, keys);

    device_unlock(in->adev);
    stream_unlock(&in->lock);

    return params_str;
}

static int in_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    return 0;
}

static int in_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    return 0;
}

static int in_set_gain(struct audio_stream_in *stream, float gain)
{
    return 0;
}

/* must be called with hw device and output stream mutexes locked */
static int start_input_stream(struct stream_in *in)
{
    ALOGD(USB_TAG"start_input_stream(card:%d device:%d)", in->profile->card, in->profile->device);
#ifdef MTK_AUDIO_DEBUG
    ALOGD(USB_TAG"usb:audio_hal::start_input_stream(card:%d device:%d)",
          in->profile->card, in->profile->device);
#endif

#ifdef MTK_AURISYS_FRAMEWORK_SUPPORT
    int ret = 0;
    ASSERT(AurisysRecordEnabled == false);
    //need to modify this restrict
    if ((in->proxy.alsa_config.channels <= 2) && (in->proxy.alsa_config.format != PCM_FORMAT_S24_3LE)) {
        int ret = proxy_open(&in->proxy);
        if (ret != 0) {
            return ret;
        }
        size_t ReadBufferSize = in_get_buffer_size((const struct audio_stream*)in) * (in->proxy.alsa_config.channels) / (in->hal_channel_count);
        AurisysRecordEnabled = init_primary_record(in, ReadBufferSize);
        ret = (AurisysRecordEnabled == true) ? 0 : 1;
        return ret;
    }
    else
#endif
    {
        ALOGD(USB_TAG"usb:audio_hal::start_input_stream: Not use Aurisys Record");
        return proxy_open(&in->proxy);
    }
}

/* TODO mutex stuff here (see out_write) */
static ssize_t in_read(struct audio_stream_in *stream, void* buffer, size_t bytes)
{
    size_t num_read_buff_bytes = 0;
    void * read_buff = buffer;
    void * out_buff = buffer;
    int ret = 0;
    int tryCount = 10;
    struct stream_in * in = (struct stream_in *)stream;

    while (mReadLockCount != 0 && tryCount--) {
        ALOGV("%s, free CPU, mReadLockCount = %d, tryCount %d", __FUNCTION__, mReadLockCount, tryCount);
        usleep(300);
        if (tryCount == 0) {
            ALOGD("%s, free CPU, mReadLockCount = %d, tryCount %d", __FUNCTION__, mReadLockCount, tryCount);
        }
    }
#ifdef MTK_AURISYS_FRAMEWORK_SUPPORT
    if (AurisysRecordReopen == true)
    {
        ALOGD(USB_TAG"+in_read(%zu), VOIP MODE, USB Reocrd should reopen...",bytes);
        ret = in_standby(&(stream->common));
        AurisysRecordReopen = false;
    }
#endif
    stream_lock(&in->lock);
    if (in->standby) {
        device_lock(in->adev);
        ret = start_input_stream(in);
        device_unlock(in->adev);
        if (ret != 0) {
            stream_unlock(&in->lock);
            ALOGD(USB_TAG"+in_read(%zu), start_input_stream, start_input_stream fail",bytes);
            usleep(bytes * 1000000 / audio_stream_in_frame_size(stream) /
                   in_get_sample_rate(&stream->common));
            memset(buffer, 0, bytes);
            //Dump debug data
            dumpPcmData(usbstreamin,(void*)buffer,num_read_buff_bytes,usbstreamin_propty);
            return bytes;
        }
        in->standby = false;
    }

    alsa_device_profile * profile = in->profile;

    /*
     * OK, we need to figure out how much data to read to be able to output the requested
     * number of bytes in the HAL format (16-bit, stereo).
     */
    num_read_buff_bytes = bytes;
    int num_device_channels = proxy_get_channel_count(&in->proxy); /* what we told Alsa */
    int num_req_channels = in->hal_channel_count; /* what we told AudioFlinger */

    if (num_device_channels != num_req_channels) {
        num_read_buff_bytes = (num_device_channels * num_read_buff_bytes) / num_req_channels;
    }

    /* Setup/Realloc the conversion buffer (if necessary). */
    if (num_read_buff_bytes != bytes) {
        if (num_read_buff_bytes > in->conversion_buffer_size) {
            /*TODO Remove this when AudioPolicyManger/AudioFlinger support arbitrary formats
              (and do these conversions themselves) */
            in->conversion_buffer_size = num_read_buff_bytes;
            in->conversion_buffer = realloc(in->conversion_buffer, in->conversion_buffer_size);
        }
        read_buff = in->conversion_buffer;
    }
#ifdef MTK_AURISYS_FRAMEWORK_SUPPORT
    if (AurisysRecordEnabled == true){
        ret = 0;
        num_read_buff_bytes = gAudioMTKAudioUSBDataClient->read(read_buff, num_read_buff_bytes);
    }
    else
#endif
    {
        ret = proxy_read(&in->proxy, read_buff, num_read_buff_bytes);
    }
    ALOGV(USB_TAG"ret = %d, bytes = %zu, num_read_buff_bytes = %zu",ret, bytes, num_read_buff_bytes);

    if ((ret == 0) && (num_read_buff_bytes > 0)) {
        if (num_device_channels != num_req_channels) {
            ALOGV(USB_TAG"chans dev:%d req:%d", num_device_channels, num_req_channels);

            out_buff = buffer;
            /* Num Channels conversion */
            audio_format_t audio_format = in_get_format(&(in->stream.common));
            unsigned sample_size_in_bytes = audio_bytes_per_sample(audio_format);
            num_read_buff_bytes =
                adjust_channels(read_buff, num_device_channels,
                                out_buff, num_req_channels,
                                sample_size_in_bytes, num_read_buff_bytes);
        }

        /* no need to acquire in->adev->lock to read mic_muted here as we don't change its state */
        if (num_read_buff_bytes > 0 && in->adev->mic_muted)
            memset(buffer, 0, num_read_buff_bytes);
    }else {
        num_read_buff_bytes = 0; // reset the value after USB headset is unplugged
       }

    stream_unlock(&in->lock);
    ALOGV(USB_TAG"audio_hal:usb in_read, bytes =%zu, num_read_buff_bytes=%zu", bytes,num_read_buff_bytes);
    //Dump debug data
    dumpPcmData(usbstreamin,(void*)buffer,num_read_buff_bytes,usbstreamin_propty);

    return num_read_buff_bytes;
}

static uint32_t in_get_input_frames_lost(struct audio_stream_in *stream)
{
    return 0;
}

static int adev_open_input_stream(struct audio_hw_device *hw_dev,
                                  audio_io_handle_t handle,
                                  audio_devices_t devicesSpec __unused,
                                  struct audio_config *config,
                                  struct audio_stream_in **stream_in,
                                  audio_input_flags_t flags __unused,
                                  const char *address /*__unused*/,
                                  audio_source_t source)
{
    ALOGD(USB_TAG"in adev_open_input_stream() rate:%" PRIu32 ", chanMask:0x%" PRIX32 ", fmt:%" PRIu8,
          config->sample_rate, config->channel_mask, config->format);
#ifdef MTK_AUDIO_DEBUG
    ALOGD("usb: in adev_open_input_stream() rate:%" PRIu32 ", chanMask:0x%" PRIX32 ", fmt:%" PRIu8,
          config->sample_rate, config->channel_mask, config->format);
#endif

    struct stream_in *in = (struct stream_in *)calloc(1, sizeof(struct stream_in));
    int ret = 0;

    if (in == NULL)
        return -ENOMEM;
    in->input_source = source;
    /* setup function pointers */
    in->stream.common.get_sample_rate = in_get_sample_rate;
    in->stream.common.set_sample_rate = in_set_sample_rate;
    in->stream.common.get_buffer_size = in_get_buffer_size;
    in->stream.common.get_channels = in_get_channels;
    in->stream.common.get_format = in_get_format;
    in->stream.common.set_format = in_set_format;
    in->stream.common.standby = in_standby;
    in->stream.common.dump = in_dump;
    in->stream.common.set_parameters = in_set_parameters;
    in->stream.common.get_parameters = in_get_parameters;
    in->stream.common.add_audio_effect = in_add_audio_effect;
    in->stream.common.remove_audio_effect = in_remove_audio_effect;

    in->stream.set_gain = in_set_gain;
    in->stream.read = in_read;
    in->stream.get_input_frames_lost = in_get_input_frames_lost;

    stream_lock_init(&in->lock);

    in->adev = (struct audio_device *)hw_dev;
    device_lock(in->adev);

    in->profile = &in->adev->in_profile;

    struct pcm_config proxy_config;
    memset(&proxy_config, 0, sizeof(proxy_config));

    /* Pull out the card/device pair */
    parse_card_device_params(address, &(in->profile->card), &(in->profile->device));

    profile_read_device_info(in->profile);

    /* Rate */
    if (config->sample_rate == 0) {
        config->sample_rate = profile_get_default_sample_rate(in->profile);
    }

    if (in->adev->device_sample_rate != 0 &&                 /* we are playing, so lock the rate */
        in->adev->device_sample_rate >= RATELOCK_THRESHOLD) {/* but only for high sample rates */
        ret = config->sample_rate != in->adev->device_sample_rate ? -EINVAL : 0;
        proxy_config.rate = config->sample_rate = in->adev->device_sample_rate;
    } else if (profile_is_sample_rate_valid(in->profile, config->sample_rate)) {
        in->adev->device_sample_rate = proxy_config.rate = config->sample_rate;
    } else {
        proxy_config.rate = config->sample_rate = profile_get_default_sample_rate(in->profile);
        ret = -EINVAL;
    }
    device_unlock(in->adev);

    /* Format */
    if (config->format == AUDIO_FORMAT_DEFAULT) {
        proxy_config.format = profile_get_default_format(in->profile);
        config->format = audio_format_from_pcm_format(proxy_config.format);
    } else {
        enum pcm_format fmt = pcm_format_from_audio_format(config->format);
        if (profile_is_format_valid(in->profile, fmt)) {
            proxy_config.format = fmt;
        } else {
            proxy_config.format = profile_get_default_format(in->profile);
            config->format = audio_format_from_pcm_format(proxy_config.format);
            ret = -EINVAL;
        }
    }

    /* Channels */
    bool calc_mask = false;
    if (config->channel_mask == AUDIO_CHANNEL_NONE) {
        /* query case */
        in->hal_channel_count = profile_get_default_channel_count(in->profile);
        calc_mask = true;
    } else {
        /* explicit case */
        in->hal_channel_count = audio_channel_count_from_in_mask(config->channel_mask);
    }

    /* The Framework is currently limited to no more than this number of channels */
    if (in->hal_channel_count > FCC_8) {
        in->hal_channel_count = FCC_8;
        calc_mask = true;
    }

    if (calc_mask) {
        /* need to calculate the mask from channel count either because this is the query case
         * or the specified mask isn't valid for this device, or is more then the FW can handle */
        in->hal_channel_mask = in->hal_channel_count <= FCC_2
            /* position mask for mono & stereo */
            ? audio_channel_in_mask_from_count(in->hal_channel_count)
            /* otherwise indexed */
            : audio_channel_mask_for_index_assignment_from_count(in->hal_channel_count);

        // if we change the mask...
        if (in->hal_channel_mask != config->channel_mask &&
            config->channel_mask != AUDIO_CHANNEL_NONE) {
            config->channel_mask = in->hal_channel_mask;
            ret = -EINVAL;
        }
    } else {
        in->hal_channel_mask = config->channel_mask;
    }

    if (ret == 0) {
        // Validate the "logical" channel count against support in the "actual" profile.
        // if they differ, choose the "actual" number of channels *closest* to the "logical".
        // and store THAT in proxy_config.channels
        proxy_config.channels =
                profile_get_closest_channel_count(in->profile, in->hal_channel_count);
        ret = proxy_prepare(&in->proxy, in->profile, &proxy_config);
        if (ret == 0) {
        in->standby = true;

        in->conversion_buffer = NULL;
        in->conversion_buffer_size = 0;

        *stream_in = &in->stream;

        /* Save this for adev_dump() */
        adev_add_stream_to_list(in->adev, &in->adev->input_stream_list, &in->list_node);
    } else {
            ALOGW("proxy_prepare error %d", ret);
            unsigned channel_count = proxy_get_channel_count(&in->proxy);
            config->channel_mask = channel_count <= FCC_2
                ? audio_channel_in_mask_from_count(channel_count)
                : audio_channel_mask_for_index_assignment_from_count(channel_count);
            config->format = audio_format_from_pcm_format(proxy_get_format(&in->proxy));
            config->sample_rate = proxy_get_sample_rate(&in->proxy);
        }
    }

    if (ret != 0) {
        // Deallocate this stream on error, because AudioFlinger won't call
        // adev_close_input_stream() in this case.
        *stream_in = NULL;
        free(in);
    }

    return ret;
}

static void adev_close_input_stream(struct audio_hw_device *hw_dev,
                                    struct audio_stream_in *stream)
{
    struct stream_in *in = (struct stream_in *)stream;
    ALOGV("adev_close_input_stream(c:%d d:%d)", in->profile->card, in->profile->device);

    adev_remove_stream_from_list(in->adev, &in->list_node);

    /* Close the pcm device */
    in_standby(&stream->common);

    free(in->conversion_buffer);

    free(stream);
}

/*
 * ADEV Functions
 */
static int adev_set_parameters(struct audio_hw_device *hw_dev, const char *kvpairs)
{
    return 0;
}

static char * adev_get_parameters(const struct audio_hw_device *hw_dev, const char *keys)
{
    return strdup("");
}

static int adev_init_check(const struct audio_hw_device *hw_dev)
{
    return 0;
}

static int adev_set_voice_volume(struct audio_hw_device *hw_dev, float volume)
{
    return -ENOSYS;
}

static int adev_set_master_volume(struct audio_hw_device *hw_dev, float volume)
{
    return -ENOSYS;
}

static int adev_set_mode(struct audio_hw_device *hw_dev, audio_mode_t mode)
{
    return 0;
}

static int adev_set_mic_mute(struct audio_hw_device *hw_dev, bool state)
{
    struct audio_device * adev = (struct audio_device *)hw_dev;
    device_lock(adev);
    adev->mic_muted = state;
    device_unlock(adev);
    return -ENOSYS;
}

static int adev_get_mic_mute(const struct audio_hw_device *hw_dev, bool *state)
{
    return -ENOSYS;
}

static int adev_dump(const struct audio_hw_device *device, int fd)
{
    dprintf(fd, "\nUSB audio module:\n");

    struct audio_device* adev = (struct audio_device*)device;
    const int kNumRetries = 3;
    const int kSleepTimeMS = 500;

    // use device_try_lock() in case we dumpsys during a deadlock
    int retry = kNumRetries;
    while (retry > 0 && device_try_lock(adev) != 0) {
      sleep(kSleepTimeMS);
      retry--;
    }

    if (retry > 0) {
        if (list_empty(&adev->output_stream_list)) {
            dprintf(fd, "  No output streams.\n");
        } else {
            struct listnode* node;
            list_for_each(node, &adev->output_stream_list) {
                struct audio_stream* stream =
                        (struct audio_stream *)node_to_item(node, struct stream_out, list_node);
                out_dump(stream, fd);
            }
        }

        if (list_empty(&adev->input_stream_list)) {
            dprintf(fd, "\n  No input streams.\n");
        } else {
            struct listnode* node;
            list_for_each(node, &adev->input_stream_list) {
                struct audio_stream* stream =
                        (struct audio_stream *)node_to_item(node, struct stream_in, list_node);
                in_dump(stream, fd);
            }
        }

        device_unlock(adev);
    } else {
        // Couldn't lock
        dprintf(fd, "  Could not obtain device lock.\n");
    }

    return 0;
}

static int adev_close(hw_device_t *device)
{
#ifdef MTK_AURISYS_FRAMEWORK_SUPPORT
    if (AudioPrimaryHAL != NULL) {
        dlclose(AudioPrimaryHAL);
        AudioPrimaryHAL = NULL;
        AurisysEnabled = false;
    }
#endif
    struct audio_device *adev = (struct audio_device *)device;
    free(device);

    return 0;
}

static int adev_open(const hw_module_t* module, const char* name, hw_device_t** device)
{
    if (strcmp(name, AUDIO_HARDWARE_INTERFACE) != 0)
        return -EINVAL;

    struct audio_device *adev = (struct audio_device*) calloc(1, sizeof(struct audio_device));

    if (!adev)
        return -ENOMEM;

    ALOGD(USB_TAG"adev_open");
    profile_init(&adev->out_profile, PCM_OUT);
    profile_init(&adev->in_profile, PCM_IN);

    list_init(&adev->output_stream_list);
    list_init(&adev->input_stream_list);

    adev->hw_device.common.tag = HARDWARE_DEVICE_TAG;
    adev->hw_device.common.version = AUDIO_DEVICE_API_VERSION_2_0;
    adev->hw_device.common.module = (struct hw_module_t *)module;
    adev->hw_device.common.close = adev_close;

    adev->hw_device.init_check = adev_init_check;
    adev->hw_device.set_voice_volume = adev_set_voice_volume;
    adev->hw_device.set_master_volume = adev_set_master_volume;
    adev->hw_device.set_mode = adev_set_mode;
    adev->hw_device.set_mic_mute = adev_set_mic_mute;
    adev->hw_device.get_mic_mute = adev_get_mic_mute;
    adev->hw_device.set_parameters = adev_set_parameters;
    adev->hw_device.get_parameters = adev_get_parameters;
    adev->hw_device.get_input_buffer_size = adev_get_input_buffer_size;
    adev->hw_device.open_output_stream = adev_open_output_stream;
    adev->hw_device.close_output_stream = adev_close_output_stream;
    adev->hw_device.open_input_stream = adev_open_input_stream;
    adev->hw_device.close_input_stream = adev_close_input_stream;
    adev->hw_device.dump = adev_dump;

    *device = &adev->hw_device.common;

#ifdef MTK_AURISYS_FRAMEWORK_SUPPORT
        ASSERT(AurisysEnabled == false);
        AurisysEnabled = in_load_primary_hal();
        ASSERT(AurisysEnabled == true);
        ALOGD(USB_TAG"adev_open : AurisysEnabled = %d", AurisysEnabled);
#endif

    return 0;
}

static struct hw_module_methods_t hal_module_methods = {
    .open = adev_open,
};

struct audio_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = AUDIO_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = AUDIO_HARDWARE_MODULE_ID,
        .name = "USB audio HW HAL",
        .author = "The Android Open Source Project",
        .methods = &hal_module_methods,
    },
};
}
}
