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

#ifndef _MTK_FOV_H
#define _MTK_FOV_H

#include "MTKFOVType.h"
#include "MTKFOVErrCode.h"
//#include "MTKFeatureSet.h"

typedef enum DrvFovObject_e {
    DRV_FOV_OBJ_NONE = 0,
    DRV_FOV_OBJ_CPU,

    DRV_FOV_OBJ_UNKNOWN = 0xFF,
} DrvFOVObject_e;

//#define TIME_PROF



/*****************************************************************************
    FOV Define and State Machine
******************************************************************************/
//#define DEBUG

typedef enum
{
    FOV_STATE_STANDBY=0,   // After Create Obj or Reset
    FOV_STATE_INIT,        // After Called FOVInit
    FOV_STATE_PROC,        // After Called FOVMain
    FOV_STATE_PROC_READY,  // After Finish FOVMain
} FOV_STATE_ENUM;

/*****************************************************************************
    Process Control
******************************************************************************/

/*****************************************************************************
    Feature Control Enum and Structure
******************************************************************************/
typedef enum
{
    FOV_FEATURE_BEGIN,         // minimum of feature id
    FOV_FEATURE_ADD_IMAGE,     // feature id to add image information
    FOV_FEATURE_GET_RESULT,    // feature id to get result
    FOV_FEATURE_GET_LOG,       // feature id to get debugging information
    FOV_FEATURE_GET_WORKBUF_SIZE,// feature id to query buffer size
    FOV_FEATURE_SET_WORKBUF_ADDR,  // feature id to set working buffer address
    FOV_FEATURE_MAX            // maximum of feature id
}   FOV_FEATURE_ENUM;

typedef enum
{
	FOV_CALIBRATION_TYPE_TRANSLATION,
	FOV_CALIBRATION_TYPE_ROTATION
} FOV_CALIBRATION_TYPE;


typedef enum
{
	FOV_ONLINE_CALIBRATION_OFF,
	FOV_ONLINE_CALIBRATION_INFINITY,
	FOV_ONLINE_CALIBRATION_ALL
} FOV_ONLINE_CALIBRATION;

typedef struct FOV_TUNING_STRUCT
{
    MFLOAT learningRate;
    MFLOAT thre_var_mv_square;
	MFLOAT FOV_WideTele_normalized_DAC_diff_thre;
	MFLOAT mappingTemporalUpdateRate;
	MFLOAT learningRate_fuzzy;
	MFLOAT interpolationWeightCenter;
}FOV_TUNING;

struct FOVImageInfo
{
	FOV_CALIBRATION_TYPE    CalibrationMode;		//translation or rotation
	FOV_ONLINE_CALIBRATION	online_calibration;		//offline, online infinity, online all
	MUINT32                 Calibration_on_Tele;    //true: tele, false: wide
	MUINT32*				input_nvram_data;       //initial calibration data
	MUINT32*                input_eeprom_data;
	MUINT32                 imageAlignCenter_x;
	MUINT32                 imageAlignCenter_y;
    MINT32                  imgW_wide_ori;          //wide sensor resolution       
    MINT32                  imgH_wide_ori; 
    MINT32                  imgW_tele_ori;          //tele sensor resolution            
    MINT32                  imgH_tele_ori;   

    MINT32                  imgW_crop;              //sensor active size          
    MINT32                  imgH_crop; 
    MINT32                  imgW_zoom;			    //valid sensor crop size          
    MINT32                  imgH_zoom; 
    MINT32                  imgW_crop_resize;       //fov input buffer size          
    MINT32                  imgH_crop_resize;   
    MUINT32                 imgW_zoom_resize;       //image result width
    MUINT32                 imgH_zoom_resize;       //image result height
    MFLOAT                  current_scale;
    MFLOAT                  switch_scale;
    MFLOAT                  fovActiveScale;
    MFLOAT                  cropRatio_initial;
    MUINT8*                 WorkingBuffAddr;         // Working buffer start address
	/*
	Online calibration parameters
	*/ 
    MUINT32                 imgW_FEFM;                    
    MUINT32                 imgH_FEFM;
    MINT32                  FEFM_WD_crop_tele;
    MINT32                  FEFM_HT_crop_tele;
    MINT32                  FEFM_WD_crop_wide;
    MINT32                  FEFM_HT_crop_wide;
    MINT16*                 Fe_wide;
    MINT16*                 Fe_tele;
    MINT16*                 Fm_wide;
	MINT16*                 Fm_tele;
    MUINT32                 FeBlockSize[2];

	MFLOAT					autoFocus_DAC;
	MFLOAT					infinitySceneProbability;
	MUINT32                 ISO;
	MUINT32                 temperature;
	/*
	On-line calibration tuning
	*/
	FOV_TUNING              fov_tuning_para;
	/*
	EEPROM
	*/
    MINT32                  wide_WD_check;
	MINT32                  wide_HT_check;
    MINT32                  tele_WD_check;
	MINT32                  tele_HT_check; 
	MFLOAT                  distNea_A_check;
	MFLOAT                  distFar_B_check;
	/*
	Multi-diatance
	*/
	MINT32                  support_multi_dist;//true or false
	MINT32                  wide_DAC;
	MINT32                  tele_DAC;
	MINT32                  AF_SUCCESS_wide;
	MINT32                  AF_SUCCESS_tele;
	MINT32                  AF_DONE_wide;
	MINT32                  AF_DONE_tele;

	MINT32 wide_DAC_INF; // calibrated wide DAC at infinity distance
	MINT32 wide_DAC_MID; // calibrated wide DAC at middle distance
	MINT32 wide_DAC_MAC; // calibrated wide DAC at macro distance
	MINT32 wide_DAC_FAR; // calibrated wide DAC at far  distance (stereo calibration) 
	MINT32 wide_DAC_NEA; // calibrated wide DAC at near distance (stereo calibration) 

	MINT32 tele_DAC_INF; // calibrated tele DAC at infinity distance
	MINT32 tele_DAC_MAC; // calibrated tele DAC at macro distance
	MINT32 tele_DAC_FAR; // calibrated tele DAC at far  distance (stereo calibration) 
	MINT32 tele_DAC_NEA; // calibrated tele DAC at near distance (stereo calibration) 
};

struct FOVResultInfo
{	
	MUINT32				    update_nvram_flag;      //return latest calibration data, 1: Update NVRAM, 2:No
	//rotation
    MINT32                  WarpMapX[4];                      
    MINT32                  WarpMapY[4];
	//translation
    MINT32                  OffsetX;
	MINT32                  OffsetY;
	MFLOAT                  scale;
	//performance               
    MINT32                  ElapseTime[3];              // performance            
    MRESULT                 RetCode;                    
};

/*******************************************************************************
*
********************************************************************************/
class MTKFOV {
public:
    static MTKFOV* createInstance(DrvFOVObject_e eobject);
    virtual void   destroyInstance(MTKFOV* obj) = 0;

    virtual ~MTKFOV();
    // Process Control
    virtual MRESULT FOVInit(MUINT32 *InitInData, MUINT32 *InitOutData);    
    virtual MRESULT FOVMain(void);                                         
    virtual MRESULT FOVReset(void);                                        

    // Feature Control
    virtual MRESULT FOVFeatureCtrl(MUINT32 FeatureID, void* pParaIn, void* pParaOut);
private:

};

#endif

