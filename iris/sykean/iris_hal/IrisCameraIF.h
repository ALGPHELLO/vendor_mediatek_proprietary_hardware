/*
 * Copyright (c) 2017 SYKEAN Limited.
 *
 * All rights are reserved.
 * Proprietary and confidential.
 * Unauthorized copying of this file, via any medium is strictly prohibited.
 * Any use is subject to an appropriate license granted by SYKEAN Company.
 */

#ifndef _SYKEAN_HARDWARE_INCLUDE_IRIS_CAMERAIF_H_
#define _SYKEAN_HARDWARE_INCLUDE_IRIS_CAMERAIF_H_

#include "IrisType.h"

#include <functional>

#define IRIS_CAMERA_PREVIEW_WIDTH   (1920)
#define IRIS_CAMERA_PREVIEW_HEIGHT  (1080)

// ---------------------------------------------------------------------------

typedef std::function<MVOID(MVOID*, long, MINT32)> PreviewCallback;

// ---------------------------------------------------------------------------

MINT32 IrisCamera_Open(MUINT32 initDev); // main:0x01, sub:0x02, main2:0x08
MINT32 IrisCamera_Close();
MINT32 IrisCamera_IrisSupport(MUINT32 *pIsSupport);
MINT32 IrisCamera_StartPreview(PreviewCallback prvCb);
MINT32 IrisCamera_StopPreview();
MINT32 IrisCamera_SetGain(MUINT32 gainValue);
MINT32 IrisCamera_SetExposure(MUINT32 expValue);

#endif  //_SYKEAN_HARDWARE_INCLUDE_IRIS_CAMERAIF_H_

