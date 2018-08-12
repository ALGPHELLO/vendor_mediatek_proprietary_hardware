/*
 * Copyright (C) 2013-2016, Shenzhen Huiding Technology Co., Ltd.
 * All Rights Reserved.
 */
#include "ftm_factory_test.h"
#include "gf_fingerprint.h"
#include "string.h"
#include <stdlib.h>

#define LOG_TAG "[GF_HAL][ftm_test_result_checker]"

#ifndef NULL
#define NULL 0
#endif

#define  TEST_NONE  0
#define  TEST_SPI  1
#define  TEST_PIXEL  2
#define  TEST_RESET_PIN  3
#define  TEST_BAD_POINT  4
#define  TEST_PERFORMANCE 5
#define  TEST_CAPTURE  6
#define  TEST_ALGO  7
#define  TEST_FW_VERSION  8
#define  TEST_SENSOR_CHECK  9
#define  TEST_BIO_ASSAY  10
#define  TEST_MAX  11

static char g_spiFwVersion[MAX_LENGTH];
static int g_chipId;
static int g_badBointNum = 0;
static char g_fwVersion[MAX_LENGTH];
static long g_totalTime = TEST_PERFORMANCE_TOTAL_TIME;
static int g_imageQuality  = TEST_CAPTURE_VALID_IMAGE_QUALITY_THRESHOLD;
static int g_validArea  = TEST_CAPTURE_VALID_IMAGE_AREA_THRESHOLD;
static short g_avgDiffVal;
static int g_badPixelNum = 0;
static int g_localBadPixelNum = 0;
static int g_localBigBadPixelNum = 0;
static int g_localSmallBadPixelNum = 0;
static float g_allTiltAngle = 0;
static float g_blockTiltAngleMax = 0;
static short g_localWorst = 0;
static int g_singular = 0;
static int g_osdUntoucded = 0;
static int g_osdTouched = 0;

