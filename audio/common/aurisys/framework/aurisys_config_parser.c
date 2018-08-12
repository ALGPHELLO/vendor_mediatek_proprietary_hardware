#include "aurisys_config_parser.h"

#include <tree.h> /* libxml */
#include <uthash.h> /* uthash */
#include <dlfcn.h> /* dlopen & dlsym */

#include <wrapped_audio.h>

#include <audio_log.h>
#include <audio_assert.h>
#include <audio_debug_tool.h>
#include <audio_memory_control.h>
#include <audio_sample_rate.h>

#include <aurisys_scenario.h>

#include <arsi_type.h>
#include <aurisys_config.h>

#include <arsi_api.h>

#include <aurisys_utility.h>



#ifdef __cplusplus
extern "C" {
#endif

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "aurisys_config_parser"


#ifdef AURISYS_DUMP_LOG_V
#undef  AUD_LOG_V
#define AUD_LOG_V(x...) AUD_LOG_D(x)
#endif


/*
 * =============================================================================
 *                     MACRO
 * =============================================================================
 */

#define XML_CHAR_CAST (xmlChar *)


#define AURISYS_CONFIG_PATH "/system/vendor/etc/aurisys_config.xml"

#define NODE_NAME_AURISYS_CONFIG "aurisys_config"
#define NODE_NAME_HAL_SCENARIOS "aurisys_scenarios"
#define NODE_NAME_HAL_LIBRARYS "hal_librarys"

#define NODE_NAME_LIBRARY "library"


#define MAX_PARAM_PATH_LEN      (256)
#define MAX_PARAM_PATH_COPY_LEN ((MAX_PARAM_PATH_LEN) - 1) /* -1: reserve for '\0' */


/*
 * =============================================================================
 *                     typedef
 * =============================================================================
 */

typedef void (*dynamic_link_arsi_assign_lib_fp_t)(AurisysLibInterface *api);



/*
 * =============================================================================
 *                     private global members
 * =============================================================================
 */

static bool init_flag = false;


/*
 * =============================================================================
 *                     private function declaration
 * =============================================================================
 */

static aurisys_scene_lib_table_t *parse_scenarios(xmlNodePtr node_scenarios);
static void dump_scene_lib_table(aurisys_scene_lib_table_t *scene_lib_table);
static int parse_xlink_libs(xmlNodePtr node_xlink_libs, aurisys_library_name_t **xlink_libraries);

static aurisys_library_config_t *parse_library_config(xmlNodePtr node_librarys);
static void dump_library_config(aurisys_library_config_t *library_config);
static int parse_components(xmlNodePtr node_components, aurisys_library_config_t *library_config);
static int parse_xlink_bufs(
    xmlNodePtr  node_xlink_bufs,
    uint32_t *audio_support_channel_number_mask,
    audio_buf_t **in,
    audio_buf_t **out,
    audio_buf_t **ref,
    uint8_t     *num_ref);



/*
 * =============================================================================
 *                     utilities declaration
 * =============================================================================
 */

static void add_library_config(aurisys_library_config_t **hash_head, aurisys_library_config_t *new_library_config);
static void add_arsi_component(aurisys_component_t **hash_head, aurisys_component_t *new_component);
static void add_scene_lib_table(aurisys_scene_lib_table_t **hash_head, aurisys_scene_lib_table_t *new_scene_lib_table);
static void add_library_name(aurisys_library_name_t **hash_head, aurisys_library_name_t *new_library_name);


static xmlNodePtr get_neighbor_node_by_name(xmlNodePtr node, const char *name);

static char *get_prop_string_by_prop(xmlNodePtr node, const char *prop);
static char *clone_string_by_prop(xmlNodePtr node, const char *prop);
static char *clone_string_by_prop_with_size(xmlNodePtr node, const char *prop, uint32_t size);



#define GET_PROP(node, prop) ((char *)xmlGetProp(node, XML_CHAR_CAST(prop)))
#define GET_INT_BY_PROP(int_type, node, prop) ((int_type)atol(get_prop_string_by_prop(node, prop)))


/*
 * =============================================================================
 *                     public function implementation
 * =============================================================================
 */

aurisys_config_t *parse_aurisys_config(void) {
    int retval = 0;

    xmlDocPtr doc = NULL;
    xmlNodePtr root = NULL;

    xmlNodePtr node_aurisys_config = NULL;
    xmlNodePtr node_hal_librarys = NULL;
    xmlNodePtr node_aurisys_scenarios = NULL;

    xmlNodePtr cur = NULL;

    if (init_flag == true) {
        AUD_LOG_E("already parsing done. return!");
        return NULL;
    }
    init_flag = true;

    /* init */
    aurisys_config_t *aurisys_config = NULL;


    /* read aurisys_config_new.xml */
    doc = xmlParseFile(AURISYS_CONFIG_PATH);
    if (doc == NULL) {
        AUD_LOG_E("xmlParseFile %s fail", AURISYS_CONFIG_PATH);
        return NULL;
    }

    root = xmlDocGetRootElement(doc);
    if (root == NULL) {
        AUD_LOG_E("xmlDocGetRootElement fail");
        retval = -1;
        goto PARSE_AURISYS_CONFIG_EXIT;
    }


    /* get node of <aurisys_config> */
    node_aurisys_config = get_neighbor_node_by_name(root, NODE_NAME_AURISYS_CONFIG);
    if (node_aurisys_config == NULL) {
        AUD_LOG_E("node_aurisys_config == NULL");
        retval = -1;
        goto PARSE_AURISYS_CONFIG_EXIT;
    }


    /* get node of <aurisys_scenarios> and parsing it */
    node_aurisys_scenarios = get_neighbor_node_by_name(node_aurisys_config->children, NODE_NAME_HAL_SCENARIOS);
    if (node_aurisys_scenarios == NULL) {
        AUD_LOG_E("node_aurisys_scenarios == NULL");
        retval = -1;
        goto PARSE_AURISYS_CONFIG_EXIT;
    }


    AUDIO_ALLOC_STRUCT(aurisys_config_t, aurisys_config);

    aurisys_config->scene_lib_table_hh = parse_scenarios(node_aurisys_scenarios);
    if (aurisys_config->scene_lib_table_hh == NULL) {
        AUD_LOG_E("parse_scenarios fail");
        goto PARSE_AURISYS_CONFIG_EXIT;
    }
    dump_scene_lib_table(aurisys_config->scene_lib_table_hh);


    /* get node of <hal_librarys> and parsing it */
    node_hal_librarys = get_neighbor_node_by_name(node_aurisys_config->children, NODE_NAME_HAL_LIBRARYS);
    if (node_hal_librarys == NULL) {
        AUD_LOG_E("node_hal_librarys == NULL");
        retval = -1;
        goto PARSE_AURISYS_CONFIG_EXIT;
    }

    aurisys_config->library_config_hh = parse_library_config(node_hal_librarys);
    if (aurisys_config->library_config_hh == NULL) {
        AUD_LOG_E("parse_library_config fail");
        goto PARSE_AURISYS_CONFIG_EXIT;
    }
    dump_library_config(aurisys_config->library_config_hh);



PARSE_AURISYS_CONFIG_EXIT:
    xmlFreeDoc(doc);
    doc = NULL;
    if (retval != 0) {
        AUD_LOG_E("%s() fail!!", __FUNCTION__);
        delete_aurisys_config(aurisys_config);
        aurisys_config = NULL;
    }

    return aurisys_config;
}


void delete_aurisys_config(aurisys_config_t *aurisys_config) {
    aurisys_library_config_t *itor_lib = NULL;
    aurisys_library_config_t *tmp_lib = NULL;

    aurisys_component_t *itor_comp = NULL;
    aurisys_component_t *tmp_comp = NULL;

    aurisys_scene_lib_table_t *itor_scene_lib_table = NULL;
    aurisys_scene_lib_table_t *tmp_scene_lib_table = NULL;

    aurisys_library_name_t *itor_library_name = NULL;
    aurisys_library_name_t *tmp_library_name = NULL;

    int i = 0;

    if (aurisys_config == NULL) {
        AUD_LOG_E("%s aurisys_config is NULL!! return", __FUNCTION__);
        return;
    }
    init_flag = false;


    /* delete library_config */
    HASH_ITER(hh, aurisys_config->library_config_hh, itor_lib, tmp_lib) {
        HASH_DEL(aurisys_config->library_config_hh, itor_lib);

        AUDIO_FREE_POINTER(itor_lib->name);
        AUDIO_FREE_POINTER(itor_lib->lib_path);
        AUDIO_FREE_POINTER(itor_lib->lib64_path);
        AUDIO_FREE_POINTER(itor_lib->param_path);
        AUDIO_FREE_POINTER(itor_lib->lib_dump_path);
        AUDIO_FREE_POINTER(itor_lib->adb_cmd_key);

        if (itor_lib->dlopen_handle != NULL) {
            dlclose(itor_lib->dlopen_handle);
            itor_lib->dlopen_handle = NULL;
        }

        AUDIO_FREE_POINTER(itor_lib->api);


        /* delete component */
        HASH_ITER(hh, itor_lib->component_hh, itor_comp, tmp_comp) {
            HASH_DEL(itor_lib->component_hh, itor_comp);

            /* UL */
            if (itor_comp->lib_config.p_ul_buf_in != NULL) {
                //AUDIO_FREE_POINTER(itor_comp->lib_config.p_ul_buf_in->data_buf.p_buffer);
                AUDIO_FREE_POINTER(itor_comp->lib_config.p_ul_buf_in);
            }

            if (itor_comp->lib_config.p_ul_buf_out != NULL) {
                //AUDIO_FREE_POINTER(itor_comp->lib_config.p_ul_buf_out->data_buf.p_buffer);
                AUDIO_FREE_POINTER(itor_comp->lib_config.p_ul_buf_out);
            }

            if (itor_comp->lib_config.p_ul_ref_bufs != NULL) {
                for (i = 0; i < itor_comp->lib_config.num_ul_ref_buf_array; i++) {
                    //AUDIO_FREE_POINTER(itor_comp->lib_config.p_ul_ref_bufs[i].data_buf.p_buffer);
                }
                AUDIO_FREE_POINTER(itor_comp->lib_config.p_ul_ref_bufs);
            }


            /* DL */
            if (itor_comp->lib_config.p_dl_buf_in != NULL) {
                //AUDIO_FREE_POINTER(itor_comp->lib_config.p_dl_buf_in->data_buf.p_buffer);
                AUDIO_FREE_POINTER(itor_comp->lib_config.p_dl_buf_in);
            }

            if (itor_comp->lib_config.p_dl_buf_out != NULL) {
                //AUDIO_FREE_POINTER(itor_comp->lib_config.p_dl_buf_out->data_buf.p_buffer);
                AUDIO_FREE_POINTER(itor_comp->lib_config.p_dl_buf_out);
            }

            if (itor_comp->lib_config.p_dl_ref_bufs != NULL) {
                for (i = 0; i < itor_comp->lib_config.num_dl_ref_buf_array; i++) {
                    //AUDIO_FREE_POINTER(itor_comp->lib_config.p_dl_ref_bufs[i].data_buf.p_buffer);
                }
                AUDIO_FREE_POINTER(itor_comp->lib_config.p_dl_ref_bufs);
            }

            AUDIO_FREE_POINTER(itor_comp);
        }

        AUDIO_FREE_POINTER(itor_lib);
    }


    /* delete scene_lib_table */
    HASH_ITER(hh, aurisys_config->scene_lib_table_hh, itor_scene_lib_table, tmp_scene_lib_table) {
        HASH_DEL(aurisys_config->scene_lib_table_hh, itor_scene_lib_table);

        /* delete uplink_library_name */
        HASH_ITER(hh, itor_scene_lib_table->uplink_library_name_list, itor_library_name, tmp_library_name) {
            HASH_DEL(itor_scene_lib_table->uplink_library_name_list, itor_library_name);
            AUDIO_FREE_POINTER(itor_library_name->name);
            AUDIO_FREE_POINTER(itor_library_name);
        }

        /* delete downlink_library_name */
        HASH_ITER(hh, itor_scene_lib_table->downlink_library_name_list, itor_library_name, tmp_library_name) {
            HASH_DEL(itor_scene_lib_table->downlink_library_name_list, itor_library_name);
            AUDIO_FREE_POINTER(itor_library_name->name);
            AUDIO_FREE_POINTER(itor_library_name);
        }

        AUDIO_FREE_POINTER(itor_scene_lib_table);
    }

    AUDIO_FREE_POINTER(aurisys_config);
}


/*
 * =============================================================================
 *                     private function declaration
 * =============================================================================
 */

static aurisys_scene_lib_table_t *parse_scenarios(xmlNodePtr node_aurisys_scenarios) {
    int retval = 0;

    xmlNodePtr node_aurisys_scenario;
    xmlNodePtr node_xlink_library_name_list;

    aurisys_scene_lib_table_t *scene_lib_table = NULL;
    aurisys_scene_lib_table_t *new_scene_lib_table = NULL;

    if (node_aurisys_scenarios == NULL) {
        AUD_LOG_E("%s node_aurisys_scenarios is NULL", __FUNCTION__);
        return NULL;
    }

    if (node_aurisys_scenarios->children == NULL) {
        AUD_LOG_E("%s node_aurisys_scenarios->children is NULL", __FUNCTION__);
        return NULL;
    }


    /* iteratively parsing <aurisys_scenario> */
    node_aurisys_scenario = get_neighbor_node_by_name(node_aurisys_scenarios->children, "aurisys_scenario");
    while (node_aurisys_scenario != NULL) {
        AUDIO_ALLOC_STRUCT(aurisys_scene_lib_table_t, new_scene_lib_table);
        new_scene_lib_table->aurisys_scenario =
            get_enum_by_string_aurisys_scenario(
                get_prop_string_by_prop(node_aurisys_scenario, "aurisys_scenario"));

        node_xlink_library_name_list = get_neighbor_node_by_name(node_aurisys_scenario->children, "uplink_library_name_list");
        if (node_xlink_library_name_list != NULL) {
            retval = parse_xlink_libs(node_xlink_library_name_list, &new_scene_lib_table->uplink_library_name_list);
            AUD_ASSERT(retval == 0);
            new_scene_lib_table->uplink_digital_gain_lib_name =
                clone_string_by_prop(node_xlink_library_name_list, "digital_gain_lib_name");
        }

        node_xlink_library_name_list = get_neighbor_node_by_name(node_aurisys_scenario->children, "downlink_library_name_list");
        if (node_xlink_library_name_list != NULL) {
            retval = parse_xlink_libs(node_xlink_library_name_list, &new_scene_lib_table->downlink_library_name_list);
            AUD_ASSERT(retval == 0);
            new_scene_lib_table->downlink_digital_gain_lib_name =
                clone_string_by_prop(node_xlink_library_name_list, "digital_gain_lib_name");
        }

        /* add lib */
        add_scene_lib_table(&scene_lib_table, new_scene_lib_table);


        /* iteratively parsing <scenario> */
        node_aurisys_scenario = get_neighbor_node_by_name(node_aurisys_scenario->next, "aurisys_scenario");
    }

    return scene_lib_table;
}


static void dump_scene_lib_table(aurisys_scene_lib_table_t *scene_lib_table) {
    aurisys_scene_lib_table_t *itor_scene_lib_table = NULL;
    aurisys_scene_lib_table_t *tmp_scene_lib_table = NULL;

    aurisys_library_name_t *itor_library_name = NULL;
    aurisys_library_name_t *tmp_library_name = NULL;


    AUD_LOG_D("%s", __FUNCTION__);

    HASH_ITER(hh, scene_lib_table, itor_scene_lib_table, tmp_scene_lib_table) {
        AUD_LOG_D("aurisys_scenario %d", itor_scene_lib_table->aurisys_scenario);

        if (itor_scene_lib_table->uplink_library_name_list) {
            HASH_ITER(hh, itor_scene_lib_table->uplink_library_name_list, itor_library_name, tmp_library_name) {
                AUD_LOG_D("uplink_library_name_list name %s", itor_library_name->name);
            }
            AUD_LOG_D("=> UL apply gain to %s", itor_scene_lib_table->uplink_digital_gain_lib_name);
        }
        if (itor_scene_lib_table->downlink_library_name_list) {
            HASH_ITER(hh, itor_scene_lib_table->downlink_library_name_list, itor_library_name, tmp_library_name) {
                AUD_LOG_D("downlink_library_name_list name %s", itor_library_name->name);
            }
            AUD_LOG_D("=> DL apply gain to %s", itor_scene_lib_table->downlink_digital_gain_lib_name);
        }
    }
}

static int parse_xlink_libs(xmlNodePtr node_xlink_libs, aurisys_library_name_t **xlink_libraries) {
    xmlNodePtr node_library;

    aurisys_library_name_t *new_library_name;

    if (node_xlink_libs == NULL) {
        AUD_LOG_E("%s node_xlink_libs is NULL", __FUNCTION__);
        return -1;
    }

    if (node_xlink_libs->children == NULL) {
        AUD_LOG_E("%s node_xlink_libs->children is NULL", __FUNCTION__);
        return -1;
    }

    /* iteratively parsing <library> */
    node_library = get_neighbor_node_by_name(node_xlink_libs->children, NODE_NAME_LIBRARY);
    while (node_library != NULL) {
        AUDIO_ALLOC_STRUCT(aurisys_library_name_t, new_library_name);

        new_library_name->name = clone_string_by_prop(node_library, "name");

        AUD_LOG_VV("library %s", new_library_name->name);

        add_library_name(xlink_libraries, new_library_name);

        /* iteratively parsing <library> */
        node_library = get_neighbor_node_by_name(node_library->next, NODE_NAME_LIBRARY);
    }

    return 0;
}


static aurisys_library_config_t *parse_library_config(xmlNodePtr node_librarys) {
    int retval = 0;

    xmlNodePtr node_library;
    xmlNodePtr node_components;

    char *dlopen_lib_path = NULL;


    aurisys_library_config_t *library_config = NULL;
    aurisys_library_config_t *new_library_config = NULL;

    dynamic_link_arsi_assign_lib_fp_t dynamic_link_arsi_assign_lib_fp = NULL;


    if (node_librarys == NULL) {
        AUD_LOG_E("%s node_librarys is NULL", __FUNCTION__);
        return NULL;
    }

    if (node_librarys->children == NULL) {
        AUD_LOG_E("%s node_librarys->children is NULL", __FUNCTION__);
        return NULL;
    }


    /* iteratively parsing <library> */
    node_library = get_neighbor_node_by_name(node_librarys->children, NODE_NAME_LIBRARY);
    while (node_library != NULL) {
        /* parsing library properties */
        AUDIO_ALLOC_STRUCT(aurisys_library_config_t, new_library_config);

        new_library_config->name          = clone_string_by_prop(node_library, "name");
        new_library_config->lib_path      = clone_string_by_prop(node_library, "lib_path");
        new_library_config->lib64_path    = clone_string_by_prop(node_library, "lib64_path");
        new_library_config->param_path    = clone_string_by_prop_with_size(
                                                node_library, "param_path",
                                                MAX_PARAM_PATH_LEN);
        new_library_config->lib_dump_path = clone_string_by_prop_with_size(
                                                node_library, "lib_dump_path",
                                                MAX_PARAM_PATH_LEN);
        new_library_config->adb_cmd_key   = clone_string_by_prop(node_library, "adb_cmd_key");

        new_library_config->component_hh = NULL;

        /* dlopen */
#if defined(__LP64__)
        dlopen_lib_path = new_library_config->lib64_path;
#else
        dlopen_lib_path = new_library_config->lib_path;
#endif
        new_library_config->dlopen_handle = dlopen(dlopen_lib_path, RTLD_NOW);
        if (new_library_config->dlopen_handle == NULL) {
            AUD_LOG_E("dlopen(%s) fail!!", dlopen_lib_path);
            AUD_ASSERT(new_library_config->dlopen_handle != NULL);
            AUDIO_FREE_POINTER(new_library_config);
            return NULL;
        }

        dynamic_link_arsi_assign_lib_fp = (dynamic_link_arsi_assign_lib_fp_t)dlsym(
                                              new_library_config->dlopen_handle,
                                              "dynamic_link_arsi_assign_lib_fp");
        if (dynamic_link_arsi_assign_lib_fp == NULL) {
            AUD_LOG_E("dlsym(%s) for %s fail!!", dlopen_lib_path, "dynamic_link_arsi_assign_lib_fp");
            AUD_ASSERT(dynamic_link_arsi_assign_lib_fp != NULL);
            AUDIO_FREE_POINTER(new_library_config);
            return NULL;
        }

        AUDIO_ALLOC_STRUCT(AurisysLibInterface, new_library_config->api);
        dynamic_link_arsi_assign_lib_fp(new_library_config->api);
        AUD_ASSERT(new_library_config->api->arsi_create_handler != NULL); /* TODO: check all api */


        /* parsing component for the library */
        node_components = get_neighbor_node_by_name(node_library->children, "components");
        AUD_ASSERT(node_components != NULL);

        retval = parse_components(node_components, new_library_config);
        AUD_ASSERT(retval == 0);

        /* add lib */
        add_library_config(&library_config, new_library_config);

        /* iteratively parsing <library> */
        node_library = get_neighbor_node_by_name(node_library->next, NODE_NAME_LIBRARY);
    }


    return library_config;
}


static void dump_library_config(aurisys_library_config_t *library_config) {
    aurisys_library_config_t *itor_lib = NULL;
    aurisys_library_config_t *tmp_lib = NULL;

    aurisys_component_t *itor_comp = NULL;
    aurisys_component_t *tmp_comp = NULL;

    uint8_t data_buf_type = 0;

    string_buf_t lib_version;

    int i = 0;


    AUD_LOG_D("%s", __FUNCTION__);

    HASH_ITER(hh, library_config, itor_lib, tmp_lib) {
        AUD_LOG_D("library name %s", itor_lib->name);
        AUD_LOG_V("library lib_path \"%s\"", itor_lib->lib_path);
        AUD_LOG_V("library lib64_path \"%s\"", itor_lib->lib64_path);
        AUD_LOG_V("library param_path \"%s\"", itor_lib->param_path);
        AUD_LOG_V("library lib_dump_path \"%s\"", itor_lib->lib_dump_path);
        AUD_LOG_V("library adb_cmd_key %s", itor_lib->adb_cmd_key);

        lib_version.memory_size = 128;
        lib_version.string_size = 0;
        AUDIO_ALLOC_CHAR_BUFFER(lib_version.p_string, lib_version.memory_size);
        if (itor_lib->api->arsi_get_lib_version != NULL) {
            itor_lib->api->arsi_get_lib_version(&lib_version);
            AUD_LOG_D("lib_version: \"%s\"", lib_version.p_string);
        } else {
            AUD_LOG_E("unknown lib_version");
        }
        AUDIO_FREE_POINTER(lib_version.p_string);


        HASH_ITER(hh, itor_lib->component_hh, itor_comp, tmp_comp) {
            AUD_LOG_D("aurisys_scenario %d", itor_comp->aurisys_scenario);
            AUD_LOG_V("sample_rate_mask 0x%x", itor_comp->sample_rate_mask);
            AUD_LOG_V("max sample_rate %d", itor_comp->lib_config.sample_rate);
            AUD_LOG_V("audio_format %d", itor_comp->lib_config.audio_format);
            AUD_LOG_V("support_format_mask 0x%x", itor_comp->support_format_mask);
            AUD_LOG_V("support_frame_ms_mask 0x%x", itor_comp->support_frame_ms_mask);
            AUD_LOG_V("frame_size_ms %d", itor_comp->lib_config.frame_size_ms);
            AUD_LOG_V("b_interleave %d", itor_comp->lib_config.b_interleave);
            AUD_LOG_V("enable_log %d", itor_comp->enable_log);
            AUD_LOG_V("enable_raw_dump %d", itor_comp->enable_raw_dump);
            AUD_LOG_V("enable_lib_dump %d", itor_comp->enable_lib_dump);
            AUD_LOG_V("enhancement_mode %d", itor_comp->enhancement_mode);
            AUD_LOG_V("num_ul_ref_buf_array %d", itor_comp->lib_config.num_ul_ref_buf_array);
            AUD_LOG_V("num_dl_ref_buf_array %d", itor_comp->lib_config.num_dl_ref_buf_array);


            /* UL */
            if (itor_comp->lib_config.p_ul_buf_in != NULL) {
                AUD_LOG_V("UL buf_in type %d", itor_comp->lib_config.p_ul_buf_in->data_buf_type);
                AUD_LOG_V("UL buf_in ch   %d", itor_comp->lib_config.p_ul_buf_in->num_channels);
                data_buf_type = itor_comp->lib_config.p_ul_buf_in->data_buf_type;
                if (data_buf_type >= DATA_BUF_UPLINK_IN &&
                    data_buf_type < NUM_DATA_BUF_TYPE) {
                    AUD_LOG_V("UL buf_in ch_mask 0x%x",
                              itor_comp->support_channel_number_mask[data_buf_type]);
                }
            }

            if (itor_comp->lib_config.p_ul_buf_out != NULL) {
                AUD_LOG_V("UL buf_out type %d", itor_comp->lib_config.p_ul_buf_out->data_buf_type);
                AUD_LOG_V("UL buf_out ch   %d", itor_comp->lib_config.p_ul_buf_out->num_channels);
                data_buf_type = itor_comp->lib_config.p_ul_buf_out->data_buf_type;
                if (data_buf_type >= DATA_BUF_UPLINK_IN &&
                    data_buf_type < NUM_DATA_BUF_TYPE) {
                    AUD_LOG_V("UL buf_out ch_mask 0x%x",
                              itor_comp->support_channel_number_mask[data_buf_type]);
                }
            }

            if (itor_comp->lib_config.p_ul_ref_bufs != NULL) {
                for (i = 0; i < itor_comp->lib_config.num_ul_ref_buf_array; i++) {
                    AUD_LOG_V("UL buf_ref[%d] type %d", i, itor_comp->lib_config.p_ul_ref_bufs[i].data_buf_type);
                    AUD_LOG_V("UL buf_ref[%d] ch   %d", i, itor_comp->lib_config.p_ul_ref_bufs[i].num_channels);
                    data_buf_type = itor_comp->lib_config.p_ul_ref_bufs[i].data_buf_type;
                    if (data_buf_type >= DATA_BUF_UPLINK_IN &&
                        data_buf_type < NUM_DATA_BUF_TYPE) {
                        AUD_LOG_V("UL buf_ref[%d] ch_mask 0x%x",
                                  i, itor_comp->support_channel_number_mask[data_buf_type]);
                    }
                }
            }

            /* DL */
            if (itor_comp->lib_config.p_dl_buf_in != NULL) {
                AUD_LOG_V("DL buf_in type %d", itor_comp->lib_config.p_dl_buf_in->data_buf_type);
                AUD_LOG_V("DL buf_in ch   %d", itor_comp->lib_config.p_dl_buf_in->num_channels);
                data_buf_type = itor_comp->lib_config.p_dl_buf_in->data_buf_type;
                if (data_buf_type >= DATA_BUF_UPLINK_IN &&
                    data_buf_type < NUM_DATA_BUF_TYPE) {
                    AUD_LOG_V("DL buf_in ch_mask 0x%x",
                              itor_comp->support_channel_number_mask[data_buf_type]);
                }
            }

            if (itor_comp->lib_config.p_dl_buf_out != NULL) {
                AUD_LOG_V("DL buf_out type %d", itor_comp->lib_config.p_dl_buf_out->data_buf_type);
                AUD_LOG_V("DL buf_out ch   %d", itor_comp->lib_config.p_dl_buf_out->num_channels);
                data_buf_type = itor_comp->lib_config.p_dl_buf_out->data_buf_type;
                if (data_buf_type >= DATA_BUF_UPLINK_IN &&
                    data_buf_type < NUM_DATA_BUF_TYPE) {
                    AUD_LOG_V("DL buf_out ch_mask 0x%x",
                              itor_comp->support_channel_number_mask[data_buf_type]);
                }
            }

            if (itor_comp->lib_config.p_dl_ref_bufs != NULL) {
                for (i = 0; i < itor_comp->lib_config.num_dl_ref_buf_array; i++) {
                    AUD_LOG_V("DL buf_ref[%d] type %d", i, itor_comp->lib_config.p_dl_ref_bufs[i].data_buf_type);
                    AUD_LOG_V("DL buf_ref[%d] ch   %d", i, itor_comp->lib_config.p_dl_ref_bufs[i].num_channels);
                    data_buf_type = itor_comp->lib_config.p_dl_ref_bufs[i].data_buf_type;
                    if (data_buf_type >= DATA_BUF_UPLINK_IN &&
                        data_buf_type < NUM_DATA_BUF_TYPE) {
                        AUD_LOG_V("DL buf_ref[%d] ch_mask 0x%x",
                                  i, itor_comp->support_channel_number_mask[data_buf_type]);
                    }
                }
            }
        }
    }
}


static int parse_components(xmlNodePtr node_components, aurisys_library_config_t *library_config) {
    int retval = 0;

    xmlNodePtr node_component;
    xmlNodePtr node_uplink_process;
    xmlNodePtr node_downlink_process;

    aurisys_component_t *new_component = NULL;
    arsi_lib_config_t *lib_config = NULL;


    if (node_components == NULL) {
        AUD_LOG_E("%s node_components is NULL", __FUNCTION__);
        return -1;
    }

    if (node_components->children == NULL) {
        AUD_LOG_E("%s node_hal_librarys->children is NULL", __FUNCTION__);
        return -1;
    }


    /* iteratively parsing <component> */
    node_component = get_neighbor_node_by_name(node_components->children, "component");
    while (node_component != NULL) {
        /* parsing component properties */
        AUDIO_ALLOC_STRUCT(aurisys_component_t, new_component);
        lib_config = &new_component->lib_config;

        new_component->aurisys_scenario =
            get_enum_by_string_aurisys_scenario(
                get_prop_string_by_prop(node_component, "aurisys_scenario"));

        new_component->sample_rate_mask = audio_sample_rate_string_to_masks(
                                              get_prop_string_by_prop(node_component,
                                                                      "sample_rate"));

        lib_config->sample_rate = audio_sample_rate_get_max_rate(
                                      new_component->sample_rate_mask);

        new_component->support_format_mask =
            get_support_format_mask(
                get_prop_string_by_prop(node_component, "audio_format"));

        lib_config->audio_format = get_format_from_mask(
                                       new_component->support_format_mask);

        new_component->support_frame_ms_mask =
            get_support_frame_ms_mask(
                get_prop_string_by_prop(node_component, "frame_size_ms"));

        lib_config->frame_size_ms = get_frame_ms_from_mask(
                                        new_component->support_frame_ms_mask);


        lib_config->b_interleave = GET_INT_BY_PROP(uint8_t,
                                                   node_component,
                                                   "b_interleave");

        new_component->enable_log = GET_INT_BY_PROP(bool,
                                                    node_component,
                                                    "enable_log");

        new_component->enable_raw_dump = GET_INT_BY_PROP(bool,
                                                         node_component,
                                                         "enable_raw_dump");

        new_component->enable_lib_dump = GET_INT_BY_PROP(bool,
                                                         node_component,
                                                         "enable_lib_dump");

        new_component->enhancement_mode = GET_INT_BY_PROP(int,
                                                          node_component,
                                                          "enhancement_mode");

        AUD_LOG_VV("aurisys_scenario %d", new_component->aurisys_scenario);
        AUD_LOG_VV("sample_rate_mask 0x%x", new_component->sample_rate_mask);
        AUD_LOG_VV("max sample_rate %d", new_component->lib_config.sample_rate);
        AUD_LOG_VV("audio_format %d", new_component->lib_config.audio_format);
        AUD_LOG_VV("frame_size_ms %d", new_component->lib_config.frame_size_ms);
        AUD_LOG_VV("b_interleave %d", new_component->lib_config.b_interleave);
        AUD_LOG_VV("enable_log %d", new_component->enable_log);
        AUD_LOG_VV("enable_raw_dump %d", new_component->enable_raw_dump);
        AUD_LOG_VV("enable_lib_dump %d", new_component->enable_lib_dump);
        AUD_LOG_VV("enhancement_mode %d", new_component->enhancement_mode);


        /* parsing uplink_library_name_list for the component */
        node_uplink_process = get_neighbor_node_by_name(node_component->children, "uplink_process");
        if (node_uplink_process != NULL) {
            retval = parse_xlink_bufs(node_uplink_process,
                                      new_component->support_channel_number_mask,
                                      &lib_config->p_ul_buf_in,
                                      &lib_config->p_ul_buf_out,
                                      &lib_config->p_ul_ref_bufs,
                                      &lib_config->num_ul_ref_buf_array);
            if (retval != 0) {
                AUD_ASSERT(retval == 0);
                AUDIO_FREE_POINTER(new_component);
                break;
            }
        }

        /* parsing downlink_library_name_list for the component */
        node_downlink_process = get_neighbor_node_by_name(node_component->children, "downlink_process");
        if (node_downlink_process != NULL) {
            retval = parse_xlink_bufs(node_downlink_process,
                                      new_component->support_channel_number_mask,
                                      &lib_config->p_dl_buf_in,
                                      &lib_config->p_dl_buf_out,
                                      &lib_config->p_dl_ref_bufs,
                                      &lib_config->num_dl_ref_buf_array);
            if (retval != 0) {
                AUD_ASSERT(retval == 0);
                AUDIO_FREE_POINTER(new_component);
                break;
            }
        }


        /* add component */
        add_arsi_component(&library_config->component_hh, new_component);

        /* iteratively parsing <component> */
        node_component = get_neighbor_node_by_name(node_component->next, "component");
    }

    return retval;
}


static int parse_xlink_bufs(
    xmlNodePtr  node_xlink_bufs,
    uint32_t *audio_support_channel_number_mask,
    audio_buf_t **in,
    audio_buf_t **out,
    audio_buf_t **ref,
    uint8_t     *num_ref) {
    int retval = 0;

    xmlNodePtr node_buf;
    xmlNodePtr node_buf_ref;

    audio_buf_t *buf_in = NULL;
    audio_buf_t *buf_out = NULL;
    audio_buf_t *buf_refs = NULL;

    int i = 0;


    if (node_xlink_bufs == NULL) {
        AUD_LOG_E("%s node_xlink_bufs is NULL", __FUNCTION__);
        return -1;
    }

    if (node_xlink_bufs->children == NULL) {
        AUD_LOG_E("%s node_xlink_bufs->children is NULL", __FUNCTION__);
        return -1;
    }


    /* buf in */
    node_buf = get_neighbor_node_by_name(node_xlink_bufs->children, "buf_in");
    AUD_ASSERT(node_buf != NULL);

    AUDIO_ALLOC_STRUCT(audio_buf_t, buf_in);

    buf_in->data_buf_type =
        get_enum_by_string_data_buf_type(
            get_prop_string_by_prop(node_buf, "data_buf_type"));

    if (buf_in->data_buf_type >= DATA_BUF_UPLINK_IN &&
        buf_in->data_buf_type < NUM_DATA_BUF_TYPE) {
        audio_support_channel_number_mask[buf_in->data_buf_type] =
            get_support_channel_number_mask(
                get_prop_string_by_prop(node_buf, "num_channels"));

        buf_in->num_channels =
            get_channel_number_from_mask(
                audio_support_channel_number_mask[buf_in->data_buf_type]);
    }

    AUD_LOG_VV("buf_in type %d", buf_in->data_buf_type);
    AUD_LOG_VV("buf_in ch   %d", buf_in->num_channels);

    *in = buf_in;


    /* buf out */
    node_buf = get_neighbor_node_by_name(node_xlink_bufs->children, "buf_out");
    AUD_ASSERT(node_buf != NULL);

    AUDIO_ALLOC_STRUCT(audio_buf_t, buf_out);

    buf_out->data_buf_type =
        get_enum_by_string_data_buf_type(
            get_prop_string_by_prop(node_buf, "data_buf_type"));

    if (buf_out->data_buf_type >= DATA_BUF_UPLINK_IN &&
        buf_out->data_buf_type < NUM_DATA_BUF_TYPE) {
        audio_support_channel_number_mask[buf_out->data_buf_type] =
            get_support_channel_number_mask(
                get_prop_string_by_prop(node_buf, "num_channels"));

        buf_out->num_channels =
            get_channel_number_from_mask(
                audio_support_channel_number_mask[buf_out->data_buf_type]);
    }

    AUD_LOG_VV("buf_out type %d", buf_out->data_buf_type);
    AUD_LOG_VV("buf_out ch   %d", buf_out->num_channels);

    *out = buf_out;


    /* buf ref */
    node_buf = get_neighbor_node_by_name(node_xlink_bufs->children, "buf_refs");
    if (node_buf) {
        /* get count */
        node_buf_ref = get_neighbor_node_by_name(node_buf->children, "buf_ref");
        while (node_buf_ref) {
            (*num_ref)++;

            /* iteratively parsing <buf_ref> */
            node_buf_ref = get_neighbor_node_by_name(node_buf_ref->next, "buf_ref");
        }

        AUDIO_ALLOC_STRUCT_ARRAY(audio_buf_t, (*num_ref), buf_refs);

        /* iteratively parsing <buf_ref> */
        node_buf_ref = get_neighbor_node_by_name(node_buf->children, "buf_ref");
        for (i = 0; i < (*num_ref); i++) {
            if (node_buf_ref == NULL) {
                break;
            }

            buf_refs[i].data_buf_type =
                get_enum_by_string_data_buf_type(
                    get_prop_string_by_prop(node_buf_ref, "data_buf_type"));

            if (buf_refs[i].data_buf_type >= DATA_BUF_UPLINK_IN &&
                buf_refs[i].data_buf_type < NUM_DATA_BUF_TYPE) {
                audio_support_channel_number_mask[buf_refs[i].data_buf_type] =
                    get_support_channel_number_mask(
                        get_prop_string_by_prop(node_buf_ref, "num_channels"));

                buf_refs[i].num_channels =
                    get_channel_number_from_mask(
                        audio_support_channel_number_mask[buf_refs[i].data_buf_type]);
            }
            AUD_LOG_VV("buf_ref[%d] type %d", i, buf_refs[i].data_buf_type);
            AUD_LOG_VV("buf_ref[%d] ch   %d", i, buf_refs[i].num_channels);

            /* iteratively parsing <buf_ref> */
            node_buf_ref = get_neighbor_node_by_name(node_buf_ref->next, "buf_ref");
        }

        *ref = buf_refs;
    }

    return retval;
}



/*
 * =============================================================================
 *                     utilities implementation
 * =============================================================================
 */
static void add_library_config(aurisys_library_config_t **hash_head, aurisys_library_config_t *new_library_config) {
    HASH_ADD_KEYPTR(
        hh,
        *hash_head,
        new_library_config->name,
        strlen(new_library_config->name),
        new_library_config);
}


static void add_arsi_component(aurisys_component_t **hash_head, aurisys_component_t *new_component) {
    HASH_ADD_INT(
        *hash_head,
        aurisys_scenario,
        new_component);
}

static void add_scene_lib_table(aurisys_scene_lib_table_t **hash_head, aurisys_scene_lib_table_t *new_scene_lib_table) {
    HASH_ADD_INT(
        *hash_head,
        aurisys_scenario,
        new_scene_lib_table);
}

static void add_library_name(aurisys_library_name_t **hash_head, aurisys_library_name_t *new_library_name) {
    HASH_ADD_KEYPTR(
        hh,
        *hash_head,
        new_library_name->name,
        strlen(new_library_name->name),
        new_library_name);
}


static xmlNodePtr get_neighbor_node_by_name(xmlNodePtr node, const char *name) {
    xmlNode *iterator = NULL;

    if (node == NULL) {
        AUD_LOG_E("%s node is NULL", __FUNCTION__);
        return NULL;
    }

    if (name == NULL) {
        AUD_LOG_E("%s name is NULL", __FUNCTION__);
        return NULL;
    }

    for (iterator = node; iterator != NULL; iterator = iterator->next) {
        if (iterator->type != XML_ELEMENT_NODE) {
            continue;
        }

        if (xmlStrcmp(iterator->name, XML_CHAR_CAST(name)) == 0) {
            break;
        }
    }

    return iterator;
}


static char *get_prop_string_by_prop(xmlNodePtr node, const char *prop) {
    char *prop_string = NULL;

    prop_string = GET_PROP(node, prop);
    if (prop_string == NULL) {
        AUD_LOG_E("prop \"%s\" not found in \"%s\"", prop, AURISYS_CONFIG_PATH);
    }
    AUD_ASSERT(prop_string != NULL);

    return prop_string;
}


static char *clone_string_by_prop(xmlNodePtr node, const char *prop) {
    char *prop_string = NULL;

    size_t clone_string_size = 0;
    char *clone_string = NULL;

    prop_string = get_prop_string_by_prop(node, prop);
    clone_string_size = strlen(prop_string) + 1;

    if (clone_string_size > 0) {
        AUDIO_ALLOC_CHAR_BUFFER(clone_string, clone_string_size);
        strncpy(clone_string, prop_string, clone_string_size - 1);
    }

    return clone_string;
}


static char *clone_string_by_prop_with_size(xmlNodePtr node, const char *prop, uint32_t size) {
    char *prop_string = NULL;

    size_t clone_string_size = 0;
    char *clone_string = NULL;

    prop_string = get_prop_string_by_prop(node, prop);
    clone_string_size = strlen(prop_string) + 1;
    AUD_ASSERT(size >= clone_string_size);

    if (size > 0) {
        AUDIO_ALLOC_CHAR_BUFFER(clone_string, size);
        strncpy(clone_string, prop_string, size - 1);
    }

    return clone_string;
}




#ifdef __cplusplus
}  /* extern "C" */
#endif

