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

std::wstring TrimString(const std::wstring& str) {
    size_t first = str.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos) return L"";
    size_t last = str.find_last_not_of(L" \t\r\n");
    return str.substr(first, last - first + 1);
}

bool ParseBool(const std::wstring& value) {
    std::wstring lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
    if (lower == L"true" || lower == L"yes" || lower == L"1") return true;
    if (lower == L"false" || lower == L"no" || lower == L"0") return false;
    return false;
}

bool CreateProgramDataDirectory() {
    std::wstring dir = L"C:\\ProgramData\\winexecsafe";
    DWORD attr = GetFileAttributesW(dir.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) {
        if (!CreateDirectoryW(dir.c_str(), nullptr)) {
            DWORD error = GetLastError();
            if (error != ERROR_ALREADY_EXISTS) {
                LogError(L"CreateProgramDataDirectory", L"Failed to create directory: " + dir);
                return false;
            }
        }
    }
    return true;
}

bool LoadConfigFile(const std::wstring& path, Config& config) {
    if (path.empty()) return false;
    
    DWORD attr = GetFileAttributesW(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND) {
            return false;
        }
        LogError(L"LoadConfigFile", L"Failed to access config file: " + path);
        return false;
    }
    
    wchar_t buffer[4096];
    DWORD result = GetPrivateProfileStringW(L"General", L"Executable", L"", buffer, sizeof(buffer) / sizeof(wchar_t), path.c_str());
    if (result > 0) {
        std::wstring value = buffer;
        wchar_t expanded[MAX_PATH];
        ExpandEnvironmentStringsW(value.c_str(), expanded, MAX_PATH);
        config.executable = expanded;
    }
    
    result = GetPrivateProfileStringW(L"General", L"Arguments", L"", buffer, sizeof(buffer) / sizeof(wchar_t), path.c_str());
    if (result > 0) {
        config.args = TrimString(buffer);
    }
    
    result = GetPrivateProfileStringW(L"General", L"JailDirectory", L"", buffer, sizeof(buffer) / sizeof(wchar_t), path.c_str());
    if (result > 0) {
        std::wstring value = buffer;
        wchar_t expanded[MAX_PATH];
        ExpandEnvironmentStringsW(value.c_str(), expanded, MAX_PATH);
        config.jailDir = expanded;
    }
    
    result = GetPrivateProfileStringW(L"General", L"WorkingDirectory", L"", buffer, sizeof(buffer) / sizeof(wchar_t), path.c_str());
    if (result > 0) {
        std::wstring value = buffer;
        wchar_t expanded[MAX_PATH];
        ExpandEnvironmentStringsW(value.c_str(), expanded, MAX_PATH);
        config.workingDir = expanded;
    }
    
    result = GetPrivateProfileStringW(L"General", L"ContainerName", L"", buffer, sizeof(buffer) / sizeof(wchar_t), path.c_str());
    if (result > 0) {
        config.containerName = TrimString(buffer);
    }
    
    result = GetPrivateProfileStringW(L"General", L"AllowNetwork", L"false", buffer, sizeof(buffer) / sizeof(wchar_t), path.c_str());
    config.allowNetwork = ParseBool(buffer);
    
    result = GetPrivateProfileStringW(L"General", L"Cleanup", L"true", buffer, sizeof(buffer) / sizeof(wchar_t), path.c_str());
    config.cleanup = ParseBool(buffer);
    
    result = GetPrivateProfileStringW(L"Permissions", L"AdditionalReadPaths", L"", buffer, sizeof(buffer) / sizeof(wchar_t), path.c_str());
    if (result > 0) {
        std::wstring paths = buffer;
        size_t pos = 0;
        while (pos < paths.length()) {
            size_t sep = paths.find(L';', pos);
            std::wstring pathItem = (sep == std::wstring::npos) ? paths.substr(pos) : paths.substr(pos, sep - pos);
            pathItem = TrimString(pathItem);
            if (!pathItem.empty()) {
                wchar_t expanded[MAX_PATH];
                ExpandEnvironmentStringsW(pathItem.c_str(), expanded, MAX_PATH);
                config.additionalReadPaths.push_back(expanded);
            }
            pos = (sep == std::wstring::npos) ? paths.length() : sep + 1;
        }
    }
    
    result = GetPrivateProfileStringW(L"Logging", L"Verbose", L"false", buffer, sizeof(buffer) / sizeof(wchar_t), path.c_str());
    config.verbose = ParseBool(buffer);
    
    result = GetPrivateProfileStringW(L"Logging", L"LogFile", L"", buffer, sizeof(buffer) / sizeof(wchar_t), path.c_str());
    if (result > 0) {
        std::wstring value = buffer;
        wchar_t expanded[MAX_PATH];
        ExpandEnvironmentStringsW(value.c_str(), expanded, MAX_PATH);
        config.logFile = expanded;
    }
    
    return true;
}

void MergeConfiguration(const Config& cliConfig, Config& mergedConfig) {
    if (!cliConfig.configPath.empty()) mergedConfig.configPath = cliConfig.configPath;
    if (!cliConfig.executable.empty()) mergedConfig.executable = cliConfig.executable;
    if (!cliConfig.args.empty()) mergedConfig.args = cliConfig.args;
    if (!cliConfig.jailDir.empty()) mergedConfig.jailDir = cliConfig.jailDir;
    if (!cliConfig.containerName.empty()) mergedConfig.containerName = cliConfig.containerName;
    if (!cliConfig.workingDir.empty()) mergedConfig.workingDir = cliConfig.workingDir;
    if (!cliConfig.logFile.empty()) mergedConfig.logFile = cliConfig.logFile;
    if (cliConfig.allowNetwork) mergedConfig.allowNetwork = true;
    if (!cliConfig.cleanup) mergedConfig.cleanup = false;
    if (cliConfig.verbose) mergedConfig.verbose = true;
}

