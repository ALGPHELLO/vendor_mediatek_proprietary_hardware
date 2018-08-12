#include "audio_ringbuf.h"

#include <string.h>

#include <audio_log.h>
#include <audio_assert.h>

/*#include  "bli_exp.h"*/


#ifdef __cplusplus
extern "C" {
#endif


#define MAX_SIZE_OF_ONE_FRAME (16) /* 32-bits * 4ch */

//---------- implementation of ringbuffer--------------


// function for get how many data is available

/**
* function for get how many data is available
* @return how many data exist
*/

uint32_t audio_ringbuf_count(const audio_ringbuf_t *ringbuf) {
    int count = 0;

    if (ringbuf->write >= ringbuf->read) {
        count = ringbuf->write - ringbuf->read;
    } else {
        count = ringbuf->size - (ringbuf->read - ringbuf->write);
    }

    if (count < 0) {
        AUD_LOG_W("%s(), read %p, write %p, size %u, count < 0",
                  __FUNCTION__, ringbuf->read, ringbuf->write, ringbuf->size);
        count = 0;
    }

    return count;
}

/**
*  function for get how free space available
* @return how free sapce
*/

uint32_t audio_ringbuf_free_space(const audio_ringbuf_t *ringbuf) {
    int count = ringbuf->size - audio_ringbuf_count(ringbuf) - MAX_SIZE_OF_ONE_FRAME;
    return (count > 0) ? count : 0;
}

/**
* copy count number bytes from ring buffer to buf
* @param buf buffer copy from
* @param ringbuf buffer copy to
* @param count number of bytes need to copy
*/

void audio_ringbuf_copy_to_linear(char *linear_buf, audio_ringbuf_t *ringbuf, int count) {
    /*
    ALOGD("ringbuf->pBase = 0x%x ringbuf->pWrite = 0x%x  ringbuf->bufLen = %d  ringbuf->pRead = 0x%x",
        ringbuf->pBufBase,ringbuf->pWrite, ringbuf->bufLen,ringbuf->pRead);*/
    if (ringbuf->read <= ringbuf->write) {
        memcpy(linear_buf, ringbuf->read, count);
        ringbuf->read += count;
    } else {
        char *end = ringbuf->base + ringbuf->size;
        int r2e = end - ringbuf->read;
        if (count <= r2e) {
            //ALOGD("2 audio_ringbuf_copy_to_linear r2e= %d count = %d",r2e,count);
            memcpy(linear_buf, ringbuf->read, count);
            ringbuf->read += count;
            if (ringbuf->read == end) {
                ringbuf->read = ringbuf->base;
            }
        } else {
            //ALOGD("3 audio_ringbuf_copy_to_linear r2e= %d count = %d",r2e,count);
            memcpy(linear_buf, ringbuf->read, r2e);
            memcpy(linear_buf + r2e, ringbuf->base, count - r2e);
            ringbuf->read = ringbuf->base + count - r2e;
        }
    }
}

/**
* copy count number bytes from buf to ringbuf
* @param ringbuf ring buffer copy from
* @param buf copy to
* @param count number of bytes need to copy
*/
void audio_ringbuf_copy_from_linear(audio_ringbuf_t *ringbuf, const char *linear_buf, int count) {
    int spaceIHave = 0;
    char *end = ringbuf->base + ringbuf->size;

    // count buffer data I have
    spaceIHave = audio_ringbuf_free_space(ringbuf);


    // if not enough, assert
    AUD_ASSERT(spaceIHave >= count);

    if (ringbuf->read <= ringbuf->write) {
        int w2e = end - ringbuf->write;
        if (count <= w2e) {
            memcpy(ringbuf->write, linear_buf, count);
            ringbuf->write += count;
            if (ringbuf->write == end) {
                ringbuf->write = ringbuf->base;
            }
        } else {
            memcpy(ringbuf->write, linear_buf, w2e);
            memcpy(ringbuf->base, linear_buf + w2e, count - w2e);
            ringbuf->write = ringbuf->base + count - w2e;
        }
    } else {
        memcpy(ringbuf->write, linear_buf, count);
        ringbuf->write += count;
    }

}


/**
* copy ring buffer from RingBufs(source) to RingBuft(target)
* @param RingBuft ring buffer copy to
* @param RingBufs copy from copy from
*/

void audio_ringbuf_copy_to_empty(audio_ringbuf_t *ringbuf_des, audio_ringbuf_t *ringbuf_src) {
    if (ringbuf_src->read <= ringbuf_src->write) {
        audio_ringbuf_copy_from_linear(ringbuf_des, ringbuf_src->read, ringbuf_src->write - ringbuf_src->read);
        //ringbuf_src->pRead = ringbuf_src->pWrite;
        // no need to update source read pointer, because it is read to empty
    } else {
        char *end = ringbuf_src->base + ringbuf_src->size;
        audio_ringbuf_copy_from_linear(ringbuf_des, ringbuf_src->read, end - ringbuf_src->read);
        audio_ringbuf_copy_from_linear(ringbuf_des, ringbuf_src->base, ringbuf_src->write - ringbuf_src->base);
    }
}


/**
* copy ring buffer from RingBufs(source) to RingBuft(target) with count
* @param RingBuft ring buffer copy to
* @param RingBufs copy from copy from
*/
int audio_ringbuf_copy_from_ringbuf(audio_ringbuf_t *ringbuf_des, audio_ringbuf_t *ringbuf_src, int count) {
    int cntInRingBufs = audio_ringbuf_count(ringbuf_src);
    int freeSpaceInRingBuft = audio_ringbuf_free_space(ringbuf_des);
    AUD_ASSERT(count <= cntInRingBufs && count <= freeSpaceInRingBuft);

    if (ringbuf_src->read <= ringbuf_src->write) {
        audio_ringbuf_copy_from_linear(ringbuf_des, ringbuf_src->read, count);
        ringbuf_src->read += count;
        // no need to update source read pointer, because it is read to empty
    } else {
        char *end = ringbuf_src->base + ringbuf_src->size;
        int cnt2e = end - ringbuf_src->read;
        if (cnt2e >= count) {
            audio_ringbuf_copy_from_linear(ringbuf_des, ringbuf_src->read, count);
            ringbuf_src->read += count;
            if (ringbuf_src->read == end) {
                ringbuf_src->read = ringbuf_src->base;
            }
        } else {
            audio_ringbuf_copy_from_linear(ringbuf_des, ringbuf_src->read, cnt2e);
            audio_ringbuf_copy_from_linear(ringbuf_des, ringbuf_src->base, count - cnt2e);
            ringbuf_src->read = ringbuf_src->base + count - cnt2e;
        }
    }
    return count;
}


/**
* fill count number zero bytes to ringbuf
* @param ringbuf ring buffer fill from
* @param count number of bytes need to copy
*/
void audio_ringbuf_write_zero(audio_ringbuf_t *ringbuf, int count) {
    int spaceIHave = 0;
    char *end = ringbuf->base + ringbuf->size;

    // count buffer data I have
    spaceIHave = audio_ringbuf_free_space(ringbuf);

    // if not enough, assert
    AUD_ASSERT(spaceIHave >= count);

    if (ringbuf->read <= ringbuf->write) {
        int w2e = end - ringbuf->write;
        if (count <= w2e) {
            memset(ringbuf->write, 0, sizeof(char)*count);
            ringbuf->write += count;
            if (ringbuf->write == end) {
                ringbuf->write = ringbuf->base;
            }
        } else {
            memset(ringbuf->write, 0, sizeof(char)*w2e);
            memset(ringbuf->base, 0, sizeof(char) * (count - w2e));
            ringbuf->write = ringbuf->base + count - w2e;
        }
    } else {
        memset(ringbuf->write, 0, sizeof(char)*count);
        ringbuf->write += count;
    }
}


/**
* write bytes size of count woith value
* @param ringbuf ring buffer copy to
* @value value put into buffer
* @count bytes ned to put.
*/
void audio_ringbuf_write_value(audio_ringbuf_t *ringbuf, const int value, const int count) {
    int spaceIHave = 0;

    // count buffer data I have
    spaceIHave = audio_ringbuf_free_space(ringbuf);

    // if not enough, assert
    AUD_ASSERT(spaceIHave >= count);

    if (ringbuf->read <= ringbuf->write) {
        char *end = ringbuf->base + ringbuf->size;
        int w2e = end - ringbuf->write;
        if (count <= w2e) {
            memset(ringbuf->write, value, count);
            ringbuf->write += count;
        } else {
            memset(ringbuf->write, value, w2e);
            memset(ringbuf->base, value, count - w2e);
            ringbuf->write = ringbuf->base + count - w2e;
        }
    } else {
        memset(ringbuf->write, value, count);
        ringbuf->write += count;
    }

}

/**
* Remove ring buffer data
* @param audio_ringbuf_t ring buffer
* @value count remove data size
*/
void audio_ringbuf_drop_data(audio_ringbuf_t *ringbuf, const int count) {
    if ((uint32_t)count > audio_ringbuf_count(ringbuf)) {
        AUD_ASSERT((uint32_t)count <= audio_ringbuf_count(ringbuf));
        return;
    }

    if (ringbuf->read <= ringbuf->write) {
        ringbuf->read += count;
    } else {
        char *end = ringbuf->base + ringbuf->size;
        int r2e = end - ringbuf->read;
        if (count <= r2e) {
            ringbuf->read += count;
            if (ringbuf->read == end) {
                ringbuf->read = ringbuf->base;
            }
        } else {
            ringbuf->read = ringbuf->base + count - r2e;
        }
    }
}


void audio_ringbuf_drop_all(audio_ringbuf_t *ringbuf) {
    ringbuf->read = ringbuf->write;
}


void audio_ringbuf_compensate_value(audio_ringbuf_t *ringbuf, const int value, const int count) {
    char *end = ringbuf->base + ringbuf->size;

    int b2r = 0;
    int left_data = 0;

    if ((uint32_t)count > audio_ringbuf_free_space(ringbuf)) {
        AUD_ASSERT((uint32_t)count <= audio_ringbuf_free_space(ringbuf));
        return;
    }


    if (ringbuf->read <= ringbuf->write) {
        b2r = ringbuf->read - ringbuf->base;
        if (b2r >= count) {
            ringbuf->read -= count;
            memset(ringbuf->read, value, count);
        } else {
            if (b2r > 0) { /* in case read == base */
                memset(ringbuf->base, value, b2r);
                left_data = count - b2r;
                ringbuf->read = end - left_data;
                memset(ringbuf->read, value, left_data);
            } else {
                ringbuf->read = end - count;
                memset(ringbuf->read, value, count);
            }
        }
    } else {
        ringbuf->read -= count;
        memset(ringbuf->read, value, count);
    }


    AUD_ASSERT(ringbuf->read  >= ringbuf->base && ringbuf->read  < end);
    AUD_ASSERT(ringbuf->write >= ringbuf->base && ringbuf->write < end);
}


void audio_ringbuf_rollback(audio_ringbuf_t *ringbuf, const int count) {
    char *end = ringbuf->base + ringbuf->size;

    int b2r = ringbuf->read - ringbuf->base;
    int left_data = 0;

    if ((uint32_t)count > audio_ringbuf_free_space(ringbuf)) {
        AUD_ASSERT((uint32_t)count <= audio_ringbuf_free_space(ringbuf));
        return;
    }


    if (ringbuf->read <= ringbuf->write) {
        b2r = ringbuf->read - ringbuf->base;
        if (b2r >= count) {
            ringbuf->read -= count;
        } else {
            if (b2r > 0) { /* in case read == base */
                left_data = count - b2r;
                ringbuf->read = end - left_data;
            } else {
                ringbuf->read = end - count;
            }
        }
    } else {
        ringbuf->read -= count;
    }
}



//---------end of ringbuffer implemenation------------------------------------------------------

#ifdef __cplusplus
}  /* extern "C" */
#endif

