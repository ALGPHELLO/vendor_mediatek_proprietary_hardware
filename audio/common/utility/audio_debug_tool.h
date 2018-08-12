#ifndef AUDIO_DEBUG_TOOL_H
#define AUDIO_DEBUG_TOOL_H

#include <audio_log.h>

#ifndef MTK_AUDIO_DEBUG_TOOL_ENABLE
#define AUDIO_DEBUG_CHECK_POINT(x...)
#else
#define DEBUG_TAG "[AUDIO_DEBUG] "

#define AUDIO_DEBUG_CHECK_POINT() \
    do { \
        AUD_LOG_E(DEBUG_TAG"%s, %s(), %uL", \
                  strrchr(__FILE__, '/') + 1, __FUNCTION__, __LINE__); \
    } while(0)
#endif



#endif /* AUDIO_DEBUG_TOOL_H */

