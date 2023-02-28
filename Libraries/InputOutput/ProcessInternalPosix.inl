// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "FileDescriptorInternalPosix.h"
#include "Process.h"

#include <errno.h>
#include <stdio.h>    // stdout, stdin
#include <sys/wait.h> // waitpid
#include <unistd.h>   // pipe fork execl _exit

struct SC::ProcessEntry::Internal
{
    static ReturnCode           ProcessHandleClose(const pid_t& handle) { return true; }
    static FileNativeDescriptor getStandardInputFDS() { return fileno(stdin); };
    static FileNativeDescriptor getStandardOutputFDS() { return fileno(stdout); };
    static FileNativeDescriptor getStandardErrorFDS() { return fileno(stderr); };
};

struct SC::ProcessEntry::ProcessHandle : public MovableHandle<pid_t, 0, ReturnCode, &Internal::ProcessHandleClose>
{
};
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
        auto exitDeferred = MakeDeferred([&] { _exit(127); });
        if (standardInput.isValid())
        {
            SC_TRY_IF(standardInput.posix().redirect(Internal::getStandardInputFDS()));
        }
        if (standardOutput.isValid())
        {
            SC_TRY_IF(standardOutput.posix().redirect(Internal::getStandardOutputFDS()));
        }
        if (standardError.isValid())
        {
            SC_TRY_IF(standardError.posix().redirect(Internal::getStandardErrorFDS()));
        }
        // TODO: Check if these close calls are still needed after we setCloseOnExec
        SC_TRY_IF(standardInput.close());
        SC_TRY_IF(standardOutput.close());
        SC_TRY_IF(standardError.close());
        lambda();
    }
    else
    {
        SC_TRY_IF(processHandlePimpl.get().assign(processID.pid));

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
