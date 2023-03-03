// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Process.h"

#include <errno.h>
#include <stdio.h>    // stdout, stdin
#include <sys/wait.h> // waitpid
#include <unistd.h>   // pipe fork execl _exit

struct SC::Process::Internal
{
    static FileDescriptorNative getStandardInputFDS() { return fileno(stdin); };
    static FileDescriptorNative getStandardOutputFDS() { return fileno(stdout); };
    static FileDescriptorNative getStandardErrorFDS() { return fileno(stderr); };
};

SC::ReturnCode SC::ProcessNativeHandleClose(pid_t& handle) { return true; }
SC::ReturnCode SC::Process::fork()
{
    processID.pid = ::fork();
    if (processID.pid < 0)
    {
        return ReturnCode("fork failed"_a8);
    }
    return true;
}

bool SC::Process::isChild() const { return processID.pid == 0; }

SC::ReturnCode SC::Process::waitProcessExit()
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
SC::ReturnCode SC::Process::spawn(Lambda&& lambda)
{
    SC_TRY_IF(fork());
    if (isChild())
    {
        auto exitDeferred = MakeDeferred([&] { _exit(127); });
        if (standardInput.handle.isValid())
        {
            SC_TRY_IF(standardInput.posix().redirect(Internal::getStandardInputFDS()));
        }
        if (standardOutput.handle.isValid())
        {
            SC_TRY_IF(standardOutput.posix().redirect(Internal::getStandardOutputFDS()));
        }
        if (standardError.handle.isValid())
        {
            SC_TRY_IF(standardError.posix().redirect(Internal::getStandardErrorFDS()));
        }
        // TODO: Check if these close calls are still needed after we setCloseOnExec
        SC_TRY_IF(standardInput.handle.close());
        SC_TRY_IF(standardOutput.handle.close());
        SC_TRY_IF(standardError.handle.close());
        lambda();
    }
    else
    {
        SC_TRY_IF(handle.assign(processID.pid));

        SC_TRY_IF(standardInput.handle.close());
        SC_TRY_IF(standardOutput.handle.close());
        SC_TRY_IF(standardError.handle.close());
        return true;
    }
    // The exit(127) inside isChild makes this unreachable
    SC_UNREACHABLE();
}

SC::ReturnCode SC::Process::run(const ProcessOptions& options)
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