void  initThreshold(int chipType) {
    switch (chipType) {
        case GF_CHIP_318M:
        case GF_CHIP_3118M:
        case GF_CHIP_518M:
        case GF_CHIP_5118M:
            strncpy(g_spiFwVersion,TEST_SPI_GFX18, sizeof(TEST_SPI_GFX18));
            strncpy(g_fwVersion ,TEST_FW_VERSION_GFX18, sizeof(TEST_FW_VERSION_GFX18));

            g_badBointNum = TEST_SENSOR_BAD_POINT_COUNT_OSWEGO;

            g_badPixelNum = TEST_BAD_POINT_BAD_PIXEL_NUM_OSWEGO;
            g_localBigBadPixelNum = TEST_BAD_POINT_LOCAL_BIG_BAD_PIXEL_NUM_OSWEGO;
            g_localSmallBadPixelNum = TEST_BAD_POINT_LOCAL_SMALL_BAD_PIXEL_NUM_OSWEGO;
            g_avgDiffVal = TEST_BAD_POINT_AVG_DIFF_VAL_OSWEGO;
            g_allTiltAngle = TEST_BAD_POINT_ALL_TILT_ANGLE_OSWEGO;
            g_blockTiltAngleMax = TEST_BAD_POINT_BLOCK_TILT_ANGLE_MAX_OSWEGO;
            break;

        case GF_CHIP_316M:
        case GF_CHIP_516M:
        case GF_CHIP_816M:
            strncpy(g_spiFwVersion ,TEST_SPI_GFX16, sizeof(TEST_SPI_GFX16));
            strncpy(g_fwVersion, TEST_FW_VERSION_GFX16, sizeof(TEST_FW_VERSION_GFX16));
            g_badBointNum = TEST_SENSOR_BAD_POINT_COUNT_OSWEGO;

            g_badPixelNum = TEST_BAD_POINT_BAD_PIXEL_NUM_OSWEGO;
            g_localBigBadPixelNum = TEST_BAD_POINT_LOCAL_BIG_BAD_PIXEL_NUM_OSWEGO;
            g_localSmallBadPixelNum = TEST_BAD_POINT_LOCAL_SMALL_BAD_PIXEL_NUM_OSWEGO;
            g_avgDiffVal = TEST_BAD_POINT_AVG_DIFF_VAL_OSWEGO;
            g_allTiltAngle = TEST_BAD_POINT_ALL_TILT_ANGLE_OSWEGO;
            g_blockTiltAngleMax = TEST_BAD_POINT_BLOCK_TILT_ANGLE_MAX_OSWEGO;
            break;

        case GF_CHIP_3208:
            g_chipId = TEST_SPI_CHIP_ID_MILAN_F;
            g_badBointNum = TEST_SENSOR_BAD_POINT_COUNT_MILAN_F;

            g_badPixelNum = TEST_BAD_POINT_TOTAL_BAD_PIXEL_NUM_MILAN_F;
            g_localBadPixelNum = TEST_BAD_POINT_LOCAL_BAD_PIXEL_NUM_MILAN_F;
            g_localWorst = TEST_BAD_POINT_LOCAL_WORST_MILAN_F;

            break;

        case GF_CHIP_3206:
            g_chipId = TEST_SPI_CHIP_ID_MILAN_G;
            g_badBointNum = TEST_SENSOR_BAD_POINT_COUNT_MILAN_G;

            g_badPixelNum = TEST_BAD_POINT_TOTAL_BAD_PIXEL_NUM_MILAN_G;
            g_localBadPixelNum = TEST_BAD_POINT_LOCAL_BAD_PIXEL_NUM_MILAN_G;
            g_localWorst = TEST_BAD_POINT_LOCAL_WORST_MILAN_G;
            break;

        case GF_CHIP_3266:
            g_chipId = TEST_SPI_CHIP_ID_MILAN_E;
            g_badBointNum = TEST_SENSOR_BAD_POINT_COUNT_MILAN_E;

            g_badPixelNum = TEST_BAD_POINT_TOTAL_BAD_PIXEL_NUM_MILAN_E;
            g_localBadPixelNum = TEST_BAD_POINT_LOCAL_BAD_PIXEL_NUM_MILAN_E;
            g_localWorst = TEST_BAD_POINT_LOCAL_WORST_MILAN_E;
            break;

        case GF_CHIP_3288:
            g_chipId = TEST_SPI_CHIP_ID_MILAN_L;
            g_badBointNum = TEST_SENSOR_BAD_POINT_COUNT_MILAN_L;

            g_badPixelNum = TEST_BAD_POINT_TOTAL_BAD_PIXEL_NUM_MILAN_L;
            g_localBadPixelNum = TEST_BAD_POINT_LOCAL_BAD_PIXEL_NUM_MILAN_L;
            g_localWorst = TEST_BAD_POINT_LOCAL_WORST_MILAN_L;
            break;

        case GF_CHIP_3228:
            g_chipId = TEST_SPI_CHIP_ID_MILAN_K;
            g_badBointNum = TEST_SENSOR_BAD_POINT_COUNT_MILAN_K;

            g_badPixelNum = TEST_BAD_POINT_TOTAL_BAD_PIXEL_NUM_MILAN_K;
            g_localBadPixelNum = TEST_BAD_POINT_LOCAL_BAD_PIXEL_NUM_MILAN_K;
            g_localWorst = TEST_BAD_POINT_LOCAL_WORST_MILAN_K;
            break;

        case GF_CHIP_3226:
            g_chipId = TEST_SPI_CHIP_ID_MILAN_J;
            g_badBointNum = TEST_SENSOR_BAD_POINT_COUNT_MILAN_J;

            g_badPixelNum = TEST_BAD_POINT_TOTAL_BAD_PIXEL_NUM_MILAN_J;
            g_localBadPixelNum = TEST_BAD_POINT_LOCAL_BAD_PIXEL_NUM_MILAN_J;
            g_localWorst = TEST_BAD_POINT_LOCAL_WORST_MILAN_J;
            break;

        case GF_CHIP_3258:
            g_chipId = TEST_SPI_CHIP_ID_MILAN_H;
            g_badBointNum = TEST_SENSOR_BAD_POINT_COUNT_MILAN_H;

            g_badPixelNum = TEST_BAD_POINT_TOTAL_BAD_PIXEL_NUM_MILAN_H;
            g_localBadPixelNum = TEST_BAD_POINT_LOCAL_BAD_PIXEL_NUM_MILAN_H;
            g_localWorst = TEST_BAD_POINT_LOCAL_WORST_MILAN_H;
            break;

        case GF_CHIP_5206:
        case GF_CHIP_5216:
            strncpy(g_spiFwVersion, TEST_SPI_FW_VERSION_MILAN_A,
                    sizeof(TEST_SPI_FW_VERSION_MILAN_A));
            strncpy(g_fwVersion, TEST_FW_VERSION_MILAN_A,
                    sizeof(TEST_FW_VERSION_MILAN_A));
            g_badBointNum = TEST_SENSOR_BAD_POINT_COUNT_MILAN_A;
            g_totalTime = TEST_PERFORMANCE_TOTAL_TIME_MILAN_A;

            g_badPixelNum = TEST_BAD_POINT_TOTAL_BAD_PIXEL_NUM_MILAN_A;
            g_localBadPixelNum = TEST_BAD_POINT_LOCAL_BAD_PIXEL_NUM_MILAN_A;
            g_localWorst = TEST_BAD_POINT_LOCAL_WORST_MILAN_A;
            g_singular = TEST_BAD_POINT_SINGULAR_MILAN_A;

            if (chipType == GF_CHIP_5206) {
                g_osdUntoucded = TEST_BIO_THRESHOLD_UNTOUCHED_MILAN_A;
                g_osdTouched = TEST_BIO_THRESHOLD_TOUCHED_MILAN_A;
            }
            break;

        case GF_CHIP_5208:
        case GF_CHIP_5218:
            strncpy(g_spiFwVersion, TEST_SPI_FW_VERSION_MILAN_C,
                    sizeof(TEST_SPI_FW_VERSION_MILAN_C));
            strncpy(g_fwVersion, TEST_FW_VERSION_MILAN_C,
                    sizeof(TEST_FW_VERSION_MILAN_C));
            g_badBointNum = TEST_SENSOR_BAD_POINT_COUNT_MILAN_C;
            g_totalTime = TEST_PERFORMANCE_TOTAL_TIME_MILAN_C;

            g_badPixelNum = TEST_BAD_POINT_TOTAL_BAD_PIXEL_NUM_MILAN_C;
            g_localBadPixelNum = TEST_BAD_POINT_LOCAL_BAD_PIXEL_NUM_MILAN_C;
            g_localWorst = TEST_BAD_POINT_LOCAL_WORST_MILAN_C;
            g_singular = TEST_BAD_POINT_SINGULAR_MILAN_C;

            if (chipType == GF_CHIP_5208) {
                g_osdUntoucded = TEST_BIO_THRESHOLD_UNTOUCHED_MILAN_C;
                g_osdTouched = TEST_BIO_THRESHOLD_TOUCHED_MILAN_C;
            }

            break;

        default:
            break;
    }
}


