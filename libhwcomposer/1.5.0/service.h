#ifndef HWC_SERVICE_H_
#define HWC_SERVICE_H_

#include <utils/Singleton.h>

#include <dfps/FpsPolicyService.h>

using namespace android;

class ServiceManager : public Singleton<ServiceManager>
{
public:
    ServiceManager();
    ~ServiceManager();

    void init();
private:
    sp<FpsPolicyService> m_fps_policy_service;
};

#endif
