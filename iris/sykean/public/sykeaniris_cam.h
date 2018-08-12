/*
 * Copyright (c) 2017 SYKEAN Limited.
 *
 * All rights are reserved.
 * Proprietary and confidential.
 * Unauthorized copying of this file, via any medium is strictly prohibited.
 * Any use is subject to an appropriate license granted by SYKEAN Company.
 */

#ifndef __IRIS_CAM_H__
#define __IRIS_CAM_H__

#include "sykeaniris_error.h"

#ifdef __cplusplus
extern "C" {
#endif


enum CamCmdId
{
    CMD_CAM_OPEN = 0,
    CMD_CAM_CLOSE,
    CMD_CAM_IRIS_SUPPORT,
    CMD_CAM_START_PREVIEW,
    CMD_CAM_STOP_PREVIEW,
    CMD_CAM_SET_GAIN,
    CMD_CAM_SET_EXPOSURE,
    CMD_CAM_MAX
};


/* Camera callback function type */
typedef int32_t (*cam_ops_t)(const uint32_t cmdId, uint8_t *pParam, const uint32_t paramLen);

//
sykean_iris_error_t iris_hal_push_preview(void *pDev, const uint8_t *pData,
        const long size, const int32_t orientation);
sykean_iris_error_t iris_hal_set_cam_ops(void *pDev, cam_ops_t camOps);


#ifdef __cplusplus
}
#endif

#endif //__IRIS_CAM_H__
