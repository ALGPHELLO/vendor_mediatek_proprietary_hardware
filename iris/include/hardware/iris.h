/*
 * Copyright (C) 2017 The SYKEAN Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_INCLUDE_HARDWARE_IRIS_H
#define ANDROID_INCLUDE_HARDWARE_IRIS_H

#include <hardware/mtk_hw_auth_token.h>

#define IRIS_MODULE_API_VERSION_1_0 HARDWARE_MODULE_API_VERSION(1, 0)
#define IRIS_MODULE_API_VERSION_2_0 HARDWARE_MODULE_API_VERSION(2, 0)
#define IRIS_HARDWARE_MODULE_ID "iris"

typedef enum iris_msg_type {
    IRIS_ERROR = -1,
    IRIS_ACQUIRED = 1,
    IRIS_TEMPLATE_ENROLLING = 3,
    IRIS_TEMPLATE_REMOVED = 4,
    IRIS_AUTHENTICATED = 5,
    IRIS_DISPLAY = 6,
    IRIS_CAPTURED = 7,
    IRIS_CAPTURE_ERROR = 8,
    IRIS_CAPTURE_STAGING = 9,
} iris_msg_type_t;

/*
 * Iris errors are meant to tell the framework to terminate the current operation and ask
 * for the user to correct the situation. These will almost always result in messaging and user
 * interaction to correct the problem.
 *
 * For example, IRIS_ERROR_CANCELED should follow any acquisition message that results in
 * a situation where the current operation can't continue without user interaction. For example,
 * if the sensor is dirty during enrollment and no further enrollment progress can be made,
 * send IRIS_ACQUIRED_IMAGER_DIRTY followed by IRIS_ERROR_CANCELED.
 */
typedef enum iris_error {
    IRIS_ERROR_HW_UNAVAILABLE = 1, /* The hardware has an error that can't be resolved. */
    IRIS_ERROR_UNABLE_TO_PROCESS = 2, /* Bad data; operation can't continue */
    IRIS_ERROR_TIMEOUT = 3, /* The operation has timed out waiting for user input. */
    IRIS_ERROR_NO_SPACE = 4, /* No space available to store a template */
    IRIS_ERROR_CANCELED = 5, /* The current operation can't proceed. See above. */
    IRIS_ERROR_UNABLE_TO_REMOVE = 6, /* The iris with given id can't be removed */
    IRIS_ERROR_LIB_FAIL = 7, /* The iris lib error; operation can't continue */
    IRIS_ERROR_LOCKOUT = 8, /* Locked out due to too many attempts */
    IRIS_ERROR_NO_MATCH = 9, /* The user does not exist in the iris database(the user not enrolled) */
    IRIS_ERROR_PROXIMITY_CLOSE = 10, /* Something close to the proximity sensor */
    IRIS_ERROR_CAMERA_UNAVAILABLE = 11, /* can't connect to the iris camera */
    IRIS_ERROR_VENDOR_BASE = 1000 /* vendor-specific error messages start here */
} iris_error_t;

/*
 * Iris errors are meant to tell the framework to terminate the current operation and ask
 * for the user to correct the situation. These will almost always result in messaging and user
 * interaction to correct the problem.
 *
 * For example, IRIS_ERROR_CANCELED should follow any acquisition message that results in
 * a situation where the current operation can't continue without user interaction. For example,
 * if the sensor is dirty during enrollment and no further enrollment progress can be made,
 * send IRIS_ACQUIRED_IMAGER_DIRTY followed by IRIS_ERROR_CANCELED.
 */
