#ifndef AUDIO_LOCK_H
#define AUDIO_LOCK_H

/* TODO: kernel & scp */
#include <string.h>

#include <stdint.h>
#include <stdbool.h>

#include <pthread.h>
#include <time.h>

#include <audio_log.h>
#include <audio_assert.h>
#include <audio_memory_control.h>

#include <audio_time.h>


#ifdef __cplusplus
extern "C" {
#endif



/*
 * =============================================================================
 *                     MACRO
 * =============================================================================
 */

#define MAX_LOCK_TIMEOUT_MS (1000)

#ifdef LOCK_TAG
#undef LOCK_TAG
#endif
#define LOCK_TAG "[ALOCK] "

#ifdef MTK_AUDIO_LOCK_ENABLE_LOG
#ifdef ALOCK_LOG
#undef ALOCK_LOG
#endif
#define ALOCK_LOG(x...) AUD_LOG_D(x)
#else
#define ALOCK_LOG(x...)
#endif

#ifdef MTK_AUDIO_LOCK_ENABLE_TRACE
#define MAX_TRACE_NUM (8)

#define MAX_TRACE_TYPE_LENGTH (16)
#define MAX_TRACE_NAME_LENGTH (64)
#define MAX_TRACE_FILE_LENGTH (64)
#define MAX_TRACE_FUNC_LENGTH (64)
#define MAX_TRACE_TIME_LENGTH (16)  /* ex: "05:34:22.804" */
#endif


/*
 * =============================================================================
 *                     struct definition
 * =============================================================================
 */

#ifdef MTK_AUDIO_LOCK_ENABLE_TRACE
typedef struct alock_log_unit_t {
    char type[MAX_TRACE_TYPE_LENGTH];
    char name[MAX_TRACE_NAME_LENGTH];
    char file[MAX_TRACE_FILE_LENGTH];
    char func[MAX_TRACE_FUNC_LENGTH];
    char time[MAX_TRACE_TIME_LENGTH];
    uint32_t line;
    uint16_t pid;
    uint16_t tid;
} alock_log_unit_t;

typedef struct alock_trace_info_t {
    alock_log_unit_t log[MAX_TRACE_NUM];
    struct timespec ts_start;
    struct timespec ts_stop;
    pthread_mutex_t idx_lock;
    uint8_t idx;
    bool timeout;
} alock_trace_info_t;
#endif


typedef struct alock_t {
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
#ifdef MTK_AUDIO_LOCK_ENABLE_TRACE
    alock_trace_info_t trace_info;
#endif
} alock_t;



/*
 * =============================================================================
 *                     public function
 * =============================================================================
 */

void alock_cleanup_handler(void *arg);

const char *get_filename(const char *file);


int alock_new(
    alock_t **pp_alock, const char *alock_name,
    const char *file, const char *func, const uint32_t line);

int alock_free(
    alock_t **pp_alock, const char *alock_name,
    const char *file, const char *func, const uint32_t line);

int alock_lock_no_timeout(
    alock_t *p_alock, const char *alock_name,
    const char *file, const char *func, const uint32_t line);

int alock_trylock(
    alock_t *p_alock, const char *alock_name,
    const char *file, const char *func, const uint32_t line);

int alock_lock_ms(
    alock_t *p_alock, const char *alock_name, const uint32_t ms,
    const char *file, const char *func, const uint32_t line);

int alock_unlock(
    alock_t *p_alock, const char *alock_name,
    const char *file, const char *func, const uint32_t line);

int alock_wait_no_timeout(
    alock_t *p_alock, const char *alock_name,
    const char *file, const char *func, const uint32_t line);

int alock_wait_ms(
    alock_t *p_alock, const char *alock_name, const uint32_t ms,
    const char *file, const char *func, const uint32_t line);

int alock_signal(
    alock_t *p_alock, const char *alock_name,
    const char *file, const char *func, const uint32_t line);




#define NEW_ALOCK(alock) \
    ({ \
        int __ret = alock_new(&(alock), #alock, \
                              get_filename(__FILE__), \
                              (const char *)__FUNCTION__, \
                              __LINE__); \
        if (__ret != 0) { AUD_WARNING_FT("new lock fail!!"); } \
        __ret; \
    })


#define FREE_ALOCK(alock) \
    ({ \
        int __ret = alock_free(&(alock), #alock, \
                               get_filename(__FILE__), \
                               (const char *)__FUNCTION__, \
                               __LINE__); \
        if (__ret != 0) { AUD_WARNING_FT("free lock fail!!"); } \
        __ret; \
    })


#define LOCK_ALOCK_NO_TIMEOUT(alock) \
    ({ \
        int __ret = alock_lock_no_timeout((alock), #alock, \
                                          get_filename(__FILE__), \
                                          (const char *)__FUNCTION__, \
                                          __LINE__); \
        if (__ret != 0) { AUD_WARNING_FT("lock fail!!"); } \
        __ret; \
    })


#define LOCK_ALOCK_TRYLOCK(alock) \
    ({ \
        int __ret = alock_trylock((alock), #alock, \
                                  get_filename(__FILE__), \
                                  (const char *)__FUNCTION__, \
                                  __LINE__); \
        __ret; \
    })


#define LOCK_ALOCK_MS(alock, ms) \
    ({ \
        int __ret = alock_lock_ms((alock), #alock, ms, \
                                  get_filename(__FILE__), \
                                  (const char *)__FUNCTION__, \
                                  __LINE__); \
        if (__ret != 0) { AUD_WARNING_FT("lock timeout!!"); } \
        __ret; \
    })


#define LOCK_ALOCK(alock) \
    LOCK_ALOCK_MS((alock), MAX_LOCK_TIMEOUT_MS)


#define UNLOCK_ALOCK(alock) \
    ({ \
        int __ret = alock_unlock((alock), #alock, \
                                 get_filename(__FILE__), \
                                 (const char *)__FUNCTION__, \
                                 __LINE__); \
        if (__ret != 0) { AUD_WARNING_FT("unlock fail!!"); } \
        __ret; \
    })


#define WAIT_ALOCK(alock) \
    ({ \
        int __ret = alock_wait_no_timeout((alock), #alock, \
                                          get_filename(__FILE__), \
                                          (const char *)__FUNCTION__, \
                                          __LINE__); \
        __ret; \
    })


#define WAIT_ALOCK_MS(alock, ms) \
    ({ \
        int __ret = alock_wait_ms((alock), #alock, ms, \
                                  get_filename(__FILE__), \
                                  (const char *)__FUNCTION__, \
                                  __LINE__); \
        __ret; \
    })


#define SIGNAL_ALOCK(alock) \
    ({ \
        int __ret = alock_signal((alock), #alock, \
                                 get_filename(__FILE__), \
                                 (const char *)__FUNCTION__, \
                                 __LINE__); \
        if (__ret != 0) { AUD_WARNING_FT("signal fail!!"); } \
        __ret; \
    })


#define CLEANUP_PUSH_ALOCK(alock) \
    pthread_cleanup_push(alock_cleanup_handler, (alock))


#define CLEANUP_POP_ALOCK(alock) \
    pthread_cleanup_pop(0)



#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* end of AUDIO_LOCK_H */

