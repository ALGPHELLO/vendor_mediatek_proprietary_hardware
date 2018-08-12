/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 */
/* MediaTek Inc. (C) 2010. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
 * AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
 * NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
 * SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
 * SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
 * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
 * THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
 * SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
 * AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
 * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 * MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek Software")
 * have been modified by MediaTek Inc. All revisions are subject to any receiver's
 * applicable license agreements with MediaTek Inc.
 */

/*
**
** Copyright 2008, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#ifndef _MTK_3D_VERIFY_H
#define _MTK_3D_VERIFY_H

#include "MTK3dVerifyType.h"
#include "MTK3dVerifyErrCode.h"

typedef enum Drv3dVerifyObject_e {
    DRV_3D_VERIFY_OBJ_NONE = 0,
    DRV_3D_VERIFY_OBJ_CPU,

    DRV_3D_VERIFY_OBJ_UNKNOWN = 0xFF,
} Drv3dVerifyObject_e;

//#define TIME_PROF



/*****************************************************************************
    FOV Define and State Machine
******************************************************************************/
//#define DEBUG

typedef enum
{
    MTK_3D_VERIFY_STATE_STANDBY=0,   // After Create Obj or Reset
    MTK_3D_VERIFY_STATE_INIT,        // After Called FOVInit
    MTK_3D_VERIFY_STATE_PROC,        // After Called FOVMain
    MTK_3D_VERIFY_STATE_PROC_READY,  // After Finish FOVMain
} MTK_3D_VERIFY_STATE_ENUM;

/*****************************************************************************
    Process Control
******************************************************************************/

/*****************************************************************************
    Feature Control Enum and Structure
******************************************************************************/
typedef enum
{
    MTK_3D_VERIFY_FEATURE_BEGIN,         // minimum of feature id
    MTK_3D_VERIFY_FEATURE_ADD_IMAGE,     // feature id to add image information
    MTK_3D_VERIFY_FEATURE_GET_RESULT,    // feature id to get result
    MTK_3D_VERIFY_FEATURE_GET_LOG,       // feature id to get debugging information
    MTK_3D_VERIFY_FEATURE_GET_WORKBUF_SIZE,// feature id to query buffer size
    MTK_3D_VERIFY_FEATURE_SET_WORKBUF_ADDR,  // feature id to set working buffer address
    MTK_3D_VERIFY_FEATURE_MAX            // maximum of feature id
}   MTK_3D_VERIFY_FEATURE_ENUM;

typedef enum
{
	MTK_3D_VERIFY_TYPE_NEAR,
	MTK_3D_VERIFY_TYPE_FAR,
	MTK_3D_VERIFY_TYPE_NEAR_AND_FAR
} MTK_3D_VERIFY_TYPE_ENUM;

/*
typedef struct MTK_3D_VERIFY_TUNING_STRUCT
{
 //NULL
}MTK_3D_VERIFY_TUNING;
*/

typedef struct MTK3dVerifyImageInfoStruct
{
  MTK_3D_VERIFY_TYPE_ENUM    verify_type;		//translation or rotation
  MUINT8*                    main_raw_near;
  MUINT8*                    main_raw_far;
  MINT32                     main_w;
  MINT32                     main_h;
  MUINT8*                    sub_raw_near;
  MUINT8*                    sub_raw_far;
  MINT32                     sub_w;
  MINT32                     sub_h;
  MUINT8*                    Config;   
  MINT32                     pattern_w;
  
  MUINT8*                    WorkingBuffAddr;         // Working buffer start address
}MTK3dVerifyImageInfo;

typedef struct MTK3dVerifyResultInfoStruct
{	
	//UI Show msg
	MINT32                  status; //near & far verify result, 0: PASS , >=1: FAIL
	MINT32                  geometry_near_status;
	MINT32                  geometry_far_status;
	MINT32                  near_boundary_distance[4];//Top, Right, Bottom, Left
	MINT32                  far_boundary_distance[4];
	MFLOAT                  near_rotation[3];//pitch, yaw, roll
	MFLOAT                  far_rotation[3];

	//debug log
	MINT32                  gVerify_Items[180];
	//performance               
    MINT32                  ElapseTime;              // performance                              
}MTK3dVerifyResultInfo;

/*******************************************************************************
*
********************************************************************************/
class MTK3dVerify {
public:
    static MTK3dVerify* createInstance(Drv3dVerifyObject_e eobject);
    virtual void   destroyInstance(MTK3dVerify* obj) = 0;

    virtual ~MTK3dVerify();
    // Process Control
    virtual MRESULT Init(MUINT32 *InitInData, MUINT32 *InitOutData);    
    virtual MRESULT Main(void);                                         
    virtual MRESULT Reset(void);                                        

    // Feature Control
    virtual MRESULT FeatureCtrl(MUINT32 FeatureID, void* pParaIn, void* pParaOut);
private:

};

#endif

