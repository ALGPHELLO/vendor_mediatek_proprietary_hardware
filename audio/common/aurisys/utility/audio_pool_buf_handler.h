#ifndef MTK_AUDIO_POOL_BUF_HANDLER_H
#define MTK_AUDIO_POOL_BUF_HANDLER_H

#include <uthash.h> /* uthash */

#include <audio_ringbuf.h>

#include <arsi_type.h>


#ifdef __cplusplus
extern "C" {
#endif



/*
 * =============================================================================
 *                     ref struct
 * =============================================================================
 */

struct MtkAudioSrcInC;
struct MtkAudioBitConverterC;
#ifdef AURISYS_DUMP_PCM
struct PcmDump_t;
#endif



/*
 * =============================================================================
 *                     MACRO
 * =============================================================================
 */


/*
 * =============================================================================
 *                     typedef
 * =============================================================================
 */

typedef struct audio_pool_buf_t {
    audio_buf_t     *buf;        /* allocated memory */
    audio_ringbuf_t  ringbuf;    /* use the ringbuf r/w pointer to control buf */
} audio_pool_buf_t;


typedef struct audio_pool_buf_formatter_t {
    audio_pool_buf_t *pool_source;
    audio_pool_buf_t *pool_target; /* SRC/BitConvert/... from pool to pool_formatted */

    char                         *linear_buf;
    uint32_t                      linear_buf_size;

    struct MtkAudioSrcInC        *bli_src;
    char                         *bli_src_out_buf;
    uint32_t                      bli_src_out_buf_size;

    struct MtkAudioBitConverterC *bit_convert;
    char                         *bit_convert_out_buf;
    uint32_t                      bit_convert_out_buf_size;



#ifdef AURISYS_DUMP_PCM
    struct PcmDump_t *pcm_dump_source;
    struct PcmDump_t *pcm_dump_blisrc;
    struct PcmDump_t *pcm_dump_target;
#endif

} audio_pool_buf_formatter_t;


typedef struct audio_pool_buf_handler_hh_t {
    data_buf_type_t data_buf_type; /* key */

    audio_pool_buf_formatter_t pool_buf_handler;

    UT_hash_handle hh; /* makes this structure hashable */
} audio_pool_buf_handler_hh_t;


/*
 * =============================================================================
 *                     struct definition
 * =============================================================================
 */


/*
 * =============================================================================
 *                     hook function
 * =============================================================================
 */


/*
 * =============================================================================
 *                     public function
 * =============================================================================
 */

void audio_pool_buf_handler_c_file_init(void);
void audio_pool_buf_handler_c_file_deinit(void);

void config_ringbuf_by_data_buf(
    audio_ringbuf_t *ringbuf,
    data_buf_t      *data_buf);


void create_pool_buf(
    audio_pool_buf_t *audio_pool_buf,
    audio_buf_t      *audio_buf_pattern,
    const uint32_t    pool_buf_size);


void destroy_pool_buf(audio_pool_buf_t *audio_pool_buf);

void audio_pool_buf_formatter_init(audio_pool_buf_formatter_t *formatter);
void audio_pool_buf_formatter_process(audio_pool_buf_formatter_t *formatter);
void audio_pool_buf_formatter_deinit(audio_pool_buf_formatter_t *formatter);



#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* end of MTK_AUDIO_POOL_BUF_HANDLER_H */

