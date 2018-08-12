/*
 * Copyright (C) 2013 The Android Open Source Project
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
#define LOG_TAG "ConsumerIrHal"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include <sys/ioctl.h>
#include <sys/times.h>
#include <dlfcn.h>
#include <cutils/log.h>
#include <cutils/properties.h>
#include <hardware/hardware.h>

#include "consumerir_extra.h"
#include "consumerir_core.h"

#define TX_DRIVER_PATH    "/dev/irtx"
#define RX_DRIVER_PATH    "/dev/ir-learning"

#define SIGNALS_GENERATE sFpSignalsGenerate

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#define IRTX_IOC_SET_CARRIER_FREQ    _IOW('R', 0, unsigned int)
#define IRTX_IOC_GET_SOLUTTION_TYPE  _IOR('R', 1, unsigned int)
#define IRTX_IOC_SET_IRTX_LED_EN     _IOW('R', 10, unsigned int)

#define SPI_IOC_READ_WAVE          _IOR('k', 1, __u8)
#define SPI_IOC_GET_SAMPLE_RATE    _IOR('k', 2, __u8)

#define PROP_IRTX_LOG_ENABLE             "irtx.hal.log.enable"
#define PROP_IRTX_LOG_ENABLE_VAL_EN      "1"  // enable
#define PROP_IRTX_MODE        "irtx.hal.mode"
#define PROP_IRTX_DUTY        "irtx.hal.duty"

#define DEFAULT_DUTY  25

static const consumerir_freq_range_t consumerir_freqs[] = {
    {.min = 30000, .max = 30000},
    {.min = 33000, .max = 33000},
    {.min = 36000, .max = 36000},
    {.min = 38000, .max = 38000},
    {.min = 40000, .max = 40000},
    {.min = 56000, .max = 56000},
};

/*-------------------------------------------------------------------
 |  Function GET_VALUE_OF_SYSTEM_PROPERTITY
 |
 |  Purpose:  Return the value of the system property
 |
 |  Parameters:
 |  Returns:  The value of the system property
 *-------------------------------------------------------------------*/
static int check_prop_match(const char *prop, const char *dest_value)
{
    char value[PROPERTY_VALUE_MAX] = {'\0'};
    int ret = 0;

    ret = property_get(prop, value, NULL);
    if (ret > 0) {
        ret = strcmp(dest_value, value);
        if (!ret) {
            ret = 1;
        }
    }

    ALOGD("%s prop:%s, dest_value:%s, value:%s, ret%d\n", __FUNCTION__, prop, dest_value, value, ret);
    ret = ((ret == 1) ? ret : 0);
    return ret;
}

/*-------------------------------------------------------------------
 |  Function CHECK_IF_SHOW_DETAIL_LOG
 |
 |  Purpose:  Return the flag of showing the detail log
 |
 |  Parameters:
 |  Returns:  The flag of showing the detail log
 *-------------------------------------------------------------------*/
static int irtx_hal_log_enabled()
{
    return check_prop_match(PROP_IRTX_LOG_ENABLE, PROP_IRTX_LOG_ENABLE_VAL_EN);
}

/*-------------------------------------------------------------------
 |  Function RECEIVE IR SIGNALS
 |
 |  Purpose:  Receive the IR raw signals from HW
 |
 |  Parameters:
 |      consumerir_device (IN) -- IRRX hal structure
 |      sample_rate (IN) -- Simple rate of HW
 |  buffer (IN) -- Buffer to store raw data
 |  len (IN) -- Length of buffer
 |
 |  Returns:  The status code of this execution.
 *-------------------------------------------------------------------*/
static int consumerir_receive(struct consumerir_extra_device *dev __unused,
                              unsigned int *sample_rate,
                              unsigned char *buffer, int *len)
{
    int fd, ret=0;

    fd = open("/dev/ir-learning", O_RDWR);
    if (fd < 0) {
        ALOGE("file open fail, errno=%d\n", errno);
        return fd;
    }

    ret = ioctl(fd, SPI_IOC_GET_SAMPLE_RATE, sample_rate);
    ALOGD("SPI_IOC_READ_WAVE ret= %d, sample_rate= %d\n", ret, *sample_rate);
    if (ret) {
        ALOGE("file ioctl(0x%x) fail, errno=%d\n" ,(unsigned int) SPI_IOC_GET_SAMPLE_RATE
                                                  ,(unsigned int) errno);
        goto exit;
    }

    ret = ioctl(fd, SPI_IOC_READ_WAVE, buffer);
    ALOGD("SPI_IOC_READ_WAVE ret=%d\n", ret);
    if (ret > 0) {
        *len = ret;
        ret = 0;
    }

exit:
    close(fd);
    return ret;
}

