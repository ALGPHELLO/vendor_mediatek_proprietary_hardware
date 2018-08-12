#ifndef MTK_AURISYS_LIB_MANAGER_H
#define MTK_AURISYS_LIB_MANAGER_H

#include <uthash.h> /* uthash */

#include <audio_log.h>
#include <audio_assert.h>
#include <audio_memory_control.h>
#include <audio_lock.h>
#include <audio_ringbuf.h>


#include <aurisys_scenario.h>

#include <arsi_type.h>



#ifdef __cplusplus
extern "C" {
#endif



/*
 * =============================================================================
 *                     ref struct
 * =============================================================================
 */

struct data_buf_t;
struct arsi_task_config_t;
struct aurisys_config_t;
struct aurisys_lib_handler_t;
//struct audio_pool_buf_handler_hh_t;
struct audio_pool_buf_t;
struct audio_pool_buf_formatter_t;
struct aurisys_user_prefer_configs_t;



/*
 * =============================================================================
 *                     struct definition
 * =============================================================================
 */

typedef struct aurisys_lib_manager_t {
    void *self; /* key */

    alock_t *lock;

    aurisys_scenario_t aurisys_scenario;
    arsi_task_config_t arsi_task_config;

    struct aurisys_lib_handler_t *uplink_lib_handler_list;
    struct aurisys_lib_handler_t *downlink_lib_handler_list;

    uint32_t num_uplink_library_hanlder;
    uint32_t num_downlink_library_hanlder;

    struct aurisys_lib_handler_t *uplink_lib_handler_for_digital_gain;
    struct aurisys_lib_handler_t *downlink_lib_handler_for_digital_gain;

    struct audio_pool_buf_formatter_t *ul_out_pool_formatter;
    struct audio_pool_buf_formatter_t *dl_out_pool_formatter;

    struct audio_pool_buf_formatter_t *aec_pool_formatter;

    //struct audio_pool_buf_handler_hh_t *pool_buf_handler_hh;
    struct audio_pool_buf_t *in_out_bufs;

    UT_hash_handle hh; /* makes this structure hashable */
} aurisys_lib_manager_t;


/*
 * =============================================================================
 *                     public function
 * =============================================================================
 */

void aurisys_lib_manager_c_file_init(void);
void aurisys_lib_manager_c_file_deinit(void);


uint8_t map_aurisys_scenario_to_task_scene(const uint32_t aurisys_scenario);

aurisys_scenario_t get_aurisys_scenario_of_manager(aurisys_lib_manager_t *manager);
uint8_t get_task_scene_of_manager(aurisys_lib_manager_t *manager);

struct arsi_task_config_t *get_arsi_task_config(aurisys_lib_manager_t *manager);


struct aurisys_lib_manager_t *new_aurisys_lib_manager(
    struct aurisys_config_t *aurisys_config,
    const aurisys_scenario_t aurisys_scenario,
    const struct aurisys_user_prefer_configs_t *p_prefer_configs);

int delete_aurisys_lib_manager(aurisys_lib_manager_t *manager);


int aurisys_create_arsi_handlers(aurisys_lib_manager_t *manager);
int aurisys_destroy_lib_handlers(aurisys_lib_manager_t *manager);


/* NOTE: auto destroy audio_pool_buf when delete_aurisys_lib_manager() */
struct audio_pool_buf_t *create_audio_pool_buf(
    const aurisys_lib_manager_t *manager,
    const data_buf_type_t        data_buf_type,
    const uint32_t               memory_size);


int aurisys_pool_buf_formatter_init(aurisys_lib_manager_t *manager);
int aurisys_pool_buf_formatter_deinit(aurisys_lib_manager_t *manager);


int aurisys_process_ul_only(
    aurisys_lib_manager_t *manager,
    struct audio_pool_buf_t *buf_in,
    struct audio_pool_buf_t *buf_out,
    struct audio_pool_buf_t *buf_aec);

int aurisys_process_dl_only(
    aurisys_lib_manager_t *manager,
    struct audio_pool_buf_t *buf_in,
    struct audio_pool_buf_t *buf_out);

int aurisys_process_ul_and_dl(
    aurisys_lib_manager_t *manager,
    struct audio_pool_buf_t *ul_buf_in,
    struct audio_pool_buf_t *ul_buf_out,
    struct audio_pool_buf_t *ul_buf_aec,
    struct audio_pool_buf_t *dl_buf_in,
    struct audio_pool_buf_t *dl_buf_out);


int aurisys_set_ul_digital_gain(
    aurisys_lib_manager_t *manager,
    const int16_t ul_analog_gain_ref_only,
    const int16_t ul_digital_gain);

int aurisys_set_dl_digital_gain(
    aurisys_lib_manager_t *manager,
    const int16_t dl_analog_gain_ref_only,
    const int16_t dl_digital_gain);




#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* end of MTK_AURISYS_LIB_MANAGER_H */

