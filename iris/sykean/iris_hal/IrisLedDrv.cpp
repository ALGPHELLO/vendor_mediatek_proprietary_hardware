/*
 * Copyright (c) 2017 SYKEAN Limited.
 *
 * All rights are reserved.
 * Proprietary and confidential.
 * Unauthorized copying of this file, via any medium is strictly prohibited.
 * Any use is subject to an appropriate license granted by SYKEAN Company.
 */

#define DEBUG_LOG_TAG "iris_hal"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/errno.h>

#include "IrisLedIF.h"
#include "flashlight.h"

#include <mtkcam/feature/iris/utils/Debug.h>


/******************************************************************************
 *
 ******************************************************************************/
#define USE_IR_LED_DUMMY        0


/******************************************************************************
 *
 ******************************************************************************/
#define IR_LED_DEV              "/dev/flashlight" // IR LED device node
#define FLASHLIGHT_TYPE_ID      (1) // 0(rear LED set), 1(front LED set)
#define FLASHLIGHT_CT_ID        (0) // 0(high color temp), 1(low color temp)
#define FLASHLIGHT_PART         (0) // HW part, just modify part 0,
                                    // if you are not sure about this.
#define FLASHLIGHT_TRUE         (1)
#define FLASHLIGHT_FALSE        (0)


/******************************************************************************
 *
 ******************************************************************************/
namespace
{
    enum
    {
        IRIS_LED_STATE_NONE,
        IRIS_LED_STATE_IDLE,
        IRIS_LED_STATE_ON,
        IRIS_LED_STATE_OFF,
    };

    static MINT32 ledState = IRIS_LED_STATE_NONE;
    static MINT32 gLedFd = -1;
}


#if !USE_IR_LED_DUMMY

/******************************************************************************
 *
 ******************************************************************************/
MINT32
strobeIoctlW(MINT32 cmd, MINT32 typeId, MINT32 ctId, MINT32 arg)
{
    struct flashlight_user_arg stbArg = {0, 0, 0};

    if (gLedFd > 0)
    {
        stbArg.type_id = typeId + 1;
        stbArg.ct_id = ctId + 1;
        stbArg.arg = arg;

        return ioctl(gLedFd, cmd, &stbArg);
    }
    else
    {
        return 1;
    }
}


/******************************************************************************
 *
 ******************************************************************************/
MINT32
strobeIoctlR(MINT32 cmd, MINT32 typeId, MINT32 ctId, MINT32 *pArg)
{
    struct flashlight_user_arg stbArg = {0, 0, 0};
    MINT32 ret = 0;

    if (gLedFd > 0)
    {
        stbArg.type_id = typeId + 1;
        stbArg.ct_id = ctId + 1;
        stbArg.arg = 0;

        ret = ioctl(gLedFd, cmd, &stbArg);
        *pArg = stbArg.arg;

        return ret;
    }
    else
    {
        return 1;
    }
}


/******************************************************************************
 *
 ******************************************************************************/
MINT32
IrisLed_Open()
{
    IRIS_LOGD("Open IrisLed");

    //====== Local Variable ======
    MINT32 ret = 0;

    do
    {
        if (IRIS_LED_STATE_NONE != ledState)
        {
            IRIS_LOGE("IrisLed already open");
            ret = -1;
            break;
        }

        if (gLedFd > 0)
        {
            close(gLedFd);
        }
        gLedFd = open(IR_LED_DEV, O_RDWR);
        if (gLedFd < 0)
        {
            IRIS_LOGE("Can't Open IR LED Dev:%s", IR_LED_DEV);
            ret = -2;
            break;
        }

        ret = strobeIoctlW(FLASHLIGHTIOC_X_SET_DRIVER,
                FLASHLIGHT_TYPE_ID, FLASHLIGHT_CT_ID, FLASHLIGHT_TRUE);
        if (0 != ret)
        {
            IRIS_LOGE("FLASHLIGHTIOC_X_SET_DRIVER fail(%d)!", ret);
            ret = -3;
            break;
        }

        ledState = IRIS_LED_STATE_IDLE;
        IRIS_LOGD("Exit");
    } while (MFALSE);

    return ret;
}


/******************************************************************************
 *
 ******************************************************************************/
MINT32
IrisLed_Close()
{
    IRIS_LOGD("Close IrisLed");

    //====== Local Variable ======
    MINT32 ret = 0;

    do
    {
        if (IRIS_LED_STATE_NONE == ledState)
        {
            IRIS_LOGE("IrisLed already close");
            ret = -1;
            break;
        }

        if (gLedFd >= 0)
        {
            close(gLedFd);
            IRIS_LOGD("IR LED Dev Close %s", IR_LED_DEV);
        }

        gLedFd = -1;
        ledState = IRIS_LED_STATE_NONE;
        IRIS_LOGD("Exit");
    } while (MFALSE);

    return ret;
}


