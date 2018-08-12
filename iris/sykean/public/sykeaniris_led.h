/*
 * Copyright (c) 2017 SYKEAN Limited.
 *
 * All rights are reserved.
 * Proprietary and confidential.
 * Unauthorized copying of this file, via any medium is strictly prohibited.
 * Any use is subject to an appropriate license granted by SYKEAN Company.
 */

#ifndef __IRIS_LED_H__
#define __IRIS_LED_H__

#include "sykeaniris_error.h"

#ifdef __cplusplus
extern "C" {
#endif


enum LedCmdId
{
    CMD_LED_OPEN = 0,
    CMD_LED_CLOSE,
    CMD_LED_GET_TYPE,
    CMD_LED_SET_TYPE,
    CMD_LED_SET_DUTY,
    CMD_LED_SET_ON,
    CMD_LED_SET_OFF,
    CMD_LED_MAX
};


/* Camera callback function type */
typedef int32_t (*led_ops_t)(const uint32_t cmdId, uint8_t *pParam, const uint32_t paramLen);

//
sykean_iris_error_t iris_hal_set_led_ops(void *pDev, led_ops_t ledOps);


#ifdef __cplusplus
}
#endif

#endif //__IRIS_LED_H__
