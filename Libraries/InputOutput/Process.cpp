// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "Process.h"

#if SC_PLATFORM_WINDOWS
#include <Windows.h>
static_assert(SC::IsSame<SC::ProcessNativeID, decltype(PROCESS_INFORMATION::dwProcessId)>::value,
              "Check Definition of ProcessNativeID");
static_assert(SC::IsSame<SC::ProcessNativeHandle, decltype(PROCESS_INFORMATION::hProcess)>::value,
              "Check Definition of ProcessNativeID");

struct SC::ProcessEntry::Internal
{
    // TODO: this could be migrated to OS
    static void exit(int code) { _exit(code); }
};

SC::ReturnCode SC::ProcessHandle::close()
{
    if (isValid())
    {
        BOOL res = ::CloseHandle(handle);
        makeInvalid();
        return res == TRUE;
    }
    return true;
}

SC::ReturnCode SC::ProcessEntry::waitProcessExit()
{
    WaitForSingleObject(processHandle.handle, INFINITE);
    DWORD processStatus;
    if (GetExitCodeProcess(processHandle.handle, &processStatus))
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
        startupInfo.hStdInput = standardInput.getRawFileDescriptor();
    }
    if (standardOutput.isValid())
    {
        startupInfo.hStdOutput = standardOutput.getRawFileDescriptor();
    }
    if (standardError.isValid())
    {
        startupInfo.hStdError = standardError.getRawFileDescriptor();
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
    processHandle.handle = processInfo.hProcess;

    SC_TRY_IF(standardInput.close());
    SC_TRY_IF(standardOutput.close());
    SC_TRY_IF(standardError.close());
    return true;
}

#elif SC_PLATFORM_APPLE
#include <errno.h>
#include <sys/wait.h> // waitpid
#include <unistd.h>   // pipe fork execl _exit

static_assert(SC::IsSame<SC::ProcessNativeID, pid_t>::value, "Check Definition of ProcessNativeID");
struct SC::ProcessEntry::Internal
{
    // TODO: this could be migrated to OS
    static __attribute__((__noreturn__)) void exit(int code) { _exit(code); }
};

SC::ReturnCode SC::ProcessHandle::close() { return true; }

SC::ReturnCode SC::ProcessEntry::fork()
{
    processID.pid = ::fork();
    if (processID.pid < 0)
    {
        return ReturnCode("fork failed"_a8);
    }
    return true;
}

bool SC::ProcessEntry::isChild() const { return processID.pid == 0; }

SC::ReturnCode SC::ProcessEntry::waitProcessExit()
{
    int   status = -1;
    pid_t waitPid;
    do
    {
        waitPid = waitpid(processID.pid, &status, 0);
    } while (waitPid == -1 and errno == EINTR);
    return true;
}

template <typename Lambda>
SC::ReturnCode SC::ProcessEntry::spawn(Lambda&& lambda)
{
    SC_TRY_IF(fork());
    if (isChild())
    {
        auto exitDeferred = MakeDeferred([&] { Internal::exit(127); });
        if (standardInput.isValid())
        {
            SC_TRY_IF(standardInput.redirect(FileDescriptor::getStandardInputFDS()));
        }
        if (standardOutput.isValid())
        {
            SC_TRY_IF(standardOutput.redirect(FileDescriptor::getStandardOutputFDS()));
        }
        if (standardError.isValid())
        {
            SC_TRY_IF(standardError.redirect(FileDescriptor::getStandardErrorFDS()));
        }
        // TODO: Check if these close calls are still needed after we setCloseOnExec
        SC_TRY_IF(standardInput.close());
        SC_TRY_IF(standardOutput.close());
        SC_TRY_IF(standardError.close());
        lambda();
    }
    else
    {
        processHandle.handle = processID.pid;
        SC_TRY_IF(standardInput.close());
        SC_TRY_IF(standardOutput.close());
        SC_TRY_IF(standardError.close());
        return true;
    }
    // The exit(127) inside isChild makes this unreachable
    SC_UNREACHABLE();
}

SC::ReturnCode SC::ProcessEntry::run(const ProcessOptions& options)
{
    auto spawnLambda = [&]()
    {
        if (options.useShell)
        {
            execl("/bin/sh", "sh", "-c", command.text.view().getNullTerminatedNative(), nullptr);
        }
        else
        {
            return false;
        }
        return true;
    };
    return spawn(spawnLambda);
}

#elif SC_PLATFORM_EMSCRIPTEN

SC::ReturnCode SC::ProcessEntry::run(const ProcessOptions& options) { return true; }
SC::ReturnCode SC::ProcessEntry::waitProcessExit() { return true; }
SC::ReturnCode SC::ProcessHandle::close() { return true; }
#endif

