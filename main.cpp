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

void LogError(const wchar_t* function, const std::wstring& additionalInfo = L"") {
    DWORD errorCode = GetLastError();
    if (errorCode == 0) {
        std::wcerr << L"[ERROR] " << function;
        if (!additionalInfo.empty()) {
            std::wcerr << L": " << additionalInfo;
        }
        std::wcerr << std::endl;
        return;
    }
    
    wchar_t* messageBuffer = nullptr;
    size_t size = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<wchar_t*>(&messageBuffer),
        0,
        nullptr
    );
    
    std::wcerr << L"[ERROR] " << function;
    if (!additionalInfo.empty()) {
        std::wcerr << L": " << additionalInfo;
    }
    std::wcerr << L": " << messageBuffer << L" (Code: 0x" << std::hex << std::setw(8) << std::setfill(L'0') << errorCode << L")" << std::endl;
    
    LocalFree(messageBuffer);
}

void LogVerbose(const std::wstring& message) {
    std::wcout << L"[VERBOSE] " << message << std::endl;
}

bool ParseCommandLine(int argc, wchar_t* argv[], Config& config) {
    config.allowNetwork = false;
    config.cleanup = true;
    config.verbose = false;
    
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        
        if (arg == L"--config") {
            if (++i >= argc) {
                std::wcerr << L"Error: --config requires a path argument" << std::endl;
                return false;
            }
            config.configPath = argv[i];
        } else if (arg == L"--executable") {
            if (++i >= argc) {
                std::wcerr << L"Error: --executable requires a path argument" << std::endl;
                return false;
            }
            config.executable = argv[i];
        } else if (arg == L"--args") {
            if (++i >= argc) {
                std::wcerr << L"Error: --args requires a string argument" << std::endl;
                return false;
            }
            config.args = argv[i];
        } else if (arg == L"--jail-dir") {
            if (++i >= argc) {
                std::wcerr << L"Error: --jail-dir requires a path argument" << std::endl;
                return false;
            }
            config.jailDir = argv[i];
        } else if (arg == L"--container-name") {
            if (++i >= argc) {
                std::wcerr << L"Error: --container-name requires a name argument" << std::endl;
                return false;
            }
            config.containerName = argv[i];
        } else if (arg == L"--allow-network") {
            config.allowNetwork = true;
        } else if (arg == L"--working-dir") {
            if (++i >= argc) {
                std::wcerr << L"Error: --working-dir requires a path argument" << std::endl;
                return false;
            }
            config.workingDir = argv[i];
        } else if (arg == L"--cleanup") {
            config.cleanup = true;
        } else if (arg == L"--no-cleanup") {
            config.cleanup = false;
        } else if (arg == L"--verbose") {
            config.verbose = true;
        } else {
            std::wcerr << L"Error: Unknown argument: " << arg << std::endl;
            return false;
        }
    }
    
    return true;
}
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