bool ValidateConfiguration(const Config& config) {
    if (config.executable.empty()) {
        std::wcerr << L"Error: Executable path is required (use --executable or config file)" << std::endl;
        return false;
    }
    
    DWORD attr = GetFileAttributesW(config.executable.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) {
        std::wcerr << L"Error: Executable file does not exist: " << config.executable << std::endl;
        return false;
    }
    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        std::wcerr << L"Error: Executable path is a directory, not a file: " << config.executable << std::endl;
        return false;
    }
    
    if (config.jailDir.empty()) {
        std::wcerr << L"Error: Jail directory is required (use --jail-dir or config file)" << std::endl;
        return false;
    }
    
    if (config.jailDir[0] != L'\\' && config.jailDir[1] != L':') {
        std::wcerr << L"Error: Jail directory must be an absolute path: " << config.jailDir << std::endl;
        return false;
    }
    
    attr = GetFileAttributesW(config.jailDir.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) {
        if (!CreateDirectoryW(config.jailDir.c_str(), nullptr)) {
            std::wcerr << L"Error: Failed to create jail directory: " << config.jailDir << std::endl;
            return false;
        }
    } else if (!(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        std::wcerr << L"Error: Jail directory path is not a directory: " << config.jailDir << std::endl;
        return false;
    }
    
    std::wstring workingDir = config.workingDir.empty() ? config.jailDir : config.workingDir;
    if (workingDir[0] != L'\\' && workingDir[1] != L':') {
        std::wcerr << L"Error: Working directory must be an absolute path: " << workingDir << std::endl;
        return false;
    }
    
    attr = GetFileAttributesW(workingDir.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) {
        if (!CreateDirectoryW(workingDir.c_str(), nullptr)) {
            std::wcerr << L"Error: Failed to create working directory: " << workingDir << std::endl;
            return false;
        }
    }
    
    return true;
}

PSID CreateOrGetAppContainerProfile(const std::wstring& containerName) {
    if (containerName.empty()) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        wchar_t name[64];
        swprintf_s(name, L"WinExecSafe_%04d%02d%02d_%02d%02d%02d",
                   st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        
        std::wstring displayName = L"WinExecSafe Container - ";
        displayName += name;
        
        PSID appContainerSid = nullptr;
        HRESULT hr = CreateAppContainerProfile(
            name,
            displayName.c_str(),
            L"Container created by WinExecSafe for process isolation",
            nullptr,
            0,
            &appContainerSid
        );
        
        if (FAILED(hr)) {
            LogError(L"CreateOrGetAppContainerProfile", L"CreateAppContainerProfile failed");
            return nullptr;
        }
        
        return appContainerSid;
    }
    
    PSID appContainerSid = nullptr;
    HRESULT hr = CreateAppContainerProfile(
        containerName.c_str(),
        containerName.c_str(),
        L"Container created by WinExecSafe for process isolation",
        nullptr,
        0,
        &appContainerSid
    );
    
    if (hr == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)) {
        hr = DeriveAppContainerSidFromAppContainerName(containerName.c_str(), &appContainerSid);
        if (FAILED(hr)) {
            LogError(L"CreateOrGetAppContainerProfile", L"DeriveAppContainerSidFromAppContainerName failed");
            return nullptr;
        }
    } else if (FAILED(hr)) {
        LogError(L"CreateOrGetAppContainerProfile", L"CreateAppContainerProfile failed");
        return nullptr;
    }
    
    return appContainerSid;
}

bool DeleteAppContainerProfile(const std::wstring& containerName) {
    if (containerName.empty()) {
        return true;
    }
    
    PSID sid = nullptr;
    HRESULT hr = DeriveAppContainerSidFromAppContainerName(containerName.c_str(), &sid);
    if (FAILED(hr)) {
        if (hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND)) {
            return true;
        }
        LogError(L"DeleteAppContainerProfile", L"DeriveAppContainerSidFromAppContainerName failed");
        return false;
    }
    
    hr = DeleteAppContainerProfile(containerName.c_str());
    if (FAILED(hr) && hr != HRESULT_FROM_WIN32(ERROR_NOT_FOUND)) {
        LogError(L"DeleteAppContainerProfile", L"DeleteAppContainerProfile failed");
        FreeSid(sid);
        return false;
    }
    
    FreeSid(sid);
    return true;
}

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
    std::vector<std::wstring> readPaths;
    readPaths.push_back(L"C:\\Windows\\System32");
    readPaths.push_back(L"C:\\Windows\\SysWOW64");
    
    for (const auto& path : config.additionalReadPaths) {
        DWORD attr = GetFileAttributesW(path.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
            readPaths.push_back(path);
        }
    }
    
    for (const auto& path : readPaths) {
        if (!GrantDirectoryAccess(path, appContainerSid, GENERIC_READ | GENERIC_EXECUTE)) {
            LogError(L"GrantReadAccessToSystemDirs", L"Failed to grant read access to: " + path);
            return false;
        }
    }
    
    return true;
}

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

DWORD LaunchInAppContainer(const Config& config, PSID appContainerSid, const SECURITY_CAPABILITIES& capabilities) {
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
