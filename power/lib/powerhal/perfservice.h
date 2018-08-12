/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein is
 * confidential and proprietary to MediaTek Inc. and/or its licensors. Without
 * the prior written permission of MediaTek inc. and/or its licensors, any
 * reproduction, modification, use or disclosure of MediaTek Software, and
 * information contained herein, in whole or in part, shall be strictly
 * prohibited.
 *
 * MediaTek Inc. (C) 2010. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER
 * ON AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL
 * WARRANTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR
 * NONINFRINGEMENT. NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH
 * RESPECT TO THE SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY,
 * INCORPORATED IN, OR SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES
 * TO LOOK ONLY TO SUCH THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO.
 * RECEIVER EXPRESSLY ACKNOWLEDGES THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO
 * OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES CONTAINED IN MEDIATEK
 * SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE
 * RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S
 * ENTIRE AND CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE
 * RELEASED HEREUNDER WILL BE, AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE
 * MEDIATEK SOFTWARE AT ISSUE, OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE
 * CHARGE PAID BY RECEIVER TO MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek
 * Software") have been modified by MediaTek Inc. All revisions are subject to
 * any receiver's applicable license agreements with MediaTek Inc.
 */

#ifndef ANDROID_PERFSERVICE_H
#define ANDROID_PERFSERVICE_H

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cutils/compiler.h>
#include <utils/threads.h>
#include "perfservice_types.h"

#define PROJ_ALL "all"

#define PROC_NAME_REINIT "re-initialized"

using namespace android;

enum {
    CHECK_NONE  = 0,
    CHECK_PASS  = 1,
    CHECK_FAIL  = 2,
    CHECK_RETRY = 3,
};

enum {
    SETFUNC_RT_FIFO   = 0,
    SETFUNC_RT_RR     = 1,
    SETFUNC_ROOTGROUP = 2,
    SETFUNC_NICE      = 3,
    SETFUNC_NUM       = 4,
};

enum {
    INDEX_SYSTEM_APK     = 0,
    INDEX_USER_SPECIFIED = 1,
    INDEX_GAME_MODE      = 2,
    INDEX_ARM_NATIVE_LIB = 3,
    INDEX_HPS            = 4,
    INDEX_VCORE          = 5,
    INDEX_TLP            = 6,
    INDEX_NUM            = 7,
    // others
    INDEX_APP_RUNNING = 8,
    INDEX_NATIVE_RUNNING = 9,
};

enum {
    APK_BENCHMARK     = 0,
    APK_GAME          = 1,
    APK_NOT_BENCHMARK = 2,
    APK_OTHERS        = 3,
};

struct predefined_scn {
    const char *proj;
    int scenarios;
    //int cpu_cores;
    //int cpu_cores_big;
    //int cpu_freq;
    //int cpu_freq_big;
    int screen_off_action; // screen off still enabled
    int scn_valid;         // 0 => invalid
};

struct predefined_app {
    int cmd;
    const char *packName;
    const char *actName;
    int param1;
    int param2;
    int param3;
    int param4;
};

struct xml_activity {
    char cmd[128];
    char actName[128];
    char packName[128];
    int param1;
    int param2;
    int param3;
    int param4;
};

struct xml_gift {
    char packName[128];
    char **AttrName;
    char **AttrValue;
};

int boost_process_by_name(char* proc_name, char* thread_name, int setfuncid, int params);
int boost_process_by_tid(int tid, int full, int setfuncid, int params);
int check_name_by_tid (int tid, const char* proc_name);
void switchNormalAndSportMode(int mode);


/* smart detect */
int smart_init(void);
int smart_table_init(void);
int smart_table_init_flag(void);
int smart_control(int enable);
int smart_reset(const char *pack, int pid);
int smart_check_pack_existed(int type, const char *pack, int uid, int fromUid);
int smart_set_benchmark_mode(int benchmark);
int smart_add_pack(const char *pack);
int smart_is_det_enable(void);
int smart_get_tlp(int pid);
int smart_dump_info(void);
int smart_update_score(int index, int set);
int smart_add_benchmark(void);
int smart_add_game(void);
int smart_update_pack(void);

int loadConTable(const char *file_name);
#endif // ANDROID_PERFSERVICE_H
