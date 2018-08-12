/*
 * Copyright (C) 2011 The Android Open Source Project
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

//#ifdef MTK_VOW_SUPPORT

#define LOG_TAG "sound_trigger_hw_default"
/*#define LOG_NDEBUG 0*/


#include <errno.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <cutils/log.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <hardware/hardware.h>
#include <system/sound_trigger.h>
#include <hardware/sound_trigger.h>
#include <voiceunlock/vowAPI_AP.h>
#include <media/AudioSystem.h>
#include <cutils/properties.h>
#include "soundtrigger.h"
#include "hardware_legacy/AudioMTKHardwareInterface.h"
#include <unistd.h>
#include <soundtriggerAssert.h>


using namespace android;
#define HAL_LIBRARY_PATH1 "/system/lib/hw"
#define HAL_LIBRARY_PATH2 "/vendor/lib/hw"
#define AUDIO_HAL_PREFIX "audio.primary"
#define PLATFORM_ID "ro.board.platform"
#define BOARD_PLATFORM_ID "ro.board.platform"
static AudioMTKHardwareInterface *gAudioMTKHardware = NULL;
static void *AudioHwhndl = NULL;
#define ALLOCATE_MEMORY_SIZE_MAX 50 * 1024 // the max memory size can be allocated

static int soundtrigger_dlopen()
{
    if (AudioHwhndl == NULL) {
        char prop[PATH_MAX];
        char path[PATH_MAX];
        do {
            if (property_get(PLATFORM_ID, prop, NULL) == 0) {
                snprintf(path, sizeof(path), "%s/%s.default.so",
                         HAL_LIBRARY_PATH1, prop);
                if (access(path, R_OK) == 0) { break; }


                if (access(path, R_OK) == 0) { break; }
            } else {
                snprintf(path, sizeof(path), "%s/%s.%s.so",
                         HAL_LIBRARY_PATH1, AUDIO_HAL_PREFIX, prop);
                if (access(path, R_OK) == 0) { break; }

                snprintf(path, sizeof(path), "%s/%s.%s.so",
                         HAL_LIBRARY_PATH2, AUDIO_HAL_PREFIX, prop);
                if (access(path, R_OK) == 0) { break; }

                if (property_get(BOARD_PLATFORM_ID, prop, NULL) == 0) {
                    snprintf(path, sizeof(path), "%s/%s.default.so",
                             HAL_LIBRARY_PATH1, prop);
                    if (access(path, R_OK) == 0) { break; }

                    snprintf(path, sizeof(path), "%s/%s.default.so",
                             HAL_LIBRARY_PATH2, prop);
                    if (access(path, R_OK) == 0) { break; }
                } else {
                    snprintf(path, sizeof(path), "%s/%s.%s.so",
                             HAL_LIBRARY_PATH1, AUDIO_HAL_PREFIX, prop);
                    if (access(path, R_OK) == 0) { break; }

                    snprintf(path, sizeof(path), "%s/%s.%s.so",
                             HAL_LIBRARY_PATH2, AUDIO_HAL_PREFIX, prop);
                    if (access(path, R_OK) == 0) { break; }
                }
            }
        } while (0);

        ALOGD("Load %s", path);
        AudioHwhndl = dlopen(path, RTLD_NOW);
        if (AudioHwhndl == NULL) {
            ALOGE("-DL open AudioHwhndl path [%s] fail", path);
            return -ENOSYS;
        } else {
            create_AudioMTKHw *func1 = (create_AudioMTKHw *)dlsym(AudioHwhndl, "createMTKAudioHardware");
            ALOGD("%s %d func1 %p", __FUNCTION__, __LINE__, func1);
            const char *dlsym_error1 = dlerror();
            if (func1 == NULL) {
                ALOGE("-dlsym createMTKAudioHardware fail");
                dlclose(AudioHwhndl);
                AudioHwhndl = NULL;
                return -ENOSYS;
            }
            gAudioMTKHardware = func1();
            ALOGD("dlopen success gAudioMTKHardware");
        }
    }
    return 0;
}

