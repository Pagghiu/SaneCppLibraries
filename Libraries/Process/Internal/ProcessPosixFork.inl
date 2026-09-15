#pragma once
#include "../../Common/Deferred.h"
#include "../../Process/Internal/EnvironmentTable.h"
#include "../../Process/Process.h"

#include <errno.h>
#include <signal.h> // sigfillset / sigdelset / sigaction
#include <stdio.h>  // stdout, stdin
#include <stdlib.h> // abort
#include <unistd.h> // pipe fork execl _exit
#if SC_PLATFORM_APPLE
#include <crt_externs.h>
// https://www.gnu.org/software/gnulib/manual/html_node/environ.html
#define environ (*_NSGetEnviron())
#else
extern char** environ;
#endif

struct SC::Process::InternalFork
{

    static FileDescriptor::Handle getStandardInputFDS() { return fileno(stdin); };
    static FileDescriptor::Handle getStandardOutputFDS() { return fileno(stdout); };
    static FileDescriptor::Handle getStandardErrorFDS() { return fileno(stderr); };

    static ResultProcess duplicateAndReplace(FileDescriptor& handle, FileDescriptor::Handle fds,
                                             ProcessError invalidRedirectionError)
    {
        FileDescriptor::Handle nativeFd;
        SC_TRY(handle.get(nativeFd, ResultProcess(invalidRedirectionError)));
        if (::dup2(nativeFd, fds) == -1)
        {
            return ResultProcess(ProcessError::LaunchFailed, ProcessErrorDetail::PosixDuplicateDescriptor,
                                 static_cast<uint32_t>(errno));
        }
        return ResultProcess(true);
    }

    static ResultProcess resetInheritedSignalHandlers()
    {
#if SC_COMPILER_FILC
        // Fil-C currently does not support the full pre-exec signal reset sequence we use on native libc builds.
        // Child processes still exec immediately afterwards, so skip this best-effort cleanup for now.
        return ResultProcess(true);
#else
        // For every signal, we restore the default action
        struct sigaction action;
        memset(&action, 0, sizeof(action));
        action.sa_handler = SIG_DFL;

        int res = sigemptyset(&action.sa_mask);
        if (res < 0)
        {
            return ResultProcess(ProcessError::LaunchFailed, ProcessErrorDetail::PosixResetSignalHandlers,
                                 static_cast<uint32_t>(errno));
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
                return ResultProcess(ProcessError::LaunchFailed, ProcessErrorDetail::PosixResetSignalHandlers,
                                     static_cast<uint32_t>(errno));
            }
        }

        // Clear all signals set
        sigset_t signalSet;

        res = sigemptyset(&signalSet);
        if (res < 0)
        {
            return ResultProcess(ProcessError::LaunchFailed, ProcessErrorDetail::PosixResetSignalHandlers,
                                 static_cast<uint32_t>(errno));
        }

        res = pthread_sigmask(SIG_SETMASK, &signalSet, NULL);
        if (res > 0) // pthread returns > 0 error codes
        {
            return ResultProcess(ProcessError::LaunchFailed, ProcessErrorDetail::PosixResetSignalHandlers,
                                 static_cast<uint32_t>(res));
        }

        return ResultProcess(true);
#endif
    }
};

