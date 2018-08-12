#define DEBUG_LOG_TAG "SERVICE"

#include "service.h"

#include <binder/IServiceManager.h>

ANDROID_SINGLETON_STATIC_INSTANCE(ServiceManager);

ServiceManager::ServiceManager()
{
}

ServiceManager::~ServiceManager()
{
}

void ServiceManager::init()
{
}