static const struct sound_trigger_properties hw_properties = {
        "The Soundtrigger HAL Project", // implementor
        "Sound Trigger stub HAL", // description
        1, // version
        { 0xed7a7d60, 0xc65e, 0x11e3, 0x9be4, { 0x00, 0x02, 0xa5, 0xd5, 0xc5, 0x1b } }, // uuid
        1, // max_sound_models
        1, // max_key_phrases
        1, // max_users
        RECOGNITION_MODE_VOICE_TRIGGER, // recognition_modes
        true, // capture_transition
        0, // max_buffer_ms
        false, // concurrent_capture
        false, // trigger_in_event
        0 // power_consumption_mw
};

struct stub_sound_trigger_device {
    struct sound_trigger_hw_device device;
    sound_model_handle_t model_handle;
    recognition_callback_t recognition_callback;
    void *recognition_cookie;
    sound_model_callback_t sound_model_callback;
    void *sound_model_cookie;
    pthread_t callback_thread;
    pthread_mutex_t lock;
    pthread_cond_t  cond;
};

enum vow_model_type {
    VOW_SPEAKER_MODE,
    VOW_INITIAL_MODE,
    VOW_MODEL_MODE_NUM
};

enum voice_wakeup {
    VOICE_UNLOCK,
    VOICE_WAKEUP_NO_RECOGNIZE,
    VOICE_WAKEUP_RECOGNIZE,
    VOICE_WAKE_UP_MODE_NUM
};

int mFd = -1;
int mphrase_extrasid = -1;
int stoprecognition = -1;
int sound_trigger_running_state = -1;

struct vow_eint_data_struct_t m_sINTData;

