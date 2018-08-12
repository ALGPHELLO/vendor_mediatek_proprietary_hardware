#include "audio_pool_buf_handler.h"

#include <dlfcn.h> /* dlopen & dlsym */

#include <uthash.h> /* uthash */

#include <wrapped_audio.h>

#include <audio_log.h>
#include <audio_assert.h>
#include <audio_debug_tool.h>
#include <audio_memory_control.h>

#include <arsi_type.h>


#include <MtkAudioBitConverterc.h>
#include <MtkAudioSrcInC.h>
#include <MtkAudioComponentEngineCommon.h>

/* pcm dump */
#ifdef AURISYS_DUMP_PCM
#include <sys/prctl.h>
#include <AudioAurisysPcmDump.h>
#endif



#ifdef __cplusplus
extern "C" {
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "audio_pool_buf_handler"


#ifdef AURISYS_DUMP_LOG_V
#undef  AUD_LOG_V
#define AUD_LOG_V(x...) AUD_LOG_D(x)
#endif


/*
 * =============================================================================
 *                     MACRO
 * =============================================================================
 */

#if defined(__LP64__)
#define AUDIO_COMPONENT_ENGINE_VENDOR_LIB_PATH "/vendor/lib64/libaudiocomponentenginec.so"
#define AUDIO_COMPONENT_ENGINE_SYSTEM_LIB_PATH "/system/lib64/libaudiocomponentenginec.so"
#else
#define AUDIO_COMPONENT_ENGINE_VENDOR_LIB_PATH "/vendor/lib/libaudiocomponentenginec.so"
#define AUDIO_COMPONENT_ENGINE_SYSTEM_LIB_PATH "/system/lib/libaudiocomponentenginec.so"
#endif

#define INIT_BLI_SRC_FUNC_NAME "InitMtkAudioSrcInC"
#define INIT_BIT_CONVERTER_FUNC_NAME "InitMtkAudioBitConverterC"


/*
 * =============================================================================
 *                     typedef
 * =============================================================================
 */

typedef void (*init_bli_src_fp_t)(struct MtkAudioSrcInC *self);
typedef void (*init_bit_converter_fp_t)(struct MtkAudioBitConverterC *self);


/*
 * =============================================================================
 *                     private global members
 * =============================================================================
 */

static const uint32_t kMaxPcmDriverBufferSize = (0x40000 + 0x2000); /* 256k + 8k */


static void *dlopen_handle; /* for dlopen libaudiocomponentenginec.so */

static init_bli_src_fp_t init_bli_src_fp;
static init_bit_converter_fp_t init_bit_converter_fp;


#ifdef AURISYS_DUMP_PCM
static const char *audio_dump = "/sdcard/mtklog/audio_dump";
static uint32_t mDumpFileNum = 0;
#endif

/*
 * =============================================================================
 *                     private function declaration
 * =============================================================================
 */

static int bli_src_init(audio_pool_buf_formatter_t *formatter);
static int bli_src_process(audio_pool_buf_formatter_t *formatter, void *pInBuffer, uint32_t inBytes, void **ppOutBuffer, uint32_t *pOutBytes);
static int bli_src_deinit(audio_pool_buf_formatter_t *formatter);

static int bit_convert_init(audio_pool_buf_formatter_t *formatter);
static int bit_convert_process(audio_pool_buf_formatter_t *formatter, void *pInBuffer, uint32_t inBytes, void **ppOutBuffer, uint32_t *pOutBytes);
static int bit_convert_deinit(audio_pool_buf_formatter_t *formatter);


/*
 * =============================================================================
 *                     utilities declaration
 * =============================================================================
 */

static BCV_PCM_FORMAT get_bcv_pcm_format(audio_format_t srcformat, audio_format_t dstformat);
static uint32_t transferFormat(char *linear_buffer, audio_format_t src_format, audio_format_t des_format, uint32_t bytes);

static double get_times_of_src(const audio_buf_t *source, const audio_buf_t *target);
static double get_times_of_bit_convert(const audio_buf_t *source, const audio_buf_t *target);

/*
 * =============================================================================
 *                     public function implementation
 * =============================================================================
 */

void audio_pool_buf_handler_c_file_init(void) {
    char *dlopen_lib_path = NULL;

    /* get dlopen_lib_path */
    if (access(AUDIO_COMPONENT_ENGINE_VENDOR_LIB_PATH, R_OK) == 0) {
        dlopen_lib_path = AUDIO_COMPONENT_ENGINE_VENDOR_LIB_PATH;
    } else if (access(AUDIO_COMPONENT_ENGINE_SYSTEM_LIB_PATH, R_OK) == 0) {
        dlopen_lib_path = AUDIO_COMPONENT_ENGINE_SYSTEM_LIB_PATH;
    } else {
        AUD_LOG_E("%s(), dlopen_lib_path not found!!", __FUNCTION__);
        AUD_ASSERT(dlopen_lib_path != NULL);
        return;
    }

    /* dlopen for libaudiocomponentenginec.so */
    dlopen_handle = dlopen(dlopen_lib_path, RTLD_NOW);
    if (dlopen_handle == NULL) {
        AUD_LOG_E("dlopen(%s) fail!!", dlopen_lib_path);
        AUD_ASSERT(dlopen_handle != NULL);
        return;
    }

    init_bli_src_fp = (init_bli_src_fp_t)dlsym(dlopen_handle, INIT_BLI_SRC_FUNC_NAME);
    if (init_bli_src_fp == NULL) {
        AUD_LOG_E("dlsym(%s) for %s fail!!", dlopen_lib_path, INIT_BLI_SRC_FUNC_NAME);
        AUD_ASSERT(init_bli_src_fp != NULL);
        return;
    }

    init_bit_converter_fp = (init_bit_converter_fp_t)dlsym(dlopen_handle, INIT_BIT_CONVERTER_FUNC_NAME);
    if (init_bit_converter_fp == NULL) {
        AUD_LOG_E("dlsym(%s) for %s fail!!", dlopen_lib_path, INIT_BIT_CONVERTER_FUNC_NAME);
        AUD_ASSERT(init_bit_converter_fp != NULL);
        return;
    }
}


void audio_pool_buf_handler_c_file_deinit(void) {
    if (dlopen_handle != NULL) {
        dlclose(dlopen_handle);
        dlopen_handle = NULL;
        init_bli_src_fp = NULL;
        init_bit_converter_fp = NULL;
    }
}


void config_ringbuf_by_data_buf(
    audio_ringbuf_t *ringbuf,
    data_buf_t      *data_buf) {
    ringbuf->base = (char *)data_buf->p_buffer;
    ringbuf->read = ringbuf->base;
    ringbuf->write = ringbuf->base;
    ringbuf->size = data_buf->memory_size;
}


void create_pool_buf(
    audio_pool_buf_t *audio_pool_buf,
    audio_buf_t      *audio_buf_pattern,
    const uint32_t    pool_buf_size) {
    AUDIO_ALLOC_STRUCT(audio_buf_t, audio_pool_buf->buf);

    /* only get the attributes */
    memcpy(audio_pool_buf->buf, audio_buf_pattern, sizeof(audio_buf_t));

    /* real buffer attributes */
    audio_pool_buf->buf->data_buf.memory_size = pool_buf_size;
    audio_pool_buf->buf->data_buf.data_size = 0;
    audio_pool_buf->buf->data_buf.p_buffer = NULL;
    AUDIO_ALLOC_BUFFER(audio_pool_buf->buf->data_buf.p_buffer,
                       audio_pool_buf->buf->data_buf.memory_size);

    config_ringbuf_by_data_buf(
        &audio_pool_buf->ringbuf,
        &audio_pool_buf->buf->data_buf);
}


void destroy_pool_buf(audio_pool_buf_t *audio_pool_buf) {
    AUDIO_FREE_POINTER(audio_pool_buf->buf->data_buf.p_buffer);
    AUDIO_FREE_POINTER(audio_pool_buf->buf);

    memset(&audio_pool_buf->ringbuf, 0, sizeof(audio_ringbuf_t));
}


void audio_pool_buf_formatter_init(audio_pool_buf_formatter_t *formatter) {
#ifdef AURISYS_DUMP_PCM
    char mDumpFileName[128];
#endif

    if (formatter == NULL) {
        AUD_LOG_E("formatter == NULL!! return");
        return;
    }

    formatter->linear_buf_size = kMaxPcmDriverBufferSize;
    AUDIO_ALLOC_CHAR_BUFFER(formatter->linear_buf, formatter->linear_buf_size);
    bli_src_init(formatter);
    bit_convert_init(formatter);


#ifdef AURISYS_DUMP_PCM
    AUDIO_ALLOC_STRUCT(PcmDump_t, formatter->pcm_dump_source);
    sprintf(mDumpFileName, "%s/%s.%d.%d.%d.source.pcm", audio_dump, LOG_TAG, mDumpFileNum, getpid(), gettid());
    InitPcmDump_t(formatter->pcm_dump_source, 32768);
    formatter->pcm_dump_source->AudioOpendumpPCMFile(
        formatter->pcm_dump_source, mDumpFileName);

    AUDIO_ALLOC_STRUCT(PcmDump_t, formatter->pcm_dump_blisrc);
    sprintf(mDumpFileName, "%s/%s.%d.%d.%d.blisrc.pcm", audio_dump, LOG_TAG, mDumpFileNum, getpid(), gettid());
    InitPcmDump_t(formatter->pcm_dump_blisrc, 32768);
    formatter->pcm_dump_blisrc->AudioOpendumpPCMFile(
        formatter->pcm_dump_blisrc, mDumpFileName);

    AUDIO_ALLOC_STRUCT(PcmDump_t, formatter->pcm_dump_target);
    sprintf(mDumpFileName, "%s/%s.%d.%d.%d.target.pcm", audio_dump, LOG_TAG, mDumpFileNum, getpid(), gettid());
    InitPcmDump_t(formatter->pcm_dump_target, 32768);
    formatter->pcm_dump_target->AudioOpendumpPCMFile(
        formatter->pcm_dump_target, mDumpFileName);

    mDumpFileNum++;
    mDumpFileNum %= MAX_DUMP_NUM;
#endif
}


void audio_pool_buf_formatter_process(audio_pool_buf_formatter_t *formatter) {
    audio_ringbuf_t *rb_in  = NULL;
    audio_ringbuf_t *rb_out = NULL;

    uint32_t data_count = 0;
    uint32_t free_count = 0;


    if (formatter == NULL) {
        AUD_ASSERT(formatter != NULL);
        return;
    }

    if (formatter->pool_source == NULL || formatter->pool_target == NULL) {
        AUD_ASSERT(formatter->pool_source != NULL);
        AUD_ASSERT(formatter->pool_target != NULL);
        return;
    }


    rb_in  = &formatter->pool_source->ringbuf;
    rb_out = &formatter->pool_target->ringbuf;

    AUD_LOG_V("%s(+), rb_in  data_count %u, free_count %u", __FUNCTION__,
              audio_ringbuf_count(rb_in), audio_ringbuf_free_space(rb_in));
    AUD_LOG_V("%s(+), rb_out data_count %u, free_count %u", __FUNCTION__,
              audio_ringbuf_count(rb_out), audio_ringbuf_free_space(rb_out));


    data_count = audio_ringbuf_count(rb_in);
    if (data_count > formatter->linear_buf_size) {
        AUD_LOG_W("%s(), data_count %u > linear_buf_size %u", __FUNCTION__, data_count, formatter->linear_buf_size);
        AUD_ASSERT(data_count <= formatter->linear_buf_size);
        data_count = formatter->linear_buf_size;
    }
    audio_ringbuf_copy_to_linear(formatter->linear_buf, rb_in, data_count);
#ifdef AURISYS_DUMP_PCM
    if (formatter->pcm_dump_source != NULL &&
        formatter->pcm_dump_source->mFilep != NULL) {
        formatter->pcm_dump_source->AudioDumpPCMData(
            formatter->pcm_dump_source, (void *)formatter->linear_buf, data_count);
    }
#endif


    // SRC
    void *pBufferAfterBliSrc = NULL;
    uint32_t bytesAfterBliSrc = 0;
    bli_src_process(formatter, formatter->linear_buf, data_count, &pBufferAfterBliSrc, &bytesAfterBliSrc);
#ifdef AURISYS_DUMP_PCM
    if (formatter->pcm_dump_blisrc != NULL &&
        formatter->pcm_dump_blisrc->mFilep != NULL) {
        formatter->pcm_dump_blisrc->AudioDumpPCMData(
            formatter->pcm_dump_blisrc, pBufferAfterBliSrc, bytesAfterBliSrc);
    }
#endif


    // bit conversion
    void *pBufferAfterBitConvertion = NULL;
    uint32_t bytesAfterBitConvertion = 0;
    bit_convert_process(formatter, pBufferAfterBliSrc, bytesAfterBliSrc, &pBufferAfterBitConvertion, &bytesAfterBitConvertion);



    // target
    free_count = audio_ringbuf_free_space(rb_out);
    if (bytesAfterBitConvertion > free_count) {
        AUD_LOG_W("%s(), bytesAfterBitConvertion %u > free_count %u. drop!!", __FUNCTION__, bytesAfterBitConvertion, free_count);
        bytesAfterBitConvertion = free_count;
    }
    audio_ringbuf_copy_from_linear(rb_out, pBufferAfterBitConvertion, bytesAfterBitConvertion);
#ifdef AURISYS_DUMP_PCM
    if (formatter->pcm_dump_target != NULL &&
        formatter->pcm_dump_target->mFilep != NULL) {
        formatter->pcm_dump_target->AudioDumpPCMData(
            formatter->pcm_dump_target, pBufferAfterBitConvertion, bytesAfterBitConvertion);
    }
#endif


    AUD_LOG_V("%s(-), rb_in  data_count %u, free_count %u", __FUNCTION__,
              audio_ringbuf_count(rb_in), audio_ringbuf_free_space(rb_in));
    AUD_LOG_V("%s(-), rb_out data_count %u, free_count %u", __FUNCTION__,
              audio_ringbuf_count(rb_out), audio_ringbuf_free_space(rb_out));
}


void audio_pool_buf_formatter_deinit(audio_pool_buf_formatter_t *formatter) {
    if (formatter == NULL) {
        AUD_LOG_E("formatter == NULL!! return");
        return;
    }

    bit_convert_deinit(formatter);
    bli_src_deinit(formatter);
    AUDIO_FREE_POINTER(formatter->linear_buf);


#ifdef AURISYS_DUMP_PCM
    if (formatter->pcm_dump_source != NULL &&
        formatter->pcm_dump_source->mFilep != NULL) {
        formatter->pcm_dump_source->AudioCloseDumpPCMFile(formatter->pcm_dump_source);
    }
    AUDIO_FREE_POINTER(formatter->pcm_dump_source);

    if (formatter->pcm_dump_target != NULL &&
        formatter->pcm_dump_target->mFilep != NULL) {
        formatter->pcm_dump_target->AudioCloseDumpPCMFile(formatter->pcm_dump_target);
    }
    AUDIO_FREE_POINTER(formatter->pcm_dump_target);
#endif
}



/*
 * =============================================================================
 *                     private implementation
 * =============================================================================
 */

static int bli_src_init(audio_pool_buf_formatter_t *formatter) {
    SRC_PCM_FORMAT src_pcm_format = SRC_IN_END;

    audio_buf_t *source = NULL;
    audio_buf_t *target = NULL;

    int ret = 0;

    source = formatter->pool_source->buf;
    target = formatter->pool_target->buf;

    // init BLI SRC if need
    if ((source->sample_rate_buffer != target->sample_rate_buffer) ||
        (source->num_channels       != target->num_channels) ||
        (source->audio_format == AUDIO_FORMAT_PCM_8_24_BIT &&
         target->audio_format == AUDIO_FORMAT_PCM_16_BIT)) {

        switch (source->audio_format) {
        case AUDIO_FORMAT_PCM_16_BIT:
            src_pcm_format = SRC_IN_Q1P15_OUT_Q1P15;
            break;
        case AUDIO_FORMAT_PCM_8_24_BIT:
            if (target->audio_format == AUDIO_FORMAT_PCM_16_BIT) {
                AUD_LOG_V("SRC_IN_Q9P23_OUT_Q1P31");
                src_pcm_format = SRC_IN_Q9P23_OUT_Q1P31; /* NOTE: sync with BCV_IN_Q1P31_OUT_Q1P15 */
            } else {
                AUD_WARNING("SRC not support AUDIO_FORMAT_PCM_8_24_BIT!!");
            }
            break;
        case AUDIO_FORMAT_PCM_32_BIT:
            src_pcm_format = SRC_IN_Q1P31_OUT_Q1P31;
            break;
        default:
            AUD_LOG_W("%s(), SRC format not support (%d->%d)", __FUNCTION__, source->audio_format, target->audio_format);
            src_pcm_format = SRC_IN_END;
            AUD_WARNING("audio_format error!");
            break;
        }

        formatter->bli_src_out_buf_size =
            (uint32_t)(formatter->linear_buf_size * get_times_of_src(source, target));
        if ((formatter->bli_src_out_buf_size % 64) != 0) {
            formatter->bli_src_out_buf_size = ((formatter->bli_src_out_buf_size / 64) + 1) * 64;
        }

        AUD_LOG_V("=>%s(), sample_rate: %d => %d, num_channels: %d => %d, audio_format: 0x%x, 0x%x, SRC_PCM_FORMAT = %d, src buf size = %u", __FUNCTION__,
                  source->sample_rate_buffer,  target->sample_rate_buffer,
                  source->num_channels, target->num_channels,
                  source->audio_format, target->audio_format,
                  src_pcm_format,
                  formatter->bli_src_out_buf_size);

        target->sample_rate_content = source->sample_rate_content;
        if (target->sample_rate_content > target->sample_rate_buffer) {
            target->sample_rate_content = target->sample_rate_buffer;
        }

        AUDIO_ALLOC_STRUCT(MtkAudioSrcInC, formatter->bli_src);
        init_bli_src_fp(formatter->bli_src);
        if (source->num_channels == 3 || source->num_channels == 4) {
            AUD_ASSERT(source->num_channels == target->num_channels);
            ret = formatter->bli_src->MultiChannel_Open(formatter->bli_src,
                                                        source->sample_rate_buffer, source->num_channels,
                                                        target->sample_rate_buffer,  target->num_channels,
                                                        src_pcm_format);
        }
        else {
            ret = formatter->bli_src->open(formatter->bli_src,
                                           source->sample_rate_buffer, source->num_channels,
                                           target->sample_rate_buffer,  target->num_channels,
                                           src_pcm_format);
        }
        AUD_ASSERT(ret == 0);

        AUDIO_ALLOC_CHAR_BUFFER(formatter->bli_src_out_buf, formatter->bli_src_out_buf_size);
    } else {
        formatter->bli_src_out_buf_size = formatter->linear_buf_size; /* for bit_convert_out_buf_size */
    }

    return 0;
}


static int bli_src_process(audio_pool_buf_formatter_t *formatter, void *pInBuffer, uint32_t inBytes, void **ppOutBuffer, uint32_t *pOutBytes) {
    if (formatter->bli_src == NULL) { // No need SRC
        *ppOutBuffer = pInBuffer;
        *pOutBytes = inBytes;
    } else {
        char *p_read = (char *)pInBuffer;
        uint32_t num_raw_data_left = inBytes;
        uint32_t num_converted_data = formatter->bli_src_out_buf_size; // max convert num_free_space

        uint32_t consumed = num_raw_data_left;
        if (formatter->pool_source->buf->num_channels == 3 ||
            formatter->pool_source->buf->num_channels == 4) {
            formatter->bli_src->MultiChannel_Process(formatter->bli_src, (int16_t *)p_read, &num_raw_data_left,
                                                     (int16_t *)formatter->bli_src_out_buf, &num_converted_data);
        }
        else {
            formatter->bli_src->Process(formatter->bli_src, (int16_t *)p_read, &num_raw_data_left,
                                        (int16_t *)formatter->bli_src_out_buf, &num_converted_data);
        }

        consumed -= num_raw_data_left;
        p_read += consumed;

        AUD_LOG_VV("%s(), num_raw_data_left = %u, num_converted_data = %u",
                   __FUNCTION__, num_raw_data_left, num_converted_data);

        if (num_raw_data_left > 0) {
            AUD_LOG_W("%s(), num_raw_data_left(%u) > 0", __FUNCTION__, num_raw_data_left);
            AUD_ASSERT(num_raw_data_left == 0);
        }

        *ppOutBuffer = formatter->bli_src_out_buf;
        *pOutBytes = num_converted_data;
    }

    //AUD_ASSERT(*ppOutBuffer != NULL && *pOutBytes != 0);

    return 0;
}


static int bli_src_deinit(audio_pool_buf_formatter_t *formatter) {
    if (formatter->bli_src != NULL) {
        formatter->bli_src->close(formatter->bli_src);
        AUDIO_FREE_POINTER(formatter->bli_src);
        AUDIO_FREE_POINTER(formatter->bli_src_out_buf);
    }

    return 0;
}


static int bit_convert_init(audio_pool_buf_formatter_t *formatter) {
    BCV_PCM_FORMAT bcv_pcm_format;

    audio_buf_t *source = NULL;
    audio_buf_t *target = NULL;

    source = formatter->pool_source->buf;
    target = formatter->pool_target->buf;

    if (source->audio_format != target->audio_format) {
        bcv_pcm_format = get_bcv_pcm_format(source->audio_format, target->audio_format);

        AUDIO_ALLOC_STRUCT(MtkAudioBitConverterC, formatter->bit_convert);
        init_bit_converter_fp(formatter->bit_convert);

        formatter->bit_convert->open(
            formatter->bit_convert,
            source->sample_rate_buffer,
            (source->num_channels > 2) ? 2 : source->num_channels,
            bcv_pcm_format);

        formatter->bit_convert->Reset(formatter->bit_convert);

        formatter->bit_convert_out_buf_size =
            (uint32_t)(formatter->bli_src_out_buf_size * get_times_of_bit_convert(source, target));
        if ((formatter->bit_convert_out_buf_size % 64) != 0) {
            formatter->bit_convert_out_buf_size = ((formatter->bit_convert_out_buf_size / 64) + 1) * 64;
        }
        AUDIO_ALLOC_CHAR_BUFFER(formatter->bit_convert_out_buf, formatter->bit_convert_out_buf_size);

        AUD_LOG_V("=>%s(), audio_format: 0x%x => 0x%x, bcv_pcm_format = 0x%x, bit_convert = %p, bit_convert_out_buf = %p, buf size = %u",
                  __FUNCTION__, source->audio_format, target->audio_format, bcv_pcm_format, formatter->bit_convert, formatter->bit_convert_out_buf, formatter->bit_convert_out_buf_size);
    }

    return 0;
}


static int bit_convert_process(audio_pool_buf_formatter_t *formatter, void *pInBuffer, uint32_t inBytes, void **ppOutBuffer, uint32_t *pOutBytes) {
    audio_buf_t *source = NULL;
    audio_buf_t *target = NULL;

    source = formatter->pool_source->buf;
    target = formatter->pool_target->buf;

    if (formatter->bit_convert != NULL) {
        *pOutBytes = formatter->bit_convert_out_buf_size;
        formatter->bit_convert->Process(formatter->bit_convert, pInBuffer, &inBytes, (void *)formatter->bit_convert_out_buf, pOutBytes);
        *ppOutBuffer = formatter->bit_convert_out_buf;
    } else {
        *ppOutBuffer = pInBuffer;
        *pOutBytes = inBytes;
    }

    //AUD_ASSERT(*ppOutBuffer != NULL && *pOutBytes != 0);
    return 0;
}


static int bit_convert_deinit(audio_pool_buf_formatter_t *formatter) {
    // deinit bit converter if need
    if (formatter->bit_convert != NULL) {
        formatter->bit_convert->close(formatter->bit_convert);
        AUDIO_FREE_POINTER(formatter->bit_convert);
    }

    AUDIO_FREE_POINTER(formatter->bit_convert_out_buf);
    return 0;
}


/*
 * =============================================================================
 *                     utilities function implementation
 * =============================================================================
 */

static BCV_PCM_FORMAT get_bcv_pcm_format(audio_format_t source, audio_format_t target) {
    BCV_PCM_FORMAT bcv_pcm_format = 0xFF;
    if (source == AUDIO_FORMAT_PCM_16_BIT) {
        if (target == AUDIO_FORMAT_PCM_8_24_BIT) {
            bcv_pcm_format = BCV_IN_Q1P15_OUT_Q9P23;
        } else if (target == AUDIO_FORMAT_PCM_32_BIT) {
            bcv_pcm_format = BCV_IN_Q1P15_OUT_Q1P31;
        }
    } else if (source == AUDIO_FORMAT_PCM_8_24_BIT) {
        if (target == AUDIO_FORMAT_PCM_16_BIT) {
            AUD_LOG_V("BCV_IN_Q1P31_OUT_Q1P15");
            bcv_pcm_format = BCV_IN_Q1P31_OUT_Q1P15; /* NOTE: sync with SRC_IN_Q9P23_OUT_Q1P31 */
        } else if (target == AUDIO_FORMAT_PCM_32_BIT) {
            bcv_pcm_format = BCV_IN_Q9P23_OUT_Q1P31;
        }
    } else if (source == AUDIO_FORMAT_PCM_32_BIT) {
        if (target == AUDIO_FORMAT_PCM_16_BIT) {
            bcv_pcm_format = BCV_IN_Q1P31_OUT_Q1P15;
        } else if (target == AUDIO_FORMAT_PCM_8_24_BIT) {
            bcv_pcm_format = BCV_IN_Q1P31_OUT_Q9P23;
        }
    }
    AUD_LOG_V("%s(), bcv_pcm_format %d", __FUNCTION__, bcv_pcm_format);
    return bcv_pcm_format;
}


static uint32_t transferFormat(char *linear_buffer, audio_format_t src_format, audio_format_t des_format, uint32_t bytes) {
    uint32_t *ptr_src_bit_r = (uint32_t *)linear_buffer;
    uint32_t src_bit = (uint32_t)AUDIO_BYTES_PER_SAMPLE(src_format);
    uint32_t des_bit = (uint32_t)AUDIO_BYTES_PER_SAMPLE(des_format);
    bool formatchanged = false;
    uint32_t i = 0;

    if (src_bit == 0 || des_bit == 0) {
        AUD_LOG_E("Cannot get bytes per sample for audio_format_t (src_format = %d, des_format = %d)\n", src_format, des_format);
        return 0;
    }

    if (des_format == AUDIO_FORMAT_PCM_24_BIT_PACKED) { //convert 8+24 to 24 bit
        char *ptr_des_bit_r = linear_buffer;
        int32_t *ptr_des_bit_w = 0;
        if (src_format == AUDIO_FORMAT_PCM_8_24_BIT) {
            for (i = 1; i < (bytes / src_bit); i++) {
                ptr_des_bit_r = ptr_des_bit_r + 3;
                ptr_des_bit_w = (int32_t *)ptr_des_bit_r;
                memcpy(ptr_des_bit_w, (ptr_src_bit_r + i), sizeof(int));
            }
            formatchanged = true;
        }
    }
    if (des_format == AUDIO_FORMAT_PCM_16_BIT) { //convert 8+24 to 16 bit
        int16_t *ptr_des_bit_w = (int16_t *)linear_buffer;
        if (src_format == AUDIO_FORMAT_PCM_8_24_BIT) {
            for (i = 0; i < (bytes / src_bit); i++) {
                *(ptr_des_bit_w + i) = (int16_t)(*(ptr_src_bit_r + i) >> 8);
            }
            formatchanged = true;
        }
    }
    //AUD_ASSERT(formatchanged == true);
    AUD_ASSERT(src_bit != 0);
    bytes = bytes * des_bit / src_bit;
    return bytes;
}


static double get_times_of_src(const audio_buf_t *source, const audio_buf_t *target) {
    uint32_t bytes_per_sample_source = (uint32_t)AUDIO_BYTES_PER_SAMPLE(source->audio_format);
    uint32_t bytes_per_sample_target = (uint32_t)AUDIO_BYTES_PER_SAMPLE(target->audio_format);

    uint32_t unit_bytes_source = source->sample_rate_buffer * source->num_channels * bytes_per_sample_source;
    uint32_t unit_bytes_target = target->sample_rate_buffer * target->num_channels * bytes_per_sample_target;

    double times = 1.0;

    if (unit_bytes_source == 0 || unit_bytes_target == 0) {
        AUD_LOG_W("%s(), audio_format: 0x%x, 0x%x error!!", __FUNCTION__,
                  source->audio_format, target->audio_format);
        return 1.0;
    }

    times = (double)unit_bytes_target / (double)unit_bytes_source;

    if (source->audio_format == AUDIO_FORMAT_PCM_8_24_BIT &&
        target->audio_format == AUDIO_FORMAT_PCM_16_BIT) { /* SRC_IN_Q9P23_OUT_Q1P31 */
        times *= 2.0;
    }

    AUD_LOG_V("%s(), times = %lf", __FUNCTION__, times);
    return times;
}


static double get_times_of_bit_convert(const audio_buf_t *source, const audio_buf_t *target) {
    uint32_t bytes_per_sample_source = (uint32_t)AUDIO_BYTES_PER_SAMPLE(source->audio_format);
    uint32_t bytes_per_sample_target = (uint32_t)AUDIO_BYTES_PER_SAMPLE(target->audio_format);

    double times = 1.0;

    if (bytes_per_sample_source == 0 || bytes_per_sample_target == 0) {
        AUD_LOG_W("%s(), audio_format: 0x%x, 0x%x error!!", __FUNCTION__,
                  source->audio_format, target->audio_format);
        return 1.0;
    }

    times = (double)bytes_per_sample_target / (double)bytes_per_sample_source;

    AUD_LOG_V("%s(), times = %lf", __FUNCTION__, times);
    return times;
}



#ifdef __cplusplus
}  /* extern "C" */
#endif

