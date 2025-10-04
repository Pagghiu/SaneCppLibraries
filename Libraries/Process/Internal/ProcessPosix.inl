// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Foundation/Assert.h"
#include "../../Foundation/Deferred.h"
#include "../../Process/Internal/EnvironmentTable.h"
#include "../../Process/Process.h"

#include <errno.h>
#include <signal.h>   // sigfillset / sigdelset / sigaction
#include <stdlib.h>   // abort
#include <string.h>   // strchr
#include <sys/wait.h> // waitpid
#include <unistd.h>   // pipe fork execl _exit

#if SC_PLATFORM_APPLE
#include <crt_externs.h>
// https://www.gnu.org/software/gnulib/manual/html_node/environ.html
#define environ (*_NSGetEnviron())
#else
extern char** environ;
#endif

//-----------------------------------------------------------------------------------------------------------------------
// ProcessEnvironment
//-----------------------------------------------------------------------------------------------------------------------

SC::size_t SC::Process::getNumberOfProcessors()
{
    const long numProc = sysconf(_SC_NPROCESSORS_ONLN);
    return static_cast<size_t>(numProc);
}

bool SC::Process::isWindowsConsoleSubsystem() { return false; }
bool SC::Process::isWindowsEmulatedProcess() { return false; }

struct SC::Process::Internal
{
    static Result waitForPid(int pid, int& status)
    {
        status = -1;
        pid_t waitPid;
        do
        {
            waitPid = ::waitpid(pid, &status, 0);
        } while (waitPid == -1 and errno == EINTR);
        if (waitPid == -1)
        {
            return Result::Error("Process::waitForExitSync - waitPid failed");
        }
        if (WIFEXITED(status) != 0)
        {
            status = WEXITSTATUS(status);
        }
        return Result(true);
    }
};

SC::Result SC::Process::waitForExitSync() { return Internal::waitForPid(processID.pid, exitStatus.status); }

SC::Result SC::Process::launchImplementation()
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
    SC_TRY(pipe.createPipe());

    // Fork child from parent here
    processID.pid = ::fork();
    SC_TRY_MSG(processID.pid >= 0, "fork failed");
    return processID.pid == 0 ? launchForkChild(pipe) : launchForkParent(pipe, &previousSignals);
}

SC::Result SC::Process::launchForkParent(PipeDescriptor& pipe, const void* previousSignals)
{
    // Parent branch
    if (pthread_sigmask(SIG_SETMASK, static_cast<const sigset_t*>(previousSignals), NULL) != 0)
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
        return Result::Error("Process::launchImplementation - execve failed");
    }
    SC_TRY_MSG(handle.assign(processID.pid), "Process::launchImplementation - handle not assigned");
    SC_TRY(stdInFd.close());
    SC_TRY(stdOutFd.close());
    SC_TRY(stdErrFd.close());
    return Result(true);
}

SC::Result SC::Process::formatArguments(Span<const StringSpan> params)
{
    StringsArena table = {command, commandArgumentsNumber, commandArgumentsByteOffset};
    for (size_t idx = 0; idx < params.sizeInElements(); ++idx)
    {
        SC_TRY(table.appendAsSingleString(params[idx]));
    }
    return Result(true);
}

//-----------------------------------------------------------------------------------------------------------------------
// ProcessEnvironment
//-----------------------------------------------------------------------------------------------------------------------
SC::ProcessEnvironment::ProcessEnvironment()
{
    environment = environ;
    char** env  = environment;

    while (env[numberOfEnvironment] != nullptr)
    {
        numberOfEnvironment++;
    }
}

SC::ProcessEnvironment::~ProcessEnvironment() {}

bool SC::ProcessEnvironment::get(size_t index, StringSpan& name, StringSpan& value) const
{
    if (index >= numberOfEnvironment)
    {
        return false;
    }
    // UTF-8 SAFETY NOTE: The '=' character (ASCII 0x3D) is not a valid UTF-8 continuation byte, and cannot appear as
    // part of any multi-byte UTF-8 sequence. Therefore, using strchr to split environment variables on '=' is safe even
    // if names/values contain UTF-8 encoded text.
    const char* currentEnv = environment[index];
    const char* equalSign  = ::strchr(currentEnv, '=');
    if (equalSign != nullptr)
    {
        name  = StringSpan({currentEnv, static_cast<size_t>(equalSign - currentEnv)}, false, StringEncoding::Ascii);
        value = StringSpan::fromNullTerminated(equalSign + 1, StringEncoding::Ascii);
    }
    return true;
}

//-----------------------------------------------------------------------------------------------------------------------
// ProcessFork
//-----------------------------------------------------------------------------------------------------------------------
SC::ProcessFork::ProcessFork() {}

SC::ProcessFork::~ProcessFork()
{
    (void)parentToFork.close();
    (void)forkToParent.close();
    if (side == ForkChild)
    {
        ::_exit(processID.pid < 0 ? EXIT_FAILURE : EXIT_SUCCESS);
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
        ::_exit(EXIT_FAILURE);
    }
    if (processID.pid < 0)
    {
        return Result::Error("waitForChild");
    }
    return Process::Internal::waitForPid(processID.pid, exitStatus.status);
}

SC::Result SC::ProcessFork::resumeChildFork()
{
    if (side == ForkChild)
    {
        ::_exit(EXIT_FAILURE);
    }
    char cmd = 0;
    SC_TRY(parentToFork.writePipe.write({&cmd, 1}));
    return Result(true);
}

SC::Result SC::ProcessFork::fork(State state)
{
    // Create a CLOSE_ON_EXEC pipe (non-inheritable) to communicate with forked child
    SC_TRY(parentToFork.createPipe());
    SC_TRY(forkToParent.createPipe());
    int pid = ::fork();
    if (pid < 0)
    {
        processID.pid = pid;
        return Result::Error("fork failed");
    }

    // Check parent / child branch
    if (pid == 0)
    {
        // Child branch
        side = ForkChild;
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
    }
    else
    {
        // parent branch
        side = ForkParent;
    }
    processID.pid = pid;
    return Result(true);
}
