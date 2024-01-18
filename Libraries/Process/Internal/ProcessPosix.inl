// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Foundation/Deferred.h"
#include "../Process.h"

#include <errno.h>
#include <signal.h>   // sigfillset / sigdelset / sigaction
#include <stdio.h>    // stdout, stdin
#include <stdlib.h>   // abort
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

    static Result resetInheritedSignalHandlers()
    {
        // For every signal, we restore the default action
        struct sigaction action;
        memset(&action, 0, sizeof(action));
        action.sa_handler = SIG_DFL;

        int res = sigemptyset(&action.sa_mask);
        if (res < 0)
        {
            return Result::Error("sigemptyset failed");
        }
#ifdef NSIG
        constexpr int numSignals = NSIG;
#else
        constexpr int numSignals = 32;
#endif
        for (int signal = 1; signal < numSignals; signal++)
        {
            if (signal == SIGKILL or signal == SIGSTOP)
                continue; // these signals are not meant to be changed

            res = sigaction(signal, &action, NULL);
            if (res < 0 && errno != EINVAL)
            {
                return Result::Error("sigaction failed");
            }
        }

        // Clear all signals set
        sigset_t signalSet;

        res = sigemptyset(&signalSet);
        if (res < 0)
        {
            return Result::Error("sigemptyset failed");
        }

        res = pthread_sigmask(SIG_SETMASK, &signalSet, NULL);
        if (res > 0) // pthread returns > 0 error codes
        {
            return Result::Error("signal_mask failed");
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
        if (exitStatus.status == EXIT_FAILURE)
            return Result::Error("Cannot run executable");
    }
    return Result(true);
}

SC::Result SC::Process::launch(Options options)
{
    SC_COMPILER_UNUSED(options);
    sigset_t emptySignals;
    sigset_t previousSignals;

    // Disable all signals before forking, to avoid them running in child during fork
    sigfillset(&emptySignals);
    sigdelset(&emptySignals, SIGABRT);
    sigdelset(&emptySignals, SIGBUS);
    sigdelset(&emptySignals, SIGILL);
    sigdelset(&emptySignals, SIGKILL);
    sigdelset(&emptySignals, SIGSEGV);
    sigdelset(&emptySignals, SIGSTOP);
    sigdelset(&emptySignals, SIGSYS);
    sigdelset(&emptySignals, SIGTRAP);
    if (pthread_sigmask(SIG_BLOCK, &emptySignals, &previousSignals) != 0)
        abort();

    // Create a CLOSE_ON_EXEC pipe (non-inheritable) to communicate execvp failure
    PipeDescriptor pipe;
    SC_TRY(pipe.createPipe(PipeDescriptor::ReadNonInheritable, PipeDescriptor::WriteNonInheritable));

    // Fork child from parent here
    SC_TRY(fork());

    // Check parent / child branch
    if (isChild())
    {
        // Child branch

        // If execvpe doesn't take control, we exit with failure code on error
        auto exitDeferred = MakeDeferred([&] { _exit(EXIT_FAILURE); });

        // Try restoring default signal handlers
        SC_TRY(Internal::resetInheritedSignalHandlers());

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

        // Construct the argv pointers array, from the command string, that contains the
        // executable and all arguments separated by null terminators
        // First parameter is executable path and also argv[0]
        // TODO: Check if argv[0] is meant to be resolved to be the full path to executable
        const char* argv[MAX_NUM_ARGUMENTS];
        const char* commandView = command.view().getNullTerminatedNative();
        for (size_t idx = 0; idx < commandArgumentsNumber; ++idx)
        {
            // Make argv point at the beginning of the idx-th arg
            argv[idx] = commandView + commandArgumentsByteOffset[idx];
        }
        argv[commandArgumentsNumber] = nullptr; // The last item also must be a nullptr to signal "end of array"

        // If execvp succeeds, this fork morphs into the new executable on the next line, and the parent communication
        // pipe, that has the CLOEXEC flags set (as it has been created as Non-inheritable) will see both sides closed,
        // allowing the pipe.readPipe.read to receive an EOF. This works also because the parent is closing the write
        // side before the read side is used for actual read.
        //
        // If execvp fails, the deferred on top of this branch will _exit(EXIT_FAILURE)
        int childErrno = ::execvp(command.view().getNullTerminatedNative(), const_cast<char* const*>(argv));

        // execvp failed, let's communicate errno back to parent before _exit(EXIT_FAILURE)
        childErrno = errno;
        // TODO: Add a write or Span overload that will not need to be called with a reinterpret_cast
        (void)pipe.writePipe.write({reinterpret_cast<char*>(&childErrno), sizeof(childErrno)});
        (void)pipe.close();
    }
    else
    {
        // Parent branch

        if (pthread_sigmask(SIG_SETMASK, &previousSignals, NULL) != 0)
            abort();
        Span<char> actuallyRead;

        // Closing the writePipe to allow read to succeed with
        // - EOF (good, execvp succeeded)
        // - int (bad, contains the errno after execvp failed)
        SC_TRY(pipe.writePipe.close());
        int childErrno;
        SC_TRY(pipe.readPipe.read({reinterpret_cast<char*>(&childErrno), sizeof(childErrno)}, actuallyRead));
        if (actuallyRead.sizeInBytes() != 0)
        {
            // Error received inside childErrno
            return Result::Error("execvp failed");
        }
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
