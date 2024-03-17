// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Containers/IntrusiveDoubleLinkedList.h"
#include "../File/FileDescriptor.h"
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

/// @brief Execute a child process with standard file descriptors redirection.
/// @n
/// Features:
///
/// - Redirect standard in/out/err of a child process to a Pipe
/// - Inherit child process file descriptors from parent process
/// - Ignore (silence) child process standard file descriptor
/// - Wait for the child process exit code
///
/// Example: execute child process (launch and wait for it to fully execute)
/// \snippet Libraries/Process/Tests/ProcessTest.cpp ProcessSnippet1
///
/// Example: execute child process, redirecting stdout to a string
/// \snippet Libraries/Process/Tests/ProcessTest.cpp ProcessSnippet2
///
/// Example: launch a child process and wait for it to finish execution
/// \snippet Libraries/Process/Tests/ProcessTest.cpp ProcessSnippet3
///
/// Example: execute child process, filling its stdin with a StringView
/// \snippet Libraries/Process/Tests/ProcessTest.cpp ProcessSnippet4
///
/// Example: read process output using a pipe, using launch + waitForExitSync
/// \snippet Libraries/Process/Tests/ProcessTest.cpp ProcessSnippet5

struct SC::Process
{
    struct StdStream
    {
        // clang-format off
        /// @brief Read the process standard output/error into the given String
        StdStream(String& externalString) { operation = Operation::String; string = &externalString;}

        /// @brief Read the process standard output/error into the given Vector
        StdStream(Vector<char>& externalVector) { operation = Operation::Vector; vector = &externalVector; }
        // clang-format on

        /// @brief Redirects child process standard output/error to a given file descriptor
        StdStream(FileDescriptor&& file)
        {
            operation = Operation::FileDescriptor;
            (void)file.get(fileDescriptor, Result::Error("Invalid redirection file descriptor"));
            file.detach();
        }

        StdStream(PipeDescriptor& pipe)
        {
            operation      = Operation::ExternalPipe;
            pipeDescriptor = &pipe;
        }

      protected:
        struct AlreadySetup
        {
        };
        StdStream() = default;
        StdStream(AlreadySetup) { operation = Operation::AlreadySetup; }
        friend struct Process;
        friend struct ProcessChain;

        enum class Operation
        {
            AlreadySetup,
            Inherit,
            Ignore,
            ExternalPipe,
            FileDescriptor,
            Vector,
            String,
            WritableSpan,
            ReadableSpan
        };
        Operation operation = Operation::Inherit;

        Span<const char> readableSpan;
        Span<char>       writableSpan;

        String*       string;
        Vector<char>* vector;

        FileDescriptor::Handle fileDescriptor;

        PipeDescriptor* pipeDescriptor;
    };

    struct StdOut : public StdStream
    {
        // clang-format off
        struct Ignore{};
        struct Inherit{};

        /// @brief Ignores child process standard output/error (child process output will be silenced)
        StdOut(Ignore) { operation = Operation::Ignore; }

        /// @brief Inherits child process standard output/error (child process will print into parent process console)
        StdOut(Inherit) { operation = Operation::Inherit; }

        /// @brief Read the process standard output/error into the given Span
        StdOut(Span<char> span) { operation = Operation::WritableSpan; writableSpan = span; }

        using StdStream::StdStream;
        friend struct ProcessChain;
        // clang-format on
    };

    using StdErr = StdOut;

    struct StdIn : public StdStream
    {
        // clang-format off
        struct Inherit{};

        /// @brief Inherits child process Input from parent process
        StdIn(Inherit) { operation = Operation::Inherit; }

        /// @brief Fills standard input with content of a C-String
        template <int N> StdIn(const char (&item)[N]) : StdIn(StringView({item, N - 1}, true, StringEncoding::Ascii)) {}

        /// @brief Fills standard input with content of a StringView
        StdIn(StringView string) : StdIn(string.toCharSpan()) {}

        /// @brief Fills standard input with content of a Span
        StdIn(Span<const char> span) { operation = Operation::ReadableSpan; readableSpan = span;}

        using StdStream::StdStream;
        friend struct ProcessChain;
        // clang-format on
    };

    ProcessDescriptor handle;    ///< Handle to the OS process
    ProcessID         processID; ///< ID of the process (can be the same as handle on some OS)

    /// @brief Waits (blocking) for process to exit after launch. It can only be called if Process::launch succeeded.
    [[nodiscard]] Result waitForExitSync();

    /// @brief Launch child process with the given arguments
    /// @param cmd Process executable path and its arguments (if any)
    /// @param stdOut Process::StdOut::Ignore{}, Process::StdOut::Inherit{} or redirect stdout to String/Vector/Span
    /// @param stdIn Process::StdIn::Ignore{}, Process::StdIn::Inherit{} or feed stdin from
    /// StringView/String/Vector/Span
    /// @param stdErr Process::StdErr::Ignore{}, Process::StdErr::Inherit{} or redirect stderr to String/Vector/Span
    /// @returns Error if the requested executable doesn't exist / is not accessible / it cannot be executed
    [[nodiscard]] Result launch(Span<const StringView> cmd,                        //
                                const StdOut&          stdOut = StdOut::Inherit{}, //
                                const StdIn&           stdIn  = StdIn::Inherit{},  //
                                const StdErr&          stdErr = StdErr::Inherit{})
    {
        SC_TRY(formatArguments(cmd));
        return launch(stdOut, stdIn, stdErr);
    }