/******************************************************************************
 *
 ******************************************************************************/
MINT32
IrisLed_GetType(MINT32 *ledType)
{
    IRIS_LOGD("Enter");

    //====== Local Variable ======
    MINT32 type = 0;
    MINT32 ret = 0;

    do
    {
        if (IRIS_LED_STATE_NONE == ledState)
        {
            IRIS_LOGE("IrisLed not open:State(%d)", ledState);
            ret = -1;
            break;
        }

        type = IR_LED_TORCH;

        if (NULL != ledType)
        {
            *ledType = type;
        }
        IRIS_LOGD("Exit");
    } while (MFALSE);

    return ret;
}


/******************************************************************************
 *
 ******************************************************************************/
MINT32
IrisLed_SetType(MINT32 ledType)
{
    IRIS_LOGD("Enter");

    //====== Local Variable ======
    MINT32 ret = 0;

    do
    {
        if (IRIS_LED_STATE_NONE == ledState)
        {
            IRIS_LOGE("IrisLed not open, can't set duty");
            ret = -1;
            break;
        }

        ledType = IR_LED_TORCH; // for build warning

        IRIS_LOGD("Exit");
    } while (MFALSE);

    return ret;
}


/******************************************************************************
 *
 ******************************************************************************/
MINT32
IrisLed_SetDuty(MUINT32 duty)
{
    IRIS_LOGD("Enter");

    //====== Local Variable ======
    MINT32 ret = 0;

    do
    {
        if (IRIS_LED_STATE_NONE == ledState)
        {
            IRIS_LOGE("IrisLed not open, can't set duty");
            ret = -1;
            break;
        }

        //====== Set IR LED Duty ======
        ret = strobeIoctlW(FLASH_IOC_SET_DUTY,
                FLASHLIGHT_TYPE_ID, FLASHLIGHT_CT_ID, (MINT32)duty);
        if (0 != ret)
        {
            IRIS_LOGE("FLASH_IOC_SET_DUTY fail(%d)!", ret);
            ret = -2;
            break;
        }

        IRIS_LOGD("Exit");
    } while (MFALSE);

    return ret;
}


/******************************************************************************
 *
 ******************************************************************************/
MINT32
IrisLed_SetOn()
{
    IRIS_LOGD("Enter");

    //====== Local Variable ======
    const MINT32 value = IR_LED_ON; // IR LED open
    const MINT32 timeOut = 0; // timeout 0 ms
    const MINT32 duty = 5; // RT4505: current 328.13mA
    MINT32 ret = 0;

    do
    {
        if (IRIS_LED_STATE_NONE == ledState)
        {
            IRIS_LOGE("IrisLed not open, can't set on");
            ret = -1;
            break;
        }

        //====== Set IR LED On ======
        ret = strobeIoctlW(FLASH_IOC_SET_TIME_OUT_TIME_MS,
                FLASHLIGHT_TYPE_ID, FLASHLIGHT_CT_ID, timeOut);
        if (0 != ret)
        {
            IRIS_LOGE("FLASH_IOC_SET_TIME_OUT_TIME_MS fail(%d)!", ret);
            ret = -2;
            break;
        }

        ret = strobeIoctlW(FLASH_IOC_SET_DUTY,
                FLASHLIGHT_TYPE_ID, FLASHLIGHT_CT_ID, duty);
        if (0 != ret)
        {
            IRIS_LOGE("FLASH_IOC_SET_DUTY fail(%d)!", ret);
            ret = -3;
            break;
        }

        ret = strobeIoctlW(FLASH_IOC_SET_ONOFF,
                FLASHLIGHT_TYPE_ID, FLASHLIGHT_CT_ID, value);
        if (0 != ret)
        {
            IRIS_LOGE("FLASH_IOC_SET_ONOFF fail(%d)!", ret);
            ret = -4;
            break;
        }

        ledState = IRIS_LED_STATE_ON;
        IRIS_LOGD("Exit");
    } while (MFALSE);

    return ret;
}


/******************************************************************************
 *
 ******************************************************************************/
