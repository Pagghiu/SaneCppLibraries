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
//-----------------------------------------------------------------------------------------------------------------------
// Process
//-----------------------------------------------------------------------------------------------------------------------

bool SC::Process::isWindowsConsoleSubsystem() { return ::GetStdHandle(STD_OUTPUT_HANDLE) == NULL; }

bool SC::Process::isWindowsEmulatedProcess()
{
    USHORT processMachine = 0;
    USHORT nativeMachine  = 0;
    ::IsWow64Process2(GetCurrentProcess(), &processMachine, &nativeMachine);
    if (processMachine == IMAGE_FILE_MACHINE_UNKNOWN)
    {
        // I am not sure why Windows returns IMAGE_FILE_MACHINE_UNKNOWN but we can dig deeper for it
        PROCESS_MACHINE_INFORMATION processMachineInfo;
        if (::GetProcessInformation(::GetCurrentProcess(), ProcessMachineTypeInfo, &processMachineInfo,
                                    sizeof(PROCESS_MACHINE_INFORMATION)))
        {
            processMachine = processMachineInfo.ProcessMachine;
        }
        else
        {
            processMachine = nativeMachine;
        }
    }
    return processMachine != nativeMachine;
}

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

//-----------------------------------------------------------------------------------------------------------------------
// ProcessEnvironment
//-----------------------------------------------------------------------------------------------------------------------
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

//-----------------------------------------------------------------------------------------------------------------------
// ProcessFork
//-----------------------------------------------------------------------------------------------------------------------
#if SC_PLATFORM_WINDOWS
#include <winternl.h>
#if SC_COMPILER_MSVC
#pragma warning(push)
#pragma warning(disable : 4201)
#endif
extern "C"
{
    typedef struct _SECTION_IMAGE_INFORMATION
    {
        PVOID  TransferAddress;
        ULONG  ZeroBits;
        SIZE_T MaximumStackSize;
        SIZE_T CommittedStackSize;
        ULONG  SubSystemType;
        union
        {
            struct
            {
                USHORT SubSystemMinorVersion;
                USHORT SubSystemMajorVersion;
            };
            ULONG SubSystemVersion;
        };
        union
        {
            struct
            {
                USHORT MajorOperatingSystemVersion;
                USHORT MinorOperatingSystemVersion;
            };
            ULONG OperatingSystemVersion;
        };
        USHORT  ImageCharacteristics;
        USHORT  DllCharacteristics;
        USHORT  Machine;
        BOOLEAN ImageContainsCode;
        union
        {
            UCHAR ImageFlags;
            struct
            {
                UCHAR ComPlusNativeReady        : 1;
                UCHAR ComPlusILOnly             : 1;
                UCHAR ImageDynamicallyRelocated : 1;
                UCHAR ImageMappedFlat           : 1;
                UCHAR BaseBelow4gb              : 1;
                UCHAR ComPlusPrefer32bit        : 1;
                UCHAR Reserved                  : 2;
            };
        };
        ULONG LoaderFlags;
        ULONG ImageFileSize;
        ULONG CheckSum;
    } SECTION_IMAGE_INFORMATION, *PSECTION_IMAGE_INFORMATION;

    typedef struct _RTL_USER_PROCESS_INFORMATION
    {
        ULONG                     Length;
        HANDLE                    ProcessHandle;
        HANDLE                    ThreadHandle;
        CLIENT_ID                 ClientId;
        SECTION_IMAGE_INFORMATION ImageInformation;
    } RTL_USER_PROCESS_INFORMATION, *PRTL_USER_PROCESS_INFORMATION;

    constexpr DWORD RTL_CLONE_PROCESS_FLAGS_CREATE_SUSPENDED = 0x00000001;
    constexpr DWORD RTL_CLONE_PROCESS_FLAGS_INHERIT_HANDLES  = 0x00000002;
    constexpr DWORD RTL_CLONE_PROCESS_FLAGS_NO_SYNCHRONIZE   = 0x00000004;

    constexpr LONG RTL_CLONE_PARENT = 0;
    constexpr LONG RTL_CLONE_CHILD  = 297;

    typedef _Return_type_success_(return >= 0) LONG NTSTATUS;
#define NtCurrentProcess() ((HANDLE)(LONG_PTR)-1)
    NTSYSAPI NTSTATUS NTAPI RtlCloneUserProcess(_In_ ULONG                          ProcessFlags,
                                                _In_opt_ PSECURITY_DESCRIPTOR       ProcessSecurityDescriptor,
                                                _In_opt_ PSECURITY_DESCRIPTOR       ThreadSecurityDescriptor,
                                                _In_opt_ HANDLE                     DebugPort,
                                                _Out_ PRTL_USER_PROCESS_INFORMATION ProcessInformation);
    /**
     * Terminates the specified process.
     *
     * @param ProcessHandle Optional. A handle to the process to be terminated. If this parameter is NULL, the calling
     * process is terminated.
     * @param ExitStatus The exit status to be used by the process and the process's termination status.
     * @return NTSTATUS Successful or errant status.
     */
    NTSYSCALLAPI NTSTATUS NTAPI NtTerminateProcess(_In_opt_ HANDLE ProcessHandle, _In_ NTSTATUS ExitStatus);
}

