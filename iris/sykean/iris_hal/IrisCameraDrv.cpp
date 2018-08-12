/*
 * Copyright (c) 2017 SYKEAN Limited.
 *
 * All rights are reserved.
 * Proprietary and confidential.
 * Unauthorized copying of this file, via any medium is strictly prohibited.
 * Any use is subject to an appropriate license granted by SYKEAN Company.
 */

#define DEBUG_LOG_TAG "iris_hal"

#include <dlfcn.h>
#include "IrisCameraIF.h"
#include "sykeaniris_cam.h"

#include <mtkcam/feature/iris/IIrisCamera.h>
#include <mtkcam/feature/iris/utils/Debug.h>

#include <memory>
#include <condition_variable>

#ifdef __LP64__
#define MODULE_PATH "/system/vendor/lib64/libmtkcam_iris.so"
#else
#define MODULE_PATH "/system/vendor/lib/libmtkcam_iris.so"
#endif

using namespace Iris;

// ---------------------------------------------------------------------------

enum
{
    IRIS_CAMERA_STATE_NONE,
    IRIS_CAMERA_STATE_IDLE,
    IRIS_CAMERA_STATE_PREVIEW,
};

static MINT32 gCamState = IRIS_CAMERA_STATE_NONE;
static PreviewCallback gPreviewCallback;

static std::unique_ptr<IIrisCamera> gIrisCamera;
static std::mutex gCallbackLock;
static void *gIrisCameraLibHandle = nullptr;

// ---------------------------------------------------------------------------

static Result releaseIrisCamDev()
{
    AutoLog();

    gIrisCamera = nullptr;
    if (gIrisCameraLibHandle)
        dlclose(gIrisCameraLibHandle);
    gIrisCameraLibHandle = nullptr;

    return OK;
}

static Result createIrisCamDev()
{
    AutoLog();

    // the types of the class factories
    typedef IIrisCamera* create_t();

    void *gIrisCameraLibHandle = dlopen(MODULE_PATH, RTLD_LAZY);
    if (gIrisCameraLibHandle == nullptr)
    {
        IRIS_LOGE("cannot open iris camera");
        return NAME_NOT_FOUND;
    }

    // reset error
    dlerror();

    create_t* getIrisCamera =
        (create_t*)dlsym(gIrisCameraLibHandle, "getIrisCamera");
    const char *dlsym_error = dlerror();
    if (dlsym_error)
    {
        IRIS_LOGE("cannot load symbol create instance %s", dlsym_error);
        dlclose(gIrisCameraLibHandle);
        gIrisCameraLibHandle = nullptr;
        return NAME_NOT_FOUND;
    }
    IRIS_LOGI("loaded symbol create instance");

    gIrisCamera.reset(getIrisCamera());
    if (!gIrisCamera.get())
    {
        IRIS_LOGE("get iris camera failed");
        return NO_INIT;
    }

    return 0;
}

static inline IIrisCamera* getIrisCamDev()
{
    if (!gIrisCamera.get())
        createIrisCamDev();

    return gIrisCamera.get();
}

// ---------------------------------------------------------------------------

Result __attribute__((unused)) __addrCallback(const RawAddr& addr
                                                    __attribute__((unused)))
{
    AutoLog();

    Result ret = OK;

    do
    {
        std::lock_guard<std::mutex> _l(gCallbackLock);

        if (gPreviewCallback)
            gPreviewCallback(addr.addr, addr.attribute.length, addr.attribute.orientation);

        IIrisCamera *irisCamera(getIrisCamDev());
        if (nullptr == irisCamera)
        {
            IRIS_LOGE("IrisCamera get device fail");
            ret = UNKNOWN_ERROR;
            break;
        }

        ret = irisCamera->sendCommand(
                Command::RETURN_CB_DATA, IRIS_CALLBACK_ADDR, (intptr_t)&addr);
        if (OK != ret)
        {
            IRIS_LOGE("IrisCamera return callback data fail(%d)", ret);
            break;
        }
    } while (MFALSE);

    return ret;
}

Result __attribute__((unused)) __handleCallback(RawHandle handle
                                                     __attribute__((unused)))
{
    // do something
    return OK;
}

Result __attribute__((unused)) __bufferCallback(RawBuffer buffer
                                                     __attribute__((unused)))
{
    // do something
    return OK;
}

// ---------------------------------------------------------------------------

