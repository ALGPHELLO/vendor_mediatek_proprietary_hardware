#ifndef MTK_AURISYS_CONFIG_H
#define MTK_AURISYS_CONFIG_H

#include <stdbool.h>

#include <uthash.h> /* uthash */

#include <audio_sample_rate.h>
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

struct aurisys_lib_handler_t;
struct AurisysLibInterface;

/*
 * =============================================================================
 *                     typedef
 * =============================================================================
 */

/* --> for <aurisys_scenarios> */
typedef struct aurisys_library_name_t {
    char *name; /* key */

    UT_hash_handle hh; /* makes this structure hashable */
} aurisys_library_name_t;


typedef struct aurisys_scene_lib_table_t {
    aurisys_scenario_t aurisys_scenario;  /* key */

    aurisys_library_name_t *uplink_library_name_list;
    aurisys_library_name_t *downlink_library_name_list;

    char *uplink_digital_gain_lib_name;
    char *downlink_digital_gain_lib_name;

    UT_hash_handle hh; /* makes this structure hashable */
} aurisys_scene_lib_table_t;
/* <-- for <aurisys_scenarios> */


/* --> for <hal_librarys> */
typedef struct aurisys_component_t {
    aurisys_scenario_t aurisys_scenario;  /* key */

    audio_sample_rate_mask_t sample_rate_mask;

    uint32_t support_format_mask; /* audio_support_format_mask_t */
    uint32_t support_frame_ms_mask; /* audio_support_frame_ms_mask_t */

    uint32_t support_channel_number_mask[NUM_DATA_BUF_TYPE]; /* audio_support_channel_number_mask_t */

    arsi_lib_config_t lib_config; /* keep max sample rate in component's lib_config */

    bool enable_log;
    bool enable_raw_dump;
    bool enable_lib_dump;
    uint32_t enhancement_mode;


    struct aurisys_lib_handler_t *lib_handler_list_for_adb_cmd; /* keep pointers for set/get param */

    UT_hash_handle hh; /* makes this structure hashable */
} aurisys_component_t;


typedef struct aurisys_library_config_t {
    char *name; /* key */
    char *lib_path;
    char *lib64_path;
    char *param_path;
    char *lib_dump_path;
    char *adb_cmd_key;

    void *dlopen_handle;
    struct AurisysLibInterface *api;

    aurisys_component_t *component_hh;

    UT_hash_handle hh; /* makes this structure hashable */
} aurisys_library_config_t;
/* <-- for <hal_librarys> */


typedef struct aurisys_config_t {
    aurisys_scene_lib_table_t *scene_lib_table_hh;
    aurisys_library_config_t  *library_config_hh;
} aurisys_config_t;



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


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* end of MTK_AURISYS_CONFIG_H */