static void *callback_thread_loop(void *context)
{
    struct stub_sound_trigger_device *stdev = (struct stub_sound_trigger_device *)context;
    bool excute_recognize_pass = false;

    ALOGI("%s", __func__);
    prctl(PR_SET_NAME, (unsigned long)"sound trigger callback", 0, 0, 0);
    pthread_mutex_lock(&stdev->lock);
    if (stdev->recognition_callback == NULL) {
        ALOGI("return: recognition_callback = NULL");
        pthread_mutex_unlock(&stdev->lock);
        goto exit;
    }
    if (mFd < 0) {
        mFd = open("/dev/vow", O_RDONLY);
    }
    ALOGI("mFd%d", mFd);
    if (mFd < 0) {
        ALOGI("open device fail!%s\n", strerror(errno));
    }
    pthread_mutex_unlock(&stdev->lock);
    if (mFd >= 0) {
        m_sINTData.eint_status = -1;
        ALOGI("1.VoiceWakeup interrupt status, status:%d ,stoprecognition:%d, sound_trigger_running_state:%d",
               m_sINTData.eint_status, stoprecognition, sound_trigger_running_state);
        while (!stoprecognition) {
            int returnvalue = read(mFd, &m_sINTData, sizeof(struct vow_eint_data_struct_t));

            ALOGI("2.VoiceWakeup interrupt status, status:%d, stoprecognition:%d",
                  m_sINTData.eint_status, stoprecognition);
            if (m_sINTData.eint_status == 0) {
                char *data;
                struct sound_trigger_phrase_recognition_event *event;

                excute_recognize_pass = true;
                if (excute_recognize_pass) {
                    pthread_mutex_lock(&stdev->lock);
                    data = (char *)calloc(1, sizeof(struct sound_trigger_phrase_recognition_event) + 1);
                    event = (struct sound_trigger_phrase_recognition_event *)data;
                    event->common.status = RECOGNITION_STATUS_SUCCESS;
                    event->common.type = SOUND_MODEL_TYPE_KEYPHRASE;
                    event->common.model = stdev->model_handle;
                    event->common.capture_available = true;
                    event->common.audio_config = AUDIO_CONFIG_INITIALIZER;
                    event->common.audio_config.sample_rate = 16000;
                    event->common.audio_config.channel_mask = AUDIO_CHANNEL_IN_MONO;
                    event->common.audio_config.format = AUDIO_FORMAT_PCM_16_BIT;
                    event->num_phrases = 1;
                    event->phrase_extras[0].id = mphrase_extrasid;
                    event->phrase_extras[0].recognition_modes = RECOGNITION_MODE_VOICE_TRIGGER;
                    event->phrase_extras[0].confidence_level = 100;
                    event->phrase_extras[0].num_levels = 1;
                    event->phrase_extras[0].levels[0].level = 100;
                    event->phrase_extras[0].levels[0].user_id = 0;
                    event->common.data_offset = sizeof(struct sound_trigger_phrase_recognition_event);
                    event->common.data_size = 1;
                    if (stdev->recognition_callback != NULL) {
                        stdev->recognition_callback(&event->common, stdev->recognition_cookie);
                    }
                    ALOGI("phrase_extras[0].id %d", mphrase_extrasid);
                    ALOGI("capture_available %d", event->common.capture_available);
                    ALOGI("%s send callback model %d", __func__, stdev->model_handle);
                    sound_trigger_running_state = 0;
                    free(data);
                    stdev->recognition_callback = NULL;
                    stdev->callback_thread = 0;
                    pthread_mutex_unlock(&stdev->lock);
                    pthread_exit(NULL);
                    break;
                }
            } else if ((m_sINTData.eint_status == -2) && (stoprecognition == 1)) {
                pthread_mutex_lock(&stdev->lock);
                ALOGD("sound_trigger callback m_sINTData.eint_status %d stoprecognition %d",
                      m_sINTData.eint_status, stoprecognition);
                sound_trigger_running_state = 0;
                stdev->recognition_callback = NULL;
                stdev->callback_thread = 0;
                pthread_mutex_unlock(&stdev->lock);
                pthread_exit(NULL);
                break;
            }
        }
    }
exit:
    pthread_mutex_lock(&stdev->lock);
    sound_trigger_running_state = 0;
    stdev->recognition_callback = NULL;
    stdev->callback_thread = 0;
    ALOGI("Exit sound_trigger callback_thread_loop");
    pthread_mutex_unlock(&stdev->lock);
    return NULL;
}

static int stdev_get_properties(const struct sound_trigger_hw_device *dev,
                                struct sound_trigger_properties *properties)
{
    struct stub_sound_trigger_device *stdev = (struct stub_sound_trigger_device *)dev;

    ALOGI("%s", __func__);
    if (properties == NULL)
        return -EINVAL;
    memcpy(properties, &hw_properties, sizeof(struct sound_trigger_properties));
    return 0;
}