uint8_t checkErrcode(token_map_t* result,uint32_t count) {
    int errorCode = GF_FINGERPRINT_ERROR_VENDOR_BASE;
    int i = 0;

    for (i = 0; i < count; i++){
        if (TEST_TOKEN_ERROR_CODE == (result + i)->token){
            errorCode =  (result + i)->value.uint32_value;
            break;
        }
    }

    return (errorCode == 0);
}

uint8_t checkSpiTestResult(token_map_t* result,uint32_t count) {
    return checkErrcode(result,count);
}

uint8_t checkSpiTestResult2(int errCode, char*  fwVersion, int ChipId) {
    return (errCode == 0);
}

uint8_t checkInterruptPinTestReuslt2(int errCode, int interruptFlag) {
    return (errCode == 0) && (interruptFlag > 0);
}

uint8_t checkInterruptPinTestReuslt(token_map_t* result,uint32_t count) {
    if (checkErrcode(result,count)) {
        int errCode = 0;

        int i = 0;

        for (i = 0; i < count; i++){
            if (TEST_TOKEN_ERROR_CODE == (result + i)->token){
                errCode =  (result + i)->value.uint32_value;
                break;
            }
        }

        if (checkInterruptPinTestReuslt2(errCode, 1)) {
            return 1;
        }
    }

    return 0;
}

