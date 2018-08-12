/*
 * Copyright (C) 2013-2016, Shenzhen Huiding Technology Co., Ltd.
 * All Rights Reserved.
 */
#ifndef __GF_HAL_COMMON_H__
#define __GF_HAL_COMMON_H__

#include <signal.h>
#include "gf_common.h"
#include "gf_error.h"
#include "gf_fingerprint.h"
#include "gf_type_define.h"

#ifdef __cplusplus
extern "C" {
#endif

enum gf_spi_speed {
    GF_SPI_LOW_SPEED = 0,
    GF_SPI_HIGH_SPEED = 1,
};

typedef struct gf_hal_function {
    gf_error_t (*init)(void *dev);
    gf_error_t (*close)(void *dev);
    gf_error_t (*cancel)(void *dev);
    gf_error_t (*test_cancel)(void *dev);
    gf_error_t (*test_prior_cancel)(void *dev);

    uint64_t (*pre_enroll)(void *dev);
    gf_error_t (*enroll)(void *dev, const void *hat, uint32_t group_id, uint32_t timeout_sec);
    gf_error_t (*post_enroll)(void *dev);
    gf_error_t (*authenticate)(void *dev, uint64_t operation_id, uint32_t group_id);
    uint64_t (*get_auth_id)(void *dev);
    gf_error_t (*remove)(void *dev, uint32_t group_id, uint32_t finger_id);
    gf_error_t (*set_active_group)(void *dev, uint32_t group_id);

    gf_error_t (*enumerate)(void *dev, void *results, uint32_t *max_size);
    gf_error_t (*enumerate_with_callback)(void *dev);
    gf_error_t (*irq)();
    gf_error_t (*screen_on)();
    gf_error_t (*screen_off)();

    gf_error_t (*set_safe_class)(void *dev, gf_safe_class_t safe_class);
    gf_error_t (*navigate)(void *dev, gf_nav_mode_t nav_mode);

    gf_error_t (*enable_fingerprint_module)(void *dev, uint8_t enable_flag);
    gf_error_t (*camera_capture)(void *dev);
    gf_error_t (*enable_ff_feature)(void *dev, uint8_t enable_flag);

    gf_error_t (*enable_bio_assay_feature)(void *dev, uint8_t enable_flag);
    gf_error_t (*start_hbd)(void* dev);
    gf_error_t (*reset_lockout)(void *dev, const void *hat);

    gf_error_t (*sync_finger_list)(void* dev, uint32_t* list, int32_t count);

    gf_error_t (*invoke_command)(uint32_t operation_id, gf_cmd_id_t cmd_id, void *buffer, int len);
    gf_error_t (*user_invoke_command)(uint32_t cmd_id, void *buffer, int len);
    gf_error_t (*dump_invoke_command)(uint32_t cmd_id, void *buffer, int len);

    gf_error_t (*authenticate_fido)(void *dev, uint32_t group_id, uint8_t *aaid, uint32_t aaid_len,
            uint8_t *challenge, uint32_t challenge_len);
    gf_error_t (*is_id_valid)(void *dev, uint32_t group_id, uint32_t finger_id);
    int32_t (*get_id_list)(void *dev, uint32_t group_id, uint32_t *list, int32_t *count);
} gf_hal_function_t;

gf_error_t gf_hal_function_init(gf_hal_function_t *hal_function, gf_chip_series_t chip_series);

void gf_hal_dump_performance(const char *func_name, gf_operation_type_t operation,
        gf_test_performance_t *dump_performance);

void gf_hal_notify_acquired_info(gf_fingerprint_acquired_info_t acquired_info);
void gf_hal_notify_error_info(gf_fingerprint_error_t err_code);
void gf_hal_notify_enrollment_progress(uint32_t group_id, uint32_t finger_id,
        uint32_t samples_remaining);
void gf_hal_notify_authentication_succeeded(uint32_t group_id, uint32_t finger_id,
        gf_hw_auth_token_t *auth_token);
void gf_hal_notify_authentication_failed();
void gf_hal_notify_authentication_fido_failed();
void gf_hal_notify_remove_succeeded(uint32_t group_id, uint32_t finger_id, uint32_t remaining_templates);
gf_error_t gf_hal_save(uint32_t group_id, uint32_t finger_id);
gf_error_t gf_hal_update_stitch(uint32_t group_id, uint32_t finger_id);

gf_error_t gf_hal_reinit();
gf_error_t gf_hal_invoke_command(gf_cmd_id_t cmd_id, void *buffer, int len);
gf_error_t gf_hal_invoke_command_ex(gf_cmd_id_t cmd_id);

void gf_hal_nav_code_convert(gf_nav_code_t nav_code, gf_nav_code_t *converted_nav_code);
gf_error_t gf_hal_download_fw();
gf_error_t gf_hal_download_cfg();
void gf_hal_create_and_set_esd_timer();
void gf_hal_long_pressed_timer_thread(union sigval v);
void gf_hal_nav_listener(gf_nav_code_t nav_code);
void gf_hal_nav_reset();
void gf_hal_nav_double_click_timer_thread(union sigval v);
void gf_hal_nav_long_press_timer_thread(union sigval v);
gf_error_t gf_hal_nav_complete(void);
void gf_hal_nav_assert_config_interval();
int64_t gf_hal_current_time_microsecond(void);
gf_error_t gf_hal_init_finished();

gf_error_t gf_hal_common_close(void *dev);
gf_error_t gf_hal_common_cancel(void *dev);
uint64_t gf_hal_common_pre_enroll(void *dev);
gf_error_t gf_hal_common_enroll(void *dev, const void *hat, uint32_t group_id,
        uint32_t timeout_sec);
gf_error_t gf_hal_common_post_enroll(void *dev);
gf_error_t gf_hal_common_authenticate(void *dev, uint64_t operation_id, uint32_t group_id);
uint64_t gf_hal_common_get_auth_id(void *dev);
gf_error_t gf_hal_common_remove(void *dev, uint32_t group_id, uint32_t finger_id);
gf_error_t gf_hal_common_set_active_group(void *dev, uint32_t group_id);
gf_error_t gf_hal_common_enumerate(void *dev, void *results, uint32_t *max_size);
gf_error_t gf_hal_common_enumerate_with_callback(void *dev);
gf_error_t gf_hal_common_screen_on();
gf_error_t gf_hal_common_screen_off();
gf_error_t gf_hal_common_set_safe_class(void *dev, gf_safe_class_t safe_class);
gf_error_t gf_hal_common_navigate(void *dev, gf_nav_mode_t nav_mode);
gf_error_t gf_hal_common_enable_fingerprint_module(void *dev, uint8_t enable_flag);
gf_error_t gf_hal_common_camera_capture(void *dev);
gf_error_t gf_hal_common_enable_ff_feature(void *dev, uint8_t enable_flag);
gf_error_t gf_hal_common_reset_lockout(void *dev, const void *hat);
gf_error_t gf_hal_common_sync_finger_list(void* dev, uint32_t* list, int32_t count);
gf_error_t gf_hal_common_user_invoke_command(uint32_t cmd_id, void *buffer, int len);
gf_error_t gf_hal_common_authenticate_fido(void *dev, uint32_t group_id, uint8_t *aaid,
        uint32_t aaid_len, uint8_t *challenge, uint32_t challenge_len);
gf_error_t gf_hal_common_is_id_valid(void *dev, uint32_t group_id, uint32_t finger_id);
gf_error_t gf_hal_common_get_id_list(void *dev, uint32_t group_id, uint32_t *list, int32_t *count);

/**
 * This API always return SUCCESS, *otp_buf_len > 0, means that successfully load backup OTP info
 */
gf_error_t gf_hal_common_load_otp_info_from_sdcard(uint8_t *otp_buf, uint32_t *otp_buf_len);
gf_error_t gf_hal_common_save_otp_info_into_sdcard(uint8_t *otp_buf, uint32_t otp_buf_len);


/**
  * Test relative method below
  */
gf_error_t gf_hal_test_invoke_command(gf_cmd_id_t cmd_id, void *buffer, int len);
gf_error_t gf_hal_test_invoke_command_ex(gf_cmd_id_t cmd_id);

void gf_hal_notify_test_acquired_info(gf_fingerprint_acquired_info_t acquired_info);
void gf_hal_notify_test_error_info(gf_fingerprint_error_t err_code);
void gf_hal_notify_test_enrollment_progress(uint32_t group_id, uint32_t finger_id,
        uint32_t samples_remaining);
void gf_hal_notify_test_authentication_succeeded(uint32_t group_id, uint32_t finger_id,
        gf_hw_auth_token_t *auth_token);
void gf_hal_notify_test_authentication_failed();
void gf_hal_notify_test_remove_succeeded(uint32_t group_id, uint32_t finger_id);

/**
  * Dump relative method below
  */
void gf_hal_dump_data(gf_operation_type_t operation, gf_error_t error_code);
gf_error_t gf_hal_common_dump_invoke_command(uint32_t cmd_id, void *buffer, int len);

#ifdef __cplusplus
}
#endif

#endif  // __GF_HAL_COMMON_H__