static int stdev_load_sound_model(const struct sound_trigger_hw_device *dev,
                                  struct sound_trigger_sound_model *sound_model,
                                  sound_model_callback_t callback,
                                  void *cookie,
                                  sound_model_handle_t *handle)
{
    struct stub_sound_trigger_device *stdev = (struct stub_sound_trigger_device *)dev;
    int status = 0;
    sound_trigger_uuid_t uuid;

    pthread_mutex_lock(&stdev->lock);
    if (handle == NULL || sound_model == NULL) {
        status = -EINVAL;
        pthread_mutex_unlock(&stdev->lock);
        return status;
    }
    if (sound_model->data_size == 0 ||
        sound_model->data_offset < sizeof(struct sound_trigger_sound_model)) {
        status = -EINVAL;
        pthread_mutex_unlock(&stdev->lock);
        return status;
    }
    if (sound_model->type == SOUND_MODEL_TYPE_GENERIC) {
        ALOGI("SOUND_MODEL_TYPE_GENERIC is not allow for soundtrigger HAL return");
        status = -EINVAL;
        pthread_mutex_unlock(&stdev->lock);
        return status;
    }
    uuid=sound_model->uuid;
    ALOGI("SOUND model type %d",sound_model->type);
    ALOGI("Start Load Model");
    ALOGI("uuid.timeHi: %d",uuid.timeHiAndVersion);
    ALOGI("%s stdev %p", __func__, stdev);
    if (stdev->model_handle == 1) {
        status = -ENOSYS;
        pthread_mutex_unlock(&stdev->lock);
        return status;
    }
    char *data = (char *)sound_model + sound_model->data_offset;
    ALOGI("Load model: data size %d data1 %d - %d",
          sound_model->data_size, data[0], data[sound_model->data_size - 1]);
    stdev->model_handle = 1;
    stdev->recognition_callback = NULL;
    stdev->sound_model_callback = callback;
    stdev->sound_model_cookie = cookie;

    *handle = stdev->model_handle;

    //load initial vow
    ALOGI("Initial VOW");
    if (mFd < 0) {
        mFd = open("/dev/vow", O_RDONLY );
    }
    if (mFd < 0) {
        ALOGI("open device fail!%s\n", strerror(errno));
    } else {
        if (ioctl(mFd, VOW_SET_CONTROL, (unsigned long)VOWControlCmd_Init) < 0) {
            ALOGI("IOCTL VOW_SET_CONTROL ERR");
        }
    }
    ALOGI("Start Load init Model");
    if (uuid.node[0]=='M'
     && uuid.node[1]=='T'
     && uuid.node[2]=='K'
     && uuid.node[3]=='I'
     && uuid.node[4]=='N'
     && uuid.node[5]=='C') {
        //load init model
        struct VOW_SoungtriggerTestInfo vow_info;
        vow_info.rawModelFileSizeInByte = sound_model->data_size;
        vow_info.rawModelFilePtr=data;
        ALOGI("Start init vowGetModelSize_Wrap");
        getSizes(&vow_info);
        if (vow_info.rtnModelSize > ALLOCATE_MEMORY_SIZE_MAX) {
            ALOGI("the memory size need to allocate is more than 50K!!!");
            status = -EINVAL;
            pthread_mutex_unlock(&stdev->lock);
            return status;
        }
        if (vow_info.rtnModelSize > 0) {
            char *pModelInfo = new char[vow_info.rtnModelSize];
            if (pModelInfo == NULL) {
                ALOGI("setVoiceUBMFile allocate memory fail!!!");
                status = -EINVAL;
                pthread_mutex_unlock(&stdev->lock);
                return status;
            }
            vow_info.rtnModel = pModelInfo;
            ALOGI("Start init vowTestingInitAP_Wrap");
            TestingInitAP(&vow_info);
            ALOGI("Start Load speaker Model to VOWKernel");
            struct vow_model_info_t update_model_info;
            if (mFd < 0) {
                mFd = open("/dev/vow", O_RDONLY );
            }
            if (mFd < 0) {
                ALOGI("open device fail!%s\n", strerror(errno));
            } else {
                update_model_info.id    = 0;
                update_model_info.size  = (long)vow_info.rtnModelSize;
                update_model_info.addr  = (long)vow_info.rtnModel;
                if (ioctl(mFd, VOW_SET_SPEAKER_MODEL, (unsigned long)&update_model_info) < 0) {
                    ALOGI("IOCTL VOW_SET_SPEAKER_MODEL ERR");
                }
            }
            delete[] pModelInfo;
            ALOGI("Load speaker Model Finish");
        }
    } else {
        if (mFd < 0) {
            mFd = open("/dev/vow", O_RDONLY );
        }
        if (mFd < 0) {
            ALOGI("open device fail!%s\n", strerror(errno));
        } else {
            struct vow_model_info_t update_model_info;
            update_model_info.id    = 0;
            update_model_info.size  = (long)sound_model->data_size;
            update_model_info.addr  = (long)data;
            if (ioctl(mFd, VOW_SET_SPEAKER_MODEL, (unsigned long)&update_model_info) < 0) {
                ALOGI("IOCTL VOW_SET_SPEAKER_MODEL ERR");
            }
            ALOGI("Load 3rd mode Finish");
        }
    }
    pthread_mutex_unlock(&stdev->lock);
    return status;
}