uint8_t checkResetPinTestReuslt2(int errCode, int resetFlag) {
    return (errCode == 0) && (resetFlag > 0);
}

uint8_t checkResetPinTestReuslt(token_map_t* result,uint32_t count) {
    if (checkErrcode(result,count)) {
        int resetFlag = 0;

        int i = 0;

        for (i = 0; i < count; i++){
            if (TEST_TOKEN_RESET_FLAG == (result + i)->token){
                resetFlag =  (result + i)->value.uint32_value;
                break;
            }
        }

        if (checkResetPinTestReuslt2(0, resetFlag)) {
            return 1;
        }
    }

    return 0;
}



uint8_t checkPixelTestResult2(int errCode, int badPixelNum) {
    return (errCode == 0) & (badPixelNum <= g_badBointNum);
}

uint8_t checkPixelTestResult(token_map_t* result,uint32_t count) {
    if (checkErrcode(result,count)) {
        int badPixelNum = 999;
        int i = 0;

        for (i = 0; i < count; i++){
            if (TEST_TOKEN_BAD_PIXEL_NUM == (result + i)->token){
                badPixelNum =  (result + i)->value.uint32_value;
                break;
            }
        }

        if (checkPixelTestResult2(0, badPixelNum)) {
            return 1;
        }
    }
    return 0;
}



uint8_t checkFwVersionTestResult(token_map_t* result,uint32_t count) {
    return checkErrcode(result,count);
}

uint8_t checkFwVersionTestResult2(int errCode, char*  fwVersion) {
    return (errCode == 0);
}

uint8_t checkPerformanceTestResult2(int errCode, int totalTime) {
    return (errCode == 0) && (totalTime < g_totalTime);
}

uint8_t checkPerformanceTestResult(token_map_t* result,uint32_t count) {
    if (checkErrcode(result,count)) {
        int totalTime = 0;
        int i = 0;

        for (i = 0; i < count; i++){
            if (TEST_TOKEN_TOTAL_TIME == (result + i)->token){
                totalTime =  (result + i)->value.uint32_value;
                break;
            }
        }

        if (checkPerformanceTestResult2(0, totalTime)) {
            return 1;
        }
    }

    return 0;
}

uint8_t checkCaptureTestResult2(int errCode, int imageQuality, int validArea) {
    return (errCode == 0) && (imageQuality >= g_imageQuality)
            && (validArea >= g_validArea);
}

uint8_t checkCaptureTestResult(token_map_t* result,uint32_t count) {
    if (checkErrcode(result,count)) {
        int imageQuality = 0;
        int validArea = 0;

        int i = 0;

        for (i = 0; i < count; i++){
            if (TEST_TOKEN_IMAGE_QUALITY == (result + i)->token){
                imageQuality =  (result + i)->value.uint32_value;
            }else if (TEST_TOKEN_VALID_AREA == (result + i)->token){
                validArea =  (result + i)->value.uint32_value;
            }
        }

        if (checkCaptureTestResult2(0, imageQuality, validArea)) {
            return 1;
        }
    }

    return 0;
}



uint8_t checkAlgoTestResult(token_map_t* result,uint32_t count) {
    return checkErrcode(result,count);
}

uint8_t checkAlgoTestResult2(int errCode) {
    return (errCode == 0);
}

uint8_t checkBadPointTestResult(token_map_t* result,uint32_t count) {
    return checkErrcode(result,count);
}

uint8_t checkSensorValidityResult(token_map_t* result,uint32_t count) {
    return checkErrcode(result,count);
}

uint8_t checkBadPointTestResult2(int errCode, int badPixelNum, int localBadPixelNum,
        float allTiltAngle, float blockTiltAngleMax, short avgDiffVal, short localWorst,
        int singular) {
    return (errCode == 0);
}

