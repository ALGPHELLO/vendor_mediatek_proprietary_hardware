#include <SpeechUtility.h>

#include <string.h>
#include <stdarg.h> /* va_list, va_start, va_arg, va_end */

#include <cutils/properties.h>

#include <audio_log.h>



#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "SpeechUtility"


namespace android {


/* dynamic enable log */
static const char kPropertyKeySpeechLogMask[PROPERTY_KEY_MAX] = "af.speech.log.mask";


void sph_memcpy(void *des, const void *src, uint32_t size) {
    char *p_src = (char *)src;
    char *p_des = (char *)des;
    uint32_t i = 0;

    for (i = 0; i < size; i++) {
        p_des[i] = p_src[i];
        asm("" ::: "memory");
    }
    asm volatile("dsb ish": : : "memory");
}


void sph_memset(void *dest, uint8_t value, uint32_t size) {
    char *p_des = (char *)dest;
    uint32_t i = 0;

    for (i = 0; i < size; i++) {
        p_des[i] = value;
        asm("" ::: "memory");
    }
    asm volatile("dsb ish": : : "memory");
}


uint32_t get_uint32_from_property(const char *property_name) {
    uint32_t retval = 0;
    char property_value[PROPERTY_VALUE_MAX];
    property_get(property_name, property_value, "0"); // default 0
    sscanf(property_value, "%u", &retval);
    return retval;
}


void set_uint32_to_property(const char *property_name, const uint32_t value) {
    if (!property_name) {
        return;
    }

    char property_value[PROPERTY_VALUE_MAX];
    snprintf(property_value, sizeof(property_value), "%u", value);
    property_set(property_name, property_value);
}


void get_string_from_property(const char *property_name, char *string, const uint32_t string_size) {
    if (!property_name || !string || !string_size) {
        return;
    }

    char property_string[PROPERTY_VALUE_MAX] = {0};
    property_get(property_name, property_string, ""); // default none
    strncpy(string, property_string, string_size - 1);
}


void set_string_to_property(const char *property_name, const char *string) {
    char property_string[PROPERTY_VALUE_MAX] = {0};
    strncpy(property_string, string, sizeof(property_string) - 1);
    property_set(property_name, property_string);
}


uint16_t sph_sample_rate_enum_to_value(const sph_sample_rate_t sample_rate_enum) {
    uint16_t sample_rate_value = 32000;

    switch (sample_rate_enum) {
    case SPH_SAMPLE_RATE_08K:
        sample_rate_value = 8000;
        break;
    case SPH_SAMPLE_RATE_16K:
        sample_rate_value = 16000;
        break;
    case SPH_SAMPLE_RATE_32K:
        sample_rate_value = 32000;
        break;
    case SPH_SAMPLE_RATE_48K:
        sample_rate_value = 48000;
        break;
    default:
        ALOGW("%s(), sample_rate_enum %d not support!! use 32000 instead",
              __FUNCTION__, sample_rate_enum);
        sample_rate_value = 32000;
    }

    return sample_rate_value;
}


sph_sample_rate_t sph_sample_rate_value_to_enum(const uint16_t sample_rate_value) {
    sph_sample_rate_t sample_rate_enum = SPH_SAMPLE_RATE_32K;

    switch (sample_rate_value) {
    case 8000:
        sample_rate_enum = SPH_SAMPLE_RATE_08K;
        break;
    case 16000:
        sample_rate_enum = SPH_SAMPLE_RATE_16K;
        break;
    case 32000:
        sample_rate_enum = SPH_SAMPLE_RATE_32K;
        break;
    case 48000:
        sample_rate_enum = SPH_SAMPLE_RATE_48K;
        break;
    default:
        ALOGW("%s(), sample_rate_value %d not support!! use 32000 instead",
              __FUNCTION__, sample_rate_value);
        sample_rate_enum = SPH_SAMPLE_RATE_32K;
    }

    return sample_rate_enum;
}


void dynamic_speech_log(uint32_t sph_log_level_mask, const char *file_path, const char *message, ...) {
    if (!file_path || !message) {
        return;
    }

    if ((sph_log_level_mask & get_uint32_from_property(kPropertyKeySpeechLogMask)) == 0) {
        return;
    }

    char printf_msg[256];
    const char *slash = strrchr(file_path, '/');
    const char *file_name = (slash) ? slash + 1 : file_path;

    va_list args;
    va_start(args, message);
    vsnprintf(printf_msg, sizeof(printf_msg), message, args);
    ALOGD("[%s] %s", file_name, printf_msg);
    va_end(args);
}


} /* end namespace android */

