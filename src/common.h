#ifndef WINEXECSAFE_COMMON_H
#define WINEXECSAFE_COMMON_H

#include <windows.h>
#include <string>
#include <vector>

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
std::wstring TrimString(const std::wstring& str);
bool ParseBool(const std::wstring& value);

#endif // WINEXECSAFE_COMMON_H