typedef enum iris_capture_error {
    CAPTURE_ERROR_HW_UNAVAILABLE = 1, /* The hardware has an error that can't be resolved. */
    CAPTURE_ERROR_UNABLE_TO_PROCESS = 2, /* Bad data; operation can't continue */
    CAPTURE_ERROR_TIMEOUT = 3, /* The operation has timed out waiting for user input. */
    CAPTURE_ERROR_NO_SPACE = 4, /* No space available to store a template */
    CAPTURE_ERROR_CANCELED = 5, /* The current operation can't proceed. See above. */
    CAPTURE_ERROR_LIB_FAIL = 6, /* The iris lib error; operation can't continue */
    CAPTURE_ERROR_LOCKOUT = 7, /* Locked out due to too many attempts */
    CAPTURE_ERROR_PROXIMITY_CLOSE = 8, /* Something close to the proximity sensor */
    CAPTURE_ERROR_LICENSE = 9, /* The customer license key is not allowed */
    CAPTURE_ERROR_EYE_COUNT = 10, /* Input the capture eye count not matched */
    CAPTURE_ERROR_TAMPERED = 11, /* The capture device has been tampered */
    CAPTURE_ERROR_INVALID_PARAM = 12, /* Input paramter invalid */
    CAPTURE_ERROR_PID_PARSER = 13, /* The input pid data can not be parsered */
    CAPTURE_ERROR_NONTRUSTED_CERT = 14, /* The input certificate nontrusted */
    CAPTURE_ERROR_ENGINE = 15, /* Capture device engine encountered some mistakes */
    CAPTURE_ERROR_CAMERA_UNAVAILABLE = 16, /* can't connect to the iris camera */
    CAPTURE_ERROR_VENDOR_BASE = 1000 /* vendor-specific error messages start here */
} iris_capture_error_t;

typedef enum iris_pid_type {
    CAPTURE_PIDTYPE_XML = 0,
    CAPTURE_PIDTYPE_PROTOBUF = 1,
} iris_pid_type_t;

typedef enum iris_bio_type {
    CAPTURE_BIOTYPE_LEFT = 0,
    CAPTURE_BIOTYPE_RIGHT = 1,
    CAPTURE_BIOTYPE_BOTH = 2,
    CAPTURE_BIOTYPE_UNKNOWN = 3,
} iris_bio_type_t;

typedef enum iris_capture_status {
    CAPTURE_IRIS_SUCCESS = 0,
    CAPTURE_IRIS_FAILED = 1,
    CAPTURE_IRIS_STARTED = 2,
    CAPTURE_IRIS_COMPRESSING = 3,
    CAPTURE_IRIS_ENCODING = 4,
} iris_capture_status_t;

/*
 * Iris acquisition info is meant as feedback for the current operation.  Anything but
 * IRIS_ACQUIRED_GOOD will be shown to the user as feedback on how to take action on the
 * current operation. For example, IRIS_ACQUIRED_IMAGER_DIRTY can be used to tell the user
 * to clean the sensor.  If this will cause the current operation to fail, an additional
 * IRIS_ERROR_CANCELED can be sent to stop the operation in progress (e.g. enrollment).
 * In general, these messages will result in a "Try again" message.
 */
typedef enum iris_acquired_info {
    IRIS_ACQUIRED_GOOD = 0,
    IRIS_ACQUIRED_PARTIAL = 1, /* sensor needs more data, i.e. longer swipe. */
    IRIS_ACQUIRED_INSUFFICIENT = 2, /* image doesn't contain enough detail for recognition */
    IRIS_ACQUIRED_IMAGER_DIRTY = 3, /* sensor needs to be cleaned */
    IRIS_ACQUIRED_DIST_TOO_FAR = 4, /* image distance too far, need adjust */
    IRIS_ACQUIRED_DIST_FAR = 5, /* image distance far, need adjust */
    IRIS_ACQUIRED_DIST_TOO_NEAR = 6, /* image distance too near, need adjust */
    IRIS_ACQUIRED_DIST_NEAR = 7, /* image distance near, need adjust */
    IRIS_ACQUIRED_DIST_NICE = 8, /* image distance nice, please keep */
    IRIS_ACQUIRED_EYE_ERROR = 9, /* image can not to locate the eyes to recognize */
    IRIS_ACQUIRED_EYE_OUT = 10, /* image detected eyes out, need adjust */
    IRIS_ACQUIRED_EYE_LITTLE = 11, /* image detected eyes little, need wide open */
    IRIS_ACQUIRED_EYE_LOOK_SIDEWAYS = 12, /* image detected eye look sideways, need adjust */
    IRIS_ACQUIRED_VENDOR_BASE = 1000 /* vendor-specific acquisition messages start here */
} iris_acquired_info_t;

typedef struct iris_eye_id {
    uint32_t gid;
    uint32_t eid;
} iris_eye_id_t;

