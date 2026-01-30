#include "appcontainer.h"

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
