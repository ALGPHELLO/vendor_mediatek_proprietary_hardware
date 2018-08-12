/*
 * Copyright (C) 2013-2016, Shenzhen Huiding Technology Co., Ltd.
 * All Rights Reserved.
 */

#ifndef __GF_USER_TYPE_DEFINE_H__
#define __GF_USER_TYPE_DEFINE_H__

#include <stdint.h>
#include "gf_type_define.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TEST_SHARE_MEMORY_CHECK_LEN 1024

typedef enum {
    GF_USER_CMD_TEST_SHARE_MEMORY_PERFORMANCE = 100,
    GF_USER_CMD_GET_LAST_IDENTIFY_ID,
    GF_USER_CMD_GET_AUTHENTICATOR_VERSION,
    GF_USER_CMD_ENUMERATE,
    GF_USER_CMD_TEST_SHARE_MEMORY_CHECK,
} gf_user_cmd_id_t;

typedef struct {
    uint32_t version;
} gf_user_authenticator_version_t;

typedef struct {
    uint32_t last_identify_id;
} gf_user_last_identify_id_t;

typedef struct {
    uint32_t size;
    uint32_t group_ids[MAX_FINGERS_PER_USER];
    uint32_t finger_ids[MAX_FINGERS_PER_USER];
} gf_user_enumerate_t;

#ifdef __cplusplus
}
#endif

#endif // __GF_COMMON_H__
