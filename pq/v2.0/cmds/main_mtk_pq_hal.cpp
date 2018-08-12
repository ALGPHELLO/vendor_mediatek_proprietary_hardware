#define LOG_TAG "vendor.mediatek.hardware.pq@2.0-service"

#define MTK_LOG_ENABLE 1

#include <iostream>
#include <cutils/log.h>
#include <vendor/mediatek/hardware/pq/2.0/IPictureQuality.h>
#include <hidl/LegacySupport.h>

using vendor::mediatek::hardware::pq::V2_0::IPictureQuality;
using android::hardware::defaultPassthroughServiceImplementation;

int main()
{
    int ret = 0;

    try {
        ret = defaultPassthroughServiceImplementation<IPictureQuality>(4);
    } catch (const std::__1::system_error & e) {

    }

    return ret;
}
