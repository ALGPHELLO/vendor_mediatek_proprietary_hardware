#ifndef AUDIO_AURISYS_PCM_DUMP_H
#define AUDIO_AURISYS_PCM_DUMP_H

#include <stdio.h>

#include <stdint.h>
#include <stdbool.h>

#include <pthread.h>
#include <time.h>

#include <audio_ringbuf.h>

#define MAX_DUMP_NUM (1024)

typedef struct PcmDump_t {
    int (*AudioOpendumpPCMFile)(struct PcmDump_t *self, const char *filepath);
    int (*AudioCloseDumpPCMFile)(struct PcmDump_t *self);
    void (*AudioDumpPCMData)(struct PcmDump_t *self, void *buffer, uint32_t bytes);
    FILE *mFilep;
    pthread_t hPCMDumpThread;
    bool mPthreadEnable;
    pthread_cond_t mPCMDataNotifyEvent;
    pthread_mutex_t mPCMDataNotifyMutex;
    pthread_mutex_t mPCMDumpMutex; // use for PCM buffer dump
    audio_ringbuf_t mRingbuffer;
    unsigned int mLineaBufSize;
} PcmDump_t;

void InitPcmDump_t(struct PcmDump_t *self, unsigned int size);

#endif
