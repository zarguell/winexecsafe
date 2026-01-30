#ifndef WINEXECSAFE_CAPABILITIES_H
#define WINEXECSAFE_CAPABILITIES_H

#include "common.h"

bool BuildCapabilities(const Config& config, PSID appContainerSid, SECURITY_CAPABILITIES& capabilities);
void CleanupCapabilities(SECURITY_CAPABILITIES& capabilities);

#endif // WINEXECSAFE_CAPABILITIES_H