#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#if SC_COMPILER_MSVC
#pragma warning(pop)
#endif
#endif
SC::ProcessFork::ProcessFork() {}

SC::ProcessFork::~ProcessFork()
{
    (void)parentToFork.close();
    (void)forkToParent.close();
    if (side == ForkChild)
    {
        // Terminate without clean-up
        NTSTATUS res = processHandle == 0 ? 0 : -1;
        ::NtTerminateProcess(NtCurrentProcess(), res);
    }
}

SC::FileDescriptor& SC::ProcessFork::getWritePipe()
{
    return side == ForkChild ? forkToParent.writePipe : parentToFork.writePipe;
}

SC::FileDescriptor& SC::ProcessFork::getReadPipe()
{
    return side == ForkChild ? parentToFork.readPipe : forkToParent.readPipe;
}

SC::Result SC::ProcessFork::waitForChild()
{
    if (side == ForkChild)
    {
        // Terminate without clean-up
        ::NtTerminateProcess(NtCurrentProcess(), -1);
    }
    NTSTATUS status;
    status = ::NtWaitForSingleObject(processHandle, FALSE, NULL);
    if (!NT_SUCCESS(status))
    {
        return Result::Error("Cannot wait for process");
    }

    DWORD processStatus = 0;
    if (::GetExitCodeProcess(processHandle, &processStatus))
    {
        exitStatus.status = static_cast<int32_t>(processStatus);
    }
    ::NtClose(processHandle);
    ::NtClose(threadHandle);
    threadHandle = detail::ProcessDescriptorDefinition::Invalid;
    return Result(true);
}

SC::Result SC::ProcessFork::resumeChildFork()
{
    if (side == ForkChild)
    {
        // Terminate without clean-up
        ::NtTerminateProcess(NtCurrentProcess(), -1);
    }
    char cmd = 0;
    SC_TRY(parentToFork.writePipe.write({&cmd, 1}));
    return Result(true);
}

SC::Result SC::ProcessFork::fork(State state)
{
    // We want this to be inheritable
    SC_TRY(parentToFork.createPipe(PipeDescriptor::ReadInheritable, PipeDescriptor::WriteInheritable));
    SC_TRY(forkToParent.createPipe(PipeDescriptor::ReadInheritable, PipeDescriptor::WriteInheritable));

    RTL_USER_PROCESS_INFORMATION processInfo;
    // RTL_CLONE_PROCESS_FLAGS_CREATE_SUSPENDED could be used instead of parentToFork.readPipe.read
    DWORD cloneFlags = RTL_CLONE_PROCESS_FLAGS_INHERIT_HANDLES;

    NTSTATUS status = ::RtlCloneUserProcess(cloneFlags, NULL, NULL, NULL, &processInfo);

    // Check parent / child branch
    if (status == RTL_CLONE_CHILD)
    {
        side = ForkChild;
        // Enables using the Console
        ::FreeConsole();
        ::AttachConsole(ATTACH_PARENT_PROCESS);

        switch (state)
        {
        case Suspended: {
            char       cmd = 0;
            Span<char> actuallyRead;
            SC_TRY(parentToFork.readPipe.read({&cmd, 1}, actuallyRead));
        }
        break;
        case Immediate: break;
        }
        processHandle = 0;
    }
    else
    {
        if (!NT_SUCCESS(status))
        {
            return Result::Error("fork failed");
        }

        processHandle = processInfo.ProcessHandle;
        threadHandle  = processInfo.ThreadHandle;
        if (cloneFlags == RTL_CLONE_PROCESS_FLAGS_CREATE_SUSPENDED)
        {
            DWORD clientProcess = static_cast<DWORD>((ptrdiff_t)processInfo.ClientId.UniqueProcess);
            DWORD clientThread  = static_cast<DWORD>((ptrdiff_t)processInfo.ClientId.UniqueThread);

            HANDLE hProcess = ::OpenProcess(PROCESS_ALL_ACCESS, FALSE, clientProcess);
            HANDLE hThread  = ::OpenThread(THREAD_ALL_ACCESS, FALSE, clientThread);

            ::ResumeThread(hThread);
            ::CloseHandle(hThread);
            ::CloseHandle(hProcess);
        }

        side = ForkParent;
    }
    return Result(true);
}