// original libConsumerir_core
//////////////////////////////////////////////////////////
// Function Name: get_duty
// Purpose: Get duty cycle from system properties
// Arguments:
//          defaultValue Default duty cycle
// Return:
//          Duty Cycle
//////////////////////////////////////////////////////////
static int get_duty(int defaultValue) {

    char value[PROPERTY_VALUE_MAX] = {'\0'};
    int ret = 0;

    ret = property_get(PROP_IRTX_DUTY, value, NULL);
    if(ret <= 0) {
        return defaultValue;
    }

    ret = atoi(value);
    if(ret <= 0) {
        return defaultValue;
    }

    return ret;
}

//////////////////////////////////////////////////////////
// Function Name: get_mode
// Purpose: Get mode from system properties
// Arguments:
//          defaultValue Default mode
// Return:
//          Mode
//////////////////////////////////////////////////////////
static int get_mode(int defaultMode) {

    char value[PROPERTY_VALUE_MAX] = {'\0'};
    int ret = 0;

    ret = property_get(PROP_IRTX_MODE, value, NULL);
    if(ret <= 0) {
        return defaultMode;
    }

    ret = atoi(value);
    if(ret <= 0) {
        return defaultMode;
    }

    return ret;
}

//////////////////////////////////////////////////////////
// Function Name: signals_generate
// Purpose: Generate the IR Signals
// Arguments:
//          type Default mode
//          carrier_freq
//          pattern
//          pattern_len
//          wave_buffer
//          buffer_len
// Return:
//          Mode
//////////////////////////////////////////////////////////
static int signals_generate(int type, int carrier_freq,
                                const int pattern[], int pattern_len,
                                unsigned int **wave_buf, int *buf_len)
{
    int total_time = 0; // micro-seconds
    long i,j;
    int buffer_len;
    int ret = 0;
    unsigned int *wave_buffer = NULL;
    int *__pattern = (int *)pattern;
    int int_ptr = 0;
    int bit_ptr = 0;
    double cycle = 0;
    double DUTY = 0.33;
    int cycle_counter = 0;
    int non_duty_cycle = 0;
    int whole_cycle = 0;
    int duty_cycle = 0;
    char current_level = 1; // start with high level
    int normal_range = 0;
    int send_signal = 0;
    int mode = 0;
    int duty = 0;
    int *temp = (int*)malloc(pattern_len * sizeof(int));
    memset (temp, 0, sizeof (int) * pattern_len);
    // Get Mode
    mode = get_mode(mode);

    // Measure the total time to calculate buffer memory size
    if (type == 1 || type == 0) {
        duty = get_duty(DEFAULT_DUTY);
        ALOGD("Pre calculate memory size ,use PWM only solution, mode%d, duty%d\n", mode, duty);
        cycle = 1000000.0/carrier_freq;
        whole_cycle = (int)(cycle + 0.5); // Round cycle to integer

        if(irtx_hal_log_enabled()) {
            ALOGD("Cycle= %1f\n", cycle);
            ALOGD("Whole_cycle = %d\n", whole_cycle);
        }
        if(mode == 0) {
            for (i = 0; i < pattern_len; i++) {
                temp[i] = ((int)((double) __pattern[i] / (double) whole_cycle + 0.5)) * whole_cycle;
                total_time += temp[i];
            }
        } else {
            for (i = 0; i < pattern_len; i++) {
                temp[i] = __pattern[i];
                total_time += temp[i];
            }
        }
    } else if (type != 0) {
        ALOGE("Invalid solution type %d\n", type);
        return -EINVAL;
    }

    /* simulate the time spent transmitting by sleeping */
    if(irtx_hal_log_enabled()) {
        ALOGD("Transmit for %d uS at %d Hz\n", total_time, carrier_freq);
        ALOGD("Pattern_len:%d, pattern[]:\n", pattern_len);
    }

    buffer_len = ceil(total_time/(float)32); // number of integers, one bit for one micro-seconds
    wave_buffer = (unsigned int *)malloc(buffer_len * sizeof(unsigned int)); // number of bytes
    if (!wave_buffer) {
        ALOGE("malloc fail, errno=%d\n", errno);
        return -ENOMEM;
    }
    ALOGD("U32 number=%d\n", buffer_len);

    memset(wave_buffer, 0, buffer_len * 4);
    if (type == 0) {
        ALOGD("Use IRTX+PWM solution\n");
        for (i = 0; i < pattern_len; i++) {
            for(j = 0; j< temp[i]; j++) {
                if (current_level)
                    *(wave_buffer+int_ptr) |= (1<<bit_ptr);
                else
                    *(wave_buffer+int_ptr) &= ~(1<<bit_ptr);
                bit_ptr++;
                if (bit_ptr == 32) {
                    bit_ptr = 0;
                    int_ptr++;
                }
            }
            current_level = !current_level;
        }
    } else if (type == 1) {
        ALOGD("use PWM only solution, mode%d, duty%d\n", mode, duty);
        if(irtx_hal_log_enabled()) {
            ALOGD("cycle= %1f\n", cycle);
            ALOGD("whole_cycle = %d\n", whole_cycle);
        }
        duty_cycle = (int)(cycle * DUTY +0.5); // Round duty cycle to integer
        ALOGD("duty_cycle = %d\n", duty_cycle);
        non_duty_cycle = whole_cycle - duty_cycle; //Get non_duty_cycle

        for (i = 0; i < pattern_len; i++) {
            cycle_counter = 0;
            if (mode == 0) { // golden pattern
                normal_range = temp[i];
                send_signal = normal_range;
            } else {
                ALOGE("mode %d is not supported\n", mode);
                normal_range = temp[i];
                send_signal = normal_range;
            }

            for (j = 0; j < send_signal; j++) {
                if (current_level) {
                    if ((cycle_counter * whole_cycle <= j) && (j < (cycle_counter * whole_cycle + duty_cycle)) && j <= normal_range) {
                        *(wave_buffer+int_ptr) |= (1 << bit_ptr);
                    } else {
                        *(wave_buffer+int_ptr) &= (~(1 << bit_ptr));
                    }
                } else {
                    *(wave_buffer+int_ptr) &= ~(1 << bit_ptr);
                }

                bit_ptr++;
                if ((j != 0) && (((j + 1) % whole_cycle) == 0)){
                    cycle_counter += 1;
                }

                if (bit_ptr == 32) {
                    bit_ptr = 0;
                    int_ptr++;
                }
            }
            current_level = !current_level;
        }
    }

    ALOGD("Converted len:%d, data:\n", buffer_len);

    *wave_buf = wave_buffer;
    *buf_len = buffer_len;
    free(temp);
    return 0;
}

