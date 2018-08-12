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

#ifndef __MTK_FEFM_H__
#define __MTK_FEFM_H__

#include "MTKFeFmType.h"
#include "MTKFeFmErrCode.h"

#define MTK_FEFM_MAX_BUFFER_COUNT (15)
#define MTK_FEFM_OUTPUT_LEN       (43)  // 2017.0531

typedef enum DrvFeFmObject_e
{
    DRV_FEFM_OBJ_NONE = 0,
    DRV_FEFM_OBJ_CPU,
	DRV_FEFM_OBJ_VPU,
    DRV_FEFM_OBJ_UNKNOWN = 0xFF,
}
DrvFeFmObject_e;

typedef enum
{
    MTK_FEFM_STATE_STANDBY=0,   // After Create Obj or Reset
    MTK_FEFM_STATE_INIT,        // After Called FOVInit
    MTK_FEFM_STATE_PROC,        // After Called FOVMain
    MTK_FEFM_STATE_PROC_READY,  // After Finish FOVMain
}
MTK_FEFM_STATE_ENUM;

/*****************************************************************************
    Process Control
******************************************************************************/

/*****************************************************************************
    Feature Control Enum and Structure
******************************************************************************/
typedef enum
{
    MTK_FEFM_FEATURE_BEGIN,             // minimum of feature id
    MTK_FEFM_FEATURE_ADD_IMAGE,         // feature id to add image information
    MTK_FEFM_FEATURE_GET_RESULT,        // feature id to get result
    MTK_FEFM_FEATURE_GET_LOG,           // feature id to get debugging information
    MTK_FEFM_FEATURE_GET_WORKBUF_SIZE,  // feature id to query buffer size
    MTK_FEFM_FEATURE_SET_WORKBUF_ADDR,  // feature id to set working buffer address
    MTK_FEFM_FEATURE_MAX                // maximum of feature id
}
MTK_FEFM_FEATURE_ENUM;

typedef enum
{
	MTK_FEFM_TYPE_DO_FE,
	MTK_FEFM_TYPE_DO_FM
}
MTK_FEFM_TYPE_ENUM;

typedef enum
{
    MTK_FEFM_INPUT_FORMAT_YV12 = 0,
    MTK_FEFM_INPUT_FORMAT_RGBA,
}
MTK_FEFM_INPUT_FORMAT_ENUM ;

typedef struct
{
    MUINT32 search_range_xL ;           // Left  Search Range - # of blocks
    MUINT32 search_range_xR ;           // Right Search Range - # of blocks
    MUINT32 search_range_yT ;           // Top   Search Range - # of blocks
    MUINT32 search_range_yD ;           // Down  Search Range - # of blocks
    MUINT32 match_ratio ;               // Error rate between minimum and second minimum error
	
	MUINT16 max_execute_fe_num ;        // Maximal # of Execution Features (Valid Points)
    MUINT32 thr_grd ;					// Threshold for Graident
    MUINT32 thr_cr ;                    // Threshold for Corner Response
}
MTK_FEFM_TUNING_PARA_STRUCT ;           // 2017.0531

/**
 *  \details utility basic image structure
 */
typedef enum MTK_FEFM_MEMORY_TYPE{
    MTK_FEFM_MEMORY_VA,
    MTK_FEFM_MEMORY_GRALLOC_Y8,
    MTK_FEFM_MEMORY_GRALLOC_IMG_Y16,
    MTK_FEFM_MEMORY_GRALLOC_IMG_FLOAT,
    MTK_FEFM_MEMORY_GRALLOC_YV12
}MTK_FEFM_MEMORY_TYPE;

typedef enum MTK_FEFM_IMAGE_FMT_ENUM
{
    MTK_FEFM_IMAGE_DATA8,
    MTK_FEFM_IMAGE_DATA16,
    MTK_FEFM_IMAGE_DATA32,

    MTK_FEFM_IMAGE_RAW8,
    MTK_FEFM_IMAGE_RAW10,
    MTK_FEFM_IMAGE_EXT_RAW8,
    MTK_FEFM_IMAGE_EXT_RAW10
} MTK_FEFM_IMAGE_FMT_ENUM;

typedef struct MTK_FEFM_Image{
    MTK_FEFM_MEMORY_TYPE eMemType;    // in:
    MTK_FEFM_IMAGE_FMT_ENUM eImgFmt;  // in:
    MINT32 i4Width;       // in: source width
    MINT32 i4Height;      // in: source height
    MINT32 i4Offset;      // in: source data offset (bytes)
    MINT32 i4Pitch;       // in: buffer pitch
    MINT32 i4Size;        // in: buffser size
    MINT32 i4PlaneNum;    // in:  plane number
    void* pvPlane[4];     //in: graphic buffer pointer
}
MTK_FEFM_Image;

typedef MTK_FEFM_Image MTK_FEFM_WBuffer;

typedef struct MTK_FEFM_WorkingBufferInfo{
    MINT32 i4WorkingBufferNum;          // out:
    MTK_FEFM_WBuffer rWorkingBuffer[MTK_FEFM_MAX_BUFFER_COUNT]; //inout
    // information for working buffer
    MINT32 i4PadWidth;             // input: pad image width
    MINT32 i4PadHeight;            // input: pad image height
} MTK_FEFM_WorkingBufferInfo;

typedef struct MTKFeFmImageInfoStruct
{
    MTK_FEFM_TYPE_ENUM             fe_fm_type;

	void* fe_data1;
	void* fe_data2;
	void* fm_idx1;
	void* fm_idx2;
	MTK_FEFM_TUNING_PARA_STRUCT    fe_fm_tuning;
    // HWFE Initialize
    MUINT16 fe_block_size ;
    MUINT16 imgW ;
    MUINT16 imgH ;
    MUINT32 stride ;
    MUINT32 format ;
    MUINT16 fe_block_num ;   // # of blocks = # of extracted features
    MUINT16 fe_block_num_x ; // # of blocks - x direction
    MUINT16 fe_block_num_y ; // # of blocks - y direction
    // ProcSrc Setting

    MTK_FEFM_Image imgAddr ;
    // WorkingBuffAddr Setting
    MTK_FEFM_WBuffer* WorkingBuffAddr ;

}MTKFeFmImageInfo;

typedef struct MTKFeFmResultInfoStruct
{	
	MINT32                  status;           
    MINT32                  ElapseTime;              // performance                              
}MTKFeFmResultInfo;

/*******************************************************************************
*
********************************************************************************/
class MTKFeFm {
public:
    static MTKFeFm* createInstance(DrvFeFmObject_e eobject);
    virtual void   destroyInstance(MTKFeFm* obj) = 0;

    virtual ~MTKFeFm();
    // Process Control
    virtual MRESULT Init(MUINT32 *InitInData, MUINT32 *InitOutData);    
    virtual MRESULT Main(void);                                         
    virtual MRESULT Reset(void);                                        

    // Feature Control
    virtual MRESULT FeatureCtrl(MUINT32 FeatureID, void* pParaIn, void* pParaOut);
private:

};

#endif