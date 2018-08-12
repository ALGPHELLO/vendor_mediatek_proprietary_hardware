/*
 * Copyright (C) 2013-2016, Shenzhen Huiding Technology Co., Ltd.
 * All Rights Reserved.
 */
#include "ftm_factory_test.h"
#include "gf_error.h"
#include "gf_type_define.h"
#include <string.h>
#include <stdlib.h>


#define LOG_TAG "[GF_HAL][ftm_test_result_parser]"
token_map_t g_token_map[MAX_SIZE];
uint32_t g_token_count = 0;

int g_ChipType = GF_CHIP_318M;
int g_ChipSeries = GF_OSWEGO_M;
int g_MaxFingers = 32;
int g_MaxFingersPerUser = 5;
int g_SupportKeyMode = 0;
int g_SupportFFMode = 1;
int g_SupportPowerKeyFeature = 0;
int g_ForbiddenUntrustedEnroll = 0;
int g_ForbiddenEnrollDuplicateFingers = 0;
int g_SupportBioAssay = 0;
int g_SupportPerformanceDump = 0;

int g_SupportNavMode = GF_NAV_MODE_NONE;

int g_EnrollingMinTemplates = 8;

int g_ValidImageQualityThreshold = 15;
int g_ValidImageAreaThreshold = 65;
int g_DuplicateFingerOverlayScore = 70;
int g_IncreaseRateBetweenStitchInfo = 15;

int g_SupportImageRescan = 1;
int g_RescanImageQualityThreshold = 10;
int g_RescanImageAreaThreshold = 60;
int g_RescanRetryCount = 1;

int g_ScreenOnAuthenticateFailRetryCount = 1;
int g_ScreenOffAuthenticateFailRetryCount = 1;

int g_ReissueKeyDownWhenEntryFfMode = 0;
int g_ReissueKeyDownWhenEntryImageMode = 1;

int g_SupportSensorBrokenCheck = 0;
int g_BrokenPixelThresholdForDisableSensor = 600;
int g_BrokenPixelThresholdForDisableStudy = 300;

int g_BadPointTestMaxFrameNumber = 2;

int g_ReportKeyEventOnlyEnrollAuthenticate = 0;

int g_RequireDownAndUpInPairsForImageMode = 1;
int g_RequireDownAndUpInPairsForFFMode = 0;
int g_RequireDownAndUpInPairsForKeyMode = 1;
int g_RequireDownAndUpInPairsForNavMode = 1;
int g_SupportSetSpiSpeedInTEE = 1;

uint32_t decodeInt32(uint8_t* result, uint32_t offset) {
    return (result[offset] & 0xff) | ((result[offset + 1] & 0xff) << 8)
            | ((result[offset + 2] & 0xff) << 16)
            | ((result[offset + 3] & 0xff) << 24);
}

uint16_t decodeInt16(uint8_t* result, uint32_t offset) {
    return (uint16_t) ((result[offset] & 0xff) | ((result[offset + 1] & 0xff) << 8));
}

uint8_t decodeInt8(uint8_t* result, uint32_t offset) {
    return result[offset];
}

float decodeFloat(uint8_t* result, uint32_t offset, uint32_t size) {
    uint32_t value = (result[offset] & 0xff) | ((result[offset + 1] & 0xff) << 8)
            | ((result[offset + 2] & 0xff) << 16)
            | ((result[offset + 3] & 0xff) << 24);
    float fvalue = 0;

    memcpy(&fvalue,&value,sizeof(value));
    return fvalue;
}

double decodeDouble(uint8_t* result, uint32_t offset, uint32_t size) {
    uint64_t  value = (result[offset] & 0xff)
            | (((uint64_t) result[offset + 1] & 0xff) << 8)
            | (((uint64_t) result[offset + 2] & 0xff) << 16)
            | (((uint64_t) result[offset + 3] & 0xff) << 24)
            | (((uint64_t) result[offset + 4] & 0xff) << 32)
            | (((uint64_t) result[offset + 5] & 0xff) << 40)
            | (((uint64_t) result[offset + 6] & 0xff) << 48)
            | (((uint64_t) result[offset + 7] & 0xff) << 56);
    double dvalue = 0;
    memcpy(&dvalue,&value,sizeof(value));
    return dvalue;
}