uint8_t checkBioTestResultWithoutTouched(token_map_t* result,uint32_t count) {
    return checkErrcode(result,count);
}

uint8_t checkBioTestResultWithoutTouched2(int errCode, int base, int avg) {
    return (errCode == 0);
}

uint8_t checkBioTestResultWithTouched(token_map_t* result,uint32_t count) {
    return checkErrcode(result,count);
}

uint8_t checkBioTestResultWithTouched2(int errCode, int base, int avg) {
    return (errCode == 0);
}

static int TEST_ITEM_OSWEGO[] = {
        TEST_SPI, /**/
        TEST_PIXEL, /**/
        TEST_RESET_PIN, /**/
        TEST_BAD_POINT, /**/
        TEST_PERFORMANCE, /**/
        TEST_CAPTURE, /**/
        TEST_ALGO, /**/
        TEST_FW_VERSION
};


void initOswegoChecker(int chipType) {
    initThreshold(chipType);
}

uint8_t Oswego_checkSpiTestResult2(int errCode, char* fwVersion, int ChipId) {

    return (errCode == 0)
            && (fwVersion != NULL && strstr(fwVersion,g_spiFwVersion) == fwVersion);
}


uint8_t Oswego_checkSpiTestResult(token_map_t* result,uint32_t count) {
    if (checkSpiTestResult(result,count)) {
        char* fwVersion = NULL;
        int i = 0;

        for (i = 0; i < count; i++){
            if (TEST_TOKEN_FW_VERSION == (result + i)->token){
                fwVersion =  (result + i)->value.buffer;
                break;
            }
        }

        if (Oswego_checkSpiTestResult2(0, fwVersion, 0)) {
            return 1;
        }
    }

    return 0;
}

uint8_t Oswego_checkFwVersionTestResult2(int errCode, char* fwVersion) {
    return (errCode == 0) && (fwVersion != NULL)
            && (strstr(fwVersion,g_fwVersion) == fwVersion);
}

uint8_t Oswego_checkFwVersionTestResult(token_map_t* result,uint32_t count) {

    if (checkFwVersionTestResult(result,count)) {
        char* fwVersion = NULL;
        int i = 0;

        for (i = 0; i < count; i++){
            if (TEST_TOKEN_FW_VERSION == (result + i)->token){
                fwVersion =  (result + i)->value.buffer;
                break;
            }
        }

        if (Oswego_checkFwVersionTestResult2(0, fwVersion)) {
            return 1;
        }
    }
    return 0;
}

uint8_t Oswego_checkBadPointTestResult2(int errCode, int badPixelNum, int localBigBadPixelNum, int localSmallBadPixelNum,
        float allTiltAngle, float blockTiltAngleMax, short avgDiffVal, short localWorst,
        int singular) {
    return (errCode == 0) & (badPixelNum < g_badPixelNum
            && localBigBadPixelNum < g_localBigBadPixelNum
            && localSmallBadPixelNum < g_localSmallBadPixelNum
            && avgDiffVal > g_avgDiffVal
            && allTiltAngle < g_allTiltAngle
            && blockTiltAngleMax < g_blockTiltAngleMax);
}

