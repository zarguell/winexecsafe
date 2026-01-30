#include "permissions.h"
#include <aclapi.h>

bool GrantDirectoryAccess(const std::wstring& directory, PSID appContainerSid, DWORD accessMask) {
    PACL oldDacl = nullptr;
    PSECURITY_DESCRIPTOR sd = nullptr;
    
    DWORD result = GetNamedSecurityInfoW(
        directory.c_str(),
        SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION,
        nullptr,
        nullptr,
        &oldDacl,
        nullptr,
        &sd
    );
    
    if (result != ERROR_SUCCESS) {
        SetLastError(result);
        LogError(L"GrantDirectoryAccess", L"GetNamedSecurityInfo failed for: " + directory);
        return false;
    }
    
    EXPLICIT_ACCESS ea = {0};
    ea.grfAccessPermissions = accessMask;
    ea.grfAccessMode = SET_ACCESS;
    ea.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
    ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea.Trustee.TrusteeType = TRUSTEE_IS_UNKNOWN;
    ea.Trustee.ptstrName = reinterpret_cast<LPWSTR>(appContainerSid);
    
    PACL newDacl = nullptr;
    result = SetEntriesInAcl(1, &ea, oldDacl, &newDacl);
    if (result != ERROR_SUCCESS) {
        SetLastError(result);
        LogError(L"GrantDirectoryAccess", L"SetEntriesInAcl failed for: " + directory);
        LocalFree(sd);
        return false;
    }
    
    result = SetNamedSecurityInfoW(
        const_cast<LPWSTR>(directory.c_str()),
        SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION,
        nullptr,
        nullptr,
        newDacl,
        nullptr
    );
    
    LocalFree(newDacl);
    LocalFree(sd);
    
    if (result != ERROR_SUCCESS) {
        SetLastError(result);
        LogError(L"GrantDirectoryAccess", L"SetNamedSecurityInfo failed for: " + directory);
        return false;
    }
    
    return true;
}

bool GrantReadAccessToSystemDirs(PSID appContainerSid, const Config& config) {
    // Only grant access to user-specified additional paths.
    // System directories like C:\Windows\System32 are already accessible to
    // AppContainers via inherited permissions, and modifying their ACLs requires admin.
    
    bool allSucceeded = true;
    for (const auto& path : config.additionalReadPaths) {
        DWORD attr = GetFileAttributesW(path.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
            if (!GrantDirectoryAccess(path, appContainerSid, GENERIC_READ | GENERIC_EXECUTE)) {
                LogError(L"GrantReadAccessToSystemDirs", L"Failed to grant read access to: " + path);
                allSucceeded = false;
            }
        }
    }
    
    return allSucceeded;
}