/*-------------------------------------------------------------------
 |  Function TRANSMIT IRTX PATTERNS
 |
 |  Purpose:  Send the IRTX Patterns to LED
 |
 |  Parameters:
 |      consumerir_device (IN) -- IRTX hal structure
 |      carrier_freq (IN) -- Carrier frequency of the IR signals
 |  pattern (IN) -- Patterns of the IR signals
 |  pattern_len -- Length of patterns
 |
 |  Returns:  The status code of this execution.
 *-------------------------------------------------------------------*/
static int consumerir_transmit(struct consumerir_device *dev __unused,
   int carrier_freq, const int pattern[], int pattern_len)
{
    int i = 0;
    int ret = 0;
    int write_size = 0;
    int fd, buffer_len;
    unsigned int *wave_buffer = NULL;
    int *__pattern = (int *)pattern;
    unsigned int solution_type = 0;

    if (carrier_freq <= 0) {
        ALOGD("[IRTX] carrier_freq error, carrier_freq=%d\n",carrier_freq);
        return -EINVAL;
    }

    // out put log for debug: start
    if (irtx_hal_log_enabled()) {
        for (i = 0; i < (pattern_len >> 3); i++) {
            int ii = (i << 3);
            ALOGD("&pattern[%d]:0x%p - %d, %d, %d, %d, %d, %d, %d, %d\n", ii, &(__pattern[ii]),
            __pattern[ii + 0], __pattern[ii + 1], __pattern[ii + 2], __pattern[ii + 3],
            __pattern[ii + 4], __pattern[ii + 5], __pattern[ii + 6], __pattern[ii + 7]);
        }
        i = (i << 3);
        for (; i < pattern_len; i++) {
            ALOGD("0x%x, ", __pattern[i]);
        }
        ALOGD("\n");
    }
    // out put log for debug: end

    fd = open(TX_DRIVER_PATH, O_RDWR);
    if (fd < 0) {
        ALOGE("file open fail, errno=%d\n", errno);
        return fd;
    }

    ret = ioctl(fd, IRTX_IOC_GET_SOLUTTION_TYPE, &solution_type);
    if(ret == -1) {
        if (irtx_hal_log_enabled()) {
            ALOGD("Ioctl fail, solution type is set to 0\n");
        }
        solution_type = 0;
    }

    // Generate Signals
    ret = signals_generate(solution_type, carrier_freq, pattern, pattern_len, &wave_buffer, &buffer_len);
    if(ret < 0) {
        ALOGE(">>> Generate Signals Fail <<<\n");
        goto exit;
    }

    ALOGD("IRTX: Send to driver : Enter");
    write_size = write(fd, (char *)wave_buffer, buffer_len * 4);
    ALOGD("IRTX: Send to driver : Exit");

    if (write_size >= 0)
        ret = 0;
    // out put log for debug: start
    if (irtx_hal_log_enabled()) {
        ALOGD("Converted Length : %d, data:\n", buffer_len);
        for (i = 0; i < (buffer_len >> 3); i++) {
            int ii = (i << 3);
            ALOGD("&wave_buffer[%d]:%p - 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n", ii, &(wave_buffer[ii])
                , wave_buffer[ii + 0], wave_buffer[ii + 1], wave_buffer[ii + 2], wave_buffer[ii + 3]
                , wave_buffer[ii + 4], wave_buffer[ii+ 5], wave_buffer[ii + 6], wave_buffer[ii + 7]);
        }
        i = (i << 3);
        for (; i < buffer_len; i++) {
            ALOGD("0x%x, ", wave_buffer[i]);
        }
        ALOGD("==\n");
    }
    // out put log for debug: end

    if (ret < 0) {
        ALOGE("File write fail, errno=%d\n", errno);
        goto exit;
    }

    exit:
    ALOGD("Done, Turn OFF IRTX\n");
    close(fd);

    if (wave_buffer) {
        free(wave_buffer);
    }

    return ret;
}