static int stdev_unload_sound_model(const struct sound_trigger_hw_device *dev,
                                    sound_model_handle_t handle)
{
    struct stub_sound_trigger_device *stdev = (struct stub_sound_trigger_device *)dev;
    int status = 0;
    int cnt = 0;

    ALOGI("Start Unload Model");
    ALOGI("%s handle %d", __func__, handle);
    pthread_mutex_lock(&stdev->lock);
    if (handle != 1) {
        status = -EINVAL;
        goto exit;
    }
    if (stdev->model_handle == 0) {
        status = -ENOSYS;
        goto exit;
    }
    stdev->model_handle = 0;
    if (mFd < 0) {
        mFd = open("/dev/vow", O_RDONLY );
    }
    if (mFd < 0) {
        ALOGI("open device fail!%s", strerror(errno));
    } else {
        if (ioctl(mFd, VOW_CLR_SPEAKER_MODEL, 0) < 0) {
            ALOGI("IOCTL VOW_CLR_SPEAKER_MODEL ERR");
        }
    }
    if (stdev->recognition_callback != NULL) {
        stdev->recognition_callback = NULL;
        pthread_mutex_unlock(&stdev->lock);
        cnt = 0;
        ALOGI("+start wait pth over");
        while (sound_trigger_running_state == 1) {
            usleep(500);
            cnt++;
            ASSERT(cnt < 2000);
        }
        ALOGI("-start wait pth over");
        pthread_mutex_lock(&stdev->lock);
    }
    ALOGI("Unload speaker Model Finish");

exit:
    pthread_mutex_unlock(&stdev->lock);
    return status;
}

static int stdev_start_recognition(const struct sound_trigger_hw_device *dev,
                                   sound_model_handle_t sound_model_handle,
                                   const struct sound_trigger_recognition_config *config,
                                   recognition_callback_t callback,
                                   void *cookie)
{
    struct stub_sound_trigger_device *stdev = (struct stub_sound_trigger_device *)dev;
    int status = 0;
    int cnt = 0;

    ALOGI("Start stdev_start_recognition");
    ALOGI("%s sound model %d", __func__, sound_model_handle);
    pthread_mutex_lock(&stdev->lock);
    if (stdev->model_handle != sound_model_handle) {
        ALOGI("ERROR_1");
        status = -ENOSYS;
        goto exit;
    }
    if (stdev->recognition_callback != NULL) {
        ALOGI("ERROR_2");
        status = -ENOSYS;
        goto exit;
    }
    ALOGI("Start setParameters");
    status = soundtrigger_dlopen();
    if (status == -ENOSYS) {
        ALOGI("%s, dlopen error", __func__);
        goto exit;
    }
    gAudioMTKHardware->setParameters(String8("MTK_VOW_ENABLE=1"));
    if (config->data_size != 0) {
        char *data = (char *)config + config->data_offset;

        ALOGI("%s data size %d data %d - %d", __func__,
              config->data_size, data[0], data[config->data_size - 1]);
    }
    mphrase_extrasid=config->phrases[0].id;
    ALOGI("startrecognition phrase_extras[0].id %d", mphrase_extrasid);
    stdev->recognition_callback = callback;
    stdev->recognition_cookie = cookie;
    ALOGI("stoprecognition: %d, sound_trigger_running_state:%d",
          stoprecognition, sound_trigger_running_state);
    if ((stoprecognition == 1) && (sound_trigger_running_state != 0)) {
        ALOGI("+ startrecognition wait for thread end ");
        pthread_mutex_unlock(&stdev->lock);
        while (sound_trigger_running_state != 0) {
            usleep(500);
            cnt++;
            ASSERT(cnt < 2000);
        }
        pthread_mutex_lock(&stdev->lock);
        ALOGI("- startrecognition wait for thread end ");
    }
    stoprecognition = 0;
    if (sound_trigger_running_state != 1) {
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_create(&stdev->callback_thread, &attr,
                        callback_thread_loop, stdev);
        pthread_attr_destroy(&attr);

        sound_trigger_running_state = 1;
    }
exit:
    pthread_mutex_unlock(&stdev->lock);
    return status;
}

