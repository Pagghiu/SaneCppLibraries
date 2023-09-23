// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Process.h"

#include <errno.h>
#include <stdio.h>    // stdout, stdin
#include <sys/wait.h> // waitpid
#include <unistd.h>   // pipe fork execl _exit

SC::ReturnCode SC::ProcessDescriptorTraits::releaseHandle(pid_t& handle)
{
    handle = Invalid;
    return true;
}

struct SC::Process::Internal
{
    static FileDescriptor::Handle getStandardInputFDS() { return fileno(stdin); };
    static FileDescriptor::Handle getStandardOutputFDS() { return fileno(stdout); };
    static FileDescriptor::Handle getStandardErrorFDS() { return fileno(stderr); };

    static ReturnCode duplicateAndReplace(FileDescriptor& handle, FileDescriptor::Handle fds)
    {
        FileDescriptor::Handle nativeFd;
        SC_TRY(handle.get(nativeFd, "duplicateAndReplace - Invalid Handle"_a8));
        if (::dup2(nativeFd, fds) == -1)
        {
            return ReturnCode("dup2 failed"_a8);
        }
        return true;
    }
};

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

SC::ReturnCode SC::Process::waitForExitSync()
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
    }
    return true;
}

template <typename Lambda>
SC::ReturnCode SC::Process::spawn(Lambda&& lambda)
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
        return true;
    }
    // The exit(127) inside isChild makes this unreachable
    SC_UNREACHABLE();
}

SC::ReturnCode SC::Process::launch(ProcessOptions options)
{
    SC_UNUSED(options);
    auto spawnLambda = [&]() { execl("/bin/sh", "sh", "-c", command.view().getNullTerminatedNative(), nullptr); };
    return spawn(spawnLambda);
}