    /// @brief Executes a  child process with the given arguments, waiting (blocking) until it's fully finished
    /// @param cmd Process executable path and its arguments (if any)
    /// @param stdOut Process::StdOut::Ignore{}, Process::StdOut::Inherit{} or redirect stdout to String/Vector/Span
    /// @param stdIn Process::StdIn::Ignore{}, Process::StdIn::Inherit{} or feed stdin from
    /// StringView/String/Vector/Span
    /// @param stdErr Process::StdErr::Ignore{}, Process::StdErr::Inherit{} or redirect stderr to String/Vector/Span
    /// @returns Error if the requested executable doesn't exist / is not accessible / it cannot be executed
    [[nodiscard]] Result exec(Span<const StringView> cmd,                        //
                              const StdOut&          stdOut = StdOut::Inherit{}, //
                              const StdIn&           stdIn  = StdIn::Inherit{},  //
                              const StdErr&          stdErr = StdErr::Inherit{})
    {
        SC_TRY(launch(cmd, stdOut, stdIn, stdErr));
        return waitForExitSync();
    }

    /// @brief gets the return code from the exited child process (valid only after exec or waitForExitSync)
    int32_t getExitStatus() const { return exitStatus.status; }

    /// @brief Returns number of (virtual) processors available
    static size_t getNumberOfProcessors();

  private:
    ProcessDescriptor::ExitStatus exitStatus; ///< Exit status code returned after process is finished

    FileDescriptor stdInFd;  ///< Descriptor of process stdin
    FileDescriptor stdOutFd; ///< Descriptor of process stdout
    FileDescriptor stdErrFd; ///< Descriptor of process stderr

    [[nodiscard]] Result launch(const StdOut& stdOutput, const StdIn& stdInput, const StdErr& stdError);

    [[nodiscard]] Result formatArguments(Span<const StringView> cmd);

    // TODO: These must be exposed and filled properly with existing values
    StringNative<255>  currentDirectory = StringEncoding::Native;
    StringNative<1024> environment      = StringEncoding::Native;

    // On Windows command holds the concatenation of executable and arguments.
    // On Posix command holds the concatenation of executable and arguments SEPARATED BY null-terminators (\0).
    // This is done so that in this single buffer with no allocation (under 255) or a single allocation (above 255)
    // we can track all arguments to be passed to execve.
    StringNative<255> command = StringEncoding::Native;
#if !SC_PLATFORM_WINDOWS // On Posix we need to track the "sub-strings" hidden in command
    static constexpr size_t MAX_NUM_ARGUMENTS = 64;
    size_t commandArgumentsByteOffset[MAX_NUM_ARGUMENTS]; // Tracking length of each argument in the command string
    size_t commandArgumentsNumber = 0;                    // Counts number of arguments (including executable name)
#endif

    friend struct IntrusiveDoubleLinkedList<Process>;
    friend struct ProcessChain;
    ProcessChain* parent = nullptr;

    Process* next = nullptr;
    Process* prev = nullptr;
    struct Internal;

    Result launchImplementation();
};

/// @brief Execute multiple child processes chaining input / output between them.
/// @n
/// Chains multiple child processes together, so that the output of a process becomes input of another (similar to what
/// happens wit the pipe (`|`) operator on Posix shells).
///
/// SC::PipeDescriptor from [File](@ref library_file) library is used to chain read / write endpoints of different
/// processes together.
///
/// Example: Inherit stdout file descriptor
/// \snippet Libraries/Process/Tests/ProcessTest.cpp processChainInheritDualSnippet
///
/// Example: Read stderr and stdout into a string
/// \snippet Libraries/Process/Tests/ProcessTest.cpp processChainPipeSingleSnippet
///
/// Example: Read standard output into a string using a Pipe
/// \snippet Libraries/Process/Tests/ProcessTest.cpp processChainPipeDualSnippet
struct SC::ProcessChain
{
    /// @brief Add a process to the chain, with given arguments
    /// @param process A non-launched Process object (allocated by caller, must be alive until waitForExitSync)
    /// @param cmd Path to executable and eventual args for this process
    /// @return Invalid result if given process failed to create pipes for I/O redirection
    /// @todo Expose options to decide if to pipe also stderr
    [[nodiscard]] Result pipe(Process& process, const Span<const StringView> cmd);

    /// @brief Launch the entire chain of processes. Reading from pipes can be done after launching.
    /// You can then call ProcessChain::waitForExitSync to block until the child process is fully finished.
    /// @return Valid result if given process chain has been launched successfully
    [[nodiscard]] Result launch(const Process::StdOut& stdOut = Process::StdOut::Inherit{}, //
                                const Process::StdIn&  stdIn  = Process::StdIn::Inherit{},  //
                                const Process::StdErr& stdErr = Process::StdErr::Inherit{});

    /// @brief Waits (blocking) for entire process chain to exit. Can be called only after ProcessChain::launch.
    /// @return Valid result if the given process chain exited normally without aborting
    [[nodiscard]] Result waitForExitSync();

    /// @brief Launch the entire chain of processes and waits for the results (calling ProcessChain::waitForExitSync)
    /// @return Valid result if given process chain has been launched and waited for exit successfully
    [[nodiscard]] Result exec(const Process::StdOut& stdOut = Process::StdOut::Inherit{}, //
                              const Process::StdIn&  stdIn  = Process::StdIn::Inherit{},  //
                              const Process::StdErr& stdErr = Process::StdErr::Inherit{})
    {
        SC_TRY(launch(stdOut, stdIn, stdErr));
        return waitForExitSync();
    }

  private:
    IntrusiveDoubleLinkedList<Process> processes;
};

//! @}
