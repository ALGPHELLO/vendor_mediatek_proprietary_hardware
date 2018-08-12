/*
 * Copyright (c) 2017 SYKEAN Limited.
 *
 * All rights are reserved.
 * Proprietary and confidential.
 * Unauthorized copying of this file, via any medium is strictly prohibited.
 * Any use is subject to an appropriate license granted by SYKEAN Company.
 */

#ifndef _SYKEAN_HARDWARE_INCLUDE_IRIS_IRISLEDIF_H_
#define _SYKEAN_HARDWARE_INCLUDE_IRIS_IRISLEDIF_H_

#include "IrisType.h"


/******************************************************************************
 *
 ******************************************************************************/
/* IR LED type enum */
typedef enum {
    IR_LED_TORCH = 1 << 0, /* IR LED driver support torch mode */
    IR_LED_FLASH = 1 << 1, /* IR LED driver support flash mode */
} IR_LED_TYPE_ENUM;


/* IR LED state enum */
typedef enum {
    IR_LED_OFF = 0,
    IR_LED_ON = 1,
} IR_LED_STATE_ENUM;


/******************************************************************************
 *
 ******************************************************************************/
MINT32 IrisLed_Open();
MINT32 IrisLed_Close();
MINT32 IrisLed_GetType(MINT32 *ledType);
MINT32 IrisLed_SetType(MINT32 ledType);
MINT32 IrisLed_SetDuty(MUINT32 duty);
MINT32 IrisLed_SetOn();
MINT32 IrisLed_SetOff();


#endif  //_SYKEAN_HARDWARE_INCLUDE_IRIS_IRISLEDIF_H_