MINT32
IrisCamera_Open(MUINT32 initDev)
{
    AutoLog();

    //====== Local Variable ======
    Result camRet = OK;
    MINT32 ret = 0;

    do
    {
        if (IRIS_CAMERA_STATE_NONE != gCamState)
        {
            IRIS_LOGE("IrisCamera already open");
            ret = -1;
            break;
        }

        IIrisCamera *irisCamera(getIrisCamDev());
        if (nullptr == irisCamera)
        {
            IRIS_LOGE("IrisCamera get device fail");
            ret = -3;
            break;
        }

        camRet = irisCamera->open();
        if (OK != camRet)
        {
            IRIS_LOGE("IrisCamera open device fail(%d)", camRet);
            ret = -4;
            break;
        }

        camRet = irisCamera->init();
        if (OK != camRet)
        {
            IRIS_LOGE("IrisCamera int device fail(%d)", camRet);
            ret = -5;
            break;
        }

        // set Camera-ID
        camRet = irisCamera->sendCommand(
                Command::SET_SRC_DEV, (intptr_t)initDev);
        if (OK != camRet)
        {
            IRIS_LOGE("IrisCamera set device id fail(%d)", camRet);
            ret = -6;
            break;
        }

        gCamState = IRIS_CAMERA_STATE_IDLE;
        IRIS_LOGI("OK!");
    } while (MFALSE);

    return ret;
}

MINT32
IrisCamera_Close()
{
    AutoLog();

    //====== Local Variable ======
    Result camRet = OK;
    MINT32 ret = 0;

    if (IRIS_CAMERA_STATE_NONE == gCamState)
    {
        IRIS_LOGE("IrisCamera already close");
        ret = -1;
    }
    else
    {
        if (IRIS_CAMERA_STATE_PREVIEW == gCamState)
        {
            ret = IrisCamera_StopPreview();
            IRIS_LOGW("IrisCamera_StopPreview(): %d", ret);
        }

        IIrisCamera *irisCamera(getIrisCamDev());
        if (nullptr == irisCamera)
        {
            IRIS_LOGE("IrisCamera get device fail");
            ret = -2;
        }
        else
        {
            camRet = irisCamera->unInit();
            if (OK != camRet)
            {
                IRIS_LOGE("IrisCamera deInit device fail(%d)", camRet);
                ret = -3;
            }

            camRet = irisCamera->close();
            if (OK != camRet)
            {
                IRIS_LOGE("IrisCamera close device fail(%d)", camRet);
                ret = -4;
            }
        }

        if (OK != releaseIrisCamDev())
        {
            IRIS_LOGE("IrisCamera release device fail");
            ret = -5;
        }

        gCamState = IRIS_CAMERA_STATE_NONE;
        if (!ret)
        {
            IRIS_LOGI("OK!");
        }
    }

    return ret;
}

MINT32
IrisCamera_IrisSupport(MUINT32 *pIsSupport)
{
    AutoLog();

    //====== Local Variable ======
    MINT32 ret = 0;

    do
    {
        if (IRIS_CAMERA_STATE_IDLE != gCamState)
        {
            IRIS_LOGE("IrisCamera not open or already preview:State(%d)", gCamState);
            ret = -1;
            break;
        }

        if (NULL != pIsSupport)
        {
            *pIsSupport = 1; // always return support
        }
    } while (MFALSE);

    return ret;
}