/*-------------------------------------------------------------------
 |  Function GET_NUMBER_OF_SUPPORT_CARRIER_FREQS_ARRAY
 |
 |  Purpose:  Return the count of supported carrier frequency array
 |
 |  Parameters:
 |     consumerir_device (IN) -- IRTX hal structure
 |
 |  Returns:  The flag of showing the detail log
 *-------------------------------------------------------------------*/
static int consumerir_get_num_carrier_freqs(struct consumerir_device *dev __unused)
{
    return ARRAY_SIZE(consumerir_freqs);
}

static int consumerir_get_carrier_freqs(struct consumerir_device *dev __unused,
    size_t len, consumerir_freq_range_t *ranges)
{
    size_t to_copy = ARRAY_SIZE(consumerir_freqs);

    to_copy = len < to_copy ? len : to_copy;
    memcpy(ranges, consumerir_freqs, to_copy * sizeof(consumerir_freq_range_t));
    return to_copy;
}

/*-------------------------------------------------------------------
 |  Function CLOSE_HAL
 |
 |  Purpose:  Close HAL
 |
 |  Parameters:
 |     dev (IN) -- HAL handle
 |
 |  Returns:  The status code of this execution.
 *-------------------------------------------------------------------*/
static int consumerir_close(hw_device_t *dev)
{
    free(dev);
    return 0;
}

/*-------------------------------------------------------------------
 |  Function OPEN HAL
 |
 |  Purpose:  OPEN HAL
 |
 |  Parameters:
 |     hw_module_t (IN) -- HAL module
 |     name (IN) -- HAL name
 |     hw_device_t (OUT) -- HAL handler
 |
 |  Returns:  The status code of this execution.
 *-------------------------------------------------------------------*/
static int consumerir_open(const hw_module_t* module, const char* name,
        hw_device_t** device)
{
    if (strcmp(name, CONSUMERIR_TRANSMITTER) != 0) {
        return -EINVAL;
    }
    if (device == NULL) {
        ALOGE("NULL device on open");
        return -EINVAL;
    }

    consumerir_extra_device_t *dev = malloc(sizeof(consumerir_extra_device_t));
    memset(dev, 0, sizeof(consumerir_extra_device_t));
    // HAL Common
    dev->common.common.tag = HARDWARE_DEVICE_TAG;
    dev->common.common.version = 0;
    dev->common.common.module = (struct hw_module_t*) module;
    dev->common.common.close = consumerir_close;
    // IR TX HAL
    dev->common.transmit = consumerir_transmit;
    dev->common.get_num_carrier_freqs = consumerir_get_num_carrier_freqs;
    dev->common.get_carrier_freqs = consumerir_get_carrier_freqs;
    // IR RX HAL
    dev->receive = consumerir_receive;
    *device = (hw_device_t*) dev;

    return 0;
}

static struct hw_module_methods_t consumerir_module_methods = {
    .open = consumerir_open,
};

consumerir_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag                = HARDWARE_MODULE_TAG,
        .module_api_version = CONSUMERIR_MODULE_API_VERSION_1_0,
        .hal_api_version    = HARDWARE_HAL_API_VERSION,
        .id                 = CONSUMERIR_HARDWARE_MODULE_ID,
        .name               = "MTK IR HAL",
        .author             = "WSD/OSS3/SS5",
        .methods            = &consumerir_module_methods,
    },
};