SC::ReturnCode SC::ProcessShell::launch()
{
    if (processes.isEmpty())
        return false;

    if (options.pipeSTDIN)
    {
        SC_TRY_IF(inputPipe.createPipe());
        SC_TRY_IF(inputPipe.setCloseOnExec());
        SC_TRY_IF(inputPipe.writePipe.disableInherit());
        SC_TRY_IF(processes.front().standardInput.assign(inputPipe.readPipe.getRawFileDescriptor()));
        inputPipe.readPipe.resetAsInvalid();
    }
    if (options.pipeSTDOUT)
    {
        SC_TRY_IF(outputPipe.createPipe());
        SC_TRY_IF(outputPipe.setCloseOnExec());
        SC_TRY_IF(outputPipe.readPipe.disableInherit());
        SC_TRY_IF(processes.back().standardOutput.assign(outputPipe.writePipe.getRawFileDescriptor()));
        outputPipe.writePipe.resetAsInvalid();
    }
    if (options.pipeSTDERR)
    {
        SC_TRY_IF(errorPipe.createPipe());
        SC_TRY_IF(errorPipe.setCloseOnExec());
        SC_TRY_IF(errorPipe.readPipe.disableInherit());
        SC_TRY_IF(processes.back().standardError.assign(errorPipe.writePipe.getRawFileDescriptor()));
        errorPipe.writePipe.resetAsInvalid();
    }

    for (ProcessEntry& process : processes)
    {
        error.returnCode = process.run(options);
        if (error.returnCode.isError())
        {
            // TODO: Decide what to do with the queue
            onError(error);
            return error.returnCode;
        }
    }
    SC_TRY_IF(inputPipe.readPipe.close());
    SC_TRY_IF(outputPipe.writePipe.close());
    SC_TRY_IF(errorPipe.writePipe.close());
    return true;
}

SC::ReturnCode SC::ProcessShell::readOutputSync(String* outputString, String* errorString)
{
    Array<char, 1024> buffer;
    SC_TRUST_RESULT(buffer.resizeWithoutInitializing(buffer.capacity()));
    FileDescriptor::ReadResult readResult;
    if (outputPipe.readPipe.isValid() && outputString)
    {
        while (not readResult.isEOF)
        {
            SC_TRY(readResult, outputPipe.readPipe.readAppend(outputString->data, {buffer.data(), buffer.size()}));
        }
        outputString->pushNullTerm();
    }
    if (errorPipe.readPipe.isValid() && errorString)
    {
        while (not readResult.isEOF)
        {
            SC_TRY(readResult, errorPipe.readPipe.readAppend(errorString->data, {buffer.data(), buffer.size()}));
        }
        errorString->pushNullTerm();
    }
    return true;
}

SC::ReturnCode SC::ProcessShell::waitSync()
{
    for (ProcessEntry& p : processes)
    {
        SC_TRY_IF(p.waitProcessExit());
    }
    processes.clear();
    SC_TRY_IF(inputPipe.writePipe.close());
    SC_TRY_IF(outputPipe.readPipe.close());
    SC_TRY_IF(errorPipe.readPipe.close());
    return error.returnCode;
}

SC::ReturnCode SC::ProcessShell::queueProcess(Span<StringView*> spanArguments)
{
    ProcessEntry process;
    if (options.useShell)
    {
        bool first = true;
        for (StringView* svp : spanArguments)
        {
            if (not first)
            {
                SC_TRY_IF(process.command.appendNullTerminated(" "));
            }
            first = false;
            if (svp->containsASCIICharacter(' '))
            {
                // has space, must escape it
                SC_TRY_IF(process.command.appendNullTerminated("\""));
                SC_TRY_IF(process.command.appendNullTerminated(*svp));
                SC_TRY_IF(process.command.appendNullTerminated("\""));
            }
            else
            {
                SC_TRY_IF(process.command.appendNullTerminated(*svp));
            }
        }
    }
    else
    {
        return "UseShell==false Not Implemented yet"_a8;
    }
    if (not processes.isEmpty())
    {
        FileDescriptorPipe chainPipe;
        SC_TRY_IF(chainPipe.createPipe());
        // TODO: Create a typed assign with move semantics instead of going raw / reset as invalid
        SC_TRY_IF(processes.back().standardOutput.assign(chainPipe.writePipe.getRawFileDescriptor()));
        SC_TRY_IF(process.standardInput.assign(chainPipe.readPipe.getRawFileDescriptor()));
        SC_TRY_IF(chainPipe.setCloseOnExec());
        chainPipe.writePipe.resetAsInvalid();
        chainPipe.readPipe.resetAsInvalid();
    }
    SC_TRY_IF(processes.push_back(move(process)));
    return true;
}