uint8_t Oswego_checkBadPointTestResult(token_map_t* result,uint32_t count) {
    if (checkBadPointTestResult(result,count)) {
        short avgDiffVal = 0;
        int badPixelNum = 0;
        float allTiltAngle = 0;
        float blockTiltAngleMax = 0;
        int localBigBadPixelNum = 0;
        int localSmallBadPixelNum = 0;

        int i = 0;

        for (i = 0; i < count; i++){
            switch ( (result + i)->token){
            case TEST_TOKEN_AVG_DIFF_VAL:
                avgDiffVal =  (result + i)->value.uint16_value;
                break;
            case TEST_TOKEN_BAD_PIXEL_NUM:
                badPixelNum =  (result + i)->value.uint32_value;
                break;
            case TEST_TOKEN_ALL_TILT_ANGLE:
                allTiltAngle =  (result + i)->value.float_value;
                break;
            case TEST_TOKEN_BLOCK_TILT_ANGLE_MAX:
                blockTiltAngleMax =  (result + i)->value.float_value;
                break;
            case TEST_TOKEN_LOCAL_BIG_BAD_PIXEL_NUM:
                localBigBadPixelNum = (result + i)->value.uint32_value;
                break;
            case TEST_TOKEN_LOCAL_SMALL_BAD_PIXEL_NUM:
                localSmallBadPixelNum = (result + i)->value.uint32_value;
                break;
            default:
                break;
            }
        }

        if (Oswego_checkBadPointTestResult2(0, badPixelNum, localBigBadPixelNum, localSmallBadPixelNum, 0,
                0, avgDiffVal, (short) 0, 0)) {
            return 1;
        }
    }

    return 0;
}


static int TEST_ITEM_MILANA[] = { //
        TEST_SPI, /**/
        TEST_PIXEL, /**/
        TEST_RESET_PIN, /**/
        TEST_FW_VERSION, /**/
        TEST_PERFORMANCE, /**/
        TEST_BAD_POINT, /**/
        TEST_CAPTURE, /**/
        TEST_BIO_ASSAY, /**/
        TEST_ALGO /**/
};

void initMilanASeriesChecker(int chipType) {
    initThreshold(chipType);
}

uint8_t MilanASeries_checkSpiTestResult2(int errCode, char* fwVersion, int ChipId) {

    return (errCode == 0)
            && (fwVersion != NULL && strstr(fwVersion,g_spiFwVersion) == fwVersion);
}


uint8_t MilanASeries_checkSpiTestResult(token_map_t* result,uint32_t count) {
    if (checkSpiTestResult(result,count)) {
        char* fwVersion = NULL;
        int i = 0;

        for (i = 0; i < count; i++){
            if (TEST_TOKEN_FW_VERSION == (result + i)->token){
                fwVersion =  (result + i)->value.buffer;
                break;
            }
        }

        if (MilanASeries_checkSpiTestResult2(0, fwVersion, 0)) {
            return 1;
        }
    }

    return 0;
}


uint8_t MilanASeries_checkFwVersionTestResult2(int errCode, char* fwVersion) {
    return (errCode == 0) && (fwVersion != NULL)
            && (strstr(fwVersion,g_fwVersion) == fwVersion);
}

uint8_t MilanASeries_checkFwVersionTestResult(token_map_t* result,uint32_t count) {

    if (checkFwVersionTestResult(result,count)) {
        char* fwVersion = NULL;
        int i = 0;

        for (i = 0; i < count; i++){
            if (TEST_TOKEN_FW_VERSION == (result + i)->token){
                fwVersion =  (result + i)->value.buffer;
                break;
            }
        }

        if (MilanASeries_checkFwVersionTestResult2(0, fwVersion)) {
            return 1;
        }
    }
    return 0;
}


uint8_t MilanASeries_checkBioTestResultWithTouched2(int errCode, int base, int avg) {
    return (errCode == 0) && (abs(base - avg) > g_osdTouched);
}



uint8_t MilanASeries_checkBioTestResultWithTouched(token_map_t* result,uint32_t count) {

    if (checkBioTestResultWithTouched(result,count)) {
        int baseValue = 0;
        int avgValue = 0;
        int i = 0;

        for (i = 0; i < count; i++){
            if (TEST_TOKEN_HBD_BASE == (result + i)->token){
                baseValue =  (result + i)->value.uint32_value;
            }else if (TEST_TOKEN_HBD_AVG == (result + i)->token){
                avgValue =  (result + i)->value.uint32_value;
            }
        }

        if (MilanASeries_checkBioTestResultWithTouched2(0, baseValue, avgValue)) {
            return 1;
        }
    }

    return 0;
}


uint8_t MilanASeries_checkBioTestResultWithoutTouched2(int errCode, int base, int avg) {
    return (errCode == 0) && (abs(base - avg) < g_osdUntoucded);
}