MINT32
IrisCamera_StartPreview(PreviewCallback prvCb)
{
    AutoLog();

    //====== Local Variable ======
    Result camRet = OK;
    Configuration setConfig;
    //Configuration getConfig;
    MINT32 ret = 0;

    do
    {
        if (IRIS_CAMERA_STATE_IDLE != gCamState)
        {
            IRIS_LOGE("IrisCamera not open or already preview:State(%d)", gCamState);
            ret = -1;
            break;
        }

        IIrisCamera *irisCamera(getIrisCamDev());
        if (nullptr == irisCamera)
        {
            IRIS_LOGE("IrisCamera get device fail");
            ret = -2;
            break;
        }

        //====== Set Sensor Configuration ======
        setConfig.mode = Mode::IRIS;
        setConfig.size.w = IRIS_CAMERA_PREVIEW_WIDTH;
        setConfig.size.h = IRIS_CAMERA_PREVIEW_HEIGHT;
        camRet = irisCamera->sendCommand(
                Command::SET_SENSOR_CONFIG, (intptr_t)&setConfig);
        if (OK != camRet)
        {
            IRIS_LOGE("IrisCamera set device config fail(%d)", camRet);
            ret = -3;
            break;
        }

        //====== Get Sensor Configuration ======
        //camRet = irisCamera->sendCommand(GET_SENSOR_CONFIG, (intptr_t)&getConfig);
        //if (OK != camRet)
        //{
        //    IRIS_LOGE("IrisCamera get device config fail(%d)", camRet);
        //    ret = -3;
        //    break;
        //}

        //====== Register addr-callback before the streaming on ======
        Callback cbAddr = Callback::createCallback<IRIS_PFN_CALLBACK_ADDR>(
                IRIS_CALLBACK_ADDR, __addrCallback);

        camRet = irisCamera->sendCommand(
                Command::REGISTER_CB_FUNC, (intptr_t)&cbAddr);
        if (OK != camRet)
        {
            IRIS_LOGE("IrisCamera register callback fail(%d)", camRet);
            ret = -4;
            break;
        }

        //====== Start Camera Preview ======
        camRet = irisCamera->sendCommand(Command::STREAMING_ON);
        if (OK != camRet)
        {
            IRIS_LOGE("IrisCamera start preview fail(%d)", camRet);
            ret = -5;
            break;
        }

        gPreviewCallback = prvCb;
        gCamState = IRIS_CAMERA_STATE_PREVIEW;
    } while (MFALSE);

    return ret;
}

MINT32
IrisCamera_StopPreview()
{
    AutoLog();

    //====== Local Variable ======
    Result camRet = OK;
    MINT32 ret = 0;

    do
    {
        if (IRIS_CAMERA_STATE_PREVIEW != gCamState)
        {
            IRIS_LOGE("IrisCamera not preview, can't stop");
            ret = -1;
            break;
        }

        IIrisCamera *irisCamera(getIrisCamDev());
        if (nullptr == irisCamera)
        {
            IRIS_LOGE("IrisCamera get device fail");
            ret = -2;
            break;
        }

        //====== Stop Camera Preview ======
        camRet = irisCamera->sendCommand(Command::STREAMING_OFF);
        if (OK != camRet)
        {
            IRIS_LOGE("IrisCamera stop preview fail(%d)", camRet);
            ret = -3;
            break;
        }

        gPreviewCallback = nullptr;
        gCamState = IRIS_CAMERA_STATE_IDLE;
    } while (MFALSE);

    return ret;
}

MINT32
IrisCamera_SetGain(MUINT32 gainValue)
{
    AutoLog();

    //====== Local Variable ======
    Result camRet = OK;
    MINT32 ret = 0;

    do
    {
        if (IRIS_CAMERA_STATE_PREVIEW != gCamState)
        {
            IRIS_LOGE("IrisCamera not preview, can't set gain");
            ret = -1;
            break;
        }

        IIrisCamera *irisCamera(getIrisCamDev());
        if (nullptr == irisCamera)
        {
            IRIS_LOGE("IrisCamera get device fail");
            ret = -2;
            break;
        }

        //====== Set Camera Gain ======
        camRet = irisCamera->sendCommand(
                Command::SET_GAIN_VALUE, (intptr_t)gainValue);
        if (OK != camRet)
        {
            IRIS_LOGE("IrisCamera set gain value fail(%d)", camRet);
            ret = -3;
            break;
        }
    } while (MFALSE);

    return ret;
}

MINT32
IrisCamera_SetExposure(MUINT32 expValue)
{
    AutoLog();

    //====== Local Variable ======
    Result camRet = OK;
    MINT32 ret = 0;

    do
    {
        if (IRIS_CAMERA_STATE_PREVIEW != gCamState)
        {
            IRIS_LOGE("IrisCamera not preview, can't set exposure");
            ret = -1;
            break;
        }

        IIrisCamera *irisCamera(getIrisCamDev());
        if (nullptr == irisCamera)
        {
            IRIS_LOGE("IrisCamera get device fail");
            ret = -2;
            break;
        }

        //====== Set Camera Exposure ======
        camRet = irisCamera->sendCommand(
                Command::SET_SHUTTER_TIME, (intptr_t)expValue);
        if (OK != camRet)
        {
            IRIS_LOGE("IrisCamera set shutter time fail(%d)", camRet);
            ret = -3;
            break;
        }
    } while (MFALSE);

    return ret;
}
