#ifndef WINEXECSAFE_PROCESS_H
#define WINEXECSAFE_PROCESS_H

#include "common.h"

DWORD LaunchInAppContainer(const Config& config, PSID appContainerSid, const SECURITY_CAPABILITIES& capabilities);

#endif // WINEXECSAFE_PROCESS_H