#ifndef AUDIO_RING_BUF_H
#define AUDIO_RING_BUF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct audio_ringbuf_t {
    char    *base;
    char    *read;
    char    *write;
    uint32_t size;
} audio_ringbuf_t;


uint32_t audio_ringbuf_count(const audio_ringbuf_t *ringbuf);
uint32_t audio_ringbuf_free_space(const audio_ringbuf_t *ringbuf);

void audio_ringbuf_copy_to_linear(char *linear_buf, audio_ringbuf_t *ringbuf, int count);
void audio_ringbuf_copy_from_linear(audio_ringbuf_t *ringbuf, const char *linear_buf, int count);

void audio_ringbuf_copy_to_empty(audio_ringbuf_t *ringbuf_des, audio_ringbuf_t *ringbuf_src);
int  audio_ringbuf_copy_from_ringbuf(audio_ringbuf_t *ringbuf_des, audio_ringbuf_t *ringbuf_src, int count);

void audio_ringbuf_write_zero(audio_ringbuf_t *ringbuf, int count);
void audio_ringbuf_write_value(audio_ringbuf_t *ringbuf, const int value, const int count);

void audio_ringbuf_drop_data(audio_ringbuf_t *ringbuf, const int count);
void audio_ringbuf_drop_all(audio_ringbuf_t *ringbuf);

void audio_ringbuf_compensate_value(audio_ringbuf_t *ringbuf, const int value, const int count);

void audio_ringbuf_rollback(audio_ringbuf_t *ringbuf, const int count);



#define AUDIO_RINGBUF_COPY_FROM_RINGBUF_ALL(des, src) \
    do { \
        uint32_t data_count = audio_ringbuf_count((src)); \
        uint32_t free_count = audio_ringbuf_free_space((des)); \
        if (data_count > free_count) { \
            AUD_LOG_W("%s(), data_count %u > free_count %u, %uL", __FUNCTION__, \
                      data_count, free_count, __LINE__); \
            data_count = free_count; \
        } \
        audio_ringbuf_copy_from_ringbuf((des), (src), data_count); \
    } while (0)



#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* end of AUDIO_RING_BUF_H */

