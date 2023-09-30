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

SC::Result SC::ProcessDescriptorTraits::releaseHandle(HANDLE& handle)
{
    if (::CloseHandle(handle) == FALSE)
        return Result::Error("ProcessNativeHandleClose - CloseHandle failed");
    return Result(true);
}

struct SC::Process::Internal
{
    // TODO: this could be migrated to SystemDebug
    static void exit(int code) { _exit(code); }
};

SC::Result SC::Process::waitForExitSync()
{
    HANDLE hProcess;
    SC_TRY(handle.get(hProcess, Result::Error("ProcesEntry::waitProcessExit - Invalid handle")));
    WaitForSingleObject(hProcess, INFINITE);
    DWORD processStatus;
    if (GetExitCodeProcess(hProcess, &processStatus))
    {
        exitStatus.status = static_cast<int32_t>(processStatus);
        return Result(true);
    }
    return Result::Error("Process::wait - GetExitCodeProcess failed");
}

// https://learn.microsoft.com/en-us/windows/win32/procthread/creating-a-child-process-with-redirected-input-and-output
SC::Result SC::Process::launch(ProcessOptions options)
{
    STARTUPINFOW startupInfo;
    const bool   someRedirection = standardInput.isValid() || standardOutput.isValid() || standardError.isValid();

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

    if (standardInput.isValid())
    {
        SC_TRY(standardInput.get(startupInfo.hStdInput, Result(false)));
    }
    if (standardOutput.isValid())
    {
        SC_TRY(standardOutput.get(startupInfo.hStdOutput, Result(false)));
    }
    if (standardError.isValid())
    {
        SC_TRY(standardError.get(startupInfo.hStdError, Result(false)));
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
        return Result::Error("CreateProcessW failed");
    }
    CloseHandle(processInfo.hThread);

    processID.pid = processInfo.dwProcessId;
    SC_TRY(handle.assign(processInfo.hProcess));
    SC_TRY(standardInput.close());
    SC_TRY(standardOutput.close());
    SC_TRY(standardError.close());
    return Result(true);
}
