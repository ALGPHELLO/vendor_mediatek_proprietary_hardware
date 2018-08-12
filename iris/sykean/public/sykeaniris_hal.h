/*
 * Copyright (c) 2017 SYKEAN Limited.
 *
 * All rights are reserved.
 * Proprietary and confidential.
 * Unauthorized copying of this file, via any medium is strictly prohibited.
 * Any use is subject to an appropriate license granted by SYKEAN Company.
 */

#ifndef __IRIS_HAL_H__
#define __IRIS_HAL_H__

#include "sykeaniris_error.h"
#include "sykeaniris_cam.h"
#include "sykeaniris_led.h"


#ifdef __cplusplus
extern "C" {
#endif

sykean_iris_error_t iris_hal_init(void *pDev, int32_t maxFailedAttempts);
sykean_iris_error_t iris_hal_exit(void *pDev);
sykean_iris_error_t iris_hal_cancel(void *pDev);

uint64_t iris_hal_pre_enroll(void *pDev);
sykean_iris_error_t iris_hal_enroll(void *pDev, const void *pHat, uint32_t groupId, uint32_t timeoutSec);
sykean_iris_error_t iris_hal_post_enroll(void *pDev);
sykean_iris_error_t iris_hal_authenticate(void *pDev, uint64_t operationId, uint32_t groupId);
uint64_t iris_hal_get_auth_id(void *pDev);
sykean_iris_error_t iris_hal_capture(void *pDev, const uint8_t *pPidData, uint32_t pidSize,
        int32_t pidType, int32_t bioType, const uint8_t *pCertChain, uint32_t certSize, uint32_t groupId);
sykean_iris_error_t iris_hal_remove(void *pDev, uint32_t groupId, uint32_t irisId);
sykean_iris_error_t iris_hal_set_active_group(void *pDev, uint32_t groupId, const char *pStorePath);
sykean_iris_error_t iris_hal_set_window(void *pDev, uint32_t groupId, void *pANW);
sykean_iris_error_t iris_hal_set_callback(void *pDev);

sykean_iris_error_t iris_hal_test_cmd(void *pDev, uint32_t cmdId, const uint8_t *pParam, uint32_t paramLen);

#ifdef __cplusplus
}
#endif

#endif //__IRIS_HAL_H__
