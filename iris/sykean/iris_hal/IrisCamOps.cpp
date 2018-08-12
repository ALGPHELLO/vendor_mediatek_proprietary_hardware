/*
 * Copyright (c) 2017 SYKEAN Limited.
 *
 * All rights are reserved.
 * Proprietary and confidential.
 * Unauthorized copying of this file, via any medium is strictly prohibited.
 * Any use is subject to an appropriate license granted by SYKEAN Company.
 */

#define DEBUG_LOG_TAG "iris_hal"

#include "IrisCameraIF.h"
#include "IrisCamOpsCb.h"
#include "sykeaniris_cam.h"

#include <mtkcam/feature/iris/utils/Debug.h>

/******************************************************************************
 *
 ******************************************************************************/
// main:0x01; sub:0x02; main2:0x08
// NOTE: device id MUST BE THE SAME as mtkcam/drv/IHalSensor.h
#define IRIS_CAM_LOC_DEV_ID     (0x02)

using namespace Iris;

namespace
{
    static void *pIrisDev = NULL;
}


/******************************************************************************
 *
 ******************************************************************************/
static MVOID vPreviewCb(MVOID *a_pParam, long size, MINT32 orientation)
{
    IRIS_LOGD("preview callback +");

    iris_hal_push_preview(pIrisDev, (const uint8_t *)a_pParam,
            (const long)size, (const int32_t)orientation);

    IRIS_LOGD("preview callback -");
}


/******************************************************************************
 *
 ******************************************************************************/
int32_t
iris_cam_ops_cmd(const uint32_t cmdId, uint8_t *pParam, const uint32_t paramLen)
{
    AutoLog();

    IRIS_LOGD("Enter(cmd=%d)", cmdId);

    //====== Local Variable ======
    int32_t ret = 0;

    switch (cmdId)
    {
        case CMD_CAM_OPEN:
            {
                // FIXME: iris sensor index is the same as sub sensor on X20
                // the device index should query from the sensor driver
                // eventually.
                uint32_t initDev = IRIS_CAM_LOC_DEV_ID;

                if ((NULL != pParam) && (paramLen == sizeof(uint32_t)))
                {
                    initDev = *(uint32_t *)pParam;
                }
                if (0 == initDev)
                {
                    initDev = IRIS_CAM_LOC_DEV_ID;
                }
                if (IrisCamera_Open(initDev) != 0)
                {
                    IRIS_LOGE("IrisCamera_Open(%x) fail", initDev);
                    ret = -1;
                }
            }
            break;

        case CMD_CAM_CLOSE:
            if (IrisCamera_Close() != 0)
            {
                IRIS_LOGE("IrisCamera_Close() fail");
                ret = -1;
            }
            break;

        case CMD_CAM_IRIS_SUPPORT:
            {
                uint32_t isIrisSupport = 0x00;

                if (IrisCamera_IrisSupport(&isIrisSupport) != 0)
                {
                    IRIS_LOGE("IrisCamera_IrisSupport() fail");
                    ret = -1;
                }
                else
                {
                    IRIS_LOGD("IrisCamera_IrisSupport() = %x", isIrisSupport);
                    if ((NULL != pParam) && (paramLen == sizeof(uint32_t)))
                    {
                        *(uint32_t *)pParam = isIrisSupport;
                    }
                }
            }
            break;

        case CMD_CAM_START_PREVIEW:
            if (IrisCamera_StartPreview(vPreviewCb) != 0)
            {
                IRIS_LOGE("IrisCamera_StartPreview() fail");
                ret = -1;
            }
            break;

        case CMD_CAM_STOP_PREVIEW:
            if (IrisCamera_StopPreview() != 0)
            {
                IRIS_LOGE("IrisCamera_StopPreview() fail");
                ret = -1;
            }
            break;

        case CMD_CAM_SET_GAIN:
            if ((NULL != pParam) && (paramLen == sizeof(uint32_t)))
            {
                uint32_t gainValue = 0;

                gainValue = *(uint32_t *)pParam;
                if (IrisCamera_SetGain(gainValue) != 0)
                {
                    IRIS_LOGE("IrisCamera_SetGain(%x) fail", gainValue);
                    ret = -1;
                }
            }
            else
            {
                IRIS_LOGE("CMD_CAM_SET_GAIN input data error");
                ret = -2;
            }
            break;

        case CMD_CAM_SET_EXPOSURE:
            if ((NULL != pParam) && (paramLen == sizeof(uint32_t)))
            {
                uint32_t expValue = 0;

                expValue = *(uint32_t *)pParam;
                if (IrisCamera_SetExposure(expValue) != 0)
                {
                    IRIS_LOGE("IrisCamera_SetExposure(%x) fail", expValue);
                    ret = -1;
                }
            }
            else
            {
                IRIS_LOGE("CMD_CAM_SET_EXPOSURE input data error");
                ret = -2;
            }
            break;

        default:
            IRIS_LOGW("iris_cam_ops_cmd(%x) command unknown", cmdId);
            ret = -1;
            break;
    }

    return ret;
}


/******************************************************************************
 *
 ******************************************************************************/
sykean_iris_error_t
iris_hal_cam_init(void *pDev)
{
    IRIS_LOGD("Init Camera Hal");

    //====== Local Variable ======
    sykean_iris_error_t ret = SYKEAN_IRIS_SUCCESS;

    do
    {
        if (NULL == pDev)
        {
            IRIS_LOGE("iris_hal_cam_init param invaild!");
            ret = SYKEAN_IRIS_ERROR_BAD_PARAMS;
            break;
        }

        if (SYKEAN_IRIS_SUCCESS != iris_hal_set_cam_ops(pDev, iris_cam_ops_cmd))
        {
            IRIS_LOGE("iris_hal_set_cam_ops() fail!");
            ret = SYKEAN_IRIS_FAIL;
            break;
        }

        pIrisDev = pDev;
    } while (0);

    if (SYKEAN_IRIS_SUCCESS != ret)
    {
        iris_hal_cam_exit(pDev);
    }

    return ret;
}


/******************************************************************************
 *
 ******************************************************************************/
sykean_iris_error_t
iris_hal_cam_exit(void *pDev)
{
    IRIS_LOGD("Uninit Camera Hal");

    //====== Local Variable ======
    sykean_iris_error_t ret = SYKEAN_IRIS_SUCCESS;

    do
    {
        if (NULL == pDev)
        {
            IRIS_LOGE("iris_hal_cam_init param invaild!");
            ret = SYKEAN_IRIS_ERROR_BAD_PARAMS;
            break;
        }

        if (SYKEAN_IRIS_SUCCESS != iris_hal_set_cam_ops(pDev, NULL))
        {
            IRIS_LOGW("iris_hal_set_cam_ops() fail!");
            ret = SYKEAN_IRIS_FAIL;
        }

        pIrisDev = NULL;
    } while (0);

    return ret;
}

