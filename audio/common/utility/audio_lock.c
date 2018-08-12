#include "audio_lock.h"

#include <string.h>

#include <errno.h>

#include <sys/time.h>
#include <sys/prctl.h>

#include <sys/types.h>
#include <unistd.h>

#include <audio_log.h>



#ifdef __cplusplus
extern "C" {
#endif


/*
 * =============================================================================
 *                     MACRO
 * =============================================================================
 */

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "AudioLock"

#define MAX_SYS_TIME_TRY_COUNT (10)
#define MAX_WAIT_BLOCKED_BY_LOCK_MS (20)


#ifndef ANDROID
#define pthread_mutex_timedlock(a,b) pthread_mutex_lock(a)
#define pthread_mutex_trylock(a) pthread_mutex_lock(a)
#endif


/*
 * =============================================================================
 *                     utilities implementation
 * =============================================================================
 */

#ifdef MTK_AUDIO_LOCK_ENABLE_TRACE
static void alock_update_trace_info(
    const char *type, alock_t *p_alock, const char *alock_name,
    const char *file, const char *func, const uint32_t line) {
    pthread_mutex_lock(&p_alock->trace_info.idx_lock);
    uint8_t idx = p_alock->trace_info.idx;
    p_alock->trace_info.idx++;
    if (p_alock->trace_info.idx == MAX_TRACE_NUM) {
        p_alock->trace_info.idx = 0;
    }

    alock_log_unit_t *log_unit = &p_alock->trace_info.log[idx];

    strncpy(log_unit->type, type, MAX_TRACE_TYPE_LENGTH - 1);
    strncpy(log_unit->name, alock_name, MAX_TRACE_NAME_LENGTH - 1);
    strncpy(log_unit->file, file, MAX_TRACE_FILE_LENGTH - 1);
    strncpy(log_unit->func, func, MAX_TRACE_FUNC_LENGTH - 1);

    char time_info_string[MAX_TRACE_TIME_LENGTH] = {0};

    struct timespec ts;
    struct tm t;

    uint32_t ms = 0;

    audio_get_timespec_realtime(&ts);
    ms = ts.tv_nsec / 1000000L;

    if (localtime_r(&ts.tv_sec, &t) == NULL) {
        memset(log_unit->time, 0, MAX_TRACE_TIME_LENGTH);
    } else if (strftime(time_info_string, MAX_TRACE_TIME_LENGTH - 1, "%T", &t) == 0) {
        memset(log_unit->time, 0, MAX_TRACE_TIME_LENGTH);
    } else {
        snprintf(log_unit->time, MAX_TRACE_TIME_LENGTH - 1, "%s.%03u", time_info_string, ms);
    }

    log_unit->line = line;
    log_unit->pid = getpid();
    log_unit->tid = gettid();

    pthread_mutex_unlock(&p_alock->trace_info.idx_lock);
}
#endif


#ifdef MTK_AUDIO_LOCK_ENABLE_TRACE
static void alock_dump_trace_info(alock_t *p_alock) {
    p_alock->trace_info.timeout = true;

    pthread_mutex_lock(&p_alock->trace_info.idx_lock);

    alock_log_unit_t *log_unit = NULL;

    uint8_t idx  = p_alock->trace_info.idx + 1;
    if (idx == MAX_TRACE_NUM) {
        idx = 0;
    }

    AUD_LOG_E(LOCK_TAG"%p: ========================= dump(+) =========================", p_alock);

    while (idx != p_alock->trace_info.idx) {
        log_unit = &p_alock->trace_info.log[idx];
        if (strlen(log_unit->type) > 0) {
            AUD_LOG_E(LOCK_TAG"%p: time: \"%s\", pid tid: \" %5d %5d \", %s(%s), %s, %s(), %uL",
                      p_alock,
                      log_unit->time,
                      log_unit->pid,
                      log_unit->tid,
                      log_unit->type,
                      log_unit->name,
                      log_unit->file,
                      log_unit->func,
                      log_unit->line);
        }

        idx++;
        if (idx == MAX_TRACE_NUM) {
            idx = 0;
        }
    }

    AUD_LOG_E(LOCK_TAG"%p: ========================= dump(-) =========================", p_alock);

    pthread_mutex_unlock(&p_alock->trace_info.idx_lock);
}
#endif


void alock_cleanup_handler(void *arg) {
    if (arg == NULL) {
        return;
    }

    alock_t *p_alock = (alock_t *)arg;
    UNLOCK_ALOCK(p_alock);
}


const char *get_filename(const char *file) {
    const char *slash = strrchr(file, '/');
    return (slash) ? slash + 1 : file;
}



int alock_new(
    alock_t **pp_alock, const char *alock_name,
    const char *file, const char *func, const uint32_t line) {
    alock_t *p_alock = (alock_t *)AUDIO_MALLOC(sizeof(alock_t));
    if (p_alock == NULL) {
        AUD_LOG_E(LOCK_TAG"%p: new(%s), %s, %s(), %uL FAIL!!",
                  p_alock, alock_name, file, func, line);
        *pp_alock = NULL;
        return -1;
    }
    *pp_alock = p_alock;

    pthread_mutex_init(&p_alock->mutex, NULL);
    pthread_cond_init(&p_alock->cond, NULL);

#ifdef MTK_AUDIO_LOCK_ENABLE_TRACE
    memset(&p_alock->trace_info, 0, sizeof(alock_trace_info_t));
    pthread_mutex_init(&p_alock->trace_info.idx_lock, NULL);
#endif

    ALOCK_LOG(LOCK_TAG"%p: new(%s), %s, %s(), %uL",
              p_alock, alock_name, file, func, line);
    return 0;
}


int alock_free(
    alock_t **pp_alock, const char *alock_name,
    const char *file, const char *func, const uint32_t line) {
    alock_t *p_alock = (alock_t *)*pp_alock;
    if (p_alock == NULL) {
        AUD_LOG_E(LOCK_TAG"%p: free(%s), %s, %s(), %uL FAIL!!",
                  p_alock, alock_name, file, func, line);
        return -1;
    }

    ALOCK_LOG(LOCK_TAG"%p: free(%s), %s, %s(), %uL",
              p_alock, alock_name, file, func, line);

#ifdef MTK_AUDIO_LOCK_ENABLE_TRACE
    pthread_mutex_destroy(&p_alock->trace_info.idx_lock);
#endif

    pthread_mutex_destroy(&p_alock->mutex);
    pthread_cond_destroy(&p_alock->cond);

    AUDIO_FREE(p_alock);
    p_alock = NULL;
    *pp_alock = NULL;

    return 0;
}


int alock_lock_no_timeout(
    alock_t *p_alock, const char *alock_name,
    const char *file, const char *func, const uint32_t line) {
    if (p_alock == NULL) {
        AUD_LOG_E(LOCK_TAG"%p: lock(%s), %s, %s(), %uL FAIL!!",
                  p_alock, alock_name, file, func, line);
        return -1;
    }

    ALOCK_LOG(LOCK_TAG"%p: lock(%s), %s, %s(), %uL",
              p_alock, alock_name, file, func, line);

    pthread_mutex_lock(&p_alock->mutex);

#ifdef MTK_AUDIO_LOCK_ENABLE_TRACE
    alock_update_trace_info("lock", p_alock, alock_name, file, func, line);
    audio_get_timespec_monotonic(&p_alock->trace_info.ts_start);
#endif

    return 0;
}


int alock_trylock(
    alock_t *p_alock, const char *alock_name,
    const char *file, const char *func, const uint32_t line) {
    int retval = 0;

    if (p_alock == NULL) {
        AUD_LOG_E(LOCK_TAG"%p: trylock(%s), %s, %s(), %uL FAIL!!",
                  p_alock, alock_name, file, func, line);
        return -1;
    }

    retval = -pthread_mutex_trylock(&p_alock->mutex);

#ifdef MTK_AUDIO_LOCK_ENABLE_TRACE
    if (retval == 0) {
        alock_update_trace_info("trylock", p_alock, alock_name, file, func, line);
        audio_get_timespec_monotonic(&p_alock->trace_info.ts_start);
    }
#endif

    ALOCK_LOG(LOCK_TAG"%p: trylock(%s), %s, %s(), %uL, retval: %d",
              p_alock, alock_name, file, func, line, retval);

    return retval;
}


int alock_lock_ms(
    alock_t *p_alock, const char *alock_name, const uint32_t ms,
    const char *file, const char *func, const uint32_t line) {
    struct timespec ts_timeout;

    struct timespec ts_start;
    struct timespec ts_stop;

    uint32_t ms_spend = 0;
    uint32_t ms_left  = ms;

    int try_count = 0;

    int retval = 0;

    if (p_alock == NULL) {
        AUD_LOG_E(LOCK_TAG"%p: lock(%s, %u), %s, %s(), %uL FAIL!!",
                  p_alock, alock_name, ms, file, func, line);
        return -1;
    }

    if (ms == 0) {
        AUD_LOG_W(LOCK_TAG"%p: lock(%s, %u), %s, %s(), %uL call alock_lock_no_timeout() due to ms = 0!!",
                  p_alock, alock_name, ms, file, func, line);
        return alock_lock_no_timeout(p_alock, alock_name, file, func, line);
    }

    ALOCK_LOG(LOCK_TAG"%p: lock(%s, %u), %s, %s(), %uL",
              p_alock, alock_name, ms, file, func, line);

    do {
        audio_get_timespec_monotonic(&ts_start);
        audio_get_timespec_timeout(&ts_timeout, ms_left);
        retval = -pthread_mutex_timedlock(&p_alock->mutex, &ts_timeout);
        audio_get_timespec_monotonic(&ts_stop);

        /* pass or other error which is not timeout */
        if (retval == 0 || retval != -ETIMEDOUT) {
            break;
        }

        /* -ETIMEDOUT */
        ms_spend += get_time_diff_ms(&ts_start, &ts_stop);
        if (ms_spend >= ms) { /* monotonic also timeout */
            break;
        }
        ms_left = ms - ms_spend;

        /* AlarmManagerService: Setting time of day to sec=xxx */
        /* SimStateMonitor: onReceive action : android.intent.action.ACTION_SUBINFO_RECORD_UPDATED */
        AUD_LOG_W(LOCK_TAG"%p: lock(%s, %u), %s, %s(), %uL, systime changed, ms_left: %u",
                  p_alock, alock_name, ms, file, func, line, ms_left);
    } while (++try_count < MAX_SYS_TIME_TRY_COUNT);

    if (retval == 0) {
#ifdef MTK_AUDIO_LOCK_ENABLE_TRACE
        alock_update_trace_info("lock", p_alock, alock_name, file, func, line);
        audio_get_timespec_monotonic(&p_alock->trace_info.ts_start);
#endif
    } else {
        AUD_LOG_E(LOCK_TAG"%p: lock(%s, %u), %s, %s(), %uL FAIL!! retval: %d",
                  p_alock, alock_name, ms, file, func, line, retval);
#ifdef MTK_AUDIO_LOCK_ENABLE_TRACE
        alock_dump_trace_info(p_alock);
#endif
    }

    return retval;
}


int alock_unlock(
    alock_t *p_alock, const char *alock_name,
    const char *file, const char *func, const uint32_t line) {
    if (p_alock == NULL) {
        AUD_LOG_E(LOCK_TAG"%p: unlock(%s), %s, %s(), %uL FAIL!!",
                  p_alock, alock_name, file, func, line);
        return -1;
    }

    ALOCK_LOG(LOCK_TAG"%p: unlock(%s), %s, %s(), %uL",
              p_alock, alock_name, file, func, line);

#ifdef MTK_AUDIO_LOCK_ENABLE_TRACE
    audio_get_timespec_monotonic(&p_alock->trace_info.ts_stop);
    alock_update_trace_info("unlock", p_alock, alock_name, file, func, line);

    if (p_alock->trace_info.timeout == true) {
        p_alock->trace_info.timeout = false;
        AUD_LOG_W(LOCK_TAG"%p: unlock(%s), %s, %s(), %uL, lock time %u ms",
                  p_alock, alock_name, file, func, line,
                  (uint32_t)get_time_diff_ms(&p_alock->trace_info.ts_start,
                                             &p_alock->trace_info.ts_stop));
    }
#endif

    pthread_mutex_unlock(&p_alock->mutex);

    return 0;
}



int alock_wait_no_timeout(
    alock_t *p_alock, const char *alock_name,
    const char *file, const char *func, const uint32_t line) {
    if (p_alock == NULL) {
        AUD_LOG_E(LOCK_TAG"%p: wait(%s), %s, %s(), %uL FAIL!!",
                  p_alock, alock_name, file, func, line);
        return -1;
    }

    ALOCK_LOG(LOCK_TAG"%p: +wait(%s), %s, %s(), %uL",
              p_alock, alock_name, file, func, line);

#ifdef MTK_AUDIO_LOCK_ENABLE_TRACE
    alock_update_trace_info("+wait", p_alock, alock_name, file, func, line);
#endif

    pthread_cond_wait(&p_alock->cond, &p_alock->mutex);

#ifdef MTK_AUDIO_LOCK_ENABLE_TRACE
    alock_update_trace_info("-wait", p_alock, alock_name, file, func, line);
#endif

    ALOCK_LOG(LOCK_TAG"%p: -wait(%s), %s, %s(), %uL",
              p_alock, alock_name, file, func, line);

    return 0;
}


int alock_wait_ms(
    alock_t *p_alock, const char *alock_name, const uint32_t ms,
    const char *file, const char *func, const uint32_t line) {
    struct timespec ts_timeout;

    struct timespec ts_start;
    struct timespec ts_stop;

    uint32_t ms_spend = 0;
    uint32_t ms_left  = ms;

    int try_count = 0;

    int retval = 0;

    if (p_alock == NULL) {
        AUD_LOG_E(LOCK_TAG"%p: wait(%s, %u), %s, %s(), %uL FAIL!!",
                  p_alock, alock_name, ms, file, func, line);
        return -1;
    }

    if (ms == 0) {
        AUD_LOG_W(LOCK_TAG"%p: wait(%s, %u), %s, %s(), %uL not wait due to ms = 0!!",
                  p_alock, alock_name, ms, file, func, line);
        return -1;
    }

    ALOCK_LOG(LOCK_TAG"%p: +wait(%s, %u), %s, %s(), %uL",
              p_alock, alock_name, ms, file, func, line);

#ifdef MTK_AUDIO_LOCK_ENABLE_TRACE
    alock_update_trace_info("+wait", p_alock, alock_name, file, func, line);
#endif

    do {
        audio_get_timespec_monotonic(&ts_start);
        audio_get_timespec_timeout(&ts_timeout, ms_left);
        retval = -pthread_cond_timedwait(&p_alock->cond, &p_alock->mutex, &ts_timeout);
        audio_get_timespec_monotonic(&ts_stop);

        /* pass or other error which is not timeout */
        if (retval == 0 || retval != -ETIMEDOUT) {
            break;
        }

        /* -ETIMEDOUT */
        ms_spend += get_time_diff_ms(&ts_start, &ts_stop);
        if (ms_spend >= ms) { /* monotonic also timeout */
            break;
        }
        ms_left = ms - ms_spend;

        /* AlarmManagerService: Setting time of day to sec=xxx */
        /* SimStateMonitor: onReceive action : android.intent.action.ACTION_SUBINFO_RECORD_UPDATED */
        AUD_LOG_W(LOCK_TAG"%p: wait(%s, %u), %s, %s(), %uL, systime changed, ms_left: %u",
                  p_alock, alock_name, ms, file, func, line, ms_left);
    } while (++try_count < MAX_SYS_TIME_TRY_COUNT);

#ifdef MTK_AUDIO_LOCK_ENABLE_TRACE
    alock_update_trace_info("-wait", p_alock, alock_name, file, func, line);
#endif

    if (retval == 0) {
        ALOCK_LOG(LOCK_TAG"%p: -wait(%s, %u), %s, %s(), %uL",
                  p_alock, alock_name, ms, file, func, line);
    } else if (retval == -ETIMEDOUT) {
        if (ms_spend > ms + MAX_WAIT_BLOCKED_BY_LOCK_MS) {
            AUD_LOG_W(LOCK_TAG"%p: -wait(%s, %u), %s, %s(), %uL FAIL!! retval: %d, ms_spend: %u",
                      p_alock, alock_name, ms, file, func, line, retval, ms_spend);
        }
    } else {
        AUD_LOG_E(LOCK_TAG"%p: -wait(%s, %u), %s, %s(), %uL FAIL!! retval: %d",
                  p_alock, alock_name, ms, file, func, line, retval);
    }

    return retval;
}


int alock_signal(
    alock_t *p_alock, const char *alock_name,
    const char *file, const char *func, const uint32_t line) {
    if (p_alock == NULL) {
        AUD_LOG_E(LOCK_TAG"%p: signal(%s), %s, %s(), %uL FAIL!!",
                  p_alock, alock_name, file, func, line);
        return -1;
    }

    ALOCK_LOG(LOCK_TAG"%p: signal(%s), %s, %s(), %uL",
              p_alock, alock_name, file, func, line);

    pthread_cond_signal(&p_alock->cond);

#ifdef MTK_AUDIO_LOCK_ENABLE_TRACE
    alock_update_trace_info("signal", p_alock, alock_name, file, func, line);
#endif

    return 0;
}



#ifdef __cplusplus
}  /* extern "C" */
#endif

