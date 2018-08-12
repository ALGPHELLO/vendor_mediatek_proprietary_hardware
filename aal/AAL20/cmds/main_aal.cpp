#define LOG_TAG "AAL"

#define MTK_LOG_ENABLE 1
#include <cutils/log.h>
#include <binder/BinderService.h>
#include <AAL20/AALService.h>
#include <iostream>
#include <stdexcept>


using namespace android;

int main(int argc, char** argv)
{
    ALOGD("AAL service start...");

    try {
        AALService::publishAndJoinThreadPool(true);
        // When AAL is launched in its own process, limit the number of
        // binder threads to 4.
        ProcessState::self()->setThreadPoolMaxThreadCount(4);
    } catch (const std::ios_base::failure & e) {
        ALOGE("AAL service start fail");
    }

    ALOGD("AAL service exit...");
    return 0;
}