uint8_t MilanASeries_checkBioTestResultWithoutTouched(token_map_t* result,uint32_t count) {

    if (checkBioTestResultWithTouched(result,count)) {
        int baseValue = 0;
        int avgValue = 0;
        int i = 0;

        for (i = 0; i < count; i++){
            if (TEST_TOKEN_HBD_BASE == (result + i)->token){
                baseValue =  (result + i)->value.uint32_value;
            }else if (TEST_TOKEN_HBD_AVG == (result + i)->token){
                avgValue =  (result + i)->value.uint32_value;
            }
        }

        if (MilanASeries_checkBioTestResultWithoutTouched2(0, baseValue, avgValue)) {
            return 1;
        }
    }
    return 0;
}

uint8_t MilanASeries_checkBadPointTestResult(token_map_t* result,uint32_t count) {
    return checkErrcode(result,count);
}




static int TEST_ITEM_MILAN_F_SERIES[] = { //
        TEST_SPI, /**/
        TEST_PIXEL, /**/
        TEST_RESET_PIN, /**/
        TEST_BAD_POINT, /**/
        TEST_PERFORMANCE, /**/
        TEST_CAPTURE, /**/
        TEST_ALGO /**/
};


void initMilanFSeriesChecker(int chipType) {
    initThreshold(chipType);
}

uint8_t MilanFSeries_checkSpiTestResult2(int errCode, char* fwVersion, int chipId) {
    return (errCode == 0) && (chipId == g_chipId);
}

uint8_t MilanFSeries_checkSpiTestResult(token_map_t* result,uint32_t count) {
    if (checkSpiTestResult(result,count)) {
        int chipID = 0;
        int i = 0;
        char*  chip = NULL;
        uint32_t size = 0;
        for (i = 0; i < count; i++){
            if (TEST_TOKEN_CHIP_ID == (result + i)->token){
                chip =  (result + i)->value.buffer;
                size =  (result + i)->buffer_size;
                break;
            }
        }

        if (chip != NULL && size >= 4) {
            chipID = decodeInt32(chip, 0) >> 8;
        }

        if (MilanFSeries_checkSpiTestResult2(0, NULL, chipID)) {
            return 1;
        }
    }

    return 0;
}


uint8_t MilanFSeries_checkBadPointTestResult2(int errCode, int badPixelNum, int localBadPixelNum,
        float allTiltAngle, float blockTiltAngleMax, short avgDiffVal, short localWorst) {
    return (errCode == 0)
            && (badPixelNum < g_badPixelNum
            && localBadPixelNum < g_localBadPixelNum
            && localWorst < g_localWorst);
}


uint8_t MilanFSeries_checkBadPointTestResult(token_map_t* result,uint32_t count) {
    if (checkBadPointTestResult(result,count)) {

        int badPixelNum = 0;
        int localBadPixelNum = 0;
        short localWorst = 0;
        int i = 0;

        for (i = 0; i < count; i++){
            if (TEST_TOKEN_BAD_PIXEL_NUM == (result + i)->token){
                badPixelNum =  (result + i)->value.uint32_value;
            }else if (TEST_TOKEN_LOCAL_BAD_PIXEL_NUM == (result + i)->token){
                localBadPixelNum =  (result + i)->value.uint32_value;
            }else if (TEST_TOKEN_LOCAL_WORST == (result + i)->token){
                localWorst =  (result + i)->value.uint16_value;
            }
        }

        if (MilanFSeries_checkBadPointTestResult2(0, badPixelNum, localBadPixelNum, 0, 0, (short) 0,
                localWorst)) {
            return 1;
        }
    }

    return 0;
}

uint8_t MilanFSeries_checkSensorValidity(token_map_t* result,uint32_t count) {
    if (checkSensorValidityResult(result,count)) {
        int i = 0;
        for (;i < count;i++) {
            if(TEST_TOKEN_SENSOR_VALIDITY == (result + i)->token){
                return (uint8_t)(result + i)->value.int32_value;
            }
        }
    }
    return 0;
}
