// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "FileDescriptorInternalWindows.h"
#include "Process.h"

#include <Windows.h>

struct SC::ProcessEntry::Internal
{
    // TODO: this could be migrated to SystemDebug
    static void       exit(int code) { _exit(code); }
    static ReturnCode ProcessHandleClose(const HANDLE& handle)
    {
        if (::CloseHandle(handle) == FALSE)
            return "ProcessHandleClose - CloseHandle failed"_a8;
        return true;
    }
};

struct SC::ProcessEntry::ProcessHandle
    : public MovableHandle<HANDLE, nullptr, ReturnCode, &Internal::ProcessHandleClose>
{
};
SC::ReturnCode SC::ProcessEntry::waitProcessExit()
{
    HANDLE handle;
    SC_TRY_IF(processHandlePimpl.get().get(handle, ReturnCode("ProcesEntry::waitProcessExit - Invalid handle"_a8)));
    WaitForSingleObject(handle, INFINITE);
    DWORD processStatus;
    if (GetExitCodeProcess(handle, &processStatus))
    {
        exitStatus.value = static_cast<int32_t>(processStatus);
        return true;
    }
    return "ProcessEntry::wait - GetExitCodeProcess failed"_a8;
}

// https://learn.microsoft.com/en-us/windows/win32/procthread/creating-a-child-process-with-redirected-input-and-output
SC::ReturnCode SC::ProcessEntry::run(const ProcessOptions& options)
{
    STARTUPINFO startupInfo;
    const bool  someRedirection = standardInput.isValid() || standardOutput.isValid() || standardError.isValid();
    const BOOL  inheritHandles  = options.inheritFileDescriptors or someRedirection ? TRUE : FALSE;

    DWORD creationFlags = 0; // CREATE_UNICODE_ENVIRONMENT;
    ZeroMemory(&startupInfo, sizeof(STARTUPINFO));
    startupInfo.cb         = sizeof(STARTUPINFO);
    startupInfo.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
    startupInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startupInfo.hStdError  = GetStdHandle(STD_ERROR_HANDLE);

    if (standardInput.isValid())
    {
        SC_TRY_IF(standardInput.fileNativeHandle.get().get(startupInfo.hStdInput, false));
    }
    if (standardOutput.isValid())
    {
        SC_TRY_IF(standardOutput.fileNativeHandle.get().get(startupInfo.hStdOutput, false));
    }
    if (standardError.isValid())
    {
        SC_TRY_IF(standardError.fileNativeHandle.get().get(startupInfo.hStdError, false));
    }
    if (someRedirection)
    {
        startupInfo.dwFlags |= STARTF_USESTDHANDLES;
    }

    // In documentation it's explicitly stated that this buffer will be modified (!?)
    LPWSTR  wideCmd = command.text.nativeWritableBytesIncludingTerminator();
    LPCWSTR wideDir = currentDirectory.view().isEmpty() ? nullptr : currentDirectory.view().getNullTerminatedNative();
    LPWSTR wideEnv = environment.view().isEmpty() ? nullptr : environment.text.nativeWritableBytesIncludingTerminator();
    PROCESS_INFORMATION processInfo;
    ZeroMemory(&processInfo, sizeof(PROCESS_INFORMATION));
    BOOL success;
    success = CreateProcessW(nullptr,        // [in, optional]      LPCWSTR               lpApplicationName,
                             wideCmd,        // [in, out, optional] LPWSTR                lpCommandLine,
                             nullptr,        // [in, optional]      LPSECURITY_ATTRIBUTES lpProcessAttributes,
                             nullptr,        // [in, optional]      LPSECURITY_ATTRIBUTES lpThreadAttributes,
                             inheritHandles, // [in]                BOOL                  bInheritHandles,
                             creationFlags,  // [in]                DWORD                 dwCreationFlags,
                             wideEnv,        // [in, optional]      LPVOID                lpEnvironment,
                             wideDir,        // [in, optional]      LPCWSTR               lpCurrentDirectory,
                             &startupInfo,   // [in]                LPSTARTUPINFOW        lpStartupInfo,
                             &processInfo);  // [out]               PROCESSINFORMATION    lpProcessInformation

    if (not success)
    {
        return "CreateProcessW failed"_a8;
    }
    CloseHandle(processInfo.hThread);

    SC_TRY_ASSIGN(processID.pid, processInfo.dwProcessId, "processInfo.dwProcessId exceeds processID.pid"_a8);
    SC_TRY_IF(processHandlePimpl.get().assign(processInfo.hProcess));
    SC_TRY_IF(standardInput.close());
    SC_TRY_IF(standardOutput.close());
    SC_TRY_IF(standardError.close());
    return true;
}