typedef struct iris_enroll {
    iris_eye_id_t iris;
    /* samples_remaining goes from N (no data collected, but N scans needed)
     * to 0 (no more data is needed to build a template). */
    uint32_t samples_remaining;
    uint64_t msg; /* Vendor specific message. Used for user guidance */
} iris_enroll_t;

typedef struct iris_removed {
    iris_eye_id_t iris;
} iris_removed_t;

typedef struct iris_acquired {
    iris_acquired_info_t acquired_info_l; /* information about the left eye image */
    iris_acquired_info_t acquired_info_r; /* information about the right eye image */
} iris_acquired_t;

typedef struct iris_authenticated {
    iris_eye_id_t iris;
    hw_auth_token_t hat;
} iris_authenticated_t;

typedef struct iris_display {
    uint8_t *data;
    size_t size;
    int32_t width;
    int32_t height;
    int32_t eyeRectL[4]; /* 0: rect left. 1: rect top. 2: rect right. 3: rect bottom */
    int32_t eyeRectR[4]; /* 0: rect left. 1: rect top. 2: rect right. 3: rect bottom */
} iris_display_t;

typedef struct iris_capture {
    uint8_t *pEncrpPid;
    size_t pidSize;
    uint8_t *pEncrpHMAC;
    size_t hmacSize;
    uint8_t *pEncrpSesKey;
    size_t sesKeySize;
} iris_capture_t;

typedef struct iris_msg {
    iris_msg_type_t type;
    union {
        iris_error_t error;
        iris_enroll_t enroll;
        iris_removed_t removed;
        iris_acquired_t acquired;
        iris_authenticated_t authenticated;
        iris_display_t display;
        iris_capture_t captured;
        iris_capture_error_t capture_error;
        iris_capture_status_t capture_staging;
    } data;
} iris_msg_t;

/* Callback function type */
typedef void (*iris_notify_t)(const iris_msg_t *msg);