void parser(uint8_t* result,uint32_t len) {
    uint32_t offset = 0;
    uint32_t count = 0;

    LOG_D(LOG_TAG, "[%s] enter", __func__);
    g_token_count = 0;
    memset(&g_token_map,0,sizeof(g_token_map));

    for (offset = 0; offset < len;) {

        uint32_t token = decodeInt32(result, offset);
        offset += 4;
        count++;

        switch (token) {
            case TEST_TOKEN_ERROR_CODE:
            case TEST_TOKEN_BAD_PIXEL_NUM:
            case TEST_TOKEN_LOCAL_BAD_PIXEL_NUM:
            case TEST_TOKEN_GET_DR_TIMESTAMP_TIME:
            case TEST_TOKEN_GET_MODE_TIME:
            case TEST_TOKEN_GET_CHIP_ID_TIME:
            case TEST_TOKEN_GET_VENDOR_ID_TIME:
            case TEST_TOKEN_GET_SENSOR_ID_TIME:
            case TEST_TOKEN_GET_FW_VERSION_TIME:
            case TEST_TOKEN_GET_IMAGE_TIME:
            case TEST_TOKEN_RAW_DATA_LEN:
            case TEST_TOKEN_IMAGE_QUALITY:
            case TEST_TOKEN_VALID_AREA:
            case TEST_TOKEN_GSC_FLAG:
            case TEST_TOKEN_KEY_POINT_NUM:
            case TEST_TOKEN_INCREATE_RATE:
            case TEST_TOKEN_OVERLAY:
            case TEST_TOKEN_GET_RAW_DATA_TIME:
            case TEST_TOKEN_PREPROCESS_TIME:
            case TEST_TOKEN_GET_FEATURE_TIME:
            case TEST_TOKEN_ENROLL_TIME:
            case TEST_TOKEN_AUTHENTICATE_TIME:
            case TEST_TOKEN_AUTHENTICATE_UPDATE_FLAG:
            case TEST_TOKEN_AUTHENTICATE_FINGER_COUNT:
            case TEST_TOKEN_AUTHENTICATE_FINGER_ITME:
            case TEST_TOKEN_TOTAL_TIME:
            case TEST_TOKEN_RESET_FLAG:
            case TEST_TOKEN_SINGULAR:
            case TEST_TOKEN_CHIP_TYPE:
            case TEST_TOKEN_CHIP_SERIES:
            case TEST_TOKEN_MAX_FINGERS:
            case TEST_TOKEN_MAX_FINGERS_PER_USER:
            case TEST_TOKEN_SUPPORT_KEY_MODE:
            case TEST_TOKEN_SUPPORT_FF_MODE:
            case TEST_TOKEN_SUPPORT_POWER_KEY_FEATURE:
            case TEST_TOKEN_FORBIDDEN_UNTRUSTED_ENROLL:
            case TEST_TOKEN_FORBIDDEN_ENROLL_DUPLICATE_FINGERS:
            case TEST_TOKEN_SUPPORT_BIO_ASSAY:
            case TEST_TOKEN_SUPPORT_PERFORMANCE_DUMP:
            case TEST_TOKEN_SUPPORT_NAV_MODE:
            case TEST_TOKEN_NAV_DOUBLE_CLICK_TIME:
            case TEST_TOKEN_NAV_LONG_PRESS_TIME:
            case TEST_TOKEN_ENROLLING_MIN_TEMPLATES:
            case TEST_TOKEN_VALID_IMAGE_QUALITY_THRESHOLD:
            case TEST_TOKEN_VALID_IMAGE_AREA_THRESHOLD:
            case TEST_TOKEN_DUPLICATE_FINGER_OVERLAY_SCORE:
            case TEST_TOKEN_INCREASE_RATE_BETWEEN_STITCH_INFO:
            case TEST_TOKEN_SCREEN_ON_AUTHENTICATE_FAIL_RETRY_COUNT:
            case TEST_TOKEN_SCREEN_OFF_AUTHENTICATE_FAIL_RETRY_COUNT:
            case TEST_TOKEN_AUTHENTICATE_ORDER:
            case TEST_TOKEN_REISSUE_KEY_DOWN_WHEN_ENTRY_FF_MODE:
            case TEST_TOKEN_REISSUE_KEY_DOWN_WHEN_ENTRY_IMAGE_MODE:
            case TEST_TOKEN_SUPPORT_SENSOR_BROKEN_CHECK:
            case TEST_TOKEN_BROKEN_PIXEL_THRESHOLD_FOR_DISABLE_SENSOR:
            case TEST_TOKEN_BROKEN_PIXEL_THRESHOLD_FOR_DISABLE_STUDY:
            case TEST_TOKEN_BAD_POINT_TEST_MAX_FRAME_NUMBER:
            case TEST_TOKEN_REPORT_KEY_EVENT_ONLY_ENROLL_AUTHENTICATE:
            case TEST_TOKEN_SUPPORT_FRR_ANALYSIS:
            case TEST_TOKEN_GET_GSC_DATA_TIME:
            case TEST_TOKEN_BIO_ASSAY_TIME:
            case TEST_TOKEN_SENSOR_VALIDITY:
            case TEST_TOKEN_ALGO_INDEX:
            case TEST_TOKEN_SAFE_CLASS:
            case TEST_TOKEN_TEMPLATE_COUNT:
            case TEST_TOKEN_ELECTRICITY_VALUE:
            case TEST_TOKEN_FINGER_EVENT:
            case TEST_TOKEN_LOCAL_SMALL_BAD_PIXEL_NUM:
            case TEST_TOKEN_LOCAL_BIG_BAD_PIXEL_NUM:
            case TEST_TOKEN_FLATNESS_BAD_PIXEL_NUM:
            case TEST_TOKEN_IS_BAD_LINE:
            case TEST_TOKEN_DUMP_IS_ENCRYPTED:
            case TEST_TOKEN_DUMP_OPERATION:
            case TEST_TOKEN_DUMP_YEAR:
            case TEST_TOKEN_DUMP_MONTH:
            case TEST_TOKEN_DUMP_DAY:
            case TEST_TOKEN_DUMP_HOUR:
            case TEST_TOKEN_DUMP_MINUTE:
            case TEST_TOKEN_DUMP_SECOND:
            case TEST_TOKEN_DUMP_MICROSECOND:
            case TEST_TOKEN_DUMP_VERSION_CODE:
            case TEST_TOKEN_DUMP_WIDTH:
            case TEST_TOKEN_DUMP_HEIGHT:
            case TEST_TOKEN_DUMP_FRAME_NUM:
            case TEST_TOKEN_DUMP_BROKEN_CHECK_FRAME_NUM:
            case TEST_TOKEN_DUMP_SELECT_INDEX:
            case TEST_TOKEN_DUMP_IMAGE_QUALITY:
            case TEST_TOKEN_DUMP_VALID_AREA:
            case TEST_TOKEN_DUMP_INCREASE_RATE_BETWEEN_STITCH_INFO:
            case TEST_TOKEN_DUMP_OVERLAP_RATE_BETWEEN_LAST_TEMPLATE:
            case TEST_TOKEN_DUMP_ENROLLING_FINGER_ID:
            case TEST_TOKEN_DUMP_DUMPLICATED_FINGER_ID:
            case TEST_TOKEN_DUMP_MATCH_SCORE:
            case TEST_TOKEN_DUMP_MATCH_FINGER_ID:
            case TEST_TOKEN_DUMP_STUDY_FLAG:
            case TEST_TOKEN_DUMP_NAV_TIMES:
            case TEST_TOKEN_DUMP_NAV_FRAME_INDEX:
            case TEST_TOKEN_DUMP_NAV_FRAME_COUNT:
            case TEST_TOKEN_DUMP_FINGER_ID:
            case TEST_TOKEN_DUMP_GROUP_ID:
            case TEST_TOKEN_DUMP_REMAINING_TEMPLATES:
            case TEST_TOKEN_SPI_RW_CMD:
            case TEST_TOKEN_SPI_RW_START_ADDR:
            case TEST_TOKEN_SPI_RW_LENGTH:
            case TEST_TOKEN_FW_DATA_LEN:
            case TEST_TOKEN_CFG_DATA_LEN:
            case TEST_TOKEN_REQUIRE_DOWN_AND_UP_IN_PAIRS_FOR_IMAGE_MODE:
            case TEST_TOKEN_REQUIRE_DOWN_AND_UP_IN_PAIRS_FOR_FF_MODE:
            case TEST_TOKEN_REQUIRE_DOWN_AND_UP_IN_PAIRS_FOR_KEY_MODE:
            case TEST_TOKEN_REQUIRE_DOWN_AND_UP_IN_PAIRS_FOR_NAV_MODE:
            case TEST_TOKEN_SUPPORT_SET_SPI_SPEED_IN_TEE:
            case TEST_TOKEN_UNDER_SATURATED_PIXEL_COUNT:
            case TEST_TOKEN_OVER_SATURATED_PIXEL_COUNT:
            case TEST_TOKEN_SATURATED_PIXEL_THRESHOLD: {
                uint32_t value = decodeInt32(result, offset);
                offset += 4;
                g_token_map[count-1].token = token;
                g_token_map[count-1].value.uint32_value = value;

                break;
            }

            case TEST_TOKEN_ALGO_VERSION:
            case TEST_TOKEN_PREPROCESS_VERSION:
            case TEST_TOKEN_FW_VERSION:
            case TEST_TOKEN_TEE_VERSION:
            case TEST_TOKEN_TA_VERSION:
            case TEST_TOKEN_CODE_FW_VERSION:
            case TEST_TOKEN_DUMP_PREPROCESS_VERSION: {
                uint32_t size = decodeInt32(result, offset);
                offset += 4;

                if (size <= MAX_LENGTH ){
                    g_token_map[count-1].token = token;
                    memcpy(g_token_map[count-1].value.buffer, (result + offset),size);
                    g_token_map[count-1].buffer_size = size;
                }

                offset += size;
                break;
            }

            case TEST_TOKEN_CHIP_ID:
            case TEST_TOKEN_VENDOR_ID:
            case TEST_TOKEN_SENSOR_ID:
            case TEST_TOKEN_PRODUCTION_DATE:
            // case TEST_TOKEN_RAW_DATA:
            // case TEST_TOKEN_BMP_DATA:
            case TEST_TOKEN_HBD_RAW_DATA:
            case TEST_TOKEN_SPI_RW_CONTENT:
            case TEST_TOKEN_CFG_DATA:
            case TEST_TOKEN_FW_DATA:
            case TEST_TOKEN_GSC_DATA:
            case TEST_TOKEN_DUMP_NAV_FRAME_NUM: {
                uint32_t size = decodeInt32(result, offset);
                offset += 4;

                if (size <= MAX_LENGTH ){
                    g_token_map[count-1].token = token;
                    memcpy(g_token_map[count-1].value.buffer, (result + offset),size);
                    g_token_map[count-1].buffer_size = size;
                }

                offset += size;
                break;
            }
            case TEST_TOKEN_RAW_DATA:
            case TEST_TOKEN_BMP_DATA:
            case TEST_TOKEN_DUMP_ENCRYPTED_DATA:
            case TEST_TOKEN_DUMP_CHIP_ID:
            case TEST_TOKEN_DUMP_VENDOR_ID:
            case TEST_TOKEN_DUMP_SENSOR_ID:
            case TEST_TOKEN_DUMP_KR:
            case TEST_TOKEN_DUMP_B:
            case TEST_TOKEN_DUMP_RAW_DATA:
            case TEST_TOKEN_DUMP_BROKEN_CHECK_RAW_DATA:
            case TEST_TOKEN_DUMP_CALI_RES:
            case TEST_TOKEN_DUMP_DATA_BMP:
            case TEST_TOKEN_DUMP_SITO_BMP:
            case TEST_TOKEN_DUMP_TEMPLATE: {
                uint32_t size = decodeInt32(result, offset);
                offset += 4;
                // data too long, do nothing
                offset += size;
                break;
            }

            case TEST_TOKEN_ALL_TILT_ANGLE:
            case TEST_TOKEN_BLOCK_TILT_ANGLE_MAX: {
                uint32_t size = decodeInt32(result, offset);
                offset += 4;
                float value = decodeFloat(result, offset, size);

                g_token_map[count-1].token = token;
                g_token_map[count-1].value.float_value = value;

                offset += size;

                break;
            }

            case TEST_TOKEN_NOISE: {
                uint32_t size = decodeInt32(result, offset);
                offset += 4;
                double value = decodeDouble(result, offset, size);

                g_token_map[count-1].token = token;
                g_token_map[count-1].value.double_value = value;

                offset += size;

                break;
            }

            case TEST_TOKEN_AVG_DIFF_VAL:
            case TEST_TOKEN_LOCAL_WORST:
            case TEST_TOKEN_IN_CIRCLE:
            case TEST_TOKEN_BIG_BUBBLE:
            case TEST_TOKEN_LINE:
            case TEST_TOKEN_HBD_BASE:
            case TEST_TOKEN_HBD_AVG: {
                short value = decodeInt16(result, offset);
                offset += 2;

                g_token_map[count-1].token = token;
                g_token_map[count-1].value.uint16_value = value;
                break;
            }

            case TEST_TOKEN_SENSOR_OTP_TYPE: {
                uint8_t value = decodeInt8(result, offset);
                offset += 1;
                g_token_map[count-1].token = token;
                g_token_map[count-1].value.uint16_value = value;
                break;
            }

            case TEST_TOKEN_DUMP_TIMESTAMP: {
                // long value = decodeInt64(result, offset);
                offset += 8;
                break;
            }

            case TEST_TOKEN_PACKAGE_VERSION:
            case TEST_TOKEN_PROTOCOL_VERSION:
            case TEST_TOKEN_CHIP_SUPPORT_BIO:
            case TEST_TOKEN_IS_BIO_ENABLE:
            case TEST_TOKEN_AUTHENTICATED_WITH_BIO_SUCCESS_COUNT:
            case TEST_TOKEN_AUTHENTICATED_WITH_BIO_FAILED_COUNT:
            case TEST_TOKEN_AUTHENTICATED_SUCCESS_COUNT:
            case TEST_TOKEN_AUTHENTICATED_FAILED_COUNT:
            case TEST_TOKEN_BUF_FULL:
            case TEST_TOKEN_UPDATE_POS: {
                int value = decodeInt32(result, offset);
                offset += 4;
                break;
            }

            case TEST_TOKEN_METADATA: {
                int size = decodeInt32(result, offset);
                offset += 4;
                offset += size;
                break;
            }

            default:
                count--;
                break;
        }
    }

    g_token_count = count;
    LOG_D(LOG_TAG, "[%s] exit", __func__);
}


