#include "AudioAurisysPcmDump.h"

#include <string.h>

#include <sys/stat.h> /* for mkdir */
#include <sys/prctl.h> /* operations on a process */

#include <audio_log.h>
#include <audio_memory_control.h>




#define MIN_PCM_DUMP_SIZE (MIN_PCM_DUMP_CHUNK*4)
#define MIN_PCM_DUMP_CHUNK (8192)
#define PCM_TIME_OUT_SEC (1)

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "AudioAurisysPcmDump"


static int AudiocheckAndCreateDirectory(const char *pC) {
    char tmp[128];
    int i = 0;
    while (*pC) {
        tmp[i] = *pC;
        if (*pC == '/' && i) {
            tmp[i] = '\0';
            if (access(tmp, F_OK) != 0) {
                if (mkdir(tmp, 0770) == -1) {
                    AUD_LOG_E("AudioDumpPCM: mkdir error!");
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

static void *PCMDumpThread(void *arg) {
    pthread_detach(pthread_self());
    prctl(PR_SET_NAME, (unsigned long)__FUNCTION__, 0, 0, 0);
    struct PcmDump_t *self = (struct PcmDump_t *)arg;

    //AUD_LOG_D("PCMDumpThread start self->mPthreadEnable = %d", self->mPthreadEnable);

    unsigned int CopySize = 0;
    char *pLinearBuf = (char *)AUDIO_MALLOC(self->mLineaBufSize);
    while (self->mPthreadEnable == true) {
        pthread_mutex_lock(&self->mPCMDataNotifyMutex);
        CopySize = audio_ringbuf_count(&self->mRingbuffer);
        //AUD_LOG_D("%s fwrite CopySize = %d mLineaBufSize = %d", __FUNCTION__, CopySize, self->mLineaBufSize);

        while (CopySize >= self->mLineaBufSize) {
            audio_ringbuf_copy_to_linear(pLinearBuf, &self->mRingbuffer,  self->mLineaBufSize);
            if (self->mFilep) {
                fwrite((void *)pLinearBuf, 1, self->mLineaBufSize, self->mFilep);
                //AUD_LOG_D("%s fwrite CopySize = %d self->mLineaBufSize = %d", __FUNCTION__, CopySize,self->mLineaBufSize);
            }
            CopySize -= self->mLineaBufSize;
        }

        struct timeval now;
        struct timespec timeout;
        gettimeofday(&now, NULL);
        timeout.tv_sec = now.tv_sec + 3;
        timeout.tv_nsec = now.tv_usec * 1000;
        pthread_cond_timedwait(&self->mPCMDataNotifyEvent, &self->mPCMDataNotifyMutex, &timeout);
        pthread_mutex_unlock(&self->mPCMDataNotifyMutex);
    }

    //AUD_LOG_D("PCMDumpThread exit hPCMDumpThread=%p", &self->hPCMDumpThread);

    pthread_mutex_lock(&self->mPCMDumpMutex);
    CopySize = audio_ringbuf_count(&self->mRingbuffer);
    if (CopySize >= self->mLineaBufSize) {
        CopySize = self->mLineaBufSize;
    }
    audio_ringbuf_copy_to_linear(pLinearBuf, &self->mRingbuffer, CopySize);
    fwrite((void *)pLinearBuf, 1, CopySize, self->mFilep);
    pthread_mutex_unlock(&self->mPCMDumpMutex);

    if (pLinearBuf) {
        AUDIO_FREE(pLinearBuf);
        pLinearBuf = NULL;
    }

    //AUD_LOG_D("PCMDumpThread  mPCMDataNotifyEvent ");
    pthread_cond_signal(&self->mPCMDataNotifyEvent);  // notify done
    pthread_exit(NULL);
    return 0;
}


static int  AudioOpendumpPCMFile(struct PcmDump_t *self, const char *filepath) {
    int ret;
    AUD_LOG_D("%s filepath = %s", __FUNCTION__, filepath);
    ret = AudiocheckAndCreateDirectory(filepath);
    if (ret < 0) {
        AUD_LOG_E("AudioOpendumpPCMFile dumpPCMData checkAndCreateDirectory() fail!!!");
    } else {
        self->mFilep = fopen(filepath, "wb");
        if (self->mFilep  != NULL) {
            pthread_mutex_lock(&self->mPCMDumpMutex);
            if (self->mPthreadEnable == false) {
                //create PCM data dump thread here
                int ret = 0;
                self->mPthreadEnable = true;
                ret = pthread_create(&self->hPCMDumpThread, NULL, PCMDumpThread, self);// with self arg
                if (ret != 0) {
                    AUD_LOG_E("hPCMDumpThread create fail!!!");
                } else {
                    AUD_LOG_D("hPCMDumpThread=%p created", &self->hPCMDumpThread);
                }
            }
            pthread_mutex_unlock(&self->mPCMDumpMutex);
        } else {
            AUD_LOG_D("%s create thread fail", __FUNCTION__);
        }
    }
    return ret;
}

static int AudioCloseDumpPCMFile(struct PcmDump_t *self) {
    AUD_LOG_D("%s", __FUNCTION__);
    int ret = 0;
    /* close thread */
    if (self->mPthreadEnable == true) {
        AUD_LOG_D("+%s pthread_mutex_lock", __FUNCTION__);
        pthread_mutex_lock(&self->mPCMDumpMutex);
        AUD_LOG_D("-%s pthread_mutex_lock", __FUNCTION__);
        self->mPthreadEnable = false;

        struct timeval now;
        struct timespec timeout;
        gettimeofday(&now, NULL);
        timeout.tv_sec = now.tv_sec + 1;
        timeout.tv_nsec = now.tv_usec * 1000;
        AUD_LOG_D("+%s pthread_cond_timedwait", __FUNCTION__);
        pthread_cond_signal(&self->mPCMDataNotifyEvent);  // notify done
        pthread_cond_timedwait(&self->mPCMDataNotifyEvent, &self->mPCMDumpMutex, &timeout);
        AUD_LOG_D("-%s pthread_cond_timedwait", __FUNCTION__);
        pthread_mutex_unlock(&self->mPCMDumpMutex);
    }

    /*  destroy */
    ret = pthread_mutex_destroy(&self->mPCMDataNotifyMutex);
    ret = pthread_mutex_destroy(&self->mPCMDumpMutex);
    ret = pthread_cond_destroy(&self->mPCMDataNotifyEvent);

    if (self->mFilep) {
        fclose(self->mFilep);
    }

    /* free rinbuffer*/
    if (self->mRingbuffer.base) {
        AUDIO_FREE(self->mRingbuffer.base);
        self->mRingbuffer.base = NULL;
    }

    self->mRingbuffer.size = 0;
    self->mRingbuffer.read = NULL;
    self->mRingbuffer.write = NULL;
    AUD_LOG_D("%s", __FUNCTION__);
    return 0;
}

static void AudioDumpPCMData(struct PcmDump_t *self, void *buffer, uint32_t bytes) {
    unsigned int CopySize = 0;
    if (self->mPthreadEnable) {
        pthread_mutex_lock(&self->mPCMDataNotifyMutex);
        CopySize  = audio_ringbuf_free_space(&self->mRingbuffer);

        if (CopySize < bytes) {
            AUD_LOG_D("warning ... AudioDumpPCMData CopySize = %d bytes = %d", CopySize, bytes);
            bytes = CopySize;
        }
        audio_ringbuf_copy_from_linear(&self->mRingbuffer, (const char *)buffer, bytes);
        pthread_cond_broadcast(&self->mPCMDataNotifyEvent);
        pthread_mutex_unlock(&self->mPCMDataNotifyMutex);
    }
}

void InitPcmDump_t(struct PcmDump_t *self, unsigned int size) {
    AUD_LOG_D("%s size = %d", __FUNCTION__, size);
    int ret = 0;
    self->AudioOpendumpPCMFile  = AudioOpendumpPCMFile;
    self->AudioCloseDumpPCMFile = AudioCloseDumpPCMFile;
    self->AudioDumpPCMData = AudioDumpPCMData;
    self->mFilep = NULL;
    self->hPCMDumpThread = 0;
    self->mPthreadEnable = false;
    ret = pthread_mutex_init(&self->mPCMDataNotifyMutex, NULL);
    ret = pthread_mutex_init(&self->mPCMDumpMutex, NULL);
    ret = pthread_cond_init(&self->mPCMDataNotifyEvent, NULL);

    /* init ring buffer*/
    if (size < 32768) {
        size = 32768;
    }
    self->mRingbuffer.base = (char *)AUDIO_MALLOC(size);
    memset((void *) self->mRingbuffer.base, 0, size);
    self->mRingbuffer.size = size;
    self->mRingbuffer.read = self->mRingbuffer.base;
    self->mRingbuffer.write = self->mRingbuffer.base;
    self->mLineaBufSize = MIN_PCM_DUMP_CHUNK;
}