/* Synchronous operation */
typedef struct iris_device {
    /**
     * Common methods of the iris device. This *must* be the first member
     * of iris_device as users of this structure will cast a hw_device_t
     * to iris_device pointer in contexts where it's known
     * the hw_device_t references a iris_device.
     */
    struct hw_device_t common;

    /*
     * Client provided callback function to receive notifications.
     * Do not set by hand, use the function above instead.
     */
    iris_notify_t notify;

    /*
     * Set notification callback:
     * Registers a user function that would receive notifications from the HAL
     * The call will block if the HAL state machine is in busy state until HAL
     * leaves the busy state.
     *
     * Function return: 0 if callback function is successfuly registered
     *                  or a negative number in case of error, generally from the errno.h set.
     */
    int (*set_notify)(struct iris_device *dev, iris_notify_t notify);

    /*
     * Iris pre-enroll enroll request:
     * Generates a unique token to upper layers to indicate the start of an enrollment transaction.
     * This token will be wrapped by security for verification and passed to enroll() for
     * verification before enrollment will be allowed. This is to ensure adding a new iris
     * template was preceded by some kind of credential confirmation (e.g. device password).
     *
     * Function return: 0 if function failed
     *                  otherwise, a uint64_t of token
     */
    uint64_t (*pre_enroll)(struct iris_device *dev);

    /*
     * Iris enroll request:
     * Switches the HAL state machine to collect and store a new iris
     * template. Switches back as soon as enroll is complete
     * (iris_msg.type == IRIS_TEMPLATE_ENROLLING &&
     *  iris_msg.data.enroll.samples_remaining == 0)
     * or after timeout_sec seconds.
     * The iris template will be assigned to the group gid. User has a choice
     * to supply the gid or set it to 0 in which case a unique group id will be generated.
     *
     * Function return: 0 if enrollment process can be successfully started
     *                  or a negative number in case of error, generally from the errno.h set.
     *                  A notify() function may be called indicating the error condition.
     */
    int (*enroll)(struct iris_device *dev, const hw_auth_token_t *hat,
                  uint32_t gid, uint32_t timeout_sec);

    /*
     * Finishes the enroll operation and invalidates the pre_enroll() generated challenge.
     * This will be called at the end of a multi-iris enrollment session to indicate
     * that no more iris will be added.
     *
     * Function return: 0 if the request is accepted
     *                  or a negative number in case of error, generally from the errno.h set.
     */
    int (*post_enroll)(struct iris_device *dev);

    /*
     * get_authenticator_id:
     * Returns a token associated with the current iris set. This value will
     * change whenever a new iris is enrolled, thus creating a new iris
     * set.
     *
     * Function return: current authenticator id or 0 if function failed.
     */
    uint64_t (*get_authenticator_id)(struct iris_device *dev);

    /*
     * Cancel pending enroll or authenticate, sending IRIS_ERROR_CANCELED
     * to all running clients. Switches the HAL state machine back to the idle state.
     * Unlike enroll_done() doesn't invalidate the pre_enroll() challenge.
     *
     * Function return: 0 if cancel request is accepted
     *                  or a negative number in case of error, generally from the errno.h set.
     */
    int (*cancel)(struct iris_device *dev);

    /*
     * Enumerate all the iris templates found in the directory set by
     * set_active_group()
     * This is a synchronous call. The function takes:
     * - A pointer to an array of iris_eye_id_t.
     * - The size of the array provided, in iris_eye_id_t elements.
     * Max_size is a bi-directional parameter and returns the actual number
     * of elements copied to the caller supplied array.
     * In the absence of errors the function returns the total number of templates
     * in the user directory.
     * If the caller has no good guess on the size of the array he should call this
     * function witn *max_size == 0 and use the return value for the array allocation.
     * The caller of this function has a complete list of the templates when *max_size
     * is the same as the function return.
     *
     * Function return: Total number of iris templates in the current storage directory.
     *                  or a negative number in case of error, generally from the errno.h set.
     */
    int (*enumerate)(struct iris_device *dev, iris_eye_id_t *results, uint32_t *max_size);

    /*
     * Iris remove request:
     * Deletes a iris template.
     * Works only within a path set by set_active_group().
     * notify() will be called with details on the template deleted.
     * iris_msg.type == IRIS_TEMPLATE_REMOVED and
     * iris_msg.data.removed.id indicating the template id removed.
     *
     * Function return: 0 if iris template(s) can be successfully deleted
     *                  or a negative number in case of error, generally from the errno.h set.
     */
    int (*remove)(struct iris_device *dev, uint32_t gid, uint32_t eid);

    /*
     * Restricts the HAL operation to a set of irises belonging to a
     * group provided.
     * The caller must provide a path to a storage location within the user's
     * data directory.
     *
     * Function return: 0 on success
     *                  or a negative number in case of error, generally from the errno.h set.
     */
    int (*set_active_group)(struct iris_device *dev, uint32_t gid, const char *store_path);

    /*
     * Restricts the HAL operation to preview the iris camera.
     *
     * Function return: 0 on success
     *                  or a negative number in case of error, generally from the errno.h set.
     */
    int (*set_native_window)(struct iris_device *dev, uint32_t gid, void *anw);

    /*
     * Authenticates an operation identifed by operation_id
     *
     * Function return: 0 on success
     *                  or a negative number in case of error, generally from the errno.h set.
     */
    int (*authenticate)(struct iris_device *dev, uint64_t operation_id, uint32_t gid);

    /*
     * Iris capture request:
     * Switches the HAL state machine to capture iris in the device.
     * Switches back as soon as capture is complete or after timeout_sec seconds.
     *
     * Function return: 0 if captured process can be successfully started
     *                  or a negative number in case of error, generally from the errno.h set.
     *                  A notify() function may be called indicating the error condition.
     */
    int (*capture)(struct iris_device *dev, const uint8_t *pid_data, uint32_t pid_size, int32_t pid_type,
                   int32_t bio_type, const uint8_t *cert_chain, uint32_t cert_size, uint32_t gid);

    /* Reserved for backward binary compatibility */
    void *reserved[4];
} iris_device_t;

typedef struct iris_module {
    /**
     * Common methods of the iris module. This *must* be the first member
     * of iris_module as users of this structure will cast a hw_module_t
     * to iris_module pointer in contexts where it's known
     * the hw_module_t references a iris_module.
     */
    struct hw_module_t common;
} iris_module_t;

#endif  /* ANDROID_INCLUDE_HARDWARE_IRIS_H */
