#include "process.h"

DWORD LaunchInAppContainer(const Config& config, PSID appContainerSid, const SECURITY_CAPABILITIES& capabilities) {
    STARTUPINFOEXW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(STARTUPINFOEXW));
    ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
    si.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    
    SIZE_T attributeListSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeListSize);
    
    si.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(
        HeapAlloc(GetProcessHeap(), 0, attributeListSize)
    );
    
    if (!si.lpAttributeList) {
        LogError(L"LaunchInAppContainer", L"Failed to allocate attribute list");
        return 1;
    }
    
    if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &attributeListSize)) {
        LogError(L"LaunchInAppContainer", L"Failed to initialize attribute list");
        HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
        return 1;
    }
    
    if (!UpdateProcThreadAttribute(
            si.lpAttributeList,
            0,
            PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES,
            const_cast<PSECURITY_CAPABILITIES>(&capabilities),
            sizeof(SECURITY_CAPABILITIES),
            nullptr,
            nullptr)) {
        LogError(L"LaunchInAppContainer", L"Failed to update process attribute");
        DeleteProcThreadAttributeList(si.lpAttributeList);
        HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
        return 1;
    }
    
    std::wstring cmdLine = L"\"" + config.executable + L"\"";
    if (!config.args.empty()) {
        cmdLine += L" " + config.args;
    }
    
    std::wstring workingDir = config.workingDir.empty() ? config.jailDir : config.workingDir;
    
    LPWSTR cmdLineBuffer = new wchar_t[cmdLine.length() + 1];
    wcscpy_s(cmdLineBuffer, cmdLine.length() + 1, cmdLine.c_str());
    
    if (!CreateProcessW(
            nullptr,
            cmdLineBuffer,
            nullptr,
            nullptr,
            FALSE,
            CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT,
            nullptr,
            workingDir.c_str(),
            &si.StartupInfo,
            &pi)) {
        LogError(L"LaunchInAppContainer", L"CreateProcess failed");
        delete[] cmdLineBuffer;
        DeleteProcThreadAttributeList(si.lpAttributeList);
        HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
        return 1;
    }
    
    delete[] cmdLineBuffer;
    
    ResumeThread(pi.hThread);
    
    WaitForSingleObject(pi.hProcess, INFINITE);
    
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    DeleteProcThreadAttributeList(si.lpAttributeList);
    HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
    
    return exitCode;
}
