// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Containers/IntrusiveDoubleLinkedList.h"
#include "../File/FileDescriptor.h"
#include "../Foundation/Function.h"
#include "../Strings/SmallString.h"
#include "ProcessDescriptor.h"

namespace SC
{
struct Process;
struct ProcessChain;
struct ProcessID;
} // namespace SC

//! @defgroup group_process Process
//! @copybrief library_process (see @ref library_process for more details)

//! @addtogroup group_process
//! @{

/// @brief Native os handle to a process identifier
struct SC::ProcessID
{
    int32_t pid = 0;
};

/// @brief Execute a child process.
///
/// Features:
/// - Redirecting standard in/out/err of a child process to a Pipe
/// - Wait for the child process exit code
///
/// @n
/**
 * Example: launch a process and wait for it to finish execution
 * @code{.cpp}
Process process;
SC_TRY(process.launch("executable.exe", "--argument1", "--argument2"));
SC_TRY(process.waitForExitSync());
 * @endcode
 *
 * Example: wait for the process to fully execute
 * @code{.cpp}
Process process;
SC_TRY(process.launch("executable.exe", "--argument1", "--argument2"));
SC_TRY(process.waitForExitSync());
 * @endcode
 *
 * Example: read process output
 * @code{.cpp}
Process process;
PipeDescriptor outputPipe;
SC_TRY(process.redirectStdOutTo(outputPipe));
SC_TRY(process.launch("executable.exe", "--argument1", "--argument2"));
String output = StringEncoding::Ascii; // Could also use SmallString<N>
SC_TRY(outputPipe.readPipe.readUntilEOF(output));
SC_TRY(process.waitForExitSync());
// ... Do something with the 'output' string
 * @endcode
 * */
struct SC::Process
{
    /// @brief Options for SC::Process::launch
    struct Options
    {
        bool inheritFileDescriptors; ///< If true, child process will inherit parent file descriptors
        Options() { inheritFileDescriptors = false; }
    };

    ProcessDescriptor handle;    ///< Handle to the OS process
    ProcessID         processID; ///< Id of the process

    ProcessDescriptor::ExitStatus exitStatus; ///< Exit status code returned after process is finished

    FileDescriptor standardInput;  ///< Descriptor of process stdin
    FileDescriptor standardOutput; ///< Descriptor of process stdout
    FileDescriptor standardError;  ///< Descriptor of process stderr

    /// @brief Waits (blocking) for process to exit after launch
    [[nodiscard]] Result waitForExitSync();

    /// @brief Launch child process with the given arguments
    /// @param args Process executable path and its arguments (if any)
    template <typename... StringView>
    [[nodiscard]] Result launch(StringView&&... args)
    {
        SC_TRY(formatArguments({forward<StringView>(args)...}));
        return launch();
    }

    /// @brief Launch child process with the given arguments
    /// @param cmd Process executable path and its arguments (if any)
    [[nodiscard]] Result launch(Span<const StringView> cmd)
    {
        SC_TRY(formatArguments(cmd));
        return launch();
    }

    /// @brief Redirect Process Standard Output to the given pipe
    /// @param pipe A reference to an empty Pipe object that will receive a pipe to read from stdout.
    [[nodiscard]] Result redirectStdOutTo(PipeDescriptor& pipe);

    /// @brief Redirect Process Standard Error to the given pipe
    /// @param pipe A reference to an empty Pipe object that will receive a pipe to read from stderr.
    [[nodiscard]] Result redirectStdErrTo(PipeDescriptor& pipe);

    /// @brief Redirect Process Standard Input to the given pipe
    /// @param pipe A reference to an empty Pipe object that will receive a pipe to write to stdin.
    [[nodiscard]] Result redirectStdInTo(PipeDescriptor& pipe);

  private:
    [[nodiscard]] Result launch(Options options = Options());
    template <typename... StringView>
    [[nodiscard]] Result formatCommand(StringView&&... args)
    {
        return formatArguments({forward<StringView>(args)...});
    }

    [[nodiscard]] Result formatArguments(Span<const StringView> cmd);

    StringNative<255>  command          = StringEncoding::Native;
    StringNative<255>  currentDirectory = StringEncoding::Native;
    StringNative<1024> environment      = StringEncoding::Native;

    friend struct IntrusiveDoubleLinkedList<Process>;
    friend struct ProcessChain;
    ProcessChain* parent = nullptr;

    Process* next = nullptr;
    Process* prev = nullptr;
    struct Internal;

    template <typename Lambda>
    [[nodiscard]] Result spawn(Lambda&& lambda);
    [[nodiscard]] Result fork();
    [[nodiscard]] bool   isChild() const;
};

