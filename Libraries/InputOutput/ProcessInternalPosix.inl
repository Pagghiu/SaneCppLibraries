// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "Process.h"

#include <errno.h>
#include <sys/wait.h> // waitpid
#include <unistd.h>   // pipe fork execl _exit

static_assert(SC::IsSame<SC::ProcessNativeID, pid_t>::value, "Check Definition of ProcessNativeID");
struct SC::ProcessEntry::Internal
{
    static __attribute__((__noreturn__)) void exit(int code) { _exit(code); }
};

SC::ReturnCode SC::ProcessNativeHandleClosePosix(const ProcessNativeHandle& handle) { return true; }

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
            SC_TRY_IF(standardInput.posix().redirect(FileDescriptorPosix::getStandardInputFDS()));
        }
        if (standardOutput.isValid())
        {
            SC_TRY_IF(standardOutput.posix().redirect(FileDescriptorPosix::getStandardOutputFDS()));
        }
        if (standardError.isValid())
        {
            SC_TRY_IF(standardError.posix().redirect(FileDescriptorPosix::getStandardErrorFDS()));
        }
        // TODO: Check if these close calls are still needed after we setCloseOnExec
        SC_TRY_IF(standardInput.close());
        SC_TRY_IF(standardOutput.close());
        SC_TRY_IF(standardError.close());
        lambda();
    }
    else
    {
        SC_TRY_IF(processHandle.assign(processID.pid));
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
