#include "aurisys_adb_command.h"

#include <stdlib.h>
#include <string.h>

#include <audio_log.h>
#include <audio_assert.h>

#include <aurisys_scenario.h>
#include <aurisys_utility.h>


#ifdef __cplusplus
extern "C" {
#endif


#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "aurisys_adb_command"



/*
 * =============================================================================
 *                     MACRO
 * =============================================================================
 */

#define ADB_CMD_STR_PARAM_FILE       "PARAM_FILE"
#define ADB_CMD_STR_LIB_DUMP_FILE    "LIB_DUMP_FILE"

#define ADB_CMD_STR_ENABLE_LOG       "ENABLE_LOG"
#define ADB_CMD_STR_ENABLE_RAW_DUMP  "ENABLE_RAW_DUMP"
#define ADB_CMD_STR_ENABLE_LIB_DUMP  "ENABLE_LIB_DUMP"

#define ADB_CMD_STR_APPLY_PARAM      "APPLY_PARAM"
#define ADB_CMD_STR_ADDR_VALUE       "ADDR_VALUE"
#define ADB_CMD_STR_KEY_VALUE        "KEY_VALUE"


#define MAX_ADB_CMD_LEN      (256)
#define MAX_ADB_CMD_COPY_LEN ((MAX_ADB_CMD_LEN) - 1) /* -1: reserve for '\0' */

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

static char g_key_value_pair_copy[MAX_ADB_CMD_LEN];


/*
 * =============================================================================
 *                     private function declaration
 * =============================================================================
 */


/*
 * =============================================================================
 *                     utilities declaration
 * =============================================================================
 */

static void parse_adb_cmd_target(char **current, aurisys_adb_command_t *adb_cmd);
static void parse_adb_cmd_aurisys_scenario(char **current, aurisys_adb_command_t *adb_cmd);
static void parse_adb_cmd_key(char **current, aurisys_adb_command_t *adb_cmd);
static void parse_adb_cmd_type_and_data(char **current, aurisys_adb_command_t *adb_cmd);



/*
 * =============================================================================
 *                     public function implementation
 * =============================================================================
 */

int parse_adb_cmd(const char *key_value_pair, aurisys_adb_command_t *adb_cmd) {
    char *current = NULL;

    AUD_ASSERT(key_value_pair != NULL);
    AUD_ASSERT(adb_cmd != NULL);

    /* copy key_value_pair to g_key_value_pair_copy */
    if (strlen(key_value_pair) > MAX_ADB_CMD_COPY_LEN) {
        AUD_LOG_W("strlen(%u) > MAX_ADB_CMD_COPY_LEN %d !!",
                  (uint32_t)strlen(key_value_pair), MAX_ADB_CMD_COPY_LEN);
        return -1;
    }
    memset(g_key_value_pair_copy, 0, sizeof(g_key_value_pair_copy));
    strncpy(g_key_value_pair_copy, key_value_pair, MAX_ADB_CMD_COPY_LEN);
    current = g_key_value_pair_copy;


    /* get target */
    parse_adb_cmd_target(&current, adb_cmd);
    if (adb_cmd->target == NULL) {
        return -1;
    }

    /* get aurisys_scenario */
    parse_adb_cmd_aurisys_scenario(&current, adb_cmd);
    if (adb_cmd->aurisys_scenario == AURISYS_SCENARIO_INVALID) {
        return -1;
    }

    /* get adb_cmd_key */
    parse_adb_cmd_key(&current, adb_cmd);
    if (adb_cmd->adb_cmd_key == NULL) {
        return -1;
    }

    /* get adb_cmd type & data */
    parse_adb_cmd_type_and_data(&current, adb_cmd);
    if (adb_cmd->adb_cmd_type == ADB_CMD_INVALID) {
        return -1;
    }

    return 0;
}


/*
 * =============================================================================
 *                     utilities implementation
 * =============================================================================
 */

static void parse_adb_cmd_target(char **current, aurisys_adb_command_t *adb_cmd) {
    const char target_dsp[] = "DSP";
    const char target_hal[] = "HAL";

    char *target = NULL;
    char *end = NULL;


    adb_cmd->target = NULL;

    end = strstr(*current, ",");
    if (end == NULL) {
        AUD_LOG_W("%s() fail", __FUNCTION__);
        return;
    }
    *end = '\0';
    target = *current;
    *current = end + 1;

    /* check value is valid */
    if (strncmp(target, target_dsp, strlen(target)) != 0 &&
        strncmp(target, target_hal, strlen(target)) != 0) {
        AUD_LOG_W("target: %s invalid!!", target);
        return;
    }

#if 1 /* TODO: integrate HAL & SCP */
    if (strncmp(target, target_dsp, strlen(target)) == 0) {
        AUD_LOG_W("should pass to SpeechDriverOpenDSP::SetParameter!!");
        return;
    }
#endif

    adb_cmd->target = target;
    AUD_LOG_V("target: %s", adb_cmd->target);
}


static void parse_adb_cmd_aurisys_scenario(char **current, aurisys_adb_command_t *adb_cmd) {
    uint32_t enum_value = 0xFFFFFFFF;

    char aurisys_scenario_str[MAX_ADB_CMD_LEN];

    const char scenario_prefix[] = "AURISYS_SCENARIO_";

    char *scenario = NULL;
    char *scenario_suffix = NULL;
    char *end   = NULL;


    adb_cmd->aurisys_scenario = AURISYS_SCENARIO_INVALID;

    end = strstr(*current, ",");
    if (end == NULL) {
        AUD_LOG_W("%s() fail", __FUNCTION__);
        return;
    }
    *end = '\0';
    scenario = *current;
    scenario_suffix = *current;
    *current = end + 1;


    if (!strncmp(scenario_prefix, scenario, strlen(scenario_prefix))) {
        AUD_LOG_V("scenario: %s", scenario);
        strncpy(aurisys_scenario_str, scenario, MAX_ADB_CMD_COPY_LEN);
    } else {
        AUD_LOG_V("scenario_suffix: %s", scenario_suffix);
        snprintf(aurisys_scenario_str, MAX_ADB_CMD_COPY_LEN, "%s%s", scenario_prefix, scenario_suffix);
    }

    AUD_LOG_V("aurisys_scenario_str: %s", aurisys_scenario_str);
    enum_value = get_enum_by_string_aurisys_scenario(aurisys_scenario_str);
    AUD_LOG_V("enum_value: %u", enum_value);

    adb_cmd->aurisys_scenario = (enum_value == 0xFFFFFFFF)
                                ? (AURISYS_SCENARIO_INVALID)
                                : ((aurisys_scenario_t)enum_value);
    AUD_LOG_V("aurisys_scenario: %u", adb_cmd->aurisys_scenario);
}


static void parse_adb_cmd_key(char **current, aurisys_adb_command_t *adb_cmd) {
    char *adb_cmd_key = NULL;

    char *end   = NULL;


    adb_cmd->adb_cmd_key = NULL;

    end = strstr(*current, ",");
    if (end == NULL) {
        AUD_LOG_W("%s() fail", __FUNCTION__);
        return;
    }
    *end = '\0';
    adb_cmd_key = *current;
    *current = end + 1;

    adb_cmd->adb_cmd_key = adb_cmd_key;
    AUD_LOG_V("adb_cmd_key: %s", adb_cmd->adb_cmd_key);
}


static void parse_adb_cmd_type_and_data(char **current, aurisys_adb_command_t *adb_cmd) {
    static char local_key_value_buf_for_lib[MAX_ADB_CMD_LEN]; /* for ADB_CMD_KEY_VALUE */

    char *adb_cmd_str = NULL;
    char *end = NULL;
    char *data = NULL;
    char *comma = NULL;


    adb_cmd->adb_cmd_type = ADB_CMD_INVALID;

    if (adb_cmd->direction != AURISYS_SET_PARAM &&
        adb_cmd->direction != AURISYS_GET_PARAM) {
        AUD_LOG_W("%s() direction %d error!!", __FUNCTION__, adb_cmd->direction);
        return;
    }


    if (adb_cmd->direction == AURISYS_SET_PARAM) {
        end = strstr(*current, "=SET");
        if (end == NULL) {
            AUD_LOG_W("%s() fail", __FUNCTION__);
            return;
        }
        *end = '\0';
    }
    adb_cmd_str = *current;


    AUD_LOG_V("%s(+) %s", __FUNCTION__, adb_cmd_str);

    if (!strncmp(adb_cmd_str, ADB_CMD_STR_PARAM_FILE, strlen(ADB_CMD_STR_PARAM_FILE))) {
        if (adb_cmd->direction == AURISYS_SET_PARAM) {
            data = adb_cmd_str + strlen(ADB_CMD_STR_PARAM_FILE) + 1; /* +1: skip ',' */
            AUD_ASSERT(strlen(data) != 0);
            adb_cmd->param_path = data;
            adb_cmd->adb_cmd_type = ADB_CMD_PARAM_FILE;
            AUD_LOG_V("%s: %s", ADB_CMD_STR_PARAM_FILE, adb_cmd->param_path);
        } else if (adb_cmd->direction == AURISYS_GET_PARAM) {
            adb_cmd->adb_cmd_type = ADB_CMD_PARAM_FILE;
        }
    } else if (!strncmp(adb_cmd_str, ADB_CMD_STR_LIB_DUMP_FILE, strlen(ADB_CMD_STR_LIB_DUMP_FILE))) {
        if (adb_cmd->direction == AURISYS_SET_PARAM) {
            data = adb_cmd_str + strlen(ADB_CMD_STR_LIB_DUMP_FILE) + 1; /* +1: skip ',' */
            AUD_ASSERT(strlen(data) != 0);
            adb_cmd->lib_dump_path = data;
            adb_cmd->adb_cmd_type = ADB_CMD_LIB_DUMP_FILE;
            AUD_LOG_V("%s: %s", ADB_CMD_STR_LIB_DUMP_FILE, adb_cmd->lib_dump_path);
        } else if (adb_cmd->direction == AURISYS_GET_PARAM) {
            adb_cmd->adb_cmd_type = ADB_CMD_LIB_DUMP_FILE;
        }
    } else if (!strncmp(adb_cmd_str, ADB_CMD_STR_ENABLE_LOG, strlen(ADB_CMD_STR_ENABLE_LOG))) {
        if (adb_cmd->direction == AURISYS_SET_PARAM) {
            data = adb_cmd_str + strlen(ADB_CMD_STR_ENABLE_LOG) + 1; /* +1: skip ',' */
            AUD_ASSERT(strlen(data) != 0);
            adb_cmd->enable_log = (*data == '0') ? 0 : 1;
            adb_cmd->adb_cmd_type = ADB_CMD_ENABLE_LOG;
            AUD_LOG_V("%s: %d", ADB_CMD_STR_ENABLE_LOG, adb_cmd->enable_log);
        } else if (adb_cmd->direction == AURISYS_GET_PARAM) {
            adb_cmd->adb_cmd_type = ADB_CMD_ENABLE_LOG;
        }
    } else if (!strncmp(adb_cmd_str, ADB_CMD_STR_ENABLE_RAW_DUMP, strlen(ADB_CMD_STR_ENABLE_RAW_DUMP))) {
        if (adb_cmd->direction == AURISYS_SET_PARAM) {
            data = adb_cmd_str + strlen(ADB_CMD_STR_ENABLE_RAW_DUMP) + 1; /* +1: skip ',' */
            AUD_ASSERT(strlen(data) != 0);
            adb_cmd->enable_raw_dump = (*data == '0') ? 0 : 1;
            adb_cmd->adb_cmd_type = ADB_CMD_ENABLE_RAW_DUMP;
            AUD_LOG_V("%s: %d", ADB_CMD_STR_ENABLE_RAW_DUMP, adb_cmd->enable_raw_dump);
        } else if (adb_cmd->direction == AURISYS_GET_PARAM) {
            adb_cmd->adb_cmd_type = ADB_CMD_ENABLE_RAW_DUMP;
        }
    } else if (!strncmp(adb_cmd_str, ADB_CMD_STR_ENABLE_LIB_DUMP, strlen(ADB_CMD_STR_ENABLE_LIB_DUMP))) {
        if (adb_cmd->direction == AURISYS_SET_PARAM) {
            data = adb_cmd_str + strlen(ADB_CMD_STR_ENABLE_LIB_DUMP) + 1; /* +1: skip ',' */
            AUD_ASSERT(strlen(data) != 0);
            adb_cmd->enable_lib_dump = (*data == '0') ? 0 : 1;
            adb_cmd->adb_cmd_type = ADB_CMD_ENABLE_LIB_DUMP;
            AUD_LOG_V("%s: %d", ADB_CMD_STR_ENABLE_LIB_DUMP, adb_cmd->enable_lib_dump);
        } else if (adb_cmd->direction == AURISYS_GET_PARAM) {
            adb_cmd->adb_cmd_type = ADB_CMD_ENABLE_LIB_DUMP;
        }
    } else if (!strncmp(adb_cmd_str, ADB_CMD_STR_APPLY_PARAM, strlen(ADB_CMD_STR_APPLY_PARAM))) {
        if (adb_cmd->direction == AURISYS_SET_PARAM) {
            data = adb_cmd_str + strlen(ADB_CMD_STR_APPLY_PARAM) + 1; /* +1: skip ',' */
            AUD_ASSERT(strlen(data) != 0);
            adb_cmd->enhancement_mode = atol(data);
            adb_cmd->adb_cmd_type = ADB_CMD_APPLY_PARAM;
            AUD_LOG_V("%s: %u", ADB_CMD_STR_APPLY_PARAM, adb_cmd->enhancement_mode);
        } else if (adb_cmd->direction == AURISYS_GET_PARAM) {
            adb_cmd->adb_cmd_type = ADB_CMD_APPLY_PARAM;
        }
    } else if (!strncmp(adb_cmd_str, ADB_CMD_STR_ADDR_VALUE, strlen(ADB_CMD_STR_ADDR_VALUE))) {
        if (adb_cmd->direction == AURISYS_SET_PARAM) {
            data = adb_cmd_str + strlen(ADB_CMD_STR_ADDR_VALUE) + 1; /* +1: skip ',' */
            sscanf(data, "%x,%x", &adb_cmd->addr_value_pair.addr, &adb_cmd->addr_value_pair.value);
            adb_cmd->adb_cmd_type = ADB_CMD_ADDR_VALUE;
            AUD_LOG_V("%s: *0x%x = 0x%x", ADB_CMD_STR_ADDR_VALUE,
                      adb_cmd->addr_value_pair.addr, adb_cmd->addr_value_pair.value);
        } else if (adb_cmd->direction == AURISYS_GET_PARAM) {
            data = adb_cmd_str + strlen(ADB_CMD_STR_ADDR_VALUE) + 1; /* +1: skip ',' */
            sscanf(data, "%x", &adb_cmd->addr_value_pair.addr);
            adb_cmd->adb_cmd_type = ADB_CMD_ADDR_VALUE;
            AUD_LOG_V("%s: 0x%x", ADB_CMD_STR_ADDR_VALUE, adb_cmd->addr_value_pair.addr);
        }
    } else if (!strncmp(adb_cmd_str, ADB_CMD_STR_KEY_VALUE, strlen(ADB_CMD_STR_KEY_VALUE))) {
        if (adb_cmd->direction == AURISYS_SET_PARAM) {
            data = adb_cmd_str + strlen(ADB_CMD_STR_KEY_VALUE) + 1; /* +1: skip ',' */
            comma = strstr(data, ",");
            AUD_ASSERT(comma != NULL);
            *comma = '=';
            AUD_LOG_V("key_value: %s", data);
            adb_cmd->key_value_pair.memory_size = strlen(data) + 1;
            adb_cmd->key_value_pair.string_size = strlen(data);
            adb_cmd->key_value_pair.p_string = data;

            adb_cmd->adb_cmd_type = ADB_CMD_KEY_VALUE;
            AUD_LOG_V("%s: %u, %u, %s", ADB_CMD_STR_KEY_VALUE,
                      adb_cmd->key_value_pair.memory_size,
                      adb_cmd->key_value_pair.string_size,
                      adb_cmd->key_value_pair.p_string);
        } else if (adb_cmd->direction == AURISYS_GET_PARAM) {
            data = adb_cmd_str + strlen(ADB_CMD_STR_KEY_VALUE) + 1; /* +1: skip ',' */
            strncpy(local_key_value_buf_for_lib, data, MAX_ADB_CMD_COPY_LEN);

            AUD_LOG_V("key: %s", local_key_value_buf_for_lib);
            adb_cmd->key_value_pair.memory_size = MAX_ADB_CMD_LEN;
            adb_cmd->key_value_pair.string_size = strlen(local_key_value_buf_for_lib);
            adb_cmd->key_value_pair.p_string = local_key_value_buf_for_lib;

            adb_cmd->adb_cmd_type = ADB_CMD_KEY_VALUE;
            AUD_LOG_V("%s: %u, %u, %s", ADB_CMD_STR_KEY_VALUE,
                      adb_cmd->key_value_pair.memory_size,
                      adb_cmd->key_value_pair.string_size,
                      adb_cmd->key_value_pair.p_string);
        }
    } else {
        AUD_LOG_W("%s not support!!", adb_cmd_str);
    }

    AUD_LOG_V("%s(-) %s, adb_cmd_type %d", __FUNCTION__, adb_cmd_str, adb_cmd->adb_cmd_type);
}



#ifdef __cplusplus
}  /* extern "C" */
#endif


