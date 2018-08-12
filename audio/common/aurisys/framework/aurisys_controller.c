#include "aurisys_controller.h"

#include <uthash.h> /* uthash */

#include <audio_log.h>
#include <audio_assert.h>
#include <audio_debug_tool.h>
#include <audio_memory_control.h>
#include <audio_lock.h>

#include <aurisys_scenario.h>

#include <arsi_type.h>
#include <aurisys_config.h>

#include <audio_pool_buf_handler.h>

#include <aurisys_utility.h>
#include <aurisys_config_parser.h>
#include <aurisys_lib_manager.h>
#include <aurisys_lib_handler.h>

#include <aurisys_adb_command.h>


#ifdef __cplusplus
extern "C" {
#endif


#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "aurisys_controller"


#ifdef AURISYS_DUMP_LOG_V
#undef  AUD_LOG_V
#define AUD_LOG_V(x...) AUD_LOG_D(x)
#endif



/*
 * =============================================================================
 *                     MACRO
 * =============================================================================
 */

#define MAX_PARAM_PATH_LEN      (256)
#define MAX_PARAM_PATH_COPY_LEN ((MAX_PARAM_PATH_LEN) - 1) /* -1: reserve for '\0' */


/*
 * =============================================================================
 *                     typedef
 * =============================================================================
 */

typedef struct aurisys_controller_t {
    alock_t *lock;

    aurisys_config_t *aurisys_config;

    aurisys_lib_manager_t *manager_hh;

    int retval_of_set_rarameter; /* for adb cmd. -1: invalid, 0: fail, 1: pass */

    bool aurisys_on;
} aurisys_controller_t;


/*
 * =============================================================================
 *                     private global members
 * =============================================================================
 */

static alock_t *g_aurisys_controller_lock;

static aurisys_controller_t *g_controller; /* singleton */


/*
 * =============================================================================
 *                     private function declaration
 * =============================================================================
 */

static int aurisys_set_parameter_to_component(
    aurisys_library_config_t *library_config,
    aurisys_component_t *component,
    aurisys_adb_command_t *adb_cmd);


/*
 * =============================================================================
 *                     utilities declaration
 * =============================================================================
 */


/*
 * =============================================================================
 *                     public function implementation
 * =============================================================================
 */

int init_aurisys_controller(void) {
    AUD_LOG_D("%s(+)", __FUNCTION__);

    if (g_aurisys_controller_lock == NULL) {
        NEW_ALOCK(g_aurisys_controller_lock);
    }

    /* create controller */
    LOCK_ALOCK_MS(g_aurisys_controller_lock, 100);
    if (g_controller != NULL) {
        AUD_LOG_E("%s(-), g_controller != NULL!! return", __FUNCTION__);
        UNLOCK_ALOCK(g_aurisys_controller_lock);
        return -1;
    }

    AUDIO_ALLOC_STRUCT(aurisys_controller_t, g_controller);
    NEW_ALOCK(g_controller->lock);
    UNLOCK_ALOCK(g_aurisys_controller_lock);


    /* init manager & handler */
    audio_pool_buf_handler_c_file_init();
    aurisys_lib_manager_c_file_init();
    aurisys_lib_handler_c_file_init();

    /* parse aurisys_config */
    LOCK_ALOCK_MS(g_controller->lock, 500);
    g_controller->aurisys_config = parse_aurisys_config();
    g_controller->retval_of_set_rarameter = -1;

    /* enable */
    g_controller->aurisys_on = true;
    UNLOCK_ALOCK(g_controller->lock);

    AUD_LOG_D("%s(-)", __FUNCTION__);
    return 0;
}


int deinit_aurisys_controller(void) {
    AUD_LOG_D("%s(+)", __FUNCTION__);

    LOCK_ALOCK_MS(g_aurisys_controller_lock, 1000);
    if (g_controller == NULL) {
        AUD_LOG_E("%s(-), g_controller == NULL!! return", __FUNCTION__);
        UNLOCK_ALOCK(g_aurisys_controller_lock);
        return -1;
    }

    /* delete aurisys_config */
    LOCK_ALOCK_MS(g_controller->lock, 500);
    delete_aurisys_config(g_controller->aurisys_config);
    g_controller->aurisys_config = NULL;
    UNLOCK_ALOCK(g_controller->lock);

    /* deinit manager & handler */
    aurisys_lib_handler_c_file_deinit();
    aurisys_lib_manager_c_file_deinit();
    audio_pool_buf_handler_c_file_deinit();


    /* destroy controller */
    FREE_ALOCK(g_controller->lock);
    AUDIO_FREE_POINTER(g_controller);
    g_controller = NULL;
    UNLOCK_ALOCK(g_aurisys_controller_lock);

#if 0 /* singleton lock */
    FREE_ALOCK(g_aurisys_controller_lock);
#endif

    AUD_LOG_D("%s(-)", __FUNCTION__);
    return 0;
}


aurisys_lib_manager_t *create_aurisys_lib_manager(
    const aurisys_scenario_t aurisys_scenario,
    const struct aurisys_user_prefer_configs_t *p_prefer_configs) {
    AUD_LOG_V("%s(), aurisys_scenario = %d", __FUNCTION__, aurisys_scenario);

    aurisys_lib_manager_t *new_manager = NULL;

    LOCK_ALOCK_MS(g_controller->lock, 1000);
    new_manager = new_aurisys_lib_manager(
                      g_controller->aurisys_config,
                      aurisys_scenario,
                      p_prefer_configs);
    HASH_ADD_PTR(g_controller->manager_hh, self, new_manager);
    UNLOCK_ALOCK(g_controller->lock);

    return new_manager;
}


int destroy_aurisys_lib_manager(aurisys_lib_manager_t *manager) {
    if (manager == NULL) {
        AUD_LOG_E("%s(), manager == NULL!! return", __FUNCTION__);
        return -1;
    }

    LOCK_ALOCK_MS(g_controller->lock, 1000);

    /* destroy manager */
    HASH_DEL(g_controller->manager_hh, manager);
    delete_aurisys_lib_manager(manager);

    UNLOCK_ALOCK(g_controller->lock);

    return 0;
}


int aurisys_set_parameter(const char *key_value_pair) {
    aurisys_adb_command_t adb_cmd;

    aurisys_library_config_t *library_config = NULL;
    aurisys_component_t *component = NULL;

    aurisys_library_config_t *itor_library_config = NULL;
    aurisys_library_config_t *tmp_library_config = NULL;

    aurisys_scenario_t itor_aurisys_scenario = AURISYS_SCENARIO_INVALID;

    int ret = 0;


    LOCK_ALOCK_MS(g_controller->lock, 1000);
    AUD_LOG_V("%s(+) %s", __FUNCTION__, key_value_pair);

    /* parse adb command */
    memset(&adb_cmd, 0, sizeof(adb_cmd));
    adb_cmd.direction = AURISYS_SET_PARAM;
    ret = parse_adb_cmd(key_value_pair, &adb_cmd);
    if (ret != 0) {
        AUD_LOG_W("%s parsing error!! return fail!!", key_value_pair);
        goto AURISYS_SET_PARAM_EXIT;
    }

    AUD_ASSERT(adb_cmd.target != NULL);
    AUD_ASSERT(adb_cmd.aurisys_scenario != AURISYS_SCENARIO_INVALID);
    AUD_ASSERT(adb_cmd.adb_cmd_key != NULL);
    AUD_ASSERT(adb_cmd.adb_cmd_type != ADB_CMD_INVALID);


    /* find library_config by adb_cmd_key */
    AUD_ASSERT(g_controller->aurisys_config != NULL);
    AUD_ASSERT(g_controller->aurisys_config->library_config_hh != NULL);
    HASH_ITER(hh, g_controller->aurisys_config->library_config_hh, itor_library_config, tmp_library_config) {
        if (!strncmp(adb_cmd.adb_cmd_key, itor_library_config->adb_cmd_key, strlen(adb_cmd.adb_cmd_key))) {
            library_config = itor_library_config;
            break;
        }
    }
    if (library_config == NULL) {
        AUD_LOG_W("%s not found for any <library>!! return fail!!", adb_cmd.adb_cmd_key);
        ret = -1;
        goto AURISYS_SET_PARAM_EXIT;
    }
    AUD_LOG_V("%s() library_config->name: %s", __FUNCTION__, library_config->name);


    /* find component by aurisys_scenario */
    AUD_ASSERT(library_config->component_hh != NULL);
    if (adb_cmd.aurisys_scenario != AURISYS_SCENARIO_ALL) {
        HASH_FIND_INT(library_config->component_hh, &adb_cmd.aurisys_scenario, component);
        if (component == NULL) {
            AUD_LOG_W("%s not support aurisys_scenario %u!! return fail!!",
                      library_config->name, adb_cmd.aurisys_scenario);
            ret = -1;
            goto AURISYS_SET_PARAM_EXIT;
        }
        ret = aurisys_set_parameter_to_component(library_config, component, &adb_cmd);
    } else {
        for (itor_aurisys_scenario = AURISYS_SCENARIO_PLAYBACK_NORMAL;
             itor_aurisys_scenario < AURISYS_SCENARIO_SIZE;
             itor_aurisys_scenario++) {
            HASH_FIND_INT(library_config->component_hh, &itor_aurisys_scenario, component);
            if (component != NULL) {
                AUD_LOG_D("%s set aurisys_scenario %u",
                          library_config->name, itor_aurisys_scenario);
                ret += aurisys_set_parameter_to_component(library_config, component, &adb_cmd);
            }
        }
    }


AURISYS_SET_PARAM_EXIT:
    g_controller->retval_of_set_rarameter = (ret == 0) ? 1 : 0;
    AUD_LOG_V("%s(-) %s, retval = %d", __FUNCTION__, key_value_pair, g_controller->retval_of_set_rarameter);
    UNLOCK_ALOCK(g_controller->lock);
    return g_controller->retval_of_set_rarameter;
}


char *aurisys_get_parameter(const char *key) {
    aurisys_adb_command_t adb_cmd;

    static char local_get_parameter_return_buf[MAX_PARAM_PATH_LEN]; /* for return string */

    aurisys_library_config_t *library_config = NULL;
    aurisys_component_t *component = NULL;

    aurisys_library_config_t *itor_library_config = NULL;
    aurisys_library_config_t *tmp_library_config = NULL;

    aurisys_lib_handler_t *itor_lib_handler = NULL;
    aurisys_lib_handler_t *tmp_lib_handler = NULL;

    aurisys_scenario_t itor_aurisys_scenario = AURISYS_SCENARIO_INVALID;

    char *equal = NULL;

    int ret = 0;


    LOCK_ALOCK_MS(g_controller->lock, 1000);
    AUD_LOG_V("%s(+) %s", __FUNCTION__, key);

    /* parse adb command */
    memset(&local_get_parameter_return_buf, 0, sizeof(local_get_parameter_return_buf));
    memset(&adb_cmd, 0, sizeof(adb_cmd));
    adb_cmd.direction = AURISYS_GET_PARAM;
    ret = parse_adb_cmd(key, &adb_cmd);
    if (ret != 0) {
        AUD_LOG_W("%s parsing error!! return fail!!", key);
        goto AURISYS_GET_PARAM_EXIT;
    }

    AUD_ASSERT(adb_cmd.target != NULL);
    AUD_ASSERT(adb_cmd.aurisys_scenario != AURISYS_SCENARIO_INVALID);
    AUD_ASSERT(adb_cmd.adb_cmd_key != NULL);
    AUD_ASSERT(adb_cmd.adb_cmd_type != ADB_CMD_INVALID);


    /* find library_config by adb_cmd_key */
    AUD_ASSERT(g_controller->aurisys_config != NULL);
    AUD_ASSERT(g_controller->aurisys_config->library_config_hh != NULL);
    HASH_ITER(hh, g_controller->aurisys_config->library_config_hh, itor_library_config, tmp_library_config) {
        if (!strncmp(adb_cmd.adb_cmd_key, itor_library_config->adb_cmd_key, strlen(adb_cmd.adb_cmd_key))) {
            library_config = itor_library_config;
            break;
        }
    }
    if (library_config == NULL) {
        AUD_LOG_W("%s not found for any <library>!! return fail!!", adb_cmd.adb_cmd_key);
        ret = -1;
        goto AURISYS_GET_PARAM_EXIT;
    }
    AUD_LOG_V("%s() library_config->name: %s", __FUNCTION__, library_config->name);


    /* find component by aurisys_scenario */
    AUD_ASSERT(library_config->component_hh != NULL);
    if (adb_cmd.aurisys_scenario != AURISYS_SCENARIO_ALL) {
        HASH_FIND_INT(library_config->component_hh, &adb_cmd.aurisys_scenario, component);
    } else {
        /* get the first matched component for AURISYS_SCENARIO_ALL */
        for (itor_aurisys_scenario = AURISYS_SCENARIO_PLAYBACK_NORMAL;
             itor_aurisys_scenario < AURISYS_SCENARIO_SIZE;
             itor_aurisys_scenario++) {
            HASH_FIND_INT(library_config->component_hh, &itor_aurisys_scenario, component);
            if (component != NULL) {
                AUD_LOG_D("%s get aurisys_scenario %u",
                          library_config->name, itor_aurisys_scenario);
                break;
            }
        }
    }
    if (component == NULL) {
        AUD_LOG_W("%s not support aurisys_scenario %u!! return fail!!",
                  library_config->name, adb_cmd.aurisys_scenario);
        ret = -1;
        goto AURISYS_GET_PARAM_EXIT;
    }


    switch (adb_cmd.adb_cmd_type) {
    /* for all same lib */
    case ADB_CMD_PARAM_FILE:
        snprintf(local_get_parameter_return_buf, MAX_PARAM_PATH_COPY_LEN,
                 "%s", library_config->param_path);
        ret = 0;
        break;
    case ADB_CMD_LIB_DUMP_FILE:
        snprintf(local_get_parameter_return_buf, MAX_PARAM_PATH_COPY_LEN,
                 "%s", library_config->lib_dump_path);
        ret = 0;
        break;
    /* for all same component */
    case ADB_CMD_ENABLE_LOG:
        snprintf(local_get_parameter_return_buf, MAX_PARAM_PATH_COPY_LEN,
                 "%d", component->enable_log);
        ret = 0;
        break;
    case ADB_CMD_ENABLE_RAW_DUMP:
        snprintf(local_get_parameter_return_buf, MAX_PARAM_PATH_COPY_LEN,
                 "%d", component->enable_raw_dump);
        ret = 0;
        break;
    case ADB_CMD_ENABLE_LIB_DUMP:
        snprintf(local_get_parameter_return_buf, MAX_PARAM_PATH_COPY_LEN,
                 "%d", component->enable_lib_dump);
        ret = 0;
        break;
    case ADB_CMD_APPLY_PARAM:
        snprintf(local_get_parameter_return_buf, MAX_PARAM_PATH_COPY_LEN,
                 "%d", component->enhancement_mode);
        ret = 0;
        break;
    /* for single library hanlder */
    default:
        /* HASH_ITER all library hanlder later */
        ret = -1;
        break;
    }

    /* list all the lib handlers in the same component */
    HASH_ITER(hh_component, component->lib_handler_list_for_adb_cmd, itor_lib_handler, tmp_lib_handler) {
        AUD_LOG_V("HASH_ITER: %p", itor_lib_handler);
        ret += aurisys_arsi_run_adb_cmd(itor_lib_handler, &adb_cmd);

        if (ret == 0) {
            switch (adb_cmd.adb_cmd_type) {
            case ADB_CMD_ADDR_VALUE:
                snprintf(local_get_parameter_return_buf, MAX_PARAM_PATH_COPY_LEN,
                         "0x%x", adb_cmd.addr_value_pair.value);
                break;
            case ADB_CMD_KEY_VALUE:
                equal = strstr(adb_cmd.key_value_pair.p_string, "=");
                if (equal == NULL) {
                    AUD_LOG_W("%s() key_value_pair.p_string %s error!!",
                              __FUNCTION__, adb_cmd.key_value_pair.p_string);
                    ret = -1;
                } else {
                    snprintf(local_get_parameter_return_buf, MAX_PARAM_PATH_COPY_LEN,
                             "%s", equal + 1); /* +1: skip '=' */
                }
                break;
            default:
                break;
            }
        }

        break; /* TODO: only run adb cmd to the first lib handler */
    }


AURISYS_GET_PARAM_EXIT:
    if (ret != 0) {
        strncpy(local_get_parameter_return_buf, "GET_FAIL", MAX_PARAM_PATH_COPY_LEN);
    }
    AUD_LOG_V("%s(-) %s, ret = %d, local_get_parameter_return_buf = %s", __FUNCTION__,
              key, ret, local_get_parameter_return_buf);
    UNLOCK_ALOCK(g_controller->lock);
    return local_get_parameter_return_buf;
}


int get_retval_of_aurisys_set_rarameter(void) {
    int ret = 0;

    LOCK_ALOCK_MS(g_controller->lock, 1000);

    AUD_ASSERT(g_controller->retval_of_set_rarameter != -1);
    ret = g_controller->retval_of_set_rarameter;
    g_controller->retval_of_set_rarameter = -1;

    UNLOCK_ALOCK(g_controller->lock);

    AUD_LOG_V("%s(), %d", __func__, ret);
    return ret;
}


/*
 * =============================================================================
 *                     private function implementation
 * =============================================================================
 */

static int aurisys_set_parameter_to_component(
    aurisys_library_config_t *library_config,
    aurisys_component_t *component,
    aurisys_adb_command_t *adb_cmd) {
    int ret = 0;

    aurisys_lib_handler_t *itor_lib_handler = NULL;
    aurisys_lib_handler_t *tmp_lib_handler = NULL;


    switch (adb_cmd->adb_cmd_type) {
    /* for all same lib */
    case ADB_CMD_PARAM_FILE:
        strncpy(library_config->param_path, adb_cmd->param_path, MAX_PARAM_PATH_LEN);
        ret = 0;
        break;
    case ADB_CMD_LIB_DUMP_FILE:
        strncpy(library_config->lib_dump_path, adb_cmd->lib_dump_path, MAX_PARAM_PATH_LEN);
        ret = 0;
        break;
    /* for all same component */
    case ADB_CMD_ENABLE_LOG:
        component->enable_log = adb_cmd->enable_log;
        ret = 0;
        break;
    case ADB_CMD_ENABLE_RAW_DUMP:
        component->enable_raw_dump = adb_cmd->enable_raw_dump;
        ret = 0;
        break;
    case ADB_CMD_ENABLE_LIB_DUMP:
        component->enable_lib_dump = adb_cmd->enable_lib_dump;
        ret = 0;
        break;
    case ADB_CMD_APPLY_PARAM:
        component->enhancement_mode = adb_cmd->enhancement_mode;
        ret = 0;
        break;
    /* for single library hanlder */
    default:
        /* HASH_ITER all library hanlder later */
        ret = -1;
        break;
    }

    /* list all the lib handlers in the same component */
    HASH_ITER(hh_component, component->lib_handler_list_for_adb_cmd, itor_lib_handler, tmp_lib_handler) {
        AUD_LOG_V("HASH_ITER: %p", itor_lib_handler);
        ret += aurisys_arsi_run_adb_cmd(itor_lib_handler, adb_cmd);
    }

    return ret;
}


void set_aurisys_on(const bool aurisys_on) {
    AUD_LOG_D("%s(), %d => %d", __func__, g_controller->aurisys_on, aurisys_on);
    g_controller->aurisys_on = aurisys_on;
}


bool get_aurisys_on() {
    return g_controller->aurisys_on;
}



#ifdef __cplusplus
}  /* extern "C" */
#endif

