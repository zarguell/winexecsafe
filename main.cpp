#include <windows.h>
#include <userenv.h>
#include <iostream>
#include "src/config.h"
#include "src/appcontainer.h"
#include "src/permissions.h"
#include "src/capabilities.h"
#include "src/process.h"

#pragma comment(lib, "userenv.lib")
#pragma comment(lib, "advapi32.lib")

int wmain(int argc, wchar_t* argv[]) {
    Config config;
    
    if (!ParseCommandLine(argc, argv, config)) {
        return 1;
    }
    
    Config mergedConfig = config;
    if (!config.configPath.empty()) {
        LoadConfigFile(config.configPath, mergedConfig);
    } else {
        std::wstring defaultConfig = L"C:\\ProgramData\\winexecsafe\\config.ini";
        LoadConfigFile(defaultConfig, mergedConfig);
    }
    
    MergeConfiguration(config, mergedConfig);
    
    if (!ValidateConfiguration(mergedConfig)) {
        return 1;
    }
    
    PSID appContainerSid = nullptr;
    try {
        appContainerSid = CreateOrGetAppContainerProfile(mergedConfig.containerName);
        if (!appContainerSid) {
            LogError(L"CreateOrGetAppContainerProfile");
            return 1;
        }
        LogVerbose(L"AppContainer profile created/retrieved: " + mergedConfig.containerName);
        
        if (!GrantDirectoryAccess(mergedConfig.jailDir, appContainerSid, GENERIC_READ | GENERIC_WRITE | GENERIC_EXECUTE)) {
            LogError(L"GrantDirectoryAccess", L"jail directory: " + mergedConfig.jailDir);
            return 1;
        }
        LogVerbose(L"Granted access to jail directory: " + mergedConfig.jailDir);
        
        if (!GrantReadAccessToSystemDirs(appContainerSid, mergedConfig)) {
            LogError(L"GrantReadAccessToSystemDirs");
        }
        LogVerbose(L"Granted read access to system directories (non-fatal)");
        
        SECURITY_CAPABILITIES capabilities = {0};
        if (!BuildCapabilities(mergedConfig, appContainerSid, capabilities)) {
            LogError(L"BuildCapabilities");
            return 1;
        }
        LogVerbose(L"Built security capabilities");
        
        DWORD exitCode = LaunchInAppContainer(mergedConfig, appContainerSid, capabilities);
        
        CleanupCapabilities(capabilities);
        
        if (mergedConfig.cleanup) {
            DeleteAppContainerProfile(mergedConfig.containerName);
            LogVerbose(L"Cleaned up AppContainer profile");
        }
        
        return exitCode;
        
    } catch (...) {
        LogError(L"wmain", L"Unhandled exception");
        if (appContainerSid) FreeSid(appContainerSid);
        return 1;
    }
    
    if (appContainerSid) FreeSid(appContainerSid);
    return 0;
}