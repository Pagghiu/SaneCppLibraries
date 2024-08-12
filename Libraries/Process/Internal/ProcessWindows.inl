// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Process.h"
#include "EnvironmentTable.h"

#if SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdlib.h> // _exit
#endif

SC::Result SC::detail::ProcessDescriptorDefinition::releaseHandle(HANDLE& handle)
{
    if (::CloseHandle(handle) == FALSE)
        return Result::Error("ProcessNativeHandleClose - CloseHandle failed");
    return Result(true);
}

bool SC::Process::isWindowsConsoleSubsystem() { return ::GetStdHandle(STD_OUTPUT_HANDLE) == NULL; }

SC::size_t SC::Process::getNumberOfProcessors()
{
    SYSTEM_INFO systemInfo;
    ::GetSystemInfo(&systemInfo);
    DWORD numProc = systemInfo.dwNumberOfProcessors;
    return static_cast<size_t>(numProc);
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
SC::Result SC::Process::launchImplementation()
{
    STARTUPINFOW startupInfo;

    const bool someRedirection = stdInFd.isValid() || stdOutFd.isValid() || stdErrFd.isValid();

    // On Windows to inherit flags they must be flagged as inheritable AND CreateProcess bInheritHandles must be true
    // TODO: This is not thread-safe in regard to handle inheritance, check out Microsoft Article on the topic
    // https://devblogs.microsoft.com/oldnewthing/20111216-00/?p=8873

    const BOOL inheritHandles = someRedirection ? TRUE : FALSE;

    DWORD creationFlags = CREATE_UNICODE_ENVIRONMENT;

    if (options.windowsHide)
    {
        creationFlags |= CREATE_NO_WINDOW;
    }
    ZeroMemory(&startupInfo, sizeof(STARTUPINFO));
    startupInfo.cb         = sizeof(STARTUPINFO);
    startupInfo.hStdInput  = ::GetStdHandle(STD_INPUT_HANDLE);
    startupInfo.hStdOutput = ::GetStdHandle(STD_OUTPUT_HANDLE);
    startupInfo.hStdError  = ::GetStdHandle(STD_ERROR_HANDLE);

    if (stdInFd.isValid())
    {
        SC_TRY(stdInFd.get(startupInfo.hStdInput, Result(false)));
    }
    if (stdOutFd.isValid())
    {
        SC_TRY(stdOutFd.get(startupInfo.hStdOutput, Result(false)));
    }
    if (stdErrFd.isValid())
    {
        SC_TRY(stdErrFd.get(startupInfo.hStdError, Result(false)));
    }
    if (someRedirection)
    {
        startupInfo.dwFlags |= STARTF_USESTDHANDLES;
    }

    // In documentation it's explicitly stated that this buffer will be modified (!?)
    LPWSTR  wideCmd = const_cast<LPWSTR>(command.view().getNullTerminatedNative());
    LPCWSTR wideDir = currentDirectory.view().isEmpty() ? nullptr : currentDirectory.view().getNullTerminatedNative();
    LPWSTR  wideEnv = nullptr; // by default inherit parent environment

    EnvironmentTable<MAX_NUM_ENVIRONMENT> environmentTable;

    const wchar_t* const* environmentArray = nullptr;

    StringsArena arena = {environment, environmentNumber, environmentByteOffset};

    ProcessEnvironment parentEnv;
    SC_TRY(environmentTable.writeTo(environmentArray, inheritEnv, arena, parentEnv));

    if (environmentArray != nullptr)
    {
        for (size_t idx = environmentNumber; environmentArray[idx] != nullptr; ++idx)
        {
            const StringView environmentString({environmentArray[idx], ::wcslen(environmentArray[idx])}, true);
            SC_TRY(arena.appendAsSingleString(environmentString));
        }
        SC_TRY(arena.appendAsSingleString({"\0"})); // add final \0 (CreateProcessW requires it to signal end of array)

        // const_cast is required by CreateProcessW signature unfortunately
        wideEnv = const_cast<LPWSTR>(environment.view().getNullTerminatedNative());
    }
    PROCESS_INFORMATION processInfo;
    ZeroMemory(&processInfo, sizeof(PROCESS_INFORMATION));
    BOOL success;
    success = ::CreateProcessW(nullptr,        // [in, optional]      LPCWSTR               lpApplicationName,
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
    ::CloseHandle(processInfo.hThread);

    processID.pid = processInfo.dwProcessId;
    SC_TRY(handle.assign(processInfo.hProcess));
    SC_TRY(stdInFd.close());
    SC_TRY(stdOutFd.close());
    SC_TRY(stdErrFd.close());
    return Result(true);
}

SC::Result SC::Process::formatArguments(Span<const StringView> params)
{
    bool first = true;

    StringConverter formattedCmd(command, StringConverter::Clear);
    for (const StringView& param : params)
    {
        if (not first)
        {
            SC_TRY(formattedCmd.appendNullTerminated(" "));
        }
        first = false;
        if (param.containsCodePoint(' ') and not param.containsCodePoint('"'))
        {
            SC_TRY(formattedCmd.appendNullTerminated("\""));
            SC_TRY(formattedCmd.appendNullTerminated(param));
            SC_TRY(formattedCmd.appendNullTerminated("\""));
        }
        else
        {
            SC_TRY(formattedCmd.appendNullTerminated(param));
        }
    }

    return Result(true);
}

SC::ProcessEnvironment::ProcessEnvironment()
{
    environment  = ::GetEnvironmentStringsW();
    wchar_t* env = environment;
    while (*env != L'\0')
    {
        size_t length                   = ::wcslen(env);
        envStrings[numberOfEnvironment] = StringView({env, length}, true);
        env += length + 1;
        numberOfEnvironment++;
    }
}

SC::ProcessEnvironment::~ProcessEnvironment() { ::FreeEnvironmentStringsW(environment); }

bool SC::ProcessEnvironment::get(size_t index, StringView& name, StringView& value) const
{
    if (index >= numberOfEnvironment)
    {
        return false;
    }
    StringView          nameValue = envStrings[index];
    StringViewTokenizer tokenizer(nameValue);
    SC_TRY(tokenizer.tokenizeNext({'='}));
    name = tokenizer.component;
    if (not tokenizer.isFinished())
    {
        value = tokenizer.remaining;
    }
    return true;
}
