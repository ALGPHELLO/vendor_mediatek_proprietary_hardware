#include "aurisys_lib_manager.h"

#include <uthash.h> /* uthash */
#include <utlist.h> /* linked list */

#include <audio_log.h>
#include <audio_assert.h>
#include <audio_debug_tool.h>
#include <audio_memory_control.h>
#include <audio_lock.h>
#include <audio_ringbuf.h>

#include <audio_task.h>
#include <aurisys_scenario.h>

#include <arsi_type.h>
#include <aurisys_config.h>

#include <audio_pool_buf_handler.h>

#include <aurisys_lib_handler.h>



#ifdef __cplusplus
extern "C" {
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "aurisys_lib_manager"


#ifdef AURISYS_DUMP_LOG_V
#undef  AUD_LOG_V
#define AUD_LOG_V(x...) AUD_LOG_D(x)
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


/*
 * =============================================================================
 *                     private global members
 * =============================================================================
 */

static alock_t *g_aurisys_lib_manager_lock;


/*
 * =============================================================================
 *                     private function declaration
 * =============================================================================
 */



/*
 * =============================================================================
 *                     private function implementation
 * =============================================================================
 */

static int aurisys_create_arsi_handlers_xlink(
    aurisys_lib_manager_t *manager,
    aurisys_lib_handler_t **handler_list);



static int aurisys_destroy_arsi_handlers_xlink(
    aurisys_lib_manager_t *manager,
    aurisys_lib_handler_t **handler_list);


/*
 * =============================================================================
 *                     public function implementation
 * =============================================================================
 */

void aurisys_lib_manager_c_file_init(void) {
    if (g_aurisys_lib_manager_lock == NULL) {
        NEW_ALOCK(g_aurisys_lib_manager_lock);
    }
}


void aurisys_lib_manager_c_file_deinit(void) {
#if 0 /* singleton lock */
    if (g_aurisys_lib_manager_lock != NULL) {
        FREE_ALOCK(g_aurisys_lib_manager_lock);
    }
#endif
}


uint8_t map_aurisys_scenario_to_task_scene(const uint32_t aurisys_scenario) {
    task_scene_t task_scene = TASK_SCENE_INVALID;

    switch (aurisys_scenario) {
    case AURISYS_SCENARIO_PLAYBACK_NORMAL:
    case AURISYS_SCENARIO_PLAYBACK_LOW_LATENCY:
        task_scene = TASK_SCENE_PLAYBACK_MP3; /* TODO */
        break;
    case AURISYS_SCENARIO_RECORD_LOW_LATENCY:
    case AURISYS_SCENARIO_RECORD_WITHOUT_AEC:
    case AURISYS_SCENARIO_RECORD_WITH_AEC:
        task_scene = TASK_SCENE_RECORD;
        break;
    case AURISYS_SCENARIO_VOIP:
    case AURISYS_SCENARIO_VOIP_WITHOUT_AEC:
        task_scene = TASK_SCENE_VOIP;
        break;
    case AURISYS_SCENARIO_PHONE_CALL:
        task_scene = TASK_SCENE_PHONE_CALL;
        break;
    default:
        AUD_ASSERT(0);
        break;
    }

    return task_scene;
}


aurisys_scenario_t get_aurisys_scenario_of_manager(aurisys_lib_manager_t *manager) {
    AUD_ASSERT(manager != NULL);
    return manager->aurisys_scenario;
}


uint8_t get_task_scene_of_manager(aurisys_lib_manager_t *manager) {
    AUD_ASSERT(manager != NULL);
    return map_aurisys_scenario_to_task_scene(manager->aurisys_scenario);
}


arsi_task_config_t *get_arsi_task_config(aurisys_lib_manager_t *manager) {
    AUD_ASSERT(manager != NULL);
    return &manager->arsi_task_config;
}


aurisys_lib_manager_t *new_aurisys_lib_manager(
    aurisys_config_t *aurisys_config,
    const aurisys_scenario_t aurisys_scenario,
    const struct aurisys_user_prefer_configs_t *p_prefer_configs) {
    aurisys_scene_lib_table_t *the_scene_lib_table = NULL;
    aurisys_lib_manager_t *new_lib_manager = NULL;

    char *gain_handler_name = NULL;
    aurisys_lib_handler_t *gain_handler = NULL;


    LOCK_ALOCK_MS(g_aurisys_lib_manager_lock, 1000);

    /* get scene_lib_table */ /* TODO: set to aurisys_create_lib_handler_list */
    HASH_FIND_INT(
        aurisys_config->scene_lib_table_hh,
        &aurisys_scenario,
        the_scene_lib_table);
    if (the_scene_lib_table == NULL) {
        AUD_ASSERT(the_scene_lib_table != NULL);
        UNLOCK_ALOCK(g_aurisys_lib_manager_lock);
        return NULL;
    }

    /* create manager */
    AUDIO_ALLOC_STRUCT(aurisys_lib_manager_t, new_lib_manager);

    NEW_ALOCK(new_lib_manager->lock);
    LOCK_ALOCK_MS(new_lib_manager->lock, 1000);

    new_lib_manager->self = new_lib_manager;
    new_lib_manager->aurisys_scenario = aurisys_scenario;

    aurisys_create_lib_handler_list(
        aurisys_config,
        new_lib_manager->aurisys_scenario,
        &new_lib_manager->uplink_lib_handler_list,
        &new_lib_manager->num_uplink_library_hanlder,
        &new_lib_manager->downlink_lib_handler_list,
        &new_lib_manager->num_downlink_library_hanlder,
        p_prefer_configs);


    /* TODO: abstract xlink function */
    if (new_lib_manager->uplink_lib_handler_list != NULL) {
        AUDIO_ALLOC_STRUCT(audio_pool_buf_formatter_t, new_lib_manager->ul_out_pool_formatter);

        gain_handler_name = the_scene_lib_table->uplink_digital_gain_lib_name;
        if (strlen(gain_handler_name) > 0) {
            AUD_LOG_V("UL gain set to %s", gain_handler_name);

            HASH_FIND(hh_manager,
                      new_lib_manager->uplink_lib_handler_list,
                      gain_handler_name,
                      (unsigned)uthash_strlen(gain_handler_name),
                      gain_handler);

            if (gain_handler == NULL) {
                AUD_ASSERT(gain_handler != NULL);
            } else {
                AUD_LOG_V("UL gain set to %s %p", gain_handler->lib_name, gain_handler);
                new_lib_manager->uplink_lib_handler_for_digital_gain = gain_handler;
            }
        }
    }

    if (new_lib_manager->downlink_lib_handler_list != NULL) {
        AUDIO_ALLOC_STRUCT(audio_pool_buf_formatter_t, new_lib_manager->dl_out_pool_formatter);

        gain_handler_name = the_scene_lib_table->downlink_digital_gain_lib_name;
        if (strlen(gain_handler_name) > 0) {
            AUD_LOG_V("DL gain set to %s", gain_handler_name);

            HASH_FIND(hh_manager,
                      new_lib_manager->downlink_lib_handler_list,
                      gain_handler_name,
                      (unsigned)uthash_strlen(gain_handler_name),
                      gain_handler);

            if (gain_handler == NULL) {
                AUD_ASSERT(gain_handler != NULL);
            } else {
                AUD_LOG_V("DL gain set to %s %p", gain_handler->lib_name, gain_handler);
                new_lib_manager->downlink_lib_handler_for_digital_gain = gain_handler;
            }
        }
    }

    /* TODO: less memory */
    AUDIO_ALLOC_STRUCT_ARRAY(audio_pool_buf_t, NUM_DATA_BUF_TYPE, new_lib_manager->in_out_bufs);


    UNLOCK_ALOCK(new_lib_manager->lock);
    UNLOCK_ALOCK(g_aurisys_lib_manager_lock);

    AUD_LOG_D("%s() done, manager %p, num_uplink_library_hanlder %d, num_downlink_library_hanlder %d",
              __FUNCTION__, new_lib_manager, new_lib_manager->num_uplink_library_hanlder, new_lib_manager->num_downlink_library_hanlder);
    return new_lib_manager;
}


int delete_aurisys_lib_manager(aurisys_lib_manager_t *manager) {
    aurisys_lib_handler_t *tmp_lib_hanlder = NULL;

    AUD_LOG_D("%s(), manager %p", __FUNCTION__, manager);

    int num_library_hanlder = 0;
    int i = 0;

    if (manager == NULL) {
        AUD_LOG_E("%s(), manager == NULL!! return", __FUNCTION__);
        return -1;
    }

    LOCK_ALOCK_MS(g_aurisys_lib_manager_lock, 1000);

    LOCK_ALOCK_MS(manager->lock, 1000);


    /* TODO: abstract xlink function */
    /* delete uplink_lib_handler_list */
    if (manager->uplink_lib_handler_list != NULL) {
        AUD_LOG_V("num_uplink_library_hanlder %d", manager->num_uplink_library_hanlder);

        aurisys_destroy_lib_handler_list(&manager->uplink_lib_handler_list);
        AUDIO_FREE_POINTER(manager->ul_out_pool_formatter);
    }

    /* delete downlink_lib_handler_list */
    if (manager->downlink_lib_handler_list != NULL) {
        AUD_LOG_V("num_downlink_library_hanlder %d", manager->num_downlink_library_hanlder);
        aurisys_destroy_lib_handler_list(&manager->downlink_lib_handler_list);
        AUDIO_FREE_POINTER(manager->dl_out_pool_formatter);
    }

    /* free in_out_bufs */
    for (i = 0; i < NUM_DATA_BUF_TYPE; i++) {
        if (manager->in_out_bufs[i].buf != NULL) {
            AUDIO_FREE_POINTER(manager->in_out_bufs[i].buf->data_buf.p_buffer);
            AUDIO_FREE_POINTER(manager->in_out_bufs[i].buf);
        }
    }
    AUDIO_FREE_POINTER(manager->in_out_bufs);


    UNLOCK_ALOCK(manager->lock);

    /* delete manager */
    FREE_ALOCK(manager->lock);
    AUDIO_FREE_POINTER(manager);

    UNLOCK_ALOCK(g_aurisys_lib_manager_lock);

    return 0;
}


int aurisys_create_arsi_handlers(aurisys_lib_manager_t *manager) {
    if (manager == NULL) {
        AUD_LOG_E("%s(), manager == NULL!! return", __FUNCTION__);
        return -1;
    }


    LOCK_ALOCK_MS(manager->lock, 1000);

    aurisys_create_arsi_handlers_xlink(manager, &manager->uplink_lib_handler_list);
    aurisys_create_arsi_handlers_xlink(manager, &manager->downlink_lib_handler_list);

    UNLOCK_ALOCK(manager->lock);

    return 0;
}

int aurisys_destroy_lib_handlers(aurisys_lib_manager_t *manager) {
    if (manager == NULL) {
        AUD_LOG_E("%s(), manager == NULL!! return", __FUNCTION__);
        return -1;
    }

    LOCK_ALOCK_MS(manager->lock, 1000);

    aurisys_destroy_arsi_handlers_xlink(manager, &manager->uplink_lib_handler_list);
    aurisys_destroy_arsi_handlers_xlink(manager, &manager->downlink_lib_handler_list);

    UNLOCK_ALOCK(manager->lock);

    return 0;
}


audio_pool_buf_t *create_audio_pool_buf(
    const aurisys_lib_manager_t *manager,
    const data_buf_type_t        data_buf_type,
    const uint32_t               memory_size) {
    audio_pool_buf_t *pool_buf = NULL;

    if (manager == NULL) {
        AUD_LOG_E("%s(), manager == NULL!! return", __FUNCTION__);
        return NULL;
    }

    pool_buf = &manager->in_out_bufs[data_buf_type];
    if (pool_buf->buf != NULL) {
        AUD_LOG_W("%s(), manager %p scenario %d data_buf_type %d pool_buf->buf != NULL",
                  __FUNCTION__,
                  manager, manager->aurisys_scenario, data_buf_type);
    } else {
        AUDIO_ALLOC_STRUCT(audio_buf_t, pool_buf->buf);

        pool_buf->buf->data_buf_type = data_buf_type;
        pool_buf->buf->data_buf.memory_size = memory_size;
        pool_buf->buf->data_buf.data_size = 0;
        AUDIO_ALLOC_BUFFER(pool_buf->buf->data_buf.p_buffer,
                           pool_buf->buf->data_buf.memory_size);

        config_ringbuf_by_data_buf(&pool_buf->ringbuf, &pool_buf->buf->data_buf);
    }

    return pool_buf;
}


int aurisys_pool_buf_formatter_init(aurisys_lib_manager_t *manager) {
    aurisys_lib_handler_t *itor_lib_handler = NULL;
    aurisys_lib_handler_t *tmp_lib_handler = NULL;

    audio_pool_buf_t *ul_in = NULL;
    audio_pool_buf_t *ul_out = NULL;
    audio_pool_buf_t *aec = NULL;
    audio_pool_buf_t *dl_in = NULL;
    audio_pool_buf_t *dl_out = NULL;

    audio_pool_buf_formatter_t *formatter = NULL;
    audio_buf_t *source = NULL;
    audio_buf_t *target = NULL;

    if (manager == NULL) {
        AUD_LOG_E("%s(), manager == NULL!! return", __FUNCTION__);
        return -1;
    }

    LOCK_ALOCK_MS(manager->lock, 1000);

    ul_in   = &manager->in_out_bufs[DATA_BUF_UPLINK_IN];
    ul_out  = &manager->in_out_bufs[DATA_BUF_UPLINK_OUT];
    dl_in   = &manager->in_out_bufs[DATA_BUF_DOWNLINK_IN];
    dl_out  = &manager->in_out_bufs[DATA_BUF_DOWNLINK_OUT];
    aec     = &manager->in_out_bufs[DATA_BUF_ECHO_REF];


    /* TODO: abstract xlink */
    if (manager->num_uplink_library_hanlder != 0) {
        AUD_ASSERT(ul_in->buf != 0);
        AUD_ASSERT(ul_out->buf != 0);

        /* init in => out */
        manager->ul_out_pool_formatter->pool_source = ul_in;
        manager->ul_out_pool_formatter->pool_target = ul_out;
#ifndef AURISYS_BYPASS_ALL_LIBRARY
        HASH_ITER(hh_manager, manager->uplink_lib_handler_list, itor_lib_handler, tmp_lib_handler) {
            /* insert a lib hanlder into formatter chain */
            formatter = &itor_lib_handler->ul_pool_formatter;
            formatter->pool_source = manager->ul_out_pool_formatter->pool_source;
            formatter->pool_target =                      &itor_lib_handler->ul_pool_in;
            manager->ul_out_pool_formatter->pool_source = &itor_lib_handler->ul_pool_out;

            source = formatter->pool_source->buf;
            target = formatter->pool_target->buf;
            AUD_LOG_D("UL Lib, lib_name %s, %p, sample_rate: %d => %d, num_channels: %d => %d, audio_format: 0x%x => 0x%x, interleave: %d => %d, frame: %d => %d",
                      itor_lib_handler->lib_name,
                      itor_lib_handler,
                      source->sample_rate_buffer, target->sample_rate_buffer,
                      source->num_channels, target->num_channels,
                      source->audio_format, target->audio_format,
                      source->b_interleave, target->b_interleave,
                      source->frame_size_ms, target->frame_size_ms);
            audio_pool_buf_formatter_init(formatter);


            /* aec */
            if (itor_lib_handler->aec_pool_in != NULL) {
                AUD_ASSERT(manager->aec_pool_formatter == NULL); /* only 1 aec in 1 manager, */
                AUDIO_ALLOC_STRUCT(audio_pool_buf_formatter_t, manager->aec_pool_formatter);
                manager->aec_pool_formatter->pool_source = aec;
                manager->aec_pool_formatter->pool_target = itor_lib_handler->aec_pool_in;
            }
        }
#endif
        formatter = manager->ul_out_pool_formatter;
        source = formatter->pool_source->buf;
        target = formatter->pool_target->buf;
        AUD_LOG_D("UL out, sample_rate: %d => %d, num_channels: %d => %d, audio_format: 0x%x => 0x%x, interleave: %d => %d, frame: %d => %d",
                  source->sample_rate_buffer, target->sample_rate_buffer,
                  source->num_channels, target->num_channels,
                  source->audio_format, target->audio_format,
                  source->b_interleave, target->b_interleave,
                  source->frame_size_ms, target->frame_size_ms);
        audio_pool_buf_formatter_init(formatter);

        if (manager->aec_pool_formatter != NULL) {
            formatter = manager->aec_pool_formatter;
            source = formatter->pool_source->buf;
            target = formatter->pool_target->buf;
            AUD_LOG_D("AEC, sample_rate: %d => %d, num_channels: %d => %d, audio_format: 0x%x => 0x%x, interleave: %d => %d, frame: %d => %d",
                      source->sample_rate_buffer, target->sample_rate_buffer,
                      source->num_channels, target->num_channels,
                      source->audio_format, target->audio_format,
                      source->b_interleave, target->b_interleave,
                      source->frame_size_ms, target->frame_size_ms);
            audio_pool_buf_formatter_init(formatter);
        }
    }

    if (manager->num_downlink_library_hanlder != 0) {
        AUD_ASSERT(dl_in->buf != 0);
        AUD_ASSERT(dl_out->buf != 0);

        /* init in => out */
        manager->dl_out_pool_formatter->pool_source = dl_in;
        manager->dl_out_pool_formatter->pool_target = dl_out;
#ifndef AURISYS_BYPASS_ALL_LIBRARY
        HASH_ITER(hh_manager, manager->downlink_lib_handler_list, itor_lib_handler, tmp_lib_handler) {
            /* insert a lib hanlder into formatter chain */
            formatter = &itor_lib_handler->dl_pool_formatter;
            formatter->pool_source = manager->dl_out_pool_formatter->pool_source;
            formatter->pool_target =                      &itor_lib_handler->dl_pool_in;
            manager->dl_out_pool_formatter->pool_source = &itor_lib_handler->dl_pool_out;

            source = formatter->pool_source->buf;
            target = formatter->pool_target->buf;
            AUD_LOG_D("DL Lib, lib_name %s, %p, sample_rate: %d => %d, num_channels: %d => %d, audio_format: 0x%x => 0x%x, interleave: %d => %d, frame: %d => %d",
                      itor_lib_handler->lib_name,
                      itor_lib_handler,
                      source->sample_rate_buffer, target->sample_rate_buffer,
                      source->num_channels, target->num_channels,
                      source->audio_format, target->audio_format,
                      source->b_interleave, target->b_interleave,
                      source->frame_size_ms, target->frame_size_ms);
            audio_pool_buf_formatter_init(formatter);
        }
#endif
        formatter = manager->dl_out_pool_formatter;
        source = formatter->pool_source->buf;
        target = formatter->pool_target->buf;
        AUD_LOG_D("DL out, sample_rate: %d => %d, num_channels: %d => %d, audio_format: 0x%x => 0x%x, interleave: %d => %d, frame: %d => %d",
                  source->sample_rate_buffer, target->sample_rate_buffer,
                  source->num_channels, target->num_channels,
                  source->audio_format, target->audio_format,
                  source->b_interleave, target->b_interleave,
                  source->frame_size_ms, target->frame_size_ms);
        audio_pool_buf_formatter_init(formatter);
    }


    UNLOCK_ALOCK(manager->lock);

    return 0;
}


int aurisys_pool_buf_formatter_deinit(aurisys_lib_manager_t *manager) {
    aurisys_lib_handler_t *itor_lib_handler = NULL;
    aurisys_lib_handler_t *tmp_lib_handler = NULL;

    if (manager == NULL) {
        AUD_LOG_E("%s(), manager == NULL!! return", __FUNCTION__);
        return -1;
    }

    LOCK_ALOCK_MS(manager->lock, 1000);

    if (manager->num_uplink_library_hanlder != 0) {
        HASH_ITER(hh_manager, manager->uplink_lib_handler_list, itor_lib_handler, tmp_lib_handler) {
            audio_pool_buf_formatter_deinit(&itor_lib_handler->ul_pool_formatter);
        }
        audio_pool_buf_formatter_deinit(manager->ul_out_pool_formatter);

        if (manager->aec_pool_formatter != NULL) {
            audio_pool_buf_formatter_deinit(manager->aec_pool_formatter);
            AUDIO_FREE_POINTER(manager->aec_pool_formatter);
        }
    }


    if (manager->num_downlink_library_hanlder != 0) {
        HASH_ITER(hh_manager, manager->downlink_lib_handler_list, itor_lib_handler, tmp_lib_handler) {
            audio_pool_buf_formatter_deinit(&itor_lib_handler->dl_pool_formatter);
        }
        audio_pool_buf_formatter_deinit(manager->dl_out_pool_formatter);
    }

    UNLOCK_ALOCK(manager->lock);

    return 0;
}


int aurisys_process_ul_only(
    aurisys_lib_manager_t *manager,
    audio_pool_buf_t *buf_in,
    audio_pool_buf_t *buf_out,
    audio_pool_buf_t *buf_aec) {
    aurisys_lib_handler_t *itor_lib_handler = NULL;
    aurisys_lib_handler_t *tmp_lib_handler = NULL;

    int process_count = 0;
    int need_format_count = 0;

    if (!manager || !buf_in || !buf_out) {
        AUD_LOG_E("%s(), NULL! return", __FUNCTION__);
        return -1;
    }

    LOCK_ALOCK_MS(manager->lock, 1000);


    AUD_ASSERT(manager->uplink_lib_handler_list != NULL);
    AUD_ASSERT(manager->num_uplink_library_hanlder != 0);
    AUD_ASSERT(manager->ul_out_pool_formatter != NULL);


    AUD_LOG_V("%s(+), rb_in  data_count %u, free_count %u", __FUNCTION__,
              audio_ringbuf_count(&buf_in->ringbuf),
              audio_ringbuf_free_space(&buf_in->ringbuf));
    AUD_LOG_VV("%s(+), rb_out data_count %u, free_count %u", __FUNCTION__,
               audio_ringbuf_count(&buf_out->ringbuf),
               audio_ringbuf_free_space(&buf_out->ringbuf));


    need_format_count = audio_ringbuf_count(&buf_in->ringbuf);

#ifndef AURISYS_BYPASS_ALL_LIBRARY
    HASH_ITER(hh_manager, manager->uplink_lib_handler_list, itor_lib_handler, tmp_lib_handler) {
        /* in src */
        if (need_format_count > 0) {
            audio_pool_buf_formatter_process(&itor_lib_handler->ul_pool_formatter);
        }

        /* aec src */
        if (itor_lib_handler->aec_pool_in != NULL && buf_aec != NULL) {
            AUD_LOG_V("%s(+), rb_aec data_count %u, free_count %u", __FUNCTION__,
                      audio_ringbuf_count(&buf_aec->ringbuf),
                      audio_ringbuf_free_space(&buf_aec->ringbuf));
            AUD_ASSERT(manager->aec_pool_formatter != NULL);
            AUD_ASSERT(audio_ringbuf_count(&buf_aec->ringbuf) != 0); /* TODO: test & remove */
            audio_pool_buf_formatter_process(manager->aec_pool_formatter);
        }

        process_count = aurisys_arsi_process_ul_only(itor_lib_handler);
        need_format_count = process_count;
    }
#endif
    if (need_format_count) {
        audio_pool_buf_formatter_process(manager->ul_out_pool_formatter);
    }


    if (buf_aec != NULL) {
        AUD_LOG_V("%s(-), rb_aec data_count %u, free_count %u", __FUNCTION__,
                  audio_ringbuf_count(&buf_aec->ringbuf),
                  audio_ringbuf_free_space(&buf_aec->ringbuf));
    }
    AUD_LOG_VV("%s(-), rb_in  data_count %u, free_count %u", __FUNCTION__,
               audio_ringbuf_count(&buf_in->ringbuf), audio_ringbuf_free_space(&buf_in->ringbuf));
    AUD_LOG_V("%s(-), rb_out data_count %u, free_count %u", __FUNCTION__,
              audio_ringbuf_count(&buf_out->ringbuf), audio_ringbuf_free_space(&buf_out->ringbuf));

    UNLOCK_ALOCK(manager->lock);

    return 0;
}


/* TODO: abstract xlink */
int aurisys_process_dl_only(
    aurisys_lib_manager_t *manager,
    struct audio_pool_buf_t *buf_in,
    struct audio_pool_buf_t *buf_out) {
    aurisys_lib_handler_t *itor_lib_handler = NULL;
    aurisys_lib_handler_t *tmp_lib_handler = NULL;

    int process_count = 0;
    int need_format_count = 0;

    if (!manager || !buf_in || !buf_out) {
        AUD_LOG_E("%s(), NULL! return", __FUNCTION__);
        return -1;
    }

    LOCK_ALOCK_MS(manager->lock, 1000);


    AUD_ASSERT(manager->downlink_lib_handler_list != NULL);
    AUD_ASSERT(manager->num_downlink_library_hanlder != 0);
    AUD_ASSERT(manager->dl_out_pool_formatter != NULL);


    AUD_LOG_V("%s(+), rb_in  data_count %u, free_count %u", __FUNCTION__,
              audio_ringbuf_count(&buf_in->ringbuf), audio_ringbuf_free_space(&buf_in->ringbuf));
    AUD_LOG_V("%s(+), rb_out data_count %u, free_count %u", __FUNCTION__,
              audio_ringbuf_count(&buf_out->ringbuf), audio_ringbuf_free_space(&buf_out->ringbuf));


    need_format_count = audio_ringbuf_count(&buf_in->ringbuf);

#ifndef AURISYS_BYPASS_ALL_LIBRARY
    HASH_ITER(hh_manager, manager->downlink_lib_handler_list, itor_lib_handler, tmp_lib_handler) {
        if (need_format_count > 0) {
            audio_pool_buf_formatter_process(&itor_lib_handler->dl_pool_formatter);
        }
        process_count = aurisys_arsi_process_dl_only(itor_lib_handler);
        need_format_count = process_count;
    }
#endif
    if (need_format_count) {
        audio_pool_buf_formatter_process(manager->dl_out_pool_formatter);
    }


    AUD_LOG_V("%s(-), rb_in  data_count %u, free_count %u", __FUNCTION__,
              audio_ringbuf_count(&buf_in->ringbuf), audio_ringbuf_free_space(&buf_in->ringbuf));
    AUD_LOG_V("%s(-), rb_out data_count %u, free_count %u", __FUNCTION__,
              audio_ringbuf_count(&buf_out->ringbuf), audio_ringbuf_free_space(&buf_out->ringbuf));

    UNLOCK_ALOCK(manager->lock);

    return 0;
}


int aurisys_process_ul_and_dl(
    aurisys_lib_manager_t *manager,
    struct audio_pool_buf_t *ul_buf_in,
    struct audio_pool_buf_t *ul_buf_out,
    struct audio_pool_buf_t *ul_buf_aec,
    struct audio_pool_buf_t *dl_buf_in,
    struct audio_pool_buf_t *dl_buf_out) {
    if (!manager ||
        !ul_buf_in || !ul_buf_out || !ul_buf_aec ||
        !dl_buf_in || !dl_buf_out) {
        AUD_LOG_E("%s(), NULL! return", __FUNCTION__);
        return -1;
    }

    return 0;
}




int aurisys_set_ul_digital_gain(
    aurisys_lib_manager_t *manager,
    const int16_t ul_analog_gain_ref_only,
    const int16_t ul_digital_gain) {
    aurisys_lib_handler_t *gain_hanlder = NULL;

    if (!manager) {
        AUD_LOG_E("%s(), manager NULL! return", __FUNCTION__);
        return -1;
    }

    gain_hanlder = manager->uplink_lib_handler_for_digital_gain;
    if (!gain_hanlder) {
        AUD_LOG_E("%s(), gain_hanlder NULL! return", __FUNCTION__);
        return -1;
    }


    LOCK_ALOCK_MS(manager->lock, 1000);

    aurisys_arsi_set_ul_digital_gain(
        gain_hanlder,
        ul_analog_gain_ref_only,
        ul_digital_gain);

    UNLOCK_ALOCK(manager->lock);
    return 0;
}

int aurisys_set_dl_digital_gain(
    aurisys_lib_manager_t *manager,
    const int16_t dl_analog_gain_ref_only,
    const int16_t dl_digital_gain) {
    aurisys_lib_handler_t *gain_hanlder = NULL;

    if (!manager) {
        AUD_LOG_E("%s(), manager NULL! return", __FUNCTION__);
        return -1;
    }

    gain_hanlder = manager->downlink_lib_handler_for_digital_gain;
    if (!gain_hanlder) {
        AUD_LOG_E("%s(), gain_hanlder NULL! return", __FUNCTION__);
        return -1;
    }


    LOCK_ALOCK_MS(manager->lock, 1000);

    aurisys_arsi_set_dl_digital_gain(
        gain_hanlder,
        dl_analog_gain_ref_only,
        dl_digital_gain);

    UNLOCK_ALOCK(manager->lock);
    return 0;
}




/*
 * =============================================================================
 *                     private function implementation
 * =============================================================================
 */

static int aurisys_create_arsi_handlers_xlink(
    aurisys_lib_manager_t *manager,
    aurisys_lib_handler_t **handler_list) {
    aurisys_lib_handler_t *itor_lib_handler = NULL;
    aurisys_lib_handler_t *tmp_lib_handler = NULL;


    if (manager == NULL || handler_list == NULL || *handler_list == NULL) {
        return -1;
    }

    HASH_ITER(hh_manager, *handler_list, itor_lib_handler, tmp_lib_handler) {
        aurisys_arsi_create_handler(itor_lib_handler, &manager->arsi_task_config);
    }

    return 0;
}


static int aurisys_destroy_arsi_handlers_xlink(
    aurisys_lib_manager_t *manager,
    aurisys_lib_handler_t **handler_list) {
    aurisys_lib_handler_t *itor_lib_hanlder = NULL;
    aurisys_lib_handler_t *tmp_lib_handler = NULL;


    if (manager == NULL || handler_list == NULL || *handler_list == NULL) {
        return -1;
    }

    HASH_ITER(hh_manager, *handler_list, itor_lib_hanlder, tmp_lib_handler) {
        aurisys_arsi_destroy_handler(itor_lib_hanlder);
    }

    return 0;
}



#ifdef __cplusplus
}  /* extern "C" */
#endif