SC::ResultProcess SC::Process::launchForkChild(PipeDescriptor& pipe)
{
    // If execvpe doesn't take control, we exit with failure code on error
    auto          exitDeferred = MakeDeferred([&] { _exit(EXIT_FAILURE); });
    ResultProcess childResult(ProcessError::LaunchFailed, ProcessErrorDetail::PosixExec);
    auto          reportDeferred =
        MakeDeferred([&] { (void)pipe.writePipe.write({reinterpret_cast<char*>(&childResult), sizeof(childResult)}); });
    auto reportFailure = [&](ResultProcess result) -> ResultProcess
    {
        childResult = result;
        return result;
    };

    // Try restoring default signal handlers
    ResultProcess result = InternalFork::resetInheritedSignalHandlers();
    if (not result)
        return reportFailure(result);

    if (stdInFd.isValid())
    {
        result = InternalFork::duplicateAndReplace(stdInFd, InternalFork::getStandardInputFDS(),
                                                   ProcessError::InvalidInputRedirection);
        if (not result)
            return reportFailure(result);
    }
    if (stdOutFd.isValid())
    {
        result = InternalFork::duplicateAndReplace(stdOutFd, InternalFork::getStandardOutputFDS(),
                                                   ProcessError::InvalidOutputRedirection);
        if (not result)
            return reportFailure(result);
    }
    if (stdErrFd.isValid())
    {
        result = InternalFork::duplicateAndReplace(stdErrFd, InternalFork::getStandardErrorFDS(),
                                                   ProcessError::InvalidOutputRedirection);
        if (not result)
            return reportFailure(result);
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
    result = stdInFd.close();
    if (not result)
        return reportFailure(result);
    result = stdOutFd.close();
    if (not result)
        return reportFailure(result);
    result = stdErrFd.close();
    if (not result)
        return reportFailure(result);

    // Switch to wanted current directory (if provided)
    if (not currentDirectory.view().isEmpty())
    {
        int res = ::chdir(currentDirectory.view().getNullTerminatedNative());
        if (res < 0)
        {
            return reportFailure(ResultProcess(ProcessError::LaunchFailed,
                                               ProcessErrorDetail::PosixChangeWorkingDirectory,
                                               static_cast<uint32_t>(errno)));
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
    result = environmentTable.writeTo(environmentArray, inheritEnv, table, parentEnv);
    if (not result)
        return reportFailure(result);
    // If execvp succeeds, this fork morphs into the new executable on the next line, and the parent communication
    // pipe, that has the CLOEXEC flags set (as it has been created as Non-inheritable) will see both sides closed,
    // allowing the pipe.readPipe.read to receive an EOF. This works also because the parent is closing the write
    // side before the read side is used for actual read.
    //
    // If execvp fails, the deferred on top of this branch will _exit(EXIT_FAILURE)
    const size_t     cmdLen = commandArgumentsNumber > 1 ? commandArgumentsByteOffset[1] : command.view().sizeInBytes();
    const StringSpan cmd({command.view().getNullTerminatedNative(), cmdLen}, true, StringEncoding::Ascii);
    const bool       usesParentEnvironment = inheritEnv and environment.view().isEmpty();
    if (usesParentEnvironment)
    {
        if (cmd.getNullTerminatedNative()[0] == '/')
        {
            (void)::execv(cmd.getNullTerminatedNative(), const_cast<char* const*>(argv));
        }
        else
        {
            (void)::execvp(cmd.getNullTerminatedNative(), const_cast<char* const*>(argv));
        }
    }
    else if (cmd.getNullTerminatedNative()[0] == '/')
    {
        // cmd holds an absolute path, let's call execve directly
        (void)::execve(cmd.getNullTerminatedNative(),               // command
                       const_cast<char* const*>(argv),              // arguments
                       const_cast<char* const*>(environmentArray)); // environment
    }
    else
    {
        // cmd holds a relative path, let's parse PATH variable to try execve prepending PATH entries (one by one)
        char        pathBuffer[1024 + 1];
        const char* cPath = ::getenv("PATH");
        if (cPath == nullptr or ::strlen(cPath) == 0)
        {
            // This is prescribed by Posix
            ::confstr(_CS_PATH, pathBuffer, 1024);
            cPath = pathBuffer;
        }
        // Split the PATH variable by ':' and try to execve with each entry using just C stdlib
        const char* pathStart = cPath;
        while (pathStart && *pathStart)
        {
            // Find the next ':' or end of string
            // UTF-8 SAFETY NOTE: The ':' character (ASCII 0x3A) is not a valid UTF-8 continuation byte, and cannot
            // appear as part of any multi-byte UTF-8 sequence. Therefore, using strchr to split PATH on ':' is safe
            // even if PATH contains UTF-8 encoded directory names.
            const char*  pathEnd = ::strchr(pathStart, ':');
            const size_t pathLen = pathEnd ? static_cast<size_t>(pathEnd - pathStart) : ::strlen(pathStart);

            // Skip empty path components (which mean current directory)
            if (pathLen > 0)
            {
                StringSpan pathComponent({pathStart, pathLen}, false, StringEncoding::Utf8);
                StringPath finalCommand;
                if (not finalCommand.append(pathComponent))
                    return reportFailure(
                        ResultProcess(ProcessError::LaunchFailed, ProcessErrorDetail::PosixBuildResolveExecutablePath));
                if (not finalCommand.append("/"))
                    return reportFailure(
                        ResultProcess(ProcessError::LaunchFailed, ProcessErrorDetail::PosixBuildResolveExecutablePath));
                if (not finalCommand.append(cmd))
                    return reportFailure(
                        ResultProcess(ProcessError::LaunchFailed, ProcessErrorDetail::PosixBuildResolveExecutablePath));
                (void)::execve(finalCommand.view().getNullTerminatedNative(), // command
                               const_cast<char* const*>(argv),                // arguments
                               const_cast<char* const*>(environmentArray));   // environment
            }

            // Move to next component
            if (pathEnd)
                pathStart = pathEnd + 1;
            else
                break;
        }
    }

    // execvp failed, the deferred above will communicate errno back to the parent before _exit(EXIT_FAILURE).
    return reportFailure(
        ResultProcess(ProcessError::LaunchFailed, ProcessErrorDetail::PosixExec, static_cast<uint32_t>(errno)));
}