static int stdev_stop_recognition(const struct sound_trigger_hw_device *dev,
                                 sound_model_handle_t sound_model_handle)
{
    struct stub_sound_trigger_device *stdev = (struct stub_sound_trigger_device *)dev;
    int status = 0;
    int cnt = 0;

    pthread_mutex_lock(&stdev->lock);
    if (stdev->model_handle != sound_model_handle) {
        status = -ENOSYS;
        goto exit;
    }
    if (stdev->recognition_callback == NULL) {
        status = -ENOSYS;
        goto exit;
    }
    ALOGI("+soundtrigger stop recognition");
    status = soundtrigger_dlopen();
    if (status == -ENOSYS) {
        ALOGI("%s, dlopen error", __func__);
        goto exit;
    }
    stoprecognition = 1;
    gAudioMTKHardware->setParameters(String8("MTK_VOW_ENABLE=0"));
    ALOGI("stdev_stop_recognition: sound model %d",sound_model_handle);
    if (stdev->recognition_callback != NULL) {
        stdev->recognition_callback = NULL;
    }
    pthread_mutex_unlock(&stdev->lock);
    ALOGI("start to call VOW_CHECK_STATUS");
    ioctl(mFd, VOW_CHECK_STATUS, 0);
    ALOGI("+start wait pth over");
    cnt = 0;
    while (sound_trigger_running_state == 1) {
        usleep(500);
        cnt++;
        ASSERT(cnt < 2000);
    }
    ALOGI("-start wait pth over");
    pthread_mutex_lock(&stdev->lock);
exit:
    pthread_mutex_unlock(&stdev->lock);
    ALOGI("-soundtrigger stop recognition");
    return status;
}


static int stdev_close(hw_device_t *device)
{
    free(device);
    if (AudioHwhndl != NULL) {
        dlclose(AudioHwhndl);
        AudioHwhndl = NULL;
        gAudioMTKHardware = NULL;
    }
    return 0;
}

static int stdev_open(const hw_module_t* module, const char* name,
                     hw_device_t** device)
{
    struct stub_sound_trigger_device *stdev;
    int ret;

    if (strcmp(name, SOUND_TRIGGER_HARDWARE_INTERFACE) != 0)
        return -EINVAL;

    stdev = (struct stub_sound_trigger_device *)calloc(1, sizeof(struct stub_sound_trigger_device));
    if (!stdev)
        return -ENOMEM;

    stdev->device.common.tag = HARDWARE_DEVICE_TAG;
    stdev->device.common.version = SOUND_TRIGGER_DEVICE_API_VERSION_1_0;
    stdev->device.common.module = (struct hw_module_t *) module;
    stdev->device.common.close = stdev_close;
    stdev->device.get_properties = stdev_get_properties;
    stdev->device.load_sound_model = stdev_load_sound_model;
    stdev->device.unload_sound_model = stdev_unload_sound_model;
    stdev->device.start_recognition = stdev_start_recognition;
    stdev->device.stop_recognition = stdev_stop_recognition;

    pthread_mutex_init(&stdev->lock, (const pthread_mutexattr_t *) NULL);

    *device = &stdev->device.common;
    return 0;
}

static struct hw_module_methods_t hal_module_methods = {
    .open = stdev_open,
};

struct sound_trigger_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = SOUND_TRIGGER_MODULE_API_VERSION_1_0,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = SOUND_TRIGGER_HARDWARE_MODULE_ID,
        .name = "MTK Audio HW HAL",
        .author = "MTK",
        .methods = &hal_module_methods,
        .dso = NULL,
        .reserved = {0},
    },
};

//#endif
