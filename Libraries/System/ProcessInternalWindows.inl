// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Process.h"

#if SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdlib.h> // _exit
#endif

struct SC::Process::Internal
{
    // TODO: this could be migrated to SystemDebug
    static void exit(int code) { _exit(code); }
};

SC::ReturnCode SC::ProcessNativeHandleClose(HANDLE& handle)
{
    if (::CloseHandle(handle) == FALSE)
        return "ProcessNativeHandleClose - CloseHandle failed"_a8;
    return true;
}

SC::ReturnCode SC::Process::waitForExitSync()
{
    HANDLE hProcess;
    SC_TRY_IF(handle.get(hProcess, ReturnCode("ProcesEntry::waitProcessExit - Invalid handle"_a8)));
    WaitForSingleObject(hProcess, INFINITE);
    DWORD processStatus;
    if (GetExitCodeProcess(hProcess, &processStatus))
    {
        exitStatus.status = static_cast<int32_t>(processStatus);
        return true;
    }
    return "Process::wait - GetExitCodeProcess failed"_a8;
}

// https://learn.microsoft.com/en-us/windows/win32/procthread/creating-a-child-process-with-redirected-input-and-output
SC::ReturnCode SC::Process::launch(ProcessOptions options)
{
    STARTUPINFO startupInfo;
    const bool  someRedirection =
        standardInput.handle.isValid() || standardOutput.handle.isValid() || standardError.handle.isValid();

    // On Windows to inherit flags they must be flagged as inheritable AND CreateProcess bInheritHandles must be true
    // TODO: This is not thread-safe in regard to handle inheritance, check out Microsoft Article on the topic
    // https://devblogs.microsoft.com/oldnewthing/20111216-00/?p=8873

    const BOOL inheritHandles = options.inheritFileDescriptors or someRedirection ? TRUE : FALSE;

    DWORD creationFlags = 0; // CREATE_UNICODE_ENVIRONMENT;
    ZeroMemory(&startupInfo, sizeof(STARTUPINFO));
    startupInfo.cb         = sizeof(STARTUPINFO);
    startupInfo.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
    startupInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startupInfo.hStdError  = GetStdHandle(STD_ERROR_HANDLE);

    if (standardInput.handle.isValid())
    {
        SC_TRY_IF(standardInput.handle.get(startupInfo.hStdInput, false));
    }
    if (standardOutput.handle.isValid())
    {
        SC_TRY_IF(standardOutput.handle.get(startupInfo.hStdOutput, false));
    }
    if (standardError.handle.isValid())
    {
        SC_TRY_IF(standardError.handle.get(startupInfo.hStdError, false));
    }
    if (someRedirection)
    {
        startupInfo.dwFlags |= STARTF_USESTDHANDLES;
    }

    // In documentation it's explicitly stated that this buffer will be modified (!?)
    LPWSTR  wideCmd = command.nativeWritableBytesIncludingTerminator();
    LPCWSTR wideDir = currentDirectory.view().isEmpty() ? nullptr : currentDirectory.view().getNullTerminatedNative();
    LPWSTR  wideEnv = environment.view().isEmpty() ? nullptr : environment.nativeWritableBytesIncludingTerminator();
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
    SC_TRY_IF(handle.assign(processInfo.hProcess));
    SC_TRY_IF(standardInput.handle.close());
    SC_TRY_IF(standardOutput.handle.close());
    SC_TRY_IF(standardError.handle.close());
    return true;
}