gf_error_t update_config(token_map_t* result,uint32_t count){
    int i = 0;
    gf_error_t err = GF_SUCCESS;

    for (i = 0; i < count; i++){
        switch ( (result + i)->token){
        case TEST_TOKEN_ERROR_CODE:
            err  = (result + i)->value.uint32_value;
            if (err != GF_SUCCESS){
                return err;
            }
            break;
        case TEST_TOKEN_CHIP_TYPE:
            g_ChipType =  (result + i)->value.uint32_value;
            break;
        case TEST_TOKEN_CHIP_SERIES:
            g_ChipSeries =  (result + i)->value.uint32_value;
            break;
        case TEST_TOKEN_MAX_FINGERS :
            g_MaxFingers =  (result + i)->value.uint32_value;
            break;
        case TEST_TOKEN_MAX_FINGERS_PER_USER:
            g_MaxFingersPerUser = (result + i)->value.uint32_value;
            break;
        case TEST_TOKEN_SUPPORT_KEY_MODE :
            g_SupportKeyMode = (result + i)->value.uint32_value;
            break;
        case TEST_TOKEN_SUPPORT_FF_MODE:
            g_SupportFFMode = (result + i)->value.uint32_value;
            break;
        case TEST_TOKEN_SUPPORT_POWER_KEY_FEATURE:
            g_SupportPowerKeyFeature = (result + i)->value.uint32_value;
            break;
        case TEST_TOKEN_FORBIDDEN_UNTRUSTED_ENROLL:
            g_ForbiddenUntrustedEnroll = (result + i)->value.uint32_value;
            break;
        case TEST_TOKEN_FORBIDDEN_ENROLL_DUPLICATE_FINGERS:
            g_ForbiddenEnrollDuplicateFingers= (result + i)->value.uint32_value;
            break;
        case TEST_TOKEN_SUPPORT_BIO_ASSAY:
            g_SupportBioAssay = (result + i)->value.uint32_value;
            break;
        case TEST_TOKEN_SUPPORT_PERFORMANCE_DUMP:
            g_SupportPerformanceDump = (result + i)->value.uint32_value;
            break;
        case TEST_TOKEN_SUPPORT_NAV_MODE :
            g_SupportNavMode = (result + i)->value.uint32_value;
            break;
        case TEST_TOKEN_ENROLLING_MIN_TEMPLATES:
            g_EnrollingMinTemplates = (result + i)->value.uint32_value;
            break;
        case TEST_TOKEN_VALID_IMAGE_QUALITY_THRESHOLD:
            g_ValidImageQualityThreshold = (result + i)->value.uint32_value;
            break;
        case TEST_TOKEN_VALID_IMAGE_AREA_THRESHOLD:
            g_ValidImageAreaThreshold = (result + i)->value.uint32_value;
            break;
        case TEST_TOKEN_DUPLICATE_FINGER_OVERLAY_SCORE:
            g_DuplicateFingerOverlayScore = (result + i)->value.uint32_value;
            break;
        case TEST_TOKEN_INCREASE_RATE_BETWEEN_STITCH_INFO:
            g_IncreaseRateBetweenStitchInfo = (result + i)->value.uint32_value;
            break;
        case TEST_TOKEN_SCREEN_ON_AUTHENTICATE_FAIL_RETRY_COUNT:
            g_ScreenOnAuthenticateFailRetryCount = (result + i)->value.uint32_value;
            break;
        case TEST_TOKEN_SCREEN_OFF_AUTHENTICATE_FAIL_RETRY_COUNT:
            g_ScreenOffAuthenticateFailRetryCount = (result + i)->value.uint32_value;
            break;
        case TEST_TOKEN_REISSUE_KEY_DOWN_WHEN_ENTRY_FF_MODE:
            g_ReissueKeyDownWhenEntryFfMode = (result + i)->value.uint32_value;
            break;
        case TEST_TOKEN_REISSUE_KEY_DOWN_WHEN_ENTRY_IMAGE_MODE :
            g_ReissueKeyDownWhenEntryImageMode = (result + i)->value.uint32_value;
            break;
        case TEST_TOKEN_SUPPORT_SENSOR_BROKEN_CHECK:
            g_SupportSensorBrokenCheck = (result + i)->value.uint32_value;
            break;
        case TEST_TOKEN_BROKEN_PIXEL_THRESHOLD_FOR_DISABLE_SENSOR:
            g_BrokenPixelThresholdForDisableSensor = (result + i)->value.uint32_value;
            break;
        case TEST_TOKEN_BROKEN_PIXEL_THRESHOLD_FOR_DISABLE_STUDY:
            g_BrokenPixelThresholdForDisableStudy = (result + i)->value.uint32_value;
            break;
        case TEST_TOKEN_BAD_POINT_TEST_MAX_FRAME_NUMBER:
            g_BadPointTestMaxFrameNumber = (result + i)->value.uint32_value;
            break;
        case TEST_TOKEN_REPORT_KEY_EVENT_ONLY_ENROLL_AUTHENTICATE:
            g_ReportKeyEventOnlyEnrollAuthenticate = (result + i)->value.uint32_value;
            break;
        default:
            break;
        }
    }

    if (g_MaxFingers < g_MaxFingersPerUser) {
        g_MaxFingers = g_MaxFingersPerUser;
    }

    return err;
}
