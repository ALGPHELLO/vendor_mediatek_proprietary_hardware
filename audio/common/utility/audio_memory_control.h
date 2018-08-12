#ifndef AUDIO_MEMORY_CONTROL_H
#define AUDIO_MEMORY_CONTROL_H

#include <string.h>

#include <audio_assert.h>


/*
 * =============================================================================
 *                     AUDIO_MALLOC & AUDIO_FREE
 * =============================================================================
 */

#ifdef MTK_AUDIO_COMMON_CODE_IN_SCP

#include <FreeRTOS.h>

#define AUDIO_MALLOC(sz) kal_pvPortMalloc(sz)
#define AUDIO_FREE(ptr)  kal_vPortFree(ptr)

#else

#include <stdlib.h>

#define AUDIO_MALLOC(sz) malloc(sz)
#define AUDIO_FREE(ptr)  free(ptr)

#endif


/*
 * =============================================================================
 *                     utilities
 * =============================================================================
 */

#define AUDIO_ALLOC_BUFFER(ptr, len) \
    do { \
        ptr = (void *)AUDIO_MALLOC(len); \
        AUD_ASSERT(ptr != NULL); \
        memset(ptr, 0, len); \
    } while(0)

#define AUDIO_ALLOC_CHAR_BUFFER(ptr, len) \
    do { \
        ptr = (char *)AUDIO_MALLOC(len); \
        AUD_ASSERT(ptr != NULL); \
        memset(ptr, 0, len); \
    } while(0)

#define AUDIO_ALLOC_STRUCT(type, ptr) \
    do { \
        ptr = (type *)AUDIO_MALLOC(sizeof(type)); \
        AUD_ASSERT(ptr != NULL); \
        memset(ptr, 0, sizeof(type)); \
    } while(0)

#define AUDIO_ALLOC_STRUCT_ARRAY(type, num, ptr) \
    do { \
        ptr = (type *)AUDIO_MALLOC(sizeof(type) * num); \
        AUD_ASSERT(ptr != NULL); \
        memset(ptr, 0, sizeof(type) * num); \
    } while(0)

#define AUDIO_FREE_POINTER(ptr) \
    do { \
        if (ptr != NULL) { \
            AUDIO_FREE(ptr); \
            ptr = NULL; \
        } \
    } while(0)


#endif /* AUDIO_MEMORY_CONTROL_H */

