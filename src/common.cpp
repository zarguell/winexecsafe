#include "common.h"
#include <iostream>
#include <iomanip>

void LogError(const wchar_t* function, const std::wstring& additionalInfo) {
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