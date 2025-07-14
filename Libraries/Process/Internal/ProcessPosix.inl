// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Foundation/Deferred.h"
#include "../Process.h"
#include "EnvironmentTable.h"

#include <errno.h>
#include <signal.h>   // sigfillset / sigdelset / sigaction
#include <stdio.h>    // stdout, stdin
#include <stdlib.h>   // abort
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
    SC_TRY(pipe.createPipe(PipeDescriptor::ReadNonInheritable, PipeDescriptor::WriteNonInheritable));

    // Fork child from parent here
    processID.pid = ::fork();
    if (processID.pid < 0)
    {
        return Result::Error("fork failed");
    }

    // Check parent / child branch
    if (processID.pid == 0)
    {
        // Child branch

        // If execvpe doesn't take control, we exit with failure code on error
        auto exitDeferred = MakeDeferred([&] { _exit(EXIT_FAILURE); });

        // Try restoring default signal handlers
        SC_TRY(Internal::resetInheritedSignalHandlers());

        if (stdInFd.isValid())
        {
            SC_TRY(Internal::duplicateAndReplace(stdInFd, Internal::getStandardInputFDS()));
        }
        if (stdOutFd.isValid())
        {
            SC_TRY(Internal::duplicateAndReplace(stdOutFd, Internal::getStandardOutputFDS()));
        }
        if (stdErrFd.isValid())
        {
            SC_TRY(Internal::duplicateAndReplace(stdErrFd, Internal::getStandardErrorFDS()));
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
        SC_TRY(stdInFd.close());
        SC_TRY(stdOutFd.close());
        SC_TRY(stdErrFd.close());

        // Switch to wanted current directory (if provided)
        if (not currentDirectory.path.view().isEmpty())
        {
            int res = ::chdir(currentDirectory.path.view().getNullTerminatedNative());
            if (res < 0)
            {
                return Result::Error("chdir failed");
            }
        }

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
        // By default, let's pass current process environment

        const char* const* environmentArray = environ;
        ProcessEnvironment parentEnv;
        StringsArena       table = {environment, environmentNumber, environmentByteOffset};

        EnvironmentTable<MAX_NUM_ENVIRONMENT> environmentTable;
        SC_TRY(environmentTable.writeTo(environmentArray, inheritEnv, table, parentEnv));
        // If execvp succeeds, this fork morphs into the new executable on the next line, and the parent communication
        // pipe, that has the CLOEXEC flags set (as it has been created as Non-inheritable) will see both sides closed,
        // allowing the pipe.readPipe.read to receive an EOF. This works also because the parent is closing the write
        // side before the read side is used for actual read.
        //
        // If execvp fails, the deferred on top of this branch will _exit(EXIT_FAILURE)
        int childErrno;

        const size_t cmdLen = commandArgumentsNumber > 1 ? commandArgumentsByteOffset[1] : command.view().sizeInBytes();
        const StringView cmd({command.view().getNullTerminatedNative(), cmdLen}, true, StringEncoding::Ascii);
        if (cmd.containsCodePoint('/'))
        {
            // cmd holds an absolute path, let's call execve directly
            childErrno = ::execve(cmd.getNullTerminatedNative(),               // command
                                  const_cast<char* const*>(argv),              // arguments
                                  const_cast<char* const*>(environmentArray)); // environment
        }
        else
        {
            // cmd holds a relative path, let's parse PATH variable to try execve prepending PATH entries (one by one)
            StringView pathEnv = StringView::fromNullTerminated(::getenv("PATH"), StringEncoding::Ascii);
            char       pathBuffer[1024 + 1];
            if (pathEnv.isEmpty())
            {
                // This is prescribed by Posix
                ::confstr(_CS_PATH, pathBuffer, 1024);
                pathEnv = StringView::fromNullTerminated(pathBuffer, StringEncoding::Ascii);
            }
            StringViewTokenizer tokenizer = pathEnv;
            while (tokenizer.tokenizeNext({':'}))
            {
                SmallStringNative<1024> finalCommand = pathEnv.getEncoding();

                StringConverter converter(finalCommand);
                SC_TRY(converter.appendNullTerminated(tokenizer.component));
                SC_TRY(converter.appendNullTerminated("/"));
                SC_TRY(converter.appendNullTerminated(cmd));
                childErrno = ::execve(finalCommand.view().getNullTerminatedNative(), // command
                                      const_cast<char* const*>(argv),                // arguments
                                      const_cast<char* const*>(environmentArray));   // environment
            }
        }

        // execvp failed, let's communicate errno back to parent before _exit(EXIT_FAILURE)
        childErrno = errno;
        // TODO: Add a write or Span overload that will not need to be called with a reinterpret_cast
        (void)pipe.writePipe.write({reinterpret_cast<char*>(&childErrno), sizeof(childErrno)});
        // We should not close the writePipe because if this happens before readPipe has been read, it will read 0
        // bytes thinking incorrectly that the process has succeeded.
        // (void)pipe.close();
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
            return Result::Error("execve failed");
        }
        SC_TRY(handle.assign(processID.pid));
        SC_TRY(stdInFd.close());
        SC_TRY(stdOutFd.close());
        SC_TRY(stdErrFd.close());
        return Result(true);
    }
    // The exit(127) inside isChild makes this unreachable
    Assert::unreachable();
}

SC::Result SC::Process::formatArguments(Span<const StringView> params)
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

bool SC::ProcessEnvironment::get(size_t index, StringView& name, StringView& value) const
{
    if (index >= numberOfEnvironment)
    {
        return false;
    }
    StringView          nameValue = StringView::fromNullTerminated(environment[index], StringEncoding::Ascii);
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
    SC_TRY(parentToFork.createPipe(PipeDescriptor::ReadNonInheritable, PipeDescriptor::WriteNonInheritable));
    SC_TRY(forkToParent.createPipe(PipeDescriptor::ReadNonInheritable, PipeDescriptor::WriteNonInheritable));
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
