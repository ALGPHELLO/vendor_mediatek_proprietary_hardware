#include "aurisys_lib_handler.h"

#include <string.h>
#include <stdarg.h> /* va_list, va_start, va_arg, va_end */

#include <sys/prctl.h>

#include <uthash.h> /* uthash */
#include <utlist.h> /* linked list */

#include <wrapped_audio.h>
#include <wrapped_errors.h>

#include <audio_log.h>
#include <audio_assert.h>
#include <audio_debug_tool.h>
#include <audio_memory_control.h>
#include <audio_lock.h>
#include <audio_ringbuf.h>
#include <audio_sample_rate.h>

#include <audio_task.h>

#include <arsi_type.h>
#include <aurisys_config.h>

#include <audio_pool_buf_handler.h>

#include <arsi_api.h>

#include <aurisys_utility.h>

#include <aurisys_adb_command.h>

#include <AudioAurisysPcmDump.h>


#ifdef __cplusplus
extern "C" {
#endif


#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "aurisys_lib_handler"


#ifdef AURISYS_DUMP_LOG_V
#undef  AUD_LOG_V
#define AUD_LOG_V(x...) AUD_LOG_D(x)
#endif


/*
 * =============================================================================
 *                     MACRO
 * =============================================================================
 */

#define DEFAULT_PRODUCT_NAME "vendor=mediatek,model=k97v1_64_op02_lwg_ss_dsp_mp3,device=k97v1_64"

#define MAX_POOL_BUF_SIZE (0x20000) /* 128K */
#define MAX_LIB_BUF_SIZE  (0x20000) /* 128K */
#define MAX_LIB_PARAM_SIZE (0x4000) /*  16K */

#define RPOCESS_DATA_SIZE_PER_TIME (0x1000)


#define LINEAR_TO_FRAME_2_CH(linear_buf, frame_buf, data_size, type) \
    do { \
        uint32_t channel_size = (data_size >> 1); \
        type *linear = (type *)(linear_buf); \
        type *frame_ch1 = (type *)(frame_buf); \
        type *frame_ch2 = (type *)(((char *)frame_ch1) + channel_size); \
        uint32_t num_sample = channel_size / sizeof(type); \
        uint32_t i = 0; \
        uint32_t j = 0; \
        for (i = 0; i < num_sample; i++) { \
            j = i << 1; \
            frame_ch1[i] = linear[j]; \
            frame_ch2[i] = linear[j + 1]; \
        } \
    } while(0)


#define LINEAR_TO_FRAME_3_CH(linear_buf, frame_buf, data_size, type) \
    do { \
        uint32_t channel_size = (data_size / 3); \
        type *linear = (type *)(linear_buf); \
        type *frame_ch1 = (type *)(frame_buf); \
        type *frame_ch2 = (type *)(((char *)frame_ch1) + channel_size); \
        type *frame_ch3 = (type *)(((char *)frame_ch2) + channel_size); \
        uint32_t num_sample = channel_size / sizeof(type); \
        uint32_t i = 0; \
        uint32_t j = 0; \
        for (i = 0; i < num_sample; i++) { \
            j = i * 3; \
            frame_ch1[i] = linear[j]; \
            frame_ch2[i] = linear[j + 1]; \
            frame_ch3[i] = linear[j + 2]; \
        } \
    } while(0)


#define LINEAR_TO_FRAME_4_CH(linear_buf, frame_buf, data_size, type) \
    do { \
        uint32_t channel_size = (data_size >> 2); \
        type *linear = (type *)(linear_buf); \
        type *frame_ch1 = (type *)(frame_buf); \
        type *frame_ch2 = (type *)(((char *)frame_ch1) + channel_size); \
        type *frame_ch3 = (type *)(((char *)frame_ch2) + channel_size); \
        type *frame_ch4 = (type *)(((char *)frame_ch3) + channel_size); \
        uint32_t num_sample = channel_size / sizeof(type); \
        uint32_t i = 0; \
        uint32_t j = 0; \
        for (i = 0; i < num_sample; i++) { \
            j = i << 2; \
            frame_ch1[i] = linear[j]; \
            frame_ch2[i] = linear[j + 1]; \
            frame_ch3[i] = linear[j + 2]; \
            frame_ch4[i] = linear[j + 3]; \
        } \
    } while(0)


#define FRAME_TO_LINEAR_2_CH(frame_buf, linear_buf, data_size, type) \
    do { \
        uint32_t channel_size = (data_size >> 1); \
        type *linear = (type *)(linear_buf); \
        type *frame_ch1 = (type *)(frame_buf); \
        type *frame_ch2 = (type *)(((char *)frame_ch1) + channel_size); \
        uint32_t num_sample = channel_size / sizeof(type); \
        uint32_t i = 0; \
        uint32_t j = 0; \
        for (i = 0; i < num_sample; i++) { \
            j = i << 1; \
            linear[j]     = frame_ch1[i]; \
            linear[j + 1] = frame_ch2[i]; \
        } \
    } while(0)


#define FRAME_TO_LINEAR_3_CH(frame_buf, linear_buf, data_size, type) \
    do { \
        uint32_t channel_size = (data_size / 3); \
        type *linear = (type *)(linear_buf); \
        type *frame_ch1 = (type *)(frame_buf); \
        type *frame_ch2 = (type *)(((char *)frame_ch1) + channel_size); \
        type *frame_ch3 = (type *)(((char *)frame_ch2) + channel_size); \
        uint32_t num_sample = channel_size / sizeof(type); \
        uint32_t i = 0; \
        uint32_t j = 0; \
        for (i = 0; i < num_sample; i++) { \
            j = i * 3; \
            linear[j]     = frame_ch1[i]; \
            linear[j + 1] = frame_ch2[i]; \
            linear[j + 2] = frame_ch3[i]; \
        } \
    } while(0)


#define FRAME_TO_LINEAR_4_CH(frame_buf, linear_buf, data_size, type) \
    do { \
        uint32_t channel_size = (data_size >> 2); \
        type *linear = (type *)(linear_buf); \
        type *frame_ch1 = (type *)(frame_buf); \
        type *frame_ch2 = (type *)(((char *)frame_ch1) + channel_size); \
        type *frame_ch3 = (type *)(((char *)frame_ch2) + channel_size); \
        type *frame_ch4 = (type *)(((char *)frame_ch3) + channel_size); \
        uint32_t num_sample = channel_size / sizeof(type); \
        uint32_t i = 0; \
        uint32_t j = 0; \
        for (i = 0; i < num_sample; i++) { \
            j = i << 2; \
            linear[j]     = frame_ch1[i]; \
            linear[j + 1] = frame_ch2[i]; \
            linear[j + 2] = frame_ch3[i]; \
            linear[j + 3] = frame_ch4[i]; \
        } \
    } while(0)



/*
 * =============================================================================
 *                     typedef
 * =============================================================================
 */



/*
 * =============================================================================
 *                     private global members
 * =============================================================================
 */

static alock_t *g_aurisys_lib_handler_lock;

static char gPhoneProductName[256];

static const char *audio_dump = "/sdcard/mtklog/audio_dump";
static uint32_t mDumpFileNum = 0;


/*
 * =============================================================================
 *                     private function declaration
 * =============================================================================
 */

static void arsi_lib_printf(const char *message, ...);
static void arsi_lib_printf_dummy(const char *message, ...);

static void allocate_data_buf(audio_buf_t *audio_buf);
/* NOTE: free data buf in release_lib_config */

static void init_audio_buf_by_lib_config(
    audio_buf_t *audio_buf,
    arsi_lib_config_t *lib_config);

static void clone_lib_config(
    arsi_lib_config_t *des,
    arsi_lib_config_t *src,
    const aurisys_component_t *the_component,
    const struct aurisys_user_prefer_configs_t *p_prefer_configs);
static void release_lib_config(arsi_lib_config_t *lib_config);

static void init_pool_buf(aurisys_lib_handler_t *lib_handler);
static void deinit_pool_buf(aurisys_lib_handler_t *lib_handler);


static void aurisys_create_lib_handler_list_xlink(
    aurisys_scenario_t aurisys_scenario,
    arsi_process_type_t arsi_process_type,
    aurisys_library_name_t *name_list,
    aurisys_library_config_t *library_config_list,
    aurisys_lib_handler_t **lib_handler_list,
    uint32_t *num_library_hanlder,
    const struct aurisys_user_prefer_configs_t *p_prefer_configs);

static void aurisys_destroy_lib_handler_list_xlink(
    aurisys_lib_handler_t **handler_list);


static status_t aurisys_parsing_param_file(
    AurisysLibInterface      *arsi_api,
    const arsi_task_config_t *arsi_task_config,
    const arsi_lib_config_t  *arsi_lib_config,
    char                     *product_info_char,
    char                     *param_file_path_char,
    const int32_t             enhancement_mode,
    data_buf_t               *param_buf,
    const debug_log_fp_t      debug_log_fp);


/*
 * =============================================================================
 *                     utilities declaration
 * =============================================================================
 */

static uint32_t get_size_per_channel(const audio_format_t audio_format);

static uint32_t get_frame_buf_size(const audio_buf_t *audio_buf);

static void char_to_string(string_buf_t *target, char *source);

static int linear_to_frame_base(char *linear_buf, char *frame_buf, uint32_t data_size, audio_format_t audio_format, uint8_t num_channels);
static int frame_base_to_linear(char *frame_buf, char *linear_buf, uint32_t data_size, audio_format_t audio_format, uint8_t num_channels);


/*
 * =============================================================================
 *                     public function implementation
 * =============================================================================
 */

void aurisys_lib_handler_c_file_init(void) {
    if (g_aurisys_lib_handler_lock == NULL) {
        NEW_ALOCK(g_aurisys_lib_handler_lock);
    }

#if 1 /* TODO */
    //property_get(PROPERTY_KEY_PRODUCT_NAME, gPhoneProductName, DEFAULT_PRODUCT_NAME);
    //ALOGV("%s(), gPhoneProductName = %s", __FUNCTION__, gPhoneProductName);
    strncpy(gPhoneProductName, DEFAULT_PRODUCT_NAME, sizeof(gPhoneProductName) - 1);
#endif
}


void aurisys_lib_handler_c_file_deinit(void) {
#if 0 /* singleton lock */
    if (g_aurisys_lib_handler_lock != NULL) {
        FREE_ALOCK(g_aurisys_lib_handler_lock);
    }
#endif
}


void aurisys_create_lib_handler_list(
    aurisys_config_t *aurisys_config,
    aurisys_scenario_t aurisys_scenario,
    aurisys_lib_handler_t **uplink_lib_handler_list,
    uint32_t *num_uplink_library_hanlder,
    aurisys_lib_handler_t **downlink_lib_handler_list,
    uint32_t *num_downlink_library_hanlder,
    const struct aurisys_user_prefer_configs_t *p_prefer_configs) {
    aurisys_scene_lib_table_t *the_scene_lib_table = NULL;


    LOCK_ALOCK_MS(g_aurisys_lib_handler_lock, 1000);

    /* get scene_lib_table */
    HASH_FIND_INT(
        aurisys_config->scene_lib_table_hh,
        &aurisys_scenario,
        the_scene_lib_table);
    if (the_scene_lib_table == NULL) {
        AUD_ASSERT(the_scene_lib_table != NULL);
        UNLOCK_ALOCK(g_aurisys_lib_handler_lock);
        return;
    }

    /* TODO: phase 1: UL/DL idep. handler => phase 2: same hanlder */

    /* create uplink_lib_handler_list libs */
    if (the_scene_lib_table->uplink_library_name_list != NULL) {
        AUD_LOG_V("%s(), uplink_lib_handler_list", __FUNCTION__);
        aurisys_create_lib_handler_list_xlink(
            aurisys_scenario,
            ARSI_PROCESS_TYPE_UL_ONLY,
            the_scene_lib_table->uplink_library_name_list,
            aurisys_config->library_config_hh,
            uplink_lib_handler_list,
            num_uplink_library_hanlder,
            p_prefer_configs);
    }

    /* create downlink_lib_handler_list libs */
    if (the_scene_lib_table->downlink_library_name_list != NULL) {
        AUD_LOG_V("%s(), downlink_lib_handler_list", __FUNCTION__);
        aurisys_create_lib_handler_list_xlink(
            aurisys_scenario,
            ARSI_PROCESS_TYPE_DL_ONLY,
            the_scene_lib_table->downlink_library_name_list,
            aurisys_config->library_config_hh,
            downlink_lib_handler_list,
            num_downlink_library_hanlder,
            p_prefer_configs);
    }

    UNLOCK_ALOCK(g_aurisys_lib_handler_lock);
}


void aurisys_destroy_lib_handler_list(aurisys_lib_handler_t **handler_list) {
    LOCK_ALOCK_MS(g_aurisys_lib_handler_lock, 1000);

    aurisys_destroy_lib_handler_list_xlink(handler_list);

    UNLOCK_ALOCK(g_aurisys_lib_handler_lock);
}


void aurisys_arsi_create_handler(
    aurisys_lib_handler_t *lib_handler,
    const arsi_task_config_t  *arsi_task_config) {
    AurisysLibInterface *arsi_api = NULL;
    void                *arsi_handler  = NULL;
    arsi_lib_config_t   *arsi_lib_config  = NULL;

    data_buf_t          *working_buf;
    data_buf_t          *param_buf;

    status_t             retval = NO_ERROR;


    LOCK_ALOCK_MS(lib_handler->lock, 500);

    /* incase the arsi_handler is already created */
    if (lib_handler->arsi_handler != NULL) {
        AUD_LOG_W("%s(-), lib_handler->arsi_handler != NULL", __FUNCTION__);
        UNLOCK_ALOCK(lib_handler->lock);
        return;
    }

    arsi_api = lib_handler->api;
    lib_handler->task_config = arsi_task_config; /* ptr -> manager's task cfg */
    arsi_lib_config = &lib_handler->lib_config;

    working_buf = &lib_handler->working_buf;
    param_buf = &lib_handler->param_buf;

    aurisys_parsing_param_file(
        arsi_api,
        arsi_task_config,
        arsi_lib_config,
        gPhoneProductName,
        lib_handler->param_path,
        *lib_handler->enhancement_mode,
        param_buf,
        lib_handler->debug_log_fp);


    retval = arsi_api->arsi_query_working_buf_size(arsi_task_config, arsi_lib_config, &working_buf->memory_size, lib_handler->debug_log_fp);
    if (working_buf->memory_size == 0 || retval != NO_ERROR) {
        AUD_LOG_E("%s(), lib_name %s, %p, working_buf->memory_size %u, retval %d",
                  __FUNCTION__, lib_handler->lib_name, lib_handler, working_buf->memory_size, retval);
        AUD_ASSERT(working_buf->memory_size != 0);
        AUD_ASSERT(retval == NO_ERROR);
        UNLOCK_ALOCK(lib_handler->lock);
        return;
    }
    AUDIO_ALLOC_BUFFER(working_buf->p_buffer, working_buf->memory_size);

    retval = arsi_api->arsi_create_handler(arsi_task_config, arsi_lib_config, param_buf, working_buf, &arsi_handler, lib_handler->debug_log_fp);
    AUD_LOG_D("%s(), lib_name %s, %p, memory_size %u, arsi_handler %p, retval %d",
              __FUNCTION__, lib_handler->lib_name, lib_handler, working_buf->memory_size,
              arsi_handler, retval);
    AUD_ASSERT(retval == NO_ERROR);
    AUD_ASSERT(arsi_handler != NULL);
    lib_handler->arsi_handler = arsi_handler;

    arsi_api->arsi_set_debug_log_fp(lib_handler->debug_log_fp, lib_handler->arsi_handler);

    if (lib_handler->lib_dump_enabled) {
        retval = arsi_api->arsi_query_max_debug_dump_buf_size(
                     &lib_handler->lib_dump_buf, lib_handler->arsi_handler);
        AUD_LOG_D("%s(), arsi_query_max_debug_dump_buf_size, retval: %d, memory_size: %u",
                  __FUNCTION__, retval, lib_handler->lib_dump_buf.memory_size);

        if (retval == NO_ERROR &&
            lib_handler->lib_dump_buf.memory_size > 0) {
            AUDIO_ALLOC_CHAR_BUFFER(lib_handler->lib_dump_buf.p_buffer,
                                    lib_handler->lib_dump_buf.memory_size);
        }
    }

    UNLOCK_ALOCK(lib_handler->lock);
}


void aurisys_arsi_destroy_handler(aurisys_lib_handler_t *lib_handler) {
    AurisysLibInterface *arsi_api = NULL;

    status_t retval = NO_ERROR;


    LOCK_ALOCK_MS(lib_handler->lock, 500);

    /* incase the arsi_handler is already destroyed */
    if (lib_handler->arsi_handler == NULL) {
        AUD_LOG_W("%s(), itor_lib_hanlder->arsi_handler == NULL", __FUNCTION__);
        UNLOCK_ALOCK(lib_handler->lock);
        return;
    }

    arsi_api = lib_handler->api;

    retval = arsi_api->arsi_destroy_handler(lib_handler->arsi_handler);
    AUD_LOG_D("%s(), lib_name %s, %p, arsi_destroy_handler, arsi_handler = %p, retval = %d",
              __FUNCTION__, lib_handler->lib_name, lib_handler, lib_handler->arsi_handler, retval);
    lib_handler->arsi_handler = NULL;

    AUDIO_FREE_POINTER(lib_handler->working_buf.p_buffer);


    UNLOCK_ALOCK(lib_handler->lock);
}


int aurisys_arsi_process_ul_only(aurisys_lib_handler_t *lib_handler) {
    int process_count = 0;

    AurisysLibInterface *arsi_api = NULL;

    audio_ringbuf_t *rb_in = NULL;
    audio_ringbuf_t *rb_out = NULL;
    audio_ringbuf_t *rb_aec = NULL;

    audio_buf_t *p_ul_buf_in = NULL;
    audio_buf_t *p_ul_buf_out = NULL;
    audio_buf_t *p_ul_ref_bufs = NULL;
    audio_buf_t *p_ul_buf_aec = NULL;

    data_buf_t *lib_dump_buf = NULL;

    uint32_t frame_buf_size_ul_in = 0;
    uint32_t frame_buf_size_ul_out = 0;
    uint32_t frame_buf_size_ul_aec = 0;

    audio_ringbuf_t *rb_linear_buf = NULL;


    uint32_t pool_in_data_count = 0;
    uint32_t retrieve_data_count = 0;

    uint32_t data_count = 0;
    uint32_t free_count = 0;

    status_t retval = NO_ERROR;

    audio_buf_t fake_aec_buf;
    char        fake_aec_mem[640] = {0};


    LOCK_ALOCK_MS(lib_handler->lock, 500);

    arsi_api = lib_handler->api;

    rb_in = &lib_handler->ul_pool_in.ringbuf;
    rb_out = &lib_handler->ul_pool_out.ringbuf;
    AUD_LOG_V("%s(+), rb_in  data_count %u, free_count %u", __FUNCTION__,
              audio_ringbuf_count(rb_in), audio_ringbuf_free_space(rb_in));
    AUD_LOG_V("%s(+), rb_out data_count %u, free_count %u", __FUNCTION__,
              audio_ringbuf_count(rb_out), audio_ringbuf_free_space(rb_out));

    if (lib_handler->aec_pool_in != NULL) {
        rb_aec = &lib_handler->aec_pool_in->ringbuf;
        AUD_LOG_V("%s(+), rb_aec data_count %u, free_count %u", __FUNCTION__,
                  audio_ringbuf_count(rb_aec), audio_ringbuf_free_space(rb_aec));
    }


    AUD_ASSERT(lib_handler->lib_config.p_ul_buf_in != NULL);
    AUD_ASSERT(lib_handler->lib_config.p_ul_buf_out != NULL);

    p_ul_buf_in   = lib_handler->lib_config.p_ul_buf_in;
    p_ul_buf_out  = lib_handler->lib_config.p_ul_buf_out;
    p_ul_ref_bufs = lib_handler->lib_config.p_ul_ref_bufs;
    p_ul_buf_aec  = lib_handler->p_ul_ref_buf_aec;

    /* fake AEC for VoIP w/o AEC */
    if (lib_handler->task_config->task_scene == TASK_SCENE_VOIP && p_ul_ref_bufs == NULL) {
        p_ul_ref_bufs = &fake_aec_buf;
        p_ul_ref_bufs->data_buf.memory_size = 640;
        p_ul_ref_bufs->data_buf.data_size = 640;
        p_ul_ref_bufs->data_buf.p_buffer = fake_aec_mem;

        p_ul_ref_bufs->data_buf_type = DATA_BUF_ECHO_REF;
        p_ul_ref_bufs->num_channels = 1;
        p_ul_ref_bufs->channel_mask = AUDIO_CHANNEL_IN_MONO;
        p_ul_ref_bufs->sample_rate_buffer = 16000;
        p_ul_ref_bufs->sample_rate_content = 16000;
        p_ul_ref_bufs->audio_format = AUDIO_FORMAT_PCM_16_BIT;
    }


    AUD_ASSERT(p_ul_buf_in->data_buf.p_buffer != NULL);
    AUD_ASSERT(p_ul_buf_out->data_buf.p_buffer != NULL);

    rb_linear_buf = &lib_handler->rb_linear_buf;
    AUD_ASSERT(rb_linear_buf->base != NULL);

    if (lib_handler->lib_dump_enabled && lib_handler->lib_dump_buf.p_buffer != NULL) {
        lib_dump_buf = &lib_handler->lib_dump_buf;
    }


    if (lib_handler->lib_config.b_interleave == 0) { /* frame base */
        frame_buf_size_ul_in = get_frame_buf_size(p_ul_buf_in);
        frame_buf_size_ul_out = get_frame_buf_size(p_ul_buf_out);

        AUD_ASSERT(p_ul_buf_in->data_buf.memory_size == frame_buf_size_ul_in);
        AUD_ASSERT(p_ul_buf_out->data_buf.memory_size == frame_buf_size_ul_out);

        if (p_ul_buf_aec != NULL) {
            frame_buf_size_ul_aec = get_frame_buf_size(p_ul_buf_aec);
            AUD_ASSERT(p_ul_buf_aec->data_buf.memory_size == frame_buf_size_ul_aec);
            if (rb_aec != NULL) {
                AUD_ASSERT(audio_ringbuf_count(rb_aec) >= frame_buf_size_ul_aec);
            }
        }

        pool_in_data_count = audio_ringbuf_count(rb_in);
        while (pool_in_data_count >= frame_buf_size_ul_in) { /* once per frame */
            if (rb_aec != NULL) { /* once per frame */
                if (audio_ringbuf_count(rb_aec) < frame_buf_size_ul_aec) {
                    break;
                }
            }

            pool_in_data_count -= frame_buf_size_ul_in;

            /* get data from pool */
            audio_ringbuf_copy_to_linear(lib_handler->linear_buf, rb_in, frame_buf_size_ul_in);
            if (lib_handler->raw_dump_enabled == 1 &&
                lib_handler->pcm_dump_ul_in != NULL &&
                lib_handler->pcm_dump_ul_in->mFilep != NULL) {
                lib_handler->pcm_dump_ul_in->AudioDumpPCMData(
                    lib_handler->pcm_dump_ul_in,
                    lib_handler->linear_buf,
                    frame_buf_size_ul_in);
            }

            /* frame base transfer */
            linear_to_frame_base(
                lib_handler->linear_buf,
                p_ul_buf_in->data_buf.p_buffer,
                frame_buf_size_ul_in,
                p_ul_buf_in->audio_format,
                p_ul_buf_in->num_channels);
            p_ul_buf_in->data_buf.data_size = frame_buf_size_ul_in;


            if (rb_aec != NULL && p_ul_buf_aec != NULL) {
                if (p_ul_buf_aec->num_channels == 1) {
                    audio_ringbuf_copy_to_linear(p_ul_buf_aec->data_buf.p_buffer, rb_aec, frame_buf_size_ul_aec);
                } else {
                    audio_ringbuf_copy_to_linear(lib_handler->linear_buf, rb_aec, frame_buf_size_ul_aec);
                    linear_to_frame_base(
                        lib_handler->linear_buf,
                        p_ul_buf_aec->data_buf.p_buffer,
                        frame_buf_size_ul_aec,
                        p_ul_buf_aec->audio_format,
                        p_ul_buf_aec->num_channels);
                }
                p_ul_buf_aec->data_buf.data_size = frame_buf_size_ul_aec;

                if (lib_handler->raw_dump_enabled == 1 &&
                    lib_handler->pcm_dump_aec != NULL &&
                    lib_handler->pcm_dump_aec->mFilep != NULL) {
                    lib_handler->pcm_dump_aec->AudioDumpPCMData(
                        lib_handler->pcm_dump_aec,
                        p_ul_buf_aec->data_buf.p_buffer,
                        p_ul_buf_aec->data_buf.data_size);
                }
            }

            /* process */
            AUD_ASSERT(p_ul_buf_out->data_buf.data_size == 0);
#if 0
            memcpy(p_ul_buf_out->data_buf.p_buffer, p_ul_buf_in->data_buf.p_buffer, frame_buf_size_ul_out);
            p_ul_buf_out->data_buf.data_size = frame_buf_size_ul_out;
            p_ul_buf_in->data_buf.data_size = 0;
#else
            retval = arsi_api->arsi_process_ul_buf(
                         p_ul_buf_in,
                         p_ul_buf_out,
                         p_ul_ref_bufs,
                         lib_dump_buf,
                         lib_handler->arsi_handler);
            if (retval != NO_ERROR) {
                AUD_LOG_W("lib_handler %p, arsi_handler %p, arsi_process_ul_buf retval = %d",
                          lib_handler, lib_handler->arsi_handler, retval);
            }
#endif
            AUD_ASSERT(p_ul_buf_in->data_buf.data_size == 0);
            AUD_ASSERT(p_ul_buf_out->data_buf.data_size != 0);


            if (p_ul_buf_in->data_buf.data_size != 0) {
                AUD_LOG_W("%s(), p_ul_buf_in->data_buf.data_size %u != 0",
                          __FUNCTION__, p_ul_buf_in->data_buf.data_size); /* TODO: drop !? */
            }
            if (p_ul_buf_out->data_buf.data_size != frame_buf_size_ul_out) {
                AUD_LOG_E("%s(), p_ul_buf_out->data_buf.data_size %u != %u",
                          __FUNCTION__, p_ul_buf_out->data_buf.data_size, frame_buf_size_ul_out);
                if (p_ul_buf_out->data_buf.data_size == 0) {
                    p_ul_buf_out->data_buf.data_size = frame_buf_size_ul_out;
                } else {
                    AUD_ASSERT(0);
                }
            }

            /* linear base transfer */
            frame_base_to_linear(
                p_ul_buf_out->data_buf.p_buffer,
                lib_handler->linear_buf,
                p_ul_buf_out->data_buf.data_size,
                p_ul_buf_out->audio_format,
                p_ul_buf_out->num_channels);
            p_ul_buf_out->data_buf.data_size = 0;


            if (lib_handler->raw_dump_enabled == 1 &&
                lib_handler->pcm_dump_ul_out != NULL &&
                lib_handler->pcm_dump_ul_out->mFilep != NULL) {
                lib_handler->pcm_dump_ul_out->AudioDumpPCMData(
                    lib_handler->pcm_dump_ul_out,
                    lib_handler->linear_buf,
                    frame_buf_size_ul_out);
            }

            if (lib_handler->lib_dump_enabled == 1 &&
                lib_handler->lib_dump != NULL &&
                lib_handler->lib_dump->mFilep != NULL &&
                lib_dump_buf != NULL) {
                AUD_ASSERT(lib_dump_buf->data_size != 0);
                lib_handler->lib_dump->AudioDumpPCMData(
                    lib_handler->lib_dump,
                    lib_dump_buf->p_buffer,
                    lib_dump_buf->data_size);
                lib_dump_buf->data_size = 0;
            }


            free_count = audio_ringbuf_free_space(rb_out);
            if (frame_buf_size_ul_out > free_count) {
                AUD_LOG_W("%s(), frame_buf_size_ul_out %u > free_count %u",
                          __FUNCTION__, frame_buf_size_ul_out, free_count);

                audio_ringbuf_copy_from_linear(rb_out, lib_handler->linear_buf, free_count); /* drop some !! */
                process_count += free_count;
            } else {
                audio_ringbuf_copy_from_linear(rb_out, lib_handler->linear_buf, frame_buf_size_ul_out);
                process_count += frame_buf_size_ul_out;
            }
        }
    } else {
        /* TODO: abstract function */
        pool_in_data_count = audio_ringbuf_count(rb_in);
        while (pool_in_data_count >= RPOCESS_DATA_SIZE_PER_TIME) {
            AUD_ASSERT(audio_ringbuf_count(rb_linear_buf) == 0); /* init: no data left in linear buf */

            retrieve_data_count = RPOCESS_DATA_SIZE_PER_TIME; /* once per frame */

            /* concatenate remain data last time */
            if (p_ul_buf_in->data_buf.data_size != 0) {
                audio_ringbuf_copy_from_linear(
                    rb_linear_buf,
                    (((char *)p_ul_buf_in->data_buf.p_buffer) + p_ul_buf_in->data_buf.data_size),
                    p_ul_buf_in->data_buf.data_size);
                retrieve_data_count -= p_ul_buf_in->data_buf.data_size;
                p_ul_buf_in->data_buf.data_size = 0;
            }

            /* get data from pool to rb_linear_buf */
            audio_ringbuf_copy_from_ringbuf(rb_linear_buf, rb_in, retrieve_data_count);
            pool_in_data_count -= retrieve_data_count;


            /* get data from rb_linear_buf to p_ul_buf_in->data_buf.p_buffer */
            data_count = audio_ringbuf_count(rb_linear_buf);
            AUD_ASSERT(data_count == RPOCESS_DATA_SIZE_PER_TIME);

            audio_ringbuf_copy_to_linear(p_ul_buf_in->data_buf.p_buffer, rb_linear_buf, RPOCESS_DATA_SIZE_PER_TIME);
            AUD_ASSERT(audio_ringbuf_count(rb_linear_buf) == 0); /* deinit: no data left in linear buf */

            p_ul_buf_in->data_buf.data_size = RPOCESS_DATA_SIZE_PER_TIME;


            /* process */
            AUD_ASSERT(p_ul_buf_out->data_buf.data_size == 0);
            retval = arsi_api->arsi_process_ul_buf(
                         p_ul_buf_in,
                         p_ul_buf_out,
                         NULL,
                         lib_dump_buf,
                         lib_handler->arsi_handler);
            if (retval != NO_ERROR) {
                AUD_LOG_W("lib_handler %p, arsi_handler %p, arsi_process_ul_buf retval = %d",
                          lib_handler, lib_handler->arsi_handler, retval);
            }
            //AUD_ASSERT(p_ul_buf_in->data_buf.data_size == 0);
            AUD_ASSERT(p_ul_buf_out->data_buf.data_size != 0);


            if (lib_handler->raw_dump_enabled == 1) {
                if (lib_handler->pcm_dump_ul_in != NULL &&
                    lib_handler->pcm_dump_ul_in->mFilep != NULL) {
                    lib_handler->pcm_dump_ul_in->AudioDumpPCMData(
                        lib_handler->pcm_dump_ul_in,
                        p_ul_buf_in->data_buf.p_buffer,
                        RPOCESS_DATA_SIZE_PER_TIME - p_ul_buf_in->data_buf.data_size);
                }
                if (lib_handler->pcm_dump_ul_out != NULL &&
                    lib_handler->pcm_dump_ul_out->mFilep != NULL) {
                    lib_handler->pcm_dump_ul_out->AudioDumpPCMData(
                        lib_handler->pcm_dump_ul_out,
                        p_ul_buf_out->data_buf.p_buffer,
                        p_ul_buf_out->data_buf.data_size);
                }
            }

            if (lib_handler->lib_dump_enabled == 1 &&
                lib_handler->lib_dump != NULL &&
                lib_handler->lib_dump->mFilep != NULL &&
                lib_dump_buf != NULL) {
                AUD_ASSERT(lib_dump_buf->data_size != 0);
                lib_handler->lib_dump->AudioDumpPCMData(
                    lib_handler->lib_dump,
                    lib_dump_buf->p_buffer,
                    lib_dump_buf->data_size);
                lib_dump_buf->data_size = 0;
            }


            free_count = audio_ringbuf_free_space(rb_out);
            if (p_ul_buf_out->data_buf.data_size > free_count) {
                AUD_LOG_W("%s(), p_ul_buf_out->data_buf.data_size %u > free_count %u",
                          __FUNCTION__, p_ul_buf_out->data_buf.data_size, free_count);

                audio_ringbuf_copy_from_linear(rb_out, p_ul_buf_out->data_buf.p_buffer, free_count); /* drop some !! */
                process_count += free_count;
            } else {
                audio_ringbuf_copy_from_linear(rb_out, p_ul_buf_out->data_buf.p_buffer, p_ul_buf_out->data_buf.data_size);
                process_count += p_ul_buf_out->data_buf.data_size;
            }
            p_ul_buf_out->data_buf.data_size = 0;
        }
    }

    AUD_LOG_V("%s(-), rb_in  data_count %u, free_count %u", __FUNCTION__,
              audio_ringbuf_count(rb_in), audio_ringbuf_free_space(rb_in));
    AUD_LOG_V("%s(-), rb_out data_count %u, free_count %u", __FUNCTION__,
              audio_ringbuf_count(rb_out), audio_ringbuf_free_space(rb_out));
    if (rb_aec != NULL) {
        AUD_LOG_V("%s(-), rb_aec data_count %u, free_count %u", __FUNCTION__,
                  audio_ringbuf_count(rb_aec), audio_ringbuf_free_space(rb_aec));
    }
    UNLOCK_ALOCK(lib_handler->lock);
    return process_count;
}


int aurisys_arsi_process_dl_only(aurisys_lib_handler_t *lib_handler) {
    int process_count = 0;

    AurisysLibInterface *arsi_api = NULL;

    audio_ringbuf_t *rb_in = NULL;
    audio_ringbuf_t *rb_out = NULL;

    audio_buf_t *p_dl_buf_in = NULL;
    audio_buf_t *p_dl_buf_out = NULL;

    data_buf_t *lib_dump_buf = NULL;

    uint32_t frame_buf_size_dl_in = 0;
    uint32_t frame_buf_size_dl_out = 0;

    audio_ringbuf_t *rb_linear_buf = NULL;


    uint32_t pool_in_data_count = 0;
    uint32_t retrieve_data_count = 0;

    uint32_t data_count = 0;
    uint32_t free_count = 0;

    status_t retval = NO_ERROR;


    LOCK_ALOCK_MS(lib_handler->lock, 500);

    arsi_api = lib_handler->api;

    rb_in = &lib_handler->dl_pool_in.ringbuf;
    rb_out = &lib_handler->dl_pool_out.ringbuf;
    AUD_LOG_V("%s(+), rb_in  data_count %u, free_count %u", __FUNCTION__,
              audio_ringbuf_count(rb_in), audio_ringbuf_free_space(rb_in));
    AUD_LOG_V("%s(+), rb_out data_count %u, free_count %u", __FUNCTION__,
              audio_ringbuf_count(rb_out), audio_ringbuf_free_space(rb_out));


    AUD_ASSERT(lib_handler->lib_config.p_dl_buf_in != NULL);
    AUD_ASSERT(lib_handler->lib_config.p_dl_buf_out != NULL);

    p_dl_buf_in  = lib_handler->lib_config.p_dl_buf_in;
    p_dl_buf_out = lib_handler->lib_config.p_dl_buf_out;

    AUD_ASSERT(p_dl_buf_in->data_buf.p_buffer != NULL);
    AUD_ASSERT(p_dl_buf_out->data_buf.p_buffer != NULL);

    rb_linear_buf = &lib_handler->rb_linear_buf;
    AUD_ASSERT(rb_linear_buf->base != NULL);

    if (lib_handler->lib_dump_enabled && lib_handler->lib_dump_buf.p_buffer != NULL) {
        lib_dump_buf = &lib_handler->lib_dump_buf;
    }


    /* TODO: abstract xlink */
    if (lib_handler->lib_config.b_interleave == 0) { /* frame base */
        frame_buf_size_dl_in = get_frame_buf_size(p_dl_buf_in);
        frame_buf_size_dl_out = get_frame_buf_size(p_dl_buf_out);

        AUD_ASSERT(p_dl_buf_in->data_buf.memory_size == frame_buf_size_dl_in);
        AUD_ASSERT(p_dl_buf_out->data_buf.memory_size == frame_buf_size_dl_out);

        pool_in_data_count = audio_ringbuf_count(rb_in);
        while (pool_in_data_count >= frame_buf_size_dl_in) { /* once per frame */
            pool_in_data_count -= frame_buf_size_dl_in;

            /* get data from pool */
            audio_ringbuf_copy_to_linear(lib_handler->linear_buf, rb_in, frame_buf_size_dl_in);
            if (lib_handler->raw_dump_enabled == 1 &&
                lib_handler->pcm_dump_dl_in != NULL &&
                lib_handler->pcm_dump_dl_in->mFilep != NULL) {
                lib_handler->pcm_dump_dl_in->AudioDumpPCMData(
                    lib_handler->pcm_dump_dl_in,
                    lib_handler->linear_buf,
                    frame_buf_size_dl_in);
            }


            /* frame base transfer */
            linear_to_frame_base(
                lib_handler->linear_buf,
                p_dl_buf_in->data_buf.p_buffer,
                frame_buf_size_dl_in,
                p_dl_buf_in->audio_format,
                p_dl_buf_in->num_channels);
            p_dl_buf_in->data_buf.data_size = frame_buf_size_dl_in;


            /* process */
            AUD_ASSERT(p_dl_buf_out->data_buf.data_size == 0);
#if 0
            memcpy(p_dl_buf_out->data_buf.p_buffer, p_dl_buf_in->data_buf.p_buffer, frame_buf_size_dl_out);
            p_dl_buf_out->data_buf.data_size = frame_buf_size_dl_out;
            p_dl_buf_in->data_buf.data_size = 0;
#else
            retval = arsi_api->arsi_process_dl_buf(
                         p_dl_buf_in,
                         p_dl_buf_out,
                         NULL,
                         lib_dump_buf,
                         lib_handler->arsi_handler);
            if (retval != NO_ERROR) {
                AUD_LOG_W("lib_handler %p, arsi_handler %p, arsi_process_dl_buf retval = %d",
                          lib_handler, lib_handler->arsi_handler, retval);
            }
#endif
            AUD_ASSERT(p_dl_buf_in->data_buf.data_size == 0);
            AUD_ASSERT(p_dl_buf_out->data_buf.data_size != 0);


            if (p_dl_buf_in->data_buf.data_size != 0) {
                AUD_LOG_W("%s(), p_dl_buf_in->data_buf.data_size %u != 0",
                          __FUNCTION__, p_dl_buf_in->data_buf.data_size); /* TODO: drop !? */
            }
            if (p_dl_buf_out->data_buf.data_size != frame_buf_size_dl_out) {
                AUD_LOG_E("%s(), p_dl_buf_out->data_buf.data_size %u != %u",
                          __FUNCTION__, p_dl_buf_out->data_buf.data_size, frame_buf_size_dl_out);
                if (p_dl_buf_out->data_buf.data_size == 0) {
                    p_dl_buf_out->data_buf.data_size = frame_buf_size_dl_out;
                } else {
                    AUD_ASSERT(0);
                }
            }

            /* linear base transfer */
            frame_base_to_linear(
                p_dl_buf_out->data_buf.p_buffer,
                lib_handler->linear_buf,
                p_dl_buf_out->data_buf.data_size,
                p_dl_buf_out->audio_format,
                p_dl_buf_out->num_channels);
            p_dl_buf_out->data_buf.data_size = 0;


            if (lib_handler->raw_dump_enabled == 1 &&
                lib_handler->pcm_dump_dl_out != NULL &&
                lib_handler->pcm_dump_dl_out->mFilep != NULL) {
                lib_handler->pcm_dump_dl_out->AudioDumpPCMData(
                    lib_handler->pcm_dump_dl_out,
                    lib_handler->linear_buf,
                    frame_buf_size_dl_out);
            }

            if (lib_handler->lib_dump_enabled == 1 &&
                lib_handler->lib_dump != NULL &&
                lib_handler->lib_dump->mFilep != NULL &&
                lib_dump_buf != NULL) {
                AUD_ASSERT(lib_dump_buf->data_size != 0);
                lib_handler->lib_dump->AudioDumpPCMData(
                    lib_handler->lib_dump,
                    lib_dump_buf->p_buffer,
                    lib_dump_buf->data_size);
                lib_dump_buf->data_size = 0;
            }


            free_count = audio_ringbuf_free_space(rb_out);
            if (frame_buf_size_dl_out > free_count) {
                AUD_LOG_W("%s(), frame_buf_size_dl_out %u > free_count %u",
                          __FUNCTION__, frame_buf_size_dl_out, free_count);

                audio_ringbuf_copy_from_linear(rb_out, lib_handler->linear_buf, free_count); /* drop some !! */
                process_count += free_count;
            } else {
                audio_ringbuf_copy_from_linear(rb_out, lib_handler->linear_buf, frame_buf_size_dl_out);
                process_count += frame_buf_size_dl_out;
            }
        }
    } else {
        /* get data from rb_in to p_dl_buf_in->data_buf.p_buffer */
        pool_in_data_count = audio_ringbuf_count(rb_in);
        AUD_ASSERT(pool_in_data_count <= MAX_LIB_BUF_SIZE);
        audio_ringbuf_copy_to_linear(p_dl_buf_in->data_buf.p_buffer, rb_in, pool_in_data_count);
        p_dl_buf_in->data_buf.data_size = pool_in_data_count;


        /* process */
        AUD_ASSERT(p_dl_buf_out->data_buf.data_size == 0);
#if 0
        memcpy(p_dl_buf_out->data_buf.p_buffer, p_dl_buf_in->data_buf.p_buffer, p_dl_buf_in->data_buf.data_size);
        p_dl_buf_out->data_buf.data_size = p_dl_buf_in->data_buf.data_size;
        p_dl_buf_in->data_buf.data_size = 0;
#else
        retval = arsi_api->arsi_process_dl_buf(
                     p_dl_buf_in,
                     p_dl_buf_out,
                     NULL,
                     lib_dump_buf,
                     lib_handler->arsi_handler);
#endif
        if (retval != NO_ERROR) {
            AUD_LOG_W("lib_handler %p, arsi_handler %p, arsi_process_dl_buf retval = %d",
                      lib_handler, lib_handler->arsi_handler, retval);
        }
        //AUD_ASSERT(p_dl_buf_in->data_buf.data_size == 0);
        AUD_ASSERT(p_dl_buf_out->data_buf.data_size != 0);

        if (lib_handler->raw_dump_enabled == 1) {
            if (lib_handler->pcm_dump_dl_in != NULL &&
                lib_handler->pcm_dump_dl_in->mFilep != NULL) {
                lib_handler->pcm_dump_dl_in->AudioDumpPCMData(
                    lib_handler->pcm_dump_dl_in,
                    p_dl_buf_in->data_buf.p_buffer,
                    pool_in_data_count - p_dl_buf_in->data_buf.data_size);
            }
            if (lib_handler->pcm_dump_dl_out != NULL &&
                lib_handler->pcm_dump_dl_out->mFilep != NULL) {
                lib_handler->pcm_dump_dl_out->AudioDumpPCMData(
                    lib_handler->pcm_dump_dl_out,
                    p_dl_buf_out->data_buf.p_buffer,
                    p_dl_buf_out->data_buf.data_size);
            }
        }

        if (lib_handler->lib_dump_enabled == 1 &&
            lib_handler->lib_dump != NULL &&
            lib_handler->lib_dump->mFilep != NULL &&
            lib_dump_buf != NULL) {
            AUD_ASSERT(lib_dump_buf->data_size != 0);
            lib_handler->lib_dump->AudioDumpPCMData(
                lib_handler->lib_dump,
                lib_dump_buf->p_buffer,
                lib_dump_buf->data_size);
            lib_dump_buf->data_size = 0;
        }

        /* concatenate remain p_dl_buf_in data */
        audio_ringbuf_rollback(rb_in, p_dl_buf_in->data_buf.data_size);
        p_dl_buf_in->data_buf.data_size = 0;


        free_count = audio_ringbuf_free_space(rb_out);
        if (p_dl_buf_out->data_buf.data_size > free_count) {
            AUD_LOG_W("%s(), p_dl_buf_out->data_buf.data_size %u > free_count %u",
                      __FUNCTION__, p_dl_buf_out->data_buf.data_size, free_count);
            AUD_WARNING("overflow!!");
            audio_ringbuf_copy_from_linear(rb_out, p_dl_buf_out->data_buf.p_buffer, free_count); /* drop some !! */
            process_count += free_count;
        } else {
            audio_ringbuf_copy_from_linear(rb_out, p_dl_buf_out->data_buf.p_buffer, p_dl_buf_out->data_buf.data_size);
            process_count += p_dl_buf_out->data_buf.data_size;
        }
        p_dl_buf_out->data_buf.data_size = 0;
    }

    AUD_LOG_V("%s(-), rb_in  data_count %u, free_count %u", __FUNCTION__,
              audio_ringbuf_count(rb_in), audio_ringbuf_free_space(rb_in));
    AUD_LOG_V("%s(-), rb_out data_count %u, free_count %u", __FUNCTION__,
              audio_ringbuf_count(rb_out), audio_ringbuf_free_space(rb_out));
    UNLOCK_ALOCK(lib_handler->lock);
    return process_count;
}


int aurisys_arsi_process_ul_and_dl(aurisys_lib_handler_t *lib_handler) {
    if (!lib_handler) {
        AUD_LOG_E("%s(), NULL! return", __FUNCTION__);
        return -1;
    }

    return 0;
}


int aurisys_arsi_set_ul_digital_gain(
    aurisys_lib_handler_t *lib_handler,
    const int16_t ul_analog_gain_ref_only,
    const int16_t ul_digital_gain) {
    status_t retval = NO_ERROR;

    LOCK_ALOCK_MS(lib_handler->lock, 500);

    retval = lib_handler->api->arsi_set_ul_digital_gain(
                 ul_analog_gain_ref_only,
                 ul_digital_gain,
                 lib_handler->arsi_handler);

    AUD_LOG_D("lib_name %s, %p, set ul_analog_gain_ref_only %d, ul_digital_gain %d, retval %d",
              lib_handler->lib_name, lib_handler, ul_analog_gain_ref_only, ul_digital_gain, retval);

    UNLOCK_ALOCK(lib_handler->lock);
    return (retval == NO_ERROR) ? 0 : -1;
}


int aurisys_arsi_set_dl_digital_gain(
    aurisys_lib_handler_t *lib_handler,
    const int16_t dl_analog_gain_ref_only,
    const int16_t dl_digital_gain) {
    status_t retval = NO_ERROR;

    LOCK_ALOCK_MS(lib_handler->lock, 500);

    retval = lib_handler->api->arsi_set_dl_digital_gain(
                 dl_analog_gain_ref_only,
                 dl_digital_gain,
                 lib_handler->arsi_handler);

    AUD_LOG_D("lib_name %s, %p, set dl_analog_gain_ref_only %d, dl_digital_gain %d, retval %d",
              lib_handler->lib_name, lib_handler, dl_analog_gain_ref_only, dl_digital_gain, retval);

    UNLOCK_ALOCK(lib_handler->lock);
    return (retval == NO_ERROR) ? 0 : -1;
}


status_t aurisys_arsi_apply_param(aurisys_lib_handler_t *lib_handler) {
    status_t retval = NO_ERROR;

    /* TODO: add try lock here */

    AUD_ASSERT(lib_handler->arsi_handler != NULL);
    retval = aurisys_parsing_param_file(
                 lib_handler->api,
                 lib_handler->task_config,
                 &lib_handler->lib_config,
                 gPhoneProductName,
                 lib_handler->param_path,
                 *lib_handler->enhancement_mode,
                 &lib_handler->param_buf,
                 lib_handler->debug_log_fp);
    if (retval != NO_ERROR) {
        AUD_LOG_E("%s(-) %p, aurisys_parsing_param_file fail", __FUNCTION__, lib_handler);
        return retval;
    }

    retval = lib_handler->api->arsi_update_param(
                 lib_handler->task_config,
                 &lib_handler->lib_config,
                 &lib_handler->param_buf,
                 lib_handler->arsi_handler);
    if (retval != NO_ERROR) {
        AUD_LOG_E("%s(-) %p, arsi_update_param fail", __FUNCTION__, lib_handler);
        return retval;
    }

    return retval;
}


int aurisys_arsi_run_adb_cmd(aurisys_lib_handler_t *lib_handler, aurisys_adb_command_t *adb_cmd) {
    status_t retval = NO_ERROR;

    AurisysLibInterface *arsi_api = NULL;
    void *arsi_handler = NULL;


    LOCK_ALOCK_MS(lib_handler->lock, 500);


    arsi_api = lib_handler->api;
    arsi_handler = lib_handler->arsi_handler;


    switch (adb_cmd->adb_cmd_type) {
    case ADB_CMD_PARAM_FILE:
        if (adb_cmd->direction == AURISYS_SET_PARAM) {
            AUD_LOG_D("lib_name %s, %p, ADB_CMD_PARAM_FILE: %s",
                      lib_handler->lib_name, lib_handler, lib_handler->param_path);
            AUD_ASSERT(!strncmp(lib_handler->param_path,
                                adb_cmd->param_path,
                                strlen(lib_handler->param_path)));
        } else if (adb_cmd->direction == AURISYS_GET_PARAM) {
            AUD_LOG_V("lib_name %s, %p, ADB_CMD_PARAM_FILE: %s",
                      lib_handler->lib_name, lib_handler, lib_handler->param_path);
        }
        break;
    case ADB_CMD_LIB_DUMP_FILE:
        if (adb_cmd->direction == AURISYS_SET_PARAM) {
            AUD_LOG_D("lib_name %s, %p, ADB_CMD_LIB_DUMP_FILE: %s",
                      lib_handler->lib_name, lib_handler, lib_handler->lib_dump_path);
            AUD_ASSERT(!strncmp(lib_handler->lib_dump_path,
                                adb_cmd->lib_dump_path,
                                strlen(lib_handler->lib_dump_path)));
        } else if (adb_cmd->direction == AURISYS_GET_PARAM) {
            AUD_LOG_V("lib_name %s, %p, ADB_CMD_PARAM_FILE: %s",
                      lib_handler->lib_name, lib_handler, lib_handler->lib_dump_path);
        }
        break;
    case ADB_CMD_ENABLE_LOG:
        if (adb_cmd->direction == AURISYS_SET_PARAM) {
            AUD_LOG_D("lib_name %s, %p, ADB_CMD_ENABLE_LOG: %d",
                      lib_handler->lib_name, lib_handler, *lib_handler->enable_log);
            AUD_ASSERT(adb_cmd->enable_log == *lib_handler->enable_log);
            lib_handler->debug_log_fp = (adb_cmd->enable_log)
                                        ? arsi_lib_printf
                                        : arsi_lib_printf_dummy;
            retval = arsi_api->arsi_set_debug_log_fp(
                         lib_handler->debug_log_fp,
                         arsi_handler);
        } else if (adb_cmd->direction == AURISYS_GET_PARAM) {
            AUD_LOG_V("lib_name %s, %p, ADB_CMD_ENABLE_LOG: %d",
                      lib_handler->lib_name, lib_handler, *lib_handler->enable_log);
        }
        break;
    case ADB_CMD_ENABLE_RAW_DUMP:
        if (adb_cmd->direction == AURISYS_SET_PARAM) {
            AUD_LOG_D("lib_name %s, %p, ADB_CMD_ENABLE_RAW_DUMP: %d",
                      lib_handler->lib_name, lib_handler, *lib_handler->enable_raw_dump);
            AUD_ASSERT(adb_cmd->enable_raw_dump == *lib_handler->enable_raw_dump);
            /* TODO: dynamically open/close pcm dump */
        } else if (adb_cmd->direction == AURISYS_GET_PARAM) {
            AUD_LOG_V("lib_name %s, %p, ADB_CMD_ENABLE_RAW_DUMP: %d",
                      lib_handler->lib_name, lib_handler, *lib_handler->enable_raw_dump);
        }
        break;
    case ADB_CMD_ENABLE_LIB_DUMP:
        if (adb_cmd->direction == AURISYS_SET_PARAM) {
            AUD_LOG_D("lib_name %s, %p, ADB_CMD_ENABLE_LIB_DUMP: %d",
                      lib_handler->lib_name, lib_handler, *lib_handler->enable_lib_dump);
            AUD_ASSERT(adb_cmd->enable_lib_dump == *lib_handler->enable_lib_dump);
            /* TODO: dynamically open/close pcm dump */
        } else if (adb_cmd->direction == AURISYS_GET_PARAM) {
            AUD_LOG_V("lib_name %s, %p, ADB_CMD_ENABLE_LIB_DUMP: %d",
                      lib_handler->lib_name, lib_handler, *lib_handler->enable_lib_dump);
        }
        break;
    case ADB_CMD_APPLY_PARAM:
        if (adb_cmd->direction == AURISYS_SET_PARAM) {
            AUD_LOG_D("lib_name %s, %p, ADB_CMD_APPLY_PARAM: %u",
                      lib_handler->lib_name, lib_handler, *lib_handler->enhancement_mode);
            AUD_ASSERT(adb_cmd->enhancement_mode == *lib_handler->enhancement_mode);
            retval = aurisys_arsi_apply_param(lib_handler);
        } else if (adb_cmd->direction == AURISYS_GET_PARAM) {
            AUD_LOG_V("lib_name %s, %p, ADB_CMD_APPLY_PARAM: %u",
                      lib_handler->lib_name, lib_handler, *lib_handler->enhancement_mode);
        }
        break;
    case ADB_CMD_ADDR_VALUE:
        if (adb_cmd->direction == AURISYS_SET_PARAM) {
            retval = arsi_api->arsi_set_addr_value(
                         adb_cmd->addr_value_pair.addr,
                         adb_cmd->addr_value_pair.value,
                         arsi_handler);
            AUD_LOG_D("lib_name %s, %p, ADB_CMD_ADDR_VALUE: *0x%x = 0x%x, retval %d",
                      lib_handler->lib_name, lib_handler,
                      adb_cmd->addr_value_pair.addr,
                      adb_cmd->addr_value_pair.value,
                      retval);
        } else if (adb_cmd->direction == AURISYS_GET_PARAM) {
            retval = arsi_api->arsi_get_addr_value(
                         adb_cmd->addr_value_pair.addr,
                         &adb_cmd->addr_value_pair.value,
                         arsi_handler);
            AUD_LOG_D("lib_name %s, %p, ADB_CMD_ADDR_VALUE: *0x%x is 0x%x, retval %d",
                      lib_handler->lib_name, lib_handler,
                      adb_cmd->addr_value_pair.addr,
                      adb_cmd->addr_value_pair.value,
                      retval);
        }
        break;
    case ADB_CMD_KEY_VALUE:
        if (adb_cmd->direction == AURISYS_SET_PARAM) {
            retval = arsi_api->arsi_set_key_value_pair(
                         &adb_cmd->key_value_pair,
                         arsi_handler);
            AUD_LOG_D("lib_name %s, %p, ADB_CMD_KEY_VALUE: %u, %u, %s, retval %d",
                      lib_handler->lib_name, lib_handler,
                      adb_cmd->key_value_pair.memory_size,
                      adb_cmd->key_value_pair.string_size,
                      adb_cmd->key_value_pair.p_string,
                      retval);
        } else if (adb_cmd->direction == AURISYS_GET_PARAM) {
            retval = arsi_api->arsi_get_key_value_pair(
                         &adb_cmd->key_value_pair,
                         arsi_handler);
            AUD_LOG_D("lib_name %s, %p, ADB_CMD_KEY_VALUE: %u, %u, %s, retval %d",
                      lib_handler->lib_name, lib_handler,
                      adb_cmd->key_value_pair.memory_size,
                      adb_cmd->key_value_pair.string_size,
                      adb_cmd->key_value_pair.p_string,
                      retval);
        }
        break;
    default:
        AUD_LOG_W("lib_name %s, %p, not support %d adb_cmd_type!!",
                  lib_handler->lib_name, lib_handler, adb_cmd->adb_cmd_type);
        retval = BAD_VALUE;
        break;
    }

    UNLOCK_ALOCK(lib_handler->lock);

    return (retval == NO_ERROR) ? 0 : -1;
}




/*
 * =============================================================================
 *                     private function implementation
 * =============================================================================
 */

static void arsi_lib_printf(const char *message, ...) {
    static char printf_msg[256];

    va_list args;
    va_start(args, message);

    vsnprintf(printf_msg, sizeof(printf_msg), message, args);
    AUD_LOG_D("[LIB] %s", printf_msg);

    va_end(args);
}


static void arsi_lib_printf_dummy(const char *message, ...) {
    if (message == NULL) {
        AUD_LOG_E("%s(), NULL!! return", __FUNCTION__);
        return;
    }

    return;
}


static void allocate_data_buf(audio_buf_t *audio_buf) {
    data_buf_t *data_buf = &audio_buf->data_buf;

    if (audio_buf->frame_size_ms != 0) {
        data_buf->memory_size = get_frame_buf_size(audio_buf);
    } else {
        data_buf->memory_size = MAX_LIB_BUF_SIZE;
    }

    AUD_ASSERT(data_buf->memory_size > 0);

    AUDIO_ALLOC_BUFFER(data_buf->p_buffer, data_buf->memory_size);
    data_buf->data_size = 0;
}


static void init_audio_buf_by_lib_config(
    audio_buf_t *audio_buf,
    arsi_lib_config_t *lib_config) {
    if (audio_buf == NULL || lib_config == NULL) {
        AUD_LOG_E("%s(), NULL!! return", __FUNCTION__);
        return;
    }

    /* same as lib_config */
    audio_buf->b_interleave = lib_config->b_interleave;
    audio_buf->frame_size_ms = lib_config->frame_size_ms;
    audio_buf->sample_rate_buffer = lib_config->sample_rate;
    audio_buf->sample_rate_content = lib_config->sample_rate; /* TODO */
    audio_buf->audio_format = lib_config->audio_format;

    /* alloc data_buf */
    allocate_data_buf(audio_buf);
    AUD_ASSERT(audio_buf->data_buf.memory_size != 0);
    AUD_ASSERT(audio_buf->data_buf.p_buffer != NULL);

    AUD_LOG_V("audio_buf data_buf_type %d", audio_buf->data_buf_type);
    AUD_LOG_V("audio_buf b_interleave %d", audio_buf->b_interleave);
    AUD_LOG_V("audio_buf frame_size_ms %d", audio_buf->frame_size_ms);
    AUD_LOG_V("audio_buf num_channels %d", audio_buf->num_channels);
    AUD_LOG_V("audio_buf channel_mask 0x%x", audio_buf->channel_mask); /* TODO */
    AUD_LOG_V("audio_buf sample_rate_buffer %u", audio_buf->sample_rate_buffer);
    AUD_LOG_V("audio_buf sample_rate_content %u", audio_buf->sample_rate_content);
    AUD_LOG_V("audio_buf audio_format 0x%x", audio_buf->audio_format);
    AUD_LOG_V("audio_buf memory_size %u", audio_buf->data_buf.memory_size);
    AUD_LOG_V("audio_buf p_buffer %p", audio_buf->data_buf.p_buffer);
}


static void clone_lib_config(
    arsi_lib_config_t *des,
    arsi_lib_config_t *src,
    const aurisys_component_t *the_component,
    const struct aurisys_user_prefer_configs_t *p_prefer_configs) {
    int i = 0;

    des->sample_rate = audio_sample_rate_get_match_rate(
                           the_component->sample_rate_mask,
                           p_prefer_configs->sample_rate);
    des->audio_format = get_dedicated_format_from_mask(
                            the_component->support_format_mask,
                            p_prefer_configs->audio_format);
    des->frame_size_ms = get_dedicated_frame_ms_from_mask(
                             the_component->support_frame_ms_mask,
                             p_prefer_configs->frame_size_ms);

    des->b_interleave = src->b_interleave;
    des->num_ul_ref_buf_array = src->num_ul_ref_buf_array;
    des->num_dl_ref_buf_array = src->num_dl_ref_buf_array;


    /* ul in */
    if (src->p_ul_buf_in != NULL) {
        AUDIO_ALLOC_STRUCT(audio_buf_t, des->p_ul_buf_in);

        /* data_buf_type & num_channels */
        memcpy(des->p_ul_buf_in, src->p_ul_buf_in, sizeof(audio_buf_t));
        des->p_ul_buf_in->num_channels =
            get_dedicated_channel_number_from_mask(
                the_component->support_channel_number_mask[des->p_ul_buf_in->data_buf_type],
                p_prefer_configs->num_channels_ul);

        init_audio_buf_by_lib_config(des->p_ul_buf_in, des);
    }

    /* ul out */
    if (src->p_ul_buf_out != NULL) {
        AUDIO_ALLOC_STRUCT(audio_buf_t, des->p_ul_buf_out);

        /* data_buf_type & num_channels */
        memcpy(des->p_ul_buf_out, src->p_ul_buf_out, sizeof(audio_buf_t));
        des->p_ul_buf_out->num_channels =
            get_dedicated_channel_number_from_mask(
                the_component->support_channel_number_mask[des->p_ul_buf_out->data_buf_type],
                p_prefer_configs->num_channels_ul);

        init_audio_buf_by_lib_config(des->p_ul_buf_out, des);
    }

    /* ul refs */
    if (src->num_ul_ref_buf_array != 0) {
        AUDIO_ALLOC_STRUCT_ARRAY(audio_buf_t, src->num_ul_ref_buf_array, des->p_ul_ref_bufs);
        for (i = 0; i < src->num_ul_ref_buf_array; i++) {
            /* data_buf_type & num_channels */
            memcpy(&des->p_ul_ref_bufs[i], &src->p_ul_ref_bufs[i], sizeof(audio_buf_t));

            init_audio_buf_by_lib_config(&des->p_ul_ref_bufs[i], des);
        }
    }

    /* dl in */
    if (src->p_dl_buf_in != NULL) {
        AUDIO_ALLOC_STRUCT(audio_buf_t, des->p_dl_buf_in);

        /* data_buf_type & num_channels */
        memcpy(des->p_dl_buf_in, src->p_dl_buf_in, sizeof(audio_buf_t));
        des->p_dl_buf_in->num_channels =
            get_dedicated_channel_number_from_mask(
                the_component->support_channel_number_mask[des->p_dl_buf_in->data_buf_type],
                p_prefer_configs->num_channels_dl);

        init_audio_buf_by_lib_config(des->p_dl_buf_in, des);
    }

    /* dl out */
    if (src->p_dl_buf_out != NULL) {
        AUDIO_ALLOC_STRUCT(audio_buf_t, des->p_dl_buf_out);

        /* data_buf_type & num_channels */
        memcpy(des->p_dl_buf_out, src->p_dl_buf_out, sizeof(audio_buf_t));
        des->p_dl_buf_out->num_channels =
            get_dedicated_channel_number_from_mask(
                the_component->support_channel_number_mask[des->p_dl_buf_out->data_buf_type],
                p_prefer_configs->num_channels_dl);

        init_audio_buf_by_lib_config(des->p_dl_buf_out, des);
    }

    /* dl refs */
    if (src->num_dl_ref_buf_array != 0) {
        AUDIO_ALLOC_STRUCT_ARRAY(audio_buf_t, src->num_dl_ref_buf_array, des->p_dl_ref_bufs);
        for (i = 0; i < src->num_dl_ref_buf_array; i++) {
            /* data_buf_type & num_channels */
            memcpy(&des->p_dl_ref_bufs[i], &src->p_dl_ref_bufs[i], sizeof(audio_buf_t));

            init_audio_buf_by_lib_config(&des->p_dl_ref_bufs[i], des);
        }
    }

    dump_lib_config(des);
}


static void release_lib_config(arsi_lib_config_t *lib_config) {
    int i = 0;

    AUD_LOG_VV("sample_rate %d", lib_config->sample_rate);
    AUD_LOG_VV("audio_format %d", lib_config->audio_format);
    AUD_LOG_VV("frame_size_ms %d", lib_config->frame_size_ms);
    AUD_LOG_VV("b_interleave %d", lib_config->b_interleave);
    AUD_LOG_VV("num_ul_ref_buf_array %d", lib_config->num_ul_ref_buf_array);
    AUD_LOG_VV("num_dl_ref_buf_array %d", lib_config->num_dl_ref_buf_array);


    /* TODO: move there config to a function */

    /* ul in */
    if (lib_config->p_ul_buf_in != NULL) {
        AUD_LOG_V("UL buf_in %p", lib_config->p_ul_buf_in->data_buf.p_buffer);
        AUDIO_FREE_POINTER(lib_config->p_ul_buf_in->data_buf.p_buffer);
        AUDIO_FREE_POINTER(lib_config->p_ul_buf_in);
    }

    /* ul out */
    if (lib_config->p_ul_buf_out != NULL) {
        AUD_LOG_V("UL buf_out %p", lib_config->p_ul_buf_out->data_buf.p_buffer);
        AUDIO_FREE_POINTER(lib_config->p_ul_buf_out->data_buf.p_buffer);
        AUDIO_FREE_POINTER(lib_config->p_ul_buf_out);
    }

    /* ul refs */
    if (lib_config->num_ul_ref_buf_array != 0) {
        for (i = 0; i < lib_config->num_ul_ref_buf_array; i++) {
            AUD_LOG_V("UL buf_ref[%d] %p", i, lib_config->p_ul_ref_bufs[i].data_buf.p_buffer);
            AUDIO_FREE_POINTER(lib_config->p_ul_ref_bufs[i].data_buf.p_buffer);
        }
        AUDIO_FREE_POINTER(lib_config->p_ul_ref_bufs);
    }

    /* dl in */
    if (lib_config->p_dl_buf_in != NULL) {
        AUD_LOG_V("DL buf_in %p", lib_config->p_dl_buf_in->data_buf.p_buffer);
        AUDIO_FREE_POINTER(lib_config->p_dl_buf_in->data_buf.p_buffer);
        AUDIO_FREE_POINTER(lib_config->p_dl_buf_in);
    }

    /* dl out */
    if (lib_config->p_dl_buf_out != NULL) {
        AUD_LOG_V("DL buf_out %p", lib_config->p_dl_buf_out->data_buf.p_buffer);
        AUDIO_FREE_POINTER(lib_config->p_dl_buf_out->data_buf.p_buffer);
        AUDIO_FREE_POINTER(lib_config->p_dl_buf_out);
    }

    /* dl refs */
    if (lib_config->num_dl_ref_buf_array != 0) {
        for (i = 0; i < lib_config->num_dl_ref_buf_array; i++) {
            AUD_LOG_V("DL buf_ref[%d] %p", i, lib_config->p_dl_ref_bufs[i].data_buf.p_buffer);
            AUDIO_FREE_POINTER(lib_config->p_dl_ref_bufs[i].data_buf.p_buffer);
        }
        AUDIO_FREE_POINTER(lib_config->p_dl_ref_bufs);
    }
}


static void init_pool_buf(aurisys_lib_handler_t *lib_handler) {
    audio_buf_t *source = NULL;
    audio_buf_t *target = NULL;

    audio_buf_t *ref = NULL;
    int i = 0;

    /* ul pool */
    if (lib_handler->lib_config.p_ul_buf_in != NULL &&
        lib_handler->lib_config.p_ul_buf_out != NULL) {

        create_pool_buf(&lib_handler->ul_pool_in,
                        lib_handler->lib_config.p_ul_buf_in,
                        MAX_POOL_BUF_SIZE);
        create_pool_buf(&lib_handler->ul_pool_out,
                        lib_handler->lib_config.p_ul_buf_out,
                        MAX_POOL_BUF_SIZE);

        source = lib_handler->ul_pool_in.buf;
        target = lib_handler->ul_pool_out.buf;
        AUD_LOG_V("UL sample_rate: %d => %d, num_channels: %d => %d, audio_format: 0x%x => 0x%x",
                  source->sample_rate_buffer, target->sample_rate_buffer,
                  source->num_channels, target->num_channels,
                  source->audio_format, target->audio_format);

        /* aec */
        if (lib_handler->lib_config.p_ul_ref_bufs != NULL) {
            for (i = 0; i < lib_handler->lib_config.num_ul_ref_buf_array; i++) {
                ref = &lib_handler->lib_config.p_ul_ref_bufs[i];
                if (ref->data_buf_type == DATA_BUF_ECHO_REF) {
                    AUDIO_ALLOC_STRUCT(audio_pool_buf_t, lib_handler->aec_pool_in);
                    create_pool_buf(lib_handler->aec_pool_in,
                                    ref,
                                    MAX_POOL_BUF_SIZE);
                    lib_handler->p_ul_ref_buf_aec = ref;
                    break;
                }
            }
        }
    }

    /* dl pool */
    if (lib_handler->lib_config.p_dl_buf_in != NULL &&
        lib_handler->lib_config.p_dl_buf_out != NULL) {
        create_pool_buf(&lib_handler->dl_pool_in,
                        lib_handler->lib_config.p_dl_buf_in,
                        MAX_POOL_BUF_SIZE);
        create_pool_buf(&lib_handler->dl_pool_out,
                        lib_handler->lib_config.p_dl_buf_out,
                        MAX_POOL_BUF_SIZE);

        source = lib_handler->dl_pool_in.buf;
        target = lib_handler->dl_pool_out.buf;
        AUD_LOG_V("DL sample_rate: %d => %d, num_channels: %d => %d, audio_format: 0x%x => 0x%x",
                  source->sample_rate_buffer, target->sample_rate_buffer,
                  source->num_channels, target->num_channels,
                  source->audio_format, target->audio_format);
    }
}


static void deinit_pool_buf(aurisys_lib_handler_t *lib_handler) {
    /* ul pool */
    if (lib_handler->lib_config.p_ul_buf_in != NULL &&
        lib_handler->lib_config.p_ul_buf_out != NULL) {
        destroy_pool_buf(&lib_handler->ul_pool_in);
        destroy_pool_buf(&lib_handler->ul_pool_out);
    }

    /* aec */
    if (lib_handler->aec_pool_in != NULL) {
        destroy_pool_buf(lib_handler->aec_pool_in);
        AUDIO_FREE_POINTER(lib_handler->aec_pool_in);
    }

    /* dl pool */
    if (lib_handler->lib_config.p_dl_buf_in != NULL &&
        lib_handler->lib_config.p_dl_buf_out != NULL) {
        destroy_pool_buf(&lib_handler->dl_pool_in);
        destroy_pool_buf(&lib_handler->dl_pool_out);
    }
}


static void aurisys_create_lib_handler_list_xlink(
    aurisys_scenario_t aurisys_scenario,
    arsi_process_type_t arsi_process_type,
    aurisys_library_name_t *name_list,
    aurisys_library_config_t *library_config_list,
    aurisys_lib_handler_t **lib_handler_list,
    uint32_t *num_library_hanlder,
    const struct aurisys_user_prefer_configs_t *p_prefer_configs) {
    aurisys_library_config_t *the_library_config = NULL;
    aurisys_component_t *the_component = NULL;
    aurisys_lib_handler_t *new_lib_handler = NULL;

    aurisys_library_name_t *itor_library_name = NULL;
    aurisys_library_name_t *tmp_library_name = NULL;

    char mDumpFileName[128];


    HASH_ITER(hh, name_list, itor_library_name, tmp_library_name) {
        HASH_FIND_STR(library_config_list,
                      itor_library_name->name,
                      the_library_config);
        if (the_library_config == NULL) {
            AUD_ASSERT(the_library_config != NULL);
            return;
        }

        HASH_FIND_INT(
            the_library_config->component_hh,
            &aurisys_scenario,
            the_component);
        if (the_component == NULL) {
            AUD_ASSERT(the_component != NULL);
            return;
        }

        AUDIO_ALLOC_STRUCT(aurisys_lib_handler_t, new_lib_handler);

        *num_library_hanlder = (*num_library_hanlder) + 1;

        new_lib_handler->self = new_lib_handler;
        new_lib_handler->lib_name = itor_library_name->name;

        NEW_ALOCK(new_lib_handler->lock);
        LOCK_ALOCK_MS(new_lib_handler->lock, 1000);

        clone_lib_config(
            &new_lib_handler->lib_config,
            &the_component->lib_config,
            the_component,
            p_prefer_configs);
        init_pool_buf(new_lib_handler);


        new_lib_handler->enable_log = &the_component->enable_log;
        new_lib_handler->enable_raw_dump = &the_component->enable_raw_dump;
        new_lib_handler->enable_lib_dump = &the_component->enable_lib_dump;
        new_lib_handler->enhancement_mode = &the_component->enhancement_mode;

        new_lib_handler->debug_log_fp = (the_component->enable_log)
                                        ? arsi_lib_printf
                                        : arsi_lib_printf_dummy;

        AUDIO_ALLOC_CHAR_BUFFER(new_lib_handler->linear_buf, MAX_POOL_BUF_SIZE);
        new_lib_handler->rb_linear_buf.base = new_lib_handler->linear_buf;
        new_lib_handler->rb_linear_buf.read = new_lib_handler->rb_linear_buf.base;
        new_lib_handler->rb_linear_buf.write = new_lib_handler->rb_linear_buf.base;
        new_lib_handler->rb_linear_buf.size = MAX_POOL_BUF_SIZE;

        new_lib_handler->api = the_library_config->api;
        new_lib_handler->param_path = the_library_config->param_path;
        new_lib_handler->lib_dump_path = the_library_config->lib_dump_path;

        new_lib_handler->param_buf.memory_size = MAX_LIB_PARAM_SIZE;
        new_lib_handler->param_buf.data_size = 0;
        AUDIO_ALLOC_BUFFER(
            new_lib_handler->param_buf.p_buffer,
            new_lib_handler->param_buf.memory_size);


        if (*new_lib_handler->enable_raw_dump) {
            new_lib_handler->raw_dump_enabled = true;
            if (arsi_process_type == ARSI_PROCESS_TYPE_UL_ONLY ||
                arsi_process_type == ARSI_PROCESS_TYPE_UL_AND_DL) {
                AUDIO_ALLOC_STRUCT(PcmDump_t, new_lib_handler->pcm_dump_ul_in);
                sprintf(mDumpFileName, "%s/%s.%d.%d.%d.ul_in.pcm", audio_dump, itor_library_name->name, mDumpFileNum, getpid(), gettid());
                InitPcmDump_t(new_lib_handler->pcm_dump_ul_in, 32768);
                new_lib_handler->pcm_dump_ul_in->AudioOpendumpPCMFile(
                    new_lib_handler->pcm_dump_ul_in, mDumpFileName);

                AUDIO_ALLOC_STRUCT(PcmDump_t, new_lib_handler->pcm_dump_ul_out);
                sprintf(mDumpFileName, "%s/%s.%d.%d.%d.ul_out.pcm", audio_dump, itor_library_name->name, mDumpFileNum, getpid(), gettid());
                InitPcmDump_t(new_lib_handler->pcm_dump_ul_out, 32768);
                new_lib_handler->pcm_dump_ul_out->AudioOpendumpPCMFile(
                    new_lib_handler->pcm_dump_ul_out, mDumpFileName);

                if (new_lib_handler->p_ul_ref_buf_aec != NULL) {
                    AUDIO_ALLOC_STRUCT(PcmDump_t, new_lib_handler->pcm_dump_aec);
                    sprintf(mDumpFileName, "%s/%s.%d.%d.%d.aec.pcm", audio_dump, itor_library_name->name, mDumpFileNum, getpid(), gettid());
                    InitPcmDump_t(new_lib_handler->pcm_dump_aec, 32768);
                    new_lib_handler->pcm_dump_aec->AudioOpendumpPCMFile(
                        new_lib_handler->pcm_dump_aec, mDumpFileName);
                }
            }

            if (arsi_process_type == ARSI_PROCESS_TYPE_DL_ONLY ||
                arsi_process_type == ARSI_PROCESS_TYPE_UL_AND_DL) {
                AUDIO_ALLOC_STRUCT(PcmDump_t, new_lib_handler->pcm_dump_dl_in);
                sprintf(mDumpFileName, "%s/%s.%d.%d.%d.dl_in.pcm", audio_dump, itor_library_name->name, mDumpFileNum, getpid(), gettid());
                InitPcmDump_t(new_lib_handler->pcm_dump_dl_in, 32768);
                new_lib_handler->pcm_dump_dl_in->AudioOpendumpPCMFile(
                    new_lib_handler->pcm_dump_dl_in, mDumpFileName);

                AUDIO_ALLOC_STRUCT(PcmDump_t, new_lib_handler->pcm_dump_dl_out);
                sprintf(mDumpFileName, "%s/%s.%d.%d.%d.dl_out.pcm", audio_dump, itor_library_name->name, mDumpFileNum, getpid(), gettid());
                InitPcmDump_t(new_lib_handler->pcm_dump_dl_out, 32768);
                new_lib_handler->pcm_dump_dl_out->AudioOpendumpPCMFile(
                    new_lib_handler->pcm_dump_dl_out, mDumpFileName);
            }
        }

        if (*new_lib_handler->enable_lib_dump) {
            new_lib_handler->lib_dump_enabled = true;
            AUDIO_ALLOC_STRUCT(PcmDump_t, new_lib_handler->lib_dump);
            if (!strcmp(new_lib_handler->lib_dump_path, "AUTO")) {
                sprintf(mDumpFileName, "%s/%s.%d.%d.%d.lib_dump.bin", audio_dump, itor_library_name->name, mDumpFileNum, getpid(), gettid());
            } else {
                sprintf(mDumpFileName, "%s", new_lib_handler->lib_dump_path);
            }
            InitPcmDump_t(new_lib_handler->lib_dump, 32768);
            new_lib_handler->lib_dump->AudioOpendumpPCMFile(
                new_lib_handler->lib_dump, mDumpFileName);
        }

        if (new_lib_handler->raw_dump_enabled || new_lib_handler->lib_dump_enabled) {
            mDumpFileNum++;
            mDumpFileNum %= MAX_DUMP_NUM;
        }



        /* hash for manager */
        HASH_ADD_KEYPTR(hh_manager, *lib_handler_list,
                        new_lib_handler->lib_name, strlen(new_lib_handler->lib_name),
                        new_lib_handler);

        /* hash for adb cmd */
        HASH_ADD_KEYPTR(hh_component, the_component->lib_handler_list_for_adb_cmd,
                        new_lib_handler->self, sizeof(new_lib_handler->self),
                        new_lib_handler);
        /*
         * keep the componet hh s.t. we can remove lib_handler in component hh
         * when we destroy lib_handler
         */
        new_lib_handler->head = &the_component->lib_handler_list_for_adb_cmd;


        UNLOCK_ALOCK(new_lib_handler->lock);
    }
}


void aurisys_destroy_lib_handler_list_xlink(aurisys_lib_handler_t **handler_list) {
    aurisys_lib_handler_t *itor_lib_hanlder = NULL;
    aurisys_lib_handler_t *tmp_lib_hanlder = NULL;


    if (handler_list == NULL || *handler_list == NULL) {
        return;
    }

    HASH_ITER(hh_manager, *handler_list, itor_lib_hanlder, tmp_lib_hanlder) {
        LOCK_ALOCK_MS(itor_lib_hanlder->lock, 1000);
        AUD_LOG_V("itor_lib_hanlder %p", itor_lib_hanlder);

        HASH_DELETE(hh_manager, *handler_list, itor_lib_hanlder);
        HASH_DELETE(hh_component, *itor_lib_hanlder->head, itor_lib_hanlder);


        if (itor_lib_hanlder->lib_dump_enabled) {
            if (itor_lib_hanlder->lib_dump != NULL &&
                itor_lib_hanlder->lib_dump->mFilep != NULL) {
                itor_lib_hanlder->lib_dump->AudioCloseDumpPCMFile(itor_lib_hanlder->lib_dump);
            }
            AUDIO_FREE_POINTER(itor_lib_hanlder->lib_dump);
            AUDIO_FREE_POINTER(itor_lib_hanlder->lib_dump_buf.p_buffer);
        }

        if (itor_lib_hanlder->raw_dump_enabled) {
            /* UL in */
            if (itor_lib_hanlder->pcm_dump_ul_in != NULL &&
                itor_lib_hanlder->pcm_dump_ul_in->mFilep != NULL) {
                itor_lib_hanlder->pcm_dump_ul_in->AudioCloseDumpPCMFile(itor_lib_hanlder->pcm_dump_ul_in);
            }
            AUDIO_FREE_POINTER(itor_lib_hanlder->pcm_dump_ul_in);
            /* UL out */
            if (itor_lib_hanlder->pcm_dump_ul_out != NULL &&
                itor_lib_hanlder->pcm_dump_ul_out->mFilep != NULL) {
                itor_lib_hanlder->pcm_dump_ul_out->AudioCloseDumpPCMFile(itor_lib_hanlder->pcm_dump_ul_out);
            }
            AUDIO_FREE_POINTER(itor_lib_hanlder->pcm_dump_ul_out);
            /* AEC */
            if (itor_lib_hanlder->pcm_dump_aec != NULL &&
                itor_lib_hanlder->pcm_dump_aec->mFilep != NULL) {
                itor_lib_hanlder->pcm_dump_aec->AudioCloseDumpPCMFile(itor_lib_hanlder->pcm_dump_aec);
            }
            AUDIO_FREE_POINTER(itor_lib_hanlder->pcm_dump_aec);
            /* DL in */
            if (itor_lib_hanlder->pcm_dump_dl_in != NULL &&
                itor_lib_hanlder->pcm_dump_dl_in->mFilep != NULL) {
                itor_lib_hanlder->pcm_dump_dl_in->AudioCloseDumpPCMFile(itor_lib_hanlder->pcm_dump_dl_in);
            }
            AUDIO_FREE_POINTER(itor_lib_hanlder->pcm_dump_dl_in);
            /* DL out */
            if (itor_lib_hanlder->pcm_dump_dl_out != NULL &&
                itor_lib_hanlder->pcm_dump_dl_out->mFilep != NULL) {
                itor_lib_hanlder->pcm_dump_dl_out->AudioCloseDumpPCMFile(itor_lib_hanlder->pcm_dump_dl_out);
            }
            AUDIO_FREE_POINTER(itor_lib_hanlder->pcm_dump_dl_out);
        }


        AUDIO_FREE_POINTER(itor_lib_hanlder->linear_buf);

        deinit_pool_buf(itor_lib_hanlder);

        release_lib_config(&itor_lib_hanlder->lib_config);

        AUDIO_FREE_POINTER(itor_lib_hanlder->param_buf.p_buffer);

        UNLOCK_ALOCK(itor_lib_hanlder->lock);
        FREE_ALOCK(itor_lib_hanlder->lock);

        AUDIO_FREE_POINTER(itor_lib_hanlder);
    }

    *handler_list = NULL;
}


static status_t aurisys_parsing_param_file(
    AurisysLibInterface      *arsi_api,
    const arsi_task_config_t *arsi_task_config,
    const arsi_lib_config_t  *arsi_lib_config,
    char                     *product_info_char,
    char                     *param_file_path_char,
    const int32_t             enhancement_mode,
    data_buf_t               *param_buf,
    const debug_log_fp_t      debug_log_fp) {
    status_t retval = NO_ERROR;
    uint32_t param_buf_size = 0;

    string_buf_t product_info;
    string_buf_t param_file_path;


    if (param_buf == NULL || param_buf->p_buffer == NULL) {
        AUD_LOG_E("param_buf == NULL || param_buf->p_buffer == NULL");
        return BAD_VALUE;
    }

    if (strlen(param_file_path_char) == 0) {
        AUD_LOG_V("<library> param_path not assigned");
        memset(param_buf->p_buffer, 0, param_buf->memory_size);
        return NO_ERROR;
    }


    char_to_string(&product_info, product_info_char);

    char_to_string(&param_file_path, param_file_path_char);

    retval = arsi_api->arsi_query_param_buf_size(arsi_task_config,
                                                 arsi_lib_config,
                                                 &product_info,
                                                 &param_file_path,
                                                 enhancement_mode,
                                                 &param_buf_size,
                                                 debug_log_fp);
    if (retval != NO_ERROR) {
        AUD_LOG_E("arsi_query_param_buf_size fail, retval %d", retval);
        return retval;
    }
    if (param_buf_size > param_buf->memory_size) {
        AUD_LOG_E("param_buf_size %u > memory_size %u!!",
                  param_buf_size, param_buf->memory_size);
        AUD_ASSERT(param_buf_size <= param_buf->memory_size);
        return -ENOMEM;
    }


    memset(param_buf->p_buffer, 0, param_buf->memory_size);
    retval = arsi_api->arsi_parsing_param_file(arsi_task_config,
                                               arsi_lib_config,
                                               &product_info,
                                               &param_file_path,
                                               enhancement_mode,
                                               param_buf,
                                               debug_log_fp);
    if (retval != NO_ERROR) {
        AUD_LOG_E("arsi_parsing_param_file fail, retval %d", retval);
        return retval;
    }

    if (param_buf->data_size == 0) {
        AUD_LOG_W("param_buf->data_size = 0!! => set to %u. Need lib fix it", param_buf_size);
        param_buf->data_size = param_buf_size;
    } else {
        AUD_LOG_V("param_buf->data_size = %u", param_buf->data_size);
    }

    AUD_LOG_D("%s(), product_info \"%s\", file_path \"%s\", enhancement_mode %d"
              ", param_buf_size %u, data_size %u",
              __FUNCTION__,
              product_info.p_string,
              param_file_path.p_string,
              enhancement_mode,
              param_buf_size,
              param_buf->data_size);

    return retval;
}


/*
 * =============================================================================
 *                     utilities implementation
 * =============================================================================
 */

static uint32_t get_size_per_channel(const audio_format_t audio_format) {
    return (uint32_t)AUDIO_BYTES_PER_SAMPLE(audio_format); /* audio.h */
}


static uint32_t get_frame_buf_size(const audio_buf_t *audio_buf) {
    uint32_t frame_buf_size = 0;

    if (audio_buf->frame_size_ms == 0) {
        AUD_LOG_W("frame_size_ms == 0, return");
        return 0;
    }

    frame_buf_size = (get_size_per_channel(audio_buf->audio_format) *
                      audio_buf->num_channels *
                      audio_buf->sample_rate_buffer *
                      audio_buf->frame_size_ms) / 1000;

    AUD_LOG_VV("%s() frame_size_ms %d", __FUNCTION__, audio_buf->frame_size_ms);
    AUD_LOG_VV("%s() num_channels %d", __FUNCTION__, audio_buf->num_channels);
    AUD_LOG_VV("%s() sample_rate_buffer %u", __FUNCTION__, audio_buf->sample_rate_buffer);
    AUD_LOG_VV("%s() audio_format 0x%x", __FUNCTION__, audio_buf->audio_format);
    AUD_LOG_VV("%s() frame_buf_size %u", __FUNCTION__, frame_buf_size);
    AUD_ASSERT(frame_buf_size > 0);
    return frame_buf_size;
}


static void char_to_string(string_buf_t *target, char *source) {
    target->memory_size = strlen(source) + 1;
    target->string_size = strlen(source);
    target->p_string = source;

    AUD_LOG_VV("memory_size = %u", target->memory_size);
    AUD_LOG_VV("string_size = %u", target->string_size);
    AUD_LOG_VV("p_string = %s",    target->p_string);
}


static int linear_to_frame_base(char *linear_buf, char *frame_buf, uint32_t data_size, audio_format_t audio_format, uint8_t num_channels) {
    if (audio_format != AUDIO_FORMAT_PCM_16_BIT &&
        audio_format != AUDIO_FORMAT_PCM_32_BIT &&
        audio_format != AUDIO_FORMAT_PCM_8_24_BIT) {
        AUD_LOG_E("%s(), not support audio_format 0x%x", __FUNCTION__, audio_format);
        return 0;
    }

    if (num_channels != 1 &&
        num_channels != 2 &&
        num_channels != 3 &&
        num_channels != 4) {
        AUD_LOG_E("%s(), not support num_channels %d", __FUNCTION__, num_channels);
        return 0;
    }


    if (num_channels == 1) {
        /* linear base and frame base is the same for mono */
        memcpy(frame_buf, linear_buf, data_size);
        return data_size;
    }


    if (num_channels == 2) {
        if (audio_format == AUDIO_FORMAT_PCM_16_BIT) {
            LINEAR_TO_FRAME_2_CH(linear_buf, frame_buf, data_size, int16_t);
        } else if (audio_format == AUDIO_FORMAT_PCM_32_BIT ||
                   audio_format == AUDIO_FORMAT_PCM_8_24_BIT) {
            LINEAR_TO_FRAME_2_CH(linear_buf, frame_buf, data_size, int32_t);
        }
    } else if (num_channels == 3) {
        if (audio_format == AUDIO_FORMAT_PCM_16_BIT) {
            LINEAR_TO_FRAME_3_CH(linear_buf, frame_buf, data_size, int16_t);
        } else if (audio_format == AUDIO_FORMAT_PCM_32_BIT ||
                   audio_format == AUDIO_FORMAT_PCM_8_24_BIT) {
            LINEAR_TO_FRAME_3_CH(linear_buf, frame_buf, data_size, int32_t);
        }
    } else if (num_channels == 4) {
        if (audio_format == AUDIO_FORMAT_PCM_16_BIT) {
            LINEAR_TO_FRAME_4_CH(linear_buf, frame_buf, data_size, int16_t);
        } else if (audio_format == AUDIO_FORMAT_PCM_32_BIT ||
                   audio_format == AUDIO_FORMAT_PCM_8_24_BIT) {
            LINEAR_TO_FRAME_4_CH(linear_buf, frame_buf, data_size, int32_t);
        }
    }

    return data_size;
}


static int frame_base_to_linear(char *frame_buf, char *linear_buf, uint32_t data_size, audio_format_t audio_format, uint8_t num_channels) {
    if (audio_format != AUDIO_FORMAT_PCM_16_BIT &&
        audio_format != AUDIO_FORMAT_PCM_32_BIT &&
        audio_format != AUDIO_FORMAT_PCM_8_24_BIT) {
        AUD_LOG_E("%s(), not support audio_format 0x%x", __FUNCTION__, audio_format);
        return 0;
    }

    if (num_channels != 1 &&
        num_channels != 2 &&
        num_channels != 3 &&
        num_channels != 4) {
        AUD_LOG_E("%s(), not support num_channels %d", __FUNCTION__, num_channels);
        return 0;
    }


    if (num_channels == 1) {
        /* linear base and frame base is the same for mono */
        memcpy(linear_buf, frame_buf, data_size);
        return data_size;
    }


    if (num_channels == 2) {
        if (audio_format == AUDIO_FORMAT_PCM_16_BIT) {
            FRAME_TO_LINEAR_2_CH(frame_buf, linear_buf, data_size, int16_t);
        } else if (audio_format == AUDIO_FORMAT_PCM_32_BIT ||
                   audio_format == AUDIO_FORMAT_PCM_8_24_BIT) {
            FRAME_TO_LINEAR_2_CH(frame_buf, linear_buf, data_size, int32_t);
        }
    } else if (num_channels == 3) {
        if (audio_format == AUDIO_FORMAT_PCM_16_BIT) {
            FRAME_TO_LINEAR_3_CH(frame_buf, linear_buf, data_size, int16_t);
        } else if (audio_format == AUDIO_FORMAT_PCM_32_BIT ||
                   audio_format == AUDIO_FORMAT_PCM_8_24_BIT) {
            FRAME_TO_LINEAR_3_CH(frame_buf, linear_buf, data_size, int32_t);
        }
    } else if (num_channels == 4) {
        if (audio_format == AUDIO_FORMAT_PCM_16_BIT) {
            FRAME_TO_LINEAR_4_CH(frame_buf, linear_buf, data_size, int16_t);
        } else if (audio_format == AUDIO_FORMAT_PCM_32_BIT ||
                   audio_format == AUDIO_FORMAT_PCM_8_24_BIT) {
            FRAME_TO_LINEAR_4_CH(frame_buf, linear_buf, data_size, int32_t);
        }
    }

    return data_size;
}


#ifdef __cplusplus
}  /* extern "C" */
#endif

