#ifndef MTK_AURISYS_CONTROLLER_H
#define MTK_AURISYS_CONTROLLER_H

#include <stdbool.h>

#include <aurisys_scenario.h>


#ifdef __cplusplus
extern "C" {
#endif



/*
 * =============================================================================
 *                     ref struct
 * =============================================================================
 */

struct aurisys_lib_manager_t;
struct aurisys_user_prefer_configs_t;


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

int init_aurisys_controller(void);
int deinit_aurisys_controller(void);

struct aurisys_lib_manager_t *create_aurisys_lib_manager(
    const aurisys_scenario_t aurisys_scenario,
    const struct aurisys_user_prefer_configs_t *p_prefer_configs);

int destroy_aurisys_lib_manager(struct aurisys_lib_manager_t *manager);

int aurisys_set_parameter(const char *key_value_pair);
char *aurisys_get_parameter(const char *key);
int get_retval_of_aurisys_set_rarameter(void);


void set_aurisys_on(const bool enable);
bool get_aurisys_on();



#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* end of MTK_AURISYS_CONTROLLER_H */

