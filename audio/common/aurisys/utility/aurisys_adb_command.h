#ifndef MTK_AURISYS_ADB_COMMAND_H
#define MTK_AURISYS_ADB_COMMAND_H

#include <stdint.h>
#include <stdbool.h>

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

enum {
    AURISYS_DIRECTION_INVALID,
    AURISYS_SET_PARAM,
    AURISYS_GET_PARAM
};

typedef uint8_t adb_cmd_direction_t;


enum {
    /* for all same lib */
    ADB_CMD_PARAM_FILE,
    ADB_CMD_LIB_DUMP_FILE,

    /* for all same component */
    ADB_CMD_ENABLE_LOG,
    ADB_CMD_ENABLE_RAW_DUMP,
    ADB_CMD_ENABLE_LIB_DUMP,
    ADB_CMD_APPLY_PARAM,

    /* for single library hanlder */
    ADB_CMD_ADDR_VALUE,
    ADB_CMD_KEY_VALUE,

    ADB_CMD_SIZE,
    ADB_CMD_INVALID
};

typedef uint8_t adb_cmd_type_t;


typedef struct addr_value_pair_t {
    uint32_t addr;
    uint32_t value;
} addr_value_pair_t;


/*
 * =============================================================================
 *                     struct definition
 * =============================================================================
 */

typedef struct aurisys_adb_command_t {
    adb_cmd_direction_t direction;

    char *target;

    aurisys_scenario_t aurisys_scenario;

    char *adb_cmd_key;

    adb_cmd_type_t adb_cmd_type;

    /* the data only corresponds to one adb_cmd_type_t => union */
    union {
        /* ADB_CMD_PARAM_FILE */
        char *param_path;

        /* ADB_CMD_LIB_DUMP_FILE */
        char *lib_dump_path;

        /* ADB_CMD_ENABLE_LOG */
        bool enable_log;

        /* ADB_CMD_ENABLE_RAW_DUMP */
        bool enable_raw_dump;

        /* ADB_CMD_ENABLE_LIB_DUMP */
        bool enable_lib_dump;

        /* ADB_CMD_APPLY_PARAM (nothing, or enhancement_mode) */
        uint32_t enhancement_mode;

        /* ADB_CMD_ADDR_VALUE */
        addr_value_pair_t addr_value_pair;

        /* ADB_CMD_KEY_VALUE */
        string_buf_t key_value_pair;
    };
} aurisys_adb_command_t;

/*
 * =============================================================================
 *                     public function
 * =============================================================================
 */

int parse_adb_cmd(const char *key_value_pair, aurisys_adb_command_t *adb_cmd);


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* end of MTK_AURISYS_ADB_COMMAND_H */