MINT32
IrisLed_SetOff()
{
    IRIS_LOGD("Enter");

    //====== Local Variable ======
    const MINT32 value = IR_LED_OFF;
    MINT32 ret = 0;

    do
    {
        if (IRIS_LED_STATE_NONE == ledState)
        {
            IRIS_LOGE("IrisLed not open, can't set off");
            ret = -1;
            break;
        }

        //====== Set IR LED Off ======
        ret = strobeIoctlW(FLASH_IOC_SET_ONOFF,
                FLASHLIGHT_TYPE_ID, FLASHLIGHT_CT_ID, value);
        if (0 != ret)
        {
            IRIS_LOGE("FLASH_IOC_SET_ONOFF fail(%d)!", ret);
            ret = -2;
            break;
        }

        ledState = IRIS_LED_STATE_OFF;
        IRIS_LOGD("Exit");
    } while (MFALSE);

    return ret;
}

#else

/******************************************************************************
 *
 ******************************************************************************/
MINT32
IrisLed_Open()
{
    IRIS_LOGD("Open IrisLed(dummy)");

    //====== Local Variable ======
    MINT32 ret = 0;

    do
    {
        if (IRIS_LED_STATE_NONE != ledState)
        {
            IRIS_LOGE("IrisLed(dummy) already open");
            ret = -1;
            break;
        }

        ledState = IRIS_LED_STATE_IDLE;
        IRIS_LOGD("Exit(dummy)");
    } while (MFALSE);

    return ret;
}


/******************************************************************************
 *
 ******************************************************************************/
MINT32
IrisLed_Close()
{
    IRIS_LOGD("Close IrisLed(dummy)");

    //====== Local Variable ======
    MINT32 ret = 0;

    do
    {
        if (IRIS_LED_STATE_NONE == ledState)
        {
            IRIS_LOGE("IrisLed(dummy) already close");
            ret = -1;
            break;
        }

        ledState = IRIS_LED_STATE_NONE;
        IRIS_LOGD("Exit(dummy)");
    } while (MFALSE);

    return ret;
}


/******************************************************************************
 *
 ******************************************************************************/
MINT32
IrisLed_GetType(MINT32 *ledType)
{
    IRIS_LOGD("Enter(dummy)");

    //====== Local Variable ======
    MINT32 type = 0;
    MINT32 ret = 0;

    do
    {
        if (IRIS_LED_STATE_NONE == ledState)
        {
            IRIS_LOGE("IrisLed(dummy) not open:State(%d)", ledState);
            ret = -1;
            break;
        }

        if (NULL != ledType)
        {
            *ledType = type;
        }
        IRIS_LOGD("Exit(dummy)");
    } while (MFALSE);

    return ret;
}


/******************************************************************************
 *
 ******************************************************************************/
MINT32
IrisLed_SetType(MINT32 ledType)
{
    IRIS_LOGD("Enter(dummy)");

    //====== Local Variable ======
    MINT32 ret = 0;

    do
    {
        if (IRIS_LED_STATE_NONE == ledState)
        {
            IRIS_LOGE("IrisLed(dummy) not open, can't set duty");
            ret = -1;
            break;
        }

        ledType; // for build warning
        IRIS_LOGD("Exit(dummy)");
    } while (MFALSE);

    return ret;
}


/******************************************************************************
 *
 ******************************************************************************/
MINT32
IrisLed_SetDuty(MUINT32 duty)
{
    IRIS_LOGD("Enter(dummy)");

    //====== Local Variable ======
    MINT32 ret = 0;

    do
    {
        if (IRIS_LED_STATE_NONE == ledState)
        {
            IRIS_LOGE("IrisLed(dummy) not open, can't set duty");
            ret = -1;
            break;
        }

        duty; // for build warning
        IRIS_LOGD("Exit(dummy)");
    } while (MFALSE);

    return ret;
}


/******************************************************************************
 *
 ******************************************************************************/
MINT32
IrisLed_SetOn()
{
    IRIS_LOGD("Enter(dummy)");

    //====== Local Variable ======
    const MINT32 value = IR_LED_ON;
    MINT32 ret = 0;

    do
    {
        if (IRIS_LED_STATE_NONE == ledState)
        {
            IRIS_LOGE("IrisLed(dummy) not open, can't set on");
            ret = -1;
            break;
        }

        ledState = IRIS_LED_STATE_ON;
        IRIS_LOGD("Exit(dummy)");
    } while (MFALSE);

    return ret;
}


/******************************************************************************
 *
 ******************************************************************************/
MINT32
IrisLed_SetOff()
{
    IRIS_LOGD("Enter(dummy)");

    //====== Local Variable ======
    const MINT32 value = IR_LED_OFF;
    MINT32 ret = 0;

    do
    {
        if (IRIS_LED_STATE_NONE == ledState)
        {
            IRIS_LOGE("IrisLed(dummy) not open, can't set off");
            ret = -1;
            break;
        }

        ledState = IRIS_LED_STATE_OFF;
        IRIS_LOGD("Exit(dummy)");
    } while (MFALSE);

    return ret;
}

#endif
