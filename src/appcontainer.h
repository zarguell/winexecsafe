#ifndef WINEXECSAFE_APPCONTAINER_H
#define WINEXECSAFE_APPCONTAINER_H

#include "common.h"

PSID CreateOrGetAppContainerProfile(const std::wstring& containerName);
bool DeleteAppContainerProfile(const std::wstring& containerName);

#endif // WINEXECSAFE_APPCONTAINER_H