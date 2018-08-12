/*
 * Copyright (c) 2017 SYKEAN Limited.
 *
 * All rights are reserved.
 * Proprietary and confidential.
 * Unauthorized copying of this file, via any medium is strictly prohibited.
 * Any use is subject to an appropriate license granted by SYKEAN Company.
 */

#ifndef __IRIS_LED_OPS_HAL_H__
#define __IRIS_LED_OPS_HAL_H__

#include "sykeaniris_error.h"

#ifdef __cplusplus
extern "C" {
#endif


/******************************************************************************
 *
 ******************************************************************************/
sykean_iris_error_t iris_hal_led_init(void *pDev);
sykean_iris_error_t iris_hal_led_exit(void *pDev);


#ifdef __cplusplus
}
#endif

#endif  //__IRIS_LED_OPS_HAL_H__

