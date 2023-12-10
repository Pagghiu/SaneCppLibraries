// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../../Foundation/Deferred.h"
#include "../Process.h"

#include <errno.h>
#include <stdio.h>    // stdout, stdin
#include <sys/wait.h> // waitpid
#include <unistd.h>   // pipe fork execl _exit

SC::Result SC::detail::ProcessDescriptorDefinition::releaseHandle(pid_t& handle)
{
    handle = Invalid;
    return Result(true);
}

struct SC::Process::Internal
{
    static FileDescriptor::Handle getStandardInputFDS() { return fileno(stdin); };
    static FileDescriptor::Handle getStandardOutputFDS() { return fileno(stdout); };
    static FileDescriptor::Handle getStandardErrorFDS() { return fileno(stderr); };

    static Result duplicateAndReplace(FileDescriptor& handle, FileDescriptor::Handle fds)
    {
        FileDescriptor::Handle nativeFd;
        SC_TRY(handle.get(nativeFd, Result::Error("duplicateAndReplace - Invalid Handle")));
        if (::dup2(nativeFd, fds) == -1)
        {
            return Result::Error("dup2 failed");
        }
        return Result(true);
    }
};

SC::Result SC::Process::fork()
{
    processID.pid = ::fork();
    if (processID.pid < 0)
    {
        return Result::Error("fork failed");
    }
    return Result(true);
}

bool SC::Process::isChild() const { return processID.pid == 0; }

SC::Result SC::Process::waitForExitSync()
{
    int   status = -1;
    pid_t waitPid;
    do
    {
        waitPid = waitpid(processID.pid, &status, 0);
    } while (waitPid == -1 and errno == EINTR);
    if (WIFEXITED(status) != 0)
    {
        exitStatus.status = WEXITSTATUS(status);
        if (exitStatus.status == 127)
            return Result::Error("Cannot run executable");
    }
    return Result(true);
}

template <typename Lambda>
SC::Result SC::Process::spawn(Lambda&& lambda)
{
    SC_TRY(fork());
    if (isChild())
    {
        auto exitDeferred = MakeDeferred([&] { _exit(127); });
        if (standardInput.isValid())
        {
            SC_TRY(Internal::duplicateAndReplace(standardInput, Internal::getStandardInputFDS()));
        }
        if (standardOutput.isValid())
        {
            SC_TRY(Internal::duplicateAndReplace(standardOutput, Internal::getStandardOutputFDS()));
        }
        if (standardError.isValid())
        {
            SC_TRY(Internal::duplicateAndReplace(standardError, Internal::getStandardErrorFDS()));
        }
        // As std handles have been duplicated / redirected, we can close all of them.
        // We explicitly close them because some may have not been marked as CLOEXEC.
        // During creation of pipes we do setInheritable(true) for all read/write FDs
        // passed to child process, that means not setting the CLOEXEC flag on the FD.
        // We need the setInheritable(true) because windows backend otherwise child
        // process will not be able to see / duplicate such file descriptors.
        // On Posix this is easier as we could just always set CLOEXEC but the FD would
        // still valid between the fork() and the exec() call to do anything needed
        // (like the duplication / redirect we're doing here) without risk of leaking
        // any FD to the newly executed child process.
        SC_TRY(standardInput.close());
        SC_TRY(standardOutput.close());
        SC_TRY(standardError.close());
        lambda();
    }
    else
    {
        SC_TRY(handle.assign(processID.pid));
        SC_TRY(standardInput.close());
        SC_TRY(standardOutput.close());
        SC_TRY(standardError.close());
        // We are not aware of a way of knowing if the spawn failed on posix until without blocking
        // on waitpid. For this reason we return true also on Windows implementation in case of error.
        return Result(true);
    }
    // The exit(127) inside isChild makes this unreachable
    Assert::unreachable();
}

SC::Result SC::Process::launch(Options options)
{
    SC_COMPILER_UNUSED(options);
    auto spawnLambda = [&]() { execl("/bin/sh", "sh", "-c", command.view().getNullTerminatedNative(), nullptr); };
    return spawn(spawnLambda);
}
