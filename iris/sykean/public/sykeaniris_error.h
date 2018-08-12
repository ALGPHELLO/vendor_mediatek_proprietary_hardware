/*
 * Copyright (c) 2017 SYKEAN Limited.
 *
 * All rights are reserved.
 * Proprietary and confidential.
 * Unauthorized copying of this file, via any medium is strictly prohibited.
 * Any use is subject to an appropriate license granted by SYKEAN Company.
 */

#ifndef __SYKEAN_IRIS_HAL_ERROR_H__
#define __SYKEAN_IRIS_HAL_ERROR_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


//
typedef enum sykean_iris_error {
    SYKEAN_IRIS_FAIL                               = -1,
    SYKEAN_IRIS_SUCCESS                            = 0,
    SYKEAN_IRIS_ERROR_OUT_OF_MEMORY                = 1001,
    SYKEAN_IRIS_ERROR_OPEN_TA_FAILED               = 1002,
    SYKEAN_IRIS_ERROR_BAD_PARAMS                   = 1003,
    SYKEAN_IRIS_ERROR_NO_SPACE                     = 1004,
    SYKEAN_IRIS_ERROR_REACH_IRIS_UPLIMIT           = 1005,
    SYKEAN_IRIS_ERROR_NOT_MATCH                    = 1006,
    SYKEAN_IRIS_ERROR_CANCELED                     = 1007,
    SYKEAN_IRIS_ERROR_TIMEOUT                      = 1008,
    SYKEAN_IRIS_ERROR_PREPROCESS_FAILED            = 1009,
    SYKEAN_IRIS_ERROR_GENERIC                      = 1010,
    SYKEAN_IRIS_ERROR_ACQUIRED_PARTIAL             = 1011,
    SYKEAN_IRIS_ERROR_ACQUIRED_IMAGER_DIRTY        = 1012,
    SYKEAN_IRIS_ERROR_DUPLICATE_IRIS               = 1013,
    SYKEAN_IRIS_ERROR_OPEN_DEVICE_FAILED           = 1014,
    SYKEAN_IRIS_ERROR_HAL_GENRAL_ERROR             = 1015,
    SYKEAN_IRIS_ERROR_HAL_FILE_DESCRIPTION_NULL    = 1016,
    SYKEAN_IRIS_ERROR_HAL_IOCTL_FAILED             = 1017,
    SYKEAN_IRIS_ERROR_HAL_TIMER_FUNC               = 1018,
    SYKEAN_IRIS_ERROR_CORRUPT_CONTENT              = 1019,
    SYKEAN_IRIS_ERROR_INCORRECT_VERSION            = 1020,
    SYKEAN_IRIS_ERROR_CORRUPT_OBJECT               = 1021,
    SYKEAN_IRIS_ERROR_INVALID_DATA                 = 1022,
    SYKEAN_IRIS_ERROR_SAVE_IRIS_TEMPLATE           = 1023,
    SYKEAN_IRIS_ERROR_IRIS_BUSY                    = 1024,
    SYKEAN_IRIS_ERROR_OPEN_SECURE_OBJECT_FAILED    = 1025,
    SYKEAN_IRIS_ERROR_READ_SECURE_OBJECT_FAILED    = 1026,
    SYKEAN_IRIS_ERROR_WRITE_SECURE_OBJECT_FAILED   = 1027,
    SYKEAN_IRIS_ERROR_SECURE_OBJECT_NOT_EXIST      = 1028,
    SYKEAN_IRIS_ERROR_WRITE_CONFIG_FAILED          = 1029,
    SYKEAN_IRIS_ERROR_TEST_SENSOR_FAILED           = 1030,
    SYKEAN_IRIS_ERROR_SET_MODE_FAILED              = 1031,
    SYKEAN_IRIS_ERROR_CHIPID_NOT_CORRECT           = 1032,
    SYKEAN_IRIS_ERROR_MAX_NUM                      = 1033,
    SYKEAN_IRIS_ERROR_TEST_BAD_POINT_FAILED        = 1034,
    SYKEAN_IRIS_ERROR_TEST_FRR_FAR_ENROLL_DIFFERENT_IRIS = 1035,
    SYKEAN_IRIS_ERROR_DUPLICATE_AREA               = 1036,
    SYKEAN_IRIS_ERROR_IRIS_NOT_EXIST               = 1037,
    SYKEAN_IRIS_ERROR_INVALID_PREPROCESS_VERSION   = 1038,  ///< This means the saved preprocess version is
                                                            ///< different from the new preprocess version.
                                                            ///< Should delete the saved calibration parameters.
    SYKEAN_IRIS_ERROR_TA_DEAD                      = 1039,
    SYKEAN_IRIS_ERROR_INIT                         = 1040,
    SYKEAN_IRIS_ERROR_UNINIT                       = 1041,
    SYKEAN_IRIS_ERROR_PROMOTE_LIMIT                = 1042,
    SYKEAN_IRIS_ERROR_GROUP_LIMIT                  = 1043,
    SYKEAN_IRIS_ERROR_PRE_ENROLL                   = 1044,
    SYKEAN_IRIS_ERROR_ENROLL                       = 1045,
    SYKEAN_IRIS_ERROR_MAX                          = 2000,
} sykean_iris_error_t;

#ifdef __cplusplus
}
#endif

#endif // _SYKEAN_IRIS_HAL_ERROR_H__
