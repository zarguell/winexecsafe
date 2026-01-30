#include "capabilities.h"

bool BuildCapabilities(const Config& config, PSID appContainerSid, SECURITY_CAPABILITIES& capabilities) {
    ZeroMemory(&capabilities, sizeof(SECURITY_CAPABILITIES));
    capabilities.AppContainerSid = appContainerSid;
    capabilities.Capabilities = nullptr;
    capabilities.CapabilityCount = 0;
    capabilities.Reserved = 0;
    
    if (!config.allowNetwork) {
        return true;
    }
    
    SID_IDENTIFIER_AUTHORITY internetClientAuthority = SECURITY_APP_PACKAGE_AUTHORITY;
    PSID internetClientSid = nullptr;
    PSID internetClientServerSid = nullptr;
    
    if (!AllocateAndInitializeSid(&internetClientAuthority, 1, 
                                   SECURITY_CAPABILITY_INTERNET_CLIENT, 
                                   0, 0, 0, 0, 0, 0, 0, 
                                   &internetClientSid)) {
        LogError(L"BuildCapabilities", L"Failed to allocate Internet Client SID");
        return false;
    }
    
    if (!AllocateAndInitializeSid(&internetClientAuthority, 1, 
                                   SECURITY_CAPABILITY_INTERNET_CLIENT_SERVER, 
                                   0, 0, 0, 0, 0, 0, 0, 
                                   &internetClientServerSid)) {
        LogError(L"BuildCapabilities", L"Failed to allocate Internet Client Server SID");
        FreeSid(internetClientSid);
        return false;
    }
    
    PSID_AND_ATTRIBUTES capabilityArray = reinterpret_cast<PSID_AND_ATTRIBUTES>(
        LocalAlloc(LPTR, 2 * sizeof(SID_AND_ATTRIBUTES))
    );
    
    if (!capabilityArray) {
        LogError(L"BuildCapabilities", L"Failed to allocate capability array");
        FreeSid(internetClientSid);
        FreeSid(internetClientServerSid);
        return false;
    }
    
    capabilityArray[0].Sid = internetClientSid;
    capabilityArray[0].Attributes = SE_GROUP_ENABLED;
    
    capabilityArray[1].Sid = internetClientServerSid;
    capabilityArray[1].Attributes = SE_GROUP_ENABLED;
    
    capabilities.Capabilities = capabilityArray;
    capabilities.CapabilityCount = 2;
    
    return true;
}

void CleanupCapabilities(SECURITY_CAPABILITIES& capabilities) {
    if (capabilities.Capabilities) {
        for (DWORD i = 0; i < capabilities.CapabilityCount; ++i) {
            if (capabilities.Capabilities[i].Sid) {
                FreeSid(capabilities.Capabilities[i].Sid);
            }
        }
        LocalFree(capabilities.Capabilities);
        capabilities.Capabilities = nullptr;
        capabilities.CapabilityCount = 0;
    }
}
