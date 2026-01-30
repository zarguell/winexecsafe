#include <windows.h>
#include <userenv.h>
#include <sddl.h>
#include <aclapi.h>
#include <shlobj.h>
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "userenv.lib")
#pragma comment(lib, "advapi32.lib")

struct Config {
    std::wstring configPath;
    std::wstring executable;
    std::wstring args;
    std::wstring jailDir;
    std::wstring containerName;
    std::wstring workingDir;
    std::vector<std::wstring> additionalReadPaths;
    bool allowNetwork;
    bool cleanup;
    bool verbose;
    std::wstring logFile;
};

class ScopedHandle {
public:
    explicit ScopedHandle(HANDLE h = nullptr) : handle_(h) {}
    ~ScopedHandle() { if (handle_ && handle_ != INVALID_HANDLE_VALUE) CloseHandle(handle_); }
    HANDLE get() const { return handle_; }
    HANDLE release() { HANDLE h = handle_; handle_ = nullptr; return h; }
private:
    HANDLE handle_;
};

void LogError(const wchar_t* function, const std::wstring& additionalInfo = L"");
void LogVerbose(const std::wstring& message);
bool ParseCommandLine(int argc, wchar_t* argv[], Config& config);
bool LoadConfigFile(const std::wstring& path, Config& config);
void MergeConfiguration(const Config& cliConfig, Config& mergedConfig);
bool ValidateConfiguration(const Config& config);
PSID CreateOrGetAppContainerProfile(const std::wstring& containerName);
bool DeleteAppContainerProfile(const std::wstring& containerName);
bool GrantDirectoryAccess(const std::wstring& directory, PSID appContainerSid, DWORD accessMask);
bool GrantReadAccessToSystemDirs(PSID appContainerSid, const Config& config);
bool BuildCapabilities(const Config& config, PSID appContainerSid, SECURITY_CAPABILITIES& capabilities);
DWORD LaunchInAppContainer(const Config& config, PSID appContainerSid, const SECURITY_CAPABILITIES& capabilities);
void CleanupCapabilities(SECURITY_CAPABILITIES& capabilities);

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
            return 1;
        }
        LogVerbose(L"Granted read access to system directories");
        
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
