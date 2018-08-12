/*
 * Copyright (c) 2017 SYKEAN Limited.
 *
 * All rights are reserved.
 * Proprietary and confidential.
 * Unauthorized copying of this file, via any medium is strictly prohibited.
 * Any use is subject to an appropriate license granted by SYKEAN Company.
 */

#define DEBUG_LOG_TAG "iris_hal"
//
#include <stdlib.h>

#include "IrisLedIF.h"
#include "IrisLedOpsCb.h"
#include "sykeaniris_led.h"

#include <mtkcam/feature/iris/utils/Debug.h>


/******************************************************************************
 *
 ******************************************************************************/
using namespace Iris;

namespace
{
    static void *pIrisDev = NULL;
}


/******************************************************************************
 *
 ******************************************************************************/
int32_t
iris_led_ops_cmd(const uint32_t cmdId, uint8_t *pParam, const uint32_t paramLen)
{
    AutoLog();

    IRIS_LOGD("Enter(cmd=%d)", cmdId);

    //====== Local Variable ======
    int32_t ret = 0;

    switch (cmdId)
    {
        case CMD_LED_OPEN:
            if (IrisLed_Open() != 0)
            {
                IRIS_LOGE("IrisLed_Openfail");
                ret = -1;
            }
            break;

        case CMD_LED_CLOSE:
            if (IrisLed_Close() != 0)
            {
                IRIS_LOGE("IrisLed_Close() fail");
                ret = -1;
            }
            break;

        case CMD_LED_GET_TYPE:
            {
                int32_t ledType = 0x00;

                if (IrisLed_GetType(&ledType) != 0)
                {
                    IRIS_LOGE("IrisLed_GetType() fail");
                    ret = -1;
                }
                else
                {
                    IRIS_LOGD("IrisLed_GetType() = %x", ledType);
                    if ((NULL != pParam) && (paramLen == sizeof(uint32_t)))
                    {
                        *(uint32_t *)pParam = ledType;
                    }
                }
            }
            break;

        case CMD_LED_SET_TYPE:
            if ((NULL != pParam) && (paramLen == sizeof(int32_t)))
            {
                int32_t ledType = 0;

                ledType = *(int32_t *)pParam;
                if (IrisLed_SetType(ledType) != 0)
                {
                    IRIS_LOGE("IrisLed_SetType(%x) fail", ledType);
                    ret = -1;
                }
            }
            else
            {
                IRIS_LOGE("CMD_LED_SET_TYPE input data error");
                ret = -2;
            }
            break;

        case CMD_LED_SET_DUTY:
            if ((NULL != pParam) && (paramLen == sizeof(uint32_t)))
            {
                uint32_t ledDuty = 0;

                ledDuty = *(uint32_t *)pParam;
                if (IrisLed_SetDuty(ledDuty) != 0)
                {
                    IRIS_LOGE("IrisLed_SetDuty() fail");
                    ret = -1;
                }
            }
            else
            {
                IRIS_LOGE("CMD_LED_SET_DUTY input data error");
                ret = -2;
            }
            break;

        case CMD_LED_SET_ON:
            if (IrisLed_SetOn() != 0)
            {
                IRIS_LOGE("IrisLed_SetOn() fail");
                ret = -1;
            }
            break;

        case CMD_LED_SET_OFF:
            if (IrisLed_SetOff() != 0)
            {
                IRIS_LOGE("IrisLed_SetOff() fail");
                ret = -1;
            }
            break;

        default:
            IRIS_LOGW("iris_led_ops_cmd(%x) command unknown", cmdId);
            ret = -1;
            break;
    }

    return ret;
}


/******************************************************************************
 *
 ******************************************************************************/
sykean_iris_error_t
iris_hal_led_init(void *pDev)
{
    IRIS_LOGD("Init Led Hal");

    //====== Local Variable ======
    sykean_iris_error_t ret = SYKEAN_IRIS_SUCCESS;

    do
    {
        if (NULL == pDev)
        {
            IRIS_LOGE("iris_hal_led_init param invaild!");
            ret = SYKEAN_IRIS_ERROR_BAD_PARAMS;
            break;
        }

        if (SYKEAN_IRIS_SUCCESS != iris_hal_set_led_ops(pDev, iris_led_ops_cmd))
        {
            IRIS_LOGE("iris_hal_set_led_ops() fail!");
            ret = SYKEAN_IRIS_FAIL;
            break;
        }

        pIrisDev = pDev;
    } while (0);

    if (SYKEAN_IRIS_SUCCESS != ret)
    {
        iris_hal_led_exit(pDev);
    }

    return ret;
}


/******************************************************************************
 *
 ******************************************************************************/
sykean_iris_error_t
iris_hal_led_exit(void *pDev)
{
    IRIS_LOGD("Uninit Led Hal");

    //====== Local Variable ======
    sykean_iris_error_t ret = SYKEAN_IRIS_SUCCESS;

    do
    {
        if (NULL == pDev)
        {
            IRIS_LOGE("iris_hal_led_init param invaild!");
            ret = SYKEAN_IRIS_ERROR_BAD_PARAMS;
            break;
        }

        if (SYKEAN_IRIS_SUCCESS != iris_hal_set_led_ops(pDev, NULL))
        {
            IRIS_LOGE("iris_hal_set_led_ops() fail!");
            ret = SYKEAN_IRIS_FAIL;
        }

        pIrisDev = NULL;
    } while (0);

    return ret;
}

