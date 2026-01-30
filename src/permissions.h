#ifndef WINEXECSAFE_PERMISSIONS_H
#define WINEXECSAFE_PERMISSIONS_H

#include "common.h"

bool GrantDirectoryAccess(const std::wstring& directory, PSID appContainerSid, DWORD accessMask);
bool GrantReadAccessToSystemDirs(PSID appContainerSid, const Config& config);

#endif // WINEXECSAFE_PERMISSIONS_H