/// @brief Execute multiple child processes chaining input / output between them.
/// @n
/// Chains multiple child processes together, so that the output of a process becomes input of another (similar to what
/// happens wit the pipe (`|`) operator on Posix shells).
///
/// SC::PipeDescriptor from [File](@ref library_file) library is used to chain read / write endpoints of different
/// processes together.
///
/// @n
/**
 * Example: print result `ls ~ | grep desktop` in current terminal
 * @code{.cpp}
ProcessChain chain([&](const ProcessChain::Error&) { });
Process      p1, p2;
SC_TRY(chain.pipe(p1, "ls", "~"));
SC_TRY(chain.pipe(p2, "grep", "Desktop"));
SC_TRY(chain.launch());
SC_TRY(chain.waitForExitSync());
 * @endcode
 *
 * Example: read result of `ls ~ | grep desktop` into a String
 * @code{.cpp}
ProcessChain chain([&](const ProcessChain::Error&) { });
Process      p1, p2;
SC_TRY(chain.pipe(p1, "ls", "~"));
SC_TRY(chain.pipe(p2, "grep", "Desktop"));
ProcessChain::Options options;
options.pipeSTDOUT = true;
SC_TRY(chain.launch(options));
String output(StringEncoding::Ascii);
SC_TRY(chain.readStdOutUntilEOFSync(output));
SC_TRY(chain.waitForExitSync());
// ... Do something with the 'output' string
 * @endcode
 * */
struct SC::ProcessChain
{
    /// @brief Specify if a child process input / output should be piped (intercepted)
    struct Options
    {
        bool pipeSTDIN;
        bool pipeSTDOUT;
        bool pipeSTDERR;
        Options()
        {
            pipeSTDIN  = false;
            pipeSTDOUT = false;
            pipeSTDERR = false;
        }
    };

    /// @brief Error context for error callback specified in constructor
    struct Error
    {
        Result returnCode = Result(true);
    };

    /// @brief construct with an error delegate
    /// @param onError a Delegate that will receive error notifications for processes that fail to start
    ProcessChain(Delegate<const Error&> onError) : onError(onError) {}

    /// @brief Add a process to the chain, with given arguments
    /// @param process A non-launched Process object (allocated by caller, must be alive until waitForExitSync)
    /// @param args Path to executable and eventual args for this process
    /// @return Invalid result if given process failed to create pipes for I/O redirection
    template <typename... StringView>
    [[nodiscard]] Result pipe(Process& process, StringView&&... args)
    {
        return pipe(process, {forward<const StringView>(args)...});
    }

    /// @brief Add a process to the chain, with given arguments
    /// @param process A non-launched Process object (allocated by caller, must be alive until waitForExitSync)
    /// @param cmd Path to executable and eventual args for this process
    /// @return Invalid result if given process failed to create pipes for I/O redirection
    [[nodiscard]] Result pipe(Process& process, const Span<const StringView> cmd);

    /// @brief Launch the entire chain of processes
    /// @param options Options for redirection I/O of the given process chain
    /// @return Valid result if given process chain has been launched successfully
    [[nodiscard]] Result launch(Options options = Options());

    /// @brief Waits (blocking) for entire process chain to exit after launch
    /// @return Valid result if the given process chain exited normally without aborting
    [[nodiscard]] Result waitForExitSync();

    /// @brief Reads stdout of last process in the chain into String
    /// @param destination A String that will contain all stdout from given process
    /// @return Valid result it was possible to read all stdout data in destination string
    [[nodiscard]] Result readStdOutUntilEOFSync(String& destination);

    /// @brief Reads stderr of last process in the chain into String
    /// @param destination A String that will contain all stderr from given process
    /// @return Valid result it was possible to read all stderr data in destination string
    [[nodiscard]] Result readStdErrUntilEOFSync(String& destination);

    /// @brief Reads stdout of last process in the chain into char buffer
    /// @param destination A char buffer that will contain all stdout from given process
    /// @return Valid result it was possible to read all stdout data in destination string
    [[nodiscard]] Result readStdOutUntilEOFSync(Vector<char>& destination);

    /// @brief Reads stderr of last process in the chain into char buffer
    /// @param destination A char buffer that will contain all stderr from given process
    /// @return Valid result it was possible to read all stderr data in destination string
    [[nodiscard]] Result readStdErrUntilEOFSync(Vector<char>& destination);

  private:
    Delegate<const Error&> onError;
    Error                  error;

    IntrusiveDoubleLinkedList<Process> processes;

    PipeDescriptor inputPipe;
    PipeDescriptor outputPipe;
    PipeDescriptor errorPipe;
};

//! @}
